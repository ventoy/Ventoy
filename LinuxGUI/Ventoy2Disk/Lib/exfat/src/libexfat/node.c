/*
	node.c (09.10.09)
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

#define EXFAT_ENTRY_NONE (-1)

struct exfat_node* exfat_get_node(struct exfat_node* node)
{
	/* if we switch to multi-threaded mode we will need atomic
	   increment here and atomic decrement in exfat_put_node() */
	node->references++;
	return node;
}

void exfat_put_node(struct exfat* ef, struct exfat_node* node)
{
	char buffer[EXFAT_UTF8_NAME_BUFFER_MAX];

	--node->references;
	if (node->references < 0)
	{
		exfat_get_name(node, buffer);
		exfat_bug("reference counter of '%s' is below zero", buffer);
	}
	else if (node->references == 0 && node != ef->root)
	{
		if (node->is_dirty)
		{
			exfat_get_name(node, buffer);
			exfat_warn("dirty node '%s' with zero references", buffer);
		}
	}
}

/**
 * This function must be called on rmdir and unlink (after the last
 * exfat_put_node()) to free clusters.
 */
int exfat_cleanup_node(struct exfat* ef, struct exfat_node* node)
{
	int rc = 0;

	if (node->references != 0)
		exfat_bug("unable to cleanup a node with %d references",
				node->references);

	if (node->is_unlinked)
	{
		/* free all clusters and node structure itself */
		rc = exfat_truncate(ef, node, 0, true);
		/* free the node even in case of error or its memory will be lost */
		free(node);
	}
	return rc;
}

static int read_entries(struct exfat* ef, struct exfat_node* dir,
		struct exfat_entry* entries, int n, off_t offset)
{
	ssize_t size;

	if (!(dir->attrib & EXFAT_ATTRIB_DIR))
		exfat_bug("attempted to read entries from a file");

	size = exfat_generic_pread(ef, dir, entries,
			sizeof(struct exfat_entry[n]), offset);
	if (size == sizeof(struct exfat_entry[n]))
		return 0; /* success */
	if (size == 0)
		return -ENOENT;
	if (size < 0)
		return -EIO;
	exfat_error("read %zd bytes instead of %zu bytes", size,
			sizeof(struct exfat_entry[n]));
	return -EIO;
}

static int write_entries(struct exfat* ef, struct exfat_node* dir,
		const struct exfat_entry* entries, int n, off_t offset)
{
	ssize_t size;

	if (!(dir->attrib & EXFAT_ATTRIB_DIR))
		exfat_bug("attempted to write entries into a file");

	size = exfat_generic_pwrite(ef, dir, entries,
			sizeof(struct exfat_entry[n]), offset);
	if (size == sizeof(struct exfat_entry[n]))
		return 0; /* success */
	if (size < 0)
		return -EIO;
	exfat_error("wrote %zd bytes instead of %zu bytes", size,
			sizeof(struct exfat_entry[n]));
	return -EIO;
}

static struct exfat_node* allocate_node(void)
{
	struct exfat_node* node = malloc(sizeof(struct exfat_node));
	if (node == NULL)
	{
		exfat_error("failed to allocate node");
		return NULL;
	}
	memset(node, 0, sizeof(struct exfat_node));
	return node;
}

static void init_node_meta1(struct exfat_node* node,
		const struct exfat_entry_meta1* meta1)
{
	node->attrib = le16_to_cpu(meta1->attrib);
	node->continuations = meta1->continuations;
	node->mtime = exfat_exfat2unix(meta1->mdate, meta1->mtime,
			meta1->mtime_cs);
	/* there is no centiseconds field for atime */
	node->atime = exfat_exfat2unix(meta1->adate, meta1->atime, 0);
}

static void init_node_meta2(struct exfat_node* node,
		const struct exfat_entry_meta2* meta2)
{
	node->size = le64_to_cpu(meta2->size);
	node->start_cluster = le32_to_cpu(meta2->start_cluster);
	node->fptr_cluster = node->start_cluster;
	node->is_contiguous = ((meta2->flags & EXFAT_FLAG_CONTIGUOUS) != 0);
}

static void init_node_name(struct exfat_node* node,
		const struct exfat_entry* entries, int n)
{
	int i;

	for (i = 0; i < n; i++)
		memcpy(node->name + i * EXFAT_ENAME_MAX,
				((const struct exfat_entry_name*) &entries[i])->name,
				EXFAT_ENAME_MAX * sizeof(le16_t));
}

