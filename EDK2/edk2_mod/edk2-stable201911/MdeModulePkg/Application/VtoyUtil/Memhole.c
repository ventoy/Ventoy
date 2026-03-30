/******************************************************************************
 * Memhole.c
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

STATIC BOOLEAN IsMemContiguous
(
    IN CONST EFI_MEMORY_DESCRIPTOR *Prev,
    IN CONST EFI_MEMORY_DESCRIPTOR *Curr,
    IN CONST EFI_MEMORY_DESCRIPTOR *Next
)
{
    UINTN Addr1 = 0;
    UINTN Addr2 = 0;

    if (Prev == NULL || Curr == NULL || Next == NULL)
    {
        return FALSE;
    }

    if (Prev->Type == EfiBootServicesData &&
        Curr->Type == EfiConventionalMemory &&
        Next->Type == EfiBootServicesData)
    {
        Addr1 = Prev->PhysicalStart + MultU64x64(SIZE_4KB, Prev->NumberOfPages);
        Addr2 = Curr->PhysicalStart + MultU64x64(SIZE_4KB, Curr->NumberOfPages);

        if (Addr1 == Curr->PhysicalStart && Addr2 == Next->PhysicalStart)
        {
            return TRUE;
        }
    }

    return FALSE;
}

STATIC EFI_MEMORY_DESCRIPTOR* GetMemDesc
(
    OUT UINTN *pSize,
    OUT UINTN *pItemSize,
    OUT UINTN *pDescCount    
)
{
    UINTN Size = 0;
    UINTN MapKey = 0;
    UINTN ItemSize = 0;
    UINTN DescCount = 0;
    UINT32 Version = 0;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_MEMORY_DESCRIPTOR *pDesc = NULL;
    EFI_MEMORY_DESCRIPTOR *Curr = NULL;

    Status = gBS->GetMemoryMap(&Size, pDesc, &MapKey, &ItemSize, &Version);
    if (EFI_BUFFER_TOO_SMALL != Status)
    {
        debug("GetMemoryMap: %r", Status);
        return NULL;
    }

    Size += SIZE_1KB;
    pDesc = AllocatePool(Size);
    if (!pDesc)
    {
        debug("AllocatePool: %lu failed", Size);
        return NULL;
    }

    ZeroMem(pDesc, Size);

    Status = gBS->GetMemoryMap(&Size, pDesc, &MapKey, &ItemSize, &Version);
    if (EFI_ERROR(Status))
    {
        debug("GetMemoryMap: %r", Status);
        FreePool(pDesc);
        return NULL;
    }

    Curr = pDesc;
    while (Curr && Curr < (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)pDesc + Size))
    {
        DescCount++;
        Curr = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Curr + ItemSize);
    }

    *pSize = Size;
    *pItemSize = ItemSize;
    *pDescCount = DescCount;

    debug("GetMemoryMap: ItemSize:%lu  Count:%lu", ItemSize, DescCount);

    return pDesc;
}

EFI_STATUS FixWindowsMemhole(IN EFI_HANDLE    ImageHandle, IN CONST CHAR16 *CmdLine)
{
    UINTN Size = 0;
    UINTN ItemSize = 0;
    UINTN DescCount = 0;
    UINTN TotalMem = 0;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_PHYSICAL_ADDRESS AllocAddr = 0;
    EFI_MEMORY_DESCRIPTOR *pDescs = NULL;
    EFI_MEMORY_DESCRIPTOR *Prev = NULL;
    EFI_MEMORY_DESCRIPTOR *Next = NULL;
    EFI_MEMORY_DESCRIPTOR *Curr = NULL;
    
    (VOID)ImageHandle;
    (VOID)CmdLine;

    pDescs = GetMemDesc(&Size, &ItemSize, &DescCount);
    if (!pDescs)
    {
        return EFI_NOT_FOUND;
    }

    if (DescCount < 500)
    {
        FreePool(pDescs);
        Printf("There is no need to fixup (%lu)\n", DescCount);
        return EFI_SUCCESS;
    }

    Curr = pDescs;
    while ((UINT8 *)Curr < (UINT8 *)pDescs + Size)
    {
        Next = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Curr + ItemSize);

        if (IsMemContiguous(Prev, Curr, Next))
        {
            AllocAddr = Curr->PhysicalStart;
            Status = gBS->AllocatePages(AllocateAddress, EfiBootServicesData, Curr->NumberOfPages, &AllocAddr);
            if (EFI_SUCCESS == Status)
            {
                TotalMem += MultU64x64(SIZE_4KB, Curr->NumberOfPages);
            }
        }

        Prev = Curr;
        Curr = Next;
    }

    Printf("Fixup Windows mmap issue OK (%lu)\n", TotalMem);
    
    return EFI_SUCCESS;
}

