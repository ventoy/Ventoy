/*
 * Create a squashfs filesystem.  This is a highly compressed read only
 * filesystem.
 *
 * Copyright (c) 2008, 2009, 2010, 2012, 2014, 2019
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
 * xattr.c
 */

#define TRUE 1
#define FALSE 0

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/xattr.h>

#include "squashfs_fs.h"
#include "squashfs_swap.h"
#include "mksquashfs.h"
#include "xattr.h"
#include "error.h"
#include "progressbar.h"

/* compressed xattr table */
static char *xattr_table = NULL;
static unsigned int xattr_size = 0;

/* cached uncompressed xattr data */
static char *data_cache = NULL;
static int cache_bytes = 0, cache_size = 0;

/* cached uncompressed xattr id table */
static struct squashfs_xattr_id *xattr_id_table = NULL;
static int xattr_ids = 0;

/* saved compressed xattr table */
unsigned int sxattr_bytes = 0, stotal_xattr_bytes = 0;

/* saved cached uncompressed xattr data */
static char *sdata_cache = NULL;
static int scache_bytes = 0;

/* saved cached uncompressed xattr id table */
static int sxattr_ids = 0;

/* xattr hash table for value duplicate detection */
static struct xattr_list *dupl_value[65536];

/* xattr hash table for id duplicate detection */
static struct dupl_id *dupl_id[65536];

/* file system globals from mksquashfs.c */
extern int no_xattrs, noX;
extern long long bytes;
extern int fd;
extern unsigned int xattr_bytes, total_xattr_bytes;

/* helper functions from mksquashfs.c */
extern unsigned short get_checksum(char *, int, unsigned short);
extern void write_destination(int, long long, int, void *);
extern long long generic_write_table(int, void *, int, void *, int);
extern int mangle(char *, char *, int, int, int, int);
extern char *pathname(struct dir_ent *);

/* helper functions and definitions from read_xattrs.c */
extern int read_xattrs_from_disk(int, struct squashfs_super_block *, int, long long *);
extern struct xattr_list *get_xattr(int, unsigned int *, int *);
extern struct prefix prefix_table[];


static int get_prefix(struct xattr_list *xattr, char *name)
{
	int i;

	xattr->full_name = strdup(name);

	for(i = 0; prefix_table[i].type != -1; i++) {
		struct prefix *p = &prefix_table[i];
		if(strncmp(xattr->full_name, p->prefix, strlen(p->prefix)) == 0)
			break;
	}

	if(prefix_table[i].type != -1) {
		xattr->name = xattr->full_name + strlen(prefix_table[i].prefix);
		xattr->size = strlen(xattr->name);
	}

	return prefix_table[i].type;
}

	
static int read_xattrs_from_system(char *filename, struct xattr_list **xattrs)
{
	ssize_t size, vsize;
	char *xattr_names, *p;
	int i;
	struct xattr_list *xattr_list = NULL;

	while(1) {
		size = llistxattr(filename, NULL, 0);
		if(size <= 0) {
			if(size < 0 && errno != ENOTSUP) {
				ERROR_START("llistxattr for %s failed in "
					"read_attrs, because %s", filename,
					strerror(errno));
				ERROR_EXIT(".  Ignoring");
			}
			return 0;
		}

		xattr_names = malloc(size);
		if(xattr_names == NULL)
			MEM_ERROR();

		size = llistxattr(filename, xattr_names, size);
		if(size < 0) {
			free(xattr_names);
			if(errno == ERANGE)
				/* xattr list grew?  Try again */
				continue;
			else {
				ERROR_START("llistxattr for %s failed in "
					"read_attrs, because %s", filename,
					strerror(errno));
				ERROR_EXIT(".  Ignoring");
				return 0;
			}
		}

		break;
	}

	for(i = 0, p = xattr_names; p < xattr_names + size; i++) {
		struct xattr_list *x = realloc(xattr_list, (i + 1) *
						sizeof(struct xattr_list));
		if(x == NULL)
			MEM_ERROR();
		xattr_list = x;

		xattr_list[i].type = get_prefix(&xattr_list[i], p);
		p += strlen(p) + 1;
		if(xattr_list[i].type == -1) {
			ERROR("Unrecognised xattr prefix %s\n",
				xattr_list[i].full_name);
			free(xattr_list[i].full_name);
			i--;
			continue;
		}

		while(1) {
			vsize = lgetxattr(filename, xattr_list[i].full_name,
								NULL, 0);
			if(vsize < 0) {
				ERROR_START("lgetxattr failed for %s in "
					"read_attrs, because %s", filename,
					strerror(errno));
				ERROR_EXIT(".  Ignoring");
				free(xattr_list[i].full_name);
				goto failed;
			}

			xattr_list[i].value = malloc(vsize);
			if(xattr_list[i].value == NULL)
				MEM_ERROR();

			vsize = lgetxattr(filename, xattr_list[i].full_name,
						xattr_list[i].value, vsize);
			if(vsize < 0) {
				free(xattr_list[i].value);
				if(errno == ERANGE)
					/* xattr grew?  Try again */
					continue;
				else {
					ERROR_START("lgetxattr failed for %s "
						"in read_attrs, because %s",
						filename, strerror(errno));
					ERROR_EXIT(".  Ignoring");
					free(xattr_list[i].full_name);
					goto failed;
				}
			}
			
			break;
		}
		xattr_list[i].vsize = vsize;

		TRACE("read_xattrs_from_system: filename %s, xattr name %s,"
			" vsize %d\n", filename, xattr_list[i].full_name,
			xattr_list[i].vsize);
	}
	free(xattr_names);
	if(i > 0)
		*xattrs = xattr_list;
	else
		free(xattr_list);
	return i;

failed:
	while(--i >= 0) {
		free(xattr_list[i].full_name);
		free(xattr_list[i].value);
	}
	free(xattr_list);
	free(xattr_names);
	return 0;
}


