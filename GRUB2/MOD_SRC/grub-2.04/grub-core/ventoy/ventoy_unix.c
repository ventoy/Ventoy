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
#include <grub/elf.h>
#include <grub/elfload.h>
#include <grub/ventoy.h>
#include "ventoy_def.h"

GRUB_MOD_LICENSE ("GPLv3+");

char g_ko_mod_path[256];
int g_conf_new_len = 0;
char *g_conf_new_data = NULL;

int g_mod_new_len = 0;
char *g_mod_new_data = NULL;

int g_mod_search_magic = 0;
int g_unix_vlnk_boot = 0;

int g_ko_fillmap_len = 0;
char *g_ko_fillmap_data = NULL;

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
    
    if (g_ko_fillmap_len > 0)
    {
        count += (g_ko_fillmap_len / 512);
        if ((g_ko_fillmap_len % 512) > 0)
        {
            count++;
        }
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

static void ventoy_unix_fill_map_data(ventoy_chain_head *chain, struct g_ventoy_map *map)
{
    grub_uint32_t i;
    ventoy_img_chunk *chunk = NULL;

    debug("Fill unix map data: <%llu> <%u> %p\n", 
        (unsigned long long)chain->os_param.vtoy_disk_size, g_img_chunk_list.cur_chunk, map);
    
    map->magic1[0] = map->magic2[0] = VENTOY_UNIX_SEG_MAGIC0;
    map->magic1[1] = map->magic2[1] = VENTOY_UNIX_SEG_MAGIC1;
    map->magic1[2] = map->magic2[2] = VENTOY_UNIX_SEG_MAGIC2;
    map->magic1[3] = map->magic2[3] = VENTOY_UNIX_SEG_MAGIC3;

    map->disksize = chain->os_param.vtoy_disk_size;
    grub_memcpy(map->diskuuid, chain->os_param.vtoy_disk_guid, 16);

    map->segnum = g_img_chunk_list.cur_chunk;
    if (g_img_chunk_list.cur_chunk > VENTOY_UNIX_MAX_SEGNUM)
    {
        debug("####[FAIL] Too many segments for the ISO file %u\n", g_img_chunk_list.cur_chunk);
        map->segnum = VENTOY_UNIX_MAX_SEGNUM;
    }
    
    for (i = 0; i < (grub_uint32_t)(map->segnum); i++)
    {
        chunk = g_img_chunk_list.chunk + i;
        map->seglist[i].seg_start_bytes = chunk->disk_start_sector * 512ULL;
        map->seglist[i].seg_end_bytes = (chunk->disk_end_sector + 1) * 512ULL;        
    }
}

static void ventoy_unix_fill_override_data(    grub_uint64_t isosize, ventoy_chain_head *chain)
{
    int i;
    int left;
    char *data = NULL;
    grub_uint64_t offset;
    grub_uint64_t sector;
    ventoy_override_chunk *cur;
    ventoy_iso9660_override *dirent;
    
    sector = (isosize + 2047) / 2048;

    cur = (ventoy_override_chunk *)((char *)chain + chain->override_chunk_offset);

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
        cur++;
    }

    if (g_mod_new_len > 0)
    {
        /* mod.ko */
        cur->img_offset = g_mod_override_offset;
        cur->override_size = sizeof(ventoy_iso9660_override);
        dirent = (ventoy_iso9660_override *)cur->override_data;
        dirent->first_sector    = (grub_uint32_t)sector;
        dirent->size            = (grub_uint32_t)g_mod_new_len;
        dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
        dirent->size_be         = grub_swap_bytes32(dirent->size);
        sector += (dirent->size + 2047) / 2048;
        cur++;
    }

    if (g_ko_fillmap_len > 0)
    {
        data = g_ko_fillmap_data;
        offset = g_mod_override_offset;

        ventoy_unix_fill_map_data(chain, (struct g_ventoy_map *)data);
        
        for (i = 0; i < g_ko_fillmap_len / 512; i++)
        {
            cur->img_offset = offset;
            cur->override_size = 512;
            grub_memcpy(cur->override_data, data, 512);

            offset += 512;
            data += 512;
            cur++;
        }

        left = (g_ko_fillmap_len % 512);
        if (left > 0)
        {
            cur->img_offset = offset;
            cur->override_size = left;
            grub_memcpy(cur->override_data, data, left);

            offset += left;
            cur++;
        }
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
        if (g_mod_search_magic > 0)
        {
            ventoy_unix_fill_map_data(chain, (struct g_ventoy_map *)(g_mod_new_data + g_mod_search_magic));
        }
    
        ventoy_unix_fill_virt(g_mod_new_data, g_mod_new_len);
    }
    
    return;
}

