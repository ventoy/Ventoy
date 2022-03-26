/******************************************************************************
 * ventoy_define.h
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */
#ifndef __VENTOY_DEFINE_H__
#define __VENTOY_DEFINE_H__

#if defined(_MSC_VER) || defined(WIN32)
#include <windows.h>
#include <stdint.h>
#else
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <linux/limits.h>
#endif

#define LOG_FILE "VentoyPlugson.log"

#define SIZE_1MB    1048576
#define SIZE_1GB    1073741824
#define JSON_BUF_MAX  (8 * SIZE_1MB)
#define TAR_BUF_MAX  (8 * SIZE_1MB)

#define VTOYIMG_PART_START_BYTES    (1024 * 1024)
#define VTOYIMG_PART_START_SECTOR   2048

#define VTOYEFI_PART_BYTES    (32 * 1024 * 1024)
#define VTOYEFI_PART_SECTORS  65536


#pragma pack(1)

typedef struct vtoy_guid
{
    uint32_t   data1;
    uint16_t   data2;
    uint16_t   data3;
    uint8_t    data4[8];
}vtoy_guid;

typedef struct PART_TABLE
{
    uint8_t  Active; // 0x00  0x80

    uint8_t  StartHead;
    uint16_t StartSector : 6;
    uint16_t StartCylinder : 10;

    uint8_t  FsFlag;

    uint8_t  EndHead;
    uint16_t EndSector : 6;
    uint16_t EndCylinder : 10;

    uint32_t StartSectorId;
    uint32_t SectorCount;
}PART_TABLE;

typedef struct MBR_HEAD
{
    uint8_t BootCode[446];
    PART_TABLE PartTbl[4];
    uint8_t Byte55;
    uint8_t ByteAA;
}MBR_HEAD;

typedef struct VTOY_GPT_HDR
{
    char     Signature[8]; /* EFI PART */
    uint8_t  Version[4];
    uint32_t Length;
    uint32_t Crc;
    uint8_t  Reserved1[4];
    uint64_t EfiStartLBA;
    uint64_t EfiBackupLBA;
    uint64_t PartAreaStartLBA;
    uint64_t PartAreaEndLBA;
    vtoy_guid   DiskGuid;
    uint64_t PartTblStartLBA;
    uint32_t PartTblTotNum;
    uint32_t PartTblEntryLen;
    uint32_t PartTblCrc;
    uint8_t  Reserved2[420];
}VTOY_GPT_HDR;

typedef struct VTOY_GPT_PART_TBL
{
    vtoy_guid   PartType;
    vtoy_guid   PartGuid;
    uint64_t StartLBA;
    uint64_t LastLBA;
    uint64_t Attr;
    uint16_t Name[36];
}VTOY_GPT_PART_TBL;

typedef struct VTOY_GPT_INFO
{
    MBR_HEAD MBR;
    VTOY_GPT_HDR Head;
    VTOY_GPT_PART_TBL PartTbl[128];
}VTOY_GPT_INFO;
#pragma pack()


#define MBR_PART_STYLE  0
#define GPT_PART_STYLE  1

#pragma pack(1)
typedef struct ventoy_guid
{
    uint32_t   data1;
    uint16_t   data2;
    uint16_t   data3;
    uint8_t    data4[8];
}ventoy_guid;
#pragma pack()

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define VLOG_LOG    1
#define VLOG_DEBUG  2

#define ulong unsigned long
#define _ll long long
#define _ull unsigned long long

#if defined(_MSC_VER) || defined(WIN32)
#define strlcpy(dst, src) strcpy_s(dst, sizeof(dst), src)
#define scnprintf(dst, len, fmt, ...) sprintf_s(dst, len, fmt, ##__VA_ARGS__)

#define vlog(fmt, ...) ventoy_syslog(VLOG_LOG, fmt, ##__VA_ARGS__)
#define vdebug(fmt, ...) ventoy_syslog(VLOG_DEBUG, fmt, ##__VA_ARGS__)

#define localtime_r(a,b) localtime_s(b,a)

#define LASTERR GetLastError()

#define CHECK_CLOSE_HANDLE(Handle) \
{\
    if (Handle != INVALID_HANDLE_VALUE) \
    {\
        CloseHandle(Handle); \
        Handle = INVALID_HANDLE_VALUE; \
    }\
}

#else
#define strlcpy(dst, src) strncpy(dst, src, sizeof(dst) - 1)
#define scnprintf(dst, len, fmt, args...) snprintf(dst, len, fmt, ##args)

#define vlog(fmt, args...) ventoy_syslog(VLOG_LOG, fmt, ##args)
#define vdebug(fmt, args...) ventoy_syslog(VLOG_DEBUG, fmt, ##args)

#define MAX_PATH PATH_MAX

#endif

#define CHECK_FREE(p) \
{\
    if (p)\
    {\
        free(p); \
        (p) = NULL; \
    }\
}

void ventoy_syslog(int level, const char *Fmt, ...);
void ventoy_set_loglevel(int level);
uint32_t ventoy_crc32(void *Buffer, uint32_t Length);


#if defined(_MSC_VER) || defined(WIN32)
static __inline void * zalloc(size_t n)
#else
static inline void * zalloc(size_t n)
#endif
{
    void *p = malloc(n);
    if (p) memset(p, 0, n);
    return p;
}


#endif /* __VENTOY_DEFINE_H__ */

