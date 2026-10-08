/******************************************************************************
 * vtoygpt.c  ---- ventoy gpt
 *
 * Copyright (c) 2026, longpanda <admin@ventoy.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

static int verbose = 0;
#define debug(fmt, ...) if(verbose) printf(fmt, ##__VA_ARGS__)


#define VOID   void
#define CHAR   char
#define UINT64 unsigned long long
#define UINT32 unsigned int
#define UINT16 unsigned short
#define CHAR16 unsigned short
#define UINT8  unsigned char

UINT32 VtoyCrc32(VOID *Buffer, UINT32 Length);

#define COMPILE_ASSERT(expr)  extern char __compile_assert[(expr) ? 1 : -1]

#pragma pack(1)

typedef struct PART_TABLE
{
    UINT8  Active;

    UINT8  StartHead;
    UINT16 StartSector : 6;
    UINT16 StartCylinder : 10;

    UINT8  FsFlag;

    UINT8  EndHead;
    UINT16 EndSector : 6;
    UINT16 EndCylinder : 10;

    UINT32 StartSectorId;
    UINT32 SectorCount;
}PART_TABLE;

typedef struct MBR_HEAD
{
    UINT8 BootCode[446];
    PART_TABLE PartTbl[4];
    UINT8 Byte55;
    UINT8 ByteAA;
}MBR_HEAD;

typedef struct GUID
{
    UINT32   data1;
    UINT16   data2;
    UINT16   data3;
    UINT8    data4[8];
}GUID;

typedef struct VTOY_GPT_HDR
{
    CHAR   Signature[8]; /* EFI PART */
    UINT8  Version[4];
    UINT32 Length;
    UINT32 Crc;
    UINT8  Reserved1[4];
    UINT64 EfiStartLBA;
    UINT64 EfiBackupLBA;
    UINT64 PartAreaStartLBA;
    UINT64 PartAreaEndLBA;
    GUID   DiskGuid;
    UINT64 PartTblStartLBA;
    UINT32 PartTblTotNum;
    UINT32 PartTblEntryLen;
    UINT32 PartTblCrc;
    UINT8  Reserved2[420];
}VTOY_GPT_HDR;

COMPILE_ASSERT(sizeof(VTOY_GPT_HDR) == 512);

typedef struct VTOY_GPT_PART_TBL
{
    GUID   PartType;
    GUID   PartGuid;
    UINT64 StartLBA;
    UINT64 LastLBA;
    UINT64 Attr;
    CHAR16 Name[36];
}VTOY_GPT_PART_TBL;
COMPILE_ASSERT(sizeof(VTOY_GPT_PART_TBL) == 128);

typedef struct VTOY_GPT_INFO
{
    MBR_HEAD MBR;
    VTOY_GPT_HDR Head;
    VTOY_GPT_PART_TBL PartTbl[128];
}VTOY_GPT_INFO;

typedef struct VTOY_BK_GPT_INFO
{
    VTOY_GPT_PART_TBL PartTbl[128];
    VTOY_GPT_HDR Head;
}VTOY_BK_GPT_INFO;

COMPILE_ASSERT(sizeof(VTOY_GPT_INFO) == 512 * 34);
COMPILE_ASSERT(sizeof(VTOY_BK_GPT_INFO) == 512 * 33);

#pragma pack()

static void dump_gpt_partuuid(int fd, int part)
{
    int i;
    int start, end;
    GUID *guid = NULL;
    VTOY_GPT_INFO *pInfo = NULL;

    pInfo = (VTOY_GPT_INFO *)malloc(sizeof(VTOY_GPT_INFO));

    lseek(fd, 0, SEEK_SET);
    read(fd, pInfo, sizeof(VTOY_GPT_INFO));

    if (part >= 1 && part <= 128)
    {
        start = end = part;
    }
    else
    {
        start = 1;
        end = 128;
    }

    for (i = start; i <= end; i++)
    {
        if (pInfo->PartTbl[i - 1].StartLBA == 0)
        {
            break;
        }

        guid = &(pInfo->PartTbl[i - 1].PartGuid);

        printf("%d %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n", i,
                guid->data1, guid->data2, guid->data3,
                guid->data4[0], guid->data4[1], guid->data4[2], guid->data4[3],
                guid->data4[4], guid->data4[5], guid->data4[6], guid->data4[7]);
    }
}

int vtoygpt_main(int argc, char **argv)
{
    int i;
    int part;
    int fd;

    for (i = 0; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] == 'v')
        {
            verbose = 1;
            break;
        }
    }

    part = (int)strtol(argv[2], NULL, 10);

    fd = open(argv[1], O_RDONLY);
    if (fd < 0)
    {
        debug("Failed to open %s\n", argv[1]);
        return 1;
    }

    dump_gpt_partuuid(fd, part);

    close(fd);

    return 0;
}

// wrapper main
#ifndef BUILD_VTOY_TOOL
int main(int argc, char **argv)
{
    return vtoygpt_main(argc, argv);
}
#endif