static int ventoy_freebsd_append_conf(char *buf, const char *isopath, const char *alias)
{
    int pos = 0;
    grub_uint32_t i;
    grub_disk_t disk;
    grub_file_t isofile;
    char uuid[64] = {0};
    const char *val = NULL;
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
    
    if (alias)
    {
        vtoy_ssprintf(buf, pos, "hint.ventoy.0.alias=\"%s\"\n", alias);
    }

    if (g_unix_vlnk_boot)
    {
        vtoy_ssprintf(buf, pos, "hint.ventoy.0.vlnk=%d\n", 1);
    }

    val = ventoy_get_env("VTOY_UNIX_REMOUNT");
    if (val && val[0] == '1' && val[1] == 0)
    {
        vtoy_ssprintf(buf, pos, "hint.ventoy.0.remount=%d\n", 1);
    }

    if (g_mod_search_magic)
    {
        debug("hint.ventoy NO need\n");
        goto out;
    }

    debug("Fill hint.ventoy info\n");
    
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

out:
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
    
    g_unix_vlnk_boot = 0;
    g_mod_search_magic = 0;
    g_conf_new_len = 0;
    g_mod_new_len = 0;
    g_mod_override_offset = 0;
    g_conf_override_offset = 0;
    g_ko_fillmap_len = 0;

    check_free(g_mod_new_data, grub_free);
    check_free(g_conf_new_data, grub_free);
    check_free(g_ko_fillmap_data, grub_free);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_unix_check_vlnk(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_file_t file;

    (void)ctxt;

    if (argc != 1)
    {
        return 1;
    }

    file = grub_file_open(args[0], VENTOY_FILE_TYPE);
    if (file)
    {
        g_unix_vlnk_boot = file->vlnk;
        grub_file_close(file);
    }

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
        debug("NAS version:<%s>\n", ver);
        if (grub_strncmp(ver, "TrueNAS-", 8) == 0)
        {
            ver += 8;
        }
        ventoy_set_env(args[1], ver);
    }
    else
    {
        debug("NAS version:<%s>\n", "NOT FOUND");
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

grub_err_t ventoy_cmd_unix_freebsd_ver_elf(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int j;
    int k;
    grub_elf_t elf = NULL;
    grub_off_t offset = 0;
    grub_uint32_t len = 0;
    char *str = NULL;
    char *data = NULL;
    void *hdr = NULL;
    char ver[64] = {0};
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 3)
    {
        debug("Invalid argc %d\n", argc);
        return 1;
    }

    data = grub_zalloc(8192);
    if (!data)
    {
        goto out;
    }

    elf = grub_elf_open(args[0], GRUB_FILE_TYPE_LINUX_INITRD);
    if (!elf)
    {
        debug("Failed to open file %s\n", args[0]);
        goto out;
    }

    if (args[1][0] == '6')
    {
        Elf64_Ehdr *e = &(elf->ehdr.ehdr64);
        Elf64_Shdr *h;
        Elf64_Shdr *s;
        Elf64_Shdr *t;
        Elf64_Half i;
        
        h = hdr = grub_zalloc(e->e_shnum * e->e_shentsize);
        if (!h)
        {
            goto out;
        }

        debug("read section header %u %u %u\n", e->e_shnum, e->e_shentsize, e->e_shstrndx);
        grub_file_seek(elf->file, e->e_shoff);
        grub_file_read(elf->file, h, e->e_shnum * e->e_shentsize);

        s = (Elf64_Shdr *)((char *)h + e->e_shstrndx * e->e_shentsize);        
        str = grub_malloc(s->sh_size + 1);
        if (!str)
        {
            goto out;
        }
        str[s->sh_size] = 0;

        debug("read string table %u %u\n", (grub_uint32_t)s->sh_offset, (grub_uint32_t)s->sh_size);
        grub_file_seek(elf->file, s->sh_offset);
        grub_file_read(elf->file, str, s->sh_size);

        for (t = h, i = 0; i < e->e_shnum; i++)
        {
            if (grub_strcmp(str + t->sh_name, ".data") == 0)
            {
                offset = t->sh_offset;
                len = t->sh_size;
                debug("find .data section at %u %u\n", (grub_uint32_t)offset, len);
                break;
            }
            t = (Elf64_Shdr *)((char *)t + e->e_shentsize);
        }
    }
    else
    {
        Elf32_Ehdr *e = &(elf->ehdr.ehdr32);
        Elf32_Shdr *h;
        Elf32_Shdr *s;
        Elf32_Shdr *t;
        Elf32_Half i;
        
        h = hdr = grub_zalloc(e->e_shnum * e->e_shentsize);
        if (!h)
        {
            goto out;
        }

        debug("read section header %u %u %u\n", e->e_shnum, e->e_shentsize, e->e_shstrndx);
        grub_file_seek(elf->file, e->e_shoff);
        grub_file_read(elf->file, h, e->e_shnum * e->e_shentsize);

        s = (Elf32_Shdr *)((char *)h + e->e_shstrndx * e->e_shentsize);        
        str = grub_malloc(s->sh_size + 1);
        if (!str)
        {
            goto out;
        }
        str[s->sh_size] = 0;

        debug("read string table %u %u\n", (grub_uint32_t)s->sh_offset, (grub_uint32_t)s->sh_size);
        grub_file_seek(elf->file, s->sh_offset);
        grub_file_read(elf->file, str, s->sh_size);

        for (t = h, i = 0; i < e->e_shnum; i++)
        {
            if (grub_strcmp(str + t->sh_name, ".data") == 0)
            {
                offset = t->sh_offset;
                len = t->sh_size;
                debug("find .data section at %u %u\n", (grub_uint32_t)offset, len);
                break;
            }
            t = (Elf32_Shdr *)((char *)t + e->e_shentsize);
        }
    }

    if (offset == 0 || len == 0)
    {
        debug(".data section not found %s\n", args[0]);
        goto out;
    }

    grub_file_seek(elf->file, offset + len - 8192);
    grub_file_read(elf->file, data, 8192);

    for (j = 0; j < 8192 - 12; j++)
    {
        if (grub_strncmp(data + j, "@(#)FreeBSD ", 12) == 0)
        {
            for (k = j + 12; k < 8192; k++)
            {
                if (0 == grub_isdigit(data[k]) && data[k] != '.')
                {
                    data[k] = 0;
                    break;
                }
            }
        
            grub_snprintf(ver, sizeof(ver), "%s", data + j + 12);
            break;
        }
    }

    if (ver[0])
    {
        k = (int)grub_strtoul(ver, NULL, 10);
        debug("freebsd version:<%s> <%d.x>\n", ver, k);
        grub_snprintf(ver, sizeof(ver), "%d.x", k);
        ventoy_set_env(args[2], ver);
    }
    else
    {
        debug("freebsd version:<%s>\n", "NOT FOUND");
    }

out:
    grub_check_free(str);
    grub_check_free(hdr);
    grub_check_free(data);
    check_free(elf, grub_elf_close);
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_unix_replace_grub_conf(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len = 0;
    grub_uint32_t i;
    char *data;
    char *pos;
    const char *val = NULL;
    grub_uint64_t offset;
    grub_file_t file;
    char extcfg[512];
    const char *confile = NULL;
    const char * loader_conf[] = 
    {
        "/boot/grub/grub.cfg",
    };

    (void)ctxt;

    if (argc != 1 && argc != 2)
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
        debug("Can't find grub.cfg file from %u locations\n", i);
        return 1;
    }

    file = ventoy_grub_file_open(GRUB_FILE_TYPE_LINUX_INITRD, "(loop)/%s", confile);
    if (!file)
    {
        debug("Failed to open %s \n", confile);
        return 1;
    }

    debug("old grub2 conf file size:%d\n", (int)file->size);

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

    pos = grub_strstr(data, "kfreebsd /boot/kernel/kernel");
    if (pos)
    {
        pos += grub_strlen("kfreebsd /boot/kernel/kernel");
        if (grub_strncmp(pos, ".gz", 3) == 0)
        {
            pos += 3;
        }

        if (argc == 2)
        {
            vtoy_ssprintf(extcfg, len, ";kfreebsd_module_elf %s; set kFreeBSD.hint.ventoy.0.alias=\"%s\"", args[0], args[1]);
        }
        else
        {
            vtoy_ssprintf(extcfg, len, ";kfreebsd_module_elf %s", args[0]);
        }

        if (g_unix_vlnk_boot)
        {
            vtoy_ssprintf(extcfg, len, ";set kFreeBSD.hint.ventoy.0.vlnk=%d", 1);
        }

        val = ventoy_get_env("VTOY_UNIX_REMOUNT");
        if (val && val[0] == '1' && val[1] == 0)
        {
            vtoy_ssprintf(extcfg, len, ";set kFreeBSD.hint.ventoy.0.remount=%d", 1);
        }
        
        grub_memmove(pos + len, pos, (int)(file->size - (pos - data)));
        grub_memcpy(pos, extcfg, len);
        g_conf_new_len += len;
    }
    else
    {
        debug("no kfreebsd found\n");
    }
    
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

    if (argc != 2 && argc != 3)
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

    debug("old conf file <%s> size:%d\n", confile, (int)file->size);

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
        g_conf_new_len += ventoy_freebsd_append_conf(data + file->size, args[1], (argc > 2) ? args[2] : NULL);
    }
    else if (grub_strcmp(args[0], "DragonFly") == 0)
    {
        g_conf_new_len += ventoy_dragonfly_append_conf(data + file->size, args[1]);
    }
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static int ventoy_unix_search_magic(char *data, int len)
{
    int i;
    grub_uint32_t *magic = NULL;    

    for (i = 0; i < len; i += 4096)
    {
        magic = (grub_uint32_t *)(data + i);
        if (magic[0] == VENTOY_UNIX_SEG_MAGIC0 && magic[1] == VENTOY_UNIX_SEG_MAGIC1 && 
            magic[2] == VENTOY_UNIX_SEG_MAGIC2 && magic[3] == VENTOY_UNIX_SEG_MAGIC3)
        {
            debug("unix find search magic at 0x%x loop:%d\n", i, (i >> 12));
            g_mod_search_magic = i;
            return 0;
        }
    }

    debug("unix can not find search magic\n");
    return 1;
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

    ventoy_unix_search_magic(g_mod_new_data, g_mod_new_len);
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_unix_ko_fillmap(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    grub_file_t file;
    grub_uint32_t magic[4];
    grub_uint32_t len;

    (void)ctxt;

    if (argc != 1)
    {
        debug("Fillmap ko invalid argc %d\n", argc);
        return 1;
    }

    debug("Fillmap ko %s\n", args[0]);

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "(loop)%s", args[0]);
    if (file)
    {
        grub_file_read(file, magic, 4); /* read for trigger */
        g_mod_override_offset = grub_iso9660_get_last_read_pos(file);
    }
    else
    {   
        debug("Can't find replace ko file from %s\n", args[0]);
        return 1;
    }

    for (i = 0; i < (int)(file->size); i += 65536)
    {
        magic[0] = 0;
        grub_file_seek(file, i);
        grub_file_read(file, magic, sizeof(magic));
    
        if (magic[0] == VENTOY_UNIX_SEG_MAGIC0 && magic[1] == VENTOY_UNIX_SEG_MAGIC1 && 
            magic[2] == VENTOY_UNIX_SEG_MAGIC2 && magic[3] == VENTOY_UNIX_SEG_MAGIC3)
        {
            debug("unix find search magic at 0x%x loop:%d\n", i, (i >> 16));
            g_mod_override_offset += i;
            break;
        }
    }

    len = (grub_uint32_t)OFFSET_OF(struct g_ventoy_map, seglist) + 
        (sizeof(struct g_ventoy_seg) * g_img_chunk_list.cur_chunk);

    g_ko_fillmap_len = (int)len;
    g_ko_fillmap_data = grub_malloc(len);
    if (!g_ko_fillmap_data)
    {
        g_ko_fillmap_len = 0;
        debug("Failed to malloc fillmap data\n");
    }

    debug("Fillmap ko segnum:%u, override len:%u data:%p\n", g_img_chunk_list.cur_chunk, len, g_ko_fillmap_data);

    grub_file_close(file);
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
    desc->part1_size = g_ventoy_disk_part_size[0];
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

    chain = ventoy_alloc_chain(size);
    if (!chain)
    {
        grub_printf("Failed to alloc chain unix memory size %u\n", size);
        grub_file_close(file);
        return 1;
    }

    ventoy_memfile_env_set("vtoy_chain_mem", chain, (ulonglong)size);

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
    ventoy_unix_fill_override_data(isosize, chain);

    /* part 5: virt chunk */
    chain->virt_chunk_offset = chain->override_chunk_offset + override_size;
    chain->virt_chunk_num = ventoy_unix_get_virt_chunk_count();
    ventoy_unix_fill_virt_data(isosize, chain);

    grub_file_close(file);
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

