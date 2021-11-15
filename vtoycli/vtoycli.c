/******************************************************************************
 * vtoycli.c
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

#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "vtoycli.h"

void ventoy_gen_preudo_uuid(void *uuid)
{
    int i;
    int fd;

    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
    {
        srand(time(NULL));
        for (i = 0; i < 8; i++)
        {
            *((uint16_t *)uuid + i) = (uint16_t)(rand() & 0xFFFF);
        }
    }
    else
    {
        read(fd, uuid, 16);
        close(fd);
    }
}

UINT64 get_disk_size_in_byte(const char *disk)
{
    int fd;
    int rc;
    const char *pos = disk;
    unsigned long long size = 0;
    char diskpath[256] = {0};
    char sizebuf[64] = {0};

    if (strncmp(disk, "/dev/", 5) == 0)
    {
        pos = disk + 5;
    }

    // Try 1: get size from sysfs
    snprintf(diskpath, sizeof(diskpath) - 1, "/sys/block/%s/size", pos);
    if (access(diskpath, F_OK) >= 0)
    {
        fd = open(diskpath, O_RDONLY);
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
        printf("%s not exist \n", diskpath);
    }

    printf("disk %s size %llu bytes\n", disk, (unsigned long long)size);
    return size;
}


int main(int argc, char **argv)
{
    if (argc < 2)
    {
        return 1;
    }
    else if (strcmp(argv[1], "fat") == 0)
    {
        return vtoyfat_main(argc - 1, argv + 1);
    }
    else if (strcmp(argv[1], "gpt") == 0)
    {
        return vtoygpt_main(argc - 1, argv + 1);
    }
    else if (strcmp(argv[1], "partresize") == 0)
    {
        return partresize_main(argc - 1, argv + 1);
    }
    else
    {
        return 1;
    }
}