static int get_xattr_size(struct xattr_list *xattr)
{
	int size = sizeof(struct squashfs_xattr_entry) +
		sizeof(struct squashfs_xattr_val) + xattr->size;

	if(xattr->type & XATTR_VALUE_OOL)
		size += XATTR_VALUE_OOL_SIZE;
	else
		size += xattr->vsize;

	return size;
}


static void *get_xattr_space(unsigned int req_size, long long *disk)
{
	int data_space;
	unsigned short c_byte;

	/*
	 * Move and compress cached uncompressed data into xattr table.
	 */
	while(cache_bytes >= SQUASHFS_METADATA_SIZE) {
		if((xattr_size - xattr_bytes) <
				((SQUASHFS_METADATA_SIZE << 1)) + 2) {
			xattr_table = realloc(xattr_table, xattr_size +
				(SQUASHFS_METADATA_SIZE << 1) + 2);
			if(xattr_table == NULL)
				MEM_ERROR();
			xattr_size += (SQUASHFS_METADATA_SIZE << 1) + 2;
		}

		c_byte = mangle(xattr_table + xattr_bytes + BLOCK_OFFSET,
			data_cache, SQUASHFS_METADATA_SIZE,
			SQUASHFS_METADATA_SIZE, noX, 0);
		TRACE("Xattr block @ 0x%x, size %d\n", xattr_bytes, c_byte);
		SQUASHFS_SWAP_SHORTS(&c_byte, xattr_table + xattr_bytes, 1);
		xattr_bytes += SQUASHFS_COMPRESSED_SIZE(c_byte) + BLOCK_OFFSET;
		memmove(data_cache, data_cache + SQUASHFS_METADATA_SIZE,
			cache_bytes - SQUASHFS_METADATA_SIZE);
		cache_bytes -= SQUASHFS_METADATA_SIZE;
	}

	/*
	 * Ensure there's enough space in the uncompressed data cache
	 */
	data_space = cache_size - cache_bytes;
	if(data_space < req_size) {
			int realloc_size = req_size - data_space;
			data_cache = realloc(data_cache, cache_size +
				realloc_size);
			if(data_cache == NULL)
				MEM_ERROR();
			cache_size += realloc_size;
	}

	if(disk)
		*disk = ((long long) xattr_bytes << 16) | cache_bytes;
	cache_bytes += req_size;
	return data_cache + cache_bytes - req_size;
}


static struct dupl_id *check_id_dupl(struct xattr_list *xattr_list, int xattrs)
{
	struct dupl_id *entry;
	int i;
	unsigned short checksum = 0;

	/* compute checksum over all xattrs */
	for(i = 0; i < xattrs; i++) {
		struct xattr_list *xattr = &xattr_list[i];

		checksum = get_checksum(xattr->full_name,
					strlen(xattr->full_name), checksum);
		checksum = get_checksum(xattr->value,
					xattr->vsize, checksum);
	}

	for(entry = dupl_id[checksum]; entry; entry = entry->next) {
		if (entry->xattrs != xattrs)
			continue;

		for(i = 0; i < xattrs; i++) {
			struct xattr_list *xattr = &xattr_list[i];
			struct xattr_list *dup_xattr = &entry->xattr_list[i];

			if(strcmp(xattr->full_name, dup_xattr->full_name))
				break;

			if(xattr->vsize != dup_xattr->vsize)
				break;

			if(memcmp(xattr->value, dup_xattr->value, xattr->vsize))
				break;
		}
		
		if(i == xattrs)
			break;
	}

