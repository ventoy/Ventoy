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

UINT8 *g_iso_data_buf = NULL;
UINTN g_iso_buf_size = 0;
BOOLEAN gMemdiskMode = FALSE;
BOOLEAN gSector512Mode = FALSE;

ventoy_sector_flag *g_sector_flag = NULL;
UINT32 g_sector_flag_num = 0;

EFI_FILE_OPEN g_original_fopen = NULL;
EFI_FILE_CLOSE g_original_fclose = NULL;
EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME g_original_open_volume = NULL;

/* EFI block device vendor device path GUID */
EFI_GUID gVtoyBlockDevicePathGuid = VTOY_BLOCK_DEVICE_PATH_GUID;

#define VENTOY_ISO9660_SECTOR_OVERFLOW  2097152

BOOLEAN g_fixup_iso9660_secover_enable = FALSE;
BOOLEAN g_fixup_iso9660_secover_start  = FALSE;
UINT64  g_fixup_iso9660_secover_1st_secs = 0;
UINT64  g_fixup_iso9660_secover_cur_secs = 0;
UINT64  g_fixup_iso9660_secover_tot_secs = 0;

STATIC UINTN g_keyboard_hook_count = 0;
STATIC BOOLEAN g_blockio_start_record_bcd = FALSE;
STATIC BOOLEAN g_blockio_bcd_read_done = FALSE;

EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *g_con_simple_input_ex = NULL;
STATIC EFI_INPUT_READ_KEY_EX g_org_read_key_ex = NULL;
STATIC EFI_INPUT_READ_KEY g_org_read_key = NULL;

STATIC EFI_LOCATE_HANDLE g_org_locate_handle = NULL;

STATIC UINT8 g_sector_buf[2048];
STATIC EFI_BLOCK_READ g_sector_2048_read = NULL;
STATIC EFI_BLOCK_WRITE g_sector_2048_write = NULL;

STATIC UINTN g_DriverBindWrapperCnt = 0;
STATIC DRIVER_BIND_WRAPPER g_DriverBindWrapperList[MAX_DRIVER_BIND_WRAPPER];

BOOLEAN ventoy_is_cdrom_dp_exist(VOID)
{
    UINTN i = 0;
    UINTN Count = 0;
    EFI_HANDLE *Handles = NULL;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath = NULL;

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiDevicePathProtocolGuid, 
                                     NULL, &Count, &Handles);
    if (EFI_ERROR(Status))
    {
        return FALSE;
    }

    for (i = 0; i < Count; i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);
        if (EFI_ERROR(Status))
        {
            continue;
        }

        while (!IsDevicePathEnd(DevicePath))
        {
            if (MEDIA_DEVICE_PATH == DevicePath->Type && MEDIA_CDROM_DP == DevicePath->SubType)
            {
                FreePool(Handles);
                return TRUE;
            }
        
            DevicePath = NextDevicePathNode(DevicePath);
        }
    }

    FreePool(Handles);
    return FALSE;         
}

#if 0
/* Block IO procotol */
#endif

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
    
    debug("read iso sector %lu count %u Buffer:%p Align:%u blk:%u", 
        Sector, Count, Buffer, pRawBlockIo->Media->IoAlign, pRawBlockIo->Media->BlockSize);

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
            else if (g_chain->disk_sector_size == 1024)
            {
                MapLba = (Sector - pchunk->img_start_sector) * 2 + pchunk->disk_start_sector;
            }
            else if (g_chain->disk_sector_size == 2048)
            {
                MapLba = (Sector - pchunk->img_start_sector) + pchunk->disk_start_sector;
            }
            else if (g_chain->disk_sector_size == 4096)
            {
                MapLba = ((Sector - pchunk->img_start_sector) >> 1) + pchunk->disk_start_sector;
            }

            secLeft = pchunk->img_end_sector + 1 - Sector;
            secRead = (Count < secLeft) ? Count : secLeft;

            Status = pRawBlockIo->ReadBlocks(pRawBlockIo, pRawBlockIo->Media->MediaId,
                                     MapLba, secRead * 2048, pCurBuf);
            if (EFI_ERROR(Status))
            {
                debug("Raw disk read block failed %r LBA:%lu Count:%u %p", Status, MapLba, secRead, pCurBuf);
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

        if (g_fixup_iso9660_secover_enable && (!g_fixup_iso9660_secover_start) && 
            pOverride->override_size == sizeof(ventoy_iso9660_override))
        {
            ventoy_iso9660_override *dirent = (ventoy_iso9660_override *)pOverride->override_data;
            if (dirent->first_sector >= VENTOY_ISO9660_SECTOR_OVERFLOW)
            {
                g_fixup_iso9660_secover_start = TRUE;
                g_fixup_iso9660_secover_cur_secs = 0;
            }
        }
    }

    if (g_blockio_start_record_bcd && FALSE == g_blockio_bcd_read_done)
    {
        if (*(UINT32 *)Buffer == 0x66676572)
        {
            g_blockio_bcd_read_done = TRUE;
        }
    }

    return EFI_SUCCESS;    
}

