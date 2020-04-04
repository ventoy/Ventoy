#ifndef SQUASHFS_FS_I
#define SQUASHFS_FS_I
/*
 * Squashfs
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008
 * Phillip Lougher <phillip@lougher.demon.co.uk>
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
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * squashfs_fs_i.h
 */

struct squashfs_inode_info {
	long long	start;
	int		offset;
	union {
		struct {
			long long	fragment_block;
			int		fragment_size;
			int		fragment_offset;
			long long	block_list_start;
		};
		struct {
			long long	dir_idx_start;
			int		dir_idx_offset;
			int		dir_idx_cnt;
			int		parent;
		};
	};
	struct inode	vfs_inode;
};
#endif