	if(entry == NULL) {
		/* no duplicate exists */
		entry = malloc(sizeof(*entry));
		if(entry == NULL)
			MEM_ERROR();
		entry->xattrs = xattrs;
		entry->xattr_list = xattr_list;
		entry->xattr_id = SQUASHFS_INVALID_XATTR;
		entry->next = dupl_id[checksum];
		dupl_id[checksum] = entry;
	}
		
	return entry;
}


static void check_value_dupl(struct xattr_list *xattr)
{
	struct xattr_list *entry;

	if(xattr->vsize < XATTR_VALUE_OOL_SIZE)
		return;

	/* Check if this is a duplicate of an existing value */
	xattr->vchecksum = get_checksum(xattr->value, xattr->vsize, 0);
	for(entry = dupl_value[xattr->vchecksum]; entry; entry = entry->vnext) {
		if(entry->vsize != xattr->vsize)
			continue;
		
		if(memcmp(entry->value, xattr->value, xattr->vsize) == 0)
			break;
	}

	if(entry == NULL) {
		/*
		 * No duplicate exists, add to hash table, and mark as
		 * requiring writing
		 */
		xattr->vnext = dupl_value[xattr->vchecksum];
		dupl_value[xattr->vchecksum] = xattr;
		xattr->ool_value = SQUASHFS_INVALID_BLK;
	} else {
		/*
		 * Duplicate exists, make type XATTR_VALUE_OOL, and
		 * remember where the duplicate is
		 */
		xattr->type |= XATTR_VALUE_OOL;
		xattr->ool_value = entry->ool_value;
		/* on appending don't free duplicate values because the
		 * duplicate value already points to the non-duplicate value */
		if(xattr->value != entry->value) {
			free(xattr->value);
			xattr->value = entry->value;
		}
	}
}


static int get_xattr_id(int xattrs, struct xattr_list *xattr_list,
		long long xattr_disk, struct dupl_id *xattr_dupl)
{
	int i, size = 0;
	struct squashfs_xattr_id *xattr_id;

	xattr_id_table = realloc(xattr_id_table, (xattr_ids + 1) *
		sizeof(struct squashfs_xattr_id));
	if(xattr_id_table == NULL)
		MEM_ERROR();

	/* get total uncompressed size of xattr data, needed for stat */
	for(i = 0; i < xattrs; i++)
		size += strlen(xattr_list[i].full_name) + 1 +
			xattr_list[i].vsize;

	xattr_id = &xattr_id_table[xattr_ids];
	xattr_id->xattr = xattr_disk;
	xattr_id->count = xattrs;
	xattr_id->size = size;

	/*
	 * keep track of total uncompressed xattr data, needed for mksquashfs
	 * file system summary
	 */
	total_xattr_bytes += size;

	xattr_dupl->xattr_id = xattr_ids ++;
	return xattr_dupl->xattr_id;
}
	

long long write_xattrs()
{
	unsigned short c_byte;
	int i, avail_bytes;
	char *datap = data_cache;
	long long start_bytes = bytes;
	struct squashfs_xattr_table header;

	if(xattr_ids == 0)
		return SQUASHFS_INVALID_BLK;

	/*
	 * Move and compress cached uncompressed data into xattr table.
	 */
	while(cache_bytes) {
		if((xattr_size - xattr_bytes) <
				((SQUASHFS_METADATA_SIZE << 1)) + 2) {
			xattr_table = realloc(xattr_table, xattr_size +
				(SQUASHFS_METADATA_SIZE << 1) + 2);
			if(xattr_table == NULL)
				MEM_ERROR();
			xattr_size += (SQUASHFS_METADATA_SIZE << 1) + 2;
		}

		avail_bytes = cache_bytes > SQUASHFS_METADATA_SIZE ?
			SQUASHFS_METADATA_SIZE : cache_bytes;
		c_byte = mangle(xattr_table + xattr_bytes + BLOCK_OFFSET, datap,
			avail_bytes, SQUASHFS_METADATA_SIZE, noX, 0);
		TRACE("Xattr block @ 0x%x, size %d\n", xattr_bytes, c_byte);
		SQUASHFS_SWAP_SHORTS(&c_byte, xattr_table + xattr_bytes, 1);
		xattr_bytes += SQUASHFS_COMPRESSED_SIZE(c_byte) + BLOCK_OFFSET;
		datap += avail_bytes;
		cache_bytes -= avail_bytes;
	}

	/*
	 * Write compressed xattr table to file system
	 */
	write_destination(fd, bytes, xattr_bytes, xattr_table);
        bytes += xattr_bytes;

	/*
	 * Swap if necessary the xattr id table
	 */
	for(i = 0; i < xattr_ids; i++)
		SQUASHFS_INSWAP_XATTR_ID(&xattr_id_table[i]);

	header.xattr_ids = xattr_ids;
	header.xattr_table_start = start_bytes;
	SQUASHFS_INSWAP_XATTR_TABLE(&header);

	return generic_write_table(xattr_ids * sizeof(struct squashfs_xattr_id),
		xattr_id_table, sizeof(header), &header, noX);
}


