/******************************************************************************
 * ventoy.c 
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
#ifdef GRUB_MACHINE_EFI
#include <grub/efi/efi.h>
#endif
#include <grub/time.h>
#include <grub/ventoy.h>
#include "ventoy_def.h"

GRUB_MOD_LICENSE ("GPLv3+");

int g_ventoy_debug = 0;
static int g_efi_os = 0xFF;
initrd_info *g_initrd_img_list = NULL;
initrd_info *g_initrd_img_tail = NULL;
int g_initrd_img_count = 0;
int g_valid_initrd_count = 0;

static grub_file_t g_old_file;

char g_img_swap_tmp_buf[1024];

img_info *g_ventoy_img_list = NULL;
int g_ventoy_img_count = 0;

img_iterator_node g_img_iterator_head;

grub_uint8_t g_ventoy_break_level = 0;
grub_uint8_t g_ventoy_debug_level = 0;
grub_uint8_t *g_ventoy_cpio_buf = NULL;
grub_uint32_t g_ventoy_cpio_size = 0;
cpio_newc_header *g_ventoy_initrd_head = NULL;
grub_uint8_t *g_ventoy_runtime_buf = NULL;

ventoy_grub_param *g_grub_param = NULL;

ventoy_guid  g_ventoy_guid = VENTOY_GUID;

ventoy_img_chunk_list g_img_chunk_list;

void ventoy_debug(const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    grub_vprintf (fmt, args);
    va_end (args);
}

int ventoy_is_efi_os(void)
{
    if (g_efi_os > 1)
    {
        g_efi_os = (grub_strstr(GRUB_PLATFORM, "efi")) ? 1 : 0;
    }

    return g_efi_os;
}

static int ventoy_string_check(const char *str, grub_char_check_func check)
{
    if (!str)
    {
        return 0;
    }
    
    for ( ; *str; str++)
    {
        if (!check(*str))
        {
            return 0;
        }
    }

    return 1;
}


static grub_ssize_t ventoy_fs_read(grub_file_t file, char *buf, grub_size_t len)
{
    grub_memcpy(buf, (char *)file->data + file->offset, len);
    return len;
}

static grub_err_t ventoy_fs_close(grub_file_t file)
{
    grub_file_close(g_old_file);
    grub_free(file->data);

    file->device = 0;
    file->name = 0;

    return 0;
}

static grub_file_t ventoy_wrapper_open(grub_file_t rawFile, enum grub_file_type type)
{
    int len;
    grub_file_t file;
    static struct grub_fs vtoy_fs =
    {
        .name = "vtoy",
        .fs_dir = 0,
        .fs_open = 0,
        .fs_read = ventoy_fs_read,
        .fs_close = ventoy_fs_close,
        .fs_label = 0,
        .next = 0
    };

    if (type != 52)
    {
        return rawFile;
    }

    file = (grub_file_t)grub_zalloc(sizeof (*file));
    if (!file)
    {
        return 0;
    }

    file->data = grub_malloc(rawFile->size + 4096);
    if (!file->data)
    {
        return 0;
    }

    grub_file_read(rawFile, file->data, rawFile->size);
    len = ventoy_fill_data(4096, (char *)file->data + rawFile->size);

    g_old_file = rawFile;
    
    file->size = rawFile->size + len;
    file->device = rawFile->device;
    file->fs = &vtoy_fs;
    file->not_easily_seekable = 1;

    return file;
}

static int ventoy_check_decimal_var(const char *name, long *value)
{
    const char *value_str = NULL;
    
    value_str = grub_env_get(name);
    if (NULL == value_str)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Variable %s not found", name);
    }

    if (!ventoy_is_decimal(value_str))
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Variable %s value '%s' is not an integer", name, value_str);
    }

    *value = grub_strtol(value_str, NULL, 10);

    return GRUB_ERR_NONE;
}

static grub_err_t ventoy_cmd_debug(grub_extcmd_context_t ctxt, int argc, char **args)
{
    if (argc != 1)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {on|off}", cmd_raw_name);
    }

    if (0 == grub_strcmp(args[0], "on"))
    {
        g_ventoy_debug = 1;
        grub_env_set("vtdebug_flag", "debug");
    }
    else
    {
        g_ventoy_debug = 0;
        grub_env_set("vtdebug_flag", "");
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_break(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;

    if (argc < 1 || (args[0][0] != '0' && args[0][0] != '1'))
    {
        grub_printf("Usage: %s {level} [debug]\r\n", cmd_raw_name);
        grub_printf(" level:\r\n");
        grub_printf("    01/11: busybox / (+cat log)\r\n");
        grub_printf("    02/12: initrd / (+cat log)\r\n");
        grub_printf("    03/13: hook / (+cat log)\r\n");
        grub_printf("\r\n");
        grub_printf(" debug:\r\n");
        grub_printf("    0: debug is on\r\n");
        grub_printf("    1: debug is off\r\n");
        grub_printf("\r\n");
        VENTOY_CMD_RETURN(GRUB_ERR_NONE);
    }

    g_ventoy_break_level = (grub_uint8_t)grub_strtoul(args[0], NULL, 16);

    if (argc > 1 && grub_strtoul(args[1], NULL, 10) > 0)
    {
        g_ventoy_debug_level = 1;
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_incr(grub_extcmd_context_t ctxt, int argc, char **args)
{
    long value_long = 0;
    char buf[32];
    
    if ((argc != 2) || (!ventoy_is_decimal(args[1])))
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {Variable} {Int}", cmd_raw_name);
    }

    if (GRUB_ERR_NONE != ventoy_check_decimal_var(args[0], &value_long))
    {
        return grub_errno;
    }

    value_long += grub_strtol(args[1], NULL, 10);

    grub_snprintf(buf, sizeof(buf), "%ld", value_long);
    grub_env_set(args[0], buf);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_file_size(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    char buf[32];
    grub_file_t file;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 2)
    {
        return rc;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (file == NULL)
    {
        debug("failed to open file <%s> for udf check\n", args[0]);
        return 1;
    }

    grub_snprintf(buf, sizeof(buf), "%llu", (unsigned long long)file->size);

    grub_env_set(args[1], buf);

    grub_file_close(file); 
    rc = 0;
    
    return rc;
}

static grub_err_t ventoy_cmd_load_iso_to_mem(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    char name[32];
    char value[32];
    char *buf = NULL;
    grub_file_t file;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 2)
    {
        return rc;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (file == NULL)
    {
        debug("failed to open file <%s> for udf check\n", args[0]);
        return 1;
    }

#ifdef GRUB_MACHINE_EFI
    buf = (char *)grub_efi_allocate_iso_buf(file->size);
#else
    buf = (char *)grub_malloc(file->size);
#endif   

    grub_file_read(file, buf, file->size);

    grub_snprintf(name, sizeof(name), "%s_addr", args[1]);
    grub_snprintf(value, sizeof(value), "0x%llx", (unsigned long long)(unsigned long)buf);
    grub_env_set(name, value);
    
    grub_snprintf(name, sizeof(name), "%s_size", args[1]);
    grub_snprintf(value, sizeof(value), "%llu", (unsigned long long)file->size);
    grub_env_set(name, value);

    grub_file_close(file); 
    rc = 0;
    
    return rc;
}

static grub_err_t ventoy_cmd_is_udf(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int rc = 1;
    grub_file_t file;
    grub_uint8_t buf[32];
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 1)
    {
        return rc;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (file == NULL)
    {
        debug("failed to open file <%s> for udf check\n", args[0]);
        return 1;
    }

    for (i = 16; i < 32; i++)
    {
        grub_file_seek(file, i * 2048);
        grub_file_read(file, buf, sizeof(buf));
        if (buf[0] == 255)
        {
            break;
        }
    }

    i++;
    grub_file_seek(file, i * 2048);
    grub_file_read(file, buf, sizeof(buf));

    if (grub_memcmp(buf + 1, "BEA01", 5) == 0)
    {
        i++;
        grub_file_seek(file, i * 2048);
        grub_file_read(file, buf, sizeof(buf));

        if (grub_memcmp(buf + 1, "NSR02", 5) == 0 ||
            grub_memcmp(buf + 1, "NSR03", 5) == 0)
        {
            rc = 0;
        }
    }

    grub_file_close(file); 

    debug("ISO UDF: %s\n", rc ? "NO" : "YES");
    
    return rc;
}

static grub_err_t ventoy_cmd_cmp(grub_extcmd_context_t ctxt, int argc, char **args)
{
    long value_long1 = 0;
    long value_long2 = 0;
    
    if ((argc != 3) || (!ventoy_is_decimal(args[0])) || (!ventoy_is_decimal(args[2])))
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {Int1} { eq|ne|gt|lt|ge|le } {Int2}", cmd_raw_name);
    }

    value_long1 = grub_strtol(args[0], NULL, 10);
    value_long2 = grub_strtol(args[2], NULL, 10);

    if (0 == grub_strcmp(args[1], "eq"))
    {
        grub_errno = (value_long1 == value_long2) ? GRUB_ERR_NONE : GRUB_ERR_TEST_FAILURE;
    }
    else if (0 == grub_strcmp(args[1], "ne"))
    {
        grub_errno = (value_long1 != value_long2) ? GRUB_ERR_NONE : GRUB_ERR_TEST_FAILURE;
    }
    else if (0 == grub_strcmp(args[1], "gt"))
    {
        grub_errno = (value_long1 > value_long2) ? GRUB_ERR_NONE : GRUB_ERR_TEST_FAILURE;
    }
    else if (0 == grub_strcmp(args[1], "lt"))
    {
        grub_errno = (value_long1 < value_long2) ? GRUB_ERR_NONE : GRUB_ERR_TEST_FAILURE;
    }
    else if (0 == grub_strcmp(args[1], "ge"))
    {
        grub_errno = (value_long1 >= value_long2) ? GRUB_ERR_NONE : GRUB_ERR_TEST_FAILURE;
    }
    else if (0 == grub_strcmp(args[1], "le"))
    {
        grub_errno = (value_long1 <= value_long2) ? GRUB_ERR_NONE : GRUB_ERR_TEST_FAILURE;
    }
    else
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {Int1} { eq ne gt lt ge le } {Int2}", cmd_raw_name);
    }
    
    return grub_errno;
}

static grub_err_t ventoy_cmd_device(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char *pos = NULL;
    char buf[128] = {0};
    
    if (argc != 2)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s path var", cmd_raw_name);
    }

    grub_strncpy(buf, (args[0][0] == '(') ? args[0] + 1 : args[0], sizeof(buf) - 1);
    pos = grub_strstr(buf, ",");
    if (pos)
    {
        *pos = 0;
    }

    grub_env_set(args[1], buf);
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_check_compatible(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    char buf[256];
    grub_disk_t disk;
    char *pos = NULL;
    const char *files[] = { "ventoy.dat", "VENTOY.DAT" };

    (void)ctxt;
    
    if (argc != 1)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s  (loop)", cmd_raw_name);
    }

    for (i = 0; i < (int)ARRAY_SIZE(files); i++)
    {
        grub_snprintf(buf, sizeof(buf) - 1, "[ -e %s/%s ]", args[0], files[i]);
        if (0 == grub_script_execute_sourcecode(buf))
        {
            debug("file %s exist, ventoy_compatible YES\n", buf);
            grub_env_set("ventoy_compatible", "YES");
            VENTOY_CMD_RETURN(GRUB_ERR_NONE);
        }
        else
        {
            debug("file %s NOT exist\n", buf);
        }
    }
    
    grub_snprintf(buf, sizeof(buf) - 1, "%s", args[0][0] == '(' ? (args[0] + 1) : args[0]);
    pos = grub_strstr(buf, ")");
    if (pos)
    {
        *pos = 0;
    }

    disk = grub_disk_open(buf);
    if (disk)
    {
        grub_disk_read(disk, 16 << 2, 0, 1024, g_img_swap_tmp_buf);
        grub_disk_close(disk);
        
        g_img_swap_tmp_buf[703] = 0;
        for (i = 319; i < 703; i++)
        {
            if (g_img_swap_tmp_buf[i] == 'V' &&
                0 == grub_strncmp(g_img_swap_tmp_buf + i, VENTOY_COMPATIBLE_STR, VENTOY_COMPATIBLE_STR_LEN))
            {
                debug("Ventoy compatible string exist at  %d, ventoy_compatible YES\n", i);
                grub_env_set("ventoy_compatible", "YES");
                VENTOY_CMD_RETURN(GRUB_ERR_NONE);
            }
        }
    }
    else
    {
        debug("failed to open disk <%s>\n", buf);
    }

    grub_env_set("ventoy_compatible", "NO");
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static int ventoy_cmp_img(img_info *img1, img_info *img2)
{
    char *s1, *s2;
    int c1 = 0;
    int c2 = 0;

    for (s1 = img1->name, s2 = img2->name; *s1 && *s2; s1++, s2++)
    {
        c1 = *s1;
        c2 = *s2;

        if (grub_islower(c1))
        {
            c1 = c1 - 'a' + 'A';
        }
        
        if (grub_islower(c2))
        {
            c2 = c2 - 'a' + 'A';
        }

        if (c1 != c2)
        {
            break;
        }
    }

    return (c1 - c2);
}

static void ventoy_swap_img(img_info *img1, img_info *img2)
{
    grub_memcpy(g_img_swap_tmp_buf, img1->name, sizeof(img1->name));
    grub_memcpy(img1->name, img2->name, sizeof(img1->name));
    grub_memcpy(img2->name, g_img_swap_tmp_buf, sizeof(img1->name));
    
    grub_memcpy(g_img_swap_tmp_buf, img1->path, sizeof(img1->path));
    grub_memcpy(img1->path, img2->path, sizeof(img1->path));
    grub_memcpy(img2->path, g_img_swap_tmp_buf, sizeof(img1->path));
}

static int ventoy_img_name_valid(const char *filename, grub_size_t namelen)
{
    grub_size_t i;

    for (i = 0; i < namelen; i++)
    {
        if (filename[i] == ' ' || filename[i] == '\t')
        {
            return 0;
        }

        if ((grub_uint8_t)(filename[i]) >= 127)
        {
            return 0;
        }
    }

    return 1;
}

static int ventoy_colect_img_files(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    grub_size_t len;
    img_info *img;
    img_info *tail;
    img_iterator_node *new_node;
    img_iterator_node *node = (img_iterator_node *)data;

    len = grub_strlen(filename);
    
    if (info->dir)
    {
        if ((len == 1 && filename[0] == '.') ||
            (len == 2 && filename[0] == '.' && filename[1] == '.'))
        {
            return 0;
        }

        new_node = grub_malloc(sizeof(img_iterator_node));
        if (new_node)
        {
            new_node->tail = node->tail;
            grub_snprintf(new_node->dir, sizeof(new_node->dir), "%s%s/", node->dir, filename);

            new_node->next = g_img_iterator_head.next;
            g_img_iterator_head.next = new_node;
        }
    }
    else
    {
        debug("Find a file %s\n", filename);

        if ((len > 4) && (0 == grub_strcasecmp(filename + len - 4, ".iso")))
        {
            if (!ventoy_img_name_valid(filename, len))
            {
                return 0;
            }
        
            img = grub_zalloc(sizeof(img_info));
            if (img)
            {
                grub_snprintf(img->name, sizeof(img->name), "%s", filename);
                grub_snprintf(img->path, sizeof(img->path), "%s%s", node->dir, filename);
                
                if (g_ventoy_img_list)
                {
                    tail = *(node->tail);
                    img->prev = tail;
                    tail->next = img;
                }
                else
                {
                    g_ventoy_img_list = img;
                }
                
                *((img_info **)(node->tail)) = img;
                g_ventoy_img_count++;

                debug("Add %s%s to list %d\n", node->dir, filename, g_ventoy_img_count);
            }
        }
    }

    return 0;
}

int ventoy_fill_data(grub_uint32_t buflen, char *buffer)
{
    int len = GRUB_UINT_MAX;
    const char *value = NULL;
    char name[32] = {0};
    char plat[32] = {0};
    char guidstr[32] = {0};
    ventoy_guid guid = VENTOY_GUID;
    const char *fmt1 = NULL;
    const char *fmt2 = NULL;
    const char *fmt3 = NULL;    
    grub_uint32_t *puint = (grub_uint32_t *)name;
    grub_uint32_t *puint2 = (grub_uint32_t *)plat;
    const char fmtdata[]={ 0x39, 0x35, 0x25, 0x00, 0x35, 0x00, 0x23, 0x30, 0x30, 0x30, 0x30, 0x66, 0x66, 0x00 };
    const char fmtcode[]={
        0x22, 0x0A, 0x2B, 0x20, 0x68, 0x62, 0x6F, 0x78, 0x20, 0x7B, 0x0A, 0x20, 0x20, 0x74, 0x6F, 0x70,
        0x20, 0x3D, 0x20, 0x25, 0x73, 0x0A, 0x20, 0x20, 0x6C, 0x65, 0x66, 0x74, 0x20, 0x3D, 0x20, 0x25,
        0x73, 0x0A, 0x20, 0x20, 0x2B, 0x20, 0x6C, 0x61, 0x62, 0x65, 0x6C, 0x20, 0x7B, 0x74, 0x65, 0x78,
        0x74, 0x20, 0x3D, 0x20, 0x22, 0x25, 0x73, 0x20, 0x25, 0x73, 0x25, 0x73, 0x22, 0x20, 0x63, 0x6F,
        0x6C, 0x6F, 0x72, 0x20, 0x3D, 0x20, 0x22, 0x25, 0x73, 0x22, 0x20, 0x61, 0x6C, 0x69, 0x67, 0x6E,
        0x20, 0x3D, 0x20, 0x22, 0x6C, 0x65, 0x66, 0x74, 0x22, 0x7D, 0x0A, 0x7D, 0x0A, 0x22, 0x00
    };

    grub_memset(name, 0, sizeof(name));
    puint[0] = grub_swap_bytes32(0x56454e54);
    puint[3] = grub_swap_bytes32(0x4f4e0000);
    puint[2] = grub_swap_bytes32(0x45525349);
    puint[1] = grub_swap_bytes32(0x4f595f56);
    value = ventoy_get_env(name);

    grub_memset(name, 0, sizeof(name));
    puint[1] = grub_swap_bytes32(0x5f544f50);
    puint[0] = grub_swap_bytes32(0x56544c45);
    fmt1 = ventoy_get_env(name);
    if (!fmt1)
    {
        fmt1 = fmtdata;
    }
    
    grub_memset(name, 0, sizeof(name));
    puint[1] = grub_swap_bytes32(0x5f4c4654);
    puint[0] = grub_swap_bytes32(0x56544c45);
    fmt2 = ventoy_get_env(name);
    
    grub_memset(name, 0, sizeof(name));
    puint[1] = grub_swap_bytes32(0x5f434c52);
    puint[0] = grub_swap_bytes32(0x56544c45);
    fmt3 = ventoy_get_env(name);

    grub_memcpy(guidstr, &guid, sizeof(guid));

    #if defined (GRUB_MACHINE_EFI)
    puint2[0] = grub_swap_bytes32(0x55454649);
    #else
    puint2[0] = grub_swap_bytes32(0x42494f53);
    #endif

    /* Easter egg :) It will be appreciated if you reserve it, but NOT mandatory. */
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wformat-nonliteral"
    len = grub_snprintf(buffer, buflen, fmtcode, 
                        fmt1 ? fmt1 : fmtdata, 
                        fmt2 ? fmt2 : fmtdata + 4, 
                        value ? value : "", plat, guidstr, 
                        fmt3 ? fmt3 : fmtdata + 6);
    #pragma GCC diagnostic pop

    grub_memset(name, 0, sizeof(name));
    puint[0] = grub_swap_bytes32(0x76746f79);
    puint[2] = grub_swap_bytes32(0x656e7365);
    puint[1] = grub_swap_bytes32(0x5f6c6963);
    ventoy_set_env(name, guidstr);

    return len;
}

