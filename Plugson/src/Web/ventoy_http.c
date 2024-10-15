/******************************************************************************
 * ventoy_http.c  ---- ventoy http
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <errno.h>
#include <time.h>

#if defined(_MSC_VER) || defined(WIN32)
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <linux/fs.h>
#include <linux/limits.h>
#include <dirent.h>
#include <pthread.h>
#endif

#include <ventoy_define.h>
#include <ventoy_json.h>
#include <ventoy_util.h>
#include <ventoy_disk.h>
#include <ventoy_http.h>
#include "fat_filelib.h"

static const char *g_json_title_postfix[bios_max + 1] = 
{
    "", "_legacy", "_uefi", "_ia32", "_aa64", "_mips", ""
};

static const char *g_ventoy_kbd_layout[] =
{
    "QWERTY_USA", "AZERTY", "CZECH_QWERTY", "CZECH_QWERTZ", "DANISH", 
    "DVORAK_USA", "FRENCH", "GERMAN", "ITALIANO", "JAPAN_106", "LATIN_USA", 
    "PORTU_BRAZIL", "QWERTY_UK", "QWERTZ", "QWERTZ_HUN", "QWERTZ_SLOV_CROAT", 
    "SPANISH", "SWEDISH", "TURKISH_Q", "VIETNAMESE",
    NULL
};

#define VTOY_DEL_ALL_PATH   "4119ae33-98ea-448e-b9c0-569aafcf1fb4"

static int g_json_exist[plugin_type_max][bios_max];
static const char *g_plugin_name[plugin_type_max] = 
{
    "control", "theme", "menu_alias", "menu_tip", 
    "menu_class", "auto_install", "persistence", "injection", 
    "conf_replace", "password", "image_list", 
    "auto_memdisk", "dud"
};

static char g_ventoy_menu_lang[MAX_LANGUAGE][8];

static char g_pub_path[2 * MAX_PATH];
static data_control g_data_control[bios_max + 1];
static data_theme g_data_theme[bios_max + 1];
static data_alias g_data_menu_alias[bios_max + 1];
static data_tip   g_data_menu_tip[bios_max + 1];
static data_class g_data_menu_class[bios_max + 1];
static data_image_list g_data_image_list[bios_max + 1];
static data_image_list *g_data_image_blacklist = g_data_image_list;
static data_auto_memdisk g_data_auto_memdisk[bios_max + 1];
static data_password g_data_password[bios_max + 1];
static data_conf_replace g_data_conf_replace[bios_max + 1];
static data_injection g_data_injection[bios_max + 1];
static data_auto_install g_data_auto_install[bios_max + 1];
static data_persistence g_data_persistence[bios_max + 1];
static data_dud g_data_dud[bios_max + 1];

static char *g_pub_json_buffer = NULL;
static char *g_pub_save_buffer = NULL;
#define JSON_BUFFER g_pub_json_buffer
#define JSON_SAVE_BUFFER g_pub_save_buffer

static pthread_mutex_t g_api_mutex;
static struct mg_context *g_ventoy_http_ctx = NULL;

#define ventoy_is_real_exist_common(xpath, xnode, xtype) \
    ventoy_path_is_real_exist(xpath, xnode, offsetof(xtype, path), offsetof(xtype, next))

static int ventoy_is_kbd_valid(const char *key)
{
    int i = 0;
    
    for (i = 0; g_ventoy_kbd_layout[i]; i++)
    {
        if (strcmp(g_ventoy_kbd_layout[i], key) == 0)
        {
            return 1;
        }
    }

    return 0;
}

static const char * ventoy_real_path(const char *org)
{
    int count = 0;
    
    if (g_sysinfo.pathcase)
    {
        scnprintf(g_pub_path, MAX_PATH, "%s", org);
        count = ventoy_path_case(g_pub_path + 1, 1);
        if (count > 0)
        {
            return g_pub_path;
        }
        return org;
    }
    else
    {
        return org;
    }
}


static int ventoy_json_result(struct mg_connection *conn, const char *err)
{
    mg_printf(conn, 
              "HTTP/1.1 200 OK \r\n"
              "Content-Type: application/json\r\n"
              "Content-Length: %d\r\n"
              "\r\n%s",
              (int)strlen(err), err);

    return 0;
}

static int ventoy_json_buffer(struct mg_connection *conn, const char *json_buf, int json_len)
{
    mg_printf(conn, 
              "HTTP/1.1 200 OK \r\n"
              "Content-Type: application/json\r\n"
              "Content-Length: %d\r\n"
              "\r\n%s",
              json_len, json_buf); 

    return 0;
}

static void ventoy_free_path_node_list(path_node *list)
{
    path_node *next = NULL;
    path_node *node = list;

    while (node)
    {
        next = node->next;
        free(node);
        node = next;
    }    
}

static int ventoy_path_is_real_exist(const char *path, void *head, size_t pathoff, size_t nextoff)
{
    char *node = NULL;
    const char *nodepath = NULL;
    const char *realpath = NULL;
    char pathbuf[MAX_PATH];

    if (strchr(path, '*'))
    {
        return 0;
    }

    realpath = ventoy_real_path(path);
    scnprintf(pathbuf, sizeof(pathbuf), "%s", realpath);

    node = (char *)head;
    while (node)
    {
        nodepath = node + pathoff;
        if (NULL == strchr(nodepath, '*'))
        {
            realpath = ventoy_real_path(nodepath);
            if (strcmp(pathbuf, realpath) == 0)
            {
                return 1;
            }
        }
        
        memcpy(&node, node + nextoff, sizeof(node));
    }

    return 0;
}

static path_node * ventoy_path_node_add_array(VTOY_JSON *array)
{
    path_node *head = NULL;
    path_node *node = NULL;
    path_node *cur = NULL;
    VTOY_JSON *item = NULL;

    for (item = array->pstChild; item; item = item->pstNext)
    {
        node = zalloc(sizeof(path_node));
        if (node)
        {                      
            scnprintf(node->path, sizeof(node->path), "%s", item->unData.pcStrVal);
            vtoy_list_add(head, cur, node);
        }
    }

    return head;
}

static int ventoy_check_fuzzy_path(char *path, int prefix)
{
    int rc;
    char c;
    char *cur = NULL;
    char *pos = NULL;
    
    if (!path)
    {
        return 0;
    }

    pos = strchr(path, '*');
    if (pos)
    {
        for (cur = pos; *cur; cur++)
        {
            if (*cur == '/')
            {
                return 0;
            }
        }
    
        while (pos != path)
        {
            if (*pos == '/')
            {
                break;
            }
            pos--;
        }

        if (*pos == '/')
        {
            if (pos != path)
            {
                c = *pos;
                *pos = 0;
                if (prefix)
                {
                    rc = ventoy_is_directory_exist("%s%s", g_cur_dir, path);
                }
                else
                {
                    rc = ventoy_is_directory_exist("%s", path);
                }
                *pos = c;

                if (rc == 0)
                {
                    return 0;
                }
            }

            return -1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        if (prefix)
        {
            return ventoy_is_file_exist("%s%s", g_cur_dir, path);                        
        }
        else
        {
            return ventoy_is_file_exist("%s", path);            
        }
    }
}

static int ventoy_path_list_cmp(path_node *list1, path_node *list2)
{
    if (NULL == list1 && NULL == list2)
    {
        return 0;
    }
    else if (list1 && list2)
    {
        while (list1 && list2)
        {
            if (strcmp(list1->path, list2->path))
            {
                return 1;
            }
        
            list1 = list1->next;
            list2 = list2->next;
        }

        if (list1 == NULL && list2 == NULL)
        {
            return 0;
        }
        return 1;
    }
    else
    {
        return 1;
    }
}


static int ventoy_api_device_info(struct mg_connection *conn, VTOY_JSON *json)
{
    int pos = 0;
    
    (void)json;

    VTOY_JSON_FMT_BEGIN(pos, JSON_BUFFER, JSON_BUF_MAX);
    VTOY_JSON_FMT_OBJ_BEGIN();
    
    VTOY_JSON_FMT_STRN("dev_name", g_sysinfo.cur_model);
    VTOY_JSON_FMT_STRN("dev_capacity", g_sysinfo.cur_capacity);
    VTOY_JSON_FMT_STRN("dev_fs", g_sysinfo.cur_fsname);
    VTOY_JSON_FMT_STRN("ventoy_ver", g_sysinfo.cur_ventoy_ver);
    VTOY_JSON_FMT_SINT("part_style", g_sysinfo.cur_part_style);
    VTOY_JSON_FMT_SINT("secure_boot", g_sysinfo.cur_secureboot);

    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    ventoy_json_buffer(conn, JSON_BUFFER, pos);
    return 0;
}

static int ventoy_api_sysinfo(struct mg_connection *conn, VTOY_JSON *json)
{
    int pos = 0;
    
    (void)json;

    VTOY_JSON_FMT_BEGIN(pos, JSON_BUFFER, JSON_BUF_MAX);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_STRN("language", ventoy_get_os_language());
    VTOY_JSON_FMT_STRN("curdir", g_cur_dir);

    //read clear
    VTOY_JSON_FMT_SINT("syntax_error", g_sysinfo.syntax_error);
    g_sysinfo.syntax_error = 0;
    
    VTOY_JSON_FMT_SINT("invalid_config", g_sysinfo.invalid_config);
    g_sysinfo.invalid_config = 0;
    
    #if defined(_MSC_VER) || defined(WIN32)
    VTOY_JSON_FMT_STRN("os", "windows");
    #else
    VTOY_JSON_FMT_STRN("os", "linux");
    #endif

    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    ventoy_json_buffer(conn, JSON_BUFFER, pos);
    return 0;
}

static int ventoy_api_handshake(struct mg_connection *conn, VTOY_JSON *json)
{
    int i = 0;
    int j = 0;
    int pos = 0;
    char key[128];
    
    (void)json;

    VTOY_JSON_FMT_BEGIN(pos, JSON_BUFFER, JSON_BUF_MAX);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_SINT("status", 0);
    VTOY_JSON_FMT_SINT("save_error", g_sysinfo.config_save_error);
    g_sysinfo.config_save_error = 0;

    for (i = 0; i < plugin_type_max; i++)
    {
        scnprintf(key, sizeof(key), "exist_%s", g_plugin_name[i]);
        VTOY_JSON_FMT_KEY(key);
        VTOY_JSON_FMT_ARY_BEGIN();
        for (j = 0; j < bios_max; j++)
        {
            VTOY_JSON_FMT_ITEM_INT(g_json_exist[i][j]);
        }
        VTOY_JSON_FMT_ARY_ENDEX();
    }
    
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    ventoy_json_buffer(conn, JSON_BUFFER, pos);
    return 0;
}

static int ventoy_api_check_exist(struct mg_connection *conn, VTOY_JSON *json)
{
    int dir = 0;
    int pos = 0;
    int exist = 0;
    const char *path = NULL;

    path = vtoy_json_get_string_ex(json, "path");
    vtoy_json_get_int(json, "dir", &dir);

    if (path)
    {
        if (dir)
        {
            exist = ventoy_is_directory_exist("%s", path);
        }
        else
        {
            exist = ventoy_is_file_exist("%s", path);
        }
    }

    VTOY_JSON_FMT_BEGIN(pos, JSON_BUFFER, JSON_BUF_MAX);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_SINT("exist", exist);
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    ventoy_json_buffer(conn, JSON_BUFFER, pos);
    return 0;
}

static int ventoy_api_check_exist2(struct mg_connection *conn, VTOY_JSON *json)
{
    int dir1 = 0;
    int dir2 = 0;
    int fuzzy1 = 0;
    int fuzzy2 = 0;
    int pos = 0;
    int exist1 = 0;
    int exist2 = 0;
    const char *path1 = NULL;
    const char *path2 = NULL;

    path1 = vtoy_json_get_string_ex(json, "path1");
    path2 = vtoy_json_get_string_ex(json, "path2");
    vtoy_json_get_int(json, "dir1", &dir1);
    vtoy_json_get_int(json, "dir2", &dir2);
    vtoy_json_get_int(json, "fuzzy1", &fuzzy1);
    vtoy_json_get_int(json, "fuzzy2", &fuzzy2);

    if (path1)
    {
        if (dir1)
        {
            exist1 = ventoy_is_directory_exist("%s", path1);
        }
        else
        {
            if (fuzzy1)
            {
                exist1 = ventoy_check_fuzzy_path((char *)path1, 0);                
            }
            else
            {
                exist1 = ventoy_is_file_exist("%s", path1);
            }
        }
    }
    
    if (path2)
    {
        if (dir2)
        {
            exist2 = ventoy_is_directory_exist("%s", path2);
        }
        else
        {
            if (fuzzy2)
            {
                exist2 = ventoy_check_fuzzy_path((char *)path2, 0);                
            }
            else
            {
                exist2 = ventoy_is_file_exist("%s", path2);
            }
        }
    }

    VTOY_JSON_FMT_BEGIN(pos, JSON_BUFFER, JSON_BUF_MAX);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_SINT("exist1", exist1);
    VTOY_JSON_FMT_SINT("exist2", exist2);
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    ventoy_json_buffer(conn, JSON_BUFFER, pos);
    return 0;
}

static int ventoy_api_check_fuzzy(struct mg_connection *conn, VTOY_JSON *json)
{
    int pos = 0;
    int exist = 0;
    const char *path = NULL;

    path = vtoy_json_get_string_ex(json, "path");
    if (path)
    {
        exist = ventoy_check_fuzzy_path((char *)path, 0);
    }

    VTOY_JSON_FMT_BEGIN(pos, JSON_BUFFER, JSON_BUF_MAX);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_SINT("exist", exist);
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    ventoy_json_buffer(conn, JSON_BUFFER, pos);
    return 0;
}


#if 0
#endif
void ventoy_data_default_control(data_control *data)
{
    memset(data, 0, sizeof(data_control));

    data->password_asterisk = 1;
    data->secondary_menu = 1;
    data->filter_dot_underscore = 1;
    data->max_search_level = -1;
    data->menu_timeout = 0;
    data->secondary_menu_timeout = 0;
    data->win11_bypass_check = 1;
    data->win11_bypass_nro = 1;
    
    strlcpy(data->default_kbd_layout, "QWERTY_USA");
    strlcpy(data->menu_language, "en_US");
}

int ventoy_data_cmp_control(data_control *data1, data_control *data2)
{
    if (data1->default_menu_mode != data2->default_menu_mode ||
        data1->treeview_style != data2->treeview_style ||
        data1->filter_dot_underscore != data2->filter_dot_underscore ||
        data1->sort_casesensitive != data2->sort_casesensitive ||
        data1->max_search_level != data2->max_search_level ||
        data1->vhd_no_warning != data2->vhd_no_warning ||
        data1->filter_iso != data2->filter_iso ||
        data1->filter_wim != data2->filter_wim ||
        data1->filter_efi != data2->filter_efi ||
        data1->filter_img != data2->filter_img ||
        data1->filter_vhd != data2->filter_vhd ||
        data1->filter_vtoy != data2->filter_vtoy ||
        data1->win11_bypass_check != data2->win11_bypass_check ||
        data1->win11_bypass_nro != data2->win11_bypass_nro ||
        data1->linux_remount != data2->linux_remount ||
        data1->password_asterisk != data2->password_asterisk ||
        data1->secondary_menu != data2->secondary_menu ||
        data1->menu_timeout != data2->menu_timeout ||
        data1->secondary_menu_timeout != data2->secondary_menu_timeout)
    {
        return 1;
    }

    if (strcmp(data1->default_search_root, data2->default_search_root) ||
        strcmp(data1->default_image, data2->default_image) ||
        strcmp(data1->default_kbd_layout, data2->default_kbd_layout) ||
        strcmp(data1->menu_language, data2->menu_language))
    {
        return 1;
    }

    return 0;
}

int ventoy_data_save_control(data_control *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    data_control *def = g_data_control + bios_max;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_ARY_BEGIN_N();

    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_DEFAULT_MENU_MODE", default_menu_mode);        
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_TREE_VIEW_MENU_STYLE", treeview_style);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_FILT_DOT_UNDERSCORE_FILE", filter_dot_underscore);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_SORT_CASE_SENSITIVE",  sort_casesensitive);

    if (data->max_search_level >= 0)
    {
        VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_MAX_SEARCH_LEVEL",  max_search_level);            
    }
    
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_VHD_NO_WARNING",  vhd_no_warning);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_FILE_FLT_ISO", filter_iso);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_FILE_FLT_WIM", filter_wim);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_FILE_FLT_EFI", filter_efi);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_FILE_FLT_IMG", filter_img);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_FILE_FLT_VHD", filter_vhd);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_FILE_FLT_VTOY", filter_vtoy);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_WIN11_BYPASS_CHECK",  win11_bypass_check);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_WIN11_BYPASS_NRO",  win11_bypass_nro);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_LINUX_REMOUNT",  linux_remount);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_SECONDARY_BOOT_MENU",  secondary_menu);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_SHOW_PASSWORD_ASTERISK",  password_asterisk);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_MENU_TIMEOUT",  menu_timeout);
    VTOY_JSON_FMT_CTRL_INT(L2, "VTOY_SECONDARY_TIMEOUT",  secondary_menu_timeout);

    VTOY_JSON_FMT_CTRL_STRN(L2, "VTOY_DEFAULT_KBD_LAYOUT", default_kbd_layout);        
    VTOY_JSON_FMT_CTRL_STRN(L2, "VTOY_MENU_LANGUAGE", menu_language);  

    if (strcmp(def->default_search_root, data->default_search_root))
    {
        VTOY_JSON_FMT_CTRL_STRN_STR(L2, "VTOY_DEFAULT_SEARCH_ROOT", ventoy_real_path(data->default_search_root));
    }
    
    if (strcmp(def->default_image, data->default_image))
    {
        VTOY_JSON_FMT_CTRL_STRN_STR(L2, "VTOY_DEFAULT_IMAGE", ventoy_real_path(data->default_image));
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}

int ventoy_data_json_control(data_control *ctrl, char *buf, int buflen)
{
    int i = 0;
    int pos = 0;
    int valid = 0;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_OBJ_BEGIN();

    VTOY_JSON_FMT_SINT("default_menu_mode", ctrl->default_menu_mode);
    VTOY_JSON_FMT_SINT("treeview_style",  ctrl->treeview_style);
    VTOY_JSON_FMT_SINT("filter_dot_underscore",  ctrl->filter_dot_underscore);
    VTOY_JSON_FMT_SINT("sort_casesensitive",  ctrl->sort_casesensitive);
    VTOY_JSON_FMT_SINT("max_search_level",  ctrl->max_search_level);    
    VTOY_JSON_FMT_SINT("vhd_no_warning",  ctrl->vhd_no_warning);
    
    VTOY_JSON_FMT_SINT("filter_iso", ctrl->filter_iso);
    VTOY_JSON_FMT_SINT("filter_wim", ctrl->filter_wim);
    VTOY_JSON_FMT_SINT("filter_efi", ctrl->filter_efi);
    VTOY_JSON_FMT_SINT("filter_img", ctrl->filter_img);
    VTOY_JSON_FMT_SINT("filter_vhd", ctrl->filter_vhd);
    VTOY_JSON_FMT_SINT("filter_vtoy", ctrl->filter_vtoy);
    VTOY_JSON_FMT_SINT("win11_bypass_check",  ctrl->win11_bypass_check);
    VTOY_JSON_FMT_SINT("win11_bypass_nro",  ctrl->win11_bypass_nro);
    VTOY_JSON_FMT_SINT("linux_remount",  ctrl->linux_remount);
    VTOY_JSON_FMT_SINT("secondary_menu",  ctrl->secondary_menu);
    VTOY_JSON_FMT_SINT("password_asterisk",  ctrl->password_asterisk);
    VTOY_JSON_FMT_SINT("menu_timeout",  ctrl->menu_timeout);
    VTOY_JSON_FMT_SINT("secondary_menu_timeout",  ctrl->secondary_menu_timeout);
    VTOY_JSON_FMT_STRN("default_kbd_layout",  ctrl->default_kbd_layout);
    VTOY_JSON_FMT_STRN("menu_language",  ctrl->menu_language);

    valid = 0;
    if (ctrl->default_search_root[0] && ventoy_is_directory_exist("%s%s", g_cur_dir, ctrl->default_search_root))
    {
        valid = 1;
    }
    VTOY_JSON_FMT_STRN("default_search_root",  ctrl->default_search_root);
    VTOY_JSON_FMT_SINT("default_search_root_valid", valid);

    
    valid = 0;
    if (ctrl->default_image[0] && ventoy_is_file_exist("%s%s", g_cur_dir, ctrl->default_image))
    {
        valid = 1;
    }
    VTOY_JSON_FMT_STRN("default_image", ctrl->default_image);
    VTOY_JSON_FMT_SINT("default_image_valid", valid);

    VTOY_JSON_FMT_KEY("menu_list");
    VTOY_JSON_FMT_ARY_BEGIN();

    for (i = 0; g_ventoy_menu_lang[i][0]; i++)
    {
        VTOY_JSON_FMT_ITEM(g_ventoy_menu_lang[i]);        
    }
    VTOY_JSON_FMT_ARY_ENDEX();
    
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

static int ventoy_api_get_control(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, control);
    return 0;
}

static int ventoy_api_save_control(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    data_control *ctrl = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    ctrl = g_data_control + index;

    VTOY_JSON_INT("default_menu_mode", ctrl->default_menu_mode);
    VTOY_JSON_INT("treeview_style", ctrl->treeview_style);
    VTOY_JSON_INT("filter_dot_underscore", ctrl->filter_dot_underscore);
    VTOY_JSON_INT("sort_casesensitive", ctrl->sort_casesensitive);
    VTOY_JSON_INT("max_search_level", ctrl->max_search_level);
    VTOY_JSON_INT("vhd_no_warning", ctrl->vhd_no_warning);
    VTOY_JSON_INT("filter_iso", ctrl->filter_iso);
    VTOY_JSON_INT("filter_wim", ctrl->filter_wim);
    VTOY_JSON_INT("filter_efi", ctrl->filter_efi);
    VTOY_JSON_INT("filter_img", ctrl->filter_img);
    VTOY_JSON_INT("filter_vhd", ctrl->filter_vhd);
    VTOY_JSON_INT("filter_vtoy", ctrl->filter_vtoy);
    VTOY_JSON_INT("win11_bypass_check", ctrl->win11_bypass_check);
    VTOY_JSON_INT("win11_bypass_nro", ctrl->win11_bypass_nro);
    VTOY_JSON_INT("linux_remount", ctrl->linux_remount);
    VTOY_JSON_INT("secondary_menu", ctrl->secondary_menu);
    VTOY_JSON_INT("password_asterisk", ctrl->password_asterisk);
    VTOY_JSON_INT("menu_timeout", ctrl->menu_timeout);
    VTOY_JSON_INT("secondary_menu_timeout", ctrl->secondary_menu_timeout);

    VTOY_JSON_STR("default_image", ctrl->default_image);
    VTOY_JSON_STR("default_search_root", ctrl->default_search_root);
    VTOY_JSON_STR("menu_language", ctrl->menu_language);
    VTOY_JSON_STR("default_kbd_layout", ctrl->default_kbd_layout);
    
    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);
    return 0;
}


#if 0
#endif

void ventoy_data_default_theme(data_theme *data)
{
    memset(data, 0, sizeof(data_theme));
    strlcpy(data->gfxmode, "1024x768");
    scnprintf(data->ventoy_left, sizeof(data->ventoy_left), "5%%");
    scnprintf(data->ventoy_top, sizeof(data->ventoy_top), "95%%");
    scnprintf(data->ventoy_color, sizeof(data->ventoy_color), "%s", "#0000ff");
}

int ventoy_data_cmp_theme(data_theme *data1, data_theme *data2)
{
    if (data1->display_mode != data2->display_mode ||
        strcmp(data1->ventoy_left, data2->ventoy_left) ||
        strcmp(data1->ventoy_top, data2->ventoy_top) ||
        strcmp(data1->gfxmode, data2->gfxmode) ||
        strcmp(data1->ventoy_color, data2->ventoy_color)
        )
    {
        return 1;
    }

    if (ventoy_path_list_cmp(data1->filelist, data2->filelist))
    {
        return 1;
    }

    if (ventoy_path_list_cmp(data1->fontslist, data2->fontslist))
    {
        return 1;
    }

    return 0;
}


int ventoy_data_save_theme(data_theme *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    path_node *node = NULL;
    data_theme *def = g_data_theme + bios_max;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_OBJ_BEGIN_N();

    if (data->filelist)
    {
        if (data->filelist->next)
        {
            VTOY_JSON_FMT_KEY_L(L2, "file");
            VTOY_JSON_FMT_ARY_BEGIN_N();

            for (node = data->filelist; node; node = node->next)
            {
                VTOY_JSON_FMT_ITEM_PATH_LN(L3, node->path);
            }

            VTOY_JSON_FMT_ARY_ENDEX_LN(L2);
        
            if (def->default_file != data->default_file)
            {
                VTOY_JSON_FMT_SINT_LN(L2, "default_file", data->default_file);                
            }
            
            if (def->resolution_fit != data->resolution_fit)
            {
                VTOY_JSON_FMT_SINT_LN(L2, "resolution_fit", data->resolution_fit);                
            }
        }
        else
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L2, "file", data->filelist->path);
        }
    }

    if (data->display_mode != def->display_mode)
    {
        if (display_mode_cli == data->display_mode)
        {
            VTOY_JSON_FMT_STRN_LN(L2, "display_mode", "CLI");
        }
        else if (display_mode_serial == data->display_mode)
        {
            VTOY_JSON_FMT_STRN_LN(L2, "display_mode", "serial");
        }
        else if (display_mode_ser_console == data->display_mode)
        {
            VTOY_JSON_FMT_STRN_LN(L2, "display_mode", "serial_console");
        }
        else
        {
            VTOY_JSON_FMT_STRN_LN(L2, "display_mode", "GUI");
        }
    }

    VTOY_JSON_FMT_DIFF_STRN(L2, "gfxmode", gfxmode);
    
    VTOY_JSON_FMT_DIFF_STRN(L2, "ventoy_left", ventoy_left);
    VTOY_JSON_FMT_DIFF_STRN(L2, "ventoy_top", ventoy_top);
    VTOY_JSON_FMT_DIFF_STRN(L2, "ventoy_color", ventoy_color);
    
    if (data->fontslist)
    {
        VTOY_JSON_FMT_KEY_L(L2, "fonts");
        VTOY_JSON_FMT_ARY_BEGIN_N();

        for (node = data->fontslist; node; node = node->next)
        {
			VTOY_JSON_FMT_ITEM_PATH_LN(L3, node->path);
        }
        
        VTOY_JSON_FMT_ARY_ENDEX_LN(L2);
    }
    
    VTOY_JSON_FMT_OBJ_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_theme(data_theme *data, char *buf, int buflen)
{
    int pos = 0;
    path_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_OBJ_BEGIN();

    VTOY_JSON_FMT_SINT("default_file",  data->default_file);
    VTOY_JSON_FMT_SINT("resolution_fit",  data->resolution_fit);
    VTOY_JSON_FMT_SINT("display_mode",  data->display_mode);
    VTOY_JSON_FMT_STRN("gfxmode", data->gfxmode);
    
    VTOY_JSON_FMT_STRN("ventoy_color", data->ventoy_color);
    VTOY_JSON_FMT_STRN("ventoy_left", data->ventoy_left);
    VTOY_JSON_FMT_STRN("ventoy_top", data->ventoy_top);
    
    VTOY_JSON_FMT_KEY("filelist");
    VTOY_JSON_FMT_ARY_BEGIN();
    for (node = data->filelist; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();
        VTOY_JSON_FMT_STRN("path", node->path);
        VTOY_JSON_FMT_SINT("valid", ventoy_is_file_exist("%s%s", g_cur_dir, node->path));
        VTOY_JSON_FMT_OBJ_ENDEX();
    }
    VTOY_JSON_FMT_ARY_ENDEX();
    
    VTOY_JSON_FMT_KEY("fontslist");
    VTOY_JSON_FMT_ARY_BEGIN();
    for (node = data->fontslist; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();
        VTOY_JSON_FMT_STRN("path", node->path);
        VTOY_JSON_FMT_SINT("valid", ventoy_is_file_exist("%s%s", g_cur_dir, node->path));
        VTOY_JSON_FMT_OBJ_ENDEX();
    }
    VTOY_JSON_FMT_ARY_ENDEX();
    
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

static int ventoy_api_get_theme(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, theme);
    return 0;
}

static int ventoy_api_save_theme(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    data_theme *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_theme + index;

    VTOY_JSON_INT("default_file", data->default_file);
    VTOY_JSON_INT("resolution_fit", data->resolution_fit);
    VTOY_JSON_INT("display_mode", data->display_mode);
    VTOY_JSON_STR("gfxmode", data->gfxmode);
    VTOY_JSON_STR("ventoy_left", data->ventoy_left);
    VTOY_JSON_STR("ventoy_top", data->ventoy_top);
    VTOY_JSON_STR("ventoy_color", data->ventoy_color);

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}


static int ventoy_api_theme_add_file(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *node = NULL;
    path_node *cur = NULL;
    data_theme *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_theme + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (ventoy_is_real_exist_common(path, data->filelist, path_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }

        node = zalloc(sizeof(path_node));
        if (node)
        {
            scnprintf(node->path, sizeof(node->path), "%s", path);

            vtoy_list_add(data->filelist, cur, node);            
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET); 
    return 0;
}

static int ventoy_api_theme_del_file(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *node = NULL;
    path_node *last = NULL;
    data_theme *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_theme + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(path_node, data->filelist);
        }
        else
        {
            vtoy_list_del(last, node, data->filelist, path);
        }    
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

static int ventoy_api_theme_add_font(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *node = NULL;
    path_node *cur = NULL;
    data_theme *data = NULL;

    vtoy_json_get_int(json, "index", &index);
    data = g_data_theme + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (ventoy_is_real_exist_common(path, data->fontslist, path_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }

        node = zalloc(sizeof(path_node));
        if (node)
        {
            scnprintf(node->path, sizeof(node->path), "%s", path);
            vtoy_list_add(data->fontslist, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}


static int ventoy_api_theme_del_font(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *node = NULL;
    path_node *last = NULL;
    data_theme *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_theme + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(path_node, data->fontslist);
        }
        else
        {
            vtoy_list_del(last, node, data->fontslist, path);            
        }    
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

#if 0
#endif

void ventoy_data_default_menu_alias(data_alias *data)
{
    memset(data, 0, sizeof(data_alias));
}

int ventoy_data_cmp_menu_alias(data_alias *data1, data_alias *data2)
{
    data_alias_node *list1 = NULL;
    data_alias_node *list2 = NULL;

    if (NULL == data1->list && NULL == data2->list)
    {
        return 0;
    }
    else if (data1->list && data2->list)
    {
        list1 = data1->list;
        list2 = data2->list;
    
        while (list1 && list2)
        {
            if ((list1->type != list2->type) || 
                strcmp(list1->path, list2->path) || 
                strcmp(list1->alias, list2->alias))
            {
                return 1;
            }
        
            list1 = list1->next;
            list2 = list2->next;
        }

        if (list1 == NULL && list2 == NULL)
        {
            return 0;
        }
        return 1;
    }
    else
    {
        return 1;
    }

    return 0;
}


int ventoy_data_save_menu_alias(data_alias *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    data_alias_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_ARY_BEGIN_N();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN_LN(L2);

        if (node->type == path_type_file)
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L3, "image", node->path);            
        }
        else
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L3, "dir", node->path);
        }
        
        VTOY_JSON_FMT_STRN_EX_LN(L3, "alias", node->alias);
        
        VTOY_JSON_FMT_OBJ_ENDEX_LN(L2);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_menu_alias(data_alias *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    data_alias_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_UINT("type", node->type);
        VTOY_JSON_FMT_STRN("path", node->path);
        if (node->type == path_type_file)
        {
            valid = ventoy_check_fuzzy_path(node->path, 1);
        }
        else
        {
            valid = ventoy_is_directory_exist("%s%s", g_cur_dir, node->path);
        }
        
        VTOY_JSON_FMT_SINT("valid", valid);
        VTOY_JSON_FMT_STRN("alias", node->alias);
        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

static int ventoy_api_get_alias(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, menu_alias);
    return 0;
}

static int ventoy_api_save_alias(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

static int ventoy_api_alias_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    int type = path_type_file;
    const char *path = NULL;
    const char *alias = NULL;
    data_alias_node *node = NULL;
    data_alias_node *cur = NULL;
    data_alias *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_menu_alias + index;

    vtoy_json_get_int(json, "type", &type);

    path = VTOY_JSON_STR_EX("path");
    alias = VTOY_JSON_STR_EX("alias");
    if (path && alias)
    {
        if (ventoy_is_real_exist_common(path, data->list, data_alias_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }

        node = zalloc(sizeof(data_alias_node));
        if (node)
        {
            node->type = type;
            scnprintf(node->path, sizeof(node->path), "%s", path);
            scnprintf(node->alias, sizeof(node->alias), "%s", alias);
            
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

static int ventoy_api_alias_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    data_alias_node *last = NULL;
    data_alias_node *node = NULL;
    data_alias *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_menu_alias + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(data_alias_node, data->list);          
        }
        else
        {
            vtoy_list_del(last, node, data->list, path);            
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);    
    return 0;
}

#if 0
#endif

void ventoy_data_default_menu_tip(data_tip *data)
{
    memset(data, 0, sizeof(data_tip));

    scnprintf(data->left, sizeof(data->left), "10%%");
    scnprintf(data->top, sizeof(data->top), "81%%");
    scnprintf(data->color, sizeof(data->color), "%s", "blue");
}

int ventoy_data_cmp_menu_tip(data_tip *data1, data_tip *data2)
{
    data_tip_node *list1 = NULL;
    data_tip_node *list2 = NULL;

    if (strcmp(data1->left, data2->left) || strcmp(data1->top, data2->top) || strcmp(data1->color, data2->color))
    {
        return 1;
    }

    if (NULL == data1->list && NULL == data2->list)
    {
        return 0;
    }
    else if (data1->list && data2->list)
    {
        list1 = data1->list;
        list2 = data2->list;
    
        while (list1 && list2)
        {
            if ((list1->type != list2->type) || 
                strcmp(list1->path, list2->path) || 
                strcmp(list1->tip, list2->tip))
            {
                return 1;
            }
        
            list1 = list1->next;
            list2 = list2->next;
        }

        if (list1 == NULL && list2 == NULL)
        {
            return 0;
        }
        return 1;
    }
    else
    {
        return 1;
    }

    return 0;
}


int ventoy_data_save_menu_tip(data_tip *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    data_tip_node *node = NULL;
    data_tip *def = g_data_menu_tip + bios_max;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_OBJ_BEGIN_N();

    VTOY_JSON_FMT_DIFF_STRN(L2, "left", left);        
    VTOY_JSON_FMT_DIFF_STRN(L2, "top", top);        
    VTOY_JSON_FMT_DIFF_STRN(L2, "color", color);        

    if (data->list)
    {
        VTOY_JSON_FMT_KEY_L(L2, "tips");
        VTOY_JSON_FMT_ARY_BEGIN_N();

        for (node = data->list; node; node = node->next)
        {
            VTOY_JSON_FMT_OBJ_BEGIN_LN(L3);

            if (node->type == path_type_file)
            {
                VTOY_JSON_FMT_STRN_PATH_LN(L4, "image", node->path);            
            }
            else
            {
                VTOY_JSON_FMT_STRN_PATH_LN(L4, "dir", node->path);
            }
            VTOY_JSON_FMT_STRN_EX_LN(L4, "tip", node->tip);
            
            VTOY_JSON_FMT_OBJ_ENDEX_LN(L3);
        }

        VTOY_JSON_FMT_ARY_ENDEX_LN(L2);
    }
    
    VTOY_JSON_FMT_OBJ_ENDEX_LN(L1);
    
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_menu_tip(data_tip *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    data_tip_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_OBJ_BEGIN();
    
    VTOY_JSON_FMT_STRN("left", data->left);
    VTOY_JSON_FMT_STRN("top", data->top);
    VTOY_JSON_FMT_STRN("color", data->color);

    VTOY_JSON_FMT_KEY("tips");
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_UINT("type", node->type);
        VTOY_JSON_FMT_STRN("path", node->path);
        if (node->type == path_type_file)
        {
            valid = ventoy_check_fuzzy_path(node->path, 1);
        }
        else
        {
            valid = ventoy_is_directory_exist("%s%s", g_cur_dir, node->path);
        }
        
        VTOY_JSON_FMT_SINT("valid", valid);
        VTOY_JSON_FMT_STRN("tip", node->tip);
        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_ENDEX();

    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

static int ventoy_api_get_tip(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, menu_tip);
    return 0;
}

static int ventoy_api_save_tip(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    data_tip *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_menu_tip + index;

    VTOY_JSON_STR("left", data->left);
    VTOY_JSON_STR("top", data->top);
    VTOY_JSON_STR("color", data->color);

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

static int ventoy_api_tip_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    int type = path_type_file;
    const char *path = NULL;
    const char *tip = NULL;
    data_tip_node *node = NULL;
    data_tip_node *cur = NULL;
    data_tip *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_menu_tip + index;

    vtoy_json_get_int(json, "type", &type);

    path = VTOY_JSON_STR_EX("path");
    tip = VTOY_JSON_STR_EX("tip");
    if (path && tip)
    {
        if (ventoy_is_real_exist_common(path, data->list, data_tip_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }
    
        node = zalloc(sizeof(data_tip_node));
        if (node)
        {
            node->type = type;
            scnprintf(node->path, sizeof(node->path), "%s", path);
            scnprintf(node->tip, sizeof(node->tip), "%s", tip);
            
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

static int ventoy_api_tip_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    data_tip_node *last = NULL;
    data_tip_node *node = NULL;
    data_tip *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_menu_tip + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(data_tip_node, data->list);          
        }
        else
        {
            vtoy_list_del(last, node, data->list, path);  
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

#if 0
#endif

void ventoy_data_default_menu_class(data_class *data)
{
    memset(data, 0, sizeof(data_class));
}

int ventoy_data_cmp_menu_class(data_class *data1, data_class *data2)
{
    data_class_node *list1 = NULL;
    data_class_node *list2 = NULL;

    if (NULL == data1->list && NULL == data2->list)
    {
        return 0;
    }
    else if (data1->list && data2->list)
    {
        list1 = data1->list;
        list2 = data2->list;
    
        while (list1 && list2)
        {
            if ((list1->type != list2->type) || 
                strcmp(list1->path, list2->path) || 
                strcmp(list1->class, list2->class))
            {
                return 1;
            }
        
            list1 = list1->next;
            list2 = list2->next;
        }

        if (list1 == NULL && list2 == NULL)
        {
            return 0;
        }
        return 1;
    }
    else
    {
        return 1;
    }

    return 0;
}


int ventoy_data_save_menu_class(data_class *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    data_class_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_ARY_BEGIN_N();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN_LN(L2);

        if (node->type == class_type_key)
        {
            VTOY_JSON_FMT_STRN_LN(L3, "key", node->path);            
        }
        else if (node->type == class_type_dir)
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L3, "dir", node->path);
        }
        else
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L3, "parent", node->path);
        }
        VTOY_JSON_FMT_STRN_LN(L3, "class", node->class);
        
        VTOY_JSON_FMT_OBJ_ENDEX_LN(L2);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_menu_class(data_class *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    data_class_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_UINT("type", node->type);
        VTOY_JSON_FMT_STRN("path", node->path);       

        if (node->type == class_type_key)
        {
            valid = 1;
        }
        else
        {
            valid = ventoy_is_directory_exist("%s%s", g_cur_dir, node->path);
        }
        VTOY_JSON_FMT_SINT("valid", valid);
        
        VTOY_JSON_FMT_STRN("class", node->class);
        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}


static int ventoy_api_get_class(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, menu_class);
    return 0;
}

static int ventoy_api_save_class(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

static int ventoy_api_class_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    int type = class_type_key;
    const char *path = NULL;
    const char *class = NULL;
    data_class_node *node = NULL;
    data_class_node *cur = NULL;
    data_class *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_menu_class + index;

    vtoy_json_get_int(json, "type", &type);

    path = VTOY_JSON_STR_EX("path");
    class = VTOY_JSON_STR_EX("class");
    if (path && class)
    {
        node = zalloc(sizeof(data_class_node));
        if (node)
        {
            node->type = type;
        
            scnprintf(node->path, sizeof(node->path), "%s", path);
            scnprintf(node->class, sizeof(node->class), "%s", class);
            
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET); 
    return 0;
}

static int ventoy_api_class_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    data_class_node *last = NULL;
    data_class_node *node = NULL;
    data_class *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_menu_class + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(data_class_node, data->list);
        }
        else
        {
            vtoy_list_del(last, node, data->list, path);            
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

#if 0
#endif

void ventoy_data_default_auto_memdisk(data_auto_memdisk *data)
{
    memset(data, 0, sizeof(data_auto_memdisk));
}

int ventoy_data_cmp_auto_memdisk(data_auto_memdisk *data1, data_auto_memdisk *data2)
{
    return ventoy_path_list_cmp(data1->list, data2->list);
}

int ventoy_data_save_auto_memdisk(data_auto_memdisk *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    path_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_ARY_BEGIN_N();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_ITEM_PATH_LN(L2, node->path);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}

int ventoy_data_json_auto_memdisk(data_auto_memdisk *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    path_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_STRN("path", node->path); 
        valid = ventoy_check_fuzzy_path(node->path, 1);
        VTOY_JSON_FMT_SINT("valid", valid);        
        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

static int ventoy_api_get_auto_memdisk(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, auto_memdisk);
    return 0;
}

static int ventoy_api_save_auto_memdisk(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    
    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);
    return 0;
}

static int ventoy_api_auto_memdisk_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *node = NULL;
    path_node *cur = NULL;
    data_auto_memdisk *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_auto_memdisk + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (ventoy_is_real_exist_common(path, data->list, path_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }

        node = zalloc(sizeof(path_node));
        if (node)
        {
            scnprintf(node->path, sizeof(node->path), "%s", path);
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);
    return 0;
}

static int ventoy_api_auto_memdisk_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *last = NULL;
    path_node *node = NULL;
    data_auto_memdisk *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_auto_memdisk + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(path_node, data->list);
        }
        else
        {
            vtoy_list_del(last, node, data->list, path);            
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);    
    return 0;
}

#if 0
#endif

void ventoy_data_default_image_list(data_image_list *data)
{
    memset(data, 0, sizeof(data_image_list));
}

int ventoy_data_cmp_image_list(data_image_list *data1, data_image_list *data2)
{
    if (data1->type != data2->type)
    {
        if (data1->list || data2->list)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    return ventoy_path_list_cmp(data1->list, data2->list);
}

int ventoy_data_save_image_list(data_image_list *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    int prelen;
    path_node *node = NULL;
    char newtitle[64];

    (void)title;

    if (!(data->list))
    {
        return 0;
    }

    prelen = (int)strlen("image_list");

    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    if (data->type == 0)
    {
        scnprintf(newtitle, sizeof(newtitle), "image_list%s", title + prelen);
    }
    else
    {
        scnprintf(newtitle, sizeof(newtitle), "image_blacklist%s", title + prelen);
    }
    VTOY_JSON_FMT_KEY_L(L1, newtitle);
    
    VTOY_JSON_FMT_ARY_BEGIN_N();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_ITEM_PATH_LN(L2, node->path);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}

int ventoy_data_json_image_list(data_image_list *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    path_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_SINT("type", data->type);   
    
    VTOY_JSON_FMT_KEY("list");
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_STRN("path", node->path); 
        valid = ventoy_check_fuzzy_path(node->path, 1);
        VTOY_JSON_FMT_SINT("valid", valid);        
        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_ENDEX();
    VTOY_JSON_FMT_OBJ_END();
    
    VTOY_JSON_FMT_END(pos);

    return pos;
}

static int ventoy_api_get_image_list(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, image_list);
    return 0;
}

static int ventoy_api_save_image_list(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    data_image_list *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_image_list + index;

    VTOY_JSON_INT("type", data->type);

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

static int ventoy_api_image_list_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *node = NULL;
    path_node *cur = NULL;
    data_image_list *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_image_list + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (ventoy_is_real_exist_common(path, data->list, path_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }

        node = zalloc(sizeof(path_node));
        if (node)
        {
            scnprintf(node->path, sizeof(node->path), "%s", path);
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);
    return 0;
}

static int ventoy_api_image_list_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    path_node *last = NULL;
    path_node *node = NULL;
    data_image_list *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_image_list + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(path_node, data->list);
        }
        else
        {
            vtoy_list_del(last, node, data->list, path);            
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

#if 0
#endif

void ventoy_data_default_password(data_password *data)
{
    memset(data, 0, sizeof(data_password));
}

int ventoy_data_cmp_password(data_password *data1, data_password *data2)
{
    menu_password *list1 = NULL;
    menu_password *list2 = NULL;

    if (strcmp(data1->bootpwd, data2->bootpwd) || 
        strcmp(data1->isopwd, data2->isopwd) || 
        strcmp(data1->wimpwd, data2->wimpwd) || 
        strcmp(data1->vhdpwd, data2->vhdpwd) || 
        strcmp(data1->imgpwd, data2->imgpwd) || 
        strcmp(data1->efipwd, data2->efipwd) || 
        strcmp(data1->vtoypwd, data2->vtoypwd)
        )
    {
        return 1;
    }

    if (NULL == data1->list && NULL == data2->list)
    {
        return 0;
    }
    else if (data1->list && data2->list)
    {
        list1 = data1->list;
        list2 = data2->list;
    
        while (list1 && list2)
        {
            if ((list1->type != list2->type) || strcmp(list1->path, list2->path))
            {
                return 1;
            }
        
            list1 = list1->next;
            list2 = list2->next;
        }

        if (list1 == NULL && list2 == NULL)
        {
            return 0;
        }
        return 1;
    }
    else
    {
        return 1;
    }

    return 0;
}


int ventoy_data_save_password(data_password *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    menu_password *node = NULL;
    data_password *def = g_data_password + bios_max;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_OBJ_BEGIN_N();

    VTOY_JSON_FMT_DIFF_STRN(L2, "bootpwd", bootpwd);
    VTOY_JSON_FMT_DIFF_STRN(L2, "isopwd", isopwd);
    VTOY_JSON_FMT_DIFF_STRN(L2, "wimpwd", wimpwd);
    VTOY_JSON_FMT_DIFF_STRN(L2, "vhdpwd", vhdpwd);
    VTOY_JSON_FMT_DIFF_STRN(L2, "imgpwd", imgpwd);
    VTOY_JSON_FMT_DIFF_STRN(L2, "efipwd", efipwd);
    VTOY_JSON_FMT_DIFF_STRN(L2, "vtoypwd", vtoypwd);

    if (data->list)
    {
        VTOY_JSON_FMT_KEY_L(L2, "menupwd");
        VTOY_JSON_FMT_ARY_BEGIN_N();

        for (node = data->list; node; node = node->next)
        {
            VTOY_JSON_FMT_OBJ_BEGIN_LN(L3);

            if (node->type == 0)
            {
                VTOY_JSON_FMT_STRN_PATH_LN(L4, "file", node->path);            
            }
            else
            {
                VTOY_JSON_FMT_STRN_PATH_LN(L4, "parent", node->path);
            }
            VTOY_JSON_FMT_STRN_LN(L4, "pwd", node->pwd);
            
            VTOY_JSON_FMT_OBJ_ENDEX_LN(L3);
        }

        VTOY_JSON_FMT_ARY_ENDEX_LN(L2);
    }
    
    VTOY_JSON_FMT_OBJ_ENDEX_LN(L1);
    
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_password(data_password *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    menu_password *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_OBJ_BEGIN();

    VTOY_JSON_FMT_STRN("bootpwd", data->bootpwd);
    VTOY_JSON_FMT_STRN("isopwd", data->isopwd);
    VTOY_JSON_FMT_STRN("wimpwd", data->wimpwd);
    VTOY_JSON_FMT_STRN("vhdpwd", data->vhdpwd);
    VTOY_JSON_FMT_STRN("imgpwd", data->imgpwd);
    VTOY_JSON_FMT_STRN("efipwd", data->efipwd);
    VTOY_JSON_FMT_STRN("vtoypwd", data->vtoypwd);

    VTOY_JSON_FMT_KEY("list");
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_SINT("type", node->type);
        VTOY_JSON_FMT_STRN("path", node->path);
        if (node->type == path_type_file)
        {
            valid = ventoy_check_fuzzy_path(node->path, 1);
        }
        else
        {
            valid = ventoy_is_directory_exist("%s%s", g_cur_dir, node->path);
        }
        
        VTOY_JSON_FMT_SINT("valid", valid);
        VTOY_JSON_FMT_STRN("pwd", node->pwd);
        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_ENDEX();

    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

static int ventoy_api_get_password(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, password);
    return 0;
}

static int ventoy_api_save_password(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    data_password *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_password + index;

    VTOY_JSON_STR("bootpwd", data->bootpwd);
    VTOY_JSON_STR("isopwd", data->isopwd);
    VTOY_JSON_STR("wimpwd", data->wimpwd);
    VTOY_JSON_STR("vhdpwd", data->vhdpwd);
    VTOY_JSON_STR("imgpwd", data->imgpwd);
    VTOY_JSON_STR("efipwd", data->efipwd);
    VTOY_JSON_STR("vtoypwd", data->vtoypwd);

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

static int ventoy_api_password_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    int type = 0;
    const char *path = NULL;
    const char *pwd = NULL;
    menu_password *node = NULL;
    menu_password *cur = NULL;
    data_password *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_password + index;

    vtoy_json_get_int(json, "type", &type);

    path = VTOY_JSON_STR_EX("path");
    pwd = VTOY_JSON_STR_EX("pwd");
    if (path && pwd)
    {
        if (ventoy_is_real_exist_common(path, data->list, menu_password))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }

        node = zalloc(sizeof(menu_password));
        if (node)
        {
            node->type = type;
            scnprintf(node->path, sizeof(node->path), "%s", path);
            scnprintf(node->pwd, sizeof(node->pwd), "%s", pwd);
            
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);    
    return 0;
}

static int ventoy_api_password_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    menu_password *last = NULL;
    menu_password *node = NULL;
    data_password *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_password + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(menu_password, data->list);
        }
        else
        {
            vtoy_list_del(last, node, data->list, path);            
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

#if 0
#endif

void ventoy_data_default_conf_replace(data_conf_replace *data)
{
    memset(data, 0, sizeof(data_conf_replace));
}

int ventoy_data_cmp_conf_replace(data_conf_replace *data1, data_conf_replace *data2)
{
    conf_replace_node *list1 = NULL;
    conf_replace_node *list2 = NULL;

    if (NULL == data1->list && NULL == data2->list)
    {
        return 0;
    }
    else if (data1->list && data2->list)
    {
        list1 = data1->list;
        list2 = data2->list;
    
        while (list1 && list2)
        {
            if (list1->image != list2->image ||
                strcmp(list1->path, list2->path) || 
                strcmp(list1->org, list2->org) || 
                strcmp(list1->new, list2->new)
                )
            {
                return 1;
            }
        
            list1 = list1->next;
            list2 = list2->next;
        }

        if (list1 == NULL && list2 == NULL)
        {
            return 0;
        }
        return 1;
    }
    else
    {
        return 1;
    }

    return 0;
}


int ventoy_data_save_conf_replace(data_conf_replace *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    conf_replace_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_ARY_BEGIN_N();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN_LN(L2);

        VTOY_JSON_FMT_STRN_PATH_LN(L3, "iso", node->path);
        VTOY_JSON_FMT_STRN_LN(L3, "org", node->org);
        VTOY_JSON_FMT_STRN_PATH_LN(L3, "new", node->new);
        if (node->image)
        {
            VTOY_JSON_FMT_SINT_LN(L3, "img", node->image);    
        }
        
        VTOY_JSON_FMT_OBJ_ENDEX_LN(L2);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_conf_replace(data_conf_replace *data, char *buf, int buflen)
{
    int pos = 0;
    conf_replace_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_STRN("path", node->path);
        VTOY_JSON_FMT_SINT("valid", ventoy_check_fuzzy_path(node->path, 1));
        VTOY_JSON_FMT_STRN("org", node->org);
        VTOY_JSON_FMT_STRN("new", node->new);
        VTOY_JSON_FMT_SINT("new_valid", ventoy_is_file_exist("%s%s", g_cur_dir, node->new));
        VTOY_JSON_FMT_SINT("img", node->image);
        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

static int ventoy_api_get_conf_replace(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, conf_replace);
    return 0;
}

static int ventoy_api_save_conf_replace(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

static int ventoy_api_conf_replace_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int image = 0;
    int index = 0;
    const char *path = NULL;
    const char *org = NULL;
    const char *new = NULL;
    conf_replace_node *node = NULL;
    conf_replace_node *cur = NULL;
    data_conf_replace *data = NULL;
    
    vtoy_json_get_int(json, "img", &image);
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_conf_replace + index;

    path = VTOY_JSON_STR_EX("path");
    org = VTOY_JSON_STR_EX("org");
    new = VTOY_JSON_STR_EX("new");
    if (path && org && new)
    {
        node = zalloc(sizeof(conf_replace_node));
        if (node)
        {
            node->image = image;
            scnprintf(node->path, sizeof(node->path), "%s", path);
            scnprintf(node->org, sizeof(node->org), "%s", org);
            scnprintf(node->new, sizeof(node->new), "%s", new);
            
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

static int ventoy_api_conf_replace_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    conf_replace_node *last = NULL;
    conf_replace_node *node = NULL;
    data_conf_replace *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_conf_replace + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(conf_replace_node, data->list);
        }
        else
        {
            vtoy_list_del(last, node, data->list, path);            
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}


#if 0
#endif

void ventoy_data_default_dud(data_dud *data)
{
    memset(data, 0, sizeof(data_dud));
}

int ventoy_data_cmp_dud(data_dud *data1, data_dud *data2)
{
    dud_node *list1 = NULL;
    dud_node *list2 = NULL;

    if (NULL == data1->list && NULL == data2->list)
    {
        return 0;
    }
    else if (data1->list && data2->list)
    {
        list1 = data1->list;
        list2 = data2->list;
    
        while (list1 && list2)
        {
            if (strcmp(list1->path, list2->path))
            {
                return 1;
            }

            /* no need to compare dud list with default */
            list1 = list1->next;
            list2 = list2->next;
        }

        if (list1 == NULL && list2 == NULL)
        {
            return 0;
        }
        return 1;
    }
    else
    {
        return 1;
    }

    return 0;
}


