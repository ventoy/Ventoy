#ifndef SQUASHFS_SWAP_H
#define SQUASHFS_SWAP_H
/*
 * Squashfs
 *
 * Copyright (c) 2008, 2009, 2010, 2013, 2019
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
 * squashfs_swap.h
 */

/*
 * macros to convert each stucture from big endian to little endian
 */

#if __BYTE_ORDER == __BIG_ENDIAN
#include <stddef.h>
extern void swap_le16(void *, void *);
extern void swap_le32(void *, void *);
extern void swap_le64(void *, void *);
extern void swap_le16_num(void *, void *, int);
extern void swap_le32_num(void *, void *, int);
extern void swap_le64_num(void *, void *, int);
extern unsigned short inswap_le16(unsigned short);
extern unsigned int inswap_le32(unsigned int);
extern long long inswap_le64(long long);
extern void inswap_le16_num(unsigned short *, int);
extern void inswap_le32_num(unsigned int *, int);
extern void inswap_le64_num(long long *, int);

#define _SQUASHFS_SWAP_SUPER_BLOCK(s, d, SWAP_FUNC) {\
	SWAP_FUNC(32, s, d, s_magic, struct squashfs_super_block);\
	SWAP_FUNC(32, s, d, inodes, struct squashfs_super_block);\
	SWAP_FUNC(32, s, d, mkfs_time, struct squashfs_super_block);\
	SWAP_FUNC(32, s, d, block_size, struct squashfs_super_block);\
	SWAP_FUNC(32, s, d, fragments, struct squashfs_super_block);\
	SWAP_FUNC(16, s, d, compression, struct squashfs_super_block);\
	SWAP_FUNC(16, s, d, block_log, struct squashfs_super_block);\
	SWAP_FUNC(16, s, d, flags, struct squashfs_super_block);\
	SWAP_FUNC(16, s, d, no_ids, struct squashfs_super_block);\
	SWAP_FUNC(16, s, d, s_major, struct squashfs_super_block);\
	SWAP_FUNC(16, s, d, s_minor, struct squashfs_super_block);\
	SWAP_FUNC(64, s, d, root_inode, struct squashfs_super_block);\
	SWAP_FUNC(64, s, d, bytes_used, struct squashfs_super_block);\
	SWAP_FUNC(64, s, d, id_table_start, struct squashfs_super_block);\
	SWAP_FUNC(64, s, d, xattr_id_table_start, struct squashfs_super_block);\
	SWAP_FUNC(64, s, d, inode_table_start, struct squashfs_super_block);\
	SWAP_FUNC(64, s, d, directory_table_start, struct squashfs_super_block);\
	SWAP_FUNC(64, s, d, fragment_table_start, struct squashfs_super_block);\
	SWAP_FUNC(64, s, d, lookup_table_start, struct squashfs_super_block);\
}

#define _SQUASHFS_SWAP_DIR_INDEX(s, d, SWAP_FUNC) {\
	SWAP_FUNC(32, s, d, index, struct squashfs_dir_index);\
	SWAP_FUNC(32, s, d, start_block, struct squashfs_dir_index);\
	SWAP_FUNC(32, s, d, size, struct squashfs_dir_index);\
}

#define _SQUASHFS_SWAP_BASE_INODE_HEADER(s, d, SWAP_FUNC) {\
	SWAP_FUNC(16, s, d, inode_type, struct squashfs_base_inode_header);\
	SWAP_FUNC(16, s, d, mode, struct squashfs_base_inode_header);\
	SWAP_FUNC(16, s, d, uid, struct squashfs_base_inode_header);\
	SWAP_FUNC(16, s, d, guid, struct squashfs_base_inode_header);\
	SWAP_FUNC(32, s, d, mtime, struct squashfs_base_inode_header);\
	SWAP_FUNC(32, s, d, inode_number, struct squashfs_base_inode_header);\
}

