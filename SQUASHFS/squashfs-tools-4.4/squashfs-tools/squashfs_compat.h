#ifndef SQUASHFS_COMPAT
#define SQUASHFS_COMPAT
/*
 * Squashfs
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2014, 2019
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
 * squashfs_compat.h
 */

/*
 * definitions for structures on disk - layout 3.x
 */

#define SQUASHFS_CHECK			2
#define SQUASHFS_CHECK_DATA(flags)		SQUASHFS_BIT(flags, \
						SQUASHFS_CHECK)

/* Max number of uids and gids */
#define SQUASHFS_UIDS			256
#define SQUASHFS_GUIDS			255

struct squashfs_super_block_3 {
	unsigned int		s_magic;
	unsigned int		inodes;
	unsigned int		bytes_used_2;
	unsigned int		uid_start_2;
	unsigned int		guid_start_2;
	unsigned int		inode_table_start_2;
	unsigned int		directory_table_start_2;
	unsigned int		s_major:16;
	unsigned int		s_minor:16;
	unsigned int		block_size_1:16;
	unsigned int		block_log:16;
	unsigned int		flags:8;
	unsigned int		no_uids:8;
	unsigned int		no_guids:8;
	int			mkfs_time /* time of filesystem creation */;
	squashfs_inode		root_inode;
	unsigned int		block_size;
	unsigned int		fragments;
	unsigned int		fragment_table_start_2;
	long long		bytes_used;
	long long		uid_start;
	long long		guid_start;
	long long		inode_table_start;
	long long		directory_table_start;
	long long		fragment_table_start;
	long long		lookup_table_start;
} __attribute__ ((packed));

struct squashfs_dir_index_3 {
	unsigned int		index;
	unsigned int		start_block;
	unsigned char		size;
	unsigned char		name[0];
} __attribute__ ((packed));

struct squashfs_base_inode_header_3 {
	unsigned int		inode_type:4;
	unsigned int		mode:12;
	unsigned int		uid:8;
	unsigned int		guid:8;
	int			mtime;
	unsigned int 		inode_number;
} __attribute__ ((packed));

struct squashfs_ipc_inode_header_3 {
	unsigned int		inode_type:4;
	unsigned int		mode:12;
	unsigned int		uid:8;
	unsigned int		guid:8;
	int			mtime;
	unsigned int 		inode_number;
	unsigned int		nlink;
} __attribute__ ((packed));

struct squashfs_dev_inode_header_3 {
	unsigned int		inode_type:4;
	unsigned int		mode:12;
	unsigned int		uid:8;
	unsigned int		guid:8;
	int			mtime;
	unsigned int 		inode_number;
	unsigned int		nlink;
	unsigned short		rdev;
} __attribute__ ((packed));
	
struct squashfs_symlink_inode_header_3 {
	unsigned int		inode_type:4;
	unsigned int		mode:12;
	unsigned int		uid:8;
	unsigned int		guid:8;
	int			mtime;
	unsigned int 		inode_number;
	unsigned int		nlink;
	unsigned short		symlink_size;
	char			symlink[0];
} __attribute__ ((packed));

struct squashfs_reg_inode_header_3 {
	unsigned int		inode_type:4;
	unsigned int		mode:12;
	unsigned int		uid:8;
	unsigned int		guid:8;
	int			mtime;
	unsigned int 		inode_number;
	squashfs_block		start_block;
	unsigned int		fragment;
	unsigned int		offset;
	unsigned int		file_size;
	unsigned short		block_list[0];
} __attribute__ ((packed));

struct squashfs_lreg_inode_header_3 {
	unsigned int		inode_type:4;
	unsigned int		mode:12;
	unsigned int		uid:8;
	unsigned int		guid:8;
	int			mtime;
	unsigned int 		inode_number;
	unsigned int		nlink;
	squashfs_block		start_block;
	unsigned int		fragment;
	unsigned int		offset;
	long long		file_size;
	unsigned short		block_list[0];
} __attribute__ ((packed));

struct squashfs_dir_inode_header_3 {
	unsigned int		inode_type:4;
	unsigned int		mode:12;
	unsigned int		uid:8;
	unsigned int		guid:8;
	int			mtime;
	unsigned int 		inode_number;
	unsigned int		nlink;
	unsigned int		file_size:19;
	unsigned int		offset:13;
	unsigned int		start_block;
	unsigned int		parent_inode;
} __attribute__  ((packed));

