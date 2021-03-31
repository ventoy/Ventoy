/*
	fat.c (09.11.10)
	File Allocation Table creation code.

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

#include "fat.h"
#include "cbm.h"
#include "uct.h"
#include "rootdir.h"
#include <unistd.h>

static off_t fat_alignment(void)
{
	return (off_t) 128 * get_sector_size();
}

static off_t fat_size(void)
{
	return get_volume_size() / get_cluster_size() * sizeof(cluster_t);
}

static cluster_t fat_write_entry(struct exfat_dev* dev, cluster_t cluster,
		cluster_t value)
{
	le32_t fat_entry = cpu_to_le32(value);
	if (exfat_write(dev, &fat_entry, sizeof(fat_entry)) < 0)
	{
		exfat_error("failed to write FAT entry 0x%x", value);
		return 0;
	}
	return cluster + 1;
}

static cluster_t fat_write_entries(struct exfat_dev* dev, cluster_t cluster,
		uint64_t length)
{
	cluster_t end = cluster + DIV_ROUND_UP(length, get_cluster_size());

	while (cluster < end - 1)
	{
		cluster = fat_write_entry(dev, cluster, cluster + 1);
		if (cluster == 0)
			return 0;
	}
	return fat_write_entry(dev, cluster, EXFAT_CLUSTER_END);
}

static int fat_write(struct exfat_dev* dev)
{
	cluster_t c = 0;

	if (!(c = fat_write_entry(dev, c, 0xfffffff8))) /* media type */
		return 1;
	if (!(c = fat_write_entry(dev, c, 0xffffffff))) /* some weird constant */
		return 1;
	if (!(c = fat_write_entries(dev, c, cbm.get_size())))
		return 1;
	if (!(c = fat_write_entries(dev, c, uct.get_size())))
		return 1;
	if (!(c = fat_write_entries(dev, c, rootdir.get_size())))
		return 1;

	return 0;
}

const struct fs_object fat =
{
	.get_alignment = fat_alignment,
	.get_size = fat_size,
	.write = fat_write,
};