#define _SQUASHFS_SWAP_IPC_INODE_HEADER(s, d, SWAP_FUNC) {\
	SWAP_FUNC(16, s, d, inode_type, struct squashfs_ipc_inode_header);\
	SWAP_FUNC(16, s, d, mode, struct squashfs_ipc_inode_header);\
	SWAP_FUNC(16, s, d, uid, struct squashfs_ipc_inode_header);\
	SWAP_FUNC(16, s, d, guid, struct squashfs_ipc_inode_header);\
	SWAP_FUNC(32, s, d, mtime, struct squashfs_ipc_inode_header);\
	SWAP_FUNC(32, s, d, inode_number, struct squashfs_ipc_inode_header);\
	SWAP_FUNC(32, s, d, nlink, struct squashfs_ipc_inode_header);\
}

#define _SQUASHFS_SWAP_LIPC_INODE_HEADER(s, d, SWAP_FUNC) {\
	SWAP_FUNC(16, s, d, inode_type, struct squashfs_lipc_inode_header);\
	SWAP_FUNC(16, s, d, mode, struct squashfs_lipc_inode_header);\
	SWAP_FUNC(16, s, d, uid, struct squashfs_lipc_inode_header);\
	SWAP_FUNC(16, s, d, guid, struct squashfs_lipc_inode_header);\
	SWAP_FUNC(32, s, d, mtime, struct squashfs_lipc_inode_header);\
	SWAP_FUNC(32, s, d, inode_number, struct squashfs_lipc_inode_header);\
	SWAP_FUNC(32, s, d, nlink, struct squashfs_lipc_inode_header);\
	SWAP_FUNC(32, s, d, xattr, struct squashfs_lipc_inode_header);\
}

#define _SQUASHFS_SWAP_DEV_INODE_HEADER(s, d, SWAP_FUNC) {\
	SWAP_FUNC(16, s, d, inode_type, struct squashfs_dev_inode_header);\
	SWAP_FUNC(16, s, d, mode, struct squashfs_dev_inode_header);\
	SWAP_FUNC(16, s, d, uid, struct squashfs_dev_inode_header);\
	SWAP_FUNC(16, s, d, guid, struct squashfs_dev_inode_header);\
	SWAP_FUNC(32, s, d, mtime, struct squashfs_dev_inode_header);\
	SWAP_FUNC(32, s, d, inode_number, struct squashfs_dev_inode_header);\
	SWAP_FUNC(32, s, d, nlink, struct squashfs_dev_inode_header);\
	SWAP_FUNC(32, s, d, rdev, struct squashfs_dev_inode_header);\
}

#define _SQUASHFS_SWAP_LDEV_INODE_HEADER(s, d, SWAP_FUNC) {\
	SWAP_FUNC(16, s, d, inode_type, struct squashfs_ldev_inode_header);\
	SWAP_FUNC(16, s, d, mode, struct squashfs_ldev_inode_header);\
	SWAP_FUNC(16, s, d, uid, struct squashfs_ldev_inode_header);\
	SWAP_FUNC(16, s, d, guid, struct squashfs_ldev_inode_header);\
	SWAP_FUNC(32, s, d, mtime, struct squashfs_ldev_inode_header);\
	SWAP_FUNC(32, s, d, inode_number, struct squashfs_ldev_inode_header);\
	SWAP_FUNC(32, s, d, nlink, struct squashfs_ldev_inode_header);\
	SWAP_FUNC(32, s, d, rdev, struct squashfs_ldev_inode_header);\
	SWAP_FUNC(32, s, d, xattr, struct squashfs_ldev_inode_header);\
}

#define _SQUASHFS_SWAP_SYMLINK_INODE_HEADER(s, d, SWAP_FUNC) {\
	SWAP_FUNC(16, s, d, inode_type, struct squashfs_symlink_inode_header);\
	SWAP_FUNC(16, s, d, mode, struct squashfs_symlink_inode_header);\
	SWAP_FUNC(16, s, d, uid, struct squashfs_symlink_inode_header);\
	SWAP_FUNC(16, s, d, guid, struct squashfs_symlink_inode_header);\
	SWAP_FUNC(32, s, d, mtime, struct squashfs_symlink_inode_header);\
	SWAP_FUNC(32, s, d, inode_number, struct squashfs_symlink_inode_header);\
	SWAP_FUNC(32, s, d, nlink, struct squashfs_symlink_inode_header);\
	SWAP_FUNC(32, s, d, symlink_size, struct squashfs_symlink_inode_header);\
}