int ventoy_data_save_dud(data_dud *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    dud_node *node = NULL;
    path_node *pathnode = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_ARY_BEGIN_N();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN_LN(L2);
        VTOY_JSON_FMT_STRN_PATH_LN(L3, "image", node->path);            

        VTOY_JSON_FMT_KEY_L(L3, "dud");
        VTOY_JSON_FMT_ARY_BEGIN_N();
        for (pathnode = node->list; pathnode; pathnode = pathnode->next)
        {
            VTOY_JSON_FMT_ITEM_PATH_LN(L4, pathnode->path);
        }
        VTOY_JSON_FMT_ARY_ENDEX_LN(L3);        
        
        VTOY_JSON_FMT_OBJ_ENDEX_LN(L2);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_dud(data_dud *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    dud_node *node = NULL;
    path_node *pathnode = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_STRN("path", node->path);
        valid = ventoy_check_fuzzy_path(node->path, 1);
        VTOY_JSON_FMT_SINT("valid", valid);
        

        VTOY_JSON_FMT_KEY("list");
        VTOY_JSON_FMT_ARY_BEGIN();
        for (pathnode = node->list; pathnode; pathnode = pathnode->next)
        {
            VTOY_JSON_FMT_OBJ_BEGIN();
            VTOY_JSON_FMT_STRN("path", pathnode->path);

            valid = ventoy_is_file_exist("%s%s", g_cur_dir, pathnode->path);
            VTOY_JSON_FMT_SINT("valid", valid);
            VTOY_JSON_FMT_OBJ_ENDEX();
        }
        VTOY_JSON_FMT_ARY_ENDEX(); 

        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

static int ventoy_api_get_dud(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, dud);
    return 0;
}

static int ventoy_api_save_dud(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}


static int ventoy_api_dud_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    dud_node *node = NULL;
    dud_node *cur = NULL;
    data_dud *data = NULL;
    VTOY_JSON *array = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_dud + index;

    array = vtoy_json_find_item(json, JSON_TYPE_ARRAY, "dud");
    path = VTOY_JSON_STR_EX("path");
    if (path && array)
    {
        if (ventoy_is_real_exist_common(path, data->list, dud_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }
        
        node = zalloc(sizeof(dud_node));
        if (node)
        {
            scnprintf(node->path, sizeof(node->path), "%s", path);
            node->list = ventoy_path_node_add_array(array);
            
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

static int ventoy_api_dud_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    dud_node *next = NULL;
    dud_node *last = NULL;
    dud_node *node = NULL;
    data_dud *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_dud + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            for (node = data->list; node; node = next)
            {
                next = node->next;
                ventoy_free_path_node_list(node->list);
                free(node);
            }
            data->list = NULL;
        }
        else
        {
            vtoy_list_del_ex(last, node, data->list, path, ventoy_free_path_node_list);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}


static int ventoy_api_dud_add_inner(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    const char *outpath = NULL;
    path_node *pcur = NULL;
    path_node *pnode = NULL;
    dud_node *node = NULL;
    data_dud *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_dud + index;

    path = VTOY_JSON_STR_EX("path");
    outpath = VTOY_JSON_STR_EX("outpath");
    if (path && outpath)
    {
        for (node = data->list; node; node = node->next)
        {
            if (strcmp(outpath, node->path) == 0)
            {
                pnode = zalloc(sizeof(path_node));
                if (pnode)
                {
                    scnprintf(pnode->path, sizeof(pnode->path), "%s", path);
                    vtoy_list_add(node->list, pcur, pnode);
                }
            
                break;
            }
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET); 
    return 0;
}

static int ventoy_api_dud_del_inner(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    const char *outpath = NULL;
    path_node *plast = NULL;
    path_node *pnode = NULL;
    dud_node *node = NULL;
    data_dud *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_dud + index;

    path = VTOY_JSON_STR_EX("path");
    outpath = VTOY_JSON_STR_EX("outpath");
    if (path && outpath)
    {
        for (node = data->list; node; node = node->next)
        {
            if (strcmp(outpath, node->path) == 0)
            {
                vtoy_list_del(plast, pnode, node->list, path);
                break;
            }
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}


#if 0
#endif

void ventoy_data_default_auto_install(data_auto_install *data)
{
    memset(data, 0, sizeof(data_auto_install));
}

int ventoy_data_cmp_auto_install(data_auto_install *data1, data_auto_install *data2)
{
    auto_install_node *list1 = NULL;
    auto_install_node *list2 = NULL;

    if (NULL == data1->list && NULL == data2->list)
    {
        return 0;
    }
    else if (data1->list && data2->list)
    {
        list1 = data1->list;
        list2 = data2->list;
    
        while (list1 && list2)
        {
            if (list1->timeout != list2->timeout ||
                list1->autosel != list2->autosel ||
                strcmp(list1->path, list2->path))
            {
                return 1;
            }

            /* no need to compare auto install list with default */
            list1 = list1->next;
            list2 = list2->next;
        }

        if (list1 == NULL && list2 == NULL)
        {
            return 0;
        }
        return 1;
    }
    else
    {
        return 1;
    }

    return 0;
}


int ventoy_data_save_auto_install(data_auto_install *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    auto_install_node *node = NULL;
    path_node *pathnode = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_ARY_BEGIN_N();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN_LN(L2);
        if (node->type == 0)
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L3, "image", node->path);            
        }
        else
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L3, "parent", node->path);                        
        }

        
        VTOY_JSON_FMT_KEY_L(L3, "template");
        VTOY_JSON_FMT_ARY_BEGIN_N();
        for (pathnode = node->list; pathnode; pathnode = pathnode->next)
        {
            VTOY_JSON_FMT_ITEM_PATH_LN(L4, pathnode->path);
        }
        VTOY_JSON_FMT_ARY_ENDEX_LN(L3);

        if (node->timeouten)
        {
            VTOY_JSON_FMT_SINT_LN(L3, "timeout", node->timeout);
        }

        if (node->autoselen)
        {
            VTOY_JSON_FMT_SINT_LN(L3, "autosel", node->autosel);                
        }

        VTOY_JSON_FMT_OBJ_ENDEX_LN(L2);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_auto_install(data_auto_install *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    auto_install_node *node = NULL;
    path_node *pathnode = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_STRN("path", node->path);

        if (node->type == 0)
        {
            valid = ventoy_check_fuzzy_path(node->path, 1);            
        }
        else
        {
            valid = ventoy_is_directory_exist("%s%s", g_cur_dir, node->path);
        }
        VTOY_JSON_FMT_SINT("valid", valid);
        VTOY_JSON_FMT_SINT("type", node->type);
        
        VTOY_JSON_FMT_BOOL("timeouten", node->timeouten);
        VTOY_JSON_FMT_BOOL("autoselen", node->autoselen);
        
        VTOY_JSON_FMT_SINT("autosel", node->autosel);
        VTOY_JSON_FMT_SINT("timeout", node->timeout);

        VTOY_JSON_FMT_KEY("list");
        VTOY_JSON_FMT_ARY_BEGIN();
        for (pathnode = node->list; pathnode; pathnode = pathnode->next)
        {
            VTOY_JSON_FMT_OBJ_BEGIN();
            VTOY_JSON_FMT_STRN("path", pathnode->path);

            valid = ventoy_is_file_exist("%s%s", g_cur_dir, pathnode->path);
            VTOY_JSON_FMT_SINT("valid", valid);
            VTOY_JSON_FMT_OBJ_ENDEX();
        }
        VTOY_JSON_FMT_ARY_ENDEX(); 

        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

static int ventoy_api_get_auto_install(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, auto_install);
    return 0;
}

static int ventoy_api_save_auto_install(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int id = -1;
    int cnt = 0;
    int index = 0;
    uint8_t timeouten = 0;
    uint8_t autoselen = 0;
    auto_install_node *node = NULL;
    data_auto_install *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    vtoy_json_get_int(json, "id", &id);

    vtoy_json_get_bool(json, "timeouten", &timeouten);
    vtoy_json_get_bool(json, "autoselen", &autoselen);
    
    data = g_data_auto_install + index;

    if (id >= 0)
    {
        for (node = data->list; node; node = node->next)
        {
            if (cnt == id)
            {
                node->timeouten = (int)timeouten;
                node->autoselen = (int)autoselen;
                VTOY_JSON_INT("timeout", node->timeout);
                VTOY_JSON_INT("autosel", node->autosel);
                break;
            }
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET); 
    return 0;
}


static int ventoy_api_auto_install_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    int type = 0;
    const char *path = NULL;
    auto_install_node *node = NULL;
    auto_install_node *cur = NULL;
    data_auto_install *data = NULL;
    VTOY_JSON *array = NULL;
    
    vtoy_json_get_int(json, "type", &type);
    vtoy_json_get_int(json, "index", &index);
    data = g_data_auto_install + index;

    array = vtoy_json_find_item(json, JSON_TYPE_ARRAY, "template");
    path = VTOY_JSON_STR_EX("path");
    if (path && array)
    {
        if (ventoy_is_real_exist_common(path, data->list, auto_install_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }     
    
        node = zalloc(sizeof(auto_install_node));
        if (node)
        {
            node->type = type;
            node->timeouten = 0;
            node->autoselen = 0;
            node->autosel = 1;
            node->timeout = 0;
            scnprintf(node->path, sizeof(node->path), "%s", path);
            node->list = ventoy_path_node_add_array(array);
            
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

static int ventoy_api_auto_install_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    auto_install_node *last = NULL;
    auto_install_node *next = NULL;
    auto_install_node *node = NULL;
    data_auto_install *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_auto_install + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            for (node = data->list; node; node = next)
            {
                next = node->next;
                ventoy_free_path_node_list(node->list);
                free(node);
            }
            data->list = NULL;
        }
        else
        {
            vtoy_list_del_ex(last, node, data->list, path, ventoy_free_path_node_list);            
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

static int ventoy_api_auto_install_add_inner(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    const char *outpath = NULL;
    path_node *pcur = NULL;
    path_node *pnode = NULL;
    auto_install_node *node = NULL;
    data_auto_install *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_auto_install + index;

    path = VTOY_JSON_STR_EX("path");
    outpath = VTOY_JSON_STR_EX("outpath");
    if (path && outpath)
    {
        for (node = data->list; node; node = node->next)
        {
            if (strcmp(outpath, node->path) == 0)
            {
                pnode = zalloc(sizeof(path_node));
                if (pnode)
                {
                    scnprintf(pnode->path, sizeof(pnode->path), "%s", path);
                    vtoy_list_add(node->list, pcur, pnode);
                }
            
                break;
            }
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

static int ventoy_api_auto_install_del_inner(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    const char *outpath = NULL;
    path_node *plast = NULL;
    path_node *pnode = NULL;
    auto_install_node *node = NULL;
    data_auto_install *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_auto_install + index;

    path = VTOY_JSON_STR_EX("path");
    outpath = VTOY_JSON_STR_EX("outpath");
    if (path && outpath)
    {
        for (node = data->list; node; node = node->next)
        {
            if (strcmp(outpath, node->path) == 0)
            {
                vtoy_list_del(plast, pnode, node->list, path);
                break;
            }
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}


#if 0
#endif


void ventoy_data_default_persistence(data_persistence *data)
{
    memset(data, 0, sizeof(data_persistence));
}

int ventoy_data_cmp_persistence(data_persistence *data1, data_persistence *data2)
{
    persistence_node *list1 = NULL;
    persistence_node *list2 = NULL;

    if (NULL == data1->list && NULL == data2->list)
    {
        return 0;
    }
    else if (data1->list && data2->list)
    {
        list1 = data1->list;
        list2 = data2->list;
    
        while (list1 && list2)
        {
            if (list1->timeout != list2->timeout ||
                list1->autosel != list2->autosel ||
                strcmp(list1->path, list2->path))
            {
                return 1;
            }

            /* no need to compare auto install list with default */
            list1 = list1->next;
            list2 = list2->next;
        }

        if (list1 == NULL && list2 == NULL)
        {
            return 0;
        }
        return 1;
    }
    else
    {
        return 1;
    }

    return 0;
}


int ventoy_data_save_persistence(data_persistence *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    persistence_node *node = NULL;
    path_node *pathnode = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_ARY_BEGIN_N();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN_LN(L2);
        VTOY_JSON_FMT_STRN_PATH_LN(L3, "image", node->path);            
        VTOY_JSON_FMT_KEY_L(L3, "backend");
        VTOY_JSON_FMT_ARY_BEGIN_N();
        for (pathnode = node->list; pathnode; pathnode = pathnode->next)
        {
            VTOY_JSON_FMT_ITEM_PATH_LN(L4, pathnode->path);
        }
        VTOY_JSON_FMT_ARY_ENDEX_LN(L3);

        if (node->timeouten)
        {
            VTOY_JSON_FMT_SINT_LN(L3, "timeout", node->timeout);
        }

        if (node->autoselen)
        {
            VTOY_JSON_FMT_SINT_LN(L3, "autosel", node->autosel);                
        }

        VTOY_JSON_FMT_OBJ_ENDEX_LN(L2);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_persistence(data_persistence *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    persistence_node *node = NULL;
    path_node *pathnode = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_STRN("path", node->path);

        valid = ventoy_check_fuzzy_path(node->path, 1);            
        VTOY_JSON_FMT_SINT("valid", valid);
        VTOY_JSON_FMT_SINT("type", node->type);
        
        VTOY_JSON_FMT_BOOL("timeouten", node->timeouten);
        VTOY_JSON_FMT_BOOL("autoselen", node->autoselen);
        
        VTOY_JSON_FMT_SINT("autosel", node->autosel);
        VTOY_JSON_FMT_SINT("timeout", node->timeout);

        VTOY_JSON_FMT_KEY("list");
        VTOY_JSON_FMT_ARY_BEGIN();
        for (pathnode = node->list; pathnode; pathnode = pathnode->next)
        {
            VTOY_JSON_FMT_OBJ_BEGIN();
            VTOY_JSON_FMT_STRN("path", pathnode->path);

            valid = ventoy_is_file_exist("%s%s", g_cur_dir, pathnode->path);
            VTOY_JSON_FMT_SINT("valid", valid);
            VTOY_JSON_FMT_OBJ_ENDEX();
        }
        VTOY_JSON_FMT_ARY_ENDEX(); 

        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}

static int ventoy_api_get_persistence(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, persistence);
    return 0;
}

static int ventoy_api_save_persistence(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int id = -1;
    int cnt = 0;
    int index = 0;
    uint8_t timeouten = 0;
    uint8_t autoselen = 0;
    persistence_node *node = NULL;
    data_persistence *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    vtoy_json_get_int(json, "id", &id);

    vtoy_json_get_bool(json, "timeouten", &timeouten);
    vtoy_json_get_bool(json, "autoselen", &autoselen);
    
    data = g_data_persistence + index;

    if (id >= 0)
    {
        for (node = data->list; node; node = node->next)
        {
            if (cnt == id)
            {
                node->timeouten = (int)timeouten;
                node->autoselen = (int)autoselen;
                VTOY_JSON_INT("timeout", node->timeout);
                VTOY_JSON_INT("autosel", node->autosel);
                break;
            }
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}


static int ventoy_api_persistence_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    persistence_node *node = NULL;
    persistence_node *cur = NULL;
    data_persistence *data = NULL;
    VTOY_JSON *array = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_persistence + index;

    array = vtoy_json_find_item(json, JSON_TYPE_ARRAY, "backend");
    path = VTOY_JSON_STR_EX("path");
    if (path && array)
    {
        if (ventoy_is_real_exist_common(path, data->list, persistence_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }
        
        node = zalloc(sizeof(persistence_node));
        if (node)
        {
            node->timeouten = 0;
            node->autoselen = 0;
            node->autosel = 1;
            node->timeout = 0;
            scnprintf(node->path, sizeof(node->path), "%s", path);
            node->list = ventoy_path_node_add_array(array);
            
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET); 
    return 0;
}

static int ventoy_api_persistence_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    persistence_node *last = NULL;
    persistence_node *next = NULL;
    persistence_node *node = NULL;
    data_persistence *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_persistence + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            for (node = data->list; node; node = next)
            {
                next = node->next;
                ventoy_free_path_node_list(node->list);
                free(node);
            }
            data->list = NULL;
        }
        else
        {
            vtoy_list_del_ex(last, node, data->list, path, ventoy_free_path_node_list);            
        }    
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}

static int ventoy_api_persistence_add_inner(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    const char *outpath = NULL;
    path_node *pcur = NULL;
    path_node *pnode = NULL;
    persistence_node *node = NULL;
    data_persistence *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_persistence + index;

    path = VTOY_JSON_STR_EX("path");
    outpath = VTOY_JSON_STR_EX("outpath");
    if (path && outpath)
    {
        for (node = data->list; node; node = node->next)
        {
            if (strcmp(outpath, node->path) == 0)
            {
                pnode = zalloc(sizeof(path_node));
                if (pnode)
                {
                    scnprintf(pnode->path, sizeof(pnode->path), "%s", path);
                    vtoy_list_add(node->list, pcur, pnode);
                }
            
                break;
            }
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

static int ventoy_api_persistence_del_inner(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    const char *outpath = NULL;
    path_node *plast = NULL;
    path_node *pnode = NULL;
    persistence_node *node = NULL;
    data_persistence *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_persistence + index;

    path = VTOY_JSON_STR_EX("path");
    outpath = VTOY_JSON_STR_EX("outpath");
    if (path && outpath)
    {
        for (node = data->list; node; node = node->next)
        {
            if (strcmp(outpath, node->path) == 0)
            {
                vtoy_list_del(plast, pnode, node->list, path);
                break;
            }
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);  
    return 0;
}


#if 0
#endif

void ventoy_data_default_injection(data_injection *data)
{
    memset(data, 0, sizeof(data_injection));
}

int ventoy_data_cmp_injection(data_injection *data1, data_injection *data2)
{
    injection_node *list1 = NULL;
    injection_node *list2 = NULL;

    if (NULL == data1->list && NULL == data2->list)
    {
        return 0;
    }
    else if (data1->list && data2->list)
    {
        list1 = data1->list;
        list2 = data2->list;
    
        while (list1 && list2)
        {
            if ((list1->type != list2->type) ||
                strcmp(list1->path, list2->path) || 
                strcmp(list1->archive, list2->archive))
            {
                return 1;
            }
        
            list1 = list1->next;
            list2 = list2->next;
        }

        if (list1 == NULL && list2 == NULL)
        {
            return 0;
        }
        return 1;
    }
    else
    {
        return 1;
    }

    return 0;
}


int ventoy_data_save_injection(data_injection *data, const char *title, char *buf, int buflen)
{
    int pos = 0;
    injection_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);

    VTOY_JSON_FMT_KEY_L(L1, title);
    VTOY_JSON_FMT_ARY_BEGIN_N();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN_LN(L2);

        if (node->type == 0)
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L3, "image", node->path);            
        }
        else
        {
            VTOY_JSON_FMT_STRN_PATH_LN(L3, "parent", node->path);
        }
        VTOY_JSON_FMT_STRN_PATH_LN(L3, "archive", node->archive);
        
        VTOY_JSON_FMT_OBJ_ENDEX_LN(L2);
    }

    VTOY_JSON_FMT_ARY_ENDEX_LN(L1);
    VTOY_JSON_FMT_END(pos);

    return pos;
}


int ventoy_data_json_injection(data_injection *data, char *buf, int buflen)
{
    int pos = 0;
    int valid = 0;
    injection_node *node = NULL;
    
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_ARY_BEGIN();

    for (node = data->list; node; node = node->next)
    {
        VTOY_JSON_FMT_OBJ_BEGIN();

        VTOY_JSON_FMT_UINT("type", node->type);
        VTOY_JSON_FMT_STRN("path", node->path);       

        if (node->type == 0)
        {
            valid = ventoy_check_fuzzy_path(node->path, 1);
        }
        else
        {
            valid = ventoy_is_directory_exist("%s%s", g_cur_dir, node->path);
        }
        VTOY_JSON_FMT_SINT("valid", valid);
        
        VTOY_JSON_FMT_STRN("archive", node->archive);

        valid = ventoy_is_file_exist("%s%s", g_cur_dir, node->archive);
        VTOY_JSON_FMT_SINT("archive_valid", valid);
        
        VTOY_JSON_FMT_OBJ_ENDEX();
    }

    VTOY_JSON_FMT_ARY_END();
    VTOY_JSON_FMT_END(pos);

    return pos;
}


static int ventoy_api_get_injection(struct mg_connection *conn, VTOY_JSON *json)
{
    api_get_func(conn, json, injection);
    return 0;
}

static int ventoy_api_save_injection(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

static int ventoy_api_injection_add(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    int type = 0;
    const char *path = NULL;
    const char *archive = NULL;
    injection_node *node = NULL;
    injection_node *cur = NULL;
    data_injection *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_injection + index;

    vtoy_json_get_int(json, "type", &type);

    path = VTOY_JSON_STR_EX("path");
    archive = VTOY_JSON_STR_EX("archive");
    if (path && archive)
    {
        if (ventoy_is_real_exist_common(path, data->list, injection_node))
        {
            ventoy_json_result(conn, VTOY_JSON_DUPLICATE);
            return 0;
        }
    
        node = zalloc(sizeof(injection_node));
        if (node)
        {
            node->type = type;
        
            scnprintf(node->path, sizeof(node->path), "%s", path);
            scnprintf(node->archive, sizeof(node->archive), "%s", archive);
            
            vtoy_list_add(data->list, cur, node);
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

static int ventoy_api_injection_del(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int index = 0;
    const char *path = NULL;
    injection_node *last = NULL;
    injection_node *node = NULL;
    data_injection *data = NULL;
    
    vtoy_json_get_int(json, "index", &index);
    data = g_data_injection + index;

    path = VTOY_JSON_STR_EX("path");
    if (path)
    {
        if (strcmp(path, VTOY_DEL_ALL_PATH) == 0)
        {
            vtoy_list_free(injection_node, data->list);
        }
        else
        {
            vtoy_list_del(last, node, data->list, path);            
        }
    }

    ret = ventoy_data_save_all();

    ventoy_json_result(conn, ret == 0 ? VTOY_JSON_SUCCESS_RET : VTOY_JSON_FAILED_RET);   
    return 0;
}

#if 0
#endif

static int ventoy_api_preview_json(struct mg_connection *conn, VTOY_JSON *json)
{
    int i = 0;
    int pos = 0;
    int len = 0;
    int utf16enclen = 0;
    char *encodebuf = NULL;
    unsigned short *utf16buf = NULL;
    
    (void)json;

    /* We can not use json directly, because it will be formated in the JS. */

    len = ventoy_data_real_save_all(0);

    utf16buf = (unsigned short *)malloc(2 * len + 16);    
    if (!utf16buf)
    {
        goto json;
    }

    utf16enclen = (int)utf8_to_utf16((unsigned char *)JSON_SAVE_BUFFER, len, utf16buf, len + 2);

    encodebuf = (char *)malloc(utf16enclen * 4 + 16);
    if (!encodebuf)
    {
        goto json;
    }

    for (i = 0; i < utf16enclen; i++)
    {
        scnprintf(encodebuf + i * 4, 5, "%04X", utf16buf[i]);
    }

json:
    VTOY_JSON_FMT_BEGIN(pos, JSON_BUFFER, JSON_BUF_MAX);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_STRN("json", (encodebuf ? encodebuf : ""));
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    CHECK_FREE(encodebuf);
    CHECK_FREE(utf16buf);

    ventoy_json_buffer(conn, JSON_BUFFER, pos);
    return 0;
}


#if 0
#endif

int ventoy_data_save_all(void)
{
    ventoy_set_writeback_event();
    return 0;
}

int ventoy_data_real_save_all(int apilock)
{
    int i = 0;
    int pos = 0;
    char title[64];

    if (apilock)
    {
        pthread_mutex_lock(&g_api_mutex);        
    }

    ssprintf(pos, JSON_SAVE_BUFFER, JSON_BUF_MAX, "{\n");

    ventoy_save_plug(control);
    ventoy_save_plug(theme);
    ventoy_save_plug(menu_alias);
    ventoy_save_plug(menu_tip);
    ventoy_save_plug(menu_class);
    ventoy_save_plug(auto_install);
    ventoy_save_plug(persistence);
    ventoy_save_plug(injection);
    ventoy_save_plug(conf_replace);
    ventoy_save_plug(password);
    ventoy_save_plug(image_list);
    ventoy_save_plug(auto_memdisk);
    ventoy_save_plug(dud);
    
    if (JSON_SAVE_BUFFER[pos - 1] == '\n' && JSON_SAVE_BUFFER[pos - 2] == ',')
    {
        JSON_SAVE_BUFFER[pos - 2] = '\n';
        pos--;
    }
    ssprintf(pos, JSON_SAVE_BUFFER, JSON_BUF_MAX, "}\n");

    if (apilock)
    {
        pthread_mutex_unlock(&g_api_mutex);        
    }

    return pos;
}

int ventoy_http_writeback(void)
{
    int ret;
    int pos;
    char filename[128];

    ventoy_get_json_path(filename, NULL);

    pos = ventoy_data_real_save_all(1);
        
    #ifdef VENTOY_SIM
    printf("%s", JSON_SAVE_BUFFER);
    #endif
    
    ret = ventoy_write_buf_to_file(filename, JSON_SAVE_BUFFER, pos);
    if (ret)
    {
        vlog("Failed to write ventoy.json file.\n");
        g_sysinfo.config_save_error = 1;
    }

    return 0;
}


static JSON_CB g_ventoy_json_cb[] = 
{
    { "sysinfo",        ventoy_api_sysinfo            },    
    { "handshake",      ventoy_api_handshake          },    
    { "check_path",     ventoy_api_check_exist        },    
    { "check_path2",    ventoy_api_check_exist2       },    
    { "check_fuzzy",    ventoy_api_check_fuzzy        },
    
    { "device_info",    ventoy_api_device_info        },    
    
    { "get_control",    ventoy_api_get_control        },    
    { "save_control",   ventoy_api_save_control       },  
    
    { "get_theme",      ventoy_api_get_theme          },    
    { "save_theme",     ventoy_api_save_theme         },    
    { "theme_add_file", ventoy_api_theme_add_file     },    
    { "theme_del_file", ventoy_api_theme_del_file     },    
    { "theme_add_font", ventoy_api_theme_add_font     },    
    { "theme_del_font", ventoy_api_theme_del_font     },
    
    { "get_alias",      ventoy_api_get_alias          },
    { "save_alias",     ventoy_api_save_alias         },   
    { "alias_add",      ventoy_api_alias_add          },
    { "alias_del",      ventoy_api_alias_del          },
    
    { "get_tip",        ventoy_api_get_tip            },
    { "save_tip",       ventoy_api_save_tip           },   
    { "tip_add",        ventoy_api_tip_add            },
    { "tip_del",        ventoy_api_tip_del            },
    
    { "get_class",      ventoy_api_get_class          },
    { "save_class",     ventoy_api_save_class         },   
    { "class_add",      ventoy_api_class_add          },
    { "class_del",      ventoy_api_class_del          },
    
    { "get_auto_memdisk",  ventoy_api_get_auto_memdisk  },
    { "save_auto_memdisk", ventoy_api_save_auto_memdisk },   
    { "auto_memdisk_add",  ventoy_api_auto_memdisk_add  },
    { "auto_memdisk_del",  ventoy_api_auto_memdisk_del  },
    
    { "get_image_list",  ventoy_api_get_image_list  },
    { "save_image_list", ventoy_api_save_image_list },
    { "image_list_add",  ventoy_api_image_list_add  },
    { "image_list_del",  ventoy_api_image_list_del  },

    { "get_conf_replace",      ventoy_api_get_conf_replace      },
    { "save_conf_replace",     ventoy_api_save_conf_replace     },   
    { "conf_replace_add",      ventoy_api_conf_replace_add      },
    { "conf_replace_del",      ventoy_api_conf_replace_del      },
    
    { "get_dud",            ventoy_api_get_dud      },
    { "save_dud",           ventoy_api_save_dud     },   
    { "dud_add",            ventoy_api_dud_add      },
    { "dud_del",            ventoy_api_dud_del      },
    { "dud_add_inner",      ventoy_api_dud_add_inner      },
    { "dud_del_inner",      ventoy_api_dud_del_inner      },
    
    { "get_auto_install",            ventoy_api_get_auto_install      },
    { "save_auto_install",           ventoy_api_save_auto_install     },   
    { "auto_install_add",            ventoy_api_auto_install_add      },
    { "auto_install_del",            ventoy_api_auto_install_del      },
    { "auto_install_add_inner",      ventoy_api_auto_install_add_inner      },
    { "auto_install_del_inner",      ventoy_api_auto_install_del_inner      },
    
    { "get_persistence",            ventoy_api_get_persistence      },
    { "save_persistence",           ventoy_api_save_persistence     },   
    { "persistence_add",            ventoy_api_persistence_add      },
    { "persistence_del",            ventoy_api_persistence_del      },
    { "persistence_add_inner",      ventoy_api_persistence_add_inner      },
    { "persistence_del_inner",      ventoy_api_persistence_del_inner      },
    
    { "get_password",      ventoy_api_get_password      },
    { "save_password",     ventoy_api_save_password     },   
    { "password_add",      ventoy_api_password_add      },
    { "password_del",      ventoy_api_password_del      },
    
    { "get_injection",      ventoy_api_get_injection     },
    { "save_injection",     ventoy_api_save_injection     },   
    { "injection_add",      ventoy_api_injection_add      },
    { "injection_del",      ventoy_api_injection_del      },
    { "preview_json",       ventoy_api_preview_json       },
    
};

static int ventoy_json_handler(struct mg_connection *conn, VTOY_JSON *json, char *jsonstr)
{
    int i;
    const char *method = NULL;

    method = vtoy_json_get_string_ex(json, "method");
    if (!method)
    {
        ventoy_json_result(conn, VTOY_JSON_SUCCESS_RET);
        return 0;
    }

    if (strcmp(method, "handshake") == 0)
    {
        ventoy_api_handshake(conn, json);
        return 0;
    }

    for (i = 0; i < (int)(sizeof(g_ventoy_json_cb) / sizeof(g_ventoy_json_cb[0])); i++)
    {
        if (strcmp(method, g_ventoy_json_cb[i].method) == 0)
        {
            g_ventoy_json_cb[i].callback(conn, json);
            break;
        }
    }

    return 0;
}

static int ventoy_request_handler(struct mg_connection *conn)
{
    int post_data_len;
    int post_buf_len;
    VTOY_JSON *json = NULL;
    char *post_data_buf = NULL;
    const struct mg_request_info *ri = NULL;
    char stack_buf[512];
    
    ri = mg_get_request_info(conn);    

    if (strcmp(ri->uri, "/vtoy/json") == 0)
    {
        if (ri->content_length > 500)
        {
            post_data_buf = malloc((int)(ri->content_length + 4));
            post_buf_len  = (int)(ri->content_length + 1);
        }
        else
        {
            post_data_buf = stack_buf;
            post_buf_len = sizeof(stack_buf);
        }
        
        post_data_len = mg_read(conn, post_data_buf, post_buf_len);
        post_data_buf[post_data_len] = 0;

        json = vtoy_json_create();
        if (JSON_SUCCESS == vtoy_json_parse(json, post_data_buf))
        {
            pthread_mutex_lock(&g_api_mutex);
            ventoy_json_handler(conn, json->pstChild, post_data_buf);
            pthread_mutex_unlock(&g_api_mutex);
        }
        else
        {
            ventoy_json_result(conn, VTOY_JSON_INVALID_RET);
        }

        vtoy_json_destroy(json);

        if (post_data_buf != stack_buf)
        {
            free(post_data_buf);
        }
        return 1;
    }
    else
    {
        return 0;
    }
}

const char *ventoy_web_openfile(const struct mg_connection *conn, const char *path, size_t *data_len)
{
    ventoy_file *node = NULL;

    (void)conn;

    if (!path)
    {
        return NULL;
    }
    
    node = ventoy_tar_find_file(path);
    if (node)
    {
        *data_len = node->size;
        return node->addr;
    }
    else
    {
        return NULL;
    }
}

#if 0
#endif

static int ventoy_parse_control(VTOY_JSON *json, void *p)
{
    int i;
    VTOY_JSON *node = NULL;
    VTOY_JSON *child = NULL;
    data_control *data = (data_control *)p;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        return 0;
    }

    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType == JSON_TYPE_OBJECT)
        {
            child = node->pstChild;
            
            if (child->enDataType != JSON_TYPE_STRING)
            {
                continue;
            }

            if (strcmp(child->pcName, "VTOY_DEFAULT_MENU_MODE") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->default_menu_mode);
            }
            else if (strcmp(child->pcName, "VTOY_WIN11_BYPASS_CHECK") == 0)
            {
                CONTROL_PARSE_INT_DEF_1(child, data->win11_bypass_check);
            }
            else if (strcmp(child->pcName, "VTOY_WIN11_BYPASS_NRO") == 0)
            {
                CONTROL_PARSE_INT_DEF_1(child, data->win11_bypass_nro);
            }
            else if (strcmp(child->pcName, "VTOY_LINUX_REMOUNT") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->linux_remount);
            }
            else if (strcmp(child->pcName, "VTOY_SECONDARY_BOOT_MENU") == 0)
            {
                CONTROL_PARSE_INT_DEF_1(child, data->secondary_menu);
            }
            else if (strcmp(child->pcName, "VTOY_SHOW_PASSWORD_ASTERISK") == 0)
            {
                CONTROL_PARSE_INT_DEF_1(child, data->password_asterisk);
            }
            else if (strcmp(child->pcName, "VTOY_TREE_VIEW_MENU_STYLE") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->treeview_style);
            }
            else if (strcmp(child->pcName, "VTOY_FILT_DOT_UNDERSCORE_FILE") == 0)
            {
                CONTROL_PARSE_INT_DEF_1(child, data->filter_dot_underscore);
            }
            else if (strcmp(child->pcName, "VTOY_SORT_CASE_SENSITIVE") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->sort_casesensitive);
            }
            else if (strcmp(child->pcName, "VTOY_MAX_SEARCH_LEVEL") == 0)
            {
                if (strcmp(child->unData.pcStrVal, "max") == 0)
                {
                    data->max_search_level = -1;
                }
                else
                {
                    data->max_search_level = (int)strtol(child->unData.pcStrVal, NULL, 10);
                }
            }
            else if (strcmp(child->pcName, "VTOY_DEFAULT_SEARCH_ROOT") == 0)
            {
                strlcpy(data->default_search_root, child->unData.pcStrVal);
            }
            else if (strcmp(child->pcName, "VTOY_DEFAULT_IMAGE") == 0)
            {
                strlcpy(data->default_image, child->unData.pcStrVal);
            }
            else if (strcmp(child->pcName, "VTOY_DEFAULT_KBD_LAYOUT") == 0)
            {
                for (i = 0; g_ventoy_kbd_layout[i]; i++)
                {
                    if (strcmp(child->unData.pcStrVal, g_ventoy_kbd_layout[i]) == 0)
                    {
                        strlcpy(data->default_kbd_layout, child->unData.pcStrVal);
                        break;
                    }
                }
            }
            else if (strcmp(child->pcName, "VTOY_MENU_LANGUAGE") == 0)
            {
                for (i = 0; g_ventoy_menu_lang[i][0]; i++)
                {
                    if (strcmp(child->unData.pcStrVal, g_ventoy_menu_lang[i]) == 0)
                    {
                        strlcpy(data->menu_language, child->unData.pcStrVal);
                        break;
                    }
                }
            }
            else if (strcmp(child->pcName, "VTOY_MENU_TIMEOUT") == 0)
            {
                data->menu_timeout = (int)strtol(child->unData.pcStrVal, NULL, 10);
            }
            else if (strcmp(child->pcName, "VTOY_SECONDARY_TIMEOUT") == 0)
            {
                data->secondary_menu_timeout = (int)strtol(child->unData.pcStrVal, NULL, 10);
            }
            else if (strcmp(child->pcName, "VTOY_VHD_NO_WARNING") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->vhd_no_warning);
            }
            else if (strcmp(child->pcName, "VTOY_FILE_FLT_ISO") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->filter_iso);
            }
            else if (strcmp(child->pcName, "VTOY_FILE_FLT_IMG") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->filter_img);
            }
            else if (strcmp(child->pcName, "VTOY_FILE_FLT_EFI") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->filter_efi);
            }
            else if (strcmp(child->pcName, "VTOY_FILE_FLT_WIM") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->filter_wim);
            }
            else if (strcmp(child->pcName, "VTOY_FILE_FLT_VHD") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->filter_vhd);
            }
            else if (strcmp(child->pcName, "VTOY_FILE_FLT_VTOY") == 0)
            {
                CONTROL_PARSE_INT_DEF_0(child, data->filter_vtoy);
            }
            
        }
    }

    return 0;
}
static int ventoy_parse_theme(VTOY_JSON *json, void *p)
{
    const char *dismode = NULL;
    VTOY_JSON *child = NULL;
    VTOY_JSON *node = NULL;
    path_node *tail = NULL;
    path_node *pnode = NULL;
    data_theme *data = (data_theme *)p;

    if (json->enDataType != JSON_TYPE_OBJECT)
    {
        return 0;
    }

    child = json->pstChild;

    dismode = vtoy_json_get_string_ex(child, "display_mode");
    vtoy_json_get_string(child, "ventoy_left", sizeof(data->ventoy_left), data->ventoy_left);
    vtoy_json_get_string(child, "ventoy_top", sizeof(data->ventoy_top), data->ventoy_top);
    vtoy_json_get_string(child, "ventoy_color", sizeof(data->ventoy_color), data->ventoy_color);
    
    vtoy_json_get_int(child, "default_file", &(data->default_file));    
    vtoy_json_get_int(child, "resolution_fit", &(data->resolution_fit));    
    vtoy_json_get_string(child, "gfxmode", sizeof(data->gfxmode), data->gfxmode);
    vtoy_json_get_string(child, "serial_param", sizeof(data->serial_param), data->serial_param);

    if (dismode)
    {
        if (strcmp(dismode, "CLI") == 0)
        {
            data->display_mode = display_mode_cli;
        }
        else if (strcmp(dismode, "serial") == 0)
        {
            data->display_mode = display_mode_serial;
        }
        else if (strcmp(dismode, "serial_console") == 0)
        {
            data->display_mode = display_mode_ser_console;
        }
        else
        {
            data->display_mode = display_mode_gui;
        }
    }

    node = vtoy_json_find_item(child, JSON_TYPE_STRING, "file");
    if (node)
    {
        data->default_file = 0;
        data->resolution_fit = 0;

        pnode = zalloc(sizeof(path_node));
        if (pnode)
        {
            strlcpy(pnode->path, node->unData.pcStrVal);
            data->filelist = pnode;
        }
    }
    else
    {
        node = vtoy_json_find_item(child, JSON_TYPE_ARRAY, "file");
        if (node)
        {
            for (node = node->pstChild; node; node = node->pstNext)
            {
                if (node->enDataType == JSON_TYPE_STRING)
                {
                    pnode = zalloc(sizeof(path_node));
                    if (pnode)
                    {
                        strlcpy(pnode->path, node->unData.pcStrVal);
                        if (data->filelist)
                        {
                            tail->next = pnode;
                            tail = pnode;
                        }
                        else
                        {
                            data->filelist = tail = pnode;
                        }
                    }
                }
            }
        }
    }

    
    node = vtoy_json_find_item(child, JSON_TYPE_ARRAY, "fonts");
    if (node)
    {
        for (node = node->pstChild; node; node = node->pstNext)
        {
            if (node->enDataType == JSON_TYPE_STRING)
            {
                pnode = zalloc(sizeof(path_node));
                if (pnode)
                {
                    strlcpy(pnode->path, node->unData.pcStrVal);
                    if (data->fontslist)
                    {
                        tail->next = pnode;
                        tail = pnode;
                    }
                    else
                    {
                        data->fontslist = tail = pnode;
                    }
                }
            }
        }
    }

    return 0;
}
static int ventoy_parse_menu_alias(VTOY_JSON *json, void *p)
{
    int type;
    const char *path = NULL;
    const char *alias = NULL;
    data_alias *data = (data_alias *)p;
    data_alias_node *tail = NULL;
    data_alias_node *pnode = NULL;
    VTOY_JSON *node = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        return 0;
    }

    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType != JSON_TYPE_OBJECT)
        {
            continue;
        }

        type = path_type_file;
        path = vtoy_json_get_string_ex(node->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(node->pstChild, "dir");
            type = path_type_dir;
        }
        alias = vtoy_json_get_string_ex(node->pstChild, "alias");

        if (path && alias)
        {
            pnode = zalloc(sizeof(data_alias_node));
            if (pnode)
            {
                pnode->type = type;
                strlcpy(pnode->path, path);
                strlcpy(pnode->alias, alias);
                
                if (data->list)
                {
                    tail->next = pnode;
                    tail = pnode;
                }
                else
                {
                    data->list = tail = pnode;
                }
            }
        }
    }    

    return 0;
}

static int ventoy_parse_menu_tip(VTOY_JSON *json, void *p)
{
    int type;
    const char *path = NULL;
    const char *tip = NULL;
    data_tip *data = (data_tip *)p;
    data_tip_node *tail = NULL;
    data_tip_node *pnode = NULL;
    VTOY_JSON *node = NULL;
    VTOY_JSON *tips = NULL;

    if (json->enDataType != JSON_TYPE_OBJECT)
    {
        return 0;
    }

    vtoy_json_get_string(json->pstChild, "left", sizeof(data->left), data->left);
    vtoy_json_get_string(json->pstChild, "top", sizeof(data->top), data->top);
    vtoy_json_get_string(json->pstChild, "color", sizeof(data->color), data->color);

    tips = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "tips");
    if (!tips)
    {
        return 0;
    }

    for (node = tips->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType != JSON_TYPE_OBJECT)
        {
            continue;
        }

        type = path_type_file;
        path = vtoy_json_get_string_ex(node->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(node->pstChild, "dir");
            type = path_type_dir;
        }
        tip = vtoy_json_get_string_ex(node->pstChild, "tip");

        if (path && tip)
        {
            pnode = zalloc(sizeof(data_tip_node));
            if (pnode)
            {
                pnode->type = type;
                strlcpy(pnode->path, path);
                strlcpy(pnode->tip, tip);
                
                if (data->list)
                {
                    tail->next = pnode;
                    tail = pnode;
                }
                else
                {
                    data->list = tail = pnode;
                }
            }
        }
    }    

    return 0;
}
static int ventoy_parse_menu_class(VTOY_JSON *json, void *p)
{
    int type;
    const char *path = NULL;
    const char *class = NULL;
    data_class *data = (data_class *)p;
    data_class_node *tail = NULL;
    data_class_node *pnode = NULL;
    VTOY_JSON *node = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        return 0;
    }

    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType != JSON_TYPE_OBJECT)
        {
            continue;
        }

        type = class_type_key;
        path = vtoy_json_get_string_ex(node->pstChild, "key");
        if (!path)
        {
            type = class_type_dir;
            path = vtoy_json_get_string_ex(node->pstChild, "dir");
            if (!path)
            {
                type = class_type_parent;
                path = vtoy_json_get_string_ex(node->pstChild, "parent");
            }
        }
        class = vtoy_json_get_string_ex(node->pstChild, "class");

        if (path && class)
        {
            pnode = zalloc(sizeof(data_class_node));
            if (pnode)
            {
                pnode->type = type;
                strlcpy(pnode->path, path);
                strlcpy(pnode->class, class);
                
                if (data->list)
                {
                    tail->next = pnode;
                    tail = pnode;
                }
                else
                {
                    data->list = tail = pnode;
                }
            }
        }
    }    

    return 0;
}
static int ventoy_parse_auto_install(VTOY_JSON *json, void *p)
{
    int type;
    int count;
    int timeout;
    int timeouten;
    int autosel;
    int autoselen;
    const char *path = NULL;
    const char *file = NULL;
    data_auto_install *data = (data_auto_install *)p;
    auto_install_node *tail = NULL;
    auto_install_node *pnode = NULL;
    path_node *pathnode = NULL;
    path_node *pathtail = NULL;
    VTOY_JSON *node = NULL;
    VTOY_JSON *filelist = NULL;
    VTOY_JSON *filenode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        return 0;
    }

    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType != JSON_TYPE_OBJECT)
        {
            continue;
        }

        type = 0;
        path = vtoy_json_get_string_ex(node->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(node->pstChild, "parent");
            type = 1;
        }
        if (!path)
        {
            continue;
        }
                
        file = vtoy_json_get_string_ex(node->pstChild, "template");
        if (file)
        {
            pnode = zalloc(sizeof(auto_install_node));
            if (pnode)
            {
                pnode->type = type;
                pnode->autosel = 1;
                strlcpy(pnode->path, path);

                pathnode = zalloc(sizeof(path_node));
                if (pathnode)
                {
                    strlcpy(pathnode->path, file);
                    pnode->list = pathnode;
                }
                else
                {
                    free(pnode);
                }

                if (data->list)
                {
                    tail->next = pnode;
                    tail = pnode;
                }
                else
                {
                    data->list = tail = pnode;
                }
            }

            continue;
        }


        timeouten = autoselen = 0;
        if (JSON_SUCCESS == vtoy_json_get_int(node->pstChild, "timeout", &timeout))
        {
            timeouten = 1;
        }
        if (JSON_SUCCESS == vtoy_json_get_int(node->pstChild, "autosel", &autosel))
        {
            autoselen = 1;
        }

        filelist = vtoy_json_find_item(node->pstChild, JSON_TYPE_ARRAY, "template");
        if (!filelist)
        {
            continue;
        }

        pnode = zalloc(sizeof(auto_install_node));
        if (!pnode)
        {
            continue;
        }

        pnode->type = type;
        pnode->autoselen = autoselen;
        pnode->timeouten = timeouten;
        pnode->timeout = timeout;
        pnode->autosel = autosel;
        strlcpy(pnode->path, path);

        count = 0;
        for (filenode = filelist->pstChild; filenode; filenode = filenode->pstNext)
        {
            if (filenode->enDataType != JSON_TYPE_STRING)
            {
                continue;
            }

            pathnode = zalloc(sizeof(path_node));
            if (pathnode)
            {
                count++;
                strlcpy(pathnode->path, filenode->unData.pcStrVal);

                if (pnode->list)
                {
                    pathtail->next = pathnode;
                    pathtail = pathnode;
                }
                else
                {
                    pnode->list = pathtail = pathnode;
                }                
            }
        }

        if (count == 0)
        {
            free(pnode);
        }
        else
        {
            if (pnode->autoselen && pnode->autosel > count)
            {
                pnode->autosel = 1;
            }

            if (data->list)
            {
                tail->next = pnode;
                tail = pnode;
            }
            else
            {
                data->list = tail = pnode;
            }
        }
    }

    return 0;
}
static int ventoy_parse_persistence(VTOY_JSON *json, void *p)
{
    int count;
    int timeout;
    int timeouten;
    int autosel;
    int autoselen;
    const char *path = NULL;
    const char *file = NULL;
    data_persistence *data = (data_persistence *)p;
    persistence_node *tail = NULL;
    persistence_node *pnode = NULL;
    path_node *pathnode = NULL;
    path_node *pathtail = NULL;
    VTOY_JSON *node = NULL;
    VTOY_JSON *filelist = NULL;
    VTOY_JSON *filenode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        return 0;
    }

    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType != JSON_TYPE_OBJECT)
        {
            continue;
        }

        path = vtoy_json_get_string_ex(node->pstChild, "image");
        if (!path)
        {
            continue;
        }
                
        file = vtoy_json_get_string_ex(node->pstChild, "backend");
        if (file)
        {
            pnode = zalloc(sizeof(persistence_node));
            if (pnode)
            {
                pnode->type = 0;
                pnode->autosel = 1;
                strlcpy(pnode->path, path);

                pathnode = zalloc(sizeof(path_node));
                if (pathnode)
                {
                    strlcpy(pathnode->path, file);
                    pnode->list = pathnode;
                }
                else
                {
                    free(pnode);
                }

                if (data->list)
                {
                    tail->next = pnode;
                    tail = pnode;
                }
                else
                {
                    data->list = tail = pnode;
                }
            }

            continue;
        }


        timeouten = autoselen = 0;
        if (JSON_SUCCESS == vtoy_json_get_int(node->pstChild, "timeout", &timeout))
        {
            timeouten = 1;
        }
        if (JSON_SUCCESS == vtoy_json_get_int(node->pstChild, "autosel", &autosel))
        {
            autoselen = 1;
        }

        filelist = vtoy_json_find_item(node->pstChild, JSON_TYPE_ARRAY, "backend");
        if (!filelist)
        {
            continue;
        }

        pnode = zalloc(sizeof(persistence_node));
        if (!pnode)
        {
            continue;
        }

        pnode->type = 0;
        pnode->autoselen = autoselen;
        pnode->timeouten = timeouten;
        pnode->timeout = timeout;
        pnode->autosel = autosel;
        strlcpy(pnode->path, path);

        count = 0;
        for (filenode = filelist->pstChild; filenode; filenode = filenode->pstNext)
        {
            if (filenode->enDataType != JSON_TYPE_STRING)
            {
                continue;
            }

            pathnode = zalloc(sizeof(path_node));
            if (pathnode)
            {
                count++;
                strlcpy(pathnode->path, filenode->unData.pcStrVal);

                if (pnode->list)
                {
                    pathtail->next = pathnode;
                    pathtail = pathnode;
                }
                else
                {
                    pnode->list = pathtail = pathnode;
                }                
            }
        }

        if (count == 0)
        {
            free(pnode);
        }
        else
        {
            if (pnode->autoselen && pnode->autosel > count)
            {
                pnode->autosel = 1;
            }

            if (data->list)
            {
                tail->next = pnode;
                tail = pnode;
            }
            else
            {
                data->list = tail = pnode;
            }
        }
    }

    return 0;
}
static int ventoy_parse_injection(VTOY_JSON *json, void *p)
{
    int type;
    const char *path = NULL;
    const char *archive = NULL;
    data_injection *data = (data_injection *)p;
    injection_node *tail = NULL;
    injection_node *pnode = NULL;
    VTOY_JSON *node = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        return 0;
    }

    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType != JSON_TYPE_OBJECT)
        {
            continue;
        }

        type = 0;
        path = vtoy_json_get_string_ex(node->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(node->pstChild, "parent");
            type = 1;
        }
        archive = vtoy_json_get_string_ex(node->pstChild, "archive");

        if (path && archive)
        {
            pnode = zalloc(sizeof(injection_node));
            if (pnode)
            {
                pnode->type = type;
                strlcpy(pnode->path, path);
                strlcpy(pnode->archive, archive);
                
                if (data->list)
                {
                    tail->next = pnode;
                    tail = pnode;
                }
                else
                {
                    data->list = tail = pnode;
                }
            }
        }
    }    

    return 0;
}
static int ventoy_parse_conf_replace(VTOY_JSON *json, void *p)
{
    int img = 0;
    const char *path = NULL;
    const char *org = NULL;
    const char *new = NULL;
    data_conf_replace *data = (data_conf_replace *)p;
    conf_replace_node *tail = NULL;
    conf_replace_node *pnode = NULL;
    VTOY_JSON *node = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        return 0;
    }

    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType != JSON_TYPE_OBJECT)
        {
            continue;
        }

        path = vtoy_json_get_string_ex(node->pstChild, "iso");
        org = vtoy_json_get_string_ex(node->pstChild, "org");
        new = vtoy_json_get_string_ex(node->pstChild, "new");

        img = 0;
        vtoy_json_get_int(node->pstChild, "img", &img);

        if (path && org && new)
        {
            pnode = zalloc(sizeof(conf_replace_node));
            if (pnode)
            {
                strlcpy(pnode->path, path);
                strlcpy(pnode->org, org);
                strlcpy(pnode->new, new);
                if (img == 1)
                {
                    pnode->image = img;                    
                }
                
                if (data->list)
                {
                    tail->next = pnode;
                    tail = pnode;
                }
                else
                {
                    data->list = tail = pnode;
                }
            }
        }
    }    

    return 0;
}
static int ventoy_parse_password(VTOY_JSON *json, void *p)
{
    int type;
    const char *bootpwd = NULL;
    const char *isopwd= NULL;
    const char *wimpwd= NULL;
    const char *imgpwd= NULL;
    const char *efipwd= NULL;
    const char *vhdpwd= NULL;
    const char *vtoypwd= NULL;
    const char *path = NULL;
    const char *pwd = NULL;
    data_password *data = (data_password *)p;
    menu_password *tail = NULL;
    menu_password *pnode = NULL;
    VTOY_JSON *node = NULL;
    VTOY_JSON *menupwd = NULL;

    if (json->enDataType != JSON_TYPE_OBJECT)
    {
        return 0;
    }

    bootpwd = vtoy_json_get_string_ex(json->pstChild, "bootpwd");
    isopwd = vtoy_json_get_string_ex(json->pstChild, "isopwd");
    wimpwd = vtoy_json_get_string_ex(json->pstChild, "wimpwd");
    imgpwd = vtoy_json_get_string_ex(json->pstChild, "imgpwd");
    efipwd = vtoy_json_get_string_ex(json->pstChild, "efipwd");
    vhdpwd = vtoy_json_get_string_ex(json->pstChild, "vhdpwd");
    vtoypwd = vtoy_json_get_string_ex(json->pstChild, "vtoypwd");


    if (bootpwd) strlcpy(data->bootpwd, bootpwd);
    if (isopwd) strlcpy(data->isopwd, isopwd);
    if (wimpwd) strlcpy(data->wimpwd, wimpwd);
    if (imgpwd) strlcpy(data->imgpwd, imgpwd);
    if (efipwd) strlcpy(data->efipwd, efipwd);
    if (vhdpwd) strlcpy(data->vhdpwd, vhdpwd);
    if (vtoypwd) strlcpy(data->vtoypwd, vtoypwd);
    
    
    menupwd = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "menupwd");
    if (!menupwd)
    {
        return 0;
    }

    for (node = menupwd->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType != JSON_TYPE_OBJECT)
        {
            continue;
        }

        type = 0;
        path = vtoy_json_get_string_ex(node->pstChild, "file");
        if (!path)
        {
            path = vtoy_json_get_string_ex(node->pstChild, "parent");
            type = 1;
        }
        pwd = vtoy_json_get_string_ex(node->pstChild, "pwd");

        if (path && pwd)
        {
            pnode = zalloc(sizeof(menu_password));
            if (pnode)
            {
                pnode->type = type;
                strlcpy(pnode->path, path);
                strlcpy(pnode->pwd, pwd);
                
                if (data->list)
                {
                    tail->next = pnode;
                    tail = pnode;
                }
                else
                {
                    data->list = tail = pnode;
                }
            }
        }
    }    

    return 0;
}

