/******************************************************************************
* vtoyjump.c
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
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <virtdisk.h>
#include <winioctl.h>
#include <VersionHelpers.h>
#include "vtoyjump.h"
#include "fat_filelib.h"

static ventoy_os_param g_os_param;
static ventoy_windows_data g_windows_data;
static UINT8 g_os_param_reserved[32];
static INT g_system_bit = VTOY_BIT;
static ventoy_guid g_ventoy_guid = VENTOY_GUID;
static HANDLE g_vtoylog_mutex = NULL;
static HANDLE g_vtoyins_mutex = NULL;

static CHAR g_prog_full_path[MAX_PATH];
static CHAR g_prog_dir[MAX_PATH];
static CHAR g_prog_name[MAX_PATH];

#define VTOY_PECMD_PATH      "X:\\Windows\\system32\\ventoy\\PECMD.EXE"
#define ORG_PECMD_PATH       "X:\\Windows\\system32\\PECMD.EXE"
#define ORG_PECMD_BK_PATH    "X:\\Windows\\system32\\PECMD.EXE_BACK.EXE"

#define AUTO_RUN_BAT    "X:\\VentoyAutoRun.bat"
#define AUTO_RUN_LOG    "X:\\VentoyAutoRun.log"

#define LOG_FILE  "X:\\Windows\\system32\\ventoy.log"
#define MUTEX_LOCK(hmutex)  if (hmutex != NULL) LockStatus = WaitForSingleObject(hmutex, INFINITE)
#define MUTEX_UNLOCK(hmutex)  if (hmutex != NULL && WAIT_OBJECT_0 == LockStatus) ReleaseMutex(hmutex)

static const char * GetFileNameInPath(const char *fullpath)
{
	int i;

	if (strstr(fullpath, ":"))
	{
		for (i = (int)strlen(fullpath); i > 0; i--)
		{
			if (fullpath[i - 1] == '/' || fullpath[i - 1] == '\\')
			{
				return fullpath + i;
			}
		}
	}

	return fullpath;
}

static int split_path_name(char *fullpath, char *dir, char *name)
{
    CHAR ch;
    CHAR *Pos = NULL;

    Pos = (CHAR *)GetFileNameInPath(fullpath);

    strcpy_s(name, MAX_PATH, Pos);

    ch = *(Pos - 1);
    *(Pos - 1) = 0;
    strcpy_s(dir, MAX_PATH, fullpath);
    *(Pos - 1) = ch;

    return 0;
}


void Log(const char *Fmt, ...)
{
	va_list Arg;
	int Len = 0;
	FILE *File = NULL;
	SYSTEMTIME Sys;
	char szBuf[1024];
    DWORD LockStatus = 0;
    DWORD PID = GetCurrentProcessId();

	GetLocalTime(&Sys);
	Len += sprintf_s(szBuf, sizeof(szBuf),
		"[%4d/%02d/%02d %02d:%02d:%02d.%03d] [%u] ",
		Sys.wYear, Sys.wMonth, Sys.wDay,
		Sys.wHour, Sys.wMinute, Sys.wSecond,
        Sys.wMilliseconds, PID);

	va_start(Arg, Fmt);
	Len += vsnprintf_s(szBuf + Len, sizeof(szBuf)-Len, sizeof(szBuf)-Len, Fmt, Arg);
	va_end(Arg);

    MUTEX_LOCK(g_vtoylog_mutex);

    fopen_s(&File, LOG_FILE, "a+");
    if (File)
    {
        fwrite(szBuf, 1, Len, File);
        fwrite("\n", 1, 1, File);
        fclose(File);
    }

    MUTEX_UNLOCK(g_vtoylog_mutex);
}


static int LoadNtDriver(const char *DrvBinPath)
{
	int i;
	int rc = 0;
	BOOL Ret;
	DWORD Status;
	SC_HANDLE hServiceMgr;
	SC_HANDLE hService;
	char name[256] = { 0 };

	for (i = (int)strlen(DrvBinPath) - 1; i >= 0; i--)
	{
		if (DrvBinPath[i] == '\\' || DrvBinPath[i] == '/')
		{
			sprintf_s(name, sizeof(name), "%s", DrvBinPath + i + 1);
			break;
		}
	}

	Log("Load NT driver: %s %s", DrvBinPath, name);

	hServiceMgr = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (hServiceMgr == NULL)
	{
		Log("OpenSCManager failed Error:%u", GetLastError());
		return 1;
	}

	Log("OpenSCManager OK");

	hService = CreateServiceA(hServiceMgr,
		name,
		name,
		SERVICE_ALL_ACCESS,
		SERVICE_KERNEL_DRIVER,
		SERVICE_DEMAND_START,
		SERVICE_ERROR_NORMAL,
		DrvBinPath,
		NULL, NULL, NULL, NULL, NULL);
	if (hService == NULL)
	{
		Status = GetLastError();
		if (Status != ERROR_IO_PENDING && Status != ERROR_SERVICE_EXISTS)
		{
			Log("CreateService failed v %u", Status);
			CloseServiceHandle(hServiceMgr);
			return 1;
		}

		hService = OpenServiceA(hServiceMgr, name, SERVICE_ALL_ACCESS);
		if (hService == NULL)
		{
			Log("OpenService failed %u", Status);
			CloseServiceHandle(hServiceMgr);
			return 1;
		}
	}

	Log("CreateService imdisk OK");

	Ret = StartServiceA(hService, 0, NULL);
	if (Ret)
	{
		Log("StartService OK");
	}
	else
	{
		Status = GetLastError();
		if (Status == ERROR_SERVICE_ALREADY_RUNNING)
		{
			rc = 0;
		}
		else
		{
			Log("StartService error  %u", Status);
			rc = 1;
		}
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hServiceMgr);

	Log("Load NT driver %s", rc ? "failed" : "success");

	return rc;
}

static int ReadWholeFile2Buf(const char *Fullpath, void **Data, DWORD *Size)
{
	int rc = 1;
	DWORD FileSize;
	DWORD dwSize;
	HANDLE Handle;
	BYTE *Buffer = NULL;

	Log("ReadWholeFile2Buf <%s>", Fullpath);

	Handle = CreateFileA(Fullpath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	if (Handle == INVALID_HANDLE_VALUE)
	{
		Log("Could not open the file<%s>, error:%u", Fullpath, GetLastError());
		goto End;
	}

	FileSize = SetFilePointer(Handle, 0, NULL, FILE_END);

	Buffer = malloc(FileSize);
	if (!Buffer)
	{
		Log("Failed to alloc memory size:%u", FileSize);
		goto End;
	}

	SetFilePointer(Handle, 0, NULL, FILE_BEGIN);
	if (!ReadFile(Handle, Buffer, FileSize, &dwSize, NULL))
	{
		Log("ReadFile failed, dwSize:%u  error:%u", dwSize, GetLastError());
		goto End;
	}

	*Data = Buffer;
	*Size = FileSize;

	Log("Success read file size:%u", FileSize);

	rc = 0;

End:
	SAFE_CLOSE_HANDLE(Handle);

	return rc;
}

static BOOL CheckPeHead(BYTE *Head)
{
	UINT32 PeOffset;

	if (Head[0] != 'M' || Head[1] != 'Z')
	{
		return FALSE;
	}

	PeOffset = *(UINT32 *)(Head + 60);
	if (*(UINT32 *)(Head + PeOffset) != 0x00004550)
	{
		return FALSE;
	}

	return TRUE;
}


static BOOL CheckOsParam(ventoy_os_param *param)
{
	UINT32 i;
	BYTE Sum = 0;

	if (memcmp(&param->guid, &g_ventoy_guid, sizeof(ventoy_guid)))
	{
		return FALSE;
	}

	for (i = 0; i < sizeof(ventoy_os_param); i++)
	{
		Sum += *((BYTE *)param + i);
	}
	
	if (Sum)
	{
		return FALSE;
	}

	if (param->vtoy_img_location_addr % 4096)
	{
		return FALSE;
	}

	return TRUE;
}

static int SaveBuffer2File(const char *Fullpath, void *Buffer, DWORD Length)
{
	int rc = 1;
	DWORD dwSize;
	HANDLE Handle;

	Log("SaveBuffer2File <%s> len:%u", Fullpath, Length);

	Handle = CreateFileA(Fullpath, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_NEW, 0, 0);
	if (Handle == INVALID_HANDLE_VALUE)
	{
		Log("Could not create new file, error:%u", GetLastError());
		goto End;
	}

	WriteFile(Handle, Buffer, Length, &dwSize, NULL);

	rc = 0;

End:
	SAFE_CLOSE_HANDLE(Handle);

	return rc;
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
    int size = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, 0);
    return MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, size + 1);
}

static BOOL IsDirExist(const char *Fmt, ...)
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

static BOOL IsFileExist(const char *Fmt, ...)
{
	va_list Arg;
	HANDLE hFile;
	DWORD Attr;
    BOOL bRet = FALSE;
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
    }
    else
    {
        hFile = CreateFileA(FilePathA, FILE_READ_EA, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    }
	if (INVALID_HANDLE_VALUE == hFile)
	{
        goto out;
	}

	CloseHandle(hFile);

    if (UTF8)
    {
        Attr = GetFileAttributesW(FilePathW);
    }
    else
    {
        Attr = GetFileAttributesA(FilePathA);
    }
	
    if (Attr & FILE_ATTRIBUTE_DIRECTORY)
    {
        goto out;
    }

    bRet = TRUE;

out:
    Log("File <%s> %s", FilePathA, (bRet ? "exist" : "NOT exist"));
    return bRet;
}

static int GetPhyDiskUUID(const char LogicalDrive, UINT8 *UUID, UINT32 *DiskSig, DISK_EXTENT *DiskExtent)
{
	BOOL Ret;
	DWORD dwSize;
	HANDLE Handle;
	VOLUME_DISK_EXTENTS DiskExtents;
	CHAR PhyPath[128];
	UINT8 SectorBuf[512];

	Log("GetPhyDiskUUID %C", LogicalDrive);

	sprintf_s(PhyPath, sizeof(PhyPath), "\\\\.\\%C:", LogicalDrive);
	Handle = CreateFileA(PhyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	if (Handle == INVALID_HANDLE_VALUE)
	{
		Log("Could not open the disk<%s>, error:%u", PhyPath, GetLastError());
		return 1;
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
		Log("DeviceIoControl IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed, error:%u", GetLastError());
		CloseHandle(Handle);
		return 1;
	}
	CloseHandle(Handle);

    memcpy(DiskExtent, DiskExtents.Extents, sizeof(DISK_EXTENT));
    Log("%C: is in PhysicalDrive%d Offset:%llu", LogicalDrive, DiskExtents.Extents[0].DiskNumber, 
        (ULONGLONG)(DiskExtents.Extents[0].StartingOffset.QuadPart));

	sprintf_s(PhyPath, sizeof(PhyPath), "\\\\.\\PhysicalDrive%d", DiskExtents.Extents[0].DiskNumber);
	Handle = CreateFileA(PhyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	if (Handle == INVALID_HANDLE_VALUE)
	{
		Log("Could not open the disk<%s>, error:%u", PhyPath, GetLastError());
		return 1;
	}

	if (!ReadFile(Handle, SectorBuf, sizeof(SectorBuf), &dwSize, NULL))
	{
		Log("ReadFile failed, dwSize:%u  error:%u", dwSize, GetLastError());
		CloseHandle(Handle);
		return 1;
	}
	
	memcpy(UUID, SectorBuf + 0x180, 16);
    if (DiskSig)
    {
        memcpy(DiskSig, SectorBuf + 0x1B8, 4);
    }

	CloseHandle(Handle);
	return 0;
}

static int VentoyMountAnywhere(HANDLE Handle)
{
    DWORD Status;
    ATTACH_VIRTUAL_DISK_PARAMETERS AttachParameters;

    Log("VentoyMountAnywhere");

    memset(&AttachParameters, 0, sizeof(AttachParameters));
    AttachParameters.Version = ATTACH_VIRTUAL_DISK_VERSION_1;

    Status = AttachVirtualDisk(Handle, NULL, ATTACH_VIRTUAL_DISK_FLAG_READ_ONLY | ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME, 0, &AttachParameters, NULL);
    if (Status != ERROR_SUCCESS)
    {
        Log("Failed to attach virtual disk ErrorCode:%u", Status);
        return 1;
    }

    return 0;
}

int VentoyMountY(HANDLE Handle)
{
    int  i;
    BOOL  bRet = FALSE;
    DWORD Status;
    DWORD physicalDriveNameSize;
    CHAR *Pos = NULL;
    WCHAR physicalDriveName[MAX_PATH];
    CHAR physicalDriveNameA[MAX_PATH];
    CHAR cdromDriveName[MAX_PATH];
    ATTACH_VIRTUAL_DISK_PARAMETERS AttachParameters;

    Log("VentoyMountY");

    memset(&AttachParameters, 0, sizeof(AttachParameters));
    AttachParameters.Version = ATTACH_VIRTUAL_DISK_VERSION_1;

    Status = AttachVirtualDisk(Handle, NULL, ATTACH_VIRTUAL_DISK_FLAG_READ_ONLY | ATTACH_VIRTUAL_DISK_FLAG_NO_DRIVE_LETTER | ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME, 0, &AttachParameters, NULL);
    if (Status != ERROR_SUCCESS)
    {
        Log("Failed to attach virtual disk ErrorCode:%u", Status);
        return 1;
    }

    memset(physicalDriveName, 0, sizeof(physicalDriveName));
    memset(physicalDriveNameA, 0, sizeof(physicalDriveNameA));

    physicalDriveNameSize = MAX_PATH;
    Status = GetVirtualDiskPhysicalPath(Handle, &physicalDriveNameSize, physicalDriveName);
    if (Status != ERROR_SUCCESS)
    {
        Log("Failed GetVirtualDiskPhysicalPath ErrorCode:%u", Status);
        return 1;
    }

    for (i = 0; physicalDriveName[i]; i++)
    {
        physicalDriveNameA[i] = (CHAR)toupper((CHAR)(physicalDriveName[i]));
    }

    Log("physicalDriveNameA=<%s>", physicalDriveNameA);

    Pos = strstr(physicalDriveNameA, "CDROM");
    if (!Pos)
    {
        Log("Not cdrom phy drive");
        return 1;
    }

    sprintf_s(cdromDriveName, sizeof(cdromDriveName), "\\Device\\%s", Pos);
    Log("cdromDriveName=<%s>", cdromDriveName);

    for (i = 0; i < 3 && (bRet == FALSE); i++)
    {
        Sleep(1000);
        bRet = DefineDosDeviceA(DDD_RAW_TARGET_PATH, "Y:", cdromDriveName);
        Log("DefineDosDeviceA %s", bRet ? "success" : "failed");
    }

    return bRet ? 0 : 1;
}

static BOOL VentoyAPINeedMountY(const char *IsoPath)
{
	(void)IsoPath;

    /* TBD */
    return FALSE;
}