struct squashfs_ldir_inode_header_3 {
	unsigned int		inode_type:4;
	unsigned int		mode:12;
	unsigned int		uid:8;
	unsigned int		guid:8;
	int			mtime;
	unsigned int 		inode_number;
	unsigned int		nlink;
	unsigned int		file_size:27;
	unsigned int		offset:13;
	unsigned int		start_block;
	unsigned int		i_count:16;
	unsigned int		parent_inode;
	struct squashfs_dir_index_3	index[0];
} __attribute__  ((packed));

union squashfs_inode_header_3 {
	struct squashfs_base_inode_header_3	base;
	struct squashfs_dev_inode_header_3	dev;
	struct squashfs_symlink_inode_header_3	symlink;
	struct squashfs_reg_inode_header_3	reg;
	struct squashfs_lreg_inode_header_3	lreg;
	struct squashfs_dir_inode_header_3	dir;
	struct squashfs_ldir_inode_header_3	ldir;
	struct squashfs_ipc_inode_header_3	ipc;
};
	
struct squashfs_dir_entry_3 {
	unsigned int		offset:13;
	unsigned int		type:3;
	unsigned int		size:8;
	int			inode_number:16;
	char			name[0];
} __attribute__ ((packed));

struct squashfs_dir_header_3 {
	unsigned int		count:8;
	unsigned int		start_block;
	unsigned int		inode_number;
} __attribute__ ((packed));

struct squashfs_fragment_entry_3 {
	long long		start_block;
	unsigned int		size;
	unsigned int		pending;
} __attribute__ ((packed));


typedef struct squashfs_super_block_3 squashfs_super_block_3;
typedef struct squashfs_dir_index_3 squashfs_dir_index_3;
typedef struct squashfs_base_inode_header_3 squashfs_base_inode_header_3;
typedef struct squashfs_ipc_inode_header_3 squashfs_ipc_inode_header_3;
typedef struct squashfs_dev_inode_header_3 squashfs_dev_inode_header_3;
typedef struct squashfs_symlink_inode_header_3 squashfs_symlink_inode_header_3;
typedef struct squashfs_reg_inode_header_3 squashfs_reg_inode_header_3;
typedef struct squashfs_lreg_inode_header_3 squashfs_lreg_inode_header_3;
typedef struct squashfs_dir_inode_header_3 squashfs_dir_inode_header_3;
typedef struct squashfs_ldir_inode_header_3 squashfs_ldir_inode_header_3;
typedef struct squashfs_dir_entry_3 squashfs_dir_entry_3;
typedef struct squashfs_dir_header_3 squashfs_dir_header_3;
typedef struct squashfs_fragment_entry_3 squashfs_fragment_entry_3;

/*
 * macros to convert each packed bitfield structure from little endian to big
 * endian and vice versa.  These are needed when creating or using a filesystem
 * on a machine with different byte ordering to the target architecture.
 *
 */

#define SQUASHFS_SWAP_START \
	int bits;\
	int b_pos;\
	unsigned long long val;\
	unsigned char *s;\
	unsigned char *d;

#define SQUASHFS_SWAP_SUPER_BLOCK_3(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_MEMSET(s, d, sizeof(struct squashfs_super_block_3));\
	SQUASHFS_SWAP((s)->s_magic, d, 0, 32);\
	SQUASHFS_SWAP((s)->inodes, d, 32, 32);\
	SQUASHFS_SWAP((s)->bytes_used_2, d, 64, 32);\
	SQUASHFS_SWAP((s)->uid_start_2, d, 96, 32);\
	SQUASHFS_SWAP((s)->guid_start_2, d, 128, 32);\
	SQUASHFS_SWAP((s)->inode_table_start_2, d, 160, 32);\
	SQUASHFS_SWAP((s)->directory_table_start_2, d, 192, 32);\
	SQUASHFS_SWAP((s)->s_major, d, 224, 16);\
	SQUASHFS_SWAP((s)->s_minor, d, 240, 16);\
	SQUASHFS_SWAP((s)->block_size_1, d, 256, 16);\
	SQUASHFS_SWAP((s)->block_log, d, 272, 16);\
	SQUASHFS_SWAP((s)->flags, d, 288, 8);\
	SQUASHFS_SWAP((s)->no_uids, d, 296, 8);\
	SQUASHFS_SWAP((s)->no_guids, d, 304, 8);\
	SQUASHFS_SWAP((s)->mkfs_time, d, 312, 32);\
	SQUASHFS_SWAP((s)->root_inode, d, 344, 64);\
	SQUASHFS_SWAP((s)->block_size, d, 408, 32);\
	SQUASHFS_SWAP((s)->fragments, d, 440, 32);\
	SQUASHFS_SWAP((s)->fragment_table_start_2, d, 472, 32);\
	SQUASHFS_SWAP((s)->bytes_used, d, 504, 64);\
	SQUASHFS_SWAP((s)->uid_start, d, 568, 64);\
	SQUASHFS_SWAP((s)->guid_start, d, 632, 64);\
	SQUASHFS_SWAP((s)->inode_table_start, d, 696, 64);\
	SQUASHFS_SWAP((s)->directory_table_start, d, 760, 64);\
	SQUASHFS_SWAP((s)->fragment_table_start, d, 824, 64);\
	SQUASHFS_SWAP((s)->lookup_table_start, d, 888, 64);\
}

