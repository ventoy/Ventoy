/*
	mount.c (22.10.09)
	exFAT file system implementation library.

	Free exFAT implementation.
	Copyright (C) 2010-2018  Andrew Nayenko

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

#include "exfat.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>

static uint64_t rootdir_size(const struct exfat* ef)
{
	uint32_t clusters = 0;
	uint32_t clusters_max = le32_to_cpu(ef->sb->cluster_count);
	cluster_t rootdir_cluster = le32_to_cpu(ef->sb->rootdir_cluster);

	/* Iterate all clusters of the root directory to calculate its size.
	   It can't be contiguous because there is no flag to indicate this. */
	do
	{
		if (clusters == clusters_max) /* infinite loop detected */
		{
			exfat_error("root directory cannot occupy all %d clusters",
					clusters);
			return 0;
		}
		if (CLUSTER_INVALID(*ef->sb, rootdir_cluster))
		{
			exfat_error("bad cluster %#x while reading root directory",
					rootdir_cluster);
			return 0;
		}
		rootdir_cluster = exfat_next_cluster(ef, ef->root, rootdir_cluster);
		clusters++;
	}
	while (rootdir_cluster != EXFAT_CLUSTER_END);

	return (uint64_t) clusters * CLUSTER_SIZE(*ef->sb);
}

static const char* get_option(const char* options, const char* option_name)
{
	const char* p;
	size_t length = strlen(option_name);

	for (p = strstr(options, option_name); p; p = strstr(p + 1, option_name))
		if ((p == options || p[-1] == ',') && p[length] == '=')
			return p + length + 1;
	return NULL;
}

static int get_int_option(const char* options, const char* option_name,
		int base, int default_value)
{
	const char* p = get_option(options, option_name);

	if (p == NULL)
		return default_value;
	return strtol(p, NULL, base);
}

static bool match_option(const char* options, const char* option_name)
{
	const char* p;
	size_t length = strlen(option_name);

	for (p = strstr(options, option_name); p; p = strstr(p + 1, option_name))
		if ((p == options || p[-1] == ',') &&
				(p[length] == ',' || p[length] == '\0'))
			return true;
	return false;
}

static void parse_options(struct exfat* ef, const char* options)
{
	int opt_umask;

	opt_umask = get_int_option(options, "umask", 8, 0);
	ef->dmask = get_int_option(options, "dmask", 8, opt_umask);
	ef->fmask = get_int_option(options, "fmask", 8, opt_umask);

	ef->uid = get_int_option(options, "uid", 10, geteuid());
	ef->gid = get_int_option(options, "gid", 10, getegid());

	ef->noatime = match_option(options, "noatime");

	switch (get_int_option(options, "repair", 10, 0))
	{
	case 1:
		ef->repair = EXFAT_REPAIR_ASK;
		break;
	case 2:
		ef->repair = EXFAT_REPAIR_YES;
		break;
	default:
		ef->repair = EXFAT_REPAIR_NO;
		break;
	}
}

static bool verify_vbr_checksum(const struct exfat* ef, void* sector)
{
	off_t sector_size = SECTOR_SIZE(*ef->sb);
	uint32_t vbr_checksum;
	int i;

	if (exfat_pread(ef->dev, sector, sector_size, 0) < 0)
	{
		exfat_error("failed to read boot sector");
		return false;
	}
	vbr_checksum = exfat_vbr_start_checksum(sector, sector_size);
	for (i = 1; i < 11; i++)
	{
		if (exfat_pread(ef->dev, sector, sector_size, i * sector_size) < 0)
		{
			exfat_error("failed to read VBR sector");
			return false;
		}
		vbr_checksum = exfat_vbr_add_checksum(sector, sector_size,
				vbr_checksum);
	}
	if (exfat_pread(ef->dev, sector, sector_size, i * sector_size) < 0)
	{
		exfat_error("failed to read VBR checksum sector");
		return false;
	}
	for (i = 0; i < sector_size / sizeof(vbr_checksum); i++)
		if (le32_to_cpu(((const le32_t*) sector)[i]) != vbr_checksum)
		{
			exfat_error("invalid VBR checksum 0x%x (expected 0x%x)",
					le32_to_cpu(((const le32_t*) sector)[i]), vbr_checksum);
			if (!EXFAT_REPAIR(invalid_vbr_checksum, ef, sector, vbr_checksum))
				return false;
		}
	return true;
}

