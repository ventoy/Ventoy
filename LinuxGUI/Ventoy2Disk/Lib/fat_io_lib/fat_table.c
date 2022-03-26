//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//                            FAT16/32 File IO Library
//                                    V2.6
//                              Ultra-Embedded.com
//                            Copyright 2003 - 2012
//
//                         Email: admin@ultra-embedded.com
//
//                                License: GPL
//   If you would like a version with a more permissive license for use in
//   closed source commercial applications please contact me for details.
//-----------------------------------------------------------------------------
//
// This file is part of FAT File IO Library.
//
// FAT File IO Library is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// FAT File IO Library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with FAT File IO Library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#include <string.h>
#include "fat_defs.h"
#include "fat_access.h"
#include "fat_table.h"

#ifndef FAT_BUFFERS
    #define FAT_BUFFERS 1
#endif

#ifndef FAT_BUFFER_SECTORS
    #define FAT_BUFFER_SECTORS 1
#endif

#if FAT_BUFFERS < 1 || FAT_BUFFER_SECTORS < 1
    #error "FAT_BUFFERS & FAT_BUFFER_SECTORS must be at least 1"
#endif

//-----------------------------------------------------------------------------
//                            FAT Sector Buffer
//-----------------------------------------------------------------------------
#define FAT32_GET_32BIT_WORD(pbuf, location)        ( GET_32BIT_WORD(pbuf->ptr, location) )
#define FAT32_SET_32BIT_WORD(pbuf, location, value) { SET_32BIT_WORD(pbuf->ptr, location, value); pbuf->dirty = 1; }
#define FAT16_GET_16BIT_WORD(pbuf, location)        ( GET_16BIT_WORD(pbuf->ptr, location) )
#define FAT16_SET_16BIT_WORD(pbuf, location, value) { SET_16BIT_WORD(pbuf->ptr, location, value); pbuf->dirty = 1; }

//-----------------------------------------------------------------------------
// fatfs_fat_init:
//-----------------------------------------------------------------------------
void fatfs_fat_init(struct fatfs *fs)
{
    int i;

    // FAT buffer chain head
    fs->fat_buffer_head = NULL;

    for (i=0;i<FAT_BUFFERS;i++)
    {
        // Initialise buffers to invalid
        fs->fat_buffers[i].address = FAT32_INVALID_CLUSTER;
        fs->fat_buffers[i].dirty = 0;
        memset(fs->fat_buffers[i].sector, 0x00, sizeof(fs->fat_buffers[i].sector));
        fs->fat_buffers[i].ptr = NULL;

        // Add to head of queue
        fs->fat_buffers[i].next = fs->fat_buffer_head;
        fs->fat_buffer_head = &fs->fat_buffers[i];
    }
}
//-----------------------------------------------------------------------------
// fatfs_fat_writeback: Writeback 'dirty' FAT sectors to disk
//-----------------------------------------------------------------------------
static int fatfs_fat_writeback(struct fatfs *fs, struct fat_buffer *pcur)
{
    if (pcur)
    {
        // Writeback sector if changed
        if (pcur->dirty)
        {
            if (fs->disk_io.write_media)
            {
                uint32 sectors = FAT_BUFFER_SECTORS;
                uint32 offset = pcur->address - fs->fat_begin_lba;

                // Limit to sectors used for the FAT
                if ((offset + FAT_BUFFER_SECTORS) <= fs->fat_sectors)
                    sectors = FAT_BUFFER_SECTORS;
                else
                    sectors = fs->fat_sectors - offset;

                if (!fs->disk_io.write_media(pcur->address, pcur->sector, sectors))
                    return 0;
            }

            pcur->dirty = 0;
        }

        return 1;
    }
    else
        return 0;
}
//-----------------------------------------------------------------------------
// fatfs_fat_read_sector: Read a FAT sector
//-----------------------------------------------------------------------------
static struct fat_buffer *fatfs_fat_read_sector(struct fatfs *fs, uint32 sector)
{
    struct fat_buffer *last = NULL;
    struct fat_buffer *pcur = fs->fat_buffer_head;

