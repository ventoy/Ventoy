/******************************************************************************
* DiskService.c
*
* Copyright (c) 2021, longpanda <admin@ventoy.net>
* Copyright (c) 2011-2021 Pete Batard <pete@akeo.ie>
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
#include <winternl.h>
#include <commctrl.h>
#include <initguid.h>
#include <vds.h>
#include "Ventoy2Disk.h"
#include "DiskService.h"

static CHAR g_WindowsDir[MAX_PATH] = {0};

const CHAR* DISK_GetWindowsDir(void)
{
	if (g_WindowsDir[0] == 0)
	{
		GetEnvironmentVariableA("SystemRoot", g_WindowsDir, MAX_PATH);
		if (g_WindowsDir[0] == 0)
		{
			sprintf_s(g_WindowsDir, MAX_PATH, "C:\\Windows");
		}
	}

	return g_WindowsDir;
}

BOOL DISK_CleanDisk(int DriveIndex)
{
	BOOL ret;

	ret = VDS_CleanDisk(DriveIndex);
	if (!ret)
	{
		ret = PSHELL_CleanDisk(DriveIndex);
	}

	return ret;
}


BOOL DISK_DeleteVtoyEFIPartition(int DriveIndex, UINT64 EfiPartOffset)
{
	BOOL ret;

	ret = VDS_DeleteVtoyEFIPartition(DriveIndex, EfiPartOffset);
	if (!ret)
	{
		ret = PSHELL_DeleteVtoyEFIPartition(DriveIndex, EfiPartOffset);
	}

	return ret;
}

BOOL DISK_ChangeVtoyEFI2ESP(int DriveIndex, UINT64 Offset)
{
	BOOL ret;

	ret = VDS_ChangeVtoyEFI2ESP(DriveIndex, Offset);
	if (!ret)
	{
		ret = PSHELL_ChangeVtoyEFI2ESP(DriveIndex, Offset);
	}

	return ret;
}


BOOL DISK_ChangeVtoyEFI2Basic(int DriveIndex, UINT64 Offset)
{
	BOOL ret;

	ret = VDS_ChangeVtoyEFI2Basic(DriveIndex, Offset);
	if (!ret)
	{
		ret = PSHELL_ChangeVtoyEFI2Basic(DriveIndex, Offset);
	}

	return ret;
}

BOOL DISK_ChangeVtoyEFIAttr(int DriveIndex, UINT64 Offset, UINT64 Attr)
{
	BOOL ret;

	ret = VDS_ChangeVtoyEFIAttr(DriveIndex, Offset, Attr);

	return ret;
}

BOOL DISK_ShrinkVolume(int DriveIndex, const char* VolumeGuid, CHAR DriveLetter, UINT64 OldBytes, UINT64 ReduceBytes)
{
	BOOL ret;

	ret = VDS_ShrinkVolume(DriveIndex, VolumeGuid, DriveLetter, OldBytes, ReduceBytes);
	if (!ret)
	{
		if (LASTERR == VDS_E_SHRINK_DIRTY_VOLUME)
		{
			Log("VDS shrink return dirty, no need to run powershell.");
		}
		else
		{
			ret = PSHELL_ShrinkVolume(DriveIndex, VolumeGuid, DriveLetter, OldBytes, ReduceBytes);
		}
	}

	return ret;
}



// Output command
typedef struct
{
	DWORD Lines;
	PCHAR Output;
} TEXTOUTPUT, * PTEXTOUTPUT;

/* Callback command types (some errorcode were filled from HPUSBFW V2.2.3 and their
   designation from docs.microsoft.com/windows/win32/api/vds/nf-vds-ivdsvolumemf2-formatex */
