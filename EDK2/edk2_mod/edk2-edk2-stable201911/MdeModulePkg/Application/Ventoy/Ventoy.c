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

BOOLEAN gDebugPrint = FALSE;
BOOLEAN gBootFallBack = FALSE;
BOOLEAN gDotEfiBoot = FALSE;
BOOLEAN gLoadIsoEfi = FALSE;
BOOLEAN gIsoUdf = FALSE;
ventoy_ram_disk g_ramdisk_param;
ventoy_chain_head *g_chain;
void *g_vtoy_img_location_buf;
ventoy_img_chunk *g_chunk;
UINT8 *g_os_param_reserved;
UINT32 g_img_chunk_num;
ventoy_override_chunk *g_override_chunk;
UINT32 g_override_chunk_num;
ventoy_virt_chunk *g_virt_chunk;
UINT32 g_virt_chunk_num;
vtoy_block_data gBlockData;
static grub_env_get_pf grub_env_get = NULL;
static grub_env_set_pf grub_env_set = NULL;

ventoy_grub_param_file_replace *g_file_replace_list = NULL;
ventoy_efi_file_replace g_efi_file_replace;

ventoy_grub_param_file_replace *g_img_replace_list = NULL;
ventoy_efi_file_replace g_img_file_replace;

CONST CHAR16 gIso9660EfiDriverPath[] = ISO9660_EFI_DRIVER_PATH;
CONST CHAR16 gUdfEfiDriverPath[] = UDF_EFI_DRIVER_PATH;

BOOLEAN g_fix_windows_1st_cdrom_issue = FALSE;

STATIC BOOLEAN g_hook_keyboard = FALSE;

CHAR16 gFirstTryBootFile[256] = {0};

STATIC EFI_GET_VARIABLE g_org_get_variable = NULL;
STATIC EFI_EXIT_BOOT_SERVICES g_org_exit_boot_service = NULL;

/* Boot filename */
UINTN gBootFileStartIndex = 1;
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

VOID EFIAPI VtoyDebug(IN CONST CHAR8  *Format, ...)
{
    VA_LIST  Marker;
    CHAR16   Buffer[512];

    VA_START (Marker, Format);
    UnicodeVSPrintAsciiFormat(Buffer, sizeof(Buffer), Format, Marker);
    VA_END (Marker);
    
    gST->ConOut->OutputString(gST->ConOut, Buffer);
}

VOID EFIAPI ventoy_clear_input(VOID)
{
    EFI_INPUT_KEY Key;
    
    gST->ConIn->Reset(gST->ConIn, FALSE);
    while (EFI_SUCCESS == gST->ConIn->ReadKeyStroke(gST->ConIn, &Key))
    {
        ;
    }
    gST->ConIn->Reset(gST->ConIn, FALSE);
}

static void EFIAPI ventoy_dump_img_chunk(ventoy_chain_head *chain)
{
    UINT32 i;
    int errcnt = 0;
    UINT64 img_sec = 0;
    ventoy_img_chunk *chunk;

    chunk = (ventoy_img_chunk *)((char *)chain + chain->img_chunk_offset);

    debug("##################### ventoy_dump_img_chunk #######################");

    for (i = 0; i < chain->img_chunk_num; i++)
    {
        debug("%2u: [ %u - %u ] <==> [ %llu - %llu ]",
               i, chunk[i].img_start_sector, chunk[i].img_end_sector, 
               chunk[i].disk_start_sector, chunk[i].disk_end_sector);

        if (i > 0 && (chunk[i].img_start_sector != chunk[i - 1].img_end_sector + 1))
        {
            errcnt++;
        }

        img_sec += chunk[i].img_end_sector - chunk[i].img_start_sector + 1;
    }

    if (errcnt == 0 && (img_sec * 2048 == g_chain->real_img_size_in_bytes))
    {
        debug("image chunk size check success");
    }
    else
    {
        debug("image chunk size check failed %d", errcnt);
    }
    
    ventoy_debug_pause();
}

static void EFIAPI ventoy_dump_override_chunk(ventoy_chain_head *chain)
{
    UINT32 i;
    ventoy_override_chunk *chunk;
    
    chunk = (ventoy_override_chunk *)((char *)chain + chain->override_chunk_offset);

    debug("##################### ventoy_dump_override_chunk #######################");

    for (i = 0; i < g_override_chunk_num; i++)
    {
        debug("%2u: [ %llu, %u ]", i, chunk[i].img_offset, chunk[i].override_size);
    }

    ventoy_debug_pause();
}

