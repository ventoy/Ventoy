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

#define VTOY_APPEND_EXT_SIZE 4096
static int g_append_ext_sector = 0;

char * ventoy_get_line(char *start)
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

        VTOY_SKIP_SPACE(start);

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

static int ventoy_linux_initrd_collect_hook(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    int len;
    initrd_info *img = NULL;
    
    (void)data;

    if (0 == info->dir)
    {
        if (grub_strncmp(filename, "initrd", 6) == 0)
        {
            len = (int)grub_strlen(filename);
            if (grub_strcmp(filename + len - 4, ".img") == 0)
            {
                img = grub_zalloc(sizeof(initrd_info));
                if (img)
                {
                    grub_snprintf(img->name, sizeof(img->name), "/boot/%s", filename);

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
                }
            }
        }
    }

    return 0;
}

static int ventoy_linux_collect_boot_initrds(void)
{
    grub_fs_t fs;
    grub_device_t dev = NULL;

    dev = grub_device_open("loop");
    if (!dev)
    {
        debug("failed to open device loop\n");
        goto end;        
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        debug("failed to probe fs %d\n", grub_errno);
        goto end;
    }

    fs->fs_dir(dev, "/boot", ventoy_linux_initrd_collect_hook, NULL);

end:
    return 0;    
}

static grub_err_t ventoy_grub_cfg_initrd_collect(const char *fileName)
{
    int i = 0;
    int len = 0;
    int dollar = 0;
    int quotation = 0;
    int initrd_dollar = 0;
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

        VTOY_SKIP_SPACE(start);

        if (grub_strncmp(start, "initrd", 6) != 0)
        {
            continue;
        }

        start += 6;
        while (*start && (!ventoy_isspace(*start)))
        {
            start++;
        }

        VTOY_SKIP_SPACE(start);

        if (*start == '"')
        {
            quotation = 1;
            start++;
        }

        while (*start)
        {
            img = grub_zalloc(sizeof(initrd_info));
            if (!img)
            {
                break;
            }

            dollar = 0;
            for (i = 0; i < 255 && (0 == ventoy_is_word_end(*start)); i++)
            {
                img->name[i] = *start++;
                if (img->name[i] == '$')
                {
                    dollar = 1;
                }
            }

            if (quotation)
            {
                len = (int)grub_strlen(img->name);
                if (len > 2 && img->name[len - 1] == '"')
                {
                    img->name[len - 1] = 0;
                }
                debug("Remove quotation <%s>\n", img->name);
            }

            /* special process for /boot/initrd$XXX.img */
            if (dollar == 1)
            {
                if (grub_strncmp(img->name, "/boot/initrd$", 13) == 0)
                {
                    len = (int)grub_strlen(img->name);
                    if (grub_strcmp(img->name + len - 4, ".img") == 0)
                    {
                        initrd_dollar++;                        
                    }                    
                }
            }

            if (dollar == 1 || ventoy_find_initrd_by_name(g_initrd_img_list, img->name))
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
                VTOY_SKIP_SPACE(start);
            }
            else
            {
                break;
            }
        }
    }

    grub_free(buf);
    grub_file_close(file);

    if (initrd_dollar > 0 && grub_strncmp(fileName, "(loop)/", 7) == 0)
    {
        debug("collect initrd variable %d\n", initrd_dollar);
        ventoy_linux_collect_boot_initrds();
    }

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