static bool check_entries(const struct exfat_entry* entry, int n)
{
	int previous = EXFAT_ENTRY_NONE;
	int current;
	int i;

	/* check transitions between entries types */
	for (i = 0; i < n + 1; previous = current, i++)
	{
		bool valid = false;

		current = (i < n) ? entry[i].type : EXFAT_ENTRY_NONE;
		switch (previous)
		{
		case EXFAT_ENTRY_NONE:
			valid = (current == EXFAT_ENTRY_FILE);
			break;
		case EXFAT_ENTRY_FILE:
			valid = (current == EXFAT_ENTRY_FILE_INFO);
			break;
		case EXFAT_ENTRY_FILE_INFO:
			valid = (current == EXFAT_ENTRY_FILE_NAME);
			break;
		case EXFAT_ENTRY_FILE_NAME:
			valid = (current == EXFAT_ENTRY_FILE_NAME ||
			         current == EXFAT_ENTRY_NONE ||
			         current >= EXFAT_ENTRY_FILE_TAIL);
			break;
		case EXFAT_ENTRY_FILE_TAIL ... 0xff:
			valid = (current >= EXFAT_ENTRY_FILE_TAIL ||
			         current == EXFAT_ENTRY_NONE);
			break;
		}

		if (!valid)
		{
			exfat_error("unexpected entry type %#x after %#x at %d/%d",
					current, previous, i, n);
			return false;
		}
	}
	return true;
}

static bool check_node(const struct exfat* ef, struct exfat_node* node,
		le16_t actual_checksum, const struct exfat_entry_meta1* meta1,
		const struct exfat_entry_meta2* meta2)
{
	int cluster_size = CLUSTER_SIZE(*ef->sb);
	uint64_t clusters_heap_size =
			(uint64_t) le32_to_cpu(ef->sb->cluster_count) * cluster_size;
	char buffer[EXFAT_UTF8_NAME_BUFFER_MAX];
	bool ret = true;

	/*
	   Validate checksum first. If it's invalid all other fields probably
	   contain just garbage.
	*/
	if (le16_to_cpu(actual_checksum) != le16_to_cpu(meta1->checksum))
	{
		exfat_get_name(node, buffer);
		exfat_error("'%s' has invalid checksum (%#hx != %#hx)", buffer,
				le16_to_cpu(actual_checksum), le16_to_cpu(meta1->checksum));
		if (!EXFAT_REPAIR(invalid_node_checksum, ef, node))
			ret = false;
	}

	/*
	   exFAT does not support sparse files but allows files with uninitialized
	   clusters. For such files valid_size means initialized data size and
	   cannot be greater than file size. See SetFileValidData() function
	   description in MSDN.
	*/
	if (le64_to_cpu(meta2->valid_size) > node->size)
	{
		exfat_get_name(node, buffer);
		exfat_error("'%s' has valid size (%"PRIu64") greater than size "
				"(%"PRIu64")", buffer, le64_to_cpu(meta2->valid_size),
				node->size);
		ret = false;
	}

	/*
	   Empty file must have zero start cluster. Non-empty file must start
	   with a valid cluster. Directories cannot be empty (i.e. must always
	   have a valid start cluster), but we will check this later while
	   reading that directory to give user a chance to read this directory.
	*/
	if (node->size == 0 && node->start_cluster != EXFAT_CLUSTER_FREE)
	{
		exfat_get_name(node, buffer);
		exfat_error("'%s' is empty but start cluster is %#x", buffer,
				node->start_cluster);
		ret = false;
	}
	if (node->size > 0 && CLUSTER_INVALID(*ef->sb, node->start_cluster))
	{
		exfat_get_name(node, buffer);
		exfat_error("'%s' points to invalid cluster %#x", buffer,
				node->start_cluster);
		ret = false;
	}

	/* File or directory cannot be larger than clusters heap. */
	if (node->size > clusters_heap_size)
	{
		exfat_get_name(node, buffer);
		exfat_error("'%s' is larger than clusters heap: %"PRIu64" > %"PRIu64,
				buffer, node->size, clusters_heap_size);
		ret = false;
	}

	/* Empty file or directory must be marked as non-contiguous. */
	if (node->size == 0 && node->is_contiguous)
	{
		exfat_get_name(node, buffer);
		exfat_error("'%s' is empty but marked as contiguous (%#hx)", buffer,
				node->attrib);
		ret = false;
	}

	/* Directory size must be aligned on at cluster boundary. */
	if ((node->attrib & EXFAT_ATTRIB_DIR) && node->size % cluster_size != 0)
	{
		exfat_get_name(node, buffer);
		exfat_error("'%s' directory size %"PRIu64" is not divisible by %d", buffer,
				node->size, cluster_size);
		ret = false;
	}

	return ret;
}

