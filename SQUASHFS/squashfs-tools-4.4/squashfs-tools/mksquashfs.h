#ifndef MKSQUASHFS_H
#define MKSQUASHFS_H
/*
 * Squashfs
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011
 * 2012, 2013, 2014, 2019
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
 * mksquashfs.h
 *
 */

struct dir_info {
	char			*pathname;
	char			*subpath;
	unsigned int		count;
	unsigned int		directory_count;
	int			depth;
	unsigned int		excluded;
	char			dir_is_ldir;
	struct dir_ent		*dir_ent;
	struct dir_ent		*list;
	DIR			*linuxdir;
};

struct dir_ent {
	char			*name;
	char			*source_name;
	char			*nonstandard_pathname;
	struct inode_info	*inode;
	struct dir_info		*dir;
	struct dir_info		*our_dir;
	struct dir_ent		*next;
};

struct inode_info {
	struct stat		buf;
	struct inode_info	*next;
	squashfs_inode		inode;
	unsigned int		inode_number;
	unsigned int		nlink;
	int			pseudo_id;
	char			type;
	char			read;
	char			root_entry;
	char			pseudo_file;
	char			no_fragments;
	char			always_use_fragments;
	char			noD;
	char			noF;
	char			symlink[0];
};

/* in memory file info */
struct file_info {
	long long		file_size;
	long long		bytes;
	long long		start;
	unsigned int		*block_list;
	struct file_info	*next;
	struct fragment		*fragment;
	unsigned short		checksum;
	unsigned short		fragment_checksum;
	char			have_frag_checksum;
	char			have_checksum;
};

/* fragment block data structures */
struct fragment {
	unsigned int		index;
	int			offset;
	int			size;
};

/* in memory uid tables */
#define ID_ENTRIES 256
#define ID_HASH(id) (id & (ID_ENTRIES - 1))
#define ISA_UID 1
#define ISA_GID 2

struct id {
	unsigned int id;
	int	index;
	char	flags;
	struct id *next;
};

/* fragment to file mapping used when appending */
struct append_file {
	struct file_info *file;
	struct append_file *next;
};

#define PSEUDO_FILE_OTHER	1
#define PSEUDO_FILE_PROCESS	2

#define IS_PSEUDO(a)		((a)->pseudo_file)
#define IS_PSEUDO_PROCESS(a)	((a)->pseudo_file & PSEUDO_FILE_PROCESS)
#define IS_PSEUDO_OTHER(a)	((a)->pseudo_file & PSEUDO_FILE_OTHER)

/*
 * Amount of physical memory to use by default, and the default queue
 * ratios
 */
#define SQUASHFS_TAKE 4
#define SQUASHFS_READQ_MEM 4
#define SQUASHFS_BWRITEQ_MEM 4
#define SQUASHFS_FWRITEQ_MEM 4

/*
 * Lowest amount of physical memory considered viable for Mksquashfs
 * to run in Mbytes
 */
#define SQUASHFS_LOWMEM 64

/* offset of data in compressed metadata blocks (allowing room for
 * compressed size */
#define BLOCK_OFFSET 2

#ifdef REPRODUCIBLE_DEFAULT
#define NOREP_STR
#define REP_STR " (default)"
#define REP_DEF 1
#else
#define NOREP_STR " (default)"
#define REP_STR
#define REP_DEF 0
#endif

extern struct cache *reader_buffer, *fragment_buffer, *reserve_cache;
struct cache *bwriter_buffer, *fwriter_buffer;
extern struct queue *to_reader, *to_deflate, *to_writer, *from_writer,
	*to_frag, *locked_fragment, *to_process_frag;
extern struct append_file **file_mapping;
extern struct seq_queue *to_main, *to_order;
extern pthread_mutex_t fragment_mutex, dup_mutex;
extern struct squashfs_fragment_entry *fragment_table;
extern struct compressor *comp;
extern int block_size;
extern struct file_info *dupl[];
extern int read_fs_bytes(int, long long, int, void *);
extern void add_file(long long, long long, long long, unsigned int *, int,
	unsigned int, int, int);
extern struct id *create_id(unsigned int);
extern unsigned int get_uid(unsigned int);
extern unsigned int get_guid(unsigned int);
extern int read_bytes(int, void *, int);
extern unsigned short get_checksum_mem(char *, int);
extern int reproducible;
#endif
