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
#include <stdlib.h>
#include <string.h>
#include "fat_defs.h"
#include "fat_access.h"
#include "fat_table.h"
#include "fat_write.h"
#include "fat_misc.h"
#include "fat_string.h"
#include "fat_filelib.h"
#include "fat_cache.h"

//-----------------------------------------------------------------------------
// Locals
//-----------------------------------------------------------------------------
static FL_FILE            _files[FATFS_MAX_OPEN_FILES];
static int                _filelib_init = 0;
static int                _filelib_valid = 0;
static struct fatfs       _fs;
static struct fat_list    _open_file_list;
static struct fat_list    _free_file_list;

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------

// Macro for checking if file lib is initialised
#define CHECK_FL_INIT()     { if (_filelib_init==0) fl_init(); }

#define FL_LOCK(a)          do { if ((a)->fl_lock) (a)->fl_lock(); } while (0)
#define FL_UNLOCK(a)        do { if ((a)->fl_unlock) (a)->fl_unlock(); } while (0)

//-----------------------------------------------------------------------------
// Local Functions
//-----------------------------------------------------------------------------
static void                _fl_init();

//-----------------------------------------------------------------------------
// _allocate_file: Find a slot in the open files buffer for a new file
//-----------------------------------------------------------------------------
static FL_FILE* _allocate_file(void)
{
    // Allocate free file
    struct fat_node *node = fat_list_pop_head(&_free_file_list);

    // Add to open list
    if (node)
        fat_list_insert_last(&_open_file_list, node);

    return fat_list_entry(node, FL_FILE, list_node);
}
//-----------------------------------------------------------------------------
// _check_file_open: Returns true if the file is already open
//-----------------------------------------------------------------------------
static int _check_file_open(FL_FILE* file)
{
    struct fat_node *node;

    // Compare open files
    fat_list_for_each(&_open_file_list, node)
    {
        FL_FILE* openFile = fat_list_entry(node, FL_FILE, list_node);

        // If not the current file
        if (openFile != file)
        {
            // Compare path and name
            if ( (fatfs_compare_names(openFile->path,file->path)) && (fatfs_compare_names(openFile->filename,file->filename)) )
                return 1;
        }
    }

    return 0;
}
//-----------------------------------------------------------------------------
// _free_file: Free open file handle
//-----------------------------------------------------------------------------
static void _free_file(FL_FILE* file)
{
    // Remove from open list
    fat_list_remove(&_open_file_list, &file->list_node);

    // Add to free list
    fat_list_insert_last(&_free_file_list, &file->list_node);
}