typedef enum {
	FCC_PROGRESS,
	FCC_DONE_WITH_STRUCTURE,
	FCC_UNKNOWN2,
	FCC_INCOMPATIBLE_FILE_SYSTEM,
	FCC_UNKNOWN4,
	FCC_UNKNOWN5,
	FCC_ACCESS_DENIED,
	FCC_MEDIA_WRITE_PROTECTED,
	FCC_VOLUME_IN_USE,
	FCC_CANT_QUICK_FORMAT,
	FCC_UNKNOWNA,
	FCC_DONE,
	FCC_BAD_LABEL,
	FCC_UNKNOWND,
	FCC_OUTPUT,
	FCC_STRUCTURE_PROGRESS,
	FCC_CLUSTER_SIZE_TOO_SMALL,
	FCC_CLUSTER_SIZE_TOO_BIG,
	FCC_VOLUME_TOO_SMALL,
	FCC_VOLUME_TOO_BIG,
	FCC_NO_MEDIA_IN_DRIVE,
	FCC_UNKNOWN15,
	FCC_UNKNOWN16,
	FCC_UNKNOWN17,
	FCC_DEVICE_NOT_READY,
	FCC_CHECKDISK_PROGRESS,
	FCC_UNKNOWN1A,
	FCC_UNKNOWN1B,
	FCC_UNKNOWN1C,
	FCC_UNKNOWN1D,
	FCC_UNKNOWN1E,
	FCC_UNKNOWN1F,
	FCC_READ_ONLY_MODE,
	FCC_UNKNOWN21,
	FCC_UNKNOWN22,
	FCC_UNKNOWN23,
	FCC_UNKNOWN24,
	FCC_ALIGNMENT_VIOLATION,
} FILE_SYSTEM_CALLBACK_COMMAND;

// FMIFS callback definition
typedef BOOLEAN(__stdcall* PFMIFSCALLBACK)(FILE_SYSTEM_CALLBACK_COMMAND Command, DWORD SubAction, PVOID ActionInfo);


// Chkdsk command in FMIFS
typedef VOID(__stdcall* PCHKDSK)(PWCHAR DriveRoot,
	PWCHAR Format,
	BOOL CorrectErrors,
	BOOL Verbose,
	BOOL CheckOnlyIfDirty,
	BOOL ScanDrive,
	PVOID Unused2,
	PVOID Unused3,
	PFMIFSCALLBACK Callback);


// media flags
#define FMIFS_HARDDISK 0xC
#define FMIFS_FLOPPY   0x8
// Format command in FMIFS
typedef VOID(__stdcall* PFORMATEX)(PWCHAR DriveRoot,
	DWORD MediaFlag,
	PWCHAR Format,
	PWCHAR Label,
	BOOL QuickFormat,
	DWORD ClusterSize,
	PFMIFSCALLBACK Callback);

#define FP_FORCE                            0x00000001
#define FP_QUICK                            0x00000002
#define FP_COMPRESSION                      0x00000004
#define FP_DUPLICATE_METADATA               0x00000008
#define FP_LARGE_FAT32                      0x00010000
#define FP_NO_BOOT                          0x00020000
#define FP_CREATE_PERSISTENCE_CONF          0x00040000

// FormatExCallback
static int g_dll_format_error = 0;
BOOLEAN __stdcall FormatExCallback(FILE_SYSTEM_CALLBACK_COMMAND Command, DWORD Modifier, PVOID Argument)
{
	PDWORD percent;
	PBOOLEAN status;

	switch (Command) {
	case FCC_PROGRESS:
		percent = (PDWORD)Argument;
		Log("Format percent: %u%%", *percent);
		break;
	case FCC_STRUCTURE_PROGRESS:	// No progress on quick format
		Log("Creating file system...");
		break;
	case FCC_DONE:
		status = (PBOOLEAN)Argument;
		if (*status == FALSE)
		{
			Log("Format error: %u ERROR_NOT_SUPPORTED=%u", LASTERR, ERROR_NOT_SUPPORTED);
			g_dll_format_error = 1;
		}
		else
		{
			Log("Format Done");
		}
		break;
	case FCC_DONE_WITH_STRUCTURE:
		Log("Format FCC_DONE_WITH_STRUCTURE");
		break;
	case FCC_INCOMPATIBLE_FILE_SYSTEM:
		Log("Incompatible File System");
		break;
	case FCC_ACCESS_DENIED:
		Log("Access denied");
		break;
	case FCC_MEDIA_WRITE_PROTECTED:
		Log("Media is write protected");
		break;
	case FCC_VOLUME_IN_USE:
		Log("Volume is in use");
		break;
	case FCC_DEVICE_NOT_READY:
		Log("The device is not ready");
		break;
	case FCC_CANT_QUICK_FORMAT:
		Log("Cannot quick format this volume");
		break;
	case FCC_BAD_LABEL:
		Log("Bad label");
		break;
	case FCC_OUTPUT:
		Log("%s", ((PTEXTOUTPUT)Argument)->Output);
		break;
	case FCC_CLUSTER_SIZE_TOO_BIG:
	case FCC_CLUSTER_SIZE_TOO_SMALL:
		Log("Unsupported cluster size");
		break;
	case FCC_VOLUME_TOO_BIG:
	case FCC_VOLUME_TOO_SMALL:
		Log("Volume is too %s", (Command == FCC_VOLUME_TOO_BIG) ? "big" : "small");
		break;
	case FCC_NO_MEDIA_IN_DRIVE:
		Log("No media in drive");
		break;
	case FCC_ALIGNMENT_VIOLATION:
		Log("Partition start offset is not aligned to the cluster size");
		break;
	default:
		Log("FormatExCallback: Received unhandled command 0x%02X - aborting", Command);
		break;
	}

	return TRUE;
}


