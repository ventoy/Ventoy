/******************************************************************************
 * VtoyUtil.c
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
#include <VtoyUtil.h>

BOOLEAN gVtoyDebugPrint = FALSE;
STATIC CONST CHAR16 *gCurFeature= NULL;
STATIC CHAR16 *gCmdLine = NULL;
STATIC grub_env_printf_pf g_env_printf = NULL;

STATIC VtoyUtilFeature gFeatureList[] = 
{
    { L"fix_windows_mmap", FixWindowsMemhole },
    { L"show_efi_drivers", ShowEfiDrivers    },
};

EFI_STATUS VtoyGetComponentName(IN UINTN Ver, IN VOID *Protocol, OUT CHAR16 **DriverName)
{
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR16 *DrvName = NULL;
    EFI_COMPONENT_NAME_PROTOCOL *NameProtocol = NULL;
    EFI_COMPONENT_NAME2_PROTOCOL *Name2Protocol = NULL;

    if (1 == Ver)
    {
        NameProtocol = (EFI_COMPONENT_NAME_PROTOCOL *)Protocol;
        Status = NameProtocol->GetDriverName(Protocol, "en", &DrvName);
        if (EFI_ERROR(Status) || NULL == DrvName)
        {
            Status = NameProtocol->GetDriverName(Protocol, "eng", &DrvName);
        }
    }
    else
    {
        Name2Protocol = (EFI_COMPONENT_NAME2_PROTOCOL *)Protocol;
        Status = Name2Protocol->GetDriverName(Protocol, "en", &DrvName);
        if (EFI_ERROR(Status) || NULL == DrvName)
        {
            Status = Name2Protocol->GetDriverName(Protocol, "eng", &DrvName);
        }
    }

    *DriverName = DrvName;
    return Status;
}

VOID EFIAPI VtoyUtilDebug(IN CONST CHAR8  *Format, ...)
{
    VA_LIST  Marker;
    CHAR8    Buffer[512];

    VA_START (Marker, Format);
    AsciiVSPrint(Buffer, sizeof(Buffer), Format, Marker);
    VA_END (Marker);

    if (g_env_printf)
    {
        g_env_printf("%s", Buffer);
    }
}

STATIC EFI_STATUS ParseCmdline(IN EFI_HANDLE ImageHandle)
{   
    CHAR16 *pPos = NULL;
    CHAR16 *pCmdLine = NULL;
    EFI_STATUS Status = EFI_SUCCESS;
    ventoy_grub_param *pGrubParam = NULL;
    EFI_LOADED_IMAGE_PROTOCOL *pImageInfo = NULL;

    Status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&pImageInfo);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    pCmdLine = (CHAR16 *)AllocatePool(pImageInfo->LoadOptionsSize + 4);
    SetMem(pCmdLine, pImageInfo->LoadOptionsSize + 4, 0);
    CopyMem(pCmdLine, pImageInfo->LoadOptions, pImageInfo->LoadOptionsSize);

    if (StrStr(pCmdLine, L"vtoyefitest"))
    {
        gST->ConOut->OutputString(gST->ConOut, L"\r\n##########################");
        gST->ConOut->OutputString(gST->ConOut, L"\r\n#########  VTOY  #########");
        gST->ConOut->OutputString(gST->ConOut, L"\r\n##########################");
        return EFI_SUCCESS;
    }
    
    if (StrStr(pCmdLine, L"debug"))
    {
        gVtoyDebugPrint = TRUE;
    }

    pPos = StrStr(pCmdLine, L"env_param=");
    if (!pPos)
    {
        return EFI_INVALID_PARAMETER;
    }

    pGrubParam = (ventoy_grub_param *)StrHexToUintn(pPos + StrLen(L"env_param="));
    g_env_printf = pGrubParam->grub_env_printf;

    pPos = StrStr(pCmdLine, L"feature=");
    if (!pPos)
    {
        return EFI_INVALID_PARAMETER;
    }

    gCurFeature = pPos + StrLen(L"feature=");
    
    gCmdLine = pCmdLine;
    
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI VtoyUtilEfiMain
(
    IN EFI_HANDLE         ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
)
{
    UINTN i;
    UINTN Len;
    
    ParseCmdline(ImageHandle);

    for (i = 0; gCurFeature && i < ARRAY_SIZE(gFeatureList); i++)
    {
        Len = StrLen(gFeatureList[i].Cmd);
        if (StrnCmp(gFeatureList[i].Cmd, gCurFeature, Len) == 0)
        {
            debug("Find main proc <%s>", gFeatureList[i].Cmd);
            gFeatureList[i].MainProc(ImageHandle, gCurFeature + Len);
            break;
        }
    }

    if (gCmdLine)
    {
        FreePool(gCmdLine);
        gCmdLine = NULL;        
    }

    return EFI_SUCCESS;
}

