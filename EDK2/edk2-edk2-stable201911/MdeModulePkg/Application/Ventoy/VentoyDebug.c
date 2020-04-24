/******************************************************************************
 * Ventoy.c
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
#include <Ventoy.h>

STATIC ventoy_system_wrapper g_system_wrapper;

static struct well_known_guid g_efi_well_known_guids[] = 
{
	{ &gEfiAbsolutePointerProtocolGuid, "AbsolutePointer" },
	{ &gEfiAcpiTableProtocolGuid, "AcpiTable" },
	{ &gEfiBlockIoProtocolGuid, "BlockIo" },
	{ &gEfiBlockIo2ProtocolGuid, "BlockIo2" },
	{ &gEfiBusSpecificDriverOverrideProtocolGuid, "BusSpecificDriverOverride" },
	{ &gEfiComponentNameProtocolGuid, "ComponentName" },
	{ &gEfiComponentName2ProtocolGuid, "ComponentName2" },
	{ &gEfiDevicePathProtocolGuid, "DevicePath" },
	{ &gEfiDriverBindingProtocolGuid, "DriverBinding" },
	{ &gEfiDiskIoProtocolGuid, "DiskIo" },
	{ &gEfiDiskIo2ProtocolGuid, "DiskIo2" },
	{ &gEfiGraphicsOutputProtocolGuid, "GraphicsOutput" },
	{ &gEfiHiiConfigAccessProtocolGuid, "HiiConfigAccess" },
	{ &gEfiHiiFontProtocolGuid, "HiiFont" },
	{ &gEfiLoadFileProtocolGuid, "LoadFile" },
	{ &gEfiLoadFile2ProtocolGuid, "LoadFile2" },
	{ &gEfiLoadedImageProtocolGuid, "LoadedImage" },
	{ &gEfiLoadedImageDevicePathProtocolGuid, "LoadedImageDevicePath"},
	{ &gEfiPciIoProtocolGuid, "PciIo" },
	{ &gEfiSerialIoProtocolGuid, "SerialIo" },
	{ &gEfiSimpleFileSystemProtocolGuid, "SimpleFileSystem" },
	{ &gEfiSimpleTextInProtocolGuid, "SimpleTextInput" },
	{ &gEfiSimpleTextInputExProtocolGuid, "SimpleTextInputEx" },
	{ &gEfiSimpleTextOutProtocolGuid, "SimpleTextOutput" },
};

STATIC CHAR8 gEfiGuidName[128];

static const char * ventoy_get_guid_name(EFI_GUID *guid)
{
    UINTN i;

    for (i = 0; i < ARRAY_SIZE(g_efi_well_known_guids); i++)
    {
        if (CompareGuid(g_efi_well_known_guids[i].guid, guid))
        {
            return g_efi_well_known_guids[i].name;
        }
    }

    AsciiSPrint(gEfiGuidName, sizeof(gEfiGuidName), "%g", guid);
    return gEfiGuidName;
}

EFI_STATUS EFIAPI
ventoy_wrapper_fs_open(EFI_FILE_HANDLE This, EFI_FILE_HANDLE *New, CHAR16 *Name, UINT64 Mode, UINT64 Attributes)
{
    (VOID)This;
    (VOID)New;
    (VOID)Name;
    (VOID)Mode;
    (VOID)Attributes;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
ventoy_wrapper_file_open_ex(EFI_FILE_HANDLE This, EFI_FILE_HANDLE *New, CHAR16 *Name, UINT64 Mode, UINT64 Attributes, EFI_FILE_IO_TOKEN *Token)
{
	return ventoy_wrapper_fs_open(This, New, Name, Mode, Attributes);
}

EFI_STATUS EFIAPI
ventoy_wrapper_file_delete(EFI_FILE_HANDLE This)
{
    (VOID)This;
	return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
ventoy_wrapper_file_set_info(EFI_FILE_HANDLE This, EFI_GUID *Type, UINTN Len, VOID *Data)
{
	return EFI_SUCCESS;
}

EFI_STATUS EFIAPI
ventoy_wrapper_file_flush(EFI_FILE_HANDLE This)
{
    (VOID)This;
	return EFI_SUCCESS;
}

/* Ex version */
EFI_STATUS EFIAPI
ventoy_wrapper_file_flush_ex(EFI_FILE_HANDLE This, EFI_FILE_IO_TOKEN *Token)
{
    (VOID)This;
    (VOID)Token;
	return EFI_SUCCESS;
}


EFI_STATUS EFIAPI
ventoy_wrapper_file_write(EFI_FILE_HANDLE This, UINTN *Len, VOID *Data)
{
    (VOID)This;
    (VOID)Len;
    (VOID)Data;

	return EFI_WRITE_PROTECTED;
}

EFI_STATUS EFIAPI
ventoy_wrapper_file_write_ex(IN EFI_FILE_PROTOCOL *This, IN OUT EFI_FILE_IO_TOKEN *Token)
{
	return ventoy_wrapper_file_write(This, &(Token->BufferSize), Token->Buffer);
}


static EFI_STATUS EFIAPI
ventoy_wrapper_file_close(EFI_FILE_HANDLE This)
{
    (VOID)This;
    return EFI_SUCCESS;
}