static void EFIAPI ventoy_dump_virt_chunk(ventoy_chain_head *chain)
{
    UINT32 i;
    ventoy_virt_chunk *node;
     
    debug("##################### ventoy_dump_virt_chunk #######################");
    debug("virt_chunk_offset=%u", chain->virt_chunk_offset);
    debug("virt_chunk_num=%u",    chain->virt_chunk_num);

    node = (ventoy_virt_chunk *)((char *)chain + chain->virt_chunk_offset);
    for (i = 0; i < chain->virt_chunk_num; i++, node++)
    {
        debug("%2u: mem:[ %u, %u, %u ]  remap:[ %u, %u, %u ]", i, 
               node->mem_sector_start,
               node->mem_sector_end,
               node->mem_sector_offset,
               node->remap_sector_start,
               node->remap_sector_end,
               node->org_sector_start);
    }
    
    ventoy_debug_pause();
}

static void EFIAPI ventoy_dump_chain(ventoy_chain_head *chain)
{
    UINT32 i = 0;
    UINT8 chksum = 0;
    UINT8 *guid;
    
    guid = chain->os_param.vtoy_disk_guid;
    for (i = 0; i < sizeof(ventoy_os_param); i++)
    {
        chksum += *((UINT8 *)(&(chain->os_param)) + i);
    }

    debug("##################### ventoy_dump_chain #######################");

    debug("os_param->chksum=0x%x (%a)", chain->os_param.chksum, chksum ? "FAILED" : "SUCCESS");
    debug("os_param->vtoy_disk_guid=%02x%02x%02x%02x", guid[0], guid[1], guid[2], guid[3]);
    debug("os_param->vtoy_disk_size=%llu",     chain->os_param.vtoy_disk_size);
    debug("os_param->vtoy_disk_part_id=%u",    chain->os_param.vtoy_disk_part_id);
    debug("os_param->vtoy_disk_part_type=%u",  chain->os_param.vtoy_disk_part_type);
    debug("os_param->vtoy_img_path=<%a>",      chain->os_param.vtoy_img_path);
    debug("os_param->vtoy_img_size=<%llu>",    chain->os_param.vtoy_img_size);
    debug("os_param->vtoy_img_location_addr=<0x%llx>", chain->os_param.vtoy_img_location_addr);
    debug("os_param->vtoy_img_location_len=<%u>",    chain->os_param.vtoy_img_location_len);
    debug("os_param->vtoy_reserved=<%u %u %u %u %u %u %u>",    
          g_os_param_reserved[0], 
          g_os_param_reserved[1], 
          g_os_param_reserved[2], 
          g_os_param_reserved[3],
          g_os_param_reserved[4],
          g_os_param_reserved[5],
          g_os_param_reserved[6]
          );

    ventoy_debug_pause();
    
    debug("chain->disk_drive=0x%x",          chain->disk_drive);
    debug("chain->disk_sector_size=%u",      chain->disk_sector_size);
    debug("chain->real_img_size_in_bytes=%llu",   chain->real_img_size_in_bytes);
    debug("chain->virt_img_size_in_bytes=%llu", chain->virt_img_size_in_bytes);
    debug("chain->boot_catalog=%u",          chain->boot_catalog);
    debug("chain->img_chunk_offset=%u",      chain->img_chunk_offset);
    debug("chain->img_chunk_num=%u",         chain->img_chunk_num);
    debug("chain->override_chunk_offset=%u", chain->override_chunk_offset);
    debug("chain->override_chunk_num=%u",    chain->override_chunk_num);

    ventoy_debug_pause();
    
    ventoy_dump_img_chunk(chain);
    ventoy_dump_override_chunk(chain);
    ventoy_dump_virt_chunk(chain);
}

static int ventoy_update_image_location(ventoy_os_param *param)
{
    EFI_STATUS Status = EFI_SUCCESS;
    UINT8 chksum = 0;
    unsigned int i;
    unsigned int length;
    UINTN address = 0;
    void *buffer = NULL;
    ventoy_image_location *location = NULL;
    ventoy_image_disk_region *region = NULL;
    ventoy_img_chunk *chunk = g_chunk;

    length = sizeof(ventoy_image_location) + (g_img_chunk_num - 1) * sizeof(ventoy_image_disk_region);

    Status = gBS->AllocatePool(EfiRuntimeServicesData, length + 4096 * 2, &buffer);
    if (EFI_ERROR(Status) || NULL == buffer)
    {
        debug("Failed to allocate runtime pool %r\n", Status);
        return 1;
    }

    address = (UINTN)buffer;
    g_vtoy_img_location_buf = buffer;

    if (address % 4096)
    {
        address += 4096 - (address % 4096);
    }

    param->chksum = 0;
    param->vtoy_img_location_addr = address;
    param->vtoy_img_location_len = length;

    /* update check sum */
    for (i = 0; i < sizeof(ventoy_os_param); i++)
    {
        chksum += *((UINT8 *)param + i);
    }
    param->chksum = (chksum == 0) ? 0 : (UINT8)(0x100 - chksum);

    location = (ventoy_image_location *)(unsigned long)(param->vtoy_img_location_addr);
    if (NULL == location)
    {
        return 0;
    }

    CopyMem(&location->guid, &param->guid, sizeof(ventoy_guid));
    location->image_sector_size = gSector512Mode ? 512 : 2048;
    location->disk_sector_size  = g_chain->disk_sector_size;
    location->region_count = g_img_chunk_num;

    region = location->regions;

    if (gSector512Mode)
    {
        for (i = 0; i < g_img_chunk_num; i++)
        {
            region->image_sector_count = chunk->disk_end_sector - chunk->disk_start_sector + 1;
            region->image_start_sector = chunk->img_start_sector * 4;
            region->disk_start_sector  = chunk->disk_start_sector;
            region++;
            chunk++;
        }
    }
    else
    {
        for (i = 0; i < g_img_chunk_num; i++)
        {
            region->image_sector_count = chunk->img_end_sector - chunk->img_start_sector + 1;
            region->image_start_sector = chunk->img_start_sector;
            region->disk_start_sector  = chunk->disk_start_sector;
            region++;
            chunk++;
        }
    }

    return 0;
}

