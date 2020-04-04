#ifndef XATTR_H
#define XATTR_H
/*
 * Create a squashfs filesystem.  This is a highly compressed read only
 * filesystem.
 *
 * Copyright (c) 2010, 2012, 2013, 2014, 2019
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
 * xattr.h
 */

#define XATTR_VALUE_OOL		SQUASHFS_XATTR_VALUE_OOL
#define XATTR_PREFIX_MASK	SQUASHFS_XATTR_PREFIX_MASK

#define XATTR_VALUE_OOL_SIZE	sizeof(long long)

/* maximum size of xattr value data that will be inlined */
#define XATTR_INLINE_MAX 	128

/* the target size of an inode's xattr name:value list.  If it
 * exceeds this, then xattr value data will be successively out of lined
 * until it meets the target */
#define XATTR_TARGET_MAX	65536

#define IS_XATTR(a)		(a != SQUASHFS_INVALID_XATTR)

struct xattr_list {
	char			*name;
	char			*full_name;
	int			size;
	int			vsize;
	void			*value;
	int			type;
	long long		ool_value;
	unsigned short		vchecksum;
	struct xattr_list	*vnext;
};

struct dupl_id {
	struct xattr_list	*xattr_list;
	int			xattrs;
	int			xattr_id;
	struct dupl_id		*next;
};

struct prefix {
	char			*prefix;
	int			type;
};

extern int generate_xattrs(int, struct xattr_list *);

#ifdef XATTR_SUPPORT
extern int get_xattrs(int, struct squashfs_super_block *);
extern int read_xattrs(void *);
extern long long write_xattrs();
extern void save_xattrs();
extern void restore_xattrs();
extern unsigned int xattr_bytes, total_xattr_bytes;
extern int write_xattr(char *, unsigned int);
extern int read_xattrs_from_disk(int, struct squashfs_super_block *, int, long long *);
extern struct xattr_list *get_xattr(int, unsigned int *, int *);
extern void free_xattr(struct xattr_list *, int);
#else
static inline int get_xattrs(int fd, struct squashfs_super_block *sBlk)
{
	if(sBlk->xattr_id_table_start != SQUASHFS_INVALID_BLK) {
		fprintf(stderr, "Xattrs in filesystem! These are not "
			"supported on this version of Squashfs\n");
		return 0;
	} else
		return SQUASHFS_INVALID_BLK;
}


static inline int read_xattrs(void *dir_ent)
{
	return SQUASHFS_INVALID_XATTR;
}


static inline long long write_xattrs()
{
	return SQUASHFS_INVALID_BLK;
}


static inline void save_xattrs()
{
}


static inline void restore_xattrs()
{
}


static inline int write_xattr(char *pathname, unsigned int xattr)
{
	return 0;
}


static inline int read_xattrs_from_disk(int fd, struct squashfs_super_block *sBlk, int flag, long long *table_start)
{
	if(sBlk->xattr_id_table_start != SQUASHFS_INVALID_BLK) {
		fprintf(stderr, "Xattrs in filesystem! These are not "
			"supported on this version of Squashfs\n");
		return 0;
	} else
		return SQUASHFS_INVALID_BLK;
}


static inline struct xattr_list *get_xattr(int i, unsigned int *count, int j)
{
	return NULL;
}
#endif

#ifdef XATTR_SUPPORT
#ifdef XATTR_DEFAULT
#define NOXOPT_STR
#define XOPT_STR " (default)"
#define XATTR_DEF 0
#else
#define NOXOPT_STR " (default)"
#define XOPT_STR
#define XATTR_DEF 1
#endif
#else
#define NOXOPT_STR " (default)"
#define XOPT_STR " (unsupported)"
#define XATTR_DEF 1
#endif
#endif
