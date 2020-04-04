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

static int ventoy_plugin_theme_entry(VTOY_JSON *json, const char *isodisk)
{
    const char *value;
    char filepath[256];
    
    value = vtoy_json_get_string_ex(json->pstChild, "file");
    if (value)
    {
        grub_snprintf(filepath, sizeof(filepath), "%s/ventoy/%s", isodisk, value);
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

    return 0;
}

static plugin_entry g_plugin_entries[] = 
{
    { "theme", ventoy_plugin_theme_entry },
};

static int ventoy_parse_plugin_config(VTOY_JSON *json, const char *isodisk)
{
    int i;
    VTOY_JSON *cur = json;

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

