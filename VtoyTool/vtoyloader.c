/******************************************************************************
 * vtoyloader.c  ---- ventoy loader (wapper for binary loader)
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
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_EXT_PARAM       256
#define CMDLINE_BUF_LEN     (1024 * 1024 * 1)
#define EXEC_PATH_FILE      "/ventoy/loader_exec_file"
#define CMDLINE_FILE        "/ventoy/loader_exec_cmdline"
#define HOOK_CMD_FILE       "/ventoy/loader_hook_cmd"
#define DEBUG_FLAG_FILE     "/ventoy/loader_debug"

static int verbose = 0;
#define debug(fmt, ...) if(verbose) printf(fmt, ##__VA_ARGS__)

static char g_exec_file[512];
static char g_hook_cmd[512];

static int vtoy_read_file_to_buf(const char *file, void *buf, int buflen)
{
    FILE *fp;

    fp = fopen(file, "r");
    if (!fp)
    {
        fprintf(stderr, "Failed to open file %s err:%d\n", file, errno);
        return 1;
    }
    fread(buf, 1, buflen, fp);
    fclose(fp);

    return 0;
}

int vtoyloader_main(int argc, char **argv)
{
    int i;
    int len;
    int rc;
    char *pos;
    char *cmdline;
    char **cmdlist;

    if (access(DEBUG_FLAG_FILE, F_OK) >= 0)
    {
        verbose = 1;
    }

    debug("ventoy loader ...\n");
    
    rc = vtoy_read_file_to_buf(EXEC_PATH_FILE, g_exec_file, sizeof(g_exec_file) - 1);
    if (rc)
    {
        return rc;
    }

    if (access(g_exec_file, F_OK) < 0)
    {
        fprintf(stderr, "File %s not exist\n", g_exec_file);
        return 1;
    }

    if (access(HOOK_CMD_FILE, F_OK) >= 0)
    {
        rc = vtoy_read_file_to_buf(HOOK_CMD_FILE,  g_hook_cmd,  sizeof(g_hook_cmd) - 1);
        debug("g_hook_cmd=<%s>\n", g_hook_cmd);
    }

    cmdline = (char *)malloc(CMDLINE_BUF_LEN);
    if (!cmdline)
    {
        fprintf(stderr, "Failed to alloc memory err:%d\n", errno);
        return 1;
    }
    memset(cmdline, 0, CMDLINE_BUF_LEN);

    if (access(CMDLINE_FILE, F_OK) >= 0)
    {
        rc = vtoy_read_file_to_buf(CMDLINE_FILE, cmdline, CMDLINE_BUF_LEN - 1);
        if (rc)
        {
            return rc;
        }
    }

    len = (int)((argc + MAX_EXT_PARAM) * sizeof(char *));
    cmdlist = (char **)malloc(len);
    if (!cmdlist)
    {
        free(cmdline);
        fprintf(stderr, "Failed to alloc memory err:%d\n", errno);
        return 1;
    }
    memset(cmdlist, 0, len);

    for (i = 0; i < argc; i++)
    {
        cmdlist[i] = argv[i];
    }
    
    cmdlist[0] = g_exec_file;
    debug("g_exec_file=<%s>\n", g_exec_file);

    pos = cmdline;
    while ((*pos) && i < MAX_EXT_PARAM)
    {
        cmdlist[i++] = pos;

        while (*pos)
        {
            if (*pos == '\r')
            {
                *pos = ' ';
            }
            else if (*pos == '\n')
            {
                *pos = 0;
                pos++;
                break;
            }
        
            pos++;
        }
    }

    debug("execv [%s]...\n", cmdlist[0]);

    // call hook script
    if (g_hook_cmd[0])
    {
        rc = system(g_hook_cmd);
        debug("system return code =<%d>  errno=<%d>\n", rc, errno);        
    }

    execv(cmdlist[0], cmdlist);
    
    return 0;
}


// wrapper main
#ifndef BUILD_VTOY_TOOL
int main(int argc, char **argv)
{
    return vtoyloader_main(argc, argv);
}
#endif