#define SQUASHFS_SWAP_BASE_INODE_CORE_3(s, d, n)\
	SQUASHFS_MEMSET(s, d, n);\
	SQUASHFS_SWAP((s)->inode_type, d, 0, 4);\
	SQUASHFS_SWAP((s)->mode, d, 4, 12);\
	SQUASHFS_SWAP((s)->uid, d, 16, 8);\
	SQUASHFS_SWAP((s)->guid, d, 24, 8);\
	SQUASHFS_SWAP((s)->mtime, d, 32, 32);\
	SQUASHFS_SWAP((s)->inode_number, d, 64, 32);

#define SQUASHFS_SWAP_BASE_INODE_HEADER_3(s, d, n) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_3(s, d, n)\
}

#define SQUASHFS_SWAP_IPC_INODE_HEADER_3(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_3(s, d, \
			sizeof(struct squashfs_ipc_inode_header_3))\
	SQUASHFS_SWAP((s)->nlink, d, 96, 32);\
}

#define SQUASHFS_SWAP_DEV_INODE_HEADER_3(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_3(s, d, \
			sizeof(struct squashfs_dev_inode_header_3)); \
	SQUASHFS_SWAP((s)->nlink, d, 96, 32);\
	SQUASHFS_SWAP((s)->rdev, d, 128, 16);\
}

#define SQUASHFS_SWAP_SYMLINK_INODE_HEADER_3(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_3(s, d, \
			sizeof(struct squashfs_symlink_inode_header_3));\
	SQUASHFS_SWAP((s)->nlink, d, 96, 32);\
	SQUASHFS_SWAP((s)->symlink_size, d, 128, 16);\
}

#define SQUASHFS_SWAP_REG_INODE_HEADER_3(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_3(s, d, \
			sizeof(struct squashfs_reg_inode_header_3));\
	SQUASHFS_SWAP((s)->start_block, d, 96, 64);\
	SQUASHFS_SWAP((s)->fragment, d, 160, 32);\
	SQUASHFS_SWAP((s)->offset, d, 192, 32);\
	SQUASHFS_SWAP((s)->file_size, d, 224, 32);\
}

#define SQUASHFS_SWAP_LREG_INODE_HEADER_3(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_3(s, d, \
			sizeof(struct squashfs_lreg_inode_header_3));\
	SQUASHFS_SWAP((s)->nlink, d, 96, 32);\
	SQUASHFS_SWAP((s)->start_block, d, 128, 64);\
	SQUASHFS_SWAP((s)->fragment, d, 192, 32);\
	SQUASHFS_SWAP((s)->offset, d, 224, 32);\
	SQUASHFS_SWAP((s)->file_size, d, 256, 64);\
}

#define SQUASHFS_SWAP_DIR_INODE_HEADER_3(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_3(s, d, \
			sizeof(struct squashfs_dir_inode_header_3));\
	SQUASHFS_SWAP((s)->nlink, d, 96, 32);\
	SQUASHFS_SWAP((s)->file_size, d, 128, 19);\
	SQUASHFS_SWAP((s)->offset, d, 147, 13);\
	SQUASHFS_SWAP((s)->start_block, d, 160, 32);\
	SQUASHFS_SWAP((s)->parent_inode, d, 192, 32);\
}

#define SQUASHFS_SWAP_LDIR_INODE_HEADER_3(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_3(s, d, \
			sizeof(struct squashfs_ldir_inode_header_3));\
	SQUASHFS_SWAP((s)->nlink, d, 96, 32);\
	SQUASHFS_SWAP((s)->file_size, d, 128, 27);\
	SQUASHFS_SWAP((s)->offset, d, 155, 13);\
	SQUASHFS_SWAP((s)->start_block, d, 168, 32);\
	SQUASHFS_SWAP((s)->i_count, d, 200, 16);\
	SQUASHFS_SWAP((s)->parent_inode, d, 216, 32);\
}

