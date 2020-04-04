#ifndef LZO_WRAPPER_H
#define LZO_WRAPPER_H
/*
 * Squashfs
 *
 * Copyright (c) 2013
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
 * lzo_wrapper.h
 *
 */

#ifndef linux
#define __BYTE_ORDER BYTE_ORDER
#define __BIG_ENDIAN BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#else
#include <endian.h>
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
extern unsigned int inswap_le32(unsigned int);

#define SQUASHFS_INSWAP_COMP_OPTS(s) { \
	(s)->algorithm = inswap_le32((s)->algorithm); \
	(s)->compression_level = inswap_le32((s)->compression_level); \
}
#else
#define SQUASHFS_INSWAP_COMP_OPTS(s)
#endif

/* Define the compression flags recognised. */
#define SQUASHFS_LZO1X_1	0
#define SQUASHFS_LZO1X_1_11	1
#define SQUASHFS_LZO1X_1_12	2
#define SQUASHFS_LZO1X_1_15	3
#define SQUASHFS_LZO1X_999	4

/* Default compression level used by SQUASHFS_LZO1X_999 */
#define SQUASHFS_LZO1X_999_COMP_DEFAULT	8

struct lzo_comp_opts {
	int algorithm;
	int compression_level;
};

struct lzo_algorithm {
	char *name;
	int size;
	int (*compress) (const lzo_bytep, lzo_uint, lzo_bytep, lzo_uintp,
		lzo_voidp);
};

struct lzo_stream {
	void *workspace;
	void *buffer;
};

#define LZO_MAX_EXPANSION(size)	(size + (size / 16) + 64 + 3)

int lzo1x_999_wrapper(const lzo_bytep, lzo_uint, lzo_bytep, lzo_uintp,
		lzo_voidp);

#endif
