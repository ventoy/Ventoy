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

UINTN g_iso_buf_size = 0;
BOOLEAN gMemdiskMode = FALSE;
BOOLEAN gDebugPrint = FALSE;
BOOLEAN gLoadIsoEfi = FALSE;
ventoy_ram_disk g_ramdisk_param;
ventoy_chain_head *g_chain;
ventoy_img_chunk *g_chunk;
UINT32 g_img_chunk_num;
ventoy_override_chunk *g_override_chunk;
UINT32 g_override_chunk_num;
ventoy_virt_chunk *g_virt_chunk;
UINT32 g_virt_chunk_num;
vtoy_block_data gBlockData;
ventoy_sector_flag *g_sector_flag = NULL;
UINT32 g_sector_flag_num = 0;
static grub_env_get_pf grub_env_get = NULL;

EFI_FILE_OPEN g_original_fopen = NULL;
EFI_FILE_CLOSE g_original_fclose = NULL;
EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME g_original_open_volume = NULL;

ventoy_grub_param_file_replace *g_file_replace_list = NULL;
ventoy_efi_file_replace g_efi_file_replace;

CHAR16 gFirstTryBootFile[256] = {0};

CONST CHAR16 gIso9660EfiDriverPath[] = ISO9660_EFI_DRIVER_PATH;

/* Boot filename */
UINTN gBootFileStartIndex = 1;
CONST CHAR16 *gEfiBootFileName[] = 
{
    L"@",
    EFI_REMOVABLE_MEDIA_FILE_NAME,
    L"\\EFI\\BOOT\\GRUBX64.EFI",
    L"\\EFI\\BOOT\\BOOTx64.EFI",
    L"\\EFI\\BOOT\\bootx64.efi",
    L"\\efi\\boot\\bootx64.efi",
};

/* EFI block device vendor device path GUID */
EFI_GUID gVtoyBlockDevicePathGuid = VTOY_BLOCK_DEVICE_PATH_GUID;

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

EFI_STATUS EFIAPI ventoy_block_io_reset 
(
    IN EFI_BLOCK_IO_PROTOCOL          *This,
    IN BOOLEAN                        ExtendedVerification
) 
{
    (VOID)This;
    (VOID)ExtendedVerification;
	return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI ventoy_read_iso_sector
(
    IN UINT64                 Sector,
    IN UINTN                  Count,
    OUT VOID                 *Buffer
)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_LBA MapLba = 0;
    UINT32 i = 0;
    UINTN secLeft = 0;
    UINTN secRead = 0;
    UINT64 ReadStart = 0;
    UINT64 ReadEnd = 0;
    UINT64 OverrideStart = 0;
    UINT64 OverrideEnd= 0;
    UINT8 *pCurBuf = (UINT8 *)Buffer;
    ventoy_img_chunk *pchunk = g_chunk;
    ventoy_override_chunk *pOverride = g_override_chunk;
    EFI_BLOCK_IO_PROTOCOL *pRawBlockIo = gBlockData.pRawBlockIo;

    debug("read iso sector %lu  count %u", Sector, Count);

    ReadStart = Sector * 2048;
    ReadEnd = (Sector + Count) * 2048;

    for (i = 0; Count > 0 && i < g_img_chunk_num; i++, pchunk++)
    {
        if (Sector >= pchunk->img_start_sector && Sector <= pchunk->img_end_sector)
        {
            if (g_chain->disk_sector_size == 512)
            {
                MapLba = (Sector - pchunk->img_start_sector) * 4 + pchunk->disk_start_sector;
            }
            else
            {
                MapLba = (Sector - pchunk->img_start_sector) * 2048 / g_chain->disk_sector_size + pchunk->disk_start_sector;
            }

            secLeft = pchunk->img_end_sector + 1 - Sector;
            secRead = (Count < secLeft) ? Count : secLeft;

            Status = pRawBlockIo->ReadBlocks(pRawBlockIo, pRawBlockIo->Media->MediaId,
                                     MapLba, secRead * 2048, pCurBuf);
            if (EFI_ERROR(Status))
            {
                debug("Raw disk read block failed %r", Status);
                return Status;
            }

            Count -= secRead;
            Sector += secRead;
            pCurBuf += secRead * 2048;
        }
    }

    if (ReadStart > g_chain->real_img_size_in_bytes)
    {
        return EFI_SUCCESS;
    }

    /* override data */
    pCurBuf = (UINT8 *)Buffer;
    for (i = 0; i < g_override_chunk_num; i++, pOverride++)
    {
        OverrideStart = pOverride->img_offset;
        OverrideEnd = pOverride->img_offset + pOverride->override_size;
    
        if (OverrideStart >= ReadEnd || ReadStart >= OverrideEnd)
        {
            continue;
        }

        if (ReadStart <= OverrideStart)
        {
            if (ReadEnd <= OverrideEnd)
            {
                CopyMem(pCurBuf + OverrideStart - ReadStart, pOverride->override_data, ReadEnd - OverrideStart);  
            }
            else
            {
                CopyMem(pCurBuf + OverrideStart - ReadStart, pOverride->override_data, pOverride->override_size);
            }
        }
        else
        {
            if (ReadEnd <= OverrideEnd)
            {
                CopyMem(pCurBuf, pOverride->override_data + ReadStart - OverrideStart, ReadEnd - ReadStart);     
            }
            else
            {
                CopyMem(pCurBuf, pOverride->override_data + ReadStart - OverrideStart, OverrideEnd - ReadStart);
            }
        }
    }

    return EFI_SUCCESS;    
}