    // Itterate through sector buffer list
    while (pcur)
    {
        // Sector within this buffer?
        if ((sector >= pcur->address) && (sector < (pcur->address + FAT_BUFFER_SECTORS)))
            break;

        // End of list?
        if (pcur->next == NULL)
        {
            // Remove buffer from list
            if (last)
                last->next = NULL;
            // We the first and last buffer in the chain?
            else
                fs->fat_buffer_head = NULL;
        }

        last = pcur;
        pcur = pcur->next;
    }

    // We found the sector already in FAT buffer chain
    if (pcur)
    {
        pcur->ptr = (uint8 *)(pcur->sector + ((sector - pcur->address) * FAT_SECTOR_SIZE));
        return pcur;
    }

    // Else, we removed the last item from the list
    pcur = last;

    // Add to start of sector buffer list (now newest sector)
    pcur->next = fs->fat_buffer_head;
    fs->fat_buffer_head = pcur;

    // Writeback sector if changed
    if (pcur->dirty)
        if (!fatfs_fat_writeback(fs, pcur))
            return 0;

    // Address is now new sector
    pcur->address = sector;

    // Read next sector
    if (!fs->disk_io.read_media(pcur->address, pcur->sector, FAT_BUFFER_SECTORS))
    {
        // Read failed, invalidate buffer address
        pcur->address = FAT32_INVALID_CLUSTER;
        return NULL;
    }

    pcur->ptr = pcur->sector;
    return pcur;
}
//-----------------------------------------------------------------------------
// fatfs_fat_purge: Purge 'dirty' FAT sectors to disk
//-----------------------------------------------------------------------------
int fatfs_fat_purge(struct fatfs *fs)
{
    struct fat_buffer *pcur = fs->fat_buffer_head;

    // Itterate through sector buffer list
    while (pcur)
    {
        // Writeback sector if changed
        if (pcur->dirty)
            if (!fatfs_fat_writeback(fs, pcur))
                return 0;

        pcur = pcur->next;
    }

    return 1;
}

