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

static int g_vhdboot_bcd_offset = 0;
static int g_vhdboot_bcd_len = 0;
static int g_vhdboot_isolen = 0;
static char *g_vhdboot_totbuf = NULL;
static char *g_vhdboot_isobuf = NULL;

static int ventoy_vhd_find_bcd(int *bcdoffset, int *bcdlen)
{
    grub_uint32_t offset;
    grub_file_t file;
    char cmdbuf[128];
    
    grub_snprintf(cmdbuf, sizeof(cmdbuf), "loopback vhdiso mem:0x%lx:size:%d", (ulong)g_vhdboot_isobuf, g_vhdboot_isolen);
    
    grub_script_execute_sourcecode(cmdbuf);

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", "(vhdiso)/boot/bcd");
    if (!file)
    {
        grub_printf("Failed to open bcd file in the image file\n");
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

static int ventoy_vhd_patch_path(char *vhdpath, ventoy_patch_vhd *patch1, ventoy_patch_vhd *patch2)
{
    int i;
    int cnt = 0;
    char *pos;
    grub_size_t pathlen;
    const char *plat;
    grub_uint16_t *unicode_path;
    const grub_uint8_t winloadexe[] = 
    {
        0x77, 0x00, 0x69, 0x00, 0x6E, 0x00, 0x6C, 0x00, 0x6F, 0x00, 0x61, 0x00, 0x64, 0x00, 0x2E, 0x00,
        0x65, 0x00, 0x78, 0x00, 0x65, 0x00 
    };

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
        pos = g_vhdboot_isobuf + g_vhdboot_bcd_offset;
    
        /* winload.exe ==> winload.efi */
        for (i = 0; i + (int)sizeof(winloadexe) < g_vhdboot_bcd_len; i++)
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

    for (pos = vhdpath; *pos; pos++)
    {
        if (*pos == '/')
        {
            *pos = '\\';
        }
    }
    
    grub_utf8_to_utf16(unicode_path, pathlen, (grub_uint8_t *)vhdpath, -1, NULL);
    grub_memcpy(patch1->vhd_file_path, unicode_path, pathlen);
    grub_memcpy(patch2->vhd_file_path, unicode_path, pathlen);

    return 0;
}

static int ventoy_vhd_patch_disk(ventoy_patch_vhd *patch1, ventoy_patch_vhd *patch2)
{
    char efipart[16] = {0};

    grub_memcpy(efipart, g_ventoy_part_info->Head.Signature, sizeof(g_ventoy_part_info->Head.Signature));

    debug("part1 type: 0x%x <%s>\n", g_ventoy_part_info->MBR.PartTbl[0].FsFlag, efipart);

    if (grub_strncmp(efipart, "EFI PART", 8) == 0)
    {
        ventoy_debug_dump_guid("GPT disk GUID: ", g_ventoy_part_info->Head.DiskGuid);
        ventoy_debug_dump_guid("GPT part GUID: ", g_ventoy_part_info->PartTbl[0].PartGuid);
        
        grub_memcpy(patch1->disk_signature_or_guid, g_ventoy_part_info->Head.DiskGuid, 16);
        grub_memcpy(patch1->part_offset_or_guid, g_ventoy_part_info->PartTbl[0].PartGuid, 16);
        grub_memcpy(patch2->disk_signature_or_guid, g_ventoy_part_info->Head.DiskGuid, 16);
        grub_memcpy(patch2->part_offset_or_guid, g_ventoy_part_info->PartTbl[0].PartGuid, 16);

        patch1->part_type = patch2->part_type = 0;
    }
    else
    {
        debug("MBR disk signature: %02x%02x%02x%02x\n",
            g_ventoy_part_info->MBR.BootCode[0x1b8 + 0], g_ventoy_part_info->MBR.BootCode[0x1b8 + 1],
            g_ventoy_part_info->MBR.BootCode[0x1b8 + 2], g_ventoy_part_info->MBR.BootCode[0x1b8 + 3]);
        grub_memcpy(patch1->disk_signature_or_guid, g_ventoy_part_info->MBR.BootCode + 0x1b8, 4);
        grub_memcpy(patch2->disk_signature_or_guid, g_ventoy_part_info->MBR.BootCode + 0x1b8, 4);
    }

    return 0;
}

grub_err_t ventoy_cmd_patch_vhdboot(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc;
    ventoy_patch_vhd *patch1;
    ventoy_patch_vhd *patch2;
    char envbuf[64];

    (void)ctxt;
    (void)argc;

    grub_env_unset("vtoy_vhd_buf_addr");

    debug("patch vhd <%s>\n", args[0]);

    if ((!g_vhdboot_enable) || (!g_vhdboot_totbuf))
    {
        debug("vhd boot not ready %d %p\n", g_vhdboot_enable, g_vhdboot_totbuf);
        return 0;
    }

    rc = ventoy_vhd_find_bcd(&g_vhdboot_bcd_offset, &g_vhdboot_bcd_len);
    if (rc)
    {
        debug("failed to get bcd location %d\n", rc);
        return 0;
    }

    patch1 = (ventoy_patch_vhd *)(g_vhdboot_isobuf + g_vhdboot_bcd_offset + 0x495a);
    patch2 = (ventoy_patch_vhd *)(g_vhdboot_isobuf + g_vhdboot_bcd_offset + 0x50aa);

    ventoy_vhd_patch_disk(patch1, patch2);
    ventoy_vhd_patch_path(args[0], patch1, patch2);

    /* set buffer and size */
#ifdef GRUB_MACHINE_EFI
    grub_snprintf(envbuf, sizeof(envbuf), "0x%lx", (ulong)g_vhdboot_totbuf);
    grub_env_set("vtoy_vhd_buf_addr", envbuf);
    grub_snprintf(envbuf, sizeof(envbuf), "%d", (int)(g_vhdboot_isolen + sizeof(ventoy_chain_head)));
    grub_env_set("vtoy_vhd_buf_size", envbuf);
#else
    grub_snprintf(envbuf, sizeof(envbuf), "0x%lx", (ulong)g_vhdboot_isobuf);
    grub_env_set("vtoy_vhd_buf_addr", envbuf);
    grub_snprintf(envbuf, sizeof(envbuf), "%d", g_vhdboot_isolen);
    grub_env_set("vtoy_vhd_buf_size", envbuf);
#endif

    return 0;
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

