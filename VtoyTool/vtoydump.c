/******************************************************************************
 * vtoydump.c  ---- Dump ventoy os parameters 
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
#include <ctype.h>
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
#if defined(_dragon_fly) || defined(_free_BSD) || defined(_QNX)
#define MMAP_FLAGS          MAP_SHARED
#else
#define MMAP_FLAGS          MAP_PRIVATE
#endif

#define SEARCH_MEM_START 0x80000
#define SEARCH_MEM_LEN   0x1c000

static int verbose = 0;
#define debug(fmt, ...) if(verbose) printf(fmt, ##__VA_ARGS__)

static ventoy_guid vtoy_guid = VENTOY_GUID;

static const char *g_ventoy_fs[ventoy_fs_max] = 
{
    "exfat", "ntfs", "ext*", "xfs", "udf", "fat"
};

static int vtoy_check_os_param(ventoy_os_param *param)
{
    uint32_t i;
    uint8_t  chksum = 0;
    uint8_t *buf = (uint8_t *)param;
    
    if (memcmp(&param->guid, &vtoy_guid, sizeof(ventoy_guid)))
    {
        uint8_t *data1 = (uint8_t *)(&param->guid);
        uint8_t *data2 = (uint8_t *)(&vtoy_guid);
        
        for (i = 0; i < 16; i++)
        {
            if (data1[i] != data2[i])
            {
                debug("guid not equal i = %u, 0x%02x, 0x%02x\n", i, data1[i], data2[i]);
            }
        }
        return 1;
    }

    for (i = 0; i < sizeof(ventoy_os_param); i++)
    {
        chksum += buf[i];
    }

    if (chksum)
    {
        debug("Invalid checksum 0x%02x\n", chksum);
        return 1;
    }

    return 0;
}

static int vtoy_os_param_from_file(const char *filename, ventoy_os_param *param)
{
    int fd = 0;
    int rc = 0;

    fd = open(filename, O_RDONLY | O_BINARY);
    if (fd < 0)
    {
        fprintf(stderr, "Failed to open file %s error %d\n", filename, errno);
        return errno;
    }

    read(fd, param, sizeof(ventoy_os_param));

    if (vtoy_check_os_param(param) == 0)
    {
        debug("find ventoy os param in file %s\n", filename);
    }
    else
    {
        debug("ventoy os pararm NOT found in file %s\n", filename);
        rc = 1;
    }
    
    close(fd);
    return rc;
}

static void vtoy_dump_os_param(ventoy_os_param *param)
{
    printf("################# dump os param ################\n");

    printf("param->chksum = 0x%x\n", param->chksum);
    printf("param->vtoy_disk_guid = %02x %02x %02x %02x\n", 
        param->vtoy_disk_guid[0], param->vtoy_disk_guid[1], 
        param->vtoy_disk_guid[2], param->vtoy_disk_guid[3]);
    
    printf("param->vtoy_disk_signature = %02x %02x %02x %02x\n", 
        param->vtoy_disk_signature[0], param->vtoy_disk_signature[1], 
        param->vtoy_disk_signature[2], param->vtoy_disk_signature[3]);
    
    printf("param->vtoy_disk_size = %llu\n", (unsigned long long)param->vtoy_disk_size);
    printf("param->vtoy_disk_part_id = %u\n", param->vtoy_disk_part_id);
    printf("param->vtoy_disk_part_type = %u\n", param->vtoy_disk_part_type);
    printf("param->vtoy_img_path = <%s>\n", param->vtoy_img_path);
    printf("param->vtoy_img_size = <%llu>\n", (unsigned long long)param->vtoy_img_size);
    printf("param->vtoy_img_location_addr = <0x%llx>\n", (unsigned long long)param->vtoy_img_location_addr);
    printf("param->vtoy_img_location_len = <%u>\n", param->vtoy_img_location_len);
    printf("param->vtoy_reserved = %02x %02x %02x %02x %02x %02x %02x %02x\n", 
        param->vtoy_reserved[0],
        param->vtoy_reserved[1],
        param->vtoy_reserved[2],
        param->vtoy_reserved[3],
        param->vtoy_reserved[4],
        param->vtoy_reserved[5],
        param->vtoy_reserved[6],
        param->vtoy_reserved[7]
        );
    
    printf("\n");
}

static int vtoy_get_disk_guid(const char *diskname, uint8_t *vtguid, uint8_t *vtsig)
{
    int i = 0;
    int fd = 0;
    char devdisk[128] = {0};

    snprintf(devdisk, sizeof(devdisk) - 1, "/dev/%s", diskname);
    
    fd = open(devdisk, O_RDONLY | O_BINARY);
    if (fd >= 0)
    {
        lseek(fd, 0x180, SEEK_SET);
        read(fd, vtguid, 16);
        
        lseek(fd, 0x1b8, SEEK_SET);
        read(fd, vtsig, 4);
        close(fd);

        debug("GUID for %s: <", devdisk);
        for (i = 0; i < 16; i++)
        {
            debug("%02x", vtguid[i]);
        }
        debug(">\n");
        
        return 0;
    }
    else
    {
        debug("failed to open %s %d\n", devdisk, errno);
        return errno;
    }
}

static unsigned long long vtoy_get_disk_size_in_byte(const char *disk)
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
        debug("get disk size from sysfs for %s\n", disk);
        
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
        debug("%s not exist \n", diskpath);
    }

    // Try 2: get size from ioctl
    snprintf(diskpath, sizeof(diskpath) - 1, "/dev/%s", disk);
    fd = open(diskpath, O_RDONLY);
    if (fd >= 0)
    {
        debug("get disk size from ioctl for %s\n", disk);
        rc = ioctl(fd, BLKGETSIZE64, &size);
        if (rc == -1)
        {
            size = 0;
            debug("failed to ioctl %d\n", rc);
        }
        close(fd);
    }
    else
    {
        debug("failed to open %s %d\n", diskpath, errno);
    }

    debug("disk %s size %llu bytes\n", disk, (unsigned long long)size);
    return size;
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
    if (name[0] == 's' && name[1] == 'r' && IS_DIGIT(name[2]))
    {
        return 0;
    }
    
    return 1;
}

static int vtoy_find_disk_by_size(unsigned long long size, char *diskname)
{
    unsigned long long cursize = 0;
    DIR* dir = NULL;
    struct dirent* p = NULL;
    int rc = 0;

    dir = opendir("/sys/block");
    if (!dir)
    {
        return 0;
    }
    
    while ((p = readdir(dir)) != NULL)
    {
        if (!vtoy_is_possible_blkdev(p->d_name))
        {
            debug("disk %s is filted by name\n", p->d_name);
            continue;
        }
    
        cursize = vtoy_get_disk_size_in_byte(p->d_name);
        debug("disk %s size %llu\n", p->d_name, (unsigned long long)cursize);
        if (cursize == size)
        {
            sprintf(diskname, "%s", p->d_name);
            rc++;
        }
    }
    closedir(dir);
    return rc;    
}

int vtoy_find_disk_by_guid(ventoy_os_param *param, char *diskname)
{
    int rc = 0;
    int count = 0;
    DIR* dir = NULL;
    struct dirent* p = NULL;
    uint8_t vtguid[16];
    uint8_t vtsig[16];

    dir = opendir("/sys/block");
    if (!dir)
    {
        return 0;
    }
    
    while ((p = readdir(dir)) != NULL)
    {
        if (!vtoy_is_possible_blkdev(p->d_name))
        {
            debug("disk %s is filted by name\n", p->d_name);        
            continue;
        }
    
        memset(vtguid, 0, sizeof(vtguid));
        memset(vtsig, 0, sizeof(vtsig));
        rc = vtoy_get_disk_guid(p->d_name, vtguid, vtsig);
        if (rc == 0 && memcmp(vtguid, param->vtoy_disk_guid, 16) == 0 && 
            memcmp(vtsig, param->vtoy_disk_signature, 4) == 0)
        {
            sprintf(diskname, "%s", p->d_name);
            count++;
        }
    }
    closedir(dir);
    
    return count;    
}

static int vtoy_find_disk_by_sig(uint8_t *sig, char *diskname)
{
    int rc = 0;
    int count = 0;
    DIR* dir = NULL;
    struct dirent* p = NULL;
    uint8_t vtguid[16];
    uint8_t vtsig[16];

    dir = opendir("/sys/block");
    if (!dir)
    {
        return 0;
    }
    
    while ((p = readdir(dir)) != NULL)
    {
        if (!vtoy_is_possible_blkdev(p->d_name))
        {
            debug("disk %s is filted by name\n", p->d_name);        
            continue;
        }

        memset(vtguid, 0, sizeof(vtguid));
        memset(vtsig, 0, sizeof(vtsig));
        rc = vtoy_get_disk_guid(p->d_name, vtguid, vtsig);
        if (rc == 0 && memcmp(vtsig, sig, 4) == 0)
        {
            sprintf(diskname, "%s", p->d_name);
            count++;
        }
    }
    closedir(dir);
    
    return count;    
}

static int vtoy_printf_iso_path(ventoy_os_param *param)
{
    printf("%s\n", param->vtoy_img_path);
    return 0;
}

static int vtoy_printf_fs(ventoy_os_param *param)
{
    const char *fs[] = 
    {
        "exfat", "ntfs", "ext", "xfs", "udf", "fat"
    };

    if (param->vtoy_disk_part_type < 6)
    {
        printf("%s\n", fs[param->vtoy_disk_part_type]);
    }
    else
    {
        printf("unknown\n");
    }
    return 0;
}

static int vtoy_vlnk_printf(ventoy_os_param *param, char *diskname)
{
    int cnt = 0;
    uint8_t disk_sig[4];
    uint8_t mbr[512];
    int fd = -1;
    char diskpath[128];
    uint8_t check[8] = { 0x56, 0x54, 0x00, 0x47, 0x65, 0x00, 0x48, 0x44 };
    
    memcpy(disk_sig, param->vtoy_reserved + 7, 4);

    debug("vlnk disk sig: %02x %02x %02x %02x \n", disk_sig[0], disk_sig[1], disk_sig[2], disk_sig[3]);

    cnt = vtoy_find_disk_by_sig(disk_sig, diskname);
    if (cnt == 1)
    {
        snprintf(diskpath, sizeof(diskpath), "/dev/%s", diskname);
        fd = open(diskpath, O_RDONLY | O_BINARY);
        if (fd >= 0)
        {
            memset(mbr, 0, sizeof(mbr));
            read(fd, mbr, sizeof(mbr));
            close(fd);

            if (memcmp(mbr + 0x190, check, 8) == 0)
            {
                printf("/dev/%s", diskname);
                return 0;                
            }
            else
            {
                debug("check data failed /dev/%s\n", diskname);
            }
        }
    }

    debug("find count=%d\n", cnt);
    printf("unknown");
    return 1;
}

static int vtoy_check_iso_path_alpnum(ventoy_os_param *param)
{
    char c;
    int i = 0;
    
    while (param->vtoy_img_path[i])
    {
        c = param->vtoy_img_path[i]; 
        
        if (isalnum(c) || c == '_' || c == '-')
        {
            
        }
        else
        {
            return 1;
        }
        i++;
    }

    return 0;
}

static int vtoy_check_device(ventoy_os_param *param, const char *device)
{
    unsigned long long size; 
    uint8_t vtguid[16] = {0};
    uint8_t vtsig[4] = {0};

    debug("vtoy_check_device for <%s>\n", device);

    size = vtoy_get_disk_size_in_byte(device);
    vtoy_get_disk_guid(device, vtguid, vtsig);

    debug("param->vtoy_disk_size=%llu size=%llu\n", 
        (unsigned long long)param->vtoy_disk_size, (unsigned long long)size);

    if (memcmp(vtguid, param->vtoy_disk_guid, 16) == 0 &&
        memcmp(vtsig, param->vtoy_disk_signature, 4) == 0)
    {
        debug("<%s> is right ventoy disk\n", device);
        return 0;
    }
    else
    {
        debug("<%s> is NOT right ventoy disk\n", device);
        return 1;
    }
}

static int vtoy_print_os_param(ventoy_os_param *param, char *diskname)
{
    int fd, size;
    int cnt = 0;
    char *path = param->vtoy_img_path;
    const char *fs;
    char diskpath[256] = {0};
    char sizebuf[64] = {0};
    
    cnt = vtoy_find_disk_by_size(param->vtoy_disk_size, diskname);
    debug("find disk by size %llu, cnt=%d...\n", (unsigned long long)param->vtoy_disk_size, cnt);
    if (1 == cnt)
    {
        if (vtoy_check_device(param, diskname) != 0)
        {
            cnt = 0;
        }
    }
    else
    {
        cnt = vtoy_find_disk_by_guid(param, diskname);
        debug("find disk by guid cnt=%d...\n", cnt);
    }
    
    if (param->vtoy_disk_part_type < ventoy_fs_max)
    {
        fs = g_ventoy_fs[param->vtoy_disk_part_type];
    }
    else
    {
        fs = "unknown";
    }

    if (1 == cnt)
    {
        if (strstr(diskname, "nvme") || strstr(diskname, "mmc") || strstr(diskname, "nbd"))
        {
            snprintf(diskpath, sizeof(diskpath) - 1, "/sys/class/block/%sp2/size", diskname);
        }
        else
        {
            snprintf(diskpath, sizeof(diskpath) - 1, "/sys/class/block/%s2/size", diskname);
        }

        if (param->vtoy_reserved[6] == 0 && access(diskpath, F_OK) >= 0)
        {
            debug("get part size from sysfs for %s\n", diskpath);

            fd = open(diskpath, O_RDONLY | O_BINARY);
            if (fd >= 0)
            {
                read(fd, sizebuf, sizeof(sizebuf));
                size = (int)strtoull(sizebuf, NULL, 10);
                close(fd);
                if ((size != (64 * 1024)) && (size != (8 * 1024)))
                {
                    debug("sizebuf=<%s> size=%d\n", sizebuf, size);
                    return 1;
                }
            }
        }
        else
        {
            debug("%s not exist \n", diskpath);
        }

        printf("/dev/%s#%s#%s\n", diskname, fs, path);
        return 0;
    }
    else
    {
        return 1;
    }
}

/*
 *  Find disk and image path from ventoy runtime data.
 *  By default data is read from phymem(legacy bios) or efivar(UEFI), if -f is input, data is read from file.
 *  
 *  -f datafile     os param data file. 
 *  -c /dev/xxx     check ventoy disk
 *  -v              be verbose
 *  -l              also print image disk location 
 */