EFI_HANDLE EFIAPI ventoy_get_parent_handle(IN EFI_DEVICE_PATH_PROTOCOL *pDevPath)
{
    EFI_HANDLE Handle = NULL;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_DEVICE_PATH_PROTOCOL *pLastNode = NULL;
    EFI_DEVICE_PATH_PROTOCOL *pCurNode = NULL;
    EFI_DEVICE_PATH_PROTOCOL *pTmpDevPath = NULL;
    
    pTmpDevPath = DuplicateDevicePath(pDevPath);
    if (!pTmpDevPath)
    {
        return NULL;
    }

    pCurNode = pTmpDevPath;
    while (!IsDevicePathEnd(pCurNode))
    {
        pLastNode = pCurNode;
        pCurNode = NextDevicePathNode(pCurNode);
    }
    if (pLastNode)
    {
        CopyMem(pLastNode, pCurNode, sizeof(EFI_DEVICE_PATH_PROTOCOL));
    }

    pCurNode = pTmpDevPath;
    Status = gBS->LocateDevicePath(&gEfiDevicePathProtocolGuid, &pCurNode, &Handle);
    debug("Status:%r Parent Handle:%p DP:%s", Status, Handle, ConvertDevicePathToText(pTmpDevPath, FALSE, FALSE));

    FreePool(pTmpDevPath);

    return Handle;
}

STATIC ventoy_ram_disk g_backup_ramdisk_param;
STATIC ventoy_os_param g_backup_os_param_var;


EFI_STATUS EFIAPI ventoy_save_ramdisk_param(VOID)
{
    UINTN DataSize;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_GUID VarGuid = VENTOY_GUID;

    DataSize = sizeof(g_backup_ramdisk_param);
    Status = gRT->GetVariable(L"VentoyRamDisk", &VarGuid, NULL, &DataSize, &g_backup_ramdisk_param);
    if (!EFI_ERROR(Status))
    {
        debug("find previous ramdisk variable <%llu>", g_backup_ramdisk_param.DiskSize);
    }
    
    Status = gRT->SetVariable(L"VentoyRamDisk", &VarGuid, 
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof(g_ramdisk_param), &(g_ramdisk_param));
    debug("set ramdisk variable %r", Status);

    return Status;
}

EFI_STATUS EFIAPI ventoy_delete_ramdisk_param(VOID)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_GUID VarGuid = VENTOY_GUID;

    if (g_backup_ramdisk_param.DiskSize > 0 && g_backup_ramdisk_param.PhyAddr > 0)
    {
        Status = gRT->SetVariable(L"VentoyRamDisk", &VarGuid, 
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof(g_backup_ramdisk_param), &g_backup_ramdisk_param);
        debug("resotre ramdisk variable %r", Status);
    }
    else
    {
        Status = gRT->SetVariable(L"VentoyRamDisk", &VarGuid, 
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  0, NULL);
        debug("delete ramdisk variable %r", Status);
    }

    return Status;
}

EFI_STATUS EFIAPI ventoy_save_variable(VOID)
{
    UINTN DataSize;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_GUID VarGuid = VENTOY_GUID;

    DataSize = sizeof(g_backup_os_param_var);
    Status = gRT->GetVariable(L"VentoyOsParam", &VarGuid, NULL, &DataSize, &g_backup_os_param_var);
    if (!EFI_ERROR(Status))
    {
        debug("find previous efi variable <%a>", g_backup_os_param_var.vtoy_img_path);
    }

    Status = gRT->SetVariable(L"VentoyOsParam", &VarGuid, 
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof(g_chain->os_param), &(g_chain->os_param));
    debug("set efi variable %r", Status);

    return Status;
}