//-----------------------------------------------------------------------------
//                        General FAT Table Operations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// fatfs_find_next_cluster: Return cluster number of next cluster in chain by
// reading FAT table and traversing it. Return 0xffffffff for end of chain.
//-----------------------------------------------------------------------------
uint32 fatfs_find_next_cluster(struct fatfs *fs, uint32 current_cluster)
{
    uint32 fat_sector_offset, position;
    uint32 nextcluster;
    struct fat_buffer *pbuf;

    // Why is '..' labelled with cluster 0 when it should be 2 ??
    if (current_cluster == 0)
        current_cluster = 2;

    // Find which sector of FAT table to read
    if (fs->fat_type == FAT_TYPE_16)
        fat_sector_offset = current_cluster / 256;
    else
        fat_sector_offset = current_cluster / 128;

    // Read FAT sector into buffer
    pbuf = fatfs_fat_read_sector(fs, fs->fat_begin_lba+fat_sector_offset);
    if (!pbuf)
        return (FAT32_LAST_CLUSTER);

    if (fs->fat_type == FAT_TYPE_16)
    {
        // Find 32 bit entry of current sector relating to cluster number
        position = (current_cluster - (fat_sector_offset * 256)) * 2;

        // Read Next Clusters value from Sector Buffer
        nextcluster = FAT16_GET_16BIT_WORD(pbuf, (uint16)position);

        // If end of chain found
        if (nextcluster >= 0xFFF8 && nextcluster <= 0xFFFF)
            return (FAT32_LAST_CLUSTER);
    }
    else
    {
        // Find 32 bit entry of current sector relating to cluster number
        position = (current_cluster - (fat_sector_offset * 128)) * 4;

        // Read Next Clusters value from Sector Buffer
        nextcluster = FAT32_GET_32BIT_WORD(pbuf, (uint16)position);

        // Mask out MS 4 bits (its 28bit addressing)
        nextcluster = nextcluster & 0x0FFFFFFF;

        // If end of chain found
        if (nextcluster >= 0x0FFFFFF8 && nextcluster <= 0x0FFFFFFF)
            return (FAT32_LAST_CLUSTER);
    }

    // Else return next cluster
    return (nextcluster);
}
//-----------------------------------------------------------------------------
// fatfs_set_fs_info_next_free_cluster: Write the next free cluster to the FSINFO table
//-----------------------------------------------------------------------------
void fatfs_set_fs_info_next_free_cluster(struct fatfs *fs, uint32 newValue)
{
    if (fs->fat_type == FAT_TYPE_16)
        ;
    else
    {
        // Load sector to change it
        struct fat_buffer *pbuf = fatfs_fat_read_sector(fs, fs->lba_begin+fs->fs_info_sector);
        if (!pbuf)
            return ;

        // Change
        FAT32_SET_32BIT_WORD(pbuf, 492, newValue);
        fs->next_free_cluster = newValue;

        // Write back FSINFO sector to disk
        if (fs->disk_io.write_media)
            fs->disk_io.write_media(pbuf->address, pbuf->sector, 1);

        // Invalidate cache entry
        pbuf->address = FAT32_INVALID_CLUSTER;
        pbuf->dirty = 0;
    }
}
//-----------------------------------------------------------------------------
// fatfs_find_blank_cluster: Find a free cluster entry by reading the FAT
//-----------------------------------------------------------------------------
#if FATFS_INC_WRITE_SUPPORT
int fatfs_find_blank_cluster(struct fatfs *fs, uint32 start_cluster, uint32 *free_cluster)
{
    uint32 fat_sector_offset, position;
    uint32 nextcluster;
    uint32 current_cluster = start_cluster;
    struct fat_buffer *pbuf;

    do
    {
        // Find which sector of FAT table to read
        if (fs->fat_type == FAT_TYPE_16)
            fat_sector_offset = current_cluster / 256;
        else
            fat_sector_offset = current_cluster / 128;

        if ( fat_sector_offset < fs->fat_sectors)
        {
            // Read FAT sector into buffer
            pbuf = fatfs_fat_read_sector(fs, fs->fat_begin_lba+fat_sector_offset);
            if (!pbuf)
                return 0;

            if (fs->fat_type == FAT_TYPE_16)
            {
                // Find 32 bit entry of current sector relating to cluster number
                position = (current_cluster - (fat_sector_offset * 256)) * 2;

                // Read Next Clusters value from Sector Buffer
                nextcluster = FAT16_GET_16BIT_WORD(pbuf, (uint16)position);
            }
            else
            {
                // Find 32 bit entry of current sector relating to cluster number
                position = (current_cluster - (fat_sector_offset * 128)) * 4;

                // Read Next Clusters value from Sector Buffer
                nextcluster = FAT32_GET_32BIT_WORD(pbuf, (uint16)position);

                // Mask out MS 4 bits (its 28bit addressing)
                nextcluster = nextcluster & 0x0FFFFFFF;
            }

            if (nextcluster !=0 )
                current_cluster++;
        }
        else
            // Otherwise, run out of FAT sectors to check...
            return 0;
    }
    while (nextcluster != 0x0);

    // Found blank entry
    *free_cluster = current_cluster;
    return 1;
}
#endif
//-----------------------------------------------------------------------------
// fatfs_fat_set_cluster: Set a cluster link in the chain. NOTE: Immediate
// write (slow).
//-----------------------------------------------------------------------------
#if FATFS_INC_WRITE_SUPPORT
int fatfs_fat_set_cluster(struct fatfs *fs, uint32 cluster, uint32 next_cluster)
{
    struct fat_buffer *pbuf;
    uint32 fat_sector_offset, position;

    // Find which sector of FAT table to read
    if (fs->fat_type == FAT_TYPE_16)
        fat_sector_offset = cluster / 256;
    else
        fat_sector_offset = cluster / 128;

    // Read FAT sector into buffer
    pbuf = fatfs_fat_read_sector(fs, fs->fat_begin_lba+fat_sector_offset);
    if (!pbuf)
        return 0;

    if (fs->fat_type == FAT_TYPE_16)
    {
        // Find 16 bit entry of current sector relating to cluster number
        position = (cluster - (fat_sector_offset * 256)) * 2;

        // Write Next Clusters value to Sector Buffer
        FAT16_SET_16BIT_WORD(pbuf, (uint16)position, ((uint16)next_cluster));
    }
    else
    {
        // Find 32 bit entry of current sector relating to cluster number
        position = (cluster - (fat_sector_offset * 128)) * 4;

        // Write Next Clusters value to Sector Buffer
        FAT32_SET_32BIT_WORD(pbuf, (uint16)position, next_cluster);
    }

    return 1;
}
#endif
//-----------------------------------------------------------------------------
// fatfs_free_cluster_chain: Follow a chain marking each element as free
//-----------------------------------------------------------------------------
#if FATFS_INC_WRITE_SUPPORT
int fatfs_free_cluster_chain(struct fatfs *fs, uint32 start_cluster)
{
    uint32 last_cluster;
    uint32 next_cluster = start_cluster;

    // Loop until end of chain
    while ( (next_cluster != FAT32_LAST_CLUSTER) && (next_cluster != 0x00000000) )
    {
        last_cluster = next_cluster;

        // Find next link
        next_cluster = fatfs_find_next_cluster(fs, next_cluster);

        // Clear last link
        fatfs_fat_set_cluster(fs, last_cluster, 0x00000000);
    }

    return 1;
}
#endif
//-----------------------------------------------------------------------------
// fatfs_fat_add_cluster_to_chain: Follow a chain marking and then add a new entry
// to the current tail.
//-----------------------------------------------------------------------------
#if FATFS_INC_WRITE_SUPPORT
int fatfs_fat_add_cluster_to_chain(struct fatfs *fs, uint32 start_cluster, uint32 newEntry)
{
    uint32 last_cluster = FAT32_LAST_CLUSTER;
    uint32 next_cluster = start_cluster;

    if (start_cluster == FAT32_LAST_CLUSTER)
        return 0;

    // Loop until end of chain
    while ( next_cluster != FAT32_LAST_CLUSTER )
    {
        last_cluster = next_cluster;

        // Find next link
        next_cluster = fatfs_find_next_cluster(fs, next_cluster);
        if (!next_cluster)
            return 0;
    }

    // Add link in for new cluster
    fatfs_fat_set_cluster(fs, last_cluster, newEntry);

    // Mark new cluster as end of chain
    fatfs_fat_set_cluster(fs, newEntry, FAT32_LAST_CLUSTER);

    return 1;
}
#endif
//-----------------------------------------------------------------------------
// fatfs_count_free_clusters:
//-----------------------------------------------------------------------------
uint32 fatfs_count_free_clusters(struct fatfs *fs)
{
    uint32 i,j;
    uint32 count = 0;
    struct fat_buffer *pbuf;

    for (i = 0; i < fs->fat_sectors; i++)
    {
        // Read FAT sector into buffer
        pbuf = fatfs_fat_read_sector(fs, fs->fat_begin_lba + i);
        if (!pbuf)
            break;

        for (j = 0; j < FAT_SECTOR_SIZE; )
        {
            if (fs->fat_type == FAT_TYPE_16)
            {
                if (FAT16_GET_16BIT_WORD(pbuf, (uint16)j) == 0)
                    count++;

                j += 2;
            }
            else
            {
                if (FAT32_GET_32BIT_WORD(pbuf, (uint16)j) == 0)
                    count++;

                j += 4;
            }
        }
    }

    return count;
}
