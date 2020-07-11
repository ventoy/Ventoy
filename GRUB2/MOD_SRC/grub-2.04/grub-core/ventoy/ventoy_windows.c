/******************************************************************************
 * ventoy_windows.c 
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
#include <grub/ventoy.h>
#include "ventoy_def.h"

GRUB_MOD_LICENSE ("GPLv3+");

static int g_iso_fs_type = 0;
static int g_wim_total_patch_count = 0;
static int g_wim_valid_patch_count = 0;
static wim_patch *g_wim_patch_head = NULL;

static grub_uint64_t g_suppress_wincd_override_offset = 0;
static grub_uint32_t g_suppress_wincd_override_data = 0;

grub_uint8_t g_temp_buf[512];

grub_ssize_t lzx_decompress ( const void *data, grub_size_t len, void *buf );

static wim_patch *ventoy_find_wim_patch(const char *path)
{
    int len = (int)grub_strlen(path);
    wim_patch *node = g_wim_patch_head;

    while (node)
    {
        if (len == node->pathlen && 0 == grub_strcmp(path, node->path))
        {
            return node;
        }
        node = node->next;
    }

    return NULL;
}

static int ventoy_collect_wim_patch(const char *bcdfile)
{
    int i, j, k;
    int rc = 1;
    grub_uint64_t magic;
    grub_file_t file = NULL;
    char *buf = NULL;
    wim_patch *node = NULL;
    char c;
    grub_uint8_t byte;
    char valid;
    char path[256];

    g_ventoy_case_insensitive = 1;
    file = grub_file_open(bcdfile, VENTOY_FILE_TYPE);
    g_ventoy_case_insensitive = 0;
    if (!file)
    {
        debug("Failed to open file %s\n", bcdfile);
        grub_errno = 0;
        goto end;
    }

    buf = grub_malloc(file->size + 8);
    if (!buf)
    {
        goto end;
    }

    grub_file_read(file, buf, file->size);

    for (i = 0; i < (int)file->size - 8; i++)
    {        
        if (buf[i + 8] != 0)
        {
            continue;
        }
        
        magic = *(grub_uint64_t *)(buf + i);
        
        /* .wim .WIM .Wim */
        if ((magic == 0x006D00690077002EULL) ||
            (magic == 0x004D00490057002EULL) ||
            (magic == 0x006D00690057002EULL))
        {
            for (j = i; j > 0; j-= 2)
            {
                if (*(grub_uint16_t *)(buf + j) == 0)
                {
                    break;
                }
            }

            if (j > 0)
            {
                byte = (grub_uint8_t)(*(grub_uint16_t *)(buf + j + 2));
                if (byte != '/' && byte != '\\')
                {
                    continue;
                }
                
                valid = 1;
                for (k = 0, j += 2; k < (int)sizeof(path) - 1 && j < i + 8; j += 2)
                {
                    byte = (grub_uint8_t)(*(grub_uint16_t *)(buf + j));
                    c = (char)byte;
                    if (byte > '~' || byte < ' ') /* not printable */
                    {
                        valid = 0;
                        break;
                    }
                    else if (c == '\\')
                    {
                        c = '/';
                    }
                    
                    path[k++] = c;
                }
                path[k++] = 0;

                debug("@@@@ Find wim flag:<%s>\n", path);

                if (0 == valid)
                {
                    debug("Invalid wim file %d\n", k);
                }
                else if (NULL == ventoy_find_wim_patch(path))
                {
                    node = grub_zalloc(sizeof(wim_patch));
                    if (node)
                    {
                        node->pathlen = grub_snprintf(node->path, sizeof(node->path), "%s", path);

                        debug("add patch <%s>\n", path);
                        
                        if (g_wim_patch_head)
                        {
                            node->next = g_wim_patch_head;
                        }
                        g_wim_patch_head = node;

                        g_wim_total_patch_count++;
                    }
                }
                else
                {
                    debug("wim <%s> already exist\n", path);
                }
            }
        }
    }

end:
    check_free(file, grub_file_close);
    grub_check_free(buf);
    return rc;
}

grub_err_t ventoy_cmd_wim_patch_count(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char buf[32];
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc == 1)
    {
        grub_snprintf(buf, sizeof(buf), "%d", g_wim_total_patch_count);
        ventoy_set_env(args[0], buf);
    }
    
    return 0;
}

grub_err_t ventoy_cmd_collect_wim_patch(grub_extcmd_context_t ctxt, int argc, char **args)
{
    wim_patch *node = NULL;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 2)
    {
        return 1;
    }

    debug("ventoy_cmd_collect_wim_patch %s %s\n", args[0], args[1]);

    if (grub_strcmp(args[0], "bcd") == 0)
    {
        ventoy_collect_wim_patch(args[1]);
        return 0;
    }

    if (NULL == ventoy_find_wim_patch(args[1]))
    {
        node = grub_zalloc(sizeof(wim_patch));
        if (node)
        {
            node->pathlen = grub_snprintf(node->path, sizeof(node->path), "%s", args[1]);

            debug("add patch <%s>\n", args[1]);
            
            if (g_wim_patch_head)
            {
                node->next = g_wim_patch_head;
            }
            g_wim_patch_head = node;

            g_wim_total_patch_count++;
        }
    }

    return 0;
}


grub_err_t ventoy_cmd_dump_wim_patch(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    wim_patch *node = NULL;

    (void)ctxt;
    (void)argc;
    (void)args;

    for (node = g_wim_patch_head; node; node = node->next)
    {
        grub_printf("%d %s [%s]\n", i++, node->path, node->valid ? "SUCCESS" : "FAIL");
    }

    return 0;
}


static int wim_name_cmp(const char *search, grub_uint16_t *name, grub_uint16_t namelen)
{
    char c1 = vtoy_to_upper(*search);
    char c2 = vtoy_to_upper(*name);

    while (namelen > 0 && (c1 == c2))
    {
        search++;
        name++;
        namelen--;
        
        c1 = vtoy_to_upper(*search);
        c2 = vtoy_to_upper(*name);
    }

    if (namelen == 0 && *search == 0)
    {
        return 0;
    }

    return 1;
}

