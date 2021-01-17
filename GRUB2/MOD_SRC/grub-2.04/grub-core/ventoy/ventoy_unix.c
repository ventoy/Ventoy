/******************************************************************************
 * ventoy_unix.c 
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
#include <grub/ventoy.h>
#include "ventoy_def.h"

GRUB_MOD_LICENSE ("GPLv3+");

char g_ko_mod_path[256];
int g_conf_new_len = 0;
char *g_conf_new_data = NULL;

int g_mod_new_len = 0;
char *g_mod_new_data = NULL;

grub_uint64_t g_mod_override_offset = 0;
grub_uint64_t g_conf_override_offset = 0;

static int ventoy_get_file_override(const char *filename, grub_uint64_t *offset)
{
    grub_file_t file;

    *offset = 0;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "(loop)%s", filename);
    if (!file)
    {
        return 1;
    }

    *offset = grub_iso9660_get_last_file_dirent_pos(file) + 2;
    
    grub_file_close(file);
    
    return 0;
}

static grub_uint32_t ventoy_unix_get_override_chunk_count(void)
{
    grub_uint32_t count = 0;

    if (g_conf_new_len > 0)
    {
        count++;
    }
    
    if (g_mod_new_len > 0)
    {
        count++;
    }

    return count;
}

static grub_uint32_t ventoy_unix_get_virt_chunk_count(void)
{
    grub_uint32_t count = 0;

    if (g_conf_new_len > 0)
    {
        count++;
    }
    
    if (g_mod_new_len > 0)
    {
        count++;
    }

    return count;
}
static grub_uint32_t ventoy_unix_get_virt_chunk_size(void)
{
    grub_uint32_t size;

    size = sizeof(ventoy_virt_chunk) * ventoy_unix_get_virt_chunk_count();

    if (g_conf_new_len > 0)
    {
        size += ventoy_align_2k(g_conf_new_len);
    }
    
    if (g_mod_new_len > 0)
    {
        size += ventoy_align_2k(g_mod_new_len);
    }

    return size;
}

static void ventoy_unix_fill_override_data(    grub_uint64_t isosize, void *override)
{
    grub_uint64_t sector;
    ventoy_override_chunk *cur;
    ventoy_iso9660_override *dirent;
    
    sector = (isosize + 2047) / 2048;

    cur = (ventoy_override_chunk *)override;

    if (g_conf_new_len > 0)
    {
        /* loader.conf */
        cur->img_offset = g_conf_override_offset;
        cur->override_size = sizeof(ventoy_iso9660_override);
        dirent = (ventoy_iso9660_override *)cur->override_data;
        dirent->first_sector    = (grub_uint32_t)sector;
        dirent->size            = (grub_uint32_t)g_conf_new_len;
        dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
        dirent->size_be         = grub_swap_bytes32(dirent->size);
        sector += (dirent->size + 2047) / 2048;
    }

    if (g_mod_new_len > 0)
    {
        /* mod.ko */
        cur++;
        cur->img_offset = g_mod_override_offset;
        cur->override_size = sizeof(ventoy_iso9660_override);
        dirent = (ventoy_iso9660_override *)cur->override_data;
        dirent->first_sector    = (grub_uint32_t)sector;
        dirent->size            = (grub_uint32_t)g_mod_new_len;
        dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
        dirent->size_be         = grub_swap_bytes32(dirent->size);
        sector += (dirent->size + 2047) / 2048;
    }

    return;
}

static void ventoy_unix_fill_virt_data(    grub_uint64_t isosize, ventoy_chain_head *chain)
{
    grub_uint64_t sector;
    grub_uint32_t offset;
    grub_uint32_t data_secs;
    char *override;
    ventoy_virt_chunk *cur;

    override = (char *)chain + chain->virt_chunk_offset;
    cur = (ventoy_virt_chunk *)override;
    
    sector = (isosize + 2047) / 2048;
    offset = 2 * sizeof(ventoy_virt_chunk);

    if (g_conf_new_len > 0)
    {
        ventoy_unix_fill_virt(g_conf_new_data, g_conf_new_len);
    }

    if (g_mod_new_len > 0)
    {
        ventoy_unix_fill_virt(g_mod_new_data, g_mod_new_len);
    }
    
    return;
}

