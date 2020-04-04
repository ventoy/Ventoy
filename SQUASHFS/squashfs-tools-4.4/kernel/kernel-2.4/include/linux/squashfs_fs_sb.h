#ifndef SQUASHFS_FS_SB
#define SQUASHFS_FS_SB
/*
 * Squashfs
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006
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
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * squashfs_fs_sb.h
 */

#include <linux/squashfs_fs.h>
#include <linux/zlib.h>

struct squashfs_cache {
	long long	block;
	int		length;
	long long	next_index;
	char		*data;
};

struct squashfs_fragment_cache {
	long long	block;
	int		length;
	unsigned int	locked;
	char		*data;
};

struct squashfs_sb_info {
	struct squashfs_super_block	sblk;
	int			devblksize;
	int			devblksize_log2;
	int			swap;
	struct squashfs_cache	*block_cache;
	struct squashfs_fragment_cache	*fragment;
	int			next_cache;
	int			next_fragment;
	int			next_meta_index;
	unsigned int		*uid;
	unsigned int		*guid;
	long long		*fragment_index;
	unsigned int		*fragment_index_2;
	unsigned int		read_size;
	char			*read_data;
	char			*read_page;
	struct semaphore	read_data_mutex;
	struct semaphore	read_page_mutex;
	struct semaphore	block_cache_mutex;
	struct semaphore	fragment_mutex;
	struct semaphore	meta_index_mutex;
	wait_queue_head_t	waitq;
	wait_queue_head_t	fragment_wait_queue;
	struct meta_index	*meta_index;
	z_stream		stream;
	struct inode		*(*iget)(struct super_block *s,  squashfs_inode_t
				inode);
	long long		(*read_blocklist)(struct inode *inode, int
				index, int readahead_blks, char *block_list,
				unsigned short **block_p, unsigned int *bsize);
	int			(*read_fragment_index_table)(struct super_block *s);
};
#endif