STATIC EFI_STATUS EFIAPI ventoy_write_iso_sector
(
    IN UINT64                 Sector,
    IN UINTN                  Count,
    IN VOID                  *Buffer
)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_LBA MapLba = 0;
    UINT32 i = 0;
    UINTN secLeft = 0;
    UINTN secRead = 0;
    UINT64 ReadStart = 0;
    UINT64 ReadEnd = 0;
    UINT8 *pCurBuf = (UINT8 *)Buffer;
    ventoy_img_chunk *pchunk = g_chunk;
    EFI_BLOCK_IO_PROTOCOL *pRawBlockIo = gBlockData.pRawBlockIo;
    
    debug("write iso sector %lu  count %u", Sector, Count);

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
            else if (g_chain->disk_sector_size == 1024)
            {
                MapLba = (Sector - pchunk->img_start_sector) * 2 + pchunk->disk_start_sector;
            }
            else if (g_chain->disk_sector_size == 2048)
            {
                MapLba = (Sector - pchunk->img_start_sector) + pchunk->disk_start_sector;
            }
            else if (g_chain->disk_sector_size == 4096)
            {
                MapLba = ((Sector - pchunk->img_start_sector) >> 1) + pchunk->disk_start_sector;
            }


            secLeft = pchunk->img_end_sector + 1 - Sector;
            secRead = (Count < secLeft) ? Count : secLeft;

            Status = pRawBlockIo->WriteBlocks(pRawBlockIo, pRawBlockIo->Media->MediaId,
                                     MapLba, secRead * 2048, pCurBuf);
            if (EFI_ERROR(Status))
            {
                debug("Raw disk write block failed %r LBA:%lu Count:%u", Status, MapLba, secRead);
                return Status;
            }

            Count -= secRead;
            Sector += secRead;
            pCurBuf += secRead * 2048;
        }
    }

    return EFI_SUCCESS;    
}

EFI_STATUS EFIAPI ventoy_block_io_ramdisk_write 
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

    if (!gSector512Mode)
    {
        return EFI_WRITE_PROTECTED;
    }

    CopyMem(g_iso_data_buf + (Lba * 2048), Buffer, BufferSize);

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

    CopyMem(Buffer, g_iso_data_buf + (Lba * 2048), BufferSize);
    
    if (g_blockio_start_record_bcd && FALSE == g_blockio_bcd_read_done)
    {
        if (*(UINT32 *)Buffer == 0x66676572)
        {
            g_blockio_bcd_read_done = TRUE;            
        }
    }

	return EFI_SUCCESS;
}

EFI_LBA EFIAPI ventoy_fixup_iso9660_sector(IN EFI_LBA Lba, UINT32 secNum)
{
    UINT32 i = 0;

    if (g_fixup_iso9660_secover_cur_secs > 0)
    {
        Lba += VENTOY_ISO9660_SECTOR_OVERFLOW;
        g_fixup_iso9660_secover_cur_secs += secNum;
        if (g_fixup_iso9660_secover_cur_secs >= g_fixup_iso9660_secover_tot_secs)
        {
            g_fixup_iso9660_secover_start = FALSE;
            goto end;
        }
    }
    else
    {
        ventoy_iso9660_override *dirent;
        ventoy_override_chunk *pOverride;

        for (i = 0, pOverride = g_override_chunk; i < g_override_chunk_num; i++, pOverride++)
        {
            dirent = (ventoy_iso9660_override *)pOverride->override_data;
            if (Lba == dirent->first_sector)
            {
                g_fixup_iso9660_secover_start = FALSE;
                goto end;
            }
        }

        if (g_fixup_iso9660_secover_start)
        {
            for (i = 0, pOverride = g_override_chunk; i < g_override_chunk_num; i++, pOverride++)
            {
                dirent = (ventoy_iso9660_override *)pOverride->override_data;
                if (Lba + VENTOY_ISO9660_SECTOR_OVERFLOW == dirent->first_sector)
                {
                    g_fixup_iso9660_secover_tot_secs = (dirent->size + 2047) / 2048;
                    g_fixup_iso9660_secover_cur_secs = secNum;
                    if (g_fixup_iso9660_secover_cur_secs >= g_fixup_iso9660_secover_tot_secs)
                    {
                        g_fixup_iso9660_secover_start = FALSE;
                    }
                    Lba += VENTOY_ISO9660_SECTOR_OVERFLOW;
                    goto end;
                }
            }
        }
    }

end:
    return Lba;
}

EFI_STATUS EFIAPI ventoy_block_io_read_real 
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
    UINT32 TmpNum = 0;
    UINT64 VirtSec = 0;
    UINT64 offset = 0;
    EFI_LBA curlba = 0;
    EFI_LBA lastlba = 0;
    UINT8 *lastbuffer;
    ventoy_sector_flag *cur_flag;
    ventoy_virt_chunk *node;
    
    debug("### block_io_read_real sector:%u count:%u Buffer:%p", (UINT32)Lba, (UINT32)BufferSize / 2048, Buffer);

    secNum = BufferSize / 2048;

    /* Workaround for SSTR PE loader error */
    if (g_fixup_iso9660_secover_start)
    {
        Lba = ventoy_fixup_iso9660_sector(Lba, secNum);
    }

    offset = Lba * 2048;

    if (offset + BufferSize <= g_chain->real_img_size_in_bytes)
    {
        return ventoy_read_iso_sector(Lba, secNum, Buffer);
    }
    else if (offset < g_chain->real_img_size_in_bytes)
    {
        TmpNum = (g_chain->real_img_size_in_bytes - offset) / 2048;
        ventoy_read_iso_sector(Lba, TmpNum, Buffer);

        Lba += TmpNum;
        secNum -= TmpNum;
        Buffer = (UINT8 *)Buffer + (g_chain->real_img_size_in_bytes - offset);
        offset = Lba * 2048;
    }

    VirtSec = g_chain->virt_img_size_in_bytes / 2048;
    if (Lba >= VirtSec)
    {
        return EFI_SUCCESS;
    }
    else if (Lba + secNum > VirtSec)
    {
        secNum = VirtSec - Lba;
    }

    debug("XXX block_io_read_real sector:%u count:%u Buffer:%p", (UINT32)Lba, (UINT32)BufferSize / 2048, Buffer);

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