static int ventoy_parse_image_list_real(VTOY_JSON *json, int type, void *p)
{
    VTOY_JSON *node = NULL;
    data_image_list *data = (data_image_list *)p;
    path_node *tail = NULL;
    path_node *pnode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        return 0;
    }

    data->type = type;

    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType == JSON_TYPE_STRING)
        {
            pnode = zalloc(sizeof(path_node));
            if (pnode)
            {
                strlcpy(pnode->path, node->unData.pcStrVal);
                if (data->list)
                {
                    tail->next = pnode;
                    tail = pnode;
                }
                else
                {
                    data->list = tail = pnode;
                }
            }
        }
    }

    return 0;
}
static int ventoy_parse_image_blacklist(VTOY_JSON *json, void *p)
{
     return ventoy_parse_image_list_real(json, 1, p);
}
static int ventoy_parse_image_list(VTOY_JSON *json, void *p)
{
    return ventoy_parse_image_list_real(json, 0, p);
}

static int ventoy_parse_auto_memdisk(VTOY_JSON *json, void *p)
{
    VTOY_JSON *node = NULL;
    data_auto_memdisk *data = (data_auto_memdisk *)p;
    path_node *tail = NULL;
    path_node *pnode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        return 0;
    }

    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType == JSON_TYPE_STRING)
        {
            pnode = zalloc(sizeof(path_node));
            if (pnode)
            {
                strlcpy(pnode->path, node->unData.pcStrVal);
                if (data->list)
                {
                    tail->next = pnode;
                    tail = pnode;
                }
                else
                {
                    data->list = tail = pnode;
                }
            }
        }
    }

    return 0;
}
static int ventoy_parse_dud(VTOY_JSON *json, void *p)
{
    int count = 0;
    const char *path = NULL;
    const char *file = NULL;
    data_dud *data = (data_dud *)p;
    dud_node *tail = NULL;
    dud_node *pnode = NULL;
    path_node *pathnode = NULL;
    path_node *pathtail = NULL;
    VTOY_JSON *node = NULL;
    VTOY_JSON *filelist = NULL;
    VTOY_JSON *filenode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        return 0;
    }

    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (node->enDataType != JSON_TYPE_OBJECT)
        {
            continue;
        }

        path = vtoy_json_get_string_ex(node->pstChild, "image");
        if (!path)
        {
            continue;
        }
                
        file = vtoy_json_get_string_ex(node->pstChild, "dud");
        if (file)
        {
            pnode = zalloc(sizeof(dud_node));
            if (pnode)
            {
                strlcpy(pnode->path, path);

                pathnode = zalloc(sizeof(path_node));
                if (pathnode)
                {
                    strlcpy(pathnode->path, file);
                    pnode->list = pathnode;
                }
                else
                {
                    free(pnode);
                }

                if (data->list)
                {
                    tail->next = pnode;
                    tail = pnode;
                }
                else
                {
                    data->list = tail = pnode;
                }
            }

            continue;
        }

        filelist = vtoy_json_find_item(node->pstChild, JSON_TYPE_ARRAY, "dud");
        if (!filelist)
        {
            continue;
        }

        pnode = zalloc(sizeof(dud_node));
        if (!pnode)
        {
            continue;
        }

        strlcpy(pnode->path, path);

        for (filenode = filelist->pstChild; filenode; filenode = filenode->pstNext)
        {
            if (filenode->enDataType != JSON_TYPE_STRING)
            {
                continue;
            }

            pathnode = zalloc(sizeof(path_node));
            if (pathnode)
            {
                strlcpy(pathnode->path, filenode->unData.pcStrVal);
                count++;

                if (pnode->list)
                {
                    pathtail->next = pathnode;
                    pathtail = pathnode;
                }
                else
                {
                    pnode->list = pathtail = pathnode;
                }                
            }
        }

        if (count == 0)
        {
            free(pnode);
        }
        else
        {
            if (data->list)
            {
                tail->next = pnode;
                tail = pnode;
            }
            else
            {
                data->list = tail = pnode;
            }
        }
    }

    return 0;
}



