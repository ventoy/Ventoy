/******************************************************************************
 * VDiskChain.h
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
 
#ifndef __VENTOY_H__
#define __VENTOY_H__

#define VDISK_MAGIC_LEN  32

#define VDISK_BLOCK_DEVICE_PATH_GUID					\
	{ 0x6ed2134e, 0xc2ea, 0x4943, { 0x99, 0x54, 0xa7, 0x76, 0xe5, 0x9c, 0x12, 0xc3 }}

#define VDISK_BLOCK_DEVICE_PATH_NAME  L"vdisk"

#if   defined (MDE_CPU_IA32)
  #define VENTOY_UEFI_DESC   L"IA32 UEFI"
#elif defined (MDE_CPU_X64)
  #define VENTOY_UEFI_DESC   L"X64 UEFI"
#elif defined (MDE_CPU_EBC)
#elif defined (MDE_CPU_ARM)
  #define VENTOY_UEFI_DESC   L"ARM UEFI"
#elif defined (MDE_CPU_AARCH64)
  #define VENTOY_UEFI_DESC   L"ARM64 UEFI"
#else
  #error Unknown Processor Type
#endif

typedef struct vdisk_block_data 
{
	EFI_HANDLE Handle;
	EFI_BLOCK_IO_MEDIA Media;       /* Media descriptor */
	EFI_BLOCK_IO_PROTOCOL BlockIo;	/* Block I/O protocol */

    UINTN DevicePathCompareLen;
	EFI_DEVICE_PATH_PROTOCOL *Path;	/* Device path protocol */

    EFI_HANDLE RawBlockIoHandle;
    EFI_BLOCK_IO_PROTOCOL *pRawBlockIo;
    EFI_DEVICE_PATH_PROTOCOL *pDiskDevPath;

    /* ventoy disk part2 ESP */
    EFI_HANDLE DiskFsHandle;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *pDiskFs;
    EFI_DEVICE_PATH_PROTOCOL *pDiskFsDevPath;

    EFI_HANDLE IsoDriverImage;
}vdisk_block_data;


#define debug(expr, ...) if (gVDiskDebugPrint) VDiskDebug("[VDISK] "expr"\r\n", ##__VA_ARGS__)
#define trace(expr, ...) VDiskDebug("[VDISK] "expr"\r\n", ##__VA_ARGS__)
#define sleep(sec) gBS->Stall(1000000 * (sec))

#define vdisk_debug_pause() \
if (gVDiskDebugPrint) \
{ \
    UINTN __Index = 0; \
    gST->ConOut->OutputString(gST->ConOut, L"[VDISK] ###### Press Enter to continue... ######\r\n");\
    gST->ConIn->Reset(gST->ConIn, FALSE); \
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &__Index);\
}

extern BOOLEAN gVDiskDebugPrint;
VOID EFIAPI VDiskDebug(IN CONST CHAR8  *Format, ...);
EFI_STATUS EFIAPI vdisk_block_io_read 
(
    IN EFI_BLOCK_IO_PROTOCOL          *This,
    IN UINT32                          MediaId,
    IN EFI_LBA                         Lba,
    IN UINTN                           BufferSize,
    OUT VOID                          *Buffer
);

extern UINT8 *g_disk_buf_addr;
extern UINT64 g_disk_buf_size;
extern vdisk_block_data gVDiskBlockData;
EFI_STATUS EFIAPI vdisk_install_blockio(IN EFI_HANDLE ImageHandle, IN UINT64 ImgSize);
int vdisk_get_vdisk_raw(UINT8 **buf, UINT32 *size);

#endif