//-----------------------------------------------------------------------------
//                                Low Level
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// _open_directory: Cycle through path string to find the start cluster
// address of the highest subdir.
//-----------------------------------------------------------------------------
static int _open_directory(char *path, uint32 *pathCluster)
{
    int levels;
    int sublevel;
    char currentfolder[FATFS_MAX_LONG_FILENAME];
    struct fat_dir_entry sfEntry;
    uint32 startcluster;

    // Set starting cluster to root cluster
    startcluster = fatfs_get_root_cluster(&_fs);

    // Find number of levels
    levels = fatfs_total_path_levels(path);

    // Cycle through each level and get the start sector
    for (sublevel=0;sublevel<(levels+1);sublevel++)
    {
        if (fatfs_get_substring(path, sublevel, currentfolder, sizeof(currentfolder)) == -1)
            return 0;

        // Find clusteraddress for folder (currentfolder)
        if (fatfs_get_file_entry(&_fs, startcluster, currentfolder,&sfEntry))
        {
            // Check entry is folder
            if (fatfs_entry_is_dir(&sfEntry))
                startcluster = ((FAT_HTONS((uint32)sfEntry.FstClusHI))<<16) + FAT_HTONS(sfEntry.FstClusLO);
            else
                return 0;
        }
        else
            return 0;
    }

    *pathCluster = startcluster;
    return 1;
}
//-----------------------------------------------------------------------------
// _create_directory: Cycle through path string and create the end directory
//-----------------------------------------------------------------------------
#if FATFS_INC_WRITE_SUPPORT
static int _create_directory(char *path)
{
    FL_FILE* file;
    struct fat_dir_entry sfEntry;
    char shortFilename[FAT_SFN_SIZE_FULL];
    int tailNum = 0;
    int i;

    // Allocate a new file handle
    file = _allocate_file();
    if (!file)
        return 0;

    // Clear filename
    memset(file->path, '\0', sizeof(file->path));
    memset(file->filename, '\0', sizeof(file->filename));

    // Split full path into filename and directory path
    if (fatfs_split_path((char*)path, file->path, sizeof(file->path), file->filename, sizeof(file->filename)) == -1)
    {
        _free_file(file);
        return 0;
    }

    // Check if file already open
    if (_check_file_open(file))
    {
        _free_file(file);
        return 0;
    }

    // If file is in the root dir
    if (file->path[0] == 0)
        file->parentcluster = fatfs_get_root_cluster(&_fs);
    else
    {
        // Find parent directory start cluster
        if (!_open_directory(file->path, &file->parentcluster))
        {
            _free_file(file);
            return 0;
        }
    }

    // Check if same filename exists in directory
    if (fatfs_get_file_entry(&_fs, file->parentcluster, file->filename,&sfEntry) == 1)
    {
        _free_file(file);
        return 0;
    }

    file->startcluster = 0;

    // Create the file space for the folder (at least one clusters worth!)
    if (!fatfs_allocate_free_space(&_fs, 1, &file->startcluster, 1))
    {
        _free_file(file);
        return 0;
    }

    // Erase new directory cluster
    memset(file->file_data_sector, 0x00, FAT_SECTOR_SIZE);
    for (i=0;i<_fs.sectors_per_cluster;i++)
    {
        if (!fatfs_write_sector(&_fs, file->startcluster, i, file->file_data_sector))
        {
            _free_file(file);
            return 0;
        }
    }

#if FATFS_INC_LFN_SUPPORT

    // Generate a short filename & tail
    tailNum = 0;
    do
    {
        // Create a standard short filename (without tail)
        fatfs_lfn_create_sfn(shortFilename, file->filename);

        // If second hit or more, generate a ~n tail
        if (tailNum != 0)
            fatfs_lfn_generate_tail((char*)file->shortfilename, shortFilename, tailNum);
        // Try with no tail if first entry
        else
            memcpy(file->shortfilename, shortFilename, FAT_SFN_SIZE_FULL);

        // Check if entry exists already or not
        if (fatfs_sfn_exists(&_fs, file->parentcluster, (char*)file->shortfilename) == 0)
            break;

        tailNum++;
    }
    while (tailNum < 9999);

    // We reached the max number of duplicate short file names (unlikely!)
    if (tailNum == 9999)
    {
        // Delete allocated space
        fatfs_free_cluster_chain(&_fs, file->startcluster);

        _free_file(file);
        return 0;
    }
#else
    // Create a standard short filename (without tail)
    if (!fatfs_lfn_create_sfn(shortFilename, file->filename))
    {
        // Delete allocated space
        fatfs_free_cluster_chain(&_fs, file->startcluster);

        _free_file(file);
        return 0;
    }

    // Copy to SFN space
    memcpy(file->shortfilename, shortFilename, FAT_SFN_SIZE_FULL);

    // Check if entry exists already
    if (fatfs_sfn_exists(&_fs, file->parentcluster, (char*)file->shortfilename))
    {
        // Delete allocated space
        fatfs_free_cluster_chain(&_fs, file->startcluster);

        _free_file(file);
        return 0;
    }
#endif

    // Add file to disk
    if (!fatfs_add_file_entry(&_fs, file->parentcluster, (char*)file->filename, (char*)file->shortfilename, file->startcluster, 0, 1))
    {
        // Delete allocated space
        fatfs_free_cluster_chain(&_fs, file->startcluster);

        _free_file(file);
        return 0;
    }

    // General
    file->filelength = 0;
    file->bytenum = 0;
    file->file_data_address = 0xFFFFFFFF;
    file->file_data_dirty = 0;
    file->filelength_changed = 0;

    // Quick lookup for next link in the chain
    file->last_fat_lookup.ClusterIdx = 0xFFFFFFFF;
    file->last_fat_lookup.CurrentCluster = 0xFFFFFFFF;

    fatfs_fat_purge(&_fs);

    _free_file(file);
    return 1;
}
#endif
//-----------------------------------------------------------------------------
// _open_file: Open a file for reading
//-----------------------------------------------------------------------------
static FL_FILE* _open_file(const char *path)
{
    FL_FILE* file;
    struct fat_dir_entry sfEntry;

    // Allocate a new file handle
    file = _allocate_file();
    if (!file)
        return NULL;

    // Clear filename
    memset(file->path, '\0', sizeof(file->path));
    memset(file->filename, '\0', sizeof(file->filename));

    // Split full path into filename and directory path
    if (fatfs_split_path((char*)path, file->path, sizeof(file->path), file->filename, sizeof(file->filename)) == -1)
    {
        _free_file(file);
        return NULL;
    }

    // Check if file already open
    if (_check_file_open(file))
    {
        _free_file(file);
        return NULL;
    }

    // If file is in the root dir
    if (file->path[0]==0)
        file->parentcluster = fatfs_get_root_cluster(&_fs);
    else
    {
        // Find parent directory start cluster
        if (!_open_directory(file->path, &file->parentcluster))
        {
            _free_file(file);
            return NULL;
        }
    }

    // Using dir cluster address search for filename
    if (fatfs_get_file_entry(&_fs, file->parentcluster, file->filename,&sfEntry))
        // Make sure entry is file not dir!
        if (fatfs_entry_is_file(&sfEntry))
        {
            // Initialise file details
            memcpy(file->shortfilename, sfEntry.Name, FAT_SFN_SIZE_FULL);
            file->filelength = FAT_HTONL(sfEntry.FileSize);
            file->bytenum = 0;
            file->startcluster = ((FAT_HTONS((uint32)sfEntry.FstClusHI))<<16) + FAT_HTONS(sfEntry.FstClusLO);
            file->file_data_address = 0xFFFFFFFF;
            file->file_data_dirty = 0;
            file->filelength_changed = 0;

            // Quick lookup for next link in the chain
            file->last_fat_lookup.ClusterIdx = 0xFFFFFFFF;
            file->last_fat_lookup.CurrentCluster = 0xFFFFFFFF;

            fatfs_cache_init(&_fs, file);

            fatfs_fat_purge(&_fs);

            return file;
        }

    _free_file(file);
    return NULL;
}
//-----------------------------------------------------------------------------
// _create_file: Create a new file
//-----------------------------------------------------------------------------
#if FATFS_INC_WRITE_SUPPORT
static FL_FILE* _create_file(const char *filename)
{
    FL_FILE* file;
    struct fat_dir_entry sfEntry;
    char shortFilename[FAT_SFN_SIZE_FULL];
    int tailNum = 0;

    // No write access?
    if (!_fs.disk_io.write_media)
        return NULL;

    // Allocate a new file handle
    file = _allocate_file();
    if (!file)
        return NULL;

    // Clear filename
    memset(file->path, '\0', sizeof(file->path));
    memset(file->filename, '\0', sizeof(file->filename));

    // Split full path into filename and directory path
    if (fatfs_split_path((char*)filename, file->path, sizeof(file->path), file->filename, sizeof(file->filename)) == -1)
    {
        _free_file(file);
        return NULL;
    }

    // Check if file already open
    if (_check_file_open(file))
    {
        _free_file(file);
        return NULL;
    }

    // If file is in the root dir
    if (file->path[0] == 0)
        file->parentcluster = fatfs_get_root_cluster(&_fs);
    else
    {
        // Find parent directory start cluster
        if (!_open_directory(file->path, &file->parentcluster))
        {
            _free_file(file);
            return NULL;
        }
    }

    // Check if same filename exists in directory
    if (fatfs_get_file_entry(&_fs, file->parentcluster, file->filename,&sfEntry) == 1)
    {
        _free_file(file);
        return NULL;
    }

    file->startcluster = 0;

    // Create the file space for the file (at least one clusters worth!)
    if (!fatfs_allocate_free_space(&_fs, 1, &file->startcluster, 1))
    {
        _free_file(file);
        return NULL;
    }

#if FATFS_INC_LFN_SUPPORT
    // Generate a short filename & tail
    tailNum = 0;
    do
    {
        // Create a standard short filename (without tail)
        fatfs_lfn_create_sfn(shortFilename, file->filename);

        // If second hit or more, generate a ~n tail
        if (tailNum != 0)
            fatfs_lfn_generate_tail((char*)file->shortfilename, shortFilename, tailNum);
        // Try with no tail if first entry
        else
            memcpy(file->shortfilename, shortFilename, FAT_SFN_SIZE_FULL);

        // Check if entry exists already or not
        if (fatfs_sfn_exists(&_fs, file->parentcluster, (char*)file->shortfilename) == 0)
            break;

        tailNum++;
    }
    while (tailNum < 9999);

    // We reached the max number of duplicate short file names (unlikely!)
    if (tailNum == 9999)
    {
        // Delete allocated space
        fatfs_free_cluster_chain(&_fs, file->startcluster);

        _free_file(file);
        return NULL;
    }
#else
    // Create a standard short filename (without tail)
    if (!fatfs_lfn_create_sfn(shortFilename, file->filename))
    {
        // Delete allocated space
        fatfs_free_cluster_chain(&_fs, file->startcluster);

        _free_file(file);
        return NULL;
    }

    // Copy to SFN space
    memcpy(file->shortfilename, shortFilename, FAT_SFN_SIZE_FULL);

    // Check if entry exists already
    if (fatfs_sfn_exists(&_fs, file->parentcluster, (char*)file->shortfilename))
    {
        // Delete allocated space
        fatfs_free_cluster_chain(&_fs, file->startcluster);

        _free_file(file);
        return NULL;
    }
#endif

    // Add file to disk
    if (!fatfs_add_file_entry(&_fs, file->parentcluster, (char*)file->filename, (char*)file->shortfilename, file->startcluster, 0, 0))
    {
        // Delete allocated space
        fatfs_free_cluster_chain(&_fs, file->startcluster);

        _free_file(file);
        return NULL;
    }

    // General
    file->filelength = 0;
    file->bytenum = 0;
    file->file_data_address = 0xFFFFFFFF;
    file->file_data_dirty = 0;
    file->filelength_changed = 0;

    // Quick lookup for next link in the chain
    file->last_fat_lookup.ClusterIdx = 0xFFFFFFFF;
    file->last_fat_lookup.CurrentCluster = 0xFFFFFFFF;

    fatfs_cache_init(&_fs, file);

    fatfs_fat_purge(&_fs);

    return file;
}
#endif
//-----------------------------------------------------------------------------
// _read_sectors: Read sector(s) from disk to file
//-----------------------------------------------------------------------------
static uint32 _read_sectors(FL_FILE* file, uint32 offset, uint8 *buffer, uint32 count)
{
    uint32 Sector = 0;
    uint32 ClusterIdx = 0;
    uint32 Cluster = 0;
    uint32 i;
    uint32 lba;

    // Find cluster index within file & sector with cluster
    ClusterIdx = offset / _fs.sectors_per_cluster;
    Sector = offset - (ClusterIdx * _fs.sectors_per_cluster);

    // Limit number of sectors read to the number remaining in this cluster
    if ((Sector + count) > _fs.sectors_per_cluster)
        count = _fs.sectors_per_cluster - Sector;

    // Quick lookup for next link in the chain
    if (ClusterIdx == file->last_fat_lookup.ClusterIdx)
        Cluster = file->last_fat_lookup.CurrentCluster;
    // Else walk the chain
    else
    {
        // Starting from last recorded cluster?
        if (ClusterIdx && ClusterIdx == file->last_fat_lookup.ClusterIdx + 1)
        {
            i = file->last_fat_lookup.ClusterIdx;
            Cluster = file->last_fat_lookup.CurrentCluster;
        }
        // Start searching from the beginning..
        else
        {
            // Set start of cluster chain to initial value
            i = 0;
            Cluster = file->startcluster;
        }

        // Follow chain to find cluster to read
        for ( ;i<ClusterIdx; i++)
        {
            uint32 nextCluster;

            // Does the entry exist in the cache?
            if (!fatfs_cache_get_next_cluster(&_fs, file, i, &nextCluster))
            {
                // Scan file linked list to find next entry
                nextCluster = fatfs_find_next_cluster(&_fs, Cluster);

                // Push entry into cache
                fatfs_cache_set_next_cluster(&_fs, file, i, nextCluster);
            }

            Cluster = nextCluster;
        }

        // Record current cluster lookup details (if valid)
        if (Cluster != FAT32_LAST_CLUSTER)
        {
            file->last_fat_lookup.CurrentCluster = Cluster;
            file->last_fat_lookup.ClusterIdx = ClusterIdx;
        }
    }

    // If end of cluster chain then return false
    if (Cluster == FAT32_LAST_CLUSTER)
        return 0;

    // Calculate sector address
    lba = fatfs_lba_of_cluster(&_fs, Cluster) + Sector;

    // Read sector of file
    if (fatfs_sector_read(&_fs, lba, buffer, count))
        return count;
    else
        return 0;
}

