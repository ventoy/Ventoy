/******************************************************************************
 * ventoy_disk.c  ---- ventoy disk
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
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/fs.h>
#include <dirent.h>
#include <time.h>
#include <ventoy_define.h>
#include <ventoy_disk.h>
#include <ventoy_util.h>
#include <fat_filelib.h>

static int g_fatlib_media_fd = 0;
static uint64_t g_fatlib_media_offset = 0;

static const char *g_ventoy_dev_type_str[VTOY_DEVICE_END] = 
{
    "unknown", "scsi", "USB", "ide", "dac960",
    "cpqarray", "file", "ataraid", "i2o",
    "ubd", "dasd", "viodasd", "sx8", "dm",
    "xvd", "sd/mmc", "virtblk", "aoe",
    "md", "loopback", "nvme", "brd", "pmem"
};

static const char * ventoy_get_dev_type_name(ventoy_dev_type type)
{
    return (type < VTOY_DEVICE_END) ? g_ventoy_dev_type_str[type] : "unknown";
}

static int ventoy_check_blk_major(int major, const char *type)
{
    int flag = 0;
    int valid = 0;
    int devnum = 0;
    int len = 0;
    char line[64];
    char *pos = NULL;
    FILE *fp = NULL;

    fp = fopen("/proc/devices", "r");
    if (!fp)
    {
        return 0;
    }

    len = (int)strlen(type);
    while (fgets(line, sizeof(line), fp))
    {
        if (flag)
        {
            pos = strchr(line, ' ');
            if (pos)
            {
                devnum = (int)strtol(line, NULL, 10);
                if (devnum == major)
                {
                    if (strncmp(pos + 1, type, len) == 0)
                    {
                        valid = 1;                        
                    }
                    break;
                }
            }
        }
        else if (strncmp(line, "Block devices:", 14) == 0)
        {
            flag = 1;
        }
    }

    fclose(fp);
    return valid;
}

static int ventoy_get_disk_devnum(const char *name, int *major, int* minor)
{
    int rc;
    char *pos;
    char devnum[16] = {0};
    
    rc = ventoy_get_sys_file_line(devnum, sizeof(devnum), "/sys/block/%s/dev", name);
    if (rc)
    {
        return 1;
    }

    pos = strstr(devnum, ":");
    if (!pos)
    {
        return 1;
    }

    *major = (int)strtol(devnum, NULL, 10);
    *minor = (int)strtol(pos + 1, NULL, 10);

    return 0;
}

static ventoy_dev_type ventoy_get_dev_type(const char *name, int major, int minor)
{
    int rc;
    char syspath[128];
    char dstpath[256];

    memset(syspath, 0, sizeof(syspath));
    memset(dstpath, 0, sizeof(dstpath));
    
    scnprintf(syspath, sizeof(syspath), "/sys/block/%s", name);
    rc = readlink(syspath, dstpath, sizeof(dstpath) - 1);
    if (rc > 0 && strstr(dstpath, "/usb"))
    {
        return VTOY_DEVICE_USB;
    }
    
    if (SCSI_BLK_MAJOR(major) && (minor % 0x10 == 0)) 
    {
        return VTOY_DEVICE_SCSI;
    }
    else if (IDE_BLK_MAJOR(major) && (minor % 0x40 == 0)) 
    {
        return VTOY_DEVICE_IDE;
    }
    else if (major == DAC960_MAJOR && (minor % 0x8 == 0)) 
    {
        return VTOY_DEVICE_DAC960;
    }
    else if (major == ATARAID_MAJOR && (minor % 0x10 == 0)) 
    {
        return VTOY_DEVICE_ATARAID;
    }
    else if (major == AOE_MAJOR && (minor % 0x10 == 0)) 
    {
        return VTOY_DEVICE_AOE;
    }
    else if (major == DASD_MAJOR && (minor % 0x4 == 0)) 
    {
        return VTOY_DEVICE_DASD;
    }
    else if (major == VIODASD_MAJOR && (minor % 0x8 == 0)) 
    {
        return VTOY_DEVICE_VIODASD;
    }
    else if (SX8_BLK_MAJOR(major) && (minor % 0x20 == 0)) 
    {
        return VTOY_DEVICE_SX8;
    }
    else if (I2O_BLK_MAJOR(major) && (minor % 0x10 == 0)) 
    {
        return VTOY_DEVICE_I2O;
    }
    else if (CPQARRAY_BLK_MAJOR(major) && (minor % 0x10 == 0)) 
    {
        return VTOY_DEVICE_CPQARRAY;
    }
    else if (UBD_MAJOR == major && (minor % 0x10 == 0)) 
    {
        return VTOY_DEVICE_UBD;
    }
    else if (XVD_MAJOR == major && (minor % 0x10 == 0)) 
    {
        return VTOY_DEVICE_XVD;
    }
    else if (SDMMC_MAJOR == major && (minor % 0x8 == 0)) 
    {
        return VTOY_DEVICE_SDMMC;
    }
    else if (ventoy_check_blk_major(major, "virtblk"))
    {
        return VTOY_DEVICE_VIRTBLK;
    }
    else if (major == LOOP_MAJOR)
    {
        return VTOY_DEVICE_LOOP;
    }
    else if (major == MD_MAJOR)
    {
        return VTOY_DEVICE_MD;
    }
    else if (major == RAM_MAJOR)
    {
        return VTOY_DEVICE_RAM;
    }
    else if (strstr(name, "nvme") && ventoy_check_blk_major(major, "blkext"))
    {
        return VTOY_DEVICE_NVME;
    }
    else if (strstr(name, "pmem") && ventoy_check_blk_major(major, "blkext"))
    {
        return VTOY_DEVICE_PMEM;
    }
    
    return VTOY_DEVICE_END;
}

static int ventoy_is_possible_blkdev(const char *name)
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
    
    /* /dev/zramX */
    if (name[0] == 'z' && name[1] == 'r' && name[2] == 'a' && name[3] == 'm')
    {
        return 0;
    }

    /* /dev/loopX */
    if (name[0] == 'l' && name[1] == 'o' && name[2] == 'o' && name[3] == 'p')
    {
        return 0;
    }

    /* /dev/dm-X */
    if (name[0] == 'd' && name[1] == 'm' && name[2] == '-' && isdigit(name[3]))
    {
        return 0;
    }

    /* /dev/srX */
    if (name[0] == 's' && name[1] == 'r' && isdigit(name[2]))
    {
        return 0;
    }
    
    return 1;
}

