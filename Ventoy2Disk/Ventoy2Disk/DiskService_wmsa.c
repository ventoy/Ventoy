/******************************************************************************
* DiskService_wsma.c
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
#include <VersionHelpers.h>
#include "Ventoy2Disk.h"
#include "DiskService.h"

STATIC BOOL IsPowershellExist(void)
{
	BOOL ret;

	if (!IsWindows8OrGreater())
	{
		Log("This is before Windows8 powershell disk not supported.");
		return FALSE;
	}

	ret = IsFileExist("C:\\Windows\\system32\\WindowsPowerShell\\v1.0\\powershell.exe");
	if (!ret)
	{
		Log("powershell.exe not exist");
	}

	return ret;
}

int PSHELL_GetPartitionNumber(int PhyDrive, UINT64 Offset)
{
	int partnum = -1;
	DWORD i = 0;
	DWORD BufLen = 0;
	DWORD dwBytes = 0;
	BOOL bRet;
	HANDLE hDrive;
	LONGLONG PartStart;
	DRIVE_LAYOUT_INFORMATION_EX *pDriveLayout = NULL;

	Log("PSHELL_GetPartitionNumber PhyDrive:%d Offset:%llu", PhyDrive, Offset);

	hDrive = GetPhysicalHandle(PhyDrive, FALSE, FALSE, FALSE);
	if (hDrive == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	BufLen = (DWORD)(sizeof(PARTITION_INFORMATION_EX)* 256);

	pDriveLayout = malloc(BufLen);
	if (!pDriveLayout)
	{
		goto out;
	}
	memset(pDriveLayout, 0, BufLen);

	bRet = DeviceIoControl(hDrive,
		IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL,
		0,
		pDriveLayout,
		BufLen,
		&dwBytes,
		NULL);
	if (!bRet)
	{
		Log("Failed to ioctrl get drive layout ex %u", LASTERR);
		goto out;
	}

	Log("PhyDrive:%d  PartitionStyle=%s  PartitionCount=%u", PhyDrive,
		(pDriveLayout->PartitionStyle == PARTITION_STYLE_MBR) ? "MBR" : "GPT", pDriveLayout->PartitionCount);

	for (i = 0; i < pDriveLayout->PartitionCount; i++)
	{
		PartStart = pDriveLayout->PartitionEntry[i].StartingOffset.QuadPart;
		if (PartStart == (LONGLONG)Offset)
		{
			Log("[*] [%d] PartitionNumber=%u Offset=%lld Length=%lld ",
				i,
				pDriveLayout->PartitionEntry[i].PartitionNumber,
				pDriveLayout->PartitionEntry[i].StartingOffset.QuadPart,
				pDriveLayout->PartitionEntry[i].PartitionLength.QuadPart
				);
			partnum = (int)pDriveLayout->PartitionEntry[i].PartitionNumber;
		}
		else
		{
			Log("[ ] [%d] PartitionNumber=%u Offset=%lld Length=%lld ",
				i,
				pDriveLayout->PartitionEntry[i].PartitionNumber,
				pDriveLayout->PartitionEntry[i].StartingOffset.QuadPart,
				pDriveLayout->PartitionEntry[i].PartitionLength.QuadPart
				);
		}
	}

out:

	CHECK_CLOSE_HANDLE(hDrive);
	CHECK_FREE(pDriveLayout);

	return partnum;
}


STATIC BOOL PSHELL_CommProc(const char *Cmd)
{
	CHAR CmdBuf[4096];
	STARTUPINFOA Si;
	PROCESS_INFORMATION Pi;

	if (!IsPowershellExist())
	{
		return FALSE;
	}

	GetStartupInfoA(&Si);
	Si.dwFlags |= STARTF_USESHOWWINDOW;
	Si.wShowWindow = SW_HIDE;

	sprintf_s(CmdBuf, sizeof(CmdBuf), "C:\\Windows\\system32\\WindowsPowerShell\\v1.0\\powershell.exe -Command \"&{ %s }\"", Cmd);

	Log("CreateProcess <%s>", CmdBuf);
	CreateProcessA(NULL, CmdBuf, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);

	Log("Wair process ...");
	WaitForSingleObject(Pi.hProcess, INFINITE);
	Log("Process finished...");

	CHECK_CLOSE_HANDLE(Pi.hProcess);
	CHECK_CLOSE_HANDLE(Pi.hThread);

	return TRUE;
}


BOOL PSHELL_CleanDisk(int DriveIndex)
{
	BOOL ret;
	CHAR CmdBuf[512];

	sprintf_s(CmdBuf, sizeof(CmdBuf), "Clear-Disk -Number %d -RemoveData -RemoveOEM -Confirm:$false", DriveIndex);
	ret = PSHELL_CommProc(CmdBuf);
	Log("CleanDiskByPowershell<%d> ret:%d (%s)", DriveIndex, ret, ret ? "SUCCESS" : "FAIL");

	return ret;
}


BOOL PSHELL_DeleteVtoyEFIPartition(int DriveIndex, UINT64 EfiPartOffset)
{
	int Part;
	BOOL ret;
	CHAR CmdBuf[512];

	Part = PSHELL_GetPartitionNumber(DriveIndex, EfiPartOffset);
	if (Part < 0)
	{
		ret = FALSE;
	}
	else
	{
		sprintf_s(CmdBuf, sizeof(CmdBuf), "Remove-Partition -DiskNumber %d -PartitionNumber %d -Confirm:$false", DriveIndex, Part);
		ret = PSHELL_CommProc(CmdBuf);
	}
	
	Log("PSHELL_DeleteVtoyEFIPartition<%d> ret:%d (%s)", DriveIndex, ret, ret ? "SUCCESS" : "FAIL");
	return ret;
}


BOOL PSHELL_ChangeVtoyEFI2ESP(int DriveIndex, UINT64 Offset)
{
	int Part;
	BOOL ret;
	CHAR CmdBuf[512];

	Part = PSHELL_GetPartitionNumber(DriveIndex, Offset);
	if (Part < 0)
	{
		ret = FALSE;
	}
	else
	{
		sprintf_s(CmdBuf, sizeof(CmdBuf), "Set-Partition -DiskNumber %d -PartitionNumber %d -gpttype '{C12A7328-F81F-11D2-BA4B-00A0C93EC93B}' -Confirm:$false", DriveIndex, Part);
		ret = PSHELL_CommProc(CmdBuf);
	}

	Log("PSHELL_ChangeVtoyEFI2ESP<%d> ret:%d (%s)", DriveIndex, ret, ret ? "SUCCESS" : "FAIL");
	return ret;
}


BOOL PSHELL_ChangeVtoyEFI2Basic(int DriveIndex, UINT64 Offset)
{
	int Part;
	BOOL ret;
	CHAR CmdBuf[512];

	Part = PSHELL_GetPartitionNumber(DriveIndex, Offset);
	if (Part < 0)
	{
		ret = FALSE;
	}
	else
	{
		sprintf_s(CmdBuf, sizeof(CmdBuf), "Set-Partition -DiskNumber %d -PartitionNumber %d -gpttype '{ebd0a0a2-b9e5-4433-87c0-68b6b72699c7}' -Confirm:$false", DriveIndex, Part);
		ret = PSHELL_CommProc(CmdBuf);
	}
	
	Log("PSHELL_ChangeVtoyEFI2Basic<%d> ret:%d (%s)", DriveIndex, ret, ret ? "SUCCESS" : "FAIL");
	return ret;
}

BOOL PSHELL_ShrinkVolume(int DriveIndex, const char* VolumeGuid, CHAR DriveLetter, UINT64 OldBytes, UINT64 ReduceBytes)
{
	int Part;
	BOOL ret;
	CHAR CmdBuf[512];

	(void)VolumeGuid;

	Part = PSHELL_GetPartitionNumber(DriveIndex, SIZE_1MB);
	if (Part < 0)
	{
		ret = FALSE;
	}
	else
	{
		sprintf_s(CmdBuf, sizeof(CmdBuf), "Resize-Partition -DiskNumber %d -PartitionNumber %d -Size %llu -Confirm:$false", 
			DriveIndex, Part, OldBytes - ReduceBytes);
		ret = PSHELL_CommProc(CmdBuf);
	}

	Log("PSHELL_ShrinkVolume<%d> %C: ret:%d (%s)", DriveIndex, DriveLetter, ret, ret ? "SUCCESS" : "FAIL");
	return ret;
}