//-----------------------------------------------------------------------------
//                                External API
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// fl_init: Initialise library
//-----------------------------------------------------------------------------
void fl_init(void)
{
    int i;

    fat_list_init(&_free_file_list);
    fat_list_init(&_open_file_list);

    // Add all file objects to free list
    for (i=0;i<FATFS_MAX_OPEN_FILES;i++)
        fat_list_insert_last(&_free_file_list, &_files[i].list_node);

    _filelib_init = 1;
}
//-----------------------------------------------------------------------------
// fl_attach_locks:
//-----------------------------------------------------------------------------
void fl_attach_locks(void (*lock)(void), void (*unlock)(void))
{
    _fs.fl_lock = lock;
    _fs.fl_unlock = unlock;
}
//-----------------------------------------------------------------------------
// fl_attach_media:
//-----------------------------------------------------------------------------
int fl_attach_media(fn_diskio_read rd, fn_diskio_write wr)
{
    int res;

    // If first call to library, initialise
    CHECK_FL_INIT();

    _fs.disk_io.read_media = rd;
    _fs.disk_io.write_media = wr;

    // Initialise FAT parameters
    if ((res = fatfs_init(&_fs)) != FAT_INIT_OK)
    {
        FAT_PRINTF(("FAT_FS: Error could not load FAT details (%d)!\r\n", res));
        return res;
    }

    _filelib_valid = 1;
    return FAT_INIT_OK;
}
//-----------------------------------------------------------------------------
// fl_shutdown: Call before shutting down system
//-----------------------------------------------------------------------------
void fl_shutdown(void)
{
    // If first call to library, initialise
    CHECK_FL_INIT();

    FL_LOCK(&_fs);
    fatfs_fat_purge(&_fs);
    FL_UNLOCK(&_fs);
}
//-----------------------------------------------------------------------------
// fopen: Open or Create a file for reading or writing
//-----------------------------------------------------------------------------
void* fl_fopen(const char *path, const char *mode)
{
    int i;
    FL_FILE* file;
    uint8 flags = 0;

    // If first call to library, initialise
    CHECK_FL_INIT();

    if (!_filelib_valid)
        return NULL;

    if (!path || !mode)
        return NULL;

    // Supported Modes:
    // "r" Open a file for reading.
    //        The file must exist.
    // "w" Create an empty file for writing.
    //        If a file with the same name already exists its content is erased and the file is treated as a new empty file.
    // "a" Append to a file.
    //        Writing operations append data at the end of the file.
    //        The file is created if it does not exist.
    // "r+" Open a file for update both reading and writing.
    //        The file must exist.
    // "w+" Create an empty file for both reading and writing.
    //        If a file with the same name already exists its content is erased and the file is treated as a new empty file.
    // "a+" Open a file for reading and appending.
    //        All writing operations are performed at the end of the file, protecting the previous content to be overwritten.
    //        You can reposition (fseek, rewind) the internal pointer to anywhere in the file for reading, but writing operations
    //        will move it back to the end of file.
    //        The file is created if it does not exist.

    for (i=0;i<(int)strlen(mode);i++)
    {
        switch (mode[i])
        {
        case 'r':
        case 'R':
            flags |= FILE_READ;
            break;
        case 'w':
        case 'W':
            flags |= FILE_WRITE;
            flags |= FILE_ERASE;
            flags |= FILE_CREATE;
            break;
        case 'a':
        case 'A':
            flags |= FILE_WRITE;
            flags |= FILE_APPEND;
            flags |= FILE_CREATE;
            break;
        case '+':
            if (flags & FILE_READ)
                flags |= FILE_WRITE;
            else if (flags & FILE_WRITE)
            {
                flags |= FILE_READ;
                flags |= FILE_ERASE;
                flags |= FILE_CREATE;
            }
            else if (flags & FILE_APPEND)
            {
                flags |= FILE_READ;
                flags |= FILE_WRITE;
                flags |= FILE_APPEND;
                flags |= FILE_CREATE;
            }
            break;
        case 'b':
        case 'B':
            flags |= FILE_BINARY;
            break;
        }
    }

    file = NULL;

#if FATFS_INC_WRITE_SUPPORT == 0
    // No write support!
    flags &= ~(FILE_CREATE | FILE_WRITE | FILE_APPEND);
#endif

    // No write access - remove write/modify flags
    if (!_fs.disk_io.write_media)
        flags &= ~(FILE_CREATE | FILE_WRITE | FILE_APPEND);

    FL_LOCK(&_fs);

    // Read
    if (flags & FILE_READ)
        file = _open_file(path);

    // Create New
#if FATFS_INC_WRITE_SUPPORT
    if (!file && (flags & FILE_CREATE))
        file = _create_file(path);
#endif

    // Write Existing (and not open due to read or create)
    if (!(flags & FILE_READ))
        if ((flags & FILE_CREATE) && !file)
            if (flags & (FILE_WRITE | FILE_APPEND))
                file = _open_file(path);

    if (file)
        file->flags = flags;

    FL_UNLOCK(&_fs);
    return file;
}
//-----------------------------------------------------------------------------
// _write_sectors: Write sector(s) to disk
//-----------------------------------------------------------------------------
#if FATFS_INC_WRITE_SUPPORT
static uint32 _write_sectors(FL_FILE* file, uint32 offset, uint8 *buf, uint32 count)
{
    uint32 SectorNumber = 0;
    uint32 ClusterIdx = 0;
    uint32 Cluster = 0;
    uint32 LastCluster = FAT32_LAST_CLUSTER;
    uint32 i;
    uint32 lba;
    uint32 TotalWriteCount = count;

    // Find values for Cluster index & sector within cluster
    ClusterIdx = offset / _fs.sectors_per_cluster;
    SectorNumber = offset - (ClusterIdx * _fs.sectors_per_cluster);

    // Limit number of sectors written to the number remaining in this cluster
    if ((SectorNumber + count) > _fs.sectors_per_cluster)
        count = _fs.sectors_per_cluster - SectorNumber;

    // Quick lookup for next link in the chain
    if (ClusterIdx == file->last_fat_lookup.ClusterIdx)
        Cluster = file->last_fat_lookup.CurrentCluster;
    // Else walk the chain
    else
    {
        // Starting from last recorded cluster?
        if (ClusterIdx && ClusterIdx == file->last_fat_lookup.ClusterIdx + 1)
        {
            i = file->last_fat_lookup.ClusterIdx;
            Cluster = file->last_fat_lookup.CurrentCluster;
        }
        // Start searching from the beginning..
        else
        {
            // Set start of cluster chain to initial value
            i = 0;
            Cluster = file->startcluster;
        }

        // Follow chain to find cluster to read
        for ( ;i<ClusterIdx; i++)
        {
            uint32 nextCluster;

            // Does the entry exist in the cache?
            if (!fatfs_cache_get_next_cluster(&_fs, file, i, &nextCluster))
            {
                // Scan file linked list to find next entry
                nextCluster = fatfs_find_next_cluster(&_fs, Cluster);

                // Push entry into cache
                fatfs_cache_set_next_cluster(&_fs, file, i, nextCluster);
            }

            LastCluster = Cluster;
            Cluster = nextCluster;

            // Dont keep following a dead end
            if (Cluster == FAT32_LAST_CLUSTER)
                break;
        }

        // If we have reached the end of the chain, allocate more!
        if (Cluster == FAT32_LAST_CLUSTER)
        {
            // Add some more cluster(s) to the last good cluster chain
            if (!fatfs_add_free_space(&_fs, &LastCluster,  (TotalWriteCount + _fs.sectors_per_cluster -1) / _fs.sectors_per_cluster))
                return 0;

            Cluster = LastCluster;
        }

        // Record current cluster lookup details
        file->last_fat_lookup.CurrentCluster = Cluster;
        file->last_fat_lookup.ClusterIdx = ClusterIdx;
    }

    // Calculate write address
    lba = fatfs_lba_of_cluster(&_fs, Cluster) + SectorNumber;

    if (fatfs_sector_write(&_fs, lba, buf, count))
        return count;
    else
        return 0;
}
#endif
//-----------------------------------------------------------------------------
// fl_fflush: Flush un-written data to the file
//-----------------------------------------------------------------------------
int fl_fflush(void *f)
{
#if FATFS_INC_WRITE_SUPPORT
    FL_FILE *file = (FL_FILE *)f;

    // If first call to library, initialise
    CHECK_FL_INIT();

    if (file)
    {
        FL_LOCK(&_fs);

        // If some write data still in buffer
        if (file->file_data_dirty)
        {
            // Write back current sector before loading next
            if (_write_sectors(file, file->file_data_address, file->file_data_sector, 1))
                file->file_data_dirty = 0;
        }

        FL_UNLOCK(&_fs);
    }
#endif
    return 0;
}
//-----------------------------------------------------------------------------
// fl_fclose: Close an open file
//-----------------------------------------------------------------------------
void fl_fclose(void *f)
{
    FL_FILE *file = (FL_FILE *)f;

    // If first call to library, initialise
    CHECK_FL_INIT();

    if (file)
    {
        FL_LOCK(&_fs);

        // Flush un-written data to file
        fl_fflush(f);

        // File size changed?
        if (file->filelength_changed)
        {
#if FATFS_INC_WRITE_SUPPORT
            // Update filesize in directory
            fatfs_update_file_length(&_fs, file->parentcluster, (char*)file->shortfilename, file->filelength);
#endif
            file->filelength_changed = 0;
        }

        file->bytenum = 0;
        file->filelength = 0;
        file->startcluster = 0;
        file->file_data_address = 0xFFFFFFFF;
        file->file_data_dirty = 0;
        file->filelength_changed = 0;

        // Free file handle
        _free_file(file);

        fatfs_fat_purge(&_fs);

        FL_UNLOCK(&_fs);
    }
}
//-----------------------------------------------------------------------------
// fl_fgetc: Get a character in the stream
//-----------------------------------------------------------------------------
int fl_fgetc(void *f)
{
    int res;
    uint8 data = 0;

    res = fl_fread(&data, 1, 1, f);
    if (res == 1)
        return (int)data;
    else
        return res;
}
//-----------------------------------------------------------------------------
// fl_fgets: Get a string from a stream
//-----------------------------------------------------------------------------
char *fl_fgets(char *s, int n, void *f)
{
    int idx = 0;

    // Space for null terminator?
    if (n > 0)
    {
        // While space (+space for null terminator)
        while (idx < (n-1))
        {
            int ch = fl_fgetc(f);

            // EOF / Error?
            if (ch < 0)
                break;

            // Store character read from stream
            s[idx++] = (char)ch;

            // End of line?
            if (ch == '\n')
                break;
        }

        if (idx > 0)
            s[idx] = '\0';
    }

    return (idx > 0) ? s : 0;
}
//-----------------------------------------------------------------------------
// fl_fread: Read a block of data from the file
//-----------------------------------------------------------------------------
int fl_fread(void * buffer, int size, int length, void *f )
{
    uint32 sector;
    uint32 offset;
    int copyCount;
    int count = size * length;
    int bytesRead = 0;

    FL_FILE *file = (FL_FILE *)f;

    // If first call to library, initialise
    CHECK_FL_INIT();

    if (buffer==NULL || file==NULL)
        return -1;

    // No read permissions
    if (!(file->flags & FILE_READ))
        return -1;

    // Nothing to be done
    if (!count)
        return 0;

    // Check if read starts past end of file
    if (file->bytenum >= file->filelength)
        return -1;

    // Limit to file size
    if ( (file->bytenum + count) > file->filelength )
        count = file->filelength - file->bytenum;

    // Calculate start sector
    sector = file->bytenum / FAT_SECTOR_SIZE;

    // Offset to start copying data from first sector
    offset = file->bytenum % FAT_SECTOR_SIZE;

    while (bytesRead < count)
    {
        // Read whole sector, read from media directly into target buffer
        if ((offset == 0) && ((count - bytesRead) >= FAT_SECTOR_SIZE))
        {
            // Read as many sectors as possible into target buffer
            uint32 sectorsRead = _read_sectors(file, sector, (uint8*)((uint8*)buffer + bytesRead), (count - bytesRead) / FAT_SECTOR_SIZE);
            if (sectorsRead)
            {
                // We have upto one sector to copy
                copyCount = FAT_SECTOR_SIZE * sectorsRead;

                // Move onto next sector and reset copy offset
                sector+= sectorsRead;
                offset = 0;
            }
            else
                break;
        }
        else
        {
            // Do we need to re-read the sector?
            if (file->file_data_address != sector)
            {
                // Flush un-written data to file
                if (file->file_data_dirty)
                    fl_fflush(file);

                // Get LBA of sector offset within file
                if (!_read_sectors(file, sector, file->file_data_sector, 1))
                    // Read failed - out of range (probably)
                    break;

                file->file_data_address = sector;
                file->file_data_dirty = 0;
            }

            // We have upto one sector to copy
            copyCount = FAT_SECTOR_SIZE - offset;

            // Only require some of this sector?
            if (copyCount > (count - bytesRead))
                copyCount = (count - bytesRead);

            // Copy to application buffer
            memcpy( (uint8*)((uint8*)buffer + bytesRead), (uint8*)(file->file_data_sector + offset), copyCount);

            // Move onto next sector and reset copy offset
            sector++;
            offset = 0;
        }

        // Increase total read count
        bytesRead += copyCount;

        // Increment file pointer
        file->bytenum += copyCount;
    }

    return bytesRead;
}
//-----------------------------------------------------------------------------
// fl_fseek: Seek to a specific place in the file
//-----------------------------------------------------------------------------
int fl_fseek( void *f, long offset, int origin )
{
    FL_FILE *file = (FL_FILE *)f;
    int res = -1;

    // If first call to library, initialise
    CHECK_FL_INIT();

    if (!file)
        return -1;

    if (origin == SEEK_END && offset != 0)
        return -1;

    FL_LOCK(&_fs);

    // Invalidate file buffer
    file->file_data_address = 0xFFFFFFFF;
    file->file_data_dirty = 0;

    if (origin == SEEK_SET)
    {
        file->bytenum = (uint32)offset;

        if (file->bytenum > file->filelength)
            file->bytenum = file->filelength;

        res = 0;
    }
    else if (origin == SEEK_CUR)
    {
        // Positive shift
        if (offset >= 0)
        {
            file->bytenum += offset;

            if (file->bytenum > file->filelength)
                file->bytenum = file->filelength;
        }
        // Negative shift
        else
        {
            // Make shift positive
            offset = -offset;

            // Limit to negative shift to start of file
            if ((uint32)offset > file->bytenum)
                file->bytenum = 0;
            else
                file->bytenum-= offset;
        }

        res = 0;
    }
    else if (origin == SEEK_END)
    {
        file->bytenum = file->filelength;
        res = 0;
    }
    else
        res = -1;

    FL_UNLOCK(&_fs);

    return res;
}
//-----------------------------------------------------------------------------
// fl_fgetpos: Get the current file position
//-----------------------------------------------------------------------------
int fl_fgetpos(void *f , uint32 * position)
{
    FL_FILE *file = (FL_FILE *)f;

    if (!file)
        return -1;

    FL_LOCK(&_fs);

    // Get position
    *position = file->bytenum;

    FL_UNLOCK(&_fs);

    return 0;
}
//-----------------------------------------------------------------------------
// fl_ftell: Get the current file position
//-----------------------------------------------------------------------------
long fl_ftell(void *f)
{
    uint32 pos = 0;

    fl_fgetpos(f, &pos);

    return (long)pos;
}
//-----------------------------------------------------------------------------
// fl_feof: Is the file pointer at the end of the stream?
//-----------------------------------------------------------------------------
int fl_feof(void *f)
{
    FL_FILE *file = (FL_FILE *)f;
    int res;

    if (!file)
        return -1;

    FL_LOCK(&_fs);

    if (file->bytenum == file->filelength)
        res = EOF;
    else
        res = 0;

    FL_UNLOCK(&_fs);

    return res;
}
//-----------------------------------------------------------------------------
// fl_fputc: Write a character to the stream
//-----------------------------------------------------------------------------
#if FATFS_INC_WRITE_SUPPORT
int fl_fputc(int c, void *f)
{
    uint8 data = (uint8)c;
    int res;

    res = fl_fwrite(&data, 1, 1, f);
    if (res == 1)
        return c;
    else
        return res;
}
#endif
//-----------------------------------------------------------------------------
// fl_fwrite: Write a block of data to the stream
//-----------------------------------------------------------------------------
#if FATFS_INC_WRITE_SUPPORT
int fl_fwrite(const void * data, int size, int count, void *f )
{
    FL_FILE *file = (FL_FILE *)f;
    uint32 sector;
    uint32 offset;
    uint32 length = (size*count);
    uint8 *buffer = (uint8 *)data;
    uint32 bytesWritten = 0;
    uint32 copyCount;

    // If first call to library, initialise
    CHECK_FL_INIT();

    if (!file)
        return -1;

    FL_LOCK(&_fs);

    // No write permissions
    if (!(file->flags & FILE_WRITE))
    {
        FL_UNLOCK(&_fs);
        return -1;
    }

    // Append writes to end of file
    if (file->flags & FILE_APPEND)
        file->bytenum = file->filelength;
    // Else write to current position

    // Calculate start sector
    sector = file->bytenum / FAT_SECTOR_SIZE;

    // Offset to start copying data from first sector
    offset = file->bytenum % FAT_SECTOR_SIZE;

    while (bytesWritten < length)
    {
        // Whole sector or more to be written?
        if ((offset == 0) && ((length - bytesWritten) >= FAT_SECTOR_SIZE))
        {
            uint32 sectorsWrote;

            // Buffered sector, flush back to disk
            if (file->file_data_address != 0xFFFFFFFF)
            {
                // Flush un-written data to file
                if (file->file_data_dirty)
                    fl_fflush(file);

                file->file_data_address = 0xFFFFFFFF;
                file->file_data_dirty = 0;
            }

            // Write as many sectors as possible
            sectorsWrote = _write_sectors(file, sector, (uint8*)(buffer + bytesWritten), (length - bytesWritten) / FAT_SECTOR_SIZE);
            copyCount = FAT_SECTOR_SIZE * sectorsWrote;

            // Increase total read count
            bytesWritten += copyCount;

            // Increment file pointer
            file->bytenum += copyCount;

            // Move onto next sector and reset copy offset
            sector+= sectorsWrote;
            offset = 0;

            if (!sectorsWrote)
                break;
        }
        else
        {
            // We have upto one sector to copy
            copyCount = FAT_SECTOR_SIZE - offset;

            // Only require some of this sector?
            if (copyCount > (length - bytesWritten))
                copyCount = (length - bytesWritten);

            // Do we need to read a new sector?
            if (file->file_data_address != sector)
            {
                // Flush un-written data to file
                if (file->file_data_dirty)
                    fl_fflush(file);

                // If we plan to overwrite the whole sector, we don't need to read it first!
                if (copyCount != FAT_SECTOR_SIZE)
                {
                    // NOTE: This does not have succeed; if last sector of file
                    // reached, no valid data will be read in, but write will
                    // allocate some more space for new data.

                    // Get LBA of sector offset within file
                    if (!_read_sectors(file, sector, file->file_data_sector, 1))
                        memset(file->file_data_sector, 0x00, FAT_SECTOR_SIZE);
                }

                file->file_data_address = sector;
                file->file_data_dirty = 0;
            }

            // Copy from application buffer into sector buffer
            memcpy((uint8*)(file->file_data_sector + offset), (uint8*)(buffer + bytesWritten), copyCount);

            // Mark buffer as dirty
            file->file_data_dirty = 1;

            // Increase total read count
            bytesWritten += copyCount;

            // Increment file pointer
            file->bytenum += copyCount;

            // Move onto next sector and reset copy offset
            sector++;
            offset = 0;
        }
    }

    // Write increased extent of the file?
    if (file->bytenum > file->filelength)
    {
        // Increase file size to new point
        file->filelength = file->bytenum;

        // We are changing the file length and this
        // will need to be writen back at some point
        file->filelength_changed = 1;
    }

#if FATFS_INC_TIME_DATE_SUPPORT
    // If time & date support is enabled, always force directory entry to be
    // written in-order to update file modify / access time & date.
    file->filelength_changed = 1;
#endif

    FL_UNLOCK(&_fs);

    return (size*count);
}
#endif
//-----------------------------------------------------------------------------
// fl_fputs: Write a character string to the stream
//-----------------------------------------------------------------------------
#if FATFS_INC_WRITE_SUPPORT
int fl_fputs(const char * str, void *f)
{
    int len = (int)strlen(str);
    int res = fl_fwrite(str, 1, len, f);

    if (res == len)
        return len;
    else
        return res;
}
#endif
//-----------------------------------------------------------------------------
// fl_remove: Remove a file from the filesystem
//-----------------------------------------------------------------------------
#if FATFS_INC_WRITE_SUPPORT
int fl_remove( const char * filename )
{
    FL_FILE* file;
    int res = -1;

    FL_LOCK(&_fs);

    // Use read_file as this will check if the file is already open!
    file = fl_fopen((char*)filename, "r");
    if (file)
    {
        // Delete allocated space
        if (fatfs_free_cluster_chain(&_fs, file->startcluster))
        {
            // Remove directory entries
            if (fatfs_mark_file_deleted(&_fs, file->parentcluster, (char*)file->shortfilename))
            {
                // Close the file handle (this should not write anything to the file
                // as we have not changed the file since opening it!)
                fl_fclose(file);

                res = 0;
            }
        }
    }

    FL_UNLOCK(&_fs);

    return res;
}
#endif
//-----------------------------------------------------------------------------
// fl_createdirectory: Create a directory based on a path
//-----------------------------------------------------------------------------
#if FATFS_INC_WRITE_SUPPORT
int fl_createdirectory(const char *path)
{
    int res;

    // If first call to library, initialise
    CHECK_FL_INIT();

    FL_LOCK(&_fs);
    res =_create_directory((char*)path);
    FL_UNLOCK(&_fs);

    return res;
}
#endif
//-----------------------------------------------------------------------------
// fl_listdirectory: List a directory based on a path
//-----------------------------------------------------------------------------
#if FATFS_DIR_LIST_SUPPORT
void fl_listdirectory(const char *path)
{
    FL_DIR dirstat;

    // If first call to library, initialise
    CHECK_FL_INIT();

    FL_LOCK(&_fs);

    FAT_PRINTF(("\r\nDirectory %s\r\n", path));

    if (fl_opendir(path, &dirstat))
    {
        struct fs_dir_ent dirent;

        while (fl_readdir(&dirstat, &dirent) == 0)
        {
#if FATFS_INC_TIME_DATE_SUPPORT
            int d,m,y,h,mn,s;
            fatfs_convert_from_fat_time(dirent.write_time, &h,&m,&s);
            fatfs_convert_from_fat_date(dirent.write_date, &d,&mn,&y);
            FAT_PRINTF(("%02d/%02d/%04d  %02d:%02d      ", d,mn,y,h,m));
#endif

            if (dirent.is_dir)
            {
                FAT_PRINTF(("%s <DIR>\r\n", dirent.filename));
            }
            else
            {
                FAT_PRINTF(("%s [%d bytes]\r\n", dirent.filename, dirent.size));
            }
        }

        fl_closedir(&dirstat);
    }

    FL_UNLOCK(&_fs);
}
#endif
//-----------------------------------------------------------------------------
// fl_opendir: Opens a directory for listing
//-----------------------------------------------------------------------------
#if FATFS_DIR_LIST_SUPPORT
FL_DIR* fl_opendir(const char* path, FL_DIR *dir)
{
    int levels;
    int res = 1;
    uint32 cluster = FAT32_INVALID_CLUSTER;

    // If first call to library, initialise
    CHECK_FL_INIT();

    FL_LOCK(&_fs);

    levels = fatfs_total_path_levels((char*)path) + 1;

    // If path is in the root dir
    if (levels == 0)
        cluster = fatfs_get_root_cluster(&_fs);
    // Find parent directory start cluster
    else
        res = _open_directory((char*)path, &cluster);

    if (res)
        fatfs_list_directory_start(&_fs, dir, cluster);

    FL_UNLOCK(&_fs);

    return cluster != FAT32_INVALID_CLUSTER ? dir : 0;
}
#endif
//-----------------------------------------------------------------------------
// fl_readdir: Get next item in directory
//-----------------------------------------------------------------------------
#if FATFS_DIR_LIST_SUPPORT
int fl_readdir(FL_DIR *dirls, fl_dirent *entry)
{
    int res = 0;

    // If first call to library, initialise
    CHECK_FL_INIT();

    FL_LOCK(&_fs);

    res = fatfs_list_directory_next(&_fs, dirls, entry);

    FL_UNLOCK(&_fs);

    return res ? 0 : -1;
}
#endif
//-----------------------------------------------------------------------------
// fl_closedir: Close directory after listing
//-----------------------------------------------------------------------------
#if FATFS_DIR_LIST_SUPPORT
int fl_closedir(FL_DIR* dir)
{
    // Not used
    return 0;
}
#endif
//-----------------------------------------------------------------------------
// fl_is_dir: Is this a directory?
//-----------------------------------------------------------------------------
#if FATFS_DIR_LIST_SUPPORT
int fl_is_dir(const char *path)
{
    int res = 0;
    FL_DIR dir;

    if (fl_opendir(path, &dir))
    {
        res = 1;
        fl_closedir(&dir);
    }

    return res;
}
#endif
//-----------------------------------------------------------------------------
// fl_format: Format a partition with either FAT16 or FAT32 based on size
//-----------------------------------------------------------------------------
#if FATFS_INC_FORMAT_SUPPORT
int fl_format(uint32 volume_sectors, const char *name)
{
    return fatfs_format(&_fs, volume_sectors, name);
}
#endif /*FATFS_INC_FORMAT_SUPPORT*/
//-----------------------------------------------------------------------------
// fl_get_fs:
//-----------------------------------------------------------------------------
#ifdef FATFS_INC_TEST_HOOKS
struct fatfs* fl_get_fs(void)
{
    return &_fs;
}
#endif
