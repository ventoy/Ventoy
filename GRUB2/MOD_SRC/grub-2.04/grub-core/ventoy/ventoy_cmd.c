/******************************************************************************
 * ventoy_cmd.c 
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
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
#include <grub/misc.h>
#include <grub/kernel.h>
#ifdef GRUB_MACHINE_EFI
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#endif
#include <grub/time.h>
#include <grub/video.h>
#include <grub/acpi.h>
#include <grub/charset.h>
#include <grub/crypto.h>
#include <grub/lib/crc.h>
#include <grub/random.h>
#include <grub/ventoy.h>
#include "ventoy_def.h"
#include "miniz.h"

GRUB_MOD_LICENSE ("GPLv3+");

static grub_uint8_t g_check_mbr_data[] = {
    0xEB, 0x63, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    
    0x56, 0x54, 0x00, 0x47, 0x65, 0x00, 0x48, 0x44, 0x00, 0x52, 0x64, 0x00, 0x20, 0x45, 0x72, 0x0D,
};

initrd_info *g_initrd_img_list = NULL;
initrd_info *g_initrd_img_tail = NULL;
int g_initrd_img_count = 0;
int g_valid_initrd_count = 0;
int g_default_menu_mode = 0;
int g_filt_dot_underscore_file = 0;
int g_filt_trash_dir = 1;
int g_sort_case_sensitive = 0;
int g_tree_view_menu_style = 0;
static grub_file_t g_old_file;
static int g_ventoy_last_entry_back;

char g_iso_path[256];
char g_img_swap_tmp_buf[1024];
img_info g_img_swap_tmp;
img_info *g_ventoy_img_list = NULL;

int g_ventoy_img_count = 0;

grub_device_t g_enum_dev = NULL;
grub_fs_t g_enum_fs = NULL;
int g_img_max_search_level = -1;
img_iterator_node g_img_iterator_head;
img_iterator_node *g_img_iterator_tail = NULL;

grub_uint8_t g_ventoy_break_level = 0;
grub_uint8_t g_ventoy_debug_level = 0;
grub_uint8_t g_ventoy_chain_type = 0;

grub_uint8_t *g_ventoy_cpio_buf = NULL;
grub_uint32_t g_ventoy_cpio_size = 0;
cpio_newc_header *g_ventoy_initrd_head = NULL;
grub_uint8_t *g_ventoy_runtime_buf = NULL;

int g_plugin_image_list = 0;

ventoy_grub_param *g_grub_param = NULL;

ventoy_guid  g_ventoy_guid = VENTOY_GUID;

ventoy_img_chunk_list g_img_chunk_list;

int g_wimboot_enable = 0;
ventoy_img_chunk_list g_wimiso_chunk_list;
char *g_wimiso_path = NULL;
grub_uint32_t g_wimiso_size = 0;

int g_vhdboot_enable = 0;

grub_uint64_t g_svd_replace_offset = 0;

int g_conf_replace_count = 0;
grub_uint64_t g_conf_replace_offset[VTOY_MAX_CONF_REPLACE] = { 0 };
conf_replace *g_conf_replace_node[VTOY_MAX_CONF_REPLACE] = { NULL };
grub_uint8_t *g_conf_replace_new_buf[VTOY_MAX_CONF_REPLACE] = { NULL };
int g_conf_replace_new_len[VTOY_MAX_CONF_REPLACE] = { 0 };
int g_conf_replace_new_len_align[VTOY_MAX_CONF_REPLACE] = { 0 };

int g_ventoy_disk_bios_id = 0;
ventoy_gpt_info *g_ventoy_part_info = NULL;
grub_uint64_t g_ventoy_disk_size = 0;
grub_uint64_t g_ventoy_disk_part_size[2];

char *g_tree_script_buf = NULL;
int g_tree_script_pos = 0;
int g_tree_script_pre = 0;

static char *g_list_script_buf = NULL;
static int g_list_script_pos = 0;

static char *g_part_list_buf = NULL;
static int g_part_list_pos = 0;
static grub_uint64_t g_part_end_max = 0;

static int g_video_mode_max = 0;
static int g_video_mode_num = 0;
static ventoy_video_mode *g_video_mode_list = NULL;

static int g_enumerate_time_checked = 0;
static grub_uint64_t g_enumerate_start_time_ms;
static grub_uint64_t g_enumerate_finish_time_ms;
int g_vtoy_file_flt[VTOY_FILE_FLT_BUTT] = {0};

static char g_iso_vd_id_publisher[130];
static char g_iso_vd_id_prepare[130];
static char g_iso_vd_id_application[130];

static int g_pager_flag = 0;
static char g_old_pager[32];

const char *g_menu_class[img_type_max] = 
{
    "vtoyiso", "vtoywim", "vtoyefi", "vtoyimg", "vtoyvhd", "vtoyvtoy"
};
    
const char *g_menu_prefix[img_type_max] = 
{
    "iso", "wim", "efi", "img", "vhd", "vtoy"
};

static const char *g_lower_chksum_name[VTOY_CHKSUM_NUM] = { "md5", "sha1", "sha256", "sha512" };
static int g_lower_chksum_namelen[VTOY_CHKSUM_NUM] = { 3, 4, 6, 6 };
static int g_chksum_retlen[VTOY_CHKSUM_NUM] = { 32, 40, 64, 128 };

static int g_vtoy_secondary_need_recover = 0;

static int g_vtoy_load_prompt = 0;
static char g_vtoy_prompt_msg[64];

static char g_json_case_mis_path[32];

static ventoy_vlnk_part *g_vlnk_part_list = NULL;

int ventoy_get_fs_type(const char *fs)
{
    if (NULL == fs)
    {
        return ventoy_fs_max;
    }
    else if (grub_strncmp(fs, "exfat", 5) == 0)
    {
        return ventoy_fs_exfat;
    }
    else if (grub_strncmp(fs, "ntfs", 4) == 0)
    {
        return ventoy_fs_ntfs;
    }
    else if (grub_strncmp(fs, "ext", 3) == 0)
    {
        return ventoy_fs_ext;
    }
    else if (grub_strncmp(fs, "xfs", 3) == 0)
    {
        return ventoy_fs_xfs;
    }
    else if (grub_strncmp(fs, "udf", 3) == 0)
    {
        return ventoy_fs_udf;
    }
    else if (grub_strncmp(fs, "fat", 3) == 0)
    {
        return ventoy_fs_fat;
    }

    return ventoy_fs_max;
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

static int ventoy_control_get_flag(const char *key)
{
    const char *val = ventoy_get_env(key);
    
    if (val && val[0] == '1' && val[1] == 0)
    {
        return 1;
    }
    return 0;
}

static grub_err_t ventoy_fs_close(grub_file_t file)
{
    grub_file_close(g_old_file);
    grub_free(file->data);

    file->device = 0;
    file->name = 0;

    return 0;
}

static int ventoy_video_hook(const struct grub_video_mode_info *info, void *hook_arg)
{
    int i;

    (void)hook_arg;

    if (info->mode_type & GRUB_VIDEO_MODE_TYPE_PURE_TEXT)
    {
        return 0;
    }
    
    for (i = 0; i < g_video_mode_num; i++)
    {
        if (g_video_mode_list[i].width == info->width && 
            g_video_mode_list[i].height == info->height &&
            g_video_mode_list[i].bpp == info->bpp)
        {
            return 0;
        }
    }

    g_video_mode_list[g_video_mode_num].width = info->width;
    g_video_mode_list[g_video_mode_num].height = info->height;
    g_video_mode_list[g_video_mode_num].bpp = info->bpp;
    g_video_mode_num++;

    if (g_video_mode_num == g_video_mode_max)
    {
        g_video_mode_max *= 2;
        g_video_mode_list = grub_realloc(g_video_mode_list, g_video_mode_max * sizeof(ventoy_video_mode));
    }

    return 0;
}

static int ventoy_video_mode_cmp(ventoy_video_mode *v1, ventoy_video_mode *v2)
{
    if (v1->bpp == v2->bpp)
    {
        if (v1->width == v2->width)
        {
            if (v1->height == v2->height)
            {
                return 0;
            }
            else
            {
                return (v1->height < v2->height) ? -1 : 1;
            }
        }
        else
        {
            return (v1->width < v2->width) ? -1 : 1;
        }
    }
    else
    {
        return (v1->bpp < v2->bpp) ? -1 : 1;
    }
}

static int ventoy_enum_video_mode(void)
{
    int i, j;
    grub_video_adapter_t adapter;
    grub_video_driver_id_t id;
    ventoy_video_mode mode;
    
    g_video_mode_num = 0;
    g_video_mode_max = 1024;
    g_video_mode_list = grub_malloc(sizeof(ventoy_video_mode) * g_video_mode_max);
    if (!g_video_mode_list)
    {
        return 0;
    }

    #ifdef GRUB_MACHINE_PCBIOS
    grub_dl_load ("vbe");
    #endif

    id = grub_video_get_driver_id ();

    FOR_VIDEO_ADAPTERS (adapter)
    {
        if (!adapter->iterate ||
            (adapter->id != id && (id != GRUB_VIDEO_DRIVER_NONE ||
             adapter->init() != GRUB_ERR_NONE)))
        {
            continue;
        }

        adapter->iterate(ventoy_video_hook, NULL);

        if (adapter->id != id)
        {
            adapter->fini();
        }
    }

    /* sort video mode */
    for (i = 0; i < g_video_mode_num; i++)
    for (j = i + 1; j < g_video_mode_num; j++)
    {
        if (ventoy_video_mode_cmp(g_video_mode_list + i, g_video_mode_list + j) < 0)
        {
            grub_memcpy(&mode, g_video_mode_list + i, sizeof(ventoy_video_mode));
            grub_memcpy(g_video_mode_list + i, g_video_mode_list + j, sizeof(ventoy_video_mode));
            grub_memcpy(g_video_mode_list + j, &mode, sizeof(ventoy_video_mode));
        }
    }
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static int ventoy_pre_parse_data(char *src, int size)
{
    char c;
    char *pos = NULL;
    char buf[256];

    if (size < 20 || grub_strncmp(src, "ventoy_left_top_color", 21))
    {
        return 0;
    }

    pos = src + 21;
    while (*pos && *pos != '\r' && *pos != '\n')
    {
        pos++;
    }

    c = *pos;
    *pos = 0;

    if (grub_strlen(src) > 200)
    {
        goto end;
    }

    grub_snprintf(buf, sizeof(buf), 
        "regexp -s 1:%s -s 2:%s -s 3:%s \"@([^@]*)@([^@]*)@([^@]*)@\" \"%s\"", 
        ventoy_left_key, ventoy_top_key, ventoy_color_key, src);

    grub_script_execute_sourcecode(buf);

end:    
    *pos = c;
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
    ventoy_pre_parse_data((char *)file->data, (int)rawFile->size);
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

grub_uint64_t ventoy_get_vtoy_partsize(int part)
{
    grub_uint64_t sectors;
    
    if (grub_strncmp(g_ventoy_part_info->Head.Signature, "EFI PART", 8) == 0)
    {
        sectors = g_ventoy_part_info->PartTbl[part].LastLBA + 1 - g_ventoy_part_info->PartTbl[part].StartLBA;
    }
    else
    {
        sectors = g_ventoy_part_info->MBR.PartTbl[part].SectorCount;
    }

    return sectors * 512;
}

static int ventoy_load_efiboot_template(char **buf, int *datalen, int *direntoff)
{
    int len;
    grub_file_t file;
    char exec[128];
    char *data = NULL;
    grub_uint32_t offset;

    file = ventoy_grub_file_open(GRUB_FILE_TYPE_LINUX_INITRD, "%s/ventoy/ventoy_efiboot.img.xz", ventoy_get_env("vtoy_efi_part"));
    if (file == NULL)
    {
        debug("failed to open file <%s>\n", "ventoy_efiboot.img.xz");
        return 1;
    }

    len = (int)file->size;
    
    data = (char *)grub_malloc(file->size);
    if (!data)
    {
        return 1;
    }
    
    grub_file_read(file, data, file->size);
    grub_file_close(file); 

    grub_snprintf(exec, sizeof(exec), "loopback efiboot mem:0x%llx:size:%d", (ulonglong)(ulong)data, len);
    grub_script_execute_sourcecode(exec);

    file = grub_file_open("(efiboot)/EFI/BOOT/BOOTX64.EFI", GRUB_FILE_TYPE_LINUX_INITRD);    
    offset = (grub_uint32_t)grub_iso9660_get_last_file_dirent_pos(file);
    grub_file_close(file);
    
    grub_script_execute_sourcecode("loopback -d efiboot");

    *buf = data;
    *datalen = len;
    *direntoff = offset + 2;

    return 0;
}

static int ventoy_set_check_result(int ret, const char *msg)
{
    char buf[32];
    
    grub_snprintf(buf, sizeof(buf), "%d", (ret & 0x7FFF));
    grub_env_set("VTOY_CHKDEV_RESULT_STRING", buf);
    grub_env_export("VTOY_CHKDEV_RESULT_STRING");

    if (ret)
    {
        grub_cls();
        grub_printf(VTOY_WARNING"\n");
        grub_printf(VTOY_WARNING"\n");
        grub_printf(VTOY_WARNING"\n\n\n");
        
        grub_printf("This is NOT a standard Ventoy device and is NOT supported (%d).\n", ret);
        grub_printf("Error message: <%s>\n\n", msg);
        grub_printf("You should follow the instructions in https://www.ventoy.net to use Ventoy.\n");
        grub_refresh();
    }

    return ret;
}

static int ventoy_check_official_device(grub_device_t dev)
{
    int workaround = 0;
    grub_file_t file;
    grub_uint64_t offset;
    char devname[64];
    grub_fs_t fs;
    grub_uint8_t mbr[512];
    grub_disk_t disk;
    grub_device_t dev2;
    char *label = NULL;
    struct grub_partition *partition;
    
    if (dev->disk == NULL || dev->disk->partition == NULL)
    {
        return ventoy_set_check_result(1 | 0x1000, "Internal Error");
    }

    if (0 == ventoy_check_file_exist("(%s,2)/ventoy/ventoy.cpio", dev->disk->name) ||
        0 == ventoy_check_file_exist("(%s,2)/grub/localboot.cfg", dev->disk->name) ||
        0 == ventoy_check_file_exist("(%s,2)/tool/mount.exfat-fuse_aarch64", dev->disk->name))
    {
        #ifndef GRUB_MACHINE_EFI
        if (0 == ventoy_check_file_exist("(ventoydisk)/ventoy/ventoy.cpio", dev->disk->name))
        {
            return ventoy_set_check_result(2 | 0x1000, "File ventoy/ventoy.cpio missing in VTOYEFI partition");
        }
        else if (0 == ventoy_check_file_exist("(ventoydisk)/grub/localboot.cfg", dev->disk->name))
        {
            return ventoy_set_check_result(2 | 0x1000, "File grub/localboot.cfg missing in VTOYEFI partition");
        }
        else if (0 == ventoy_check_file_exist("(ventoydisk)/tool/mount.exfat-fuse_aarch64", dev->disk->name))
        {
            return ventoy_set_check_result(2 | 0x1000, "File tool/mount.exfat-fuse_aarch64 missing in VTOYEFI partition");
        }
        else
        {
            workaround = 1;
        }
        #endif
    }

    /* We must have partition 2 */
    if (workaround)
    {
        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", "(ventoydisk)/ventoy/ventoy.cpio");
    }
    else
    {
        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "(%s,2)/ventoy/ventoy.cpio", dev->disk->name);        
    }
    if (!file)
    {
        return ventoy_set_check_result(3 | 0x1000, "File ventoy/ventoy.cpio open failed in VTOYEFI partition");
    }

    if (NULL == grub_strstr(file->fs->name, "fat"))
    {
        grub_file_close(file);
        return ventoy_set_check_result(4 | 0x1000, "VTOYEFI partition is not FAT filesystem");
    }

    partition = dev->disk->partition;
    if (partition->number != 0 || partition->start != 2048)
    {
        return ventoy_set_check_result(5, "Ventoy partition is not start at 1MB");
    }

    if (workaround)
    {
        if (grub_strncmp(g_ventoy_part_info->Head.Signature, "EFI PART", 8) == 0)
        {
            ventoy_gpt_part_tbl *PartTbl = g_ventoy_part_info->PartTbl;
            if (PartTbl[1].StartLBA != PartTbl[0].LastLBA + 1 ||
                (PartTbl[1].LastLBA + 1 - PartTbl[1].StartLBA) != 65536)
            {
                grub_file_close(file);
                return ventoy_set_check_result(6, "Disk partition layout check failed.");
            }
        }
        else
        {
            ventoy_part_table *PartTbl = g_ventoy_part_info->MBR.PartTbl;
            if (PartTbl[1].StartSectorId != PartTbl[0].StartSectorId + PartTbl[0].SectorCount ||
                PartTbl[1].SectorCount != 65536)
            {
                grub_file_close(file);
                return ventoy_set_check_result(6, "Disk partition layout check failed.");
            }
        }
    }
    else
    {
        offset = partition->start + partition->len;
        partition = file->device->disk->partition;
        if ((partition->number != 1) || (partition->len != 65536) || (offset != partition->start))
        {
            grub_file_close(file);
            return ventoy_set_check_result(7, "Disk partition layout check failed.");
        }
    }

    grub_file_close(file);

    if (workaround == 0)
    {
        grub_snprintf(devname, sizeof(devname), "%s,2", dev->disk->name);
        dev2 = grub_device_open(devname);
        if (!dev2)
        {
            return ventoy_set_check_result(8, "Disk open failed");
        }

        fs = grub_fs_probe(dev2);
        if (!fs)
        {
            grub_device_close(dev2);
            return ventoy_set_check_result(9, "FS probe failed");
        }

        fs->fs_label(dev2, &label);
        if ((!label) || grub_strncmp("VTOYEFI", label, 7))
        {
            grub_device_close(dev2);
            return ventoy_set_check_result(10, "Partition name is not VTOYEFI");
        }

        grub_device_close(dev2);    
    }

    /* MBR check */
    disk = grub_disk_open(dev->disk->name);
    if (!disk)
    {
        return ventoy_set_check_result(11, "Disk open failed");
    }

    grub_memset(mbr, 0, 512);
    grub_disk_read(disk, 0, 0, 512, mbr);
    grub_disk_close(disk);
    
    if (grub_memcmp(g_check_mbr_data, mbr, 0x30) || grub_memcmp(g_check_mbr_data + 0x30, mbr + 0x190, 16))
    {
        return ventoy_set_check_result(12, "MBR check failed");
    }

    return ventoy_set_check_result(0, NULL);
}

static int ventoy_check_ignore_flag(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    if (0 == info->dir)
    {
        if (filename && filename[0] == '.' && 0 == grub_strncmp(filename, ".ventoyignore", 13))
        {
            *((int *)data) = 1;
            return 0;
        }
    }

    return 0;
}

grub_uint64_t ventoy_grub_get_file_size(const char *fmt, ...)
{
    grub_uint64_t size = 0;
    grub_file_t file;
    va_list ap;
    char fullpath[256] = {0};

    va_start (ap, fmt);
    grub_vsnprintf(fullpath, 255, fmt, ap);
    va_end (ap);
    
    file = grub_file_open(fullpath, VENTOY_FILE_TYPE);
    if (!file)
    {
        debug("grub_file_open failed <%s>\n", fullpath);
        grub_errno = 0;
        return 0;
    }

    size = file->size;
    grub_file_close(file);
    return size;
}

grub_file_t ventoy_grub_file_open(enum grub_file_type type, const char *fmt, ...)
{
    va_list ap;
    grub_file_t file;
    char fullpath[512] = {0};

    va_start (ap, fmt);
    grub_vsnprintf(fullpath, 511, fmt, ap);
    va_end (ap);

    file = grub_file_open(fullpath, type);
    if (!file)
    {
        debug("grub_file_open failed <%s> %d\n", fullpath, grub_errno);
        grub_errno = 0;
    }

    return file;
}

int ventoy_is_dir_exist(const char *fmt, ...)
{
    va_list ap;
    int len;
    char *pos = NULL;
    char buf[512] = {0};

    grub_snprintf(buf, sizeof(buf), "%s", "[ -d \"");
    pos = buf + 6;

    va_start (ap, fmt);
    len = grub_vsnprintf(pos, 511, fmt, ap);
    va_end (ap);

    grub_strncpy(pos + len, "\" ]", 3);

    debug("script exec %s\n", buf);

    if (0 == grub_script_execute_sourcecode(buf))
    {
        return 1;
    }

    return 0;
}

int ventoy_gzip_compress(void *mem_in, int mem_in_len, void *mem_out, int mem_out_len)
{
	mz_stream s;
    grub_uint8_t *outbuf;
    grub_uint8_t gzHdr[10] = 
    {
		0x1F, 0x8B,	/* magic */
		8,		    /* z method */
		0,		    /* flags */
		0,0,0,0,	/* mtime */
		4,		    /* xfl */
		3,		    /* OS */
	};

	grub_memset(&s, 0, sizeof(mz_stream));

    mz_deflateInit2(&s, 1, MZ_DEFLATED, -MZ_DEFAULT_WINDOW_BITS, 6, MZ_DEFAULT_STRATEGY);
    
    outbuf = (grub_uint8_t *)mem_out;

    mem_out_len -= sizeof(gzHdr) + 8;
    grub_memcpy(outbuf, gzHdr, sizeof(gzHdr));
    outbuf += sizeof(gzHdr);

    s.avail_in = mem_in_len;
    s.next_in = mem_in;

    s.avail_out = mem_out_len;
    s.next_out = outbuf;

    mz_deflate(&s, MZ_FINISH);

    mz_deflateEnd(&s);

    outbuf += s.total_out;
    *(grub_uint32_t *)outbuf = grub_getcrc32c(0, outbuf, s.total_out);
    *(grub_uint32_t *)(outbuf + 4) = (grub_uint32_t)(s.total_out);

    return s.total_out + sizeof(gzHdr) + 8;    
}