static int ventoy_is_pe64(grub_uint8_t *buffer)
{
    grub_uint32_t pe_off;

    if (buffer[0] != 'M' || buffer[1] != 'Z')
    {
        return 0;
    }
    
    pe_off = *(grub_uint32_t *)(buffer + 60);

    if (buffer[pe_off] != 'P' || buffer[pe_off + 1] != 'E')
    {
        return 0;
    }

    if (*(grub_uint16_t *)(buffer + pe_off + 24) == 0x020b)
    {
        return 1;
    }

    return 0;
}

grub_err_t ventoy_cmd_wimdows_reset(grub_extcmd_context_t ctxt, int argc, char **args)
{
    wim_patch *next = NULL;
    wim_patch *node = g_wim_patch_head;

    (void)ctxt;
    (void)argc;
    (void)args;

    while (node)
    {
        next = node->next;
        grub_free(node);
        node = next;
    }

    g_wim_patch_head = NULL;
    g_wim_total_patch_count = 0;
    g_wim_valid_patch_count = 0;

    return 0;
}

static int ventoy_load_jump_exe(const char *path, grub_uint8_t **data, grub_uint32_t *size, wim_hash *hash)
{
    grub_uint32_t i;
    grub_uint32_t align;
    grub_file_t file;

    debug("windows load jump %s\n", path);
    
    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", path);
    if (!file)
    {
        debug("Can't open file %s\n", path); 
        return 1;
    }

    align = ventoy_align((int)file->size, 2048);

    debug("file %s size:%d align:%u\n", path, (int)file->size, align);

    *size = (grub_uint32_t)file->size;
    *data = (grub_uint8_t *)grub_malloc(align);
    if ((*data) == NULL)
    {
        debug("Failed to alloc memory size %u\n", align);
        goto end;
    }

    grub_file_read(file, (*data), file->size);

    if (hash)
    {
        grub_crypto_hash(GRUB_MD_SHA1, hash->sha1, (*data), file->size);

        if (g_ventoy_debug)
        {
            debug("%s", "jump bin 64 hash: ");
            for (i = 0; i < sizeof(hash->sha1); i++)
            {
                ventoy_debug("%02x ", hash->sha1[i]);
            }
            ventoy_debug("\n");
        }
    }

end:

    grub_file_close(file);
    return 0;
}

static int ventoy_get_override_info(grub_file_t file, wim_tail *wim_data)
{
    grub_uint32_t start_block;
    grub_uint64_t file_offset;
    grub_uint64_t override_offset;
    grub_uint32_t override_len;
    grub_uint64_t fe_entry_size_offset;
    
    if (grub_strcmp(file->fs->name, "iso9660") == 0)
    {
        g_iso_fs_type = wim_data->iso_type = 0;
        override_len = sizeof(ventoy_iso9660_override);
        override_offset = grub_iso9660_get_last_file_dirent_pos(file) + 2;

        grub_file_read(file, &start_block, 1); // just read for hook trigger
        file_offset = grub_iso9660_get_last_read_pos(file);

        debug("iso9660 wim size:%llu override_offset:%llu file_offset:%llu\n", 
            (ulonglong)file->size, (ulonglong)override_offset, (ulonglong)file_offset);
    }
    else
    {
        g_iso_fs_type = wim_data->iso_type = 1;    
        override_len = sizeof(ventoy_udf_override);
        override_offset = grub_udf_get_last_file_attr_offset(file, &start_block, &fe_entry_size_offset);
        
        file_offset = grub_udf_get_file_offset(file);

        debug("UDF wim size:%llu override_offset:%llu file_offset:%llu start_block=%u\n", 
            (ulonglong)file->size, (ulonglong)override_offset, (ulonglong)file_offset, start_block);
    }

    wim_data->file_offset = file_offset;
    wim_data->udf_start_block = start_block;
    wim_data->fe_entry_size_offset = fe_entry_size_offset;
    wim_data->override_offset = override_offset;
    wim_data->override_len = override_len;

    return 0;
}

static int ventoy_read_resource(grub_file_t fp, wim_resource_header *head, void **buffer)
{
    int decompress_len = 0;
    int total_decompress = 0;
    grub_uint32_t i = 0;
    grub_uint32_t chunk_num = 0;
    grub_uint32_t chunk_size = 0;
    grub_uint32_t last_chunk_size = 0;
    grub_uint32_t last_decompress_size = 0;
    grub_uint32_t cur_offset = 0;
    grub_uint8_t *cur_dst = NULL;
    grub_uint8_t *buffer_compress = NULL;
    grub_uint8_t *buffer_decompress = NULL;
    grub_uint32_t *chunk_offset = NULL;

    buffer_decompress = (grub_uint8_t *)grub_malloc(head->raw_size + head->size_in_wim);
    if (NULL == buffer_decompress)
    {
        return 0;
    }

    grub_file_seek(fp, head->offset);

    if (head->size_in_wim == head->raw_size)
    {
        grub_file_read(fp, buffer_decompress, head->size_in_wim);
        *buffer = buffer_decompress;
        return 0;
    }

    buffer_compress = buffer_decompress + head->raw_size;
    grub_file_read(fp, buffer_compress, head->size_in_wim);

    chunk_num = (head->raw_size + WIM_CHUNK_LEN - 1) / WIM_CHUNK_LEN;
    cur_offset = (chunk_num - 1) * 4;
    chunk_offset = (grub_uint32_t *)buffer_compress;
    
    cur_dst = buffer_decompress;
    
    for (i = 0; i < chunk_num - 1; i++)
    {
        chunk_size = (i == 0) ? chunk_offset[i] : chunk_offset[i] - chunk_offset[i - 1];

        if (WIM_CHUNK_LEN == chunk_size)
        {
            grub_memcpy(cur_dst, buffer_compress + cur_offset, chunk_size);
            decompress_len = (int)chunk_size; 
        }
        else
        {
            decompress_len = (int)lzx_decompress(buffer_compress + cur_offset, chunk_size, cur_dst);
        }

        //debug("chunk_size:%u decompresslen:%d\n", chunk_size, decompress_len);
        
        total_decompress += decompress_len;
        cur_dst += decompress_len;
        cur_offset += chunk_size;
    }

    /* last chunk */
    last_chunk_size = (grub_uint32_t)(head->size_in_wim - cur_offset);
    last_decompress_size = head->raw_size - total_decompress;
    
    if (last_chunk_size < WIM_CHUNK_LEN && last_chunk_size == last_decompress_size)
    {
        debug("Last chunk %u uncompressed\n", last_chunk_size);
        grub_memcpy(cur_dst, buffer_compress + cur_offset, last_chunk_size);
        decompress_len = (int)last_chunk_size; 
    }
    else
    {
        decompress_len = (int)lzx_decompress(buffer_compress + cur_offset, head->size_in_wim - cur_offset, cur_dst);            
    }
    
    cur_dst += decompress_len;
    total_decompress += decompress_len;

    if (cur_dst != buffer_decompress + head->raw_size)
    {
        debug("head->size_in_wim:%llu head->raw_size:%llu cur_dst:%p buffer_decompress:%p total_decompress:%d\n", 
            (ulonglong)head->size_in_wim, (ulonglong)head->raw_size, cur_dst, buffer_decompress, total_decompress);
        grub_free(buffer_decompress);
        return 1;
    }
    
    *buffer = buffer_decompress;
    return 0;
}


