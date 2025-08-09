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
#include <udisks/udisks.h>

static int g_fatlib_media_fd = 0;
static uint64_t g_fatlib_media_offset = 0;

// UDisks2 context
static UDisksClient *client;
GArray *disks = NULL;

// static const char *g_ventoy_dev_type_str[VTOY_DEVICE_END] = 
// {
//     "unknown", "scsi", "USB", "ide", "dac960",
//     "cpqarray", "file", "ataraid", "i2o",
//     "ubd", "dasd", "viodasd", "sx8", "dm",
//     "xvd", "sd/mmc", "virtblk", "aoe",
//     "md", "loopback", "nvme", "brd", "pmem"
// };

/**
 * Get a ref to the client [transfer: none]
 */
UDisksClient *get_udisks_client()
{
    return client;
}

/**
 * Null safe disks length check
 */
int get_disks_len()
{
    if (disks == NULL) return 0;

    return disks->len;
}

/**
 * Do not free the returned pointer
 */
ventoy_disk *disks_get_by_name(const char *diskname)
{
   
    for (int i = 0; i < disks->len; i++)
    {
        ventoy_disk *current = &g_array_index(disks, ventoy_disk, i);

        if (strcmp(current->disk_name, diskname) == 0)
        {
            return current;
        }
    }

    return NULL;
}

int ventoy_disk_open(ventoy_disk *disk, const char *mode, int flags)
{
    
    GVariant *options;

    {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&builder,
                              "{sv}",
                              "flags",
                              g_variant_new_int32(flags));
        options = g_variant_builder_end(&builder);
        g_variant_ref_sink(options);
    }

    GVariant *out_fd = NULL;
    GUnixFDList *out_fd_list = NULL;
    
    GError *error = NULL;
    udisks_block_call_open_device_sync(disk->blockdev,
                                       mode,
                                       options,
                                       NULL,
                                       &out_fd,
                                       &out_fd_list,
                                       NULL,
                                       &error);

    g_variant_unref(options);
    
    if (error != NULL)
    {
        vlog("Failed to open device: %s\n", error->message);
        g_error_free(error);
        return -1;
    }

    gint fd = g_unix_fd_list_get(out_fd_list, g_variant_get_handle(out_fd), &error);

    if (error != NULL)
    {
        vlog("Failed to open device: %s\n", error->message);
        g_error_free(error);
        return -1;
    }

    return (int)fd;
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

static uint64_t ventoy_get_disk_devnum(const ventoy_disk *disk)
{
    return udisks_block_get_device_number(disk->blockdev);
}


int ventoy_is_disk_4k_native(ventoy_disk *disk)
{
    int rc = 0;
    int logsector = 0;
    int physector = 0;

    int fd = ventoy_disk_open(disk, "r", O_BINARY);
    if (fd < 1)
    {
        vlog("Can't open device %s", disk->disk_model);
        return -1;
    };

    ioctl(fd, BLKSSZGET, &logsector);
    ioctl(fd, BLKPBSZGET, &physector);
    
    if (logsector == 4096 && physector == 4096)
    {
        rc = 1;
    }

    vdebug("is 4k native disk <%s> <%d>\n", disk, rc);    
    return rc;
}

uint64_t ventoy_get_disk_size_in_byte(const ventoy_disk *disk)
{
    return udisks_block_get_size(disk->blockdev);
}

// We can get this from the drive
// int ventoy_get_disk_vendor(const ventoy_disk *disk)
// {
//     return udisks_block_get_drive(disk->blockdev)->
// }

// int ventoy_get_disk_model(const ventoy_disk *disk)
// {
//     return ventoy_get_sys_file_line(modelbuf, bufsize, "/sys/block/%s/device/model", name);
// }

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

                snprintf(verbuf, bufsize - 1, "%s", pos);
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

