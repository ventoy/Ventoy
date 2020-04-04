#ifndef SQUASHFS_FS
#define SQUASHFS_FS
/*
 * Squashfs
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012,
 * 2013, 2014, 2017, 2019
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
 * squashfs_fs.h
 */

#define SQUASHFS_CACHED_FRAGMENTS	CONFIG_SQUASHFS_FRAGMENT_CACHE_SIZE	
#define SQUASHFS_MAJOR			4
#define SQUASHFS_MINOR			0
#define SQUASHFS_MAGIC			0x73717368
#define SQUASHFS_MAGIC_SWAP		0x68737173
#define SQUASHFS_START			0

/* size of metadata (inode and directory) blocks */
#define SQUASHFS_METADATA_SIZE		8192
#define SQUASHFS_METADATA_LOG		13

/* default size of data blocks */
#define SQUASHFS_FILE_SIZE		131072

#define SQUASHFS_FILE_MAX_SIZE		1048576
#define SQUASHFS_FILE_MAX_LOG		20

/* Max number of uids and gids */
#define SQUASHFS_IDS			65536

/* Max length of filename (not 255) */
#define SQUASHFS_NAME_LEN		256

/* Max value for directory header count */
#define SQUASHFS_DIR_COUNT		256

#define SQUASHFS_INVALID		((long long) 0xffffffffffff)
#define SQUASHFS_INVALID_FRAG		((unsigned int) 0xffffffff)
#define SQUASHFS_INVALID_XATTR		((unsigned int) 0xffffffff)
#define SQUASHFS_INVALID_BLK		((long long) -1)
#define SQUASHFS_USED_BLK		((long long) -2)

/* Filesystem flags */
#define SQUASHFS_NOI			0
#define SQUASHFS_NOD			1
#define SQUASHFS_CHECK			2
#define SQUASHFS_NOF			3
#define SQUASHFS_NO_FRAG		4
#define SQUASHFS_ALWAYS_FRAG		5
#define SQUASHFS_DUPLICATE		6
#define SQUASHFS_EXPORT			7
#define SQUASHFS_NOX			8
#define SQUASHFS_NO_XATTR		9
#define SQUASHFS_COMP_OPT		10
#define SQUASHFS_NOID			11

#define SQUASHFS_BIT(flag, bit)		((flag >> bit) & 1)

#define SQUASHFS_UNCOMPRESSED_INODES(flags)	SQUASHFS_BIT(flags, \
						SQUASHFS_NOI)

#define SQUASHFS_UNCOMPRESSED_DATA(flags)	SQUASHFS_BIT(flags, \
						SQUASHFS_NOD)

#define SQUASHFS_UNCOMPRESSED_FRAGMENTS(flags)	SQUASHFS_BIT(flags, \
						SQUASHFS_NOF)

#define SQUASHFS_NO_FRAGMENTS(flags)		SQUASHFS_BIT(flags, \
						SQUASHFS_NO_FRAG)

#define SQUASHFS_ALWAYS_FRAGMENTS(flags)	SQUASHFS_BIT(flags, \
						SQUASHFS_ALWAYS_FRAG)

#define SQUASHFS_DUPLICATES(flags)		SQUASHFS_BIT(flags, \
						SQUASHFS_DUPLICATE)

#define SQUASHFS_EXPORTABLE(flags)		SQUASHFS_BIT(flags, \
						SQUASHFS_EXPORT)

#define SQUASHFS_UNCOMPRESSED_XATTRS(flags)	SQUASHFS_BIT(flags, \
						SQUASHFS_NOX)

#define SQUASHFS_NO_XATTRS(flags)		SQUASHFS_BIT(flags, \
						SQUASHFS_NO_XATTR)

#define SQUASHFS_COMP_OPTS(flags)		SQUASHFS_BIT(flags, \
						SQUASHFS_COMP_OPT)

#define SQUASHFS_UNCOMPRESSED_IDS(flags)	SQUASHFS_BIT(flags, \
						SQUASHFS_NOID)

#define SQUASHFS_MKFLAGS(noi, nod, nof, nox, noid, no_frag, always_frag, \
		duplicate_checking, exportable, no_xattr, comp_opt) (noi | \
		(nod << 1) | (nof << 3) | (no_frag << 4) | \
		(always_frag << 5) | (duplicate_checking << 6) | \
		(exportable << 7) | (nox << 8) | (no_xattr << 9) | \
		(comp_opt << 10) | (noid << 11))