BOOL DLL_FormatVolume(char DriveLetter, int fs, DWORD ClusterSize)
{
	PWCHAR  Label = L"Ventoy";
	PWCHAR  Format = NULL;
	WCHAR   RootDirectory[MAX_PATH] = { 0 };
	HMODULE ifsModule;
	PFORMATEX FormatEx;

	ifsModule = LoadLibraryA("fmifs.dll");
	if (NULL == ifsModule)
	{
		Log("LoadLibrary fmifs.dll failed %u", LASTERR);
		return FALSE;
	}

	Log("Find ifsModule");

	FormatEx = (PFORMATEX)GetProcAddress(ifsModule, "FormatEx");
	if (FormatEx == NULL)
	{
		Log("Failed to get FormatEx handler\n");
		return FALSE;
	}
	Log("Find FormatEx=%p", FormatEx);

	RootDirectory[0] = DriveLetter;
	RootDirectory[1] = L':';
	RootDirectory[2] = L'\\';
	RootDirectory[3] = (WCHAR)0;

	DWORD media;
	DWORD driveType;
	driveType = GetDriveTypeW(RootDirectory);
	if (driveType != DRIVE_FIXED)
		media = FMIFS_FLOPPY;
	if (driveType == DRIVE_FIXED)
		media = FMIFS_HARDDISK;

	Format = GetVentoyFsFmtNameByTypeW(fs);

	g_dll_format_error = 0;

	Log("Call FormatEx Function for %C: %s ClusterSize=%u(%uKB)", DriveLetter, GetVentoyFsFmtNameByTypeA(fs), ClusterSize, ClusterSize / 1024);
	FormatEx(RootDirectory, media, Format, Label, FP_FORCE | FP_QUICK, ClusterSize, FormatExCallback);
	FreeLibrary(ifsModule);

	if (g_dll_format_error)
	{
		Log("Format failed by DLL");
		return FALSE;
	}

	Log("Format success by DLL");
	return TRUE;
}


BOOL DISK_FormatVolume(char DriveLetter, int fs, UINT64 VolumeSize)
{
	int i;
	DWORD ClusterSize = 0;
	BOOL ret = FALSE;
	FmtFunc astFmtFunc[] =
	{
		FMT_DEF(VDS_FormatVolume),
		FMT_DEF(DLL_FormatVolume),
		FMT_DEF(PSHELL_FormatVolume),
		FMT_DEF(DSPT_FormatVolume),
		FMT_DEF(CMD_FormatVolume),
		{ NULL, NULL }
	};

	ClusterSize = (DWORD)GetClusterSize();
	Log("DISK_FormatVolume %C:\\ %s VolumeSize=%llu ClusterSize=%u(%uKB)",
		DriveLetter, GetVentoyFsNameByType(fs), (ULONGLONG)VolumeSize, ClusterSize, ClusterSize / 1024);

	for (i = 0; astFmtFunc[i].formatFunc; i++)
	{
		Log("%s ...", astFmtFunc[i].name);
		ret = astFmtFunc[i].formatFunc(DriveLetter, fs, ClusterSize);
		if (ret)
		{
			break;
		}
	}

	return ret;
}
