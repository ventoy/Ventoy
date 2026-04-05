/******************************************************************************
 * ventoy_vhd.c 
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

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/disk.h>
#include <grub/device.h>
#include <grub/term.h>
#include <grub/partition.h>
#include <grub/file.h>
#include <grub/normal.h>
#include <grub/extcmd.h>
#include <grub/datetime.h>
#include <grub/i18n.h>
#include <grub/net.h>
#include <grub/time.h>
#include <grub/crypto.h>
#include <grub/charset.h>
#ifdef GRUB_MACHINE_EFI
#include <grub/efi/efi.h>
#endif
#include <grub/ventoy.h>
#include "ventoy_def.h"

GRUB_MOD_LICENSE ("GPLv3+");

static int g_vhdboot_isolen = 0;
static char *g_vhdboot_totbuf = NULL;
static char *g_vhdboot_isobuf = NULL;
static grub_uint64_t g_img_trim_head_secnum = 0;

static int ventoy_vhd_find_bcd(int *bcdoffset, int *bcdlen, const char *path)
{
    grub_uint32_t offset;
    grub_file_t file;
    char cmdbuf[128];
    
    grub_snprintf(cmdbuf, sizeof(cmdbuf), "loopback vhdiso mem:0x%lx:size:%d", (ulong)g_vhdboot_isobuf, g_vhdboot_isolen);
    
    grub_script_execute_sourcecode(cmdbuf);

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "(vhdiso)%s", path);
    if (!file)
    {
        return 1;
    }

    grub_file_read(file, &offset, 4);
    offset = (grub_uint32_t)grub_iso9660_get_last_read_pos(file);

    *bcdoffset = (int)offset;
    *bcdlen = (int)file->size;

    debug("vhdiso bcd file offset:%d len:%d\n", *bcdoffset, *bcdlen);
    
    grub_file_close(file);
    
    grub_script_execute_sourcecode("loopback -d vhdiso");

    return 0;
}

static int ventoy_vhd_patch_path(char *vhdpath, ventoy_patch_vhd *patch1, ventoy_patch_vhd *patch2, 
    int bcdoffset, int bcdlen)
{
    int i;
    int cnt = 0;
    char *pos;
    grub_size_t pathlen;
    const char *plat;
    char *newpath = NULL;
    grub_uint16_t *unicode_path;
    const grub_uint8_t winloadexe[] = 
    {
        0x77, 0x00, 0x69, 0x00, 0x6E, 0x00, 0x6C, 0x00, 0x6F, 0x00, 0x61, 0x00, 0x64, 0x00, 0x2E, 0x00,
        0x65, 0x00, 0x78, 0x00, 0x65, 0x00 
    };

    while ((*vhdpath) != '/')
    {
        vhdpath++;
    }

    pathlen = sizeof(grub_uint16_t) * (grub_strlen(vhdpath) + 1);
    debug("unicode path for <%s> len:%d\n", vhdpath, (int)pathlen);
    
    unicode_path = grub_zalloc(pathlen);
    if (!unicode_path)
    {
        return 0;
    }

    plat = grub_env_get("grub_platform");

    if (plat && (plat[0] == 'e')) /* UEFI */
    {
        pos = g_vhdboot_isobuf + bcdoffset;
    
        /* winload.exe ==> winload.efi */
        for (i = 0; i + (int)sizeof(winloadexe) < bcdlen; i++)
        {
            if (*((grub_uint32_t *)(pos + i)) == 0x00690077 && 
                grub_memcmp(pos + i, winloadexe, sizeof(winloadexe)) == 0)
            {
                pos[i + sizeof(winloadexe) - 4] = 0x66;
                pos[i + sizeof(winloadexe) - 2] = 0x69;
                cnt++;
            }
        }

        debug("winload patch %d times\n", cnt);
    }

    newpath = grub_strdup(vhdpath);
    for (pos = newpath; *pos; pos++)
    {
        if (*pos == '/')
        {
            *pos = '\\';
        }
    }
    
    grub_utf8_to_utf16(unicode_path, pathlen, (grub_uint8_t *)newpath, -1, NULL);
    grub_memcpy(patch1->vhd_file_path, unicode_path, pathlen);
    grub_memcpy(patch2->vhd_file_path, unicode_path, pathlen);

    grub_free(newpath);
    return 0;
}

