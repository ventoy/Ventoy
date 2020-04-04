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
 * unsquash-34.c
 *
 * Helper functions used by unsquash-3 and unsquash-4.
 */

#include "unsquashfs.h"

long long *alloc_index_table(int indexes)
{
	static long long *alloc_table = NULL;
	static int alloc_size = 0;
	int length = indexes * sizeof(long long);

	if(alloc_size < length || length == 0) {
		long long *table = realloc(alloc_table, length);

		if(table == NULL && length !=0)
			EXIT_UNSQUASH("alloc_index_table: failed to allocate "
				"index table\n");

		alloc_table = table;
		alloc_size = length;
	}

	return alloc_table;
}