static wim_directory_entry * search_wim_dirent(wim_directory_entry *dir, const char *search_name)
{
    do 
    {
        if (dir->len && dir->name_len)
        {
            if (wim_name_cmp(search_name, (grub_uint16_t *)(dir + 1), dir->name_len / 2) == 0)
            {
                return dir;
            }
        }
        dir = (wim_directory_entry *)((grub_uint8_t *)dir + dir->len);
    } while(dir->len);
        
    return NULL;
}

static wim_directory_entry * search_full_wim_dirent
(
    void *meta_data, 
    wim_directory_entry *dir,
    const char **path    
)
{
    wim_directory_entry *subdir = NULL;
    wim_directory_entry *search = dir;

    while (*path)
    {
        subdir = (wim_directory_entry *)((char *)meta_data + search->subdir);
        search = search_wim_dirent(subdir, *path);
        path++;
    }
    
    return search;
}

static wim_directory_entry * search_replace_wim_dirent(void *meta_data, wim_directory_entry *dir)
{
    wim_directory_entry *wim_dirent = NULL;
    const char *winpeshl_path[] = { "Windows", "System32", "winpeshl.exe", NULL };
    //const char *native_path[] = { "Windows", "System32", "native.exe", NULL };

    wim_dirent = search_full_wim_dirent(meta_data, dir, winpeshl_path);
    debug("search winpeshl.exe %p\n", wim_dirent);
    if (wim_dirent)
    {
        return wim_dirent;
    }

    #if 0
    wim_dirent = search_full_wim_dirent(meta_data, dir, native_path);
    debug("search native.exe %p\n", wim_dirent);
    if (wim_dirent)
    {
        return wim_dirent;
    }
    #endif

    return NULL;
}


static wim_lookup_entry * ventoy_find_look_entry(wim_header *header, wim_lookup_entry *lookup, wim_hash *hash)
{
    grub_uint32_t i = 0;
    
    for (i = 0; i < (grub_uint32_t)header->lookup.raw_size / sizeof(wim_lookup_entry); i++)
    {
        if (grub_memcmp(&lookup[i].hash, hash, sizeof(wim_hash)) == 0)
        {
            return lookup + i;
        }
    }

    return NULL;
}

static wim_lookup_entry * ventoy_find_meta_entry(wim_header *header, wim_lookup_entry *lookup)
{
    grub_uint32_t i = 0;
    grub_uint32_t index = 0;;

    if ((header == NULL) || (lookup == NULL))
    {
        return NULL;
    }
    
    for (i = 0; i < (grub_uint32_t)header->lookup.raw_size / sizeof(wim_lookup_entry); i++)
    {
        if (lookup[i].resource.flags & RESHDR_FLAG_METADATA)
        {
            index++;
            if (index == header->boot_index)
            {
                return lookup + i;
            }
        }
    }

    return NULL;
}

static int ventoy_update_all_hash(wim_patch *patch, void *meta_data, wim_directory_entry *dir)
{
    if ((meta_data == NULL) || (dir == NULL))
    {
        return 0;
    }

    if (dir->len < sizeof(wim_directory_entry))
    {
        return 0;
    }

    do
    {
        if (dir->subdir == 0 && grub_memcmp(dir->hash.sha1, patch->old_hash.sha1, sizeof(wim_hash)) == 0)
        {
            debug("find target file, name_len:%u upadte hash\n", dir->name_len);
            grub_memcpy(dir->hash.sha1, &(patch->wim_data.bin_hash), sizeof(wim_hash));
        }
        
        if (dir->subdir)
        {
            ventoy_update_all_hash(patch, meta_data, (wim_directory_entry *)((char *)meta_data + dir->subdir));
        }
    
        dir = (wim_directory_entry *)((char *)dir + dir->len);
    } while (dir->len >= sizeof(wim_directory_entry));

    return 0;
}

static int ventoy_cat_exe_file_data(wim_tail *wim_data, grub_uint32_t exe_len, grub_uint8_t *exe_data)
{
    int pe64 = 0;
    char file[256];
    grub_uint32_t jump_len = 0;
    grub_uint32_t jump_align = 0;
    grub_uint8_t *jump_data = NULL;

    pe64 = ventoy_is_pe64(exe_data);
    
    grub_snprintf(file, sizeof(file), "%s/vtoyjump%d.exe", grub_env_get("vtoy_path"), pe64 ? 64 : 32);
    ventoy_load_jump_exe(file, &jump_data, &jump_len, NULL);
    jump_align = ventoy_align(jump_len, 16);
    
    wim_data->jump_exe_len = jump_len;
    wim_data->bin_raw_len = jump_align + sizeof(ventoy_os_param) + sizeof(ventoy_windows_data) + exe_len;
    wim_data->bin_align_len = ventoy_align(wim_data->bin_raw_len, 2048);
    
    wim_data->jump_bin_data = grub_malloc(wim_data->bin_align_len);
    if (wim_data->jump_bin_data)
    {
        grub_memcpy(wim_data->jump_bin_data, jump_data, jump_len);
        grub_memcpy(wim_data->jump_bin_data + jump_align + sizeof(ventoy_os_param) + sizeof(ventoy_windows_data), exe_data, exe_len);
    }

    debug("jump_exe_len:%u bin_raw_len:%u bin_align_len:%u\n", 
        wim_data->jump_exe_len, wim_data->bin_raw_len, wim_data->bin_align_len);
    
    return 0;
}