static int ventoy_vhd_read_parttbl(const char *filename, ventoy_gpt_info *gpt, int *index, grub_uint64_t *poffset)
{
    int i;
    int find = 0;
    int ret = 1;
    grub_uint64_t start;
    grub_file_t file = NULL;
    grub_disk_t disk = NULL;
    grub_uint8_t zeroguid[16] = {0};

    file = grub_file_open(filename, VENTOY_FILE_TYPE);
    if (!file)
    {
        goto end;
    }

    disk = grub_disk_open(file->device->disk->name);
    if (!disk)
    {
        goto end;
    }

    grub_disk_read(disk, 0, 0, sizeof(ventoy_gpt_info), gpt);

    start = file->device->disk->partition->start;

    if (grub_memcmp(gpt->Head.Signature, "EFI PART", 8) == 0)
    {
        debug("GPT part start: %llu\n", (ulonglong)start);
        for (i = 0; i < 128; i++)
        {
            if (grub_memcmp(gpt->PartTbl[i].PartGuid, zeroguid, 16))
            {
                if (start == gpt->PartTbl[i].StartLBA)
                {
                    *index = i;
                    find = 1;
                    break;
                }
            }
        }
    }
    else
    {
        debug("MBR part start: %llu\n", (ulonglong)start);
        for (i = 0; i < 4; i++)
        {
            if ((grub_uint32_t)start == gpt->MBR.PartTbl[i].StartSectorId)
            {
                *index = i;
                find = 1;
                break;
            }
        }
    }    

    if (find == 0) // MBR Logical partition
    {
        if (file->device->disk->partition->number > 0)
        {
            *index = file->device->disk->partition->number;
            debug("Fall back part number: %d\n", *index);
        }
    }

    *poffset = start;
    ret = 0;

end:
    check_free(file, grub_file_close);
    check_free(disk, grub_disk_close);

    return ret;
}

static int ventoy_vhd_patch_disk(const char *vhdpath, ventoy_patch_vhd *patch1, ventoy_patch_vhd *patch2)
{
    int partIndex = 0;
    grub_uint64_t offset = 0;
    char efipart[16] = {0};
    ventoy_gpt_info *gpt = NULL;

    if (vhdpath[0] == '/')
    {
        gpt = g_ventoy_part_info;  
        partIndex = 0;
        debug("This is Ventoy ISO partIndex %d %s\n", partIndex, vhdpath);
    }
    else
    {
        gpt = grub_zalloc(sizeof(ventoy_gpt_info));
        ventoy_vhd_read_parttbl(vhdpath, gpt, &partIndex, &offset);
        debug("This is HDD partIndex %d %s\n", partIndex, vhdpath);
    }

    grub_memcpy(efipart, gpt->Head.Signature, sizeof(gpt->Head.Signature));

    grub_memset(patch1, 0, OFFSET_OF(ventoy_patch_vhd, vhd_file_path));
    grub_memset(patch2, 0, OFFSET_OF(ventoy_patch_vhd, vhd_file_path));

    if (grub_strncmp(efipart, "EFI PART", 8) == 0)
    {
        ventoy_debug_dump_guid("GPT disk GUID: ", gpt->Head.DiskGuid);
        ventoy_debug_dump_guid("GPT partIndex GUID: ", gpt->PartTbl[partIndex].PartGuid);
        
        grub_memcpy(patch1->disk_signature_or_guid, gpt->Head.DiskGuid, 16);
        grub_memcpy(patch1->part_offset_or_guid, gpt->PartTbl[partIndex].PartGuid, 16);
        grub_memcpy(patch2->disk_signature_or_guid, gpt->Head.DiskGuid, 16);
        grub_memcpy(patch2->part_offset_or_guid, gpt->PartTbl[partIndex].PartGuid, 16);

        patch1->part_type = patch2->part_type = 0;
    }
    else
    {
        if (offset == 0)
        {
            offset = gpt->MBR.PartTbl[partIndex].StartSectorId;
        }
        offset *= 512;
        debug("MBR disk signature: %02x%02x%02x%02x Part(%d) offset:%llu\n",
            gpt->MBR.BootCode[0x1b8 + 0], gpt->MBR.BootCode[0x1b8 + 1],
            gpt->MBR.BootCode[0x1b8 + 2], gpt->MBR.BootCode[0x1b8 + 3],
            partIndex + 1, offset);

        grub_memcpy(patch1->part_offset_or_guid, &offset, 8);
        grub_memcpy(patch2->part_offset_or_guid, &offset, 8);
        
        grub_memcpy(patch1->disk_signature_or_guid, gpt->MBR.BootCode + 0x1b8, 4);
        grub_memcpy(patch2->disk_signature_or_guid, gpt->MBR.BootCode + 0x1b8, 4);

        patch1->part_type = patch2->part_type = 1;
    }

    if (gpt != g_ventoy_part_info)
    {
        grub_free(gpt);
    }

    return 0;
}