EFI_STATUS EFIAPI ventoy_delete_variable(VOID)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_GUID VarGuid = VENTOY_GUID;

    if (0 == CompareMem(&(g_backup_os_param_var.guid), &VarGuid, sizeof(EFI_GUID)))
    {
        Status = gRT->SetVariable(L"VentoyOsParam", &VarGuid, 
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof(g_backup_os_param_var), &(g_backup_os_param_var));
        debug("restore efi variable %r", Status);
    }
    else
    {
        Status = gRT->SetVariable(L"VentoyOsParam", &VarGuid, 
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  0, NULL);
        debug("delete efi variable %r", Status);
    }

    return Status;
}

#if (VENTOY_DEVICE_WARN != 0)
STATIC VOID ventoy_warn_invalid_device(VOID)
{
    STATIC BOOLEAN flag = FALSE;

    if (flag)
    {
        return;
    }

    flag = TRUE;
    gST->ConOut->ClearScreen(gST->ConOut);
    gST->ConOut->OutputString(gST->ConOut, VTOY_WARNING L"\r\n");
    gST->ConOut->OutputString(gST->ConOut, VTOY_WARNING L"\r\n");
    gST->ConOut->OutputString(gST->ConOut, VTOY_WARNING L"\r\n\r\n\r\n");

    gST->ConOut->OutputString(gST->ConOut, L"This is NOT a standard Ventoy device and is NOT supported.\r\n\r\n");
    gST->ConOut->OutputString(gST->ConOut, L"You should follow the official instructions in https://www.ventoy.net\r\n");
    
    gST->ConOut->OutputString(gST->ConOut, L"\r\n\r\nWill exit after 10 seconds ...... ");

    sleep(10);
}
#else
STATIC VOID ventoy_warn_invalid_device(VOID)
{
    
}
#endif

STATIC EFI_STATUS EFIAPI ventoy_load_image
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


STATIC EFI_STATUS EFIAPI ventoy_find_iso_disk(IN EFI_HANDLE ImageHandle)
{
    UINTN i = 0;
    UINTN Count = 0;
    UINT64 DiskSize = 0;
    MBR_HEAD *pMBR = NULL;
    UINT8 *pBuffer = NULL;
    EFI_HANDLE *Handles;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_BLOCK_IO_PROTOCOL *pBlockIo;

    pBuffer = AllocatePool(2048);
    if (!pBuffer)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiBlockIoProtocolGuid, 
                                     NULL, &Count, &Handles);
    if (EFI_ERROR(Status))
    {
        FreePool(pBuffer);
        return Status;
    }

    for (i = 0; i < Count; i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiBlockIoProtocolGuid, (VOID **)&pBlockIo);
        if (EFI_ERROR(Status))
        {
            continue;
        }

        DiskSize = (pBlockIo->Media->LastBlock + 1) * pBlockIo->Media->BlockSize;
        debug("This Disk size: %llu", DiskSize);
        if (g_chain->os_param.vtoy_disk_size != DiskSize)
        {
            continue;
        }

        Status = pBlockIo->ReadBlocks(pBlockIo, pBlockIo->Media->MediaId, 0, 512, pBuffer);
        if (EFI_ERROR(Status))
        {
            debug("ReadBlocks filed %r", Status);
            continue;
        }

        if (CompareMem(g_chain->os_param.vtoy_disk_guid, pBuffer + 0x180, 16) == 0)
        {
            pMBR = (MBR_HEAD *)pBuffer;
            if (g_os_param_reserved[6] == 0 && pMBR->PartTbl[0].FsFlag != 0xEE)
            {
                if (pMBR->PartTbl[0].StartSectorId != 2048 ||
                    pMBR->PartTbl[1].SectorCount != 65536 ||
                    pMBR->PartTbl[1].StartSectorId != pMBR->PartTbl[0].StartSectorId + pMBR->PartTbl[0].SectorCount)
                {
                    debug("Failed to check disk part table");
                    ventoy_warn_invalid_device();
                }
            }
        
            gBlockData.RawBlockIoHandle = Handles[i];
            gBlockData.pRawBlockIo = pBlockIo;
            gBS->OpenProtocol(Handles[i], &gEfiDevicePathProtocolGuid, 
                              (VOID **)&(gBlockData.pDiskDevPath),
                              ImageHandle,
                              Handles[i],
                              EFI_OPEN_PROTOCOL_GET_PROTOCOL);
            
            debug("Find Ventoy Disk Handle:%p DP:%s", Handles[i], 
                ConvertDevicePathToText(gBlockData.pDiskDevPath, FALSE, FALSE));
            break;
        }
    }

    FreePool(Handles);

    if (i >= Count)
    {
        return EFI_NOT_FOUND;
    }
    else
    {
        return EFI_SUCCESS;
    }
}