#if 0
ventoy grub cmds
#endif

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
        grub_printf("    0: debug is off\r\n");
        grub_printf("    1: debug is on\r\n");
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

static grub_err_t ventoy_cmd_strstr(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;

    if (argc != 2)
    {
        return 1;
    }

    return (grub_strstr(args[0], args[1])) ? 0 : 1;
}

static grub_err_t ventoy_cmd_strbegin(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char *c0, *c1;
    
    (void)ctxt;

    if (argc != 2)
    {
        return 1;
    }

    c0 = args[0];
    c1 = args[1];

    while (*c0 && *c1)
    {
        if (*c0 != *c1)
        {
            return 1;
        }
        c0++;
        c1++;
    }

    if (*c1)
    {
        return 1;
    }

    return 0;
}

static grub_err_t ventoy_cmd_strcasebegin(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char *c0, *c1;
    
    (void)ctxt;

    if (argc != 2)
    {
        return 1;
    }

    c0 = args[0];
    c1 = args[1];

    while (*c0 && *c1)
    {
        if ((*c0 != *c1) && (*c0 != grub_toupper(*c1)))
        {
            return 1;
        }
        c0++;
        c1++;
    }

    if (*c1)
    {
        return 1;
    }

    return 0;
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

static grub_err_t ventoy_cmd_mod(grub_extcmd_context_t ctxt, int argc, char **args)
{
    ulonglong value1 = 0;
    ulonglong value2 = 0;
    char buf[32];
    
    if (argc != 3)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {Int} {Int} {Variable}", cmd_raw_name);
    }

    value1 = grub_strtoull(args[0], NULL, 10);
    value2 = grub_strtoull(args[1], NULL, 10);

    grub_snprintf(buf, sizeof(buf), "%llu", (value1 & (value2 - 1)));
    grub_env_set(args[2], buf);

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

static grub_err_t ventoy_cmd_load_wimboot(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_file_t file;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    g_wimboot_enable = 0;
    g_wimiso_size = 0;
    grub_check_free(g_wimiso_path);
    grub_check_free(g_wimiso_chunk_list.chunk);

    file = grub_file_open(args[0], VENTOY_FILE_TYPE);
    if (!file)
    {
        return 0;
    }

    grub_memset(&g_wimiso_chunk_list, 0, sizeof(g_wimiso_chunk_list));
    g_wimiso_chunk_list.chunk = grub_malloc(sizeof(ventoy_img_chunk) * DEFAULT_CHUNK_NUM);
    if (NULL == g_wimiso_chunk_list.chunk)
    {
        return grub_error(GRUB_ERR_OUT_OF_MEMORY, "Can't allocate image chunk memoty\n");
    }
    
    g_wimiso_chunk_list.max_chunk = DEFAULT_CHUNK_NUM;
    g_wimiso_chunk_list.cur_chunk = 0;

    ventoy_get_block_list(file, &g_wimiso_chunk_list, file->device->disk->partition->start);

    g_wimboot_enable = 1;
    g_wimiso_path = grub_strdup(args[0]);
    g_wimiso_size = (grub_uint32_t)(file->size);
    grub_file_close(file);

    return 0;
}

static grub_err_t ventoy_cmd_concat_efi_iso(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len = 0;
    int totlen = 0;
    int offset = 0;
    grub_file_t file;
    char *buf = NULL;
    char *data = NULL;
    ventoy_iso9660_override *dirent;
    
    (void)ctxt;

    if (argc != 2)
    {
        return 1;
    }

    totlen = sizeof(ventoy_chain_head);

    if (ventoy_load_efiboot_template(&buf, &len, &offset))
    {
        debug("failed to load efiboot template %d\n", len);
        return 1;
    }

    totlen += len;
    
    debug("efiboot template len:%d offset:%d\n", len, offset);

    file = ventoy_grub_file_open(GRUB_FILE_TYPE_LINUX_INITRD, "%s", args[0]);
    if (file == NULL)
    {
        debug("failed to open file <%s>\n", args[0]);
        return 1;
    }

    if (grub_strncmp(args[0], g_iso_path, grub_strlen(g_iso_path)))
    {
        file->vlnk = 1;
    }

    totlen += ventoy_align_2k(file->size);

    dirent = (ventoy_iso9660_override *)(buf + offset);
    dirent->first_sector = len / 2048;
    dirent->first_sector_be = grub_swap_bytes32(dirent->first_sector);
    dirent->size = (grub_uint32_t)file->size;
    dirent->size_be = grub_swap_bytes32(dirent->size);

    debug("rawiso len:%d efilen:%d total:%d\n", len, (int)file->size, totlen);

#ifdef GRUB_MACHINE_EFI
    data = (char *)grub_efi_allocate_iso_buf(totlen);
#else
    data = (char *)grub_malloc(totlen);
#endif  

    ventoy_fill_os_param(file, (ventoy_os_param *)data);

    grub_memcpy(data + sizeof(ventoy_chain_head), buf, len);
    grub_check_free(buf);

    grub_file_read(file, data + sizeof(ventoy_chain_head) + len, file->size);
    grub_file_close(file); 

    ventoy_memfile_env_set(args[1], data, (ulonglong)totlen);

    return 0;
}

grub_err_t ventoy_cmd_set_wim_prompt(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_vtoy_load_prompt = 0;
    grub_memset(g_vtoy_prompt_msg, 0, sizeof(g_vtoy_prompt_msg));
    
    if (argc == 2 && args[0][0] == '1')
    {
        g_vtoy_load_prompt = 1;
        grub_snprintf(g_vtoy_prompt_msg, sizeof(g_vtoy_prompt_msg), "%s", args[1]);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

int ventoy_need_prompt_load_file(void)
{
    return g_vtoy_load_prompt;
}

grub_ssize_t ventoy_load_file_with_prompt(grub_file_t file, void *buf, grub_ssize_t size)
{
    grub_uint64_t ro = 0;
    grub_uint64_t div = 0;
    grub_ssize_t left = size;
    char *cur = (char *)buf;

    grub_printf("\r%s   1%%    ", g_vtoy_prompt_msg); 
    grub_refresh();

    while (left >= VTOY_SIZE_2MB)
    {
        grub_file_read(file, cur, VTOY_SIZE_2MB);
        cur += VTOY_SIZE_2MB;
        left -= VTOY_SIZE_2MB;

        div = grub_divmod64((grub_uint64_t)((size - left) * 100), (grub_uint64_t)size, &ro);
        if (div < 1)
        {
            div = 1;
        }
        grub_printf("\r%s   %d%%    ", g_vtoy_prompt_msg, (int)div);
        grub_refresh();  
    }

    if (left > 0)
    {
        grub_file_read(file, cur, left);
    }

    grub_printf("\r%s   100%%     \n", g_vtoy_prompt_msg);
    grub_refresh(); 

    return size;
}

static grub_err_t ventoy_cmd_load_file_to_mem(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    char *buf = NULL;
    grub_file_t file;
    enum grub_file_type type;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 3)
    {
        return rc;
    }

    if (grub_strcmp(args[0], "nodecompress") == 0)
    {
        type = VENTOY_FILE_TYPE;
    }
    else
    {
        type = GRUB_FILE_TYPE_LINUX_INITRD;
    }

    file = ventoy_grub_file_open(type, "%s", args[1]);
    if (file == NULL)
    {
        debug("failed to open file <%s>\n", args[1]);
        return 1;
    }

#ifdef GRUB_MACHINE_EFI
    buf = (char *)grub_efi_allocate_chain_buf(file->size);
#else
    buf = (char *)grub_malloc(file->size);
#endif   

    if (!buf)
    {
        grub_file_close(file);
        return 1;
    }

    if (g_vtoy_load_prompt)
    {
         ventoy_load_file_with_prompt(file, buf, file->size);
    }
    else
    {
        grub_file_read(file, buf, file->size);
    }

    ventoy_memfile_env_set(args[2], buf, (ulonglong)(file->size));

    grub_file_close(file); 
    rc = 0;
    
    return rc;
}

static grub_err_t ventoy_cmd_load_img_memdisk(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    int headlen;
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

    headlen = sizeof(ventoy_chain_head);

#ifdef GRUB_MACHINE_EFI
    buf = (char *)grub_efi_allocate_iso_buf(headlen + file->size);
#else
    buf = (char *)grub_malloc(headlen + file->size);
#endif   

    ventoy_fill_os_param(file, (ventoy_os_param *)buf);

    grub_file_read(file, buf + headlen, file->size);

    ventoy_memfile_env_set(args[1], buf, (ulonglong)(file->size));

    grub_file_close(file); 
    rc = 0;
    
    return rc;
}

static grub_err_t ventoy_cmd_iso9660_is_joliet(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;
    
    if (grub_iso9660_is_joliet())
    {
        debug("This time has joliet process\n");
        return 0;
    }
    else
    {
        return 1;
    }
}

static grub_err_t ventoy_cmd_iso9660_nojoliet(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;

    if (argc != 1)
    {
        return 1;
    }

    if (args[0][0] == '1')
    {
        grub_iso9660_set_nojoliet(1);
    }
    else
    {
        grub_iso9660_set_nojoliet(0);
    }

    return 0;
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
        grub_snprintf(buf, sizeof(buf) - 1, "[ -e \"%s/%s\" ]", args[0], files[i]);
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
        for (i = 318; i < 703; i++)
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

int ventoy_cmp_img(img_info *img1, img_info *img2)
{
    char *s1, *s2;
    int c1 = 0;
    int c2 = 0;

    if (g_plugin_image_list == VENTOY_IMG_WHITE_LIST)
    {
        return (img1->plugin_list_index - img2->plugin_list_index);
    }

    for (s1 = img1->name, s2 = img2->name; *s1 && *s2; s1++, s2++)
    {
        c1 = *s1;
        c2 = *s2;

        if (0 == g_sort_case_sensitive)
        {
            if (grub_islower(c1))
            {
                c1 = c1 - 'a' + 'A';
            }
            
            if (grub_islower(c2))
            {
                c2 = c2 - 'a' + 'A';
            }
        }

        if (c1 != c2)
        {
            break;
        }
    }

    return (c1 - c2);
}

static int ventoy_cmp_subdir(img_iterator_node *node1, img_iterator_node *node2)
{
    char *s1, *s2;
    int c1 = 0;
    int c2 = 0;

    if (g_plugin_image_list == VENTOY_IMG_WHITE_LIST)
    {
        return (node1->plugin_list_index - node2->plugin_list_index);
    }

    for (s1 = node1->dir, s2 = node2->dir; *s1 && *s2; s1++, s2++)
    {
        c1 = *s1;
        c2 = *s2;

        if (0 == g_sort_case_sensitive)
        {
            if (grub_islower(c1))
            {
                c1 = c1 - 'a' + 'A';
            }
            
            if (grub_islower(c2))
            {
                c2 = c2 - 'a' + 'A';
            }
        }

        if (c1 != c2)
        {
            break;
        }
    }

    return (c1 - c2);
}

void ventoy_swap_img(img_info *img1, img_info *img2)
{
    grub_memcpy(&g_img_swap_tmp, img1, sizeof(img_info));
    
    grub_memcpy(img1, img2, sizeof(img_info));
    img1->next = g_img_swap_tmp.next;
    img1->prev = g_img_swap_tmp.prev;

    g_img_swap_tmp.next = img2->next;
    g_img_swap_tmp.prev = img2->prev;
    grub_memcpy(img2, &g_img_swap_tmp, sizeof(img_info));
}

int ventoy_img_name_valid(const char *filename, grub_size_t namelen)
{
    (void)namelen;
    
    if (g_filt_dot_underscore_file && filename[0] == '.' && filename[1] == '_')
    {
        return 0;
    }

    return 1;
}

static int ventoy_vlnk_iterate_partition(struct grub_disk *disk, const grub_partition_t partition, void *data)
{
    ventoy_vlnk_part *node = NULL;
    grub_uint32_t SelfSig;
    grub_uint32_t *pSig = (grub_uint32_t *)data;

    /* skip Ventoy partition 1/2 */
    grub_memcpy(&SelfSig, g_ventoy_part_info->MBR.BootCode + 0x1b8, 4);
    if (partition->number < 2 && SelfSig == *pSig)
    {
        return 0;
    }

    node = grub_zalloc(sizeof(ventoy_vlnk_part));
    if (node)
    {
        node->disksig = *pSig;
        node->partoffset = (partition->start << GRUB_DISK_SECTOR_BITS);
        grub_snprintf(node->disk, sizeof(node->disk) - 1, "%s", disk->name);
        grub_snprintf(node->device, sizeof(node->device) - 1, "%s,%d", disk->name, partition->number + 1);

        node->next = g_vlnk_part_list;
        g_vlnk_part_list = node;
    }

    return 0;
}

static int ventoy_vlnk_iterate_disk(const char *name, void *data)
{
    grub_disk_t disk;
    grub_uint32_t sig;

    (void)data;

    disk = grub_disk_open(name);
    if (disk)
    {
        grub_disk_read(disk, 0, 0x1b8, 4, &sig);
        grub_partition_iterate(disk, ventoy_vlnk_iterate_partition, &sig);
        grub_disk_close(disk);
    }

    return 0;
}

static int ventoy_vlnk_probe_fs(ventoy_vlnk_part *cur)
{
    const char *fs[ventoy_fs_max + 1] = 
    {
        "exfat", "ntfs", "ext2", "xfs", "udf", "fat", NULL
    };

    if (!cur->dev)
    {
        cur->dev = grub_device_open(cur->device);
    }

    if (cur->dev)
    {
        cur->fs = grub_fs_list_probe(cur->dev, fs);
    }

    return 0;
}

static int ventoy_check_vlnk_data(ventoy_vlnk *vlnk, int print, char *dst, int size)
{
    int diskfind = 0;
    int partfind = 0;
    int filefind = 0;
    char *disk, *device;
    grub_uint32_t readcrc, calccrc;
    ventoy_vlnk_part *cur;
    grub_fs_t fs = NULL;
    
    if (grub_memcmp(&(vlnk->guid), &g_ventoy_guid, sizeof(ventoy_guid)))
    {
        if (print)
        {
            grub_printf("VLNK invalid guid\n");
            grub_refresh();
        }
        return 1;
    }

    readcrc = vlnk->crc32;
    vlnk->crc32 = 0;
    calccrc = grub_getcrc32c(0, vlnk, sizeof(ventoy_vlnk));
    if (readcrc != calccrc)
    {
        if (print)
        {
            grub_printf("VLNK invalid crc 0x%08x 0x%08x\n", calccrc, readcrc);
            grub_refresh();
        }
        return 1;
    }

    if (!g_vlnk_part_list)
    {
        grub_disk_dev_iterate(ventoy_vlnk_iterate_disk, NULL);
    }

    for (cur = g_vlnk_part_list; cur && filefind == 0; cur = cur->next)
    {
        if (cur->disksig == vlnk->disk_signature)
        {
            diskfind = 1;
            disk = cur->disk;
            if (cur->partoffset == vlnk->part_offset)
            {
                partfind = 1;
                device = cur->device;

                if (cur->probe == 0)
                {
                    cur->probe = 1;
                    ventoy_vlnk_probe_fs(cur);
                }
    
                if (!fs)
                {
                    fs = cur->fs;
                }
                
                if (cur->fs)
                {
                    struct grub_file file;

                    grub_memset(&file, 0, sizeof(file));
                    file.device = cur->dev;
                    if (cur->fs->fs_open(&file, vlnk->filepath) == GRUB_ERR_NONE)
                    {
                        filefind = 1;
                        cur->fs->fs_close(&file);
                        grub_snprintf(dst, size - 1, "(%s)%s", cur->device, vlnk->filepath);
                    }
                    else
                    {
                        grub_errno = 0;
                    }
                }
            }
        }
    }

    if (print)
    {
        grub_printf("\n==== VLNK Information ====\n"
                    "Disk Signature: %08x\n"
                    "Partition Offset: %llu\n"
                    "File Path: <%s>\n\n",
                    vlnk->disk_signature, (ulonglong)vlnk->part_offset, vlnk->filepath);

        if (diskfind)
        {
            grub_printf("Disk Find: [ YES ] [ %s ]\n", disk);
        }
        else
        {
            grub_printf("Disk Find: [ NO ]\n");
        }
        
        if (partfind)
        {
            grub_printf("Part Find: [ YES ] [ %s ] [ %s ]\n", device, fs ? fs->name : "N/A");
        }
        else
        {
            grub_printf("Part Find: [ NO ]\n");
        }
        grub_printf("File Find: [ %s ]\n", filefind ? "YES" : "NO");
        if (filefind)
        {
            grub_printf("VLNK File: <%s>\n", dst);
        }
        
        grub_printf("\n");
        grub_refresh();
    }

    return (1 - filefind);
}

int ventoy_add_vlnk_file(char *dir, const char *name)
{
    int rc = 1;
    char src[512];
    char dst[512];
    grub_file_t file = NULL;
    ventoy_vlnk vlnk;

    if (!dir)
    {
        grub_snprintf(src, sizeof(src), "%s%s", g_iso_path, name);
    }
    else if (dir[0] == '/')
    {
        grub_snprintf(src, sizeof(src), "%s%s%s", g_iso_path, dir, name);
    }
    else
    {
        grub_snprintf(src, sizeof(src), "%s/%s%s", g_iso_path, dir, name);
    }

    file = grub_file_open(src, VENTOY_FILE_TYPE);
    if (!file)
    {
        return 1;
    }

    grub_memset(&vlnk, 0, sizeof(vlnk));
    grub_file_read(file, &vlnk, sizeof(vlnk));
    grub_file_close(file);

    if (ventoy_check_vlnk_data(&vlnk, 0, dst, sizeof(dst)) == 0)
    {
        rc = grub_file_add_vlnk(src, dst);        
    }

    return rc;
}

static int ventoy_collect_img_files(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    //int i = 0;
    int type = 0;
    int ignore = 0;
    int index = 0;
    int vlnk = 0;
    grub_size_t len;
    img_info *img;
    img_info *tail;
    const menu_tip *tip;
    img_iterator_node *tmp;
    img_iterator_node *new_node;
    img_iterator_node *node = (img_iterator_node *)data;

    if (g_enumerate_time_checked == 0)
    {
        g_enumerate_finish_time_ms = grub_get_time_ms();
        if ((g_enumerate_finish_time_ms - g_enumerate_start_time_ms) >= 3000)
        {
            grub_cls();
            grub_printf("\n\n Ventoy scanning files, please wait...\n");
            grub_refresh();
            g_enumerate_time_checked = 1;
        }        
    }

    len = grub_strlen(filename);
    
    if (info->dir)
    {
        if (node->level + 1 > g_img_max_search_level)
        {
            return 0;
        }
    
        if ((len == 1 && filename[0] == '.') ||
            (len == 2 && filename[0] == '.' && filename[1] == '.'))
        {
            return 0;
        }

        if (!ventoy_img_name_valid(filename, len))
        {
            return 0;
        }

        if (g_filt_trash_dir)
        {
            if (0 == grub_strncmp(filename, ".trash-", 7) ||
                0 == grub_strcmp(filename, ".Trashes") ||
                0 == grub_strncmp(filename, "$RECYCLE.BIN", 12))
            {
                return 0;
            }
        }

        if (g_plugin_image_list == VENTOY_IMG_WHITE_LIST)
        {
            grub_snprintf(g_img_swap_tmp_buf, sizeof(g_img_swap_tmp_buf), "%s%s/", node->dir, filename);
            index = ventoy_plugin_get_image_list_index(vtoy_class_directory, g_img_swap_tmp_buf);
            if (index == 0)
            {
                debug("Directory %s not found in image_list plugin config...\n", g_img_swap_tmp_buf);
                return 0; 
            }
        }

        new_node = grub_zalloc(sizeof(img_iterator_node));
        if (new_node)
        {
            new_node->level = node->level + 1;
            new_node->plugin_list_index = index;
            new_node->dirlen = grub_snprintf(new_node->dir, sizeof(new_node->dir), "%s%s/", node->dir, filename);

            g_enum_fs->fs_dir(g_enum_dev, new_node->dir, ventoy_check_ignore_flag, &ignore);
            if (ignore)
            {
                debug("Directory %s ignored...\n", new_node->dir);
                grub_free(new_node);
                return 0;
            }

            new_node->tail = node->tail;

            new_node->parent = node;
            if (!node->firstchild)
            {
                node->firstchild = new_node;
            }

            if (g_img_iterator_tail)
            {
                g_img_iterator_tail->next = new_node;
                g_img_iterator_tail = new_node;
            }
            else
            {
                g_img_iterator_head.next = new_node;
                g_img_iterator_tail = new_node;
            }
        }
    }
    else
    {
        debug("Find a file %s\n", filename);
        if (len < 4)
        {
            return 0;
        }

        if (FILE_FLT(ISO) && 0 == grub_strcasecmp(filename + len - 4, ".iso"))
        {
            type = img_type_iso;
        }
        else if (FILE_FLT(WIM) && g_wimboot_enable && (0 == grub_strcasecmp(filename + len - 4, ".wim")))
        {
            type = img_type_wim;
        }
        else if (FILE_FLT(VHD) && g_vhdboot_enable && (0 == grub_strcasecmp(filename + len - 4, ".vhd") || 
                (len >= 5 && 0 == grub_strcasecmp(filename + len - 5, ".vhdx"))))
        {
            type = img_type_vhd;
        }
        #ifdef GRUB_MACHINE_EFI
        else if (FILE_FLT(EFI) && 0 == grub_strcasecmp(filename + len - 4, ".efi"))
        {
            type = img_type_efi;
        }
        #endif
        else if (FILE_FLT(IMG) && 0 == grub_strcasecmp(filename + len - 4, ".img"))
        {
            if (len == 18 && grub_strncmp(filename, "ventoy_", 7) == 0)
            {
                if (grub_strncmp(filename + 7, "wimboot", 7) == 0 ||
                    grub_strncmp(filename + 7, "vhdboot", 7) == 0)
                {
                    return 0;
                }
            }
            type = img_type_img;
        }
        else if (FILE_FLT(VTOY) && len >= 5 && 0 == grub_strcasecmp(filename + len - 5, ".vtoy"))
        {
            type = img_type_vtoy;
        }
        else if (len >= 9 && 0 == grub_strcasecmp(filename + len - 5, ".vcfg"))
        {
            if (filename[len - 9] == '.' || (len >= 10 && filename[len - 10] == '.'))
            {
                grub_snprintf(g_img_swap_tmp_buf, sizeof(g_img_swap_tmp_buf), "%s%s", node->dir, filename);
                ventoy_plugin_add_custom_boot(g_img_swap_tmp_buf);
            }
            return 0;
        }
        else
        {
            return 0;
        }

        if (g_filt_dot_underscore_file && filename[0] == '.' && filename[1] == '_')
        {
            return 0;
        }

        if (g_plugin_image_list)
        {
            grub_snprintf(g_img_swap_tmp_buf, sizeof(g_img_swap_tmp_buf), "%s%s", node->dir, filename);
            index = ventoy_plugin_get_image_list_index(vtoy_class_image_file, g_img_swap_tmp_buf);
            if (VENTOY_IMG_WHITE_LIST == g_plugin_image_list && index == 0)
            {
                debug("File %s not found in image_list plugin config...\n", g_img_swap_tmp_buf);
                return 0; 
            }
            else if (VENTOY_IMG_BLACK_LIST == g_plugin_image_list && index > 0)
            {
                debug("File %s found in image_blacklist plugin config %d ...\n", g_img_swap_tmp_buf, index);
                return 0; 
            }
        }

        if (info->size == VTOY_FILT_MIN_FILE_SIZE || info->size == 0)
        {
            if (grub_file_is_vlnk_suffix(filename, len))
            {
                vlnk = 1;
                if (ventoy_add_vlnk_file(node->dir, filename) != 0)
                {
                    return 0;
                }
            }
        }
        
        img = grub_zalloc(sizeof(img_info));
        if (img)
        {
            img->type = type;
            img->plugin_list_index = index;
            grub_snprintf(img->name, sizeof(img->name), "%s", filename);

            img->pathlen = grub_snprintf(img->path, sizeof(img->path), "%s%s", node->dir, img->name);

            img->size = info->size;
            if (vlnk || 0 == img->size)
            {
                if (node->dir[0] == '/')
                {
                    img->size = ventoy_grub_get_file_size("%s%s%s", g_iso_path, node->dir, filename);                    
                }
                else
                {
                    img->size = ventoy_grub_get_file_size("%s/%s%s", g_iso_path, node->dir, filename);                    
                }
            }

            if (img->size < VTOY_FILT_MIN_FILE_SIZE)
            {
                debug("img <%s> size too small %llu\n", img->name, (ulonglong)img->size);
                grub_free(img);
                return 0;
            }
            
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
            
            img->id = g_ventoy_img_count;
            img->parent = node;
            if (node && NULL == node->firstiso)
            {
                node->firstiso = img;
            }

            node->isocnt++;
            tmp = node->parent;
            while (tmp)
            {
                tmp->isocnt++;
                tmp = tmp->parent;
            }
            
            *((img_info **)(node->tail)) = img;
            g_ventoy_img_count++;

            img->alias = ventoy_plugin_get_menu_alias(vtoy_alias_image_file, img->path);

            tip = ventoy_plugin_get_menu_tip(vtoy_tip_image_file, img->path);
            if (tip)
            {
                img->tip1 = tip->tip1;
                img->tip2 = tip->tip2;
            }
            
            img->class = ventoy_plugin_get_menu_class(vtoy_class_image_file, img->name, img->path);
            if (!img->class)
            {
                img->class = g_menu_class[type];
            }
            img->menu_prefix = g_menu_prefix[type];

            if (img_type_iso == type)
            {
                if (ventoy_plugin_check_memdisk(img->path))
                {
                    img->menu_prefix = "miso";
                }
            }
            else if (img_type_img == type)
            {
                if (ventoy_plugin_check_memdisk(img->path))
                {
                    img->menu_prefix = "mimg";
                }
            }

            debug("Add %s%s to list %d\n", node->dir, filename, g_ventoy_img_count);
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

    puint2[0] = grub_swap_bytes32(g_ventoy_plat_data);    

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

static int
ventoy_password_get (char buf[], unsigned buf_size)
{
  unsigned i, cur_len = 0;
  int key;
  struct grub_term_coordinate *pos = grub_term_save_pos ();

    while (1)
    {
        key = grub_getkey (); 
        if (key == '\n' || key == '\r')
            break;

        if (key == GRUB_TERM_ESC)
        {
            cur_len = 0;
            break;
        }

        if (key == '\b')
        {
            if (cur_len)
            {
                grub_term_restore_pos (pos);
                for (i = 0; i < cur_len; i++)
                    grub_xputs (" ");
                grub_term_restore_pos (pos);
                cur_len--;
                for (i = 0; i < cur_len; i++)
                    grub_xputs ("*");
                grub_refresh ();
            }
            continue;
        }

        if (!grub_isprint (key))
            continue;

        if (cur_len + 2 < buf_size)
            buf[cur_len++] = key;
        grub_xputs ("*");
        grub_refresh ();
    }

    grub_memset (buf + cur_len, 0, buf_size - cur_len);

    grub_xputs ("\n");
    grub_refresh ();
    grub_free (pos);

    return (key != GRUB_TERM_ESC);
}

static int ventoy_get_password(char buf[], unsigned buf_size)
{
    const char *env = NULL;

    env = grub_env_get("VTOY_SHOW_PASSWORD_ASTERISK");
    if (env && env[0] == '0' && env[1] == 0)
    {
        return grub_password_get(buf, buf_size);
    }
    else
    {
        return ventoy_password_get(buf, buf_size);
    }
}

int ventoy_check_password(const vtoy_password *pwd, int retry)
{
    int offset;
    char input[256];
    grub_uint8_t md5[16];

    while (retry--)
    {
        grub_memset(input, 0, sizeof(input));

        grub_printf("Enter password: ");
        grub_refresh();
        
        if (pwd->type == VTOY_PASSWORD_TXT)
        {
            ventoy_get_password(input, 128);
            if (grub_strcmp(pwd->text, input) == 0)
            {
                return 0;
            }
        }
        else if (pwd->type == VTOY_PASSWORD_MD5)
        {
            ventoy_get_password(input, 128);
            grub_crypto_hash(GRUB_MD_MD5, md5, input, grub_strlen(input));
            if (grub_memcmp(pwd->md5, md5, 16) == 0)
            {
                return 0;
            }
        }
        else if (pwd->type == VTOY_PASSWORD_SALT_MD5)
        {
            offset = (int)grub_snprintf(input, 128, "%s", pwd->salt);
            ventoy_get_password(input + offset, 128);
            
            grub_crypto_hash(GRUB_MD_MD5, md5, input, grub_strlen(input));
            if (grub_memcmp(pwd->md5, md5, 16) == 0)
            {
                return 0;
            }
        }
        
        grub_printf("Invalid password!\n\n");
        grub_refresh();
    }

    return 1;
}

static img_info * ventoy_get_min_iso(img_iterator_node *node)
{
    img_info *minimg = NULL;
    img_info *img = (img_info *)(node->firstiso);

    while (img && (img_iterator_node *)(img->parent) == node)
    {
        if (img->select == 0 && (NULL == minimg || ventoy_cmp_img(img, minimg) < 0))
        {
            minimg = img;
        }
        img = img->next;
    }

    if (minimg)
    {
        minimg->select = 1;
    }

    return minimg;
}

static img_iterator_node * ventoy_get_min_child(img_iterator_node *node)
{
    img_iterator_node *Minchild = NULL;
    img_iterator_node *child = node->firstchild;

    while (child && child->parent == node)
    {
        if (child->select == 0 && (NULL == Minchild || ventoy_cmp_subdir(child, Minchild) < 0))
        {
            Minchild = child;
        }
        child = child->next;
    }

    if (Minchild)
    {
        Minchild->select = 1;
    }

    return Minchild;
}

static int ventoy_dynamic_tree_menu(img_iterator_node *node)
{
    int offset = 1;
    img_info *img = NULL;
    const char *dir_class = NULL;
    const char *dir_alias = NULL;
    img_iterator_node *child = NULL;
    const menu_tip *tip = NULL;
    
    if (node->isocnt == 0 || node->done == 1)
    {
        return 0;
    }

    if (node->parent && node->parent->dirlen < node->dirlen)
    {
        offset = node->parent->dirlen;
    }

    if (node == &g_img_iterator_head)
    {
        if (g_default_menu_mode == 0)
        {
            if (g_tree_view_menu_style == 0)
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, 
                              "menuentry \"%-10s [%s]\" --class=\"vtoyret\" VTOY_RET {\n  "
                              "  echo 'return ...' \n"
                              "}\n", "<--", ventoy_get_vmenu_title("VTLANG_RET_TO_LISTVIEW"));
            }
            else
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, 
                              "menuentry \"[%s]\" --class=\"vtoyret\" VTOY_RET {\n  "
                              "  echo 'return ...' \n"
                              "}\n", ventoy_get_vmenu_title("VTLANG_RET_TO_LISTVIEW"));
            }
        }

        g_tree_script_pre = g_tree_script_pos;
    }
    else
    {
        node->dir[node->dirlen - 1] = 0;
        dir_class = ventoy_plugin_get_menu_class(vtoy_class_directory, node->dir, node->dir);
        if (!dir_class)
        {
            dir_class = "vtoydir";
        }

        tip = ventoy_plugin_get_menu_tip(vtoy_tip_directory, node->dir);

        dir_alias = ventoy_plugin_get_menu_alias(vtoy_alias_directory, node->dir);
        if (dir_alias)
        {
            if (g_tree_view_menu_style == 0)
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, 
                              "submenu \"%-10s %s\" --class=\"%s\" --id=\"DIR_%s\" _VTIP_%p {\n", 
                              "DIR", dir_alias, dir_class, node->dir + offset, tip);
            }
            else
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, 
                              "submenu \"%s\" --class=\"%s\" --id=\"DIR_%s\" _VTIP_%p {\n", 
                              dir_alias, dir_class, node->dir + offset, tip);
            }
        }
        else
        {
            dir_alias = node->dir + offset;

            if (g_tree_view_menu_style == 0)
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, 
                              "submenu \"%-10s [%s]\" --class=\"%s\" --id=\"DIR_%s\" _VTIP_%p {\n", 
                              "DIR", dir_alias, dir_class, node->dir + offset, tip);
            }
            else
            {
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, 
                              "submenu \"[%s]\" --class=\"%s\" --id=\"DIR_%s\" _VTIP_%p {\n", 
                              dir_alias, dir_class, node->dir + offset, tip);
            }
        }

        if (g_tree_view_menu_style == 0)
        {
            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, 
                          "menuentry \"%-10s [%s/..]\" --class=\"vtoyret\" VTOY_RET {\n  "
                          "  echo 'return ...' \n"
                          "}\n", "<--", node->dir);
        }
        else
        {
            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, 
                          "menuentry \"[%s/..]\" --class=\"vtoyret\" VTOY_RET {\n  "
                          "  echo 'return ...' \n"
                          "}\n", node->dir);
        }
    }

    while ((child = ventoy_get_min_child(node)) != NULL)
    {
        ventoy_dynamic_tree_menu(child);
    }

    while ((img = ventoy_get_min_iso(node)) != NULL)
    {
        if (g_tree_view_menu_style == 0)
        {
            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, 
                          "menuentry \"%-10s %s%s\" --class=\"%s\" --id=\"VID_%p\" {\n"
                          "  %s_%s \n" 
                          "}\n", 
                          grub_get_human_size(img->size, GRUB_HUMAN_SIZE_SHORT), 
                          img->unsupport ? "[***********] " : "", 
                          img->alias ? img->alias : img->name, img->class, img,
                          img->menu_prefix,
                          img->unsupport ? "unsupport_menuentry" : "common_menuentry");
        }
        else
        {
            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, 
                          "menuentry \"%s%s\" --class=\"%s\" --id=\"VID_%p\" {\n"
                          "  %s_%s \n" 
                          "}\n", 
                          img->unsupport ? "[***********] " : "", 
                          img->alias ? img->alias : img->name, img->class, img,
                          img->menu_prefix,
                          img->unsupport ? "unsupport_menuentry" : "common_menuentry");
        }
    }

    if (node != &g_img_iterator_head)
    {
        vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, "}\n");
    }

    node->done = 1;
    return 0;    
}

