/*
 * Unsquash a squashfs filesystem.  This is a highly compressed read only
 * filesystem.
 *
 * Copyright (c) 2019
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
 * unsquash-123.c
 *
 * Helper functions used by unsquash-1, unsquash-2 and unsquash-3.
 */

#include "unsquashfs.h"
#include "squashfs_compat.h"

int read_ids(int ids, long long start, long long end, unsigned int **id_table)
{
	/* Note on overflow limits:
	 * Size of ids is 2^8
	 * Max length is 2^8*4 or 1024
	 */
	int res;
	int length = ids * sizeof(unsigned int);

	/*
	 * The size of the index table (length bytes) should match the
	 * table start and end points
	 */
	if(length != (end - start)) {
		ERROR("read_ids: Bad inode count in super block\n");
		return FALSE;
	}

	TRACE("read_ids: no_ids %d\n", ids);

	*id_table = malloc(length);
	if(*id_table == NULL) {
		ERROR("read_ids: failed to allocate uid/gid table\n");
		return FALSE;
	}

	if(swap) {
		unsigned int *sid_table = malloc(length);

		if(sid_table == NULL) {
			ERROR("read_ids: failed to allocate uid/gid table\n");
			return FALSE;
		}

		res = read_fs_bytes(fd, start, length, sid_table);
		if(res == FALSE) {
			ERROR("read_ids: failed to read uid/gid table"
				"\n");
			free(sid_table);
			return FALSE;
		}
		SQUASHFS_SWAP_INTS_3((*id_table), sid_table, ids);
		free(sid_table);
	} else {
		res = read_fs_bytes(fd, start, length, *id_table);
		if(res == FALSE) {
			ERROR("read_ids: failed to read uid/gid table"
				"\n");
			return FALSE;
		}
	}

	return TRUE;
}