#define _SQUASHFS_SWAP_REG_INODE_HEADER(s, d, SWAP_FUNC) {\
	SWAP_FUNC(16, s, d, inode_type, struct squashfs_reg_inode_header);\
	SWAP_FUNC(16, s, d, mode, struct squashfs_reg_inode_header);\
	SWAP_FUNC(16, s, d, uid, struct squashfs_reg_inode_header);\
	SWAP_FUNC(16, s, d, guid, struct squashfs_reg_inode_header);\
	SWAP_FUNC(32, s, d, mtime, struct squashfs_reg_inode_header);\
	SWAP_FUNC(32, s, d, inode_number, struct squashfs_reg_inode_header);\
	SWAP_FUNC(32, s, d, start_block, struct squashfs_reg_inode_header);\
	SWAP_FUNC(32, s, d, fragment, struct squashfs_reg_inode_header);\
	SWAP_FUNC(32, s, d, offset, struct squashfs_reg_inode_header);\
	SWAP_FUNC(32, s, d, file_size, struct squashfs_reg_inode_header);\
}

#define _SQUASHFS_SWAP_LREG_INODE_HEADER(s, d, SWAP_FUNC) {\
	SWAP_FUNC(16, s, d, inode_type, struct squashfs_lreg_inode_header);\
	SWAP_FUNC(16, s, d, mode, struct squashfs_lreg_inode_header);\
	SWAP_FUNC(16, s, d, uid, struct squashfs_lreg_inode_header);\
	SWAP_FUNC(16, s, d, guid, struct squashfs_lreg_inode_header);\
	SWAP_FUNC(32, s, d, mtime, struct squashfs_lreg_inode_header);\
	SWAP_FUNC(32, s, d, inode_number, struct squashfs_lreg_inode_header);\
	SWAP_FUNC(64, s, d, start_block, struct squashfs_lreg_inode_header);\
	SWAP_FUNC(64, s, d, file_size, struct squashfs_lreg_inode_header);\
	SWAP_FUNC(64, s, d, sparse, struct squashfs_lreg_inode_header);\
	SWAP_FUNC(32, s, d, nlink, struct squashfs_lreg_inode_header);\
	SWAP_FUNC(32, s, d, fragment, struct squashfs_lreg_inode_header);\
	SWAP_FUNC(32, s, d, offset, struct squashfs_lreg_inode_header);\
	SWAP_FUNC(32, s, d, xattr, struct squashfs_lreg_inode_header);\
}

#define _SQUASHFS_SWAP_DIR_INODE_HEADER(s, d, SWAP_FUNC) {\
	SWAP_FUNC(16, s, d, inode_type, struct squashfs_dir_inode_header);\
	SWAP_FUNC(16, s, d, mode, struct squashfs_dir_inode_header);\
	SWAP_FUNC(16, s, d, uid, struct squashfs_dir_inode_header);\
	SWAP_FUNC(16, s, d, guid, struct squashfs_dir_inode_header);\
	SWAP_FUNC(32, s, d, mtime, struct squashfs_dir_inode_header);\
	SWAP_FUNC(32, s, d, inode_number, struct squashfs_dir_inode_header);\
	SWAP_FUNC(32, s, d, start_block, struct squashfs_dir_inode_header);\
	SWAP_FUNC(32, s, d, nlink, struct squashfs_dir_inode_header);\
	SWAP_FUNC(16, s, d, file_size, struct squashfs_dir_inode_header);\
	SWAP_FUNC(16, s, d, offset, struct squashfs_dir_inode_header);\
	SWAP_FUNC(32, s, d, parent_inode, struct squashfs_dir_inode_header);\
}