#if 0
#endif


static int ventoy_load_old_json(const char *filename)
{
    int ret = 0;
    int offset = 0;
    int buflen = 0;
    char *buffer = NULL;
    unsigned char *start = NULL;
    VTOY_JSON *json = NULL;
    VTOY_JSON *node = NULL;
    VTOY_JSON *next = NULL;

    ret = ventoy_read_file_to_buf(filename, 4, (void **)&buffer, &buflen);
    if (ret)
    {
        vlog("Failed to read old ventoy.json file.\n");
        return 1;
    }
    buffer[buflen] = 0;

    start = (unsigned char *)buffer;

    if (start[0] == 0xef && start[1] == 0xbb && start[2] == 0xbf)
    {
        offset = 3;
    }
    else if ((start[0] == 0xff && start[1] == 0xfe) || (start[0] == 0xfe && start[1] == 0xff))
    {
        vlog("ventoy.json is in UCS-2 encoding, ignore it.\n");
        free(buffer);
        return 1;
    }

    json = vtoy_json_create();
    if (!json)
    {
        free(buffer);
        return 1;
    }
    
    if (vtoy_json_parse_ex(json, buffer + offset, buflen - offset) == JSON_SUCCESS)
    {
        vlog("parse ventoy.json success\n");

        for (node = json->pstChild; node; node = node->pstNext)
        for (next = node->pstNext; next; next = next->pstNext)
        {
            if (node->pcName && next->pcName && strcmp(node->pcName, next->pcName) == 0)
            {
                vlog("ventoy.json contains duplicate key <%s>.\n", node->pcName);
                g_sysinfo.invalid_config = 1;
                ret = 1;
                goto end;
            }
        }

        for (node = json->pstChild; node; node = node->pstNext)
        {
            ventoy_parse_json(control);
            ventoy_parse_json(theme);
            ventoy_parse_json(menu_alias);
            ventoy_parse_json(menu_tip);
            ventoy_parse_json(menu_class);
            ventoy_parse_json(auto_install);
            ventoy_parse_json(persistence);
            ventoy_parse_json(injection);
            ventoy_parse_json(conf_replace);
            ventoy_parse_json(password);
            ventoy_parse_json(image_list);
            ventoy_parse_json(image_blacklist);
            ventoy_parse_json(auto_memdisk);
            ventoy_parse_json(dud);
        }
    }
    else
    {
        vlog("ventoy.json has syntax error.\n");    
        g_sysinfo.syntax_error = 1;
        ret = 1;
    }

end:
    vtoy_json_destroy(json);

    free(buffer);
    return ret;
}


