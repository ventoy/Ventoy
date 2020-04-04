/*
 * Copyright (c) 2009, 2010, 2013
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
 * lzma_wrapper.c
 *
 * Support for LZMA1 compression using LZMA SDK (4.65 used in
 * development, other versions may work) http://www.7-zip.org/sdk.html
 */

#include <LzmaLib.h>

#include "squashfs_fs.h"
#include "compressor.h"

#define LZMA_HEADER_SIZE	(LZMA_PROPS_SIZE + 8)

static int lzma_compress(void *strm, void *dest, void *src, int size, int block_size,
		int *error)
{
	return 0;
}


static int lzma_uncompress(void *dest, void *src, int size, int outsize,
	int *error)
{
	unsigned char *s = src;
	size_t outlen, inlen = size - LZMA_HEADER_SIZE;
	int res;

	outlen = s[LZMA_PROPS_SIZE] |
		(s[LZMA_PROPS_SIZE + 1] << 8) |
		(s[LZMA_PROPS_SIZE + 2] << 16) |
		(s[LZMA_PROPS_SIZE + 3] << 24);

	if(outlen > outsize) {
		*error = 0;
		return -1;
	}

	res = LzmaUncompress(dest, &outlen, src + LZMA_HEADER_SIZE, &inlen, src,
		LZMA_PROPS_SIZE);
	
	if(res == SZ_OK)
		return outlen;
	else {
		*error = res;
		return -1;
	}
}


struct compressor lzma_comp_ops = {
	.init = NULL,
	.compress = lzma_compress,
	.uncompress = lzma_uncompress,
	.options = NULL,
	.usage = NULL,
	.id = LZMA_COMPRESSION,
	.name = "lzma",
	.supported = 1
};