#define _SQUASHFS_SWAP_LDIR_INODE_HEADER(s, d, SWAP_FUNC) {\
	SWAP_FUNC(16, s, d, inode_type, struct squashfs_ldir_inode_header);\
	SWAP_FUNC(16, s, d, mode, struct squashfs_ldir_inode_header);\
	SWAP_FUNC(16, s, d, uid, struct squashfs_ldir_inode_header);\
	SWAP_FUNC(16, s, d, guid, struct squashfs_ldir_inode_header);\
	SWAP_FUNC(32, s, d, mtime, struct squashfs_ldir_inode_header);\
	SWAP_FUNC(32, s, d, inode_number, struct squashfs_ldir_inode_header);\
	SWAP_FUNC(32, s, d, nlink, struct squashfs_ldir_inode_header);\
	SWAP_FUNC(32, s, d, file_size, struct squashfs_ldir_inode_header);\
	SWAP_FUNC(32, s, d, start_block, struct squashfs_ldir_inode_header);\
	SWAP_FUNC(32, s, d, parent_inode, struct squashfs_ldir_inode_header);\
	SWAP_FUNC(16, s, d, i_count, struct squashfs_ldir_inode_header);\
	SWAP_FUNC(16, s, d, offset, struct squashfs_ldir_inode_header);\
	SWAP_FUNC(32, s, d, xattr, struct squashfs_ldir_inode_header);\
}

#define _SQUASHFS_SWAP_DIR_ENTRY(s, d, SWAP_FUNC) {\
	SWAP_FUNC(16, s, d, offset, struct squashfs_dir_entry);\
	SWAP_FUNC##S(16, s, d, inode_number, struct squashfs_dir_entry);\
	SWAP_FUNC(16, s, d, type, struct squashfs_dir_entry);\
	SWAP_FUNC(16, s, d, size, struct squashfs_dir_entry);\
}

#define _SQUASHFS_SWAP_DIR_HEADER(s, d, SWAP_FUNC) {\
	SWAP_FUNC(32, s, d, count, struct squashfs_dir_header);\
	SWAP_FUNC(32, s, d, start_block, struct squashfs_dir_header);\
	SWAP_FUNC(32, s, d, inode_number, struct squashfs_dir_header);\
}

#define _SQUASHFS_SWAP_FRAGMENT_ENTRY(s, d, SWAP_FUNC) {\
	SWAP_FUNC(64, s, d, start_block, struct squashfs_fragment_entry);\
	SWAP_FUNC(32, s, d, size, struct squashfs_fragment_entry);\
}

#define _SQUASHFS_SWAP_XATTR_ENTRY(s, d, SWAP_FUNC) {\
	SWAP_FUNC(16, s, d, type, struct squashfs_xattr_entry);\
	SWAP_FUNC(16, s, d, size, struct squashfs_xattr_entry);\
}

#define _SQUASHFS_SWAP_XATTR_VAL(s, d, SWAP_FUNC) {\
	SWAP_FUNC(32, s, d, vsize, struct squashfs_xattr_val);\
}

#define _SQUASHFS_SWAP_XATTR_ID(s, d, SWAP_FUNC) {\
	SWAP_FUNC(64, s, d, xattr, struct squashfs_xattr_id);\
	SWAP_FUNC(32, s, d, count, struct squashfs_xattr_id);\
	SWAP_FUNC(32, s, d, size, struct squashfs_xattr_id);\
}

#define _SQUASHFS_SWAP_XATTR_TABLE(s, d, SWAP_FUNC) {\
	SWAP_FUNC(64, s, d, xattr_table_start, struct squashfs_xattr_table);\
	SWAP_FUNC(32, s, d, xattr_ids, struct squashfs_xattr_table);\
}

/* big endian architecture copy and swap macros */
#define SQUASHFS_SWAP_SUPER_BLOCK(s, d)	\
			_SQUASHFS_SWAP_SUPER_BLOCK(s, d, SWAP_LE)
#define SQUASHFS_SWAP_DIR_INDEX(s, d) \
			_SQUASHFS_SWAP_DIR_INDEX(s, d, SWAP_LE)
#define SQUASHFS_SWAP_BASE_INODE_HEADER(s, d) \
			_SQUASHFS_SWAP_BASE_INODE_HEADER(s, d, SWAP_LE)
#define SQUASHFS_SWAP_IPC_INODE_HEADER(s, d) \
			_SQUASHFS_SWAP_IPC_INODE_HEADER(s, d, SWAP_LE)
#define SQUASHFS_SWAP_LIPC_INODE_HEADER(s, d) \
			_SQUASHFS_SWAP_LIPC_INODE_HEADER(s, d, SWAP_LE)
