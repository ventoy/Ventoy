/******************************************************************************
 * Ventoy2Disk.c
 *
 * Copyright (c) 2020, longpanda <admin@ventoy.net>
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

#include <Windows.h>
#include "resource.h"
#include "Language.h"
#include "Ventoy2Disk.h"

PHY_DRIVE_INFO *g_PhyDriveList = NULL;
DWORD g_PhyDriveCount = 0;
static int g_FilterRemovable = 0;
int g_FilterUSB = 1;
int g_ForceOperation = 1;
int g_NoNeedInputYes = 0;
int g_WriteImage = 0;

int ParseCmdLineOption(LPSTR lpCmdLine)
{
    int i;
    char cfgfile[MAX_PATH];

    if (lpCmdLine && lpCmdLine[0])
    {
        Log("CmdLine:<%s>", lpCmdLine);
    }

    for (i = 0; i < __argc; i++)
    {
        if (strncmp(__argv[i], "-U", 2) == 0 ||
			strncmp(__argv[i], "-u", 2) == 0)
        {
            g_FilterUSB = 0;
        }
        else if (strncmp(__argv[i], "-F", 2) == 0)
        {
            g_ForceOperation = 1;
        }
        else if (strncmp(__argv[i], "-Y", 2) == 0 || strncmp(__argv[i], "-y", 2) == 0)
        {
            g_NoNeedInputYes = 1;
        }
    }

    GetCurrentDirectoryA(sizeof(cfgfile), cfgfile);
    strcat_s(cfgfile, sizeof(cfgfile), "\\Ventoy2Disk.ini");

    if (0 == GetPrivateProfileIntA("Filter", "USB", 1, cfgfile))
    {
        g_FilterUSB = 0;
    }

    if (1 == GetPrivateProfileIntA("Operation", "Force", 0, cfgfile))
    {
        g_ForceOperation = 1;
    }

    Log("Control Flag: %d %d %d", g_FilterRemovable, g_FilterUSB, g_ForceOperation);

    return 0;
}

BOOL IsVentoyPhyDrive(int PhyDrive, UINT64 SizeBytes, MBR_HEAD *pMBR, UINT64 *Part2StartSector, UINT64 *GptPart2Attr)
{
    int i;
    BOOL bRet;
    DWORD dwSize;
    HANDLE hDrive;
    MBR_HEAD MBR;
    UINT32 PartStartSector;
    UINT32 PartSectorCount;
    CHAR PhyDrivePath[128];
    CHAR GUIDStr[128];
    GUID ZeroGuid = { 0 };
	VTOY_GPT_INFO *pGpt = NULL;

    safe_sprintf(PhyDrivePath, "\\\\.\\PhysicalDrive%d", PhyDrive);
    hDrive = CreateFileA(PhyDrivePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    Log("Create file Handle:%p %s status:%u", hDrive, PhyDrivePath, LASTERR);

    if (hDrive == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

	bRet = ReadFile(hDrive, &MBR, sizeof(MBR), &dwSize, NULL);
	Log("Read MBR Ret:%u Size:%u code:%u", bRet, dwSize, LASTERR);

    if ((!bRet) || (dwSize != sizeof(MBR)))
    {
		CHECK_CLOSE_HANDLE(hDrive);
        return FALSE;
    }

    if (MBR.Byte55 != 0x55 || MBR.ByteAA != 0xAA)
    {
        Log("Byte55 ByteAA not match 0x%x 0x%x", MBR.Byte55, MBR.ByteAA);
		CHECK_CLOSE_HANDLE(hDrive);
        return FALSE;
    }

    

	if (MBR.PartTbl[0].FsFlag == 0xEE)
	{
		pGpt = malloc(sizeof(VTOY_GPT_INFO));
		if (!pGpt)
		{
			CHECK_CLOSE_HANDLE(hDrive);
			return FALSE;
		}

		SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);
		bRet = ReadFile(hDrive, pGpt, sizeof(VTOY_GPT_INFO), &dwSize, NULL);
		CHECK_CLOSE_HANDLE(hDrive);
		if ((!bRet) || (dwSize != sizeof(VTOY_GPT_INFO)))
		{
			Log("Failed to read gpt info %d %u %d", bRet, dwSize, LASTERR);
			return FALSE;
		}

		if (memcmp(pGpt->Head.Signature, "EFI PART", 8))
		{
			Log("Invalid GPT signature");
			return FALSE;
		}

        for (i = 0; i < 128; i++)
        {
            if (memcmp(&(pGpt->PartTbl[i].PartGuid), &ZeroGuid, sizeof(GUID)) == 0)
            {
                continue;
            }

            Log("=========== Disk%d GPT Partition %d ============", PhyDrive, i + 1);
            
            Log("PartTbl.PartType = %s", GUID2String(&pGpt->PartTbl[i].PartType, GUIDStr, sizeof(GUIDStr)));
            Log("PartTbl.PartGuid = %s", GUID2String(&pGpt->PartTbl[i].PartGuid, GUIDStr, sizeof(GUIDStr)));
            Log("PartTbl.StartLBA = %llu", (ULONGLONG)pGpt->PartTbl[i].StartLBA);
            Log("PartTbl.LastLBA = %llu", (ULONGLONG)pGpt->PartTbl[i].LastLBA);
            Log("PartTbl.Attribute = 0x%llx", pGpt->PartTbl[i].Attr);
            Log("PartTbl.Name = %S", pGpt->PartTbl[i].Name);
        }

		if (memcmp(pGpt->PartTbl[1].Name, L"VTOYEFI", 7 * 2))
		{
			if (pGpt->PartTbl[1].Name[0])
			{
				Log("Invalid ventoy efi part name <%S>", pGpt->PartTbl[1].Name);
			}
			else
			{
				Log("Invalid ventoy efi part name <null>");
			}
			
			return FALSE;
		}

        if (pGpt->PartTbl[0].StartLBA != 2048)
        {
            Log("Part1 not match %llu", pGpt->PartTbl[0].StartLBA);
            return FALSE;
        }

        PartSectorCount = VENTOY_EFI_PART_SIZE / 512;

        if (pGpt->PartTbl[1].StartLBA != pGpt->PartTbl[0].LastLBA + 1 ||
            (UINT32)(pGpt->PartTbl[1].LastLBA + 1 - pGpt->PartTbl[1].StartLBA) != PartSectorCount)
        {
            Log("Part2 not match [%llu %llu] [%llu %llu]",
                pGpt->PartTbl[0].StartLBA, pGpt->PartTbl[0].LastLBA,
                pGpt->PartTbl[1].StartLBA, pGpt->PartTbl[1].LastLBA);
            return FALSE;
        }

        *GptPart2Attr = pGpt->PartTbl[1].Attr;
		*Part2StartSector = pGpt->PartTbl[1].StartLBA;

        memcpy(pMBR, &(pGpt->MBR), sizeof(MBR_HEAD));
	}
	else
	{
		CHECK_CLOSE_HANDLE(hDrive);

        for (i = 0; i < 4; i++)
        {
            Log("=========== Disk%d MBR Partition %d ============", PhyDrive, i + 1);
            Log("PartTbl.Active = 0x%x", MBR.PartTbl[i].Active);
            Log("PartTbl.FsFlag = 0x%x", MBR.PartTbl[i].FsFlag);
            Log("PartTbl.StartSectorId = %u", MBR.PartTbl[i].StartSectorId);
            Log("PartTbl.SectorCount = %u", MBR.PartTbl[i].SectorCount);
            Log("PartTbl.StartHead = %u", MBR.PartTbl[i].StartHead);
            Log("PartTbl.StartSector = %u", MBR.PartTbl[i].StartSector);
            Log("PartTbl.StartCylinder = %u", MBR.PartTbl[i].StartCylinder);
            Log("PartTbl.EndHead = %u", MBR.PartTbl[i].EndHead);
            Log("PartTbl.EndSector = %u", MBR.PartTbl[i].EndSector);
            Log("PartTbl.EndCylinder = %u", MBR.PartTbl[i].EndCylinder);
        }

		if (MBR.PartTbl[0].StartSectorId != 2048)
		{
			Log("Part1 not match %u", MBR.PartTbl[0].StartSectorId);
			return FALSE;
		}

		PartStartSector = MBR.PartTbl[0].StartSectorId + MBR.PartTbl[0].SectorCount;
		PartSectorCount = VENTOY_EFI_PART_SIZE / 512;

		if (MBR.PartTbl[1].StartSectorId != PartStartSector ||
			MBR.PartTbl[1].SectorCount != PartSectorCount)
		{
			Log("Part2 not match [0x%x 0x%x] [%u %u] [%u %u]",
				MBR.PartTbl[1].FsFlag, 0xEF,
				MBR.PartTbl[1].StartSectorId, PartStartSector,
				MBR.PartTbl[1].SectorCount, PartSectorCount);
			return FALSE;
		}

		if (MBR.PartTbl[0].Active != 0x80 && MBR.PartTbl[1].Active != 0x80)
		{
			Log("Part1 and Part2 are both NOT active 0x%x 0x%x", MBR.PartTbl[0].Active, MBR.PartTbl[1].Active);
            if (MBR.PartTbl[2].Active != 0x80 && MBR.PartTbl[3].Active != 0x80)
            {
                Log("Part3 and Part4 are both NOT active 0x%x 0x%x", MBR.PartTbl[2].Active, MBR.PartTbl[3].Active);
                //return FALSE;
            }
		}

		*Part2StartSector = MBR.PartTbl[1].StartSectorId;

        memcpy(pMBR, &MBR, sizeof(MBR_HEAD));
	}

    Log("PhysicalDrive%d is ventoy disk", PhyDrive);
    return TRUE;
}

int GetVolumeClusterSize(char Drive)
{
    CHAR Volume[32] = { 0 };
    DWORD SectorsPerCluster = 0;
    DWORD BytesPerSector = 0;
    DWORD NumberOfFreeClusters = 0;
    DWORD TotalNumberOfClusters = 0;

    sprintf_s(Volume, sizeof(Volume), "%C:\\", Drive);

    if (GetDiskFreeSpaceA(Volume, &SectorsPerCluster, &BytesPerSector, &NumberOfFreeClusters, &TotalNumberOfClusters))
    {
        Log("GetVolumeClusterSize %s SUCCESS %u %u %u", Volume, SectorsPerCluster, BytesPerSector, SectorsPerCluster * BytesPerSector);
        return (int)(SectorsPerCluster * BytesPerSector);        
    }
    else
    {
        Log("GetVolumeClusterSize %s failed err:%u", Volume, LASTERR);
    }

    return 0;
}

int GetVentoyFsNameInPhyDrive(PHY_DRIVE_INFO* CurDrive)
{
    int i = 0;
    UINT64 Offset;

    CHAR Volume[128] = { 0 };
    CHAR FsName[MAX_PATH] = { 0 };

    while (CurDrive->DriveLetters[i])
    {
        if (GetPhyDriveByLogicalDrive(CurDrive->DriveLetters[i], &Offset) >= 0)
        {
            if (Offset == SIZE_1MB)
            {
                sprintf_s(Volume, sizeof(Volume), "%C:\\", CurDrive->DriveLetters[i]);
                Log("Find the partition 1 logical drive is %s", Volume);
                break;
            }
        }
        i++;
    }

    sprintf_s(CurDrive->VentoyFsType, sizeof(CurDrive->VentoyFsType), "??");

    if (Volume[0])
    {
        CurDrive->VentoyFsClusterSize = GetVolumeClusterSize(Volume[0]);

        if (GetVolumeInformationA(Volume, NULL, 0, NULL, NULL, NULL, FsName, MAX_PATH))
        {
            if (_stricmp(FsName, "exFAT") == 0)
            {
                sprintf_s(CurDrive->VentoyFsType, sizeof(CurDrive->VentoyFsType), "exFAT");
            }
            else if (_stricmp(FsName, "NTFS") == 0)
            {
                sprintf_s(CurDrive->VentoyFsType, sizeof(CurDrive->VentoyFsType), "NTFS");
            }
            else if (_stricmp(FsName, "FAT") == 0 || _stricmp(FsName, "FAT32") == 0)
            {
                sprintf_s(CurDrive->VentoyFsType, sizeof(CurDrive->VentoyFsType), "FAT32");
            }
            else
            {
                sprintf_s(CurDrive->VentoyFsType, sizeof(CurDrive->VentoyFsType), "%s", FsName);
            }

            Log("GetVentoyFsNameInPhyDrive %d %s <%s> <%s>", CurDrive->PhyDrive, Volume, FsName, CurDrive->VentoyFsType);
        }
        else
        {
            Log("GetVolumeInformationA %s failed %u", Volume, LASTERR);
        }
    }
    else
    {
        Log("GetVentoyFsNameInPhyDrive %s not found", Volume);
    }

    return 0;
}

static int FilterPhysicalDrive(PHY_DRIVE_INFO *pDriveList, DWORD DriveCount)
{
    DWORD i; 
    DWORD LogDrive;
    int Count = 0;
    int Letter = 'A';
    int Id = 0;
    int LetterCount = 0;
    UINT64 Part2GPTAttr = 0;
	UINT64 Part2StartSector = 0;
    PHY_DRIVE_INFO *CurDrive;
	MBR_HEAD MBR;
    int LogLetter[VENTOY_MAX_PHY_DRIVE];
    int PhyDriveId[VENTOY_MAX_PHY_DRIVE];

    for (LogDrive = GetLogicalDrives(); LogDrive > 0; LogDrive >>= 1)
    {
        if (LogDrive & 0x01)
        {
            LogLetter[LetterCount] = Letter;
			PhyDriveId[LetterCount] = GetPhyDriveByLogicalDrive(Letter, NULL);

            Log("Logical Drive:%C  ===> PhyDrive:%d", LogLetter[LetterCount], PhyDriveId[LetterCount]);
            LetterCount++;
        }
        
        Letter++;
    }    

    for (i = 0; i < DriveCount; i++)
    {
        Part2GPTAttr = 0;
        CurDrive = pDriveList + i;

        CurDrive->Id = -1;
        memset(CurDrive->DriveLetters, 0, sizeof(CurDrive->DriveLetters));

        if (g_FilterRemovable && (!CurDrive->RemovableMedia))
        {
            Log("<%s %s> is filtered for not removable.", CurDrive->VendorId, CurDrive->ProductId);
            continue;
        }

        if (g_FilterUSB && CurDrive->BusType != BusTypeUsb)
        {
            Log("<%s %s> is filtered for not USB type.", CurDrive->VendorId, CurDrive->ProductId);
            continue;
        }
        
        CurDrive->Id = Id++;

        for (Count = 0, Letter = 0; Letter < LetterCount; Letter++)
        {
            if (PhyDriveId[Letter] == CurDrive->PhyDrive)
            {
                if (Count + 1 < sizeof(CurDrive->DriveLetters) / sizeof(CHAR))
                {
                    CurDrive->DriveLetters[Count] = LogLetter[Letter];
                }
                Count++;
            }
        }

        if (IsVentoyPhyDrive(CurDrive->PhyDrive, CurDrive->SizeInBytes, &MBR, &Part2StartSector, &Part2GPTAttr))
        {
            memcpy(&(CurDrive->MBR), &MBR, sizeof(MBR));
            CurDrive->PartStyle = (MBR.PartTbl[0].FsFlag == 0xEE) ? 1 : 0;
            CurDrive->Part2GPTAttr = Part2GPTAttr;
            GetVentoyVerInPhyDrive(CurDrive, Part2StartSector, CurDrive->VentoyVersion, sizeof(CurDrive->VentoyVersion), &(CurDrive->SecureBootSupport));
            Log("PhyDrive %d is Ventoy Disk ver:%s SecureBoot:%u", CurDrive->PhyDrive, CurDrive->VentoyVersion, CurDrive->SecureBootSupport);

            GetVentoyFsNameInPhyDrive(CurDrive);

            if (CurDrive->VentoyVersion[0] == 0)
            {
                CurDrive->VentoyVersion[0] = '?';
                Log("Unknown Ventoy Version");
            }
        }
    }

    // for safe
    for (i = 0; i < DriveCount; i++)
    {
        CurDrive = pDriveList + i;
        if (CurDrive->Id < 0)
        {
            CurDrive->PhyDrive = 0x00FFFFFF;
        }
    }

    return Id;
}

PHY_DRIVE_INFO * GetPhyDriveInfoById(int Id)
{
    DWORD i;
    for (i = 0; i < g_PhyDriveCount; i++)
    {
        if (g_PhyDriveList[i].Id >= 0 && g_PhyDriveList[i].Id == Id)
        {
            return g_PhyDriveList + i;
        }
    }

    return NULL;
}


PHY_DRIVE_INFO * GetPhyDriveInfoByPhyDrive(int PhyDrive)
{
	DWORD i;
	for (i = 0; i < g_PhyDriveCount; i++)
	{
		if (g_PhyDriveList[i].PhyDrive == PhyDrive)
		{
			return g_PhyDriveList + i;
		}
	}

	return NULL;
}



int SortPhysicalDrive(PHY_DRIVE_INFO *pDriveList, DWORD DriveCount)
{
	DWORD i, j;
	BOOL flag;
	PHY_DRIVE_INFO TmpDriveInfo;

	for (i = 0; i < DriveCount; i++)
	{
		for (j = i + 1; j < DriveCount; j++)
		{
			flag = FALSE;

			if (pDriveList[i].BusType == BusTypeUsb && pDriveList[j].BusType == BusTypeUsb)
			{
				if (pDriveList[i].RemovableMedia == FALSE && pDriveList[j].RemovableMedia == TRUE)
				{
					flag = TRUE;
				}
			}
			else if (pDriveList[j].BusType == BusTypeUsb)
			{
				flag = TRUE;
			}
			else
			{
				if (pDriveList[j].PhyDrive < pDriveList[i].PhyDrive)
				{
					flag = TRUE;
				}
			}

			if (flag)
			{
				memcpy(&TmpDriveInfo, pDriveList + i, sizeof(PHY_DRIVE_INFO));
				memcpy(pDriveList + i, pDriveList + j, sizeof(PHY_DRIVE_INFO));
				memcpy(pDriveList + j, &TmpDriveInfo, sizeof(PHY_DRIVE_INFO));
			}
		}
	}

	return 0;
}

int Ventoy2DiskInit(void)
{
    Log("\n===================== Enum All PhyDrives =====================");
    g_PhyDriveList = (PHY_DRIVE_INFO *)malloc(sizeof(PHY_DRIVE_INFO)* VENTOY_MAX_PHY_DRIVE);
    if (NULL == g_PhyDriveList)
    {
        Log("Failed to alloc phy drive memory");
        return FALSE;
    }
    memset(g_PhyDriveList, 0, sizeof(PHY_DRIVE_INFO)* VENTOY_MAX_PHY_DRIVE);

    GetAllPhysicalDriveInfo(g_PhyDriveList, &g_PhyDriveCount);

	SortPhysicalDrive(g_PhyDriveList, g_PhyDriveCount);

    FilterPhysicalDrive(g_PhyDriveList, g_PhyDriveCount);

    return 0;
}

int Ventoy2DiskDestroy(void)
{
    free(g_PhyDriveList);
	g_PhyDriveList = NULL;
	g_PhyDriveCount = 0;

    return 0;
}