int generate_xattrs(int xattrs, struct xattr_list *xattr_list)
{
	int total_size, i;
	int xattr_value_max;
	void *xp;
	long long xattr_disk;
	struct dupl_id *xattr_dupl;

	/*
	 * check if the file xattrs are a complete duplicate of a pre-existing
	 * id
	 */
	xattr_dupl = check_id_dupl(xattr_list, xattrs);
	if(xattr_dupl->xattr_id != SQUASHFS_INVALID_XATTR)
		return xattr_dupl->xattr_id;
	 
	/*
	 * Scan the xattr_list deciding which type to assign to each
	 * xattr.  The choice is fairly straightforward, and depends on the
	 * size of each xattr name/value and the overall size of the
	 * resultant xattr list stored in the xattr metadata table.
	 *
	 * Choices are whether to store data inline or out of line.
	 *
	 * The overall goal is to optimise xattr scanning and lookup, and
	 * to enable the file system layout to scale from a couple of
	 * small xattr name/values to a large number of large xattr
	 * names/values without affecting performance.  While hopefully
	 * enabling the common case of a couple of small xattr name/values
	 * to be stored efficiently
	 *
	 * Code repeatedly scans, doing the following
	 *		move xattr data out of line if it exceeds
	 *		xattr_value_max.  Where xattr_value_max is
	 *		initially XATTR_INLINE_MAX.  If the final uncompressed
	 *		xattr list is larger than XATTR_TARGET_MAX then more
	 *		aggressively move xattr data out of line by repeatedly
	 *	 	setting inline threshold to 1/2, then 1/4, 1/8 of
	 *		XATTR_INLINE_MAX until target achieved or there's
	 *		nothing left to move out of line
	 */
	xattr_value_max = XATTR_INLINE_MAX;
	while(1) {
		for(total_size = 0, i = 0; i < xattrs; i++) {
			struct xattr_list *xattr = &xattr_list[i];
			xattr->type &= XATTR_PREFIX_MASK; /* all inline */
			if (xattr->vsize > xattr_value_max)
				xattr->type |= XATTR_VALUE_OOL;

			total_size += get_xattr_size(xattr);
		}

		/*
		 * If the total size of the uncompressed xattr list is <=
		 * XATTR_TARGET_MAX we're done
		 */
		if(total_size <= XATTR_TARGET_MAX)
			break;

		if(xattr_value_max == XATTR_VALUE_OOL_SIZE)
			break;

		/*
		 * Inline target not yet at minimum and so reduce it, and
		 * try again
		 */
		xattr_value_max /= 2;
		if(xattr_value_max < XATTR_VALUE_OOL_SIZE)
			xattr_value_max = XATTR_VALUE_OOL_SIZE;
	}

	/*
	 * Check xattr values for duplicates
	 */
	for(i = 0; i < xattrs; i++) {
		check_value_dupl(&xattr_list[i]);
	}

	/*
	 * Add each out of line value to the file system xattr table
	 * if it doesn't already exist as a duplicate
	 */
	for(i = 0; i < xattrs; i++) {
		struct xattr_list *xattr = &xattr_list[i];

		if((xattr->type & XATTR_VALUE_OOL) &&
				(xattr->ool_value == SQUASHFS_INVALID_BLK)) {
			struct squashfs_xattr_val val;
			int size = sizeof(val) + xattr->vsize;
			xp = get_xattr_space(size, &xattr->ool_value);
			val.vsize = xattr->vsize;
			SQUASHFS_SWAP_XATTR_VAL(&val, xp);
			memcpy(xp + sizeof(val), xattr->value, xattr->vsize);
		}
	}

	/*
	 * Create xattr list and add to file system xattr table
	 */
	get_xattr_space(0, &xattr_disk);
	for(i = 0; i < xattrs; i++) {
		struct xattr_list *xattr = &xattr_list[i];
		struct squashfs_xattr_entry entry;
		struct squashfs_xattr_val val;

		xp = get_xattr_space(sizeof(entry) + xattr->size, NULL);
		entry.type = xattr->type;
		entry.size = xattr->size;
		SQUASHFS_SWAP_XATTR_ENTRY(&entry, xp);
		memcpy(xp + sizeof(entry), xattr->name, xattr->size);

		if(xattr->type & XATTR_VALUE_OOL) {
			int size = sizeof(val) + XATTR_VALUE_OOL_SIZE;
			xp = get_xattr_space(size, NULL);
			val.vsize = XATTR_VALUE_OOL_SIZE;
			SQUASHFS_SWAP_XATTR_VAL(&val, xp);
			SQUASHFS_SWAP_LONG_LONGS(&xattr->ool_value, xp +
				sizeof(val), 1);
		} else {
			int size = sizeof(val) + xattr->vsize;
			xp = get_xattr_space(size, &xattr->ool_value);
			val.vsize = xattr->vsize;
			SQUASHFS_SWAP_XATTR_VAL(&val, xp);
			memcpy(xp + sizeof(val), xattr->value, xattr->vsize);
		}
	}

	/*
	 * Add to xattr id lookup table
	 */
	return get_xattr_id(xattrs, xattr_list, xattr_disk, xattr_dupl);
}