static int VentoyAttachVirtualDisk(HANDLE Handle, const char *IsoPath)
{
    int DriveYFree;
    DWORD Drives;
    
    Drives = GetLogicalDrives();
    if ((1 << 24) & Drives)
    {
        Log("Y: is occupied");
        DriveYFree = 0;
    }
    else
    {
        Log("Y: is free now");
        DriveYFree = 1;
    }

	if (DriveYFree && VentoyAPINeedMountY(IsoPath))
    {
        return VentoyMountY(Handle);
    }
    else
    {
        return VentoyMountAnywhere(Handle);
    }
}

int VentoyMountISOByAPI(const char *IsoPath)
{
    int i;
	HANDLE Handle;
	DWORD Status;
	WCHAR wFilePath[512] = { 0 };
	VIRTUAL_STORAGE_TYPE StorageType;
	OPEN_VIRTUAL_DISK_PARAMETERS OpenParameters;

	Log("VentoyMountISOByAPI <%s>", IsoPath);

    if (IsUTF8Encode(IsoPath))
    {
        Log("This is UTF8 encoding");
        MultiByteToWideChar(CP_UTF8, 0, IsoPath, (int)strlen(IsoPath), wFilePath, (int)(sizeof(wFilePath) / sizeof(WCHAR)));
    }
    else
    {
        Log("This is ANSI encoding");
        MultiByteToWideChar(CP_ACP, 0, IsoPath, (int)strlen(IsoPath), wFilePath, (int)(sizeof(wFilePath) / sizeof(WCHAR)));
    }

	memset(&StorageType, 0, sizeof(StorageType));
	memset(&OpenParameters, 0, sizeof(OpenParameters));
	
	OpenParameters.Version = OPEN_VIRTUAL_DISK_VERSION_1;

    for (i = 0; i < 10; i++)
    {
        Status = OpenVirtualDisk(&StorageType, wFilePath, VIRTUAL_DISK_ACCESS_READ, 0, &OpenParameters, &Handle);
        if (ERROR_FILE_NOT_FOUND == Status || ERROR_PATH_NOT_FOUND == Status)
        {
            Log("OpenVirtualDisk ErrorCode:%u, now wait and retry...", Status);
            Sleep(1000);
        }
        else
        {
            if (ERROR_SUCCESS == Status)
            {
                Log("OpenVirtualDisk success");
            }
            else if (ERROR_VIRTDISK_PROVIDER_NOT_FOUND == Status)
            {
                Log("VirtualDisk for ISO file is not supported in current system");
            }
            else
            {
                Log("Failed to open virtual disk ErrorCode:%u", Status);
            }
            break;
        }
    }

    if (Status != ERROR_SUCCESS)
    {
        return 1;
    }

	Log("OpenVirtualDisk success");

    Status = VentoyAttachVirtualDisk(Handle, IsoPath);
	if (Status != ERROR_SUCCESS)
	{
		Log("Failed to attach virtual disk ErrorCode:%u", Status);
		CloseHandle(Handle);
		return 1;
	}

    Log("VentoyAttachVirtualDisk success");

	CloseHandle(Handle);
	return 0;
}