static int commit_super_block(const struct exfat* ef)
{
	if (exfat_pwrite(ef->dev, ef->sb, sizeof(struct exfat_super_block), 0) < 0)
	{
		exfat_error("failed to write super block");
		return 1;
	}
	return exfat_fsync(ef->dev);
}

static int prepare_super_block(const struct exfat* ef)
{
	if (le16_to_cpu(ef->sb->volume_state) & EXFAT_STATE_MOUNTED)
		exfat_warn("volume was not unmounted cleanly");

	if (ef->ro)
		return 0;

	ef->sb->volume_state = cpu_to_le16(
			le16_to_cpu(ef->sb->volume_state) | EXFAT_STATE_MOUNTED);
	return commit_super_block(ef);
}

static void exfat_free(struct exfat* ef)
{
	exfat_close(ef->dev);	/* first of all, close the descriptor */
	ef->dev = NULL;			/* struct exfat_dev is freed by exfat_close() */
	free(ef->root);
	ef->root = NULL;
	free(ef->zero_cluster);
	ef->zero_cluster = NULL;
	free(ef->cmap.chunk);
	ef->cmap.chunk = NULL;
	free(ef->upcase);
	ef->upcase = NULL;
	free(ef->sb);
	ef->sb = NULL;
}

int exfat_mount(struct exfat* ef, const char* spec, const char* options)
{
	int rc;
	enum exfat_mode mode;

	exfat_tzset();
	memset(ef, 0, sizeof(struct exfat));

	parse_options(ef, options);

	if (match_option(options, "ro"))
		mode = EXFAT_MODE_RO;
	else if (match_option(options, "ro_fallback"))
		mode = EXFAT_MODE_ANY;
	else
		mode = EXFAT_MODE_RW;
	ef->dev = exfat_open(spec, mode);
	if (ef->dev == NULL)
		return -EIO;
	if (exfat_get_mode(ef->dev) == EXFAT_MODE_RO)
	{
		if (mode == EXFAT_MODE_ANY)
			ef->ro = -1;
		else
			ef->ro = 1;
	}

	ef->sb = malloc(sizeof(struct exfat_super_block));
	if (ef->sb == NULL)
	{
		exfat_error("failed to allocate memory for the super block");
		exfat_free(ef);
		return -ENOMEM;
	}
	memset(ef->sb, 0, sizeof(struct exfat_super_block));

	if (exfat_pread(ef->dev, ef->sb, sizeof(struct exfat_super_block), 0) < 0)
	{
		exfat_error("failed to read boot sector");
		exfat_free(ef);
		return -EIO;
	}
	if (memcmp(ef->sb->oem_name, "EXFAT   ", 8) != 0)
	{
		exfat_error("exFAT file system is not found");
		exfat_free(ef);
		return -EIO;
	}
	/* sector cannot be smaller than 512 bytes */
	if (ef->sb->sector_bits < 9)
	{
		exfat_error("too small sector size: 2^%hhd", ef->sb->sector_bits);
		exfat_free(ef);
		return -EIO;
	}
	/* officially exFAT supports cluster size up to 32 MB */
	if ((int) ef->sb->sector_bits + (int) ef->sb->spc_bits > 25)
	{
		exfat_error("too big cluster size: 2^(%hhd+%hhd)",
				ef->sb->sector_bits, ef->sb->spc_bits);
		exfat_free(ef);
		return -EIO;
	}
	ef->zero_cluster = malloc(CLUSTER_SIZE(*ef->sb));
	if (ef->zero_cluster == NULL)
	{
		exfat_error("failed to allocate zero sector");
		exfat_free(ef);
		return -ENOMEM;
	}
	/* use zero_cluster as a temporary buffer for VBR checksum verification */
	if (!verify_vbr_checksum(ef, ef->zero_cluster))
	{
		exfat_free(ef);
		return -EIO;
	}
	memset(ef->zero_cluster, 0, CLUSTER_SIZE(*ef->sb));
	if (ef->sb->version.major != 1 || ef->sb->version.minor != 0)
	{
		exfat_error("unsupported exFAT version: %hhu.%hhu",
				ef->sb->version.major, ef->sb->version.minor);
		exfat_free(ef);
		return -EIO;
	}
	if (ef->sb->fat_count != 1)
	{
		exfat_error("unsupported FAT count: %hhu", ef->sb->fat_count);
		exfat_free(ef);
		return -EIO;
	}
	if (le64_to_cpu(ef->sb->sector_count) * SECTOR_SIZE(*ef->sb) >
			exfat_get_size(ef->dev))
	{
		/* this can cause I/O errors later but we don't fail mounting to let
		   user rescue data */
		exfat_warn("file system in sectors is larger than device: "
				"%"PRIu64" * %d > %"PRIu64,
				le64_to_cpu(ef->sb->sector_count), SECTOR_SIZE(*ef->sb),
				exfat_get_size(ef->dev));
	}
	if ((off_t) le32_to_cpu(ef->sb->cluster_count) * CLUSTER_SIZE(*ef->sb) >
			exfat_get_size(ef->dev))
	{
		exfat_error("file system in clusters is larger than device: "
				"%u * %d > %"PRIu64,
				le32_to_cpu(ef->sb->cluster_count), CLUSTER_SIZE(*ef->sb),
				exfat_get_size(ef->dev));
		exfat_free(ef);
		return -EIO;
	}

	ef->root = malloc(sizeof(struct exfat_node));
	if (ef->root == NULL)
	{
		exfat_error("failed to allocate root node");
		exfat_free(ef);
		return -ENOMEM;
	}
	memset(ef->root, 0, sizeof(struct exfat_node));
	ef->root->attrib = EXFAT_ATTRIB_DIR;
	ef->root->start_cluster = le32_to_cpu(ef->sb->rootdir_cluster);
	ef->root->fptr_cluster = ef->root->start_cluster;
	ef->root->name[0] = cpu_to_le16('\0');
	ef->root->size = rootdir_size(ef);
	if (ef->root->size == 0)
	{
		exfat_free(ef);
		return -EIO;
	}
	/* exFAT does not have time attributes for the root directory */
	ef->root->mtime = 0;
	ef->root->atime = 0;
	/* always keep at least 1 reference to the root node */
	exfat_get_node(ef->root);

	rc = exfat_cache_directory(ef, ef->root);
	if (rc != 0)
		goto error;
	if (ef->upcase == NULL)
	{
		exfat_error("upcase table is not found");
		goto error;
	}
	if (ef->cmap.chunk == NULL)
	{
		exfat_error("clusters bitmap is not found");
		goto error;
	}

	if (prepare_super_block(ef) != 0)
		goto error;

	return 0;

error:
	exfat_put_node(ef, ef->root);
	exfat_reset_cache(ef);
	exfat_free(ef);
	return -EIO;
}

static void finalize_super_block(struct exfat* ef)
{
	if (ef->ro)
		return;

	ef->sb->volume_state = cpu_to_le16(
			le16_to_cpu(ef->sb->volume_state) & ~EXFAT_STATE_MOUNTED);

	/* Some implementations set the percentage of allocated space to 0xff
	   on FS creation and never update it. In this case leave it as is. */
	if (ef->sb->allocated_percent != 0xff)
	{
		uint32_t free, total;

		free = exfat_count_free_clusters(ef);
		total = le32_to_cpu(ef->sb->cluster_count);
		ef->sb->allocated_percent = ((total - free) * 100 + total / 2) / total;
	}

	commit_super_block(ef);	/* ignore return code */
}

void exfat_unmount(struct exfat* ef)
{
	exfat_flush_nodes(ef);	/* ignore return code */
	exfat_flush(ef);		/* ignore return code */
	exfat_put_node(ef, ef->root);
	exfat_reset_cache(ef);
	finalize_super_block(ef);
	exfat_free(ef);			/* will close the descriptor */
}