int ventoy_fill_windows_rtdata(void *buf, char *isopath)
{
    char *pos = NULL;
    char *script = NULL;
    ventoy_windows_data *data = (ventoy_windows_data *)buf;

    grub_memset(data, 0, sizeof(ventoy_windows_data));

    pos = grub_strstr(isopath, "/");
    if (!pos)
    {
        return 1;
    }

    script = ventoy_plugin_get_cur_install_template(pos);
    if (script)
    {
        debug("auto install script <%s>\n", script);
        grub_snprintf(data->auto_install_script, sizeof(data->auto_install_script) - 1, "%s", script);
    }
    else
    {
        debug("auto install script skipped or not configed %s\n", pos);
    }
    
    return 0;
}

static int ventoy_update_before_chain(ventoy_os_param *param, char *isopath)
{
    grub_uint32_t jump_align = 0;
    wim_lookup_entry *meta_look = NULL;
    wim_security_header *security = NULL;
    wim_directory_entry *rootdir = NULL;
    wim_header *head = NULL;
    wim_lookup_entry *lookup = NULL;
    wim_patch *node = NULL;
    wim_tail *wim_data = NULL;

    for (node = g_wim_patch_head; node; node = node->next)
    {
        if (0 == node->valid)
        {
            continue;
        }

        wim_data = &node->wim_data;
        head = &wim_data->wim_header;
        lookup = (wim_lookup_entry *)wim_data->new_lookup_data;

        jump_align = ventoy_align(wim_data->jump_exe_len, 16);
        if (wim_data->jump_bin_data)
        {
            grub_memcpy(wim_data->jump_bin_data + jump_align, param, sizeof(ventoy_os_param));        
            ventoy_fill_windows_rtdata(wim_data->jump_bin_data + jump_align + sizeof(ventoy_os_param), isopath);
        }

        grub_crypto_hash(GRUB_MD_SHA1, wim_data->bin_hash.sha1, wim_data->jump_bin_data, wim_data->bin_raw_len);

        security = (wim_security_header *)wim_data->new_meta_data;
        rootdir = (wim_directory_entry *)(wim_data->new_meta_data + ((security->len + 7) & 0xFFFFFFF8U));

        /* update all winpeshl.exe dirent entry's hash */
        ventoy_update_all_hash(node, wim_data->new_meta_data, rootdir);

        /* update winpeshl.exe lookup entry data (hash/offset/length) */
        if (node->replace_look)
        {
            debug("update replace lookup entry_id:%ld\n", ((long)node->replace_look - (long)lookup) / sizeof(wim_lookup_entry));
            node->replace_look->resource.raw_size = wim_data->bin_raw_len;
            node->replace_look->resource.size_in_wim = wim_data->bin_raw_len;
            node->replace_look->resource.flags = 0;
            node->replace_look->resource.offset = wim_data->wim_align_size;

            grub_memcpy(node->replace_look->hash.sha1, wim_data->bin_hash.sha1, sizeof(wim_hash));
        }

        /* update metadata's hash */
        meta_look = ventoy_find_meta_entry(head, lookup);
        if (meta_look)
        {
            debug("find meta lookup entry_id:%ld\n", ((long)meta_look - (long)lookup) / sizeof(wim_lookup_entry));
            grub_memcpy(&meta_look->resource, &head->metadata, sizeof(wim_resource_header));
            grub_crypto_hash(GRUB_MD_SHA1, meta_look->hash.sha1, wim_data->new_meta_data, wim_data->new_meta_len);
        }
    }

    return 0;
}

