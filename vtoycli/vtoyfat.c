/******************************************************************************
 * vtoyfat.c  ---- Parse fat file system
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h> 
#include <fcntl.h>

#include <fat_filelib.h>

static int g_disk_fd = 0;

static int vtoy_disk_read(uint32 sector, uint8 *buffer, uint32 sector_count)
{
    lseek(g_disk_fd, sector * 512, SEEK_SET);
    read(g_disk_fd, buffer, sector_count * 512);
    
    return 1;
}

static int check_secure_boot(void)
{
    void *flfile = NULL;
    
    flfile = fl_fopen("/EFI/BOOT/grubx64_real.efi", "rb");
    if (flfile)
    {
        fl_fclose(flfile);
        return 0;
    }
    
    return 1;
}

static int get_ventoy_version(void)
{
    int rc = 1;
    int size = 0;
    char *buf = NULL;
    char *pos = NULL;
    char *end = NULL;
    void *flfile = NULL;

    flfile = fl_fopen("/grub/grub.cfg", "rb");
    if (flfile)
    {
        fl_fseek(flfile, 0, SEEK_END);
        size = (int)fl_ftell(flfile);

        fl_fseek(flfile, 0, SEEK_SET);

        buf = malloc(size + 1);
        if (buf)
        {
            fl_fread(buf, 1, size, flfile);
            buf[size] = 0;

            pos = strstr(buf, "VENTOY_VERSION=");
            if (pos)
            {
                pos += strlen("VENTOY_VERSION=");
                if (*pos == '"')
                {
                    pos++;
                }

                end = pos;
                while (*end != 0 && *end != '"' && *end != '\r' && *end != '\n')
                {
                    end++;
                }

                *end = 0;

                printf("%s\n", pos);
                rc = 0;
            }
            free(buf);
        }

        fl_fclose(flfile);
    }

    return rc;
}

int vtoyfat_main(int argc, char **argv)
{
    int op = 0;
    int rc = 1;
    char *disk;

    if (argc != 2 && argc != 3)
    {   
        printf("Usage: vtoyfat /dev/sdbs \n");
        printf("Usage: vtoyfat -s /dev/sdbs \n");
        return 1;
    }
    
    if (argv[1][0] == '-' && argv[1][1] == 'T')
    {
        return 0;
    }
    
    disk = argv[1];
    if (argv[1][0] == '-' && argv[1][1] == 's')
    {
        op = 1;
        disk = argv[2];
    } 
    
    g_disk_fd = open(disk, O_RDONLY);
    if (g_disk_fd < 0)
    {
        printf("Failed to open %s\n", disk);
        return 1;
    }

    fl_init();

    if (0 == fl_attach_media(vtoy_disk_read, NULL))
    {
        if (op == 0)
        {
            rc = get_ventoy_version();
        }
        else
        {
            rc = check_secure_boot();
        }        
    }

    fl_shutdown();

    close(g_disk_fd);

    return rc;
}

