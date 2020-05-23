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
#include <grub/ventoy.h>
#include "ventoy_def.h"

GRUB_MOD_LICENSE ("GPLv3+");

static char g_iso_disk_name[128];
static install_template *g_install_template_head = NULL;
static persistence_config *g_persistence_head = NULL;

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

static int ventoy_plugin_theme_entry(VTOY_JSON *json, const char *isodisk)
{
    const char *value;
    char filepath[256];
    
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

    return 0;
}


static int ventoy_plugin_auto_install_entry(VTOY_JSON *json, const char *isodisk)
{
    const char *iso = NULL;
    const char *script = NULL;
    VTOY_JSON *pNode = NULL;
    install_template *node = NULL;
    install_template *next = NULL;

    (void)isodisk;

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
            grub_free(node);
        }

        g_install_template_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        iso = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (iso && iso[0] == '/')
        {
            script = vtoy_json_get_string_ex(pNode->pstChild, "template");
            if (script && script[0] == '/')
            {
                node = grub_zalloc(sizeof(install_template));
                if (node)
                {
                    grub_snprintf(node->isopath, sizeof(node->isopath), "%s", iso);
                    grub_snprintf(node->templatepath, sizeof(node->templatepath), "%s", script);

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


static int ventoy_plugin_persistence_entry(VTOY_JSON *json, const char *isodisk)
{
    const char *iso = NULL;
    const char *persist = NULL;
    VTOY_JSON *pNode = NULL;
    persistence_config *node = NULL;
    persistence_config *next = NULL;

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
            grub_free(node);
        }

        g_persistence_head = NULL;
    }

    for (pNode = json->pstChild; pNode; pNode = pNode->pstNext)
    {
        iso = vtoy_json_get_string_ex(pNode->pstChild, "image");
        if (iso && iso[0] == '/')
        {
            persist = vtoy_json_get_string_ex(pNode->pstChild, "backend");
            if (persist && persist[0] == '/')
            {
                node = grub_zalloc(sizeof(persistence_config));
                if (node)
                {
                    grub_snprintf(node->isopath, sizeof(node->isopath), "%s", iso);
                    grub_snprintf(node->filepath, sizeof(node->filepath), "%s", persist);

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


static plugin_entry g_plugin_entries[] = 
{
    { "control", ventoy_plugin_control_entry },
    { "theme", ventoy_plugin_theme_entry },
    { "auto_install", ventoy_plugin_auto_install_entry },
    { "persistence", ventoy_plugin_persistence_entry },
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
    install_template *node = NULL;

    for (node = g_install_template_head; node; node = node->next)
    {
        grub_printf("IMAGE:<%s>\n", node->isopath);
        grub_printf("SCRIPT:<%s>\n\n", node->templatepath);
    }

    return;
}

void ventoy_plugin_dump_persistence(void)
{
    int rc;
    persistence_config *node = NULL;
    ventoy_img_chunk_list chunk_list;
    
    for (node = g_persistence_head; node; node = node->next)
    {
        grub_printf("IMAGE:<%s>\n", node->isopath);
        grub_printf("PERSIST:<%s>", node->filepath);

        rc = ventoy_plugin_get_persistent_chunklist(node->isopath, &chunk_list);
        if (rc == 0)
        {
            grub_printf(" [ SUCCESS ]\n\n");
            grub_free(chunk_list.chunk);
        }
        else
        {
            grub_printf(" [ FAILED ]\n\n");
        }
    }

    return;
}


char * ventoy_plugin_get_install_template(const char *isopath)
{
    install_template *node = NULL;

    for (node = g_install_template_head; node; node = node->next)
    {
        if (grub_strcmp(node->isopath, isopath) == 0)
        {
            return node->templatepath;
        }
    }

    return NULL;
}

int ventoy_plugin_get_persistent_chunklist(const char *isopath, ventoy_img_chunk_list *chunk_list)
{
    int rc = 1;
    grub_uint64_t start = 0;
    grub_file_t file = NULL;
    persistence_config *node = NULL;

    for (node = g_persistence_head; node; node = node->next)
    {
        if (grub_strcmp(node->isopath, isopath) == 0)
        {
            break;
        }
    }

    if (NULL == node)
    {
        goto end;
    }

    file = ventoy_grub_file_open(VENTOY_FILE_TYPE, "%s%s", g_iso_disk_name, node->filepath);
    if (!file)
    {
        debug("Failed to open file %s%s\n", g_iso_disk_name, node->filepath);
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

