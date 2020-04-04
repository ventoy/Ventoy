/*
 * Read a squashfs filesystem.  This is a highly compressed read only
 * filesystem.
 *
 * Copyright (c) 2010, 2012, 2013, 2019
 * Phillip Lougher <phillip@squashfs.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * read_xattrs.c
 */

/*
 * Common xattr read code shared between mksquashfs and unsquashfs
 */

#define TRUE 1
#define FALSE 0
#include <stdio.h>
#include <string.h>

#ifndef linux
#define __BYTE_ORDER BYTE_ORDER
#define __BIG_ENDIAN BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#else
#include <endian.h>
#endif

#include "squashfs_fs.h"
#include "squashfs_swap.h"
#include "xattr.h"
#include "error.h"

#include <stdlib.h>

extern int read_fs_bytes(int, long long, int, void *);
extern int read_block(int, long long, long long *, int, void *);

static struct hash_entry {
	long long		start;
	unsigned int		offset;
	struct hash_entry	*next;
} *hash_table[65536];

static struct squashfs_xattr_id *xattr_ids;
static void *xattrs = NULL;
static long long xattr_table_start;

/*
 * Prefix lookup table, storing mapping to/from prefix string and prefix id
 */
struct prefix prefix_table[] = {
	{ "user.", SQUASHFS_XATTR_USER },
	{ "trusted.", SQUASHFS_XATTR_TRUSTED },
	{ "security.", SQUASHFS_XATTR_SECURITY },
	{ "", -1 }
};

/*
 * store mapping from location of compressed block in fs ->
 * location of uncompressed block in memory
 */
static void save_xattr_block(long long start, int offset)
{
	struct hash_entry *hash_entry = malloc(sizeof(*hash_entry));
	int hash = start & 0xffff;

	TRACE("save_xattr_block: start %lld, offset %d\n", start, offset);

	if(hash_entry == NULL)
		MEM_ERROR();

	hash_entry->start = start;
	hash_entry->offset = offset;
	hash_entry->next = hash_table[hash];
	hash_table[hash] = hash_entry;
}


/*
 * map from location of compressed block in fs ->
 * location of uncompressed block in memory
 */
static int get_xattr_block(long long start)
{
	int hash = start & 0xffff;
	struct hash_entry *hash_entry = hash_table[hash];

	for(; hash_entry; hash_entry = hash_entry->next)
		if(hash_entry->start == start)
			break;

	TRACE("get_xattr_block: start %lld, offset %d\n", start,
		hash_entry ? hash_entry->offset : -1);

	return hash_entry ? hash_entry->offset : -1;
}


/*
 * construct the xattr_list entry from the fs xattr, including
 * mapping name and prefix into a full name
 */
static int read_xattr_entry(struct xattr_list *xattr,
	struct squashfs_xattr_entry *entry, void *name)
{
	int i, len, type = entry->type & XATTR_PREFIX_MASK;

	for(i = 0; prefix_table[i].type != -1; i++)
		if(prefix_table[i].type == type)
			break;

	if(prefix_table[i].type == -1) {
		ERROR("read_xattr_entry: Unrecognised xattr type %d\n", type);
		return 0;
	}

	len = strlen(prefix_table[i].prefix);
	xattr->full_name = malloc(len + entry->size + 1);
	if(xattr->full_name == NULL)
		MEM_ERROR();

	memcpy(xattr->full_name, prefix_table[i].prefix, len);
	memcpy(xattr->full_name + len, name, entry->size);
	xattr->full_name[len + entry->size] = '\0';
	xattr->name = xattr->full_name + len;
	xattr->size = entry->size;
	xattr->type = type;

	return 1;
}


/*
 * Read and decompress the xattr id table and the xattr metadata.
 * This is cached in memory for later use by get_xattr()
 */
