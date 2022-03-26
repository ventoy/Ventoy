/******************************************************************************
 * VDiskChain.c
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
#include <Library/UefiDecompressLib.h>
#include <Protocol/LoadedImage.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <Protocol/BlockIo.h>
#include <Protocol/RamDisk.h>
#include <Protocol/SimpleFileSystem.h>
#include <VDiskChain.h>

BOOLEAN gVDiskDebugPrint = FALSE;
vdisk_block_data gVDiskBlockData;

/* Boot filename */
CONST CHAR16 *gEfiBootFileName[] = 
{
    L"@",
    EFI_REMOVABLE_MEDIA_FILE_NAME,
#if   defined (MDE_CPU_IA32)
    L"\\EFI\\BOOT\\GRUBIA32.EFI",
    L"\\EFI\\BOOT\\BOOTia32.EFI",
    L"\\EFI\\BOOT\\bootia32.efi",
    L"\\efi\\boot\\bootia32.efi",
#elif defined (MDE_CPU_X64)
    L"\\EFI\\BOOT\\GRUBX64.EFI",
    L"\\EFI\\BOOT\\BOOTx64.EFI",
    L"\\EFI\\BOOT\\bootx64.efi",
    L"\\efi\\boot\\bootx64.efi",
#elif defined (MDE_CPU_ARM)
    L"\\EFI\\BOOT\\GRUBARM.EFI",
    L"\\EFI\\BOOT\\BOOTarm.EFI",
    L"\\EFI\\BOOT\\bootarm.efi",
    L"\\efi\\boot\\bootarm.efi",
#elif defined (MDE_CPU_AARCH64)
    L"\\EFI\\BOOT\\GRUBAA64.EFI",
    L"\\EFI\\BOOT\\BOOTaa64.EFI",
    L"\\EFI\\BOOT\\bootaa64.efi",
    L"\\efi\\boot\\bootaa64.efi",
#endif
    
};

UINT8 *g_disk_buf_addr = NULL;
UINT64 g_disk_buf_size = 0;

STATIC EFI_GET_VARIABLE g_org_get_variable = NULL;
STATIC EFI_EXIT_BOOT_SERVICES g_org_exit_boot_service = NULL;

VOID EFIAPI VDiskDebug(IN CONST CHAR8  *Format, ...)
{
    VA_LIST  Marker;
    CHAR16   Buffer[512];

    VA_START (Marker, Format);
    UnicodeVSPrintAsciiFormat(Buffer, sizeof(Buffer), Format, Marker);
    VA_END (Marker);
    
    gST->ConOut->OutputString(gST->ConOut, Buffer);
}

VOID EFIAPI vdisk_clear_input(VOID)
{
    EFI_INPUT_KEY Key;
    
    gST->ConIn->Reset(gST->ConIn, FALSE);
    while (EFI_SUCCESS == gST->ConIn->ReadKeyStroke(gST->ConIn, &Key))
    {
        ;
    }
    gST->ConIn->Reset(gST->ConIn, FALSE);
}

STATIC EFI_STATUS EFIAPI vdisk_load_image
(
    IN EFI_HANDLE ImageHandle,
    IN EFI_DEVICE_PATH_PROTOCOL *pDevicePath,
    IN CONST CHAR16 *FileName,
    IN UINTN FileNameLen,
    OUT EFI_HANDLE *Image
)
{
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR16 TmpBuf[256] = {0};
    FILEPATH_DEVICE_PATH *pFilePath = NULL;
    EFI_DEVICE_PATH_PROTOCOL *pImgPath = NULL;

    pFilePath = (FILEPATH_DEVICE_PATH *)TmpBuf;
    pFilePath->Header.Type = MEDIA_DEVICE_PATH;
    pFilePath->Header.SubType = MEDIA_FILEPATH_DP;
    pFilePath->Header.Length[0] = FileNameLen + sizeof(EFI_DEVICE_PATH_PROTOCOL);
    pFilePath->Header.Length[1] = 0;
    CopyMem(pFilePath->PathName, FileName, FileNameLen);
    
    pImgPath = AppendDevicePathNode(pDevicePath, (EFI_DEVICE_PATH_PROTOCOL *)pFilePath);
    if (!pImgPath)
    {
        return EFI_NOT_FOUND;
    }
    
    Status = gBS->LoadImage(FALSE, ImageHandle, pImgPath, NULL, 0, Image);
    
    debug("Load Image File %r DP: <%s>", Status, ConvertDevicePathToText(pImgPath, FALSE, FALSE));

    FreePool(pImgPath);
    
    return Status;
}