static grub_err_t ventoy_cmd_list_img(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_fs_t fs;
    grub_device_t dev = NULL;
    img_info *cur = NULL;
    img_info *tail = NULL;
    char *device_name = NULL;
    char buf[32];
    img_iterator_node *node = NULL;
    
    (void)ctxt;

    if (argc != 2)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {device} {cntvar}", cmd_raw_name);
    }

    if (g_ventoy_img_list || g_ventoy_img_count)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Must clear image before list");
    }

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        goto fail;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        goto fail;        
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        goto fail;
    }

    grub_memset(&g_img_iterator_head, 0, sizeof(g_img_iterator_head));

    g_img_iterator_head.tail = &tail;
    grub_strcpy(g_img_iterator_head.dir, "/");    

    fs->fs_dir(dev, "/", ventoy_colect_img_files, &g_img_iterator_head);

    while (g_img_iterator_head.next)
    {
        node = g_img_iterator_head.next;
        g_img_iterator_head.next = node->next;

        fs->fs_dir(dev, node->dir, ventoy_colect_img_files, node);
        grub_free(node);
    }

    /* sort image list by image name */
    for (cur = g_ventoy_img_list; cur; cur = cur->next)
    {
        for (tail = cur->next; tail; tail = tail->next)
        {
            if (ventoy_cmp_img(cur, tail) > 0)
            {
                ventoy_swap_img(cur, tail);
            }
        }
    }

    grub_snprintf(buf, sizeof(buf), "%d", g_ventoy_img_count);
    grub_env_set(args[1], buf);