static int ventoy_cpio_newc_get_int(char *value)
{
    char buf[16] = {0};

    grub_memcpy(buf, value, 8);
    return (int)grub_strtoul(buf, NULL, 16);
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

static grub_uint32_t ventoy_linux_get_virt_chunk_count(void)
{
    int i;
    grub_uint32_t count = g_valid_initrd_count;
    
    if (g_conf_replace_count > 0)
    {
        for (i = 0; i < g_conf_replace_count; i++)
        {
            if (g_conf_replace_offset[i] > 0)
            {
                count++;
            }
        }
    }
    
    if (g_append_ext_sector > 0)
    {
        count++;
    }
    
    return count;
}

static grub_uint32_t ventoy_linux_get_virt_chunk_size(void)
{
    int i;
    grub_uint32_t size;
    
    size = (sizeof(ventoy_virt_chunk) + g_ventoy_cpio_size) * g_valid_initrd_count;

    if (g_conf_replace_count > 0)
    {
        for (i = 0; i < g_conf_replace_count; i++)
        {
            if (g_conf_replace_offset[i] > 0)
            {
                size += sizeof(ventoy_virt_chunk) + g_conf_replace_new_len_align[i];
            }
        }
    }
    
    if (g_append_ext_sector > 0)
    {
        size += sizeof(ventoy_virt_chunk) + VTOY_APPEND_EXT_SIZE;
    }

    return size;
}

static void ventoy_linux_fill_virt_data(    grub_uint64_t isosize, ventoy_chain_head *chain)
{
    int i = 0;
    int id = 0;
    int virtid = 0;
    initrd_info *node;
    grub_uint64_t sector;
    grub_uint32_t offset;
    grub_uint32_t cpio_secs;
    grub_uint32_t initrd_secs;
    char *override;
    ventoy_virt_chunk *cur;
    ventoy_grub_param_file_replace *replace = NULL;
    char name[32];

    override = (char *)chain + chain->virt_chunk_offset;
    sector = (isosize + 2047) / 2048;
    cpio_secs = g_ventoy_cpio_size / 2048;

    offset = ventoy_linux_get_virt_chunk_count() * sizeof(ventoy_virt_chunk);
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
        virtid++;
    }

    /* Lenovo EasyStartup need an addional sector for boundary check */
    if (g_append_ext_sector > 0)
    {
        cpio_secs = VTOY_APPEND_EXT_SIZE / 2048;
    
        cur->mem_sector_start   = sector;
        cur->mem_sector_end     = cur->mem_sector_start + cpio_secs;
        cur->mem_sector_offset  = offset;
        cur->remap_sector_start = 0;
        cur->remap_sector_end   = 0;
        cur->org_sector_start   = 0;

        grub_memset(override + offset, 0, VTOY_APPEND_EXT_SIZE);

        chain->virt_img_size_in_bytes += VTOY_APPEND_EXT_SIZE;

        offset += VTOY_APPEND_EXT_SIZE;
        sector += cpio_secs;
        cur++;
        virtid++;
    }

    if (g_conf_replace_count > 0)
    {
        for (i = 0; i < g_conf_replace_count; i++)
        {
            if (g_conf_replace_offset[i] > 0)
            {
                cpio_secs = g_conf_replace_new_len_align[i] / 2048;
            
                cur->mem_sector_start   = sector;
                cur->mem_sector_end     = cur->mem_sector_start + cpio_secs;
                cur->mem_sector_offset  = offset;
                cur->remap_sector_start = 0;
                cur->remap_sector_end   = 0;
                cur->org_sector_start   = 0;

                grub_memcpy(override + offset, g_conf_replace_new_buf[i], g_conf_replace_new_len[i]);

                chain->virt_img_size_in_bytes += g_conf_replace_new_len_align[i];

                replace = g_grub_param->img_replace + i;
                if (replace->magic == GRUB_IMG_REPLACE_MAGIC)
                {
                    replace->new_file_virtual_id = virtid;
                }

                offset += g_conf_replace_new_len_align[i];
                sector += cpio_secs;
                cur++;
                virtid++;
            }
        }
    }

    return;
}

static grub_uint32_t ventoy_linux_get_override_chunk_count(void)
{
    int i;
    grub_uint32_t count = g_valid_initrd_count;

    if (g_conf_replace_count > 0)
    {
        for (i = 0; i < g_conf_replace_count; i++)
        {
            if (g_conf_replace_offset[i] > 0)
            {
                count++;
            }
        }
    }

    if (g_svd_replace_offset > 0)
    {
        count++;
    }
    
    return count;
}

static grub_uint32_t ventoy_linux_get_override_chunk_size(void)
{
    int i;
    int count = g_valid_initrd_count;

    if (g_conf_replace_count > 0)
    {
        for (i = 0; i < g_conf_replace_count; i++)
        {
            if (g_conf_replace_offset[i] > 0)
            {
                count++;
            }
        }
    }

    if (g_svd_replace_offset > 0)
    {
        count++;
    }

    return sizeof(ventoy_override_chunk) * count;
}