EFI_STATUS EFIAPI ventoy_block_io_ramdisk_read 
(
    IN EFI_BLOCK_IO_PROTOCOL          *This,
    IN UINT32                          MediaId,
    IN EFI_LBA                         Lba,
    IN UINTN                           BufferSize,
    OUT VOID                          *Buffer
) 
{
    //debug("### ventoy_block_io_ramdisk_read sector:%u count:%u", (UINT32)Lba, (UINT32)BufferSize / 2048);

    (VOID)This;
    (VOID)MediaId;

    CopyMem(Buffer, (char *)g_chain + (Lba * 2048), BufferSize);

	return EFI_SUCCESS;
}

EFI_STATUS EFIAPI ventoy_block_io_read 
(
    IN EFI_BLOCK_IO_PROTOCOL          *This,
    IN UINT32                          MediaId,
    IN EFI_LBA                         Lba,
    IN UINTN                           BufferSize,
    OUT VOID                          *Buffer
) 
{
    UINT32 i = 0;
    UINT32 j = 0;
    UINT32 lbacount = 0;
    UINT32 secNum = 0;
    UINT64 offset = 0;
    EFI_LBA curlba = 0;
    EFI_LBA lastlba = 0;
    UINT8 *lastbuffer;
    ventoy_sector_flag *cur_flag;
    ventoy_virt_chunk *node;
    
    //debug("### ventoy_block_io_read sector:%u count:%u", (UINT32)Lba, (UINT32)BufferSize / 2048);

    secNum = BufferSize / 2048;
    offset = Lba * 2048;

    if (offset + BufferSize <= g_chain->real_img_size_in_bytes)
    {
        return ventoy_read_iso_sector(Lba, secNum, Buffer);
    }

    if (secNum > g_sector_flag_num)
    {
        cur_flag = AllocatePool(secNum * sizeof(ventoy_sector_flag));
        if (NULL == cur_flag)
        {
            return EFI_OUT_OF_RESOURCES;
        }

        FreePool(g_sector_flag);
        g_sector_flag = cur_flag;
        g_sector_flag_num = secNum;
    }

    for (curlba = Lba, cur_flag = g_sector_flag, j = 0; j < secNum; j++, curlba++, cur_flag++)
    {
        cur_flag->flag = 0;
        for (node = g_virt_chunk, i = 0; i < g_virt_chunk_num; i++, node++)
        {
            if (curlba >= node->mem_sector_start && curlba < node->mem_sector_end)
            {
                CopyMem((UINT8 *)Buffer + j * 2048, 
                       (char *)g_virt_chunk + node->mem_sector_offset + (curlba - node->mem_sector_start) * 2048,
                       2048);
                cur_flag->flag = 1;
                break;
            }
            else if (curlba >= node->remap_sector_start && curlba < node->remap_sector_end)
            {
                cur_flag->remap_lba = node->org_sector_start + curlba - node->remap_sector_start;
                cur_flag->flag = 2;
                break;
            }
        }
    }

    for (curlba = Lba, cur_flag = g_sector_flag, j = 0; j < secNum; j++, curlba++, cur_flag++)
    {
        if (cur_flag->flag == 2)
        {
            if (lastlba == 0)
            {
                lastbuffer = (UINT8 *)Buffer + j * 2048;
                lastlba = cur_flag->remap_lba;
                lbacount = 1;
            }
            else if (lastlba + lbacount == cur_flag->remap_lba)
            {
                lbacount++;
            }
            else
            {
                ventoy_read_iso_sector(lastlba, lbacount, lastbuffer);
                lastbuffer = (UINT8 *)Buffer + j * 2048;
                lastlba = cur_flag->remap_lba;
                lbacount = 1;
            }
        }
    }

    if (lbacount > 0)
    {
        ventoy_read_iso_sector(lastlba, lbacount, lastbuffer);
    }

	return EFI_SUCCESS;
}

