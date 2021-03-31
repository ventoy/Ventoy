/*
	vbr.c (09.11.10)
	Volume Boot Record creation code.

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

#include "vbr.h"
#include "fat.h"
#include "cbm.h"
#include "uct.h"
#include "rootdir.h"
#include <string.h>

static off_t vbr_alignment(void)
{
	return get_sector_size();
}

static off_t vbr_size(void)
{
	return 12 * get_sector_size();
}

static void init_sb(struct exfat_super_block* sb)
{
	uint32_t clusters_max;
	uint32_t fat_sectors;

	clusters_max = get_volume_size() / get_cluster_size();
	fat_sectors = DIV_ROUND_UP((off_t) clusters_max * sizeof(cluster_t),
			get_sector_size());

	memset(sb, 0, sizeof(struct exfat_super_block));
	sb->jump[0] = 0xeb;
	sb->jump[1] = 0x76;
	sb->jump[2] = 0x90;
	memcpy(sb->oem_name, "EXFAT   ", sizeof(sb->oem_name));
	sb->sector_start = cpu_to_le64(get_first_sector());
	sb->sector_count = cpu_to_le64(get_volume_size() / get_sector_size());
	sb->fat_sector_start = cpu_to_le32(
			fat.get_alignment() / get_sector_size());
	sb->fat_sector_count = cpu_to_le32(ROUND_UP(
			le32_to_cpu(sb->fat_sector_start) + fat_sectors,
				1 << get_spc_bits()) -
			le32_to_cpu(sb->fat_sector_start));
	sb->cluster_sector_start = cpu_to_le32(
			get_position(&cbm) / get_sector_size());
	sb->cluster_count = cpu_to_le32(clusters_max -
			((le32_to_cpu(sb->fat_sector_start) +
			  le32_to_cpu(sb->fat_sector_count)) >> get_spc_bits()));
	sb->rootdir_cluster = cpu_to_le32(
			(get_position(&rootdir) - get_position(&cbm)) / get_cluster_size()
			+ EXFAT_FIRST_DATA_CLUSTER);
	sb->volume_serial = cpu_to_le32(get_volume_serial());
	sb->version.major = 1;
	sb->version.minor = 0;
	sb->volume_state = cpu_to_le16(0);
	sb->sector_bits = get_sector_bits();
	sb->spc_bits = get_spc_bits();
	sb->fat_count = 1;
	sb->drive_no = 0x80;
	sb->allocated_percent = 0;
	sb->boot_signature = cpu_to_le16(0xaa55);
}

static int vbr_write(struct exfat_dev* dev)
{
	struct exfat_super_block sb;
	uint32_t checksum;
	le32_t* sector = malloc(get_sector_size());
	size_t i;

	if (sector == NULL)
	{
		exfat_error("failed to allocate sector-sized block of memory");
		return 1;
	}

	init_sb(&sb);
	if (exfat_write(dev, &sb, sizeof(struct exfat_super_block)) < 0)
	{
		free(sector);
		exfat_error("failed to write super block sector");
		return 1;
	}
	checksum = exfat_vbr_start_checksum(&sb, sizeof(struct exfat_super_block));

	memset(sector, 0, get_sector_size());
	sector[get_sector_size() / sizeof(sector[0]) - 1] =
			cpu_to_le32(0xaa550000);
	for (i = 0; i < 8; i++)
	{
		if (exfat_write(dev, sector, get_sector_size()) < 0)
		{
			free(sector);
			exfat_error("failed to write a sector with boot signature");
			return 1;
		}
		checksum = exfat_vbr_add_checksum(sector, get_sector_size(), checksum);
	}

	memset(sector, 0, get_sector_size());
	for (i = 0; i < 2; i++)
	{
		if (exfat_write(dev, sector, get_sector_size()) < 0)
		{
			free(sector);
			exfat_error("failed to write an empty sector");
			return 1;
		}
		checksum = exfat_vbr_add_checksum(sector, get_sector_size(), checksum);
	}

	for (i = 0; i < get_sector_size() / sizeof(sector[0]); i++)
		sector[i] = cpu_to_le32(checksum);
	if (exfat_write(dev, sector, get_sector_size()) < 0)
	{
		free(sector);
		exfat_error("failed to write checksum sector");
		return 1;
	}

	free(sector);
	return 0;
}

const struct fs_object vbr =
{
	.get_alignment = vbr_alignment,
	.get_size = vbr_size,
	.write = vbr_write,
};
