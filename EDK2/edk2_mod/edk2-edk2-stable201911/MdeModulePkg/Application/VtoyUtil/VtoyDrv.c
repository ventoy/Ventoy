/******************************************************************************
 * VtoyDrv.c
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

STATIC UINTN g_EfiDriverNameCnt = 0;
STATIC CHAR16 *g_EfiDriverNameList[1024] = { NULL };

STATIC EFI_STATUS AddEfiDriverName(IN CHAR16 *DriverName)
{
    UINTN i = 0;

    if (g_EfiDriverNameCnt >= 1024)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    for (i = 0; i < g_EfiDriverNameCnt; i++)
    {
        if (g_EfiDriverNameList[i] && StrCmp(g_EfiDriverNameList[i], DriverName) == 0)
        {
            break;
        }
    }

    if (i >= g_EfiDriverNameCnt)
    {
        g_EfiDriverNameList[g_EfiDriverNameCnt] = DriverName;
        g_EfiDriverNameCnt++;
    }

    return EFI_SUCCESS;
}

EFI_STATUS ShowEfiDrivers(IN EFI_HANDLE    ImageHandle, IN CONST CHAR16 *CmdLine)
{
    UINTN i = 0;
    UINTN Count = 0;
    CHAR16 *DriverName = NULL;
    EFI_HANDLE *Handles = NULL;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_COMPONENT_NAME_PROTOCOL *NameProtocol = NULL;
    EFI_COMPONENT_NAME2_PROTOCOL *Name2Protocol = NULL;
    
    (VOID)ImageHandle;
    (VOID)CmdLine;

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

        DriverName = NULL;
        Status = VtoyGetComponentName(2, Name2Protocol, &DriverName);
        if ((!EFI_ERROR(Status)) && (DriverName))
        {
            AddEfiDriverName(DriverName);
        }
    }

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

        DriverName = NULL;
        Status = VtoyGetComponentName(1, Name2Protocol, &DriverName);
        if ((!EFI_ERROR(Status)) && (DriverName))
        {
            AddEfiDriverName(DriverName);
        }
    }

    FreePool(Handles);

    for (i = 0; i < g_EfiDriverNameCnt; i++)
    {
        Printf("%2d  %s\n", i, g_EfiDriverNameList[i]);
    }
    
    return EFI_SUCCESS;
}

