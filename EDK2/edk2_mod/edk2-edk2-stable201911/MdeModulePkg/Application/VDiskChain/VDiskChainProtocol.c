/******************************************************************************
 * VDiskChainProtocol.c
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

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Protocol/LoadedImage.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/RamDisk.h>
#include <Protocol/SimpleFileSystem.h>
#include <VDiskChain.h>

/* EFI block device vendor device path GUID */
EFI_GUID gVDiskBlockDevicePathGuid = VDISK_BLOCK_DEVICE_PATH_GUID;

EFI_STATUS EFIAPI vdisk_block_io_reset 
(
    IN EFI_BLOCK_IO_PROTOCOL          *This,
    IN BOOLEAN                        ExtendedVerification
) 
{
    (VOID)This;
    (VOID)ExtendedVerification;
	return EFI_SUCCESS;
}

EFI_STATUS EFIAPI vdisk_block_io_flush(IN EFI_BLOCK_IO_PROTOCOL *This)
{
	(VOID)This;
	return EFI_SUCCESS;
}

EFI_STATUS EFIAPI vdisk_block_io_read
(
    IN EFI_BLOCK_IO_PROTOCOL          *This,
    IN UINT32                          MediaId,
    IN EFI_LBA                         Lba,
    IN UINTN                           BufferSize,
    OUT VOID                          *Buffer
)
{
    (VOID)This;
    (VOID)MediaId;

    debug("vdisk_block_io_read %lu %lu\n", Lba, BufferSize / 512);
    CopyMem(Buffer, g_disk_buf_addr + (Lba * 512), BufferSize);

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI vdisk_block_io_write
(
    IN EFI_BLOCK_IO_PROTOCOL          *This,
    IN UINT32                          MediaId,
    IN EFI_LBA                         Lba,
    IN UINTN                           BufferSize,
    IN VOID                           *Buffer
)
{
    (VOID)This;
    (VOID)MediaId;
    (VOID)Buffer;

    debug("vdisk_block_io_read %lu %lu\n", Lba, BufferSize / 512);
    return EFI_WRITE_PROTECTED;
}

EFI_STATUS EFIAPI vdisk_fill_device_path(VOID)
{
    UINTN NameLen = 0;
    UINT8 TmpBuf[128] = {0};
    VENDOR_DEVICE_PATH *venPath = NULL;

    venPath = (VENDOR_DEVICE_PATH *)TmpBuf;
    NameLen = StrSize(VDISK_BLOCK_DEVICE_PATH_NAME);
    venPath->Header.Type = HARDWARE_DEVICE_PATH;
    venPath->Header.SubType = HW_VENDOR_DP;
    venPath->Header.Length[0] = sizeof(VENDOR_DEVICE_PATH) + NameLen;
    venPath->Header.Length[1] = 0;
    CopyMem(&venPath->Guid, &gVDiskBlockDevicePathGuid, sizeof(EFI_GUID));
    CopyMem(venPath + 1, VDISK_BLOCK_DEVICE_PATH_NAME, NameLen);
    
    gVDiskBlockData.Path = AppendDevicePathNode(NULL, (EFI_DEVICE_PATH_PROTOCOL *)TmpBuf);
    gVDiskBlockData.DevicePathCompareLen = sizeof(VENDOR_DEVICE_PATH) + NameLen;

    debug("gVDiskBlockData.Path=<%s>\n", ConvertDevicePathToText(gVDiskBlockData.Path, FALSE, FALSE));

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI vdisk_connect_driver(IN EFI_HANDLE ControllerHandle, IN CONST CHAR16 *DrvName)
{
    UINTN i = 0;
    UINTN Count = 0;
    CHAR16 *DriverName = NULL;
    EFI_HANDLE *Handles = NULL;
    EFI_HANDLE DrvHandles[2] = { NULL };
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_COMPONENT_NAME_PROTOCOL *NameProtocol = NULL;
    EFI_COMPONENT_NAME2_PROTOCOL *Name2Protocol = NULL;

    debug("vdisk_connect_driver <%s>...", DrvName);

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiComponentName2ProtocolGuid, 
                                     NULL, &Count, &Handles);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    for (i = 0; i < Count; i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiComponentName2ProtocolGuid, (VOID **)&Name2Protocol);
        if (EFI_ERROR(Status))
        {
            continue;
        }

        Status = Name2Protocol->GetDriverName(Name2Protocol, "en", &DriverName);
        if (EFI_ERROR(Status) || NULL == DriverName)
        {
            continue;
        }

        if (StrStr(DriverName, DrvName))
        {
            debug("Find driver name2:<%s>: <%s>", DriverName, DrvName);
            DrvHandles[0] = Handles[i];
            break;
        }
    }

    if (i < Count)
    {
        Status = gBS->ConnectController(ControllerHandle, DrvHandles, NULL, TRUE);
        debug("vdisk_connect_driver:<%s> <%r>", DrvName, Status);
        goto end;
    }

    debug("%s NOT found, now try COMPONENT_NAME", DrvName);

    Count = 0;
    FreePool(Handles);
    Handles = NULL;

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiComponentNameProtocolGuid, 
                                     NULL, &Count, &Handles);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    for (i = 0; i < Count; i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiComponentNameProtocolGuid, (VOID **)&NameProtocol);
        if (EFI_ERROR(Status))
        {
            continue;
        }

        Status = NameProtocol->GetDriverName(NameProtocol, "en", &DriverName);
        if (EFI_ERROR(Status))
        {
            continue;
        }

        if (StrStr(DriverName, DrvName))
        {
            debug("Find driver name:<%s>: <%s>", DriverName, DrvName);
            DrvHandles[0] = Handles[i];
            break;
        }
    }

    if (i < Count)
    {
        Status = gBS->ConnectController(ControllerHandle, DrvHandles, NULL, TRUE);
        debug("vdisk_connect_driver:<%s> <%r>", DrvName, Status);
        goto end;
    }

    Status = EFI_NOT_FOUND;
    