#define SQUASHFS_SWAP_DIR_INDEX_3(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_MEMSET(s, d, sizeof(struct squashfs_dir_index_3));\
	SQUASHFS_SWAP((s)->index, d, 0, 32);\
	SQUASHFS_SWAP((s)->start_block, d, 32, 32);\
	SQUASHFS_SWAP((s)->size, d, 64, 8);\
}

#define SQUASHFS_SWAP_DIR_HEADER_3(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_MEMSET(s, d, sizeof(struct squashfs_dir_header_3));\
	SQUASHFS_SWAP((s)->count, d, 0, 8);\
	SQUASHFS_SWAP((s)->start_block, d, 8, 32);\
	SQUASHFS_SWAP((s)->inode_number, d, 40, 32);\
}

#define SQUASHFS_SWAP_DIR_ENTRY_3(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_MEMSET(s, d, sizeof(struct squashfs_dir_entry_3));\
	SQUASHFS_SWAP((s)->offset, d, 0, 13);\
	SQUASHFS_SWAP((s)->type, d, 13, 3);\
	SQUASHFS_SWAP((s)->size, d, 16, 8);\
	SQUASHFS_SWAP((s)->inode_number, d, 24, 16);\
}

#define SQUASHFS_SWAP_INODE_T_3(s, d) SQUASHFS_SWAP_LONG_LONGS_3(s, d, 1)

#define SQUASHFS_SWAP_SHORTS_3(s, d, n) {\
	int entry;\
	int bit_position;\
	SQUASHFS_SWAP_START\
	SQUASHFS_MEMSET(s, d, n * 2);\
	for(entry = 0, bit_position = 0; entry < n; entry++, bit_position += \
			16)\
		SQUASHFS_SWAP(s[entry], d, bit_position, 16);\
}

#define SQUASHFS_SWAP_INTS_3(s, d, n) {\
	int entry;\
	int bit_position;\
	SQUASHFS_SWAP_START\
	SQUASHFS_MEMSET(s, d, n * 4);\
	for(entry = 0, bit_position = 0; entry < n; entry++, bit_position += \
			32)\
		SQUASHFS_SWAP(s[entry], d, bit_position, 32);\
}

#define SQUASHFS_SWAP_LONG_LONGS_3(s, d, n) {\
	int entry;\
	int bit_position;\
	SQUASHFS_SWAP_START\
	SQUASHFS_MEMSET(s, d, n * 8);\
	for(entry = 0, bit_position = 0; entry < n; entry++, bit_position += \
			64)\
		SQUASHFS_SWAP(s[entry], d, bit_position, 64);\
}

#define SQUASHFS_SWAP_DATA(s, d, n, bits) {\
	int entry;\
	int bit_position;\
	SQUASHFS_SWAP_START\
	SQUASHFS_MEMSET(s, d, n * bits / 8);\
	for(entry = 0, bit_position = 0; entry < n; entry++, bit_position += \
			bits)\
		SQUASHFS_SWAP(s[entry], d, bit_position, bits);\
}

#define SQUASHFS_SWAP_FRAGMENT_INDEXES_3(s, d, n) SQUASHFS_SWAP_LONG_LONGS_3(s, d, n)
#define SQUASHFS_SWAP_LOOKUP_BLOCKS_3(s, d, n) SQUASHFS_SWAP_LONG_LONGS_3(s, d, n)

#define SQUASHFS_SWAP_FRAGMENT_ENTRY_3(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_MEMSET(s, d, sizeof(struct squashfs_fragment_entry_3));\
	SQUASHFS_SWAP((s)->start_block, d, 0, 64);\
	SQUASHFS_SWAP((s)->size, d, 64, 32);\
}

/* fragment and fragment table defines */
#define SQUASHFS_FRAGMENT_BYTES_3(A)	((A) * sizeof(struct squashfs_fragment_entry_3))

#define SQUASHFS_FRAGMENT_INDEX_3(A)	(SQUASHFS_FRAGMENT_BYTES_3(A) / \
					SQUASHFS_METADATA_SIZE)

#define SQUASHFS_FRAGMENT_INDEX_OFFSET_3(A)	(SQUASHFS_FRAGMENT_BYTES_3(A) % \
						SQUASHFS_METADATA_SIZE)

#define SQUASHFS_FRAGMENT_INDEXES_3(A)	((SQUASHFS_FRAGMENT_BYTES_3(A) + \
					SQUASHFS_METADATA_SIZE - 1) / \
					SQUASHFS_METADATA_SIZE)