static int parse_file_entries(struct exfat* ef, struct exfat_node* node,
		const struct exfat_entry* entries, int n)
{
	const struct exfat_entry_meta1* meta1;
	const struct exfat_entry_meta2* meta2;
	int mandatory_entries;

	if (!check_entries(entries, n))
		return -EIO;

	meta1 = (const struct exfat_entry_meta1*) &entries[0];
	if (meta1->continuations < 2)
	{
		exfat_error("too few continuations (%hhu)", meta1->continuations);
		return -EIO;
	}
	meta2 = (const struct exfat_entry_meta2*) &entries[1];
	if (meta2->flags & ~(EXFAT_FLAG_ALWAYS1 | EXFAT_FLAG_CONTIGUOUS))
	{
		exfat_error("unknown flags in meta2 (%#hhx)", meta2->flags);
		return -EIO;
	}
	mandatory_entries = 2 + DIV_ROUND_UP(meta2->name_length, EXFAT_ENAME_MAX);
	if (meta1->continuations < mandatory_entries - 1)
	{
		exfat_error("too few continuations (%hhu < %d)",
				meta1->continuations, mandatory_entries - 1);
		return -EIO;
	}

	init_node_meta1(node, meta1);
	init_node_meta2(node, meta2);
	init_node_name(node, entries + 2, mandatory_entries - 2);

	if (!check_node(ef, node, exfat_calc_checksum(entries, n), meta1, meta2))
		return -EIO;

	return 0;
}

static int parse_file_entry(struct exfat* ef, struct exfat_node* parent,
		struct exfat_node** node, off_t* offset, int n)
{
	struct exfat_entry entries[n];
	int rc;

	rc = read_entries(ef, parent, entries, n, *offset);
	if (rc != 0)
		return rc;

	/* a new node has zero references */
	*node = allocate_node();
	if (*node == NULL)
		return -ENOMEM;
	(*node)->entry_offset = *offset;

	rc = parse_file_entries(ef, *node, entries, n);
	if (rc != 0)
	{
		free(*node);
		return rc;
	}

	*offset += sizeof(struct exfat_entry[n]);
	return 0;
}

static void decompress_upcase(uint16_t* output, const le16_t* source,
		size_t size)
{
	size_t si;
	size_t oi;

	for (oi = 0; oi < EXFAT_UPCASE_CHARS; oi++)
		output[oi] = oi;

	for (si = 0, oi = 0; si < size && oi < EXFAT_UPCASE_CHARS; si++)
	{
		uint16_t ch = le16_to_cpu(source[si]);

		if (ch == 0xffff && si + 1 < size)	/* indicates a run */
			oi += le16_to_cpu(source[++si]);
		else
			output[oi++] = ch;
	}
}

/*
 * Read one entry in a directory at offset position and build a new node
 * structure.
 */
static int readdir(struct exfat* ef, struct exfat_node* parent,
		struct exfat_node** node, off_t* offset)
{
	int rc;
	struct exfat_entry entry;
	const struct exfat_entry_meta1* meta1;
	const struct exfat_entry_upcase* upcase;
	const struct exfat_entry_bitmap* bitmap;
	const struct exfat_entry_label* label;
	uint64_t upcase_size = 0;
	le16_t* upcase_comp = NULL;

