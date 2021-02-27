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
#include "fat_write.h"
#include "fat_string.h"
#include "fat_misc.h"
#include "fat_format.h"

#if FATFS_INC_FORMAT_SUPPORT

//-----------------------------------------------------------------------------
// Tables
//-----------------------------------------------------------------------------
struct sec_per_clus_table
{
    uint32  sectors;
    uint8   sectors_per_cluster;
};

struct sec_per_clus_table _cluster_size_table16[] =
{
    { 32680, 2},    // 16MB - 1K
    { 262144, 4},   // 128MB - 2K
    { 524288, 8},   // 256MB - 4K
    { 1048576, 16}, // 512MB - 8K
    { 2097152, 32}, // 1GB - 16K
    { 4194304, 64}, // 2GB - 32K
    { 8388608, 128},// 2GB - 64K [Warning only supported by Windows XP onwards]
    { 0 , 0 }       // Invalid
};

struct sec_per_clus_table _cluster_size_table32[] =
{
    { 532480, 1},     // 260MB - 512b
    { 16777216, 8},   // 8GB - 4K
    { 33554432, 16},  // 16GB - 8K
    { 67108864, 32},  // 32GB - 16K
    { 0xFFFFFFFF, 64},// >32GB - 32K
    { 0 , 0 }         // Invalid
};