int ventoy_get_vtoy_data(ventoy_disk *disk)
{
    if (disk->table == NULL) return 1;

    int i;
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
    
    vtoy = &(disk->vtoydata);
    gpt = &(vtoy->gptinfo);

    /* step 1: Init memory */
    memset(vtoy, 0, sizeof(disk_ventoy_data));

    vdebug("ventoy_get_vtoy_data %s\n", udisks_block_get_device(disk->blockdev));

    /* step 2: Check size */
    if (udisks_block_get_size(disk->blockdev) < (2 * VTOYEFI_PART_BYTES))
    {
        vdebug("disk %s is too small %llu\n", udisks_block_get_device(disk->blockdev), (_ull)udisks_block_get_size(disk->blockdev));
        return 1;
    }

    /* step 3: open the device */
    int fd = ventoy_disk_open(disk, "r", O_BINARY);

    if (fd < 1) return 1;

    /* step 4: read gpt table */
    len = (int)read(fd, gpt, sizeof(VTOY_GPT_INFO));
    if (len != sizeof(VTOY_GPT_INFO))
    {
        vdebug("failed to read %s %d\n", udisks_block_get_device(disk->blockdev), errno);
        goto end;
    }

    /* step 5: handle invalid magic */
    if (gpt->MBR.Byte55 != 0x55 || gpt->MBR.ByteAA != 0xAA)
    {
        vdebug("Invalid mbr magic 0x%x 0x%x\n", gpt->MBR.Byte55, gpt->MBR.ByteAA);
        goto end;
    }
    
    /* step 6: save partitions start/end and preserved space */
    if (strcmp(udisks_partition_table_get_type_(disk->table), "gpt"))
    {
        part_style = GPT_PART_STYLE;

        /* checks that 2 partitions are present */
        if (gpt->PartTbl[0].StartLBA == 0 || gpt->PartTbl[1].StartLBA == 0)
        {
            vdebug("NO ventoy efi part layout <%llu %llu>\n", 
                (_ull)gpt->PartTbl[0].StartLBA,
                (_ull)gpt->PartTbl[1].StartLBA);
            goto end;
        }

        /* the second one must be naed VTOYEFI for ventoy to be installed */
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

        preserved_space = udisks_block_get_size(disk->blockdev) - (part2_start_sector + part2_sector_count + 33) * 512;
    }
    else
    {
        part_style = MBR_PART_STYLE;

        part1_start_sector = gpt->MBR.PartTbl[0].StartSectorId;
        part1_sector_count = gpt->MBR.PartTbl[0].SectorCount;
        part2_start_sector = gpt->MBR.PartTbl[1].StartSectorId;
        part2_sector_count = gpt->MBR.PartTbl[1].SectorCount;

        preserved_space = udisks_block_get_size(disk->blockdev) - (part2_start_sector + part2_sector_count) * 512;
    }

    /* step 7: check for ventoy partition layout */
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

    /* step 8: check for secure boot */
    vdebug("now check secure boot for %s ...\n", udisks_block_get_device(disk->blockdev));

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

end:
    return 0;
}

int ventoy_get_disk_info(ventoy_disk *disk)
{
    
    disk->is4kn = ventoy_is_disk_4k_native(disk);
    if (disk->is4kn < 0) return 1;
    
    strcpy(disk->disk_path, udisks_block_get_device(disk->blockdev));
    strcpy(disk->disk_name, disk->disk_path);

    // ventoy_get_disk_devnum(name, &disk->major, &disk->minor);
    // ventoy_get_disk_vendor(name, vendor, sizeof(vendor));
    // ventoy_get_disk_model(name, model, sizeof(model));

    scnprintf(disk->human_readable_size, "%llu GB", (_ull)ventoy_get_human_readable_gb(udisks_block_get_size(disk->blockdev)));
    UDisksDrive *drive = udisks_client_get_drive_for_block(client, disk->blockdev);

    char *vendor = udisks_drive_get_vendor(drive);
    if (vendor == NULL) vendor = "-";
    
    char *model = udisks_drive_get_model(drive);
    if (vendor == NULL) vendor = "-";

    char *bus = udisks_drive_get_connection_bus(drive);
    if (bus == NULL) bus = "-";
    
    scnprintf(disk->disk_model, "%s %s (%s)", vendor, model, bus);

    ventoy_get_vtoy_data(disk);
    
    // vdebug("disk:<%s %d:%d> model:<%s> size:%llu (%s)\n", 
    //     disk->disk_path, disk->major, disk->minor, disk->disk_model, udisks_block_get_size(disk->blockdev), disk->human_readable_size);

    if (disk->vtoydata.ventoy_valid)
    {
        vdebug("%s Ventoy:<%s> %s secureboot:%d preserve:%llu\n", udisks_block_get_device(disk->blockdev), disk->vtoydata.ventoy_ver, 
            disk->vtoydata.partition_style == MBR_PART_STYLE ? "MBR" : "GPT",
            disk->vtoydata.secure_boot_flag, (_ull)(disk->vtoydata.preserved_space));
    }
    else
    {
        vdebug("%s NO Ventoy detected\n", udisks_block_get_device(disk->blockdev));
    }

    return 0;
}