#define SQUASHFS_FRAGMENT_INDEX_BYTES_3(A)	(SQUASHFS_FRAGMENT_INDEXES_3(A) *\
						sizeof(long long))

/* inode lookup table defines */
#define SQUASHFS_LOOKUP_BYTES_3(A)	((A) * sizeof(squashfs_inode))

#define SQUASHFS_LOOKUP_BLOCK_3(A)		(SQUASHFS_LOOKUP_BYTES_3(A) / \
						SQUASHFS_METADATA_SIZE)

#define SQUASHFS_LOOKUP_BLOCK_OFFSET_3(A)	(SQUASHFS_LOOKUP_BYTES_3(A) % \
						SQUASHFS_METADATA_SIZE)

#define SQUASHFS_LOOKUP_BLOCKS_3(A)	((SQUASHFS_LOOKUP_BYTES_3(A) + \
					SQUASHFS_METADATA_SIZE - 1) / \
					SQUASHFS_METADATA_SIZE)

#define SQUASHFS_LOOKUP_BLOCK_BYTES_3(A)	(SQUASHFS_LOOKUP_BLOCKS(A) *\
					sizeof(long long))

/*
 * definitions for structures on disk - layout 1.x
 */
#define SQUASHFS_TYPES			5
#define SQUASHFS_IPC_TYPE		0

struct squashfs_base_inode_header_1 {
	unsigned int		inode_type:4;
	unsigned int		mode:12; /* protection */
	unsigned int		uid:4; /* index into uid table */
	unsigned int		guid:4; /* index into guid table */
} __attribute__ ((packed));

struct squashfs_ipc_inode_header_1 {
	unsigned int		inode_type:4;
	unsigned int		mode:12; /* protection */
	unsigned int		uid:4; /* index into uid table */
	unsigned int		guid:4; /* index into guid table */
	unsigned int		type:4;
	unsigned int		offset:4;
} __attribute__ ((packed));

struct squashfs_dev_inode_header_1 {
	unsigned int		inode_type:4;
	unsigned int		mode:12; /* protection */
	unsigned int		uid:4; /* index into uid table */
	unsigned int		guid:4; /* index into guid table */
	unsigned short		rdev;
} __attribute__ ((packed));
	
struct squashfs_symlink_inode_header_1 {
	unsigned int		inode_type:4;
	unsigned int		mode:12; /* protection */
	unsigned int		uid:4; /* index into uid table */
	unsigned int		guid:4; /* index into guid table */
	unsigned short		symlink_size;
	char			symlink[0];
} __attribute__ ((packed));

struct squashfs_reg_inode_header_1 {
	unsigned int		inode_type:4;
	unsigned int		mode:12; /* protection */
	unsigned int		uid:4; /* index into uid table */
	unsigned int		guid:4; /* index into guid table */
	int			mtime;
	unsigned int		start_block;
	unsigned int		file_size:32;
	unsigned short		block_list[0];
} __attribute__ ((packed));

struct squashfs_dir_inode_header_1 {
	unsigned int		inode_type:4;
	unsigned int		mode:12; /* protection */
	unsigned int		uid:4; /* index into uid table */
	unsigned int		guid:4; /* index into guid table */
	unsigned int		file_size:19;
	unsigned int		offset:13;
	int			mtime;
	unsigned int		start_block:24;
} __attribute__  ((packed));

union squashfs_inode_header_1 {
	struct squashfs_base_inode_header_1	base;
	struct squashfs_dev_inode_header_1	dev;
	struct squashfs_symlink_inode_header_1	symlink;
	struct squashfs_reg_inode_header_1	reg;
	struct squashfs_dir_inode_header_1	dir;
	struct squashfs_ipc_inode_header_1	ipc;
};

typedef struct squashfs_dir_index_1 squashfs_dir_index_1;
typedef struct squashfs_base_inode_header_1 squashfs_base_inode_header_1;
typedef struct squashfs_ipc_inode_header_1 squashfs_ipc_inode_header_1;
typedef struct squashfs_dev_inode_header_1 squashfs_dev_inode_header_1;
typedef struct squashfs_symlink_inode_header_1 squashfs_symlink_inode_header_1;
typedef struct squashfs_reg_inode_header_1 squashfs_reg_inode_header_1;
typedef struct squashfs_dir_inode_header_1 squashfs_dir_inode_header_1;