#define SQUASHFS_SWAP_DEV_INODE_HEADER(s, d) \
			_SQUASHFS_SWAP_DEV_INODE_HEADER(s, d, SWAP_LE)
#define SQUASHFS_SWAP_LDEV_INODE_HEADER(s, d) \
			_SQUASHFS_SWAP_LDEV_INODE_HEADER(s, d, SWAP_LE)
#define SQUASHFS_SWAP_SYMLINK_INODE_HEADER(s, d) \
			_SQUASHFS_SWAP_SYMLINK_INODE_HEADER(s, d, SWAP_LE)
#define SQUASHFS_SWAP_REG_INODE_HEADER(s, d) \
			_SQUASHFS_SWAP_REG_INODE_HEADER(s, d, SWAP_LE)
#define SQUASHFS_SWAP_LREG_INODE_HEADER(s, d) \
			_SQUASHFS_SWAP_LREG_INODE_HEADER(s, d, SWAP_LE)
#define SQUASHFS_SWAP_DIR_INODE_HEADER(s, d) \
			_SQUASHFS_SWAP_DIR_INODE_HEADER(s, d, SWAP_LE)
#define SQUASHFS_SWAP_LDIR_INODE_HEADER(s, d) \
			_SQUASHFS_SWAP_LDIR_INODE_HEADER(s, d, SWAP_LE)
#define SQUASHFS_SWAP_DIR_ENTRY(s, d) \
			_SQUASHFS_SWAP_DIR_ENTRY(s, d, SWAP_LE)
#define SQUASHFS_SWAP_DIR_HEADER(s, d) \
			_SQUASHFS_SWAP_DIR_HEADER(s, d, SWAP_LE)
#define SQUASHFS_SWAP_FRAGMENT_ENTRY(s, d) \
			_SQUASHFS_SWAP_FRAGMENT_ENTRY(s, d, SWAP_LE)
#define SQUASHFS_SWAP_XATTR_ENTRY(s, d) \
			 _SQUASHFS_SWAP_XATTR_ENTRY(s, d, SWAP_LE)
#define SQUASHFS_SWAP_XATTR_VAL(s, d) \
			_SQUASHFS_SWAP_XATTR_VAL(s, d, SWAP_LE)
#define SQUASHFS_SWAP_XATTR_ID(s, d) \
			 _SQUASHFS_SWAP_XATTR_ID(s, d, SWAP_LE)
#define SQUASHFS_SWAP_XATTR_TABLE(s, d) \
			_SQUASHFS_SWAP_XATTR_TABLE(s, d, SWAP_LE)
#define SWAP_LE(bits, s, d, field, type) \
			SWAP_LE##bits(((void *)(s)) + offsetof(type, field), \
				((void *)(d)) + offsetof(type, field))
#define SWAP_LES(bits, s, d, field, type) \
			SWAP_LE(bits, s, d, field, type)
#define SQUASHFS_SWAP_INODE_T(s, d) SQUASHFS_SWAP_LONG_LONGS(s, d, 1)
#define SQUASHFS_SWAP_FRAGMENT_INDEXES(s, d, n) \
			SQUASHFS_SWAP_LONG_LONGS(s, d, n)
#define SQUASHFS_SWAP_LOOKUP_BLOCKS(s, d, n) SQUASHFS_SWAP_LONG_LONGS(s, d, n)
#define SQUASHFS_SWAP_ID_BLOCKS(s, d, n) SQUASHFS_SWAP_LONG_LONGS(s, d, n)

#define SQUASHFS_SWAP_SHORTS(s, d, n) swap_le16_num(s, d, n)
#define SQUASHFS_SWAP_INTS(s, d, n) swap_le32_num(s, d, n)
#define SQUASHFS_SWAP_LONG_LONGS(s, d, n) swap_le64_num(s, d, n)

#define SWAP_LE16(s, d)		swap_le16(s, d)
#define SWAP_LE32(s, d)		swap_le32(s, d)
#define SWAP_LE64(s, d)		swap_le64(s, d)

