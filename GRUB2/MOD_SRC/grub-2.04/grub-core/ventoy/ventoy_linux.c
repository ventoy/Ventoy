/******************************************************************************
 * ventoy_linux.c 
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


static char * ventoy_get_line(char *start)
{
    if (start == NULL)
    {
        return NULL;
    }

    while (*start && *start != '\n')
    {
        start++;
    }

    if (*start == 0)
    {
        return NULL;
    }
    else
    {
        *start = 0;
        return start + 1;
    }
}

static initrd_info * ventoy_find_initrd_by_name(initrd_info *list, const char *name)
{
    initrd_info *node = list;

    while (node)
    {
        if (grub_strcmp(node->name, name) == 0)
        {
            return node;
        }
        node = node->next;
    }

    return NULL;
}

grub_err_t ventoy_cmd_clear_initrd_list(grub_extcmd_context_t ctxt, int argc, char **args)
{
    initrd_info *node = g_initrd_img_list;
    initrd_info *next;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    while (node)
    {
        next = node->next;
        grub_free(node);
        node = next;
    }

    g_initrd_img_list = NULL;
    g_initrd_img_tail = NULL;
    g_initrd_img_count = 0;
    g_valid_initrd_count = 0;

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_dump_initrd_list(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    initrd_info *node = g_initrd_img_list;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    grub_printf("###################\n");
    grub_printf("initrd info list: valid count:%d\n", g_valid_initrd_count);

    while (node)
    {
        grub_printf("%s ", node->size > 0 ? "*" : " ");
        grub_printf("%02u %s  offset:%llu  size:%llu \n", i++, node->name, (unsigned long long)node->offset, 
                    (unsigned long long)node->size);
        node = node->next;
    }

    grub_printf("###################\n");

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static void ventoy_parse_directory(char *path, char *dir, int buflen)
{   
    int end;
    char *pos;

    pos = grub_strstr(path, ")");
    if (!pos)
    {
        pos = path;
    }

    end = grub_snprintf(dir, buflen, "%s", pos + 1);
    while (end > 0)
    {
        if (dir[end] == '/')
        {
            dir[end + 1] = 0;
            break;
        }
        end--;
    }
}

static grub_err_t ventoy_isolinux_initrd_collect(grub_file_t file, const char *prefix)
{
    int i = 0;
    int offset;
    int prefixlen = 0;
    char *buf = NULL;
    char *pos = NULL;
    char *start = NULL;
    char *nextline = NULL;
    initrd_info *img = NULL;

    prefixlen = grub_strlen(prefix);

    buf = grub_zalloc(file->size + 2);
    if (!buf)
    {
        return 0;
    }

    grub_file_read(file, buf, file->size);

    for (start = buf; start; start = nextline)
    {
        nextline = ventoy_get_line(start);

        while (ventoy_isspace(*start))
        {
            start++;
        }

        offset = 7; // strlen("initrd=") or "INITRD " or "initrd "
        pos = grub_strstr(start, "initrd=");
        if (pos == NULL)
        {
            pos = start;

            if (grub_strncmp(start, "INITRD", 6) != 0 && grub_strncmp(start, "initrd", 6) != 0)
            {
                if (grub_strstr(start, "xen") &&
                    ((pos = grub_strstr(start, "--- /install.img")) != NULL ||
                     (pos = grub_strstr(start, "--- initrd.img")) != NULL
                    ))
                {
                    offset = 4; // "--- "
                }
                else
                {
                    continue;
                }
            }
        }

        pos += offset; 

        while (1)
        {
            i = 0;
            img = grub_zalloc(sizeof(initrd_info));
            if (!img)
            {
                break;
            }

            if (*pos != '/')
            {
                grub_strcpy(img->name, prefix);
                i = prefixlen;
            }

            while (i < 255 && (0 == ventoy_is_word_end(*pos)))
            {
                img->name[i++] = *pos++;
            }

            if (ventoy_find_initrd_by_name(g_initrd_img_list, img->name))
            {
                grub_free(img);
            }
            else
            {
                if (g_initrd_img_list)
                {
                    img->prev = g_initrd_img_tail;
                    g_initrd_img_tail->next = img;
                }
                else
                {
                    g_initrd_img_list = img;
                }

                g_initrd_img_tail = img;
                g_initrd_img_count++;
            }

            if (*pos == ',')
            {
                pos++;
            }
            else
            {
                break;
            }
        }
    }

    grub_free(buf);
    return GRUB_ERR_NONE;
}

static int ventoy_isolinux_initrd_hook(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    grub_file_t file = NULL;
    ventoy_initrd_ctx *ctx = (ventoy_initrd_ctx *)data;

    (void)info;

    if (NULL == grub_strstr(filename, ".cfg") && NULL == grub_strstr(filename, ".CFG"))
    {
        return 0;
    }

    debug("init hook dir <%s%s>\n", ctx->path_prefix, filename);

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", ctx->path_prefix, filename);
    if (!file)
    {
        return 0;
    }

    ventoy_isolinux_initrd_collect(file, ctx->dir_prefix);
    grub_file_close(file);

    return 0;
}

grub_err_t ventoy_cmd_isolinux_initrd_collect(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_fs_t fs;
    grub_device_t dev = NULL;
    char *device_name = NULL;
    ventoy_initrd_ctx ctx;
    char directory[256];
    
    (void)ctxt;
    (void)argc;

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        goto end;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        goto end;        
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        goto end;
    }

    debug("isolinux initrd collect %s\n", args[0]);

    ventoy_parse_directory(args[0], directory, sizeof(directory) - 1);
    ctx.path_prefix = args[0];
    ctx.dir_prefix = (argc > 1) ? args[1] : directory;

    debug("path_prefix=<%s> dir_prefix=<%s>\n", ctx.path_prefix, ctx.dir_prefix);

    fs->fs_dir(dev, directory, ventoy_isolinux_initrd_hook, &ctx);

end:
    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_grub_cfg_initrd_collect(const char *fileName)
{
    int i = 0;
    grub_file_t file = NULL;
    char *buf = NULL;
    char *start = NULL;
    char *nextline = NULL;
    initrd_info *img = NULL;

    debug("grub initrd collect %s\n", fileName);

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", fileName);
    if (!file)
    {
        return 0;
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
        nextline = ventoy_get_line(start);

        while (ventoy_isspace(*start))
        {
            start++;
        }

        if (grub_strncmp(start, "initrd", 6) != 0)
        {
            continue;
        }

        start += 6;
        while (*start && (!ventoy_isspace(*start)))
        {
            start++;
        }

        while (ventoy_isspace(*start))
        {
            start++;
        }

        while (*start)
        {
            img = grub_zalloc(sizeof(initrd_info));
            if (!img)
            {
                break;
            }
            
            for (i = 0; i < 255 && (0 == ventoy_is_word_end(*start)); i++)
            {
                img->name[i] = *start++;
            }

            if (ventoy_find_initrd_by_name(g_initrd_img_list, img->name))
            {
                grub_free(img);
            }
            else
            {
                if (g_initrd_img_list)
                {
                    img->prev = g_initrd_img_tail;
                    g_initrd_img_tail->next = img;
                }
                else
                {
                    g_initrd_img_list = img;
                }

                g_initrd_img_tail = img;
                g_initrd_img_count++;
            }

            if (*start == ' ' || *start == '\t')
            {
                while (ventoy_isspace(*start))
                {
                    start++;
                }
            }
            else
            {
                break;
            }
        }
    }

    grub_free(buf);
    grub_file_close(file);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static int ventoy_grub_initrd_hook(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    char filePath[256];
    ventoy_initrd_ctx *ctx = (ventoy_initrd_ctx *)data;

    (void)info;

    debug("ventoy_grub_initrd_hook %s\n", filename);

    if (NULL == grub_strstr(filename, ".cfg") && 
        NULL == grub_strstr(filename, ".CFG") && 
        NULL == grub_strstr(filename, ".conf"))
    {
        return 0;
    }

    debug("init hook dir <%s%s>\n", ctx->path_prefix, filename);

    grub_snprintf(filePath, sizeof(filePath) - 1, "%s%s", ctx->dir_prefix, filename);
    ventoy_grub_cfg_initrd_collect(filePath);

    return 0;
}

grub_err_t ventoy_cmd_grub_initrd_collect(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_fs_t fs;
    grub_device_t dev = NULL;
    char *device_name = NULL;
    ventoy_initrd_ctx ctx;
    
    (void)ctxt;
    (void)argc;

    if (argc != 2)
    {
        return 0;
    }

    debug("grub initrd collect %s %s\n", args[0], args[1]);

    if (grub_strcmp(args[0], "file") == 0)
    {
        return ventoy_grub_cfg_initrd_collect(args[1]);
    }

    device_name = grub_file_get_device_name(args[1]);
    if (!device_name)
    {
        debug("failed to get device name %s\n", args[1]);
        goto end;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        debug("failed to open device %s\n", device_name);
        goto end;        
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        debug("failed to probe fs %d\n", grub_errno);
        goto end;
    }

    ctx.dir_prefix = args[1];
    ctx.path_prefix = grub_strstr(args[1], device_name);
    if (ctx.path_prefix)
    {
        ctx.path_prefix += grub_strlen(device_name) + 1;
    }
    else
    {
        ctx.path_prefix = args[1];
    }

    debug("ctx.path_prefix:<%s>\n", ctx.path_prefix);

    fs->fs_dir(dev, ctx.path_prefix, ventoy_grub_initrd_hook, &ctx);

end:
    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);


    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_specify_initrd_file(grub_extcmd_context_t ctxt, int argc, char **args)
{
    initrd_info *img = NULL;

    (void)ctxt;
    (void)argc;

    debug("ventoy_cmd_specify_initrd_file %s\n", args[0]);

    img = grub_zalloc(sizeof(initrd_info));
    if (!img)
    {
        return 1;
    }

    grub_strncpy(img->name, args[0], sizeof(img->name));
    if (ventoy_find_initrd_by_name(g_initrd_img_list, img->name))
    {
        debug("%s is already exist\n", args[0]);
        grub_free(img);
    }
    else
    {
        if (g_initrd_img_list)
        {
            img->prev = g_initrd_img_tail;
            g_initrd_img_tail->next = img;
        }
        else
        {
            g_initrd_img_list = img;
        }

        g_initrd_img_tail = img;
        g_initrd_img_count++;
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static void ventoy_cpio_newc_fill_int(grub_uint32_t value, char *buf, int buflen)
{
    int i;
    int len;
    char intbuf[32];

    len = grub_snprintf(intbuf, sizeof(intbuf), "%x", value);

    for (i = 0; i < buflen; i++)
    {
        buf[i] = '0';
    }

    if (len > buflen)
    {
        grub_printf("int buf len overflow %d %d\n", len, buflen);
    }
    else
    {
        grub_memcpy(buf + buflen - len, intbuf, len);    
    }
}

int ventoy_cpio_newc_fill_head(void *buf, int filesize, const void *filedata, const char *name)
{
    int namelen = 0;
    int headlen = 0;
    static grub_uint32_t cpio_ino = 0xFFFFFFF0;
    cpio_newc_header *cpio = (cpio_newc_header *)buf;
    
    namelen = grub_strlen(name) + 1;
    headlen = sizeof(cpio_newc_header) + namelen;
    headlen = ventoy_align(headlen, 4);

    grub_memset(cpio, '0', sizeof(cpio_newc_header));
    grub_memset(cpio + 1, 0, headlen - sizeof(cpio_newc_header));

    grub_memcpy(cpio->c_magic, "070701", 6);
    ventoy_cpio_newc_fill_int(cpio_ino--, cpio->c_ino, 8);
    ventoy_cpio_newc_fill_int(0100777, cpio->c_mode, 8);
    ventoy_cpio_newc_fill_int(1, cpio->c_nlink, 8);
    ventoy_cpio_newc_fill_int(filesize, cpio->c_filesize, 8);
    ventoy_cpio_newc_fill_int(namelen, cpio->c_namesize, 8);
    grub_memcpy(cpio + 1, name, namelen);

    if (filedata)
    {
        grub_memcpy((char *)cpio + headlen, filedata, filesize);
    }

    return headlen;
}

static grub_uint32_t ventoy_linux_get_virt_chunk_size(void)
{
    return (sizeof(ventoy_virt_chunk) + g_ventoy_cpio_size) * g_valid_initrd_count;
}

static void ventoy_linux_fill_virt_data(    grub_uint64_t isosize, ventoy_chain_head *chain)
{
    int id = 0;
    initrd_info *node;
    grub_uint64_t sector;
    grub_uint32_t offset;
    grub_uint32_t cpio_secs;
    grub_uint32_t initrd_secs;
    char *override;
    ventoy_virt_chunk *cur;
    char name[32];

    override = (char *)chain + chain->virt_chunk_offset;
    sector = (isosize + 2047) / 2048;
    cpio_secs = g_ventoy_cpio_size / 2048;

    offset = g_valid_initrd_count * sizeof(ventoy_virt_chunk);
    cur = (ventoy_virt_chunk *)override;

    for (node = g_initrd_img_list; node; node = node->next)
    {
        if (node->size == 0)
        {
            continue;
        }

        initrd_secs = (grub_uint32_t)((node->size + 2047) / 2048);

        cur->mem_sector_start   = sector;
        cur->mem_sector_end     = cur->mem_sector_start + cpio_secs;
        cur->mem_sector_offset  = offset;
        cur->remap_sector_start = cur->mem_sector_end;
        cur->remap_sector_end   = cur->remap_sector_start + initrd_secs;
        cur->org_sector_start   = (grub_uint32_t)(node->offset / 2048);

        grub_memcpy(g_ventoy_runtime_buf, &chain->os_param, sizeof(ventoy_os_param));

        grub_memset(name, 0, 16);
        grub_snprintf(name, sizeof(name), "initrd%03d", ++id);

        grub_memcpy(g_ventoy_initrd_head + 1, name, 16);
        ventoy_cpio_newc_fill_int((grub_uint32_t)node->size, g_ventoy_initrd_head->c_filesize, 8);

        grub_memcpy(override + offset, g_ventoy_cpio_buf, g_ventoy_cpio_size);

        chain->virt_img_size_in_bytes += g_ventoy_cpio_size + initrd_secs * 2048;

        offset += g_ventoy_cpio_size;
        sector += cpio_secs + initrd_secs;
        cur++;
    }

    return;
}

static grub_uint32_t ventoy_linux_get_override_chunk_size(void)
{
    return sizeof(ventoy_override_chunk) * g_valid_initrd_count;
}

static void ventoy_linux_fill_override_data(    grub_uint64_t isosize, void *override)
{
    initrd_info *node;
    grub_uint64_t sector;
    ventoy_override_chunk *cur;

    sector = (isosize + 2047) / 2048;

    cur = (ventoy_override_chunk *)override;
    for (node = g_initrd_img_list; node; node = node->next)
    {
        if (node->size == 0)
        {
            continue;
        }

        if (node->iso_type == 0)
        {
            ventoy_iso9660_override *dirent = (ventoy_iso9660_override *)node->override_data;

            node->override_length   = sizeof(ventoy_iso9660_override);
            dirent->first_sector    = (grub_uint32_t)sector;
            dirent->size            = (grub_uint32_t)(node->size + g_ventoy_cpio_size);
            dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
            dirent->size_be         = grub_swap_bytes32(dirent->size);

            sector += (dirent->size + 2047) / 2048;
        }
        else
        {
            ventoy_udf_override *udf = (ventoy_udf_override *)node->override_data;
            
            node->override_length = sizeof(ventoy_udf_override);
            udf->length   = (grub_uint32_t)(node->size + g_ventoy_cpio_size);
            udf->position = (grub_uint32_t)sector - node->udf_start_block;

            sector += (udf->length + 2047) / 2048;
        }

        cur->img_offset = node->override_offset;
        cur->override_size = node->override_length;
        grub_memcpy(cur->override_data, node->override_data, cur->override_size);
        cur++;
    }

    return;
}

grub_err_t ventoy_cmd_initrd_count(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char buf[32] = {0};
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc == 1)
    {
        grub_snprintf(buf, sizeof(buf), "%d", g_initrd_img_count);
        grub_env_set(args[0], buf);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_valid_initrd_count(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char buf[32] = {0};
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc == 1)
    {
        grub_snprintf(buf, sizeof(buf), "%d", g_valid_initrd_count);
        grub_env_set(args[0], buf);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_linux_locate_initrd(int filt, int *filtcnt)
{
    int data;
    int filtbysize = 1;
    int sizefilt = 0;
    grub_file_t file;
    initrd_info *node;

    debug("ventoy_linux_locate_initrd %d\n", filt);

    g_valid_initrd_count = 0;

    if (grub_env_get("INITRD_NO_SIZE_FILT"))
    {
        filtbysize = 0;
    }
    
    for (node = g_initrd_img_list; node; node = node->next)
    {
        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "(loop)%s", node->name);
        if (!file)
        {
            continue;
        }

        debug("file <%s> size:%d\n", node->name, (int)file->size);

        /* initrd file too small */
        if (filtbysize 
            && (NULL == grub_strstr(node->name, "minirt.gz"))
            && (NULL == grub_strstr(node->name, "initrd.xz"))
            )
        {
            if (filt > 0 && file->size <= g_ventoy_cpio_size + 2048)
            {
                debug("file size too small %d\n", (int)g_ventoy_cpio_size);
                grub_file_close(file);
                sizefilt++;
                continue;
            }
        }

        if (grub_strcmp(file->fs->name, "iso9660") == 0)
        {
            node->iso_type = 0;
            node->override_offset = grub_iso9660_get_last_file_dirent_pos(file) + 2;
            
            grub_file_read(file, &data, 1); // just read for hook trigger
            node->offset = grub_iso9660_get_last_read_pos(file);
        }
        else
        {
            /* TBD */
        }

        node->size = file->size;
        g_valid_initrd_count++;

        grub_file_close(file);
    }

    *filtcnt = sizefilt;

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}


