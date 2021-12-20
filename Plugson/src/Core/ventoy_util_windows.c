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
#include <ventoy_define.h>
#include <ventoy_util.h>
#include <ventoy_disk.h>
#include "fat_filelib.h"

static void TrimString(CHAR *String)
{
    CHAR *Pos1 = String;
    CHAR *Pos2 = String;
    size_t Len = strlen(String);

    while (Len > 0)
    {
        if (String[Len - 1] != ' ' && String[Len - 1] != '\t')
        {
            break;
        }
        String[Len - 1] = 0;
        Len--;
    }

    while (*Pos1 == ' ' || *Pos1 == '\t')
    {
        Pos1++;
    }

    while (*Pos1)
    {
        *Pos2++ = *Pos1++;
    }
    *Pos2++ = 0;

    return;
}

void ventoy_gen_preudo_uuid(void *uuid)
{
    CoCreateGuid((GUID *)uuid);
}

static int IsUTF8Encode(const char *src)
{
    int i;
    const UCHAR *Byte = (const UCHAR *)src;

    for (i = 0; i < MAX_PATH && Byte[i]; i++)
    {
        if (Byte[i] > 127)
        {
            return 1;
        }
    }

    return 0;
}

static int Utf8ToUtf16(const char* src, WCHAR * dst)
{
	return MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, MAX_PATH * sizeof(WCHAR));
}

static int Utf16ToUtf8(const WCHAR* src, CHAR * dst)
{
	int size = WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, MAX_PATH, NULL, 0);
	dst[size] = 0;
	return size;
}


