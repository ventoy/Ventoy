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
#include <Protocol/DriverBinding.h>
#include <Ventoy.h>

#define PROCOTOL_SLEEP_MSECONDS  0

#define debug_sleep() if (PROCOTOL_SLEEP_MSECONDS) gBS->Stall(1000 * PROCOTOL_SLEEP_MSECONDS)

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

STATIC EFI_STATUS EFIAPI ventoy_handle_protocol
(
    IN  EFI_HANDLE                Handle,
    IN  EFI_GUID                 *Protocol,
    OUT VOID                    **Interface
)
{
    EFI_STATUS Status = EFI_SUCCESS;
    
    debug("ventoy_handle_protocol:%a", ventoy_get_guid_name(Protocol)); debug_sleep();
    Status = g_system_wrapper.OriHandleProtocol(Handle, Protocol, Interface);

    if (CompareGuid(Protocol, &gEfiSimpleFileSystemProtocolGuid))
    {
        EFI_FILE_PROTOCOL *FileProtocol = NULL;
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *pFile = *((EFI_SIMPLE_FILE_SYSTEM_PROTOCOL **)(Interface));
        
        pFile->OpenVolume(pFile, &FileProtocol);
        
        trace("Handle FS Protocol: %p OpenVolume:%p, FileProtocol:%p, Open:%p", 
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
    debug("ventoy_open_protocol:<%p> %a", Handle, ventoy_get_guid_name(Protocol));  debug_sleep();
    return g_system_wrapper.OriOpenProtocol(Handle, Protocol, Interface, AgentHandle, ControllerHandle, Attributes);
}

STATIC EFI_STATUS EFIAPI ventoy_locate_protocol
(
    IN  EFI_GUID   *Protocol,
    IN  VOID       *Registration, OPTIONAL
    OUT VOID      **Interface
)
{
    debug("ventoy_locate_protocol:%a", ventoy_get_guid_name(Protocol));  debug_sleep();
    return g_system_wrapper.OriLocateProtocol(Protocol, Registration, Interface);
}

STATIC EFI_STATUS EFIAPI ventoy_locate_handle_buffer
(
    IN     EFI_LOCATE_SEARCH_TYPE        SearchType,
    IN     EFI_GUID                     *Protocol,      OPTIONAL
    IN     VOID                         *SearchKey,     OPTIONAL
    IN OUT UINTN                        *NoHandles,
    OUT    EFI_HANDLE                  **Buffer
)
{
    debug("ventoy_locate_handle_buffer:%a", ventoy_get_guid_name(Protocol));  debug_sleep();
    return g_system_wrapper.OriLocateHandleBuffer(SearchType, Protocol, SearchKey, NoHandles, Buffer);
}

STATIC EFI_STATUS EFIAPI ventoy_protocol_per_handle
(
    IN  EFI_HANDLE      Handle,
    OUT EFI_GUID        ***ProtocolBuffer,
    OUT UINTN           *ProtocolBufferCount
)
{
    debug("ventoy_protocol_per_handle:%p", Handle);  debug_sleep();
    return g_system_wrapper.OriProtocolsPerHandle(Handle, ProtocolBuffer, ProtocolBufferCount);
}

EFI_STATUS EFIAPI ventoy_locate_handle
(
    IN     EFI_LOCATE_SEARCH_TYPE    SearchType,
    IN     EFI_GUID                 *Protocol,    OPTIONAL
    IN     VOID                     *SearchKey,   OPTIONAL
    IN OUT UINTN                    *BufferSize,
    OUT    EFI_HANDLE               *Buffer
)
{
    UINTN i;
    EFI_HANDLE Handle;
    EFI_STATUS Status = EFI_SUCCESS;
    
    debug("ventoy_locate_handle: %d %a %p", SearchType, ventoy_get_guid_name(Protocol), SearchKey); 
    Status = g_system_wrapper.OriLocateHandle(SearchType, Protocol, SearchKey, BufferSize, Buffer);
    debug("ventoy_locate_handle: %r Handle Count:%u", Status, *BufferSize/sizeof(EFI_HANDLE));

    if (EFI_SUCCESS == Status)
    {
        for (i = 0; i < *BufferSize / sizeof(EFI_HANDLE); i++)
        {
            if (Buffer[i] == gBlockData.Handle)
            {
                Handle = Buffer[0];
                Buffer[0] = Buffer[i];
                Buffer[i] = Handle;
                debug("####### Handle at %u", i);
                break;
            }
        }
    }

    debug_sleep();

    return Status;
}

STATIC EFI_STATUS EFIAPI ventoy_locate_device_path
(
    IN     EFI_GUID                          *Protocol,
    IN OUT EFI_DEVICE_PATH_PROTOCOL         **DevicePath,
    OUT    EFI_HANDLE                        *Device
)
{
    debug("ventoy_locate_device_path:%a", ventoy_get_guid_name(Protocol));  debug_sleep();
    return g_system_wrapper.OriLocateDevicePath(Protocol, DevicePath, Device);
}

EFI_STATUS EFIAPI ventoy_wrapper_system(VOID)
{
    ventoy_wrapper(gBS, g_system_wrapper, LocateProtocol,       ventoy_locate_protocol);
    ventoy_wrapper(gBS, g_system_wrapper, HandleProtocol,       ventoy_handle_protocol);
    ventoy_wrapper(gBS, g_system_wrapper, OpenProtocol,         ventoy_open_protocol);
    ventoy_wrapper(gBS, g_system_wrapper, LocateHandleBuffer,   ventoy_locate_handle_buffer);
    ventoy_wrapper(gBS, g_system_wrapper, ProtocolsPerHandle,   ventoy_protocol_per_handle);
    ventoy_wrapper(gBS, g_system_wrapper, LocateHandle,         ventoy_locate_handle);
    ventoy_wrapper(gBS, g_system_wrapper, LocateDevicePath,     ventoy_locate_device_path);

    return EFI_SUCCESS;
}