grub_err_t ventoy_cmd_linux_locate_initrd(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int sizefilt = 0;

    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_linux_locate_initrd(1, &sizefilt);

    if (g_valid_initrd_count == 0 && sizefilt > 0)
    {
        ventoy_linux_locate_initrd(0, &sizefilt);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_load_cpio(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc;
    char *template_file = NULL;
    char *template_buf = NULL;
    char *persistent_buf = NULL;
    grub_uint8_t *buf = NULL;
    grub_uint32_t mod;
    grub_uint32_t headlen;
    grub_uint32_t initrd_head_len;
    grub_uint32_t padlen;
    grub_uint32_t img_chunk_size;
    grub_uint32_t template_size = 0;
    grub_uint32_t persistent_size = 0;
    grub_file_t file;
    grub_file_t scriptfile;
    ventoy_img_chunk_list chunk_list;

    (void)ctxt;
    (void)argc;

    if (argc != 3)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s cpiofile\n", cmd_raw_name); 
    }

    if (g_img_chunk_list.chunk == NULL || g_img_chunk_list.cur_chunk == 0)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "image chunk is null\n");
    }

    img_chunk_size = g_img_chunk_list.cur_chunk * sizeof(ventoy_img_chunk);

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't open file %s\n", args[0]); 
    }

    if (g_ventoy_cpio_buf)
    {
        grub_free(g_ventoy_cpio_buf);
        g_ventoy_cpio_buf = NULL;
        g_ventoy_cpio_size = 0;
    }

    rc = ventoy_plugin_get_persistent_chunklist(args[1], &chunk_list);
    if (rc == 0 && chunk_list.cur_chunk > 0 && chunk_list.chunk)
    {
        persistent_size = chunk_list.cur_chunk * sizeof(ventoy_img_chunk);
        persistent_buf = (char *)(chunk_list.chunk);
    }

    template_file = ventoy_plugin_get_install_template(args[1]);
    if (template_file)
    {
        debug("auto install template: <%s>\n", template_file);
        scriptfile = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", args[2], template_file);
        if (scriptfile)
        {
            debug("auto install script size %d\n", (int)scriptfile->size);
            template_size = scriptfile->size;
            template_buf = grub_malloc(template_size);
            if (template_buf)
            {
                grub_file_read(scriptfile, template_buf, template_size);
            }

            grub_file_close(scriptfile);
        }
        else
        {
            debug("Failed to open install script %s%s\n", args[2], template_file);
        }
    }

    g_ventoy_cpio_buf = grub_malloc(file->size + 4096 + template_size + persistent_size + img_chunk_size);
    if (NULL == g_ventoy_cpio_buf)
    {
        grub_file_close(file);
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't alloc memory %llu\n", file->size + 4096 + img_chunk_size); 
    }

    grub_file_read(file, g_ventoy_cpio_buf, file->size);

    buf = (grub_uint8_t *)(g_ventoy_cpio_buf + file->size - 4);
    while (*((grub_uint32_t *)buf) != 0x37303730)
    {
        buf -= 4;
    }

    /* get initrd head len */
    initrd_head_len = ventoy_cpio_newc_fill_head(buf, 0, NULL, "initrd000.xx");

    /* step1: insert image chunk data to cpio */
    headlen = ventoy_cpio_newc_fill_head(buf, img_chunk_size, g_img_chunk_list.chunk, "ventoy/ventoy_image_map");
    buf += headlen + ventoy_align(img_chunk_size, 4);

    if (template_buf)
    {
        headlen = ventoy_cpio_newc_fill_head(buf, template_size, template_buf, "ventoy/autoinstall");
        buf += headlen + ventoy_align(template_size, 4);
    }

    if (persistent_size > 0 && persistent_buf)
    {
        headlen = ventoy_cpio_newc_fill_head(buf, persistent_size, persistent_buf, "ventoy/ventoy_persistent_map");
        buf += headlen + ventoy_align(persistent_size, 4);

        grub_free(persistent_buf);
        persistent_buf = NULL;
    }

    /* step2: insert os param to cpio */
    headlen = ventoy_cpio_newc_fill_head(buf, 0, NULL, "ventoy/ventoy_os_param");
    padlen = sizeof(ventoy_os_param);
    g_ventoy_cpio_size = (grub_uint32_t)(buf - g_ventoy_cpio_buf) + headlen + padlen + initrd_head_len;
    mod = g_ventoy_cpio_size % 2048;
    if (mod)
    {
        g_ventoy_cpio_size += 2048 - mod;
        padlen += 2048 - mod;
    }

    /* update os param data size, the data will be updated before chain boot */
    ventoy_cpio_newc_fill_int(padlen, ((cpio_newc_header *)buf)->c_filesize, 8);
    g_ventoy_runtime_buf = (grub_uint8_t *)buf + headlen;

    /* step3: fill initrd cpio head, the file size will be updated before chain boot */
    g_ventoy_initrd_head = (cpio_newc_header *)(g_ventoy_runtime_buf + padlen);
    ventoy_cpio_newc_fill_head(g_ventoy_initrd_head, 0, NULL, "initrd000.xx");

    grub_file_close(file);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}


grub_err_t ventoy_cmd_linux_chain_data(grub_extcmd_context_t ctxt, int argc, char **args)
{
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

    compatible = grub_env_get("ventoy_compatible");
    if (compatible && compatible[0] == 'Y')
    {
        ventoy_compatible = 1;
    }

    if ((NULL == g_img_chunk_list.chunk) || (0 == ventoy_compatible && g_ventoy_cpio_buf == NULL))
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
        override_size = ventoy_linux_get_override_chunk_size();
        virt_chunk_size = ventoy_linux_get_virt_chunk_size();
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

    if (g_valid_initrd_count == 0)
    {
        return 0;
    }

    /* part 4: override chunk */
    chain->override_chunk_offset = chain->img_chunk_offset + img_chunk_size;
    chain->override_chunk_num = g_valid_initrd_count;
    ventoy_linux_fill_override_data(isosize, (char *)chain + chain->override_chunk_offset);

    /* part 5: virt chunk */
    chain->virt_chunk_offset = chain->override_chunk_offset + override_size;
    chain->virt_chunk_num = g_valid_initrd_count;
    ventoy_linux_fill_virt_data(isosize, chain);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