int ventoy_http_start(const char *ip, const char *port)
{
    int i = 0;
    int ret = 0;
    char addr[128];
    char filename[128];
    char backupname[128];
    struct mg_callbacks callbacks;
    const char *options[] = 
    {
	    "listening_ports",    "24681",
        "document_root",      "www",
        "index_files",        "index.html",
        "num_threads",        "16",
        "error_log_file",     LOG_FILE,
	    "request_timeout_ms", "10000",
	     NULL
    };

    for (i = 0; i <= bios_max; i++)
    {
        ventoy_data_default_control(g_data_control + i);
        ventoy_data_default_theme(g_data_theme + i);
        ventoy_data_default_menu_alias(g_data_menu_alias + i);
        ventoy_data_default_menu_class(g_data_menu_class + i);
        ventoy_data_default_menu_tip(g_data_menu_tip + i);        
        ventoy_data_default_auto_install(g_data_auto_install + i);
        ventoy_data_default_persistence(g_data_persistence + i);
        ventoy_data_default_injection(g_data_injection + i);
        ventoy_data_default_conf_replace(g_data_conf_replace + i);
        ventoy_data_default_password(g_data_password + i);
        ventoy_data_default_image_list(g_data_image_list + i);
        ventoy_data_default_auto_memdisk(g_data_auto_memdisk + i);
        ventoy_data_default_dud(g_data_dud + i);
    }

    ventoy_get_json_path(filename, backupname);
    if (ventoy_is_file_exist("%s", filename))
    {
        ventoy_copy_file(filename, backupname);
        ret = ventoy_load_old_json(filename);
        if (ret == 0)
        {
            ventoy_data_real_save_all(0);
        }
    }

    /* option */
    scnprintf(addr, sizeof(addr), "%s:%s", ip, port);
    options[1] = addr;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.begin_request = ventoy_request_handler;
#ifndef VENTOY_SIM
    callbacks.open_file = ventoy_web_openfile;
#endif
    g_ventoy_http_ctx = mg_start(&callbacks, NULL, options);

    ventoy_start_writeback_thread(ventoy_http_writeback);

    return g_ventoy_http_ctx ? 0 : 1;
}