static int ventoy_set_default_menu(void)
{
    int img_len = 0;
    char *pos = NULL;
    char *end = NULL;
    char *def = NULL;
    const char *strdata = NULL;
    img_info *cur = NULL;
    img_info *default_node = NULL;
    const char *default_image = NULL;

    default_image = ventoy_get_env("VTOY_DEFAULT_IMAGE");        
    if (default_image && default_image[0] == '/')
    {
        img_len = grub_strlen(default_image);

        for (cur = g_ventoy_img_list; cur; cur = cur->next)
        {
            if (img_len == cur->pathlen && grub_strcmp(default_image, cur->path) == 0)
            {
                default_node = cur;
                break;
            }
        }

        if (!default_node)
        {
            return 1;
        }

        if (0 == g_default_menu_mode)
        {
            vtoy_ssprintf(g_list_script_buf, g_list_script_pos, "set default='VID_%p'\n", default_node);
        }
        else
        {
            def = grub_strdup(default_image);
            if (!def)
            {
                return 1;
            }

            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, "set default=%c", '\'');

            strdata = ventoy_get_env("VTOY_DEFAULT_SEARCH_ROOT");
            if (strdata && strdata[0] == '/')
            {
                pos = def + grub_strlen(strdata);
                if (*pos == '/')
                {
                    pos++;
                }
            }
            else
            {
                pos = def + 1;
            }

            while ((end = grub_strchr(pos, '/')) != NULL)
            {
                *end = 0;                
                vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, "DIR_%s>", pos);
                pos = end + 1;
            }

            vtoy_ssprintf(g_tree_script_buf, g_tree_script_pos, "VID_%p'\n", default_node);
            grub_free(def);
        }
    }

    return 0;
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

static grub_err_t ventoy_cmd_ext_select_img_path(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len = 0;
    char id[32] = {0};
    img_info *cur = g_ventoy_img_list;

    (void)ctxt;
    
    if (argc != 1)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {var}", cmd_raw_name);
    }

    len = (int)grub_strlen(args[0]);

    while (cur)
    {
        if (len == cur->pathlen && 0 == grub_strcmp(args[0], cur->path))
        {
            break;
        }
        cur = cur->next;
    }

    if (!cur)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "No such image");
    }

    grub_snprintf(id, sizeof(id), "VID_%p", cur);
    grub_env_set("chosen", id);
    grub_env_export("chosen");

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static char g_fake_vlnk_src[512];
static char g_fake_vlnk_dst[512];
static grub_uint64_t g_fake_vlnk_size;
static grub_err_t ventoy_cmd_set_fake_vlnk(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_fake_vlnk_size = (grub_uint64_t)grub_strtoull(args[2], NULL, 10);

    grub_strncpy(g_fake_vlnk_dst, args[0], sizeof(g_fake_vlnk_dst));
    grub_snprintf(g_fake_vlnk_src, sizeof(g_fake_vlnk_src), "%s/________VENTOYVLNK.vlnk.%s", g_iso_path, args[1]);

    grub_file_vtoy_vlnk(g_fake_vlnk_src, g_fake_vlnk_dst);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_reset_fake_vlnk(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_fake_vlnk_src[0] = 0;
    g_fake_vlnk_dst[0] = 0;
    g_fake_vlnk_size = 0;
    grub_file_vtoy_vlnk(NULL, NULL);
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}


static grub_err_t ventoy_cmd_chosen_img_path(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char value[32];
    char *pos = NULL;
    char *last = NULL;
    const char *id = NULL;
    img_info *cur = NULL;

    (void)ctxt;
    
    if (argc < 1 || argc > 3)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {var}", cmd_raw_name);
    }

    if (g_fake_vlnk_src[0] && g_fake_vlnk_dst[0])
    {
        pos = grub_strchr(g_fake_vlnk_src, '/');
        grub_env_set(args[0], pos);
        if (argc > 1)
        {
            grub_snprintf(value, sizeof(value), "%llu", (ulonglong)(g_fake_vlnk_size));
            grub_env_set(args[1], value);        
        }
        
        if (argc > 2)
        {
            for (last = pos; *pos; pos++)
            {
                if (*pos == '/')
                {
                    last = pos;
                }
            }
            grub_env_set(args[2], last + 1);
        }

        goto end;
    }

    id = grub_env_get("chosen");

    pos = grub_strstr(id, "VID_");
    if (pos)
    {
        cur = (img_info *)(void *)grub_strtoul(pos + 4, NULL, 16);
    }
    else
    {
        cur = g_ventoy_img_list;
    }

    if (!cur)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "No such image");
    }

    grub_env_set(args[0], cur->path);

    if (argc > 1)
    {
        grub_snprintf(value, sizeof(value), "%llu", (ulonglong)(cur->size));
        grub_env_set(args[1], value);        
    }
    
    if (argc > 2)
    {
        grub_snprintf(value, sizeof(value), "%llu", (ulonglong)(cur->size));
        grub_env_set(args[2], cur->name);
    }

end:
    g_svd_replace_offset = 0;

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}


static grub_err_t ventoy_cmd_list_img(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len;
    grub_fs_t fs;
    grub_device_t dev = NULL;
    img_info *cur = NULL;
    img_info *tail = NULL;
    img_info *min = NULL;
    img_info *head = NULL;
    const char *strdata = NULL;
    char *device_name = NULL;
    char buf[32];
    img_iterator_node *node = NULL;
    img_iterator_node *tmp = NULL;

    (void)ctxt;

    if (argc != 2)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s {device} {cntvar}", cmd_raw_name);
    }

    if (g_ventoy_img_list || g_ventoy_img_count)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Must clear image before list");
    }

    VTOY_CMD_CHECK(1);

    g_enumerate_time_checked  = 0;
    g_enumerate_start_time_ms = grub_get_time_ms();

    strdata = ventoy_get_env("VTOY_FILT_DOT_UNDERSCORE_FILE");
    if (strdata && strdata[0] == '1' && strdata[1] == 0)
    {
        g_filt_dot_underscore_file = 1;
    }
    
    strdata = ventoy_get_env("VTOY_FILT_TRASH_DIR");
    if (strdata && strdata[0] == '0' && strdata[1] == 0)
    {
        g_filt_trash_dir = 0;
    }

    strdata = ventoy_get_env("VTOY_SORT_CASE_SENSITIVE");
    if (strdata && strdata[0] == '1' && strdata[1] == 0)
    {
        g_sort_case_sensitive = 1;
    }

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        goto fail;
    }

    g_enum_dev = dev = grub_device_open(device_name);
    if (!dev)
    {
        goto fail;        
    }

    g_enum_fs = fs = grub_fs_probe(dev);
    if (!fs)
    {
        goto fail;
    }

    if (ventoy_get_fs_type(fs->name) >= ventoy_fs_max)
    {
        debug("unsupported fs:<%s>\n", fs->name);
        ventoy_set_env("VTOY_NO_ISO_TIP", "unsupported file system");
        goto fail;
    }

    ventoy_set_env("vtoy_iso_fs", fs->name);

    strdata = ventoy_get_env("VTOY_DEFAULT_MENU_MODE");
    if (strdata && strdata[0] == '1')
    {
        g_default_menu_mode = 1;
    }

    grub_memset(&g_img_iterator_head, 0, sizeof(g_img_iterator_head));

    grub_snprintf(g_iso_path, sizeof(g_iso_path), "%s", args[0]);

    strdata = ventoy_get_env("VTOY_DEFAULT_SEARCH_ROOT");
    if (strdata && strdata[0] == '/')
    {
        len = grub_snprintf(g_img_iterator_head.dir, sizeof(g_img_iterator_head.dir) - 1, "%s", strdata);
        if (g_img_iterator_head.dir[len - 1] != '/')
        {
            g_img_iterator_head.dir[len++] = '/';
        }
        g_img_iterator_head.dirlen = len;
    }
    else
    {
        g_img_iterator_head.dirlen = 1;
        grub_strcpy(g_img_iterator_head.dir, "/"); 
    }

    g_img_iterator_head.tail = &tail;

    if (g_img_max_search_level < 0)
    {
        g_img_max_search_level = GRUB_INT_MAX;
        strdata = ventoy_get_env("VTOY_MAX_SEARCH_LEVEL");
        if (strdata && ventoy_is_decimal(strdata))
        {
            g_img_max_search_level = (int)grub_strtoul(strdata, NULL, 10);
        }
    }

    g_vtoy_file_flt[VTOY_FILE_FLT_ISO]  = ventoy_control_get_flag("VTOY_FILE_FLT_ISO");
    g_vtoy_file_flt[VTOY_FILE_FLT_WIM]  = ventoy_control_get_flag("VTOY_FILE_FLT_WIM");
    g_vtoy_file_flt[VTOY_FILE_FLT_EFI]  = ventoy_control_get_flag("VTOY_FILE_FLT_EFI");
    g_vtoy_file_flt[VTOY_FILE_FLT_IMG]  = ventoy_control_get_flag("VTOY_FILE_FLT_IMG");
    g_vtoy_file_flt[VTOY_FILE_FLT_VHD]  = ventoy_control_get_flag("VTOY_FILE_FLT_VHD");
    g_vtoy_file_flt[VTOY_FILE_FLT_VTOY] = ventoy_control_get_flag("VTOY_FILE_FLT_VTOY");

    for (node = &g_img_iterator_head; node; node = node->next)
    {
        fs->fs_dir(dev, node->dir, ventoy_collect_img_files, node);        
    }

    strdata = ventoy_get_env("VTOY_TREE_VIEW_MENU_STYLE");
    if (strdata && strdata[0] == '1' && strdata[1] == 0)
    {
        g_tree_view_menu_style = 1;
    }

    ventoy_set_default_menu();

    for (node = &g_img_iterator_head; node; node = node->next)
    {
        ventoy_dynamic_tree_menu(node);
    }

    /* free node */
    node = g_img_iterator_head.next;    
    while (node)
    {
        tmp = node->next;
        grub_free(node);
        node = tmp;
    }
    
    /* sort image list by image name */
    while (g_ventoy_img_list)
    {
        min = g_ventoy_img_list;
        for (cur = g_ventoy_img_list->next; cur; cur = cur->next)
        {
            if (ventoy_cmp_img(min, cur) > 0)
            {
                min = cur;
            }
        }

        if (min->prev)
        {
            min->prev->next = min->next;                
        }
        
        if (min->next)
        {
            min->next->prev = min->prev;
        }

        if (min == g_ventoy_img_list)
        {
            g_ventoy_img_list = min->next;
        }

        if (head == NULL)
        {
            head = tail = min;
            min->prev = NULL;
            min->next = NULL;
        }
        else
        {
            tail->next = min;
            min->prev = tail;
            min->next = NULL;
            tail = min;
        }
    }

    g_ventoy_img_list = head;

    if (g_default_menu_mode == 1)
    {
        vtoy_ssprintf(g_list_script_buf, g_list_script_pos, 
                      "menuentry \"%s [%s]\" --class=\"vtoyret\" VTOY_RET {\n  "
                      "  echo 'return ...' \n"
                      "}\n", "<--", ventoy_get_vmenu_title("VTLANG_RET_TO_TREEVIEW"));
    }

    for (cur = g_ventoy_img_list; cur; cur = cur->next)
    {
        vtoy_ssprintf(g_list_script_buf, g_list_script_pos,
                  "menuentry \"%s%s\" --class=\"%s\" --id=\"VID_%p\" {\n"
                  "  %s_%s \n" 
                  "}\n", 
                  cur->unsupport ? "[***********] " : "", 
                  cur->alias ? cur->alias : cur->name, cur->class, cur,
                  cur->menu_prefix,
                  cur->unsupport ? "unsupport_menuentry" : "common_menuentry");
    }

    g_tree_script_buf[g_tree_script_pos] = 0;
    g_list_script_buf[g_list_script_pos] = 0;

    grub_snprintf(buf, sizeof(buf), "%d", g_ventoy_img_count);
    grub_env_set(args[1], buf);