EFI_STATUS EFIAPI ventoy_block_io_read
(
    IN EFI_BLOCK_IO_PROTOCOL          *This,
    IN UINT32                          MediaId,
    IN EFI_LBA                         Lba,
    IN UINTN                           BufferSize,
    OUT VOID                          *Buffer
) 
{
    UINT32 IoAlign = 0;
    VOID *NewBuf = NULL;
    EFI_STATUS Status = EFI_OUT_OF_RESOURCES;

    if (gBlockData.pRawBlockIo && gBlockData.pRawBlockIo->Media)
    {
        IoAlign = gBlockData.pRawBlockIo->Media->IoAlign;
    }

    if ((IoAlign == 0) || (((UINTN) Buffer & (IoAlign - 1)) == 0))
    {
        Status = ventoy_block_io_read_real(This, MediaId, Lba, BufferSize, Buffer);
    }
    else
    {
        NewBuf = AllocatePages(EFI_SIZE_TO_PAGES(BufferSize + IoAlign));
        if (NewBuf)
        {
            Status = ventoy_block_io_read_real(This, MediaId, Lba, BufferSize, NewBuf);
            CopyMem(Buffer, NewBuf, BufferSize);
            FreePages(NewBuf, EFI_SIZE_TO_PAGES(BufferSize + IoAlign));
        }
    }

    return Status;
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
    UINT32 secNum = 0;
    UINT64 offset = 0;
    
    (VOID)This;
    (VOID)MediaId;

    if (!gSector512Mode)
    {
        return EFI_WRITE_PROTECTED;
    }

    secNum = BufferSize / 2048;
    offset = Lba * 2048;

    return ventoy_write_iso_sector(Lba, secNum, Buffer);
}

EFI_STATUS EFIAPI ventoy_block_io_flush(IN EFI_BLOCK_IO_PROTOCOL *This)
{
	(VOID)This;
	return EFI_SUCCESS;
}

STATIC UINTN ventoy_get_current_device_path_id(VOID)
{
    UINTN i = 0;
    UINTN Count = 0;
    UINTN MaxId = 0;
    UINTN CurId = 0;
    BOOLEAN Find = FALSE;
    EFI_HANDLE *Handles = NULL;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath = NULL;
    VENDOR_DEVICE_PATH *venPath = NULL;
    
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiDevicePathProtocolGuid, 
                                     NULL, &Count, &Handles);
    if (EFI_ERROR(Status))
    {
        return 0;
    }

    for (i = 0; i < Count; i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);
        if (EFI_ERROR(Status))
        {
            continue;
        }

        if (DevicePath->Type == HARDWARE_DEVICE_PATH && DevicePath->SubType == HW_VENDOR_DP)
        {
            venPath = (VENDOR_DEVICE_PATH *)DevicePath;
            if (CompareGuid(&venPath->Guid, &gVtoyBlockDevicePathGuid))
            {
                CurId = StrDecimalToUintn((CHAR16 *)(venPath + 1) + StrLen(L"ventoy_"));
                MaxId = MAX(MaxId, CurId);
                Find = TRUE;
            }
        }
    }

    FreePool(Handles);

    return Find ? (MaxId + 1) : 0;
}

EFI_STATUS EFIAPI ventoy_fill_device_path(VOID)
{
    UINTN CurVtoyDpId = 0;
    UINTN NameLen = 0;
    UINT8 TmpBuf[128] = {0};
    VENDOR_DEVICE_PATH *venPath = NULL;
    CHAR16 VtoyDpName[32];

    CurVtoyDpId = ventoy_get_current_device_path_id();
    UnicodeSPrintAsciiFormat(VtoyDpName, sizeof(VtoyDpName), "ventoy_%03lu", CurVtoyDpId);

    venPath = (VENDOR_DEVICE_PATH *)TmpBuf;
    NameLen = StrSize(VtoyDpName);
    venPath->Header.Type = HARDWARE_DEVICE_PATH;
    venPath->Header.SubType = HW_VENDOR_DP;
    venPath->Header.Length[0] = sizeof(VENDOR_DEVICE_PATH) + NameLen;
    venPath->Header.Length[1] = 0;
    CopyMem(&venPath->Guid, &gVtoyBlockDevicePathGuid, sizeof(EFI_GUID));
    CopyMem(venPath + 1, VtoyDpName, NameLen);
    
    gBlockData.Path = AppendDevicePathNode(NULL, (EFI_DEVICE_PATH_PROTOCOL *)TmpBuf);
    gBlockData.DevicePathCompareLen = sizeof(VENDOR_DEVICE_PATH) + NameLen;

    debug("gBlockData.Path=<%lu><%s>\n", CurVtoyDpId, ConvertDevicePathToText(gBlockData.Path, FALSE, FALSE));

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI ventoy_connect_driver(IN EFI_HANDLE ControllerHandle, IN CONST CHAR16 *DrvName)
{
    UINTN i = 0;
    UINTN Count = 0;
    CHAR16 *DriverName = NULL;
    EFI_HANDLE *Handles = NULL;
    EFI_HANDLE DrvHandles[2] = { NULL };
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_COMPONENT_NAME_PROTOCOL *NameProtocol = NULL;
    EFI_COMPONENT_NAME2_PROTOCOL *Name2Protocol = NULL;

    debug("ventoy_connect_driver <%s>...", DrvName);

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

        VENTOY_GET_COMPONENT_NAME(Name2Protocol, DriverName);

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
        debug("ventoy_connect_driver:<%s> <%r>", DrvName, Status);
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

        VENTOY_GET_COMPONENT_NAME(NameProtocol, DriverName);

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
        debug("ventoy_connect_driver:<%s> <%r>", DrvName, Status);
        goto end;
    }

    Status = EFI_NOT_FOUND;
    
end:
    FreePool(Handles);
    
    return Status;
}