static HANDLE g_FatPhyDrive;
static UINT64 g_Part2StartSec;

static int CopyFileFromFatDisk(const CHAR* SrcFile, const CHAR *DstFile)
{
	int rc = 1;
	int size = 0;
	char *buf = NULL;
	void *flfile = NULL;

	Log("CopyFileFromFatDisk (%s)==>(%s)", SrcFile, DstFile);

	flfile = fl_fopen(SrcFile, "rb");
	if (flfile)
	{
		fl_fseek(flfile, 0, SEEK_END);
		size = (int)fl_ftell(flfile);
		fl_fseek(flfile, 0, SEEK_SET);

		buf = (char *)malloc(size);
		if (buf)
		{
			fl_fread(buf, 1, size, flfile);

			rc = 0;
			SaveBuffer2File(DstFile, buf, size);
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
		Log("ReadFile error bRet:%u WriteSize:%u dwSize:%u ErrCode:%u\n", bRet, ReadSize, dwSize, GetLastError());
	}

	return 1;
}

static BOOL Is2K10PE(void)
{
	BOOL bRet = FALSE;
	FILE *fp = NULL;
	CHAR szLine[1024];

	fopen_s(&fp, "X:\\Windows\\System32\\PECMD.INI", "r");
	if (!fp)
	{
		return FALSE;
	}

	memset(szLine, 0, sizeof(szLine));
	while (fgets(szLine, sizeof(szLine) - 1, fp))
	{
		if (strstr(szLine, "2k10\\"))
		{
			bRet = TRUE;
			break;
		}
	}

	fclose(fp);
	return bRet;
}

static CHAR GetIMDiskMountLogicalDrive(void)
{
	CHAR Letter = 'Y';
	DWORD Drives;
	DWORD Mask = 0x1000000;

	// fixed use M as mountpoint for 2K10 PE
	if (Is2K10PE())
	{
		Log("Use M: for 2K10 PE");
		return 'M';
	}

	Drives = GetLogicalDrives();
    Log("Drives=0x%x", Drives);
    
	while (Mask)
	{
		if ((Drives & Mask) == 0)
		{
			break;
		}

		Letter--;
		Mask >>= 1;
	}

	return Letter;
}

UINT64 GetVentoyEfiPartStartSector(HANDLE hDrive)
{
	BOOL bRet;
	DWORD dwSize; 
	MBR_HEAD MBR;	
	VTOY_GPT_INFO *pGpt = NULL;
	UINT64 StartSector = 0;

	SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);

	bRet = ReadFile(hDrive, &MBR, sizeof(MBR), &dwSize, NULL);
	Log("Read MBR Ret:%u Size:%u code:%u", bRet, dwSize, LASTERR);

	if ((!bRet) || (dwSize != sizeof(MBR)))
	{
		0;
	}

	if (MBR.PartTbl[0].FsFlag == 0xEE)
	{
		Log("GPT partition style");

		pGpt = malloc(sizeof(VTOY_GPT_INFO));
		if (!pGpt)
		{
			return 0;
		}

		SetFilePointer(hDrive, 0, NULL, FILE_BEGIN);
		bRet = ReadFile(hDrive, pGpt, sizeof(VTOY_GPT_INFO), &dwSize, NULL);		
		if ((!bRet) || (dwSize != sizeof(VTOY_GPT_INFO)))
		{
			Log("Failed to read gpt info %d %u %d", bRet, dwSize, LASTERR);
			return 0;
		}

		StartSector = pGpt->PartTbl[1].StartLBA;
		free(pGpt);
	}
	else
	{
		Log("MBR partition style");
		StartSector = MBR.PartTbl[1].StartSectorId;
	}

	Log("GetVentoyEfiPart StartSector: %llu", StartSector);
	return StartSector;
}

static int VentoyRunImdisk(const char *IsoPath, const char *imdiskexe)
{
	CHAR Letter;
	CHAR Cmdline[512];
	WCHAR CmdlineW[512];
	PROCESS_INFORMATION Pi;

	Log("VentoyRunImdisk <%s> <%s>", IsoPath, imdiskexe);

	Letter = GetIMDiskMountLogicalDrive();
	sprintf_s(Cmdline, sizeof(Cmdline), "%s -a -o ro -f \"%s\" -m %C:", imdiskexe, IsoPath, Letter);
	Log("mount iso to %C: use imdisk cmd <%s>", Letter, Cmdline);

	if (IsUTF8Encode(IsoPath))
	{
		STARTUPINFOW Si;
		GetStartupInfoW(&Si);
		Si.dwFlags |= STARTF_USESHOWWINDOW;
		Si.wShowWindow = SW_HIDE;

		Utf8ToUtf16(Cmdline, CmdlineW);
		CreateProcessW(NULL, CmdlineW, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);

		Log("This is UTF8 encoding");
	}
	else
	{
		STARTUPINFOA Si;
		GetStartupInfoA(&Si);
		Si.dwFlags |= STARTF_USESHOWWINDOW;
		Si.wShowWindow = SW_HIDE;

		CreateProcessA(NULL, Cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);

		Log("This is ANSI encoding");
	}

	Log("Wait for imdisk process ...");
	WaitForSingleObject(Pi.hProcess, INFINITE);
	Log("imdisk process finished");

	return 0;
}

