/*
	mkexfat.h (09.11.10)
	FS creation engine.

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

#ifndef MKFS_MKEXFAT_H_INCLUDED
#define MKFS_MKEXFAT_H_INCLUDED

#include "exfat.h"

struct fs_object
{
	off_t (*get_alignment)(void);
	off_t (*get_size)(void);
	int (*write)(struct exfat_dev* dev);
};

extern const struct fs_object* objects[];

int get_sector_bits(void);
int get_spc_bits(void);
off_t get_volume_size(void);
const le16_t* get_volume_label(void);
uint32_t get_volume_serial(void);
uint64_t get_first_sector(void);
int get_sector_size(void);
int get_cluster_size(void);

int mkfs(struct exfat_dev* dev, off_t volume_size);
off_t get_position(const struct fs_object* object);

#endif /* ifndef MKFS_MKEXFAT_H_INCLUDED */
