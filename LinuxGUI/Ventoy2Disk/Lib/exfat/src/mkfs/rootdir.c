/*
	rootdir.c (09.11.10)
	Root directory creation code.

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

#include "rootdir.h"
#include "uct.h"
#include "cbm.h"
#include "uctc.h"
#include <string.h>

static off_t rootdir_alignment(void)
{
	return get_cluster_size();
}

static off_t rootdir_size(void)
{
	return get_cluster_size();
}

static void init_label_entry(struct exfat_entry_label* label_entry)
{
	memset(label_entry, 0, sizeof(struct exfat_entry_label));
	label_entry->type = EXFAT_ENTRY_LABEL ^ EXFAT_ENTRY_VALID;

	if (utf16_length(get_volume_label()) == 0)
		return;

	memcpy(label_entry->name, get_volume_label(),
			EXFAT_ENAME_MAX * sizeof(le16_t));
	label_entry->length = utf16_length(get_volume_label());
	label_entry->type |= EXFAT_ENTRY_VALID;
}

static void init_bitmap_entry(struct exfat_entry_bitmap* bitmap_entry)
{
	memset(bitmap_entry, 0, sizeof(struct exfat_entry_bitmap));
	bitmap_entry->type = EXFAT_ENTRY_BITMAP;
	bitmap_entry->start_cluster = cpu_to_le32(EXFAT_FIRST_DATA_CLUSTER);
	bitmap_entry->size = cpu_to_le64(cbm.get_size());
}

static void init_upcase_entry(struct exfat_entry_upcase* upcase_entry)
{
	size_t i;
	uint32_t sum = 0;

	for (i = 0; i < sizeof(upcase_table); i++)
		sum = ((sum << 31) | (sum >> 1)) + upcase_table[i];

	memset(upcase_entry, 0, sizeof(struct exfat_entry_upcase));
	upcase_entry->type = EXFAT_ENTRY_UPCASE;
	upcase_entry->checksum = cpu_to_le32(sum);
	upcase_entry->start_cluster = cpu_to_le32(
			(get_position(&uct) - get_position(&cbm)) / get_cluster_size() +
			EXFAT_FIRST_DATA_CLUSTER);
	upcase_entry->size = cpu_to_le64(sizeof(upcase_table));
}

static int rootdir_write(struct exfat_dev* dev)
{
	struct exfat_entry_label label_entry;
	struct exfat_entry_bitmap bitmap_entry;
	struct exfat_entry_upcase upcase_entry;

	init_label_entry(&label_entry);
	init_bitmap_entry(&bitmap_entry);
	init_upcase_entry(&upcase_entry);

	if (exfat_write(dev, &label_entry, sizeof(struct exfat_entry)) < 0)
		return 1;
	if (exfat_write(dev, &bitmap_entry, sizeof(struct exfat_entry)) < 0)
		return 1;
	if (exfat_write(dev, &upcase_entry, sizeof(struct exfat_entry)) < 0)
		return 1;
	return 0;
}

const struct fs_object rootdir =
{
	.get_alignment = rootdir_alignment,
	.get_size = rootdir_size,
	.write = rootdir_write,
};