int VentoyMountISOByImdisk(const char *IsoPath, DWORD PhyDrive)
{
	int rc = 1;
	BOOL bRet;
	DWORD dwBytes;
	HANDLE hDrive;
	CHAR PhyPath[MAX_PATH];
	GET_LENGTH_INFORMATION LengthInfo;

	Log("VentoyMountISOByImdisk %s", IsoPath);

	if (IsFileExist("X:\\Windows\\System32\\imdisk.exe"))
	{
		Log("imdisk.exe exist, use it directly...");
		VentoyRunImdisk(IsoPath, "imdisk.exe");
		return 0;
	}

	sprintf_s(PhyPath, sizeof(PhyPath), "\\\\.\\PhysicalDrive%d", PhyDrive);
    hDrive = CreateFileA(PhyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	if (hDrive == INVALID_HANDLE_VALUE)
	{
		Log("Could not open the disk<%s>, error:%u", PhyPath, GetLastError());
		goto End;
	}

	bRet = DeviceIoControl(hDrive, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &LengthInfo, sizeof(LengthInfo), &dwBytes, NULL);
	if (!bRet)
	{
		Log("Could not get phy disk %s size, error:%u", PhyPath, GetLastError());
		goto End;
	}

	g_FatPhyDrive = hDrive;
	g_Part2StartSec = GetVentoyEfiPartStartSector(hDrive);

	Log("Parse FAT fs...");

	fl_init();

	if (0 == fl_attach_media(VentoyFatDiskRead, NULL))
	{
		if (g_system_bit == 64)
		{
			CopyFileFromFatDisk("/ventoy/imdisk/64/imdisk.sys", "ventoy\\imdisk.sys");
			CopyFileFromFatDisk("/ventoy/imdisk/64/imdisk.exe", "ventoy\\imdisk.exe");
			CopyFileFromFatDisk("/ventoy/imdisk/64/imdisk.cpl", "ventoy\\imdisk.cpl");
		}
		else
		{
			CopyFileFromFatDisk("/ventoy/imdisk/32/imdisk.sys", "ventoy\\imdisk.sys");
			CopyFileFromFatDisk("/ventoy/imdisk/32/imdisk.exe", "ventoy\\imdisk.exe");
			CopyFileFromFatDisk("/ventoy/imdisk/32/imdisk.cpl", "ventoy\\imdisk.cpl");
		}
		
		GetCurrentDirectoryA(sizeof(PhyPath), PhyPath);
		strcat_s(PhyPath, sizeof(PhyPath), "\\ventoy\\imdisk.sys");

		if (LoadNtDriver(PhyPath) == 0)
		{
			VentoyRunImdisk(IsoPath, "ventoy\\imdisk.exe");
			rc = 0;
		}
	}
	fl_shutdown();

End:

	SAFE_CLOSE_HANDLE(hDrive);

	return rc;
}

static int MountIsoFile(CONST CHAR *IsoPath, DWORD PhyDrive)
{
    if (IsWindows8OrGreater())
    {
        Log("This is Windows 8 or latter...");
        if (VentoyMountISOByAPI(IsoPath) == 0)
        {
            Log("Mount iso by API success");
            return 0;
        }
        else
        {
            Log("Mount iso by API failed, maybe not supported, try imdisk");
            return VentoyMountISOByImdisk(IsoPath, PhyDrive);
        }
    }
    else
    {
        Log("This is before Windows 8 ...");
        if (VentoyMountISOByImdisk(IsoPath, PhyDrive) == 0)
        {
            Log("Mount iso by imdisk success");
            return 0;
        }
        else
        {
            return VentoyMountISOByAPI(IsoPath);
        }
    }
}

static int GetPhyDriveByLogicalDrive(int DriveLetter)
{
    BOOL Ret;
    DWORD dwSize;
    HANDLE Handle;
    VOLUME_DISK_EXTENTS DiskExtents;
    CHAR PhyPath[128];

    sprintf_s(PhyPath, sizeof(PhyPath), "\\\\.\\%C:", (CHAR)DriveLetter);

    Handle = CreateFileA(PhyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        Log("Could not open the disk<%s>, error:%u", PhyPath, GetLastError());
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
        Log("DeviceIoControl IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed %s, error:%u", PhyPath, GetLastError());
        SAFE_CLOSE_HANDLE(Handle);
        return -1;
    }
    SAFE_CLOSE_HANDLE(Handle);

    Log("LogicalDrive:%s PhyDrive:%d Offset:%llu ExtentLength:%llu",
        PhyPath,
        DiskExtents.Extents[0].DiskNumber,
        DiskExtents.Extents[0].StartingOffset.QuadPart,
        DiskExtents.Extents[0].ExtentLength.QuadPart
        );

    return (int)DiskExtents.Extents[0].DiskNumber;
}


static int DeleteVentoyPart2MountPoint(DWORD PhyDrive)
{
    CHAR Letter = 'A';
    DWORD Drives;
    DWORD PhyDisk;
    CHAR DriveName[] = "?:\\";

    Log("DeleteVentoyPart2MountPoint Phy%u ...", PhyDrive);

    Drives = GetLogicalDrives();
    while (Drives)
    {
        if ((Drives & 0x01) && IsFileExist("%C:\\ventoy\\ventoy.cpio", Letter))
        {
            Log("File %C:\\ventoy\\ventoy.cpio exist", Letter);

            PhyDisk = GetPhyDriveByLogicalDrive(Letter);
            Log("PhyDisk=%u for %C", PhyDisk, Letter);

            if (PhyDisk == PhyDrive)
            {
                DriveName[0] = Letter;
                DeleteVolumeMountPointA(DriveName);
                return 0;
            }
        }

        Letter++;
        Drives >>= 1;
    }

    return 1;
}

static BOOL check_tar_archive(const char *archive, CHAR *tarName)
{
    int len;
    int nameLen;
    const char *pos = archive;
    const char *slash = archive;

    while (*pos)
    {
        if (*pos == '\\' || *pos == '/')
        {
            slash = pos;
        }
        pos++;
    }

    len = (int)strlen(slash);

    if (len > 7 && (strncmp(slash + len - 7, ".tar.gz", 7) == 0 || strncmp(slash + len - 7, ".tar.xz", 7) == 0))
    {
        nameLen = (int)sprintf_s(tarName, MAX_PATH, "X:%s", slash);
        tarName[nameLen - 3] = 0;
        return TRUE;
    }
    else if (len > 8 && strncmp(slash + len - 8, ".tar.bz2", 8) == 0)
    {
        nameLen = (int)sprintf_s(tarName, MAX_PATH, "X:%s", slash);
        tarName[nameLen - 4] = 0;
        return TRUE;
    }
    else if (len > 9 && strncmp(slash + len - 9, ".tar.lzma", 9) == 0)
    {
        nameLen = (int)sprintf_s(tarName, MAX_PATH, "X:%s", slash);
        tarName[nameLen - 5] = 0;
        return TRUE;
    }

    return FALSE;
}

static UCHAR *g_unxz_buffer = NULL;
static int g_unxz_len = 0;

static void unxz_error(char *x)
{
    Log("%s", x);
}

static int unxz_flush(void *src, unsigned int size)
{
    memcpy(g_unxz_buffer + g_unxz_len, src, size);
    g_unxz_len += (int)size;

    return (int)size;
}

static int DecompressInjectionArchive(const char *archive, DWORD PhyDrive)
{
    int rc = 1;
    int writelen = 0;
    UCHAR *Buffer = NULL;
    UCHAR *RawBuffer = NULL;
    BOOL bRet;
    DWORD dwBytes;
    DWORD dwSize;
    HANDLE hDrive;
    HANDLE hOut;
    DWORD flags = CREATE_NO_WINDOW;
    CHAR StrBuf[MAX_PATH];
    CHAR tarName[MAX_PATH];
    STARTUPINFOA Si;
    PROCESS_INFORMATION Pi;
    PROCESS_INFORMATION NewPi;
    GET_LENGTH_INFORMATION LengthInfo;
    SECURITY_ATTRIBUTES Sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    Log("DecompressInjectionArchive %s", archive);

    sprintf_s(StrBuf, sizeof(StrBuf), "\\\\.\\PhysicalDrive%d", PhyDrive);
    hDrive = CreateFileA(StrBuf, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (hDrive == INVALID_HANDLE_VALUE)
    {
        Log("Could not open the disk<%s>, error:%u", StrBuf, GetLastError());
        goto End;
    }

    bRet = DeviceIoControl(hDrive, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &LengthInfo, sizeof(LengthInfo), &dwBytes, NULL);
    if (!bRet)
    {
        Log("Could not get phy disk %s size, error:%u", StrBuf, GetLastError());
        goto End;
    }

    g_FatPhyDrive = hDrive;
	g_Part2StartSec = GetVentoyEfiPartStartSector(hDrive);

    Log("Parse FAT fs...");

    fl_init();

    if (0 == fl_attach_media(VentoyFatDiskRead, NULL))
    {
		if (g_system_bit == 64)
        {
            CopyFileFromFatDisk("/ventoy/7z/64/7za.xz", "ventoy\\7za.xz");
        }
        else
        {
            CopyFileFromFatDisk("/ventoy/7z/32/7za.xz", "ventoy\\7za.xz");
        }

        ReadWholeFile2Buf("ventoy\\7za.xz", &Buffer, &dwSize);
        Log("7za.xz file size:%u", dwSize);

        RawBuffer = malloc(SIZE_1MB * 4);
        if (RawBuffer)
        {
            g_unxz_buffer = RawBuffer;
            g_unxz_len = 0;
            unxz(Buffer, (int)dwSize, NULL, unxz_flush, NULL, &writelen, unxz_error);
            if (writelen == (int)dwSize)
            {
                Log("Decompress success 7za.xz(%u) ---> 7za.exe(%d)", dwSize, g_unxz_len);
            }
            else
            {
                Log("Decompress failed 7za.xz(%u) ---> 7za.exe(%u)", dwSize, dwSize);
            }

            SaveBuffer2File("ventoy\\7za.exe", RawBuffer, (DWORD)g_unxz_len);

            g_unxz_buffer = NULL;
            g_unxz_len = 0;
            free(RawBuffer);
        }
        else
        {
            Log("Failed to alloc 4MB memory");
        }

        sprintf_s(StrBuf, sizeof(StrBuf), "ventoy\\7za.exe x -y -aoa -oX:\\ %s", archive);

        Log("extract inject to X:");
        Log("cmdline:<%s>", StrBuf);

        GetStartupInfoA(&Si);

        hOut = CreateFileA("ventoy\\7z.log",
            FILE_APPEND_DATA,
            FILE_SHARE_WRITE | FILE_SHARE_READ,
            &Sa,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

        Si.dwFlags |= STARTF_USESTDHANDLES;

        if (hOut != INVALID_HANDLE_VALUE)
        {
            Si.hStdError = hOut;
            Si.hStdOutput = hOut;
        }

        CreateProcessA(NULL, StrBuf, NULL, NULL, TRUE, flags, NULL, NULL, &Si, &Pi);
        WaitForSingleObject(Pi.hProcess, INFINITE);

        //
        // decompress tar archive, for tar.gz/tar.xz/tar.bz2
        //
        if (check_tar_archive(archive, tarName))
        {
            Log("Decompress tar archive...<%s>", tarName);

            sprintf_s(StrBuf, sizeof(StrBuf), "ventoy\\7za.exe x -y -aoa -oX:\\ %s", tarName);

            CreateProcessA(NULL, StrBuf, NULL, NULL, TRUE, flags, NULL, NULL, &Si, &NewPi);
            WaitForSingleObject(NewPi.hProcess, INFINITE);

            Log("Now delete %s", tarName);
            DeleteFileA(tarName);
        }

        SAFE_CLOSE_HANDLE(hOut);
    }
    fl_shutdown();

End:

    SAFE_CLOSE_HANDLE(hDrive);

    return rc;
}

static int ProcessUnattendedInstallation(const char *script)
{
    DWORD dw;
    HKEY hKey;
    LSTATUS Ret;
    CHAR Letter;
    CHAR CurDir[MAX_PATH];

    Log("Copy unattended XML ...");
    
    GetCurrentDirectory(sizeof(CurDir), CurDir);
    Letter = CurDir[0];
    if ((Letter >= 'A' && Letter <= 'Z') || (Letter >= 'a' && Letter <= 'z'))
    {
        Log("Current Drive Letter: %C", Letter);
    }
    else
    {
        Letter = 'X';
    }
    
    sprintf_s(CurDir, sizeof(CurDir), "%C:\\Autounattend.xml", Letter);
    Log("Copy file <%s> --> <%s>", script, CurDir);
    CopyFile(script, CurDir, FALSE);

    Ret = RegCreateKeyEx(HKEY_LOCAL_MACHINE, "System\\Setup", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dw);
    if (ERROR_SUCCESS == Ret)
    {
        Ret = RegSetValueEx(hKey, "UnattendFile", 0, REG_SZ, CurDir, (DWORD)(strlen(CurDir) + 1));
    }

    return 0;
}

static int Windows11BypassCheck(const char *isofile, const char MntLetter)
{
    int Ret = 1;
    DWORD dwHandle;
    DWORD dwSize;
    DWORD dwValue = 1;
    UINT VerLen = 0;
    CHAR *Buffer = NULL;
    VS_FIXEDFILEINFO* VerInfo = NULL;
    CHAR CheckFile[MAX_PATH];
    UINT16 Major, Minor, Build, Revision;

    Log("Windows11BypassCheck for <%s> %C:", isofile, MntLetter);

    if (FALSE == IsFileExist("%C:\\sources\\boot.wim", MntLetter) ||
        FALSE == IsFileExist("%C:\\sources\\compatresources.dll", MntLetter))
    {
        Log("boot.wim/compatresources.dll not exist, this is not a windows install media.");
        goto End;
    }

    if (FALSE == IsFileExist("%C:\\sources\\install.wim", MntLetter) && 
        FALSE == IsFileExist("%C:\\sources\\install.esd", MntLetter))
    {
        Log("install.wim/install.esd not exist, this is not a windows install media.");
        goto End;
    }

    sprintf_s(CheckFile, sizeof(CheckFile), "%C:\\sources\\compatresources.dll", MntLetter);
    dwSize = GetFileVersionInfoSizeA(CheckFile, &dwHandle);
    if (0 == dwSize)
    {
        Log("Failed to get file version info size: %u", LASTERR);
        goto End;
    }

    Buffer = malloc(dwSize);
    if (!Buffer)
    {
        goto End;
    }

    if (FALSE == GetFileVersionInfoA(CheckFile, dwHandle, dwSize, Buffer))
    {
        Log("Failed to get file version info : %u", LASTERR);
        goto End;
    }

    if (VerQueryValueA(Buffer, "\\", (LPVOID)&VerInfo, &VerLen) && VerLen != 0)
    {
        if (VerInfo->dwSignature == VS_FFI_SIGNATURE)
        {
            Major = HIWORD(VerInfo->dwFileVersionMS);
            Minor = LOWORD(VerInfo->dwFileVersionMS);
            Build = HIWORD(VerInfo->dwFileVersionLS);
            Revision = LOWORD(VerInfo->dwFileVersionLS);

            Log("FileVersionze: <%u %u %u %u>", Major, Minor, Build, Revision);

            if (Major == 10 && Build > 20000)
            {
                Major = 11;
            }

            if (Major != 11)
            {
                Log("This is not Windows 11, not need to bypass.", Major);
                goto End;
            }
        }
    }

    //Now we really need to bypass windows 11 check. create registry
    HKEY hKey = NULL;
    HKEY hSubKey = NULL;
    LSTATUS Status;

    Status = RegCreateKeyExA(HKEY_LOCAL_MACHINE, "System\\Setup", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwSize);
    if (ERROR_SUCCESS != Status)
    {
        Log("Failed to create reg key System\\Setup %u %u", LASTERR, Status);
        goto End;
    }

    Status = RegCreateKeyExA(hKey, "LabConfig", 0, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_CREATE_SUB_KEY, NULL, &hSubKey, &dwSize);
    if (ERROR_SUCCESS != Status)
    {
        Log("Failed to create LabConfig reg  %u %u", LASTERR, Status);
        goto End;
    }

    //set reg value
    Status += RegSetValueExA(hSubKey, "BypassRAMCheck", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
    Status += RegSetValueExA(hSubKey, "BypassTPMCheck", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
    Status += RegSetValueExA(hSubKey, "BypassSecureBootCheck", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
    Status += RegSetValueExA(hSubKey, "BypassStorageCheck", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
    Status += RegSetValueExA(hSubKey, "BypassCPUCheck", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));

    Log("Create bypass registry %s %u", (Status == ERROR_SUCCESS) ? "SUCCESS" : "FAILED", Status);

    Ret = 0;

End:
    if (Buffer)
    {
        free(Buffer);
    }
    
    return Ret; 
}

static BOOL CheckVentoyDisk(DWORD DiskNum)
{
    DWORD dwSize = 0;
    CHAR PhyPath[128];
    UINT8 SectorBuf[512];
    HANDLE Handle;
    UINT8 check[8] = { 0x56, 0x54, 0x00, 0x47, 0x65, 0x00, 0x48, 0x44 };

    sprintf_s(PhyPath, sizeof(PhyPath), "\\\\.\\PhysicalDrive%d", DiskNum);
    Handle = CreateFileA(PhyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        Log("Could not open the disk<%s>, error:%u", PhyPath, GetLastError());
        return FALSE;
    }

    if (!ReadFile(Handle, SectorBuf, sizeof(SectorBuf), &dwSize, NULL))
    {
        Log("ReadFile failed, dwSize:%u  error:%u", dwSize, GetLastError());
        CloseHandle(Handle);
        return FALSE;
    }

    CloseHandle(Handle);

    if (memcmp(SectorBuf + 0x190, check, 8) == 0)
    {
        return TRUE;
    }

    return FALSE;
}


static int VentoyHook(ventoy_os_param *param)
{
    int i;
    int rc;
    BOOL find = FALSE;
    BOOL vtoyfind = FALSE;
    CHAR Letter;
    CHAR MntLetter;
    CHAR VtoyLetter;
    DWORD Drives;
    DWORD NewDrives;
    DWORD VtoyDiskNum;
    UINT32 DiskSig;
    UINT32 VtoySig;
	DISK_EXTENT DiskExtent;
    DISK_EXTENT VtoyDiskExtent;
	UINT8 UUID[16];
	CHAR IsoPath[MAX_PATH];

	Log("VentoyHook Path:<%s>", param->vtoy_img_path);

    if (IsUTF8Encode(param->vtoy_img_path))
    {
        Log("This file is UTF8 encoding\n");
    }

    for (i = 0; i < 5; i++)
    {
        Letter = 'A';
        Drives = GetLogicalDrives();
        Log("Logic Drives: 0x%x", Drives);

        while (Drives)
        {
            if (Drives & 0x01)
            {
                sprintf_s(IsoPath, sizeof(IsoPath), "%C:\\%s", Letter, param->vtoy_img_path);
                if (IsFileExist("%s", IsoPath))
                {
                    Log("File exist under %C:", Letter);
                    memset(UUID, 0, sizeof(UUID));
                    memset(&DiskExtent, 0, sizeof(DiskExtent));
                    if (GetPhyDiskUUID(Letter, UUID, NULL, &DiskExtent) == 0)
                    {
                        if (memcmp(UUID, param->vtoy_disk_guid, 16) == 0)
                        {
                            Log("Disk UUID match");
                            find = TRUE;
                            break;
                        }
                    }
                }
                else
                {
                    Log("File NOT exist under %C:", Letter);
                }
            }

            Drives >>= 1;
            Letter++;
        }

        if (find)
        {
            break;
        }
        else
        {
            Log("Now wait and retry ...");
            Sleep(1000);
        }
    }

    if (find == FALSE)
	{
		Log("Failed to find ISO file");
		return 1;
	}

	Log("Find ISO file <%s>", IsoPath);
    
    //Find VtoyLetter in Vlnk Mode
    if (g_os_param_reserved[6] == 1)
    {
        memcpy(&VtoySig, g_os_param_reserved + 7, 4);
        for (i = 0; i < 5; i++)
        {
            VtoyLetter = 'A';
            Drives = GetLogicalDrives();
            Log("Logic Drives: 0x%x  VentoySig:%08X", Drives, VtoySig);

            while (Drives)
            {
                if (Drives & 0x01)
                {
                    memset(UUID, 0, sizeof(UUID));
                    memset(&VtoyDiskExtent, 0, sizeof(VtoyDiskExtent));
                    DiskSig = 0;
                    if (GetPhyDiskUUID(VtoyLetter, UUID, &DiskSig, &VtoyDiskExtent) == 0)
                    {
                        Log("DiskSig=%08X PartStart=%lld", DiskSig, VtoyDiskExtent.StartingOffset.QuadPart);
                        if (DiskSig == VtoySig && VtoyDiskExtent.StartingOffset.QuadPart == SIZE_1MB)
                        {
                            Log("Ventoy Disk Sig match");
                            vtoyfind = TRUE;
                            break;
                        }
                    }
                }

                Drives >>= 1;
                VtoyLetter++;
            }

            if (vtoyfind)
            {
                Log("Find Ventoy Letter: %C", VtoyLetter);
                break;
            }
            else
            {
                Log("Now wait and retry ...");
                Sleep(1000);
            }
        }

        if (vtoyfind == FALSE)
        {
            Log("Failed to find ventoy disk");
            return 1;
        }

        VtoyDiskNum = VtoyDiskExtent.DiskNumber;
    }
    else
    {
        VtoyLetter = Letter;
        Log("No vlnk mode %C", Letter);

        VtoyDiskNum = DiskExtent.DiskNumber;
    }

    if (CheckVentoyDisk(VtoyDiskNum))
    {
        Log("Disk check OK %C: %u", VtoyLetter, VtoyDiskNum);
    }
    else
    {
        Log("Failed to check ventoy disk %u", VtoyDiskNum);
        return 1;
    }

    Drives = GetLogicalDrives();
    Log("Drives before mount: 0x%x", Drives);

    rc = MountIsoFile(IsoPath, VtoyDiskNum);

    NewDrives = GetLogicalDrives();
    Log("Drives after mount: 0x%x (0x%x)", NewDrives, (NewDrives ^ Drives));

    MntLetter = 'A';
    NewDrives = (NewDrives ^ Drives);
    while (NewDrives)
    {
        if (NewDrives & 0x01)
        {
            if ((NewDrives >> 1) == 0)
            {
                Log("The ISO file is mounted at %C:", MntLetter);
            }
            else
            {
                Log("Maybe the ISO file is mounted at %C:", MntLetter);
            }
            break;
        }

        NewDrives >>= 1;
        MntLetter++;
    }

    Log("Mount ISO FILE: %s", rc == 0 ? "SUCCESS" : "FAILED");

    //Windows 11 bypass check
    if (g_windows_data.windows11_bypass_check == 1)
    {
        Windows11BypassCheck(IsoPath, MntLetter);
    }

    // for protect
    rc = DeleteVentoyPart2MountPoint(VtoyDiskNum);
    Log("Delete ventoy mountpoint: %s", rc == 0 ? "SUCCESS" : "NO NEED");
    
    if (g_windows_data.auto_install_script[0])
    {
        sprintf_s(IsoPath, sizeof(IsoPath), "%C:%s", VtoyLetter, g_windows_data.auto_install_script);
        if (IsFileExist("%s", IsoPath))
        {
            Log("use auto install script %s...", IsoPath);
            ProcessUnattendedInstallation(IsoPath);
        }
        else
        {
            Log("auto install script %s not exist", IsoPath);
        }
    }
    else
    {
        Log("auto install no need");
    }

    if (g_windows_data.injection_archive[0])
    {
        sprintf_s(IsoPath, sizeof(IsoPath), "%C:%s", VtoyLetter, g_windows_data.injection_archive);
        if (IsFileExist("%s", IsoPath))
        {
            Log("decompress injection archive %s...", IsoPath);
            DecompressInjectionArchive(IsoPath, VtoyDiskNum);

            if (IsFileExist("%s", AUTO_RUN_BAT))
            {
                HANDLE hOut;
                DWORD flags = CREATE_NO_WINDOW;
                CHAR StrBuf[1024];
                STARTUPINFOA Si;
                PROCESS_INFORMATION Pi;
                SECURITY_ATTRIBUTES Sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

                Log("%s exist, now run it...", AUTO_RUN_BAT);

                GetStartupInfoA(&Si);

                hOut = CreateFileA(AUTO_RUN_LOG,
                    FILE_APPEND_DATA,
                    FILE_SHARE_WRITE | FILE_SHARE_READ,
                    &Sa,
                    OPEN_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);

                Si.dwFlags |= STARTF_USESTDHANDLES;
                if (hOut != INVALID_HANDLE_VALUE)
                {
                    Si.hStdError = hOut;
                    Si.hStdOutput = hOut;
                }

                sprintf_s(IsoPath, sizeof(IsoPath), "%C:\\%s", Letter, param->vtoy_img_path);
                sprintf_s(StrBuf, sizeof(StrBuf), "cmd.exe /c %s \"%s\" %C", AUTO_RUN_BAT, IsoPath, MntLetter);
                CreateProcessA(NULL, StrBuf, NULL, NULL, TRUE, flags, NULL, NULL, &Si, &Pi);
                WaitForSingleObject(Pi.hProcess, INFINITE);

                SAFE_CLOSE_HANDLE(hOut);
            }
            else
            {
                Log("%s not exist...", AUTO_RUN_BAT);
            }
        }
        else
        {
            Log("injection archive %s not exist", IsoPath);
        }
    }
    else
    {
        Log("no injection archive found");
    }

    return 0;
}


int VentoyJumpWimboot(INT argc, CHAR **argv, CHAR *LunchFile)
{
    int rc = 1;
    char *buf = NULL;
    DWORD size = 0;
    DWORD Pos;

	Log("VentoyJumpWimboot %dbit", g_system_bit);

    sprintf_s(LunchFile, MAX_PATH, "X:\\setup.exe");

    ReadWholeFile2Buf("wimboot.data", &buf, &size);
    Log("wimboot.data size:%d", size);

    memcpy(&g_os_param, buf, sizeof(ventoy_os_param));
    memcpy(&g_windows_data, buf + sizeof(ventoy_os_param), sizeof(ventoy_windows_data));
    memcpy(g_os_param_reserved, g_os_param.vtoy_reserved, sizeof(g_os_param_reserved));

    if (g_os_param_reserved[0] == 1)
    {
        Log("break here for debug .....");
        goto End;
    }

    // convert / to \\   
    for (Pos = 0; Pos < sizeof(g_os_param.vtoy_img_path) && g_os_param.vtoy_img_path[Pos]; Pos++)
    {
        if (g_os_param.vtoy_img_path[Pos] == '/')
        {
            g_os_param.vtoy_img_path[Pos] = '\\';
        }
    }

    if (g_os_param_reserved[0] == 2)
    {
        Log("skip hook for debug .....");
        rc = 0;
        goto End;
    }

    rc = VentoyHook(&g_os_param);

End:

    if (buf)
    {
        free(buf);
    }

    return rc;
}

static int ventoy_check_create_directory(void)
{
    if (IsDirExist("ventoy"))
    {
        Log("ventoy directory already exist");
    }
    else
    {
        Log("ventoy directory not exist, now create it.");
        if (!CreateDirectoryA("ventoy", NULL))
        {
            Log("Failed to create ventoy directory err:%u", GetLastError());
            return 1;
        }
    }

    return 0;
}

int VentoyJump(INT argc, CHAR **argv, CHAR *LunchFile)
{
	int rc = 1;
    int stat = 0;
	DWORD Pos;
	DWORD PeStart;
    DWORD FileSize;
    DWORD LockStatus = 0;
	BYTE *Buffer = NULL; 
	CHAR ExeFileName[MAX_PATH];

	sprintf_s(ExeFileName, sizeof(ExeFileName), "%s", argv[0]);
	if (!IsFileExist("%s", ExeFileName))
	{
		Log("File %s NOT exist, now try %s.exe", ExeFileName, ExeFileName);
		sprintf_s(ExeFileName, sizeof(ExeFileName), "%s.exe", argv[0]);

		Log("File %s exist ? %s", ExeFileName, IsFileExist("%s", ExeFileName) ? "YES" : "NO");
	}

	if (ReadWholeFile2Buf(ExeFileName, (void **)&Buffer, &FileSize))
	{
		goto End;
	}
	
	Log("VentoyJump %dbit", g_system_bit);

    MUTEX_LOCK(g_vtoyins_mutex);
    stat = ventoy_check_create_directory();
    MUTEX_UNLOCK(g_vtoyins_mutex);

    if (stat != 0)
    {
        goto End;
    }

	for (PeStart = 0; PeStart < FileSize; PeStart += 16)
	{
		if (CheckOsParam((ventoy_os_param *)(Buffer + PeStart)) && 
            CheckPeHead(Buffer + PeStart + sizeof(ventoy_os_param) + sizeof(ventoy_windows_data)))
		{
			Log("Find os pararm at %u", PeStart);

            memcpy(&g_os_param, Buffer + PeStart, sizeof(ventoy_os_param));
            memcpy(&g_windows_data, Buffer + PeStart + sizeof(ventoy_os_param), sizeof(ventoy_windows_data));            
            memcpy(g_os_param_reserved, g_os_param.vtoy_reserved, sizeof(g_os_param_reserved));

            if (g_os_param_reserved[0] == 1)
			{
				Log("break here for debug .....");
				goto End;
			}

			// convert / to \\   
            for (Pos = 0; Pos < sizeof(g_os_param.vtoy_img_path) && g_os_param.vtoy_img_path[Pos]; Pos++)
			{
                if (g_os_param.vtoy_img_path[Pos] == '/')
				{
                    g_os_param.vtoy_img_path[Pos] = '\\';
				}
			}

			PeStart += sizeof(ventoy_os_param) + sizeof(ventoy_windows_data);
			sprintf_s(LunchFile, MAX_PATH, "ventoy\\%s", GetFileNameInPath(ExeFileName));

            MUTEX_LOCK(g_vtoyins_mutex);
            if (IsFileExist("%s", LunchFile))
            {
                Log("vtoyjump multiple call ...");
                rc = 0;
                MUTEX_UNLOCK(g_vtoyins_mutex);
                goto End;
            }

			SaveBuffer2File(LunchFile, Buffer + PeStart, FileSize - PeStart);
            MUTEX_UNLOCK(g_vtoyins_mutex);

			break;
		}
	}

	if (PeStart >= FileSize)
	{
		Log("OS param not found");
		goto End;
	}

    if (g_os_param_reserved[0] == 2)
    {
        Log("skip hook for debug .....");
        rc = 0;
        goto End;
    }

    rc = VentoyHook(&g_os_param);

End:

	if (Buffer)
	{
		free(Buffer);
	}

	return rc;
}


int real_main(int argc, char **argv)
{
	int i = 0;
	int rc = 0;
	CHAR NewFile[MAX_PATH];
	CHAR LunchFile[MAX_PATH];
	CHAR CallParam[1024] = { 0 };
	STARTUPINFOA Si;
	PROCESS_INFORMATION Pi;

	Log("#### real_main #### argc = %d", argc);
    Log("program full path: <%s>", g_prog_full_path);
    Log("program dir: <%s>", g_prog_dir);
    Log("program name:: <%s>", g_prog_name);

    Log("argc = %d", argc);
	for (i = 0; i < argc; i++)
	{
		Log("argv[%d]=<%s>", i, argv[i]);
		if (i > 0)
		{
			strcat_s(CallParam, sizeof(CallParam), " ");
			strcat_s(CallParam, sizeof(CallParam), argv[i]);
		}
	}

	GetStartupInfoA(&Si);
	memset(LunchFile, 0, sizeof(LunchFile));

	if (strstr(argv[0], "vtoyjump.exe"))
	{
		rc = VentoyJumpWimboot(argc, argv, LunchFile);
	}
	else
	{
		rc = VentoyJump(argc, argv, LunchFile);
	}

	Log("LunchFile=<%s> CallParam=<%s>", LunchFile, CallParam);

	if (_stricmp(g_prog_name, "winpeshl.exe") != 0 && IsFileExist("ventoy\\%s", g_prog_name))
	{
		sprintf_s(NewFile, sizeof(NewFile), "%s_BACK.EXE", g_prog_full_path);
		MoveFileA(g_prog_full_path, NewFile);
		Log("Move <%s> to <%s>", g_prog_full_path, NewFile);

		sprintf_s(NewFile, sizeof(NewFile), "ventoy\\%s", g_prog_name);
		CopyFileA(NewFile, g_prog_full_path, TRUE);
		Log("Copy <%s> to <%s>", NewFile, g_prog_full_path);

		sprintf_s(LunchFile, sizeof(LunchFile), "%s", g_prog_full_path);
		Log("Final lunchFile is <%s>", LunchFile);
	}
    else
    {
        Log("We don't need to recover original <%s>", g_prog_name);
    }

	if (g_os_param_reserved[0] == 3)
	{
		Log("Open log for debug ...");
		sprintf_s(LunchFile, sizeof(LunchFile), "%s", "notepad.exe ventoy.log");
	}
	else
	{
		if (CallParam[0])
		{
			strcat_s(LunchFile, sizeof(LunchFile), CallParam);
		}
		else if (NULL == strstr(LunchFile, "setup.exe"))
		{
			Log("Not setup.exe, hide windows.");
			Si.dwFlags |= STARTF_USESHOWWINDOW;
			Si.wShowWindow = SW_HIDE;
		}

		Log("Ventoy jump %s ...", rc == 0 ? "success" : "failed");
	}

	Log("Now launch <%s> ...", LunchFile);

	if (g_os_param_reserved[0] == 4)
	{
		Log("Open cmd for debug ...");
		sprintf_s(LunchFile, sizeof(LunchFile), "%s", "cmd.exe");
	}

    Log("Backup log at this point");
    CopyFileA(LOG_FILE, "X:\\Windows\\ventoy.backup", TRUE);

	CreateProcessA(NULL, LunchFile, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);

	for (i = 0; rc && i < 1800; i++)
	{
		Log("Ventoy hook failed, now wait and retry ...");
		Sleep(1000);
		rc = VentoyHook(&g_os_param);
	}

	Log("Wait process...");
	WaitForSingleObject(Pi.hProcess, INFINITE);

	Log("vtoyjump finished");
	return 0;
}

static void VentoyToUpper(CHAR *str)
{
    int i;
    for (i = 0; str[i]; i++)
    {
        str[i] = (CHAR)toupper(str[i]);
    }
}

int main(int argc, char **argv)
{
	int i;
	STARTUPINFOA Si;
	PROCESS_INFORMATION Pi;
	CHAR CurDir[MAX_PATH];
    CHAR NewArgv0[MAX_PATH];
	CHAR CallParam[1024] = { 0 };

	g_vtoylog_mutex = CreateMutexA(NULL, FALSE, "VTOYLOG_LOCK");
	g_vtoyins_mutex = CreateMutexA(NULL, FALSE, "VTOYINS_LOCK");

	Log("######## VentoyJump %dbit ##########", g_system_bit);

	GetCurrentDirectoryA(sizeof(CurDir), CurDir);
	Log("Current directory is <%s>", CurDir);
	
	GetModuleFileNameA(NULL, g_prog_full_path, MAX_PATH);
    split_path_name(g_prog_full_path, g_prog_dir, g_prog_name);

	Log("EXE path: <%s> dir:<%s> name:<%s>", g_prog_full_path, g_prog_dir, g_prog_name);

	if (_stricmp(g_prog_name, "WinLogon.exe") == 0)
	{
		Log("This time is rejump back ...");
		
		strcpy_s(g_prog_full_path, sizeof(g_prog_full_path), argv[1]);
        split_path_name(g_prog_full_path, g_prog_dir, g_prog_name);

		return real_main(argc - 1, argv + 1);
	}
	else if (_stricmp(g_prog_name, "PECMD.exe") == 0)
	{
        strcpy_s(NewArgv0, sizeof(NewArgv0), g_prog_dir);
        VentoyToUpper(NewArgv0);
        
        if (NULL == strstr(NewArgv0, "SYSTEM32") && IsFileExist(ORG_PECMD_BK_PATH))
        {
            Log("Just call original pecmd.exe");
            strcpy_s(CallParam, sizeof(CallParam), ORG_PECMD_PATH);
        }
        else
        {
            Log("We need to rejump for pecmd ...");

            ventoy_check_create_directory();
            CopyFileA(g_prog_full_path, "ventoy\\WinLogon.exe", TRUE);

            sprintf_s(CallParam, sizeof(CallParam), "ventoy\\WinLogon.exe %s", g_prog_full_path);
        }
		
		for (i = 1; i < argc; i++)
		{
			strcat_s(CallParam, sizeof(CallParam), " ");
			strcat_s(CallParam, sizeof(CallParam), argv[i]);
		}

		Log("Now rejump to <%s> ...", CallParam);
		GetStartupInfoA(&Si);
		CreateProcessA(NULL, CallParam, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);

		Log("Wait rejump process...");
		WaitForSingleObject(Pi.hProcess, INFINITE);
		Log("rejump finished");
		return 0;
	}
	else
	{
		Log("We don't need to rejump ...");

        strcpy_s(NewArgv0, sizeof(NewArgv0), g_prog_full_path);
        argv[0] = NewArgv0;

		return real_main(argc, argv);
	}
}