	for (;;)
	{
		rc = read_entries(ef, parent, &entry, 1, *offset);
		if (rc != 0)
			return rc;

		switch (entry.type)
		{
		case EXFAT_ENTRY_FILE:
			meta1 = (const struct exfat_entry_meta1*) &entry;
			return parse_file_entry(ef, parent, node, offset,
					1 + meta1->continuations);

		case EXFAT_ENTRY_UPCASE:
			if (ef->upcase != NULL)
				break;
			upcase = (const struct exfat_entry_upcase*) &entry;
			if (CLUSTER_INVALID(*ef->sb, le32_to_cpu(upcase->start_cluster)))
			{
				exfat_error("invalid cluster 0x%x in upcase table",
						le32_to_cpu(upcase->start_cluster));
				return -EIO;
			}
			upcase_size = le64_to_cpu(upcase->size);
			if (upcase_size == 0 ||
				upcase_size > EXFAT_UPCASE_CHARS * sizeof(uint16_t) ||
				upcase_size % sizeof(uint16_t) != 0)
			{
				exfat_error("bad upcase table size (%"PRIu64" bytes)",
						upcase_size);
				return -EIO;
			}
			upcase_comp = malloc(upcase_size);
			if (upcase_comp == NULL)
			{
				exfat_error("failed to allocate upcase table (%"PRIu64" bytes)",
						upcase_size);
				return -ENOMEM;
			}

			/* read compressed upcase table */
			if (exfat_pread(ef->dev, upcase_comp, upcase_size,
					exfat_c2o(ef, le32_to_cpu(upcase->start_cluster))) < 0)
			{
				free(upcase_comp);
				exfat_error("failed to read upper case table "
						"(%"PRIu64" bytes starting at cluster %#x)",
						upcase_size,
						le32_to_cpu(upcase->start_cluster));
				return -EIO;
			}

			/* decompress upcase table */
			ef->upcase = calloc(EXFAT_UPCASE_CHARS, sizeof(uint16_t));
			if (ef->upcase == NULL)
			{
				free(upcase_comp);
				exfat_error("failed to allocate decompressed upcase table");
				return -ENOMEM;
			}
			decompress_upcase(ef->upcase, upcase_comp,
					upcase_size / sizeof(uint16_t));
			free(upcase_comp);
			break;

		case EXFAT_ENTRY_BITMAP:
			bitmap = (const struct exfat_entry_bitmap*) &entry;
			ef->cmap.start_cluster = le32_to_cpu(bitmap->start_cluster);
			if (CLUSTER_INVALID(*ef->sb, ef->cmap.start_cluster))
			{
				exfat_error("invalid cluster 0x%x in clusters bitmap",
						ef->cmap.start_cluster);
				return -EIO;
			}
			ef->cmap.size = le32_to_cpu(ef->sb->cluster_count);
			if (le64_to_cpu(bitmap->size) < DIV_ROUND_UP(ef->cmap.size, 8))
			{
				exfat_error("invalid clusters bitmap size: %"PRIu64
						" (expected at least %u)",
						le64_to_cpu(bitmap->size),
						DIV_ROUND_UP(ef->cmap.size, 8));
				return -EIO;
			}
			/* FIXME bitmap can be rather big, up to 512 MB */
			ef->cmap.chunk_size = ef->cmap.size;
			ef->cmap.chunk = malloc(BMAP_SIZE(ef->cmap.chunk_size));
			if (ef->cmap.chunk == NULL)
			{
				exfat_error("failed to allocate clusters bitmap chunk "
						"(%"PRIu64" bytes)", le64_to_cpu(bitmap->size));
				return -ENOMEM;
			}

			if (exfat_pread(ef->dev, ef->cmap.chunk,
					BMAP_SIZE(ef->cmap.chunk_size),
					exfat_c2o(ef, ef->cmap.start_cluster)) < 0)
			{
				exfat_error("failed to read clusters bitmap "
						"(%"PRIu64" bytes starting at cluster %#x)",
						le64_to_cpu(bitmap->size), ef->cmap.start_cluster);
				return -EIO;
			}
			break;

		case EXFAT_ENTRY_LABEL:
			label = (const struct exfat_entry_label*) &entry;
			if (label->length > EXFAT_ENAME_MAX)
			{
				exfat_error("too long label (%hhu chars)", label->length);
				return -EIO;
			}
			if (utf16_to_utf8(ef->label, label->name,
						sizeof(ef->label), EXFAT_ENAME_MAX) != 0)
				return -EIO;
			break;

		default:
			if (!(entry.type & EXFAT_ENTRY_VALID))
				break; /* deleted entry, ignore it */

			exfat_error("unknown entry type %#hhx", entry.type);
			if (!EXFAT_REPAIR(unknown_entry, ef, parent, &entry, *offset))
				return -EIO;
		}
		*offset += sizeof(entry);
	}
	/* we never reach here */
}

int exfat_cache_directory(struct exfat* ef, struct exfat_node* dir)
{
	off_t offset = 0;
	int rc;
	struct exfat_node* node;
	struct exfat_node* current = NULL;

	if (dir->is_cached)
		return 0; /* already cached */

	while ((rc = readdir(ef, dir, &node, &offset)) == 0)
	{
		node->parent = dir;
		if (current != NULL)
		{
			current->next = node;
			node->prev = current;
		}
		else
			dir->child = node;

		current = node;
	}

	if (rc != -ENOENT)
	{
		/* rollback */
		for (current = dir->child; current; current = node)
		{
			node = current->next;
			free(current);
		}
		dir->child = NULL;
		return rc;
	}

	dir->is_cached = true;
	return 0;
}

static void tree_attach(struct exfat_node* dir, struct exfat_node* node)
{
	node->parent = dir;
	if (dir->child)
	{
		dir->child->prev = node;
		node->next = dir->child;
	}
	dir->child = node;
}

static void tree_detach(struct exfat_node* node)
{
	if (node->prev)
		node->prev->next = node->next;
	else /* this is the first node in the list */
		node->parent->child = node->next;
	if (node->next)
		node->next->prev = node->prev;
	node->parent = NULL;
	node->prev = NULL;
	node->next = NULL;
}

static void reset_cache(struct exfat* ef, struct exfat_node* node)
{
	char buffer[EXFAT_UTF8_NAME_BUFFER_MAX];

	while (node->child)
	{
		struct exfat_node* p = node->child;
		reset_cache(ef, p);
		tree_detach(p);
		free(p);
	}
	node->is_cached = false;
	if (node->references != 0)
	{
		exfat_get_name(node, buffer);
		exfat_warn("non-zero reference counter (%d) for '%s'",
				node->references, buffer);
	}
	if (node != ef->root && node->is_dirty)
	{
		exfat_get_name(node, buffer);
		exfat_bug("node '%s' is dirty", buffer);
	}
	while (node->references)
		exfat_put_node(ef, node);
}