STATIC EFI_STATUS EFIAPI vdisk_decompress_vdisk(IN EFI_LOADED_IMAGE_PROTOCOL *pImageInfo)
{
    UINT32 Size;
    UINT32 DestinationSize;
    UINT32 ScratchSize;
    UINT8 *buf;
    VOID  *ScratchBuf;
    EFI_STATUS Status = EFI_SUCCESS;

    (VOID)pImageInfo;

    vdisk_get_vdisk_raw(&buf, &Size);
    UefiDecompressGetInfo(buf + VDISK_MAGIC_LEN, Size - VDISK_MAGIC_LEN, &DestinationSize, &ScratchSize);
    debug("vdisk: size:%u realsize:%u", Size, DestinationSize);

    g_disk_buf_size = DestinationSize;
    g_disk_buf_addr = AllocatePool(DestinationSize);
    ScratchBuf = AllocatePool(ScratchSize);
    
    Status = UefiDecompress(buf + VDISK_MAGIC_LEN, g_disk_buf_addr, ScratchBuf);
    FreePool(ScratchBuf);

    debug("Status:%r %p %u", Status, g_disk_buf_addr, (UINT32)g_disk_buf_size);
    
    return EFI_SUCCESS;
}

STATIC EFI_STATUS vdisk_patch_vdisk_path(CHAR16 *pos)
{
    UINTN i;
    UINTN j;
    CHAR16 *end;
    CHAR8 *buf = (char *)g_disk_buf_addr;
    
    if (*pos == L'\"')
    {
        pos++;
    }

    end = StrStr(pos, L".vtoy");
    end += 5;//string length
    
    for (i = 0; i < g_disk_buf_size; i++)
    {
        if (*(UINT32 *)(buf + i) == 0x59595959)
        {
            for (j = 0; j < 300; j++)
            {
                if (buf[i + j] != 'Y')
                {
                    break;
                }
            }

            if (j >= 300)
            {
                break; 
            }
        }
    }

    if (i >= g_disk_buf_size)
    {
        debug("No need to fill vdisk path");
        return 0;
    }

    debug("Fill vdisk path at %d", i);        

    while (pos != end)
    {
        buf[i++] = (CHAR8)(*pos++);
    }

    buf[i++] = '\"';
    
    while (buf[i] == 'Y' || buf[i] == '\"')
    {
        buf[i] = ' ';
        i++;
    }   

    return 0;    
}

EFI_STATUS EFIAPI vdisk_get_variable_wrapper
(
    IN     CHAR16                      *VariableName,
    IN     EFI_GUID                    *VendorGuid,
    OUT    UINT32                      *Attributes,    OPTIONAL
    IN OUT UINTN                       *DataSize,
    OUT    VOID                        *Data           OPTIONAL
)
{
    EFI_STATUS Status = EFI_SUCCESS;
    
    Status = g_org_get_variable(VariableName, VendorGuid, Attributes, DataSize, Data);
    if (StrCmp(VariableName, L"SecureBoot") == 0)
    {
        if ((*DataSize == 1) && Data)
        {
            *(UINT8 *)Data = 0;
        }
    }

    return Status;
}

EFI_STATUS EFIAPI vdisk_exit_boot_service_wrapper
(
    IN  EFI_HANDLE                   ImageHandle,
    IN  UINTN                        MapKey
)
{
    if (g_org_get_variable)
    {
        gRT->GetVariable = g_org_get_variable;
        g_org_get_variable = NULL;
    }

    return g_org_exit_boot_service(ImageHandle, MapKey);
}

