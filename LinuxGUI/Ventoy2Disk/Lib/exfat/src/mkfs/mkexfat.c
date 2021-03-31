/*
	mkexfat.c (22.04.12)
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

#include "mkexfat.h"
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static int check_size(off_t volume_size)
{
	const struct fs_object** pp;
	off_t position = 0;

	for (pp = objects; *pp; pp++)
	{
		position = ROUND_UP(position, (*pp)->get_alignment());
		position += (*pp)->get_size();
	}

	if (position > volume_size)
	{
		struct exfat_human_bytes vhb;

		exfat_humanize_bytes(volume_size, &vhb);
		exfat_error("too small device (%"PRIu64" %s)", vhb.value, vhb.unit);
		return 1;
	}

	return 0;

}

static int erase_object(struct exfat_dev* dev, const void* block,
		size_t block_size, off_t start, off_t size)
{
	const off_t block_count = DIV_ROUND_UP(size, block_size);
	off_t i;

	if (exfat_seek(dev, start, SEEK_SET) == (off_t) -1)
	{
		exfat_error("seek to 0x%"PRIx64" failed", start);
		return 1;
	}
	for (i = 0; i < size; i += block_size)
	{
		if (exfat_write(dev, block, MIN(size - i, block_size)) < 0)
		{
			exfat_error("failed to erase block %"PRIu64"/%"PRIu64
					" at 0x%"PRIx64, i + 1, block_count, start);
			return 1;
		}
	}
	return 0;
}

static int erase(struct exfat_dev* dev)
{
	const struct fs_object** pp;
	off_t position = 0;
	const size_t block_size = 1024 * 1024;
	void* block = malloc(block_size);

	if (block == NULL)
	{
		exfat_error("failed to allocate erase block of %zu bytes", block_size);
		return 1;
	}
	memset(block, 0, block_size);

	for (pp = objects; *pp; pp++)
	{
		position = ROUND_UP(position, (*pp)->get_alignment());
		if (erase_object(dev, block, block_size, position,
				(*pp)->get_size()) != 0)
		{
			free(block);
			return 1;
		}
		position += (*pp)->get_size();
	}

	free(block);
	return 0;
}

static int create(struct exfat_dev* dev)
{
	const struct fs_object** pp;
	off_t position = 0;

	for (pp = objects; *pp; pp++)
	{
		position = ROUND_UP(position, (*pp)->get_alignment());
		if (exfat_seek(dev, position, SEEK_SET) == (off_t) -1)
		{
			exfat_error("seek to 0x%"PRIx64" failed", position);
			return 1;
		}
		if ((*pp)->write(dev) != 0)
			return 1;
		position += (*pp)->get_size();
	}
	return 0;
}

int mkfs(struct exfat_dev* dev, off_t volume_size)
{
	if (check_size(volume_size) != 0)
		return 1;

    exfat_debug("Creating... ");
	//fputs("Creating... ", stdout);
	//fflush(stdout);
	if (erase(dev) != 0)
		return 1;
	if (create(dev) != 0)
		return 1;
	//puts("done.");

	//fputs("Flushing... ", stdout);
	//fflush(stdout);
    exfat_debug("Flushing... ");
	if (exfat_fsync(dev) != 0)
		return 1;
	//puts("done.");

	return 0;
}

off_t get_position(const struct fs_object* object)
{
	const struct fs_object** pp;
	off_t position = 0;

	for (pp = objects; *pp; pp++)
	{
		position = ROUND_UP(position, (*pp)->get_alignment());
		if (*pp == object)
			return position;
		position += (*pp)->get_size();
	}
	exfat_bug("unknown object");

    return 0;
}