void exfat_reset_cache(struct exfat* ef)
{
	reset_cache(ef, ef->root);
}

int exfat_flush_node(struct exfat* ef, struct exfat_node* node)
{
	struct exfat_entry entries[1 + node->continuations];
	struct exfat_entry_meta1* meta1 = (struct exfat_entry_meta1*) &entries[0];
	struct exfat_entry_meta2* meta2 = (struct exfat_entry_meta2*) &entries[1];
	int rc;

	if (!node->is_dirty)
		return 0; /* no need to flush */

	if (ef->ro)
		exfat_bug("unable to flush node to read-only FS");

	if (node->parent == NULL)
		return 0; /* do not flush unlinked node */

	rc = read_entries(ef, node->parent, entries, 1 + node->continuations,
			node->entry_offset);
	if (rc != 0)
		return rc;
	if (!check_entries(entries, 1 + node->continuations))
		return -EIO;

	meta1->attrib = cpu_to_le16(node->attrib);
	exfat_unix2exfat(node->mtime, &meta1->mdate, &meta1->mtime,
			&meta1->mtime_cs);
	exfat_unix2exfat(node->atime, &meta1->adate, &meta1->atime, NULL);
	meta2->size = meta2->valid_size = cpu_to_le64(node->size);
	meta2->start_cluster = cpu_to_le32(node->start_cluster);
	meta2->flags = EXFAT_FLAG_ALWAYS1;
	/* empty files must not be marked as contiguous */
	if (node->size != 0 && node->is_contiguous)
		meta2->flags |= EXFAT_FLAG_CONTIGUOUS;
	/* name hash remains unchanged, no need to recalculate it */

	meta1->checksum = exfat_calc_checksum(entries, 1 + node->continuations);
	rc = write_entries(ef, node->parent, entries, 1 + node->continuations,
			node->entry_offset);
	if (rc != 0)
		return rc;

	node->is_dirty = false;
	return exfat_flush(ef);
}

static int erase_entries(struct exfat* ef, struct exfat_node* dir, int n,
		off_t offset)
{
	struct exfat_entry entries[n];
	int rc;
	int i;

	rc = read_entries(ef, dir, entries, n, offset);
	if (rc != 0)
		return rc;
	for (i = 0; i < n; i++)
		entries[i].type &= ~EXFAT_ENTRY_VALID;
	return write_entries(ef, dir, entries, n, offset);
}

static int erase_node(struct exfat* ef, struct exfat_node* node)
{
	int rc;

	exfat_get_node(node->parent);
	rc = erase_entries(ef, node->parent, 1 + node->continuations,
			node->entry_offset);
	if (rc != 0)
	{
		exfat_put_node(ef, node->parent);
		return rc;
	}
	rc = exfat_flush_node(ef, node->parent);
	exfat_put_node(ef, node->parent);
	return rc;
}

static int shrink_directory(struct exfat* ef, struct exfat_node* dir,
		off_t deleted_offset)
{
	const struct exfat_node* node;
	const struct exfat_node* last_node;
	uint64_t entries = 0;
	uint64_t new_size;

	if (!(dir->attrib & EXFAT_ATTRIB_DIR))
		exfat_bug("attempted to shrink a file");
	if (!dir->is_cached)
		exfat_bug("attempted to shrink uncached directory");

	for (last_node = node = dir->child; node; node = node->next)
	{
		if (deleted_offset < node->entry_offset)
		{
			/* there are other entries after the removed one, no way to shrink
			   this directory */
			return 0;
		}
		if (last_node->entry_offset < node->entry_offset)
			last_node = node;
	}

	if (last_node)
	{
		/* offset of the last entry */
		entries += last_node->entry_offset / sizeof(struct exfat_entry);
		/* two subentries with meta info */
		entries += 2;
		/* subentries with file name */
		entries += DIV_ROUND_UP(utf16_length(last_node->name),
				EXFAT_ENAME_MAX);
	}

	new_size = DIV_ROUND_UP(entries * sizeof(struct exfat_entry),
				 CLUSTER_SIZE(*ef->sb)) * CLUSTER_SIZE(*ef->sb);
	if (new_size == 0) /* directory always has at least 1 cluster */
		new_size = CLUSTER_SIZE(*ef->sb);
	if (new_size == dir->size)
		return 0;
	return exfat_truncate(ef, dir, new_size, true);
}

static int delete(struct exfat* ef, struct exfat_node* node)
{
	struct exfat_node* parent = node->parent;
	off_t deleted_offset = node->entry_offset;
	int rc;

	exfat_get_node(parent);
	rc = erase_node(ef, node);
	if (rc != 0)
	{
		exfat_put_node(ef, parent);
		return rc;
	}
	tree_detach(node);
	rc = shrink_directory(ef, parent, deleted_offset);
	node->is_unlinked = true;
	if (rc != 0)
	{
		exfat_flush_node(ef, parent);
		exfat_put_node(ef, parent);
		return rc;
	}
	exfat_update_mtime(parent);
	rc = exfat_flush_node(ef, parent);
	exfat_put_node(ef, parent);
	return rc;
}

