/******************************************************************************
* vtoyjump.h
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
#ifndef __VTOYJUMP_H__
#define __VTOYJUMP_H__

#pragma comment( linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" ) 

#define SIZE_1MB   (1024 * 1024)
#define VENTOY_EFI_PART_SIZE   (32 * SIZE_1MB)

#define VENTOY_GUID { 0x77772020, 0x2e77, 0x6576, { 0x6e, 0x74, 0x6f, 0x79, 0x2e, 0x6e, 0x65, 0x74 }}

#pragma pack(1)

typedef struct ventoy_guid
{
	UINT32   data1;
	UINT16   data2;
	UINT16   data3;
	UINT8    data4[8];
}ventoy_guid;


typedef struct ventoy_os_param
{
	ventoy_guid  guid;             // VENTOY_GUID
	UINT8        chksum;           // checksum

	UINT8   vtoy_disk_guid[16];
	UINT64  vtoy_disk_size;       // disk size in bytes
	UINT16  vtoy_disk_part_id;    // begin with 1
	UINT16  vtoy_disk_part_type;  // 0:exfat   1:ntfs  other: reserved
	char    vtoy_img_path[384];   // It seems to be enough, utf-8 format
	UINT64  vtoy_img_size;        // image file size in bytes

	/*
	* Ventoy will write a copy of ventoy_image_location data into runtime memory
	* this is the physically address and length of that memory.
	* Address 0 means no such data exist.
	* Address will be aligned by 4KB.
	*
	*/
	UINT64  vtoy_img_location_addr;
	UINT32  vtoy_img_location_len;

	UINT64  vtoy_reserved[4];     // Internal use by ventoy

	UINT8   reserved[31];
}ventoy_os_param;

typedef struct ventoy_windows_data
{
    char auto_install_script[384];
    UINT8 reserved[128];
}ventoy_windows_data;

#pragma pack()


#define SAFE_CLOSE_HANDLE(handle) \
{\
	if (handle != INVALID_HANDLE_VALUE) \
	{\
		CloseHandle(handle); \
		(handle) = INVALID_HANDLE_VALUE; \
	}\
}


#endif