fail:

    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}


static grub_err_t ventoy_cmd_clear_img(grub_extcmd_context_t ctxt, int argc, char **args)
{
    img_info *next = NULL;
    img_info *cur = g_ventoy_img_list;

    (void)ctxt;
    (void)argc;
    (void)args;

    while (cur)
    {
        next = cur->next;
        grub_free(cur);
        cur = next;
    }
    
    g_ventoy_img_list = NULL;
    g_ventoy_img_count = 0;
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_img_name(grub_extcmd_context_t ctxt, int argc, char **args)
{
    long img_id = 0;
    img_info *cur = g_ventoy_img_list;

    (void)ctxt;
    
    if (argc != 2 || (!ventoy_is_decimal(args[0])))
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {imageID} {var}", cmd_raw_name);
    }

    img_id = grub_strtol(args[0], NULL, 10);
    if (img_id >= g_ventoy_img_count)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "No such many images %ld %ld", img_id, g_ventoy_img_count);
    }

    debug("Find image %ld name \n", img_id);

    while (cur && img_id > 0)
    {
        img_id--;
        cur = cur->next;
    }

    if (!cur)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "No such many images");
    }

    debug("image name is %s\n", cur->name);

    grub_env_set(args[1], cur->name);
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_chosen_img_path(grub_extcmd_context_t ctxt, int argc, char **args)
{
    const char *name = NULL;
    img_info *cur = g_ventoy_img_list;

    (void)ctxt;
    
    if (argc != 1)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {var}", cmd_raw_name);
    }

    name = grub_env_get("chosen");

    while (cur)
    {
        if (0 == grub_strcmp(name, cur->name))
        {
            grub_env_set(args[0], cur->path);
            break;
        }
        cur = cur->next;
    }

    if (!cur)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "No such image");
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static int ventoy_get_disk_guid(const char *filename, grub_uint8_t *guid)
{
    grub_disk_t disk;
    char *device_name;
    char *pos;
    char *pos2;
    
    device_name = grub_file_get_device_name(filename);
    if (!device_name)
    {
        return 1;
    }

    pos = device_name;
    if (pos[0] == '(')
    {
        pos++;
    }

    pos2 = grub_strstr(pos, ",");
    if (!pos2)
    {
        pos2 = grub_strstr(pos, ")");
    }
    
    if (pos2)
    {
        *pos2 = 0;
    }

    disk = grub_disk_open(pos);
    if (disk)
    {
        grub_disk_read(disk, 0, 0x180, 16, guid);
        grub_disk_close(disk);
    }
    else
    {
        return 1;
    }

    grub_free(device_name);
    return 0;
}