fail:

    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

int ventoy_get_disk_guid(const char *filename, grub_uint8_t *guid, grub_uint8_t *signature)
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
        grub_disk_read(disk, 0, 0x1b8, 4, signature);
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

static grub_uint32_t ventoy_get_bios_eltorito_rba(grub_file_t file, grub_uint32_t sector)
{
    grub_uint8_t buf[512];

    grub_file_seek(file, sector * 2048);
    grub_file_read(file, buf, sizeof(buf));

    if (buf[0] == 0x01 && buf[1] == 0x00 && 
        buf[30] == 0x55 && buf[31] == 0xaa && buf[32] == 0x88)
    {
        return *((grub_uint32_t *)(buf + 40));
    }

    return 0;
}

int ventoy_has_efi_eltorito(grub_file_t file, grub_uint32_t sector)
{
    int i;
    int x86count = 0;
    grub_uint8_t buf[512];
    grub_uint8_t parttype[] = { 0x04, 0x06, 0x0B, 0x0C };

    grub_file_seek(file, sector * 2048);
    grub_file_read(file, buf, sizeof(buf));

    if (buf[0] == 0x01 && buf[1] == 0xEF)
    {
        debug("%s efi eltorito in Validation Entry\n", file->name);
        return 1;
    }

    if (buf[0] == 0x01 && buf[1] == 0x00)
    {
        x86count++;
    }

    for (i = 64; i < (int)sizeof(buf); i += 32)
    {
        if ((buf[i] == 0x90 || buf[i] == 0x91) && buf[i + 1] == 0xEF)
        {
            debug("%s efi eltorito offset %d 0x%02x\n", file->name, i, buf[i]);
            return 1;
        }

        if ((buf[i] == 0x90 || buf[i] == 0x91) && buf[i + 1] == 0x00 && x86count == 1)
        {
            debug("0x9100 assume %s efi eltorito offset %d 0x%02x\n", file->name, i, buf[i]);
            return 1;
        }
    }

    if (x86count && buf[32] == 0x88 && buf[33] == 0x04)
    {
        for (i = 0; i < (int)(ARRAY_SIZE(parttype)); i++)
        {
            if (buf[36] == parttype[i])
            {
                debug("hard disk image assume %s efi eltorito, part type 0x%x\n", file->name, buf[36]);
                return 1;
            }
        }
    }

    debug("%s does not contain efi eltorito\n", file->name);
    return 0;
}

void ventoy_fill_os_param(grub_file_t file, ventoy_os_param *param)
{
    char *pos;
    const char *fs = NULL;
    const char *val = NULL;
    const char *cdprompt = NULL;
    grub_uint32_t i;
    grub_uint8_t  chksum = 0;
    grub_disk_t   disk;

    disk = file->device->disk;
    grub_memcpy(&param->guid, &g_ventoy_guid, sizeof(ventoy_guid));

    param->vtoy_disk_size = disk->total_sectors * (1 << disk->log_sector_size);
    param->vtoy_disk_part_id = disk->partition->number + 1;
    param->vtoy_disk_part_type = ventoy_get_fs_type(file->fs->name);

    pos = grub_strstr(file->name, "/");
    if (!pos)
    {
        pos = file->name;
    }

    grub_snprintf(param->vtoy_img_path, sizeof(param->vtoy_img_path), "%s", pos);
    
    ventoy_get_disk_guid(file->name, param->vtoy_disk_guid, param->vtoy_disk_signature);

    param->vtoy_img_size = file->size;

    param->vtoy_reserved[0] = g_ventoy_break_level;
    param->vtoy_reserved[1] = g_ventoy_debug_level;
    
    param->vtoy_reserved[2] = g_ventoy_chain_type;

    /* Windows CD/DVD prompt   0:suppress  1:reserved */
    param->vtoy_reserved[4] = 0;
    if (g_ventoy_chain_type == 1) /* Windows */
    {
        cdprompt = ventoy_get_env("VTOY_WINDOWS_CD_PROMPT");
        if (cdprompt && cdprompt[0] == '1' && cdprompt[1] == 0)
        {
            param->vtoy_reserved[4] = 1;
        }
    }
    
    fs = ventoy_get_env("ventoy_fs_probe");
    if (fs && grub_strcmp(fs, "udf") == 0)
    {
        param->vtoy_reserved[3] = 1;
    }

    param->vtoy_reserved[5] = 0;
    val = ventoy_get_env("VTOY_LINUX_REMOUNT");
    if (val && val[0] == '1' && val[1] == 0)
    {
        param->vtoy_reserved[5] = 1;
    }

    /* ventoy_disk_signature used for vlnk */
    param->vtoy_reserved[6] = file->vlnk;
    grub_memcpy(param->vtoy_reserved + 7, g_ventoy_part_info->MBR.BootCode + 0x1b8, 4);

    /* calculate checksum */
    for (i = 0; i < sizeof(ventoy_os_param); i++)
    {
        chksum += *((grub_uint8_t *)param + i);
    }
    param->chksum = (grub_uint8_t)(0x100 - chksum);

    return;
}

int ventoy_check_block_list(grub_file_t file, ventoy_img_chunk_list *chunklist, grub_disk_addr_t start)
{
    grub_uint32_t i = 0;
    grub_uint64_t total = 0;
    grub_uint64_t fileblk = 0;
    ventoy_img_chunk *chunk = NULL;

    for (i = 0; i < chunklist->cur_chunk; i++)
    {
        chunk = chunklist->chunk + i;
        
        if (chunk->disk_start_sector <= start)
        {
            debug("%u disk start invalid %lu\n", i, (ulong)start);
            return 1;
        }

        total += chunk->disk_end_sector + 1 - chunk->disk_start_sector;
    }

    fileblk = (file->size + 511) / 512;

    if (total != fileblk)
    {
        debug("Invalid total: %llu %llu\n", (ulonglong)total, (ulonglong)fileblk);
        if ((file->size % 512) && (total + 1 == fileblk))
        {
            debug("maybe img file to be processed.\n");
            return 0;
        }
        
        return 1;
    }

    return 0;
}

int ventoy_get_block_list(grub_file_t file, ventoy_img_chunk_list *chunklist, grub_disk_addr_t start)
{
    int fs_type;
    int len;
    grub_uint32_t i = 0;
    grub_uint32_t sector = 0;
    grub_uint32_t count = 0;
    grub_off_t size = 0;
    grub_off_t read = 0;

    fs_type = ventoy_get_fs_type(file->fs->name);
    if (fs_type == ventoy_fs_exfat)
    {
        grub_fat_get_file_chunk(start, file, chunklist);        
    }
    else if (fs_type == ventoy_fs_ext)
    {
        grub_ext_get_file_chunk(start, file, chunklist);        
    }
    else
    {
        file->read_hook = (grub_disk_read_hook_t)(void *)grub_disk_blocklist_read;
        file->read_hook_data = chunklist;

        for (size = file->size; size > 0; size -= read)
        {
            read = (size > VTOY_SIZE_1GB) ? VTOY_SIZE_1GB : size;
            grub_file_read(file, NULL, read);
        }

        for (i = 0; start > 0 && i < chunklist->cur_chunk; i++)
        {
            chunklist->chunk[i].disk_start_sector += start;
            chunklist->chunk[i].disk_end_sector += start;
        }

        if (ventoy_fs_udf == fs_type)
        {
            for (i = 0; i < chunklist->cur_chunk; i++)
            {
                count = (chunklist->chunk[i].disk_end_sector + 1 - chunklist->chunk[i].disk_start_sector) >> 2;
                chunklist->chunk[i].img_start_sector = sector;
                chunklist->chunk[i].img_end_sector = sector + count - 1;
                sector += count;
            }
        }
    }

    len = (int)grub_strlen(file->name);
    if ((len > 4 && grub_strncasecmp(file->name + len - 4, ".img", 4) == 0) ||
        (len > 4 && grub_strncasecmp(file->name + len - 4, ".vhd", 4) == 0) ||
        (len > 5 && grub_strncasecmp(file->name + len - 5, ".vhdx", 5) == 0) ||
        (len > 5 && grub_strncasecmp(file->name + len - 5, ".vtoy", 5) == 0))
    {
        for (i = 0; i < chunklist->cur_chunk; i++)
        {
            count = chunklist->chunk[i].disk_end_sector + 1 - chunklist->chunk[i].disk_start_sector;
            if (count < 4)
            {
                count = 1;
            }
            else
            {
                count >>= 2;
            }
            
            chunklist->chunk[i].img_start_sector = sector;
            chunklist->chunk[i].img_end_sector = sector + count - 1;
            sector += count;
        }
    }

    return 0;
}

