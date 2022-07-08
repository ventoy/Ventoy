/******************************************************************************
 * vtoyexpand.c  ---- ventoy auto install script variable expansion
 *
 * Copyright (c) 2022, longpanda <admin@ventoy.net>
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
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/fs.h>
#include <dirent.h>
#include "vtoytool.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define TMP_FILE    "/ventoy/tmp_var_expansion"

#define SIZE_1MB    (1024 * 1024)
#define ulong unsigned long
#define ulonglong  unsigned long long

typedef struct disk_info
{
    char name[128];
    ulonglong size;
    int isUSB;
    int isSDX;
}disk_info;

static disk_info *g_disk_list = NULL;
static int g_disk_num = 0;
static const char *g_vtoy_disk_name = NULL;

static void vlog(const char *fmt, ...)
{
    int n = 0;
    va_list arg;
    FILE *fp = NULL;
    char log[1024];

    fp = fopen("/ventoy/autoinstall.log", "a+");
    if (fp)
    {
        va_start(arg, fmt);
        n += vsnprintf(log, sizeof(log) - 1, fmt, arg);
        va_end(arg);

        fwrite(log, 1, n, fp);        
        fclose(fp);
    }
}

static int copy_file(const char *file1, const char *file2)
{
    int n;
    int size;
    int ret = 1;
    FILE *fp1 = NULL;
    FILE *fp2 = NULL;
    char *buf = NULL;

    fp1 = fopen(file1, "rb");
    if (!fp1)
    {
        vlog("Failed to read file <%s>\n", file1);
        goto end;
    }
    
    fp2 = fopen(file2, "wb+");
    if (!fp2)
    {
        vlog("Failed to create file <%s>\n", file2);
        goto end;
    }

    fseek(fp1, 0, SEEK_END);
    size = (int)ftell(fp1);
    fseek(fp1, 0, SEEK_SET);

    buf = malloc(size);
    if (!buf)
    {
        vlog("Failed to malloc buf\n");
        goto end;
    }

    n = fread(buf, 1, size, fp1);
    if (n != size)
    {
        vlog("Failed to read <%s> %d %d\n", file1, n, size);
        goto end;
    }
    
    n = fwrite(buf, 1, size, fp2);
    if (n != size)
    {
        vlog("Failed to write <%s> %d %d\n", file2, n, size);
        goto end;
    }

    ret = 0;

end:

    if (fp1)
        fclose(fp1);
    if (fp2)
        fclose(fp2);
    if (buf)
        free(buf);

    return ret;
}

static int vtoy_is_possible_blkdev(const char *name)
{
    if (name[0] == '.')
    {
        return 0;
    }

    /* /dev/ramX */
    if (name[0] == 'r' && name[1] == 'a' && name[2] == 'm')
    {
        return 0;
    }

    /* /dev/loopX */
    if (name[0] == 'l' && name[1] == 'o' && name[2] == 'o' && name[3] == 'p')
    {
        return 0;
    }

    /* /dev/dm-X */
    if (name[0] == 'd' && name[1] == 'm' && name[2] == '-' && IS_DIGIT(name[3]))
    {
        return 0;
    }

    /* /dev/srX */
    if (name[0] == 's' && name[1] == 'r' && (name[2] >= '0' && name[2] <= '9'))
    {
        return 0;
    }
    
    return 1;
}

static ulonglong vtoy_get_disk_size_in_byte(const char *disk)
{
    int fd;
    int rc;
    unsigned long long size = 0;
    char diskpath[256] = {0};
    char sizebuf[64] = {0};

    // Try 1: get size from sysfs
    snprintf(diskpath, sizeof(diskpath) - 1, "/sys/block/%s/size", disk);
    if (access(diskpath, F_OK) >= 0)
    {
        vlog("get disk size from sysfs for %s\n", disk);
        
        fd = open(diskpath, O_RDONLY | O_BINARY);
        if (fd >= 0)
        {
            read(fd, sizebuf, sizeof(sizebuf));
            size = strtoull(sizebuf, NULL, 10);
            close(fd);
            return (size * 512);
        }
    }
    else
    {
        vlog("%s not exist \n", diskpath);
    }

    // Try 2: get size from ioctl
    snprintf(diskpath, sizeof(diskpath) - 1, "/dev/%s", disk);
    fd = open(diskpath, O_RDONLY);
    if (fd >= 0)
    {
        vlog("get disk size from ioctl for %s\n", disk);
        rc = ioctl(fd, BLKGETSIZE64, &size);
        if (rc == -1)
        {
            size = 0;
            vlog("failed to ioctl %d\n", rc);
        }
        close(fd);
    }
    else
    {
        vlog("failed to open %s %d\n", diskpath, errno);
    }

    vlog("disk %s size %llu bytes\n", disk, (ulonglong)size);
    return size;
}