end:
    FreePool(Handles);
    
    return Status;
}

EFI_STATUS EFIAPI vdisk_install_blockio(IN EFI_HANDLE ImageHandle, IN UINT64 ImgSize)
{   
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_BLOCK_IO_PROTOCOL *pBlockIo = &(gVDiskBlockData.BlockIo);
    
    vdisk_fill_device_path();

    debug("install block io protocol %p", ImageHandle);
    vdisk_debug_pause();

    gVDiskBlockData.Media.BlockSize = 512;
    gVDiskBlockData.Media.LastBlock = ImgSize / 512 - 1;
    gVDiskBlockData.Media.ReadOnly = TRUE;
    gVDiskBlockData.Media.MediaPresent = 1;
    gVDiskBlockData.Media.LogicalBlocksPerPhysicalBlock = 1;

	pBlockIo->Revision = EFI_BLOCK_IO_PROTOCOL_REVISION3;
	pBlockIo->Media = &(gVDiskBlockData.Media);
	pBlockIo->Reset = vdisk_block_io_reset;
    pBlockIo->ReadBlocks = vdisk_block_io_read;
	pBlockIo->WriteBlocks = vdisk_block_io_write;
	pBlockIo->FlushBlocks = vdisk_block_io_flush;

    Status = gBS->InstallMultipleProtocolInterfaces(&gVDiskBlockData.Handle,
            &gEfiBlockIoProtocolGuid, &gVDiskBlockData.BlockIo,
            &gEfiDevicePathProtocolGuid, gVDiskBlockData.Path,
            NULL);
    debug("Install protocol %r %p", Status, gVDiskBlockData.Handle);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Status = vdisk_connect_driver(gVDiskBlockData.Handle, L"Disk I/O Driver");
    debug("Connect disk IO driver %r", Status);

    Status = vdisk_connect_driver(gVDiskBlockData.Handle, L"Partition Driver");
    debug("Connect partition driver %r", Status);
    if (EFI_ERROR(Status))
    {
        Status = gBS->ConnectController(gVDiskBlockData.Handle, NULL, NULL, TRUE);
        debug("Connect all controller %r", Status);
    }

    vdisk_debug_pause();

    return EFI_SUCCESS;
}