EFI_STATUS EFIAPI ventoy_block_io_write 
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
    (VOID)Lba;
    (VOID)BufferSize;
    (VOID)Buffer;
	return EFI_WRITE_PROTECTED;
}

EFI_STATUS EFIAPI ventoy_block_io_flush(IN EFI_BLOCK_IO_PROTOCOL *This)
{
	(VOID)This;
	return EFI_SUCCESS;
}


EFI_STATUS EFIAPI ventoy_fill_device_path(VOID)
{
    UINTN NameLen = 0;
    UINT8 TmpBuf[128] = {0};
    VENDOR_DEVICE_PATH *venPath = NULL;

    venPath = (VENDOR_DEVICE_PATH *)TmpBuf;
    NameLen = StrSize(VTOY_BLOCK_DEVICE_PATH_NAME);
    venPath->Header.Type = HARDWARE_DEVICE_PATH;
    venPath->Header.SubType = HW_VENDOR_DP;
    venPath->Header.Length[0] = sizeof(VENDOR_DEVICE_PATH) + NameLen;
    venPath->Header.Length[1] = 0;
    CopyMem(&venPath->Guid, &gVtoyBlockDevicePathGuid, sizeof(EFI_GUID));
    CopyMem(venPath + 1, VTOY_BLOCK_DEVICE_PATH_NAME, NameLen);
    
    gBlockData.Path = AppendDevicePathNode(NULL, (EFI_DEVICE_PATH_PROTOCOL *)TmpBuf);
    gBlockData.DevicePathCompareLen = sizeof(VENDOR_DEVICE_PATH) + NameLen;

    debug("gBlockData.Path=<%s>\n", ConvertDevicePathToText(gBlockData.Path, FALSE, FALSE));

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI ventoy_save_ramdisk_param(VOID)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_GUID VarGuid = VENTOY_GUID;
    
    Status = gRT->SetVariable(L"VentoyRamDisk", &VarGuid, 
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof(g_ramdisk_param), &(g_ramdisk_param));
    debug("set efi variable %r", Status);

    return Status;
}

EFI_STATUS EFIAPI ventoy_del_ramdisk_param(VOID)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_GUID VarGuid = VENTOY_GUID;
    
    Status = gRT->SetVariable(L"VentoyRamDisk", &VarGuid, 
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  0, NULL);
    debug("delete efi variable %r", Status);

    return Status;
}