int ventoy_http_stop(void)
{
    if (g_ventoy_http_ctx)
    {
        mg_stop(g_ventoy_http_ctx);        
    }

    ventoy_stop_writeback_thread();
    return 0;
}

int ventoy_http_init(void)
{
    int i = 0;
    
#ifdef VENTOY_SIM
    char *Buffer = NULL;
    int BufLen = 0;

    ventoy_read_file_to_buf("www/menulist", 4, (void **)&Buffer, &BufLen);
    if (Buffer)
    {
        for (i = 0; i < BufLen / 5; i++)
        {
            memcpy(g_ventoy_menu_lang[i], Buffer + i * 5, 5);
            g_ventoy_menu_lang[i][5] = 0;
        }
        free(Buffer);
    }
#else
    ventoy_file *file;
    
    file = ventoy_tar_find_file("www/menulist");
    if (file)
    {
        for (i = 0; i < file->size / 5; i++)
        {
            memcpy(g_ventoy_menu_lang[i], (char *)(file->addr) + i * 5, 5);
            g_ventoy_menu_lang[i][5] = 0;
        }
    }
#endif

    if (!g_pub_json_buffer)
    {
        g_pub_json_buffer = malloc(JSON_BUF_MAX * 2);
        g_pub_save_buffer = g_pub_json_buffer + JSON_BUF_MAX;
    }   


    pthread_mutex_init(&g_api_mutex, NULL);
    return 0;
}

void ventoy_http_exit(void)
{
    check_free(g_pub_json_buffer);
    g_pub_json_buffer = NULL;
    g_pub_save_buffer = NULL;
    
    pthread_mutex_destroy(&g_api_mutex);
}