static int ventoy_find_vhdpatch_offset(int bcdoffset, int bcdlen, int *offset)
{
    int i;
    int cnt = 0;
    grub_uint8_t *buf = (grub_uint8_t *)(g_vhdboot_isobuf + bcdoffset);
    grub_uint8_t magic[16] = { 
        0x5C, 0x00, 0x58, 0x00, 0x58, 0x00, 0x58, 0x00, 0x58, 0x00, 0x58, 0x00, 0x58, 0x00, 0x58, 0x00 
    };

    for (i = 0; i < bcdlen - 16 && cnt < 2; i++)
    {
        if (*(grub_uint32_t *)(buf + i) == 0x0058005C)
        {
            if (grub_memcmp(magic, buf + i, 16) == 0)
            {
                *offset++ = i - (int)OFFSET_OF(ventoy_patch_vhd, vhd_file_path); 
                cnt++;
            }
        }
    }

    return 0;
}

grub_err_t ventoy_cmd_patch_vhdboot(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc;
    int bcdoffset, bcdlen;
    int patchoffset[2];
    ventoy_patch_vhd *patch1;
    ventoy_patch_vhd *patch2;

    (void)ctxt;
    (void)argc;

    grub_env_unset("vtoy_vhd_buf_addr");

    debug("patch vhd <%s>\n", args[0]);

    if ((!g_vhdboot_enable) || (!g_vhdboot_totbuf))
    {
        debug("vhd boot not ready %d %p\n", g_vhdboot_enable, g_vhdboot_totbuf);
        return 0;
    }

    rc = ventoy_vhd_find_bcd(&bcdoffset, &bcdlen, "/boot/bcd");
    if (rc)
    {
        debug("failed to get bcd location %d\n", rc);
    }
    else
    {
        ventoy_find_vhdpatch_offset(bcdoffset, bcdlen, patchoffset);
        patch1 = (ventoy_patch_vhd *)(g_vhdboot_isobuf + bcdoffset + patchoffset[0]);
        patch2 = (ventoy_patch_vhd *)(g_vhdboot_isobuf + bcdoffset + patchoffset[1]);

        debug("Find /boot/bcd (%d %d) now patch it (offset: 0x%x 0x%x) ...\n", 
              bcdoffset, bcdlen, patchoffset[0], patchoffset[1]);
        ventoy_vhd_patch_disk(args[0], patch1, patch2);
        ventoy_vhd_patch_path(args[0], patch1, patch2, bcdoffset, bcdlen);
    }

    rc = ventoy_vhd_find_bcd(&bcdoffset, &bcdlen, "/boot/BCD");
    if (rc)
    {
        debug("No file /boot/BCD \n");
    }
    else
    {
        ventoy_find_vhdpatch_offset(bcdoffset, bcdlen, patchoffset);
        patch1 = (ventoy_patch_vhd *)(g_vhdboot_isobuf + bcdoffset + patchoffset[0]);
        patch2 = (ventoy_patch_vhd *)(g_vhdboot_isobuf + bcdoffset + patchoffset[1]);
        
        debug("Find /boot/BCD (%d %d) now patch it (offset: 0x%x 0x%x) ...\n", 
              bcdoffset, bcdlen, patchoffset[0], patchoffset[1]);
        ventoy_vhd_patch_disk(args[0], patch1, patch2);
        ventoy_vhd_patch_path(args[0], patch1, patch2, bcdoffset, bcdlen);
    }

    /* set buffer and size */
#ifdef GRUB_MACHINE_EFI
    ventoy_memfile_env_set("vtoy_vhd_buf", g_vhdboot_totbuf, (ulonglong)(g_vhdboot_isolen + sizeof(ventoy_chain_head)));
#else
    ventoy_memfile_env_set("vtoy_vhd_buf", g_vhdboot_isobuf, (ulonglong)g_vhdboot_isolen);
#endif

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_load_vhdboot(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int buflen;
    grub_file_t file;
    
    (void)ctxt;
    (void)argc;

    g_vhdboot_enable = 0;
    grub_check_free(g_vhdboot_totbuf);

    file = grub_file_open(args[0], VENTOY_FILE_TYPE);
    if (!file)
    {
        return 0;
    }

    debug("load vhd boot: <%s> <%lu>\n", args[0], (ulong)file->size);

    if (file->size < VTOY_SIZE_1KB * 32)
    {
        grub_file_close(file);
        return 0;
    }

    g_vhdboot_isolen = (int)file->size;

    buflen = (int)(file->size + sizeof(ventoy_chain_head));

#ifdef GRUB_MACHINE_EFI
    g_vhdboot_totbuf = (char *)grub_efi_allocate_iso_buf(buflen);
#else
    g_vhdboot_totbuf = (char *)grub_malloc(buflen);
#endif
    
    if (!g_vhdboot_totbuf)
    {
        grub_file_close(file);
        return 0;
    }

    g_vhdboot_isobuf = g_vhdboot_totbuf + sizeof(ventoy_chain_head);

    grub_file_read(file, g_vhdboot_isobuf, file->size);
    grub_file_close(file);

    g_vhdboot_enable = 1;

    return 0;
}

static int ventoy_raw_trim_head(grub_uint64_t offset)
{
    grub_uint32_t i;
    grub_uint32_t memsize;
    grub_uint32_t imgstart = 0;
    grub_uint32_t imgsecs = 0;
    grub_uint64_t sectors = 0;
    grub_uint64_t cursecs = 0;
    grub_uint64_t delta = 0;

    if ((!g_img_chunk_list.chunk) || (!offset))
    {
        debug("image chunk not ready %p %lu\n", g_img_chunk_list.chunk, (ulong)offset);
        return 0;
    }

    debug("image trim head %lu\n", (ulong)offset);

    for (i = 0; i < g_img_chunk_list.cur_chunk; i++)
    {
        cursecs = g_img_chunk_list.chunk[i].disk_end_sector + 1 - g_img_chunk_list.chunk[i].disk_start_sector;
        sectors += cursecs;
        if (sectors >= offset)
        {
            delta = cursecs - (sectors - offset);
            break;
        }
    }

    if (sectors < offset || i >= g_img_chunk_list.cur_chunk)
    {
        debug("Invalid size %lu %lu\n", (ulong)sectors, (ulong)offset);
        return 0;
    }

    if (sectors == offset)
    {
        memsize = (g_img_chunk_list.cur_chunk - (i + 1)) * sizeof(ventoy_img_chunk);
        grub_memmove(g_img_chunk_list.chunk, g_img_chunk_list.chunk + i + 1, memsize);
        g_img_chunk_list.cur_chunk -= (i + 1);
    }
    else
    {
        g_img_chunk_list.chunk[i].disk_start_sector += delta;
        g_img_chunk_list.chunk[i].img_start_sector += (grub_uint32_t)(delta / 4);
    
        if (i > 0)
        {
            memsize = (g_img_chunk_list.cur_chunk - i) * sizeof(ventoy_img_chunk);
            grub_memmove(g_img_chunk_list.chunk, g_img_chunk_list.chunk + i, memsize);
            g_img_chunk_list.cur_chunk -= i;
        }
    }

    for (i = 0; i < g_img_chunk_list.cur_chunk; i++)
    {
        imgsecs = g_img_chunk_list.chunk[i].img_end_sector + 1 - g_img_chunk_list.chunk[i].img_start_sector;        
        g_img_chunk_list.chunk[i].img_start_sector = imgstart;
        g_img_chunk_list.chunk[i].img_end_sector = imgstart + (imgsecs - 1);
        imgstart += imgsecs;
    }

    return 0;
}

grub_err_t ventoy_cmd_get_vtoy_type(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int altboot = 0;
    int offset = -1;
    grub_file_t file;
    grub_uint8_t data = 0;
    vhd_footer_t vhdfoot;
    VDIPREHEADER vdihdr;
    char type[16] = {0};
    ventoy_gpt_info *gpt = NULL;
    
    (void)ctxt;

    g_img_trim_head_secnum = 0;

    if (argc != 4)
    {
        return 0;
    }

    file = grub_file_open(args[0], VENTOY_FILE_TYPE);
    if (!file)
    {
        debug("Failed to open file %s\n", args[0]);
        return 0;
    }

    grub_snprintf(type, sizeof(type), "unknown");
    
    grub_file_seek(file, file->size - 512);
    grub_file_read(file, &vhdfoot, sizeof(vhdfoot));

    if (grub_strncmp(vhdfoot.cookie, "conectix", 8) == 0)
    {
        offset = 0;
        grub_snprintf(type, sizeof(type), "vhd%u", grub_swap_bytes32(vhdfoot.disktype));
    }
    else
    {
        grub_file_seek(file, 0);
        grub_file_read(file, &vdihdr, sizeof(vdihdr));
        if (vdihdr.u32Signature == VDI_IMAGE_SIGNATURE)            
        {
            grub_snprintf(type, sizeof(type), "vdi");
            if (grub_strncmp(vdihdr.szFileInfo, VDI_IMAGE_FILE_INFO, grub_strlen(VDI_IMAGE_FILE_INFO)) == 0)
            {
                offset = 2 * 1048576;
                g_img_trim_head_secnum = offset / 512;
                debug("VDI V1\n");
            }
            else if (grub_strncmp(vdihdr.szFileInfo, VDI_IMAGE_FILE_INFO2, grub_strlen(VDI_IMAGE_FILE_INFO2)) == 0)
            {
                offset = 2 * 1048576;
                g_img_trim_head_secnum = offset / 512;
                debug("VDI V2\n");
            }
            else
            {
                debug("invalid file info <%s>\n", vdihdr.szFileInfo);
            }
        }
        else
        {
            offset = 0;
            grub_snprintf(type, sizeof(type), "raw");
        }
    }

    grub_env_set(args[1], type);
    debug("<%s> vtoy type: <%s> offset:%d\n", args[0], type, offset);
    
    if (offset >= 0)
    {
        gpt = grub_zalloc(sizeof(ventoy_gpt_info));
        if (!gpt)
        {
            grub_env_set(args[1], "unknown");
            goto end;
        }
    
        grub_file_seek(file, offset);
        grub_file_read(file, gpt, sizeof(ventoy_gpt_info));

        if (gpt->MBR.Byte55 != 0x55 || gpt->MBR.ByteAA != 0xAA)
        {
            grub_env_set(args[1], "unknown");
            debug("invalid mbr signature: 0x%x 0x%x offset=%d\n", gpt->MBR.Byte55, gpt->MBR.ByteAA, offset);
            goto end;
        }

        if (grub_memcmp(gpt->Head.Signature, "EFI PART", 8) == 0)
        {
            grub_env_set(args[2], "gpt");
            debug("part type: %s\n", "GPT");

            if (gpt->MBR.PartTbl[0].FsFlag == 0xEE)
            {
                for (i = 0; i < 128; i++)
                {
                    if (grub_memcmp(gpt->PartTbl[i].PartType, "Hah!IdontNeedEFI", 16) == 0)
                    {
                        debug("part %d is grub_bios part\n", i);
                        altboot = 1;
                        grub_env_set(args[3], "1");
                        break;
                    }
                    else if (gpt->PartTbl[i].LastLBA == 0)
                    {
                        break;
                    }
                }
            }

            if (!altboot)
            {
                if (gpt->MBR.BootCode[92] == 0x22)
                {
                    grub_file_seek(file, offset + 17908);
                    grub_file_read(file, &data, 1);
                    if (data == 0x23)
                    {
                        altboot = 1;
                        grub_env_set(args[3], "1");                        
                    }
                    else
                    {
                        debug("offset data=0x%x\n", data);
                    }
                }
                else
                {
                    debug("BootCode: 0x%x\n", gpt->MBR.BootCode[92]);
                }
            }
        }
        else
        {
            grub_env_set(args[2], "mbr");
            debug("part type: %s\n", "MBR");

            for (i = 0; i < 4; i++)
            {
                if (gpt->MBR.PartTbl[i].FsFlag == 0xEF)
                {
                    debug("part %d is esp part in MBR mode\n", i);
                    altboot = 1;
                    grub_env_set(args[3], "1");
                    break;
                }
            }
        }
    }
    else
    {
        debug("part type: %s\n", "xxx");
    }

end:
    grub_check_free(gpt);
    grub_file_close(file);
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);    
}