#define SQUASHFS_SWAP_BASE_INODE_CORE_1(s, d, n) \
	SQUASHFS_MEMSET(s, d, n);\
	SQUASHFS_SWAP((s)->inode_type, d, 0, 4);\
	SQUASHFS_SWAP((s)->mode, d, 4, 12);\
	SQUASHFS_SWAP((s)->uid, d, 16, 4);\
	SQUASHFS_SWAP((s)->guid, d, 20, 4);

#define SQUASHFS_SWAP_BASE_INODE_HEADER_1(s, d, n) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_1(s, d, n)\
}

#define SQUASHFS_SWAP_IPC_INODE_HEADER_1(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_1(s, d, \
			sizeof(struct squashfs_ipc_inode_header_1));\
	SQUASHFS_SWAP((s)->type, d, 24, 4);\
	SQUASHFS_SWAP((s)->offset, d, 28, 4);\
}

#define SQUASHFS_SWAP_DEV_INODE_HEADER_1(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_1(s, d, \
			sizeof(struct squashfs_dev_inode_header_1));\
	SQUASHFS_SWAP((s)->rdev, d, 24, 16);\
}

#define SQUASHFS_SWAP_SYMLINK_INODE_HEADER_1(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_1(s, d, \
			sizeof(struct squashfs_symlink_inode_header_1));\
	SQUASHFS_SWAP((s)->symlink_size, d, 24, 16);\
}

#define SQUASHFS_SWAP_REG_INODE_HEADER_1(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_1(s, d, \
			sizeof(struct squashfs_reg_inode_header_1));\
	SQUASHFS_SWAP((s)->mtime, d, 24, 32);\
	SQUASHFS_SWAP((s)->start_block, d, 56, 32);\
	SQUASHFS_SWAP((s)->file_size, d, 88, 32);\
}

#define SQUASHFS_SWAP_DIR_INODE_HEADER_1(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_1(s, d, \
			sizeof(struct squashfs_dir_inode_header_1));\
	SQUASHFS_SWAP((s)->file_size, d, 24, 19);\
	SQUASHFS_SWAP((s)->offset, d, 43, 13);\
	SQUASHFS_SWAP((s)->mtime, d, 56, 32);\
	SQUASHFS_SWAP((s)->start_block, d, 88, 24);\
}

/*
 * definitions for structures on disk - layout 2.x
 */
struct squashfs_dir_index_2 {
	unsigned int		index:27;
	unsigned int		start_block:29;
	unsigned char		size;
	unsigned char		name[0];
} __attribute__ ((packed));

struct squashfs_base_inode_header_2 {
	unsigned int		inode_type:4;
	unsigned int		mode:12; /* protection */
	unsigned int		uid:8; /* index into uid table */
	unsigned int		guid:8; /* index into guid table */
} __attribute__ ((packed));

struct squashfs_ipc_inode_header_2 {
	unsigned int		inode_type:4;
	unsigned int		mode:12; /* protection */
	unsigned int		uid:8; /* index into uid table */
	unsigned int		guid:8; /* index into guid table */
} __attribute__ ((packed));

struct squashfs_dev_inode_header_2 {
	unsigned int		inode_type:4;
	unsigned int		mode:12; /* protection */
	unsigned int		uid:8; /* index into uid table */
	unsigned int		guid:8; /* index into guid table */
	unsigned short		rdev;
} __attribute__ ((packed));
	
struct squashfs_symlink_inode_header_2 {
	unsigned int		inode_type:4;
	unsigned int		mode:12; /* protection */
	unsigned int		uid:8; /* index into uid table */
	unsigned int		guid:8; /* index into guid table */
	unsigned short		symlink_size;
	char			symlink[0];
} __attribute__ ((packed));

struct squashfs_reg_inode_header_2 {
	unsigned int		inode_type:4;
	unsigned int		mode:12; /* protection */
	unsigned int		uid:8; /* index into uid table */
	unsigned int		guid:8; /* index into guid table */
	int			mtime;
	unsigned int		start_block;
	unsigned int		fragment;
	unsigned int		offset;
	unsigned int		file_size:32;
	unsigned short		block_list[0];
} __attribute__ ((packed));

struct squashfs_dir_inode_header_2 {
	unsigned int		inode_type:4;
	unsigned int		mode:12; /* protection */
	unsigned int		uid:8; /* index into uid table */
	unsigned int		guid:8; /* index into guid table */
	unsigned int		file_size:19;
	unsigned int		offset:13;
	int			mtime;
	unsigned int		start_block:24;
} __attribute__  ((packed));