/* Max number of types and file types */
#define SQUASHFS_DIR_TYPE		1
#define SQUASHFS_FILE_TYPE		2
#define SQUASHFS_SYMLINK_TYPE		3
#define SQUASHFS_BLKDEV_TYPE		4
#define SQUASHFS_CHRDEV_TYPE		5
#define SQUASHFS_FIFO_TYPE		6
#define SQUASHFS_SOCKET_TYPE		7
#define SQUASHFS_LDIR_TYPE		8
#define SQUASHFS_LREG_TYPE		9
#define SQUASHFS_LSYMLINK_TYPE		10
#define SQUASHFS_LBLKDEV_TYPE		11
#define SQUASHFS_LCHRDEV_TYPE		12
#define SQUASHFS_LFIFO_TYPE		13
#define SQUASHFS_LSOCKET_TYPE		14

/* Xattr types */
#define SQUASHFS_XATTR_USER		0
#define SQUASHFS_XATTR_TRUSTED		1
#define SQUASHFS_XATTR_SECURITY		2
#define SQUASHFS_XATTR_VALUE_OOL	256
#define SQUASHFS_XATTR_PREFIX_MASK	0xff

/* Flag whether block is compressed or uncompressed, bit is set if block is
 * uncompressed */
#define SQUASHFS_COMPRESSED_BIT		(1 << 15)

#define SQUASHFS_COMPRESSED_SIZE(B)	(((B) & ~SQUASHFS_COMPRESSED_BIT) ? \
		(B) & ~SQUASHFS_COMPRESSED_BIT :  SQUASHFS_COMPRESSED_BIT)

#define SQUASHFS_COMPRESSED(B)		(!((B) & SQUASHFS_COMPRESSED_BIT))

#define SQUASHFS_COMPRESSED_BIT_BLOCK		(1 << 24)

#define SQUASHFS_COMPRESSED_SIZE_BLOCK(B)	((B) & \
	~SQUASHFS_COMPRESSED_BIT_BLOCK)

#define SQUASHFS_COMPRESSED_BLOCK(B)	(!((B) & SQUASHFS_COMPRESSED_BIT_BLOCK))

/*
 * Inode number ops.  Inodes consist of a compressed block number, and an
 * uncompressed  offset within that block
 */
#define SQUASHFS_INODE_BLK(a)		((unsigned int) ((a) >> 16))

#define SQUASHFS_INODE_OFFSET(a)	((unsigned int) ((a) & 0xffff))

#define SQUASHFS_MKINODE(A, B)		((squashfs_inode)(((squashfs_inode) (A)\
					<< 16) + (B)))

/* Compute 32 bit VFS inode number from squashfs inode number */
#define SQUASHFS_MK_VFS_INODE(a, b)	((unsigned int) (((a) << 8) + \
					((b) >> 2) + 1))

/* Translate between VFS mode and squashfs mode */
#define SQUASHFS_MODE(a)		((a) & 0xfff)

/* fragment and fragment table defines */
#define SQUASHFS_FRAGMENT_BYTES(A)	((A) * \
					sizeof(struct squashfs_fragment_entry))

#define SQUASHFS_FRAGMENT_INDEX(A)	(SQUASHFS_FRAGMENT_BYTES(A) / \
					SQUASHFS_METADATA_SIZE)

#define SQUASHFS_FRAGMENT_INDEX_OFFSET(A)	(SQUASHFS_FRAGMENT_BYTES(A) % \
						SQUASHFS_METADATA_SIZE)

#define SQUASHFS_FRAGMENT_INDEXES(A)	((SQUASHFS_FRAGMENT_BYTES(A) + \
					SQUASHFS_METADATA_SIZE - 1) / \
					SQUASHFS_METADATA_SIZE)

#define SQUASHFS_FRAGMENT_INDEX_BYTES(A)	(SQUASHFS_FRAGMENT_INDEXES(A) *\
						sizeof(long long))

/* inode lookup table defines */
#define SQUASHFS_LOOKUP_BYTES(A)	((A) * sizeof(squashfs_inode))

#define SQUASHFS_LOOKUP_BLOCK(A)		(SQUASHFS_LOOKUP_BYTES(A) / \
						SQUASHFS_METADATA_SIZE)

#define SQUASHFS_LOOKUP_BLOCK_OFFSET(A)		(SQUASHFS_LOOKUP_BYTES(A) % \
						SQUASHFS_METADATA_SIZE)

#define SQUASHFS_LOOKUP_BLOCKS(A)	((SQUASHFS_LOOKUP_BYTES(A) + \
					SQUASHFS_METADATA_SIZE - 1) / \
					SQUASHFS_METADATA_SIZE)

