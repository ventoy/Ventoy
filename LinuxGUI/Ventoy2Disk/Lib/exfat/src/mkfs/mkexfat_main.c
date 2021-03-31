/*
	main.c (15.08.10)
	Creates exFAT file system.

	Free exFAT implementation.
	Copyright (C) 2011-2018  Andrew Nayenko

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "mkexfat.h"
#include "vbr.h"
#include "fat.h"
#include "cbm.h"
#include "uct.h"
#include "rootdir.h"
#include "exfat.h"
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

const struct fs_object* objects[] =
{
	&vbr,
	&vbr,
	&fat,
	/* clusters heap */
	&cbm,
	&uct,
	&rootdir,
	NULL,
};

static struct
{
	int sector_bits;
	int spc_bits;
	off_t volume_size;
	le16_t volume_label[EXFAT_ENAME_MAX + 1];
	uint32_t volume_serial;
	uint64_t first_sector;
}
param;

extern int g_vtoy_exfat_disk_fd;
extern uint64_t g_vtoy_exfat_part_size;

int get_sector_bits(void)
{
	return param.sector_bits;
}

int get_spc_bits(void)
{
	return param.spc_bits;
}

off_t get_volume_size(void)
{
	return param.volume_size;
}

const le16_t* get_volume_label(void)
{
	return param.volume_label;
}

uint32_t get_volume_serial(void)
{
	return param.volume_serial;
}

uint64_t get_first_sector(void)
{
	return param.first_sector;
}

int get_sector_size(void)
{
	return 1 << get_sector_bits();
}

int get_cluster_size(void)
{
	return get_sector_size() << get_spc_bits();
}

static int setup_spc_bits(int sector_bits, int user_defined, off_t volume_size)
{
	int i;

	if (user_defined != -1)
	{
		off_t cluster_size = 1 << sector_bits << user_defined;
		if (volume_size / cluster_size > EXFAT_LAST_DATA_CLUSTER)
		{
			struct exfat_human_bytes chb, vhb;

			exfat_humanize_bytes(cluster_size, &chb);
			exfat_humanize_bytes(volume_size, &vhb);
			exfat_error("cluster size %"PRIu64" %s is too small for "
					"%"PRIu64" %s volume, try -s %d",
					chb.value, chb.unit,
					vhb.value, vhb.unit,
					1 << setup_spc_bits(sector_bits, -1, volume_size));
			return -1;
		}
		return user_defined;
	}

	if (volume_size < 256ull * 1024 * 1024)
		return MAX(0, 12 - sector_bits);	/* 4 KB */
	if (volume_size < 32ull * 1024 * 1024 * 1024)
		return MAX(0, 15 - sector_bits);	/* 32 KB */

	for (i = 17; ; i++)						/* 128 KB or more */
		if (DIV_ROUND_UP(volume_size, 1 << i) <= EXFAT_LAST_DATA_CLUSTER)
			return MAX(0, i - sector_bits);
}

static int setup_volume_label(le16_t label[EXFAT_ENAME_MAX + 1], const char* s)
{
	memset(label, 0, (EXFAT_ENAME_MAX + 1) * sizeof(le16_t));
	if (s == NULL)
		return 0;
	return utf8_to_utf16(label, s, EXFAT_ENAME_MAX + 1, strlen(s));
}

static uint32_t setup_volume_serial(uint32_t user_defined)
{
	struct timeval now;

	if (user_defined != 0)
		return user_defined;

	if (gettimeofday(&now, NULL) != 0)
	{
		exfat_error("failed to form volume id");
		return 0;
	}
	return (now.tv_sec << 20) | now.tv_usec;
}

static int setup(struct exfat_dev* dev, int sector_bits, int spc_bits,
		const char* volume_label, uint32_t volume_serial,
		uint64_t first_sector)
{
	param.sector_bits = sector_bits;
	param.first_sector = first_sector;
	param.volume_size = exfat_get_size(dev);

	param.spc_bits = setup_spc_bits(sector_bits, spc_bits, param.volume_size);
	if (param.spc_bits == -1)
		return 1;

	if (setup_volume_label(param.volume_label, volume_label) != 0)
		return 1;

	param.volume_serial = setup_volume_serial(volume_serial);
	if (param.volume_serial == 0)
		return 1;

	return mkfs(dev, param.volume_size);
}

static int logarithm2(int n)
{
	int i;

	for (i = 0; i < sizeof(int) * CHAR_BIT - 1; i++)
		if ((1 << i) == n)
			return i;
	return -1;
}

static void usage(const char* prog)
{
	fprintf(stderr, "Usage: %s [-i volume-id] [-n label] "
			"[-p partition-first-sector] "
			"[-s sectors-per-cluster] [-V] <device>\n", prog);
	exit(1);
}

int mkexfat_main(const char *devpath, int fd, uint64_t part_sector_count)
{
	int spc_bits = -1;
	uint32_t volume_serial = 0;
	uint64_t first_sector = 0;
	struct exfat_dev* dev;

#if 0
	while ((opt = getopt(argc, argv, "i:n:p:s:V")) != -1)
	{
		switch (opt)
		{
		case 'i':
			volume_serial = strtol(optarg, NULL, 16);
			break;
		case 'n':
			volume_label = optarg;
			break;
		case 'p':
			first_sector = strtoll(optarg, NULL, 10);
			break;
		case 's':
			spc_bits = logarithm2(atoi(optarg));
			if (spc_bits < 0)
			{
				exfat_error("invalid option value: '%s'", optarg);
				return 1;
			}
			break;
		case 'V':
			puts("Copyright (C) 2011-2018  Andrew Nayenko");
			return 0;
		default:
			usage(argv[0]);
			break;
		}
	}
#endif /* #if 0 */

    /* 
     * DiskSize > 32GB  Cluster Size use 128KB
     * DiskSize < 32GB  Cluster Size use 32KB 
     */
    if ((part_sector_count / 2097152) > 32)
    {
        spc_bits = logarithm2(256);
    }
    else
    {
        spc_bits = logarithm2(64);
    }

    g_vtoy_exfat_disk_fd = fd;
    g_vtoy_exfat_part_size = part_sector_count * 512;
    
	dev = exfat_open(devpath, EXFAT_MODE_RW);
	if (dev == NULL)
		return 1;
	if (setup(dev, 9, spc_bits, "Ventoy", volume_serial, first_sector) != 0)
	{
		exfat_close(dev);
		return 1;
	}
	if (exfat_close(dev) != 0)
		return 1;
    
	return 0;
}