// static int ventoy_disk_compare(const ventoy_disk *disk1, const ventoy_disk *disk2)
// {
//     if (disk1->type == VTOY_DEVICE_USB && disk2->type == VTOY_DEVICE_USB)
//     {
//         return strcmp(disk1->disk_name, disk2->disk_name);
//     }
//     else if (disk1->type == VTOY_DEVICE_USB)
//     {
//         return -1;
//     }
//     else if (disk2->type == VTOY_DEVICE_USB)
//     {
//         return 1;
//     }
//     else
//     {
//         return strcmp(disk1->disk_name, disk2->disk_name);
//     }
// }


int ventoy_disk_enumerate_all(void)
{
    vdebug("ventoy_disk_enumerate_all\n");

    if (disks != NULL)
    {
        for (int i = 0; i < disks->len; ++i)
        {
            ventoy_disk *disk = &g_array_index(disks, ventoy_disk, i);

            if (disk->obj) g_object_unref(disk->obj);
            if (disk->blockdev) g_object_unref(disk->blockdev);
            if (disk->table) g_object_unref(disk->table);
        }

        g_array_free(disks, TRUE);
    }

    disks = g_array_new(FALSE, FALSE, sizeof(ventoy_disk));

    GList *el = g_dbus_object_manager_get_objects(udisks_client_get_object_manager(client));

    for (; el != NULL; el = el->next)
    {
        UDisksObject *obj = UDISKS_OBJECT (el->data);
        UDisksBlock *block = udisks_object_get_block(obj);
        UDisksPartitionTable *table = udisks_object_get_partition_table(obj);

        if (block == NULL) continue;

        if (udisks_block_get_device(block) == NULL) continue;

        if (udisks_block_get_hint_system(block)) continue;
        
        if (udisks_block_get_hint_ignore(block)) continue;

        if (udisks_object_peek_partition(obj) != NULL) continue;  // It must not be a partition

        // The block and the drive are not the same object
        if (udisks_block_get_drive(block) == NULL || strcmp(udisks_block_get_drive(block), "/") == 0) continue;

        ventoy_disk disk;
        disk.obj = obj;
        disk.blockdev = block;
        disk.table = table;
        ventoy_get_disk_info(&disk);//check for error..

        g_array_append_val(disks, disk);
    }

    // Here we could sort the array...

    return 0;
}

void ventoy_disk_dump(ventoy_disk *cur)
{
    if (cur->vtoydata.ventoy_valid)
    {
        vdebug("%s [%s]\tVentoy: %s %s secureboot:%d preserve:%llu\n", 
            udisks_block_get_device(cur->blockdev), cur->human_readable_size,
            cur->vtoydata.ventoy_ver, cur->vtoydata.partition_style == MBR_PART_STYLE ? "MBR" : "GPT",
            cur->vtoydata.secure_boot_flag, (_ull)(cur->vtoydata.preserved_space));
    }
    else
    {
        vdebug("%s [%s]\tVentoy: NA\n", udisks_block_get_device(cur->blockdev), cur->human_readable_size); 
    }
}

void ventoy_disk_dump_all(void)
{
    vdebug("============= DISK DUMP ============\n");
    for (int i = 0; i < disks->len; i++)
    {
        ventoy_disk *disk = &g_array_index(disks, ventoy_disk, i);
        ventoy_disk_dump(disk);
    }
}

int ventoy_disk_install(ventoy_disk *disk, void *efipartimg)
{
    return 0;
}


int ventoy_disk_init(void)
{
    GError *error = NULL;
    client = udisks_client_new_sync(NULL, &error);
    disks = NULL;

    if (!client)
    {
        vlog("Error connecting to UDisks: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    ventoy_disk_enumerate_all();
    ventoy_disk_dump_all();
    
    return 0;
}

void ventoy_disk_exit(void)
{
    for (int i = 0; i < disks->len; ++i)
    {
        ventoy_disk disk = g_array_index(disks, ventoy_disk, i);

        g_object_unref(disk.blockdev);
        g_object_unref(disk.table);
    }
    g_array_free(disks, TRUE);
    disks = NULL;
    g_object_unref(client);
}