static int ventoy_wimdows_locate_wim(const char *disk, wim_patch *patch)
{
    int rc;
    grub_file_t file;
    grub_uint32_t exe_len;
    grub_uint8_t *exe_data = NULL;
    grub_uint8_t *decompress_data = NULL;
    wim_lookup_entry *lookup = NULL;
    wim_security_header *security = NULL;
    wim_directory_entry *rootdir = NULL;
    wim_directory_entry *search = NULL;
    wim_header *head = &(patch->wim_data.wim_header);    
    wim_tail *wim_data = &patch->wim_data;
    
    debug("windows locate wim start %s\n", patch->path);

    g_ventoy_case_insensitive = 1;
    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", disk, patch->path);
    g_ventoy_case_insensitive = 0;
    
    if (!file)
    {
        debug("File %s%s NOT exist\n", disk, patch->path);
        return 1;
    }

    ventoy_get_override_info(file, &patch->wim_data);

    grub_file_seek(file, 0);
    grub_file_read(file, head, sizeof(wim_header));

    if (grub_memcmp(head->signature, WIM_HEAD_SIGNATURE, sizeof(head->signature)))
    {
        debug("Not a valid wim file %s\n", (char *)head->signature);
        grub_file_close(file);
        return 1;
    }

    if ((head->flags & FLAG_HEADER_COMPRESS_XPRESS) || (head->flags & FLAG_HEADER_COMPRESS_LZMS))
    {
        debug("Xpress or LZMS compress is not supported 0x%x\n", head->flags);
        grub_file_close(file);
        return 1;
    }

    rc = ventoy_read_resource(file, &head->metadata, (void **)&decompress_data);
    if (rc)
    {
        grub_printf("failed to read meta data %d\n", rc);
        grub_file_close(file);
        return 1;
    }

    security = (wim_security_header *)decompress_data;
    rootdir = (wim_directory_entry *)(decompress_data + ((security->len + 7) & 0xFFFFFFF8U));

    /* search winpeshl.exe dirent entry */
    search = search_replace_wim_dirent(decompress_data, rootdir);
    if (!search)
    {
        debug("Failed to find replace file %p\n", search);
        grub_file_close(file);
        return 1;
    }
    
    debug("find replace file at %p\n", search);
    
    grub_memcpy(&patch->old_hash, search->hash.sha1, sizeof(wim_hash));

    debug("read lookup offset:%llu size:%llu\n", (ulonglong)head->lookup.offset, (ulonglong)head->lookup.raw_size);
    lookup = grub_malloc(head->lookup.raw_size);
    grub_file_seek(file, head->lookup.offset);
    grub_file_read(file, lookup, head->lookup.raw_size);

    /* find and extact winpeshl.exe */
    patch->replace_look = ventoy_find_look_entry(head, lookup, &patch->old_hash);
    if (patch->replace_look)
    {
        exe_len = (grub_uint32_t)patch->replace_look->resource.raw_size;
        debug("find replace lookup entry_id:%ld raw_size:%u\n", 
            ((long)patch->replace_look - (long)lookup) / sizeof(wim_lookup_entry), exe_len);

        if (0 == ventoy_read_resource(file, &(patch->replace_look->resource), (void **)&(exe_data)))
        {
            ventoy_cat_exe_file_data(wim_data, exe_len, exe_data);
            grub_free(exe_data);
        }
        else
        {
            debug("failed to read replace file meta data %u\n", exe_len);
        }
    }
    else
    {
        debug("failed to find lookup entry for replace file 0x%02x 0x%02x\n", 
               patch->old_hash.sha1[0],  patch->old_hash.sha1[1]);
    }

    wim_data->wim_raw_size = (grub_uint32_t)file->size;
    wim_data->wim_align_size = ventoy_align(wim_data->wim_raw_size, 2048);
    
    grub_check_free(wim_data->new_meta_data);
    wim_data->new_meta_data = decompress_data;
    wim_data->new_meta_len = head->metadata.raw_size;
    wim_data->new_meta_align_len = ventoy_align(wim_data->new_meta_len, 2048);
    
    grub_check_free(wim_data->new_lookup_data);
    wim_data->new_lookup_data = (grub_uint8_t *)lookup;
    wim_data->new_lookup_len = (grub_uint32_t)head->lookup.raw_size;
    wim_data->new_lookup_align_len = ventoy_align(wim_data->new_lookup_len, 2048);

    head->metadata.flags = RESHDR_FLAG_METADATA;
    head->metadata.offset = wim_data->wim_align_size + wim_data->bin_align_len;
    head->metadata.size_in_wim = wim_data->new_meta_len;
    head->metadata.raw_size = wim_data->new_meta_len;

    head->lookup.flags = 0;
    head->lookup.offset = head->metadata.offset + wim_data->new_meta_align_len;
    head->lookup.size_in_wim = wim_data->new_lookup_len;
    head->lookup.raw_size = wim_data->new_lookup_len;

    grub_file_close(file);

    debug("%s", "windows locate wim finish\n");
    return 0;
}

grub_err_t ventoy_cmd_locate_wim_patch(grub_extcmd_context_t ctxt, int argc, char **args)
{
    wim_patch *node = g_wim_patch_head;

    (void)ctxt;
    (void)argc;
    (void)args;

    while (node)
    {
        if (0 == ventoy_wimdows_locate_wim(args[0], node))
        {
            node->valid = 1;
            g_wim_valid_patch_count++;
        }

        node = node->next;
    }

    return 0;
}

static grub_uint32_t ventoy_get_override_chunk_num(void)
{
    grub_uint32_t chunk_num = 0;
    
    if (g_iso_fs_type == 0)
    {
        /* ISO9660: */
        /* per wim */
        /* 1: file_size and file_offset */
        /* 2: new wim file header */
        chunk_num = g_wim_valid_patch_count * 2;
    }
    else
    {
        /* UDF: */
        /* global: */
        /* 1: block count in Partition Descriptor */

        /* per wim */
        /* 1: file_size in file_entry or extend_file_entry */
        /* 2: data_size and position in extend data short ad */
        /* 3: new wim file header */
        chunk_num = g_wim_valid_patch_count * 3 + 1;
    }

    if (g_suppress_wincd_override_offset > 0)
    {
        chunk_num++;
    }

    return chunk_num;
}

static void ventoy_fill_suppress_wincd_override_data(void *override)
{
    ventoy_override_chunk *cur = (ventoy_override_chunk *)override;

    cur->override_size = 4;
    cur->img_offset = g_suppress_wincd_override_offset;
    grub_memcpy(cur->override_data, &g_suppress_wincd_override_data, cur->override_size);
}

static void ventoy_windows_fill_override_data_iso9660(    grub_uint64_t isosize, void *override)
{
    grub_uint64_t sector;
    grub_uint32_t new_wim_size;
    ventoy_override_chunk *cur;
    wim_patch *node = NULL;
    wim_tail *wim_data = NULL;
    ventoy_iso9660_override *dirent = NULL;

    sector = (isosize + 2047) / 2048;

    cur = (ventoy_override_chunk *)override;

    if (g_suppress_wincd_override_offset > 0)
    {
        ventoy_fill_suppress_wincd_override_data(cur);
        cur++;
    }

    debug("ventoy_windows_fill_override_data_iso9660 %lu\n", (ulong)isosize);

    for (node = g_wim_patch_head; node; node = node->next)
    {
        wim_data = &node->wim_data;
        if (0 == node->valid)
        {
            continue;
        }
        
        new_wim_size = wim_data->wim_align_size + wim_data->bin_align_len + 
                wim_data->new_meta_align_len + wim_data->new_lookup_align_len;

        dirent = (ventoy_iso9660_override *)wim_data->override_data;

        dirent->first_sector    = (grub_uint32_t)sector;
        dirent->size            = new_wim_size;
        dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
        dirent->size_be         = grub_swap_bytes32(dirent->size);

        sector += (new_wim_size / 2048);

        /* override 1: position and length in dirent */
        cur->img_offset = wim_data->override_offset;
        cur->override_size = wim_data->override_len;
        grub_memcpy(cur->override_data, wim_data->override_data, cur->override_size);
        cur++;

        /* override 2: new wim file header */
        cur->img_offset = wim_data->file_offset;
        cur->override_size = sizeof(wim_header);
        grub_memcpy(cur->override_data, &(wim_data->wim_header), cur->override_size);
        cur++;
    }

    return;
}