grub_uint32_t ventoy_get_iso_boot_catlog(grub_file_t file)
{
    eltorito_descriptor desc;

    grub_memset(&desc, 0, sizeof(desc));
    grub_file_seek(file, 17 * 2048);
    grub_file_read(file, &desc, sizeof(desc));

    if (desc.type != 0 || desc.version != 1)
    {
        return 0;
    }

    if (grub_strncmp((char *)desc.id, "CD001", 5) != 0 ||
        grub_strncmp((char *)desc.system_id, "EL TORITO SPECIFICATION", 23) != 0)
    {
        return 0;
    }

    return desc.sector;    
}

int ventoy_has_efi_eltorito(grub_file_t file, grub_uint32_t sector)
{
    int i;
    grub_uint8_t buf[512];

    grub_file_seek(file, sector * 2048);
    grub_file_read(file, buf, sizeof(buf));

    if (buf[0] == 0x01 && buf[1] == 0xEF)
    {
        debug("%s efi eltorito in Validation Entry\n", file->name);
        return 1;
    }

    for (i = 64; i < (int)sizeof(buf); i += 32)
    {
        if ((buf[i] == 0x90 || buf[i] == 0x91) && buf[i + 1] == 0xEF)
        {
            debug("%s efi eltorito offset %d 0x%02x\n", file->name, i, buf[i]);
            return 1;
        }
    }

    debug("%s does not contain efi eltorito\n", file->name);
    return 0;
}