//-----------------------------------------------------------------------------
// fatfs_calc_cluster_size: Calculate what cluster size should be used
//-----------------------------------------------------------------------------
static uint8 fatfs_calc_cluster_size(uint32 sectors, int is_fat32)
{
    int i;

    if (!is_fat32)
    {
        for (i=0; _cluster_size_table16[i].sectors_per_cluster != 0;i++)
            if (sectors <= _cluster_size_table16[i].sectors)
                return _cluster_size_table16[i].sectors_per_cluster;
    }
    else
    {
        for (i=0; _cluster_size_table32[i].sectors_per_cluster != 0;i++)
            if (sectors <= _cluster_size_table32[i].sectors)
                return _cluster_size_table32[i].sectors_per_cluster;
    }

    return 0;
}
//-----------------------------------------------------------------------------
// fatfs_erase_sectors: Erase a number of sectors
//-----------------------------------------------------------------------------
static int fatfs_erase_sectors(struct fatfs *fs, uint32 lba, int count)
{
    int i;

    // Zero sector first
    memset(fs->currentsector.sector, 0, FAT_SECTOR_SIZE);

    for (i=0;i<count;i++)
        if (!fs->disk_io.write_media(lba + i, fs->currentsector.sector, 1))
            return 0;

    return 1;
}
//-----------------------------------------------------------------------------
// fatfs_create_boot_sector: Create the boot sector
//-----------------------------------------------------------------------------
static int fatfs_create_boot_sector(struct fatfs *fs, uint32 boot_sector_lba, uint32 vol_sectors, const char *name, int is_fat32)
{
    uint32 total_clusters;
    int i;

    // Zero sector initially
    memset(fs->currentsector.sector, 0, FAT_SECTOR_SIZE);

    // OEM Name & Jump Code
    fs->currentsector.sector[0] = 0xEB;
    fs->currentsector.sector[1] = 0x3C;
    fs->currentsector.sector[2] = 0x90;
    fs->currentsector.sector[3] = 0x4D;
    fs->currentsector.sector[4] = 0x53;
    fs->currentsector.sector[5] = 0x44;
    fs->currentsector.sector[6] = 0x4F;
    fs->currentsector.sector[7] = 0x53;
    fs->currentsector.sector[8] = 0x35;
    fs->currentsector.sector[9] = 0x2E;
    fs->currentsector.sector[10] = 0x30;

    // Bytes per sector
    fs->currentsector.sector[11] = (FAT_SECTOR_SIZE >> 0) & 0xFF;
    fs->currentsector.sector[12] = (FAT_SECTOR_SIZE >> 8) & 0xFF;

    // Get sectors per cluster size for the disk
    fs->sectors_per_cluster = fatfs_calc_cluster_size(vol_sectors, is_fat32);
    if (!fs->sectors_per_cluster)
        return 0; // Invalid disk size

    // Sectors per cluster
    fs->currentsector.sector[13] = fs->sectors_per_cluster;

    // Reserved Sectors
    if (!is_fat32)
        fs->reserved_sectors = 8;
    else
        fs->reserved_sectors = 32;
    fs->currentsector.sector[14] = (fs->reserved_sectors >> 0) & 0xFF;
    fs->currentsector.sector[15] = (fs->reserved_sectors >> 8) & 0xFF;

    // Number of FATS
    fs->num_of_fats = 2;
    fs->currentsector.sector[16] = fs->num_of_fats;

    // Max entries in root dir (FAT16 only)
    if (!is_fat32)
    {
        fs->root_entry_count = 512;
        fs->currentsector.sector[17] = (fs->root_entry_count >> 0) & 0xFF;
        fs->currentsector.sector[18] = (fs->root_entry_count >> 8) & 0xFF;
    }
    else
    {
        fs->root_entry_count = 0;
        fs->currentsector.sector[17] = 0;
        fs->currentsector.sector[18] = 0;
    }

    // [FAT16] Total sectors (use FAT32 count instead)
    fs->currentsector.sector[19] = 0x00;
    fs->currentsector.sector[20] = 0x00;

    // Media type
    fs->currentsector.sector[21] = 0xF8;


    // FAT16 BS Details
    if (!is_fat32)
    {
        // Count of sectors used by the FAT table (FAT16 only)
        total_clusters = (vol_sectors / fs->sectors_per_cluster) + 1;
        fs->fat_sectors = (total_clusters/(FAT_SECTOR_SIZE/2)) + 1;
        fs->currentsector.sector[22] = (uint8)((fs->fat_sectors >> 0) & 0xFF);
        fs->currentsector.sector[23] = (uint8)((fs->fat_sectors >> 8) & 0xFF);

        // Sectors per track
        fs->currentsector.sector[24] = 0x00;
        fs->currentsector.sector[25] = 0x00;

        // Heads
        fs->currentsector.sector[26] = 0x00;
        fs->currentsector.sector[27] = 0x00;

        // Hidden sectors
        fs->currentsector.sector[28] = 0x20;
        fs->currentsector.sector[29] = 0x00;
        fs->currentsector.sector[30] = 0x00;
        fs->currentsector.sector[31] = 0x00;

        // Total sectors for this volume
        fs->currentsector.sector[32] = (uint8)((vol_sectors>>0)&0xFF);
        fs->currentsector.sector[33] = (uint8)((vol_sectors>>8)&0xFF);
        fs->currentsector.sector[34] = (uint8)((vol_sectors>>16)&0xFF);
        fs->currentsector.sector[35] = (uint8)((vol_sectors>>24)&0xFF);

        // Drive number
        fs->currentsector.sector[36] = 0x00;

        // Reserved
        fs->currentsector.sector[37] = 0x00;

        // Boot signature
        fs->currentsector.sector[38] = 0x29;

        // Volume ID
        fs->currentsector.sector[39] = 0x12;
        fs->currentsector.sector[40] = 0x34;
        fs->currentsector.sector[41] = 0x56;
        fs->currentsector.sector[42] = 0x78;

        // Volume name
        for (i=0;i<11;i++)
        {
            if (i < (int)strlen(name))
                fs->currentsector.sector[i+43] = name[i];
            else
                fs->currentsector.sector[i+43] = ' ';
        }

        // File sys type
        fs->currentsector.sector[54] = 'F';
        fs->currentsector.sector[55] = 'A';
        fs->currentsector.sector[56] = 'T';
        fs->currentsector.sector[57] = '1';
        fs->currentsector.sector[58] = '6';
        fs->currentsector.sector[59] = ' ';
        fs->currentsector.sector[60] = ' ';
        fs->currentsector.sector[61] = ' ';

        // Signature
        fs->currentsector.sector[510] = 0x55;
        fs->currentsector.sector[511] = 0xAA;
    }
    // FAT32 BS Details
    else
    {
        // Count of sectors used by the FAT table (FAT16 only)
        fs->currentsector.sector[22] = 0;
        fs->currentsector.sector[23] = 0;

        // Sectors per track (default)
        fs->currentsector.sector[24] = 0x3F;
        fs->currentsector.sector[25] = 0x00;

        // Heads (default)
        fs->currentsector.sector[26] = 0xFF;
        fs->currentsector.sector[27] = 0x00;

        // Hidden sectors
        fs->currentsector.sector[28] = 0x00;
        fs->currentsector.sector[29] = 0x00;
        fs->currentsector.sector[30] = 0x00;
        fs->currentsector.sector[31] = 0x00;

        // Total sectors for this volume
        fs->currentsector.sector[32] = (uint8)((vol_sectors>>0)&0xFF);
        fs->currentsector.sector[33] = (uint8)((vol_sectors>>8)&0xFF);
        fs->currentsector.sector[34] = (uint8)((vol_sectors>>16)&0xFF);
        fs->currentsector.sector[35] = (uint8)((vol_sectors>>24)&0xFF);

        total_clusters = (vol_sectors / fs->sectors_per_cluster) + 1;
        fs->fat_sectors = (total_clusters/(FAT_SECTOR_SIZE/4)) + 1;

        // BPB_FATSz32
        fs->currentsector.sector[36] = (uint8)((fs->fat_sectors>>0)&0xFF);
        fs->currentsector.sector[37] = (uint8)((fs->fat_sectors>>8)&0xFF);
        fs->currentsector.sector[38] = (uint8)((fs->fat_sectors>>16)&0xFF);
        fs->currentsector.sector[39] = (uint8)((fs->fat_sectors>>24)&0xFF);

        // BPB_ExtFlags
        fs->currentsector.sector[40] = 0;
        fs->currentsector.sector[41] = 0;

        // BPB_FSVer
        fs->currentsector.sector[42] = 0;
        fs->currentsector.sector[43] = 0;

        // BPB_RootClus
        fs->currentsector.sector[44] = (uint8)((fs->rootdir_first_cluster>>0)&0xFF);
        fs->currentsector.sector[45] = (uint8)((fs->rootdir_first_cluster>>8)&0xFF);
        fs->currentsector.sector[46] = (uint8)((fs->rootdir_first_cluster>>16)&0xFF);
        fs->currentsector.sector[47] = (uint8)((fs->rootdir_first_cluster>>24)&0xFF);

        // BPB_FSInfo
        fs->currentsector.sector[48] = (uint8)((fs->fs_info_sector>>0)&0xFF);
        fs->currentsector.sector[49] = (uint8)((fs->fs_info_sector>>8)&0xFF);

        // BPB_BkBootSec
        fs->currentsector.sector[50] = 6;
        fs->currentsector.sector[51] = 0;

        // Drive number
        fs->currentsector.sector[64] = 0x00;

        // Boot signature
        fs->currentsector.sector[66] = 0x29;

        // Volume ID
        fs->currentsector.sector[67] = 0x12;
        fs->currentsector.sector[68] = 0x34;
        fs->currentsector.sector[69] = 0x56;
        fs->currentsector.sector[70] = 0x78;

        // Volume name
        for (i=0;i<11;i++)
        {
            if (i < (int)strlen(name))
                fs->currentsector.sector[i+71] = name[i];
            else
                fs->currentsector.sector[i+71] = ' ';
        }

        // File sys type
        fs->currentsector.sector[82] = 'F';
        fs->currentsector.sector[83] = 'A';
        fs->currentsector.sector[84] = 'T';
        fs->currentsector.sector[85] = '3';
        fs->currentsector.sector[86] = '2';
        fs->currentsector.sector[87] = ' ';
        fs->currentsector.sector[88] = ' ';
        fs->currentsector.sector[89] = ' ';

        // Signature
        fs->currentsector.sector[510] = 0x55;
        fs->currentsector.sector[511] = 0xAA;
    }

    if (fs->disk_io.write_media(boot_sector_lba, fs->currentsector.sector, 1))
        return 1;
    else
        return 0;
}
//-----------------------------------------------------------------------------
// fatfs_create_fsinfo_sector: Create the FSInfo sector (FAT32)
//-----------------------------------------------------------------------------
static int fatfs_create_fsinfo_sector(struct fatfs *fs, uint32 sector_lba)
{
    // Zero sector initially
    memset(fs->currentsector.sector, 0, FAT_SECTOR_SIZE);

    // FSI_LeadSig
    fs->currentsector.sector[0] = 0x52;
    fs->currentsector.sector[1] = 0x52;
    fs->currentsector.sector[2] = 0x61;
    fs->currentsector.sector[3] = 0x41;

    // FSI_StrucSig
    fs->currentsector.sector[484] = 0x72;
    fs->currentsector.sector[485] = 0x72;
    fs->currentsector.sector[486] = 0x41;
    fs->currentsector.sector[487] = 0x61;

    // FSI_Free_Count
    fs->currentsector.sector[488] = 0xFF;
    fs->currentsector.sector[489] = 0xFF;
    fs->currentsector.sector[490] = 0xFF;
    fs->currentsector.sector[491] = 0xFF;

    // FSI_Nxt_Free
    fs->currentsector.sector[492] = 0xFF;
    fs->currentsector.sector[493] = 0xFF;
    fs->currentsector.sector[494] = 0xFF;
    fs->currentsector.sector[495] = 0xFF;

    // Signature
    fs->currentsector.sector[510] = 0x55;
    fs->currentsector.sector[511] = 0xAA;

    if (fs->disk_io.write_media(sector_lba, fs->currentsector.sector, 1))
        return 1;
    else
        return 0;
}
//-----------------------------------------------------------------------------
// fatfs_erase_fat: Erase FAT table using fs details in fs struct
//-----------------------------------------------------------------------------
static int fatfs_erase_fat(struct fatfs *fs, int is_fat32)
{
    uint32 i;

    // Zero sector initially
    memset(fs->currentsector.sector, 0, FAT_SECTOR_SIZE);

    // Initialise default allocate / reserved clusters
    if (!is_fat32)
    {
        SET_16BIT_WORD(fs->currentsector.sector, 0, 0xFFF8);
        SET_16BIT_WORD(fs->currentsector.sector, 2, 0xFFFF);
    }
    else
    {
        SET_32BIT_WORD(fs->currentsector.sector, 0, 0x0FFFFFF8);
        SET_32BIT_WORD(fs->currentsector.sector, 4, 0xFFFFFFFF);
        SET_32BIT_WORD(fs->currentsector.sector, 8, 0x0FFFFFFF);
    }

    if (!fs->disk_io.write_media(fs->fat_begin_lba + 0, fs->currentsector.sector, 1))
        return 0;

    // Zero remaining FAT sectors
    memset(fs->currentsector.sector, 0, FAT_SECTOR_SIZE);
    for (i=1;i<fs->fat_sectors*fs->num_of_fats;i++)
        if (!fs->disk_io.write_media(fs->fat_begin_lba + i, fs->currentsector.sector, 1))
            return 0;

    return 1;
}
//-----------------------------------------------------------------------------
// fatfs_format_fat16: Format a FAT16 partition
//-----------------------------------------------------------------------------
int fatfs_format_fat16(struct fatfs *fs, uint32 volume_sectors, const char *name)
{
    fs->currentsector.address = FAT32_INVALID_CLUSTER;
    fs->currentsector.dirty = 0;

    fs->next_free_cluster = 0; // Invalid

    fatfs_fat_init(fs);

    // Make sure we have read + write functions
    if (!fs->disk_io.read_media || !fs->disk_io.write_media)
        return FAT_INIT_MEDIA_ACCESS_ERROR;

    // Volume is FAT16
    fs->fat_type = FAT_TYPE_16;

    // Not valid for FAT16
    fs->fs_info_sector = 0;
    fs->rootdir_first_cluster = 0;

    // Sector 0: Boot sector
    // NOTE: We don't need an MBR, it is a waste of a good sector!
    fs->lba_begin = 0;
    if (!fatfs_create_boot_sector(fs, fs->lba_begin, volume_sectors, name, 0))
        return 0;

    // For FAT16 (which this may be), rootdir_first_cluster is actuall rootdir_first_sector
    fs->rootdir_first_sector = fs->reserved_sectors + (fs->num_of_fats * fs->fat_sectors);
    fs->rootdir_sectors = ((fs->root_entry_count * 32) + (FAT_SECTOR_SIZE - 1)) / FAT_SECTOR_SIZE;

    // First FAT LBA address
    fs->fat_begin_lba = fs->lba_begin + fs->reserved_sectors;

    // The address of the first data cluster on this volume
    fs->cluster_begin_lba = fs->fat_begin_lba + (fs->num_of_fats * fs->fat_sectors);

    // Initialise FAT sectors
    if (!fatfs_erase_fat(fs, 0))
        return 0;

    // Erase Root directory
    if (!fatfs_erase_sectors(fs, fs->lba_begin + fs->rootdir_first_sector, fs->rootdir_sectors))
        return 0;

    return 1;
}
//-----------------------------------------------------------------------------
// fatfs_format_fat32: Format a FAT32 partition
//-----------------------------------------------------------------------------
int fatfs_format_fat32(struct fatfs *fs, uint32 volume_sectors, const char *name)
{
    fs->currentsector.address = FAT32_INVALID_CLUSTER;
    fs->currentsector.dirty = 0;

    fs->next_free_cluster = 0; // Invalid

    fatfs_fat_init(fs);

    // Make sure we have read + write functions
    if (!fs->disk_io.read_media || !fs->disk_io.write_media)
        return FAT_INIT_MEDIA_ACCESS_ERROR;

    // Volume is FAT32
    fs->fat_type = FAT_TYPE_32;

    // Basic defaults for normal FAT32 partitions
    fs->fs_info_sector = 1;
    fs->rootdir_first_cluster = 2;

    // Sector 0: Boot sector
    // NOTE: We don't need an MBR, it is a waste of a good sector!
    fs->lba_begin = 0;
    if (!fatfs_create_boot_sector(fs, fs->lba_begin, volume_sectors, name, 1))
        return 0;

    // First FAT LBA address
    fs->fat_begin_lba = fs->lba_begin + fs->reserved_sectors;

    // The address of the first data cluster on this volume
    fs->cluster_begin_lba = fs->fat_begin_lba + (fs->num_of_fats * fs->fat_sectors);

    // Initialise FSInfo sector
    if (!fatfs_create_fsinfo_sector(fs, fs->fs_info_sector))
        return 0;

    // Initialise FAT sectors
    if (!fatfs_erase_fat(fs, 1))
        return 0;

    // Erase Root directory
    if (!fatfs_erase_sectors(fs, fatfs_lba_of_cluster(fs, fs->rootdir_first_cluster), fs->sectors_per_cluster))
        return 0;

    return 1;
}
//-----------------------------------------------------------------------------
// fatfs_format: Format a partition with either FAT16 or FAT32 based on size
//-----------------------------------------------------------------------------
int fatfs_format(struct fatfs *fs, uint32 volume_sectors, const char *name)
{
    // 2GB - 32K limit for safe behaviour for FAT16
    if (volume_sectors <= 4194304)
        return fatfs_format_fat16(fs, volume_sectors, name);
    else
        return fatfs_format_fat32(fs, volume_sectors, name);
}
#endif /*FATFS_INC_FORMAT_SUPPORT*/
