#ifndef UNSQUASHFS_H
#define UNSQUASHFS_H
/*
 * Unsquash a squashfs filesystem.  This is a highly compressed read only
 * filesystem.
 *
 * Copyright (c) 2009, 2010, 2013, 2014, 2019
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
 * unsquashfs.h
 */

#define TRUE 1
#define FALSE 0
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <utime.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <regex.h>
#include <signal.h>
#include <pthread.h>
#include <math.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#ifndef linux
#define __BYTE_ORDER BYTE_ORDER
#define __BIG_ENDIAN BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#else
#include <endian.h>
#endif

#include "squashfs_fs.h"
#include "error.h"

#define CALCULATE_HASH(start)	(start & 0xffff)

/*
 * Unified superblock containing fields for all superblocks
 */
struct super_block {
	struct squashfs_super_block s;
	/* fields only used by squashfs 3 and earlier layouts */
	unsigned int		no_uids;
	unsigned int		no_guids;
	long long		uid_start;
	long long		guid_start;
};

struct hash_table_entry {
	long long	start;
	long long	bytes;
	struct hash_table_entry *next;
};

struct inode {
	int blocks;
	char *block_ptr;
	long long data;
	int fragment;
	int frag_bytes;
	gid_t gid;
	int inode_number;
	int mode;
	int offset;
	long long start;
	char *symlink;
	time_t time;
	int type;
	uid_t uid;
	char sparse;
	unsigned int xattr;
};

typedef struct squashfs_operations {
	struct dir *(*opendir)(unsigned int block_start,
		unsigned int offset, struct inode **i);
	void (*read_fragment)(unsigned int fragment, long long *start_block,
		int *size);
	void (*read_block_list)(unsigned int *block_list, char *block_ptr,
		int blocks);
	struct inode *(*read_inode)(unsigned int start_block,
		unsigned int offset);
} squashfs_operations;

struct test {
	int mask;
	int value;
	int position;
	char mode;
};


/* Cache status struct.  Caches are used to keep
  track of memory buffers passed between different threads */
struct cache {
	int	max_buffers;
	int	count;
	int	used;
	int	buffer_size;
	int	wait_free;
	int	wait_pending;
	pthread_mutex_t	mutex;
	pthread_cond_t wait_for_free;
	pthread_cond_t wait_for_pending;
	struct cache_entry *free_list;
	struct cache_entry *hash_table[65536];
};

/* struct describing a cache entry passed between threads */
struct cache_entry {
	struct cache *cache;
	long long block;
	int	size;
	int	used;
	int error;
	int	pending;
	struct cache_entry *hash_next;
	struct cache_entry *hash_prev;
	struct cache_entry *free_next;
	struct cache_entry *free_prev;
	char *data;
};

/* struct describing queues used to pass data between threads */
struct queue {
	int	size;
	int	readp;
	int	writep;
	pthread_mutex_t	mutex;
	pthread_cond_t empty;
	pthread_cond_t full;
	void **data;
};

/* default size of fragment buffer in Mbytes */
#define FRAGMENT_BUFFER_DEFAULT 256
/* default size of data buffer in Mbytes */
#define DATA_BUFFER_DEFAULT 256

#define DIR_ENT_SIZE	16

struct dir_ent	{
	char		name[SQUASHFS_NAME_LEN + 1];
	unsigned int	start_block;
	unsigned int	offset;
	unsigned int	type;
};

struct dir {
	int		dir_count;
	int 		cur_entry;
	unsigned int	mode;
	uid_t		uid;
	gid_t		guid;
	unsigned int	mtime;
	unsigned int xattr;
	struct dir_ent	*dirs;
};

struct file_entry {
	int offset;
	int size;
	struct cache_entry *buffer;
};


struct squashfs_file {
	int fd;
	int blocks;
	long long file_size;
	int mode;
	uid_t uid;
	gid_t gid;
	time_t time;
	char *pathname;
	char sparse;
	unsigned int xattr;
};

struct path_entry {
	char *name;
	regex_t *preg;
	struct pathname *paths;
};

struct pathname {
	int names;
	struct path_entry *name;
};

struct pathnames {
	int count;
	struct pathname *path[0];
};
#define PATHS_ALLOC_SIZE 10

/* globals */
extern struct super_block sBlk;
extern int swap;
extern struct hash_table_entry *inode_table_hash[65536],
	*directory_table_hash[65536];
extern pthread_mutex_t screen_mutex;
extern int progress_enabled;
extern int inode_number;
extern int lookup_type[];
extern int fd;
extern int no_xattrs;
extern struct queue *to_reader, *to_inflate, *to_writer;
extern struct cache *fragment_cache, *data_cache;

/* unsquashfs.c */
extern void *read_inode_table(long long, long long);
extern void *read_directory_table(long long, long long);
extern long long lookup_entry(struct hash_table_entry **, long long);
extern int read_fs_bytes(int fd, long long, int, void *);
extern int read_block(int, long long, long long *, int, void *);
extern void enable_progress_bar();
extern void disable_progress_bar();
extern void dump_queue(struct queue *);
extern void dump_cache(struct cache *);

/* unsquash-1.c */
extern squashfs_operations *read_filesystem_tables_1();

/* unsquash-2.c */
extern squashfs_operations *read_filesystem_tables_2();

/* unsquash-3.c */
extern squashfs_operations *read_filesystem_tables_3();

/* unsquash-4.c */
extern squashfs_operations *read_filesystem_tables_4();

/* unsquash-123.c */
extern int read_ids(int, long long, long long, unsigned int **);

/* unsquash-34.c */
extern long long *alloc_index_table(int);
#endif