int exfat_unlink(struct exfat* ef, struct exfat_node* node)
{
	if (node->attrib & EXFAT_ATTRIB_DIR)
		return -EISDIR;
	return delete(ef, node);
}

int exfat_rmdir(struct exfat* ef, struct exfat_node* node)
{
	int rc;

	if (!(node->attrib & EXFAT_ATTRIB_DIR))
		return -ENOTDIR;
	/* check that directory is empty */
	rc = exfat_cache_directory(ef, node);
	if (rc != 0)
		return rc;
	if (node->child)
		return -ENOTEMPTY;
	return delete(ef, node);
}

static int check_slot(struct exfat* ef, struct exfat_node* dir, off_t offset,
		int n)
{
	struct exfat_entry entries[n];
	int rc;
	size_t i;

	/* Root directory contains entries, that don't have any nodes associated
	   with them (clusters bitmap, upper case table, label). We need to be
	   careful not to overwrite them. */
	if (dir != ef->root)
		return 0;

	rc = read_entries(ef, dir, entries, n, offset);
	if (rc != 0)
		return rc;
	for (i = 0; i < n; i++)
		if (entries[i].type & EXFAT_ENTRY_VALID)
			return -EINVAL;
	return 0;
}

static int find_slot(struct exfat* ef, struct exfat_node* dir,
		off_t* offset, int n)
{
	bitmap_t* dmap;
	struct exfat_node* p;
	size_t i;
	int contiguous = 0;

	if (!dir->is_cached)
		exfat_bug("directory is not cached");

	/* build a bitmap of valid entries in the directory */
	dmap = calloc(BMAP_SIZE(dir->size / sizeof(struct exfat_entry)),
			sizeof(bitmap_t));
	if (dmap == NULL)
	{
		exfat_error("failed to allocate directory bitmap (%"PRIu64")",
				dir->size / sizeof(struct exfat_entry));
		return -ENOMEM;
	}
	for (p = dir->child; p != NULL; p = p->next)
		for (i = 0; i < 1 + p->continuations; i++)
			BMAP_SET(dmap, p->entry_offset / sizeof(struct exfat_entry) + i);

	/* find a slot in the directory entries bitmap */
	for (i = 0; i < dir->size / sizeof(struct exfat_entry); i++)
	{
		if (BMAP_GET(dmap, i) == 0)
		{
			if (contiguous++ == 0)
				*offset = (off_t) i * sizeof(struct exfat_entry);
			if (contiguous == n)
				/* suitable slot is found, check that it's not occupied */
				switch (check_slot(ef, dir, *offset, n))
				{
				case 0:
					free(dmap);
					return 0;
				case -EIO:
					free(dmap);
					return -EIO;
				case -EINVAL:
					/* slot at (i-n) is occupied, go back and check (i-n+1) */
					i -= contiguous - 1;
					contiguous = 0;
					break;
				}
		}
		else
			contiguous = 0;
	}
	free(dmap);

	/* no suitable slots found, extend the directory */
	if (contiguous == 0)
		*offset = dir->size;
	return exfat_truncate(ef, dir,
			ROUND_UP(dir->size + sizeof(struct exfat_entry[n - contiguous]),
					CLUSTER_SIZE(*ef->sb)),
			true);
}

static int commit_entry(struct exfat* ef, struct exfat_node* dir,
		const le16_t* name, off_t offset, uint16_t attrib)
{
	struct exfat_node* node;
	const size_t name_length = utf16_length(name);
	const int name_entries = DIV_ROUND_UP(name_length, EXFAT_ENAME_MAX);
	struct exfat_entry entries[2 + name_entries];
	struct exfat_entry_meta1* meta1 = (struct exfat_entry_meta1*) &entries[0];
	struct exfat_entry_meta2* meta2 = (struct exfat_entry_meta2*) &entries[1];
	int i;
	int rc;

	memset(entries, 0, sizeof(struct exfat_entry[2]));

	meta1->type = EXFAT_ENTRY_FILE;
	meta1->continuations = 1 + name_entries;
	meta1->attrib = cpu_to_le16(attrib);
	exfat_unix2exfat(time(NULL), &meta1->crdate, &meta1->crtime,
			&meta1->crtime_cs);
	meta1->adate = meta1->mdate = meta1->crdate;
	meta1->atime = meta1->mtime = meta1->crtime;
	meta1->mtime_cs = meta1->crtime_cs; /* there is no atime_cs */

	meta2->type = EXFAT_ENTRY_FILE_INFO;
	meta2->flags = EXFAT_FLAG_ALWAYS1;
	meta2->name_length = name_length;
	meta2->name_hash = exfat_calc_name_hash(ef, name, name_length);
	meta2->start_cluster = cpu_to_le32(EXFAT_CLUSTER_FREE);

	for (i = 0; i < name_entries; i++)
	{
		struct exfat_entry_name* name_entry;

		name_entry = (struct exfat_entry_name*) &entries[2 + i];
		name_entry->type = EXFAT_ENTRY_FILE_NAME;
		name_entry->__unknown = 0;
		memcpy(name_entry->name, name + i * EXFAT_ENAME_MAX,
				EXFAT_ENAME_MAX * sizeof(le16_t));
	}

	meta1->checksum = exfat_calc_checksum(entries, 2 + name_entries);
	rc = write_entries(ef, dir, entries, 2 + name_entries, offset);
	if (rc != 0)
		return rc;

	node = allocate_node();
	if (node == NULL)
		return -ENOMEM;
	node->entry_offset = offset;
	memcpy(node->name, name, name_length * sizeof(le16_t));
	init_node_meta1(node, meta1);
	init_node_meta2(node, meta2);

	tree_attach(dir, node);
	return 0;
}