#define SQUASHFS_LOOKUP_BLOCK_BYTES(A)	(SQUASHFS_LOOKUP_BLOCKS(A) *\
					sizeof(long long))

/* uid lookup table defines */
#define SQUASHFS_ID_BYTES(A)	((A) * sizeof(unsigned int))

#define SQUASHFS_ID_BLOCK(A)		(SQUASHFS_ID_BYTES(A) / \
						SQUASHFS_METADATA_SIZE)

#define SQUASHFS_ID_BLOCK_OFFSET(A)		(SQUASHFS_ID_BYTES(A) % \
						SQUASHFS_METADATA_SIZE)

#define SQUASHFS_ID_BLOCKS(A)	((SQUASHFS_ID_BYTES(A) + \
					SQUASHFS_METADATA_SIZE - 1) / \
					SQUASHFS_METADATA_SIZE)

#define SQUASHFS_ID_BLOCK_BYTES(A)	(SQUASHFS_ID_BLOCKS(A) *\
					sizeof(long long))

/* xattr id lookup table defines */
#define SQUASHFS_XATTR_BYTES(A)		((A) * sizeof(struct squashfs_xattr_id))

#define SQUASHFS_XATTR_BLOCK(A)		(SQUASHFS_XATTR_BYTES(A) / \
					SQUASHFS_METADATA_SIZE)

#define SQUASHFS_XATTR_BLOCK_OFFSET(A)	(SQUASHFS_XATTR_BYTES(A) % \
					SQUASHFS_METADATA_SIZE)

#define SQUASHFS_XATTR_BLOCKS(A)	((SQUASHFS_XATTR_BYTES(A) + \
					SQUASHFS_METADATA_SIZE - 1) / \
					SQUASHFS_METADATA_SIZE)

#define SQUASHFS_XATTR_BLOCK_BYTES(A)	(SQUASHFS_XATTR_BLOCKS(A) *\
					sizeof(long long))

#define SQUASHFS_XATTR_BLK(A)		((unsigned int) ((A) >> 16))

#define SQUASHFS_XATTR_OFFSET(A)	((unsigned int) ((A) & 0xffff))

/* cached data constants for filesystem */
#define SQUASHFS_CACHED_BLKS		8

#define SQUASHFS_MAX_FILE_SIZE_LOG	64

#define SQUASHFS_MAX_FILE_SIZE		((long long) 1 << \
					(SQUASHFS_MAX_FILE_SIZE_LOG - 2))

#define SQUASHFS_MARKER_BYTE		0xff

/* meta index cache */
#define SQUASHFS_META_INDEXES	(SQUASHFS_METADATA_SIZE / sizeof(unsigned int))
#define SQUASHFS_META_ENTRIES	31
#define SQUASHFS_META_NUMBER	8
#define SQUASHFS_SLOTS		4

struct meta_entry {
	long long		data_block;
	unsigned int		index_block;
	unsigned short		offset;
	unsigned short		pad;
};

struct meta_index {
	unsigned int		inode_number;
	unsigned int		offset;
	unsigned short		entries;
	unsigned short		skip;
	unsigned short		locked;
	unsigned short		pad;
	struct meta_entry	meta_entry[SQUASHFS_META_ENTRIES];
};


/*
 * definitions for structures on disk
 */

typedef long long		squashfs_block;
typedef long long		squashfs_inode;

#define ZLIB_COMPRESSION	1
#define LZMA_COMPRESSION	2
#define LZO_COMPRESSION		3
#define XZ_COMPRESSION		4
#define LZ4_COMPRESSION		5
#define ZSTD_COMPRESSION	6

struct squashfs_super_block {
	unsigned int		s_magic;
	unsigned int		inodes;
	unsigned int		mkfs_time /* time of filesystem creation */;
	unsigned int		block_size;
	unsigned int		fragments;
	unsigned short		compression;
	unsigned short		block_log;
	unsigned short		flags;
	unsigned short		no_ids;
	unsigned short		s_major;
	unsigned short		s_minor;
	squashfs_inode		root_inode;
	long long		bytes_used;
	long long		id_table_start;
	long long		xattr_id_table_start;
	long long		inode_table_start;
	long long		directory_table_start;
	long long		fragment_table_start;
	long long		lookup_table_start;
};

struct squashfs_dir_index {
	unsigned int		index;
	unsigned int		start_block;
	unsigned int		size;
	unsigned char		name[0];
};

struct squashfs_base_inode_header {
	unsigned short		inode_type;
	unsigned short		mode;
	unsigned short		uid;
	unsigned short		guid;
	unsigned int		mtime;
	unsigned int 		inode_number;
};

