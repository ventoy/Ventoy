/******************************************************************************
 * ventoy_plugin.c 
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
#include <grub/crypto.h>
#include <grub/time.h>
#include <grub/font.h>
#include <grub/video.h>
#include <grub/ventoy.h>
#include "ventoy_def.h"

GRUB_MOD_LICENSE ("GPLv3+");

char g_arch_mode_suffix[64];
static char g_iso_disk_name[128];
static vtoy_password g_boot_pwd;
static vtoy_password g_file_type_pwd[img_type_max];
static install_template *g_install_template_head = NULL;
static dud *g_dud_head = NULL;
static menu_password *g_pwd_head = NULL;
static persistence_config *g_persistence_head = NULL;
static menu_tip *g_menu_tip_head = NULL;
static menu_alias *g_menu_alias_head = NULL;
static menu_class *g_menu_class_head = NULL;
static custom_boot *g_custom_boot_head = NULL;
static injection_config *g_injection_head = NULL;
static auto_memdisk *g_auto_memdisk_head = NULL;
static image_list *g_image_list_head = NULL;
static conf_replace *g_conf_replace_head = NULL;
static VTOY_JSON *g_menu_lang_json = NULL;

static int g_theme_id = 0;
static int g_theme_res_fit = 0;
static int g_theme_num = 0;
static theme_list *g_theme_head = NULL;
static int g_theme_random = vtoy_theme_random_boot_second;
static char g_theme_single_file[256];
static char g_cur_menu_language[32] = {0};
static char g_push_menu_language[32] = {0};

static int ventoy_plugin_is_parent(const char *pat, int patlen, const char *isopath)
{
    if (patlen > 1)
    {
        if (isopath[patlen] == '/' && ventoy_strncmp(pat, isopath, patlen) == 0 &&
            grub_strchr(isopath + patlen + 1, '/') == NULL)
        {
            return 1;
        }
    }
    else
    {
        if (pat[0] == '/' && grub_strchr(isopath + 1, '/') == NULL)
        {
            return 1;
        }
    }

    return 0;
}

static int ventoy_plugin_control_check(VTOY_JSON *json, const char *isodisk)
{
    int rc = 0;
    VTOY_JSON *pNode = NULL;
    VTOY_JSON *pChild = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array type %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType == JSON_TYPE_OBJECT)
        {
            pChild = pNode->pstChild;
            if (pChild->enDataType == JSON_TYPE_STRING)
            {
                if (grub_strcmp(pChild->pcName, "VTOY_DEFAULT_IMAGE") == 0)
                {                    
                    grub_printf("%s: %s [%s]\n", pChild->pcName, pChild->unData.pcStrVal,
                        ventoy_check_file_exist("%s%s", isodisk, pChild->unData.pcStrVal) ? "OK" : "NOT EXIST");
                }
                else
                {
                    grub_printf("%s: %s\n", pChild->pcName, pChild->unData.pcStrVal);                    
                }
            }
            else
            {
                grub_printf("%s is NOT string type\n", pChild->pcName);
                rc = 1;
            }
        }
        else
        {
            grub_printf("%s is not an object\n", pNode->pcName);
            rc = 1;
        }
    }

    return rc;
}

static int ventoy_plugin_control_entry(VTOY_JSON *json, const char *isodisk)
{
    VTOY_JSON *pNode = NULL;
    VTOY_JSON *pChild = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType == JSON_TYPE_OBJECT)
        {
            pChild = pNode->pstChild;
            if (pChild->enDataType == JSON_TYPE_STRING && pChild->pcName && pChild->unData.pcStrVal)
            {
                ventoy_set_env(pChild->pcName, pChild->unData.pcStrVal);
            }
        }
    }

    return 0;
}

static int ventoy_plugin_theme_check(VTOY_JSON *json, const char *isodisk)
{
    int exist = 0;
    const char *value;
    VTOY_JSON *node;
    
    value = vtoy_json_get_string_ex(json->pstChild, "file");
    if (value)
    {
        grub_printf("file: %s\n", value);
        if (value[0] == '/')
        {
            exist = ventoy_check_file_exist("%s%s", isodisk, value);
        }
        else
        {
            exist = ventoy_check_file_exist("%s/ventoy/%s", isodisk, value);
        }
        
        if (exist == 0)
        {
            grub_printf("Theme file %s does NOT exist\n", value);
            return 1;
        }
    }
    else
    {
        node = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "file");
        if (node)
        {
            for (node = node->pstChild; node; node = node->pstNext)
            {
                value = node->unData.pcStrVal;
                grub_printf("file: %s\n", value);
                if (value[0] == '/')
                {
                    exist = ventoy_check_file_exist("%s%s", isodisk, value);
                }
                else
                {
                    exist = ventoy_check_file_exist("%s/ventoy/%s", isodisk, value);
                }

                if (exist == 0)
                {
                    grub_printf("Theme file %s does NOT exist\n", value);
                    return 1;
                }
            }

            value = vtoy_json_get_string_ex(json->pstChild, "random");
            if (value)
            {
                grub_printf("random: %s\n", value);
            }
        }
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "gfxmode");
    if (value)
    {
        grub_printf("gfxmode: %s\n", value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "display_mode");
    if (value)
    {
        grub_printf("display_mode: %s\n", value);
    }

    value = vtoy_json_get_string_ex(json->pstChild, "serial_param");
    if (value)
    {
        grub_printf("serial_param %s\n", value);
    }

    value = vtoy_json_get_string_ex(json->pstChild, "ventoy_left");
    if (value)
    {
        grub_printf("ventoy_left: %s\n", value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "ventoy_top");
    if (value)
    {
        grub_printf("ventoy_top: %s\n", value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "ventoy_color");
    if (value)
    {
        grub_printf("ventoy_color: %s\n", value);
    }

    node = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "fonts");
    if (node)
    {
        for (node = node->pstChild; node; node = node->pstNext)
        {
            if (node->enDataType == JSON_TYPE_STRING)
            {
                if (ventoy_check_file_exist("%s%s", isodisk, node->unData.pcStrVal))
                {
                    grub_printf("%s [OK]\n", node->unData.pcStrVal);
                }
                else
                {
                    grub_printf("%s [NOT EXIST]\n", node->unData.pcStrVal);
                }
            }
        }
    }
    else
    {
        grub_printf("fonts NOT found\n");
    }

    return 0;
}

static int ventoy_plugin_theme_entry(VTOY_JSON *json, const char *isodisk)
{
    const char *value;
    char val[64];
    char filepath[256];
    VTOY_JSON *node = NULL;
    theme_list *tail = NULL;
    theme_list *themenode = NULL;

    value = vtoy_json_get_string_ex(json->pstChild, "file");
    if (value)
    {
        if (value[0] == '/')
        {
            grub_snprintf(filepath, sizeof(filepath), "%s%s", isodisk, value);
        }
        else
        {
            grub_snprintf(filepath, sizeof(filepath), "%s/ventoy/%s", isodisk, value);
        }
        
        if (ventoy_check_file_exist(filepath) == 0)
        {
            debug("Theme file %s does not exist\n", filepath);
            return 0;
        }

        debug("vtoy_theme %s\n", filepath);
        ventoy_env_export("vtoy_theme", filepath);
        grub_snprintf(g_theme_single_file, sizeof(g_theme_single_file), "%s", filepath);
    }
    else
    {
        node = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "file");
        if (node)
        {
            for (node = node->pstChild; node; node = node->pstNext)
            {
                value = node->unData.pcStrVal;
                if (value[0] == '/')
                {
                    grub_snprintf(filepath, sizeof(filepath), "%s%s", isodisk, value);
                }
                else
                {
                    grub_snprintf(filepath, sizeof(filepath), "%s/ventoy/%s", isodisk, value);
                }

                if (ventoy_check_file_exist(filepath) == 0)
                {
                    continue;
                }

                themenode = grub_zalloc(sizeof(theme_list));
                if (themenode)
                {
                    grub_snprintf(themenode->theme.path, sizeof(themenode->theme.path), "%s", filepath);
                    if (g_theme_head)
                    {
                        tail->next = themenode;
                    }
                    else
                    {
                        g_theme_head = themenode;
                    }
                    tail = themenode;
                    g_theme_num++;
                }
            }

            ventoy_env_export("vtoy_theme", "random");
            value = vtoy_json_get_string_ex(json->pstChild, "random");
            if (value)
            {
                if (grub_strcmp(value, "boot_second") == 0)
                {
                    g_theme_random = vtoy_theme_random_boot_second;
                }
                else if (grub_strcmp(value, "boot_day") == 0)
                {
                    g_theme_random = vtoy_theme_random_boot_day;
                }
                else if (grub_strcmp(value, "boot_month") == 0)
                {
                    g_theme_random = vtoy_theme_random_boot_month;
                }
            }
        }
    }

    grub_snprintf(val, sizeof(val), "%d", g_theme_num);
    grub_env_set("VTOY_THEME_COUNT", val);
    grub_env_export("VTOY_THEME_COUNT");
    if (g_theme_num > 0)
    {
        vtoy_json_get_int(json->pstChild, "default_file", &g_theme_id);
        if (g_theme_id == 0)
        {
            vtoy_json_get_int(json->pstChild, "resolution_fit", &g_theme_res_fit);
            if (g_theme_res_fit != 1)
            {
                g_theme_res_fit = 0;
            }

            grub_snprintf(val, sizeof(val), "%d", g_theme_res_fit);
            ventoy_env_export("vtoy_res_fit", val);
        }
        
        if (g_theme_id > g_theme_num || g_theme_id < 0)
        {
            g_theme_id = 0;
        }
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "gfxmode");
    if (value)
    {
        debug("vtoy_gfxmode %s\n", value);
        ventoy_env_export("vtoy_gfxmode", value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "display_mode");
    if (value)
    {
        debug("display_mode %s\n", value);
        ventoy_env_export("vtoy_display_mode", value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "serial_param");
    if (value)
    {
        debug("serial_param %s\n", value);
        ventoy_env_export("vtoy_serial_param", value);
    }

    value = vtoy_json_get_string_ex(json->pstChild, "ventoy_left");
    if (value)
    {
        ventoy_env_export(ventoy_left_key, value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "ventoy_top");
    if (value)
    {
        ventoy_env_export(ventoy_top_key, value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "ventoy_color");
    if (value)
    {
        ventoy_env_export(ventoy_color_key, value);
    }

    node = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "fonts");
    if (node)
    {
        for (node = node->pstChild; node; node = node->pstNext)
        {
            if (node->enDataType == JSON_TYPE_STRING && 
                ventoy_check_file_exist("%s%s", isodisk, node->unData.pcStrVal))
            {
                grub_snprintf(filepath, sizeof(filepath), "%s%s", isodisk, node->unData.pcStrVal);
                grub_font_load(filepath);
            }
        }
    }

    return 0;
}

static int ventoy_plugin_check_path(const char *path, const char *file)
{
    if (file[0] != '/')
    {
        grub_printf("%s is NOT begin with '/' \n", file);
        return 1;
    }

    if (grub_strchr(file, '\\'))
    {
        grub_printf("%s contains invalid '\\' \n", file);
        return 1;
    }
    
    if (grub_strstr(file, "//"))
    {
        grub_printf("%s contains invalid double slash\n", file);
        return 1;
    }

    if (grub_strstr(file, "../"))
    {
        grub_printf("%s contains invalid '../' \n", file);
        return 1;
    }

    if (!ventoy_check_file_exist("%s%s", path, file))
    {
        grub_printf("%s%s does NOT exist\n", path, file);
        return 1;
    }

    return 0;
}

static int ventoy_plugin_check_fullpath
(
    VTOY_JSON *json, 
    const char *isodisk, 
    const char *key,
    int *pathnum
)
{
    int rc = 0;
    int ret = 0;
    int cnt = 0;
    VTOY_JSON *node = json;
    VTOY_JSON *child = NULL;
    
    while (node)
    {
        if (0 == grub_strcmp(key, node->pcName))
        {
            break;
        }
        node = node->pstNext;
    }

    if (!node)
    {
        return 1;
    }

    if (JSON_TYPE_STRING == node->enDataType)
    {
        cnt = 1;
        ret = ventoy_plugin_check_path(isodisk, node->unData.pcStrVal);
        grub_printf("%s: %s [%s]\n", key, node->unData.pcStrVal, ret ? "FAIL" : "OK");
    }
    else if (JSON_TYPE_ARRAY == node->enDataType)
    {
        for (child = node->pstChild; child; child = child->pstNext)
        {
            if (JSON_TYPE_STRING != child->enDataType)
            {
                grub_printf("Non string json type\n");
            }
            else
            {
                rc = ventoy_plugin_check_path(isodisk, child->unData.pcStrVal);
                grub_printf("%s: %s [%s]\n", key, child->unData.pcStrVal, rc ? "FAIL" : "OK");
                ret += rc;
                cnt++;
            }
        }
    }

    *pathnum = cnt;
    return ret;
}

static int ventoy_plugin_parse_fullpath
(
    VTOY_JSON *json, 
    const char *isodisk, 
    const char *key, 
    file_fullpath **fullpath,
    int *pathnum
)
{
    int rc = 1;
    int count = 0;
    VTOY_JSON *node = json;
    VTOY_JSON *child = NULL;
    file_fullpath *path = NULL;
    
    while (node)
    {
        if (0 == grub_strcmp(key, node->pcName))
        {
            break;
        }
        node = node->pstNext;
    }

    if (!node)
    {
        return 1;
    }

    if (JSON_TYPE_STRING == node->enDataType)
    {
        debug("%s is string type data\n", node->pcName);

        if ((node->unData.pcStrVal[0] != '/') || (!ventoy_check_file_exist("%s%s", isodisk, node->unData.pcStrVal)))
        {
            debug("%s%s file not found\n", isodisk, node->unData.pcStrVal);
            return 1;
        }
        
        path = (file_fullpath *)grub_zalloc(sizeof(file_fullpath));
        if (path)
        {
            grub_snprintf(path->path, sizeof(path->path), "%s", node->unData.pcStrVal);
            *fullpath = path;
            *pathnum = 1;
            rc = 0;
        }
    }
    else if (JSON_TYPE_ARRAY == node->enDataType)
    {
        for (child = node->pstChild; child; child = child->pstNext)
        {
            if ((JSON_TYPE_STRING != child->enDataType) || (child->unData.pcStrVal[0] != '/'))
            {
                debug("Invalid data type:%d\n", child->enDataType);
                return 1;
            }
            count++;
        }
        debug("%s is array type data, count=%d\n", node->pcName, count);
        
        path = (file_fullpath *)grub_zalloc(sizeof(file_fullpath) * count);
        if (path)
        {
            *fullpath = path;
            
            for (count = 0, child = node->pstChild; child; child = child->pstNext)
            {
                if (ventoy_check_file_exist("%s%s", isodisk, child->unData.pcStrVal))
                {
                    grub_snprintf(path->path, sizeof(path->path), "%s", child->unData.pcStrVal);
                    path++;
                    count++;
                }
            }

            *pathnum = count;
            rc = 0;
        }
    }

    return rc;
}

static int ventoy_plugin_auto_install_check(VTOY_JSON *json, const char *isodisk)
{
    int pathnum = 0;
    int autosel = 0;
    int timeout = 0;
    char *pos = NULL;
    const char *iso = NULL;
    VTOY_JSON *pNode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array type %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType != JSON_TYPE_OBJECT)
        {
            grub_printf("NOT object type\n");
        }
    
        if ((iso = vtoy_json_get_string_ex(pNode->pstChild, "image")) != NULL)
        {
            pos = grub_strchr(iso, '*');
            if (pos || 0 == ventoy_plugin_check_path(isodisk, iso))
            {
                grub_printf("image: %s [%s]\n", iso, (pos ? "*" : "OK"));                
                ventoy_plugin_check_fullpath(pNode->pstChild, isodisk, "template", &pathnum);
                
                if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "autosel", &autosel))
                {
                    if (autosel >= 0 && autosel <= pathnum)
                    {
                        grub_printf("autosel: %d [OK]\n", autosel);
                    }
                    else
                    {
                        grub_printf("autosel: %d [FAIL]\n", autosel);
                    }
                }
                
                if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "timeout", &timeout))
                {
                    if (timeout >= 0)
                    {
                        grub_printf("timeout: %d [OK]\n", timeout);
                    }
                    else
                    {
                        grub_printf("timeout: %d [FAIL]\n", timeout);
                    }
                }
            }
            else
            {
                grub_printf("image: %s [FAIL]\n", iso);
            }
        }
        else if ((iso = vtoy_json_get_string_ex(pNode->pstChild, "parent")) != NULL)
        {
            if (ventoy_is_dir_exist("%s%s", isodisk, iso))
            {
                grub_printf("parent: %s [OK]\n", iso);
                ventoy_plugin_check_fullpath(pNode->pstChild, isodisk, "template", &pathnum);
                
                if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "autosel", &autosel))
                {
                    if (autosel >= 0 && autosel <= pathnum)
                    {
                        grub_printf("autosel: %d [OK]\n", autosel);
                    }
                    else
                    {
                        grub_printf("autosel: %d [FAIL]\n", autosel);
                    }
                }
                
                if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "timeout", &timeout))
                {
                    if (timeout >= 0)
                    {
                        grub_printf("timeout: %d [OK]\n", timeout);
                    }
                    else
                    {
                        grub_printf("timeout: %d [FAIL]\n", timeout);
                    }
                }
            }
            else
            {
                grub_printf("parent: %s [FAIL]\n", iso);
            }
        }
        else
        {
            grub_printf("image not found\n");
        }
    }

    return 0;
}

static int ventoy_plugin_auto_install_entry(VTOY_JSON *json, const char *isodisk)
{
    int type = 0;
    int pathnum = 0;
    int autosel = 0;
    int timeout = 0;
    const char *iso = NULL;
    VTOY_JSON *pNode = NULL;
    install_template *node = NULL;
    install_template *next = NULL;
    file_fullpath *templatepath = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_install_template_head)
    {
        for (node = g_install_template_head; node; node = next)
        {
            next = node->next;
            grub_check_free(node->templatepath);
            grub_free(node);
        }

        g_install_template_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = auto_install_type_file;
        iso = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (!iso)
        {
            type = auto_install_type_parent;
            iso = vtoy_json_get_string_ex(pNode->pstChild, "parent");
        }
        
        if (iso && iso[0] == '/')
        {
            if (0 == ventoy_plugin_parse_fullpath(pNode->pstChild, isodisk, "template", &templatepath, &pathnum))
            {
                node = grub_zalloc(sizeof(install_template));
                if (node)
                {
                    node->type = type;
                    node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", iso);
                    node->templatepath = templatepath;
                    node->templatenum = pathnum;

                    node->autosel = -1;
                    node->timeout = -1;
                    if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "autosel", &autosel))
                    {
                        if (autosel >= 0 && autosel <= pathnum)
                        {
                            node->autosel = autosel;
                        }
                    }
                    
                    if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "timeout", &timeout))
                    {
                        if (timeout >= 0)
                        {
                            node->timeout = timeout;
                        }
                    }

                    if (g_install_template_head)
                    {
                        node->next = g_install_template_head;
                    }
                    
                    g_install_template_head = node;
                }
            }
        }
    }

    return 0;
}

static int ventoy_plugin_dud_check(VTOY_JSON *json, const char *isodisk)
{
    int pathnum = 0;
    char *pos = NULL;
    const char *iso = NULL;
    VTOY_JSON *pNode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array type %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType != JSON_TYPE_OBJECT)
        {
            grub_printf("NOT object type\n");
        }
    
        iso = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (iso)
        {
            pos = grub_strchr(iso, '*');
            if (pos || 0 == ventoy_plugin_check_path(isodisk, iso))
            {
                grub_printf("image: %s [%s]\n", iso, (pos ? "*" : "OK"));
                ventoy_plugin_check_fullpath(pNode->pstChild, isodisk, "dud", &pathnum);
            }
            else
            {
                grub_printf("image: %s [FAIL]\n", iso);
            }
        }
        else
        {
            grub_printf("image not found\n");
        }
    }

    return 0;
}

static int ventoy_plugin_dud_entry(VTOY_JSON *json, const char *isodisk)
{
    int pathnum = 0;
    const char *iso = NULL;
    VTOY_JSON *pNode = NULL;
    dud *node = NULL;
    dud *next = NULL;
    file_fullpath *dudpath = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_dud_head)
    {
        for (node = g_dud_head; node; node = next)
        {
            next = node->next;
            grub_check_free(node->dudpath);
            grub_free(node);
        }

        g_dud_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        iso = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (iso && iso[0] == '/')
        {
            if (0 == ventoy_plugin_parse_fullpath(pNode->pstChild, isodisk, "dud", &dudpath, &pathnum))
            {
                node = grub_zalloc(sizeof(dud));
                if (node)
                {
                    node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", iso);
                    node->dudpath = dudpath;
                    node->dudnum = pathnum;
                    node->files = grub_zalloc(sizeof(dudfile) * pathnum);

                    if (node->files)
                    {
                        if (g_dud_head)
                        {
                            node->next = g_dud_head;
                        }
                        
                        g_dud_head = node;
                    }
                    else
                    {
                        grub_free(node);
                    }
                }
            }
        }
    }

    return 0;
}

static int ventoy_plugin_parse_pwdstr(char *pwdstr, vtoy_password *pwd)
{
    int i;
    int len;
    char ch;
    char *pos;
    char bytes[3];
    vtoy_password tmpPwd;
    
    len = (int)grub_strlen(pwdstr);
    if (len > 64)
    {
        if (NULL == pwd) grub_printf("Password too long %d\n", len);
        return 1;
    }

    grub_memset(&tmpPwd, 0, sizeof(tmpPwd));

    if (grub_strncmp(pwdstr, "txt#", 4) == 0)
    {
        tmpPwd.type = VTOY_PASSWORD_TXT;
        grub_snprintf(tmpPwd.text, sizeof(tmpPwd.text), "%s", pwdstr + 4);
    }
    else if (grub_strncmp(pwdstr, "md5#", 4) == 0)
    {
        if ((len - 4) == 32)
        {
            for (i = 0; i < 16; i++)
            {
                bytes[0] = pwdstr[4 + i * 2];
                bytes[1] = pwdstr[4 + i * 2 + 1];
                bytes[2] = 0;
                
                if (grub_isxdigit(bytes[0]) && grub_isxdigit(bytes[1]))
                {
                    tmpPwd.md5[i] = (grub_uint8_t)grub_strtoul(bytes, NULL, 16);
                }
                else
                {
                    if (NULL == pwd) grub_printf("Invalid md5 hex format %s %d\n", pwdstr, i);
                    return 1;
                }
            }
            tmpPwd.type = VTOY_PASSWORD_MD5;
        }
        else if ((len - 4) > 32)
        {
            pos = grub_strchr(pwdstr + 4, '#');
            if (!pos)
            {
                if (NULL == pwd) grub_printf("Invalid md5 password format %s\n", pwdstr);
                return 1;
            }

            if (len - 1 - ((long)pos - (long)pwdstr) != 32)
            {
                if (NULL == pwd) grub_printf("Invalid md5 salt password format %s\n", pwdstr);
                return 1;
            }
        
            ch = *pos;
            *pos = 0;
            grub_snprintf(tmpPwd.salt, sizeof(tmpPwd.salt), "%s", pwdstr + 4);
            *pos = ch;

            pos++;
            for (i = 0; i < 16; i++)
            {
                bytes[0] = pos[i * 2];
                bytes[1] = pos[i * 2 + 1];
                bytes[2] = 0;
                
                if (grub_isxdigit(bytes[0]) && grub_isxdigit(bytes[1]))
                {
                    tmpPwd.md5[i] = (grub_uint8_t)grub_strtoul(bytes, NULL, 16);
                }
                else
                {
                    if (NULL == pwd) grub_printf("Invalid md5 hex format %s %d\n", pwdstr, i);
                    return 1;
                }
            }

            tmpPwd.type = VTOY_PASSWORD_SALT_MD5;
        }
        else
        {
            if (NULL == pwd) grub_printf("Invalid md5 password format %s\n", pwdstr);
            return 1;
        }
    }
    else
    {
        if (NULL == pwd) grub_printf("Invalid password format %s\n", pwdstr);
        return 1;
    }

    if (pwd)
    {
        grub_memcpy(pwd, &tmpPwd, sizeof(tmpPwd));
    }

    return 0;
}

static int ventoy_plugin_get_pwd_type(const char *pwd)
{
    int i;
    char pwdtype[64];

    for (i = 0; pwd && i < (int)ARRAY_SIZE(g_menu_prefix); i++)
    {
        grub_snprintf(pwdtype, sizeof(pwdtype), "%spwd", g_menu_prefix[i]);
        if (grub_strcmp(pwdtype, pwd) == 0)
        {
            return img_type_start + i; 
        }
    }
    
    return -1;
}

static int ventoy_plugin_pwd_entry(VTOY_JSON *json, const char *isodisk)
{
    int type = -1;
    const char *iso = NULL;
    const char *pwd = NULL;
    VTOY_JSON *pNode = NULL;
    VTOY_JSON *pCNode = NULL;
    menu_password *node = NULL;
    menu_password *tail = NULL;
    menu_password *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_OBJECT)
    {
        debug("Not object %d\n", json->enDataType);
        return 0;
    }

    if (g_pwd_head)
    {
        for (node = g_pwd_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_pwd_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->pcName && grub_strcmp("bootpwd", pNode->pcName) == 0)
        {
            ventoy_plugin_parse_pwdstr(pNode->unData.pcStrVal, &g_boot_pwd);
        }
        else if ((type = ventoy_plugin_get_pwd_type(pNode->pcName)) >= 0)
        {
            ventoy_plugin_parse_pwdstr(pNode->unData.pcStrVal, g_file_type_pwd + type);
        }
        else if (pNode->pcName && grub_strcmp("menupwd", pNode->pcName) == 0)
        {
            for (pCNode = pNode->pstChild; pCNode; pCNode = pCNode->pstNext)
            {
                if (pCNode->enDataType != JSON_TYPE_OBJECT)
                {
                    continue;
                }

                type = vtoy_menu_pwd_file;
                iso = vtoy_json_get_string_ex(pCNode->pstChild, "file");
                if (!iso)
                {
                    type = vtoy_menu_pwd_parent;
                    iso = vtoy_json_get_string_ex(pCNode->pstChild, "parent");                    
                }                
                
                pwd = vtoy_json_get_string_ex(pCNode->pstChild, "pwd");
                if (iso && pwd && iso[0] == '/')
                {
                    node = grub_zalloc(sizeof(menu_password));
                    if (node)
                    {
                        node->type = type;
                        node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", iso);

                        if (ventoy_plugin_parse_pwdstr((char *)pwd, &(node->password)))
                        {
                            grub_free(node);
                            continue;
                        }

                        if (g_pwd_head)
                        {
                            tail->next = node;
                        }
                        else
                        {
                            g_pwd_head = node;
                        }
                        tail = node;
                    }
                }
            }
        }
    }

    return 0;
}

static int ventoy_plugin_pwd_check(VTOY_JSON *json, const char *isodisk)
{
    int type = -1;
    char *pos = NULL;
    const char *iso = NULL;
    const char *pwd = NULL;
    VTOY_JSON *pNode = NULL;
    VTOY_JSON *pCNode = NULL;

    if (json->enDataType != JSON_TYPE_OBJECT)
    {
        grub_printf("Not object %d\n", json->enDataType);
        return 0;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->pcName && grub_strcmp("bootpwd", pNode->pcName) == 0)
        {
            if (0 == ventoy_plugin_parse_pwdstr(pNode->unData.pcStrVal, NULL))
            {
                grub_printf("bootpwd:<%s>\n", pNode->unData.pcStrVal);
            }
            else
            {
                grub_printf("Invalid bootpwd.\n");
            }
        }
        else if ((type = ventoy_plugin_get_pwd_type(pNode->pcName)) >= 0)
        {
            if (0 == ventoy_plugin_parse_pwdstr(pNode->unData.pcStrVal, NULL))
            {
                grub_printf("%s:<%s>\n", pNode->pcName, pNode->unData.pcStrVal);
            }
            else
            {
                grub_printf("Invalid pwd <%s>\n", pNode->unData.pcStrVal);
            }
        }
        else if (pNode->pcName && grub_strcmp("menupwd", pNode->pcName) == 0)
        {
            grub_printf("\n");
            for (pCNode = pNode->pstChild; pCNode; pCNode = pCNode->pstNext)
            {
                if (pCNode->enDataType != JSON_TYPE_OBJECT)
                {
                    grub_printf("Not object %d\n", pCNode->enDataType);
                    continue;
                }

                if ((iso = vtoy_json_get_string_ex(pCNode->pstChild, "file")) != NULL)
                {
                    pos = grub_strchr(iso, '*');
                    if (pos || 0 == ventoy_plugin_check_path(isodisk, iso))
                    {
                        pwd = vtoy_json_get_string_ex(pCNode->pstChild, "pwd");

                        if (0 == ventoy_plugin_parse_pwdstr((char *)pwd, NULL))
                        {
                            grub_printf("file:<%s> [%s]\n", iso, (pos ? "*" : "OK"));
                            grub_printf("pwd:<%s>\n\n", pwd);
                        }
                        else
                        {
                            grub_printf("Invalid password for <%s>\n", iso);
                        }
                    }
                    else
                    {
                        grub_printf("<%s%s> not found\n", isodisk, iso);
                    }
                }
                else if ((iso = vtoy_json_get_string_ex(pCNode->pstChild, "parent")) != NULL)
                {
                    if (ventoy_is_dir_exist("%s%s", isodisk, iso))
                    {
                        pwd = vtoy_json_get_string_ex(pCNode->pstChild, "pwd");
                        if (0 == ventoy_plugin_parse_pwdstr((char *)pwd, NULL))
                        {
                            grub_printf("dir:<%s> [%s]\n", iso, (pos ? "*" : "OK"));
                            grub_printf("pwd:<%s>\n\n", pwd);
                        }
                        else
                        {
                            grub_printf("Invalid password for <%s>\n", iso);
                        }
                    }
                    else
                    {
                        grub_printf("<%s%s> not found\n", isodisk, iso);
                    }
                }
                else
                {
                    grub_printf("No file item found in json.\n");
                }
            }
        }
    }

    return 0;
}

static int ventoy_plugin_persistence_check(VTOY_JSON *json, const char *isodisk)
{
    int autosel = 0;
    int timeout = 0;
    int pathnum = 0;
    char *pos = NULL;
    const char *iso = NULL;
    VTOY_JSON *pNode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array type %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType != JSON_TYPE_OBJECT)
        {
            grub_printf("NOT object type\n");
        }
    
        iso = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (iso)
        {
            pos = grub_strchr(iso, '*');
            if (pos || 0 == ventoy_plugin_check_path(isodisk, iso))
            {
                grub_printf("image: %s [%s]\n", iso, (pos ? "*" : "OK"));
                ventoy_plugin_check_fullpath(pNode->pstChild, isodisk, "backend", &pathnum);

                if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "autosel", &autosel))
                {
                    if (autosel >= 0 && autosel <= pathnum)
                    {
                        grub_printf("autosel: %d [OK]\n", autosel);
                    }
                    else
                    {
                        grub_printf("autosel: %d [FAIL]\n", autosel);
                    }
                }
                
                if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "timeout", &timeout))
                {
                    if (timeout >= 0)
                    {
                        grub_printf("timeout: %d [OK]\n", timeout);
                    }
                    else
                    {
                        grub_printf("timeout: %d [FAIL]\n", timeout);
                    }
                }
            } 
            else
            {
                grub_printf("image: %s [FAIL]\n", iso);
            }
        }
        else
        {
            grub_printf("image not found\n");
        }
    }

    return 0;
}

static int ventoy_plugin_persistence_entry(VTOY_JSON *json, const char *isodisk)
{
    int autosel = 0;
    int timeout = 0;
    int pathnum = 0;
    const char *iso = NULL;
    VTOY_JSON *pNode = NULL;
    persistence_config *node = NULL;
    persistence_config *next = NULL;
    file_fullpath *backendpath = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_persistence_head)
    {
        for (node = g_persistence_head; node; node = next)
        {
            next = node->next;
            grub_check_free(node->backendpath);
            grub_free(node);
        }

        g_persistence_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        iso = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (iso && iso[0] == '/')
        {
            if (0 == ventoy_plugin_parse_fullpath(pNode->pstChild, isodisk, "backend", &backendpath, &pathnum))
            {
                node = grub_zalloc(sizeof(persistence_config));
                if (node)
                {
                    node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", iso);
                    node->backendpath = backendpath;
                    node->backendnum = pathnum;

                    node->autosel = -1;
                    node->timeout = -1;
                    if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "autosel", &autosel))
                    {
                        if (autosel >= 0 && autosel <= pathnum)
                        {
                            node->autosel = autosel;
                        }
                    }
                    
                    if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "timeout", &timeout))
                    {
                        if (timeout >= 0)
                        {
                            node->timeout = timeout;
                        }
                    }

                    if (g_persistence_head)
                    {
                        node->next = g_persistence_head;
                    }
                    
                    g_persistence_head = node;
                }
            }
        }
    }

    return 0;
}

static int ventoy_plugin_menualias_check(VTOY_JSON *json, const char *isodisk)
{
    int type;
    const char *path = NULL;
    const char *alias = NULL;
    VTOY_JSON *pNode = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = vtoy_alias_image_file;
        path = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(pNode->pstChild, "dir");
            type = vtoy_alias_directory;
        }
        
        alias = vtoy_json_get_string_ex(pNode->pstChild, "alias");
        if (path && path[0] == '/' && alias)
        {
            if (vtoy_alias_image_file == type)
            {
                if (grub_strchr(path, '*'))
                {
                    grub_printf("image: <%s> [ * ]\n", path);
                }
                else if (ventoy_check_file_exist("%s%s", isodisk, path))
                {
                    grub_printf("image: <%s> [ OK ]\n", path);
                }
                else
                {
                    grub_printf("image: <%s> [ NOT EXIST ]\n", path);
                }
            }
            else
            {
                if (ventoy_is_dir_exist("%s%s", isodisk, path))
                {
                    grub_printf("dir: <%s> [ OK ]\n", path);
                }
                else
                {
                    grub_printf("dir: <%s> [ NOT EXIST ]\n", path);
                }
            }

            grub_printf("alias: <%s>\n\n", alias);
        }
    }

    return 0;
}

static int ventoy_plugin_menualias_entry(VTOY_JSON *json, const char *isodisk)
{
    int type;
    const char *path = NULL;
    const char *alias = NULL;
    VTOY_JSON *pNode = NULL;
    menu_alias *node = NULL;
    menu_alias *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_menu_alias_head)
    {
        for (node = g_menu_alias_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_menu_alias_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = vtoy_alias_image_file;
        path = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(pNode->pstChild, "dir");
            type = vtoy_alias_directory;
        }
        
        alias = vtoy_json_get_string_ex(pNode->pstChild, "alias");
        if (path && path[0] == '/' && alias)
        {
            node = grub_zalloc(sizeof(menu_alias));
            if (node)
            {
                node->type = type;
                node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", path);
                grub_snprintf(node->alias, sizeof(node->alias), "%s", alias);

                if (g_menu_alias_head)
                {
                    node->next = g_menu_alias_head;
                }
                
                g_menu_alias_head = node;
            }
        }
    }

    return 0;
}

static int ventoy_plugin_menutip_check(VTOY_JSON *json, const char *isodisk)
{
    int type;
    const char *path = NULL;
    const char *tip = NULL;
    VTOY_JSON *pNode = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_OBJECT)
    {
        grub_printf("Not object %d\n", json->enDataType);
        return 1;
    }

    tip = vtoy_json_get_string_ex(json->pstChild, "left");
    if (tip)
    {
        grub_printf("left: <%s>\n", tip);
    }
    
    tip = vtoy_json_get_string_ex(json->pstChild, "top");
    if (tip)
    {
        grub_printf("top: <%s>\n", tip);
    }
    
    tip = vtoy_json_get_string_ex(json->pstChild, "color");
    if (tip)
    {
        grub_printf("color: <%s>\n", tip);
    }

    pNode = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "tips");
    for (pNode = pNode->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = vtoy_tip_image_file;
        path = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(pNode->pstChild, "dir");
            type = vtoy_tip_directory;
        }
        
        if (path && path[0] == '/')
        {
            if (vtoy_tip_image_file == type)
            {
                if (grub_strchr(path, '*'))
                {
                    grub_printf("image: <%s> [ * ]\n", path);
                }
                else if (ventoy_check_file_exist("%s%s", isodisk, path))
                {
                    grub_printf("image: <%s> [ OK ]\n", path);
                }
                else
                {
                    grub_printf("image: <%s> [ NOT EXIST ]\n", path);
                }
            }
            else
            {
                if (ventoy_is_dir_exist("%s%s", isodisk, path))
                {
                    grub_printf("dir: <%s> [ OK ]\n", path);
                }
                else
                {
                    grub_printf("dir: <%s> [ NOT EXIST ]\n", path);
                }
            }

            tip = vtoy_json_get_string_ex(pNode->pstChild, "tip");
            if (tip)
            {
                grub_printf("tip: <%s>\n", tip);
            }
            else
            {
                tip = vtoy_json_get_string_ex(pNode->pstChild, "tip1");
                if (tip)
                    grub_printf("tip1: <%s>\n", tip);
                else
                    grub_printf("tip1: <NULL>\n");
                
                tip = vtoy_json_get_string_ex(pNode->pstChild, "tip2");
                if (tip)
                    grub_printf("tip2: <%s>\n", tip);
                else
                    grub_printf("tip2: <NULL>\n");
            }
        }
        else
        {
            grub_printf("image: <%s> [ INVALID ]\n", path);
        }
    }

    return 0;
}

static int ventoy_plugin_menutip_entry(VTOY_JSON *json, const char *isodisk)
{
    int type;
    const char *path = NULL;
    const char *tip = NULL;
    VTOY_JSON *pNode = NULL;
    menu_tip *node = NULL;
    menu_tip *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_OBJECT)
    {
        debug("Not object %d\n", json->enDataType);
        return 0;
    }

    pNode = vtoy_json_find_item(json->pstChild, JSON_TYPE_ARRAY, "tips");
    if (pNode == NULL)
    {
        debug("Not tips found\n");
        return 0;
    }

    if (g_menu_tip_head)
    {
        for (node = g_menu_tip_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_menu_tip_head = NULL;
    }

    tip = vtoy_json_get_string_ex(json->pstChild, "left");
    if (tip)
    {
        grub_env_set("VTOY_TIP_LEFT", tip);
    }
    
    tip = vtoy_json_get_string_ex(json->pstChild, "top");
    if (tip)
    {
        grub_env_set("VTOY_TIP_TOP", tip);
    }
    
    tip = vtoy_json_get_string_ex(json->pstChild, "color");
    if (tip)
    {
        grub_env_set("VTOY_TIP_COLOR", tip);
    }

    for (pNode = pNode->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = vtoy_tip_image_file;
        path = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (!path)
        {
            path = vtoy_json_get_string_ex(pNode->pstChild, "dir");
            type = vtoy_tip_directory;
        }
        
        if (path && path[0] == '/')
        {
            node = grub_zalloc(sizeof(menu_tip));
            if (node)
            {
                node->type = type;
                node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", path);

                tip = vtoy_json_get_string_ex(pNode->pstChild, "tip");
                if (tip)
                {
                    grub_snprintf(node->tip1, 1000, "%s", tip);
                }
                else
                {
                    tip = vtoy_json_get_string_ex(pNode->pstChild, "tip1");
                    if (tip)
                        grub_snprintf(node->tip1, 1000, "%s", tip);

                    tip = vtoy_json_get_string_ex(pNode->pstChild, "tip2");
                    if (tip)
                        grub_snprintf(node->tip2, 1000, "%s", tip);
                }

                if (g_menu_tip_head)
                {
                    node->next = g_menu_tip_head;
                }
                
                g_menu_tip_head = node;
            }
        }
    }

    return 0;
}

static int ventoy_plugin_injection_check(VTOY_JSON *json, const char *isodisk)
{
    int type = 0;
    const char *path = NULL;
    const char *archive = NULL;
    VTOY_JSON *pNode = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array %d\n", json->enDataType);
        return 0;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = injection_type_file;
        path = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (!path)
        {
            type = injection_type_parent;
            path = vtoy_json_get_string_ex(pNode->pstChild, "parent");
            if (!path)
            {
                grub_printf("image/parent not found\n");
                continue;
            }
        }

        archive = vtoy_json_get_string_ex(pNode->pstChild, "archive");
        if (!archive)
        {
            grub_printf("archive not found\n");
            continue;
        }

        if (type == injection_type_file)
        {
            if (grub_strchr(path, '*'))
            {
                grub_printf("image: <%s> [*]\n", path);
            }
            else
            {
                grub_printf("image: <%s> [%s]\n", path, ventoy_check_file_exist("%s%s", isodisk, path) ? "OK" : "NOT EXIST");            
            }
        }
        else
        {
            grub_printf("parent: <%s> [%s]\n", path, 
                ventoy_is_dir_exist("%s%s", isodisk, path) ? "OK" : "NOT EXIST");
        }

        grub_printf("archive: <%s> [%s]\n\n", archive, ventoy_check_file_exist("%s%s", isodisk, archive) ? "OK" : "NOT EXIST");
    }

    return 0;
}

static int ventoy_plugin_injection_entry(VTOY_JSON *json, const char *isodisk)
{
    int type = 0;
    const char *path = NULL;
    const char *archive = NULL;
    VTOY_JSON *pNode = NULL;
    injection_config *node = NULL;
    injection_config *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_injection_head)
    {
        for (node = g_injection_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_injection_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = injection_type_file;
        path = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (!path)
        {
            type = injection_type_parent;
            path = vtoy_json_get_string_ex(pNode->pstChild, "parent");
        }
        
        archive = vtoy_json_get_string_ex(pNode->pstChild, "archive");
        if (path && path[0] == '/' && archive && archive[0] == '/')
        {
            node = grub_zalloc(sizeof(injection_config));
            if (node)
            {
                node->type = type;
                node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", path);
                grub_snprintf(node->archive, sizeof(node->archive), "%s", archive);

                if (g_injection_head)
                {
                    node->next = g_injection_head;
                }
                
                g_injection_head = node;
            }
        }
    }

    return 0;
}

static int ventoy_plugin_menuclass_entry(VTOY_JSON *json, const char *isodisk)
{
    int type;
    int parent = 0;
    const char *key = NULL;
    const char *class = NULL;
    VTOY_JSON *pNode = NULL;
    menu_class *tail = NULL;
    menu_class *node = NULL;
    menu_class *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_menu_class_head)
    {
        for (node = g_menu_class_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_menu_class_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        parent = 0;
        type = vtoy_class_image_file;
        key = vtoy_json_get_string_ex(pNode->pstChild, "key");
        if (!key)
        {
            key = vtoy_json_get_string_ex(pNode->pstChild, "parent");
            if (key)
            {
                parent = 1;
            }
            else
            {
                key = vtoy_json_get_string_ex(pNode->pstChild, "dir");
                type = vtoy_class_directory;
            }
        }
        
        class = vtoy_json_get_string_ex(pNode->pstChild, "class");
        if (key && class)
        {
            node = grub_zalloc(sizeof(menu_class));
            if (node)
            {
                node->type = type;
                node->parent = parent;
                node->patlen = grub_snprintf(node->pattern, sizeof(node->pattern), "%s", key);
                grub_snprintf(node->class, sizeof(node->class), "%s", class);

                if (g_menu_class_head)
                {
                    tail->next = node;
                }
                else
                {
                    g_menu_class_head = node;
                }
                tail = node;
            }
        }
    }

    return 0;
}

static int ventoy_plugin_menuclass_check(VTOY_JSON *json, const char *isodisk)
{
    const char *name = NULL;
    const char *key = NULL;
    const char *class = NULL;
    VTOY_JSON *pNode = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        name = "key";
        key = vtoy_json_get_string_ex(pNode->pstChild, "key");
        if (!key)
        {
            name = "parent";
            key = vtoy_json_get_string_ex(pNode->pstChild, "parent");
            if (!key)
            {
                name = "dir";       
                key = vtoy_json_get_string_ex(pNode->pstChild, "dir"); 
            }
        }
        
        class = vtoy_json_get_string_ex(pNode->pstChild, "class");
        if (key && class)
        {
            grub_printf("%s: <%s>\n", name,  key);
            grub_printf("class: <%s>\n\n", class);
        }
    }

    return 0;
}

static int ventoy_plugin_custom_boot_entry(VTOY_JSON *json, const char *isodisk)
{
    int type;
    int len;
    const char *key = NULL;
    const char *cfg = NULL;
    VTOY_JSON *pNode = NULL;
    custom_boot *tail = NULL;
    custom_boot *node = NULL;
    custom_boot *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_custom_boot_head)
    {
        for (node = g_custom_boot_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_custom_boot_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = vtoy_custom_boot_image_file;
        key = vtoy_json_get_string_ex(pNode->pstChild, "file");
        if (!key)
        {
            key = vtoy_json_get_string_ex(pNode->pstChild, "dir");
            type = vtoy_custom_boot_directory;
        }
        
        cfg = vtoy_json_get_string_ex(pNode->pstChild, "vcfg");
        if (key && cfg)
        {
            node = grub_zalloc(sizeof(custom_boot));
            if (node)
            {
                node->type = type;
                node->pathlen = grub_snprintf(node->path, sizeof(node->path), "%s", key);
                len = (int)grub_snprintf(node->cfg, sizeof(node->cfg), "%s", cfg);

                if (len >= 5 && grub_strncmp(node->cfg + len - 5, ".vcfg", 5) == 0)
                {
                    if (g_custom_boot_head)
                    {
                        tail->next = node;
                    }
                    else
                    {
                        g_custom_boot_head = node;
                    }
                    tail = node;
                }
                else
                {
                    grub_free(node);
                }
            }
        }
    }

    return 0;
}

static int ventoy_plugin_custom_boot_check(VTOY_JSON *json, const char *isodisk)
{
    int type;
    int len;
    const char *key = NULL;
    const char *cfg = NULL;
    VTOY_JSON *pNode = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        type = vtoy_custom_boot_image_file;
        key = vtoy_json_get_string_ex(pNode->pstChild, "file");
        if (!key)
        {
            key = vtoy_json_get_string_ex(pNode->pstChild, "dir"); 
            type = vtoy_custom_boot_directory;
        }
        
        cfg = vtoy_json_get_string_ex(pNode->pstChild, "vcfg");
        len = (int)grub_strlen(cfg);
        if (key && cfg)
        {
            if (len < 5 || grub_strncmp(cfg + len - 5, ".vcfg", 5))
            {
                grub_printf("<%s> does not have \".vcfg\" suffix\n\n", cfg);
            }
            else
            {
                grub_printf("%s: <%s>\n", (type == vtoy_custom_boot_directory) ? "dir" : "file",  key);
                grub_printf("vcfg: <%s>\n\n", cfg);                
            }
        }
    }

    return 0;
}

static int ventoy_plugin_conf_replace_entry(VTOY_JSON *json, const char *isodisk)
{
    int img = 0;
    const char *isof = NULL;
    const char *orgf = NULL;
    const char *newf = NULL;
    VTOY_JSON *pNode = NULL;
    conf_replace *tail = NULL;
    conf_replace *node = NULL;
    conf_replace *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_conf_replace_head)
    {
        for (node = g_conf_replace_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_conf_replace_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        isof = vtoy_json_get_string_ex(pNode->pstChild, "iso");
        orgf = vtoy_json_get_string_ex(pNode->pstChild, "org");
        newf = vtoy_json_get_string_ex(pNode->pstChild, "new");
        if (isof && orgf && newf && isof[0] == '/' && orgf[0] == '/' && newf[0] == '/')
        {
            node = grub_zalloc(sizeof(conf_replace));
            if (node)
            {
                if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "img", &img))
                {
                    node->img = img;
                }

                node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", isof);
                grub_snprintf(node->orgconf, sizeof(node->orgconf), "%s", orgf);
                grub_snprintf(node->newconf, sizeof(node->newconf), "%s", newf);

                if (g_conf_replace_head)
                {
                    tail->next = node;
                }
                else
                {
                    g_conf_replace_head = node;
                }
                tail = node;
            }
        }
    }

    return 0;
}

static int ventoy_plugin_conf_replace_check(VTOY_JSON *json, const char *isodisk)
{
    int img = 0;
    const char *isof = NULL;
    const char *orgf = NULL;
    const char *newf = NULL;
    VTOY_JSON *pNode = NULL;
    grub_file_t file = NULL;
    char cmd[256];

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        isof = vtoy_json_get_string_ex(pNode->pstChild, "iso");
        orgf = vtoy_json_get_string_ex(pNode->pstChild, "org");
        newf = vtoy_json_get_string_ex(pNode->pstChild, "new");
        if (isof && orgf && newf && isof[0] == '/' && orgf[0] == '/' && newf[0] == '/')
        {
            if (ventoy_check_file_exist("%s%s", isodisk, isof))
            {
                grub_printf("iso:<%s> [OK]\n", isof);
                
                grub_snprintf(cmd, sizeof(cmd), "loopback vtisocheck \"%s%s\"", isodisk, isof);
                grub_script_execute_sourcecode(cmd);

                file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "(vtisocheck)/%s", orgf);
                if (file)
                {
                    if (grub_strcmp(file->fs->name, "iso9660") == 0)
                    {
                        grub_printf("org:<%s> [OK]\n", orgf);
                    }
                    else
                    {
                        grub_printf("org:<%s> [Exist But NOT ISO9660]\n", orgf);
                    }
                    grub_file_close(file);
                }
                else
                {
                    grub_printf("org:<%s> [NOT Exist]\n", orgf);
                }
                
                grub_script_execute_sourcecode("loopback -d vtisocheck");
            }
            else if (grub_strchr(isof, '*'))
            {
                grub_printf("iso:<%s> [*]\n", isof);
                grub_printf("org:<%s>\n", orgf);
            }
            else
            {
                grub_printf("iso:<%s> [NOT Exist]\n", isof);
                grub_printf("org:<%s>\n", orgf);
            }

            file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", isodisk, newf);
            if (file)
            {
                if (file->size > vtoy_max_replace_file_size)
                {
                    grub_printf("new:<%s> [Too Big %lu] \n", newf, (ulong)file->size);
                }
                else
                {
                    grub_printf("new1:<%s> [OK]\n", newf);                    
                }
                grub_file_close(file);
            }
            else
            {
                grub_printf("new:<%s> [NOT Exist]\n", newf);   
            }

            if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "img", &img))
            {
                grub_printf("img:<%d>\n", img);           
            }
            
            grub_printf("\n");
        }
    }

    return 0;
}

static int ventoy_plugin_auto_memdisk_entry(VTOY_JSON *json, const char *isodisk)
{
    VTOY_JSON *pNode = NULL;
    auto_memdisk *node = NULL;
    auto_memdisk *next = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_auto_memdisk_head)
    {
        for (node = g_auto_memdisk_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_auto_memdisk_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType == JSON_TYPE_STRING)
        {
            node = grub_zalloc(sizeof(auto_memdisk));
            if (node)
            {
                node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", pNode->unData.pcStrVal);

                if (g_auto_memdisk_head)
                {
                    node->next = g_auto_memdisk_head;
                }
                
                g_auto_memdisk_head = node;
            }
        }
    }

    return 0;
}

static int ventoy_plugin_auto_memdisk_check(VTOY_JSON *json, const char *isodisk)
{
    VTOY_JSON *pNode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType == JSON_TYPE_STRING)
        {
            grub_printf("<%s> ", pNode->unData.pcStrVal);

            if (grub_strchr(pNode->unData.pcStrVal, '*'))
            {
                grub_printf(" [*]\n");
            }
            else if (ventoy_check_file_exist("%s%s", isodisk, pNode->unData.pcStrVal))
            {
                grub_printf(" [OK]\n");
            }
            else
            {
                grub_printf(" [NOT EXIST]\n");
            }
        }
    }

    return 0;
}

static int ventoy_plugin_image_list_entry(VTOY_JSON *json, const char *isodisk)
{
    VTOY_JSON *pNode = NULL;
    image_list *node = NULL;
    image_list *next = NULL;
    image_list *tail = NULL;

    (void)isodisk;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        debug("Not array %d\n", json->enDataType);
        return 0;
    }

    if (g_image_list_head)
    {
        for (node = g_image_list_head; node; node = next)
        {
            next = node->next;
            grub_free(node);
        }

        g_image_list_head = NULL;
    }

    if (grub_strncmp(json->pcName, "image_blacklist", 15) == 0)
    {
        g_plugin_image_list = VENTOY_IMG_BLACK_LIST;
    }
    else
    {
        g_plugin_image_list = VENTOY_IMG_WHITE_LIST;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType == JSON_TYPE_STRING)
        {
            node = grub_zalloc(sizeof(image_list));
            if (node)
            {
                node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", pNode->unData.pcStrVal);

                if (g_image_list_head)
                {
                    tail->next = node;
                }
                else
                {
                    g_image_list_head = node;
                }
                tail = node;
            }
        }
    }

    return 0;
}

static int ventoy_plugin_image_list_check(VTOY_JSON *json, const char *isodisk)
{
    VTOY_JSON *pNode = NULL;

    if (json->enDataType != JSON_TYPE_ARRAY)
    {
        grub_printf("Not array %d\n", json->enDataType);
        return 1;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        if (pNode->enDataType == JSON_TYPE_STRING)
        {
            grub_printf("<%s> ", pNode->unData.pcStrVal);

            if (grub_strchr(pNode->unData.pcStrVal, '*'))
            {
                grub_printf(" [*]\n");
            }
            else if (ventoy_check_file_exist("%s%s", isodisk, pNode->unData.pcStrVal))
            {
                grub_printf(" [OK]\n");
            }
            else
            {
                grub_printf(" [NOT EXIST]\n");
            }
        }
    }

    return 0;
}

static plugin_entry g_plugin_entries[] = 
{
    { "control", ventoy_plugin_control_entry, ventoy_plugin_control_check, 0 },
    { "theme", ventoy_plugin_theme_entry, ventoy_plugin_theme_check, 0 },
    { "auto_install", ventoy_plugin_auto_install_entry, ventoy_plugin_auto_install_check, 0 },
    { "persistence", ventoy_plugin_persistence_entry, ventoy_plugin_persistence_check, 0 },
    { "menu_alias", ventoy_plugin_menualias_entry, ventoy_plugin_menualias_check, 0 },
    { "menu_tip", ventoy_plugin_menutip_entry, ventoy_plugin_menutip_check, 0 },
    { "menu_class", ventoy_plugin_menuclass_entry, ventoy_plugin_menuclass_check, 0 },
    { "injection", ventoy_plugin_injection_entry, ventoy_plugin_injection_check, 0 },
    { "auto_memdisk", ventoy_plugin_auto_memdisk_entry, ventoy_plugin_auto_memdisk_check, 0 },
    { "image_list", ventoy_plugin_image_list_entry, ventoy_plugin_image_list_check, 0 },
    { "image_blacklist", ventoy_plugin_image_list_entry, ventoy_plugin_image_list_check, 0 },
    { "conf_replace", ventoy_plugin_conf_replace_entry, ventoy_plugin_conf_replace_check, 0 },
    { "dud", ventoy_plugin_dud_entry, ventoy_plugin_dud_check, 0 },
    { "password", ventoy_plugin_pwd_entry, ventoy_plugin_pwd_check, 0 },
    { "custom_boot", ventoy_plugin_custom_boot_entry, ventoy_plugin_custom_boot_check, 0 },
};

static int ventoy_parse_plugin_config(VTOY_JSON *json, const char *isodisk)
{
    int i;
    char key[128];
    VTOY_JSON *cur = NULL;

    grub_snprintf(g_iso_disk_name, sizeof(g_iso_disk_name), "%s", isodisk);

    for (cur = json; cur; cur = cur->pstNext)
    {
        for (i = 0; i < (int)ARRAY_SIZE(g_plugin_entries); i++)
        {
            grub_snprintf(key, sizeof(key), "%s_%s", g_plugin_entries[i].key, g_arch_mode_suffix);
            if (g_plugin_entries[i].flag == 0 && grub_strcmp(key, cur->pcName) == 0)
            {
                debug("Plugin entry for %s\n", g_plugin_entries[i].key);
                g_plugin_entries[i].entryfunc(cur, isodisk);
                g_plugin_entries[i].flag = 1;
                break;
            }
        }
    }

    
    for (cur = json; cur; cur = cur->pstNext)
    {
        for (i = 0; i < (int)ARRAY_SIZE(g_plugin_entries); i++)
        {
            if (g_plugin_entries[i].flag == 0 && grub_strcmp(g_plugin_entries[i].key, cur->pcName) == 0)
            {
                debug("Plugin entry for %s\n", g_plugin_entries[i].key);
                g_plugin_entries[i].entryfunc(cur, isodisk);
                g_plugin_entries[i].flag = 1;
                break;
            }
        }
    }

    return 0;
}

grub_err_t ventoy_cmd_load_plugin(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 0;
    int offset = 0;
    char *buf = NULL;
    grub_uint8_t *code = NULL;
    grub_file_t file;
    VTOY_JSON *json = NULL;
    
    (void)ctxt;
    (void)argc;

    grub_env_set("VTOY_TIP_LEFT", "10%");
    grub_env_set("VTOY_TIP_TOP", "80%+5");
    grub_env_set("VTOY_TIP_COLOR", "blue");
    grub_env_set("VTOY_TIP_ALIGN", "left");

    file = ventoy_grub_file_open(GRUB_FILE_TYPE_LINUX_INITRD, "%s/ventoy/ventoy.json", args[0]);
    if (!file)
    {
        return GRUB_ERR_NONE;
    }

    debug("json configuration file size %d\n", (int)file->size);

    buf = grub_malloc(file->size + 1);
    if (!buf)
    {
        grub_file_close(file);
        return 1;
    }
    
    buf[file->size] = 0;
    grub_file_read(file, buf, file->size);
    grub_file_close(file);

    json = vtoy_json_create();
    if (!json)
    {
        return 1;
    }

    code = (grub_uint8_t *)buf;
    if (code[0] == 0xef && code[1] == 0xbb && code[2] == 0xbf)
    {
        offset = 3; /* Skip UTF-8 BOM */
    }
    else if ((code[0] == 0xff && code[1] == 0xfe) || (code[0] == 0xfe && code[1] == 0xff))
    {
        grub_env_set("VTOY_PLUGIN_SYNTAX_ERROR", "1");
        grub_env_export("VTOY_PLUGIN_SYNTAX_ERROR");

        grub_env_set("VTOY_PLUGIN_ENCODE_ERROR", "1");
        grub_env_export("VTOY_PLUGIN_ENCODE_ERROR");

        debug("Failed to parse json string %d\n", ret);
        grub_free(buf);
        return 1;
    }
    
    ret = vtoy_json_parse(json, buf + offset);
    if (ret)
    {
        grub_env_set("VTOY_PLUGIN_SYNTAX_ERROR", "1");
        grub_env_export("VTOY_PLUGIN_SYNTAX_ERROR");
        
        debug("Failed to parse json string %d\n", ret);
        grub_free(buf);
        return 1;
    }

    ventoy_parse_plugin_config(json->pstChild, args[0]);

    vtoy_json_destroy(json);

    grub_free(buf);

    if (g_boot_pwd.type)
    {        
        grub_printf("\n\n======= %s ======\n\n", grub_env_get("VTOY_TEXT_MENU_VER"));
        if (ventoy_check_password(&g_boot_pwd, 3))
        {
            grub_printf("\n!!! Password check failed, will exit after 5 seconds. !!!\n");
            grub_refresh();
            grub_sleep(5);
            grub_exit();
        }
    }

    if (g_menu_tip_head)
    {
        grub_env_set("VTOY_MENU_TIP_ENABLE", "1");
    }
    else
    {
        grub_env_unset("VTOY_MENU_TIP_ENABLE");
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

void ventoy_plugin_dump_injection(void)
{
    injection_config *node = NULL;

    for (node = g_injection_head; node; node = node->next)
    {
        grub_printf("\n%s:<%s>\n", (node->type == injection_type_file) ? "IMAGE" : "PARENT", node->isopath);
        grub_printf("ARCHIVE:<%s>\n", node->archive);
    }

    return;
}


void ventoy_plugin_dump_auto_install(void)
{
    int i;
    install_template *node = NULL;

    for (node = g_install_template_head; node; node = node->next)
    {
        grub_printf("\n%s:<%s> <%d>\n", 
            (node->type == auto_install_type_file) ? "IMAGE" : "PARENT",
            node->isopath, node->templatenum);
        for (i = 0; i < node->templatenum; i++)
        {
            grub_printf("SCRIPT %d:<%s>\n", i, node->templatepath[i].path);            
        }
    }

    return;
}

void ventoy_plugin_dump_persistence(void)
{
    int rc;
    int i = 0;
    persistence_config *node = NULL;
    ventoy_img_chunk_list chunk_list;

    for (node = g_persistence_head; node; node = node->next)
    {
        grub_printf("\nIMAGE:<%s> <%d>\n", node->isopath, node->backendnum);

        for (i = 0; i < node->backendnum; i++)
        {
            grub_printf("PERSIST %d:<%s>", i, node->backendpath[i].path);            
            rc = ventoy_plugin_get_persistent_chunklist(node->isopath, i, &chunk_list);
            if (rc == 0)
            {
                grub_printf(" [ SUCCESS ]\n");
                grub_free(chunk_list.chunk);
            }
            else
            {
                grub_printf(" [ FAILED ]\n");
            }
        }
    }

    return;
}

install_template * ventoy_plugin_find_install_template(const char *isopath)
{
    int len;
    install_template *node = NULL;

    if (!g_install_template_head)
    {
        return NULL;
    }

    len = (int)grub_strlen(isopath);
    for (node = g_install_template_head; node; node = node->next)
    {
        if (node->type == auto_install_type_file)
        {
            if (node->pathlen == len && ventoy_strcmp(node->isopath, isopath) == 0)
            {
                return node;
            }
        }
    }
    
    for (node = g_install_template_head; node; node = node->next)
    {
        if (node->type == auto_install_type_parent)
        {
            if (node->pathlen < len && ventoy_plugin_is_parent(node->isopath, node->pathlen, isopath))
            {
                return node;
            }
        }
    }

    return NULL;
}

char * ventoy_plugin_get_cur_install_template(const char *isopath, install_template **cur)
{
    install_template *node = NULL;

    if (cur)
    {
        *cur = NULL;
    }

    node = ventoy_plugin_find_install_template(isopath);
    if ((!node) || (!node->templatepath))
    {
        return NULL;
    }

    if (node->cursel < 0 || node->cursel >= node->templatenum)
    {
        return NULL;
    }

    if (cur)
    {
        *cur = node;
    }

    return node->templatepath[node->cursel].path;
}

persistence_config * ventoy_plugin_find_persistent(const char *isopath)
{
    int len;
    persistence_config *node = NULL;

    if (!g_persistence_head)
    {
        return NULL;
    }

    len = (int)grub_strlen(isopath);
    for (node = g_persistence_head; node; node = node->next)
    {
        if ((len == node->pathlen) && (ventoy_strcmp(node->isopath, isopath) == 0))
        {
            return node;
        }
    }

    return NULL;
}

int ventoy_plugin_get_persistent_chunklist(const char *isopath, int index, ventoy_img_chunk_list *chunk_list)
{
    int rc = 1;
    int len = 0;
    char *path = NULL;
    grub_uint64_t start = 0;
    grub_file_t file = NULL;
    persistence_config *node = NULL;

    node = ventoy_plugin_find_persistent(isopath);
    if ((!node) || (!node->backendpath))
    {
        return 1;
    }

    if (index < 0)
    {
        index = node->cursel;
    }

    if (index < 0 || index >= node->backendnum)
    {
        return 1;
    }

    path = node->backendpath[index].path;

    if (node->backendpath[index].vlnk_add == 0)
    {
        len = grub_strlen(path);
        if (len > 9 && grub_strncmp(path + len - 9, ".vlnk.dat", 9) == 0)
        {
            ventoy_add_vlnk_file(NULL, path);
            node->backendpath[index].vlnk_add = 1;
        }
    }
    
    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", g_iso_disk_name, path);
    if (!file)
    {
        debug("Failed to open file %s%s\n", g_iso_disk_name, path);
        goto end;
    }

    grub_memset(chunk_list, 0, sizeof(ventoy_img_chunk_list));
    chunk_list->chunk = grub_malloc(sizeof(ventoy_img_chunk) * DEFAULT_CHUNK_NUM);
    if (NULL == chunk_list->chunk)
    {
        goto end;
    }
    
    chunk_list->max_chunk = DEFAULT_CHUNK_NUM;
    chunk_list->cur_chunk = 0;

    start = file->device->disk->partition->start;
    ventoy_get_block_list(file, chunk_list, start);
    
    if (0 != ventoy_check_block_list(file, chunk_list, start))
    {
        grub_free(chunk_list->chunk);
        chunk_list->chunk = NULL;
        goto end;
    }

    rc = 0;

end:
    if (file)
        grub_file_close(file);

    return rc;
}

const char * ventoy_plugin_get_injection(const char *isopath)
{
    int len;
    injection_config *node = NULL;

    if (!g_injection_head)
    {
        return NULL;
    }

    len = (int)grub_strlen(isopath);
    for (node = g_injection_head; node; node = node->next)
    {
        if (node->type == injection_type_file)
        {
            if (node->pathlen == len && ventoy_strcmp(node->isopath, isopath) == 0)
            {
                return node->archive;
            }
        }
    }
    
    for (node = g_injection_head; node; node = node->next)
    {
        if (node->type == injection_type_parent)
        {
            if (node->pathlen < len && ventoy_plugin_is_parent(node->isopath, node->pathlen, isopath))
            {
                return node->archive;
            }
        }
    }

    return NULL;
}

const char * ventoy_plugin_get_menu_alias(int type, const char *isopath)
{
    int len;
    menu_alias *node = NULL;

    if (!g_menu_alias_head)
    {
        return NULL;
    }

    len = (int)grub_strlen(isopath);
    for (node = g_menu_alias_head; node; node = node->next)
    {
        if (node->type == type && node->pathlen && 
            node->pathlen == len && ventoy_strcmp(node->isopath, isopath) == 0)
        {
            return node->alias;
        }
    }

    return NULL;
}

const menu_tip * ventoy_plugin_get_menu_tip(int type, const char *isopath)
{
    int len;
    menu_tip *node = NULL;

    if (!g_menu_tip_head)
    {
        return NULL;
    }

    len = (int)grub_strlen(isopath);
    for (node = g_menu_tip_head; node; node = node->next)
    {
        if (node->type == type && node->pathlen && 
            node->pathlen == len && ventoy_strcmp(node->isopath, isopath) == 0)
        {
            return node;
        }
    }

    return NULL;
}

const char * ventoy_plugin_get_menu_class(int type, const char *name, const char *path)
{
    int namelen;
    int pathlen;
    menu_class *node = NULL;

    if (!g_menu_class_head)
    {
        return NULL;
    }
    
    namelen = (int)grub_strlen(name); 
    pathlen = (int)grub_strlen(path); 
    
    if (vtoy_class_image_file == type)
    {
        for (node = g_menu_class_head; node; node = node->next)
        {
            if (node->type != type)
            {
                continue;
            }

            if (node->parent == 0)
            {
                if ((node->patlen < namelen) && grub_strstr(name, node->pattern))
                {
                    return node->class;
                }
            }
        }
        
        for (node = g_menu_class_head; node; node = node->next)
        {
            if (node->type != type)
            {
                continue;
            }

            if (node->parent)
            {
                if ((node->patlen < pathlen) && ventoy_plugin_is_parent(node->pattern, node->patlen, path))
                {
                    return node->class;
                }
            }
        }
    }
    else
    {
        for (node = g_menu_class_head; node; node = node->next)
        {
            if (node->type == type && node->patlen == namelen && grub_strncmp(name, node->pattern, namelen) == 0)
            {
                return node->class;
            }
        }
    }

    return NULL;
}

int ventoy_plugin_add_custom_boot(const char *vcfgpath)
{
    int len;
    custom_boot *node = NULL;
    
    node = grub_zalloc(sizeof(custom_boot));
    if (node)
    {
        node->type = vtoy_custom_boot_image_file;
        node->pathlen = grub_snprintf(node->path, sizeof(node->path), "%s", vcfgpath);
        grub_snprintf(node->cfg, sizeof(node->cfg), "%s", vcfgpath);

        /* .vcfg */
        len = node->pathlen - 5;
        node->path[len] = 0;
        node->pathlen = len;

        if (g_custom_boot_head)
        {
            node->next = g_custom_boot_head;
        }
        g_custom_boot_head = node;
    }
    
    return 0;
}

const char * ventoy_plugin_get_custom_boot(const char *isopath)
{
    int i;
    int len;
    custom_boot *node = NULL;

    if (!g_custom_boot_head)
    {
        return NULL;
    }

    len = (int)grub_strlen(isopath);
    
    for (node = g_custom_boot_head; node; node = node->next)
    {
        if (node->type == vtoy_custom_boot_image_file)
        {
            if (node->pathlen == len && grub_strncmp(isopath, node->path, len) == 0)
            {
                return node->cfg;                
            }
        }
        else
        {
            if (node->pathlen < len && isopath[node->pathlen] == '/' && 
                grub_strncmp(isopath, node->path, node->pathlen) == 0)
            {
                for (i = node->pathlen + 1; i < len; i++)
                {
                    if (isopath[i] == '/')
                    {
                        break;
                    }
                }

                if (i >= len)
                {
                    return node->cfg;                
                }
            }
        }
    }

    return NULL;
}

grub_err_t ventoy_cmd_dump_custom_boot(grub_extcmd_context_t ctxt, int argc, char **args)
{
    custom_boot *node = NULL;

    (void)argc;
    (void)ctxt;
    (void)args;

    for (node = g_custom_boot_head; node; node = node->next)
    {
        grub_printf("[%s] <%s>:<%s>\n", (node->type == vtoy_custom_boot_directory) ? "dir" : "file", 
            node->path, node->cfg);
    }

    return 0;
}

int ventoy_plugin_check_memdisk(const char *isopath)
{
    int len;
    auto_memdisk *node = NULL;

    if (!g_auto_memdisk_head)
    {
        return 0;
    }

    len = (int)grub_strlen(isopath);    
    for (node = g_auto_memdisk_head; node; node = node->next)
    {
        if (node->pathlen == len && ventoy_strncmp(node->isopath, isopath, len) == 0)
        {
            return 1;
        }
    }

    return 0;
}

int ventoy_plugin_get_image_list_index(int type, const char *name)
{
    int len;
    int index = 1;
    image_list *node = NULL;

    if (!g_image_list_head)
    {
        return 0;
    }

    len = (int)grub_strlen(name);    
    
    for (node = g_image_list_head; node; node = node->next, index++)
    {
        if (vtoy_class_directory == type)
        {
            if (len < node->pathlen && ventoy_strncmp(node->isopath, name, len) == 0)
            {
                return index;
            }
        }
        else
        {
            if (len == node->pathlen && ventoy_strncmp(node->isopath, name, len) == 0)
            {
                return index;
            }
        }
    }

    return 0;
}

int ventoy_plugin_find_conf_replace(const char *iso, conf_replace *nodes[VTOY_MAX_CONF_REPLACE])
{
    int n = 0;
    int len;
    conf_replace *node;

    if (!g_conf_replace_head)
    {
        return 0;
    }

    len = (int)grub_strlen(iso);
    
    for (node = g_conf_replace_head; node; node = node->next)
    {
        if (node->pathlen == len && ventoy_strncmp(node->isopath, iso, len) == 0)
        {
            nodes[n++] = node;
            if (n >= VTOY_MAX_CONF_REPLACE)
            {
                return n;
            }
        }
    }
    
    return n;
}

dud * ventoy_plugin_find_dud(const char *iso)
{
    int len;
    dud *node;

    if (!g_dud_head)
    {
        return NULL;
    }

    len = (int)grub_strlen(iso);
    for (node = g_dud_head; node; node = node->next)
    {
        if (node->pathlen == len && ventoy_strncmp(node->isopath, iso, len) == 0)
        {
            return node;
        }
    }
    
    return NULL;
}

int ventoy_plugin_load_dud(dud *node, const char *isopart)
{
    int i;
    char *buf;
    grub_file_t file;

    for (i = 0; i < node->dudnum; i++)
    {
        if (node->files[i].size > 0)
        {
            debug("file %d has been loaded\n", i);
            continue;
        }
    
        file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", isopart, node->dudpath[i].path);
        if (file)
        {
            buf = grub_malloc(file->size);
            if (buf)
            {
                grub_file_read(file, buf, file->size);            
                node->files[i].size = (int)file->size;
                node->files[i].buf = buf;                
            }
            grub_file_close(file);
        }
    }

    return 0;
}

static const vtoy_password * ventoy_plugin_get_password(const char *isopath)
{
    int i;
    int len;
    const char *pos = NULL;
    menu_password *node = NULL;

    if (!isopath)
    {
        return NULL;
    }

    if (g_pwd_head)
    {
        len = (int)grub_strlen(isopath);    
        for (node = g_pwd_head; node; node = node->next)
        {
            if (node->type == vtoy_menu_pwd_file)
            {
                if (node->pathlen == len && ventoy_strncmp(node->isopath, isopath, len) == 0)
                {
                    return &(node->password);
                }
            }
        }

        for (node = g_pwd_head; node; node = node->next)
        {   
            if (node->type == vtoy_menu_pwd_parent)
            {
                if (node->pathlen < len && ventoy_plugin_is_parent(node->isopath, node->pathlen, isopath))
                {
                    return &(node->password);
                }
            }
        }
    }

    while (*isopath)
    {
        if (*isopath == '.')
        {
            pos = isopath;
        }
        isopath++;
    }

    if (pos)
    {
        for (i = 0; i < (int)ARRAY_SIZE(g_menu_prefix); i++)
        {
            if (g_file_type_pwd[i].type && 0 == grub_strcasecmp(pos + 1, g_menu_prefix[i]))
            {
                return g_file_type_pwd + i;
            }
        }
    }

    return NULL;
}

grub_err_t ventoy_cmd_check_password(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret;
    const vtoy_password *pwd = NULL;
    
    (void)ctxt;
    (void)argc;

    pwd = ventoy_plugin_get_password(args[0]);
    if (pwd)
    {
        if (0 == ventoy_check_password(pwd, 1))
        {
            ret = 1;
        }
        else
        {
            ret = 0;
        }
    }
    else
    {
        ret = 1;
    }

    grub_errno = 0;
    return ret;
}

grub_err_t ventoy_cmd_plugin_check_json(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    int ret = 0;
    char *buf = NULL;
    char key[128];
    grub_file_t file;
    VTOY_JSON *node = NULL;
    VTOY_JSON *json = NULL;
    
    (void)ctxt;

    if (argc != 3)
    {
        return 0;
    }

    file = ventoy_grub_file_open(GRUB_FILE_TYPE_LINUX_INITRD, "%s/ventoy/ventoy.json", args[0]);
    if (!file)
    {
        grub_printf("Plugin json file /ventoy/ventoy.json does NOT exist.\n");
        grub_printf("Attention: directory name and filename are both case-sensitive.\n");
        goto end;
    }

    buf = grub_malloc(file->size + 1);
    if (!buf)
    {
        grub_printf("Failed to malloc memory %lu.\n", (ulong)(file->size + 1));
        goto end;
    }
    
    buf[file->size] = 0;
    grub_file_read(file, buf, file->size);

    json = vtoy_json_create();
    if (!json)
    {
        grub_printf("Failed to create json\n");
        goto end;
    }

    ret = vtoy_json_parse(json, buf);
    if (ret)
    {
        grub_printf("Syntax error detected in ventoy.json, please check it.\n");
        goto end;
    }

    grub_snprintf(key, sizeof(key), "%s_%s", args[1], g_arch_mode_suffix);
    for (node = json->pstChild; node; node = node->pstNext)
    {
        if (grub_strcmp(node->pcName, key) == 0)
        {
            break;
        }
    }

    if (!node)
    {
        for (node = json->pstChild; node; node = node->pstNext)
        {
            if (grub_strcmp(node->pcName, args[1]) == 0)
            {
                break;
            }
        }

        if (!node)
        {
            grub_printf("%s is NOT found in ventoy.json\n", args[1]);
            goto end;            
        }
    }

    for (i = 0; i < (int)ARRAY_SIZE(g_plugin_entries); i++)
    {
        if (grub_strcmp(g_plugin_entries[i].key, args[1]) == 0)
        {
            if (g_plugin_entries[i].checkfunc)
            {
                ret = g_plugin_entries[i].checkfunc(node, args[2]);                
            }
            break;
        }
    }
    
end:
    check_free(file, grub_file_close);
    check_free(json, vtoy_json_destroy);
    grub_check_free(buf);

    return 0;
}

grub_err_t ventoy_cmd_select_theme_cfg(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int pos = 0;
    int bufsize = 0;
    char *name = NULL;
    char *buf = NULL;
    theme_list *node = NULL;

    (void)argc;
    (void)args;
    (void)ctxt;

    if (g_theme_single_file[0])
    {
        return 0;
    }

    if (g_theme_num < 2)
    {
        return 0;
    }

    bufsize = (g_theme_num + 1) * 1024;
    buf = grub_malloc(bufsize);
    if (!buf)
    {
        return 0;
    }
    
    for (node = g_theme_head; node; node = node->next)
    {
        name = grub_strstr(node->theme.path, ")/");
        if (name)
        {
            name++;
        }
        else
        {
            name = node->theme.path;
        }
    
        pos += grub_snprintf(buf + pos, bufsize - pos, 
            "menuentry \"%s\" --class=debug_theme_item --class=debug_theme_select --class=F5tool {\n"
                "vt_set_theme_path \"%s\"\n"
            "}\n",
            name, node->theme.path);
    }

    pos += grub_snprintf(buf + pos, bufsize - pos, 
            "menuentry \"$VTLANG_RETURN_PREVIOUS\" --class=vtoyret VTOY_RET {\n"
                "echo 'Return ...'\n"
            "}\n");

    grub_script_execute_sourcecode(buf);
    grub_free(buf);
    
    return 0;
}

extern char g_ventoy_theme_path[256];

grub_err_t ventoy_cmd_set_theme(grub_extcmd_context_t ctxt, int argc, char **args)
{
    grub_uint32_t i = 0;
    grub_uint32_t mod = 0;
    grub_uint32_t theme_num = 0;
    theme_list *node = g_theme_head;
    struct grub_datetime datetime;
    struct grub_video_mode_info info;
    char buf[64];
    char **pThemePath = NULL;

    (void)argc;
    (void)args;
    (void)ctxt;

    if (g_theme_single_file[0])
    {
        debug("single theme %s\n", g_theme_single_file);
        grub_env_set("theme", g_theme_single_file);
        goto end;
    }

    debug("g_theme_num = %d\n", g_theme_num);
    
    if (g_theme_num == 0)
    {
        goto end;
    }
    
    if (g_theme_id > 0 && g_theme_id <= g_theme_num)
    {
        for (i = 0; i < (grub_uint32_t)(g_theme_id - 1) && node; i++)
        {
            node = node->next;
        }

        grub_env_set("theme", node->theme.path);
        goto end;
    }

    pThemePath = (char **)grub_zalloc(sizeof(char *) * g_theme_num);
    if (!pThemePath)
    {
        goto end;
    }

    if (g_theme_res_fit)
    {
        if (grub_video_get_info(&info) == GRUB_ERR_NONE)
        {
            debug("get video info success %ux%u\n", info.width, info.height);
            grub_snprintf(buf, sizeof(buf), "%ux%u", info.width, info.height);
            for (node = g_theme_head; node; node = node->next)
            {
                if (grub_strstr(node->theme.path, buf))
                {
                    pThemePath[theme_num++] = node->theme.path;
                }
            }
        }
    }

    if (theme_num == 0)
    {
        for (node = g_theme_head; node; node = node->next)
        {
            pThemePath[theme_num++] = node->theme.path;
        }
    }

    if (theme_num == 1)
    {
        mod = 0;
        debug("Only 1 theme match, no need to random.\n");
    }
    else
    {
        grub_memset(&datetime, 0, sizeof(datetime));
        grub_get_datetime(&datetime);

        if (g_theme_random == vtoy_theme_random_boot_second)
        {
            grub_divmod32((grub_uint32_t)datetime.second, theme_num, &mod);
        }
        else if (g_theme_random == vtoy_theme_random_boot_day)
        {
            grub_divmod32((grub_uint32_t)datetime.day, theme_num, &mod);
        }
        else if (g_theme_random == vtoy_theme_random_boot_month)
        {
            grub_divmod32((grub_uint32_t)datetime.month, theme_num, &mod);
        }

        debug("%04d/%02d/%02d %02d:%02d:%02d theme_num:%d mod:%d\n",
              datetime.year, datetime.month, datetime.day,
              datetime.hour, datetime.minute, datetime.second,
              theme_num, mod);
    }

    if (argc > 0 && grub_strcmp(args[0], "switch") == 0)
    {
        grub_snprintf(g_ventoy_theme_path, sizeof(g_ventoy_theme_path), "%s", pThemePath[mod]);        
    }
    else
    {        
        debug("random theme %s\n", pThemePath[mod]);
        grub_env_set("theme", pThemePath[mod]);
    }
    g_ventoy_menu_refresh = 1;

end:

    grub_check_free(pThemePath);
    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

grub_err_t ventoy_cmd_set_theme_path(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)argc;
    (void)ctxt;

    if (argc == 0)
    {
        g_ventoy_theme_path[0] = 0;
    }
    else
    {
        grub_snprintf(g_ventoy_theme_path, sizeof(g_ventoy_theme_path), "%s", args[0]);
    }

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

const char *ventoy_get_vmenu_title(const char *vMenu)
{
    return vtoy_json_get_string_ex(g_menu_lang_json->pstChild, vMenu);
}

int ventoy_plugin_load_menu_lang(int init, const char *lang)
{
    int ret = 1;
    grub_file_t file = NULL;
    char *buf = NULL;

    if (grub_strcmp(lang, g_cur_menu_language) == 0)
    {
        debug("Same menu lang %s\n", lang);
        return 0;
    }
    grub_snprintf(g_cur_menu_language, sizeof(g_cur_menu_language), "%s", lang);

    debug("Load menu lang %s\n", g_cur_menu_language);

    if (g_menu_lang_json)
    {
        vtoy_json_destroy(g_menu_lang_json);
        g_menu_lang_json = NULL;
    }

    g_menu_lang_json = vtoy_json_create();
    if (!g_menu_lang_json)
    {
        goto end;
    }

    file = ventoy_grub_file_open(GRUB_FILE_TYPE_LINUX_INITRD, "(vt_menu_tarfs)/menu/%s.json", lang);
    if (!file)
    {
        goto end;
    }

    buf = grub_malloc(file->size + 1);
    if (!buf)
    {
        grub_printf("Failed to malloc memory %lu.\n", (ulong)(file->size + 1));
        goto end;
    }

    buf[file->size] = 0;
    grub_file_read(file, buf, file->size);

    vtoy_json_parse(g_menu_lang_json, buf);

    if (g_default_menu_mode == 0)
    {
        grub_snprintf(g_ventoy_hotkey_tip, sizeof(g_ventoy_hotkey_tip), "%s", ventoy_get_vmenu_title("VTLANG_STR_HOTKEY_TREE"));
    }
    else
    {
        grub_snprintf(g_ventoy_hotkey_tip, sizeof(g_ventoy_hotkey_tip), "%s", ventoy_get_vmenu_title("VTLANG_STR_HOTKEY_LIST"));
    }

    if (init == 0)
    {
        ventoy_menu_push_key(GRUB_TERM_ESC);
        ventoy_menu_push_key(GRUB_TERM_ESC);
        g_ventoy_menu_refresh = 1;        
    }
    ret = 0;

end:

    check_free(file, grub_file_close);
    grub_check_free(buf);

    return ret;
}

grub_err_t ventoy_cmd_cur_menu_lang(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)ctxt;
    (void)argc;

    if (argc > 0)
    {
        grub_env_set(args[0], g_cur_menu_language);
    }
    else
    {
        grub_printf("%s\n", g_cur_menu_language);
        grub_printf("%s\n", g_ventoy_hotkey_tip);
        grub_refresh();        
    }

    VENTOY_CMD_RETURN(0);
}

grub_err_t ventoy_cmd_push_menulang(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)argc;
    (void)ctxt;

    if (g_push_menu_language[0] == 0)
    {
        grub_memcpy(g_push_menu_language, g_cur_menu_language, sizeof(g_push_menu_language));
        ventoy_plugin_load_menu_lang(0, args[0]);
    }

    VENTOY_CMD_RETURN(0);
}

grub_err_t ventoy_cmd_pop_menulang(grub_extcmd_context_t ctxt, int argc, char **args)
{
    (void)argc;
    (void)ctxt;
    (void)args;

    if (g_push_menu_language[0])
    {
        ventoy_plugin_load_menu_lang(0, g_push_menu_language);
        g_push_menu_language[0] = 0;
    }

    VENTOY_CMD_RETURN(0);
}


