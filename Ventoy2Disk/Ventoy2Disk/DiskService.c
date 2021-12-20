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