int read_xattrs_from_disk(int fd, struct squashfs_super_block *sBlk, int flag, long long *table_start)
{
	/*
	 * Note on overflow limits:
	 * Size of ids (id_table.xattr_ids) is 2^32 (unsigned int)
	 * Max size of bytes is 2^32*16 or 2^36
	 * Max indexes is (2^32*16)/8K or 2^23
	 * Max index_bytes is ((2^32*16)/8K)*8 or 2^26 or 64M
	 */
	int res, i, indexes, index_bytes;
	unsigned int ids;
	long long bytes;
	long long *index, start, end;
	struct squashfs_xattr_table id_table;

	TRACE("read_xattrs_from_disk\n");

	if(sBlk->xattr_id_table_start == SQUASHFS_INVALID_BLK)
		return SQUASHFS_INVALID_BLK;

	/*
	 * Read xattr id table, containing start of xattr metadata and the
	 * number of xattrs in the file system
	 */
	res = read_fs_bytes(fd, sBlk->xattr_id_table_start, sizeof(id_table),
		&id_table);
	if(res == 0)
		return 0;

	SQUASHFS_INSWAP_XATTR_TABLE(&id_table);

	/*
	 * Compute index table values
	 */
	ids = id_table.xattr_ids;
	xattr_table_start = id_table.xattr_table_start;
	index_bytes = SQUASHFS_XATTR_BLOCK_BYTES((long long) ids);
	indexes = SQUASHFS_XATTR_BLOCKS((long long) ids);

	/*
	 * The size of the index table (index_bytes) should match the
	 * table start and end points
	 */
	if(index_bytes != (sBlk->bytes_used - (sBlk->xattr_id_table_start + sizeof(id_table)))) {
		ERROR("read_xattrs_from_disk: Bad xattr_ids count in super block\n");
		return 0;
	}

	/*
	 * id_table.xattr_table_start stores the start of the compressed xattr
	 * metadata blocks.  This by definition is also the end of the previous
	 * filesystem table - the id lookup table.
	 */
	if(table_start != NULL)
		*table_start = id_table.xattr_table_start;

	/*
	 * If flag is set then return once we've read the above
	 * table_start.  That value is necessary for sanity checking,
	 * but we don't actually want to extract the xattrs, and so
	 * stop here.
	 */
	if(flag)
		return id_table.xattr_ids;

	/*
	 * Allocate and read the index to the xattr id table metadata
	 * blocks
	 */
	index = malloc(index_bytes);
	if(index == NULL)
		MEM_ERROR();

	res = read_fs_bytes(fd, sBlk->xattr_id_table_start + sizeof(id_table),
		index_bytes, index);
	if(res ==0)
		goto failed1;

	SQUASHFS_INSWAP_LONG_LONGS(index, indexes);

	/*
	 * Allocate enough space for the uncompressed xattr id table, and
	 * read and decompress it
	 */
	bytes = SQUASHFS_XATTR_BYTES((long long) ids);
	xattr_ids = malloc(bytes);
	if(xattr_ids == NULL)
		MEM_ERROR();

	for(i = 0; i < indexes; i++) {
		int expected = (i + 1) != indexes ? SQUASHFS_METADATA_SIZE :
					bytes & (SQUASHFS_METADATA_SIZE - 1);
		int length = read_block(fd, index[i], NULL, expected,
			((unsigned char *) xattr_ids) +
			((long long) i * SQUASHFS_METADATA_SIZE));
		TRACE("Read xattr id table block %d, from 0x%llx, length "
			"%d\n", i, index[i], length);
		if(length == 0) {
			ERROR("Failed to read xattr id table block %d, "
				"from 0x%llx, length %d\n", i, index[i],
				length);
			goto failed2;
		}
	}

	/*
	 * Read and decompress the xattr metadata
	 *
	 * Note the first xattr id table metadata block is immediately after
	 * the last xattr metadata block, so we can use index[0] to work out
	 * the end of the xattr metadata
	 */
	start = xattr_table_start;
	end = index[0];
	for(i = 0; start < end; i++) {
		int length;
		xattrs = realloc(xattrs, (i + 1) * SQUASHFS_METADATA_SIZE);
		if(xattrs == NULL)
			MEM_ERROR();

		/* store mapping from location of compressed block in fs ->
		 * location of uncompressed block in memory */
		save_xattr_block(start, i * SQUASHFS_METADATA_SIZE);

		length = read_block(fd, start, &start, 0,
			((unsigned char *) xattrs) +
			(i * SQUASHFS_METADATA_SIZE));
		TRACE("Read xattr block %d, length %d\n", i, length);
		if(length == 0) {
			ERROR("Failed to read xattr block %d\n", i);
			goto failed3;
		}

		/*
		 * If this is not the last metadata block in the xattr metadata
		 * then it should be SQUASHFS_METADATA_SIZE in size.
		 * Note, we can't use expected in read_block() above for this
		 * because we don't know if this is the last block until
		 * after reading.
		 */
		if(start != end && length != SQUASHFS_METADATA_SIZE) {
			ERROR("Xattr block %d should be %d bytes in length, "
				"it is %d bytes\n", i, SQUASHFS_METADATA_SIZE,
				length);
			goto failed3;
		}
	}

	/* swap if necessary the xattr id entries */
	for(i = 0; i < ids; i++)
		SQUASHFS_INSWAP_XATTR_ID(&xattr_ids[i]);

	free(index);

	return ids;

failed3:
	free(xattrs);
failed2:
	free(xattr_ids);
failed1:
	free(index);

	return 0;
}