static int ventoy_freebsd_append_conf(char *buf, const char *isopath)
{
    int pos = 0;
    grub_uint32_t i;
    grub_disk_t disk;
    grub_file_t isofile;
    char uuid[64] = {0};
    ventoy_img_chunk *chunk;
    grub_uint8_t disk_sig[4];
    grub_uint8_t disk_guid[16];

    debug("ventoy_freebsd_append_conf %s\n", isopath);
    
    isofile = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", isopath);
    if (!isofile)
    {
        return 1;
    }

    vtoy_ssprintf(buf, pos, "ventoy_load=\"%s\"\n", "YES");
    vtoy_ssprintf(buf, pos, "ventoy_name=\"%s\"\n", g_ko_mod_path);

    disk = isofile->device->disk;

    ventoy_get_disk_guid(isofile->name, disk_guid, disk_sig);

    for (i = 0; i < 16; i++)
    {
        grub_snprintf(uuid + i * 2, sizeof(uuid), "%02x", disk_guid[i]);
    }

    vtoy_ssprintf(buf, pos, "hint.ventoy.0.disksize=%llu\n", (ulonglong)(disk->total_sectors * (1 << disk->log_sector_size)));
    vtoy_ssprintf(buf, pos, "hint.ventoy.0.diskuuid=\"%s\"\n", uuid);
    vtoy_ssprintf(buf, pos, "hint.ventoy.0.disksignature=%02x%02x%02x%02x\n", disk_sig[0], disk_sig[1], disk_sig[2], disk_sig[3]);
    vtoy_ssprintf(buf, pos, "hint.ventoy.0.segnum=%u\n", g_img_chunk_list.cur_chunk);

    for (i = 0; i < g_img_chunk_list.cur_chunk; i++)
    {
        chunk = g_img_chunk_list.chunk + i;
        vtoy_ssprintf(buf, pos, "hint.ventoy.%u.seg=\"0x%llx@0x%llx\"\n", 
            i, (ulonglong)(chunk->disk_start_sector * 512),
            (ulonglong)((chunk->disk_end_sector + 1) * 512));
    }

    grub_file_close(isofile);

    return pos;
}

static int ventoy_dragonfly_append_conf(char *buf, const char *isopath)
{
    int pos = 0;

    debug("ventoy_dragonfly_append_conf %s\n", isopath);

    vtoy_ssprintf(buf, pos, "tmpfs_load=\"%s\"\n", "YES");
    vtoy_ssprintf(buf, pos, "dm_target_linear_load=\"%s\"\n", "YES");
    vtoy_ssprintf(buf, pos, "initrd.img_load=\"%s\"\n", "YES");
    vtoy_ssprintf(buf, pos, "initrd.img_type=\"%s\"\n", "md_image");
    vtoy_ssprintf(buf, pos, "vfs.root.mountfrom=\"%s\"\n", "ufs:md0s0");

    return pos;
}