void ventoy_fill_os_param(grub_file_t file, ventoy_os_param *param)
{
    char *pos;
    grub_uint32_t i;
    grub_uint8_t  chksum = 0;
    grub_disk_t   disk;

    disk = file->device->disk;
    grub_memcpy(&param->guid, &g_ventoy_guid, sizeof(ventoy_guid));

    param->vtoy_disk_size = disk->total_sectors * (1 << disk->log_sector_size);
    param->vtoy_disk_part_id = disk->partition->number + 1;

    if (grub_strcmp(file->fs->name, "exfat") == 0)
    {
        param->vtoy_disk_part_type = 0;
    }
    else if (grub_strcmp(file->fs->name, "ntfs") == 0)
    {
        param->vtoy_disk_part_type = 1;
    }
    else
    {
        param->vtoy_disk_part_type = 0xFFFF;
    }

    pos = grub_strstr(file->name, "/");
    if (!pos)
    {
        pos = file->name;
    }

    grub_snprintf(param->vtoy_img_path, sizeof(param->vtoy_img_path), "%s", pos);
    
    ventoy_get_disk_guid(file->name, param->vtoy_disk_guid);

    param->vtoy_img_size = file->size;

    param->vtoy_reserved[0] = g_ventoy_break_level;
    param->vtoy_reserved[1] = g_ventoy_debug_level;

    /* calculate checksum */
    for (i = 0; i < sizeof(ventoy_os_param); i++)
    {
        chksum += *((grub_uint8_t *)param + i);
    }
    param->chksum = (grub_uint8_t)(0x100 - chksum);

    return;
}