EFI_STATUS EFIAPI ventoy_set_variable(VOID)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_GUID VarGuid = VENTOY_GUID;
    
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
    
    Status = gRT->SetVariable(L"VentoyOsParam", &VarGuid, 
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  0, NULL);
    debug("delete efi variable %r", Status);

    return Status;
}


EFI_STATUS EFIAPI ventoy_install_blockio(IN EFI_HANDLE ImageHandle, IN UINT64 ImgSize)
{   
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_BLOCK_IO_PROTOCOL *pBlockIo = &(gBlockData.BlockIo);
    
    ventoy_fill_device_path();
    
    gBlockData.Media.BlockSize = 2048;
    gBlockData.Media.LastBlock = ImgSize / 2048 - 1;
    gBlockData.Media.ReadOnly = TRUE;
    gBlockData.Media.MediaPresent = 1;
    gBlockData.Media.LogicalBlocksPerPhysicalBlock = 1;

	pBlockIo->Revision = EFI_BLOCK_IO_PROTOCOL_REVISION3;
	pBlockIo->Media = &(gBlockData.Media);
	pBlockIo->Reset = ventoy_block_io_reset;

    if (gMemdiskMode)
    {
        pBlockIo->ReadBlocks = ventoy_block_io_ramdisk_read;
    }
    else
    {
    	pBlockIo->ReadBlocks = ventoy_block_io_read;        
    }
    
	pBlockIo->WriteBlocks = ventoy_block_io_write;
	pBlockIo->FlushBlocks = ventoy_block_io_flush;

    Status = gBS->InstallMultipleProtocolInterfaces(&gBlockData.Handle,
            &gEfiBlockIoProtocolGuid, &gBlockData.BlockIo,
            &gEfiDevicePathProtocolGuid, gBlockData.Path,
            NULL);

    debug("Install protocol %r", Status);

    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Status = gBS->ConnectController(gBlockData.Handle, NULL, NULL, 1);
    debug("Connect controller %r", Status);

    return EFI_SUCCESS;
}


EFI_STATUS EFIAPI ventoy_load_image
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
            
    Status = ventoy_load_image(ImageHandle, gBlockData.pDiskFsDevPath, 
                               gIso9660EfiDriverPath, 
                               sizeof(gIso9660EfiDriverPath), 
                               &Image);
    debug("load iso efi driver status:%r", Status);

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
    location->image_sector_size = 2048;
    location->disk_sector_size  = g_chain->disk_sector_size;
    location->region_count = g_img_chunk_num;

    region = location->regions;

    for (i = 0; i < g_img_chunk_num; i++)
    {
        region->image_sector_count = chunk->img_end_sector - chunk->img_start_sector + 1;
        region->image_start_sector = chunk->img_start_sector;
        region->disk_start_sector  = chunk->disk_start_sector;
        region++;
        chunk++;
    }

    return 0;
}

