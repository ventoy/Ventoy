/******************************************************************************
 * vtoycli.h
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

#ifndef __VTOYCLI_H__
#define __VTOYCLI_H__

#define VENTOY_EFI_PART_ATTR   0xC000000000000001ULL

#define SIZE_1MB   (1024 * 1024)
#define VENTOY_EFI_PART_SIZE   (32 * SIZE_1MB)

#define check_free(p) if (p) free(p)
#define check_close(fd) if (fd >= 0) close(fd)

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

UINT32 VtoyCrc32(VOID *Buffer, UINT32 Length);
int vtoygpt_main(int argc, char **argv);
int vtoyfat_main(int argc, char **argv);
int partresize_main(int argc, char **argv);
void ventoy_gen_preudo_uuid(void *uuid);
UINT64 get_disk_size_in_byte(const char *disk);
    
#endif /* __VTOYCLI_H__ */