grub_err_t ventoy_cmd_raw_chain_data(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t size = 0;
    grub_uint32_t img_chunk_size = 0;
    grub_file_t file;
    grub_disk_t disk;
    const char *pLastChain = NULL;
    ventoy_chain_head *chain;
    
    (void)ctxt;
    (void)argc;

    if (NULL == g_img_chunk_list.chunk)
    {
        grub_printf("ventoy not ready\n");
        return 1;
    }

    if (g_img_trim_head_secnum > 0)
    {
        ventoy_raw_trim_head(g_img_trim_head_secnum);
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        return 1;
    }

    if (grub_strncmp(args[0], g_iso_path, grub_strlen(g_iso_path)))
    {
        file->vlnk = 1;
    }

    img_chunk_size = g_img_chunk_list.cur_chunk * sizeof(ventoy_img_chunk);
    
    size = sizeof(ventoy_chain_head) + img_chunk_size;
    
    pLastChain = grub_env_get("vtoy_chain_mem_addr");
    if (pLastChain)
    {
        chain = (ventoy_chain_head *)grub_strtoul(pLastChain, NULL, 16);
        if (chain)
        {
            debug("free last chain memory %p\n", chain);
            grub_free(chain);
        }
    }

    chain = ventoy_alloc_chain(size);
    if (!chain)
    {
        grub_printf("Failed to alloc chain raw memory size %u\n", size);
        grub_file_close(file);
        return 1;
    }

    ventoy_memfile_env_set("vtoy_chain_mem", chain, (ulonglong)size);

    grub_env_export("vtoy_chain_mem_addr");
    grub_env_export("vtoy_chain_mem_size");

    grub_memset(chain, 0, sizeof(ventoy_chain_head));

    /* part 1: os parameter */
    g_ventoy_chain_type = ventoy_chain_linux;
    ventoy_fill_os_param(file, &(chain->os_param));

    /* part 2: chain head */
    disk = file->device->disk;
    chain->disk_drive = disk->id;
    chain->disk_sector_size = (1 << disk->log_sector_size);

    chain->real_img_size_in_bytes = file->size;
    if (g_img_trim_head_secnum > 0)
    {
        chain->real_img_size_in_bytes -= g_img_trim_head_secnum * 512;
    }
    
    chain->virt_img_size_in_bytes = chain->real_img_size_in_bytes;
    chain->boot_catalog = 0;

    /* part 3: image chunk */
    chain->img_chunk_offset = sizeof(ventoy_chain_head);
    chain->img_chunk_num = g_img_chunk_list.cur_chunk;
    grub_memcpy((char *)chain + chain->img_chunk_offset, g_img_chunk_list.chunk, img_chunk_size);

    grub_file_seek(file, g_img_trim_head_secnum * 512);
    grub_file_read(file, chain->boot_catalog_sector, 512);

    grub_file_close(file);
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}