static grub_err_t ventoy_cmd_img_sector(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc;
    grub_file_t file;
    grub_disk_addr_t start;
    
    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't open file %s\n", args[0]); 
    }

    g_conf_replace_count = 0;
    grub_memset(g_conf_replace_node, 0, sizeof(g_conf_replace_node ));
    grub_memset(g_conf_replace_offset, 0, sizeof(g_conf_replace_offset ));
    
    if (g_img_chunk_list.chunk)
    {
        grub_free(g_img_chunk_list.chunk);
    }

    if (ventoy_get_fs_type(file->fs->name) >= ventoy_fs_max)
    {
        grub_file_close(file);
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Unsupported filesystem %s\n", file->fs->name); 
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

    start = file->device->disk->partition->start;

    ventoy_get_block_list(file, &g_img_chunk_list, start);

    rc = ventoy_check_block_list(file, &g_img_chunk_list, start);
    grub_file_close(file);
    
    if (rc)
    {
        return grub_error(GRUB_ERR_NOT_IMPLEMENTED_YET, "Unsupported chunk list.\n");
    }

    grub_memset(&g_grub_param->file_replace, 0, sizeof(g_grub_param->file_replace));
    grub_memset(&g_grub_param->img_replace, 0, sizeof(g_grub_param->img_replace));
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_select_conf_replace(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int n;
    grub_uint64_t offset = 0;
    grub_uint32_t align = 0;
    grub_file_t file = NULL;
    conf_replace *node = NULL;
    conf_replace *nodes[VTOY_MAX_CONF_REPLACE] = { NULL };
    ventoy_grub_param_file_replace *replace = NULL;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    debug("select conf replace argc:%d\n", argc);

    if (argc < 2)
    {
        return 0;
    }

    n = ventoy_plugin_find_conf_replace(args[1], nodes);
    if (!n)
    {
        debug("Conf replace not found for %s\n", args[1]);
        goto end;
    }

    debug("Find %d conf replace for %s\n", n, args[1]);

    g_conf_replace_count = n;
    for (i = 0; i < n; i++)
    {
        node = nodes[i];

        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "(loop)%s", node->orgconf);
        if (file)
        {
            offset = grub_iso9660_get_last_file_dirent_pos(file);
            grub_file_close(file);  
        }
        else if (node->img > 0)
        {
            offset = 0;
        }
        else
        {
            debug("<(loop)%s> NOT exist\n", node->orgconf);
            continue;
        }

        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", args[0], node->newconf);
        if (!file)
        {
            debug("New config file <%s%s> NOT exist\n", args[0], node->newconf);
            continue;
        }

        align = ((int)file->size + 2047) / 2048 * 2048;

        if (align > vtoy_max_replace_file_size)
        {
            debug("New config file <%s%s> too big\n", args[0], node->newconf);
            grub_file_close(file);
            continue;
        }

        grub_file_read(file, g_conf_replace_new_buf[i], file->size);
        grub_file_close(file);
        g_conf_replace_new_len[i] = (int)file->size;
        g_conf_replace_new_len_align[i] = align;

        g_conf_replace_node[i] = node;
        g_conf_replace_offset[i] = offset + 2;

        if (node->img > 0)
        {
            replace = &(g_grub_param->img_replace[i]);
            replace->magic = GRUB_IMG_REPLACE_MAGIC;
            grub_snprintf(replace->old_file_name[replace->old_name_cnt], 256, "%s", node->orgconf);
            replace->old_name_cnt++;
        }

        debug("conf_replace OK: newlen[%d]: %d img:%d\n", i, g_conf_replace_new_len[i], node->img);
    }

end:
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static int ventoy_var_expand(int *record, int *flag, const char *var, char *expand, int len)
{
    int i = 0;
    int n = 0;
    char c;
    const char *ch = var;

    *record = 0;
    expand[0] = 0;

    while (*ch)
    {
        if (*ch == '_' || (*ch >= '0' && *ch <= '9') || (*ch >= 'A' && *ch <= 'Z') || (*ch >= 'a' && *ch <= 'z'))
        {
            ch++;
            n++;
        }
        else
        {
            debug("Invalid variable letter <%c>\n", *ch);
            goto end;
        }
    }

    if (n > 32)
    {
        debug("Invalid variable length:%d <%s>\n", n, var);
        goto end;
    }

    if (grub_strncmp(var, "VT_", 3) == 0) /* built-in variables */
    {

    }
    else
    {
        if (*flag == 0)
        {
            *flag = 1;
            grub_printf("\n===================  Variables Expansion  ===================\n\n");
        }
    
        grub_printf("<%s>: ", var);
        grub_refresh();

        while (i < (len - 1))
        {
            c = grub_getkey();
            if ((c == '\n') || (c == '\r'))
            {
                if (i > 0)
                {
                    grub_printf("\n");
                    grub_refresh();
                    *record = 1;
                    break;                    
                }
            }
            else if (grub_isprint(c))
            {
                if (i + 1 < (len - 1))
                {
                    grub_printf("%c", c);
                    grub_refresh();
                    expand[i++] = c;
                    expand[i] = 0;
                }
            }
            else if (c == '\b')
            {
                if (i > 0)
                {
                    expand[i - 1] = ' ';
                    grub_printf("\r<%s>: %s", var, expand);

                    expand[i - 1] = 0;
                    grub_printf("\r<%s>: %s", var, expand);
                    
                    grub_refresh();
                    i--;
                }
            }
        }
    }

end:
    if (expand[0] == 0)
    {
        grub_snprintf(expand, len, "$$%s$$", var);
    }
    
    return 0;
}

static int ventoy_auto_install_var_expand(install_template *node)
{
    int pos = 0;
    int flag = 0;
    int record = 0;
    int newlen = 0;
    char *start = NULL;
    char *end = NULL;
    char *newbuf = NULL;
    char *curline = NULL;
    char *nextline = NULL;
    grub_uint8_t *code = NULL;
    char value[512];
    var_node *CurNode = NULL;
    var_node *pVarList = NULL;

    code = (grub_uint8_t *)node->filebuf;

    if (node->filelen >= VTOY_SIZE_1MB)
    {
        debug("auto install script too long %d\n", node->filelen);
        return 0;
    }
    
    if ((code[0] == 0xff && code[1] == 0xfe) || (code[0] == 0xfe && code[1] == 0xff))
    {
        debug("UCS-2 encoding NOT supported\n");
        return 0;
    }

    start = grub_strstr(node->filebuf, "$$");
    if (!start)
    {
        debug("no need to expand variable, no start.\n");
        return 0;
    }

    end = grub_strstr(start + 2, "$$");
    if (!end)
    {
        debug("no need to expand variable, no end.\n");
        return 0;
    }

    newlen = grub_max(node->filelen * 10, VTOY_SIZE_128KB);
    newbuf = grub_malloc(newlen);
    if (!newbuf)
    {
        debug("Failed to alloc newbuf %d\n", newlen);
        return 0;
    }

    for (curline = node->filebuf; curline; curline = nextline)
    {
        nextline = ventoy_get_line(curline);

        start = grub_strstr(curline, "$$");
        if (start)
        {
            end = grub_strstr(start + 2, "$$");
        }

        if (start && end)
        {
            *start = *end = 0;
            VTOY_APPEND_NEWBUF(curline);

            for (CurNode = pVarList; CurNode; CurNode = CurNode->next)
            {
                if (grub_strcmp(start + 2, CurNode->var) == 0)
                {
                    grub_snprintf(value, sizeof(value) - 1, "%s", CurNode->val);
                    break;
                }
            }

            if (!CurNode)
            {
                value[sizeof(value) - 1] = 0;
                ventoy_var_expand(&record, &flag, start + 2, value, sizeof(value) - 1);

                if (record)
                {
                    CurNode = grub_zalloc(sizeof(var_node));
                    if (CurNode)
                    {
                        grub_snprintf(CurNode->var, sizeof(CurNode->var), "%s", start + 2);
                        grub_snprintf(CurNode->val, sizeof(CurNode->val), "%s", value);
                        CurNode->next = pVarList;
                        pVarList = CurNode;
                    }
                }
            }
            
            VTOY_APPEND_NEWBUF(value);
            
            VTOY_APPEND_NEWBUF(end + 2);
        }
        else
        {
            VTOY_APPEND_NEWBUF(curline);
        }

        if (pos > 0 && newbuf[pos - 1] == '\r')
        {
            newbuf[pos - 1] = '\n';
        }
        else
        {
            newbuf[pos++] = '\n';
        }
    }

    grub_free(node->filebuf);
    node->filebuf = newbuf;
    node->filelen = pos;

    while (pVarList)
    {
        CurNode = pVarList->next;
        grub_free(pVarList);
        pVarList = CurNode;
    }

    return 0;
}

static grub_err_t ventoy_cmd_sel_auto_install(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    int pos = 0;
    int defidx = 1;
    char *buf = NULL;
    grub_file_t file = NULL;
    char configfile[128];
    install_template *node = NULL;
        
    (void)ctxt;
    (void)argc;
    (void)args;

    debug("select auto installation argc:%d\n", argc);

    if (argc < 1)
    {
        return 0;
    }

    node = ventoy_plugin_find_install_template(args[0]);
    if (!node)
    {
        debug("Auto install template not found for %s\n", args[0]);
        return 0;
    }

    if (node->autosel >= 0 && node->autosel <= node->templatenum)
    {
        defidx = node->autosel;
        if (node->timeout < 0)
        {
            node->cursel = node->autosel - 1;
            debug("Auto install template auto select %d\n", node->autosel);
            goto load;
        }
    }

    buf = (char *)grub_malloc(VTOY_MAX_SCRIPT_BUF);
    if (!buf)
    {
        return 0;
    }

    if (node->timeout > 0)
    {
        vtoy_ssprintf(buf, pos, "set timeout=%d\n", node->timeout);        
    }
    
    vtoy_ssprintf(buf, pos, "menuentry \"$VTLANG_NO_AUTOINS_SCRIPT\" --class=\"sel_auto_install\" {\n"
                  "  echo %s\n}\n", "");

    for (i = 0; i < node->templatenum; i++)
    {
        vtoy_ssprintf(buf, pos, "menuentry \"%s %s\" --class=\"sel_auto_install\" {\n"
                  "  echo \"\"\n}\n",
                  ventoy_get_vmenu_title("VTLANG_AUTOINS_USE"),
                  node->templatepath[i].path);
    }

    g_ventoy_menu_esc = 1;
    g_ventoy_suppress_esc = 1;
    g_ventoy_suppress_esc_default = defidx;
    g_ventoy_secondary_menu_on = 1;
    
    grub_snprintf(configfile, sizeof(configfile), "configfile mem:0x%llx:size:%d", (ulonglong)(ulong)buf, pos);
    grub_script_execute_sourcecode(configfile);
    
    g_ventoy_menu_esc = 0;
    g_ventoy_suppress_esc = 0;
    g_ventoy_suppress_esc_default = 1;
    g_ventoy_secondary_menu_on = 0;

    grub_free(buf);

    node->cursel = g_ventoy_last_entry - 1;

load:
    grub_check_free(node->filebuf);
    node->filelen = 0;

    if (node->cursel >= 0 && node->cursel < node->templatenum)
    {
        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", ventoy_get_env("vtoy_iso_part"), 
            node->templatepath[node->cursel].path);
        if (file)
        {
            node->filebuf = grub_malloc(file->size + 8);
            if (node->filebuf)
            {
                grub_file_read(file, node->filebuf, file->size);
                grub_file_close(file);
                
                grub_memset(node->filebuf + file->size, 0, 8);
                node->filelen = (int)file->size;

                ventoy_auto_install_var_expand(node);
            }
        }
        else
        {
            debug("Failed to open auto install script <%s%s>\n", 
                ventoy_get_env("vtoy_iso_part"), node->templatepath[node->cursel].path);
        }
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_sel_persistence(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    int pos = 0;
    int defidx = 1;
    char *buf = NULL;
    char configfile[128];
    persistence_config *node;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    debug("select persistence argc:%d\n", argc);

    if (argc < 1)
    {
        return 0;
    }

    node = ventoy_plugin_find_persistent(args[0]);
    if (!node)
    {
        debug("Persistence image not found for %s\n", args[0]);
        return 0;
    }

    if (node->autosel >= 0 && node->autosel <= node->backendnum)
    {
        defidx = node->autosel;
        if (node->timeout < 0)
        {
            node->cursel = node->autosel - 1;
            debug("Persistence image auto select %d\n", node->autosel);
            return 0;            
        }
    }

    buf = (char *)grub_malloc(VTOY_MAX_SCRIPT_BUF);
    if (!buf)
    {
        return 0;
    }

    if (node->timeout > 0)
    {
        vtoy_ssprintf(buf, pos, "set timeout=%d\n", node->timeout);        
    }

    vtoy_ssprintf(buf, pos, "menuentry \"$VTLANG_NO_PERSIST\" --class=\"sel_persistence\" {\n"
                  "  echo %s\n}\n", "");
    
    for (i = 0; i < node->backendnum; i++)
    {
        vtoy_ssprintf(buf, pos, "menuentry \"%s %s\" --class=\"sel_persistence\" {\n"
                      "  echo \"\"\n}\n",
                      ventoy_get_vmenu_title("VTLANG_PERSIST_USE"),
                      node->backendpath[i].path);
        
    }

    g_ventoy_menu_esc = 1;
    g_ventoy_suppress_esc = 1;
    g_ventoy_suppress_esc_default = defidx;
    g_ventoy_secondary_menu_on = 1;

    grub_snprintf(configfile, sizeof(configfile), "configfile mem:0x%llx:size:%d", (ulonglong)(ulong)buf, pos);
    grub_script_execute_sourcecode(configfile);
    
    g_ventoy_menu_esc = 0;
    g_ventoy_suppress_esc = 0;
    g_ventoy_suppress_esc_default = 1;
    g_ventoy_secondary_menu_on = 0;

    grub_free(buf);

    node->cursel = g_ventoy_last_entry - 1;

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

static grub_err_t ventoy_cmd_test_block_list(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t i;
    grub_file_t file;
    ventoy_img_chunk_list chunklist;
    
    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Can't open file %s\n", args[0]); 
    }

    /* get image chunk data */
    grub_memset(&chunklist, 0, sizeof(chunklist));
    chunklist.chunk = grub_malloc(sizeof(ventoy_img_chunk) * DEFAULT_CHUNK_NUM);
    if (NULL == chunklist.chunk)
    {
        return grub_error(GRUB_ERR_OUT_OF_MEMORY, "Can't allocate image chunk memoty\n");
    }
    
    chunklist.max_chunk = DEFAULT_CHUNK_NUM;
    chunklist.cur_chunk = 0;

    ventoy_get_block_list(file, &chunklist, 0);
    
    if (0 != ventoy_check_block_list(file, &chunklist, 0))
    {
        grub_printf("########## UNSUPPORTED ###############\n");
    }

    grub_printf("filesystem: <%s> entry number:<%u>\n", file->fs->name, chunklist.cur_chunk);

    for (i = 0; i < chunklist.cur_chunk; i++)
    {
        grub_printf("%llu+%llu,", (ulonglong)chunklist.chunk[i].disk_start_sector,
            (ulonglong)(chunklist.chunk[i].disk_end_sector + 1 - chunklist.chunk[i].disk_start_sector));
    }

    grub_printf("\n==================================\n");

    for (i = 0; i < chunklist.cur_chunk; i++)
    {
        grub_printf("%2u: [%llu %llu] - [%llu %llu]\n", i, 
            (ulonglong)chunklist.chunk[i].img_start_sector,
            (ulonglong)chunklist.chunk[i].img_end_sector,
            (ulonglong)chunklist.chunk[i].disk_start_sector,
            (ulonglong)chunklist.chunk[i].disk_end_sector
            );
    }

    grub_free(chunklist.chunk);
    grub_file_close(file);

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

static grub_err_t ventoy_cmd_get_replace_file_cnt(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char buf[32];
    ventoy_grub_param_file_replace *replace = &(g_grub_param->file_replace);
    
    (void)ctxt;

    if (argc >= 1)
    {
        grub_snprintf(buf, sizeof(buf), "%u", replace->old_name_cnt);
        grub_env_set(args[0], buf);        
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_dump_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc == 0)
    {
        grub_printf("List Mode: CurLen:%d  MaxLen:%u\n", g_list_script_pos, VTOY_MAX_SCRIPT_BUF);
        grub_printf("%s", g_list_script_buf);
    }
    else
    {
        grub_printf("Tree Mode: CurLen:%d  MaxLen:%u\n", g_tree_script_pos, VTOY_MAX_SCRIPT_BUF);
        grub_printf("%s", g_tree_script_buf);        
    }

    return 0;
}

static grub_err_t ventoy_cmd_dump_img_list(grub_extcmd_context_t ctxt, int argc, char **args)
{
    img_info *cur = g_ventoy_img_list;
        
    (void)ctxt;
    (void)argc;
    (void)args;

    while (cur)
    {
        grub_printf("path:<%s> id=%d list_index=%d\n", cur->path, cur->id, cur->plugin_list_index);
        grub_printf("name:<%s>\n\n", cur->name);
        cur = cur->next;
    }

    return 0;
}

static grub_err_t ventoy_cmd_dump_injection(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_plugin_dump_injection();

    return 0;
}

static grub_err_t ventoy_cmd_dump_auto_install(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_plugin_dump_auto_install();

    return 0;
}

static grub_err_t ventoy_cmd_dump_persistence(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_plugin_dump_persistence();

    return 0;
}

static int ventoy_check_mode_by_name(char *filename, const char *suffix)
{
    int i;
    int len1;
    int len2;
    
    len1 = (int)grub_strlen(filename);
    len2 = (int)grub_strlen(suffix);

    if (len1 <= len2)
    {
        return 0;
    }

    for (i = len1 - 1; i >= 0; i--)
    {
        if (filename[i] == '.')
        {
            break;
        }
    }

    if (i < len2 + 1)
    {
        return 0;
    }

    if (filename[i - len2 - 1] != '_')
    {
        return 0;
    }

    if (grub_strncasecmp(filename + (i - len2), suffix, len2) == 0)
    {
        return 1;
    }
    
    return 0;
}

static grub_err_t ventoy_cmd_check_mode(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (argc != 1 && argc != 2)
    {
        return 1;
    }

    if (args[0][0] == '0')
    {
        if (g_ventoy_memdisk_mode)
        {
            return 0;
        }

        if (argc == 2 && ventoy_check_mode_by_name(args[1], "vtmemdisk"))
        {
            return 0;
        }

        return 1;
    }
    else if (args[0][0] == '1')
    {
        return g_ventoy_iso_raw ? 0 : 1;
    }
    else if (args[0][0] == '2')
    {
        return g_ventoy_iso_uefi_drv ? 0 : 1;
    }
    else if (args[0][0] == '3')
    {
        if (g_ventoy_grub2_mode)
        {
            return 0;
        }
    
        if (argc == 2 && ventoy_check_mode_by_name(args[1], "vtgrub2"))
        {
            return 0;
        }

        return 1;
    }
    else if (args[0][0] == '4')
    {
        if (g_ventoy_wimboot_mode)
        {
            return 0;
        }
    
        if (argc == 2 && ventoy_check_mode_by_name(args[1], "vtwimboot"))
        {
            return 0;
        }

        return 1;
    }

    return 1;
}

static grub_err_t ventoy_cmd_dynamic_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    static int configfile_mode = 0;
    char memfile[128] = {0};
    
    (void)ctxt;
    (void)argc;
    (void)args;

    /* 
     * args[0]:  0:normal     1:configfile
     * args[1]:  0:list_buf   1:tree_buf
     */

    if (argc != 2)
    {
        debug("Invalid argc %d\n", argc);
        return 0;
    }

    VTOY_CMD_CHECK(1);

    if (args[0][0] == '0')
    {
        if (args[1][0] == '0')
        {
            grub_script_execute_sourcecode(g_list_script_buf);            
        }
        else
        {
            grub_script_execute_sourcecode(g_tree_script_buf); 
        }
    }
    else
    {
        if (configfile_mode)
        {
            debug("Now already in F3 mode %d\n", configfile_mode);
            return 0;
        }

        if (args[1][0] == '0')
        {
            grub_snprintf(memfile, sizeof(memfile), "configfile mem:0x%llx:size:%d", 
                (ulonglong)(ulong)g_list_script_buf, g_list_script_pos);
        }
        else
        {
             g_ventoy_last_entry = -1;
            grub_snprintf(memfile, sizeof(memfile), "configfile mem:0x%llx:size:%d", 
                (ulonglong)(ulong)g_tree_script_buf, g_tree_script_pos); 
        }

        configfile_mode = 1;
        grub_script_execute_sourcecode(memfile);
        configfile_mode = 0;
    }
    
    return 0;
}

static grub_err_t ventoy_cmd_file_exist_nocase(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_file_t file;

    (void)ctxt;

    if (argc != 1)
    {
        return 1;
    }
    
    g_ventoy_case_insensitive = 1;
    file = grub_file_open(args[0], VENTOY_FILE_TYPE);
    g_ventoy_case_insensitive = 0;

    grub_errno = 0;

    if (file)
    {
        grub_file_close(file);
        return 0;
    }
    return 1;
}

static grub_err_t ventoy_cmd_find_bootable_hdd(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int id = 0;
    int find = 0;
    grub_disk_t disk;
    const char *isopath = NULL;
    char hdname[32];
    ventoy_mbr_head mbr;
    
    (void)ctxt;
    (void)argc;

    if (argc != 1)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s variable\n", cmd_raw_name); 
    }

    isopath = grub_env_get("vtoy_iso_part");
    if (!isopath)
    {
        debug("isopath is null %p\n", isopath);
        return 0;
    }

    debug("isopath is %s\n", isopath);

    for (id = 0; id < 30 && (find == 0); id++)
    {
        grub_snprintf(hdname, sizeof(hdname), "hd%d,", id);
        if (grub_strstr(isopath, hdname))
        {
            debug("skip %s ...\n", hdname);
            continue;
        }

        grub_snprintf(hdname, sizeof(hdname), "hd%d", id);
        
        disk = grub_disk_open(hdname);
        if (!disk)
        {
            debug("%s not exist\n", hdname);
            break;
        }

        grub_memset(&mbr, 0, sizeof(mbr));
        if (0 == grub_disk_read(disk, 0, 0, 512, &mbr))
        {
            if (mbr.Byte55 == 0x55 && mbr.ByteAA == 0xAA)
            {
                if (mbr.PartTbl[0].Active == 0x80 || mbr.PartTbl[1].Active == 0x80 ||
                    mbr.PartTbl[2].Active == 0x80 || mbr.PartTbl[3].Active == 0x80)
                {
                    
                    grub_env_set(args[0], hdname);
                    find = 1;
                }
            }
            debug("%s is %s\n", hdname, find ? "bootable" : "NOT bootable");
        }
        else
        {
            debug("read %s failed\n", hdname);
        }

        grub_disk_close(disk);
    }

    return 0;
}

static grub_err_t ventoy_cmd_read_1st_line(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len = 1024;
    grub_file_t file;
    char *buf = NULL;
        
    (void)ctxt;
    (void)argc;

    if (argc != 2)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s file var \n", cmd_raw_name); 
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        debug("failed to open file %s\n", args[0]);
        return 0;
    }

    buf = grub_malloc(len);
    if (!buf)
    {
        goto end;
    }

    buf[len - 1] = 0;
    grub_file_read(file, buf, len - 1);

    ventoy_get_line(buf);
    ventoy_set_env(args[1], buf);

end:

    grub_check_free(buf);
    grub_file_close(file);
    
    return 0;
}

static int ventoy_img_partition_callback (struct grub_disk *disk, const grub_partition_t partition, void *data)
{
    grub_uint64_t end_max = 0;
    int *pCnt = (int *)data;
    
    (void)disk;

    (*pCnt)++;
    g_part_list_pos += grub_snprintf(g_part_list_buf + g_part_list_pos, VTOY_MAX_SCRIPT_BUF - g_part_list_pos,
        "0 %llu linear /dev/ventoy %llu\n",
        (ulonglong)partition->len, (ulonglong)partition->start);

    end_max = (partition->len + partition->start) * 512;
    if (end_max > g_part_end_max)
    {
        g_part_end_max = end_max;
    }
        
    return 0;
}

static grub_err_t ventoy_cmd_img_part_info(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int cnt = 0;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    char buf[64];
    
    (void)ctxt;

    g_part_list_pos = 0;
    g_part_end_max = 0;
    grub_env_unset("vtoy_img_part_file");

    if (argc != 1)
    {
        return 1;
    }

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        debug("ventoy_cmd_img_part_info failed, %s\n", args[0]);
        goto end;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        debug("grub_device_open failed, %s\n", device_name);
        goto end;        
    }

    grub_partition_iterate(dev->disk, ventoy_img_partition_callback, &cnt);

    grub_snprintf(buf, sizeof(buf), "newc:vtoy_dm_table:mem:0x%llx:size:%d", (ulonglong)(ulong)g_part_list_buf, g_part_list_pos);
    grub_env_set("vtoy_img_part_file", buf);

    grub_snprintf(buf, sizeof(buf), "%d", cnt);
    grub_env_set("vtoy_img_part_cnt", buf);
    
    grub_snprintf(buf, sizeof(buf), "%llu", (ulonglong)g_part_end_max);
    grub_env_set("vtoy_img_max_part_end", buf);

end:

    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);
    
    return 0;
}


static grub_err_t ventoy_cmd_file_strstr(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    grub_file_t file;
    char *buf = NULL;
        
    (void)ctxt;
    (void)argc;

    if (argc != 2)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s file str \n", cmd_raw_name); 
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        debug("failed to open file %s\n", args[0]);
        return 1;
    }

    buf = grub_malloc(file->size + 1);
    if (!buf)
    {
        goto end;
    }

    buf[file->size] = 0;
    grub_file_read(file, buf, file->size);

    if (grub_strstr(buf, args[1]))
    {
        rc = 0;
    }

end:

    grub_check_free(buf);
    grub_file_close(file);
    
    return rc;
}

static grub_err_t ventoy_cmd_parse_volume(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len;
    grub_file_t file;
    char buf[64];
    grub_uint64_t size;
    ventoy_iso9660_vd pvd;
        
    (void)ctxt;
    (void)argc;

    if (argc != 4)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s sysid volid space \n", cmd_raw_name); 
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        debug("failed to open file %s\n", args[0]);
        return 0;
    }

    grub_file_seek(file, 16 * 2048);
    len = (int)grub_file_read(file, &pvd, sizeof(pvd));
    if (len != sizeof(pvd))
    {
        debug("failed to read pvd %d\n", len);
        goto end;
    }

    grub_memset(buf, 0, sizeof(buf));
    grub_memcpy(buf, pvd.sys, sizeof(pvd.sys));
    ventoy_set_env(args[1], buf);

    grub_memset(buf, 0, sizeof(buf));
    grub_memcpy(buf, pvd.vol, sizeof(pvd.vol));
    ventoy_set_env(args[2], buf);

    size = pvd.space;
    size *= 2048;
    grub_snprintf(buf, sizeof(buf), "%llu", (ulonglong)size);
    ventoy_set_env(args[3], buf);
    
end:
    grub_file_close(file);
    
    return 0;
}

static grub_err_t ventoy_cmd_parse_create_date(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len;
    grub_file_t file;
    char buf[64];
    
    (void)ctxt;
    (void)argc;

    if (argc != 2)
    {
        return grub_error(GRUB_ERR_BAD_ARGUMENT, "Usage: %s var \n", cmd_raw_name); 
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        debug("failed to open file %s\n", args[0]);
        return 0;
    }

    grub_memset(buf, 0, sizeof(buf));
    grub_file_seek(file, 16 * 2048 + 813);
    len = (int)grub_file_read(file, buf, 17);
    if (len != 17)
    {
        debug("failed to read create date %d\n", len);
        goto end;
    }

    ventoy_set_env(args[1], buf);

end:
    grub_file_close(file);
    
    return 0;
}

static grub_err_t ventoy_cmd_img_hook_root(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_env_hook_root(1);
    
    return 0;
}

static grub_err_t ventoy_cmd_img_unhook_root(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    ventoy_env_hook_root(0);
    
    return 0;
}

#ifdef GRUB_MACHINE_EFI
static grub_err_t ventoy_cmd_check_secureboot_var(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 1;
    grub_uint8_t *var;
    grub_size_t size;
    grub_efi_guid_t global = GRUB_EFI_GLOBAL_VARIABLE_GUID;

    (void)ctxt;
    (void)argc;
    (void)args;

    var = grub_efi_get_variable("SecureBoot", &global, &size);
    if (var && *var == 1)
    {
        return 0;
    }
    
    return ret;
}
#else
static grub_err_t ventoy_cmd_check_secureboot_var(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;
    return 1;
}
#endif

static grub_err_t ventoy_cmd_img_check_range(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int ret = 1;
    grub_file_t file;
    grub_uint64_t FileSectors = 0;
    ventoy_gpt_info *gpt = NULL;
    ventoy_part_table *pt = NULL;
    grub_uint8_t zeroguid[16] = {0};
    
    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s", args[0]);
    if (!file)
    {
        debug("failed to open file %s\n", args[0]);
        return 1;
    }

    if (file->size % 512)
    {
        debug("unaligned file size: %llu\n", (ulonglong)file->size);
        goto out;
    }

    gpt = grub_zalloc(sizeof(ventoy_gpt_info));
    if (!gpt)
    {
        goto out;
    }

    FileSectors = file->size / 512;

    grub_file_read(file, gpt, sizeof(ventoy_gpt_info));
    if (grub_strncmp(gpt->Head.Signature, "EFI PART", 8) == 0)
    {
        debug("This is EFI partition table\n");

        for (i = 0; i < 128; i++)
        {
            if (grub_memcmp(gpt->PartTbl[i].PartGuid, zeroguid, 16))
            {
                if (FileSectors < gpt->PartTbl[i].LastLBA)
                {
                    debug("out of range: part[%d] LastLBA:%llu FileSectors:%llu\n", i, 
                        (ulonglong)gpt->PartTbl[i].LastLBA, (ulonglong)FileSectors);
                    goto out;
                }
            }
        }
    }
    else
    {
        debug("This is MBR partition table\n");

        for (i = 0; i < 4; i++)
        {
            pt = gpt->MBR.PartTbl + i;
            if (FileSectors < pt->StartSectorId + pt->SectorCount)
            {
                debug("out of range: part[%d] LastLBA:%llu FileSectors:%llu\n", i, 
                       (ulonglong)(pt->StartSectorId + pt->SectorCount), 
                       (ulonglong)FileSectors);
                goto out;
            }
        }
    }
    
    ret = 0;
    
out:
    grub_file_close(file);
    grub_check_free(gpt);
    grub_errno = GRUB_ERR_NONE;
    return ret;
}

static grub_err_t ventoy_cmd_clear_key(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int ret;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    for (i = 0; i < 500; i++)
    {
        ret = grub_getkey_noblock();
        if (ret == GRUB_TERM_NO_KEY)
        {
            break;
        }
    }

    if (i >= 500)
    {
        grub_cls();
        grub_printf("\n\n Still have key input after clear.\n");
        grub_refresh();
        grub_sleep(5);
    }
    
    return 0;
}

static grub_err_t ventoy_cmd_acpi_param(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int buflen;
    int datalen;
    int loclen;
    int img_chunk_num;
    int image_sector_size;
    char cmd[64];
    ventoy_chain_head *chain;
    ventoy_img_chunk *chunk;
    ventoy_os_param *osparam;
    ventoy_image_location *location;
    ventoy_image_disk_region *region;
    struct grub_acpi_table_header *acpi;
    
    (void)ctxt;
    
    if (argc != 2)
    {
        return 1;
    }

    debug("ventoy_cmd_acpi_param %s %s\n", args[0], args[1]);

    chain = (ventoy_chain_head *)(ulong)grub_strtoul(args[0], NULL, 16);
    if (!chain)
    {
        return 1;
    }

    image_sector_size = (int)grub_strtol(args[1], NULL, 10);

    if (grub_memcmp(&g_ventoy_guid, &(chain->os_param.guid), 16))
    {
        debug("Invalid ventoy guid 0x%x\n", chain->os_param.guid.data1);
        return 1;
    }

    img_chunk_num = chain->img_chunk_num;

    loclen = sizeof(ventoy_image_location) + (img_chunk_num - 1) * sizeof(ventoy_image_disk_region);
    datalen = sizeof(ventoy_os_param) + loclen;
    
    buflen = sizeof(struct grub_acpi_table_header) + datalen;
    acpi = grub_zalloc(buflen);
    if (!acpi)
    {
        return 1;
    }
    
    /* Step1: Fill acpi table header */
    grub_memcpy(acpi->signature, "VTOY", 4);
    acpi->length = buflen;
    acpi->revision = 1;
    grub_memcpy(acpi->oemid, "VENTOY", 6);
    grub_memcpy(acpi->oemtable, "OSPARAMS", 8);
    acpi->oemrev = 1;
    acpi->creator_id[0] = 1;
    acpi->creator_rev = 1;

    /* Step2: Fill data */
    osparam = (ventoy_os_param *)(acpi + 1);
    grub_memcpy(osparam, &chain->os_param, sizeof(ventoy_os_param));
    osparam->vtoy_img_location_addr = 0;
    osparam->vtoy_img_location_len  = loclen;
    osparam->chksum = 0;
    osparam->chksum = 0x100 - grub_byte_checksum(osparam, sizeof(ventoy_os_param));

    location = (ventoy_image_location *)(osparam + 1);
    grub_memcpy(&location->guid, &osparam->guid, sizeof(ventoy_guid));
    location->image_sector_size = image_sector_size;
    location->disk_sector_size  = chain->disk_sector_size;
    location->region_count = img_chunk_num;

    region = location->regions;
    chunk = (ventoy_img_chunk *)((char *)chain + chain->img_chunk_offset);
    if (512 == image_sector_size)
    {
        for (i = 0; i < img_chunk_num; i++)
        {
            region->image_sector_count = chunk->disk_end_sector - chunk->disk_start_sector + 1;
            region->image_start_sector = chunk->img_start_sector * 4;
            region->disk_start_sector  = chunk->disk_start_sector;
            region++;
            chunk++;
        }
    }
    else
    {
        for (i = 0; i < img_chunk_num; i++)
        {
            region->image_sector_count = chunk->img_end_sector - chunk->img_start_sector + 1;
            region->image_start_sector = chunk->img_start_sector;
            region->disk_start_sector  = chunk->disk_start_sector;        
            region++;
            chunk++;
        }
    }

    /* Step3: Fill acpi checksum */
    acpi->checksum = 0;
    acpi->checksum = 0x100 - grub_byte_checksum(acpi, acpi->length);

    /* load acpi table */
    grub_snprintf(cmd, sizeof(cmd), "acpi mem:0x%lx:size:%d", (ulong)acpi, acpi->length);
    grub_script_execute_sourcecode(cmd);

    grub_free(acpi);
    
    VENTOY_CMD_RETURN(0);
}

