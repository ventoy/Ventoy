/*
	cluster.c (03.09.09)
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
#include <errno.h>
#include <string.h>
#include <inttypes.h>

/*
 * Sector to absolute offset.
 */
static off_t s2o(const struct exfat* ef, off_t sector)
{
	return sector << ef->sb->sector_bits;
}

/*
 * Cluster to sector.
 */
static off_t c2s(const struct exfat* ef, cluster_t cluster)
{
	if (cluster < EXFAT_FIRST_DATA_CLUSTER)
		exfat_bug("invalid cluster number %u", cluster);
	return le32_to_cpu(ef->sb->cluster_sector_start) +
		((off_t) (cluster - EXFAT_FIRST_DATA_CLUSTER) << ef->sb->spc_bits);
}

/*
 * Cluster to absolute offset.
 */
off_t exfat_c2o(const struct exfat* ef, cluster_t cluster)
{
	return s2o(ef, c2s(ef, cluster));
}

/*
 * Sector to cluster.
 */
static cluster_t s2c(const struct exfat* ef, off_t sector)
{
	return ((sector - le32_to_cpu(ef->sb->cluster_sector_start)) >>
			ef->sb->spc_bits) + EXFAT_FIRST_DATA_CLUSTER;
}

/*
 * Size in bytes to size in clusters (rounded upwards).
 */
static uint32_t bytes2clusters(const struct exfat* ef, uint64_t bytes)
{
	uint64_t cluster_size = CLUSTER_SIZE(*ef->sb);
	return DIV_ROUND_UP(bytes, cluster_size);
}

cluster_t exfat_next_cluster(const struct exfat* ef,
		const struct exfat_node* node, cluster_t cluster)
{
	le32_t next;
	off_t fat_offset;

	if (cluster < EXFAT_FIRST_DATA_CLUSTER)
		exfat_bug("bad cluster 0x%x", cluster);

	if (node->is_contiguous)
		return cluster + 1;
	fat_offset = s2o(ef, le32_to_cpu(ef->sb->fat_sector_start))
		+ cluster * sizeof(cluster_t);
	if (exfat_pread(ef->dev, &next, sizeof(next), fat_offset) < 0)
		return EXFAT_CLUSTER_BAD; /* the caller should handle this and print
		                             appropriate error message */
	return le32_to_cpu(next);
}

cluster_t exfat_advance_cluster(const struct exfat* ef,
		struct exfat_node* node, uint32_t count)
{
	uint32_t i;

	if (node->fptr_index > count)
	{
		node->fptr_index = 0;
		node->fptr_cluster = node->start_cluster;
	}

	for (i = node->fptr_index; i < count; i++)
	{
		node->fptr_cluster = exfat_next_cluster(ef, node, node->fptr_cluster);
		if (CLUSTER_INVALID(*ef->sb, node->fptr_cluster))
			break; /* the caller should handle this and print appropriate 
			          error message */
	}
	node->fptr_index = count;
	return node->fptr_cluster;
}

static cluster_t find_bit_and_set(bitmap_t* bitmap, size_t start, size_t end)
{
	const size_t start_index = start / sizeof(bitmap_t) / 8;
	const size_t end_index = DIV_ROUND_UP(end, sizeof(bitmap_t) * 8);
	size_t i;
	size_t start_bitindex;
	size_t end_bitindex;
	size_t c;

	for (i = start_index; i < end_index; i++)
	{
		if (bitmap[i] == ~((bitmap_t) 0))
			continue;
		start_bitindex = MAX(i * sizeof(bitmap_t) * 8, start);
		end_bitindex = MIN((i + 1) * sizeof(bitmap_t) * 8, end);
		for (c = start_bitindex; c < end_bitindex; c++)
			if (BMAP_GET(bitmap, c) == 0)
			{
				BMAP_SET(bitmap, c);
				return c + EXFAT_FIRST_DATA_CLUSTER;
			}
	}
	return EXFAT_CLUSTER_END;
}

static int flush_nodes(struct exfat* ef, struct exfat_node* node)
{
	struct exfat_node* p;

	for (p = node->child; p != NULL; p = p->next)
	{
		int rc = flush_nodes(ef, p);
		if (rc != 0)
			return rc;
	}
	return exfat_flush_node(ef, node);
}

int exfat_flush_nodes(struct exfat* ef)
{
	return flush_nodes(ef, ef->root);
}

int exfat_flush(struct exfat* ef)
{
	if (ef->cmap.dirty)
	{
		if (exfat_pwrite(ef->dev, ef->cmap.chunk,
				BMAP_SIZE(ef->cmap.chunk_size),
				exfat_c2o(ef, ef->cmap.start_cluster)) < 0)
		{
			exfat_error("failed to write clusters bitmap");
			return -EIO;
		}
		ef->cmap.dirty = false;
	}

	return 0;
}

