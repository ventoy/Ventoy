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
}VDS_PARA;


//VDS com
int  VDS_Init(void);
BOOL VDS_CleanDisk(int DriveIndex);
BOOL VDS_DeleteAllPartitions(int DriveIndex);
BOOL VDS_DeleteVtoyEFIPartition(int DriveIndex);
BOOL VDS_ChangeVtoyEFIAttr(int DriveIndex, UINT64 Attr);
BOOL VDS_CreateVtoyEFIPart(int DriveIndex, UINT64 Offset);
BOOL VDS_ChangeVtoyEFI2ESP(int DriveIndex, UINT64 Offset);
BOOL VDS_ChangeVtoyEFI2Basic(int DriveIndex, UINT64 Offset);
BOOL VDS_FormatVtoyEFIPart(int DriveIndex, UINT64 Offset);

//diskpart.exe
BOOL DSPT_CleanDisk(int DriveIndex);


//
// Internel define
//




#endif