struct squashfs_ldir_inode_header_2 {
	unsigned int		inode_type:4;
	unsigned int		mode:12; /* protection */
	unsigned int		uid:8; /* index into uid table */
	unsigned int		guid:8; /* index into guid table */
	unsigned int		file_size:27;
	unsigned int		offset:13;
	int			mtime;
	unsigned int		start_block:24;
	unsigned int		i_count:16;
	struct squashfs_dir_index_2	index[0];
} __attribute__  ((packed));

union squashfs_inode_header_2 {
	struct squashfs_base_inode_header_2	base;
	struct squashfs_dev_inode_header_2	dev;
	struct squashfs_symlink_inode_header_2	symlink;
	struct squashfs_reg_inode_header_2	reg;
	struct squashfs_dir_inode_header_2	dir;
	struct squashfs_ldir_inode_header_2	ldir;
	struct squashfs_ipc_inode_header_2	ipc;
};
	
struct squashfs_dir_header_2 {
	unsigned int		count:8;
	unsigned int		start_block:24;
} __attribute__ ((packed));

struct squashfs_dir_entry_2 {
	unsigned int		offset:13;
	unsigned int		type:3;
	unsigned int		size:8;
	char			name[0];
} __attribute__ ((packed));

struct squashfs_fragment_entry_2 {
	unsigned int		start_block;
	unsigned int		size;
} __attribute__ ((packed));

typedef struct squashfs_dir_index_2 squashfs_dir_index_2;
typedef struct squashfs_base_inode_header_2 squashfs_base_inode_header_2;
typedef struct squashfs_ipc_inode_header_2 squashfs_ipc_inode_header_2;
typedef struct squashfs_dev_inode_header_2 squashfs_dev_inode_header_2;
typedef struct squashfs_symlink_inode_header_2 squashfs_symlink_inode_header_2;
typedef struct squashfs_reg_inode_header_2 squashfs_reg_inode_header_2;
typedef struct squashfs_lreg_inode_header_2 squashfs_lreg_inode_header_2;
typedef struct squashfs_dir_inode_header_2 squashfs_dir_inode_header_2;
typedef struct squashfs_ldir_inode_header_2 squashfs_ldir_inode_header_2;
typedef struct squashfs_dir_entry_2 squashfs_dir_entry_2;
typedef struct squashfs_dir_header_2 squashfs_dir_header_2;
typedef struct squashfs_fragment_entry_2 squashfs_fragment_entry_2;

#define SQUASHFS_SWAP_BASE_INODE_CORE_2(s, d, n)\
	SQUASHFS_MEMSET(s, d, n);\
	SQUASHFS_SWAP((s)->inode_type, d, 0, 4);\
	SQUASHFS_SWAP((s)->mode, d, 4, 12);\
	SQUASHFS_SWAP((s)->uid, d, 16, 8);\
	SQUASHFS_SWAP((s)->guid, d, 24, 8);\

#define SQUASHFS_SWAP_BASE_INODE_HEADER_2(s, d, n) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_2(s, d, n)\
}

#define SQUASHFS_SWAP_IPC_INODE_HEADER_2(s, d) \
	SQUASHFS_SWAP_BASE_INODE_HEADER_2(s, d, sizeof(struct squashfs_ipc_inode_header_2))

#define SQUASHFS_SWAP_DEV_INODE_HEADER_2(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_2(s, d, \
			sizeof(struct squashfs_dev_inode_header_2)); \
	SQUASHFS_SWAP((s)->rdev, d, 32, 16);\
}

#define SQUASHFS_SWAP_SYMLINK_INODE_HEADER_2(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_2(s, d, \
			sizeof(struct squashfs_symlink_inode_header_2));\
	SQUASHFS_SWAP((s)->symlink_size, d, 32, 16);\
}

#define SQUASHFS_SWAP_REG_INODE_HEADER_2(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_2(s, d, \
			sizeof(struct squashfs_reg_inode_header_2));\
	SQUASHFS_SWAP((s)->mtime, d, 32, 32);\
	SQUASHFS_SWAP((s)->start_block, d, 64, 32);\
	SQUASHFS_SWAP((s)->fragment, d, 96, 32);\
	SQUASHFS_SWAP((s)->offset, d, 128, 32);\
	SQUASHFS_SWAP((s)->file_size, d, 160, 32);\
}

#define SQUASHFS_SWAP_DIR_INODE_HEADER_2(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_2(s, d, \
			sizeof(struct squashfs_dir_inode_header_2));\
	SQUASHFS_SWAP((s)->file_size, d, 32, 19);\
	SQUASHFS_SWAP((s)->offset, d, 51, 13);\
	SQUASHFS_SWAP((s)->mtime, d, 64, 32);\
	SQUASHFS_SWAP((s)->start_block, d, 96, 24);\
}

