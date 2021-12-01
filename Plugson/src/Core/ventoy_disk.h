/******************************************************************************
 * ventoy_disk.h
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
#ifndef __VENTOY_DISK_H__
#define __VENTOY_DISK_H__

#define MAX_DISK 256
typedef struct ventoy_disk
{
    char devname[64];
    
    int pathcase;
    char cur_fsname[64];
    char cur_capacity[64];
    char cur_model[256];
    char cur_ventoy_ver[64];
    int cur_secureboot;
    int cur_part_style;
    
}ventoy_disk;

#if defined(_MSC_VER) || defined(WIN32)



#else

typedef enum 
{
    VTOY_DEVICE_UNKNOWN = 0,
    VTOY_DEVICE_SCSI,
    VTOY_DEVICE_USB,
    VTOY_DEVICE_IDE,
    VTOY_DEVICE_DAC960,
    VTOY_DEVICE_CPQARRAY,
    VTOY_DEVICE_FILE,
    VTOY_DEVICE_ATARAID,
    VTOY_DEVICE_I2O,
    VTOY_DEVICE_UBD,
    VTOY_DEVICE_DASD,
    VTOY_DEVICE_VIODASD,
    VTOY_DEVICE_SX8,
    VTOY_DEVICE_DM,
    VTOY_DEVICE_XVD,
    VTOY_DEVICE_SDMMC,
    VTOY_DEVICE_VIRTBLK,
    VTOY_DEVICE_AOE,
    VTOY_DEVICE_MD,
    VTOY_DEVICE_LOOP,
    VTOY_DEVICE_NVME,
    VTOY_DEVICE_RAM,
    VTOY_DEVICE_PMEM,
    
    VTOY_DEVICE_END
}ventoy_dev_type;

/* from <linux/major.h> */
#define IDE0_MAJOR              3
#define IDE1_MAJOR              22
#define IDE2_MAJOR              33
#define IDE3_MAJOR              34
#define IDE4_MAJOR              56
#define IDE5_MAJOR              57
#define SCSI_CDROM_MAJOR        11
#define SCSI_DISK0_MAJOR        8
#define SCSI_DISK1_MAJOR        65
#define SCSI_DISK2_MAJOR        66
#define SCSI_DISK3_MAJOR        67
#define SCSI_DISK4_MAJOR        68
#define SCSI_DISK5_MAJOR        69
#define SCSI_DISK6_MAJOR        70
#define SCSI_DISK7_MAJOR        71
#define SCSI_DISK8_MAJOR        128
#define SCSI_DISK9_MAJOR        129
#define SCSI_DISK10_MAJOR       130
#define SCSI_DISK11_MAJOR       131
#define SCSI_DISK12_MAJOR       132
#define SCSI_DISK13_MAJOR       133
#define SCSI_DISK14_MAJOR       134
#define SCSI_DISK15_MAJOR       135
#define COMPAQ_SMART2_MAJOR     72
#define COMPAQ_SMART2_MAJOR1    73
#define COMPAQ_SMART2_MAJOR2    74
#define COMPAQ_SMART2_MAJOR3    75
#define COMPAQ_SMART2_MAJOR4    76
#define COMPAQ_SMART2_MAJOR5    77
#define COMPAQ_SMART2_MAJOR6    78
#define COMPAQ_SMART2_MAJOR7    79
#define COMPAQ_SMART_MAJOR      104
#define COMPAQ_SMART_MAJOR1     105
#define COMPAQ_SMART_MAJOR2     106
#define COMPAQ_SMART_MAJOR3     107
#define COMPAQ_SMART_MAJOR4     108
#define COMPAQ_SMART_MAJOR5     109
#define COMPAQ_SMART_MAJOR6     110
#define COMPAQ_SMART_MAJOR7     111
#define DAC960_MAJOR            48
#define ATARAID_MAJOR           114
#define I2O_MAJOR1              80
#define I2O_MAJOR2              81
#define I2O_MAJOR3              82
#define I2O_MAJOR4              83
#define I2O_MAJOR5              84
#define I2O_MAJOR6              85
#define I2O_MAJOR7              86
#define I2O_MAJOR8              87
#define UBD_MAJOR               98
#define DASD_MAJOR              94
#define VIODASD_MAJOR           112
#define AOE_MAJOR               152
#define SX8_MAJOR1              160
#define SX8_MAJOR2              161
#define XVD_MAJOR               202
#define SDMMC_MAJOR             179
#define LOOP_MAJOR              7
#define MD_MAJOR                9
#define BLKEXT_MAJOR            259
#define RAM_MAJOR               1

#define SCSI_BLK_MAJOR(M) (                                             \
                (M) == SCSI_DISK0_MAJOR                                 \
                || (M) == SCSI_CDROM_MAJOR                              \
                || ((M) >= SCSI_DISK1_MAJOR && (M) <= SCSI_DISK7_MAJOR) \
                || ((M) >= SCSI_DISK8_MAJOR && (M) <= SCSI_DISK15_MAJOR))

#define IDE_BLK_MAJOR(M) \
    ((M) == IDE0_MAJOR || \
    (M) == IDE1_MAJOR || \
    (M) == IDE2_MAJOR || \
    (M) == IDE3_MAJOR || \
    (M) == IDE4_MAJOR || \
    (M) == IDE5_MAJOR)

#define SX8_BLK_MAJOR(M) ((M) >= SX8_MAJOR1 && (M) <= SX8_MAJOR2)
#define I2O_BLK_MAJOR(M) ((M) >= I2O_MAJOR1 && (M) <= I2O_MAJOR8)
#define CPQARRAY_BLK_MAJOR(M)  \
    (((M) >= COMPAQ_SMART2_MAJOR && (M) <= COMPAQ_SMART2_MAJOR7) || \
    (COMPAQ_SMART_MAJOR <= (M) && (M) <= COMPAQ_SMART_MAJOR7))

#endif

int ventoy_disk_init(void);
void ventoy_disk_exit(void);
int ventoy_get_disk_info(char **argv);
const ventoy_disk * ventoy_get_disk_list(int *num);
const ventoy_disk * ventoy_get_disk_node(int id);
int CheckRuntimeEnvironment(char Letter, ventoy_disk *disk);

#endif /* __VENTOY_DISK_H__ */

