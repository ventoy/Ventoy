/*
 * Squashfs - a compressed read only filesystem for Linux
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
 * squashfs.h
 */

#ifdef CONFIG_SQUASHFS_1_0_COMPATIBILITY
#undef CONFIG_SQUASHFS_1_0_COMPATIBILITY
#endif
#ifdef SQUASHFS_TRACE
#define TRACE(s, args...)	printk(KERN_NOTICE "SQUASHFS: "s, ## args)
#else
#define TRACE(s, args...)	{}
#endif

#define ERROR(s, args...)	printk(KERN_ERR "SQUASHFS error: "s, ## args)

#define SERROR(s, args...)	do { \
				if (!silent) \
				printk(KERN_ERR "SQUASHFS error: "s, ## args);\
				} while(0)

#define WARNING(s, args...)	printk(KERN_WARNING "SQUASHFS: "s, ## args)

#define SQUASHFS_I(INO)			(&INO->u.squashfs_i)

#define i_size_read(INO)		(INO->i_size)

#if defined(CONFIG_SQUASHFS_1_0_COMPATIBILITY ) || defined(CONFIG_SQUASHFS_2_0_COMPATIBILITY)
#define SQSH_EXTERN
extern unsigned int squashfs_read_data(struct super_block *s, char *buffer,
				long long index, unsigned int length,
				long long *next_index);
extern int squashfs_get_cached_block(struct super_block *s, char *buffer,
				long long block, unsigned int offset,
				int length, long long *next_block,
				unsigned int *next_offset);
extern void release_cached_fragment(struct squashfs_sb_info *msblk, struct
					squashfs_fragment_cache *fragment);
extern struct squashfs_fragment_cache *get_cached_fragment(struct super_block
					*s, long long start_block,
					int length);
extern struct address_space_operations squashfs_symlink_aops;
extern struct address_space_operations squashfs_aops;
extern struct address_space_operations squashfs_aops_4K;
extern struct file_operations squashfs_dir_ops;
extern struct inode_operations squashfs_dir_inode_ops;
#else
#define SQSH_EXTERN static
#endif

#ifdef CONFIG_SQUASHFS_1_0_COMPATIBILITY
extern int squashfs_1_0_supported(struct squashfs_sb_info *msblk);
#else
static inline int squashfs_1_0_supported(struct squashfs_sb_info *msblk)
{
	return 0;
}
#endif

#ifdef CONFIG_SQUASHFS_2_0_COMPATIBILITY
extern int squashfs_2_0_supported(struct squashfs_sb_info *msblk);
#else
static inline int squashfs_2_0_supported(struct squashfs_sb_info *msblk)
{
	return 0;
}
#endif