static int create(struct exfat* ef, const char* path, uint16_t attrib)
{
	struct exfat_node* dir;
	struct exfat_node* existing;
	off_t offset = -1;
	le16_t name[EXFAT_NAME_MAX + 1];
	int rc;

	rc = exfat_split(ef, &dir, &existing, name, path);
	if (rc != 0)
		return rc;
	if (existing != NULL)
	{
		exfat_put_node(ef, existing);
		exfat_put_node(ef, dir);
		return -EEXIST;
	}

	rc = find_slot(ef, dir, &offset,
			2 + DIV_ROUND_UP(utf16_length(name), EXFAT_ENAME_MAX));
	if (rc != 0)
	{
		exfat_put_node(ef, dir);
		return rc;
	}
	rc = commit_entry(ef, dir, name, offset, attrib);
	if (rc != 0)
	{
		exfat_put_node(ef, dir);
		return rc;
	}
	exfat_update_mtime(dir);
	rc = exfat_flush_node(ef, dir);
	exfat_put_node(ef, dir);
	return rc;
}

int exfat_mknod(struct exfat* ef, const char* path)
{
	return create(ef, path, EXFAT_ATTRIB_ARCH);
}

int exfat_mkdir(struct exfat* ef, const char* path)
{
	int rc;
	struct exfat_node* node;

	rc = create(ef, path, EXFAT_ATTRIB_DIR);
	if (rc != 0)
		return rc;
	rc = exfat_lookup(ef, &node, path);
	if (rc != 0)
		return 0;
	/* directories always have at least one cluster */
	rc = exfat_truncate(ef, node, CLUSTER_SIZE(*ef->sb), true);
	if (rc != 0)
	{
		delete(ef, node);
		exfat_put_node(ef, node);
		return rc;
	}
	rc = exfat_flush_node(ef, node);
	if (rc != 0)
	{
		delete(ef, node);
		exfat_put_node(ef, node);
		return rc;
	}
	exfat_put_node(ef, node);
	return 0;
}

static int rename_entry(struct exfat* ef, struct exfat_node* dir,
		struct exfat_node* node, const le16_t* name, off_t new_offset)
{
	const size_t name_length = utf16_length(name);
	const int name_entries = DIV_ROUND_UP(name_length, EXFAT_ENAME_MAX);
	struct exfat_entry entries[2 + name_entries];
	struct exfat_entry_meta1* meta1 = (struct exfat_entry_meta1*) &entries[0];
	struct exfat_entry_meta2* meta2 = (struct exfat_entry_meta2*) &entries[1];
	int rc;
	int i;

	rc = read_entries(ef, node->parent, entries, 2, node->entry_offset);
	if (rc != 0)
		return rc;

	meta1->continuations = 1 + name_entries;
	meta2->name_length = name_length;
	meta2->name_hash = exfat_calc_name_hash(ef, name, name_length);

	rc = erase_node(ef, node);
	if (rc != 0)
		return rc;

	node->entry_offset = new_offset;
	node->continuations = 1 + name_entries;

	for (i = 0; i < name_entries; i++)
	{
		struct exfat_entry_name* name_entry;

		name_entry = (struct exfat_entry_name*) &entries[2 + i];
		name_entry->type = EXFAT_ENTRY_FILE_NAME;
		name_entry->__unknown = 0;
		memcpy(name_entry->name, name + i * EXFAT_ENAME_MAX,
				EXFAT_ENAME_MAX * sizeof(le16_t));
	}

	meta1->checksum = exfat_calc_checksum(entries, 2 + name_entries);
	rc = write_entries(ef, dir, entries, 2 + name_entries, new_offset);
	if (rc != 0)
		return rc;