static grub_err_t ventoy_cmd_push_last_entry(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_ventoy_last_entry_back = g_ventoy_last_entry;
    g_ventoy_last_entry = -1;
    
    return 0;
}

static grub_err_t ventoy_cmd_pop_last_entry(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_ventoy_last_entry = g_ventoy_last_entry_back;
    
    return 0;
}

static int ventoy_lib_module_callback(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    const char *pos = filename + 1;

    if (info->dir)
    {
        while (*pos)
        {
            if (*pos == '.')
            {
                if ((*(pos - 1) >= '0' && *(pos - 1) <= '9') && (*(pos + 1) >= '0' && *(pos + 1) <= '9'))
                {
                    grub_strncpy((char *)data, filename, 128);
                    return 1;
                }
            }
            pos++;
        }
    }

    return 0;
}

static grub_err_t ventoy_cmd_lib_module_ver(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    grub_fs_t fs = NULL;
    char buf[128] = {0};
    
    (void)ctxt;

    if (argc != 3)
    {
        debug("ventoy_cmd_lib_module_ver, invalid param num %d\n", argc);
        return 1;
    }

    debug("ventoy_cmd_lib_module_ver %s %s %s\n", args[0], args[1], args[2]);

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        debug("grub_file_get_device_name failed, %s\n", args[0]);
        goto end;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        debug("grub_device_open failed, %s\n", device_name);
        goto end;        
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        debug("grub_fs_probe failed, %s\n", device_name);
        goto end;
    }

    fs->fs_dir(dev, args[1], ventoy_lib_module_callback, buf);

    if (buf[0])
    {
        ventoy_set_env(args[2], buf);        
    }
    
    rc = 0;
    
end:

    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);
    
    return rc;
}

int ventoy_load_part_table(const char *diskname)
{
    char name[64];
    int ret;
    grub_disk_t disk;
    grub_device_t dev;
    
    g_ventoy_part_info = grub_zalloc(sizeof(ventoy_gpt_info));
    if (!g_ventoy_part_info)
    {
        return 1;
    }

    disk = grub_disk_open(diskname);
    if (!disk)
    {
        debug("Failed to open disk %s\n", diskname);
        return 1;
    }

    g_ventoy_disk_size = disk->total_sectors * (1U << disk->log_sector_size);

    g_ventoy_disk_bios_id = disk->id;

    grub_disk_read(disk, 0, 0, sizeof(ventoy_gpt_info), g_ventoy_part_info);
    grub_disk_close(disk);

    grub_snprintf(name, sizeof(name), "%s,1", diskname);
    dev = grub_device_open(name);
    if (dev)
    {
        /* Check for official Ventoy device */
        ret = ventoy_check_official_device(dev);
        grub_device_close(dev);

        if (ret)
        {
            return 1;
        }
    }

    g_ventoy_disk_part_size[0] = ventoy_get_vtoy_partsize(0);
    g_ventoy_disk_part_size[1] = ventoy_get_vtoy_partsize(1);

    return 0;
}

static void ventoy_prompt_end(void)
{
    int op = 0;
    char c;

    grub_printf("\n\n\n");
    grub_printf(" 1 --- Exit grub\n");
    grub_printf(" 2 --- Reboot\n");
    grub_printf(" 3 --- Shut down\n");
    grub_printf("Please enter your choice: ");
    grub_refresh();

    while (1)
    {
        c = grub_getkey();
        if (c >= '1' && c <= '3')
        {
            if (op == 0)
            {
                op = c - '0';
                grub_printf("%c", c);
                grub_refresh();
            }
        }
        else if (c == '\r' || c == '\n')
        {
            if (op)
            {
                if (op == 1)
                {
                    grub_exit();
                }
                else if (op == 2)
                {
                    grub_reboot();
                }
                else if (op == 3)
                {   
                    grub_script_execute_sourcecode("halt");
                }
            }
        }
        else if (c == '\b')
        {
            if (op)
            {
                op = 0;
                grub_printf("\rPlease enter your choice:   ");
                grub_printf("\rPlease enter your choice: ");
                grub_refresh();
            }
        }
    }
}

static grub_err_t ventoy_cmd_load_part_table(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret;
    
    (void)argc;
    (void)ctxt;

    ret = ventoy_load_part_table(args[0]);
    if (ret)
    {
        ventoy_prompt_end();
    }

    g_ventoy_disk_part_size[0] = ventoy_get_vtoy_partsize(0);
    g_ventoy_disk_part_size[1] = ventoy_get_vtoy_partsize(1);

    return 0;
}

static grub_err_t ventoy_cmd_check_custom_boot(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 1;
    const char *vcfg = NULL;

    (void)argc;
    (void)ctxt;

    vcfg = ventoy_plugin_get_custom_boot(args[0]);
    if (vcfg)
    {
        debug("custom boot <%s>:<%s>\n", args[0], vcfg);
        grub_env_set(args[1], vcfg);
        ret = 0;
    }
    else
    {
        debug("custom boot <%s>:<NOT FOUND>\n", args[0]);
    }

    grub_errno = 0;
    return ret;
}


static grub_err_t ventoy_cmd_part_exist(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int id;
    grub_uint8_t zeroguid[16] = {0};

    (void)argc;
    (void)ctxt;

    id = (int)grub_strtoul(args[0], NULL, 10);
    grub_errno = 0;
    
    if (grub_memcmp(g_ventoy_part_info->Head.Signature, "EFI PART", 8) == 0)
    {
        if (id >= 1 && id <= 128)
        {
            if (grub_memcmp(g_ventoy_part_info->PartTbl[id - 1].PartGuid, zeroguid, 16))
            {
                return 0;
            }
        }        
    }
    else
    {
        if (id >= 1 && id <= 4)
        {
            if (g_ventoy_part_info->MBR.PartTbl[id - 1].FsFlag)
            {
                return 0;
            }
        }
    }

    return 1;
}

static grub_err_t ventoy_cmd_get_fs_label(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int rc = 1;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    grub_fs_t fs = NULL;
    char *label = NULL;
    
    (void)ctxt;

    debug("get fs label for %s\n", args[0]);

    if (argc != 2)
    {
        debug("ventoy_cmd_get_fs_label, invalid param num %d\n", argc);
        return 1;
    }

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        debug("grub_file_get_device_name failed, %s\n", args[0]);
        goto end;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        debug("grub_device_open failed, %s\n", device_name);
        goto end;        
    }

    fs = grub_fs_probe(dev);
    if (NULL == fs || NULL == fs->fs_label)
    {
        debug("grub_fs_probe failed, %s %p %p\n", device_name, fs, fs->fs_label);
        goto end;
    }

    fs->fs_label(dev, &label);
    if (label)
    {
        debug("label=<%s>\n", label);
        ventoy_set_env(args[1], label);
        grub_free(label);
    }

    rc = 0;
    
end:

    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);
    
    return rc;
}

static int ventoy_fs_enum_1st_file(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    if (!info->dir)
    {
        grub_snprintf((char *)data, 256, "%s", filename);
        return 1;
    }

    return 0;
}

static int ventoy_fs_enum_1st_dir(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    if (info->dir && filename && filename[0] != '.')
    {
        grub_snprintf((char *)data, 256, "%s", filename);
        return 1;
    }

    return 0;
}

static grub_err_t ventoy_fs_enum_1st_child(int argc, char **args, grub_fs_dir_hook_t hook)
{
    int rc = 1;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    grub_fs_t fs = NULL;
    char name[256] ={0};
    
    if (argc != 3)
    {
        debug("ventoy_fs_enum_1st_child, invalid param num %d\n", argc);
        return 1;
    }

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        debug("grub_file_get_device_name failed, %s\n", args[0]);
        goto end;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        debug("grub_device_open failed, %s\n", device_name);
        goto end;        
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        debug("grub_fs_probe failed, %s\n", device_name);
        goto end;
    }

    fs->fs_dir(dev, args[1], hook, name);
    if (name[0])
    {
        ventoy_set_env(args[2], name);
    }
    
    rc = 0;
    
end:

    check_free(device_name, grub_free);
    check_free(dev, grub_device_close);
    
    return rc;
}

static grub_err_t ventoy_cmd_fs_enum_1st_file(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    return ventoy_fs_enum_1st_child(argc, args, ventoy_fs_enum_1st_file);
}

static grub_err_t ventoy_cmd_fs_enum_1st_dir(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    return ventoy_fs_enum_1st_child(argc, args, ventoy_fs_enum_1st_dir);
}

static grub_err_t ventoy_cmd_basename(grub_extcmd_context_t ctxt, int argc, char **args)
{
    char c;
    char *pos = NULL;
    char *end = NULL;
    
    (void)ctxt;

    if (argc != 2)
    {
        debug("ventoy_cmd_basename, invalid param num %d\n", argc);
        return 1;
    }

    for (pos = args[0]; *pos; pos++)
    {
        if (*pos == '.')
        {
            end = pos;
        }
    }

    if (end)
    {
        c = *end;
        *end = 0;
    }

    grub_env_set(args[1], args[0]);

    if (end)
    {
        *end = c;
    }

    return 0;
}

static grub_err_t ventoy_cmd_basefile(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    int len;
    const char *buf;
    
    (void)ctxt;

    if (argc != 2)
    {
        debug("ventoy_cmd_basefile, invalid param num %d\n", argc);
        return 1;
    }

    buf = args[0];
    len = (int)grub_strlen(buf);
    for (i = len; i > 0; i--)
    {
        if (buf[i - 1] == '/')
        {
            grub_env_set(args[1], buf + i);
            return 0;
        }
    }

    grub_env_set(args[1], buf);

    return 0;
}

static grub_err_t ventoy_cmd_enum_video_mode(grub_extcmd_context_t ctxt, int argc, char **args)
{
    struct grub_video_mode_info info;
    char buf[32];
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (!g_video_mode_list)
    {
        ventoy_enum_video_mode();
    }

    if (grub_video_get_info(&info) == GRUB_ERR_NONE)
    {
        grub_snprintf(buf, sizeof(buf), "Resolution (%ux%u)", info.width, info.height);
    }
    else
    {
        grub_snprintf(buf, sizeof(buf), "Resolution (0x0)");
    }

    grub_env_set("VTOY_CUR_VIDEO_MODE", buf);

    grub_snprintf(buf, sizeof(buf), "%d", g_video_mode_num);
    grub_env_set("VTOY_VIDEO_MODE_NUM", buf);

    VENTOY_CMD_RETURN(0);
}

static grub_err_t vt_cmd_update_cur_video_mode(grub_extcmd_context_t ctxt, int argc, char **args)
{
    struct grub_video_mode_info info;
    char buf[32];
    
    (void)ctxt;
    (void)argc;
    (void)args;

    if (grub_video_get_info(&info) == GRUB_ERR_NONE)
    {
        grub_snprintf(buf, sizeof(buf), "%ux%ux%u", info.width, info.height, info.bpp);
    }
    else
    {
        grub_snprintf(buf, sizeof(buf), "0x0x0");
    }

    grub_env_set(args[0], buf);

    VENTOY_CMD_RETURN(0);
}

static grub_err_t ventoy_cmd_get_video_mode(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int id;
    char buf[32];
    
    (void)ctxt;
    (void)argc;

    if (!g_video_mode_list)
    {
        return 0;
    }

    id = (int)grub_strtoul(args[0], NULL, 10);
    if (id < g_video_mode_num)
    {
        grub_snprintf(buf, sizeof(buf), "%ux%ux%u", 
            g_video_mode_list[id].width, g_video_mode_list[id].height, g_video_mode_list[id].bpp);
    }

    grub_env_set(args[1], buf);

    VENTOY_CMD_RETURN(0);
}

static grub_err_t ventoy_cmd_get_efivdisk_offset(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t i;
    grub_uint32_t loadsector = 0;
    grub_file_t file;
    char value[32];
    grub_uint32_t boot_catlog = 0;
    grub_uint8_t buf[512];
    
    (void)ctxt;

    if (argc != 2)
    {
        debug("ventoy_cmd_get_efivdisk_offset, invalid param num %d\n", argc);
        return 1;
    }

    file = grub_file_open(args[0], VENTOY_FILE_TYPE);
    if (!file)
    {
        debug("failed to open %s\n", args[0]);
        return 1;
    }

    boot_catlog = ventoy_get_iso_boot_catlog(file);
    if (boot_catlog == 0)
    {
        debug("No bootcatlog found\n");
        grub_file_close(file);
        return 1;
    }

    grub_memset(buf, 0, sizeof(buf));
    grub_file_seek(file, boot_catlog * 2048);
    grub_file_read(file, buf, sizeof(buf));
    grub_file_close(file);

    for (i = 0; i < sizeof(buf); i += 32)
    {
        if ((buf[i] == 0 || buf[i] == 0x90 || buf[i] == 0x91) && buf[i + 1] == 0xEF)
        {
            if (buf[i + 32] == 0x88)
            {
                loadsector = *(grub_uint32_t *)(buf + i + 32 + 8);
                grub_snprintf(value, sizeof(value), "%u", loadsector * 4); //change to sector size 512
                break;
            }
        }
    }

    if (loadsector == 0)
    {
        debug("No EFI eltorito info found\n");
        return 1;
    }

    debug("ventoy_cmd_get_efivdisk_offset <%s>\n", value);
    grub_env_set(args[1], value);
    VENTOY_CMD_RETURN(0);
}

static int ventoy_collect_replace_initrd(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    int curpos;
    int printlen;
    grub_size_t len;
    replace_fs_dir *pfsdir = (replace_fs_dir *)data;

    if (pfsdir->initrd[0])
    {
        return 1;
    }

    curpos = pfsdir->curpos;
    len = grub_strlen(filename);
    
    if (info->dir)
    {
        if ((len == 1 && filename[0] == '.') ||
            (len == 2 && filename[0] == '.' && filename[1] == '.'))
        {
            return 0;
        }

        //debug("#### [DIR] <%s> <%s>\n", pfsdir->fullpath, filename);
        pfsdir->dircnt++;
        
        printlen = grub_snprintf(pfsdir->fullpath + curpos, 512 - curpos, "%s/", filename);
        pfsdir->curpos = curpos + printlen;
        pfsdir->fs->fs_dir(pfsdir->dev, pfsdir->fullpath, ventoy_collect_replace_initrd, pfsdir);
        pfsdir->curpos = curpos;
        pfsdir->fullpath[curpos] = 0;
    }
    else
    {
        //debug("#### [FILE] <%s> <%s>\n", pfsdir->fullpath, filename);
        pfsdir->filecnt++;
        
        /* We consider the xxx.img file bigger than 32MB is the initramfs file */
        if (len > 4 && grub_strncmp(filename + len - 4, ".img", 4) == 0)
        {
            if (info->size > 32 * VTOY_SIZE_1MB)
            {
                grub_snprintf(pfsdir->initrd, sizeof(pfsdir->initrd), "%s%s", pfsdir->fullpath, filename);
                return 1;
            }
        }
    }

    return 0;
}