static bool set_next_cluster(const struct exfat* ef, bool contiguous,
		cluster_t current, cluster_t next)
{
	off_t fat_offset;
	le32_t next_le32;

	if (contiguous)
		return true;
	fat_offset = s2o(ef, le32_to_cpu(ef->sb->fat_sector_start))
		+ current * sizeof(cluster_t);
	next_le32 = cpu_to_le32(next);
	if (exfat_pwrite(ef->dev, &next_le32, sizeof(next_le32), fat_offset) < 0)
	{
		exfat_error("failed to write the next cluster %#x after %#x", next,
				current);
		return false;
	}
	return true;
}

static cluster_t allocate_cluster(struct exfat* ef, cluster_t hint)
{
	cluster_t cluster;

	hint -= EXFAT_FIRST_DATA_CLUSTER;
	if (hint >= ef->cmap.chunk_size)
		hint = 0;

	cluster = find_bit_and_set(ef->cmap.chunk, hint, ef->cmap.chunk_size);
	if (cluster == EXFAT_CLUSTER_END)
		cluster = find_bit_and_set(ef->cmap.chunk, 0, hint);
	if (cluster == EXFAT_CLUSTER_END)
	{
		exfat_error("no free space left");
		return EXFAT_CLUSTER_END;
	}

	ef->cmap.dirty = true;
	return cluster;
}

static void free_cluster(struct exfat* ef, cluster_t cluster)
{
	if (cluster - EXFAT_FIRST_DATA_CLUSTER >= ef->cmap.size)
		exfat_bug("caller must check cluster validity (%#x, %#x)", cluster,
				ef->cmap.size);

	BMAP_CLR(ef->cmap.chunk, cluster - EXFAT_FIRST_DATA_CLUSTER);
	ef->cmap.dirty = true;
}

static bool make_noncontiguous(const struct exfat* ef, cluster_t first,
		cluster_t last)
{
	cluster_t c;

	for (c = first; c < last; c++)
		if (!set_next_cluster(ef, false, c, c + 1))
			return false;
	return true;
}

static int shrink_file(struct exfat* ef, struct exfat_node* node,
		uint32_t current, uint32_t difference);

static int grow_file(struct exfat* ef, struct exfat_node* node,
		uint32_t current, uint32_t difference)
{
	cluster_t previous;
	cluster_t next;
	uint32_t allocated = 0;

	if (difference == 0)
		exfat_bug("zero clusters count passed");

	if (node->start_cluster != EXFAT_CLUSTER_FREE)
	{
		/* get the last cluster of the file */
		previous = exfat_advance_cluster(ef, node, current - 1);
		if (CLUSTER_INVALID(*ef->sb, previous))
		{
			exfat_error("invalid cluster 0x%x while growing", previous);
			return -EIO;
		}
	}
	else
	{
		if (node->fptr_index != 0)
			exfat_bug("non-zero pointer index (%u)", node->fptr_index);
		/* file does not have clusters (i.e. is empty), allocate
		   the first one for it */
		previous = allocate_cluster(ef, 0);
		if (CLUSTER_INVALID(*ef->sb, previous))
			return -ENOSPC;
		node->fptr_cluster = node->start_cluster = previous;
		allocated = 1;
		/* file consists of only one cluster, so it's contiguous */
		node->is_contiguous = true;
	}

	while (allocated < difference)
	{
		next = allocate_cluster(ef, previous + 1);
		if (CLUSTER_INVALID(*ef->sb, next))
		{
			if (allocated != 0)
				shrink_file(ef, node, current + allocated, allocated);
			return -ENOSPC;
		}
		if (next != previous - 1 && node->is_contiguous)
		{
			/* it's a pity, but we are not able to keep the file contiguous
			   anymore */
			if (!make_noncontiguous(ef, node->start_cluster, previous))
				return -EIO;
			node->is_contiguous = false;
			node->is_dirty = true;
		}
		if (!set_next_cluster(ef, node->is_contiguous, previous, next))
			return -EIO;
		previous = next;
		allocated++;
	}

	if (!set_next_cluster(ef, node->is_contiguous, previous,
			EXFAT_CLUSTER_END))
		return -EIO;
	return 0;
}

static int shrink_file(struct exfat* ef, struct exfat_node* node,
		uint32_t current, uint32_t difference)
{
	cluster_t previous;
	cluster_t next;

	if (difference == 0)
		exfat_bug("zero difference passed");
	if (node->start_cluster == EXFAT_CLUSTER_FREE)
		exfat_bug("unable to shrink empty file (%u clusters)", current);
	if (current < difference)
		exfat_bug("file underflow (%u < %u)", current, difference);

	/* crop the file */
	if (current > difference)
	{
		cluster_t last = exfat_advance_cluster(ef, node,
				current - difference - 1);
		if (CLUSTER_INVALID(*ef->sb, last))
		{
			exfat_error("invalid cluster 0x%x while shrinking", last);
			return -EIO;
		}
		previous = exfat_next_cluster(ef, node, last);
		if (!set_next_cluster(ef, node->is_contiguous, last,
				EXFAT_CLUSTER_END))
			return -EIO;
	}
	else
	{
		previous = node->start_cluster;
		node->start_cluster = EXFAT_CLUSTER_FREE;
		node->is_dirty = true;
	}
	node->fptr_index = 0;
	node->fptr_cluster = node->start_cluster;

	/* free remaining clusters */
	while (difference--)
	{
		if (CLUSTER_INVALID(*ef->sb, previous))
		{
			exfat_error("invalid cluster 0x%x while freeing after shrink",
					previous);
			return -EIO;
		}

		next = exfat_next_cluster(ef, node, previous);
		if (!set_next_cluster(ef, node->is_contiguous, previous,
				EXFAT_CLUSTER_FREE))
			return -EIO;
		free_cluster(ef, previous);
		previous = next;
	}
	return 0;
}

