/******************************************************************************
 * partresize.c  ---- ventoy part resize util
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
#include <fat_filelib.h>
#include "vtoycli.h"

static int g_disk_fd = 0;
static UINT64 g_disk_offset = 0;
static GUID g_ZeroGuid = {0};
static GUID g_WindowsDataPartGuid = { 0xebd0a0a2, 0xb9e5, 0x4433, { 0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7 } };

static int vtoy_disk_read(uint32 sector, uint8 *buffer, uint32 sector_count)
{
    UINT64 offset = sector * 512ULL;
    
    lseek(g_disk_fd, g_disk_offset + offset, SEEK_SET);
    read(g_disk_fd, buffer, sector_count * 512);
    
    return 1;
}

static int vtoy_disk_write(uint32 sector, uint8 *buffer, uint32 sector_count)
{
    UINT64 offset = sector * 512ULL;
    
    lseek(g_disk_fd, g_disk_offset + offset, SEEK_SET);
    write(g_disk_fd, buffer, sector_count * 512);
    
    return 1;
}


static int gpt_check(const char *disk)
{
    int fd = -1;
    int rc = 1;
    VTOY_GPT_INFO *pGPT = NULL;

    fd = open(disk, O_RDONLY);
    if (fd < 0)
    {
        printf("Failed to open %s\n", disk);
        goto out;
    }

    pGPT = malloc(sizeof(VTOY_GPT_INFO));
    if (NULL == pGPT)
    {
        goto out;
    }
    memset(pGPT, 0, sizeof(VTOY_GPT_INFO));
    
    read(fd, pGPT, sizeof(VTOY_GPT_INFO));

    if (pGPT->MBR.PartTbl[0].FsFlag == 0xEE && memcmp(pGPT->Head.Signature, "EFI PART", 8) == 0)
	{
        rc = 0;
	}

out:
    check_close(fd);        
    check_free(pGPT);
    return rc;
}

static int part_check(const char *disk)
{
    int i;
    int fd = -1;
    int rc = 0;
    int Index = 0;
    int Count = 0;
    int PartStyle = 0;
    UINT64 Part1Start;
    UINT64 Part1End;
    UINT64 NextPartStart;
    UINT64 DiskSizeInBytes;
    VTOY_GPT_INFO *pGPT = NULL;

    DiskSizeInBytes = get_disk_size_in_byte(disk);
    if (DiskSizeInBytes == 0)
    {
        printf("Failed to get disk size of %s\n", disk);
        goto out;
    }
    
    fd = open(disk, O_RDONLY);
    if (fd < 0)
    {
        printf("Failed to open %s\n", disk);
        goto out;
    }

    pGPT = malloc(sizeof(VTOY_GPT_INFO));
    if (NULL == pGPT)
    {
        goto out;
    }
    memset(pGPT, 0, sizeof(VTOY_GPT_INFO));
    
    read(fd, pGPT, sizeof(VTOY_GPT_INFO));

    if (pGPT->MBR.PartTbl[0].FsFlag == 0xEE && memcmp(pGPT->Head.Signature, "EFI PART", 8) == 0)
	{
        PartStyle = 1;
	}
	else
	{
        PartStyle = 0;
	}

    if (PartStyle == 0)
    {
		PART_TABLE *PartTbl = pGPT->MBR.PartTbl;

        for (Count = 0, i = 0; i < 4; i++)
        {
            if (PartTbl[i].SectorCount > 0)
            {
				printf("MBR Part%d SectorStart:%u SectorCount:%u\n", i + 1, PartTbl[i].StartSectorId, PartTbl[i].SectorCount);
                Count++;
            }
        }

		//We must have a free partition table for VTOYEFI partition
		if (Count >= 4)
		{
			printf("###[FAIL] 4 MBR partition tables are all used.\n");
			goto out;
		}

		if (PartTbl[0].SectorCount > 0)
		{
			Part1Start = PartTbl[0].StartSectorId;
			Part1End = PartTbl[0].SectorCount + Part1Start;
		}
		else
		{
			printf("###[FAIL] MBR Partition 1 is invalid\n");
			goto out;
		}

		Index = -1;
		NextPartStart = DiskSizeInBytes / 512ULL;
		for (i = 1; i < 4; i++)
		{
			if (PartTbl[i].SectorCount > 0 && NextPartStart > PartTbl[i].StartSectorId)
			{
				Index = i;
				NextPartStart = PartTbl[i].StartSectorId;
			}
		}

        NextPartStart *= 512ULL;
		printf("DiskSize:%llu NextPartStart:%llu(LBA:%llu) Index:%d\n", 
            DiskSizeInBytes, NextPartStart, NextPartStart / 512ULL, Index);
    }
    else
    {
		VTOY_GPT_PART_TBL *PartTbl = pGPT->PartTbl;

        for (Count = 0, i = 0; i < 128; i++)
        {
            if (memcmp(&(PartTbl[i].PartGuid), &g_ZeroGuid, sizeof(GUID)))
            {
				printf("GPT Part%d StartLBA:%llu LastLBA:%llu\n", i + 1, PartTbl[i].StartLBA, PartTbl[i].LastLBA);
                Count++;
            }
        }

		if (Count >= 128)
		{
			printf("###[FAIL] 128 GPT partition tables are all used.\n");
			goto out;
		}

		if (memcmp(&(PartTbl[0].PartGuid), &g_ZeroGuid, sizeof(GUID)))
		{
			Part1Start = PartTbl[0].StartLBA;
			Part1End = PartTbl[0].LastLBA + 1;
		}
		else
		{
			printf("###[FAIL] GPT Partition 1 is invalid\n");
			goto out;
		}

		Index = -1;
		NextPartStart = (pGPT->Head.PartAreaEndLBA + 1);
		for (i = 1; i < 128; i++)
		{
			if (memcmp(&(PartTbl[i].PartGuid), &g_ZeroGuid, sizeof(GUID)) && NextPartStart > PartTbl[i].StartLBA)
			{
				Index = i;
				NextPartStart = PartTbl[i].StartLBA;
			}
		}

		NextPartStart *= 512ULL;
		printf("DiskSize:%llu NextPartStart:%llu(LBA:%llu) Index:%d\n",
            DiskSizeInBytes, NextPartStart, NextPartStart / 512ULL, Index);
    }

	printf("Valid partition table (%s): Valid partition count:%d\n", (PartStyle == 0) ? "MBR" : "GPT", Count);

	//Partition 1 MUST start at 1MB
	Part1Start *= 512ULL;
	Part1End *= 512ULL;

	printf("Partition 1 start at: %llu %lluKB, end:%llu, NextPartStart:%llu\n", 
		Part1Start, Part1Start / 1024, Part1End, NextPartStart);
    if (Part1Start != SIZE_1MB)
    {
        printf("###[FAIL] Partition 1 is not start at 1MB\n");
        goto out;
    }


	//If we have free space after partition 1
	if (NextPartStart - Part1End >= VENTOY_EFI_PART_SIZE)
	{
		printf("Free space after partition 1 (%llu) is enough for VTOYEFI part\n", NextPartStart - Part1End);
		rc = 1;
	}
	else if (NextPartStart == Part1End)
	{
		printf("There is no free space after partition 1\n");
        rc = 2;
	}
	else
	{
		printf("The free space after partition 1 is not enough\n");
        rc = 2;
	}

out:
    check_close(fd);        
    check_free(pGPT);
    return rc;
}

static int secureboot_proc(char *disk, UINT64 part2start)
{
	int rc = 0;
	int size;
    int fd = -1;
	char *filebuf = NULL;
	void *file = NULL;

    fd = open(disk, O_RDWR);
    if (fd < 0)
    {
        printf("Failed to open %s\n", disk);
        return 1;
    }

    g_disk_fd = fd;
    g_disk_offset = part2start * 512ULL;

	fl_init();

	if (0 == fl_attach_media(vtoy_disk_read, vtoy_disk_write))
	{
		file = fl_fopen("/EFI/BOOT/grubx64_real.efi", "rb");
		printf("Open ventoy efi file %p\n", file);
		if (file)
		{
			fl_fseek(file, 0, SEEK_END);
			size = (int)fl_ftell(file);
			fl_fseek(file, 0, SEEK_SET);

			printf("ventoy x64 efi file size %d ...\n", size);

			filebuf = (char *)malloc(size);
			if (filebuf)
			{
				fl_fread(filebuf, 1, size, file);
			}

			fl_fclose(file);

			fl_remove("/EFI/BOOT/BOOTX64.EFI");
			fl_remove("/EFI/BOOT/grubx64.efi");
			fl_remove("/EFI/BOOT/grubx64_real.efi");
			fl_remove("/EFI/BOOT/MokManager.efi");
			fl_remove("/EFI/BOOT/mmx64.efi");
            fl_remove("/ENROLL_THIS_KEY_IN_MOKMANAGER.cer");

			file = fl_fopen("/EFI/BOOT/BOOTX64.EFI", "wb");
			printf("Open bootx64 efi file %p\n", file);
			if (file)
			{
				if (filebuf)
				{
					fl_fwrite(filebuf, 1, size, file);
				}
				
				fl_fflush(file);
				fl_fclose(file);
			}

			if (filebuf)
			{
				free(filebuf);
			}
		}

        file = fl_fopen("/EFI/BOOT/grubia32_real.efi", "rb");
        printf("Open ventoy ia32 efi file %p\n", file);
        if (file)
        {
            fl_fseek(file, 0, SEEK_END);
            size = (int)fl_ftell(file);
            fl_fseek(file, 0, SEEK_SET);

            printf("ventoy efi file size %d ...\n", size);

            filebuf = (char *)malloc(size);
            if (filebuf)
            {
                fl_fread(filebuf, 1, size, file);
            }

            fl_fclose(file);

            fl_remove("/EFI/BOOT/BOOTIA32.EFI");
            fl_remove("/EFI/BOOT/grubia32.efi");
            fl_remove("/EFI/BOOT/grubia32_real.efi");
            fl_remove("/EFI/BOOT/mmia32.efi");            

            file = fl_fopen("/EFI/BOOT/BOOTIA32.EFI", "wb");
            printf("Open bootia32 efi file %p\n", file);
            if (file)
            {
                if (filebuf)
                {
                    fl_fwrite(filebuf, 1, size, file);
                }

                fl_fflush(file);
                fl_fclose(file);
            }

            if (filebuf)
            {
                free(filebuf);
            }
        }

	}
	else
	{
		rc = 1;
	}

	fl_shutdown();
    fsync(fd);

	return rc;
}

static int VentoyFillMBRLocation(UINT64 DiskSizeInBytes, UINT32 StartSectorId, UINT32 SectorCount, PART_TABLE *Table)
{
    UINT8 Head;
    UINT8 Sector;
    UINT8 nSector = 63;
    UINT8 nHead = 8;    
    UINT32 Cylinder;
    UINT32 EndSectorId;

    while (nHead != 0 && (DiskSizeInBytes / 512 / nSector / nHead) > 1024)
    {
        nHead = (UINT8)nHead * 2;
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


static int WriteDataToPhyDisk(int fd, UINT64 offset, void *buffer, int len)
{
    ssize_t wrlen;
    off_t newseek;
        
    newseek = lseek(fd, offset, SEEK_SET);
    if (newseek != offset)
    {
        printf("Failed to lseek %llu %lld %d\n", offset, (long long)newseek, errno);
        return 0;
    }
    
    wrlen = write(fd, buffer, len);
    if ((int)wrlen != len)
    {
        printf("Failed to write %d %d %d\n", len, (int)wrlen, errno);
        return 0;
    }
    
    return 1;
}

static int VentoyFillBackupGptHead(VTOY_GPT_INFO *pInfo, VTOY_GPT_HDR *pHead)
{
    UINT64 LBA;
    UINT64 BackupLBA;

    memcpy(pHead, &pInfo->Head, sizeof(VTOY_GPT_HDR));

    LBA = pHead->EfiStartLBA;
    BackupLBA = pHead->EfiBackupLBA;
    
    pHead->EfiStartLBA = BackupLBA;
    pHead->EfiBackupLBA = LBA;
    pHead->PartTblStartLBA = BackupLBA + 1 - 33;

    pHead->Crc = 0;
    pHead->Crc = VtoyCrc32(pHead, pHead->Length);

    return 0;
}

static int update_part_table(char *disk, UINT64 part2start)
{
    int i;
    int j;
    int fd = -1;
    int rc = 1;
    int PartStyle = 0;
    ssize_t len = 0;
    UINT64 DiskSizeInBytes;
    VTOY_GPT_INFO *pGPT = NULL;
    VTOY_GPT_HDR *pBack = NULL;

    DiskSizeInBytes = get_disk_size_in_byte(disk);
    if (DiskSizeInBytes == 0)
    {
        printf("Failed to get disk size of %s\n", disk);
        goto out;
    }
    
    fd = open(disk, O_RDWR);
    if (fd < 0)
    {
        printf("Failed to open %s\n", disk);
        goto out;
    }

    pGPT = malloc(sizeof(VTOY_GPT_INFO) + sizeof(VTOY_GPT_HDR));
    if (NULL == pGPT)
    {
        goto out;
    }
    memset(pGPT, 0, sizeof(VTOY_GPT_INFO) + sizeof(VTOY_GPT_HDR));

    pBack = (VTOY_GPT_HDR *)(pGPT + 1);
    
    len = read(fd, pGPT, sizeof(VTOY_GPT_INFO));
    if (len != (ssize_t)sizeof(VTOY_GPT_INFO))
    {
        printf("Failed to read partition table %d err:%d\n", (int)len, errno);
        goto out;
    }

    if (pGPT->MBR.PartTbl[0].FsFlag == 0xEE && memcmp(pGPT->Head.Signature, "EFI PART", 8) == 0)
	{
        PartStyle = 1;
	}
	else
	{
        PartStyle = 0;
	}

    if (PartStyle == 0)
    {
		PART_TABLE *PartTbl = pGPT->MBR.PartTbl;

        for (i = 1; i < 4; i++)
        {
            if (PartTbl[i].SectorCount == 0)
            {
				break;
            }
        }

		if (i >= 4)
		{
			printf("###[FAIL] Can not find a free MBR partition table.\n");
			goto out;
		}

        for (j = i - 1; j > 0; j--)
		{
			printf("Move MBR partition table %d --> %d\n", j + 1, j + 2);
			memcpy(PartTbl + (j + 1), PartTbl + j, sizeof(PART_TABLE));
		}

        memset(PartTbl + 1, 0, sizeof(PART_TABLE));
        VentoyFillMBRLocation(DiskSizeInBytes, (UINT32)part2start, VENTOY_EFI_PART_SIZE / 512, PartTbl + 1);
		PartTbl[1].Active = 0x00;
		PartTbl[1].FsFlag = 0xEF; // EFI System Partition

        PartTbl[0].Active = 0x80; // bootable
        PartTbl[0].SectorCount = (UINT32)part2start - 2048;
        
        if (!WriteDataToPhyDisk(fd, 0, &(pGPT->MBR), 512))
		{
			printf("MBR write MBR failed\n");
			goto out;
		}

        fsync(fd);
        printf("MBR update partition table success.\n");
        rc = 0;
    }
    else
    {
		VTOY_GPT_PART_TBL *PartTbl = pGPT->PartTbl;

        for (i = 1; i < 128; i++)
        {
            if (memcmp(&(PartTbl[i].PartGuid), &g_ZeroGuid, sizeof(GUID)) == 0)
            {
				break;
            }
        }

		if (i >= 128)
		{
			printf("###[FAIL] Can not find a free GPT partition table.\n");
			goto out;
		}

		for (j = i - 1; j > 0; j--)
		{
			printf("Move GPT partition table %d --> %d\n", j + 1, j + 2);
			memcpy(PartTbl + (j + 1), PartTbl + j, sizeof(VTOY_GPT_PART_TBL));
		}

        // to fix windows issue
        memset(PartTbl + 1, 0, sizeof(VTOY_GPT_PART_TBL));
		memcpy(&(PartTbl[1].PartType), &g_WindowsDataPartGuid, sizeof(GUID));
		ventoy_gen_preudo_uuid(&(PartTbl[1].PartGuid));

        PartTbl[0].LastLBA = part2start - 1;

        PartTbl[1].StartLBA = PartTbl[0].LastLBA + 1;
		PartTbl[1].LastLBA = PartTbl[1].StartLBA + VENTOY_EFI_PART_SIZE / 512 - 1;
		PartTbl[1].Attr = VENTOY_EFI_PART_ATTR;
        PartTbl[1].Name[0] = 'V';
        PartTbl[1].Name[1] = 'T';
        PartTbl[1].Name[2] = 'O';
        PartTbl[1].Name[3] = 'Y';
        PartTbl[1].Name[4] = 'E';
        PartTbl[1].Name[5] = 'F';
        PartTbl[1].Name[6] = 'I';
        PartTbl[1].Name[7] = 0;

		//Update CRC
		pGPT->Head.PartTblCrc = VtoyCrc32(pGPT->PartTbl, sizeof(pGPT->PartTbl));
		pGPT->Head.Crc = 0;
		pGPT->Head.Crc = VtoyCrc32(&(pGPT->Head), pGPT->Head.Length);

		printf("pGPT->Head.EfiStartLBA=%llu\n", pGPT->Head.EfiStartLBA);
		printf("pGPT->Head.EfiBackupLBA=%llu\n", pGPT->Head.EfiBackupLBA);

		VentoyFillBackupGptHead(pGPT, pBack);
		if (!WriteDataToPhyDisk(fd, pGPT->Head.EfiBackupLBA * 512, pBack, 512))
		{
			printf("GPT write backup head failed\n");
			goto out;
		}

		if (!WriteDataToPhyDisk(fd, (pGPT->Head.EfiBackupLBA - 32) * 512, pGPT->PartTbl, 512 * 32))
		{
			printf("GPT write backup partition table failed\n");
			goto out;
		}

		if (!WriteDataToPhyDisk(fd, 0, pGPT, 512 * 34))
		{
			printf("GPT write MBR & Main partition table failed\n");
			goto out;
		}

        fsync(fd);
        printf("GPT update partition table success.\n");
        rc = 0;
    }

out:
    check_close(fd);        
    check_free(pGPT);
    return rc;
}

int partresize_main(int argc, char **argv)
{
    UINT64 sector;
    
    if (argc != 3 && argc != 4)
    {
        printf("usage: partresize -c/-f /dev/sdb\n");
        return 1;
    }

    if (strcmp(argv[1], "-c") == 0)
    {
        return part_check(argv[2]);
    }
    else if (strcmp(argv[1], "-s") == 0)
    {
        sector = strtoull(argv[3], NULL, 10);
        return secureboot_proc(argv[2], sector);
    }
    else if (strcmp(argv[1], "-p") == 0)
    {
        sector = strtoull(argv[3], NULL, 10);    
        return update_part_table(argv[2], sector);
    }
    else if (strcmp(argv[1], "-t") == 0)
    {
        return gpt_check(argv[2]);
    }
    else
    {
        return 1;
    }
}