struct squashfs_ipc_inode_header {
	unsigned short		inode_type;
	unsigned short		mode;
	unsigned short		uid;
	unsigned short		guid;
	unsigned int		mtime;
	unsigned int 		inode_number;
	unsigned int		nlink;
};

struct squashfs_lipc_inode_header {
	unsigned short		inode_type;
	unsigned short		mode;
	unsigned short		uid;
	unsigned short		guid;
	unsigned int		mtime;
	unsigned int 		inode_number;
	unsigned int		nlink;
	unsigned int		xattr;
};

struct squashfs_dev_inode_header {
	unsigned short		inode_type;
	unsigned short		mode;
	unsigned short		uid;
	unsigned short		guid;
	unsigned int		mtime;
	unsigned int 		inode_number;
	unsigned int		nlink;
	unsigned int		rdev;
};
	
struct squashfs_ldev_inode_header {
	unsigned short		inode_type;
	unsigned short		mode;
	unsigned short		uid;
	unsigned short		guid;
	unsigned int		mtime;
	unsigned int 		inode_number;
	unsigned int		nlink;
	unsigned int		rdev;
	unsigned int		xattr;
};
	
struct squashfs_symlink_inode_header {
	unsigned short		inode_type;
	unsigned short		mode;
	unsigned short		uid;
	unsigned short		guid;
	unsigned int		mtime;
	unsigned int 		inode_number;
	unsigned int		nlink;
	unsigned int		symlink_size;
	char			symlink[0];
};

struct squashfs_reg_inode_header {
	unsigned short		inode_type;
	unsigned short		mode;
	unsigned short		uid;
	unsigned short		guid;
	unsigned int		mtime;
	unsigned int 		inode_number;
	unsigned int		start_block;
	unsigned int		fragment;
	unsigned int		offset;
	unsigned int		file_size;
	unsigned int		block_list[0];
};

struct squashfs_lreg_inode_header {
	unsigned short		inode_type;
	unsigned short		mode;
	unsigned short		uid;
	unsigned short		guid;
	unsigned int		mtime;
	unsigned int 		inode_number;
	squashfs_block		start_block;
	long long		file_size;
	long long		sparse;
	unsigned int		nlink;
	unsigned int		fragment;
	unsigned int		offset;
	unsigned int		xattr;
	unsigned int		block_list[0];
};

struct squashfs_dir_inode_header {
	unsigned short		inode_type;
	unsigned short		mode;
	unsigned short		uid;
	unsigned short		guid;
	unsigned int		mtime;
	unsigned int 		inode_number;
	unsigned int		start_block;
	unsigned int		nlink;
	unsigned short		file_size;
	unsigned short		offset;
	unsigned int		parent_inode;
};

struct squashfs_ldir_inode_header {
	unsigned short		inode_type;
	unsigned short		mode;
	unsigned short		uid;
	unsigned short		guid;
	unsigned int		mtime;
	unsigned int 		inode_number;
	unsigned int		nlink;
	unsigned int		file_size;
	unsigned int		start_block;
	unsigned int		parent_inode;
	unsigned short		i_count;
	unsigned short		offset;
	unsigned int		xattr;
	struct squashfs_dir_index	index[0];
};

union squashfs_inode_header {
	struct squashfs_base_inode_header	base;
	struct squashfs_dev_inode_header	dev;
	struct squashfs_ldev_inode_header	ldev;
	struct squashfs_symlink_inode_header	symlink;
	struct squashfs_reg_inode_header	reg;
	struct squashfs_lreg_inode_header	lreg;
	struct squashfs_dir_inode_header	dir;
	struct squashfs_ldir_inode_header	ldir;
	struct squashfs_ipc_inode_header	ipc;
	struct squashfs_lipc_inode_header	lipc;
};
	
struct squashfs_dir_entry {
	unsigned short		offset;
	short			inode_number;
	unsigned short		type;
	unsigned short		size;
	char			name[0];
};

struct squashfs_dir_header {
	unsigned int		count;
	unsigned int		start_block;
	unsigned int		inode_number;
};

struct squashfs_fragment_entry {
	long long		start_block;
	unsigned int		size;
	unsigned int		unused;
};

struct squashfs_xattr_entry {
	unsigned short		type;
	unsigned short		size;
};

struct squashfs_xattr_val {
	unsigned int		vsize;
};

struct squashfs_xattr_id {
	long long		xattr;
	unsigned int		count;
	unsigned int		size;
};

struct squashfs_xattr_table {
	long long		xattr_table_start;
	unsigned int		xattr_ids;
	unsigned int		unused;
};

#endif