int ventoy_path_case(char *path, int slash)
{
	int i;
	int j = 0;
    int count = 0;
	int isUTF8 = 0;
	BOOL bRet;
	HANDLE handle = INVALID_HANDLE_VALUE;
	WCHAR Buffer[MAX_PATH + 16];
	WCHAR FilePathW[MAX_PATH];
	CHAR FilePathA[MAX_PATH];
	FILE_NAME_INFO *pInfo = NULL;

	if (g_sysinfo.pathcase == 0)
	{
		return 0;
	}

    if (path == NULL || path[0] == '/' || path[0] == '\\')
    {
        return 0;
    }

	isUTF8 = IsUTF8Encode(path);
	if (isUTF8)
	{
		Utf8ToUtf16(path, FilePathW);
		handle = CreateFileW(FilePathW, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
	}
	else
	{
		handle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
	}

	if (handle != INVALID_HANDLE_VALUE)
	{
		bRet = GetFileInformationByHandleEx(handle, FileNameInfo, Buffer, sizeof(Buffer));
		if (bRet)
		{
			pInfo = (FILE_NAME_INFO *)Buffer;

			if (slash)
			{
				for (i = 0; i < (int)(pInfo->FileNameLength / sizeof(WCHAR)); i++)
				{
					if (pInfo->FileName[i] == L'\\')
					{
						pInfo->FileName[i] = L'/';
					}
				}
			}

			pInfo->FileName[(pInfo->FileNameLength / sizeof(WCHAR))] = 0;

			memset(FilePathA, 0, sizeof(FilePathA));
			Utf16ToUtf8(pInfo->FileName, FilePathA);

			if (FilePathA[1] == ':')
			{
				j = 3;
			}
			else
			{
				j = 1;
			}

			for (i = 0; i < MAX_PATH && j < MAX_PATH; i++, j++)
			{
				if (FilePathA[j] == 0)
				{
					break;
				}
                
				if (path[i] != FilePathA[j])
                {
					path[i] = FilePathA[j];
                    count++;
                }
			}
		}

        CHECK_CLOSE_HANDLE(handle);
	}

    return count;
}



int ventoy_is_directory_exist(const char *Fmt, ...)
{
    va_list Arg;
    DWORD Attr;
    int UTF8 = 0;
    CHAR FilePathA[MAX_PATH];
    WCHAR FilePathW[MAX_PATH];

    va_start(Arg, Fmt);
    vsnprintf_s(FilePathA, sizeof(FilePathA), sizeof(FilePathA), Fmt, Arg);
    va_end(Arg);

    UTF8 = IsUTF8Encode(FilePathA);

    if (UTF8)
    {
        Utf8ToUtf16(FilePathA, FilePathW);
        Attr = GetFileAttributesW(FilePathW);
    }
    else
    {
        Attr = GetFileAttributesA(FilePathA);
    }

    if (Attr != INVALID_FILE_ATTRIBUTES && (Attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        return TRUE;
    }

    return FALSE;
}

int ventoy_is_file_exist(const char *Fmt, ...)
{
    va_list Arg;
    HANDLE hFile;
    DWORD Attr;
    int UTF8 = 0;
    CHAR FilePathA[MAX_PATH];
    WCHAR FilePathW[MAX_PATH];

    va_start(Arg, Fmt);
    vsnprintf_s(FilePathA, sizeof(FilePathA), sizeof(FilePathA), Fmt, Arg);
    va_end(Arg);

    UTF8 = IsUTF8Encode(FilePathA);
    if (UTF8)
    {
        Utf8ToUtf16(FilePathA, FilePathW);
        hFile = CreateFileW(FilePathW, FILE_READ_EA, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        Attr = GetFileAttributesW(FilePathW);
    }
    else
    {
        hFile = CreateFileA(FilePathA, FILE_READ_EA, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        Attr = GetFileAttributesA(FilePathA);
    }

    if (INVALID_HANDLE_VALUE == hFile)
    {
        return 0;
    }
    CloseHandle(hFile);

    if (Attr & FILE_ATTRIBUTE_DIRECTORY)
    {
        return 0;
    }

    return 1;
}

const char * ventoy_get_os_language(void)
{
    if (GetUserDefaultUILanguage() == 0x0804)
    {
        return "cn";
    }
    else
    {
        return "en";
    }
}


int GetPhyDriveByLogicalDrive(int DriveLetter, UINT64 *Offset)
{
    BOOL Ret;
    DWORD dwSize;
    HANDLE Handle;
    VOLUME_DISK_EXTENTS DiskExtents;
    CHAR PhyPath[128];

    scnprintf(PhyPath, sizeof(PhyPath), "\\\\.\\%C:", (CHAR)DriveLetter);

    Handle = CreateFileA(PhyPath, 0, 0, 0, OPEN_EXISTING, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE)
    {
		vlog("CreateFileA %s failed %u\n", PhyPath, LASTERR);
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
		vlog("IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTSfailed %u\n", LASTERR);
        CHECK_CLOSE_HANDLE(Handle);
        return -1;
    }
    CHECK_CLOSE_HANDLE(Handle);

    if (Offset)
    {
        *Offset = (UINT64)(DiskExtents.Extents[0].StartingOffset.QuadPart);
    }

    return (int)DiskExtents.Extents[0].DiskNumber;
}

int GetPhyDriveInfo(int PhyDrive, UINT64 *Size, CHAR *Vendor, CHAR *Product)
{
    int ret = 1;
    BOOL bRet;
    DWORD dwBytes;
	CHAR Drive[64];
    HANDLE Handle = INVALID_HANDLE_VALUE;
    GET_LENGTH_INFORMATION LengthInfo;
    STORAGE_PROPERTY_QUERY Query;
    STORAGE_DESCRIPTOR_HEADER DevDescHeader;
    STORAGE_DEVICE_DESCRIPTOR *pDevDesc = NULL;

	sprintf_s(Drive, sizeof(Drive), "\\\\.\\PhysicalDrive%d", PhyDrive);
	Handle = CreateFileA(Drive, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (Handle == INVALID_HANDLE_VALUE)
    {
		vlog("CreateFileA %s failed %u\n", Drive, LASTERR);
        goto out;
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
		vlog("IOCTL_DISK_GET_LENGTH_INFO failed %u\n", LASTERR);
        return 1;
    }

    if (Size)
    {
        *Size = (UINT64)LengthInfo.Length.QuadPart;
    }

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
		vlog("IOCTL_STORAGE_QUERY_PROPERTY failed %u\n", LASTERR);
        goto out;
    }

    if (DevDescHeader.Size < sizeof(STORAGE_DEVICE_DESCRIPTOR))
    {
		vlog("DevDescHeader.size invalid %u\n", DevDescHeader.Size);
        goto out;
    }

    pDevDesc = (STORAGE_DEVICE_DESCRIPTOR *)malloc(DevDescHeader.Size);
    if (!pDevDesc)
    {
		vlog("malloc failed\n");
        goto out;
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
		vlog("IOCTL_STORAGE_QUERY_PROPERTY failed %u\n", LASTERR);
        goto out;
    }

    if (pDevDesc->VendorIdOffset && Vendor)
    {
        strcpy_s(Vendor, 128, (char *)pDevDesc + pDevDesc->VendorIdOffset);
        TrimString(Vendor);
    }

    if (pDevDesc->ProductIdOffset && Product)
    {
        strcpy_s(Product, 128, (char *)pDevDesc + pDevDesc->ProductIdOffset);
        TrimString(Product);
    }

    ret = 0;

out:
    CHECK_FREE(pDevDesc);
    CHECK_CLOSE_HANDLE(Handle);
    return ret;
}


int ventoy_get_file_size(const char *file)
{
	int Size = -1;
	HANDLE hFile;
	
	hFile = CreateFileA(file, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		Size = (int)GetFileSize(hFile, NULL);
		CHECK_CLOSE_HANDLE(hFile);
	}

	return Size;
}


static HANDLE g_FatPhyDrive;
static UINT64 g_Part2StartSec;

const CHAR* ParseVentoyVersionFromString(CHAR *Buf)
{
	CHAR *Pos = NULL;
	CHAR *End = NULL;
	static CHAR LocalVersion[64] = { 0 };
	
	Pos = strstr(Buf, "VENTOY_VERSION=");
	if (Pos)
	{
		Pos += strlen("VENTOY_VERSION=");
		if (*Pos == '"')
		{
			Pos++;
		}

		End = Pos;
		while (*End != 0 && *End != '"' && *End != '\r' && *End != '\n')
		{
			End++;
		}

		*End = 0;

		sprintf_s(LocalVersion, sizeof(LocalVersion), "%s", Pos);
		return LocalVersion;
	}

	return "";
}
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
        
    }

    return 1;
}

static int GetVentoyVersion(int PhyDrive, ventoy_disk *disk)
{
    int ret = 1;
    BOOL bRet;
    DWORD dwBytes;
    UINT64 Part2Offset;
    HANDLE Handle = INVALID_HANDLE_VALUE;
	VTOY_GPT_INFO *pGPT = NULL;
	CHAR Drive[64];
    void *flfile = NULL;
    UCHAR MbrData[] = 
    {
        0xEB, 0x63, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x56, 0x54, 0x00, 0x47, 0x65, 0x00, 0x48, 0x44, 0x00, 0x52, 0x64, 0x00, 0x20, 0x45, 0x72, 0x0D,
    };

	sprintf_s(Drive, sizeof(Drive), "\\\\.\\PhysicalDrive%d", PhyDrive);
	Handle = CreateFileA(Drive, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (Handle == INVALID_HANDLE_VALUE)
    {
		vlog("CreateFileA %s failed %u\n", Drive, LASTERR);
        goto out;
    }

	pGPT = zalloc(sizeof(VTOY_GPT_INFO));
	if (!pGPT)
	{
		goto out;
	}

	bRet = ReadFile(Handle, pGPT, sizeof(VTOY_GPT_INFO), &dwBytes, NULL);
	if (!bRet || dwBytes != sizeof(VTOY_GPT_INFO))
    {
		vlog("ReadFile failed %u\n", LASTERR);
        goto out;
    }

	if (memcmp(pGPT->MBR.BootCode, MbrData, 0x30) || memcmp(pGPT->MBR.BootCode + 0x190, MbrData + 0x30, 0x10))
    {
		vlog("Invalid MBR Code %u\n", LASTERR);
        goto out;
    }

	if (pGPT->MBR.PartTbl[0].FsFlag == 0xEE)
    {
		if (memcmp(pGPT->Head.Signature, "EFI PART", 8))
        {
			vlog("Invalid GPT Signature\n");
            goto out;
        }

		Part2Offset = pGPT->PartTbl[1].StartLBA;
        disk->cur_part_style = 1;
    }
    else
    {
		Part2Offset = pGPT->MBR.PartTbl[1].StartSectorId;
        disk->cur_part_style = 0;        
    }


    g_FatPhyDrive = Handle;
    g_Part2StartSec = Part2Offset;

    fl_init();

    if (0 == fl_attach_media(VentoyFatDiskRead, NULL))
    {
        ret = GetVentoyVersionFromFatFile(disk->cur_ventoy_ver, sizeof(disk->cur_ventoy_ver));
        if (ret == 0)
        {
            flfile = fl_fopen("/EFI/BOOT/grubx64_real.efi", "rb");
            if (flfile)
            {
                disk->cur_secureboot = 1;
                fl_fclose(flfile);
            }
        }
    }
    
    fl_shutdown();

out:
	CHECK_FREE(pGPT);
    CHECK_CLOSE_HANDLE(Handle);
    return ret;
}

int CheckRuntimeEnvironment(char Letter, ventoy_disk *disk)
{
    int PhyDrive;
    UINT64 Offset = 0;
    char Drive[32];
    DWORD FsFlag;
	CHAR Vendor[128] = { 0 };
	CHAR Product[128] = { 0 };
    CHAR FsName[MAX_PATH];

    PhyDrive = GetPhyDriveByLogicalDrive(Letter, &Offset);
    if (PhyDrive < 0)
    {
		vlog("GetPhyDriveByLogicalDrive failed %d %llu\n", PhyDrive, (ULONGLONG)Offset);
        return 1;
    }
	if (Offset != 1048576)
	{
		vlog("Partition offset is NOT 1MB. This is NOT ventoy image partition (%llu)\n", (ULONGLONG)Offset);
		return 1;
	}

    if (GetPhyDriveInfo(PhyDrive, &Offset, Vendor, Product) != 0)
    {
		vlog("GetPhyDriveInfo failed\n");
        return 1;
    }

    sprintf_s(disk->cur_capacity, sizeof(disk->cur_capacity), "%dGB", (int)ventoy_get_human_readable_gb(Offset));
    sprintf_s(disk->cur_model, sizeof(disk->cur_model), "%s %s", Vendor, Product);

    scnprintf(Drive, sizeof(Drive), "%C:\\", Letter);
    if (0 == GetVolumeInformationA(Drive, NULL, 0, NULL, NULL, &FsFlag, FsName, MAX_PATH))
    {
		vlog("GetVolumeInformationA failed %u\n", LASTERR);
        return 1;
    }

    if (_stricmp(FsName, "NTFS") == 0)
    {
        disk->pathcase = 1;
    }
    
    strlcpy(disk->cur_fsname, FsName);

    if (GetVentoyVersion(PhyDrive, disk) != 0)
    {
		vlog("GetVentoyVersion failed %u\n", LASTERR);
        return 1;
    }

    return 0;
}



static volatile int g_thread_stop = 0;
static HANDLE g_writeback_thread;
static HANDLE g_writeback_event;

DWORD WINAPI ventoy_local_thread_run(LPVOID lpParameter)
{
	ventoy_http_writeback_pf callback = (ventoy_http_writeback_pf)lpParameter;

    while (1)
    {
        WaitForSingleObject(g_writeback_event, INFINITE);
        if (g_thread_stop)
        {
            break;
        }
        else
        {
            callback();
        }
    }

    return 0;
}


void ventoy_set_writeback_event(void)
{
    SetEvent(g_writeback_event);
}


int ventoy_start_writeback_thread(ventoy_http_writeback_pf callback)
{
    g_thread_stop = 0;
    g_writeback_event = CreateEventA(NULL, FALSE, FALSE, "VTOYWRBK");
    g_writeback_thread = CreateThread(NULL, 0, ventoy_local_thread_run, callback, 0, NULL);

    return 0;
}


void ventoy_stop_writeback_thread(void)
{
    g_thread_stop = 1;
    ventoy_set_writeback_event();
    
    WaitForSingleObject(g_writeback_thread, INFINITE);

    CHECK_CLOSE_HANDLE(g_writeback_thread);
    CHECK_CLOSE_HANDLE(g_writeback_event);
}

int ventoy_read_file_to_buf(const char *FileName, int ExtLen, void **Bufer, int *BufLen)
{
    int UTF8 = 0;
    int Size = 0;
    BOOL bRet;
    DWORD dwBytes;
    HANDLE hFile;
    char *buffer = NULL;
    WCHAR FilePathW[MAX_PATH];

    UTF8 = IsUTF8Encode(FileName);
    if (UTF8)
    {
        Utf8ToUtf16(FileName, FilePathW);
        hFile = CreateFileW(FilePathW, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    }
    else
    {
        hFile = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    }

    if (hFile == INVALID_HANDLE_VALUE)
    {
		vlog("Failed to open %s %u\n", FileName, LASTERR);
        return 1;
    }

    Size = (int)GetFileSize(hFile, NULL);
    buffer = malloc(Size + ExtLen);
    if (!buffer)
    {
        vlog("Failed to alloc file buffer\n");
        CloseHandle(hFile);
        return 1;
    }

    bRet = ReadFile(hFile, buffer, (DWORD)Size, &dwBytes, NULL);
    if ((!bRet) || ((DWORD)Size != dwBytes))
    {
        vlog("Failed to read file <%s> %u err:%u", FileName, dwBytes, LASTERR);
        CloseHandle(hFile);
        free(buffer);
        return 1;
    }

    *Bufer = buffer;
    *BufLen = Size;
    
    CloseHandle(hFile);
    return 0;
}

int ventoy_write_buf_to_file(const char *FileName, void *Bufer, int BufLen)
{
    BOOL bRet;
    DWORD dwBytes;
    HANDLE hFile;

    hFile = CreateFileA(FileName, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (hFile == INVALID_HANDLE_VALUE)
    {
		vlog("CreateFile %s failed %u\n", FileName, LASTERR);
        return 1;
    }

    bRet = WriteFile(hFile, Bufer, (DWORD)BufLen, &dwBytes, NULL);

    if ((!bRet) || ((DWORD)BufLen != dwBytes))
    {
        vlog("Failed to write file <%s> %u err:%u", FileName, dwBytes, LASTERR);
        CloseHandle(hFile);
        return 1;
    }
    
    FlushFileBuffers(hFile);
    CloseHandle(hFile);

    return 0;
}

int ventoy_copy_file(const char *a, const char *b)
{
    CopyFileA(a, b, FALSE);
    return 0;
}