STATIC BOOLEAN ventoy_filesystem_need_wrapper(IN CONST CHAR16 *DrvName)
{
    UINTN i;
    CHAR16 UpperDrvName[256];

    StrCpyS(UpperDrvName, 256, DrvName);

    for (i = 0; i < 256 && UpperDrvName[i]; i++)
    {
        if (UpperDrvName[i] >= 'a' && UpperDrvName[i] <= 'z')
        {
            UpperDrvName[i] = 'A' + (UpperDrvName[i] - 'a');
        }
    }

    /*
     * suppress some file system drivers 
     *  1. rEFInd File System Driver
     *
     */
    
    if (StrStr(UpperDrvName, L"REFIND") && StrStr(UpperDrvName, L"FILE SYSTEM"))
    {
        return TRUE;
    }

    return FALSE;
}

STATIC VOID ventoy_add_filesystem_wrapper
(
    IN EFI_DRIVER_BINDING_PROTOCOL *DriverBindProtocol, 
    IN CONST CHAR16 *DriverName
)
{
    UINTN j;

    if (g_DriverBindWrapperCnt >= MAX_DRIVER_BIND_WRAPPER)
    {
        debug("driver binding wrapper overflow %lu", g_DriverBindWrapperCnt);
        return;
    }

    if (!ventoy_filesystem_need_wrapper(DriverName))
    {
        return;
    }

    for (j = 0; j < g_DriverBindWrapperCnt; j++)
    {
        if (g_DriverBindWrapperList[j].DriverBinding == DriverBindProtocol)
        {
            debug("Duplicate driverbinding <%s> %p %lu %lu", DriverName, DriverBindProtocol, j, g_DriverBindWrapperCnt);
            break;
        }
    }

    if (j >= g_DriverBindWrapperCnt)
    {
        g_DriverBindWrapperList[g_DriverBindWrapperCnt].DriverBinding = DriverBindProtocol;
        g_DriverBindWrapperList[g_DriverBindWrapperCnt].pfOldSupport = DriverBindProtocol->Supported;
        g_DriverBindWrapperCnt++;
        debug("Add driverbinding <%s> %p %lu", DriverName, DriverBindProtocol, g_DriverBindWrapperCnt);
    }
}

STATIC EFI_STATUS ventoy_find_filesystem_driverbind(VOID)
{
    UINTN i = 0;
    UINTN Count = 0;
    CHAR16 *DriverName = NULL;
    EFI_HANDLE *Handles = NULL;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_COMPONENT_NAME_PROTOCOL *NameProtocol = NULL;
    EFI_COMPONENT_NAME2_PROTOCOL *Name2Protocol = NULL;
    EFI_DRIVER_BINDING_PROTOCOL *DriverBindProtocol = NULL;
    
    debug("ventoy_find_filesystem_driverbind...");

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

        VENTOY_GET_COMPONENT_NAME(Name2Protocol, DriverName);

        Status = gBS->HandleProtocol(Handles[i], &gEfiDriverBindingProtocolGuid, (VOID **)&DriverBindProtocol);
        if (EFI_ERROR(Status))
        {
            debug("### 2 No DriverBind <%s> <%r>", DriverName, Status);
            continue;
        }

        ventoy_add_filesystem_wrapper(DriverBindProtocol, DriverName);
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

        VENTOY_GET_COMPONENT_NAME(NameProtocol, DriverName);

        Status = gBS->HandleProtocol(Handles[i], &gEfiDriverBindingProtocolGuid, (VOID **)&DriverBindProtocol);
        if (EFI_ERROR(Status))
        {
            debug("### 1 No DriverBind <%s> <%r>", DriverName, Status);
            continue;
        }

        ventoy_add_filesystem_wrapper(DriverBindProtocol, DriverName);
    }

    FreePool(Handles);
    
    return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI ventoy_wrapper_driver_bind_support
(
    IN EFI_DRIVER_BINDING_PROTOCOL            *This,
    IN EFI_HANDLE                              ControllerHandle,
    IN EFI_DEVICE_PATH_PROTOCOL               *RemainingDevicePath OPTIONAL
)
{
    UINTN i;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath = NULL;
    EFI_DRIVER_BINDING_SUPPORTED pfOldSupport = NULL;

    for (i = 0; i < g_DriverBindWrapperCnt; i++)
    {
        if (g_DriverBindWrapperList[i].DriverBinding == This)
        {
            pfOldSupport = g_DriverBindWrapperList[i].pfOldSupport;
            break;
        }
    }

    debug("ventoy_wrapper_driver_bind_support %lu %p", i, pfOldSupport);

    if (!pfOldSupport)
    {
        return EFI_UNSUPPORTED;
    }

    Status = gBS->HandleProtocol(ControllerHandle, &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);
    if (EFI_ERROR(Status))
    {
        goto out;
    }

    if (0 == CompareMem(gBlockData.Path, DevicePath, gBlockData.DevicePathCompareLen))
    {
        debug("return EFI_UNSUPPORTED for ventoy");
        return EFI_UNSUPPORTED;
    }

out:
    return pfOldSupport(This, ControllerHandle, RemainingDevicePath);
}

EFI_STATUS ventoy_disable_ex_filesystem(VOID)
{
    UINTN i;

    ventoy_find_filesystem_driverbind();

    for (i = 0; i < g_DriverBindWrapperCnt; i++)
    {
        g_DriverBindWrapperList[i].DriverBinding->Supported = ventoy_wrapper_driver_bind_support;
    }

    debug("Wrapper Ex Driver Binding %lu", g_DriverBindWrapperCnt);
    ventoy_debug_pause();
    
    return EFI_SUCCESS;
}

