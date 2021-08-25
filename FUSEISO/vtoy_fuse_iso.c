/******************************************************************************
 * vtoy_fuse_iso.c
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

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

typedef unsigned int uint32_t;

typedef struct dmtable_entry
{
    uint32_t isoSector;
    uint32_t sectorNum;
    unsigned long long diskSector;
}dmtable_entry;

#define MAX_ENTRY_NUM  (1024 * 1024 / sizeof(dmtable_entry))

static int verbose = 0;
#define debug(fmt, ...) if(verbose) printf(fmt, ##__VA_ARGS__)

static int g_disk_fd = -1;
static uint64_t g_iso_file_size;
static char g_mnt_point[512];
static char g_iso_file_name[512];
static dmtable_entry *g_disk_entry_list = NULL;
static int g_disk_entry_num = 0;

static int ventoy_iso_getattr(const char *path, struct stat *statinfo)
{
    int ret = -ENOENT;

    if (path && statinfo)
    {
        memset(statinfo, 0, sizeof(struct stat));

        if (path[0] == '/' && path[1] == 0)
        {
            statinfo->st_mode  = S_IFDIR | 0755;
            statinfo->st_nlink = 2;
            ret = 0;
        }
        else if (strcmp(path, g_iso_file_name) == 0)
        {
            statinfo->st_mode  = S_IFREG | 0444;
            statinfo->st_nlink = 1;
            statinfo->st_size  = g_iso_file_size;
            ret = 0;
        }
    }
    
    return ret;
}

static int ventoy_iso_readdir
(
    const char *path, 
    void *buf, 
    fuse_fill_dir_t filler,
    off_t offset, 
    struct fuse_file_info *file
)
{
    (void)offset;
    (void)file;

    if (path[0] != '/' || path[1] != 0)
    {
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, g_iso_file_name + 1, NULL, 0);

    return 0;
}

static int ventoy_iso_open(const char *path, struct fuse_file_info *file)
{
    if (strcmp(path, g_iso_file_name) != 0)
    {
        return -ENOENT;
    }

    if ((file->flags & 3) != O_RDONLY)
    {
        return -EACCES;
    }

    return 0;
}

static int ventoy_read_iso_sector(uint32_t sector, uint32_t num, char *buf)
{
    uint32_t i = 0;
    uint32_t leftSec = 0;
    uint32_t readSec = 0;
    off_t offset = 0;
    dmtable_entry *entry = NULL;
    
    for (i = 0; i < g_disk_entry_num && num > 0; i++)
    {
        entry = g_disk_entry_list + i;

        if (sector >= entry->isoSector && sector < entry->isoSector + entry->sectorNum)
        {
            offset = (entry->diskSector + (sector - entry->isoSector)) * 512;

            leftSec = entry->sectorNum - (sector - entry->isoSector);
            readSec = (leftSec > num) ? num : leftSec;

            pread(g_disk_fd, buf, readSec * 512, offset);

            sector += readSec;
            buf += readSec * 512;
            num -= readSec;
        }
    }

    return 0;
}

static int ventoy_iso_read
(
    const char *path, char *buf, 
    size_t size, off_t offset,
    struct fuse_file_info *file
)
{
    uint32_t mod = 0;
    uint32_t align = 0;
    uint32_t sector = 0;
    uint32_t number = 0;
    size_t leftsize = 0;
    char secbuf[512];
    
    (void)file;
    
    if(strcmp(path, g_iso_file_name) != 0)
    {
        return -ENOENT;        
    }

    if (offset >= g_iso_file_size)
    {
        return 0;
    }

    if (offset + size > g_iso_file_size)
    {
        size = g_iso_file_size - offset;
    }
    
    leftsize = size;
    sector = offset / 512;

    mod = offset % 512;
    if (mod > 0)
    {
        align = 512 - mod;
        ventoy_read_iso_sector(sector, 1, secbuf);

        if (leftsize > align)
        {
            memcpy(buf, secbuf + mod, align);
            buf += align;
            offset += align;
            sector++;
            leftsize -= align;
        }
        else
        {
            memcpy(buf, secbuf + mod, leftsize);
            return size;
        }
    }

    number = leftsize / 512;
    ventoy_read_iso_sector(sector, number, buf);
    buf += number * 512;

    mod = leftsize % 512;
    if (mod > 0)
    {
        ventoy_read_iso_sector(sector + number, 1, secbuf);
        memcpy(buf, secbuf, mod);
    }

    return size;
}

static struct fuse_operations ventoy_op = 
{
    .getattr    = ventoy_iso_getattr,
    .readdir    = ventoy_iso_readdir,
    .open       = ventoy_iso_open,
    .read       = ventoy_iso_read,
};

static int ventoy_parse_dmtable(const char *filename)
{
    FILE *fp = NULL;
    char diskname[128] = {0};
    char line[256] = {0};
    dmtable_entry *entry= g_disk_entry_list;

    fp = fopen(filename, "r");
    if (NULL == fp)
    {
        printf("Failed to open file %s\n", filename);
        return 1;
    }

    /* read untill the last line */
    while (fgets(line, sizeof(line), fp) && g_disk_entry_num < MAX_ENTRY_NUM)
    {
        sscanf(line, "%u %u linear %s %llu", 
               &entry->isoSector, &entry->sectorNum, 
               diskname, &entry->diskSector);

        g_iso_file_size += (uint64_t)entry->sectorNum * 512ULL;
        g_disk_entry_num++;
        entry++;
    }
    fclose(fp);

    if (g_disk_entry_num >= MAX_ENTRY_NUM)
    {
        fprintf(stderr, "ISO file has too many fragments ( more than %u )\n", MAX_ENTRY_NUM);
        return 1;
    }

    debug("iso file size: %llu disk name %s\n", g_iso_file_size, diskname);

    g_disk_fd = open(diskname, O_RDONLY);
    if (g_disk_fd < 0)
    {
        debug("Failed to open %s\n", diskname);
        return 1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    int rc;
    int ch;
    char filename[512] = {0};

    /* Avoid to be killed by systemd */
    if (access("/etc/initrd-release", F_OK) >= 0)
    {		
        argv[0][0] = '@';
    }

    g_iso_file_name[0] = '/';
    
    while ((ch = getopt(argc, argv, "f:s:m:v::t::")) != -1)
    {
        if (ch == 'f')
        {
            strncpy(filename, optarg, sizeof(filename) - 1);
        }
        else if (ch == 'm')
        {
            strncpy(g_mnt_point, optarg, sizeof(g_mnt_point) - 1);
        }
        else if (ch == 's')
        {
            strncpy(g_iso_file_name + 1, optarg, sizeof(g_iso_file_name) - 2);
        }
        else if (ch == 'v')
        {
            verbose = 1;
        }
        else if (ch == 't') // for test
        {
            return 0;
        }
    }

    if (filename[0] == 0)
    {
        fprintf(stderr, "Must input dmsetup table file with -f\n");
        return 1;
    }

    if (g_mnt_point[0] == 0)
    {
        fprintf(stderr, "Must input mount point with -m\n");
        return 1;
    }

    if (g_iso_file_name[1] == 0)
    {
        strncpy(g_iso_file_name + 1, "ventoy.iso", sizeof(g_iso_file_name) - 2);
    }

    debug("ventoy fuse iso: %s %s %s\n", filename, g_iso_file_name, g_mnt_point);

    g_disk_entry_list = malloc(MAX_ENTRY_NUM * sizeof(dmtable_entry));
    if (NULL == g_disk_entry_list)
    {
        return 1;
    }

    rc = ventoy_parse_dmtable(filename);
    if (rc)
    {
        free(g_disk_entry_list);
        return rc;
    }

    argv[1] = g_mnt_point;
    argv[2] = NULL;
    rc = fuse_main(2, argv, &ventoy_op, NULL);

    close(g_disk_fd);

    free(g_disk_entry_list);
    return rc;
}