STATIC EFI_STATUS EFIAPI ventoy_find_iso_disk_fs(IN EFI_HANDLE ImageHandle)
{
    UINTN i = 0;
    UINTN Count = 0;
    EFI_HANDLE Parent = NULL;
    EFI_HANDLE *Handles = NULL;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *pFile = NULL;
    EFI_DEVICE_PATH_PROTOCOL *pDevPath = NULL;

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, 
                                     NULL, &Count, &Handles);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    debug("ventoy_find_iso_disk_fs fs count:%u", Count);

    for (i = 0; i < Count; i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&pFile);
        if (EFI_ERROR(Status))
        {
            continue;
        }

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
        Parent = ventoy_get_parent_handle(pDevPath);

        if (Parent == gBlockData.RawBlockIoHandle)
        {
            debug("Find ventoy disk fs");
            gBlockData.DiskFsHandle = Handles[i];
            gBlockData.pDiskFs = pFile;
            gBlockData.pDiskFsDevPath = pDevPath;
            break;
        }
    }

    FreePool(Handles);

    return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI ventoy_load_isoefi_driver(IN EFI_HANDLE ImageHandle)
{
    EFI_HANDLE Image = NULL;
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR16 LogVar[4] = L"5";

    if (gIsoUdf)
    {
        Status = ventoy_load_image(ImageHandle, gBlockData.pDiskFsDevPath, 
                                   gUdfEfiDriverPath, 
                                   sizeof(gUdfEfiDriverPath), 
                                   &Image);
        debug("load iso UDF efi driver status:%r", Status);
    }
    else
    {
        Status = ventoy_load_image(ImageHandle, gBlockData.pDiskFsDevPath, 
                                   gIso9660EfiDriverPath, 
                                   sizeof(gIso9660EfiDriverPath), 
                                   &Image);
        debug("load iso 9660 efi driver status:%r", Status);        
    }

    if (gDebugPrint)
    {
        gRT->SetVariable(L"FS_LOGGING", &gShellVariableGuid, 
                         EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                         sizeof(LogVar), LogVar);
    }

    gRT->SetVariable(L"FS_NAME_NOCASE", &gShellVariableGuid, 
                     EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                     sizeof(LogVar), LogVar);

    gBlockData.IsoDriverImage = Image;
    Status = gBS->StartImage(Image, NULL, NULL);
    debug("Start iso efi driver status:%r", Status);

    return EFI_SUCCESS;
}

