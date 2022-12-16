/******************************************************************************
 * DiskService.h
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

#ifndef __DISKSERVICE_H__
#define __DISKSERVICE_H__

typedef struct VDS_PARA
{
    UINT64 Attr;
    GUID Type;
    GUID Id;
    WCHAR Name[36];
	ULONG NameLen;
    ULONGLONG Offset;
	CHAR DriveLetter;
    DWORD ClusterSize;
}VDS_PARA;

//DISK API
BOOL DISK_CleanDisk(int DriveIndex);
BOOL DISK_DeleteVtoyEFIPartition(int DriveIndex, UINT64 EfiPartOffset);
BOOL DISK_ChangeVtoyEFIAttr(int DriveIndex, UINT64 Offset, UINT64 Attr);
BOOL DISK_ChangeVtoyEFI2ESP(int DriveIndex, UINT64 Offset);
BOOL DISK_ChangeVtoyEFI2Basic(int DriveIndex, UINT64 Offset);
BOOL DISK_ShrinkVolume(int DriveIndex, const char* VolumeGuid, CHAR DriveLetter, UINT64 OldBytes, UINT64 ReduceBytes);
BOOL DISK_FormatVolume(char DriveLetter, int fs, UINT64 VolumeSize);


//VDS com
BOOL VDS_CleanDisk(int DriveIndex);
BOOL VDS_DeleteAllPartitions(int DriveIndex);
BOOL VDS_DeleteVtoyEFIPartition(int DriveIndex, UINT64 EfiPartOffset);
BOOL VDS_ChangeVtoyEFIAttr(int DriveIndex, UINT64 Offset, UINT64 Attr);
BOOL VDS_ChangeVtoyEFI2ESP(int DriveIndex, UINT64 Offset);
BOOL VDS_ChangeVtoyEFI2Basic(int DriveIndex, UINT64 Offset);
BOOL VDS_ShrinkVolume(int DriveIndex, const char* VolumeGuid, CHAR DriveLetter, UINT64 OldBytes, UINT64 ReduceBytes);
BOOL VDS_IsLastAvaliable(void);
BOOL VDS_FormatVolume(char DriveLetter, int fs, DWORD ClusterSize);

//diskpart.exe
BOOL DSPT_CleanDisk(int DriveIndex);
BOOL DSPT_FormatVolume(char DriveLetter, int fs, DWORD ClusterSize);

BOOL CMD_FormatVolume(char DriveLetter, int fs, DWORD ClusterSize);

//powershell.exe
BOOL PSHELL_CleanDisk(int DriveIndex);
BOOL PSHELL_DeleteVtoyEFIPartition(int DriveIndex, UINT64 EfiPartOffset);
BOOL PSHELL_ChangeVtoyEFI2ESP(int DriveIndex, UINT64 Offset);
BOOL PSHELL_ChangeVtoyEFI2Basic(int DriveIndex, UINT64 Offset);
BOOL PSHELL_ShrinkVolume(int DriveIndex, const char* VolumeGuid, CHAR DriveLetter, UINT64 OldBytes, UINT64 ReduceBytes);
BOOL PSHELL_FormatVolume(char DriveLetter, int fs, DWORD ClusterSize);

const CHAR* DISK_GetWindowsDir(void);

//
// Internel define
//


typedef BOOL(*FormatVolume_PF)(char DriveLetter, int fs, DWORD ClusterSize);

typedef struct FmtFunc
{
    const char* name;
    FormatVolume_PF formatFunc;
}FmtFunc;

#define FMT_DEF(func) { #func, func }


#endif
