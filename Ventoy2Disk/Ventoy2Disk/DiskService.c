/******************************************************************************
* DiskService.c
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

#include <Windows.h>
#include <winternl.h>
#include <commctrl.h>
#include <initguid.h>
#include <vds.h>
#include "Ventoy2Disk.h"
#include "DiskService.h"

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

// Callback command types
typedef enum
{
	PROGRESS,
	DONEWITHSTRUCTURE,
	UNKNOWN2,
	UNKNOWN3,
	UNKNOWN4,
	UNKNOWN5,
	INSUFFICIENTRIGHTS,
	UNKNOWN7,
	UNKNOWN8,
	UNKNOWN9,
	UNKNOWNA,
	DONE,
	UNKNOWNC,
	UNKNOWND,
	OUTPUT,
	STRUCTUREPROGRESS
} CALLBACKCOMMAND;

// FMIFS callback definition
typedef BOOLEAN(__stdcall* PFMIFSCALLBACK)(CALLBACKCOMMAND Command, DWORD SubAction, PVOID ActionInfo);


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


// FormatExCallback
static int g_dll_format_error = 0;
BOOLEAN __stdcall FormatExCallback(CALLBACKCOMMAND Command, DWORD Modifier, PVOID Argument)
{
	PDWORD percent;
	PBOOLEAN status;

	switch (Command)
	{
	case PROGRESS:
		percent = (PDWORD)Argument;
		Log("Format percent: %d \n", *percent);
		break;

	case OUTPUT:
		break;

	case DONE:
		status = (PBOOLEAN)Argument;
		if (*status == FALSE)
		{
			g_dll_format_error = 1;
		}
		else
		{
		}

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
	FormatEx(RootDirectory, media, Format, Label, TRUE, ClusterSize, FormatExCallback);
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
	DWORD ClusterSize = 0;
	BOOL ret = FALSE;

	ClusterSize = (DWORD)GetClusterSize();
	Log("DISK_FormatVolume %C:\\ %s VolumeSize=%llu ClusterSize=%u(%uKB)",
		DriveLetter, GetVentoyFsNameByType(fs), (ULONGLONG)VolumeSize, ClusterSize, ClusterSize/1024);

	ret = DLL_FormatVolume(DriveLetter, fs, ClusterSize);
	if (!ret)
	{
		ret = VDS_FormatVolume(DriveLetter, fs, ClusterSize);
		if (!ret)
		{
			ret = DSPT_FormatVolume(DriveLetter, fs, ClusterSize);
			if (!ret)
			{
				ret = PSHELL_FormatVolume(DriveLetter, fs, ClusterSize);
			}
		}
	}

	return ret;
}
