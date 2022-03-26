#ifndef __FAT_OPTS_H__
#define __FAT_OPTS_H__

#ifdef FATFS_USE_CUSTOM_OPTS_FILE
    #include "fat_custom.h"
#endif

//-------------------------------------------------------------
// Configuration
//-------------------------------------------------------------

// Is the processor little endian (1) or big endian (0)
#ifndef FATFS_IS_LITTLE_ENDIAN
    #define FATFS_IS_LITTLE_ENDIAN          1
#endif

// Max filename Length
#ifndef FATFS_MAX_LONG_FILENAME
    #define FATFS_MAX_LONG_FILENAME         260
#endif

// Max open files (reduce to lower memory requirements)
#ifndef FATFS_MAX_OPEN_FILES
    #define FATFS_MAX_OPEN_FILES            2
#endif

// Number of sectors per FAT_BUFFER (min 1)
#ifndef FAT_BUFFER_SECTORS
    #define FAT_BUFFER_SECTORS              1
#endif

// Max FAT sectors to buffer (min 1)
// (mem used is FAT_BUFFERS * FAT_BUFFER_SECTORS * FAT_SECTOR_SIZE)
#ifndef FAT_BUFFERS
    #define FAT_BUFFERS                     1
#endif

// Size of cluster chain cache (can be undefined)
// Mem used = FAT_CLUSTER_CACHE_ENTRIES * 4 * 2
// Improves access speed considerably
//#define FAT_CLUSTER_CACHE_ENTRIES         128

// Include support for writing files (1 / 0)?
#ifndef FATFS_INC_WRITE_SUPPORT
    #define FATFS_INC_WRITE_SUPPORT         1
#endif

// Support long filenames (1 / 0)?
// (if not (0) only 8.3 format is supported)
#ifndef FATFS_INC_LFN_SUPPORT
    #define FATFS_INC_LFN_SUPPORT           1
#endif

// Support directory listing (1 / 0)?
#ifndef FATFS_DIR_LIST_SUPPORT
    #define FATFS_DIR_LIST_SUPPORT          1
#endif

// Support time/date (1 / 0)?
#ifndef FATFS_INC_TIME_DATE_SUPPORT
    #define FATFS_INC_TIME_DATE_SUPPORT     0
#endif

// Include support for formatting disks (1 / 0)?
#ifndef FATFS_INC_FORMAT_SUPPORT
    #define FATFS_INC_FORMAT_SUPPORT        1
#endif

// Sector size used
#define FAT_SECTOR_SIZE                     512

// Printf output (directory listing / debug)
#ifndef FAT_PRINTF
    // Don't include stdio, but there is a printf function available
    #ifdef FAT_PRINTF_NOINC_STDIO
        extern int printf(const char* ctrl1, ... );
        #define FAT_PRINTF(a)               printf a
    // Include stdio to use printf
    #else
        #include <stdio.h>
        #define FAT_PRINTF(a)               printf a
    #endif
#endif

// Time/Date support requires time.h
#if FATFS_INC_TIME_DATE_SUPPORT
    #include <time.h>
#endif

#endif