static void ventoy_windows_fill_override_data_udf(    grub_uint64_t isosize, void *override)
{
    grub_uint32_t data32;
    grub_uint64_t data64;
    grub_uint64_t sector;
    grub_uint32_t new_wim_size;
    grub_uint64_t total_wim_size = 0;
    grub_uint32_t udf_start_block = 0;
    ventoy_override_chunk *cur;
    wim_patch *node = NULL;
    wim_tail *wim_data = NULL;
    ventoy_udf_override *udf = NULL;

    sector = (isosize + 2047) / 2048;

    cur = (ventoy_override_chunk *)override;
    
    if (g_suppress_wincd_override_offset > 0)
    {
        ventoy_fill_suppress_wincd_override_data(cur);
        cur++;
    }

    debug("ventoy_windows_fill_override_data_udf %lu\n", (ulong)isosize);

    for (node = g_wim_patch_head; node; node = node->next)
    {
        wim_data = &node->wim_data;
        if (node->valid)
        {
            if (udf_start_block == 0)
            {
                udf_start_block = wim_data->udf_start_block;
            }
            new_wim_size = wim_data->wim_align_size + wim_data->bin_align_len + 
                wim_data->new_meta_align_len + wim_data->new_lookup_align_len;
            total_wim_size += new_wim_size;
        }
    }

    //override 1: sector number in pd data 
    cur->img_offset = grub_udf_get_last_pd_size_offset();
    cur->override_size = 4;
    data32 = sector - udf_start_block + (total_wim_size / 2048);
    grub_memcpy(cur->override_data, &(data32), 4);

    for (node = g_wim_patch_head; node; node = node->next)
    {
        wim_data = &node->wim_data;
        if (0 == node->valid)
        {
            continue;
        }
        
        new_wim_size = wim_data->wim_align_size + wim_data->bin_align_len + 
                wim_data->new_meta_align_len + wim_data->new_lookup_align_len;

        //override 2: filesize in file_entry
        cur++;
        cur->img_offset = wim_data->fe_entry_size_offset;
        cur->override_size = 8;
        data64 = new_wim_size;
        grub_memcpy(cur->override_data, &(data64), 8);

        udf = (ventoy_udf_override *)wim_data->override_data;
        udf->length   = new_wim_size;
        udf->position = (grub_uint32_t)sector - udf_start_block;

        sector += (new_wim_size / 2048);

        /* override 3: position and length in extend data */
        cur++;
        cur->img_offset = wim_data->override_offset;
        cur->override_size = wim_data->override_len;
        grub_memcpy(cur->override_data, wim_data->override_data, cur->override_size);

        /* override 4: new wim file header */
        cur++;
        cur->img_offset = wim_data->file_offset;
        cur->override_size = sizeof(wim_header);
        grub_memcpy(cur->override_data, &(wim_data->wim_header), cur->override_size);
    }

    return;
}

static grub_uint32_t ventoy_windows_get_virt_data_size(void)
{
    grub_uint32_t size = 0;
    wim_tail *wim_data = NULL;
    wim_patch *node = g_wim_patch_head;
    
    while (node)
    {
        if (node->valid)
        {
            wim_data = &node->wim_data;
            size += sizeof(ventoy_virt_chunk) + wim_data->bin_align_len + 
                    wim_data->new_meta_align_len + wim_data->new_lookup_align_len;
        }
        node = node->next;
    }
    
    return size;
}

static void ventoy_windows_fill_virt_data(    grub_uint64_t isosize, ventoy_chain_head *chain)
{
    grub_uint64_t sector;
    grub_uint32_t offset;
    grub_uint32_t wim_secs;
    grub_uint32_t mem_secs;
    char *override = NULL;
    ventoy_virt_chunk *cur = NULL;
    wim_tail *wim_data = NULL;
    wim_patch *node = NULL;    

    sector = (isosize + 2047) / 2048;
    offset = sizeof(ventoy_virt_chunk) * g_wim_valid_patch_count;

    override = (char *)chain + chain->virt_chunk_offset;
    cur = (ventoy_virt_chunk *)override;

    for (node = g_wim_patch_head; node; node = node->next)
    {
        if (0 == node->valid)
        {
            continue;
        }

        wim_data = &node->wim_data;

        wim_secs = wim_data->wim_align_size / 2048;
        mem_secs = (wim_data->bin_align_len + wim_data->new_meta_align_len + wim_data->new_lookup_align_len) / 2048;

        cur->remap_sector_start = sector;
        cur->remap_sector_end   = cur->remap_sector_start + wim_secs;
        cur->org_sector_start   = (grub_uint32_t)(wim_data->file_offset / 2048);
        
        cur->mem_sector_start   = cur->remap_sector_end;
        cur->mem_sector_end     = cur->mem_sector_start + mem_secs;
        cur->mem_sector_offset  = offset;

        sector += wim_secs + mem_secs;
        cur++;

        grub_memcpy(override + offset, wim_data->jump_bin_data, wim_data->bin_raw_len);
        offset += wim_data->bin_align_len;

        grub_memcpy(override + offset, wim_data->new_meta_data, wim_data->new_meta_len);
        offset += wim_data->new_meta_align_len;
        
        grub_memcpy(override + offset, wim_data->new_lookup_data, wim_data->new_lookup_len);
        offset += wim_data->new_lookup_align_len;

        chain->virt_img_size_in_bytes += wim_data->wim_align_size + 
                                         wim_data->bin_align_len + 
                                         wim_data->new_meta_align_len + 
                                         wim_data->new_lookup_align_len;
    }

    return;
}

