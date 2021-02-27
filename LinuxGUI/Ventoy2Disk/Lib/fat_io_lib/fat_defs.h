#ifndef __FAT_DEFS_H__
#define __FAT_DEFS_H__

#include "fat_opts.h"
#include "fat_types.h"

//-----------------------------------------------------------------------------
//            FAT32 Offsets
//        Name                Offset
//-----------------------------------------------------------------------------

// Boot Sector
#define BS_JMPBOOT              0    // Length = 3
#define BS_OEMNAME              3    // Length = 8
#define BPB_BYTSPERSEC          11    // Length = 2
#define BPB_SECPERCLUS          13    // Length = 1
#define BPB_RSVDSECCNT          14    // Length = 2
#define BPB_NUMFATS             16    // Length = 1
#define BPB_ROOTENTCNT          17    // Length = 2
#define BPB_TOTSEC16            19    // Length = 2
#define BPB_MEDIA               21    // Length = 1
#define    BPB_FATSZ16          22    // Length = 2
#define BPB_SECPERTRK           24    // Length = 2
#define BPB_NUMHEADS            26    // Length = 2
#define BPB_HIDDSEC             28    // Length = 4
#define BPB_TOTSEC32            32    // Length = 4

// FAT 12/16
#define BS_FAT_DRVNUM           36    // Length = 1
#define BS_FAT_BOOTSIG          38    // Length = 1
#define BS_FAT_VOLID            39    // Length = 4
#define BS_FAT_VOLLAB           43    // Length = 11
#define BS_FAT_FILSYSTYPE       54    // Length = 8

// FAT 32
#define BPB_FAT32_FATSZ32       36    // Length = 4
#define BPB_FAT32_EXTFLAGS      40    // Length = 2
#define BPB_FAT32_FSVER         42    // Length = 2
#define BPB_FAT32_ROOTCLUS      44    // Length = 4
#define BPB_FAT32_FSINFO        48    // Length = 2
#define BPB_FAT32_BKBOOTSEC     50    // Length = 2
#define BS_FAT32_DRVNUM         64    // Length = 1
#define BS_FAT32_BOOTSIG        66    // Length = 1
#define BS_FAT32_VOLID          67    // Length = 4
#define BS_FAT32_VOLLAB         71    // Length = 11
#define BS_FAT32_FILSYSTYPE     82    // Length = 8

//-----------------------------------------------------------------------------
// FAT Types
//-----------------------------------------------------------------------------
#define FAT_TYPE_FAT12          1
#define FAT_TYPE_FAT16          2
#define FAT_TYPE_FAT32          3

//-----------------------------------------------------------------------------
// FAT32 Specific Statics
//-----------------------------------------------------------------------------
#define SIGNATURE_POSITION              510
#define SIGNATURE_VALUE                 0xAA55
#define PARTITION1_TYPECODE_LOCATION    450
#define FAT32_TYPECODE1                 0x0B
#define FAT32_TYPECODE2                 0x0C
#define PARTITION1_LBA_BEGIN_LOCATION   454
#define PARTITION1_SIZE_LOCATION        458

#define FAT_DIR_ENTRY_SIZE              32
#define FAT_SFN_SIZE_FULL               11
#define FAT_SFN_SIZE_PARTIAL            8

//-----------------------------------------------------------------------------
// FAT32 File Attributes and Types
//-----------------------------------------------------------------------------
#define FILE_ATTR_READ_ONLY             0x01
#define FILE_ATTR_HIDDEN                0x02
#define FILE_ATTR_SYSTEM                0x04
#define FILE_ATTR_SYSHID                0x06
#define FILE_ATTR_VOLUME_ID             0x08
#define FILE_ATTR_DIRECTORY             0x10
#define FILE_ATTR_ARCHIVE               0x20
#define FILE_ATTR_LFN_TEXT              0x0F
#define FILE_HEADER_BLANK               0x00
#define FILE_HEADER_DELETED             0xE5
#define FILE_TYPE_DIR                   0x10
#define FILE_TYPE_FILE                  0x20

//-----------------------------------------------------------------------------
// Time / Date details
//-----------------------------------------------------------------------------
#define FAT_TIME_HOURS_SHIFT            11
#define FAT_TIME_HOURS_MASK             0x1F
#define FAT_TIME_MINUTES_SHIFT          5
#define FAT_TIME_MINUTES_MASK           0x3F
#define FAT_TIME_SECONDS_SHIFT          0
#define FAT_TIME_SECONDS_MASK           0x1F
#define FAT_TIME_SECONDS_SCALE          2
#define FAT_DATE_YEAR_SHIFT             9
#define FAT_DATE_YEAR_MASK              0x7F
#define FAT_DATE_MONTH_SHIFT            5
#define FAT_DATE_MONTH_MASK             0xF
#define FAT_DATE_DAY_SHIFT              0
#define FAT_DATE_DAY_MASK               0x1F
#define FAT_DATE_YEAR_OFFSET            1980

//-----------------------------------------------------------------------------
// Other Defines
//-----------------------------------------------------------------------------
#define FAT32_LAST_CLUSTER              0xFFFFFFFF
#define FAT32_INVALID_CLUSTER           0xFFFFFFFF

STRUCT_PACK_BEGIN
struct fat_dir_entry STRUCT_PACK
{
    uint8 Name[11];
    uint8 Attr;
    uint8 NTRes;
    uint8 CrtTimeTenth;
    uint8 CrtTime[2];
    uint8 CrtDate[2];
    uint8 LstAccDate[2];
    uint16 FstClusHI;
    uint8 WrtTime[2];
    uint8 WrtDate[2];
    uint16 FstClusLO;
    uint32 FileSize;
} STRUCT_PACKED;
STRUCT_PACK_END

#endif