uint64_t ventoy_get_disk_size_in_byte(const char *disk)
{
    int fd;
    int rc;
    unsigned long long size = 0;
    char diskpath[256] = {0};
    char sizebuf[64] = {0};

    // Try 1: get size from sysfs
    scnprintf(diskpath, sizeof(diskpath) - 1, "/sys/block/%s/size", disk);
    if (access(diskpath, F_OK) >= 0)
    {
        vdebug("get disk size from sysfs for %s\n", disk);
        
        fd = open(diskpath, O_RDONLY | O_BINARY);
        if (fd >= 0)
        {
            read(fd, sizebuf, sizeof(sizebuf));
            size = strtoull(sizebuf, NULL, 10);
            close(fd);
            return (uint64_t)(size * 512);
        }
    }
    else
    {
        vdebug("%s not exist \n", diskpath);
    }

    // Try 2: get size from ioctl
    scnprintf(diskpath, sizeof(diskpath) - 1, "/dev/%s", disk);
    fd = open(diskpath, O_RDONLY);
    if (fd >= 0)
    {
        vdebug("get disk size from ioctl for %s\n", disk);
        rc = ioctl(fd, BLKGETSIZE64, &size);
        if (rc == -1)
        {
            size = 0;
            vdebug("failed to ioctl %d\n", rc);
        }
        close(fd);
    }
    else
    {
        vdebug("failed to open %s %d\n", diskpath, errno);
    }

    vdebug("disk %s size %llu bytes\n", disk, size);
    return size;
}

int ventoy_get_disk_vendor(const char *name, char *vendorbuf, int bufsize)
{
    return ventoy_get_sys_file_line(vendorbuf, bufsize, "/sys/block/%s/device/vendor", name);
}

int ventoy_get_disk_model(const char *name, char *modelbuf, int bufsize)
{
    return ventoy_get_sys_file_line(modelbuf, bufsize, "/sys/block/%s/device/model", name);
}