STATIC EFI_STATUS EFIAPI vdisk_disable_secure_boot(IN EFI_HANDLE ImageHandle)
{
    /* step1: wrapper security protocol. */
    /* Do we still need it since we have been loaded ? */
    
    
    /* step2: fake SecureBoot variable */
    g_org_exit_boot_service = gBS->ExitBootServices;
    gBS->ExitBootServices = vdisk_exit_boot_service_wrapper;
    
    g_org_get_variable = gRT->GetVariable;
    gRT->GetVariable = vdisk_get_variable_wrapper;

    return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI vdisk_parse_cmdline(IN EFI_HANDLE ImageHandle)
{   
    CHAR16 *Pos = NULL;
    CHAR16 *pCmdLine = NULL;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_LOADED_IMAGE_PROTOCOL *pImageInfo = NULL;

    Status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&pImageInfo);
    if (EFI_ERROR(Status))
    {
        VDiskDebug("Failed to handle load image protocol %r\n", Status);
        return Status;
    }

    pCmdLine = (CHAR16 *)AllocatePool(pImageInfo->LoadOptionsSize + 4);
    SetMem(pCmdLine, pImageInfo->LoadOptionsSize + 4, 0);
    CopyMem(pCmdLine, pImageInfo->LoadOptions, pImageInfo->LoadOptionsSize);

    if (StrStr(pCmdLine, L"debug"))
    {
        gVDiskDebugPrint = TRUE;
    }
    
    debug("cmdline:<%s>", pCmdLine);
    vdisk_debug_pause();

    Pos = StrStr(pCmdLine, L"vdisk=");
    if (NULL == Pos || NULL == StrStr(pCmdLine, L".vtoy"))
    {
        VDiskDebug("vdisk parameter not found!\n");
        return EFI_NOT_FOUND;
    }

    vdisk_decompress_vdisk(pImageInfo);

    vdisk_patch_vdisk_path(Pos + 6);

    if (StrStr(pCmdLine, L"secureboot=off"))
    {
        vdisk_disable_secure_boot(ImageHandle);
    }

    FreePool(pCmdLine);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI vdisk_boot(IN EFI_HANDLE ImageHandle)
{
    UINTN t = 0;
    UINTN i = 0;
    UINTN j = 0;
    UINTN Find = 0;
    UINTN Count = 0;
    EFI_HANDLE Image = NULL;
    EFI_HANDLE *Handles = NULL;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *pFile = NULL;
    EFI_DEVICE_PATH_PROTOCOL *pDevPath = NULL;

    for (t = 0; t < 3; t++)
    {
        Count = 0;
        Handles = NULL;

        Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, 
                                     NULL, &Count, &Handles);
        if (EFI_ERROR(Status))
        {
            return Status;
        }

        debug("vdisk_boot fs count:%u", Count);

        for (i = 0; i < Count; i++)
        {
            Status = gBS->HandleProtocol(Handles[i], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&pFile);
            if (EFI_ERROR(Status))
            {
                continue;
            }

            debug("FS:%u Protocol:%p  OpenVolume:%p", i, pFile, pFile->OpenVolume);

            Status = gBS->OpenProtocol(Handles[i], &gEfiDevicePathProtocolGuid, 
                                       (VOID **)&pDevPath,
                                       ImageHandle,
                                       Handles[i],
                                       EFI_OPEN_PROTOCOL_GET_PROTOCOL);
            if (EFI_ERROR(Status))
            {
                debug("Failed to open device path protocol %r", Status);
                continue;
            }

            debug("Handle:%p FS DP: <%s>", Handles[i], ConvertDevicePathToText(pDevPath, FALSE, FALSE));
            if (CompareMem(gVDiskBlockData.Path, pDevPath, gVDiskBlockData.DevicePathCompareLen))
            {
                debug("Not ventoy disk file system");
                continue;
            }

            for (j = 1; j < ARRAY_SIZE(gEfiBootFileName); j++)
            {
                Status = vdisk_load_image(ImageHandle, pDevPath, gEfiBootFileName[j], 
                                           StrSize(gEfiBootFileName[j]), &Image);
                if (EFI_SUCCESS == Status)
                {
                    break;
                }
                debug("Failed to load image %r <%s>", Status, gEfiBootFileName[j]);
            }

            if (j >= ARRAY_SIZE(gEfiBootFileName))
            {
                continue;
            }

            Find++;
            debug("Find boot file, now try to boot .....");
            vdisk_debug_pause();

            if (gVDiskDebugPrint)
            {
                gST->ConIn->Reset(gST->ConIn, FALSE);
            }

            /* can't add debug print here */
            //ventoy_wrapper_system();
            Status = gBS->StartImage(Image, NULL, NULL);
            if (EFI_ERROR(Status))
            {
                debug("Failed to start image %r", Status);
                sleep(3);
                gBS->UnloadImage(Image);
                break;
            }
        }

        FreePool(Handles);

        if (Find == 0)
        {
            debug("Fs not found, now wait and retry...");
            sleep(2);
        }
    }

    if (Find == 0)
    {
        return EFI_NOT_FOUND;
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI VDiskChainEfiMain
(
    IN EFI_HANDLE         ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
)
{
    EFI_STATUS Status = EFI_SUCCESS;

    gST->ConOut->ClearScreen(gST->ConOut);
    vdisk_clear_input();

    Status = vdisk_parse_cmdline(ImageHandle);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    vdisk_install_blockio(ImageHandle, g_disk_buf_size);
    vdisk_debug_pause();

    Status = vdisk_boot(ImageHandle);
    
    gBS->DisconnectController(gVDiskBlockData.Handle, NULL, NULL);
    gBS->UninstallMultipleProtocolInterfaces(gVDiskBlockData.Handle,
            &gEfiBlockIoProtocolGuid, &gVDiskBlockData.BlockIo,
            &gEfiDevicePathProtocolGuid, gVDiskBlockData.Path,
            NULL);

    if (EFI_NOT_FOUND == Status)
    {
        gST->ConOut->OutputString(gST->ConOut, L"No bootfile found for UEFI!\r\n");
        gST->ConOut->OutputString(gST->ConOut, L"Maybe the image does not support " VENTOY_UEFI_DESC  L"!\r\n");
        sleep(30);
    }
    
    vdisk_clear_input();
    gST->ConOut->ClearScreen(gST->ConOut);

    return EFI_SUCCESS;
}