static int ventoy_windows_drive_map(ventoy_chain_head *chain)
{
    grub_disk_t disk;
        
    debug("drive map begin <%p> ...\n", chain);

    if (chain->disk_drive == 0x80)
    {
        disk = grub_disk_open("hd1");
        if (disk)
        {
            grub_disk_close(disk);
            debug("drive map needed %p\n", disk);
            chain->drive_map = 0x81;
        }
        else
        {
            debug("failed to open disk %s\n", "hd1");
        }
    }
    else
    {
        debug("no need to map 0x%x\n", chain->disk_drive);
    }

    return 0;
}

static int ventoy_suppress_windows_cd_prompt(void)
{
    int rc = 1;
    const char  *cdprompt = NULL;
    grub_uint64_t readpos = 0;
    grub_file_t file = NULL;
    grub_uint8_t data[32];

    cdprompt = ventoy_get_env("VTOY_WINDOWS_CD_PROMPT");
    if (cdprompt && cdprompt[0] == '1' && cdprompt[1] == 0)
    {
        debug("VTOY_WINDOWS_CD_PROMPT:<%s>\n", cdprompt);
        return 0;
    }

    g_ventoy_case_insensitive = 1;
    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/boot/bootfix.bin", "(loop)");
    g_ventoy_case_insensitive = 0;

    if (!file)
    {
        debug("Failed to open %s\n", "bootfix.bin");
        goto end;
    }

    grub_file_read(file, data, 32);

    if (file->fs && file->fs->name && grub_strcmp(file->fs->name, "udf") == 0)
    {
        readpos = grub_udf_get_file_offset(file);
    }
    else
    {
        readpos = grub_iso9660_get_last_read_pos(file);
    }

    debug("bootfix.bin readpos:%lu (sector:%lu)  data: %02x %02x %02x %02x\n", 
        (ulong)readpos, (ulong)readpos / 2048, data[24], data[25], data[26], data[27]);

    if (*(grub_uint32_t *)(data + 24) == 0x13cd0080)
    {
        g_suppress_wincd_override_offset = readpos + 24;
        g_suppress_wincd_override_data = 0x13cd00fd;

        rc = 0;
    }

    debug("g_suppress_wincd_override_offset:%lu\n", (ulong)g_suppress_wincd_override_offset);

end:
    check_free(file, grub_file_close);

    return rc;
}