int vtoydump_main(int argc, char **argv)
{
    int rc;
    int ch;
    int print_path = 0;
    int check_ascii = 0;
    int print_fs = 0;
    int vlnk_print = 0;
    char filename[256] = {0};
    char diskname[256] = {0};
    char device[64] = {0};
    ventoy_os_param *param = NULL;

    while ((ch = getopt(argc, argv, "a:c:f:p:t:s:v::")) != -1)
    {
        if (ch == 'f')
        {
            strncpy(filename, optarg, sizeof(filename) - 1);
        }
        else if (ch == 'v')
        {
            verbose = 1;
        }
        else if (ch == 'c')
        {
            strncpy(device, optarg, sizeof(device) - 1);
        }
        else if (ch == 'p')
        {
            print_path = 1;
            strncpy(filename, optarg, sizeof(filename) - 1);
        }
        else if (ch == 'a')
        {
            check_ascii = 1;
            strncpy(filename, optarg, sizeof(filename) - 1);
        }
        else if (ch == 't')
        {
            vlnk_print = 1;
            strncpy(filename, optarg, sizeof(filename) - 1);
        }
        else if (ch == 's')
        {
            print_fs = 1;
            strncpy(filename, optarg, sizeof(filename) - 1);
        }
        else
        {
            fprintf(stderr, "Usage: %s -f datafile [ -v ] \n", argv[0]);
            return 1;
        }
    }

    if (filename[0] == 0)
    {
        fprintf(stderr, "Usage: %s -f datafile [ -v ] \n", argv[0]);
        return 1;
    }

    param = malloc(sizeof(ventoy_os_param));
    if (NULL == param)
    {
        fprintf(stderr, "failed to alloc memory with size %d error %d\n", 
                (int)sizeof(ventoy_os_param), errno);
        return 1;
    }
    
    memset(param, 0, sizeof(ventoy_os_param));

    debug("get os pararm from file %s\n", filename);
    rc = vtoy_os_param_from_file(filename, param);
    if (rc)
    {
        debug("ventoy os param not found %d %d\n", rc, ENOENT);
        if (ENOENT == rc)
        {
            debug("now try with file %s\n", "/ventoy/ventoy_os_param");
            rc = vtoy_os_param_from_file("/ventoy/ventoy_os_param", param);
            if (rc)
            {
                goto end;
            }
        }
        else
        {
            goto end;            
        }        
    }

    if (verbose)
    {
        vtoy_dump_os_param(param);
    }

    if (print_path)
    {
        rc = vtoy_printf_iso_path(param);
    }
    else if (print_fs)
    {
        rc = vtoy_printf_fs(param);
    }
    else if (vlnk_print)
    {
        rc = vtoy_vlnk_printf(param, diskname);
    }
    else if (device[0])
    {
        rc = vtoy_check_device(param, device);
    }
    else if (check_ascii)
    {
        rc = vtoy_check_iso_path_alpnum(param);
    }
    else
    {
        // print os param, you can change the output format in the function
        rc = vtoy_print_os_param(param, diskname);
    }

end:
    if (param)
    {
        free(param);
    }
    return rc;
}

// wrapper main
#ifndef BUILD_VTOY_TOOL
int main(int argc, char **argv)
{
    return vtoydump_main(argc, argv);
}
#endif