#define SQUASHFS_SWAP_LDIR_INODE_HEADER_2(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_SWAP_BASE_INODE_CORE_2(s, d, \
			sizeof(struct squashfs_ldir_inode_header_2));\
	SQUASHFS_SWAP((s)->file_size, d, 32, 27);\
	SQUASHFS_SWAP((s)->offset, d, 59, 13);\
	SQUASHFS_SWAP((s)->mtime, d, 72, 32);\
	SQUASHFS_SWAP((s)->start_block, d, 104, 24);\
	SQUASHFS_SWAP((s)->i_count, d, 128, 16);\
}

#define SQUASHFS_SWAP_DIR_INDEX_2(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_MEMSET(s, d, sizeof(struct squashfs_dir_index_2));\
	SQUASHFS_SWAP((s)->index, d, 0, 27);\
	SQUASHFS_SWAP((s)->start_block, d, 27, 29);\
	SQUASHFS_SWAP((s)->size, d, 56, 8);\
}
#define SQUASHFS_SWAP_DIR_HEADER_2(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_MEMSET(s, d, sizeof(struct squashfs_dir_header_2));\
	SQUASHFS_SWAP((s)->count, d, 0, 8);\
	SQUASHFS_SWAP((s)->start_block, d, 8, 24);\
}

#define SQUASHFS_SWAP_DIR_ENTRY_2(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_MEMSET(s, d, sizeof(struct squashfs_dir_entry_2));\
	SQUASHFS_SWAP((s)->offset, d, 0, 13);\
	SQUASHFS_SWAP((s)->type, d, 13, 3);\
	SQUASHFS_SWAP((s)->size, d, 16, 8);\
}

#define SQUASHFS_SWAP_FRAGMENT_ENTRY_2(s, d) {\
	SQUASHFS_SWAP_START\
	SQUASHFS_MEMSET(s, d, sizeof(struct squashfs_fragment_entry_2));\
	SQUASHFS_SWAP((s)->start_block, d, 0, 32);\
	SQUASHFS_SWAP((s)->size, d, 32, 32);\
}

#define SQUASHFS_SWAP_FRAGMENT_INDEXES_2(s, d, n) SQUASHFS_SWAP_INTS_3(s, d, n)

/* fragment and fragment table defines */
#define SQUASHFS_FRAGMENT_BYTES_2(A)	((A) * sizeof(struct squashfs_fragment_entry_2))

#define SQUASHFS_FRAGMENT_INDEX_2(A)	(SQUASHFS_FRAGMENT_BYTES_2(A) / \
					SQUASHFS_METADATA_SIZE)

#define SQUASHFS_FRAGMENT_INDEX_OFFSET_2(A)	(SQUASHFS_FRAGMENT_BYTES_2(A) % \
						SQUASHFS_METADATA_SIZE)

#define SQUASHFS_FRAGMENT_INDEXES_2(A)	((SQUASHFS_FRAGMENT_BYTES_2(A) + \
					SQUASHFS_METADATA_SIZE - 1) / \
					SQUASHFS_METADATA_SIZE)

#define SQUASHFS_FRAGMENT_INDEX_BYTES_2(A)	(SQUASHFS_FRAGMENT_INDEXES_2(A) *\
						sizeof(int))
/*
 * macros used to swap each structure entry, taking into account
 * bitfields and different bitfield placing conventions on differing architectures
 */
#if __BYTE_ORDER == __BIG_ENDIAN
	/* convert from little endian to big endian */
#define SQUASHFS_SWAP(value, p, pos, tbits) _SQUASHFS_SWAP(value, p, pos, tbits, b_pos)
#else
	/* convert from big endian to little endian */
#define SQUASHFS_SWAP(value, p, pos, tbits) _SQUASHFS_SWAP(value, p, pos, tbits, 64 - tbits - b_pos)
#endif

#define _SQUASHFS_SWAP(value, p, pos, tbits, SHIFT) {\
	b_pos = pos % 8;\
	val = 0;\
	s = (unsigned char *)p + (pos / 8);\
	d = ((unsigned char *) &val) + 7;\
	for(bits = 0; bits < (tbits + b_pos); bits += 8) \
		*d-- = *s++;\
	value = (val >> (SHIFT));\
}
#define SQUASHFS_MEMSET(s, d, n)	memset(s, 0, n);
#endif