int read_xattrs(void *d)
{
	struct dir_ent *dir_ent = d;
	struct inode_info *inode = dir_ent->inode;
	char *filename = pathname(dir_ent);
	struct xattr_list *xattr_list;
	int xattrs;

	if(no_xattrs || IS_PSEUDO(inode) || inode->root_entry)
		return SQUASHFS_INVALID_XATTR;

	xattrs = read_xattrs_from_system(filename, &xattr_list);
	if(xattrs == 0)
		return SQUASHFS_INVALID_XATTR;

	return generate_xattrs(xattrs, xattr_list);
}


/*
 * Add the existing xattr ids and xattr metadata in the file system being
 * appended to, to the in-memory xattr cache.  This allows duplicate checking to
 * take place against the xattrs already in the file system being appended to,
 * and ensures the pre-existing xattrs are written out along with any new xattrs
 */
int get_xattrs(int fd, struct squashfs_super_block *sBlk)
{
	int ids, res, i, id;
	unsigned int count;

	TRACE("get_xattrs\n");

	res = read_xattrs_from_disk(fd, sBlk, FALSE, NULL);
	if(res == SQUASHFS_INVALID_BLK || res == 0)
		goto done;
	ids = res;

	/*
	 * for each xattr id read and construct its list of xattr
	 * name:value pairs, and add them to the in-memory xattr cache
	 */
	for(i = 0; i < ids; i++) {
		struct xattr_list *xattr_list = get_xattr(i, &count, &res);
		if(res) {
			free_xattr(xattr_list, count);
			return FALSE;
		}
		id = generate_xattrs(count, xattr_list);

		/*
		 * Sanity check, the new xattr id should be the same as the
		 * xattr id in the original file system
		 */
		if(id != i) {
			ERROR("BUG, different xattr_id in get_xattrs\n");
			res = 0;
			goto done;
		}
	}

done:
	return res;
}


/*
 * Save current state of xattrs, needed for restoring state in the event of an
 * abort in appending
 */
void save_xattrs()
{
	/* save the current state of the compressed xattr data */
	sxattr_bytes = xattr_bytes;
	stotal_xattr_bytes = total_xattr_bytes;

	/*
	 * save the current state of the cached uncompressed xattr data.
	 * Note we have to save the contents of the data cache because future
	 * operations will delete the current contents
	 */
	sdata_cache = malloc(cache_bytes);
	if(sdata_cache == NULL)
		MEM_ERROR();

	memcpy(sdata_cache, data_cache, cache_bytes);
	scache_bytes = cache_bytes;

	/* save the current state of the xattr id table */
	sxattr_ids = xattr_ids;
}


/*
 * Restore xattrs in the event of an abort in appending
 */
void restore_xattrs()
{
	/* restore the state of the compressed xattr data */
	xattr_bytes = sxattr_bytes;
	total_xattr_bytes = stotal_xattr_bytes;

	/* restore the state of the uncomoressed xattr data */
	memcpy(data_cache, sdata_cache, scache_bytes);
	cache_bytes = scache_bytes;

	/* restore the state of the xattr id table */
	xattr_ids = sxattr_ids;
}
