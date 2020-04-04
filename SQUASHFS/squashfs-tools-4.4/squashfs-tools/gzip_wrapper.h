#ifndef GZIP_WRAPPER_H
#define GZIP_WRAPPER_H
/*
 * Squashfs
 *
 * Copyright (c) 2014
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
 * gzip_wrapper.h
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
extern unsigned int inswap_le16(unsigned short);
extern unsigned int inswap_le32(unsigned int);

#define SQUASHFS_INSWAP_COMP_OPTS(s) { \
	(s)->compression_level = inswap_le32((s)->compression_level); \
	(s)->window_size = inswap_le16((s)->window_size); \
	(s)->strategy = inswap_le16((s)->strategy); \
}
#else
#define SQUASHFS_INSWAP_COMP_OPTS(s)
#endif

/* Default compression */
#define GZIP_DEFAULT_COMPRESSION_LEVEL 9
#define GZIP_DEFAULT_WINDOW_SIZE 15

struct gzip_comp_opts {
	int compression_level;
	short window_size;
	short strategy;
};

struct strategy {
	char *name;
	int strategy;
	int selected;
};

struct gzip_strategy {
	int strategy;
	int length;
	void *buffer;
};

struct gzip_stream {
	z_stream stream;
	int strategies;
	struct gzip_strategy strategy[0];
};
#endif