STATIC EFI_STATUS EFIAPI ventoy_parse_cmdline(IN EFI_HANDLE ImageHandle)
{   
    UINT32 i = 0;
    UINT32 old_cnt = 0;
    UINTN size = 0;
    UINT8 chksum = 0;
    CHAR16 *pPos = NULL;
    CHAR16 *pCmdLine = NULL;
    EFI_STATUS Status = EFI_SUCCESS;
    ventoy_grub_param *pGrubParam = NULL;
    EFI_LOADED_IMAGE_PROTOCOL *pImageInfo = NULL;

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

    if (StrStr(pCmdLine, L"isoefi=on"))
    {
        gLoadIsoEfi = TRUE;
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
    grub_env_get = pGrubParam->grub_env_get;

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

    pPos = StrStr(pCmdLine, L"mem:");
    g_chain = (ventoy_chain_head *)StrHexToUintn(pPos + 4);

    pPos = StrStr(pPos, L"size:");
    size = StrDecimalToUintn(pPos + 5);

    debug("memory addr:%p size:%lu", g_chain, size);

    if (StrStr(pCmdLine, L"memdisk"))
    {
        g_iso_buf_size = size;
        gMemdiskMode = TRUE;
    }
    else
    {
        g_chunk = (ventoy_img_chunk *)((char *)g_chain + g_chain->img_chunk_offset);
        g_img_chunk_num = g_chain->img_chunk_num;
        g_override_chunk = (ventoy_override_chunk *)((char *)g_chain + g_chain->override_chunk_offset);
        g_override_chunk_num = g_chain->override_chunk_num;
        g_virt_chunk = (ventoy_virt_chunk *)((char *)g_chain + g_chain->virt_chunk_offset);
        g_virt_chunk_num = g_chain->virt_chunk_num;

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

    FreePool(pCmdLine);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI ventoy_wrapper_file_open
(
    EFI_FILE_HANDLE This, 
    EFI_FILE_HANDLE *New,
    CHAR16 *Name, 
    UINT64 Mode, 
    UINT64 Attributes
)
{
    UINT32 i = 0;
    UINT32 j = 0;
    UINT64 Sectors = 0;
    EFI_STATUS Status = EFI_SUCCESS;
    CHAR8 TmpName[256];
    ventoy_virt_chunk *virt = NULL;

    Status = g_original_fopen(This, New, Name, Mode, Attributes);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    if (g_file_replace_list && g_file_replace_list->magic == GRUB_FILE_REPLACE_MAGIC &&
        g_file_replace_list->new_file_virtual_id < g_virt_chunk_num)
    {
        AsciiSPrint(TmpName, sizeof(TmpName), "%s", Name);
        for (j = 0; j < 4; j++)
        {
            if (0 == AsciiStrCmp(g_file_replace_list[i].old_file_name[j], TmpName))
            {
                g_original_fclose(*New);
                *New = &g_efi_file_replace.WrapperHandle;
                ventoy_wrapper_file_procotol(*New);

                virt = g_virt_chunk + g_file_replace_list->new_file_virtual_id;

                Sectors = (virt->mem_sector_end - virt->mem_sector_start) + (virt->remap_sector_end - virt->remap_sector_start);
                
                g_efi_file_replace.BlockIoSectorStart = virt->mem_sector_start;
                g_efi_file_replace.FileSizeBytes = Sectors * 2048;

                if (gDebugPrint)
                {
                    debug("## ventoy_wrapper_file_open <%s> BlockStart:%lu Sectors:%lu Bytes:%lu", Name,
                        g_efi_file_replace.BlockIoSectorStart, Sectors, Sectors * 2048);
                    sleep(3);
                }
                
                return Status;
            }
        }
    }

    return Status;
}

EFI_STATUS EFIAPI ventoy_wrapper_open_volume
(
    IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL     *This,
    OUT EFI_FILE_PROTOCOL                 **Root
)
{
    EFI_STATUS Status = EFI_SUCCESS;
    
    Status = g_original_open_volume(This, Root);
    if (!EFI_ERROR(Status))
    {
        g_original_fopen = (*Root)->Open;
        g_original_fclose = (*Root)->Close;
        (*Root)->Open = ventoy_wrapper_file_open;
    }

    return Status;
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
                //ventoy_wrapper_system();
            }

            if (g_file_replace_list && g_file_replace_list->magic == GRUB_FILE_REPLACE_MAGIC)
            {
                g_original_open_volume = pFile->OpenVolume;
                pFile->OpenVolume = ventoy_wrapper_open_volume;
            }
            
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

    if (g_chain->os_param.vtoy_img_location_addr)
    {
        FreePool((VOID *)(UINTN)g_chain->os_param.vtoy_img_location_addr);
    }

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI ventoy_ramdisk_boot(IN EFI_HANDLE ImageHandle)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_RAM_DISK_PROTOCOL *RamDisk = NULL;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath = NULL;

    debug("RamDisk Boot ...");

    Status = gBS->LocateProtocol(&gEfiRamDiskProtocolGuid, NULL, (VOID **)&RamDisk);
    if (EFI_ERROR(Status))
    {
        debug("Failed to locate ramdisk protocol %r", Status);
        return Status;
    }
    debug("Locate RamDisk Protocol %r ...", Status);

    Status = RamDisk->Register((UINTN)g_chain, (UINT64)g_iso_buf_size, &gEfiVirtualCdGuid, NULL, &DevicePath);
    if (EFI_ERROR(Status))
    {
        debug("Failed to register ramdisk %r", Status);
        return Status;
    }
    
    debug("Register RamDisk %r ...", Status);
    debug("RamDisk DevicePath:<%s> ...", ConvertDevicePathToText(DevicePath, FALSE, FALSE));

    ventoy_debug_pause();

    gBlockData.Path = DevicePath;
    gBlockData.DevicePathCompareLen = GetDevicePathSize(DevicePath) - sizeof(EFI_DEVICE_PATH_PROTOCOL);

    Status = ventoy_boot(ImageHandle);
    if (EFI_NOT_FOUND == Status)
    {
        gST->ConOut->OutputString(gST->ConOut, L"No bootfile found for UEFI!\r\n");
        gST->ConOut->OutputString(gST->ConOut, L"Maybe the image does not support " VENTOY_UEFI_DESC  L"!\r\n");
        sleep(300);
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
    
    g_sector_flag_num = 512; /* initial value */

    g_sector_flag = AllocatePool(g_sector_flag_num * sizeof(ventoy_sector_flag));
    if (NULL == g_sector_flag)
    {
        return EFI_OUT_OF_RESOURCES;
    }

    gST->ConOut->ClearScreen(gST->ConOut);
    ventoy_clear_input();

    ventoy_parse_cmdline(ImageHandle);

    if (gMemdiskMode)
    {
        g_ramdisk_param.PhyAddr = (UINT64)(UINTN)g_chain;
        g_ramdisk_param.DiskSize = (UINT64)g_iso_buf_size;

        ventoy_save_ramdisk_param();
        
        ventoy_install_blockio(ImageHandle, g_iso_buf_size);
        Status = ventoy_boot(ImageHandle);
        if (EFI_NOT_FOUND == Status)
        {
            gST->ConOut->OutputString(gST->ConOut, L"No bootfile found for UEFI!\r\n");
            gST->ConOut->OutputString(gST->ConOut, L"Maybe the image does not support " VENTOY_UEFI_DESC  L"!\r\n");
            sleep(300);
        }

        ventoy_del_ramdisk_param();
    }
    else
    {
        ventoy_set_variable();
        ventoy_find_iso_disk(ImageHandle);

        if (gLoadIsoEfi)
        {
            ventoy_find_iso_disk_fs(ImageHandle);
            ventoy_load_isoefi_driver(ImageHandle);
        }

        ventoy_debug_pause();
        
        ventoy_install_blockio(ImageHandle, g_chain->virt_img_size_in_bytes);

        ventoy_debug_pause();

        Status = ventoy_boot(ImageHandle);
        if (EFI_NOT_FOUND == Status)
        {
            if (!gLoadIsoEfi)
            {
                gLoadIsoEfi = TRUE;
                ventoy_find_iso_disk_fs(ImageHandle);
                ventoy_load_isoefi_driver(ImageHandle);

                Status = ventoy_boot(ImageHandle);
            }

            if (EFI_NOT_FOUND == Status)
            {
                gST->ConOut->OutputString(gST->ConOut, L"No bootfile found for UEFI!\r\n");
                gST->ConOut->OutputString(gST->ConOut, L"Maybe the image does not support " VENTOY_UEFI_DESC  L"!\r\n");
                sleep(60);
            }
        }

        ventoy_clean_env();
    }
    
    ventoy_clear_input();
    gST->ConOut->ClearScreen(gST->ConOut);

    return EFI_SUCCESS;
}