static int get_disk_num(void)
{
    int n = 0;
    DIR* dir = NULL;
    struct dirent* p = NULL;

    dir = opendir("/sys/block");
    if (!dir)
    {
        return 0;
    }
    
    while ((p = readdir(dir)) != NULL)
    {
        n++;
    }
    closedir(dir);

    return n;
}

static int is_usb_disk(const char *diskname)
{
    int rc;
    char dstpath[1024] = { 0 };
    char syspath[1024] = { 0 };
    
    snprintf(syspath, sizeof(syspath), "/sys/block/%s", diskname);
    rc = readlink(syspath, dstpath, sizeof(dstpath) - 1);
    if (rc > 0 && strstr(dstpath, "/usb"))
    {
        return 1;
    }

    return 0;
}

static int get_all_disk(void)
{
    int i = 0;
    int j = 0;
    int num = 0;
    ulonglong cursize = 0;
    DIR* dir = NULL;
    struct dirent* p = NULL;
    disk_info *node = NULL;
    disk_info tmpnode;

    num = get_disk_num();
    if (num <= 0)
    {
        return 1;
    }

    g_disk_list = malloc(num * sizeof(disk_info));
    if (!g_disk_list)
    {
        return 1;
    }
    memset(g_disk_list, 0, num * sizeof(disk_info));

    dir = opendir("/sys/block");
    if (!dir)
    {
        return 0;
    }

    while (((p = readdir(dir)) != NULL) && g_disk_num < num)
    {
        if (!vtoy_is_possible_blkdev(p->d_name))
        {
            vlog("disk %s is filted by name\n", p->d_name);
            continue;
        }
    
        cursize = vtoy_get_disk_size_in_byte(p->d_name);

        node = g_disk_list + g_disk_num;
        g_disk_num++;

        
        snprintf(node->name, sizeof(node->name), p->d_name);
        node->size = cursize;
        node->isUSB = is_usb_disk(p->d_name);
        if (strncmp(node->name, "sd", 2) == 0)
        {
            node->isSDX = 1;
        }
    }
    closedir(dir);

    /* sort disks */
    for (i = 0; i < g_disk_num; i++)
    {
        for (j = i + 1; j < g_disk_num; j++)
        {
            if (g_disk_list[i].isSDX && g_disk_list[j].isSDX)
            {
                if (strcmp(g_disk_list[i].name, g_disk_list[j].name) > 0)
                {
                    memcpy(&tmpnode, g_disk_list + i, sizeof(tmpnode));
                    memcpy(g_disk_list + i, g_disk_list + j, sizeof(tmpnode));
                    memcpy(g_disk_list + j, &tmpnode, sizeof(tmpnode));
                }
            }
        }
    }

    vlog("============ DISK DUMP BEGIN ===========\n");
    for (i = 0; i < g_disk_num; i++)
    {
        node = g_disk_list + i;
        vlog("[%d] %s %dGB(%llu) USB:%d\n", i, node->name,
            node->size / 1024 / 1024 / 1024, node->size, node->isUSB);
    }
    vlog("============ DISK DUMP END ===========\n");
    
    return 0;
}