	memcpy(node->name, name, (EXFAT_NAME_MAX + 1) * sizeof(le16_t));
	tree_detach(node);
	tree_attach(dir, node);
	return 0;
}

int exfat_rename(struct exfat* ef, const char* old_path, const char* new_path)
{
	struct exfat_node* node;
	struct exfat_node* existing;
	struct exfat_node* dir;
	off_t offset = -1;
	le16_t name[EXFAT_NAME_MAX + 1];
	int rc;

	rc = exfat_lookup(ef, &node, old_path);
	if (rc != 0)
		return rc;

	rc = exfat_split(ef, &dir, &existing, name, new_path);
	if (rc != 0)
	{
		exfat_put_node(ef, node);
		return rc;
	}

	/* check that target is not a subdirectory of the source */
	if (node->attrib & EXFAT_ATTRIB_DIR)
	{
		struct exfat_node* p;

		for (p = dir; p; p = p->parent)
			if (node == p)
			{
				if (existing != NULL)
					exfat_put_node(ef, existing);
				exfat_put_node(ef, dir);
				exfat_put_node(ef, node);
				return -EINVAL;
			}
	}

	if (existing != NULL)
	{
		/* remove target if it's not the same node as source */
		if (existing != node)
		{
			if (existing->attrib & EXFAT_ATTRIB_DIR)
			{
				if (node->attrib & EXFAT_ATTRIB_DIR)
					rc = exfat_rmdir(ef, existing);
				else
					rc = -ENOTDIR;
			}
			else
			{
				if (!(node->attrib & EXFAT_ATTRIB_DIR))
					rc = exfat_unlink(ef, existing);
				else
					rc = -EISDIR;
			}
			exfat_put_node(ef, existing);
			if (rc != 0)
			{
				/* free clusters even if something went wrong; overwise they
				   will be just lost */
				exfat_cleanup_node(ef, existing);
				exfat_put_node(ef, dir);
				exfat_put_node(ef, node);
				return rc;
			}
			rc = exfat_cleanup_node(ef, existing);
			if (rc != 0)
			{
				exfat_put_node(ef, dir);
				exfat_put_node(ef, node);
				return rc;
			}
		}
		else
			exfat_put_node(ef, existing);
	}

	rc = find_slot(ef, dir, &offset,
			2 + DIV_ROUND_UP(utf16_length(name), EXFAT_ENAME_MAX));
	if (rc != 0)
	{
		exfat_put_node(ef, dir);
		exfat_put_node(ef, node);
		return rc;
	}
	rc = rename_entry(ef, dir, node, name, offset);
	if (rc != 0)
	{
		exfat_put_node(ef, dir);
		exfat_put_node(ef, node);
		return rc;
	}
	rc = exfat_flush_node(ef, dir);
	exfat_put_node(ef, dir);
	exfat_put_node(ef, node);
	/* node itself is not marked as dirty, no need to flush it */
	return rc;
}

void exfat_utimes(struct exfat_node* node, const struct timespec tv[2])
{
	node->atime = tv[0].tv_sec;
	node->mtime = tv[1].tv_sec;
	node->is_dirty = true;
}

void exfat_update_atime(struct exfat_node* node)
{
	node->atime = time(NULL);
	node->is_dirty = true;
}

void exfat_update_mtime(struct exfat_node* node)
{
	node->mtime = time(NULL);
	node->is_dirty = true;
}

const char* exfat_get_label(struct exfat* ef)
{
	return ef->label;
}

static int find_label(struct exfat* ef, off_t* offset)
{
	struct exfat_entry entry;
	int rc;

	for (*offset = 0; ; *offset += sizeof(entry))
	{
		rc = read_entries(ef, ef->root, &entry, 1, *offset);
		if (rc != 0)
			return rc;

		if (entry.type == EXFAT_ENTRY_LABEL)
			return 0;
	}
}

int exfat_set_label(struct exfat* ef, const char* label)
{
	le16_t label_utf16[EXFAT_ENAME_MAX + 1];
	int rc;
	off_t offset;
	struct exfat_entry_label entry;

	memset(label_utf16, 0, sizeof(label_utf16));
	rc = utf8_to_utf16(label_utf16, label, EXFAT_ENAME_MAX + 1, strlen(label));
	if (rc != 0)
		return rc;

	rc = find_label(ef, &offset);
	if (rc == -ENOENT)
		rc = find_slot(ef, ef->root, &offset, 1);
	if (rc != 0)
		return rc;

	entry.type = EXFAT_ENTRY_LABEL;
	entry.length = utf16_length(label_utf16);
	memcpy(entry.name, label_utf16, sizeof(entry.name));
	if (entry.length == 0)
		entry.type ^= EXFAT_ENTRY_VALID;

	rc = write_entries(ef, ef->root, (struct exfat_entry*) &entry, 1, offset);
	if (rc != 0)
		return rc;

	strcpy(ef->label, label);
	return 0;
}