static grub_err_t ventoy_cmd_img_sector(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_file_t file;
    
    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't open file %s\n", args[0]); 
    }

    if (g_img_chunk_list.chunk)
    {
        grub_free(g_img_chunk_list.chunk);
    }

    /* get image chunk data */
    grub_memset(&g_img_chunk_list, 0, sizeof(g_img_chunk_list));
    g_img_chunk_list.chunk = grub_malloc(sizeof(ventoy_img_chunk) * DEFAULT_CHUNK_NUM);
    if (NULL == g_img_chunk_list.chunk)
    {
        return grub_error(GRUB_ERR_OUT_OF_MEMORY, "Can't allocate image chunk memoty\n");
    }
    
    g_img_chunk_list.max_chunk = DEFAULT_CHUNK_NUM;
    g_img_chunk_list.cur_chunk = 0;

    debug("get fat file chunk part start:%llu\n", (unsigned long long)file->device->disk->partition->start);
    grub_fat_get_file_chunk(file->device->disk->partition->start, file, &g_img_chunk_list);

    grub_file_close(file);

    grub_memset(&g_grub_param->file_replace, 0, sizeof(g_grub_param->file_replace));

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_dump_img_sector(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t i;
    ventoy_img_chunk *cur;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    for (i = 0; i < g_img_chunk_list.cur_chunk; i++)
    {
        cur = g_img_chunk_list.chunk + i;
        grub_printf("image:[%u - %u]   <==>  disk:[%llu - %llu]\n", 
            cur->img_start_sector, cur->img_end_sector,
            (unsigned long long)cur->disk_start_sector, (unsigned long long)cur->disk_end_sector
            );
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_add_replace_file(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    ventoy_grub_param_file_replace *replace = NULL;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc >= 2)
    {
        replace = &(g_grub_param->file_replace);
        replace->magic = GRUB_FILE_REPLACE_MAGIC;
            
        replace->old_name_cnt = 0;
        for (i = 0; i < 4 && i + 1 < argc; i++)
        {
            replace->old_name_cnt++;
            grub_snprintf(replace->old_file_name[i], sizeof(replace->old_file_name[i]), "%s", args[i + 1]);
        }
        
        replace->new_file_virtual_id = (grub_uint32_t)grub_strtoul(args[0], NULL, 10);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_file_t ventoy_grub_file_open(enum grub_file_type type, const char *fmt, ...)
{
    va_list ap;
    grub_file_t file;
    char fullpath[256] = {0};

    va_start (ap, fmt);
    grub_vsnprintf(fullpath, 255, fmt, ap);
    va_end (ap);

    file = grub_file_open(fullpath, type);
    if (!file)
    {
        debug("grub_file_open failed <%s>\n", fullpath);
        grub_errno = 0;
    }

    return file;
}

int ventoy_is_file_exist(const char *fmt, ...)
{
    va_list ap;
    int len;
    char *pos = NULL;
    char buf[256] = {0};

    grub_snprintf(buf, sizeof(buf), "%s", "[ -f ");
    pos = buf + 5;

    va_start (ap, fmt);
    len = grub_vsnprintf(pos, 255, fmt, ap);
    va_end (ap);

    grub_strncpy(pos + len, " ]", 2);

    debug("script exec %s\n", buf);

    if (0 == grub_script_execute_sourcecode(buf))
    {
        return 1;
    }

    return 0;
}

static int ventoy_env_init(void)
{
    char buf[64];

    grub_env_set("vtdebug_flag", "");

    ventoy_filt_register(0, ventoy_wrapper_open);

    g_grub_param = (ventoy_grub_param *)grub_zalloc(sizeof(ventoy_grub_param));
    if (g_grub_param)
    {
        g_grub_param->grub_env_get = grub_env_get;
        grub_snprintf(buf, sizeof(buf), "%p", g_grub_param);
        grub_env_set("env_param", buf);
    }

    return 0;
}

static cmd_para ventoy_cmds[] = 
{
    { "vt_incr",  ventoy_cmd_incr,  0, NULL, "{Var} {INT}",   "Increase integer variable",    NULL },
    { "vt_debug", ventoy_cmd_debug, 0, NULL, "{on|off}",   "turn debug on/off",    NULL },
    { "vtdebug", ventoy_cmd_debug, 0, NULL, "{on|off}",   "turn debug on/off",    NULL },
    { "vtbreak", ventoy_cmd_break, 0, NULL, "{level}",   "set debug break",    NULL },
    { "vt_cmp",   ventoy_cmd_cmp, 0, NULL, "{Int1} { eq|ne|gt|lt|ge|le } {Int2}", "Comare two integers", NULL },
    { "vt_device", ventoy_cmd_device, 0, NULL, "path var", "", NULL },
    { "vt_check_compatible",   ventoy_cmd_check_compatible, 0, NULL, "", "", NULL },
    { "vt_list_img", ventoy_cmd_list_img, 0, NULL, "{device} {cntvar}", "find all iso file in device", NULL },
    { "vt_clear_img", ventoy_cmd_clear_img, 0, NULL, "", "clear image list", NULL },
    { "vt_img_name", ventoy_cmd_img_name, 0, NULL, "{imageID} {var}", "get image name", NULL },
    { "vt_chosen_img_path", ventoy_cmd_chosen_img_path, 0, NULL, "{var}", "get chosen img path", NULL },
    { "vt_img_sector", ventoy_cmd_img_sector, 0, NULL, "{imageName}", "", NULL },
    { "vt_dump_img_sector", ventoy_cmd_dump_img_sector, 0, NULL, "", "", NULL },
    { "vt_load_cpio", ventoy_cmd_load_cpio, 0, NULL, "", "", NULL },

    { "vt_is_udf", ventoy_cmd_is_udf, 0, NULL, "", "", NULL },
    { "vt_file_size", ventoy_cmd_file_size, 0, NULL, "", "", NULL },
    { "vt_load_iso_to_mem", ventoy_cmd_load_iso_to_mem, 0, NULL, "", "", NULL },
    
    { "vt_linux_parse_initrd_isolinux", ventoy_cmd_isolinux_initrd_collect, 0, NULL, "{cfgfile}", "", NULL },
    { "vt_linux_parse_initrd_grub", ventoy_cmd_grub_initrd_collect, 0, NULL, "{cfgfile}", "", NULL },
    { "vt_linux_specify_initrd_file", ventoy_cmd_specify_initrd_file, 0, NULL, "", "", NULL },
    { "vt_linux_clear_initrd", ventoy_cmd_clear_initrd_list, 0, NULL, "", "", NULL },
    { "vt_linux_dump_initrd", ventoy_cmd_dump_initrd_list, 0, NULL, "", "", NULL },
    { "vt_linux_initrd_count", ventoy_cmd_initrd_count, 0, NULL, "", "", NULL },
    { "vt_linux_valid_initrd_count", ventoy_cmd_valid_initrd_count, 0, NULL, "", "", NULL },
    { "vt_linux_locate_initrd", ventoy_cmd_linux_locate_initrd, 0, NULL, "", "", NULL },
    { "vt_linux_chain_data", ventoy_cmd_linux_chain_data, 0, NULL, "", "", NULL },

    { "vt_windows_reset",      ventoy_cmd_wimdows_reset, 0, NULL, "", "", NULL },
    { "vt_windows_locate_wim", ventoy_cmd_wimdows_locate_wim, 0, NULL, "", "", NULL },
    { "vt_windows_chain_data", ventoy_cmd_windows_chain_data, 0, NULL, "", "", NULL },

    { "vt_add_replace_file", ventoy_cmd_add_replace_file, 0, NULL, "", "", NULL },

    
    { "vt_load_plugin", ventoy_cmd_load_plugin, 0, NULL, "", "", NULL },
};



GRUB_MOD_INIT(ventoy)
{
    grub_uint32_t i;
    cmd_para *cur = NULL;

    ventoy_env_init();
    
    for (i = 0; i < ARRAY_SIZE(ventoy_cmds); i++)
    {
        cur = ventoy_cmds + i;
        cur->cmd = grub_register_extcmd(cur->name, cur->func, cur->flags, 
                                        cur->summary, cur->description, cur->parser);
    }
}

GRUB_MOD_FINI(ventoy)
{
    grub_uint32_t i;
    
    for (i = 0; i < ARRAY_SIZE(ventoy_cmds); i++)
    {
        grub_unregister_extcmd(ventoy_cmds[i].cmd);
    }
}

