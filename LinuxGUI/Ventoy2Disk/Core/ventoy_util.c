/******************************************************************************
 * ventoy_util.c  ---- ventoy util
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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <linux/fs.h>
#include <dirent.h>
#include <time.h>
#include <ventoy_define.h>
#include <ventoy_util.h>

uint8_t g_mbr_template[512];

void ventoy_gen_preudo_uuid(void *uuid)
{
    int i;
    int fd;

    fd = open("/dev/urandom", O_RDONLY | O_BINARY);
    if (fd < 0)
    {
        srand(time(NULL));
        for (i = 0; i < 8; i++)
        {
            *((uint16_t *)uuid + i) = (uint16_t)(rand() & 0xFFFF);
        }
    }
    else
    {
        read(fd, uuid, 16);
        close(fd);
    }
}

uint64_t ventoy_get_human_readable_gb(uint64_t SizeBytes)
{
    int i;
    int Pow2 = 1;
    double Delta;
    double GB = SizeBytes * 1.0 / 1000 / 1000 / 1000;

    if ((SizeBytes % SIZE_1GB) == 0)
    {
        return (uint64_t)(SizeBytes / SIZE_1GB);
    }

    for (i = 0; i < 12; i++)
    {
        if (Pow2 > GB)
        {
            Delta = (Pow2 - GB) / Pow2;
        }
        else
        {
            Delta = (GB - Pow2) / Pow2;
        }

        if (Delta < 0.05)
        {
            return Pow2;
        }

        Pow2 <<= 1;
    }

    return (uint64_t)GB;
}


int ventoy_get_sys_file_line(char *buffer, int buflen, const char *fmt, ...)
{
    int len;
    char c;
    char path[256];
    va_list arg;

    va_start(arg, fmt);
    vsnprintf(path, 256, fmt, arg);
    va_end(arg);

    if (access(path, F_OK) >= 0)
    {
        FILE *fp = fopen(path, "r");
        memset(buffer, 0, buflen);
        len = (int)fread(buffer, 1, buflen - 1, fp);
        fclose(fp);

        while (len > 0)
        {
            c = buffer[len - 1];
            if (c == '\r' || c == '\n' || c == ' ' || c == '\t')
            {
                buffer[len - 1] = 0;
                len--;
            }
            else
            {
                break;
            }
        }
        
        return 0;
    }
    else
    {
        vdebug("%s not exist \n", path);
        return 1;
    }
}

int ventoy_is_disk_mounted(const char *devpath)
{
    int len;
    int mount = 0;
    char line[512];
    FILE *fp = NULL;

    fp = fopen("/proc/mounts", "r");
    if (!fp)
    {
        return 0;
    }

    len = (int)strlen(devpath);
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, devpath, len) == 0)
        {
            mount = 1;
            vdebug("%s mounted <%s>\n", devpath, line);
            goto end;
        }
    }

end:
    fclose(fp);
    return mount;
}

int ventoy_try_umount_disk(const char *devpath)
{
    int rc;
    int len;
    char line[512];
    char *pos1 = NULL;
    char *pos2 = NULL;
    FILE *fp = NULL;

    fp = fopen("/proc/mounts", "r");
    if (!fp)
    {
        return 0;
    }

    len = (int)strlen(devpath);
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, devpath, len) == 0)
        {
            pos1 = strchr(line, ' ');
            if (pos1)
            {
                pos2 = strchr(pos1 + 1, ' ');
                if (pos2)
                {
                    *pos2 = 0;
                }

                rc = umount(pos1 + 1);
                if (rc)
                {
                    vdebug("umount %s %s [ failed ] error:%d\n", devpath, pos1 + 1, errno);                                        
                }
                else
                {
                    vdebug("umount %s %s [ success ]\n", devpath, pos1 + 1);
                }
            }
        }
    }

    fclose(fp);
    return 0;
}


int ventoy_read_file_to_buf(const char *FileName, int ExtLen, void **Bufer, int *BufLen)
{
    int FileSize;
    FILE *fp = NULL;
    void *Data = NULL;

    fp = fopen(FileName, "rb");
    if (fp == NULL)
    {
        vlog("Failed to open file %s", FileName);
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    FileSize = (int)ftell(fp);

    Data = malloc(FileSize + ExtLen);
    if (!Data)
    {
        fclose(fp);
        return 1;
    }

    fseek(fp, 0, SEEK_SET);
    fread(Data, 1, FileSize, fp);

    fclose(fp);

    *Bufer = Data;
    *BufLen = FileSize;

    return 0;
}

const char * ventoy_get_local_version(void)
{
    int rc;
    int FileSize;
    char *Pos = NULL;
    char *Buf = NULL;
    static char LocalVersion[64] = { 0 };

    if (LocalVersion[0] == 0)
    {
        rc = ventoy_read_file_to_buf("ventoy/version", 1, (void **)&Buf, &FileSize);
        if (rc)
        {
            return "";
        }
        Buf[FileSize] = 0;

        for (Pos = Buf; *Pos; Pos++)
        {
            if (*Pos == '\r' || *Pos == '\n')
            {
                *Pos = 0;
                break;
            }
        }

        scnprintf(LocalVersion, "%s", Buf);
        free(Buf);
    }
    
    return LocalVersion;
}

int VentoyGetLocalBootImg(MBR_HEAD *pMBR)
{
    memcpy(pMBR, g_mbr_template, 512);
    return 0;    
}

static int VentoyFillProtectMBR(uint64_t DiskSizeBytes, MBR_HEAD *pMBR)
{
    ventoy_guid Guid;
    uint32_t DiskSignature;
    uint64_t DiskSectorCount;

    VentoyGetLocalBootImg(pMBR);

    ventoy_gen_preudo_uuid(&Guid);

    memcpy(&DiskSignature, &Guid, sizeof(uint32_t));

    vdebug("Disk signature: 0x%08x\n", DiskSignature);

    memcpy(pMBR->BootCode + 0x1B8, &DiskSignature, 4);
    memcpy(pMBR->BootCode + 0x180, &Guid, 16);

    DiskSectorCount = DiskSizeBytes / 512 - 1;
    if (DiskSectorCount > 0xFFFFFFFF)
    {
        DiskSectorCount = 0xFFFFFFFF;
    }

    memset(pMBR->PartTbl, 0, sizeof(pMBR->PartTbl));

    pMBR->PartTbl[0].Active = 0x00;
    pMBR->PartTbl[0].FsFlag = 0xee; // EE

    pMBR->PartTbl[0].StartHead = 0;
    pMBR->PartTbl[0].StartSector = 1;
    pMBR->PartTbl[0].StartCylinder = 0;
    pMBR->PartTbl[0].EndHead = 254;
    pMBR->PartTbl[0].EndSector = 63;
    pMBR->PartTbl[0].EndCylinder = 1023;

    pMBR->PartTbl[0].StartSectorId = 1;
    pMBR->PartTbl[0].SectorCount = (uint32_t)DiskSectorCount;

    pMBR->Byte55 = 0x55;
    pMBR->ByteAA = 0xAA;

    pMBR->BootCode[92] = 0x22;

    return 0;
}

static int ventoy_fill_gpt_partname(uint16_t Name[36], const char *asciiName)
{
    int i;
    int len;

    memset(Name, 0, 36 * sizeof(uint16_t));
    len = (int)strlen(asciiName);
    for (i = 0; i < 36 && i < len; i++)
    {
        Name[i] = asciiName[i];
    }
    
    return 0;
}

int ventoy_fill_gpt(uint64_t size, uint64_t reserve, int align4k, VTOY_GPT_INFO *gpt)
{
    uint64_t ReservedSector = 33;
    uint64_t ModSectorCount = 0;
    uint64_t Part1SectorCount = 0;
    uint64_t DiskSectorCount = size / 512;
    VTOY_GPT_HDR *Head = &gpt->Head;
    VTOY_GPT_PART_TBL *Table = gpt->PartTbl;
    ventoy_guid WindowsDataPartType = { 0xebd0a0a2, 0xb9e5, 0x4433, { 0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7 } };
    //ventoy_guid EspPartType = { 0xc12a7328, 0xf81f, 0x11d2, { 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b } };
	//ventoy_guid BiosGrubPartType = { 0x21686148, 0x6449, 0x6e6f, { 0x74, 0x4e, 0x65, 0x65, 0x64, 0x45, 0x46, 0x49 } };

    VentoyFillProtectMBR(size, &gpt->MBR);

    if (reserve > 0)
    {
        ReservedSector += reserve / 512;
    }

    Part1SectorCount = DiskSectorCount - ReservedSector - (VTOYEFI_PART_BYTES / 512) - 2048;

    ModSectorCount = (Part1SectorCount % 8);
    if (ModSectorCount)
    {
        vlog("Part1SectorCount:%llu is not aligned by 4KB (%llu)\n", (_ull)Part1SectorCount, (_ull)ModSectorCount);
    }

    // check aligned with 4KB
    if (align4k)
    {
        if (ModSectorCount)
        {
            vdebug("Disk need to align with 4KB %u\n", (uint32_t)ModSectorCount);
            Part1SectorCount -= ModSectorCount;
        }
        else
        {
            vdebug("no need to align with 4KB\n");
        }
    }

    memcpy(Head->Signature, "EFI PART", 8);
    Head->Version[2] = 0x01;
    Head->Length = 92;
    Head->Crc = 0;
    Head->EfiStartLBA = 1;
    Head->EfiBackupLBA = DiskSectorCount - 1;
    Head->PartAreaStartLBA = 34;
    Head->PartAreaEndLBA = DiskSectorCount - 34;
    ventoy_gen_preudo_uuid(&Head->DiskGuid);
    Head->PartTblStartLBA = 2;
    Head->PartTblTotNum = 128;
    Head->PartTblEntryLen = 128;


    memcpy(&(Table[0].PartType), &WindowsDataPartType, sizeof(ventoy_guid));
    ventoy_gen_preudo_uuid(&(Table[0].PartGuid));
    Table[0].StartLBA = 2048;
    Table[0].LastLBA = 2048 + Part1SectorCount - 1;
    Table[0].Attr = 0;
    ventoy_fill_gpt_partname(Table[0].Name, "Ventoy");

    // to fix windows issue
    //memcpy(&(Table[1].PartType), &EspPartType, sizeof(GUID));
    memcpy(&(Table[1].PartType), &WindowsDataPartType, sizeof(ventoy_guid));
    ventoy_gen_preudo_uuid(&(Table[1].PartGuid));
    Table[1].StartLBA = Table[0].LastLBA + 1;
    Table[1].LastLBA = Table[1].StartLBA + VTOYEFI_PART_BYTES / 512 - 1;
    Table[1].Attr = 0xC000000000000001ULL;
    ventoy_fill_gpt_partname(Table[1].Name, "VTOYEFI");

#if 0
	memcpy(&(Table[2].PartType), &BiosGrubPartType, sizeof(ventoy_guid));
	ventoy_gen_preudo_uuid(&(Table[2].PartGuid));
	Table[2].StartLBA = 34;
	Table[2].LastLBA = 2047;
	Table[2].Attr = 0;
#endif

    //Update CRC
    Head->PartTblCrc = ventoy_crc32(Table, sizeof(gpt->PartTbl));
    Head->Crc = ventoy_crc32(Head, Head->Length);

    return 0;
}

int VentoyFillMBRLocation(uint64_t DiskSizeInBytes, uint32_t StartSectorId, uint32_t SectorCount, PART_TABLE *Table)
{
    uint8_t Head;
    uint8_t Sector;
    uint8_t nSector = 63;
    uint8_t nHead = 8;    
    uint32_t Cylinder;
    uint32_t EndSectorId;

    while (nHead != 0 && (DiskSizeInBytes / 512 / nSector / nHead) > 1024)
    {
        nHead = (uint8_t)nHead * 2;
    }

    if (nHead == 0)
    {
        nHead = 255;
    }

    Cylinder = StartSectorId / nSector / nHead;
    Head = StartSectorId / nSector % nHead;
    Sector = StartSectorId % nSector + 1;

    Table->StartHead = Head;
    Table->StartSector = Sector;
    Table->StartCylinder = Cylinder;

    EndSectorId = StartSectorId + SectorCount - 1;
    Cylinder = EndSectorId / nSector / nHead;
    Head = EndSectorId / nSector % nHead;
    Sector = EndSectorId % nSector + 1;

    Table->EndHead = Head;
    Table->EndSector = Sector;
    Table->EndCylinder = Cylinder;

    Table->StartSectorId = StartSectorId;
    Table->SectorCount = SectorCount;

    return 0;
}

int ventoy_fill_mbr(uint64_t size, uint64_t reserve, int align4k, MBR_HEAD *pMBR)
{
    ventoy_guid Guid;
    uint32_t DiskSignature;
    uint32_t DiskSectorCount;
    uint32_t PartSectorCount;
    uint32_t PartStartSector;
	uint32_t ReservedSector;

    VentoyGetLocalBootImg(pMBR);

    ventoy_gen_preudo_uuid(&Guid);

    memcpy(&DiskSignature, &Guid, sizeof(uint32_t));

    vdebug("Disk signature: 0x%08x\n", DiskSignature);

    memcpy(pMBR->BootCode + 0x1B8, &DiskSignature, 4);
    memcpy(pMBR->BootCode + 0x180, &Guid, 16);

    if (size / 512 > 0xFFFFFFFF)
    {
        DiskSectorCount = 0xFFFFFFFF;
    }
    else
    {
        DiskSectorCount = (uint32_t)(size / 512);
    }

	if (reserve <= 0)
	{
		ReservedSector = 0;
	}
	else
	{
		ReservedSector = (uint32_t)(reserve / 512);
	}

    // check aligned with 4KB
    if (align4k)
    {
        uint64_t sectors = size / 512;
        if (sectors % 8)
        {
            vlog("Disk need to align with 4KB %u\n", (uint32_t)(sectors % 8));
            ReservedSector += (uint32_t)(sectors % 8);
        }
        else
        {
            vdebug("no need to align with 4KB\n");
        }
    }

	vlog("ReservedSector: %u\n", ReservedSector);

    //Part1
    PartStartSector = VTOYIMG_PART_START_SECTOR;
	PartSectorCount = DiskSectorCount - ReservedSector - VTOYEFI_PART_BYTES / 512 - PartStartSector;
    VentoyFillMBRLocation(size, PartStartSector, PartSectorCount, pMBR->PartTbl);

    pMBR->PartTbl[0].Active = 0x80; // bootable
    pMBR->PartTbl[0].FsFlag = 0x07; // exFAT/NTFS/HPFS

    //Part2
    PartStartSector += PartSectorCount;
    PartSectorCount = VTOYEFI_PART_BYTES / 512;
    VentoyFillMBRLocation(size, PartStartSector, PartSectorCount, pMBR->PartTbl + 1);

    pMBR->PartTbl[1].Active = 0x00; 
    pMBR->PartTbl[1].FsFlag = 0xEF; // EFI System Partition

    pMBR->Byte55 = 0x55;
    pMBR->ByteAA = 0xAA;

    return 0;
}

