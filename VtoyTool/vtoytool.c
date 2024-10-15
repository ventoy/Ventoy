/******************************************************************************
 * vtoytool.c  ---- ventoy os tool
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef int (*main_func)(int argc, char **argv);

typedef struct cmd_def
{
    const char *cmd;
    main_func func;
}cmd_def;

int vtoydump_main(int argc, char **argv);
int vtoydm_main(int argc, char **argv);
int vtoytool_install(int argc, char **argv);
int vtoyloader_main(int argc, char **argv);
int vtoyvine_main(int argc, char **argv);
int vtoyksym_main(int argc, char **argv);
int vtoykmod_main(int argc, char **argv);
int vtoyexpand_main(int argc, char **argv);

static char *g_vtoytool_name = NULL;
static cmd_def g_cmd_list[] = 
{
    { "vine_patch_loader",  vtoyvine_main  },
    { "vtoydump",    vtoydump_main    },
    { "vtoydm",      vtoydm_main      },
    { "loader",      vtoyloader_main  },
    { "hald",        vtoyloader_main  },
    { "vtoyksym",    vtoyksym_main  },
    { "vtoykmod",    vtoykmod_main  },
    { "vtoyexpand",  vtoyexpand_main  },
    { "--install",   vtoytool_install },
};


int vtoytool_install(int argc, char **argv)
{
    int i;
    char toolpath[128];
    char filepath[128];
    
    for (i = 0; i < sizeof(g_cmd_list) / sizeof(g_cmd_list[0]); i++)
    {
        if (g_cmd_list[i].cmd[0] != '-')
        {
            snprintf(toolpath, sizeof(toolpath), "/ventoy/tool/%s", g_vtoytool_name);
            snprintf(filepath, sizeof(filepath), "/ventoy/tool/%s", g_cmd_list[i].cmd);
            link(toolpath, filepath);
        }
    }
    
    return 0;
}

int main(int argc, char **argv)
{
    int i;
    
    if ((g_vtoytool_name = strstr(argv[0], "vtoytool")) != NULL)
    {
        argc--;
        argv++;
    }

    if (argc == 0)
    {
        fprintf(stderr, "Invalid param number\n");
        return 1;
    }

    for (i = 0; i < sizeof(g_cmd_list) / sizeof(g_cmd_list[0]); i++)
    {
        if (strstr(argv[0], g_cmd_list[i].cmd))
        {
            return g_cmd_list[i].func(argc, argv);
        }
    }

    fprintf(stderr, "Invalid cmd %s\n", argv[0]);
    return 1;
}

