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
#include <grub/time.h>
#include <grub/font.h>
#include <grub/ventoy.h>
#include "ventoy_def.h"

GRUB_MOD_LICENSE ("GPLv3+");

static char g_iso_disk_name[128];
static install_template *g_install_template_head = NULL;
static persistence_config *g_persistence_head = NULL;
static menu_alias *g_menu_alias_head = NULL;
static menu_class *g_menu_class_head = NULL;
static injection_config *g_injection_head = NULL;

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
                grub_printf("%s: %s\n", pChild->pcName, pChild->unData.pcStrVal);
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
            exist = ventoy_is_file_exist("%s%s", isodisk, value);
        }
        else
        {
            exist = ventoy_is_file_exist("%s/ventoy/%s", isodisk, value);
        }
        
        if (exist == 0)
        {
            grub_printf("Theme file %s does NOT exist\n", value);
            return 1;
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
    char filepath[256];
    VTOY_JSON *node;
    
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
        
        if (ventoy_is_file_exist(filepath) == 0)
        {
            debug("Theme file %s does not exist\n", filepath);
            return 0;
        }

        debug("vtoy_theme %s\n", filepath);
        grub_env_set("vtoy_theme", filepath);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "gfxmode");
    if (value)
    {
        debug("vtoy_gfxmode %s\n", value);
        grub_env_set("vtoy_gfxmode", value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "display_mode");
    if (value)
    {
        debug("display_mode %s\n", value);
        grub_env_set("vtoy_display_mode", value);
    }

    value = vtoy_json_get_string_ex(json->pstChild, "ventoy_left");
    if (value)
    {
        grub_env_set("VTLE_LFT", value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "ventoy_top");
    if (value)
    {
        grub_env_set("VTLE_TOP", value);
    }
    
    value = vtoy_json_get_string_ex(json->pstChild, "ventoy_color");
    if (value)
    {
        grub_env_set("VTLE_CLR", value);
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

    if (!ventoy_is_file_exist("%s%s", path, file))
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

        if ((node->unData.pcStrVal[0] != '/') || (!ventoy_is_file_exist("%s%s", isodisk, node->unData.pcStrVal)))
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
                if (ventoy_is_file_exist("%s%s", isodisk, child->unData.pcStrVal))
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
            if (0 == ventoy_plugin_check_path(isodisk, iso))
            {
                grub_printf("image: %s [OK]\n", iso);
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

static int ventoy_plugin_auto_install_entry(VTOY_JSON *json, const char *isodisk)
{
    int pathnum = 0;
    int autosel = 0;
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
        iso = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (iso && iso[0] == '/')
        {
            if (0 == ventoy_plugin_parse_fullpath(pNode->pstChild, isodisk, "template", &templatepath, &pathnum))
            {
                node = grub_zalloc(sizeof(install_template));
                if (node)
                {
                    node->pathlen = grub_snprintf(node->isopath, sizeof(node->isopath), "%s", iso);
                    node->templatepath = templatepath;
                    node->templatenum = pathnum;

                    node->autosel = -1;
                    if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "autosel", &autosel))
                    {
                        if (autosel >= 0 && autosel <= pathnum)
                        {
                            node->autosel = autosel;
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

static int ventoy_plugin_persistence_check(VTOY_JSON *json, const char *isodisk)
{
    int autosel = 0;
    int pathnum = 0;
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
            if (0 == ventoy_plugin_check_path(isodisk, iso))
            {
                grub_printf("image: %s [OK]\n", iso);
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
                    if (JSON_SUCCESS == vtoy_json_get_int(pNode->pstChild, "autosel", &autosel))
                    {
                        if (autosel >= 0 && autosel <= pathnum)
                        {
                            node->autosel = autosel;
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
                if (ventoy_is_file_exist("%s%s", isodisk, path))
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


static int ventoy_plugin_injection_check(VTOY_JSON *json, const char *isodisk)
{
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
        path = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (!path)
        {
            grub_printf("image not found\n");
            continue;
        }

        archive = vtoy_json_get_string_ex(pNode->pstChild, "archive");
        if (!archive)
        {
            grub_printf("archive not found\n");
            continue;
        }

        grub_printf("image: <%s> [%s]\n", path, ventoy_check_file_exist("%s%s", isodisk, path) ? "OK" : "NOT EXIST");
        grub_printf("archive: <%s> [%s]\n\n", archive, ventoy_check_file_exist("%s%s", isodisk, archive) ? "OK" : "NOT EXIST");
    }

    return 0;
}

static int ventoy_plugin_injection_entry(VTOY_JSON *json, const char *isodisk)
{
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
        path = vtoy_json_get_string_ex(pNode->pstChild, "image");
        archive = vtoy_json_get_string_ex(pNode->pstChild, "archive");
        if (path && path[0] == '/' && archive && archive[0] == '/')
        {
            node = grub_zalloc(sizeof(injection_config));
            if (node)
            {
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
        type = vtoy_class_image_file;
        key = vtoy_json_get_string_ex(pNode->pstChild, "key");
        if (!key)
        {
            key = vtoy_json_get_string_ex(pNode->pstChild, "dir");
            type = vtoy_class_directory;
        }
        
        class = vtoy_json_get_string_ex(pNode->pstChild, "class");
        if (key && class)
        {
            node = grub_zalloc(sizeof(menu_class));
            if (node)
            {
                node->type = type;
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
    int type;
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
        type = vtoy_class_image_file;
        key = vtoy_json_get_string_ex(pNode->pstChild, "key");
        if (!key)
        {
            key = vtoy_json_get_string_ex(pNode->pstChild, "dir"); 
            type = vtoy_class_directory;
        }
        
        class = vtoy_json_get_string_ex(pNode->pstChild, "class");
        if (key && class)
        {
            grub_printf("%s: <%s>\n", (type == vtoy_class_directory) ? "dir" : "key",  key);
            grub_printf("class: <%s>\n\n", class);
        }
    }

    return 0;
}

static plugin_entry g_plugin_entries[] = 
{
    { "control", ventoy_plugin_control_entry, ventoy_plugin_control_check },
    { "theme", ventoy_plugin_theme_entry, ventoy_plugin_theme_check },
    { "auto_install", ventoy_plugin_auto_install_entry, ventoy_plugin_auto_install_check },
    { "persistence", ventoy_plugin_persistence_entry, ventoy_plugin_persistence_check },
    { "menu_alias", ventoy_plugin_menualias_entry, ventoy_plugin_menualias_check },
    { "menu_class", ventoy_plugin_menuclass_entry, ventoy_plugin_menuclass_check },
    { "injection", ventoy_plugin_injection_entry, ventoy_plugin_injection_check },
};

static int ventoy_parse_plugin_config(VTOY_JSON *json, const char *isodisk)
{
    int i;
    VTOY_JSON *cur = json;

    grub_snprintf(g_iso_disk_name, sizeof(g_iso_disk_name), "%s", isodisk);

    while (cur)
    {
        for (i = 0; i < (int)ARRAY_SIZE(g_plugin_entries); i++)
        {
            if (grub_strcmp(g_plugin_entries[i].key, cur->pcName) == 0)
            {
                debug("Plugin entry for %s\n", g_plugin_entries[i].key);
                g_plugin_entries[i].entryfunc(cur, isodisk);
                break;
            }
        }
    
        cur = cur->pstNext;
    }

    return 0;
}

grub_err_t ventoy_cmd_load_plugin(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int ret = 0;
    char *buf = NULL;
    grub_file_t file;
    VTOY_JSON *json = NULL;
    
    (void)ctxt;
    (void)argc;

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/ventoy/ventoy.json", args[0]);
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

    

    ret = vtoy_json_parse(json, buf);
    if (ret)
    {
        debug("Failed to parse json string %d\n", ret);
        grub_free(buf);
        return 1;
    }

    ventoy_parse_plugin_config(json->pstChild, args[0]);

    vtoy_json_destroy(json);

    grub_free(buf);

    VENTOY_CMD_RETURN(GRUB_ERR_NONE);
}

void ventoy_plugin_dump_auto_install(void)
{
    int i;
    install_template *node = NULL;

    for (node = g_install_template_head; node; node = node->next)
    {
        grub_printf("\nIMAGE:<%s> <%d>\n", node->isopath, node->templatenum);
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
    install_template *node = NULL;
    int len = (int)grub_strlen(isopath);
    
    for (node = g_install_template_head; node; node = node->next)
    {
        if (node->pathlen == len && grub_strcmp(node->isopath, isopath) == 0)
        {
            return node;
        }
    }

    return NULL;
}

char * ventoy_plugin_get_cur_install_template(const char *isopath)
{
    install_template *node = NULL;

    node = ventoy_plugin_find_install_template(isopath);
    if ((!node) || (!node->templatepath))
    {
        return NULL;
    }

    if (node->cursel < 0 || node->cursel >= node->templatenum)
    {
        return NULL;
    }

    return node->templatepath[node->cursel].path;
}

persistence_config * ventoy_plugin_find_persistent(const char *isopath)
{
    persistence_config *node = NULL;
    int len = (int)grub_strlen(isopath);
    
    for (node = g_persistence_head; node; node = node->next)
    {
        if ((len == node->pathlen) && (grub_strcmp(node->isopath, isopath) == 0))
        {
            return node;
        }
    }

    return NULL;
}

int ventoy_plugin_get_persistent_chunklist(const char *isopath, int index, ventoy_img_chunk_list *chunk_list)
{
    int rc = 1;
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

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", g_iso_disk_name, node->backendpath[index].path);
    if (!file)
    {
        debug("Failed to open file %s%s\n", g_iso_disk_name, node->backendpath[index].path);
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
    injection_config *node = NULL;
    int len = (int)grub_strlen(isopath);

    for (node = g_injection_head; node; node = node->next)
    {
        if (node->pathlen == len && grub_strcmp(node->isopath, isopath) == 0)
        {
            return node->archive;
        }
    }

    return NULL;
}

const char * ventoy_plugin_get_menu_alias(int type, const char *isopath)
{
    menu_alias *node = NULL;
    int len = (int)grub_strlen(isopath);

    for (node = g_menu_alias_head; node; node = node->next)
    {
        if (node->type == type && node->pathlen && 
            node->pathlen == len && grub_strcmp(node->isopath, isopath) == 0)
        {
            return node->alias;
        }
    }

    return NULL;
}

const char * ventoy_plugin_get_menu_class(int type, const char *name)
{
    menu_class *node = NULL;
    int len = (int)grub_strlen(name);

    if (vtoy_class_image_file == type)
    {
        for (node = g_menu_class_head; node; node = node->next)
        {
            if (node->type == type && node->patlen <= len && grub_strstr(name, node->pattern))
            {
                return node->class;
            }
        }
    }
    else
    {
        for (node = g_menu_class_head; node; node = node->next)
        {
            if (node->type == type && node->patlen == len && grub_strncmp(name, node->pattern, len) == 0)
            {
                return node->class;
            }
        }
    }

    return NULL;
}

grub_err_t ventoy_cmd_plugin_check_json(grub_extcmd_context_t ctxt, int argc, char **args)
{
    int i = 0;
    int ret = 0;
    char *buf = NULL;
    grub_file_t file;
    VTOY_JSON *node = NULL;
    VTOY_JSON *json = NULL;
    
    (void)ctxt;

    if (argc != 3)
    {
        return 0;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s/ventoy/ventoy.json", args[0]);
    if (!file)
    {
        grub_printf("Plugin json file /ventoy/ventoy.json does NOT exist.\n");
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