static bool erase_raw(struct exfat* ef, size_t size, off_t offset)
{
	if (exfat_pwrite(ef->dev, ef->zero_cluster, size, offset) < 0)
	{
		exfat_error("failed to erase %zu bytes at %"PRId64, size, offset);
		return false;
	}
	return true;
}

static int erase_range(struct exfat* ef, struct exfat_node* node,
		uint64_t begin, uint64_t end)
{
	uint64_t cluster_boundary;
	cluster_t cluster;

	if (begin >= end)
		return 0;

	cluster_boundary = (begin | (CLUSTER_SIZE(*ef->sb) - 1)) + 1;
	cluster = exfat_advance_cluster(ef, node,
			begin / CLUSTER_SIZE(*ef->sb));
	if (CLUSTER_INVALID(*ef->sb, cluster))
	{
		exfat_error("invalid cluster 0x%x while erasing", cluster);
		return -EIO;
	}
	/* erase from the beginning to the closest cluster boundary */
	if (!erase_raw(ef, MIN(cluster_boundary, end) - begin,
			exfat_c2o(ef, cluster) + begin % CLUSTER_SIZE(*ef->sb)))
		return -EIO;
	/* erase whole clusters */
	while (cluster_boundary < end)
	{
		cluster = exfat_next_cluster(ef, node, cluster);
		/* the cluster cannot be invalid because we have just allocated it */
		if (CLUSTER_INVALID(*ef->sb, cluster))
			exfat_bug("invalid cluster 0x%x after allocation", cluster);
		if (!erase_raw(ef, CLUSTER_SIZE(*ef->sb), exfat_c2o(ef, cluster)))
			return -EIO;
		cluster_boundary += CLUSTER_SIZE(*ef->sb);
	}
	return 0;
}

int exfat_truncate(struct exfat* ef, struct exfat_node* node, uint64_t size,
		bool erase)
{
	uint32_t c1 = bytes2clusters(ef, node->size);
	uint32_t c2 = bytes2clusters(ef, size);
	int rc = 0;

	if (node->references == 0 && node->parent)
		exfat_bug("no references, node changes can be lost");

	if (node->size == size)
		return 0;

	if (c1 < c2)
		rc = grow_file(ef, node, c1, c2 - c1);
	else if (c1 > c2)
		rc = shrink_file(ef, node, c1, c1 - c2);

	if (rc != 0)
		return rc;

	if (erase)
	{
		rc = erase_range(ef, node, node->size, size);
		if (rc != 0)
			return rc;
	}

	exfat_update_mtime(node);
	node->size = size;
	node->is_dirty = true;
	return 0;
}

uint32_t exfat_count_free_clusters(const struct exfat* ef)
{
	uint32_t free_clusters = 0;
	uint32_t i;

	for (i = 0; i < ef->cmap.size; i++)
		if (BMAP_GET(ef->cmap.chunk, i) == 0)
			free_clusters++;
	return free_clusters;
}

static int find_used_clusters(const struct exfat* ef,
		cluster_t* a, cluster_t* b)
{
	const cluster_t end = le32_to_cpu(ef->sb->cluster_count);

	/* find first used cluster */
	for (*a = *b + 1; *a < end; (*a)++)
		if (BMAP_GET(ef->cmap.chunk, *a - EXFAT_FIRST_DATA_CLUSTER))
			break;
	if (*a >= end)
		return 1;

	/* find last contiguous used cluster */
	for (*b = *a; *b < end; (*b)++)
		if (BMAP_GET(ef->cmap.chunk, *b - EXFAT_FIRST_DATA_CLUSTER) == 0)
		{
			(*b)--;
			break;
		}

	return 0;
}

int exfat_find_used_sectors(const struct exfat* ef, off_t* a, off_t* b)
{
	cluster_t ca, cb;

	if (*a == 0 && *b == 0)
		ca = cb = EXFAT_FIRST_DATA_CLUSTER - 1;
	else
	{
		ca = s2c(ef, *a);
		cb = s2c(ef, *b);
	}
	if (find_used_clusters(ef, &ca, &cb) != 0)
		return 1;
	if (*a != 0 || *b != 0)
		*a = c2s(ef, ca);
	*b = c2s(ef, cb) + (CLUSTER_SIZE(*ef->sb) - 1) / SECTOR_SIZE(*ef->sb);
	return 0;
}