grub_err_t ventoy_cmd_windows_chain_data(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int unknown_image = 0;
    int ventoy_compatible = 0;
    grub_uint32_t size = 0;
    grub_uint64_t isosize = 0;
    grub_uint32_t boot_catlog = 0;
    grub_uint32_t img_chunk_size = 0;
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

    debug("chain data begin <%s> ...\n", args[0]);

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

    if (0 == ventoy_compatible && g_wim_valid_patch_count == 0)
    {
        unknown_image = 1;
        debug("Warning: %s was not recognized by Ventoy\n", args[0]);
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

    g_suppress_wincd_override_offset = 0;
    if (!ventoy_is_efi_os()) /* legacy mode */
    {
        ventoy_suppress_windows_cd_prompt();
    }

    img_chunk_size = g_img_chunk_list.cur_chunk * sizeof(ventoy_img_chunk);
    
    if (ventoy_compatible || unknown_image)
    {
        override_size = g_suppress_wincd_override_offset > 0 ? sizeof(ventoy_override_chunk) : 0;
        size = sizeof(ventoy_chain_head) + img_chunk_size + override_size;
    }
    else
    {
        override_size = ventoy_get_override_chunk_num() * sizeof(ventoy_override_chunk);
        virt_chunk_size = ventoy_windows_get_virt_data_size();
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
    g_ventoy_chain_type = ventoy_chain_windows;
    ventoy_fill_os_param(file, &(chain->os_param));

    if (0 == unknown_image)
    {
        ventoy_update_before_chain(&(chain->os_param), args[0]);
    }

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

    if (ventoy_compatible || unknown_image)
    {
        if (g_suppress_wincd_override_offset > 0)
        {
            chain->override_chunk_offset = chain->img_chunk_offset + img_chunk_size;
            chain->override_chunk_num = 1;
            ventoy_fill_suppress_wincd_override_data((char *)chain + chain->override_chunk_offset);
        }

        return 0;
    }

    if (0 == g_wim_valid_patch_count)
    {
        return 0;
    }

    /* part 4: override chunk */
    chain->override_chunk_offset = chain->img_chunk_offset + img_chunk_size;
    chain->override_chunk_num = ventoy_get_override_chunk_num();

    if (g_iso_fs_type == 0)
    {
        ventoy_windows_fill_override_data_iso9660(isosize, (char *)chain + chain->override_chunk_offset);
    }
    else
    {
        ventoy_windows_fill_override_data_udf(isosize, (char *)chain + chain->override_chunk_offset);        
    }

    /* part 5: virt chunk */
    chain->virt_chunk_offset = chain->override_chunk_offset + override_size;
    chain->virt_chunk_num = g_wim_valid_patch_count;
    ventoy_windows_fill_virt_data(isosize, chain);

    if (ventoy_is_efi_os() == 0)
    {
        ventoy_windows_drive_map(chain);        
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_uint32_t ventoy_get_wim_iso_offset(const char *filepath)
{
    grub_uint32_t imgoffset;
    grub_file_t file;
    char cmdbuf[128];
    
    grub_snprintf(cmdbuf, sizeof(cmdbuf), "loopback wimiso %s", filepath);
    grub_script_execute_sourcecode(cmdbuf);

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", "(wimiso)/boot/boot.wim");
    if (!file)
    {
        grub_printf("Failed to open boot.wim file in the image file\n");
        return 0;
    }

    imgoffset = (grub_uint32_t)grub_iso9660_get_last_file_dirent_pos(file) + 2;

    debug("wimiso wim direct offset: %u\n", imgoffset);
    
    grub_file_close(file);
    
    grub_script_execute_sourcecode("loopback -d wimiso");

    return imgoffset;
}

static int ventoy_get_wim_chunklist(const char *filename, ventoy_img_chunk_list *wimchunk, grub_uint64_t *wimsize)
{
    grub_file_t wimfile;
    
    wimfile = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", filename);
    if (!wimfile)
    {
        return 1;
    }

    grub_memset(wimchunk, 0, sizeof(ventoy_img_chunk_list));
    wimchunk->chunk = grub_malloc(sizeof(ventoy_img_chunk) * DEFAULT_CHUNK_NUM);
    if (NULL == wimchunk->chunk)
    {
        grub_file_close(wimfile);
        return grub_error(GRUB_ERR_OUT_OF_MEMORY, "Can't allocate image chunk memoty\n");
    }
    
    wimchunk->max_chunk = DEFAULT_CHUNK_NUM;
    wimchunk->cur_chunk = 0;
    
    ventoy_get_block_list(wimfile, wimchunk, wimfile->device->disk->partition->start);

    *wimsize = wimfile->size;
    grub_file_close(wimfile);

    return 0;
}

grub_err_t ventoy_cmd_wim_chain_data(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t i = 0;
    grub_uint32_t imgoffset = 0;
    grub_uint32_t size = 0;
    grub_uint32_t isosector = 0;
    grub_uint64_t wimsize = 0;
    grub_uint32_t boot_catlog = 0;
    grub_uint32_t img_chunk1_size = 0;
    grub_uint32_t img_chunk2_size = 0;
    grub_uint32_t override_size = 0;
    grub_file_t file;
    grub_disk_t disk;
    const char *pLastChain = NULL;
    ventoy_chain_head *chain;
    ventoy_iso9660_override *dirent;
    ventoy_img_chunk *chunknode;
    ventoy_override_chunk *override;
    ventoy_img_chunk_list wimchunk;
    char envbuf[128];
    
    (void)ctxt;
    (void)argc;

    debug("wim chain data begin <%s> ...\n", args[0]);

    if (NULL == g_wimiso_chunk_list.chunk || NULL == g_wimiso_path)
    {
        grub_printf("ventoy not ready\n");
        return 1;
    }

    imgoffset = ventoy_get_wim_iso_offset(g_wimiso_path);
    if (imgoffset == 0)
    {
        grub_printf("image offset not found\n");
        return 1;
    }

    if (0 != ventoy_get_wim_chunklist(args[0], &wimchunk, &wimsize))
    {
        grub_printf("Failed to get wim chunklist\n");
        return 1;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", g_wimiso_path);
    if (!file)
    {
        return 1;
    }

    boot_catlog = ventoy_get_iso_boot_catlog(file);

    img_chunk1_size = g_wimiso_chunk_list.cur_chunk * sizeof(ventoy_img_chunk);
    img_chunk2_size = wimchunk.cur_chunk * sizeof(ventoy_img_chunk);
    override_size = sizeof(ventoy_override_chunk);
    
    size = sizeof(ventoy_chain_head) + img_chunk1_size + img_chunk2_size + override_size;

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
    g_ventoy_chain_type = ventoy_chain_wim;
    ventoy_fill_os_param(file, &(chain->os_param));

    /* part 2: chain head */
    disk = file->device->disk;
    chain->disk_drive = disk->id;
    chain->disk_sector_size = (1 << disk->log_sector_size);
    chain->real_img_size_in_bytes = ventoy_align_2k(file->size) + ventoy_align_2k(wimsize);
    chain->virt_img_size_in_bytes = chain->real_img_size_in_bytes;
    chain->boot_catalog = boot_catlog;

    if (!ventoy_is_efi_os())
    {
        grub_file_seek(file, boot_catlog * 2048);
        grub_file_read(file, chain->boot_catalog_sector, sizeof(chain->boot_catalog_sector));
    }

    /* part 3: image chunk */
    chain->img_chunk_offset = sizeof(ventoy_chain_head);
    chain->img_chunk_num = g_wimiso_chunk_list.cur_chunk + wimchunk.cur_chunk;
    grub_memcpy((char *)chain + chain->img_chunk_offset, g_wimiso_chunk_list.chunk, img_chunk1_size);

    /* fs cluster size >= 2048, so don't need to proc align */

    /* align by 2048 */
    chunknode = wimchunk.chunk + wimchunk.cur_chunk - 1;
    i = (chunknode->disk_end_sector + 1 - chunknode->disk_start_sector) % 4;
    if (i)
    {
        chunknode->disk_end_sector += 4 - i;
    }

    isosector = (grub_uint32_t)((file->size + 2047) / 2048);
    for (i = 0; i < wimchunk.cur_chunk; i++)
    {
        chunknode = wimchunk.chunk + i;
        chunknode->img_start_sector = isosector;
        chunknode->img_end_sector = chunknode->img_start_sector + 
            ((chunknode->disk_end_sector + 1 - chunknode->disk_start_sector) / 4) - 1;
        isosector = chunknode->img_end_sector + 1;
    }
    
    grub_memcpy((char *)chain + chain->img_chunk_offset + img_chunk1_size, wimchunk.chunk, img_chunk2_size);

    /* part 4: override chunk */
    chain->override_chunk_offset = chain->img_chunk_offset + img_chunk1_size + img_chunk2_size;
    chain->override_chunk_num = 1;

    override = (ventoy_override_chunk *)((char *)chain + chain->override_chunk_offset);
    override->img_offset = imgoffset;
    override->override_size = sizeof(ventoy_iso9660_override);
    
    dirent = (ventoy_iso9660_override *)(override->override_data);
    dirent->first_sector    = (grub_uint32_t)((file->size + 2047) / 2048);
    dirent->size            = (grub_uint32_t)(wimsize);
    dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
    dirent->size_be         = grub_swap_bytes32(dirent->size);

    debug("imgoffset=%u first_sector=0x%x size=0x%x\n", imgoffset, dirent->first_sector, dirent->size);

    if (ventoy_is_efi_os() == 0)
    {
        ventoy_windows_drive_map(chain);        
    }

    grub_file_close(file);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