static int expand_var(const char *var, char *value, int len)
{
    int i;
    int index = -1;
    ulonglong uiDst = 0;
    ulonglong delta = 0;
    ulonglong maxsize = 0;
    ulonglong maxdelta = 0xFFFFFFFFFFFFFFFFULL;
    disk_info *node = NULL;
    value[0] = 0;

    if (strcmp(var, "VT_LINUX_DISK_SDX_1ST_NONVTOY") == 0)
    {
        for (i = 0; i < g_disk_num; i++)
        {
            node = g_disk_list + i;
            if (node->size > 0 && node->isSDX && strcmp(node->name, g_vtoy_disk_name) != 0)
            {
                vlog("%s=<%s>\n", var, node->name);
                snprintf(value, len, "%s", node->name);
                return 0;
            }
        }

        vlog("[Error] %s not found\n", var);
    }
    else if (strcmp(var, "VT_LINUX_DISK_SDX_1ST_NONUSB") == 0)
    {
        for (i = 0; i < g_disk_num; i++)
        {
            node = g_disk_list + i;
            if (node->size > 0 && node->isSDX && node->isUSB == 0)
            {
                vlog("%s=<%s>\n", var, node->name);
                snprintf(value, len, "%s", node->name);
                return 0;
            }
        }

        vlog("[Error] %s not found\n", var);
    }
    else if (strcmp(var, "VT_LINUX_DISK_MAX_SIZE") == 0)
    {
        for (i = 0; i < g_disk_num; i++)
        {
            node = g_disk_list + i;
            if (node->size > 0 && node->size > maxsize)
            {
                index = i;
                maxsize = node->size;
            }
        }

        if (index >= 0)
        {
            vlog("%s=<%s>\n", var, g_disk_list[index].name);
            snprintf(value, len, "%s", g_disk_list[index].name);
            return 0;
        }
        else
        {
            vlog("[Error] %s not found\n", var);
        }
    }
    else if (strncmp(var, "VT_LINUX_DISK_CLOSEST_", 22) == 0)
    {
        uiDst = strtoul(var + 22, NULL, 10);
        uiDst = uiDst * (1024ULL * 1024ULL * 1024ULL);
    
        for (i = 0; i < g_disk_num; i++)
        {
            node = g_disk_list + i;
            if (node->size == 0)
            {
                continue;
            }
        
            if (node->size > uiDst)
            {
                delta = node->size - uiDst;
            }
            else
            {
                delta = uiDst - node->size;
            }
            
            if (delta < maxdelta)
            {
                index = i;
                maxdelta = delta;
            }
        }

        if (index >= 0)
        {
            vlog("%s=<%s>\n", var, g_disk_list[index].name);
            snprintf(value, len, "%s", g_disk_list[index].name);
            return 0;
        }
        else
        {
            vlog("[Error] %s not found\n", var);
        }
    }
    else
    {
        vlog("Invalid var name <%s>\n", var);
        snprintf(value, len, "$$%s$$", var);
    }

    if (value[0] == 0)
    {
        snprintf(value, len, "$$%s$$", var);
    }

    return 0;
}

int vtoyexpand_main(int argc, char **argv)
{
    FILE *fp = NULL;
    FILE *fout = NULL;
    char *start = NULL;
    char *end = NULL;
    char line[4096];
    char value[256];

    vlog("========= vtoyexpand_main %d ========\n", argc);

    if (argc != 3)
    {
        return 1;
    }

    g_vtoy_disk_name = argv[2];
    if (strncmp(g_vtoy_disk_name, "/dev/", 5) == 0)
    {
        g_vtoy_disk_name += 5;
    }
    vlog("<%s> <%s> <%s>\n", argv[1], argv[2], g_vtoy_disk_name);    

    get_all_disk();
    
    fp = fopen(argv[1], "r");
    if (!fp)
    {
        vlog("Failed to open file <%s>\n", argv[1]);
        return 1;
    }

    fout = fopen(TMP_FILE, "w+");
    if (!fout)
    {
        vlog("Failed to create file <%s>\n", TMP_FILE);
        fclose(fp);
        return 1;
    }

    memset(line, 0, sizeof(line));
    memset(value, 0, sizeof(value));
    
    while (fgets(line, sizeof(line), fp))
    {
        start = strstr(line, "$$VT_");
        if (start)
        {
            end = strstr(start + 5, "$$");
        }

        if (start && end)
        {
            *start = 0;
            fprintf(fout, "%s", line);

            *end = 0;
            expand_var(start + 2, value, sizeof(value));
            fprintf(fout, "%s", value);
            
            fprintf(fout, "%s", end + 2);

            memset(value, 0, sizeof(value));
        }
        else
        {
            fprintf(fout, "%s", line);
        }

        line[0] = line[4095] = 0;
    }
    
    fclose(fp);
    fclose(fout);

    vlog("delete file <%s>\n", argv[1]);
    remove(argv[1]);

    vlog("Copy file <%s> --> <%s>\n", TMP_FILE, argv[1]);
    copy_file(TMP_FILE, argv[1]);
    
    return 0;
}

// wrapper main
#ifndef BUILD_VTOY_TOOL
int main(int argc, char **argv)
{
    return vtoyexpand_main(argc, argv);
}
#endif