void free_xattr(struct xattr_list *xattr_list, int count)
{
	int i;

	for(i = 0; i < count; i++)
		free(xattr_list[i].full_name);

	free(xattr_list);
}


/*
 * Construct and return the list of xattr name:value pairs for the passed xattr
 * id
 *
 * There are two users for get_xattr(), Mksquashfs uses it to read the
 * xattrs from the filesystem on appending, and Unsquashfs uses it
 * to retrieve the xattrs for writing to disk.
 *
 * Unfortunately, the two users disagree on what to do with unknown
 * xattr prefixes, Mksquashfs wants to treat this as fatal otherwise
 * this will cause xattrs to be be lost on appending.  Unsquashfs
 * on the otherhand wants to retrieve the xattrs which are known and
 * to ignore the rest, this allows Unsquashfs to cope more gracefully
 * with future versions which may have unknown xattrs, as long as the
 * general xattr structure is adhered to, Unsquashfs should be able
 * to safely ignore unknown xattrs, and to write the ones it knows about,
 * this is better than completely refusing to retrieve all the xattrs.
 *
 * So return an error flag if any unrecognised types were found.
 */
struct xattr_list *get_xattr(int i, unsigned int *count, int *failed)
{
	long long start;
	struct xattr_list *xattr_list = NULL;
	unsigned int offset;
	void *xptr;
	int j, n, res = 1;

	TRACE("get_xattr\n");

	if(xattr_ids[i].count == 0) {
		ERROR("get_xattr: xattr count unexpectedly 0 - corrupt fs?\n");
		*failed = TRUE;
		*count = 0;
		return NULL;
	} else
		*failed = FALSE;

	start = SQUASHFS_XATTR_BLK(xattr_ids[i].xattr) + xattr_table_start;
	offset = SQUASHFS_XATTR_OFFSET(xattr_ids[i].xattr);
	xptr = xattrs + get_xattr_block(start) + offset;

	TRACE("get_xattr: xattr_id %d, count %d, start %lld, offset %d\n", i,
			xattr_ids[i].count, start, offset);

	for(j = 0, n = 0; n < xattr_ids[i].count; n++) {
		struct squashfs_xattr_entry entry;
		struct squashfs_xattr_val val;

		if(res != 0) {
			xattr_list = realloc(xattr_list, (j + 1) *
						sizeof(struct xattr_list));
			if(xattr_list == NULL)
				MEM_ERROR();
		}
			
		SQUASHFS_SWAP_XATTR_ENTRY(xptr, &entry);
		xptr += sizeof(entry);

		res = read_xattr_entry(&xattr_list[j], &entry, xptr);
		if(res == 0) {
			/* unknown type, skip, and set error flag */
			xptr += entry.size;
			SQUASHFS_SWAP_XATTR_VAL(xptr, &val);
			xptr += sizeof(val) + val.vsize;
			*failed = TRUE;
			continue;
		}

		xptr += entry.size;
			
		TRACE("get_xattr: xattr %d, type %d, size %d, name %s\n", j,
			entry.type, entry.size, xattr_list[j].full_name); 

		if(entry.type & SQUASHFS_XATTR_VALUE_OOL) {
			long long xattr;
			void *ool_xptr;

			xptr += sizeof(val);
			SQUASHFS_SWAP_LONG_LONGS(xptr, &xattr, 1);
			xptr += sizeof(xattr);	
			start = SQUASHFS_XATTR_BLK(xattr) + xattr_table_start;
			offset = SQUASHFS_XATTR_OFFSET(xattr);
			ool_xptr = xattrs + get_xattr_block(start) + offset;
			SQUASHFS_SWAP_XATTR_VAL(ool_xptr, &val);
			xattr_list[j].value = ool_xptr + sizeof(val);
		} else {
			SQUASHFS_SWAP_XATTR_VAL(xptr, &val);
			xattr_list[j].value = xptr + sizeof(val);
			xptr += sizeof(val) + val.vsize;
		}

		TRACE("get_xattr: xattr %d, vsize %d\n", j, val.vsize);

		xattr_list[j++].vsize = val.vsize;
	}

	*count = j;
	return xattr_list;
}