/* big endian architecture swap in-place macros */
#define SQUASHFS_INSWAP_SUPER_BLOCK(s) \
			_SQUASHFS_SWAP_SUPER_BLOCK(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_DIR_INDEX(s) \
			_SQUASHFS_SWAP_DIR_INDEX(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_BASE_INODE_HEADER(s) \
			_SQUASHFS_SWAP_BASE_INODE_HEADER(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_IPC_INODE_HEADER(s) \
			_SQUASHFS_SWAP_IPC_INODE_HEADER(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_LIPC_INODE_HEADER(s) \
			_SQUASHFS_SWAP_LIPC_INODE_HEADER(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_DEV_INODE_HEADER(s) \
			_SQUASHFS_SWAP_DEV_INODE_HEADER(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_LDEV_INODE_HEADER(s) \
			_SQUASHFS_SWAP_LDEV_INODE_HEADER(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_SYMLINK_INODE_HEADER(s) \
			_SQUASHFS_SWAP_SYMLINK_INODE_HEADER(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_REG_INODE_HEADER(s) \
			_SQUASHFS_SWAP_REG_INODE_HEADER(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_LREG_INODE_HEADER(s) \
			_SQUASHFS_SWAP_LREG_INODE_HEADER(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_DIR_INODE_HEADER(s) \
			_SQUASHFS_SWAP_DIR_INODE_HEADER(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_LDIR_INODE_HEADER(s) \
			_SQUASHFS_SWAP_LDIR_INODE_HEADER(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_DIR_ENTRY(s) \
			_SQUASHFS_SWAP_DIR_ENTRY(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_DIR_HEADER(s) \
			_SQUASHFS_SWAP_DIR_HEADER(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_FRAGMENT_ENTRY(s) \
			_SQUASHFS_SWAP_FRAGMENT_ENTRY(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_XATTR_ENTRY(s) \
			 _SQUASHFS_SWAP_XATTR_ENTRY(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_XATTR_VAL(s) \
			_SQUASHFS_SWAP_XATTR_VAL(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_XATTR_ID(s) \
			 _SQUASHFS_SWAP_XATTR_ID(s, s, INSWAP_LE)
#define SQUASHFS_INSWAP_XATTR_TABLE(s) \
			_SQUASHFS_SWAP_XATTR_TABLE(s, s, INSWAP_LE)
#define INSWAP_LE(bits, s, d, field, type) \
			(s)->field = inswap_le##bits((s)->field)
#define INSWAP_LES(bits, s, d, field, type) \
			(s)->field = (short) inswap_le##bits((unsigned short) \
				(s)->field)
#define SQUASHFS_INSWAP_INODE_T(s) s = inswap_le64(s)
#define SQUASHFS_INSWAP_FRAGMENT_INDEXES(s, n) inswap_le64_num(s, n)
#define SQUASHFS_INSWAP_LOOKUP_BLOCKS(s, n) inswap_le64_num(s, n)
#define SQUASHFS_INSWAP_ID_BLOCKS(s, n) inswap_le64_num(s, n)
#define SQUASHFS_INSWAP_SHORTS(s, n) inswap_le16_num(s, n)
#define SQUASHFS_INSWAP_INTS(s, n) inswap_le32_num(s, n)
#define SQUASHFS_INSWAP_LONG_LONGS(s, n) inswap_le64_num(s, n)
#else
/* little endian architecture, just copy */
#define SQUASHFS_SWAP_SUPER_BLOCK(s, d)	\
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_super_block))
#define SQUASHFS_SWAP_DIR_INDEX(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_dir_index))
#define SQUASHFS_SWAP_BASE_INODE_HEADER(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_base_inode_header))
#define SQUASHFS_SWAP_IPC_INODE_HEADER(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_ipc_inode_header))
#define SQUASHFS_SWAP_LIPC_INODE_HEADER(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_lipc_inode_header))
#define SQUASHFS_SWAP_DEV_INODE_HEADER(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_dev_inode_header))
#define SQUASHFS_SWAP_LDEV_INODE_HEADER(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_ldev_inode_header))
#define SQUASHFS_SWAP_SYMLINK_INODE_HEADER(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_symlink_inode_header))
#define SQUASHFS_SWAP_REG_INODE_HEADER(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_reg_inode_header))
#define SQUASHFS_SWAP_LREG_INODE_HEADER(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_lreg_inode_header))
#define SQUASHFS_SWAP_DIR_INODE_HEADER(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_dir_inode_header))
#define SQUASHFS_SWAP_LDIR_INODE_HEADER(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_ldir_inode_header))
#define SQUASHFS_SWAP_DIR_ENTRY(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_dir_entry))
#define SQUASHFS_SWAP_DIR_HEADER(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_dir_header))
#define SQUASHFS_SWAP_FRAGMENT_ENTRY(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_fragment_entry))
#define SQUASHFS_SWAP_XATTR_ENTRY(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_xattr_entry))
#define SQUASHFS_SWAP_XATTR_VAL(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_xattr_val))
#define SQUASHFS_SWAP_XATTR_ID(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_xattr_id))
#define SQUASHFS_SWAP_XATTR_TABLE(s, d) \
		SQUASHFS_MEMCPY(s, d, sizeof(struct squashfs_xattr_table))
#define SQUASHFS_SWAP_INODE_T(s, d) SQUASHFS_SWAP_LONG_LONGS(s, d, 1)
#define SQUASHFS_SWAP_FRAGMENT_INDEXES(s, d, n) \
			SQUASHFS_SWAP_LONG_LONGS(s, d, n)
#define SQUASHFS_SWAP_LOOKUP_BLOCKS(s, d, n) SQUASHFS_SWAP_LONG_LONGS(s, d, n)
#define SQUASHFS_SWAP_ID_BLOCKS(s, d, n) SQUASHFS_SWAP_LONG_LONGS(s, d, n)

#define SQUASHFS_MEMCPY(s, d, n)	memcpy(d, s, n)
#define SQUASHFS_SWAP_SHORTS(s, d, n)	memcpy(d, s, n * sizeof(short))
#define SQUASHFS_SWAP_INTS(s, d, n)	memcpy(d, s, n * sizeof(int))
#define SQUASHFS_SWAP_LONG_LONGS(s, d, n) \
					memcpy(d, s, n * sizeof(long long))

/* little endian architecture, data already in place so do nothing */
#define SQUASHFS_INSWAP_SUPER_BLOCK(s)
#define SQUASHFS_INSWAP_DIR_INDEX(s)
#define SQUASHFS_INSWAP_BASE_INODE_HEADER(s)
#define SQUASHFS_INSWAP_IPC_INODE_HEADER(s)
#define SQUASHFS_INSWAP_LIPC_INODE_HEADER(s)
#define SQUASHFS_INSWAP_DEV_INODE_HEADER(s)
#define SQUASHFS_INSWAP_LDEV_INODE_HEADER(s)
#define SQUASHFS_INSWAP_SYMLINK_INODE_HEADER(s)
#define SQUASHFS_INSWAP_REG_INODE_HEADER(s)
#define SQUASHFS_INSWAP_LREG_INODE_HEADER(s)
#define SQUASHFS_INSWAP_DIR_INODE_HEADER(s)
#define SQUASHFS_INSWAP_LDIR_INODE_HEADER(s)
#define SQUASHFS_INSWAP_DIR_ENTRY(s)
#define SQUASHFS_INSWAP_DIR_HEADER(s)
#define SQUASHFS_INSWAP_FRAGMENT_ENTRY(s)
#define SQUASHFS_INSWAP_XATTR_ENTRY(s)
#define SQUASHFS_INSWAP_XATTR_VAL(s)
#define SQUASHFS_INSWAP_XATTR_ID(s)
#define SQUASHFS_INSWAP_XATTR_TABLE(s)
#define SQUASHFS_INSWAP_INODE_T(s)
#define SQUASHFS_INSWAP_FRAGMENT_INDEXES(s, n)
#define SQUASHFS_INSWAP_LOOKUP_BLOCKS(s, n)
#define SQUASHFS_INSWAP_ID_BLOCKS(s, n)
#define SQUASHFS_INSWAP_SHORTS(s, n)
#define SQUASHFS_INSWAP_INTS(s, n)
#define SQUASHFS_INSWAP_LONG_LONGS(s, n)
#endif
#endif