STATIC EFI_STATUS ventoy_proc_img_replace_name(ventoy_grub_param_file_replace *replace)
{
    UINT32 i;
    char tmp[256];

    if (replace->magic != GRUB_IMG_REPLACE_MAGIC)
    {
        return EFI_SUCCESS;
    }

    if (replace->old_file_name[0][0] == 0)
    {
        return EFI_SUCCESS;
    }

    AsciiStrCpyS(tmp, sizeof(tmp), replace->old_file_name[0]);
    
    for (i = 0; i < 256 && tmp[i]; i++)
    {
        if (tmp[i] == '/')
        {
            tmp[i] = '\\';
        }
    }

    AsciiStrCpyS(replace->old_file_name[0], 256, tmp);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI ventoy_get_variable_wrapper
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

EFI_STATUS EFIAPI ventoy_exit_boot_service_wrapper
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

STATIC EFI_STATUS EFIAPI ventoy_disable_secure_boot(IN EFI_HANDLE ImageHandle)
{
    UINT8 Value = 0;
    UINTN DataSize = 1;
    EFI_STATUS Status = EFI_SUCCESS;

    Status = gRT->GetVariable(L"SecureBoot", &gEfiGlobalVariableGuid, NULL, &DataSize, &Value);
    if (!EFI_ERROR(Status))
    {
        if (DataSize == 1 && Value == 0)
        {
            debug("Current secure boot is off, no need to disable");
            return EFI_SUCCESS;
        }
    }

    debug("ventoy_disable_secure_boot");

    /* step1: wrapper security protocol. */
    /* Do we still need it since we have been loaded ? */
    
    
    /* step2: fake SecureBoot variable */
    g_org_exit_boot_service = gBS->ExitBootServices;
    gBS->ExitBootServices = ventoy_exit_boot_service_wrapper;
    
    g_org_get_variable = gRT->GetVariable;
    gRT->GetVariable = ventoy_get_variable_wrapper;

    return EFI_SUCCESS;
}


STATIC EFI_STATUS EFIAPI ventoy_parse_cmdline(IN EFI_HANDLE ImageHandle)
{   
    UINT32 i = 0;
    UINT32 old_cnt = 0;
    UINTN size = 0;
    UINT8 chksum = 0;
    const char *pEnv = NULL;
    CHAR16 *pPos = NULL;
    CHAR16 *pCmdLine = NULL;
    EFI_STATUS Status = EFI_SUCCESS;
    ventoy_grub_param *pGrubParam = NULL;
    EFI_LOADED_IMAGE_PROTOCOL *pImageInfo = NULL;
    ventoy_chain_head *chain = NULL;

    Status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&pImageInfo);
    if (EFI_ERROR(Status))
    {
        VtoyDebug("Failed to handle load image protocol %r", Status);
        return Status;
    }

    pCmdLine = (CHAR16 *)AllocatePool(pImageInfo->LoadOptionsSize + 4);
    SetMem(pCmdLine, pImageInfo->LoadOptionsSize + 4, 0);
    CopyMem(pCmdLine, pImageInfo->LoadOptions, pImageInfo->LoadOptionsSize);

    if (StrStr(pCmdLine, L"debug"))
    {
        gDebugPrint = TRUE;
    }
    
    if (StrStr(pCmdLine, L"fallback"))
    {
        gBootFallBack = TRUE;
    }
    
    if (StrStr(pCmdLine, L"dotefi"))
    {
        gDotEfiBoot = TRUE;
    }

    if (StrStr(pCmdLine, L"isoefi=on"))
    {
        gLoadIsoEfi = TRUE;
    }
    
    if (StrStr(pCmdLine, L"iso_udf"))
    {
        gIsoUdf = TRUE;
    }

    pPos = StrStr(pCmdLine, L"FirstTry=@");
    if (pPos)
    {
        pPos += StrLen(L"FirstTry=");
        for (i = 0; i < ARRAY_SIZE(gFirstTryBootFile); i++, pPos++)
        {
            if (*pPos != L' ' && *pPos != L'\t' && *pPos)
            {
                gFirstTryBootFile[i] = (*pPos == '@') ? '\\' : *pPos;
            }
            else
            {
                break;
            }
        }

        gEfiBootFileName[0] = gFirstTryBootFile;
        gBootFileStartIndex = 0;
    }

    debug("cmdline:<%s>", pCmdLine);

    if (gFirstTryBootFile[0])
    {
        debug("First Try:<%s>", gFirstTryBootFile);
    }

    pPos = StrStr(pCmdLine, L"env_param=");
    if (!pPos)
    {
        return EFI_INVALID_PARAMETER;
    }
    
    pGrubParam = (ventoy_grub_param *)StrHexToUintn(pPos + StrLen(L"env_param="));
    grub_env_set = pGrubParam->grub_env_set;
    grub_env_get = pGrubParam->grub_env_get;
    pEnv = grub_env_get("VTOY_CHKDEV_RESULT_STRING");
    if (!pEnv)
    {
        return EFI_INVALID_PARAMETER;
    }

    if (pEnv[0] != '0' || pEnv[1] != 0)
    {
        ventoy_warn_invalid_device();
        return EFI_INVALID_PARAMETER;
    }
    
    g_file_replace_list = &pGrubParam->file_replace;
    old_cnt = g_file_replace_list->old_file_cnt;
    debug("file replace: magic:0x%x virtid:%u name count:%u <%a> <%a> <%a> <%a>",
        g_file_replace_list->magic,
        g_file_replace_list->new_file_virtual_id,
        old_cnt,
        old_cnt > 0 ? g_file_replace_list->old_file_name[0] : "",
        old_cnt > 1 ? g_file_replace_list->old_file_name[1] : "",
        old_cnt > 2 ? g_file_replace_list->old_file_name[2] : "",
        old_cnt > 3 ? g_file_replace_list->old_file_name[3] : ""
        );

    g_img_replace_list = &pGrubParam->img_replace;
    ventoy_proc_img_replace_name(g_img_replace_list);
    old_cnt = g_img_replace_list->old_file_cnt;
    debug("img replace: magic:0x%x virtid:%u name count:%u <%a> <%a> <%a> <%a>",
        g_img_replace_list->magic,
        g_img_replace_list->new_file_virtual_id,
        old_cnt,
        old_cnt > 0 ? g_img_replace_list->old_file_name[0] : "",
        old_cnt > 1 ? g_img_replace_list->old_file_name[1] : "",
        old_cnt > 2 ? g_img_replace_list->old_file_name[2] : "",
        old_cnt > 3 ? g_img_replace_list->old_file_name[3] : ""
        );
    
    pPos = StrStr(pCmdLine, L"mem:");
    chain = (ventoy_chain_head *)StrHexToUintn(pPos + 4);

    pPos = StrStr(pPos, L"size:");
    size = StrDecimalToUintn(pPos + 5);

    debug("memory addr:%p size:%lu", chain, size);

    if (StrStr(pCmdLine, L"sector512"))
    {
        gSector512Mode = TRUE;
    }

    if (StrStr(pCmdLine, L"memdisk"))
    {
        g_iso_data_buf = (UINT8 *)chain + sizeof(ventoy_chain_head);
        g_iso_buf_size = size - sizeof(ventoy_chain_head);
        debug("memdisk mode iso_buf_size:%u", g_iso_buf_size);

        g_chain = chain;
        g_os_param_reserved = (UINT8 *)(g_chain->os_param.vtoy_reserved);
        gMemdiskMode = TRUE;
    }
    else
    {
        debug("This is normal mode");
        g_chain = AllocatePool(size);
        CopyMem(g_chain, chain, size);
    
        g_chunk = (ventoy_img_chunk *)((char *)g_chain + g_chain->img_chunk_offset);
        g_img_chunk_num = g_chain->img_chunk_num;
        g_override_chunk = (ventoy_override_chunk *)((char *)g_chain + g_chain->override_chunk_offset);
        g_override_chunk_num = g_chain->override_chunk_num;
        g_virt_chunk = (ventoy_virt_chunk *)((char *)g_chain + g_chain->virt_chunk_offset);
        g_virt_chunk_num = g_chain->virt_chunk_num;

        g_os_param_reserved = (UINT8 *)(g_chain->os_param.vtoy_reserved);

        /* Workaround for Windows & ISO9660 */
        if (g_os_param_reserved[2] == ventoy_chain_windows && g_os_param_reserved[3] == 0)
        {
            g_fixup_iso9660_secover_enable = TRUE;
        }

        if (g_os_param_reserved[2] == ventoy_chain_windows && g_os_param_reserved[4] != 1)
        {
            g_hook_keyboard = TRUE;
        }
        
        if (g_os_param_reserved[5] == 1 && g_os_param_reserved[2] == ventoy_chain_linux)
        {
            ventoy_disable_secure_boot(ImageHandle);
        }

        debug("internal param: secover:%u keyboard:%u", g_fixup_iso9660_secover_enable, g_hook_keyboard);

        for (i = 0; i < sizeof(ventoy_os_param); i++)
        {
            chksum += *((UINT8 *)(&(g_chain->os_param)) + i);
        }

        if (gDebugPrint)
        {
            debug("os param checksum: 0x%x %a", g_chain->os_param.chksum, chksum ? "FAILED" : "SUCCESS");
        }

        ventoy_update_image_location(&(g_chain->os_param));

        if (gDebugPrint)
        {
            ventoy_dump_chain(g_chain);
        }
    }

    g_fix_windows_1st_cdrom_issue = FALSE;
    if (ventoy_chain_windows == g_os_param_reserved[2] || 
        ventoy_chain_wim == g_os_param_reserved[2])
    {
        if (ventoy_is_cdrom_dp_exist())
        {
            debug("fixup the 1st cdrom influences when boot windows ...");
            g_fix_windows_1st_cdrom_issue = TRUE;
        }
    }

    ventoy_debug_pause();

    FreePool(pCmdLine);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI ventoy_clean_env(VOID)
{
    FreePool(g_sector_flag);
    g_sector_flag_num = 0;

    if (gLoadIsoEfi && gBlockData.IsoDriverImage)
    {
        gBS->UnloadImage(gBlockData.IsoDriverImage);
    }

    gBS->DisconnectController(gBlockData.Handle, NULL, NULL);

    gBS->UninstallMultipleProtocolInterfaces(gBlockData.Handle,
            &gEfiBlockIoProtocolGuid, &gBlockData.BlockIo,
            &gEfiDevicePathProtocolGuid, gBlockData.Path,
            NULL);

    ventoy_delete_variable();

    if (g_vtoy_img_location_buf)
    {
        FreePool(g_vtoy_img_location_buf);
    }

    if (!gMemdiskMode)
    {
        FreePool(g_chain);        
    }

    return EFI_SUCCESS;
}

STATIC EFI_STATUS ventoy_hook_start(VOID)
{
    /* don't add debug print in this function */

    if (g_fix_windows_1st_cdrom_issue)
    {
        ventoy_hook_1st_cdrom_start();
    }

    /* let this the last */
    if (g_hook_keyboard)
    {
        ventoy_hook_keyboard_start();
    }

    return EFI_SUCCESS;
}

STATIC EFI_STATUS ventoy_hook_stop(VOID)
{
    /* don't add debug print in this function */

    if (g_fix_windows_1st_cdrom_issue)
    {
        ventoy_hook_1st_cdrom_stop();
    }

    /* let this the last */
    if (g_hook_keyboard)
    {
        ventoy_hook_keyboard_stop();
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI ventoy_boot(IN EFI_HANDLE ImageHandle)
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

        debug("ventoy_boot fs count:%u", Count);

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
            if (CompareMem(gBlockData.Path, pDevPath, gBlockData.DevicePathCompareLen))
            {
                debug("Not ventoy disk file system");
                continue;
            }

            for (j = gBootFileStartIndex; j < ARRAY_SIZE(gEfiBootFileName); j++)
            {
                Status = ventoy_load_image(ImageHandle, pDevPath, gEfiBootFileName[j], 
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
            ventoy_debug_pause();

            if (gDebugPrint)
            {
                gST->ConIn->Reset(gST->ConIn, FALSE);
            }
            
            if ((g_file_replace_list && g_file_replace_list->magic == GRUB_FILE_REPLACE_MAGIC) ||
                (g_img_replace_list && g_img_replace_list->magic == GRUB_IMG_REPLACE_MAGIC))
            {
                ventoy_wrapper_push_openvolume(pFile->OpenVolume);
                pFile->OpenVolume = ventoy_wrapper_open_volume;
            }

            ventoy_hook_start();
            /* can't add debug print here */
            //ventoy_wrapper_system();
            Status = gBS->StartImage(Image, NULL, NULL);
            ventoy_hook_stop();
            
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
            if (gDotEfiBoot)
            {
                break;
            }
        
            debug("Fs not found, now wait and retry...");
            sleep(1);
        }
    }

    if (Find == 0)
    {
        return EFI_NOT_FOUND;
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI VentoyEfiMain
(
    IN EFI_HANDLE         ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *Protocol;
    
    g_sector_flag_num = 512; /* initial value */

    g_sector_flag = AllocatePool(g_sector_flag_num * sizeof(ventoy_sector_flag));
    if (NULL == g_sector_flag)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    Status = gBS->HandleProtocol(gST->ConsoleInHandle, &gEfiSimpleTextInputExProtocolGuid, (VOID **)&Protocol);
    if (EFI_SUCCESS == Status)
    {
        g_con_simple_input_ex = Protocol;
    }

    gST->ConOut->ClearScreen(gST->ConOut);
    ventoy_clear_input();

    Status = ventoy_parse_cmdline(ImageHandle);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    ventoy_disable_ex_filesystem();

    if (gMemdiskMode)
    {
        g_ramdisk_param.PhyAddr = (UINT64)(UINTN)g_iso_data_buf;
        g_ramdisk_param.DiskSize = (UINT64)g_iso_buf_size;

        ventoy_save_ramdisk_param();

        if (gLoadIsoEfi)
        {
            ventoy_find_iso_disk(ImageHandle);
            ventoy_find_iso_disk_fs(ImageHandle);
            ventoy_load_isoefi_driver(ImageHandle);
        }
        
        ventoy_install_blockio(ImageHandle, g_iso_buf_size);
        ventoy_debug_pause();

        Status = ventoy_boot(ImageHandle);
        
        ventoy_delete_ramdisk_param();

        if (gLoadIsoEfi && gBlockData.IsoDriverImage)
        {
            gBS->UnloadImage(gBlockData.IsoDriverImage);
        }

        gBS->DisconnectController(gBlockData.Handle, NULL, NULL);
        gBS->UninstallMultipleProtocolInterfaces(gBlockData.Handle,
                &gEfiBlockIoProtocolGuid, &gBlockData.BlockIo,
                &gEfiDevicePathProtocolGuid, gBlockData.Path,
                NULL);
    }
    else
    {
        ventoy_save_variable();
        Status = ventoy_find_iso_disk(ImageHandle);
        if (!EFI_ERROR(Status))
        {
            if (gLoadIsoEfi)
            {
                ventoy_find_iso_disk_fs(ImageHandle);
                ventoy_load_isoefi_driver(ImageHandle);
            }

            ventoy_debug_pause();
            
            ventoy_install_blockio(ImageHandle, g_chain->virt_img_size_in_bytes);

            ventoy_debug_pause();

            Status = ventoy_boot(ImageHandle);
        }
        
        ventoy_clean_env();
    }

    if (FALSE == gDotEfiBoot && FALSE == gBootFallBack)
    {
        if (EFI_NOT_FOUND == Status)
        {
            gST->ConOut->OutputString(gST->ConOut, L"No bootfile found for UEFI!\r\n");
            gST->ConOut->OutputString(gST->ConOut, L"Maybe the image does not support " VENTOY_UEFI_DESC  L"!\r\n");
            sleep(30);
        }
    }
    
    ventoy_clear_input();
    gST->ConOut->ClearScreen(gST->ConOut);

    if (gDotEfiBoot && (EFI_NOT_FOUND == Status))
    {
        grub_env_set("vtoy_dotefi_retry", "YES");            
    }

    ventoy_enable_ex_filesystem();

    return EFI_SUCCESS;
}