static grub_err_t ventoy_cmd_search_replace_initrd(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i;
    char *pos = NULL;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    grub_fs_t fs = NULL;
    replace_fs_dir *pfsdir = NULL;
    
    (void)ctxt;

    if (argc != 2)
    {
        debug("ventoy_cmd_search_replace_initrd, invalid param num %d\n", argc);
        return 1;
    }

    pfsdir = grub_zalloc(sizeof(replace_fs_dir));
    if (!pfsdir)
    {
        return 1;
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

    pfsdir->dev = dev;
    pfsdir->fs = fs;
    pfsdir->curpos = 1;
    pfsdir->fullpath[0] = '/';
    fs->fs_dir(dev, "/", ventoy_collect_replace_initrd, pfsdir);

    if (pfsdir->initrd[0])
    {
        debug("Replace initrd <%s> <%d %d>\n", pfsdir->initrd, pfsdir->dircnt, pfsdir->filecnt);

        for (i = 0; i < (int)sizeof(pfsdir->initrd) && pfsdir->initrd[i]; i++)
        {
            if (pfsdir->initrd[i] == '/')
            {
                pfsdir->initrd[i] = '\\';
            }
        }

        pos = (pfsdir->initrd[0] == '\\') ? pfsdir->initrd + 1 : pfsdir->initrd;
        grub_env_set(args[1], pos);
    }
    else
    {
        debug("Replace initrd NOT found <%s> <%d %d>\n", args[0], pfsdir->dircnt, pfsdir->filecnt);
    }

fail:
    
    grub_check_free(pfsdir);
    grub_check_free(device_name);
    check_free(dev, grub_device_close);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_push_pager(grub_extcmd_context_t ctxt, int argc, char **args)
{
    const char *pager = NULL;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    pager = grub_env_get("pager");
    if (NULL == pager)
    {
        g_pager_flag = 1;
        grub_env_set("pager", "1");
    }
    else if (pager[0] == '1')
    {
        g_pager_flag = 0;
    }
    else
    {
        grub_snprintf(g_old_pager, sizeof(g_old_pager), "%s", pager);
        g_pager_flag = 2;
        grub_env_set("pager", "1");
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_pop_pager(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (g_pager_flag == 1)
    {
        grub_env_unset("pager");
    }
    else if (g_pager_flag == 2)
    {
        grub_env_set("pager", g_old_pager);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static int ventoy_chk_case_file(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    if (g_json_case_mis_path[0])
    {
        return 1;
    }

    if (0 == info->dir && grub_strcasecmp(filename, "ventoy.json") == 0)
    {
        grub_snprintf(g_json_case_mis_path, 32, "%s/%s", (char *)data, filename);
        return 1;
    }
    return 0;
}

static int ventoy_chk_case_dir(const char *filename, const struct grub_dirhook_info *info, void *data)
{
    char path[16];
    chk_case_fs_dir *fs_dir = (chk_case_fs_dir *)data;

    if (g_json_case_mis_path[0])
    {
        return 1;
    }

    if (info->dir && (filename[0] == 'v' || filename[0] == 'V'))
    {
        if (grub_strcasecmp(filename, "ventoy") == 0)
        {
            grub_snprintf(path, sizeof(path), "/%s", filename);
            fs_dir->fs->fs_dir(fs_dir->dev, path, ventoy_chk_case_file, path);
            if (g_json_case_mis_path[0])
            {
                return 1;
            }
        }
    }
    
    return 0;
}

static grub_err_t ventoy_cmd_chk_json_pathcase(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int fstype = 0;
    char *device_name = NULL;
    grub_device_t dev = NULL;
    grub_fs_t fs = NULL;
    chk_case_fs_dir fs_dir;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    device_name = grub_file_get_device_name(args[0]);
    if (!device_name)
    {
        goto out;
    }

    dev = grub_device_open(device_name);
    if (!dev)
    {
        goto out;
    }

    fs = grub_fs_probe(dev);
    if (!fs)
    {
        goto out;
    }

    fstype = ventoy_get_fs_type(fs->name);
    if (fstype == ventoy_fs_fat || fstype == ventoy_fs_exfat || fstype >= ventoy_fs_max)
    {
        goto out;
    }

    g_json_case_mis_path[0] = 0;
    fs_dir.dev = dev;
    fs_dir.fs = fs;
    fs->fs_dir(dev, "/", ventoy_chk_case_dir, &fs_dir);

    if (g_json_case_mis_path[0])
    {
        grub_env_set("VTOY_PLUGIN_PATH_CASE_MISMATCH", g_json_case_mis_path);
    }

out:

    grub_check_free(device_name);
    check_free(dev, grub_device_close);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t grub_cmd_gptpriority(grub_extcmd_context_t ctxt, int argc, char **args)
{
  grub_disk_t disk;
  grub_partition_t part;
  char priority_str[3]; /* Maximum value 15 */

  (void)ctxt;

  if (argc < 2 || argc > 3)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
                       "gptpriority DISKNAME PARTITIONNUM [VARNAME]");

  /* Open the disk if it exists */
  disk = grub_disk_open (args[0]);
  if (!disk)
    {
      return grub_error (GRUB_ERR_BAD_ARGUMENT,
                         "Not a disk");
    }

  part = grub_partition_probe (disk, args[1]);
  if (!part)
    {
      grub_disk_close (disk);
      return grub_error (GRUB_ERR_BAD_ARGUMENT,
                         "No such partition");
    }

  if (grub_strcmp (part->partmap->name, "gpt"))
    {
      grub_disk_close (disk);
      return grub_error (GRUB_ERR_BAD_PART_TABLE,
                         "Not a GPT partition");
    }

  grub_snprintf (priority_str, sizeof(priority_str), "%u",
                 (grub_uint32_t)((part->gpt_attrib >> 48) & 0xfULL));

  if (argc == 3)
    {
      grub_env_set (args[2], priority_str);
      grub_env_export (args[2]);
    }
  else
    {
      grub_printf ("Priority is %s\n", priority_str);
    }

  grub_disk_close (disk);
  return GRUB_ERR_NONE;
}


static grub_err_t grub_cmd_syslinux_nojoliet(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 1;
    int joliet = 0;
    grub_file_t file = NULL;
    grub_uint32_t loadrba = 0;
    grub_uint32_t boot_catlog = 0;
    grub_uint8_t sector[512];
    boot_info_table *info = NULL;
    
    (void)ctxt;
    (void)argc;

    /* This also trigger a iso9660 fs parse */
    if (ventoy_check_file_exist("(loop)/isolinux/isolinux.cfg"))
    {
        return 0;
    }

    joliet = grub_iso9660_is_joliet();
    if (joliet == 0)
    {
        return 1;
    }

    file = grub_file_open(args[0], VENTOY_FILE_TYPE);
    if (!file)
    {
        debug("failed to open %s\n", args[0]);
        return 1;
    }

    boot_catlog = ventoy_get_iso_boot_catlog(file);
    if (boot_catlog == 0)
    {
        debug("no bootcatlog found %u\n", boot_catlog);
        goto out;
    }
    
    loadrba = ventoy_get_bios_eltorito_rba(file, boot_catlog);
    if (loadrba == 0)
    {
        debug("no bios eltorito rba found %u\n", loadrba);
        goto out;
    }

    grub_file_seek(file, loadrba * 2048);
    grub_file_read(file, sector, 512);

    info = (boot_info_table *)sector;
    if (info->bi_data0 == 0x7c6ceafa && 
        info->bi_data1 == 0x90900000 && 
        info->bi_PrimaryVolumeDescriptor == 16 && 
        info->bi_BootFileLocation == loadrba)
    {
        debug("bootloader is syslinux, %u.\n", loadrba);
        ret = 0;
    }

out:

    grub_file_close(file);
    grub_errno = GRUB_ERR_NONE;
    return ret;
}

static grub_err_t grub_cmd_vlnk_dump_part(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int n = 0;
    ventoy_vlnk_part *node;
    
    (void)ctxt;
    (void)argc;
    (void)args;

    for (node = g_vlnk_part_list; node; node = node->next)
    {
        grub_printf("[%d] %s  disksig:%08x  offset:%llu  fs:%s\n", 
                    ++n, node->device, node->disksig, 
                    (ulonglong)node->partoffset, (node->fs ? node->fs->name : "N/A"));
    }

    return 0;
}

static grub_err_t grub_cmd_is_vlnk_name(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int len = 0;
    
    (void)ctxt;

    if (argc == 1)
    {
        len = (int)grub_strlen(args[0]);
        if (grub_file_is_vlnk_suffix(args[0], len))
        {
            return 0;
        }        
    }

    return 1;
}

static grub_err_t grub_cmd_get_vlnk_dst(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int vlnk = 0;
    const char *name = NULL;
    
    (void)ctxt;

    if (argc == 2)
    {
        grub_env_unset(args[1]);
        name = grub_file_get_vlnk(args[0], &vlnk);
        if (vlnk)
        {
            debug("VLNK SRC: <%s>\n", args[0]);
            debug("VLNK DST: <%s>\n", name);
            grub_env_set(args[1], name);
            return 0;
        }
    }

    return 1;
}

static grub_err_t grub_cmd_check_vlnk(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 1;
    int len = 0;
    grub_file_t file = NULL;
    ventoy_vlnk vlnk;
    char dst[512];
    
    (void)ctxt;

    if (argc != 1)
    {
        goto out;
    }

    len = (int)grub_strlen(args[0]);
    if (!grub_file_is_vlnk_suffix(args[0], len))
    {
        grub_printf("Invalid vlnk suffix\n");
        goto out;
    }

    file = grub_file_open(args[0], VENTOY_FILE_TYPE | GRUB_FILE_TYPE_NO_VLNK);
    if (!file)
    {
        grub_printf("Failed to open %s\n", args[0]);
        goto out;
    }

    if (file->size != 32768)
    {
        grub_printf("Invalid vlnk file (size=%llu).\n", (ulonglong)file->size);
        goto out;
    }

    grub_memset(&vlnk, 0, sizeof(vlnk));
    grub_file_read(file, &vlnk, sizeof(vlnk));

    ret = ventoy_check_vlnk_data(&vlnk, 1, dst, sizeof(dst));

out:

    grub_refresh();
    check_free(file, grub_file_close);
    grub_errno = GRUB_ERR_NONE;
    return ret;
}

static grub_err_t ventoy_iso_vd_id_clear(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    g_iso_vd_id_publisher[0] = 0;
    g_iso_vd_id_prepare[0] = 0;
    g_iso_vd_id_application[0] = 0;

    return 0;
}

static grub_err_t ventoy_cmd_iso_vd_id_parse(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 1;
    int offset = 318;
    grub_file_t file = NULL;
    
    (void)ctxt;
    (void)argc;

    file = grub_file_open(args[0], VENTOY_FILE_TYPE);
    if (!file)
    {
        grub_printf("Failed to open %s\n", args[0]);
        goto out;
    }

    grub_file_seek(file, 16 * 2048 + offset);
    grub_file_read(file, g_iso_vd_id_publisher, 128);

    offset += 128;
    grub_file_seek(file, 16 * 2048 + offset);
    grub_file_read(file, g_iso_vd_id_prepare, 128);

    offset += 128;
    grub_file_seek(file, 16 * 2048 + offset);
    grub_file_read(file, g_iso_vd_id_application, 128);

out:

    check_free(file, grub_file_close);
    grub_errno = GRUB_ERR_NONE;
    return ret;
}

static grub_err_t ventoy_cmd_iso_vd_id_begin(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 1;
    char *id = g_iso_vd_id_publisher;
    
    (void)ctxt;
    (void)argc;

    if (args[0][0] == '1')
    {
        id = g_iso_vd_id_prepare;
    }
    else if (args[0][0] == '2')
    {
        id = g_iso_vd_id_application;
    }

    if (args[1][0] == '0' && grub_strncasecmp(id, args[2], grub_strlen(args[2])) == 0)
    {
        ret = 0;
    }
    
    if (args[1][0] == '1' && grub_strncmp(id, args[2], grub_strlen(args[2])) == 0)
    {
        ret = 0;
    }

    grub_errno = GRUB_ERR_NONE;
    return ret;
}

static grub_err_t ventoy_cmd_fn_mutex_lock(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;

    g_ventoy_fn_mutex = 0;
    if (argc == 1 && args[0][0] == '1' && args[0][1] == 0)
    {
        g_ventoy_fn_mutex = 1;
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_dump_rsv_page(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint64_t total;
    grub_uint64_t org_required;
    grub_uint64_t new_required;
    
    (void)ctxt;
    (void)argc;
    (void)args;

#ifdef GRUB_MACHINE_EFI
    grub_efi_get_reserved_page_num(&total, &org_required, &new_required);
    grub_printf("Total pages: %llu\n", (unsigned long long)total);
    grub_printf("OrgReq pages: %llu\n", (unsigned long long)org_required);
    grub_printf("NewReq pages: %llu\n", (unsigned long long)new_required);
#else
    (void)total;
    (void)org_required;
    (void)new_required;
    grub_printf("Non EFI mode!\n");
#endif

    grub_refresh();

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_need_secondary_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    const char *env = NULL;
    
    (void)ctxt;
    (void)argc;

    if (g_ventoy_memdisk_mode || g_ventoy_grub2_mode || g_ventoy_wimboot_mode || g_ventoy_iso_raw)
    {
        return 1;
    }

    if (ventoy_check_mode_by_name(args[0], "vtgrub2") ||
        ventoy_check_mode_by_name(args[0], "vtwimboot") ||
        ventoy_check_mode_by_name(args[0], "vtmemdisk") ||
        ventoy_check_mode_by_name(args[0], "vtnormal")
        )
    {
        return 1;
    }

    env = grub_env_get("VTOY_SECONDARY_BOOT_MENU");
    if (env && env[0] == '0' && env[1] == 0)
    {
        return 1;
    }

    return 0;
}

static grub_err_t ventoy_cmd_show_secondary_menu(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int n = 0;
    int pos = 0;
    int len = 0;
    int select = 0;
    int timeout = 0;
    char *cmd = NULL;
    const char *env = NULL;
    ulonglong fsize = 0;
    char cfgfile[128];
    int seldata[16] = {0};

    (void)ctxt;
    (void)argc;

    len = 8 * VTOY_SIZE_1KB;
    cmd = (char *)grub_malloc(len);
    if (!cmd)
    {
        return 1;
    }

    g_vtoy_secondary_need_recover = 0;
    grub_env_unset("VTOY_SECOND_EXIT");
    grub_env_unset("VTOY_CHKSUM_FILE_PATH");

    env = grub_env_get("VTOY_SECONDARY_TIMEOUT");
    if (env)
    {
        timeout = (int)grub_strtol(env, NULL, 10);
    }

    if (timeout > 0)
    {
        vtoy_len_ssprintf(cmd, pos, len, "set timeout=%d\n", timeout);
    }

    fsize = grub_strtoull(args[2], NULL, 10);

    vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_NORMAL_MODE", "second_normal"); seldata[n++] = 1;

    if (grub_strcmp(args[1], "Unix") != 0)
    {
        if (grub_strcmp(args[1], "Windows") == 0)
        {
            vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_WIMBOOT_MODE", "second_wimboot"); seldata[n++] = 2;
        }
        else
        {
            vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_GRUB2_MODE", "second_grub2"); seldata[n++] = 3;
        }

        if (fsize <= VTOY_SIZE_1GB)
        {
            vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_MEMDISK_MODE", "second_memdisk"); seldata[n++] = 4;
        }
    }

    vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_FILE_CHKSUM", "second_checksum"); seldata[n++] = 5;
    vtoy_dummy_menuentry(cmd, pos, len, "$VTLANG_RETURN_PRV_NOESC", "second_return"); seldata[n++] = 6;

    do {
        grub_errno = GRUB_ERR_NONE;
        g_ventoy_menu_esc = 1;
        g_ventoy_suppress_esc = 1;
        g_ventoy_suppress_esc_default = 0;
        g_ventoy_secondary_menu_on = 1;
        grub_snprintf(cfgfile, sizeof(cfgfile), "configfile mem:0x%llx:size:%d", (ulonglong)(ulong)cmd, pos);
        grub_script_execute_sourcecode(cfgfile);
        g_ventoy_menu_esc = 0;
        g_ventoy_suppress_esc = 0;
        g_ventoy_suppress_esc_default = 1;
        g_ventoy_secondary_menu_on = 0;

        select = seldata[g_ventoy_last_entry];
        
        if (select == 2)
        {
            g_ventoy_wimboot_mode = 1;
            g_vtoy_secondary_need_recover = 1;
        }
        else if (select == 3)
        {
            g_ventoy_grub2_mode = 1;
            g_vtoy_secondary_need_recover = 2;
        }
        else if (select == 4)
        {
            g_ventoy_memdisk_mode = 1;
            g_vtoy_secondary_need_recover = 3;
        }
        else if (select == 5)
        {
            grub_env_set("VTOY_CHKSUM_FILE_PATH", args[0]);
            grub_script_execute_sourcecode("configfile $vtoy_efi_part/grub/checksum.cfg");
        }
        else if (select == 6)
        {
            grub_env_set("VTOY_SECOND_EXIT", "1");
        }
    }while (select == 5);

    grub_free(cmd);
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_secondary_recover_mode(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;
    (void)args;

    if (g_vtoy_secondary_need_recover == 1)
    {
        g_ventoy_wimboot_mode = 0;
    }
    else if (g_vtoy_secondary_need_recover == 2)
    {
        g_ventoy_grub2_mode = 0;
    }
    else if (g_vtoy_secondary_need_recover == 3)
    {
        g_ventoy_memdisk_mode = 0;
    }

    g_vtoy_secondary_need_recover = 0;
    
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

static grub_err_t ventoy_cmd_fs_ignore_case(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;

    if (args[0][0] == '0')
    {
        g_ventoy_case_insensitive = 0;
    }
    else
    {
        g_ventoy_case_insensitive = 1;
    }

    return 0;
}

static grub_err_t ventoy_cmd_init_menu_lang(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;

    ventoy_plugin_load_menu_lang(1, args[0]);
    VENTOY_CMD_RETURN(0);
}

static grub_err_t ventoy_cmd_load_menu_lang(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;

    ventoy_plugin_load_menu_lang(0, args[0]);
    VENTOY_CMD_RETURN(0);
}

static int ventoy_chksum_pathcmp(int chktype, char *rlpath, char *rdpath)
{
    char *pos1 = NULL;
    char *pos2 = NULL;

    if (chktype == 2)
    {
        pos1 = ventoy_str_basename(rlpath);
        pos2 = ventoy_str_basename(rdpath);
        return grub_strcmp(pos1, pos2);
    }
    else if (chktype == 3 || chktype == 4)
    {
        if (grub_strcmp(rlpath, rdpath) == 0 || grub_strcmp(rlpath + 1, rdpath) == 0)
        {
            return 0;
        }
    }
    
    return 1;
}

static int ventoy_find_checksum
(
    grub_file_t file, 
    const char *uname, 
    int retlen, 
    char *path, 
    int chktype, 
    char *chksum
)
{
    int ulen;
    char *pos = NULL;
    char *pos1 = NULL;
    char *pos2 = NULL;
    char *buf = NULL;
    char *currline = NULL;
    char *nextline = NULL;

    ulen = (int)grub_strlen(uname);

    /* read file to buffer */
    buf = grub_malloc(file->size + 4);
    if (!buf)
    {
        return 1;
    }
    grub_file_read(file, buf, file->size);
    buf[file->size] = 0;

    /* parse each line */
    for (currline = buf; currline; currline = nextline)
    {
        nextline = ventoy_get_line(currline);
        VTOY_SKIP_SPACE(currline);

        if (grub_strncasecmp(currline, uname, ulen) == 0)
        {
            pos = grub_strchr(currline, '=');
            pos1 = grub_strchr(currline, '(');
            pos2 = grub_strchr(currline, ')');
            
            if (pos && pos1 && pos2)
            {
                *pos2 = 0;
                if (ventoy_chksum_pathcmp(chktype, path, pos1 + 1) == 0)
                {
                    VTOY_SKIP_SPACE_NEXT(pos, 1);
                    grub_memcpy(chksum, pos, retlen);
                    goto end;
                }
            }
        }
        else if (ventoy_str_len_alnum(currline, retlen))
        {
            VTOY_SKIP_SPACE_NEXT_EX(pos, currline, retlen);
            if (ventoy_chksum_pathcmp(chktype, path, pos) == 0)
            {
                grub_memcpy(chksum, currline, retlen);
                goto end;                        
            }
        }
    }

end:
    grub_free(buf);
    return 0;
}

static int ventoy_check_chkfile(const char *isopart, char *path, const char *lchkname, grub_file_t *pfile)
{
    int ret = 0;
    int cnt = 0;
    char c = 0;
    char *pos = NULL;
    grub_file_t file = NULL;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s.%s", isopart, path, lchkname);
    if (file)
    {
        VTOY_GOTO_END(1);
    }

    cnt = ventoy_str_chrcnt(path, '/');
    if (cnt > 1)
    {
        pos = grub_strrchr(path, '/');
        c = *pos;
        *pos = 0;

        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s/VENTOY_CHECKSUM", isopart, path);
        if (file)
        {
            *pos = c;
            VTOY_GOTO_END(2);
        }
        *pos = c;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/VENTOY_CHECKSUM", isopart);
    if (file)
    {
        ret = (cnt > 1) ? 3 : 4;
    }

end:

    if (pfile)
    {
        *pfile = file;        
    }
    else
    {
        check_free(file, grub_file_close);
    }
    return ret;
}

static grub_err_t ventoy_cmd_cmp_checksum(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int index = 0;
    int chktype = 0;
    char *pos = NULL;
    grub_file_t file = NULL;
    const char *calc_value = NULL;
    const char *isopart = NULL;
    char fchksum[64];
    char readchk[256] = {0};
    char filebuf[512] = {0};
    char uchkname[16];

    (void)ctxt;

    index = (int)grub_strtol(args[0], NULL, 10);
    if (argc != 2 || index < 0 || index >= VTOY_CHKSUM_NUM)
    {
        return 1;
    }

    grub_strncpy(uchkname, g_lower_chksum_name[index], sizeof(uchkname));
    ventoy_str_toupper(uchkname);
    
    isopart = grub_env_get("vtoy_iso_part");
    calc_value = grub_env_get("VT_LAST_CHECK_SUM");    

    chktype = ventoy_check_chkfile(isopart, args[1], g_lower_chksum_name[index], &file);
    if (chktype <= 0)
    {
        grub_printf("\n\nNo checksum file found.\n");
        goto end;
    }

    if (chktype == 1)
    {
        grub_snprintf(fchksum, sizeof(fchksum), ".%s", g_lower_chksum_name[index]);
        grub_memset(filebuf, 0, sizeof(filebuf));
        grub_file_read(file, filebuf, 511);

        pos = grub_strchr(filebuf, '=');
        if (pos)
        {
            VTOY_SKIP_SPACE_NEXT(pos, 1);
            grub_memcpy(readchk, pos, g_chksum_retlen[index]);
        }
        else
        {
            grub_memcpy(readchk, filebuf, g_chksum_retlen[index]);
        }
    }
    else if (chktype == 3 || chktype == 4)
    {
        grub_snprintf(fchksum, sizeof(fchksum), "global VENTOY_CHECKSUM");
        ventoy_find_checksum(file, uchkname, g_chksum_retlen[index], args[1], chktype, readchk);
        if (readchk[0] == 0)
        {
            grub_printf("\n\n%s value not found in %s.\n", uchkname, fchksum);
            goto end;
        }
    }
    else
    {
        grub_snprintf(fchksum, sizeof(fchksum), "local VENTOY_CHECKSUM");
        ventoy_find_checksum(file, uchkname, g_chksum_retlen[index], args[1], chktype, readchk);
        if (readchk[0] == 0)
        {
            grub_file_close(file);
            file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/VENTOY_CHECKSUM", isopart);
            if (file)
            {
                grub_snprintf(fchksum, sizeof(fchksum), "global VENTOY_CHECKSUM");
                ventoy_find_checksum(file, uchkname, g_chksum_retlen[index], args[1], 3, readchk);
                if (readchk[0] == 0)
                {
                    grub_printf("\n\n%s value not found in both local and global VENTOY_CHECKSUM.\n", uchkname);
                    goto end;
                }
            }
        }
    }

    if (grub_strcasecmp(calc_value, readchk) == 0)
    {
        grub_printf("\n\nCheck %s value with %s file.  [ SUCCESS ]\n", uchkname, fchksum);
    }
    else
    {
        grub_printf("\n\nCheck %s value with %s file.  [ ERROR ]\n", uchkname, fchksum);
        grub_printf("The %s value in %s file is:\n%s\n", uchkname, fchksum, readchk);
    }

end:
    grub_refresh();
    check_free(file, grub_file_close);
    VENTOY_CMD_RETURN(0);
}

static int ventoy_find_all_checksum
(
    grub_file_t file, 
    char *path,
    int chktype,
    int exists[VTOY_CHKSUM_NUM],
    int *ptotexist
)
{
    int i;
    int ulen;
    int tot = 0;
    char c = 0;
    char *pos = NULL;
    char *pos1 = NULL;
    char *pos2 = NULL;
    char *buf = NULL;
    char *currline = NULL;
    char *nextline = NULL;
    const char *uname = NULL;

    tot = *ptotexist;

    /* read file to buffer */
    buf = grub_malloc(file->size + 4);
    if (!buf)
    {
        return 1;
    }
    grub_file_read(file, buf, file->size);
    buf[file->size] = 0;

    /* parse each line */
    for (currline = buf; currline; currline = nextline)
    {
        nextline = ventoy_get_line(currline);
        VTOY_SKIP_SPACE(currline);

        for (i = 0; i < VTOY_CHKSUM_NUM; i++)
        {
            if (exists[i])
            {
                continue;
            }
        
            uname = g_lower_chksum_name[i];
            ulen = g_lower_chksum_namelen[i];

            if (grub_strncasecmp(currline, uname, ulen) == 0)
            {
                pos = grub_strchr(currline, '=');
                pos1 = grub_strchr(currline, '(');
                pos2 = grub_strchr(currline, ')');
                
                if (pos && pos1 && pos2)
                {
                    c = *pos2;
                    *pos2 = 0;
                    if (ventoy_chksum_pathcmp(chktype, path, pos1 + 1) == 0)
                    {
                        exists[i] = 1;
                        tot++;
                    }
                    *pos2 = c;
                }
            }
            else if (ventoy_str_len_alnum(currline, g_chksum_retlen[i]))
            {
                VTOY_SKIP_SPACE_NEXT_EX(pos, currline, g_chksum_retlen[i]);
                if (ventoy_chksum_pathcmp(chktype, path, pos) == 0)
                {
                    exists[i] = 1;
                    tot++;
                }
            }

            if (tot >= VTOY_CHKSUM_NUM)
            {
                goto end;
            }
        }
    }

end:

    *ptotexist = tot;
    grub_free(buf);
    return 0;
}

static grub_err_t ventoy_cmd_vtoychksum_exist(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    int cnt = 0;
    char c = 0;
    int tip = 0;
    char *pos = NULL;
    grub_file_t file = NULL;
    const char *isopart = NULL;
    int exists[VTOY_CHKSUM_NUM] = { 0, 0, 0, 0 };
    int totexist = 0;

    (void)argc;
    (void)ctxt;
    
    isopart = grub_env_get("vtoy_iso_part");

    for (i = 0; i < VTOY_CHKSUM_NUM; i++)
    {
        if (ventoy_check_file_exist("%s%s.%s", isopart, args[0], g_lower_chksum_name[i]))
        {
            exists[i] = 1;
            totexist++;
        }
    }

    if (totexist == VTOY_CHKSUM_NUM)
    {
        goto end;
    }

    cnt = ventoy_str_chrcnt(args[0], '/');
    if (cnt > 1)
    {
        pos = grub_strrchr(args[0], '/');
        c = *pos;
        *pos = 0;
        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s/VENTOY_CHECKSUM", isopart, args[0]);
        *pos = c;
        
        if (file)
        {
            if (tip == 0 && file->size > (32 * VTOY_SIZE_1KB))
            {
                tip = 1;
                grub_printf("Reading checksum file...\n");
                grub_refresh();
            }
        
            debug("parse local VENTOY_CHECKSUM\n");
            ventoy_find_all_checksum(file, args[0], 2, exists, &totexist);
            grub_file_close(file);
        }
    }

    if (totexist == VTOY_CHKSUM_NUM)
    {
        goto end;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/VENTOY_CHECKSUM", isopart);
    if (file)
    {
        if (tip == 0 && file->size > (32 * VTOY_SIZE_1KB))
        {
            tip = 1;
            grub_printf("Reading checksum file...\n");
            grub_refresh();
        }

        debug("parse global VENTOY_CHECKSUM\n");
        ventoy_find_all_checksum(file, args[0], (cnt > 1) ? 3 : 4, exists, &totexist);
        grub_file_close(file);
    }

end:

    ventoy_env_int_set("VT_EXIST_MD5", exists[0]);
    ventoy_env_int_set("VT_EXIST_SHA1", exists[1]);
    ventoy_env_int_set("VT_EXIST_SHA256", exists[2]);
    ventoy_env_int_set("VT_EXIST_SHA512", exists[3]);

    VENTOY_CMD_RETURN(0);
}


static const char * ventoy_menu_lang_read_hook(struct grub_env_var *var, const char *val)
{
    (void)var;
    return ventoy_get_vmenu_title(val);
}

int ventoy_env_init(void)
{
    int i;
    char buf[64];

    grub_env_set("vtdebug_flag", "");

    grub_register_vtoy_menu_lang_hook(ventoy_menu_lang_read_hook);
    ventoy_ctrl_var_init();
    ventoy_global_var_init();

    g_part_list_buf = grub_malloc(VTOY_PART_BUF_LEN);
    g_tree_script_buf = grub_malloc(VTOY_MAX_SCRIPT_BUF);
    g_list_script_buf = grub_malloc(VTOY_MAX_SCRIPT_BUF);
    for (i = 0; i < VTOY_MAX_CONF_REPLACE; i++)
    {
        g_conf_replace_new_buf[i] = grub_malloc(vtoy_max_replace_file_size);         
    }

    ventoy_filt_register(0, ventoy_wrapper_open);

    g_grub_param = (ventoy_grub_param *)grub_zalloc(sizeof(ventoy_grub_param));
    if (g_grub_param)
    {
        g_grub_param->grub_env_get = grub_env_get;
        g_grub_param->grub_env_set = (grub_env_set_pf)grub_env_set;
        g_grub_param->grub_env_printf = (grub_env_printf_pf)grub_printf;
        grub_snprintf(buf, sizeof(buf), "%p", g_grub_param);
        grub_env_set("env_param", buf);
        grub_env_set("ventoy_env_param", buf);

        grub_env_export("env_param");
        grub_env_export("ventoy_env_param");
    }

    grub_env_export("vtoy_winpeshl_ini_addr");
    grub_env_export("vtoy_winpeshl_ini_size");

    grub_snprintf(buf, sizeof(buf), "0x%lx", (ulong)ventoy_chain_file_size);
    grub_env_set("vtoy_chain_file_size", buf);
    grub_env_export("vtoy_chain_file_size");

    grub_snprintf(buf, sizeof(buf), "0x%lx", (ulong)ventoy_chain_file_read);
    grub_env_set("vtoy_chain_file_read", buf);
    grub_env_export("vtoy_chain_file_read");
    
    grub_snprintf(buf, sizeof(buf), "0x%lx", (ulong)ventoy_get_vmenu_title);
    grub_env_set("VTOY_VMENU_FUNC_ADDR", buf);
    grub_env_export("VTOY_VMENU_FUNC_ADDR");

    grub_snprintf(buf, sizeof(buf), "%s-%s", GRUB_TARGET_CPU, GRUB_PLATFORM);
    grub_env_set("grub_cpu_platform", buf);
    grub_env_export("grub_cpu_platform");

    return 0;
}



static cmd_para ventoy_cmds[] = 
{
    { "vt_browser_disk",  ventoy_cmd_browser_disk,  0, NULL, "",   "",    NULL },
    { "vt_browser_dir",  ventoy_cmd_browser_dir,  0, NULL, "",   "",    NULL },
    { "vt_incr",  ventoy_cmd_incr,  0, NULL, "{Var} {INT}",   "Increase integer variable",    NULL },
    { "vt_mod",  ventoy_cmd_mod,  0, NULL, "{Int} {Int} {Var}",   "mod integer variable",    NULL },
    { "vt_strstr",  ventoy_cmd_strstr,  0, NULL, "",   "",    NULL },
    { "vt_str_begin",  ventoy_cmd_strbegin,  0, NULL, "",   "",    NULL },
    { "vt_str_casebegin",  ventoy_cmd_strcasebegin,  0, NULL, "",   "",    NULL },
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
    { "vt_ext_select_img_path", ventoy_cmd_ext_select_img_path, 0, NULL, "{var}", "select chosen img path", NULL },
    { "vt_img_sector", ventoy_cmd_img_sector, 0, NULL, "{imageName}", "", NULL },
    { "vt_dump_img_sector", ventoy_cmd_dump_img_sector, 0, NULL, "", "", NULL },
    { "vt_load_wimboot", ventoy_cmd_load_wimboot, 0, NULL, "", "", NULL },
    { "vt_load_vhdboot", ventoy_cmd_load_vhdboot, 0, NULL, "", "", NULL },
    { "vt_patch_vhdboot", ventoy_cmd_patch_vhdboot, 0, NULL, "", "", NULL },
    { "vt_raw_chain_data", ventoy_cmd_raw_chain_data, 0, NULL, "", "", NULL },
    { "vt_get_vtoy_type", ventoy_cmd_get_vtoy_type, 0, NULL, "", "", NULL },
    { "vt_check_custom_boot", ventoy_cmd_check_custom_boot, 0, NULL, "", "", NULL },
    { "vt_dump_custom_boot", ventoy_cmd_dump_custom_boot, 0, NULL, "", "", NULL },

    { "vt_skip_svd", ventoy_cmd_skip_svd, 0, NULL, "", "", NULL },
    { "vt_cpio_busybox64", ventoy_cmd_cpio_busybox_64, 0, NULL, "", "", NULL },
    { "vt_load_cpio", ventoy_cmd_load_cpio, 0, NULL, "", "", NULL },
    { "vt_trailer_cpio", ventoy_cmd_trailer_cpio, 0, NULL, "", "", NULL },
    { "vt_push_last_entry", ventoy_cmd_push_last_entry, 0, NULL, "", "", NULL },
    { "vt_pop_last_entry", ventoy_cmd_pop_last_entry, 0, NULL, "", "", NULL },
    { "vt_get_lib_module_ver", ventoy_cmd_lib_module_ver, 0, NULL, "", "", NULL },

    { "vt_load_part_table", ventoy_cmd_load_part_table, 0, NULL, "", "", NULL },
    { "vt_check_part_exist", ventoy_cmd_part_exist, 0, NULL, "", "", NULL },
    { "vt_get_fs_label", ventoy_cmd_get_fs_label, 0, NULL, "", "", NULL },
    { "vt_fs_enum_1st_file", ventoy_cmd_fs_enum_1st_file, 0, NULL, "", "", NULL },
    { "vt_fs_enum_1st_dir", ventoy_cmd_fs_enum_1st_dir, 0, NULL, "", "", NULL },
    { "vt_file_basename", ventoy_cmd_basename, 0, NULL, "", "", NULL },    
    { "vt_file_basefile", ventoy_cmd_basefile, 0, NULL, "", "", NULL },    
    { "vt_enum_video_mode", ventoy_cmd_enum_video_mode, 0, NULL, "", "", NULL },    
    { "vt_get_video_mode", ventoy_cmd_get_video_mode, 0, NULL, "", "", NULL },    
    { "vt_update_cur_video_mode", vt_cmd_update_cur_video_mode, 0, NULL, "", "", NULL },    

    
    { "vt_find_first_bootable_hd", ventoy_cmd_find_bootable_hdd, 0, NULL, "", "", NULL },
    { "vt_dump_menu", ventoy_cmd_dump_menu, 0, NULL, "", "", NULL },
    { "vt_dynamic_menu", ventoy_cmd_dynamic_menu, 0, NULL, "", "", NULL },
    { "vt_check_mode", ventoy_cmd_check_mode, 0, NULL, "", "", NULL },
    { "vt_dump_img_list", ventoy_cmd_dump_img_list, 0, NULL, "", "", NULL },
    { "vt_dump_injection", ventoy_cmd_dump_injection, 0, NULL, "", "", NULL },
    { "vt_dump_auto_install", ventoy_cmd_dump_auto_install, 0, NULL, "", "", NULL },
    { "vt_dump_persistence", ventoy_cmd_dump_persistence, 0, NULL, "", "", NULL },
    { "vt_select_auto_install", ventoy_cmd_sel_auto_install, 0, NULL, "", "", NULL },
    { "vt_select_persistence", ventoy_cmd_sel_persistence, 0, NULL, "", "", NULL },
    { "vt_select_conf_replace", ventoy_select_conf_replace, 0, NULL, "", "", NULL },

    { "vt_iso9660_nojoliet", ventoy_cmd_iso9660_nojoliet, 0, NULL, "", "", NULL },
    { "vt_iso9660_isjoliet", ventoy_cmd_iso9660_is_joliet, 0, NULL, "", "", NULL },
    { "vt_is_udf", ventoy_cmd_is_udf, 0, NULL, "", "", NULL },
    { "vt_file_size", ventoy_cmd_file_size, 0, NULL, "", "", NULL },
    { "vt_load_file_to_mem", ventoy_cmd_load_file_to_mem, 0, NULL, "", "", NULL },
    { "vt_load_img_memdisk", ventoy_cmd_load_img_memdisk, 0, NULL, "", "", NULL },
    { "vt_concat_efi_iso", ventoy_cmd_concat_efi_iso, 0, NULL, "", "", NULL },
    
    { "vt_linux_parse_initrd_isolinux", ventoy_cmd_isolinux_initrd_collect, 0, NULL, "{cfgfile}", "", NULL },
    { "vt_linux_parse_initrd_grub", ventoy_cmd_grub_initrd_collect, 0, NULL, "{cfgfile}", "", NULL },
    { "vt_linux_specify_initrd_file", ventoy_cmd_specify_initrd_file, 0, NULL, "", "", NULL },
    { "vt_linux_clear_initrd", ventoy_cmd_clear_initrd_list, 0, NULL, "", "", NULL },
    { "vt_linux_dump_initrd", ventoy_cmd_dump_initrd_list, 0, NULL, "", "", NULL },
    { "vt_linux_initrd_count", ventoy_cmd_initrd_count, 0, NULL, "", "", NULL },
    { "vt_linux_valid_initrd_count", ventoy_cmd_valid_initrd_count, 0, NULL, "", "", NULL },
    { "vt_linux_locate_initrd", ventoy_cmd_linux_locate_initrd, 0, NULL, "", "", NULL },
    { "vt_linux_chain_data", ventoy_cmd_linux_chain_data, 0, NULL, "", "", NULL },
    { "vt_linux_get_main_initrd_index", ventoy_cmd_linux_get_main_initrd_index, 0, NULL, "", "", NULL },

    { "vt_windows_reset",      ventoy_cmd_wimdows_reset, 0, NULL, "", "", NULL },
    { "vt_windows_chain_data", ventoy_cmd_windows_chain_data, 0, NULL, "", "", NULL },
    { "vt_windows_wimboot_data", ventoy_cmd_windows_wimboot_data, 0, NULL, "", "", NULL },
    { "vt_windows_collect_wim_patch", ventoy_cmd_collect_wim_patch, 0, NULL, "", "", NULL },
    { "vt_windows_locate_wim_patch", ventoy_cmd_locate_wim_patch, 0, NULL, "", "", NULL },
    { "vt_windows_count_wim_patch", ventoy_cmd_wim_patch_count, 0, NULL, "", "", NULL },
    { "vt_dump_wim_patch", ventoy_cmd_dump_wim_patch, 0, NULL, "", "", NULL },
    { "vt_wim_check_bootable", ventoy_cmd_wim_check_bootable, 0, NULL, "", "", NULL },
    { "vt_wim_chain_data", ventoy_cmd_wim_chain_data, 0, NULL, "", "", NULL },

    { "vt_add_replace_file", ventoy_cmd_add_replace_file, 0, NULL, "", "", NULL },
    { "vt_get_replace_file_cnt", ventoy_cmd_get_replace_file_cnt, 0, NULL, "", "", NULL },
    { "vt_test_block_list", ventoy_cmd_test_block_list, 0, NULL, "", "", NULL },
    { "vt_file_exist_nocase", ventoy_cmd_file_exist_nocase, 0, NULL, "", "", NULL },

    
    { "vt_load_plugin", ventoy_cmd_load_plugin, 0, NULL, "", "", NULL },
    { "vt_check_plugin_json", ventoy_cmd_plugin_check_json, 0, NULL, "", "", NULL },
    { "vt_check_password", ventoy_cmd_check_password, 0, NULL, "", "", NULL },
    
    { "vt_1st_line", ventoy_cmd_read_1st_line, 0, NULL, "", "", NULL },
    { "vt_file_strstr", ventoy_cmd_file_strstr, 0, NULL, "", "", NULL },
    { "vt_img_part_info", ventoy_cmd_img_part_info, 0, NULL, "", "", NULL },

    
    { "vt_parse_iso_volume", ventoy_cmd_parse_volume, 0, NULL, "", "", NULL },
    { "vt_parse_iso_create_date", ventoy_cmd_parse_create_date, 0, NULL, "", "", NULL },
    { "vt_parse_freenas_ver", ventoy_cmd_parse_freenas_ver, 0, NULL, "", "", NULL },
    { "vt_unix_parse_freebsd_ver", ventoy_cmd_unix_freebsd_ver, 0, NULL, "", "", NULL },
    { "vt_unix_parse_freebsd_ver_elf", ventoy_cmd_unix_freebsd_ver_elf, 0, NULL, "", "", NULL },
    { "vt_unix_reset", ventoy_cmd_unix_reset, 0, NULL, "", "", NULL },
    { "vt_unix_check_vlnk", ventoy_cmd_unix_check_vlnk, 0, NULL, "", "", NULL },
    { "vt_unix_replace_conf", ventoy_cmd_unix_replace_conf, 0, NULL, "", "", NULL },
    { "vt_unix_replace_grub_conf", ventoy_cmd_unix_replace_grub_conf, 0, NULL, "", "", NULL },
    { "vt_unix_replace_ko", ventoy_cmd_unix_replace_ko, 0, NULL, "", "", NULL },
    { "vt_unix_ko_fillmap", ventoy_cmd_unix_ko_fillmap, 0, NULL, "", "", NULL },
    { "vt_unix_fill_image_desc", ventoy_cmd_unix_fill_image_desc, 0, NULL, "", "", NULL },
    { "vt_unix_gzip_new_ko", ventoy_cmd_unix_gzip_newko, 0, NULL, "", "", NULL },
    { "vt_unix_chain_data", ventoy_cmd_unix_chain_data, 0, NULL, "", "", NULL },

    { "vt_img_hook_root", ventoy_cmd_img_hook_root, 0, NULL, "", "", NULL },
    { "vt_img_unhook_root", ventoy_cmd_img_unhook_root, 0, NULL, "", "", NULL },
    { "vt_acpi_param", ventoy_cmd_acpi_param, 0, NULL, "", "", NULL },
    { "vt_check_secureboot_var", ventoy_cmd_check_secureboot_var, 0, NULL, "", "", NULL },
    { "vt_clear_key", ventoy_cmd_clear_key, 0, NULL, "", "", NULL },
    { "vt_img_check_range", ventoy_cmd_img_check_range, 0, NULL, "", "", NULL },
    { "vt_is_pe64", ventoy_cmd_is_pe64, 0, NULL, "", "", NULL },
    { "vt_sel_wimboot", ventoy_cmd_sel_wimboot, 0, NULL, "", "", NULL },
    { "vt_set_wim_load_prompt", ventoy_cmd_set_wim_prompt, 0, NULL, "", "", NULL },
    { "vt_set_theme", ventoy_cmd_set_theme, 0, NULL, "", "", NULL },
    { "vt_set_theme_path", ventoy_cmd_set_theme_path, 0, NULL, "", "", NULL },
    { "vt_select_theme_cfg", ventoy_cmd_select_theme_cfg, 0, NULL, "", "", NULL },

    { "vt_get_efi_vdisk_offset", ventoy_cmd_get_efivdisk_offset, 0, NULL, "", "", NULL },
    { "vt_search_replace_initrd", ventoy_cmd_search_replace_initrd, 0, NULL, "", "", NULL },
    { "vt_push_pager", ventoy_cmd_push_pager, 0, NULL, "", "", NULL },
    { "vt_pop_pager", ventoy_cmd_pop_pager, 0, NULL, "", "", NULL },
    { "vt_check_json_path_case", ventoy_cmd_chk_json_pathcase, 0, NULL, "", "", NULL },
    { "vt_append_extra_sector", ventoy_cmd_append_ext_sector, 0, NULL, "", "", NULL },
    { "gptpriority", grub_cmd_gptpriority, 0, NULL, "", "", NULL },
    { "vt_syslinux_need_nojoliet", grub_cmd_syslinux_nojoliet, 0, NULL, "", "", NULL },
    { "vt_vlnk_check", grub_cmd_check_vlnk, 0, NULL, "", "", NULL },
    { "vt_vlnk_dump_part", grub_cmd_vlnk_dump_part, 0, NULL, "", "", NULL },
    { "vt_is_vlnk_name", grub_cmd_is_vlnk_name, 0, NULL, "", "", NULL },
    { "vt_get_vlnk_dst", grub_cmd_get_vlnk_dst, 0, NULL, "", "", NULL },
    { "vt_set_fake_vlnk", ventoy_cmd_set_fake_vlnk, 0, NULL, "", "", NULL },
    { "vt_reset_fake_vlnk", ventoy_cmd_reset_fake_vlnk, 0, NULL, "", "", NULL },
    { "vt_iso_vd_id_parse", ventoy_cmd_iso_vd_id_parse, 0, NULL, "", "", NULL },
    { "vt_iso_vd_id_clear", ventoy_iso_vd_id_clear, 0, NULL, "", "", NULL },
    { "vt_iso_vd_id_begin", ventoy_cmd_iso_vd_id_begin, 0, NULL, "", "", NULL },
    { "vt_fn_mutex_lock", ventoy_cmd_fn_mutex_lock, 0, NULL, "", "", NULL },
    { "vt_efi_dump_rsv_page", ventoy_cmd_dump_rsv_page, 0, NULL, "", "", NULL },
    { "vt_is_standard_winiso", ventoy_cmd_is_standard_winiso, 0, NULL, "", "", NULL },
    { "vt_sel_winpe_wim", ventoy_cmd_sel_winpe_wim, 0, NULL, "", "", NULL },
    { "vt_need_secondary_menu", ventoy_cmd_need_secondary_menu, 0, NULL, "", "", NULL },
    { "vt_show_secondary_menu", ventoy_cmd_show_secondary_menu, 0, NULL, "", "", NULL },
    { "vt_fs_ignore_case", ventoy_cmd_fs_ignore_case, 0, NULL, "", "", NULL },
    { "vt_systemd_menu", ventoy_cmd_linux_systemd_menu, 0, NULL, "", "", NULL },
    { "vt_limine_menu", ventoy_cmd_linux_limine_menu, 0, NULL, "", "", NULL },
    { "vt_secondary_recover_mode", ventoy_cmd_secondary_recover_mode, 0, NULL, "", "", NULL },
    { "vt_load_menu_lang", ventoy_cmd_load_menu_lang, 0, NULL, "", "", NULL },
    { "vt_init_menu_lang", ventoy_cmd_init_menu_lang, 0, NULL, "", "", NULL },
    { "vt_cur_menu_lang", ventoy_cmd_cur_menu_lang, 0, NULL, "", "", NULL },
    { "vt_vtoychksum_exist", ventoy_cmd_vtoychksum_exist, 0, NULL, "", "", NULL },
    { "vt_cmp_checksum", ventoy_cmd_cmp_checksum, 0, NULL, "", "", NULL },
    { "vt_push_menu_lang", ventoy_cmd_push_menulang, 0, NULL, "", "", NULL },
    { "vt_pop_menu_lang", ventoy_cmd_pop_menulang, 0, NULL, "", "", NULL },

};

int ventoy_register_all_cmd(void)
{
    grub_uint32_t i;
    cmd_para *cur = NULL;
    
    for (i = 0; i < ARRAY_SIZE(ventoy_cmds); i++)
    {
        cur = ventoy_cmds + i;
        cur->cmd = grub_register_extcmd(cur->name, cur->func, cur->flags, 
                                        cur->summary, cur->description, cur->parser);
    }

    return 0;
}

int ventoy_unregister_all_cmd(void)
{
    grub_uint32_t i;
    
    for (i = 0; i < ARRAY_SIZE(ventoy_cmds); i++)
    {
        grub_unregister_extcmd(ventoy_cmds[i].cmd);
    }
    
    return 0;
}