static EFI_STATUS EFIAPI
ventoy_wrapper_file_set_pos(EFI_FILE_HANDLE This, UINT64 Position)
{
    (VOID)This;
    
    g_efi_file_replace.CurPos = Position;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
ventoy_wrapper_file_get_pos(EFI_FILE_HANDLE This, UINT64 *Position)
{
    (VOID)This;

    *Position = g_efi_file_replace.CurPos;

    return EFI_SUCCESS;
}


static EFI_STATUS EFIAPI
ventoy_wrapper_file_get_info(EFI_FILE_HANDLE This, EFI_GUID *Type, UINTN *Len, VOID *Data)
{
    EFI_FILE_INFO *Info = (EFI_FILE_INFO *) Data;

    debug("ventoy_wrapper_file_get_info ... %u", *Len);

    if (!CompareGuid(Type, &gEfiFileInfoGuid))
    {
        return EFI_INVALID_PARAMETER;
    }

    if (*Len == 0)
    {
        *Len = 384;
        return EFI_BUFFER_TOO_SMALL;
    }

    ZeroMem(Data, sizeof(EFI_FILE_INFO));

    Info->Size = sizeof(EFI_FILE_INFO);
    Info->FileSize = g_efi_file_replace.FileSizeBytes;
    Info->PhysicalSize = g_efi_file_replace.FileSizeBytes;
    Info->Attribute = EFI_FILE_READ_ONLY;
    //Info->FileName = EFI_FILE_READ_ONLY;

    *Len = Info->Size;
    
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
ventoy_wrapper_file_read(EFI_FILE_HANDLE This, UINTN *Len, VOID *Data)
{
    EFI_LBA Lba;
    UINTN ReadLen = *Len;
    
    (VOID)This;

    debug("ventoy_wrapper_file_read ... %u", *Len);

    if (g_efi_file_replace.CurPos + ReadLen > g_efi_file_replace.FileSizeBytes)
    {
        ReadLen = g_efi_file_replace.FileSizeBytes - g_efi_file_replace.CurPos;
    }

    Lba = g_efi_file_replace.CurPos / 2048 + g_efi_file_replace.BlockIoSectorStart;

    ventoy_block_io_read(NULL, 0, Lba, ReadLen, Data);

    *Len = ReadLen;

    g_efi_file_replace.CurPos += ReadLen;

    return EFI_SUCCESS;
}


EFI_STATUS EFIAPI
ventoy_wrapper_file_read_ex(IN EFI_FILE_PROTOCOL *This, IN OUT EFI_FILE_IO_TOKEN *Token)
{
	return ventoy_wrapper_file_read(This, &(Token->BufferSize), Token->Buffer);
}

EFI_STATUS EFIAPI ventoy_wrapper_file_procotol(EFI_FILE_PROTOCOL *File)
{
    File->Revision    = EFI_FILE_PROTOCOL_REVISION2;
    File->Open        = ventoy_wrapper_fs_open;
    File->Close       = ventoy_wrapper_file_close;
    File->Delete      = ventoy_wrapper_file_delete;
    File->Read        = ventoy_wrapper_file_read;
    File->Write       = ventoy_wrapper_file_write;
    File->GetPosition = ventoy_wrapper_file_get_pos;
    File->SetPosition = ventoy_wrapper_file_set_pos;
    File->GetInfo     = ventoy_wrapper_file_get_info;
    File->SetInfo     = ventoy_wrapper_file_set_info;
    File->Flush       = ventoy_wrapper_file_flush;
    File->OpenEx      = ventoy_wrapper_file_open_ex;
    File->ReadEx      = ventoy_wrapper_file_read_ex;
    File->WriteEx     = ventoy_wrapper_file_write_ex;
    File->FlushEx     = ventoy_wrapper_file_flush_ex;

    return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI ventoy_handle_protocol
(
    IN  EFI_HANDLE                Handle,
    IN  EFI_GUID                 *Protocol,
    OUT VOID                    **Interface
)
{
    EFI_STATUS Status = EFI_SUCCESS;
    
    debug("ventoy_handle_protocol:%a", ventoy_get_guid_name(Protocol)); 
    Status = g_system_wrapper.OriHandleProtocol(Handle, Protocol, Interface);

    if (CompareGuid(Protocol, &gEfiSimpleFileSystemProtocolGuid))
    {
        EFI_FILE_PROTOCOL *FileProtocol = NULL;
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *pFile = *((EFI_SIMPLE_FILE_SYSTEM_PROTOCOL **)(Interface));
        
        pFile->OpenVolume(pFile, &FileProtocol);
        
        debug("Handle FS Protocol: %p OpenVolume:%p, FileProtocol:%p, Open:%p", 
            pFile, pFile->OpenVolume, FileProtocol, FileProtocol->Open); 

        sleep(3);
    }

    return Status;
}

STATIC EFI_STATUS EFIAPI ventoy_open_protocol
(
    IN  EFI_HANDLE                 Handle,
    IN  EFI_GUID                  *Protocol,
    OUT VOID                     **Interface, OPTIONAL
    IN  EFI_HANDLE                 AgentHandle,
    IN  EFI_HANDLE                 ControllerHandle,
    IN  UINT32                     Attributes
)
{
    debug("ventoy_open_protocol:%a", ventoy_get_guid_name(Protocol));
    return g_system_wrapper.OriOpenProtocol(Handle, Protocol, Interface, AgentHandle, ControllerHandle, Attributes);
}

STATIC EFI_STATUS EFIAPI ventoy_locate_protocol
(
    IN  EFI_GUID   *Protocol,
    IN  VOID       *Registration, OPTIONAL
    OUT VOID      **Interface
)
{
    debug("ventoy_locate_protocol:%a", ventoy_get_guid_name(Protocol));
    return g_system_wrapper.OriLocateProtocol(Protocol, Registration, Interface);
}

EFI_STATUS EFIAPI ventoy_wrapper_system(VOID)
{
    ventoy_wrapper(gBS, g_system_wrapper, LocateProtocol, ventoy_locate_protocol);
    ventoy_wrapper(gBS, g_system_wrapper, HandleProtocol, ventoy_handle_protocol);
    ventoy_wrapper(gBS, g_system_wrapper, OpenProtocol,    ventoy_open_protocol);

    return EFI_SUCCESS;
}