static void ventoy_linux_fill_override_data(    grub_uint64_t isosize, void *override)
{
    int i;
    initrd_info *node;
    grub_uint32_t mod;
    grub_uint32_t newlen;
    grub_uint64_t sector;
    ventoy_override_chunk *cur;
    ventoy_iso9660_override *dirent;
    ventoy_udf_override *udf;

    sector = (isosize + 2047) / 2048;

    cur = (ventoy_override_chunk *)override;
    for (node = g_initrd_img_list; node; node = node->next)
    {
        if (node->size == 0)
        {
            continue;
        }

        newlen = (grub_uint32_t)(node->size + g_ventoy_cpio_size);
        mod = newlen % 4; 
        if (mod > 0)
        {
            newlen += 4 - mod; /* cpio must align with 4 */
        }

        if (node->iso_type == 0)
        {
            dirent = (ventoy_iso9660_override *)node->override_data;

            node->override_length   = sizeof(ventoy_iso9660_override);
            dirent->first_sector    = (grub_uint32_t)sector;
            dirent->size            = newlen;
            dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
            dirent->size_be         = grub_swap_bytes32(dirent->size);

            sector += (dirent->size + 2047) / 2048;
        }
        else
        {
            udf = (ventoy_udf_override *)node->override_data;
            
            node->override_length = sizeof(ventoy_udf_override);
            udf->length   = newlen;
            udf->position = (grub_uint32_t)sector - node->udf_start_block;

            sector += (udf->length + 2047) / 2048;
        }

        cur->img_offset = node->override_offset;
        cur->override_size = node->override_length;
        grub_memcpy(cur->override_data, node->override_data, cur->override_size);
        cur++;
    }

    if (g_conf_replace_count > 0)
    {
        for (i = 0; i < g_conf_replace_count; i++)
        {
            if (g_conf_replace_offset[i] > 0)
            {        
                cur->img_offset = g_conf_replace_offset[i];
                cur->override_size = sizeof(ventoy_iso9660_override);

                newlen = (grub_uint32_t)(g_conf_replace_new_len[i]);

                dirent = (ventoy_iso9660_override *)cur->override_data;
                dirent->first_sector    = (grub_uint32_t)sector;
                dirent->size            = newlen;
                dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
                dirent->size_be         = grub_swap_bytes32(dirent->size);

                sector += (dirent->size + 2047) / 2048;
                cur++;
            }
        }
    }

    if (g_svd_replace_offset > 0)
    {        
        cur->img_offset = g_svd_replace_offset;
        cur->override_size = 1;
        cur->override_data[0] = 0xFF;
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
            && (NULL == grub_strstr(node->name, "initrd.gz"))
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

        /* skip hdt.img */
        if (file->size <= VTOY_SIZE_1MB && grub_strcmp(node->name, "/boot/hdt.img") == 0)
        {
            continue;
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


grub_err_t ventoy_cmd_linux_get_main_initrd_index(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int index = 0;
    char buf[32];
    initrd_info *node = NULL;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 1)
    {
        return 1;
    }

    if (g_initrd_img_count == 1)
    {
        ventoy_set_env(args[0], "0");
        VENTOY_CMD_RETURN(GRUB_ERR_NONE);
    }

    for (node = g_initrd_img_list; node; node = node->next)
    {
        if (node->size <= 0)
        {
            continue;
        }
    
        if (grub_strstr(node->name, "ucode") || grub_strstr(node->name, "-firmware"))
        {
            index++;
            continue;
        }

        grub_snprintf(buf, sizeof(buf), "%d", index);
        ventoy_set_env(args[0], buf);
        break;
    }

    debug("main initrd index:%d\n", index);

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

static int ventoy_cpio_busybox64(cpio_newc_header *head, const char *file)
{
    char *name;
    int namelen;
    int offset;
    int count = 0;
    char filepath[128];

    grub_snprintf(filepath, sizeof(filepath), "ventoy/busybox/%s", file);
    
    name = (char *)(head + 1);
    while (name[0] && count < 2)
    {
        if (grub_strcmp(name, "ventoy/busybox/ash") == 0)
        {
            grub_memcpy(name, "ventoy/busybox/32h", 18);
            count++;
        }
        else if (grub_strcmp(name, filepath) == 0)
        {
            grub_memcpy(name, "ventoy/busybox/ash", 18);
            count++;
        }

        namelen = ventoy_cpio_newc_get_int(head->c_namesize);
        offset = sizeof(cpio_newc_header) + namelen;
        offset = ventoy_align(offset, 4);
        offset += ventoy_cpio_newc_get_int(head->c_filesize);
        offset = ventoy_align(offset, 4);
        
        head = (cpio_newc_header *)((char *)head + offset);
        name = (char *)(head + 1);
    }

    return 0;
}


grub_err_t ventoy_cmd_cpio_busybox_64(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    debug("ventoy_cmd_busybox_64 %d\n", argc);
    ventoy_cpio_busybox64((cpio_newc_header *)g_ventoy_cpio_buf, args[0]);
    return 0;
}

grub_err_t ventoy_cmd_skip_svd(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    grub_file_t file;
    char buf[16];
    
    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't open file %s\n", args[0]); 
    }

    for (i = 0; i < 10; i++)
    {
        buf[0] = 0;
        grub_file_seek(file, (17 + i) * 2048);
        grub_file_read(file, buf, 16);

        if (buf[0] == 2 && grub_strncmp(buf + 1, "CD001", 5) == 0)
        {
            debug("Find SVD at VD %d\n", i);
            g_svd_replace_offset = (17 + i) * 2048;
            break;
        }
    }

    if (i >= 10)
    {
        debug("SVD not found %d\n", (int)g_svd_replace_offset);
    }

    grub_file_close(file);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_append_ext_sector(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (args[0][0] == '1')
    {
        g_append_ext_sector = 1;        
    }
    else
    {
        g_append_ext_sector = 0;
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_load_cpio(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int rc;
    char *pos = NULL;
    char *template_file = NULL;
    char *template_buf = NULL;
    char *persistent_buf = NULL;
    char *injection_buf = NULL;
    dud *dudnode = NULL;
    char tmpname[128];
    const char *injection_file = NULL;
    grub_uint8_t *buf = NULL;
    grub_uint32_t mod;
    grub_uint32_t headlen;
    grub_uint32_t initrd_head_len;
    grub_uint32_t padlen;
    grub_uint32_t img_chunk_size;
    grub_uint32_t template_size = 0;
    grub_uint32_t persistent_size = 0;
    grub_uint32_t injection_size = 0;
    grub_uint32_t dud_size = 0;
    grub_file_t file;
    grub_file_t archfile;
    grub_file_t tmpfile;
    install_template *template_node = NULL;
    ventoy_img_chunk_list chunk_list;

    (void)ctxt;
    (void)argc;

    if (argc != 4)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s cpiofile\n", cmd_raw_name); 
    }

    if (g_img_chunk_list.chunk == NULL || g_img_chunk_list.cur_chunk == 0)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "image chunk is null\n");
    }

    img_chunk_size = g_img_chunk_list.cur_chunk * sizeof(ventoy_img_chunk);

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/%s", args[0], VTOY_COMM_CPIO);
    if (!file)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't open file %s/%s\n", args[0], VTOY_COMM_CPIO); 
    }

    archfile = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/%s", args[0], VTOY_ARCH_CPIO);
    if (!archfile)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't open file %s/%s\n", args[0], VTOY_ARCH_CPIO);
        grub_file_close(file);
    }

    debug("load %s %s success\n", VTOY_COMM_CPIO, VTOY_ARCH_CPIO);

    if (g_ventoy_cpio_buf)
    {
        grub_free(g_ventoy_cpio_buf);
        g_ventoy_cpio_buf = NULL;
        g_ventoy_cpio_size = 0;
    }

    rc = ventoy_plugin_get_persistent_chunklist(args[1], -1, &chunk_list);
    if (rc == 0 && chunk_list.cur_chunk > 0 && chunk_list.chunk)
    {
        persistent_size = chunk_list.cur_chunk * sizeof(ventoy_img_chunk);
        persistent_buf = (char *)(chunk_list.chunk);
    }

    template_file = ventoy_plugin_get_cur_install_template(args[1], &template_node);
    if (template_file)
    {
        debug("auto install template: <%s> <addr:%p> <len:%d>\n", 
            template_file, template_node->filebuf, template_node->filelen);
        
        template_size = template_node->filelen;
        template_buf = grub_malloc(template_size);
        if (template_buf)
        {
            grub_memcpy(template_buf, template_node->filebuf, template_size);
        }
    }
    else
    {
        debug("auto install script skipped or not configed %s\n", args[1]);
    }

    injection_file = ventoy_plugin_get_injection(args[1]);
    if (injection_file)
    {
        debug("injection archive: <%s>\n", injection_file);
        tmpfile = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", args[2], injection_file);
        if (tmpfile)
        {
            debug("injection archive size:%d\n", (int)tmpfile->size);
            injection_size = tmpfile->size;
            injection_buf = grub_malloc(injection_size);
            if (injection_buf)
            {
                grub_file_read(tmpfile, injection_buf, injection_size);
            }

            grub_file_close(tmpfile);
        }
        else
        {
            debug("Failed to open injection archive %s%s\n", args[2], injection_file);
        }
    }
    else
    {
        debug("injection not configed %s\n", args[1]);
    }

    dudnode = ventoy_plugin_find_dud(args[1]);
    if (dudnode)
    {
        debug("dud file: <%d>\n", dudnode->dudnum);
        ventoy_plugin_load_dud(dudnode, args[2]);
        for (i = 0; i < dudnode->dudnum; i++)
        {
            if (dudnode->files[i].size > 0)
            {
                dud_size += dudnode->files[i].size + sizeof(cpio_newc_header);                
            }
        }
    }
    else
    {
        debug("dud not configed %s\n", args[1]);
    }

    g_ventoy_cpio_buf = grub_malloc(file->size + archfile->size + 40960 + template_size + 
        persistent_size + injection_size + dud_size + img_chunk_size);
    if (NULL == g_ventoy_cpio_buf)
    {
        grub_file_close(file);
        grub_file_close(archfile);
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't alloc memory %llu\n", file->size);
    }

    grub_file_read(file, g_ventoy_cpio_buf, file->size);
    buf = (grub_uint8_t *)(g_ventoy_cpio_buf + file->size - 4);
    while (*((grub_uint32_t *)buf) != 0x37303730)
    {
        buf -= 4;
    }

    grub_file_read(archfile, buf, archfile->size);
    buf += (archfile->size - 4);
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
        grub_check_free(template_buf);
    }

    if (persistent_size > 0 && persistent_buf)
    {
        headlen = ventoy_cpio_newc_fill_head(buf, persistent_size, persistent_buf, "ventoy/ventoy_persistent_map");
        buf += headlen + ventoy_align(persistent_size, 4);
        grub_check_free(persistent_buf);
    }

    if (injection_size > 0 && injection_buf)
    {
        headlen = ventoy_cpio_newc_fill_head(buf, injection_size, injection_buf, "ventoy/ventoy_injection");
        buf += headlen + ventoy_align(injection_size, 4);

        grub_free(injection_buf);
        injection_buf = NULL;
    }

    if (dud_size > 0)
    {
        for (i = 0; i < dudnode->dudnum; i++)
        {
            pos = grub_strrchr(dudnode->dudpath[i].path, '.');
            grub_snprintf(tmpname, sizeof(tmpname), "ventoy/ventoy_dud%d%s", i, (pos ? pos : ".iso"));
            dud_size = dudnode->files[i].size;
            headlen = ventoy_cpio_newc_fill_head(buf, dud_size, dudnode->files[i].buf, tmpname);
            buf += headlen + ventoy_align(dud_size, 4);
        }
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
    grub_file_close(archfile);

    if (grub_strcmp(args[3], "busybox=64") == 0)
    {
        debug("cpio busybox proc %s\n", args[3]);
        ventoy_cpio_busybox64((cpio_newc_header *)g_ventoy_cpio_buf, "64h");
    }
    else if (grub_strcmp(args[3], "busybox=a64") == 0)
    {
        debug("cpio busybox proc %s\n", args[3]);
        ventoy_cpio_busybox64((cpio_newc_header *)g_ventoy_cpio_buf, "a64");
    }
    else if (grub_strcmp(args[3], "busybox=m64") == 0)
    {
        debug("cpio busybox proc %s\n", args[3]);
        ventoy_cpio_busybox64((cpio_newc_header *)g_ventoy_cpio_buf, "m64");
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_trailer_cpio(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int mod;
    int bufsize;
    int namelen;
    int offset;
    char *name;
    grub_uint8_t *bufend;
    cpio_newc_header *head;
    grub_file_t file;
    const grub_uint8_t trailler[124] = {
        0x30, 0x37, 0x30, 0x37, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x42, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x54, 0x52,
        0x41, 0x49, 0x4C, 0x45, 0x52, 0x21, 0x21, 0x21, 0x00, 0x00, 0x00, 0x00 
    };

    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", args[0], args[1]);
    if (!file)
    {
        return 1;
    }

    grub_memset(g_ventoy_runtime_buf, 0, sizeof(ventoy_os_param));
    ventoy_fill_os_param(file, (ventoy_os_param *)g_ventoy_runtime_buf);

    grub_file_close(file);

    grub_memcpy(g_ventoy_initrd_head, trailler, sizeof(trailler));
    bufend = (grub_uint8_t *)g_ventoy_initrd_head + sizeof(trailler);

    bufsize = (int)(bufend - g_ventoy_cpio_buf);
    mod = bufsize % 512;
    if (mod)
    {
        grub_memset(bufend, 0, 512 - mod);
        bufsize += 512 - mod;
    }

    if (argc > 1 && grub_strcmp(args[2], "noinit") == 0)
    {
        head = (cpio_newc_header *)g_ventoy_cpio_buf;
        name = (char *)(head + 1);

        while (grub_strcmp(name, "TRAILER!!!"))
        {
            if (grub_strcmp(name, "init") == 0)
            {
                grub_memcpy(name, "xxxx", 4);
            }
            else if (grub_strcmp(name, "linuxrc") == 0)
            {
                grub_memcpy(name, "vtoyxrc", 7);
            }
            else if (grub_strcmp(name, "sbin") == 0)
            {
                grub_memcpy(name, "vtoy", 4);
            }
            else if (grub_strcmp(name, "sbin/init") == 0)
            {
                grub_memcpy(name, "vtoy/vtoy", 9);
            }

            namelen = ventoy_cpio_newc_get_int(head->c_namesize);
            offset = sizeof(cpio_newc_header) + namelen;
            offset = ventoy_align(offset, 4);
            offset += ventoy_cpio_newc_get_int(head->c_filesize);
            offset = ventoy_align(offset, 4);
            
            head = (cpio_newc_header *)((char *)head + offset);
            name = (char *)(head + 1);
        }
    }

    ventoy_memfile_env_set("ventoy_cpio", g_ventoy_cpio_buf, (ulonglong)bufsize);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_linux_chain_data(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len = 0;
    int ventoy_compatible = 0;
    grub_uint32_t size = 0;
    grub_uint64_t isosize = 0;
    grub_uint32_t boot_catlog = 0;
    grub_uint32_t img_chunk_size = 0;
    grub_uint32_t override_count = 0;
    grub_uint32_t override_size = 0;
    grub_uint32_t virt_chunk_count = 0;
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

    len = (int)grub_strlen(args[0]);
    if (len >= 4 && 0 == grub_strcasecmp(args[0] + len - 4, ".img"))
    {
        debug("boot catlog %u for img file\n", boot_catlog);
    }
    else
    {
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
    }
    
    img_chunk_size = g_img_chunk_list.cur_chunk * sizeof(ventoy_img_chunk);

    override_count = ventoy_linux_get_override_chunk_count();
    virt_chunk_count = ventoy_linux_get_virt_chunk_count();
    
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

    chain = ventoy_alloc_chain(size);
    if (!chain)
    {
        grub_printf("Failed to alloc chain linux memory size %u\n", size);
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
    if (override_count > 0)
    {
        chain->override_chunk_offset = chain->img_chunk_offset + img_chunk_size;
        chain->override_chunk_num = override_count;
        ventoy_linux_fill_override_data(isosize, (char *)chain + chain->override_chunk_offset);        
    }

    /* part 5: virt chunk */
    if (virt_chunk_count > 0)
    {        
        chain->virt_chunk_offset = chain->override_chunk_offset + override_size;
        chain->virt_chunk_num = virt_chunk_count;
        ventoy_linux_fill_virt_data(isosize, chain);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static char *ventoy_systemd_conf_tag(char *buf, const char *tag, int optional)
{
    int taglen = 0;
    char *start = NULL;
    char *nextline = NULL;

    taglen = grub_strlen(tag);
    for (start = buf; start; start = nextline)
    {
        nextline = ventoy_get_line(start);
        VTOY_SKIP_SPACE(start);

        if (grub_strncmp(start, tag, taglen) == 0 && (start[taglen] == ' ' || start[taglen] == '\t'))
        {
            start += taglen;
            VTOY_SKIP_SPACE(start); 
            return start;
        }
    }

    if (optional == 0)
    {
        debug("tag<%s> NOT found\n", tag);        
    }
    return NULL;
}

static int ventoy_systemd_conf_hook(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    int oldpos = 0;
    char *tag = NULL;
    char *bkbuf = NULL;
    char *filebuf = NULL;
    grub_file_t file = NULL;
    systemd_menu_ctx *ctx = (systemd_menu_ctx *)data;

    debug("ventoy_systemd_conf_hook %s\n", filename);

    if (info->dir || NULL == grub_strstr(filename, ".conf"))
    {
        return 0;
    }


    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/loader/entries/%s", ctx->dev, filename);
    if (!file)
    {
        return 0;
    }

    filebuf = grub_zalloc(2 * file->size + 8);
    if (!filebuf)
    {
        goto out;
    }

    bkbuf = filebuf + file->size + 4;
    grub_file_read(file, bkbuf, file->size);

    oldpos = ctx->pos;

    /* title --> menuentry */
    grub_memcpy(filebuf, bkbuf, file->size);
    tag = ventoy_systemd_conf_tag(filebuf, "title", 0);
    vtoy_check_goto_out(tag);
    vtoy_len_ssprintf(ctx->buf, ctx->pos, ctx->len, "menuentry \"%s\" {\n", tag);

    /* linux xxx */
    grub_memcpy(filebuf, bkbuf, file->size);
    tag = ventoy_systemd_conf_tag(filebuf, "linux", 0);
    if (!tag)
    {
        ctx->pos = oldpos;
        goto out;
    }
    vtoy_len_ssprintf(ctx->buf, ctx->pos, ctx->len, "  echo \"Downloading kernel ...\"\n  linux %s ", tag);

    /* kernel options */
    grub_memcpy(filebuf, bkbuf, file->size);
    tag = ventoy_systemd_conf_tag(filebuf, "options", 0);
    vtoy_len_ssprintf(ctx->buf, ctx->pos, ctx->len, "%s \n", tag ? tag : "");

    
    /* initrd xxx xxx xxx */
    vtoy_len_ssprintf(ctx->buf, ctx->pos, ctx->len, "  echo \"Downloading initrd ...\"\n  initrd ");
    grub_memcpy(filebuf, bkbuf, file->size);
    tag = ventoy_systemd_conf_tag(filebuf, "initrd", 1);
    while (tag)
    {
        vtoy_len_ssprintf(ctx->buf, ctx->pos, ctx->len, "%s ", tag);
        tag = ventoy_systemd_conf_tag(tag + grub_strlen(tag) + 1, "initrd", 1);
    }

    vtoy_len_ssprintf(ctx->buf, ctx->pos, ctx->len, "\n  boot\n}\n");

out:
    grub_check_free(filebuf);
    grub_file_close(file);
    return 0;
}

grub_err_t ventoy_cmd_linux_systemd_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    static char *buf = NULL;
    grub_fs_t fs;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    systemd_menu_ctx ctx;
    
    (void)ctxt;
    (void)argc;

    if (!buf)
    {
        buf = grub_malloc(VTOY_LINUX_SYSTEMD_MENU_MAX_BUF);
        if (!buf)
        {
            goto end;
        }        
    }

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        debug("failed to get device name %s\n", args[0]);
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

    ctx.dev = args[0];
    ctx.buf = buf;
    ctx.pos = 0;
    ctx.len = VTOY_LINUX_SYSTEMD_MENU_MAX_BUF;
    fs->fs_dir(dev, "/loader/entries", ventoy_systemd_conf_hook, &ctx);

    ventoy_memfile_env_set(args[1], buf, (ulonglong)(ctx.pos));

end:
    grub_check_free(device_name);
    check_free(dev, grub_device_close);
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static int ventoy_limine_path_convert(char *path)
{
    char newpath[256] = {0};

    if (grub_strncmp(path, "boot://2/", 9) == 0)
    {
        grub_snprintf(newpath, sizeof(newpath), "(vtimghd,2)/%s", path + 9);
    }
    else if (grub_strncmp(path, "boot://1/", 9) == 0)
    {
        grub_snprintf(newpath, sizeof(newpath), "(vtimghd,1)/%s", path + 9);
    }

    if (newpath[0])
    {
        grub_snprintf(path, 1024, "%s", newpath);
    }

    return 0;
}

grub_err_t ventoy_cmd_linux_limine_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int pos = 0;
    int sub = 0;
    int len = VTOY_LINUX_SYSTEMD_MENU_MAX_BUF;
    char *filebuf = NULL;
    char *start = NULL;
    char *nextline = NULL;
    grub_file_t file = NULL;
    char *title = NULL;
    char *kernel = NULL;
    char *initrd = NULL;
    char *param = NULL;
    static char *buf = NULL;
    
    (void)ctxt;
    (void)argc;

    if (!buf)
    {
        buf = grub_malloc(len + 4 * 1024);
        if (!buf)
        {
            goto end;
        }        
    }

    title = buf + len;
    kernel = title + 1024;
    initrd = kernel + 1024;
    param = initrd + 1024;
    
    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, args[0]);
    if (!file)
    {
        return 0;
    }

    filebuf = grub_zalloc(file->size + 8);
    if (!filebuf)
    {
        goto end;
    }

    grub_file_read(file, filebuf, file->size);
    grub_file_close(file);

    
    title[0] = kernel[0] = initrd[0] = param[0] = 0;
    for (start = filebuf; start; start = nextline)
    {
        nextline = ventoy_get_line(start);
        VTOY_SKIP_SPACE(start);

        if (start[0] == ':')
        {
            if (start[1] == ':')
            {
                grub_snprintf(title, 1024, "%s", start + 2);
            }
            else
            {
                if (sub)
                {
                    vtoy_len_ssprintf(buf, pos, len, "}\n");
                    sub = 0;
                }

                if (nextline && nextline[0] == ':' && nextline[1] == ':')
                {
                    vtoy_len_ssprintf(buf, pos, len, "submenu \"[+] %s\" {\n", start + 2);
                    sub = 1;
                    title[0] = 0;
                }
                else
                {
                    grub_snprintf(title, 1024, "%s", start + 1);                    
                }
            }
        }
        else if (grub_strncmp(start, "KERNEL_PATH=", 12) == 0)
        {
            grub_snprintf(kernel, 1024, "%s", start + 12);
        }
        else if (grub_strncmp(start, "MODULE_PATH=", 12) == 0)
        {
            grub_snprintf(initrd, 1024, "%s", start + 12);
        }
        else if (grub_strncmp(start, "KERNEL_CMDLINE=", 15) == 0)
        {
            grub_snprintf(param, 1024, "%s", start + 15);
        }

        if (title[0] && kernel[0] && initrd[0] && param[0])
        {
            ventoy_limine_path_convert(kernel);
            ventoy_limine_path_convert(initrd);
        
            vtoy_len_ssprintf(buf, pos, len, "menuentry \"%s\" {\n", title);
            vtoy_len_ssprintf(buf, pos, len, "  echo \"Downloading kernel ...\"\n  linux %s %s\n", kernel, param);
            vtoy_len_ssprintf(buf, pos, len, "  echo \"Downloading initrd ...\"\n  initrd %s\n", initrd);
            vtoy_len_ssprintf(buf, pos, len, "}\n");
        
            title[0] = kernel[0] = initrd[0] = param[0] = 0;
        }
    }

    if (sub)
    {
        vtoy_len_ssprintf(buf, pos, len, "}\n");
        sub = 0;
    }

    ventoy_memfile_env_set(args[1], buf, (ulonglong)pos);

end:
    grub_check_free(filebuf);
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

