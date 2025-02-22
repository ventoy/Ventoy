/******************************************************************************
 * vtoygpt.c  ---- ventoy gpt util
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
#include <stdio.h>
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
#include <dirent.h>
#include "vtoycli.h"

void DumpGuid(const char *prefix, GUID *guid)
{
    printf("%s: %08x-%04x-%04x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n",
        prefix,
        guid->data1, guid->data2, guid->data3,
        guid->data4[0], guid->data4[1], guid->data4[2], guid->data4[3],
        guid->data4[4], guid->data4[5], guid->data4[6], guid->data4[7]
        );
}

void DumpHead(VTOY_GPT_HDR *pHead)
{
    UINT32 CrcRead;
    UINT32 CrcCalc;
    
    printf("Signature:<%s>\n", pHead->Signature);
    printf("Version:<%02x %02x %02x %02x>\n", pHead->Version[0], pHead->Version[1], pHead->Version[2], pHead->Version[3]);
    printf("Length:%u\n", pHead->Length);
    printf("Crc:0x%08x\n", pHead->Crc);
    printf("EfiStartLBA:%lu\n", pHead->EfiStartLBA);
    printf("EfiBackupLBA:%lu\n", pHead->EfiBackupLBA);
    printf("PartAreaStartLBA:%lu\n", pHead->PartAreaStartLBA);
    printf("PartAreaEndLBA:%lu\n", pHead->PartAreaEndLBA);
    DumpGuid("DiskGuid", &pHead->DiskGuid);
    
    printf("PartTblStartLBA:%lu\n", pHead->PartTblStartLBA);
    printf("PartTblTotNum:%u\n", pHead->PartTblTotNum);
    printf("PartTblEntryLen:%u\n", pHead->PartTblEntryLen);
    printf("PartTblCrc:0x%08x\n", pHead->PartTblCrc);

    CrcRead = pHead->Crc;
    pHead->Crc = 0;
    CrcCalc = VtoyCrc32(pHead, pHead->Length);

    if (CrcCalc != CrcRead)
    {
        printf("Head CRC Check Failed\n");
    }
    else
    {
        printf("Head CRC Check SUCCESS [%x] [%x]\n", CrcCalc, CrcRead);
    }

    CrcRead = pHead->PartTblCrc;
    CrcCalc = VtoyCrc32(pHead + 1, pHead->PartTblEntryLen * pHead->PartTblTotNum);
    if (CrcCalc != CrcRead)
    {
        printf("Part Table CRC Check Failed\n");
    }
    else
    {
        printf("Part Table CRC Check SUCCESS [%x] [%x]\n", CrcCalc, CrcRead);
    }
}

void DumpPartTable(VTOY_GPT_PART_TBL *Tbl)
{
    int i;
    
    DumpGuid("PartType", &Tbl->PartType);
    DumpGuid("PartGuid", &Tbl->PartGuid);
    printf("StartLBA:%lu\n", Tbl->StartLBA);
    printf("LastLBA:%lu\n", Tbl->LastLBA);
    printf("Attr:0x%lx\n", Tbl->Attr);
    printf("Name:");

    for (i = 0; i < 36 && Tbl->Name[i]; i++)
    {
        printf("%c", (CHAR)(Tbl->Name[i]));
    }
    printf("\n");    
}

void DumpMBR(MBR_HEAD *pMBR)
{
    int i;
    
    for (i = 0; i < 4; i++)
    {
        printf("=========== Partition Table %d ============\n", i + 1);
        printf("PartTbl.Active = 0x%x\n", pMBR->PartTbl[i].Active);
        printf("PartTbl.FsFlag = 0x%x\n", pMBR->PartTbl[i].FsFlag);
        printf("PartTbl.StartSectorId = %u\n", pMBR->PartTbl[i].StartSectorId);
        printf("PartTbl.SectorCount = %u\n", pMBR->PartTbl[i].SectorCount);
        printf("PartTbl.StartHead = %u\n", pMBR->PartTbl[i].StartHead);
        printf("PartTbl.StartSector = %u\n", pMBR->PartTbl[i].StartSector);
        printf("PartTbl.StartCylinder = %u\n", pMBR->PartTbl[i].StartCylinder);
        printf("PartTbl.EndHead = %u\n", pMBR->PartTbl[i].EndHead);
        printf("PartTbl.EndSector = %u\n", pMBR->PartTbl[i].EndSector);
        printf("PartTbl.EndCylinder = %u\n", pMBR->PartTbl[i].EndCylinder);
    }
}

int DumpGptInfo(VTOY_GPT_INFO *pGptInfo)
{
    int i;

    DumpMBR(&pGptInfo->MBR);
    DumpHead(&pGptInfo->Head);

    for (i = 0; i < 128; i++)
    {
        if (pGptInfo->PartTbl[i].StartLBA == 0)
        {
            break;
        }
    
        printf("=====Part %d=====\n", i);
        DumpPartTable(pGptInfo->PartTbl + i);
    }

    return 0;
}

int vtoygpt_main(int argc, char **argv)
{
    int i;
    int fd;
    UINT64 DiskSize;
    CHAR16 *Name = NULL;
    VTOY_GPT_INFO *pMainGptInfo = NULL;
    VTOY_BK_GPT_INFO *pBackGptInfo = NULL;

    if (argc != 3)
    {
        printf("usage: vtoygpt -f /dev/sdb\n");
        return 1;
    }

    fd = open(argv[2], O_RDWR);
    if (fd < 0)
    {
        printf("Failed to open %s\n", argv[2]);
        return 1;
    }

    pMainGptInfo = malloc(sizeof(VTOY_GPT_INFO));
    pBackGptInfo = malloc(sizeof(VTOY_BK_GPT_INFO));
    if (NULL == pMainGptInfo || NULL == pBackGptInfo)
    {
        close(fd);
        return 1;
    }

    read(fd, pMainGptInfo, sizeof(VTOY_GPT_INFO));
    
    if (argv[1][0] == '-' && argv[1][1] == 'd')
    {
        DumpGptInfo(pMainGptInfo);
    }
    else
    {
        DiskSize = lseek(fd, 0, SEEK_END);
        lseek(fd, DiskSize - 33 * 512, SEEK_SET);
        read(fd, pBackGptInfo, sizeof(VTOY_BK_GPT_INFO));

        Name = pMainGptInfo->PartTbl[1].Name;
        if (Name[0] == 'V' && Name[1] == 'T' && Name[2] == 'O' && Name[3] == 'Y')
        {
            if (pMainGptInfo->PartTbl[1].Attr != VENTOY_EFI_PART_ATTR)
            {                
                pMainGptInfo->PartTbl[1].Attr = VENTOY_EFI_PART_ATTR;
                pMainGptInfo->Head.PartTblCrc = VtoyCrc32(pMainGptInfo->PartTbl, sizeof(pMainGptInfo->PartTbl));
                pMainGptInfo->Head.Crc = 0;
                pMainGptInfo->Head.Crc = VtoyCrc32(&pMainGptInfo->Head, pMainGptInfo->Head.Length);

                pBackGptInfo->PartTbl[1].Attr = VENTOY_EFI_PART_ATTR;
                pBackGptInfo->Head.PartTblCrc = VtoyCrc32(pBackGptInfo->PartTbl, sizeof(pBackGptInfo->PartTbl));
                pBackGptInfo->Head.Crc = 0;
                pBackGptInfo->Head.Crc = VtoyCrc32(&pBackGptInfo->Head, pBackGptInfo->Head.Length);

                lseek(fd, 512, SEEK_SET);
                write(fd, (UINT8 *)pMainGptInfo + 512, sizeof(VTOY_GPT_INFO) - 512);

                lseek(fd, DiskSize - 33 * 512, SEEK_SET);
                write(fd, pBackGptInfo, sizeof(VTOY_BK_GPT_INFO));

                fsync(fd);
            }
        }
    }

    free(pMainGptInfo);
    free(pBackGptInfo);
    close(fd);

    return 0;
}