EFI_STATUS ventoy_enable_ex_filesystem(VOID)
{
    UINTN i;

    for (i = 0; i < g_DriverBindWrapperCnt; i++)
    {
        g_DriverBindWrapperList[i].DriverBinding->Supported = g_DriverBindWrapperList[i].pfOldSupport;
    }
    g_DriverBindWrapperCnt = 0;

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI ventoy_block_io_read_512
(
    IN EFI_BLOCK_IO_PROTOCOL          *This,
    IN UINT32                          MediaId,
    IN EFI_LBA                         Lba,
    IN UINTN                           BufferSize,
    OUT VOID                          *Buffer
)
{
    EFI_LBA Mod;
    UINTN ReadSize;
    UINT8 *CurBuf = NULL;
    EFI_STATUS Status = EFI_SUCCESS;

    debug("ventoy_block_io_read_512 %lu %lu Buffer:%p\n", Lba, BufferSize / 512, Buffer);

    CurBuf = (UINT8 *)Buffer;

    Mod = Lba % 4;
    if (Mod > 0)
    {
        Status |= g_sector_2048_read(This, MediaId, Lba / 4, 2048, g_sector_buf);

        if (BufferSize <= (4 - Mod) * 512)
        {
            CopyMem(CurBuf, g_sector_buf + Mod * 512, BufferSize);
            return EFI_SUCCESS;
        }
        else
        {
            ReadSize = (4 - Mod) * 512;
            CopyMem(CurBuf, g_sector_buf + Mod * 512, ReadSize);
            CurBuf += ReadSize;
            Lba += (4 - Mod);
            BufferSize -= ReadSize;
        }
    }

    if (BufferSize >= 2048)
    {
        ReadSize = BufferSize / 2048 * 2048;
            
        Status |= g_sector_2048_read(This, MediaId, Lba / 4, ReadSize, CurBuf);
        CurBuf += ReadSize;
        
        Lba += ReadSize / 512;
        BufferSize -= ReadSize;
    }

    if (BufferSize > 0)
    {
        Status |= g_sector_2048_read(This, MediaId, Lba / 4, 2048, g_sector_buf);
        CopyMem(CurBuf, g_sector_buf, BufferSize);
    }

    return Status;
}

EFI_STATUS EFIAPI ventoy_block_io_write_512
(
    IN EFI_BLOCK_IO_PROTOCOL          *This,
    IN UINT32                          MediaId,
    IN EFI_LBA                         Lba,
    IN UINTN                           BufferSize,
    IN VOID                           *Buffer
)
{
    EFI_LBA Mod;
    UINTN ReadSize;
    UINT8 *CurBuf = NULL;
    EFI_STATUS Status = EFI_SUCCESS;

    debug("ventoy_block_io_write_512 %lu %lu\n", Lba, BufferSize / 512);

    CurBuf = (UINT8 *)Buffer;

    Mod = Lba % 4;
    if (Mod > 0)
    {
        Status |= g_sector_2048_read(This, MediaId, Lba / 4, 2048, g_sector_buf);

        if (BufferSize <= (4 - Mod) * 512)
        {
            CopyMem(g_sector_buf + Mod * 512, CurBuf, BufferSize);
            return g_sector_2048_write(This, MediaId, Lba / 4, 2048, g_sector_buf);
        }
        else
        {
            ReadSize = (4 - Mod) * 512;
            CopyMem(g_sector_buf + Mod * 512, CurBuf, ReadSize);
            g_sector_2048_write(This, MediaId, Lba / 4, 2048, g_sector_buf);
            
            CurBuf += ReadSize;
            Lba += (4 - Mod);
            BufferSize -= ReadSize;
        }
    }

    if (BufferSize >= 2048)
    {
        ReadSize = BufferSize / 2048 * 2048;
            
        Status |= g_sector_2048_write(This, MediaId, Lba / 4, ReadSize, CurBuf);
        CurBuf += ReadSize;
        
        Lba += ReadSize / 512;
        BufferSize -= ReadSize;
    }

    if (BufferSize > 0)
    {
        Status |= g_sector_2048_read(This, MediaId, Lba / 4, 2048, g_sector_buf);
        
        CopyMem(g_sector_buf, CurBuf, BufferSize);
        g_sector_2048_write(This, MediaId, Lba / 4, 2048, g_sector_buf);
    }

    return Status;
}

EFI_STATUS EFIAPI ventoy_install_blockio(IN EFI_HANDLE ImageHandle, IN UINT64 ImgSize)
{   
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_BLOCK_IO_PROTOCOL *pBlockIo = &(gBlockData.BlockIo);
    
    ventoy_fill_device_path();

    debug("install block io protocol %p", ImageHandle);
    ventoy_debug_pause();

    if (gSector512Mode)
    {
        gBlockData.Media.BlockSize = 512;
        gBlockData.Media.LastBlock = ImgSize / 512 - 1;
        gBlockData.Media.ReadOnly = FALSE;
    }
    else
    {
        gBlockData.Media.BlockSize = 2048;
        gBlockData.Media.LastBlock = ImgSize / 2048 - 1;        
        gBlockData.Media.ReadOnly = TRUE;
    }
    
    gBlockData.Media.MediaPresent = 1;
    gBlockData.Media.LogicalBlocksPerPhysicalBlock = 1;

	pBlockIo->Revision = EFI_BLOCK_IO_PROTOCOL_REVISION3;
	pBlockIo->Media = &(gBlockData.Media);
	pBlockIo->Reset = ventoy_block_io_reset;

    if (gSector512Mode)
    {
        g_sector_2048_read = gMemdiskMode ? ventoy_block_io_ramdisk_read : ventoy_block_io_read;
        g_sector_2048_write = gMemdiskMode ? ventoy_block_io_ramdisk_write : ventoy_block_io_write;
        pBlockIo->ReadBlocks = ventoy_block_io_read_512;
    	pBlockIo->WriteBlocks = ventoy_block_io_write_512;
    }
    else
    {
        pBlockIo->ReadBlocks = gMemdiskMode ? ventoy_block_io_ramdisk_read : ventoy_block_io_read;        
    	pBlockIo->WriteBlocks = ventoy_block_io_write;
    }
        
	pBlockIo->FlushBlocks = ventoy_block_io_flush;

    Status = gBS->InstallMultipleProtocolInterfaces(&gBlockData.Handle,
            &gEfiBlockIoProtocolGuid, &gBlockData.BlockIo,
            &gEfiDevicePathProtocolGuid, gBlockData.Path,
            NULL);
    debug("Install protocol %r %p", Status, gBlockData.Handle);
    if (EFI_ERROR(Status))
    {
        return Status;
    }

    Status = ventoy_connect_driver(gBlockData.Handle, L"Disk I/O Driver");
    debug("Connect disk IO driver %r", Status);

    Status = ventoy_connect_driver(gBlockData.Handle, L"Partition Driver");
    debug("Connect partition driver %r", Status);
    if (EFI_ERROR(Status))
    {
        Status = gBS->ConnectController(gBlockData.Handle, NULL, NULL, TRUE);
        debug("Connect all controller %r", Status);
    }

    ventoy_debug_pause();

    return EFI_SUCCESS;
}

#if 0
/* For file replace */
#endif

STATIC EFI_STATUS EFIAPI
ventoy_wrapper_fs_open(EFI_FILE_HANDLE This, EFI_FILE_HANDLE *New, CHAR16 *Name, UINT64 Mode, UINT64 Attributes)
{
    (VOID)This;
    (VOID)New;
    (VOID)Name;
    (VOID)Mode;
    (VOID)Attributes;
    return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI
ventoy_wrapper_file_open_ex(EFI_FILE_HANDLE This, EFI_FILE_HANDLE *New, CHAR16 *Name, UINT64 Mode, UINT64 Attributes, EFI_FILE_IO_TOKEN *Token)
{
	return ventoy_wrapper_fs_open(This, New, Name, Mode, Attributes);
}

STATIC EFI_STATUS EFIAPI
ventoy_wrapper_file_delete(EFI_FILE_HANDLE This)
{
    (VOID)This;
	return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI
ventoy_wrapper_file_set_info(EFI_FILE_HANDLE This, EFI_GUID *Type, UINTN Len, VOID *Data)
{
	return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI
ventoy_wrapper_file_flush(EFI_FILE_HANDLE This)
{
    (VOID)This;
	return EFI_SUCCESS;
}

/* Ex version */
STATIC EFI_STATUS EFIAPI
ventoy_wrapper_file_flush_ex(EFI_FILE_HANDLE This, EFI_FILE_IO_TOKEN *Token)
{
    (VOID)This;
    (VOID)Token;
	return EFI_SUCCESS;
}

/* Ex version */
STATIC EFI_STATUS EFIAPI
ventoy_wrapper_file_flush_ex_img0(EFI_FILE_HANDLE This, EFI_FILE_IO_TOKEN *Token)
{
    (VOID)This;
    (VOID)Token;
	return EFI_SUCCESS;
}
/* Ex version */
STATIC EFI_STATUS EFIAPI
ventoy_wrapper_file_flush_ex_img1(EFI_FILE_HANDLE This, EFI_FILE_IO_TOKEN *Token)
{
    (VOID)This;
    (VOID)Token;
	return EFI_SUCCESS;
}

#define DEF_WRAP_FUNC(n) \
STATIC EFI_STATUS EFIAPI ventoy_wrapper_file_flush_ex_img#n(EFI_FILE_HANDLE This, EFI_FILE_IO_TOKEN *Token) \
{\
    (VOID)This;\
    (VOID)Token;\
	return EFI_SUCCESS;\
}

#define ITEM_WRAP_FUNC(n) ventoy_wrapper_file_flush_ex_img#n

#if (VTOY_MAX_CONF_REPLACE > 2)
DEF_WRAP_FUNC(2);
#endif
#if (VTOY_MAX_CONF_REPLACE > 3)
DEF_WRAP_FUNC(3);
#endif
#if (VTOY_MAX_CONF_REPLACE > 4)
DEF_WRAP_FUNC(4);
#endif
#if (VTOY_MAX_CONF_REPLACE > 5)
DEF_WRAP_FUNC(5);
#endif
#if (VTOY_MAX_CONF_REPLACE > 6)
DEF_WRAP_FUNC(6);
#endif
#if (VTOY_MAX_CONF_REPLACE > 7)
DEF_WRAP_FUNC(7);
#endif
#if (VTOY_MAX_CONF_REPLACE > 8)
#error "VTOY_MAX_CONF_REPLACE overflow"
#endif

static EFI_FILE_FLUSH_EX g_img_flush_func[VTOY_MAX_CONF_REPLACE] = 
{
    ventoy_wrapper_file_flush_ex_img0,
    ventoy_wrapper_file_flush_ex_img1,
#if (VTOY_MAX_CONF_REPLACE > 2)
    ITEM_WRAP_FUNC(2),
#endif
#if (VTOY_MAX_CONF_REPLACE > 3)
    ITEM_WRAP_FUNC(3),
#endif
#if (VTOY_MAX_CONF_REPLACE > 4)
    ITEM_WRAP_FUNC(4),
#endif
#if (VTOY_MAX_CONF_REPLACE > 5)
    ITEM_WRAP_FUNC(5),
#endif
#if (VTOY_MAX_CONF_REPLACE > 6)
    ITEM_WRAP_FUNC(6),
#endif
#if (VTOY_MAX_CONF_REPLACE > 7)
    ITEM_WRAP_FUNC(7),
#endif
};

STATIC ventoy_efi_file_replace *ventoy_wrapper_get_replace(EFI_FILE_HANDLE This)
{
    UINTN i;

    if (This->FlushEx == ventoy_wrapper_file_flush_ex)
    {
        return &g_efi_file_replace;
    }

    for (i = 0; i < VTOY_MAX_CONF_REPLACE; i++)
    {
        if (This->FlushEx == g_img_flush_func[i])
        {
            return g_img_file_replace + i;
        }
    }
    
    return NULL;
}

STATIC EFI_STATUS EFIAPI
ventoy_wrapper_file_write(EFI_FILE_HANDLE This, UINTN *Len, VOID *Data)
{
    (VOID)This;
    (VOID)Len;
    (VOID)Data;

	return EFI_WRITE_PROTECTED;
}

STATIC EFI_STATUS EFIAPI
ventoy_wrapper_file_write_ex(IN EFI_FILE_PROTOCOL *This, IN OUT EFI_FILE_IO_TOKEN *Token)
{
	return ventoy_wrapper_file_write(This, &(Token->BufferSize), Token->Buffer);
}


STATIC EFI_STATUS EFIAPI
ventoy_wrapper_file_close(EFI_FILE_HANDLE This)
{
    (VOID)This;
    return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI
ventoy_wrapper_file_set_pos(EFI_FILE_HANDLE This, UINT64 Position)
{
    ventoy_efi_file_replace *replace = NULL;

    replace = ventoy_wrapper_get_replace(This);
    
    if (Position <= replace->FileSizeBytes)
    {
        replace->CurPos = Position;
    }
    else
    {
        replace->CurPos = replace->FileSizeBytes;
    }
    
    return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI
ventoy_wrapper_file_get_pos(EFI_FILE_HANDLE This, UINT64 *Position)
{
    ventoy_efi_file_replace *replace = NULL;

    replace = ventoy_wrapper_get_replace(This);

    *Position = replace->CurPos;

    return EFI_SUCCESS;
}


STATIC EFI_STATUS EFIAPI
ventoy_wrapper_file_get_info(EFI_FILE_HANDLE This, EFI_GUID *Type, UINTN *Len, VOID *Data)
{
    EFI_FILE_INFO *Info = (EFI_FILE_INFO *) Data;
    ventoy_efi_file_replace *replace = NULL;

    replace = ventoy_wrapper_get_replace(This);

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
    Info->FileSize = replace->FileSizeBytes;
    Info->PhysicalSize = replace->FileSizeBytes;
    Info->Attribute = EFI_FILE_READ_ONLY;
    //Info->FileName = EFI_FILE_READ_ONLY;

    *Len = Info->Size;
    
    return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI
ventoy_wrapper_file_read(EFI_FILE_HANDLE This, UINTN *Len, VOID *Data)
{
    EFI_LBA Lba;
    UINTN ReadLen = *Len;
    ventoy_efi_file_replace *replace = NULL;

    replace = ventoy_wrapper_get_replace(This);
    
    debug("ventoy_wrapper_file_read ... %u", *Len);

    if (replace->CurPos + ReadLen > replace->FileSizeBytes)
    {
        ReadLen = replace->FileSizeBytes - replace->CurPos;
    }

    Lba = replace->CurPos / 2048 + replace->BlockIoSectorStart;

    ventoy_block_io_read(NULL, 0, Lba, ReadLen, Data);

    *Len = ReadLen;

    replace->CurPos += ReadLen;

    return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI
ventoy_wrapper_file_read_ex(IN EFI_FILE_PROTOCOL *This, IN OUT EFI_FILE_IO_TOKEN *Token)
{
	return ventoy_wrapper_file_read(This, &(Token->BufferSize), Token->Buffer);
}

STATIC EFI_STATUS EFIAPI ventoy_wrapper_file_procotol(EFI_FILE_PROTOCOL *File, BOOLEAN Img, UINTN Index)
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

    if (Img)
    {
        File->FlushEx = g_img_flush_func[Index];
    }
    else
    {
        File->FlushEx = ventoy_wrapper_file_flush_ex;
    }

    return EFI_SUCCESS;
}

STATIC BOOLEAN EFIAPI ventoy_replace_name_match(CHAR8 *pReplace, CHAR8 *pName)
{
    UINTN Len1, Len2;
    
    Len1 = AsciiStrLen(pReplace);
    Len2 = AsciiStrLen(pName);

    if (Len1 == 0 || Len2 == 0)
    {
        return FALSE;
    }

    if (0 == AsciiStriCmp(pReplace, pName))
    {
        return TRUE;
    }

    if (Len1 > 2 && Len2 > 2)
    {
        if ((pReplace[0] != '\\') && (pName[0] == '\\') && (0 == AsciiStriCmp(pReplace, pName + 1)))
        {
            return TRUE;
        }
    }

    return FALSE;
}

STATIC EFI_STATUS EFIAPI ventoy_wrapper_file_open
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
    CHAR8 OldName[256];
    ventoy_virt_chunk *virt = NULL;
    ventoy_grub_param_file_replace *replace = NULL;

    debug("## ventoy_wrapper_file_open <%s> ", Name);

    if ((Mode & EFI_FILE_MODE_WRITE) > 0 && StrCmp(Name, L"\\loader\\random-seed") == 0)
    {
        if (gDebugPrint)
        {
            debug("## ventoy_wrapper_file_open return NOT_FOUND for random-seed %lx", Mode);
            sleep(3);
        }
        return EFI_NOT_FOUND;
    }

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
            if (ventoy_replace_name_match(g_file_replace_list[i].old_file_name[j], TmpName))
            {
                g_original_fclose(*New);
                *New = &g_efi_file_replace.WrapperHandle;
                ventoy_wrapper_file_procotol(*New, FALSE, 0);

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

        if (StrCmp(Name, L"\\EFI\\BOOT") == 0)
        {
            (*New)->Open = ventoy_wrapper_file_open;
        }
    }

    for (i = 0; g_img_replace_list && i < VTOY_MAX_CONF_REPLACE; i++)
    {
        replace = g_img_replace_list + i;
        if (replace->magic != GRUB_IMG_REPLACE_MAGIC || replace->new_file_virtual_id >= g_virt_chunk_num)
        {
            continue;
        }

        AsciiSPrint(TmpName, sizeof(TmpName), "%s", Name);
        for (j = 0; j < replace->old_file_cnt; j++)
        {
            AsciiStrCpyS(OldName, sizeof(OldName), replace->old_file_name[j]);
            if ((0 == AsciiStrCmp(OldName, TmpName)) ||
                 (AsciiStrnCmp(OldName, "\\loader\\entries\\", 16) == 0 && 
                  AsciiStrCmp(OldName + 16, TmpName) == 0
                  )  
                )
            {
                g_original_fclose(*New);
                *New = &(g_img_file_replace[i].WrapperHandle);
                ventoy_wrapper_file_procotol(*New, TRUE, i);

                virt = g_virt_chunk + replace->new_file_virtual_id;

                Sectors = (virt->mem_sector_end - virt->mem_sector_start) + (virt->remap_sector_end - virt->remap_sector_start);
                
                g_img_file_replace[i].BlockIoSectorStart = virt->mem_sector_start;
                g_img_file_replace[i].FileSizeBytes = Sectors * 2048;

                if (gDebugPrint)
                {
                    debug("## ventoy_wrapper_file_open2 <%s> BlockStart:%lu Sectors:%lu Bytes:%lu", Name,
                        g_img_file_replace[i].BlockIoSectorStart, Sectors, Sectors * 2048);
                    sleep(3);
                }
                
                return Status;
            }
        }
    }

    if (g_img_replace_list && StrCmp(Name, L"\\loader\\entries") == 0)
    {
        (*New)->Open = ventoy_wrapper_file_open;
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

EFI_STATUS EFIAPI ventoy_wrapper_push_openvolume(IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME OpenVolume)
{
    g_original_open_volume = OpenVolume;
    return EFI_SUCCESS;
}

#if 0
/* For auto skip Windows 'Press any key to boot from CD or DVD ...' */
#endif

STATIC EFI_STATUS EFIAPI ventoy_wrapper_read_key_ex
(
    IN  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This,
    OUT EFI_KEY_DATA                      *KeyData
)
{
    /* only hook once before BCD file read */
    if (g_keyboard_hook_count == 0 && g_blockio_bcd_read_done == FALSE)
    {
        g_keyboard_hook_count++;

        KeyData->Key.ScanCode = SCAN_DELETE;
        KeyData->Key.UnicodeChar = 0;
        KeyData->KeyState.KeyShiftState = 0;
        KeyData->KeyState.KeyToggleState = 0;
        
        return EFI_SUCCESS;
    }
    
    return g_org_read_key_ex(This, KeyData);
}

EFI_STATUS EFIAPI ventoy_wrapper_read_key
(
    IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL       *This,
    OUT EFI_INPUT_KEY                       *Key
)
{
    /* only hook once before BCD file read */
    if (g_keyboard_hook_count == 0 && g_blockio_bcd_read_done == FALSE)
    {
        g_keyboard_hook_count++;

        Key->ScanCode = SCAN_DELETE;
        Key->UnicodeChar = 0;
        return EFI_SUCCESS;
    }

    return g_org_read_key(This, Key);
}

EFI_STATUS ventoy_hook_keyboard_start(VOID)
{
    g_blockio_start_record_bcd = TRUE;
    g_blockio_bcd_read_done = FALSE;
    g_keyboard_hook_count = 0;

    if (g_con_simple_input_ex)
    {
        g_org_read_key_ex = g_con_simple_input_ex->ReadKeyStrokeEx;
        g_con_simple_input_ex->ReadKeyStrokeEx = ventoy_wrapper_read_key_ex;
    }

    g_org_read_key = gST->ConIn->ReadKeyStroke;
    gST->ConIn->ReadKeyStroke = ventoy_wrapper_read_key;
    
    return EFI_SUCCESS;
}

EFI_STATUS ventoy_hook_keyboard_stop(VOID)
{
    g_blockio_start_record_bcd = FALSE;
    g_blockio_bcd_read_done = FALSE;
    g_keyboard_hook_count = 0;

    if (g_con_simple_input_ex)
    {
        g_con_simple_input_ex->ReadKeyStrokeEx = g_org_read_key_ex;
    }

    gST->ConIn->ReadKeyStroke = g_org_read_key;

    return EFI_SUCCESS;
}

#if 0
/* Fixup the 1st cdrom influnce for Windows boot */
#endif

STATIC EFI_STATUS EFIAPI ventoy_wrapper_locate_handle
(
    IN     EFI_LOCATE_SEARCH_TYPE    SearchType,
    IN     EFI_GUID                 *Protocol,    OPTIONAL
    IN     VOID                     *SearchKey,   OPTIONAL
    IN OUT UINTN                    *BufferSize,
    OUT    EFI_HANDLE               *Buffer
)
{
    UINTN i;
    EFI_HANDLE Handle = NULL;
    EFI_STATUS Status = EFI_SUCCESS;

    Status = g_org_locate_handle(SearchType, Protocol, SearchKey, BufferSize, Buffer);
    
    if (EFI_SUCCESS == Status && Protocol && CompareGuid(&gEfiBlockIoProtocolGuid, Protocol))
    {
        for (i = 0; i < (*BufferSize) / sizeof(EFI_HANDLE); i++)
        {
            if (Buffer[i] == gBlockData.Handle)
            {
                Handle = Buffer[0];
                Buffer[0] = Buffer[i];
                Buffer[i] = Handle;
                break;
            }
        }
    }

    return Status;
}

EFI_STATUS ventoy_hook_1st_cdrom_start(VOID)
{
    g_org_locate_handle = gBS->LocateHandle;
    gBS->LocateHandle = ventoy_wrapper_locate_handle;
    
    return EFI_SUCCESS;
}

EFI_STATUS ventoy_hook_1st_cdrom_stop(VOID)
{
    gBS->LocateHandle = g_org_locate_handle;
    g_org_locate_handle = NULL;
    
    return EFI_SUCCESS;
}