static int fatlib_media_sector_read(uint32 sector, uint8 *buffer, uint32 sector_count)
{
    lseek(g_fatlib_media_fd, (sector + g_fatlib_media_offset) * 512ULL, SEEK_SET);
    read(g_fatlib_media_fd, buffer, sector_count * 512);
    
    return 1;
}

static int fatlib_is_secure_boot_enable(void)
{
    void *flfile = NULL;
    
    flfile = fl_fopen("/EFI/BOOT/grubx64_real.efi", "rb");
    if (flfile)
    {
        vlog("/EFI/BOOT/grubx64_real.efi find, secure boot in enabled\n");
        fl_fclose(flfile);
        return 1;
    }
    else
    {
        vlog("/EFI/BOOT/grubx64_real.efi not exist\n");
    }
    
    return 0;
}

static int fatlib_get_ventoy_version(char *verbuf, int bufsize)
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

                scnprintf(verbuf, bufsize - 1, "%s", pos);
                rc = 0;
            }
            free(buf);
        }

        fl_fclose(flfile);
    }
    else
    {
        vdebug("No grub.cfg found\n");
    }
    
    return rc;
}

/* <BEGIN>: Deleted by longpanda, 20211028 PN:XX LABEL:XX */
#if 0
int ventoy_get_vtoy_data(ventoy_disk *info, int *ppartstyle)
{
    int i;
    int fd;
    int len;
    int rc = 1;
    int ret = 1;
    int part_style;
    uint64_t part1_start_sector;
    uint64_t part1_sector_count;
    uint64_t part2_start_sector;
    uint64_t part2_sector_count;
    uint64_t preserved_space;
    char name[64] = {0};
    disk_ventoy_data *vtoy = NULL;    
    VTOY_GPT_INFO *gpt = NULL;
    
    vtoy = &(info->vtoydata);
    gpt = &(vtoy->gptinfo);
    memset(vtoy, 0, sizeof(disk_ventoy_data));

    vdebug("ventoy_get_vtoy_data %s\n", info->disk_path);

    if (info->size_in_byte < (2 * VTOYEFI_PART_BYTES))
    {
        vdebug("disk %s is too small %llu\n", info->disk_path, (_ull)info->size_in_byte);
        return 1;
    }

    fd = open(info->disk_path, O_RDONLY | O_BINARY);
    if (fd < 0)
    {
        vdebug("failed to open %s %d\n", info->disk_path, errno);
        return 1;
    }

    len = (int)read(fd, &(vtoy->gptinfo), sizeof(VTOY_GPT_INFO));
    if (len != sizeof(VTOY_GPT_INFO))
    {
        vdebug("failed to read %s %d\n", info->disk_path, errno);
        goto end;
    }

    if (gpt->MBR.Byte55 != 0x55 || gpt->MBR.ByteAA != 0xAA)
    {
        vdebug("Invalid mbr magic 0x%x 0x%x\n", gpt->MBR.Byte55, gpt->MBR.ByteAA);
        goto end;
    }

    if (gpt->MBR.PartTbl[0].FsFlag == 0xEE && strncmp(gpt->Head.Signature, "EFI PART", 8) == 0)
    {
        part_style = GPT_PART_STYLE;
        if (ppartstyle)
        {
            *ppartstyle = part_style;
        }

        if (gpt->PartTbl[0].StartLBA == 0 || gpt->PartTbl[1].StartLBA == 0)
        {
            vdebug("NO ventoy efi part layout <%llu %llu>\n", 
                (_ull)gpt->PartTbl[0].StartLBA,
                (_ull)gpt->PartTbl[1].StartLBA);
            goto end;
        }

        for (i = 0; i < 36; i++)
        {
            name[i] = (char)(gpt->PartTbl[1].Name[i]);
        }
        if (strcmp(name, "VTOYEFI"))
        {
            vdebug("Invalid efi part2 name <%s>\n", name);
            goto end;
        }

        part1_start_sector = gpt->PartTbl[0].StartLBA;
        part1_sector_count = gpt->PartTbl[0].LastLBA - part1_start_sector + 1;
        part2_start_sector = gpt->PartTbl[1].StartLBA;
        part2_sector_count = gpt->PartTbl[1].LastLBA - part2_start_sector + 1;

        preserved_space = info->size_in_byte - (part2_start_sector + part2_sector_count + 33) * 512;
    }
    else
    {
        part_style = MBR_PART_STYLE;
        if (ppartstyle)
        {
            *ppartstyle = part_style;
        }
        
        part1_start_sector = gpt->MBR.PartTbl[0].StartSectorId;
        part1_sector_count = gpt->MBR.PartTbl[0].SectorCount;
        part2_start_sector = gpt->MBR.PartTbl[1].StartSectorId;
        part2_sector_count = gpt->MBR.PartTbl[1].SectorCount;

        preserved_space = info->size_in_byte - (part2_start_sector + part2_sector_count) * 512;
    }

    if (part1_start_sector != VTOYIMG_PART_START_SECTOR ||
        part2_sector_count != VTOYEFI_PART_SECTORS ||
        (part1_start_sector + part1_sector_count) != part2_start_sector)
    {
        vdebug("Not valid ventoy partition layout [%llu %llu] [%llu %llu]\n", 
               part1_start_sector, part1_sector_count, part2_start_sector, part2_sector_count);
        goto end;
    }

    vdebug("ventoy partition layout check OK: [%llu %llu] [%llu %llu]\n", 
               part1_start_sector, part1_sector_count, part2_start_sector, part2_sector_count);

    vtoy->ventoy_valid = 1;

    vdebug("now check secure boot for %s ...\n", info->disk_path);

    g_fatlib_media_fd = fd;
    g_fatlib_media_offset = part2_start_sector;
    fl_init();

    if (0 == fl_attach_media(fatlib_media_sector_read, NULL))
    {
        ret = fatlib_get_ventoy_version(vtoy->ventoy_ver, sizeof(vtoy->ventoy_ver));
        if (ret == 0 && vtoy->ventoy_ver[0])
        {
            vtoy->secure_boot_flag = fatlib_is_secure_boot_enable();            
        }
        else
        {
            vdebug("fatlib_get_ventoy_version failed %d\n", ret);
        }
    }
    else
    {
        vdebug("fl_attach_media failed\n");
    }

    fl_shutdown();
    g_fatlib_media_fd = -1;
    g_fatlib_media_offset = 0;

    if (vtoy->ventoy_ver[0] == 0)
    {
        vtoy->ventoy_ver[0] = '?';
    }

    if (0 == vtoy->ventoy_valid)
    {
        goto end;
    }
    
    lseek(fd, 2040 * 512, SEEK_SET);
    read(fd, vtoy->rsvdata, sizeof(vtoy->rsvdata));

    vtoy->preserved_space = preserved_space;
    vtoy->partition_style = part_style;
    vtoy->part2_start_sector = part2_start_sector;

    rc = 0;
end:
    vtoy_safe_close_fd(fd);
    return rc;
}
#endif /* #if 0 */
/* <END>  : Deleted by longpanda, 20211028 PN:XX LABEL:XX */


int ventoy_get_disk_info(char **argv)
{
    uint64_t size;
    char vendor[128];
    char model[128];
    char *disk = argv[4];

    if (strncmp(argv[4], "/dev/", 4) == 0)
    {
        disk += 4;
    }
    ventoy_get_disk_vendor(disk, vendor, sizeof(vendor));
    ventoy_get_disk_model(disk, model, sizeof(model));

    scnprintf(g_sysinfo.cur_model, sizeof(g_sysinfo.cur_model), "%s %s  [%s]", vendor, model, argv[4]);
    strlcpy(g_sysinfo.cur_ventoy_ver, argv[5]);
    strlcpy(g_sysinfo.cur_fsname, argv[6]);
    g_sysinfo.cur_part_style = (int)strtol(argv[7], NULL, 10);
    g_sysinfo.cur_secureboot = (int)strtol(argv[8], NULL, 10);

    size = ventoy_get_disk_size_in_byte(disk);
    scnprintf(g_sysinfo.cur_capacity, sizeof(g_sysinfo.cur_capacity), "%dGB", (int)ventoy_get_human_readable_gb(size));
    
    return 0;
}

int ventoy_disk_init(void)
{
    return 0;
}

void ventoy_disk_exit(void)
{
}


