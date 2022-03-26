/******************************************************************************
 * PhyDrive.c
 *
 * Copyright (c) 2020, longpanda <admin@ventoy.net>
 * Copyright (c) 2011-2020, Pete Batard <pete@akeo.ie>
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
#include <time.h>
#include <winternl.h>
#include <commctrl.h>
#include <initguid.h>
#include "resource.h"
#include "Language.h"
#include "Ventoy2Disk.h"
#include "fat_filelib.h"
#include "ff.h"
#include "DiskService.h"

static int g_backup_bin_index = 0;


static BOOL WriteDataToPhyDisk(HANDLE hDrive, UINT64 Offset, VOID *buffer, DWORD len)
{
	BOOL bRet;
	DWORD dwSize = 0;
	LARGE_INTEGER liCurPosition;
	LARGE_INTEGER liNewPosition;

	liCurPosition.QuadPart = (LONGLONG)Offset;
	liNewPosition.QuadPart = 0;
	if (0 == SetFilePointerEx(hDrive, liCurPosition, &liNewPosition, FILE_BEGIN) ||
		liNewPosition.QuadPart != liCurPosition.QuadPart)
	{
		Log("SetFilePointerEx Failed %u", LASTERR);
		return FALSE;
	}

	bRet = WriteFile(hDrive, buffer, len, &dwSize, NULL);
	if (bRet == FALSE || dwSize != len)
	{
		Log("Write file error %u %u", dwSize, LASTERR);
		return FALSE;
	}

	return TRUE;
}


static DWORD GetVentoyVolumeName(int PhyDrive, UINT64 StartSectorId, CHAR *NameBuf, UINT32 BufLen, BOOL DelSlash)
{
    size_t len;
    BOOL bRet;
    DWORD dwSize;
    HANDLE hDrive;
    HANDLE hVolume;
    UINT64 PartOffset;
    DWORD Status = ERROR_NOT_FOUND;
    DISK_EXTENT *pExtents = NULL;
    CHAR VolumeName[MAX_PATH] = { 0 };
    VOLUME_DISK_EXTENTS DiskExtents;

    PartOffset = 512ULL * StartSectorId;

	Log("GetVentoyVolumeName PhyDrive %d SectorStart:%llu PartOffset:%llu", PhyDrive, (ULONGLONG)StartSectorId, (ULONGLONG)PartOffset);

    hVolume = FindFirstVolumeA(VolumeName, sizeof(VolumeName));
    if (hVolume == INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    do {

        len = strlen(VolumeName);
        Log("Find volume:%s", VolumeName);

        VolumeName[len - 1] = 0;

        hDrive = CreateFileA(VolumeName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hDrive == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        bRet = DeviceIoControl(hDrive,
            IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
            NULL,
            0,
            &DiskExtents,
            (DWORD)(sizeof(DiskExtents)),
            (LPDWORD)&dwSize,
            NULL);

        Log("IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS bRet:%u code:%u", bRet, LASTERR);
        Log("NumberOfDiskExtents:%u DiskNumber:%u", DiskExtents.NumberOfDiskExtents, DiskExtents.Extents[0].DiskNumber);

        if (bRet && DiskExtents.NumberOfDiskExtents == 1)
        {
            pExtents = DiskExtents.Extents;

            Log("This volume DiskNumber:%u offset:%llu", pExtents->DiskNumber, (ULONGLONG)pExtents->StartingOffset.QuadPart);
            if ((int)pExtents->DiskNumber == PhyDrive && pExtents->StartingOffset.QuadPart == PartOffset)
            {
                Log("This volume match");

                if (!DelSlash)
                {
                    VolumeName[len - 1] = '\\';
                }

                sprintf_s(NameBuf, BufLen, "%s", VolumeName);
                Status = ERROR_SUCCESS;
                CloseHandle(hDrive);
                break;
            }
        }

        CloseHandle(hDrive);
    } while (FindNextVolumeA(hVolume, VolumeName, sizeof(VolumeName)));

    FindVolumeClose(hVolume);

    Log("GetVentoyVolumeName return %u", Status);
    return Status;
}

static int GetLettersBelongPhyDrive(int PhyDrive, char *DriveLetters, size_t Length)
{
    int n = 0;
    DWORD DataSize = 0;
    CHAR *Pos = NULL;
    CHAR *StringBuf = NULL;

    DataSize = GetLogicalDriveStringsA(0, NULL);
    StringBuf = (CHAR *)malloc(DataSize + 1);
    if (StringBuf == NULL)
    {
        return 1;
    }

    GetLogicalDriveStringsA(DataSize, StringBuf);

    for (Pos = StringBuf; *Pos; Pos += strlen(Pos) + 1)
    {
        if (n < (int)Length && PhyDrive == GetPhyDriveByLogicalDrive(Pos[0], NULL))
        {
            Log("%C: is belong to phydrive%d", Pos[0], PhyDrive);
            DriveLetters[n++] = Pos[0];
        }
    }

    free(StringBuf);
    return 0;
}

HANDLE GetPhysicalHandle(int Drive, BOOLEAN bLockDrive, BOOLEAN bWriteAccess, BOOLEAN bWriteShare)
{
    int i;
    DWORD dwSize;
    DWORD LastError;
    UINT64 EndTime;
    HANDLE hDrive = INVALID_HANDLE_VALUE;
    CHAR PhyDrive[128];
    CHAR DevPath[MAX_PATH] = { 0 };

    safe_sprintf(PhyDrive, "\\\\.\\PhysicalDrive%d", Drive);

    if (0 == QueryDosDeviceA(PhyDrive + 4, DevPath, sizeof(DevPath)))
    {
        Log("QueryDosDeviceA failed error:%u", GetLastError());
        strcpy_s(DevPath, sizeof(DevPath), "???");
    }
    else
    {
        Log("QueryDosDeviceA success %s", DevPath);
    }

    for (i = 0; i < DRIVE_ACCESS_RETRIES; i++)
    {
        // Try without FILE_SHARE_WRITE (unless specifically requested) so that
        // we won't be bothered by the OS or other apps when we set up our data.
        // However this means we might have to wait for an access gap...
        // We keep FILE_SHARE_READ though, as this shouldn't hurt us any, and is
        // required for enumeration.
        hDrive = CreateFileA(PhyDrive,
            GENERIC_READ | (bWriteAccess ? GENERIC_WRITE : 0),
            FILE_SHARE_READ | (bWriteShare ? FILE_SHARE_WRITE : 0),
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
            NULL);

        LastError = GetLastError();
        Log("[%d] CreateFileA %s code:%u %p", i, PhyDrive, LastError, hDrive);

        if (hDrive != INVALID_HANDLE_VALUE)
        {
            break;
        }

        if ((LastError != ERROR_SHARING_VIOLATION) && (LastError != ERROR_ACCESS_DENIED))
        {
            break;
        }

        if (i == 0)
        {
            Log("Waiting for access on %s [%s]...", PhyDrive, DevPath);
        }
        else if (!bWriteShare && (i > DRIVE_ACCESS_RETRIES / 3))
        {
            // If we can't seem to get a hold of the drive for some time, try to enable FILE_SHARE_WRITE...
            Log("Warning: Could not obtain exclusive rights. Retrying with write sharing enabled...");
            bWriteShare = TRUE;

            // Try to report the process that is locking the drive
            // We also use bit 6 as a flag to indicate that SearchProcess was called.
            //access_mask = SearchProcess(DevPath, SEARCH_PROCESS_TIMEOUT, TRUE, TRUE, FALSE) | 0x40;

        }
        Sleep(DRIVE_ACCESS_TIMEOUT / DRIVE_ACCESS_RETRIES);
    }

    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Could not open %s %u", PhyDrive, LASTERR);
        goto End;
    }

    if (bWriteAccess)
    {
        Log("Opened %s for %s write access", PhyDrive, bWriteShare ? "shared" : "exclusive");
    }

    if (bLockDrive)
    {
        if (DeviceIoControl(hDrive, FSCTL_ALLOW_EXTENDED_DASD_IO, NULL, 0, NULL, 0, &dwSize, NULL))
        {
            Log("I/O boundary checks disabled");
        }

        EndTime = GetTickCount64() + DRIVE_ACCESS_TIMEOUT;

        do {
            if (DeviceIoControl(hDrive, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL))
            {
                Log("FSCTL_LOCK_VOLUME success");
                goto End;
            }
            Sleep(DRIVE_ACCESS_TIMEOUT / DRIVE_ACCESS_RETRIES);
        } while (GetTickCount64() < EndTime);

        // If we reached this section, either we didn't manage to get a lock or the user cancelled
        Log("Could not lock access to %s %u", PhyDrive, LASTERR);

        // See if we can report the processes are accessing the drive
        //if (!IS_ERROR(FormatStatus) && (access_mask == 0))
        //    access_mask = SearchProcess(DevPath, SEARCH_PROCESS_TIMEOUT, TRUE, TRUE, FALSE);
        // Try to continue if the only access rights we saw were for read-only
        //if ((access_mask & 0x07) != 0x01)
        //    safe_closehandle(hDrive);

        CHECK_CLOSE_HANDLE(hDrive);
    }

End:

    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Can get handle of %s, maybe some process control it.", DevPath);
    }

    return hDrive;
}

int GetPhyDriveByLogicalDrive(int DriveLetter, UINT64 *Offset)
{
    BOOL Ret;
    DWORD dwSize;
    HANDLE Handle;
    VOLUME_DISK_EXTENTS DiskExtents;
    CHAR PhyPath[128];

    safe_sprintf(PhyPath, "\\\\.\\%C:", (CHAR)DriveLetter);

    Handle = CreateFileA(PhyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        Log("Could not open the disk<%s>, error:%u", PhyPath, LASTERR);
        return -1;
    }

    Ret = DeviceIoControl(Handle,
        IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
        NULL,
        0,
        &DiskExtents,
        (DWORD)(sizeof(DiskExtents)),
        (LPDWORD)&dwSize,
        NULL);

    if (!Ret || DiskExtents.NumberOfDiskExtents == 0)
    {
        Log("DeviceIoControl IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed %s, error:%u", PhyPath, LASTERR);
        CHECK_CLOSE_HANDLE(Handle);
        return -1;
    }
    CHECK_CLOSE_HANDLE(Handle);

    Log("LogicalDrive:%s PhyDrive:%d Offset:%llu ExtentLength:%llu",
        PhyPath,
        DiskExtents.Extents[0].DiskNumber,
        DiskExtents.Extents[0].StartingOffset.QuadPart,
        DiskExtents.Extents[0].ExtentLength.QuadPart
        );

	if (Offset)
	{
		*Offset = (UINT64)(DiskExtents.Extents[0].StartingOffset.QuadPart);
	}

    return (int)DiskExtents.Extents[0].DiskNumber;
}

int GetAllPhysicalDriveInfo(PHY_DRIVE_INFO *pDriveList, DWORD *pDriveCount)
{
    int i;
    int Count;
    int id;
    int Letter = 'A';
    BOOL  bRet;
    DWORD dwBytes;
    DWORD DriveCount = 0;
    HANDLE Handle = INVALID_HANDLE_VALUE;
    CHAR PhyDrive[128];
    PHY_DRIVE_INFO *CurDrive = pDriveList;
    GET_LENGTH_INFORMATION LengthInfo;
    STORAGE_PROPERTY_QUERY Query;
    STORAGE_DESCRIPTOR_HEADER DevDescHeader;
    STORAGE_DEVICE_DESCRIPTOR *pDevDesc;
    int PhyDriveId[VENTOY_MAX_PHY_DRIVE];

    Count = GetPhysicalDriveCount();

    for (i = 0; i < Count && i < VENTOY_MAX_PHY_DRIVE; i++)
    {
        PhyDriveId[i] = i;
    }

    dwBytes = GetLogicalDrives();
    Log("Logical Drives: 0x%x", dwBytes);
    while (dwBytes)
    {
        if (dwBytes & 0x01)
        {
			id = GetPhyDriveByLogicalDrive(Letter, NULL);
            Log("%C --> %d", Letter, id);
            if (id >= 0)
            {
                for (i = 0; i < Count; i++)
                {
                    if (PhyDriveId[i] == id)
                    {
                        break;
                    }
                }

                if (i >= Count)
                {
                    Log("Add phy%d to list", i);
                    PhyDriveId[Count] = id;
                    Count++;
                }
            }
        }

        Letter++;
        dwBytes >>= 1;
    }

    for (i = 0; i < Count && DriveCount < VENTOY_MAX_PHY_DRIVE; i++)
    {
        CHECK_CLOSE_HANDLE(Handle);

        safe_sprintf(PhyDrive, "\\\\.\\PhysicalDrive%d", PhyDriveId[i]);
        Handle = CreateFileA(PhyDrive, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);        
        Log("Create file Handle:%p %s status:%u", Handle, PhyDrive, LASTERR);

        if (Handle == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        bRet = DeviceIoControl(Handle,
                               IOCTL_DISK_GET_LENGTH_INFO, NULL,
                               0,
                               &LengthInfo,
                               sizeof(LengthInfo),
                               &dwBytes,
                               NULL);
        if (!bRet)
        {
            Log("DeviceIoControl IOCTL_DISK_GET_LENGTH_INFO failed error:%u", LASTERR);
            continue;
        }

        Log("PHYSICALDRIVE%d size %llu bytes", i, (ULONGLONG)LengthInfo.Length.QuadPart);

        Query.PropertyId = StorageDeviceProperty;
        Query.QueryType = PropertyStandardQuery;

        bRet = DeviceIoControl(Handle,
                               IOCTL_STORAGE_QUERY_PROPERTY,
                               &Query,
                               sizeof(Query),
                               &DevDescHeader,
                               sizeof(STORAGE_DESCRIPTOR_HEADER),
                               &dwBytes,
                               NULL);
        if (!bRet)
        {
            Log("DeviceIoControl1 error:%u dwBytes:%u", LASTERR, dwBytes);
            continue;
        }

        if (DevDescHeader.Size < sizeof(STORAGE_DEVICE_DESCRIPTOR))
        {
            Log("Invalid DevDescHeader.Size:%u", DevDescHeader.Size);
            continue;
        }

        pDevDesc = (STORAGE_DEVICE_DESCRIPTOR *)malloc(DevDescHeader.Size);
        if (!pDevDesc)
        {
            Log("failed to malloc error:%u len:%u", LASTERR, DevDescHeader.Size);
            continue;
        }

        bRet = DeviceIoControl(Handle,
                               IOCTL_STORAGE_QUERY_PROPERTY,
                               &Query,
                               sizeof(Query),
                               pDevDesc,
                               DevDescHeader.Size,
                               &dwBytes,
                               NULL);
        if (!bRet)
        {
            Log("DeviceIoControl2 error:%u dwBytes:%u", LASTERR, dwBytes);
            free(pDevDesc);
            continue;
        }

        CurDrive->PhyDrive = i;
        CurDrive->SizeInBytes = LengthInfo.Length.QuadPart;
        CurDrive->DeviceType = pDevDesc->DeviceType;
        CurDrive->RemovableMedia = pDevDesc->RemovableMedia;
        CurDrive->BusType = pDevDesc->BusType;

        if (pDevDesc->VendorIdOffset)
        {
            safe_strcpy(CurDrive->VendorId, (char *)pDevDesc + pDevDesc->VendorIdOffset);
            TrimString(CurDrive->VendorId);
        }

        if (pDevDesc->ProductIdOffset)
        {
            safe_strcpy(CurDrive->ProductId, (char *)pDevDesc + pDevDesc->ProductIdOffset);
            TrimString(CurDrive->ProductId);
        }

        if (pDevDesc->ProductRevisionOffset)
        {
            safe_strcpy(CurDrive->ProductRev, (char *)pDevDesc + pDevDesc->ProductRevisionOffset);
            TrimString(CurDrive->ProductRev);
        }

        if (pDevDesc->SerialNumberOffset)
        {
            safe_strcpy(CurDrive->SerialNumber, (char *)pDevDesc + pDevDesc->SerialNumberOffset);
            TrimString(CurDrive->SerialNumber);
        }

        CurDrive++;
        DriveCount++;

        free(pDevDesc);

        CHECK_CLOSE_HANDLE(Handle);
    }

    for (i = 0, CurDrive = pDriveList; i < (int)DriveCount; i++, CurDrive++)
    {
        Log("PhyDrv:%d BusType:%-4s Removable:%u Size:%dGB(%llu) Name:%s %s",
            CurDrive->PhyDrive, GetBusTypeString(CurDrive->BusType), CurDrive->RemovableMedia,
            GetHumanReadableGBSize(CurDrive->SizeInBytes), CurDrive->SizeInBytes,
            CurDrive->VendorId, CurDrive->ProductId);
    }

    *pDriveCount = DriveCount;

    return 0;
}


static HANDLE g_FatPhyDrive;
static UINT64 g_Part2StartSec;
static int GetVentoyVersionFromFatFile(CHAR *VerBuf, size_t BufLen)
{
    int rc = 1;
    int size = 0;
    char *buf = NULL;
    void *flfile = NULL;

    flfile = fl_fopen("/grub/grub.cfg", "rb");
    if (flfile)
    {
        fl_fseek(flfile, 0, SEEK_END);
        size = (int)fl_ftell(flfile);

        fl_fseek(flfile, 0, SEEK_SET);

        buf = (char *)malloc(size + 1);
        if (buf)
        {
            fl_fread(buf, 1, size, flfile);
            buf[size] = 0;

            rc = 0;
            sprintf_s(VerBuf, BufLen, "%s", ParseVentoyVersionFromString(buf));
            free(buf);
        }

        fl_fclose(flfile);
    }

    return rc;
}

static int VentoyFatDiskRead(uint32 Sector, uint8 *Buffer, uint32 SectorCount)
{
    DWORD dwSize;
    BOOL bRet;
    DWORD ReadSize;
    LARGE_INTEGER liCurrentPosition;

    liCurrentPosition.QuadPart = Sector + g_Part2StartSec;
    liCurrentPosition.QuadPart *= 512;
    SetFilePointerEx(g_FatPhyDrive, liCurrentPosition, &liCurrentPosition, FILE_BEGIN);

    ReadSize = (DWORD)(SectorCount * 512);

    bRet = ReadFile(g_FatPhyDrive, Buffer, ReadSize, &dwSize, NULL);
    if (bRet == FALSE || dwSize != ReadSize)
    {
        Log("ReadFile error bRet:%u WriteSize:%u dwSize:%u ErrCode:%u\n", bRet, ReadSize, dwSize, LASTERR);
    }

    return 1;
}


int GetVentoyVerInPhyDrive(const PHY_DRIVE_INFO *pDriveInfo, UINT64 Part2StartSector, CHAR *VerBuf, size_t BufLen, BOOL *pSecureBoot)
{
    int rc = 0;
    HANDLE hDrive;
    void *flfile;

    hDrive = GetPhysicalHandle(pDriveInfo->PhyDrive, FALSE, FALSE, FALSE);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        return 1;
    }
    
    g_FatPhyDrive = hDrive;
	g_Part2StartSec = Part2StartSector;

    Log("Parse FAT fs...");

    fl_init();

    if (0 == fl_attach_media(VentoyFatDiskRead, NULL))
    {
        Log("attach media success...");
        rc = GetVentoyVersionFromFatFile(VerBuf, BufLen);
    }
    else
    {
        Log("attach media failed...");
        rc = 1;
    }

    Log("GetVentoyVerInPhyDrive rc=%d...", rc);
    if (rc == 0)
    {
        Log("VentoyVerInPhyDrive %d is <%s>...", pDriveInfo->PhyDrive, VerBuf);

        flfile = fl_fopen("/EFI/BOOT/grubx64_real.efi", "rb");
        if (flfile)
        {
            *pSecureBoot = TRUE;
            fl_fclose(flfile);
        }
    }

    fl_shutdown();

    CHECK_CLOSE_HANDLE(hDrive);

    return rc;
}





static unsigned int g_disk_unxz_len = 0;
static BYTE *g_part_img_pos = NULL;
static BYTE *g_part_img_buf[VENTOY_EFI_PART_SIZE / SIZE_1MB];


static int VentoyFatMemRead(uint32 Sector, uint8 *Buffer, uint32 SectorCount)
{
	uint32 i;
	uint32 offset;
	BYTE *MbBuf = NULL;

	for (i = 0; i < SectorCount; i++)
	{
		offset = (Sector + i) * 512;

		if (g_part_img_buf[1] == NULL)
		{
			MbBuf = g_part_img_buf[0] + offset;
			memcpy(Buffer + i * 512, MbBuf, 512);
		}
		else
		{
			MbBuf = g_part_img_buf[offset / SIZE_1MB];
			memcpy(Buffer + i * 512, MbBuf + (offset % SIZE_1MB), 512);
		}
	}

	return 1;
}


static int VentoyFatMemWrite(uint32 Sector, uint8 *Buffer, uint32 SectorCount)
{
	uint32 i;
	uint32 offset;
	BYTE *MbBuf = NULL;

	for (i = 0; i < SectorCount; i++)
	{
		offset = (Sector + i) * 512;

		if (g_part_img_buf[1] == NULL)
		{
			MbBuf = g_part_img_buf[0] + offset;
			memcpy(MbBuf, Buffer + i * 512, 512);
		}
		else
		{
			MbBuf = g_part_img_buf[offset / SIZE_1MB];
			memcpy(MbBuf + (offset % SIZE_1MB), Buffer + i * 512, 512);
		}
	}

	return 1;
}

int VentoyProcSecureBoot(BOOL SecureBoot)
{
	int rc = 0;
	int size;
	char *filebuf = NULL;
	void *file = NULL;

	Log("VentoyProcSecureBoot %d ...", SecureBoot);
	
	if (SecureBoot)
	{
		Log("Secure boot is enabled ...");
		return 0;
	}

	fl_init();

	if (0 == fl_attach_media(VentoyFatMemRead, VentoyFatMemWrite))
	{
		file = fl_fopen("/EFI/BOOT/grubx64_real.efi", "rb");
		Log("Open ventoy efi file %p ", file);
		if (file)
		{
			fl_fseek(file, 0, SEEK_END);
			size = (int)fl_ftell(file);
			fl_fseek(file, 0, SEEK_SET);

			Log("ventoy efi file size %d ...", size);

			filebuf = (char *)malloc(size);
			if (filebuf)
			{
				fl_fread(filebuf, 1, size, file);
			}

			fl_fclose(file);

			Log("Now delete all efi files ...");
			fl_remove("/EFI/BOOT/BOOTX64.EFI");
			fl_remove("/EFI/BOOT/grubx64.efi");
			fl_remove("/EFI/BOOT/grubx64_real.efi");
			fl_remove("/EFI/BOOT/MokManager.efi");
            fl_remove("/ENROLL_THIS_KEY_IN_MOKMANAGER.cer");

			file = fl_fopen("/EFI/BOOT/BOOTX64.EFI", "wb");
			Log("Open bootx64 efi file %p ", file);
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
        Log("Open ventoy efi file %p ", file);
        if (file)
        {
            fl_fseek(file, 0, SEEK_END);
            size = (int)fl_ftell(file);
            fl_fseek(file, 0, SEEK_SET);

            Log("ventoy efi file size %d ...", size);

            filebuf = (char *)malloc(size);
            if (filebuf)
            {
                fl_fread(filebuf, 1, size, file);
            }

            fl_fclose(file);

            Log("Now delete all efi files ...");
            fl_remove("/EFI/BOOT/BOOTIA32.EFI");
            fl_remove("/EFI/BOOT/grubia32.efi");
            fl_remove("/EFI/BOOT/grubia32_real.efi");
            fl_remove("/EFI/BOOT/mmia32.efi");            

            file = fl_fopen("/EFI/BOOT/BOOTIA32.EFI", "wb");
            Log("Open bootia32 efi file %p ", file);
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

	return rc;
}



static int disk_xz_flush(void *src, unsigned int size)
{
    unsigned int i;
    BYTE *buf = (BYTE *)src;

    for (i = 0; i < size; i++)
    {
        *g_part_img_pos = *buf++;

        g_disk_unxz_len++;
        if ((g_disk_unxz_len % SIZE_1MB) == 0)
        {
            g_part_img_pos = g_part_img_buf[g_disk_unxz_len / SIZE_1MB];
        }
        else
        {
            g_part_img_pos++;
        }
    }

    return (int)size;
}

static void unxz_error(char *x)
{
    Log("%s", x);
}

static BOOL TryWritePart2(HANDLE hDrive, UINT64 StartSectorId)
{
    BOOL bRet;
    DWORD TrySize = 16 * 1024;
    DWORD dwSize;
    BYTE *Buffer = NULL;
    unsigned char *data = NULL;
    LARGE_INTEGER liCurrentPosition;

    liCurrentPosition.QuadPart = StartSectorId * 512;
    SetFilePointerEx(hDrive, liCurrentPosition, &liCurrentPosition, FILE_BEGIN);
    
    Buffer = malloc(TrySize);

    bRet = WriteFile(hDrive, Buffer, TrySize, &dwSize, NULL);

    free(Buffer);

    Log("Try write part2 bRet:%u dwSize:%u code:%u", bRet, dwSize, LASTERR);

    if (bRet && dwSize == TrySize)
    {
        return TRUE;
    }

    return FALSE;
}

static int FormatPart2Fat(HANDLE hDrive, UINT64 StartSectorId)
{
    int i;
    int rc = 0;
    int len = 0;
    int writelen = 0;
    int partwrite = 0;
    int Pos = PT_WRITE_VENTOY_START;
    DWORD dwSize = 0;
    BOOL bRet;
    unsigned char *data = NULL;
    LARGE_INTEGER liCurrentPosition;
	LARGE_INTEGER liNewPosition;
    BYTE *CheckBuf = NULL;

	Log("FormatPart2Fat %llu...", (ULONGLONG)StartSectorId);

    CheckBuf = malloc(SIZE_1MB);
    if (!CheckBuf)
    {
        Log("Failed to malloc check buf");
        return 1;
    }

    rc = ReadWholeFileToBuf(VENTOY_FILE_DISK_IMG, 0, (void **)&data, &len);
    if (rc)
    {
        Log("Failed to read img file %p %u", data, len);
        free(CheckBuf);
        return 1;
    }

    liCurrentPosition.QuadPart = StartSectorId * 512;
    SetFilePointerEx(hDrive, liCurrentPosition, &liNewPosition, FILE_BEGIN);

    memset(g_part_img_buf, 0, sizeof(g_part_img_buf));

    g_part_img_buf[0] = (BYTE *)malloc(VENTOY_EFI_PART_SIZE);
    if (g_part_img_buf[0])
    {
        Log("Malloc whole img buffer success, now decompress ...");
        unxz(data, len, NULL, NULL, g_part_img_buf[0], &writelen, unxz_error);

        if (len == writelen)
        {
            Log("decompress finished success");

			VentoyProcSecureBoot(g_SecureBoot);

            for (i = 0; i < VENTOY_EFI_PART_SIZE / SIZE_1MB; i++)
            {
                dwSize = 0;
				bRet = WriteFile(hDrive, g_part_img_buf[0] + i * SIZE_1MB, SIZE_1MB, &dwSize, NULL);
                Log("Write part data bRet:%u dwSize:%u code:%u", bRet, dwSize, LASTERR);

                if (!bRet)
                {
                    rc = 1;
                    goto End;
                }

                PROGRESS_BAR_SET_POS(Pos);
                if (i % 2 == 0)
                {
                    Pos++;
                }
            }

            //Read and check the data
            liCurrentPosition.QuadPart = StartSectorId * 512;
            SetFilePointerEx(hDrive, liCurrentPosition, &liNewPosition, FILE_BEGIN);

            for (i = 0; i < VENTOY_EFI_PART_SIZE / SIZE_1MB; i++)
            {
                bRet = ReadFile(hDrive, CheckBuf, SIZE_1MB, &dwSize, NULL);
                Log("Read part data bRet:%u dwSize:%u code:%u", bRet, dwSize, LASTERR);

                if (!bRet || memcmp(CheckBuf, g_part_img_buf[0] + i * SIZE_1MB, SIZE_1MB))
                {
                    Log("### [Check Fail] The data write and read does not match");
                    rc = 1;
                    goto End;
                }

                PROGRESS_BAR_SET_POS(Pos);
                if (i % 2 == 0)
                {
                    Pos++;
                }
            }
        }
        else
        {
            rc = 1;
            Log("decompress finished failed");
            goto End;
        }
    }
    else
    {
        Log("Failed to malloc whole img size %u, now split it", VENTOY_EFI_PART_SIZE);

        partwrite = 1;
        for (i = 0; i < VENTOY_EFI_PART_SIZE / SIZE_1MB; i++)
        {
            g_part_img_buf[i] = (BYTE *)malloc(SIZE_1MB);
            if (g_part_img_buf[i] == NULL)
            {
                rc = 1;
                goto End;
            }
        }

        Log("Malloc part img buffer success, now decompress ...");

        g_part_img_pos = g_part_img_buf[0];

        unxz(data, len, NULL, disk_xz_flush, NULL, NULL, unxz_error);

        if (g_disk_unxz_len == VENTOY_EFI_PART_SIZE)
        {
            Log("decompress finished success");
			
			VentoyProcSecureBoot(g_SecureBoot);

            for (i = 0; i < VENTOY_EFI_PART_SIZE / SIZE_1MB; i++)
            {
                dwSize = 0;
                bRet = WriteFile(hDrive, g_part_img_buf[i], SIZE_1MB, &dwSize, NULL);
                Log("Write part data bRet:%u dwSize:%u code:%u", bRet, dwSize, LASTERR);

                if (!bRet)
                {
                    rc = 1;
                    goto End;
                }
                
                PROGRESS_BAR_SET_POS(Pos);
                if (i % 2 == 0)
                {
                    Pos++;
                }
            }

            //Read and check the data
            liCurrentPosition.QuadPart = StartSectorId * 512;
            SetFilePointerEx(hDrive, liCurrentPosition, &liNewPosition, FILE_BEGIN);

            for (i = 0; i < VENTOY_EFI_PART_SIZE / SIZE_1MB; i++)
            {
                bRet = ReadFile(hDrive, CheckBuf, SIZE_1MB, &dwSize, NULL);
                Log("Read part data bRet:%u dwSize:%u code:%u", bRet, dwSize, LASTERR);

                if (!bRet || memcmp(CheckBuf, g_part_img_buf[i], SIZE_1MB))
                {
                    Log("### [Check Fail] The data write and read does not match");
                    rc = 1;
                    goto End;
                }

                PROGRESS_BAR_SET_POS(Pos);
                if (i % 2 == 0)
                {
                    Pos++;
                }
            }
        }
        else
        {
            rc = 1;
            Log("decompress finished failed");
            goto End;
        }
    }

End:

    if (data) free(data);
    if (CheckBuf)free(CheckBuf);

    if (partwrite)
    {
        for (i = 0; i < VENTOY_EFI_PART_SIZE / SIZE_1MB; i++)
        {
            if (g_part_img_buf[i]) free(g_part_img_buf[i]);
        }
    }
    else
    {
        if (g_part_img_buf[0]) free(g_part_img_buf[0]);
    }

    return rc;
}

static int WriteGrubStage1ToPhyDrive(HANDLE hDrive, int PartStyle)
{
    int Len = 0;
    int readLen = 0;
    BOOL bRet;
    DWORD dwSize;
    BYTE *ImgBuf = NULL;
    BYTE *RawBuf = NULL;

    Log("WriteGrubStage1ToPhyDrive ...");

    RawBuf = (BYTE *)malloc(SIZE_1MB);
    if (!RawBuf)
    {
        return 1;
    }

    if (ReadWholeFileToBuf(VENTOY_FILE_STG1_IMG, 0, (void **)&ImgBuf, &Len))
    {
        Log("Failed to read stage1 img");
        free(RawBuf);
        return 1;
    }

    unxz(ImgBuf, Len, NULL, NULL, RawBuf, &readLen, unxz_error);

    if (PartStyle)
    {
        Log("Write GPT stage1 ...");
        RawBuf[500] = 35;//update blocklist
        SetFilePointer(hDrive, 512 * 34, NULL, FILE_BEGIN);        
        bRet = WriteFile(hDrive, RawBuf, SIZE_1MB - 512 * 34, &dwSize, NULL);
    }
    else
    {
        Log("Write MBR stage1 ...");
        SetFilePointer(hDrive, 512, NULL, FILE_BEGIN);
        bRet = WriteFile(hDrive, RawBuf, SIZE_1MB - 512, &dwSize, NULL);
    }

    Log("WriteFile Ret:%u dwSize:%u ErrCode:%u", bRet, dwSize, GetLastError());

    free(RawBuf);
    free(ImgBuf);
    return 0;
}



static int FormatPart1exFAT(UINT64 DiskSizeBytes)
{
    MKFS_PARM Option;
    FRESULT Ret;

    Option.fmt = FM_EXFAT;
    Option.n_fat = 1;
    Option.align = 8;
    Option.n_root = 1;

    // < 32GB select 32KB as cluster size
    // > 32GB select 128KB as cluster size
    if (DiskSizeBytes / 1024 / 1024 / 1024 <= 32)
    {
        Option.au_size = 32768;
    }
    else
    {
        Option.au_size = 131072;
    }

    Log("Formatting Part1 exFAT ...");

	disk_io_reset_write_error();

    Ret = f_mkfs(TEXT("0:"), &Option, 0, 8 * 1024 * 1024);
    if (FR_OK == Ret)
    {
		if (disk_io_is_write_error())
		{
			Log("Formatting Part1 exFAT failed, write error.");
			return 1;
		}

        Log("Formatting Part1 exFAT success");
        return 0;
    }
    else
    {
        Log("Formatting Part1 exFAT failed");
        return 1;
    }
}



int ClearVentoyFromPhyDrive(HWND hWnd, PHY_DRIVE_INFO *pPhyDrive, char *pDrvLetter)
{
    int i;
    int rc = 0;
    int state = 0;
    HANDLE hDrive;
    DWORD dwSize;
    BOOL bRet;
    CHAR MountDrive;
    CHAR DriveName[] = "?:\\";
    CHAR DriveLetters[MAX_PATH] = { 0 };
    LARGE_INTEGER liCurrentPosition;
    char *pTmpBuf = NULL;
    MBR_HEAD MBR;

    *pDrvLetter = 0;

    Log("ClearVentoyFromPhyDrive PhyDrive%d <<%s %s %dGB>>",
        pPhyDrive->PhyDrive, pPhyDrive->VendorId, pPhyDrive->ProductId,
        GetHumanReadableGBSize(pPhyDrive->SizeInBytes));

    PROGRESS_BAR_SET_POS(PT_LOCK_FOR_CLEAN);

    Log("Lock disk for clean ............................. ");

    hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, FALSE, FALSE);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Failed to open physical disk");
        return 1;
    }

    GetLettersBelongPhyDrive(pPhyDrive->PhyDrive, DriveLetters, sizeof(DriveLetters));

    if (DriveLetters[0] == 0)
    {
        Log("No drive letter was assigned...");
        DriveName[0] = GetFirstUnusedDriveLetter();
        Log("GetFirstUnusedDriveLetter %C: ...", DriveName[0]);
    }
    else
    {
        // Unmount all mounted volumes that belong to this drive
        // Do it in reverse so that we always end on the first volume letter
        for (i = (int)strlen(DriveLetters); i > 0; i--)
        {
            DriveName[0] = DriveLetters[i - 1];
            bRet = DeleteVolumeMountPointA(DriveName);
            Log("Delete mountpoint %s ret:%u code:%u", DriveName, bRet, GetLastError());
        }
    }

    MountDrive = DriveName[0];
    Log("Will use '%C:' as volume mountpoint", DriveName[0]);

    // It kind of blows, but we have to relinquish access to the physical drive
    // for VDS to be able to delete the partitions that reside on it...
    DeviceIoControl(hDrive, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL);
    CHECK_CLOSE_HANDLE(hDrive);

    PROGRESS_BAR_SET_POS(PT_DEL_ALL_PART);

    if (!VDS_DeleteAllPartitions(pPhyDrive->PhyDrive))
    {
        Log("Notice: Could not delete partitions: %u", GetLastError());
    }

    Log("Deleting all partitions ......................... OK");

    PROGRESS_BAR_SET_POS(PT_LOCK_FOR_WRITE);

    Log("Lock disk for write ............................. ");
    hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, TRUE, FALSE);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Failed to GetPhysicalHandle for write.");
        rc = 1;
        goto End;
    }

    // clear first and last 2MB space
    pTmpBuf = malloc(SIZE_2MB);
    if (!pTmpBuf)
    {
        Log("Failed to alloc memory.");
        rc = 1;
        goto End;
    }
    memset(pTmpBuf, 0, SIZE_2MB);

    SET_FILE_POS(512);
    bRet = WriteFile(hDrive, pTmpBuf, SIZE_2MB - 512, &dwSize, NULL);
    Log("Write fisrt 1MB ret:%d size:%u err:%d", bRet, dwSize, LASTERR);
    if (!bRet)
    {
        rc = 1;
        goto End;
    }

    SET_FILE_POS(pPhyDrive->SizeInBytes - SIZE_2MB);
    bRet = WriteFile(hDrive, pTmpBuf, SIZE_2MB, &dwSize, NULL);
    Log("Write 2nd 1MB ret:%d size:%u err:%d", bRet, dwSize, LASTERR);
    if (!bRet)
    {
        rc = 1;
        goto End;
    }

    SET_FILE_POS(0);

    if (pPhyDrive->SizeInBytes > 2199023255552ULL)
    {
        VTOY_GPT_INFO *pGptInfo;
        VTOY_GPT_HDR BackupHead;
        LARGE_INTEGER liCurrentPosition;

        pGptInfo = (VTOY_GPT_INFO *)pTmpBuf;

        VentoyFillWholeGpt(pPhyDrive->SizeInBytes, pGptInfo);

        SET_FILE_POS(pPhyDrive->SizeInBytes - 512);
        VentoyFillBackupGptHead(pGptInfo, &BackupHead);
        if (!WriteFile(hDrive, &BackupHead, sizeof(VTOY_GPT_HDR), &dwSize, NULL))
        {
            rc = 1;
            Log("Write GPT Backup Head Failed, dwSize:%u (%u) ErrCode:%u", dwSize, sizeof(VTOY_GPT_INFO), GetLastError());
            goto End;
        }

        SET_FILE_POS(pPhyDrive->SizeInBytes - 512 * 33);
        if (!WriteFile(hDrive, pGptInfo->PartTbl, sizeof(pGptInfo->PartTbl), &dwSize, NULL))
        {
            rc = 1;
            Log("Write GPT Backup Part Table Failed, dwSize:%u (%u) ErrCode:%u", dwSize, sizeof(VTOY_GPT_INFO), GetLastError());
            goto End;
        }

        SET_FILE_POS(0);
        if (!WriteFile(hDrive, pGptInfo, sizeof(VTOY_GPT_INFO), &dwSize, NULL))
        {
            rc = 1;
            Log("Write GPT Info Failed, dwSize:%u (%u) ErrCode:%u", dwSize, sizeof(VTOY_GPT_INFO), GetLastError());
            goto End;
        }

        Log("Write GPT Info OK ...");
    }
    else
    {
        bRet = ReadFile(hDrive, &MBR, sizeof(MBR), &dwSize, NULL);
        Log("Read MBR ret:%d size:%u err:%d", bRet, dwSize, LASTERR);
        if (!bRet)
        {
            rc = 1;
            goto End;
        }

        //clear boot code and partition table (reserved disk signature)
        memset(MBR.BootCode, 0, 440);
        memset(MBR.PartTbl, 0, sizeof(MBR.PartTbl));

        VentoyFillMBRLocation(pPhyDrive->SizeInBytes, 2048, (UINT32)(pPhyDrive->SizeInBytes / 512 - 2048), MBR.PartTbl);

        MBR.PartTbl[0].Active = 0x00; // bootable
        MBR.PartTbl[0].FsFlag = 0x07; // exFAT/NTFS/HPFS

        SET_FILE_POS(0);
        bRet = WriteFile(hDrive, &MBR, 512, &dwSize, NULL);
        Log("Write MBR ret:%d size:%u err:%d", bRet, dwSize, LASTERR);
        if (!bRet)
        {
            rc = 1;
            goto End;
        }
    }

    Log("Clear Ventoy successfully finished");

	//Refresh Drive Layout
	DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &dwSize, NULL);

End:
    
    PROGRESS_BAR_SET_POS(PT_MOUNT_VOLUME);
    
    if (pTmpBuf)
    {
        free(pTmpBuf);
    }

    if (rc == 0)
    {
        Log("Mounting Ventoy Partition ....................... ");
        Sleep(1000);

        state = 0;
        memset(DriveLetters, 0, sizeof(DriveLetters));
        GetLettersBelongPhyDrive(pPhyDrive->PhyDrive, DriveLetters, sizeof(DriveLetters));
        Log("Logical drive letter after write ventoy: <%s>", DriveLetters);

        for (i = 0; i < sizeof(DriveLetters) && DriveLetters[i]; i++)
        {
            DriveName[0] = DriveLetters[i];
            Log("%s is ventoy part1, already mounted", DriveName);
            state = 1;
        }

        if (state != 1)
        {
            Log("need to mount ventoy part1...");
            if (0 == GetVentoyVolumeName(pPhyDrive->PhyDrive, 2048, DriveLetters, sizeof(DriveLetters), FALSE))
            {
                DriveName[0] = MountDrive;
                bRet = SetVolumeMountPointA(DriveName, DriveLetters);
                Log("SetVolumeMountPoint <%s> <%s> bRet:%u code:%u", DriveName, DriveLetters, bRet, GetLastError());

                *pDrvLetter = MountDrive;
            }
            else
            {
                Log("Failed to find ventoy volume");
            }
        }

        Log("OK\n");
    }
    else
    {
        FindProcessOccupyDisk(hDrive, pPhyDrive);
    }

    CHECK_CLOSE_HANDLE(hDrive);
    return rc;
}

int InstallVentoy2FileImage(PHY_DRIVE_INFO *pPhyDrive, int PartStyle)
{
    int i;
    int rc = 1;
    int Len = 0;
    int dataLen = 0;
    UINT size = 0;
    UINT segnum = 0;
    UINT32 chksum = 0;
    UINT64 data_offset = 0;
    UINT64 Part2StartSector = 0;
    UINT64 Part1StartSector = 0;
    UINT64 Part1SectorCount = 0;
    UINT8 *pData = NULL;    
    UINT8 *pBkGptPartTbl = NULL;
    BYTE *ImgBuf = NULL;
    MBR_HEAD *pMBR = NULL;
    VTSI_FOOTER *pImgFooter = NULL;
    VTSI_SEGMENT *pSegment = NULL;
    VTOY_GPT_INFO *pGptInfo = NULL;
    VTOY_GPT_HDR *pBkGptHdr = NULL;
    FILE *fp = NULL;

    Log("InstallVentoy2FileImage %s PhyDrive%d <<%s %s %dGB>>",
        PartStyle ? "GPT" : "MBR", pPhyDrive->PhyDrive, pPhyDrive->VendorId, pPhyDrive->ProductId,
        GetHumanReadableGBSize(pPhyDrive->SizeInBytes));

    PROGRESS_BAR_SET_POS(PT_LOCK_FOR_CLEAN);

    size = SIZE_1MB + VENTOY_EFI_PART_SIZE + 33 * 512 + VTSI_IMG_MAX_SEG * sizeof(VTSI_SEGMENT) + sizeof(VTSI_FOOTER);

    pData = (UINT8 *)malloc(size);
    if (!pData)
    {
        Log("malloc image buffer failed %d.", size);
        goto End;
    }

    pImgFooter = (VTSI_FOOTER *)(pData + size - sizeof(VTSI_FOOTER));
    pSegment = (VTSI_SEGMENT *)((UINT8 *)pImgFooter - VTSI_IMG_MAX_SEG * sizeof(VTSI_SEGMENT));
    memset(pImgFooter, 0, sizeof(VTSI_FOOTER));
    memset(pSegment, 0, VTSI_IMG_MAX_SEG * sizeof(VTSI_SEGMENT));

    PROGRESS_BAR_SET_POS(PT_WRITE_VENTOY_START);

    Log("Writing Boot Image ............................. ");
    if (ReadWholeFileToBuf(VENTOY_FILE_STG1_IMG, 0, (void **)&ImgBuf, &Len))
    {
        Log("Failed to read stage1 img");
        goto End;
    }

    unxz(ImgBuf, Len, NULL, NULL, pData, &dataLen, unxz_error);
    SAFE_FREE(ImgBuf);

    Log("decompress %s len:%d", VENTOY_FILE_STG1_IMG, dataLen);

    if (PartStyle)
    {
        pData[500] = 35;//update blocklist
        memmove(pData + 34 * 512, pData, SIZE_1MB - 512 * 34);
        memset(pData, 0, 34 * 512);

        pGptInfo = (VTOY_GPT_INFO *)pData;
        memset(pGptInfo, 0, sizeof(VTOY_GPT_INFO));
        VentoyFillGpt(pPhyDrive->SizeInBytes, pGptInfo);

        pBkGptPartTbl = pData + SIZE_1MB + VENTOY_EFI_PART_SIZE;
        memset(pBkGptPartTbl, 0, 33 * 512);

        memcpy(pBkGptPartTbl, pGptInfo->PartTbl, 32 * 512);
        pBkGptHdr = (VTOY_GPT_HDR *)(pBkGptPartTbl + 32 * 512);
        VentoyFillBackupGptHead(pGptInfo, pBkGptHdr);

        Part1StartSector = pGptInfo->PartTbl[0].StartLBA;
        Part1SectorCount = pGptInfo->PartTbl[0].LastLBA - Part1StartSector + 1;
        Part2StartSector = pGptInfo->PartTbl[1].StartLBA;

        Log("Write GPT Info OK ...");
    }
    else
    {
        memmove(pData + 512, pData, SIZE_1MB - 512);
        memset(pData, 0, 512);

        pMBR = (MBR_HEAD *)pData;
        VentoyFillMBR(pPhyDrive->SizeInBytes, pMBR, PartStyle);
        Part1StartSector = pMBR->PartTbl[0].StartSectorId;
        Part1SectorCount = pMBR->PartTbl[0].SectorCount;
        Part2StartSector = pMBR->PartTbl[1].StartSectorId;

        Log("Write MBR OK ...");
    }

    Log("Writing EFI part Image ............................. ");
    rc = ReadWholeFileToBuf(VENTOY_FILE_DISK_IMG, 0, (void **)&ImgBuf, &Len);
    if (rc)
    {
        Log("Failed to read img file %p %u", ImgBuf, Len);
        goto End;
    }

    PROGRESS_BAR_SET_POS(PT_WRITE_VENTOY_START + 28);
    memset(g_part_img_buf, 0, sizeof(g_part_img_buf));
    unxz(ImgBuf, Len, NULL, NULL, pData + SIZE_1MB, &dataLen, unxz_error);
    if (dataLen == Len)
    {
        Log("decompress finished success");
        g_part_img_buf[0] = pData + SIZE_1MB;

        VentoyProcSecureBoot(g_SecureBoot);
    }
    else
    {
        Log("decompress finished failed");
        goto End;
    }

    fopen_s(&fp, "VentoySparseImg.vtsi", "wb+");
    if (!fp)
    {
        Log("Failed to create Ventoy img file");
        goto End;
    }

    Log("Writing stage1 data ............................. ");

    fwrite(pData, 1, SIZE_1MB, fp);

    pSegment[0].disk_start_sector = 0;
    pSegment[0].sector_num = SIZE_1MB / 512;
    pSegment[0].data_offset = data_offset;
    data_offset += pSegment[0].sector_num * 512;

    disk_io_set_param(INVALID_HANDLE_VALUE, Part1StartSector + Part1SectorCount);// include the 2048 sector gap
    disk_io_set_imghook(fp, pSegment + 1, VTSI_IMG_MAX_SEG - 1, data_offset);

    Log("Formatting part1 exFAT ...");
    if (0 != FormatPart1exFAT(pPhyDrive->SizeInBytes))
    {
        Log("FormatPart1exFAT failed.");
        disk_io_reset_imghook(&segnum, &data_offset);
        goto End;
    }

    disk_io_reset_imghook(&segnum, &data_offset);
    segnum++;

    Log("current segment number:%d dataoff:%ld", segnum, (long)data_offset);

    //write data
    Log("Writing part2 data ............................. ");
    fwrite(pData + SIZE_1MB, 1, VENTOY_EFI_PART_SIZE, fp);
    pSegment[segnum].disk_start_sector = Part2StartSector;
    pSegment[segnum].sector_num = VENTOY_EFI_PART_SIZE / 512;
    pSegment[segnum].data_offset = data_offset;
    data_offset += pSegment[segnum].sector_num * 512;
    segnum++;

    if (PartStyle)
    {
        Log("Writing backup gpt table ............................. ");
        fwrite(pBkGptPartTbl, 1, 33 * 512, fp);
        pSegment[segnum].disk_start_sector = pPhyDrive->SizeInBytes / 512 - 33;
        pSegment[segnum].sector_num = 33;
        pSegment[segnum].data_offset = data_offset;
        data_offset += pSegment[segnum].sector_num * 512;
        segnum++;
    }

    Log("Writing segment metadata ............................. ");

    for (i = 0; i < (int)segnum; i++)
    {
        Log("SEG[%d]:  PhySector:%llu SectorNum:%llu DataOffset:%llu(sector:%llu)", i, pSegment[i].disk_start_sector, pSegment[i].sector_num,
            pSegment[i].data_offset, pSegment[i].data_offset / 512);
    }

    dataLen = segnum * sizeof(VTSI_SEGMENT);
    fwrite(pSegment, 1, dataLen, fp);

    if (dataLen % 512)
    {
        //pData + SIZE_1MB - 8192 is a temp data buffer with zero
        fwrite(pData + SIZE_1MB - 8192, 1, 512 - (dataLen % 512), fp);
    }

    //Fill footer
    pImgFooter->magic = VTSI_IMG_MAGIC;
    pImgFooter->version = 1;
    pImgFooter->disk_size = pPhyDrive->SizeInBytes;
    memcpy(&pImgFooter->disk_signature, pPhyDrive->MBR.BootCode + 0x1b8, 4);
    pImgFooter->segment_num = segnum;
    pImgFooter->segment_offset = data_offset;

    for (i = 0, chksum = 0; i < (int)(segnum * sizeof(VTSI_SEGMENT)); i++)
    {
        chksum += *((UINT8 *)pSegment + i);
    }
    pImgFooter->segment_chksum = ~chksum;

    for (i = 0, chksum = 0; i < sizeof(VTSI_FOOTER); i++)
    {
        chksum += *((UINT8 *)pImgFooter + i);
    }
    pImgFooter->foot_chksum = ~chksum;

    Log("Writing footer segnum(%u)  segoffset(%llu) ......................", segnum, data_offset);
    Log("disk_size=%llu disk_signature=%lx segment_offset=%llu", pImgFooter->disk_size, pImgFooter->disk_signature, pImgFooter->segment_offset);

    fwrite(pImgFooter, 1, sizeof(VTSI_FOOTER), fp);
    fclose(fp);

    Log("Writing Ventoy image file finished, the file size should be %llu .", data_offset + 512 + ((dataLen + 511) / 512 * 512));

    rc = 0;

End:

    PROGRESS_BAR_SET_POS(PT_MOUNT_VOLUME);

    Log("retcode:%d\n", rc);

    SAFE_FREE(pData);
    SAFE_FREE(ImgBuf);
    
    return rc;
}


int InstallVentoy2PhyDrive(PHY_DRIVE_INFO *pPhyDrive, int PartStyle, int TryId)
{
    int i;
    int rc = 0;
    int state = 0;
    HANDLE hDrive;
    DWORD dwSize;
    BOOL bRet;
    CHAR MountDrive;
    CHAR DriveName[] = "?:\\";
    CHAR DriveLetters[MAX_PATH] = { 0 };
    MBR_HEAD MBR;
    VTOY_GPT_INFO *pGptInfo = NULL;
    UINT64 Part1StartSector = 0;
    UINT64 Part1SectorCount = 0;
    UINT64 Part2StartSector = 0;

	Log("#####################################################");
    Log("InstallVentoy2PhyDrive try%d %s PhyDrive%d <<%s %s %dGB>>", TryId,
        PartStyle ? "GPT" : "MBR", pPhyDrive->PhyDrive, pPhyDrive->VendorId, pPhyDrive->ProductId,
        GetHumanReadableGBSize(pPhyDrive->SizeInBytes));
	Log("#####################################################");

    if (PartStyle)
    {
        pGptInfo = malloc(sizeof(VTOY_GPT_INFO));
        memset(pGptInfo, 0, sizeof(VTOY_GPT_INFO));
    }

    PROGRESS_BAR_SET_POS(PT_LOCK_FOR_CLEAN);

    if (PartStyle)
    {
        VentoyFillGpt(pPhyDrive->SizeInBytes, pGptInfo);
        Part1StartSector = pGptInfo->PartTbl[0].StartLBA;
        Part1SectorCount = pGptInfo->PartTbl[0].LastLBA - Part1StartSector + 1;
        Part2StartSector = pGptInfo->PartTbl[1].StartLBA;
    }
    else
    {
        VentoyFillMBR(pPhyDrive->SizeInBytes, &MBR, PartStyle);
        Part1StartSector = MBR.PartTbl[0].StartSectorId;
        Part1SectorCount = MBR.PartTbl[0].SectorCount;
        Part2StartSector = MBR.PartTbl[1].StartSectorId;
    }

    Log("Lock disk for clean ............................. ");

    hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, FALSE, FALSE);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Failed to open physical disk");
        free(pGptInfo);
        return 1;
    }

    GetLettersBelongPhyDrive(pPhyDrive->PhyDrive, DriveLetters, sizeof(DriveLetters));

    if (DriveLetters[0] == 0)
    {
        Log("No drive letter was assigned...");
        DriveName[0] = GetFirstUnusedDriveLetter();
        Log("GetFirstUnusedDriveLetter %C: ...", DriveName[0]);
    }
    else
    {
        // Unmount all mounted volumes that belong to this drive
        // Do it in reverse so that we always end on the first volume letter
        for (i = (int)strlen(DriveLetters); i > 0; i--)
        {
            DriveName[0] = DriveLetters[i - 1];
            bRet = DeleteVolumeMountPointA(DriveName);
            Log("Delete mountpoint %s ret:%u code:%u", DriveName, bRet, GetLastError());
        }
    }

    MountDrive = DriveName[0];
    Log("Will use '%C:' as volume mountpoint", DriveName[0]);

    // It kind of blows, but we have to relinquish access to the physical drive
    // for VDS to be able to delete the partitions that reside on it...
    DeviceIoControl(hDrive, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL);
    CHECK_CLOSE_HANDLE(hDrive);

    PROGRESS_BAR_SET_POS(PT_DEL_ALL_PART);

    if (!VDS_DeleteAllPartitions(pPhyDrive->PhyDrive))
    {
        Log("Notice: Could not delete partitions: 0x%x, but we continue.", GetLastError());
    }

    Log("Deleting all partitions ......................... OK");

    PROGRESS_BAR_SET_POS(PT_LOCK_FOR_WRITE);

    Log("Lock disk for write ............................. ");
    hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, TRUE, FALSE);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Failed to GetPhysicalHandle for write.");
        rc = 1;
        goto End;
    }

    //Refresh Drive Layout
    DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &dwSize, NULL);

    disk_io_set_param(hDrive, Part1StartSector + Part1SectorCount);// include the 2048 sector gap

    PROGRESS_BAR_SET_POS(PT_FORMAT_PART1);

    if (PartStyle == 1 && pPhyDrive->PartStyle == 0)
    {
        Log("Wait for format part1 ...");
        Sleep(1000 * 5);
    }

    Log("Formatting part1 exFAT ...");
    if (0 != FormatPart1exFAT(pPhyDrive->SizeInBytes))
    {
        Log("FormatPart1exFAT failed.");
        rc = 1;
        goto End;
    }

    PROGRESS_BAR_SET_POS(PT_FORMAT_PART2);
    Log("Writing part2 FAT img ...");
    
    if (0 != FormatPart2Fat(hDrive, Part2StartSector))
    {
        Log("FormatPart2Fat failed.");
        rc = 1;
        goto End;
    }

    PROGRESS_BAR_SET_POS(PT_WRITE_STG1_IMG);
    Log("Writing Boot Image ............................. ");
    if (WriteGrubStage1ToPhyDrive(hDrive, PartStyle) != 0)
    {
        Log("WriteGrubStage1ToPhyDrive failed.");
        rc = 1;
        goto End;
    }

    PROGRESS_BAR_SET_POS(PT_WRITE_PART_TABLE);
    Log("Writing Partition Table ........................ ");
    SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);

    if (PartStyle)
    {
        VTOY_GPT_HDR BackupHead;
        LARGE_INTEGER liCurrentPosition;

        SET_FILE_POS(pPhyDrive->SizeInBytes - 512);
        VentoyFillBackupGptHead(pGptInfo, &BackupHead);
        if (!WriteFile(hDrive, &BackupHead, sizeof(VTOY_GPT_HDR), &dwSize, NULL))
        {
            rc = 1;
            Log("Write GPT Backup Head Failed, dwSize:%u (%u) ErrCode:%u", dwSize, sizeof(VTOY_GPT_INFO), GetLastError());
            goto End;
        }

        SET_FILE_POS(pPhyDrive->SizeInBytes - 512 * 33);
        if (!WriteFile(hDrive, pGptInfo->PartTbl, sizeof(pGptInfo->PartTbl), &dwSize, NULL))
        {
            rc = 1;
            Log("Write GPT Backup Part Table Failed, dwSize:%u (%u) ErrCode:%u", dwSize, sizeof(VTOY_GPT_INFO), GetLastError());
            goto End;
        }

        SET_FILE_POS(0);
        if (!WriteFile(hDrive, pGptInfo, sizeof(VTOY_GPT_INFO), &dwSize, NULL))
        {
            rc = 1;
            Log("Write GPT Info Failed, dwSize:%u (%u) ErrCode:%u", dwSize, sizeof(VTOY_GPT_INFO), GetLastError());
            goto End;
        }

        Log("Write GPT Info OK ...");
        memcpy(&(pPhyDrive->MBR), &(pGptInfo->MBR), 512);
    }
    else
    {
        if (!WriteFile(hDrive, &MBR, sizeof(MBR), &dwSize, NULL))
        {
            rc = 1;
            Log("Write MBR Failed, dwSize:%u ErrCode:%u", dwSize, GetLastError());
            goto End;
        }
        Log("Write MBR OK ...");
        memcpy(&(pPhyDrive->MBR), &MBR, 512);
    }

    //Refresh Drive Layout
    DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &dwSize, NULL);

End:

    PROGRESS_BAR_SET_POS(PT_MOUNT_VOLUME);

    if (rc == 0)
    {
        Log("Mounting Ventoy Partition ....................... ");
        Sleep(1000);

        state = 0;
        memset(DriveLetters, 0, sizeof(DriveLetters));
        GetLettersBelongPhyDrive(pPhyDrive->PhyDrive, DriveLetters, sizeof(DriveLetters));
        Log("Logical drive letter after write ventoy: <%s>", DriveLetters);

        for (i = 0; i < sizeof(DriveLetters) && DriveLetters[i]; i++)
        {
            DriveName[0] = DriveLetters[i];
            if (IsVentoyLogicalDrive(DriveName[0]))
            {
                Log("%s is ventoy part2, delete mountpoint", DriveName);
                DeleteVolumeMountPointA(DriveName);
            }
            else
            {
                Log("%s is ventoy part1, already mounted", DriveName);
                state = 1;
            }
        }

        if (state != 1)
        {
            Log("need to mount ventoy part1...");
            
            if (0 == GetVentoyVolumeName(pPhyDrive->PhyDrive, Part1StartSector, DriveLetters, sizeof(DriveLetters), FALSE))
            {
                DriveName[0] = MountDrive;
                bRet = SetVolumeMountPointA(DriveName, DriveLetters);
                Log("SetVolumeMountPoint <%s> <%s> bRet:%u code:%u", DriveName, DriveLetters, bRet, GetLastError());
            }
            else
            {
                Log("Failed to find ventoy volume");
            }
        }
        Log("OK\n");
    }
    else
    {
		PROGRESS_BAR_SET_POS(PT_LOCK_FOR_CLEAN);

        FindProcessOccupyDisk(hDrive, pPhyDrive);

		if (!VDS_IsLastAvaliable())
		{
			Log("###### [Error:] Virtual Disk Service (VDS) Unavailable ######");
			Log("###### [Error:] Virtual Disk Service (VDS) Unavailable ######");
			Log("###### [Error:] Virtual Disk Service (VDS) Unavailable ######");
			Log("###### [Error:] Virtual Disk Service (VDS) Unavailable ######");
			Log("###### [Error:] Virtual Disk Service (VDS) Unavailable ######");
		}
    }

    if (pGptInfo)
    {
        free(pGptInfo);
    }

    CHECK_CLOSE_HANDLE(hDrive);
    return rc;
}


int PartitionResizeForVentoy(PHY_DRIVE_INFO *pPhyDrive)
{
	int i, j;
	int rc = 1;
	int PhyDrive;
	int PartStyle;
	INT64 ReservedValue;
	UINT64 RecudeBytes;
	GUID Guid;
	MBR_HEAD MBR;
	VTOY_GPT_INFO *pGPT;
	MBR_HEAD *pMBR;
	DWORD dwSize = 0;
	VTOY_GPT_HDR BackupHead;
	HANDLE hDrive = INVALID_HANDLE_VALUE;
	GUID ZeroGuid = { 0 };
	static GUID WindowsDataPartType = { 0xebd0a0a2, 0xb9e5, 0x4433, { 0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7 } };
	static GUID EspPartType = { 0xc12a7328, 0xf81f, 0x11d2, { 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b } };
	static GUID BiosGrubPartType = { 0x21686148, 0x6449, 0x6e6f, { 0x74, 0x4e, 0x65, 0x65, 0x64, 0x45, 0x46, 0x49 } };

	Log("#####################################################");
	Log("PartitionResizeForVentoy PhyDrive%d <<%s %s %dGB>>",
		pPhyDrive->PhyDrive, pPhyDrive->VendorId, pPhyDrive->ProductId,
		GetHumanReadableGBSize(pPhyDrive->SizeInBytes));
	Log("#####################################################");

	pGPT = &(pPhyDrive->Gpt);
	pMBR = &(pPhyDrive->Gpt.MBR);
	Log("Disksize:%llu Part2Start:%llu", pPhyDrive->SizeInBytes, pPhyDrive->ResizePart2StartSector * 512);

	if (pMBR->PartTbl[0].FsFlag == 0xEE && memcmp(pGPT->Head.Signature, "EFI PART", 8) == 0)
	{
		PartStyle = 1;
	}
	else
	{
		PartStyle = 0;
	}

	PROGRESS_BAR_SET_POS(PT_LOCK_FOR_CLEAN);

	RecudeBytes = VENTOY_EFI_PART_SIZE;
	ReservedValue = GetReservedSpaceInMB();
	if (ReservedValue > 0)
	{
		Log("Reduce add reserved space %lldMB", (LONGLONG)ReservedValue);
		RecudeBytes += (UINT64)(ReservedValue * SIZE_1MB);
	}


	if (pPhyDrive->ResizeNoShrink == FALSE)
	{
		Log("Need to shrink the volume");
		if (DISK_ShrinkVolume(pPhyDrive->PhyDrive, pPhyDrive->ResizeVolumeGuid, pPhyDrive->Part1DriveLetter, pPhyDrive->ResizeOldPart1Size, RecudeBytes))
		{
			Log("Shrink volume success, now check again");

			hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, TRUE, FALSE);
			if (hDrive == INVALID_HANDLE_VALUE)
			{
				Log("Failed to GetPhysicalHandle for update.");
				goto End;
			}

			//Refresh Drive Layout
			DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &dwSize, NULL);

			CHECK_CLOSE_HANDLE(hDrive);


			if (PartResizePreCheck(NULL) && pPhyDrive->ResizeNoShrink)
			{
				Log("Recheck after Shrink volume success");
				Log("After shrink Disksize:%llu Part2Start:%llu", pPhyDrive->SizeInBytes, pPhyDrive->ResizePart2StartSector * 512);
			}
			else
			{
				Log("Recheck after Shrink volume failed %u", pPhyDrive->ResizeNoShrink);
				goto End;
			}
		}
		else
		{
			Log("Shrink volume failed");
			goto End;
		}
	}


	//Now try write data
	hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, TRUE, FALSE);
	if (hDrive == INVALID_HANDLE_VALUE)
	{
		Log("Failed to GetPhysicalHandle for update.");
		goto End;
	}


	//Write partition 2 data
	PROGRESS_BAR_SET_POS(PT_FORMAT_PART2);
	if (0 != FormatPart2Fat(hDrive, pPhyDrive->ResizePart2StartSector))
	{
		Log("FormatPart2Fat failed.");
		goto End;
	}

	//Write grub stage2 gap
	PROGRESS_BAR_SET_POS(PT_WRITE_STG1_IMG);
	Log("Writing Boot Image ............................. ");
	if (WriteGrubStage1ToPhyDrive(hDrive, PartStyle) != 0)
	{
		Log("WriteGrubStage1ToPhyDrive failed.");
		goto End;
	}


	//Write partition table
	PROGRESS_BAR_SET_POS(PT_WRITE_PART_TABLE);
	Log("Writing partition table ............................. ");

	VentoyGetLocalBootImg(&MBR);
	CoCreateGuid(&Guid);
	memcpy(MBR.BootCode + 0x180, &Guid, 16);
	memcpy(pMBR->BootCode, MBR.BootCode, 440);

	if (PartStyle == 0)
	{
		for (i = 1; i < 4; i++)
		{
			if (pMBR->PartTbl[i].SectorCount == 0)
			{
				break;
			}
		}

		if (i >= 4)
		{
			Log("Can not find MBR free partition table");
			goto End;
		}

		for (j = i - 1; j > 0; j--)
		{
			Log("Move MBR partition table %d --> %d", j + 1, j + 2);
			memcpy(pMBR->PartTbl + (j + 1), pMBR->PartTbl + j, sizeof(PART_TABLE));
		}

        memset(pMBR->PartTbl + 1, 0, sizeof(PART_TABLE));
		VentoyFillMBRLocation(pPhyDrive->SizeInBytes, (UINT32)pPhyDrive->ResizePart2StartSector, VENTOY_EFI_PART_SIZE / 512, pMBR->PartTbl + 1);
		pMBR->PartTbl[0].Active = 0x80; // bootable
		pMBR->PartTbl[1].Active = 0x00;
		pMBR->PartTbl[1].FsFlag = 0xEF; // EFI System Partition

		if (!WriteDataToPhyDisk(hDrive, 0, pMBR, 512))
		{
			Log("Legacy BIOS write MBR failed");
			goto End;
		}
	}
	else
	{
		for (i = 1; i < 128; i++)
		{
			if (memcmp(&(pGPT->PartTbl[i].PartGuid), &ZeroGuid, sizeof(GUID)) == 0)
			{
				break;
			}
		}

		if (i >= 128)
		{
			Log("Can not find GPT free partition table");
			goto End;
		}

		for (j = i - 1; j > 0; j--)
		{
			Log("Move GPT partition table %d --> %d", j + 1, j + 2);
			memcpy(pGPT->PartTbl + (j + 1), pGPT->PartTbl + j, sizeof(VTOY_GPT_PART_TBL));
		}


		pMBR->BootCode[92] = 0x22;

		// to fix windows issue
        memset(pGPT->PartTbl + 1, 0, sizeof(VTOY_GPT_PART_TBL));
		memcpy(&(pGPT->PartTbl[1].PartType), &WindowsDataPartType, sizeof(GUID));
		CoCreateGuid(&(pGPT->PartTbl[1].PartGuid));

		pGPT->PartTbl[1].StartLBA = pGPT->PartTbl[0].LastLBA + 1;
		pGPT->PartTbl[1].LastLBA = pGPT->PartTbl[1].StartLBA + VENTOY_EFI_PART_SIZE / 512 - 1;
		pGPT->PartTbl[1].Attr = 0xC000000000000001ULL;
		memcpy(pGPT->PartTbl[1].Name, L"VTOYEFI", 7 * 2);

		//Update CRC
		pGPT->Head.PartTblCrc = VentoyCrc32(pGPT->PartTbl, sizeof(pGPT->PartTbl));
        pGPT->Head.Crc = 0;
		pGPT->Head.Crc = VentoyCrc32(&(pGPT->Head), pGPT->Head.Length);

		Log("pGPT->Head.EfiStartLBA=%llu", (ULONGLONG)pGPT->Head.EfiStartLBA);
		Log("pGPT->Head.EfiBackupLBA=%llu", (ULONGLONG)pGPT->Head.EfiBackupLBA);

		VentoyFillBackupGptHead(pGPT, &BackupHead);
		if (!WriteDataToPhyDisk(hDrive, pGPT->Head.EfiBackupLBA * 512, &BackupHead, 512))
		{
			Log("UEFI write backup head failed");
			goto End;
		}

		if (!WriteDataToPhyDisk(hDrive, (pGPT->Head.EfiBackupLBA - 32) * 512, pGPT->PartTbl, 512 * 32))
		{
			Log("UEFI write backup partition table failed");
			goto End;
		}

		if (!WriteDataToPhyDisk(hDrive, 0, pGPT, 512 * 34))
		{
			Log("UEFI write MBR & Main partition table failed");
			goto End;
		}
	}



	//Refresh Drive Layout
	DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &dwSize, NULL);
	
	//We must close handle here, because it will block the refresh bellow
	CHECK_CLOSE_HANDLE(hDrive);

	Sleep(2000);

	//Refresh disk list
	PhyDrive = pPhyDrive->PhyDrive;

	Log("#### Now Refresh PhyDrive ####");
	Ventoy2DiskDestroy();
	Ventoy2DiskInit();
	
	pPhyDrive = GetPhyDriveInfoByPhyDrive(PhyDrive);
	if (pPhyDrive)
	{
		if (pPhyDrive->VentoyVersion[0] == 0)
		{
			Log("After process the Ventoy version is still invalid");
			goto End;
		}

		Log("### Ventoy non-destructive installation successfully finished <%s>", pPhyDrive->VentoyVersion);
	}
	else
	{
		Log("### Ventoy non-destructive installation successfully finished <not found>");
	}

	InitComboxCtrl(g_DialogHwnd, PhyDrive);

	rc = 0;

End:
	CHECK_CLOSE_HANDLE(hDrive);
	return rc;
}


static BOOL DiskCheckWriteAccess(HANDLE hDrive)
{
	DWORD dwSize;
	BOOL ret = FALSE;
	BOOL bRet = FALSE;
	BYTE Buffer[512];
	LARGE_INTEGER liCurPosition;
	LARGE_INTEGER liNewPosition;

	liCurPosition.QuadPart = 2039 * 512;
	liNewPosition.QuadPart = 0;
	if (0 == SetFilePointerEx(hDrive, liCurPosition, &liNewPosition, FILE_BEGIN) ||
		liNewPosition.QuadPart != liCurPosition.QuadPart)
	{
		Log("SetFilePointer1 Failed %u", LASTERR);
		goto out;
	}


	dwSize = 0;
	ret = ReadFile(hDrive, Buffer, 512, &dwSize, NULL);
	if ((!ret) || (dwSize != 512))
	{
		Log("Failed to read %d %u 0x%x", ret, dwSize, LASTERR);
		goto out;
	}


	liCurPosition.QuadPart = 2039 * 512;
	liNewPosition.QuadPart = 0;
	if (0 == SetFilePointerEx(hDrive, liCurPosition, &liNewPosition, FILE_BEGIN) ||
		liNewPosition.QuadPart != liCurPosition.QuadPart)
	{
		Log("SetFilePointer2 Failed %u", LASTERR);
		goto out;
	}

	dwSize = 0;
	ret = WriteFile(hDrive, Buffer, 512, &dwSize, NULL);
	if ((!ret) || dwSize != 512)
	{
		Log("Failed to write %d %u %u", ret, dwSize, LASTERR);
		goto out;
	}

	bRet = TRUE;

out:
	
	return bRet;
}

static BOOL BackupDataBeforeCleanDisk(int PhyDrive, UINT64 DiskSize, BYTE **pBackup)
{
	DWORD dwSize;
	DWORD dwStatus;
	BOOL Return = FALSE;
	BOOL ret = FALSE;
	BYTE *backup = NULL;
	UINT64 offset;
	HANDLE hDrive = INVALID_HANDLE_VALUE;
	LARGE_INTEGER liCurPosition;
	LARGE_INTEGER liNewPosition;
	VTOY_GPT_INFO *pGPT = NULL;

	Log("BackupDataBeforeCleanDisk %d", PhyDrive);

	// step1: check write access
	hDrive = GetPhysicalHandle(PhyDrive, TRUE, TRUE, FALSE);
	if (hDrive == INVALID_HANDLE_VALUE)
	{
		Log("Failed to GetPhysicalHandle for write.");
		goto out;
	}

	if (DiskCheckWriteAccess(hDrive))
	{
		Log("DiskCheckWriteAccess success");
		CHECK_CLOSE_HANDLE(hDrive);
	}
	else
	{
		Log("DiskCheckWriteAccess failed");
		goto out;
	}

	//step2 backup 4MB data
	backup = malloc(SIZE_1MB * 4);
	if (!backup)
	{
		goto out;
	}

	hDrive = GetPhysicalHandle(PhyDrive, FALSE, FALSE, FALSE);
	if (hDrive == INVALID_HANDLE_VALUE)
	{
		goto out;
	}

	//read first 2MB
	dwStatus = SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);
	if (dwStatus != 0)
	{
		goto out;
	}
	
	dwSize = 0;
	ret = ReadFile(hDrive, backup, SIZE_2MB, &dwSize, NULL);
	if ((!ret) || (dwSize != SIZE_2MB))
	{
		Log("Failed to read %d %u 0x%x", ret, dwSize, LASTERR);
		goto out;
	}
	
	pGPT = (VTOY_GPT_INFO *)backup;
	offset = pGPT->Head.EfiBackupLBA * 512;
	if (offset >= (DiskSize - SIZE_2MB) && offset < DiskSize)
	{
		Log("EFI partition table check success"); 
	}
	else
	{
		Log("Backup EFI LBA not in last 2MB range: %llu", pGPT->Head.EfiBackupLBA);
		goto out;
	}

	//read last 2MB
	liCurPosition.QuadPart = DiskSize - SIZE_2MB;
	liNewPosition.QuadPart = 0;
	if (0 == SetFilePointerEx(hDrive, liCurPosition, &liNewPosition, FILE_BEGIN) ||
		liNewPosition.QuadPart != liCurPosition.QuadPart)
	{
		goto out;
	}

	dwSize = 0;
	ret = ReadFile(hDrive, backup + SIZE_2MB, SIZE_2MB, &dwSize, NULL);
	if ((!ret) || (dwSize != SIZE_2MB))
	{
		Log("Failed to read %d %u 0x%x", ret, dwSize, LASTERR);
		goto out;
	}

	*pBackup = backup;
	backup = NULL; //For don't free later
	Return = TRUE;

out:
	CHECK_CLOSE_HANDLE(hDrive);
	if (backup)
		free(backup);

	return Return;
}


static BOOL WriteBackupDataToDisk(HANDLE hDrive, UINT64 Offset, BYTE *Data, DWORD Length)
{
	DWORD dwSize = 0;
	BOOL ret = FALSE;
	LARGE_INTEGER liCurPosition;
	LARGE_INTEGER liNewPosition;

	Log("WriteBackupDataToDisk %llu %p %u", Offset, Data, Length);

	liCurPosition.QuadPart = Offset;
	liNewPosition.QuadPart = 0;
	if (0 == SetFilePointerEx(hDrive, liCurPosition, &liNewPosition, FILE_BEGIN) ||
		liNewPosition.QuadPart != liCurPosition.QuadPart)
	{
		return FALSE;
	}

	ret = WriteFile(hDrive, Data, Length, &dwSize, NULL);
	if ((!ret) || dwSize != Length)
	{
		Log("Failed to write %d %u %u", ret, dwSize, LASTERR);
		return FALSE;
	}

	Log("WriteBackupDataToDisk %llu %p %u success", Offset, Data, Length);
	return TRUE;
}


int UpdateVentoy2PhyDrive(PHY_DRIVE_INFO *pPhyDrive, int TryId)
{
	int i;
	int rc = 0;
	int MaxRetry = 4;
	BOOL ForceMBR = FALSE;
	BOOL Esp2Basic = FALSE;
	BOOL ChangeAttr = FALSE;
	BOOL CleanDisk = FALSE;
	BOOL DelEFI = FALSE;
	BOOL bWriteBack = TRUE;
	HANDLE hVolume;
	HANDLE hDrive;
	DWORD Status;
	DWORD dwSize;
	BOOL bRet;
	CHAR DriveName[] = "?:\\";
	CHAR DriveLetters[MAX_PATH] = { 0 };
	CHAR BackBinFile[MAX_PATH];
	UINT64 StartSector;
	UINT64 ReservedMB = 0;
	MBR_HEAD BootImg;
	MBR_HEAD MBR;
	BYTE *pBackup = NULL;
	VTOY_GPT_INFO *pGptInfo = NULL;
	VTOY_GPT_INFO *pGptBkup = NULL;
	UINT8 ReservedData[4096];

	Log("#####################################################");
	Log("UpdateVentoy2PhyDrive try%d %s PhyDrive%d <<%s %s %dGB>>", TryId,
		pPhyDrive->PartStyle ? "GPT" : "MBR", pPhyDrive->PhyDrive, pPhyDrive->VendorId, pPhyDrive->ProductId,
		GetHumanReadableGBSize(pPhyDrive->SizeInBytes));
	Log("#####################################################");

	PROGRESS_BAR_SET_POS(PT_LOCK_FOR_CLEAN);

	Log("Lock disk for umount ............................ ");

	hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, FALSE, FALSE);
	if (hDrive == INVALID_HANDLE_VALUE)
	{
		Log("Failed to open physical disk");
		return 1;
	}

	if (pPhyDrive->PartStyle)
	{
		pGptInfo = malloc(2 * sizeof(VTOY_GPT_INFO));
		if (!pGptInfo)
		{
			return 1;
		}

		memset(pGptInfo, 0, 2 * sizeof(VTOY_GPT_INFO));
		pGptBkup = pGptInfo + 1;

		// Read GPT Info
		SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);
		ReadFile(hDrive, pGptInfo, sizeof(VTOY_GPT_INFO), &dwSize, NULL);
		memcpy(pGptBkup, pGptInfo, sizeof(VTOY_GPT_INFO));

		//MBR will be used to compare with local boot image
		memcpy(&MBR, &pGptInfo->MBR, sizeof(MBR_HEAD));

		StartSector = pGptInfo->PartTbl[1].StartLBA;
		Log("GPT StartSector in PartTbl:%llu", (ULONGLONG)StartSector);

		ReservedMB = (pPhyDrive->SizeInBytes / 512 - (StartSector + VENTOY_EFI_PART_SIZE / 512) - 33) / 2048;
		Log("GPT Reserved Disk Space:%llu MB", (ULONGLONG)ReservedMB);
	}
	else
	{
		// Read MBR
		SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);
		ReadFile(hDrive, &MBR, sizeof(MBR), &dwSize, NULL);

		StartSector = MBR.PartTbl[1].StartSectorId;
		Log("MBR StartSector in PartTbl:%llu", (ULONGLONG)StartSector);

		ReservedMB = (pPhyDrive->SizeInBytes / 512 - (StartSector + VENTOY_EFI_PART_SIZE / 512)) / 2048;
		Log("MBR Reserved Disk Space:%llu MB", (ULONGLONG)ReservedMB);
	}

	//Read Reserved Data
	SetFilePointer(hDrive, 512 * 2040, NULL, FILE_BEGIN);
	ReadFile(hDrive, ReservedData, sizeof(ReservedData), &dwSize, NULL);

	GetLettersBelongPhyDrive(pPhyDrive->PhyDrive, DriveLetters, sizeof(DriveLetters));

	if (DriveLetters[0] == 0)
	{
		Log("No drive letter was assigned...");
	}
	else
	{
		// Unmount all mounted volumes that belong to this drive
		// Do it in reverse so that we always end on the first volume letter
		for (i = (int)strlen(DriveLetters); i > 0; i--)
		{
			DriveName[0] = DriveLetters[i - 1];
			if (IsVentoyLogicalDrive(DriveName[0]))
			{
				Log("%s is ventoy logical drive", DriveName);
				bRet = DeleteVolumeMountPointA(DriveName);
				Log("Delete mountpoint %s ret:%u code:%u", DriveName, bRet, LASTERR);
				break;
			}
		}
	}

	// It kind of blows, but we have to relinquish access to the physical drive
	// for VDS to be able to delete the partitions that reside on it...
	DeviceIoControl(hDrive, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL);
	CHECK_CLOSE_HANDLE(hDrive);

	if (pPhyDrive->PartStyle == 1)
	{
		Log("TryId=%d EFI GPT partition type is 0x%llx", TryId, pPhyDrive->Part2GPTAttr);
		PROGRESS_BAR_SET_POS(PT_DEL_ALL_PART);

		if (TryId == 1)
		{
			Log("Change GPT partition type to ESP");
			if (DISK_ChangeVtoyEFI2ESP(pPhyDrive->PhyDrive, StartSector * 512ULL))
			{
				Esp2Basic = TRUE;
				Sleep(3000);
			}
		}
		else if (TryId == 2)
		{
			Log("Change GPT partition attribute");
			if (DISK_ChangeVtoyEFIAttr(pPhyDrive->PhyDrive, StartSector * 512ULL, 0x8000000000000001))
			{
				ChangeAttr = TRUE;
				Sleep(2000);
			}
		}
		else if (TryId == 3)
		{
			DISK_DeleteVtoyEFIPartition(pPhyDrive->PhyDrive, StartSector * 512ULL);
			DelEFI = TRUE;
		}
		else if (TryId == 4)
		{
			Log("Clean disk GPT partition table");
			if (BackupDataBeforeCleanDisk(pPhyDrive->PhyDrive, pPhyDrive->SizeInBytes, &pBackup))
			{
				sprintf_s(BackBinFile, sizeof(BackBinFile), ".\\ventoy\\phydrive%d_%u_%d.bin",
					pPhyDrive->PhyDrive, GetCurrentProcessId(), g_backup_bin_index++);
				SaveBufToFile(BackBinFile, pBackup, 4 * SIZE_1MB);
				Log("Save backup data to %s", BackBinFile);

				Log("Success to backup data before clean");
				CleanDisk = TRUE;
				DISK_CleanDisk(pPhyDrive->PhyDrive);
				Sleep(3000);
			}
			else
			{
				Log("Failed to backup data before clean");
			}
		}
	}
	
    PROGRESS_BAR_SET_POS(PT_LOCK_FOR_WRITE);

    Log("Lock disk for update ............................ ");
    hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, TRUE, FALSE);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Failed to GetPhysicalHandle for write.");
        rc = 1;
        goto End;
    }

    PROGRESS_BAR_SET_POS(PT_LOCK_VOLUME);

    Log("Lock volume for update .......................... ");
    hVolume = INVALID_HANDLE_VALUE;

	//If we change VTOYEFI to ESP, it can not have s volume name, so don't try to get it.
	if (CleanDisk)
	{
		//writeback the last 2MB
		if (!WriteBackupDataToDisk(hDrive, pPhyDrive->SizeInBytes - SIZE_2MB, pBackup + SIZE_2MB, SIZE_2MB))
		{
			bWriteBack = FALSE;
		}

		//write the first 2MB except parttable
		if (!WriteBackupDataToDisk(hDrive, 34 * 512, pBackup + 34 * 512, SIZE_2MB - 34 * 512))
		{
			bWriteBack = FALSE;
		}

		Status = ERROR_NOT_FOUND;
	}
	else if (DelEFI)
	{
		Status = ERROR_NOT_FOUND;
	}
	else if (Esp2Basic)
	{
		Status = ERROR_NOT_FOUND;
	}
	else
	{
		for (i = 0; i < MaxRetry; i++)
		{
			Status = GetVentoyVolumeName(pPhyDrive->PhyDrive, StartSector, DriveLetters, sizeof(DriveLetters), TRUE);
			if (ERROR_SUCCESS == Status)
			{
				break;
			}
			else
			{
				Log("==== Volume not found, wait and retry %d... ====", i);
				Sleep(2);
			}
		}
	}
	
    if (ERROR_SUCCESS == Status)
    {
        Log("Now lock and dismount volume <%s>", DriveLetters);

        for (i = 0; i < MaxRetry; i++)
        {
            hVolume = CreateFileA(DriveLetters,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
                NULL);

            if (hVolume == INVALID_HANDLE_VALUE)
            {
                Log("Failed to create file volume, errcode:%u, wait and retry ...", LASTERR);
                Sleep(2000);
            }
            else
            {
                break;
            }
        }

        if (hVolume == INVALID_HANDLE_VALUE)
        {
            Log("Failed to create file volume, errcode:%u", LASTERR);
        }
        else
        {
            bRet = DeviceIoControl(hVolume, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL);
            Log("FSCTL_LOCK_VOLUME bRet:%u code:%u", bRet, LASTERR);

            bRet = DeviceIoControl(hVolume, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL);
            Log("FSCTL_DISMOUNT_VOLUME bRet:%u code:%u", bRet, LASTERR);
        }
    }
    else if (ERROR_NOT_FOUND == Status)
    {
        Log("Volume not found, maybe not supported");
    }
    else
    {
        rc = 1;
        goto End;
    }

	bRet = TryWritePart2(hDrive, StartSector);
	if (FALSE == bRet && Esp2Basic)
	{
		Log("TryWritePart2 agagin ...");
		Sleep(3000);
		bRet = TryWritePart2(hDrive, StartSector);
	}

	if (!bRet)
    {
		if (pPhyDrive->PartStyle == 0)
		{
			if (DiskCheckWriteAccess(hDrive))
			{
				Log("MBR DiskCheckWriteAccess success");

				ForceMBR = TRUE;

				Log("Try write failed, now delete partition 2 for MBR...");
				CHECK_CLOSE_HANDLE(hDrive);

				Log("Now delete partition 2...");
				DISK_DeleteVtoyEFIPartition(pPhyDrive->PhyDrive, StartSector * 512ULL);

				hDrive = GetPhysicalHandle(pPhyDrive->PhyDrive, TRUE, TRUE, FALSE);
				if (hDrive == INVALID_HANDLE_VALUE)
				{
					Log("Failed to GetPhysicalHandle for write.");
					rc = 1;
					goto End;
				}
			}
			else
			{
				Log("MBR DiskCheckWriteAccess failed");
			}
		}
		else
		{
			Log("TryWritePart2 failed ....");
			rc = 1;
			goto End;
		}
    }

    PROGRESS_BAR_SET_POS(PT_FORMAT_PART2);

    Log("Write Ventoy to disk ............................ ");
    if (0 != FormatPart2Fat(hDrive, StartSector))
    {
        rc = 1;
        goto End;
    }

    if (hVolume != INVALID_HANDLE_VALUE)
    {
        bRet = DeviceIoControl(hVolume, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL);
        Log("FSCTL_UNLOCK_VOLUME bRet:%u code:%u", bRet, LASTERR);
        CHECK_CLOSE_HANDLE(hVolume);
    }

    Log("Updating Boot Image ............................. ");
    if (WriteGrubStage1ToPhyDrive(hDrive, pPhyDrive->PartStyle) != 0)
    {
        rc = 1;
        goto End;
    }

    //write reserved data
    SetFilePointer(hDrive, 512 * 2040, NULL, FILE_BEGIN);    
    bRet = WriteFile(hDrive, ReservedData, sizeof(ReservedData), &dwSize, NULL);
    Log("Write resv data ret:%u dwSize:%u Error:%u", bRet, dwSize, LASTERR);

    // Boot Image
    VentoyGetLocalBootImg(&BootImg);

    // Use Old UUID
    memcpy(BootImg.BootCode + 0x180, MBR.BootCode + 0x180, 16);
    if (pPhyDrive->PartStyle)
    {
        BootImg.BootCode[92] = 0x22;
    }

    if (ForceMBR == FALSE && memcmp(BootImg.BootCode, MBR.BootCode, 440) == 0)
    {
        Log("Boot image has no difference, no need to write.");
    }
    else
    {
        Log("Boot image need to write %u.", ForceMBR);

        SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);

        memcpy(MBR.BootCode, BootImg.BootCode, 440);
        bRet = WriteFile(hDrive, &MBR, 512, &dwSize, NULL);
        Log("Write Boot Image ret:%u dwSize:%u Error:%u", bRet, dwSize, LASTERR);
    }

    if (pPhyDrive->PartStyle == 0)
    {
        if (0x00 == MBR.PartTbl[0].Active && 0x80 == MBR.PartTbl[1].Active)
        {
            Log("Need to chage 1st partition active and 2nd partition inactive.");

            MBR.PartTbl[0].Active = 0x80;
            MBR.PartTbl[1].Active = 0x00;

            SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);
            bRet = WriteFile(hDrive, &MBR, 512, &dwSize, NULL);
            Log("Write NEW MBR ret:%u dwSize:%u Error:%u", bRet, dwSize, LASTERR);
        }
    }

	if (CleanDisk)
	{
		if (!WriteBackupDataToDisk(hDrive, 0, pBackup, 34 * 512))
		{
			bWriteBack = FALSE;
		}

		free(pBackup);

		if (bWriteBack)
		{
			Log("Write backup data success, now delete %s", BackBinFile);
			DeleteFileA(BackBinFile);
		}
		else
		{
			Log("Write backup data failed");
		}

		Sleep(1000);
	}
	else if (DelEFI)
	{
		VTOY_GPT_HDR BackupHdr;

		VentoyFillBackupGptHead(pGptBkup, &BackupHdr);
		if (!WriteBackupDataToDisk(hDrive, 512 * pGptBkup->Head.EfiBackupLBA, (BYTE*)(&BackupHdr), 512))
		{
			bWriteBack = FALSE;
		}

		if (!WriteBackupDataToDisk(hDrive, 512 * (pGptBkup->Head.EfiBackupLBA - 32), (BYTE*)(pGptBkup->PartTbl), 32 * 512))
		{
			bWriteBack = FALSE;
		}

		if (!WriteBackupDataToDisk(hDrive, 512, (BYTE*)pGptBkup + 512, 33 * 512))
		{
			bWriteBack = FALSE;
		}

		if (bWriteBack)
		{
			Log("Write backup partition table success");
		}
		else
		{
			Log("Write backup partition table failed");
		}

		Sleep(1000);
	}

    //Refresh Drive Layout
    DeviceIoControl(hDrive, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &dwSize, NULL);

End:

	if (hVolume != INVALID_HANDLE_VALUE)
	{
		bRet = DeviceIoControl(hVolume, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &dwSize, NULL);
		Log("FSCTL_UNLOCK_VOLUME bRet:%u code:%u", bRet, LASTERR);
		CHECK_CLOSE_HANDLE(hVolume);
	}

    if (rc == 0)
    {
        Log("OK");
    }
    else
    {
		PROGRESS_BAR_SET_POS(PT_LOCK_FOR_CLEAN);
        FindProcessOccupyDisk(hDrive, pPhyDrive);
    }

    CHECK_CLOSE_HANDLE(hDrive);

    if (Esp2Basic)
    {
		Log("Recover GPT partition type to basic");
		DISK_ChangeVtoyEFI2Basic(pPhyDrive->PhyDrive, StartSector * 512);
    }

	if (pPhyDrive->PartStyle == 1)
	{
		if (ChangeAttr || ((pPhyDrive->Part2GPTAttr >> 56) != 0xC0))
		{
			Log("Change EFI partition attr %u <0x%llx> to <0x%llx>", ChangeAttr, pPhyDrive->Part2GPTAttr, 0xC000000000000001ULL);
			if (DISK_ChangeVtoyEFIAttr(pPhyDrive->PhyDrive, StartSector * 512ULL, 0xC000000000000001ULL))
			{
				Log("Change EFI partition attr success");
				pPhyDrive->Part2GPTAttr = 0xC000000000000001ULL;
			}
			else
			{
				Log("Change EFI partition attr failed");
			}
		}
	}

    if (pGptInfo)
    {
        free(pGptInfo);
    }
    
    return rc;
}