grub_err_t ventoy_cmd_unix_reset(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;
    
    g_conf_new_len = 0;
    g_mod_new_len = 0;
    g_mod_override_offset = 0;
    g_conf_override_offset = 0;

    check_free(g_mod_new_data, grub_free);
    check_free(g_conf_new_data, grub_free);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_parse_freenas_ver(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_file_t file;
    const char *ver = NULL;
    char *buf = NULL;
    VTOY_JSON *json = NULL;
    
    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        debug("Failed to open file %s\n", args[0]);
        return 1;
    }

    buf = grub_malloc(file->size + 2);
    if (!buf)
    {
        grub_file_close(file);
        return 0;
    }
    grub_file_read(file, buf, file->size);
    buf[file->size] = 0;

    json = vtoy_json_create();
    if (!json)
    {
        goto end;
    }

    if (vtoy_json_parse(json, buf))
    {
        goto end;
    }

    ver = vtoy_json_get_string_ex(json->pstChild, "Version");
    if (ver)
    {
        debug("freenas version:<%s>\n", ver);
        ventoy_set_env(args[1], ver);
    }
    else
    {
        debug("freenas version:<%s>\n", "NOT FOUND");
        grub_env_unset(args[1]);
    }

end:
    grub_check_free(buf);
    check_free(json, vtoy_json_destroy);
    grub_file_close(file);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_unix_freebsd_ver(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_file_t file;
    char *buf;
    char *start = NULL;
    char *nextline = NULL;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    file = ventoy_grub_file_open(GRUB_FILE_TYPE_LINUX_INITRD, "%s", args[0]);
    if (!file)
    {
        debug("Failed to open file %s\n", args[0]);
        return 1;
    }

    buf = grub_zalloc(file->size + 2);
    if (!buf)
    {
        grub_file_close(file);
        return 0;
    }
    grub_file_read(file, buf, file->size);

    for (start = buf; start; start = nextline)
    {
        if (grub_strncmp(start, "USERLAND_VERSION", 16) == 0)
        {
            nextline = start;
            while (*nextline && *nextline != '\r' && *nextline != '\n')
            {
                nextline++;
            }

            *nextline = 0;            
            break;
        }
        nextline = ventoy_get_line(start);
    }

    if (start)
    {
        debug("freebsd version:<%s>\n", start);
        ventoy_set_env(args[1], start);
    }
    else
    {
        debug("freebsd version:<%s>\n", "NOT FOUND");
        grub_env_unset(args[1]);
    }

    grub_free(buf);
    grub_file_close(file);
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_unix_replace_conf(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t i;
    char *data;
    grub_uint64_t offset;
    grub_file_t file;
    const char *confile = NULL;
    const char * loader_conf[] = 
    {
        "/boot/loader.conf",
        "/boot/defaults/loader.conf",
    };

    (void)ctxt;

    if (argc != 2)
    {
        debug("Replace conf invalid argc %d\n", argc);
        return 1;
    }
 
    for (i = 0; i < sizeof(loader_conf) / sizeof(loader_conf[0]); i++)
    {
        if (ventoy_get_file_override(loader_conf[i], &offset) == 0)
        {
            confile = loader_conf[i];
            g_conf_override_offset = offset;
            break;
        }
    }

    if (confile == NULL)
    {   
        debug("Can't find loader.conf file from %u locations\n", i);
        return 1;
    }

    file = ventoy_grub_file_open(GRUB_FILE_TYPE_LINUX_INITRD, "(loop)/%s", confile);
    if (!file)
    {
        debug("Failed to open %s \n", confile);
        return 1;
    }

    debug("old conf file size:%d\n", (int)file->size);

    data = grub_malloc(VTOY_MAX_SCRIPT_BUF);
    if (!data)
    {
        grub_file_close(file);    
        return 1;
    }

    grub_file_read(file, data, file->size);
    grub_file_close(file);
    
    g_conf_new_data = data;
    g_conf_new_len = (int)file->size;

    if (grub_strcmp(args[0], "FreeBSD") == 0)
    {
        g_conf_new_len += ventoy_freebsd_append_conf(data + file->size, args[1]);
    }
    else if (grub_strcmp(args[0], "DragonFly") == 0)
    {
        g_conf_new_len += ventoy_dragonfly_append_conf(data + file->size, args[1]);
    }
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_unix_replace_ko(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char *data;
    grub_uint64_t offset;
    grub_file_t file;

    (void)ctxt;

    if (argc != 2)
    {
        debug("Replace ko invalid argc %d\n", argc);
        return 1;
    }

    debug("replace ko %s\n", args[0]);

    if (ventoy_get_file_override(args[0], &offset) == 0)
    {
        grub_snprintf(g_ko_mod_path, sizeof(g_ko_mod_path), "%s", args[0]);
        g_mod_override_offset = offset;
    }
    else
    {   
        debug("Can't find replace ko file from %s\n", args[0]);
        return 1;
    }

    file = ventoy_grub_file_open(GRUB_FILE_TYPE_LINUX_INITRD, "%s", args[1]);
    if (!file)
    {
        debug("Failed to open %s \n", args[1]);
        return 1;
    }

    debug("new ko file size:%d\n", (int)file->size);

    data = grub_malloc(file->size);
    if (!data)
    {
        debug("Failed to alloc memory for new ko %d\n", (int)file->size);
        grub_file_close(file);    
        return 1;
    }

    grub_file_read(file, data, file->size);
    grub_file_close(file);
    
    g_mod_new_data = data;
    g_mod_new_len = (int)file->size;
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_unix_fill_image_desc(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    grub_uint8_t *byte;
    grub_uint32_t memsize;
    ventoy_image_desc *desc;
    grub_uint8_t flag[32] = {
        0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };

    (void)ctxt;
    (void)argc;
    (void)args;

    debug("ventoy_cmd_unix_fill_image_desc %p\n", g_mod_new_data);

    if (!g_mod_new_data)
    {
        goto end;
    }

    byte = (grub_uint8_t *)g_mod_new_data;
    for (i = 0; i < g_mod_new_len - 32; i += 16)
    {
        if (byte[i] == 0xFF && byte[i + 1] == 0xEE)
        {
            if (grub_memcmp(flag, byte + i, 32) == 0)
            {
                debug("Find position flag at %d(0x%x)\n", i, i);
                break;                
            }
        }
    }

    if (i >= g_mod_new_len - 32)
    {
        debug("Failed to find position flag %d\n", i);
        goto end;
    }

    desc = (ventoy_image_desc *)(byte + i);
    desc->disk_size = g_ventoy_disk_size;
    desc->part1_size = ventoy_get_part1_size(g_ventoy_part_info);
    grub_memcpy(desc->disk_uuid, g_ventoy_part_info->MBR.BootCode + 0x180, 16);
    grub_memcpy(desc->disk_signature, g_ventoy_part_info->MBR.BootCode + 0x1B8, 4);

    desc->img_chunk_count = g_img_chunk_list.cur_chunk;
    memsize = g_img_chunk_list.cur_chunk * sizeof(ventoy_img_chunk);

    debug("image chunk count:%u  memsize:%u\n", desc->img_chunk_count, memsize);

    if (memsize >= VTOY_SIZE_1MB * 8)
    {
        grub_printf("image chunk count:%u  memsize:%u too big\n", desc->img_chunk_count, memsize);
        goto end;
    }

    grub_memcpy(desc + 1, g_img_chunk_list.chunk, memsize);

end:
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_unix_gzip_newko(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int newlen;
    grub_uint8_t *buf;

    (void)ctxt;
    (void)argc;
    (void)args;

    debug("ventoy_cmd_unix_gzip_newko %p\n", g_mod_new_data);

    if (!g_mod_new_data)
    {
        goto end;
    }

    buf = grub_malloc(g_mod_new_len);
    if (!buf)
    {
        goto end;
    }

    newlen = ventoy_gzip_compress(g_mod_new_data, g_mod_new_len, buf, g_mod_new_len);

    grub_free(g_mod_new_data);

    debug("gzip org len:%d  newlen:%d\n", g_mod_new_len, newlen);

    g_mod_new_data = (char *)buf;
    g_mod_new_len = newlen;

end:
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_unix_chain_data(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ventoy_compatible = 0;
    grub_uint32_t size = 0;
    grub_uint64_t isosize = 0;
    grub_uint32_t boot_catlog = 0;
    grub_uint32_t img_chunk_size = 0;
    grub_uint32_t override_count = 0;
    grub_uint32_t override_size = 0;
    grub_uint32_t virt_chunk_size = 0;
    grub_file_t file;
    grub_disk_t disk;
    const char *pLastChain = NULL;
    const char *compatible;
    ventoy_chain_head *chain;
    char envbuf[64];
    
    (void)ctxt;
    (void)argc;

    compatible = grub_env_get("ventoy_compatible");
    if (compatible && compatible[0] == 'Y')
    {
        ventoy_compatible = 1;
    }

    if (NULL == g_img_chunk_list.chunk)
    {
        grub_printf("ventoy not ready\n");
        return 1;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        return 1;
    }

    isosize = file->size;

    boot_catlog = ventoy_get_iso_boot_catlog(file);
    if (boot_catlog)
    {
        if (ventoy_is_efi_os() && (!ventoy_has_efi_eltorito(file, boot_catlog)))
        {
            grub_env_set("LoadIsoEfiDriver", "on");
        }
    }
    else
    {
        if (ventoy_is_efi_os())
        {
            grub_env_set("LoadIsoEfiDriver", "on");
        }
        else
        {
            return grub_error(GRUB_ERR_BAD_ARGUMENT, "File %s is not bootable", args[0]);
        }
    }
    
    img_chunk_size = g_img_chunk_list.cur_chunk * sizeof(ventoy_img_chunk);
    
    if (ventoy_compatible)
    {
        size = sizeof(ventoy_chain_head) + img_chunk_size;
    }
    else
    {
        override_count = ventoy_unix_get_override_chunk_count();
        override_size = override_count * sizeof(ventoy_override_chunk);
        
        virt_chunk_size = ventoy_unix_get_virt_chunk_size();
        size = sizeof(ventoy_chain_head) + img_chunk_size + override_size + virt_chunk_size;
    }
    
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

    chain = grub_malloc(size);
    if (!chain)
    {
        grub_printf("Failed to alloc chain memory size %u\n", size);
        grub_file_close(file);
        return 1;
    }

    grub_snprintf(envbuf, sizeof(envbuf), "0x%lx", (unsigned long)chain);
    grub_env_set("vtoy_chain_mem_addr", envbuf);
    grub_snprintf(envbuf, sizeof(envbuf), "%u", size);
    grub_env_set("vtoy_chain_mem_size", envbuf);

    grub_memset(chain, 0, sizeof(ventoy_chain_head));

    /* part 1: os parameter */
    g_ventoy_chain_type = ventoy_chain_linux;
    ventoy_fill_os_param(file, &(chain->os_param));

    /* part 2: chain head */
    disk = file->device->disk;
    chain->disk_drive = disk->id;
    chain->disk_sector_size = (1 << disk->log_sector_size);
    chain->real_img_size_in_bytes = file->size;
    chain->virt_img_size_in_bytes = (file->size + 2047) / 2048 * 2048;
    chain->boot_catalog = boot_catlog;

    if (!ventoy_is_efi_os())
    {
        grub_file_seek(file, boot_catlog * 2048);
        grub_file_read(file, chain->boot_catalog_sector, sizeof(chain->boot_catalog_sector));
    }

    /* part 3: image chunk */
    chain->img_chunk_offset = sizeof(ventoy_chain_head);
    chain->img_chunk_num = g_img_chunk_list.cur_chunk;
    grub_memcpy((char *)chain + chain->img_chunk_offset, g_img_chunk_list.chunk, img_chunk_size);

    if (ventoy_compatible)
    {
        return 0;
    }

    /* part 4: override chunk */
    chain->override_chunk_offset = chain->img_chunk_offset + img_chunk_size;
    chain->override_chunk_num = override_count;
    ventoy_unix_fill_override_data(isosize, (char *)chain + chain->override_chunk_offset);

    /* part 5: virt chunk */
    chain->virt_chunk_offset = chain->override_chunk_offset + override_size;
    chain->virt_chunk_num = ventoy_unix_get_virt_chunk_count();
    ventoy_unix_fill_virt_data(isosize, chain);

    grub_file_close(file);
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

