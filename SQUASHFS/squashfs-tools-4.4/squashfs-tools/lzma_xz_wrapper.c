/*
 * Copyright (c) 2010, 2013
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
 * lzma_xz_wrapper.c
 *
 * Support for LZMA1 compression using XZ Utils liblzma http://tukaani.org/xz/
 */

#include <stdio.h>
#include <string.h>
#include <lzma.h>

#include "squashfs_fs.h"
#include "compressor.h"

#define LZMA_PROPS_SIZE 5
#define LZMA_UNCOMP_SIZE 8
#define LZMA_HEADER_SIZE (LZMA_PROPS_SIZE + LZMA_UNCOMP_SIZE)

#define LZMA_OPTIONS 5
#define MEMLIMIT (32 * 1024 * 1024)

static int lzma_compress(void *dummy, void *dest, void *src,  int size,
	int block_size, int *error)
{
	unsigned char *d = (unsigned char *) dest;
	lzma_options_lzma opt;
	lzma_stream strm = LZMA_STREAM_INIT;
	int res;

	lzma_lzma_preset(&opt, LZMA_OPTIONS);
	opt.dict_size = block_size;

	res = lzma_alone_encoder(&strm, &opt);
	if(res != LZMA_OK) {
		lzma_end(&strm);
		goto failed;
	}

	strm.next_out = dest;
	strm.avail_out = block_size;
	strm.next_in = src;
	strm.avail_in = size;

	res = lzma_code(&strm, LZMA_FINISH);
	lzma_end(&strm);

	if(res == LZMA_STREAM_END) {
		/*
	 	 * Fill in the 8 byte little endian uncompressed size field in
		 * the LZMA header.  8 bytes is excessively large for squashfs
		 * but this is the standard LZMA header and which is expected by
		 * the kernel code
	 	 */

		d[LZMA_PROPS_SIZE] = size & 255;
		d[LZMA_PROPS_SIZE + 1] = (size >> 8) & 255;
		d[LZMA_PROPS_SIZE + 2] = (size >> 16) & 255;
		d[LZMA_PROPS_SIZE + 3] = (size >> 24) & 255;
		d[LZMA_PROPS_SIZE + 4] = 0;
		d[LZMA_PROPS_SIZE + 5] = 0;
		d[LZMA_PROPS_SIZE + 6] = 0;
		d[LZMA_PROPS_SIZE + 7] = 0;

		return (int) strm.total_out;
	}

	if(res == LZMA_OK)
		/*
	 	 * Output buffer overflow.  Return out of buffer space
	 	 */
		return 0;

failed:
	/*
	 * All other errors return failure, with the compressor
	 * specific error code in *error
	 */
	*error = res;
	return -1;
}


static int lzma_uncompress(void *dest, void *src, int size, int outsize,
	int *error)
{
	lzma_stream strm = LZMA_STREAM_INIT;
	int uncompressed_size = 0, res;
	unsigned char lzma_header[LZMA_HEADER_SIZE];

	res = lzma_alone_decoder(&strm, MEMLIMIT);
	if(res != LZMA_OK) {
		lzma_end(&strm);
		goto failed;
	}

	memcpy(lzma_header, src, LZMA_HEADER_SIZE);
	uncompressed_size = lzma_header[LZMA_PROPS_SIZE] |
		(lzma_header[LZMA_PROPS_SIZE + 1] << 8) |
		(lzma_header[LZMA_PROPS_SIZE + 2] << 16) |
		(lzma_header[LZMA_PROPS_SIZE + 3] << 24);

	if(uncompressed_size > outsize) {
		res = 0;
		goto failed;
	}

	memset(lzma_header + LZMA_PROPS_SIZE, 255, LZMA_UNCOMP_SIZE);

	strm.next_out = dest;
	strm.avail_out = outsize;
	strm.next_in = lzma_header;
	strm.avail_in = LZMA_HEADER_SIZE;

	res = lzma_code(&strm, LZMA_RUN);

	if(res != LZMA_OK || strm.avail_in != 0) {
		lzma_end(&strm);
		goto failed;
	}

	strm.next_in = src + LZMA_HEADER_SIZE;
	strm.avail_in = size - LZMA_HEADER_SIZE;

	res = lzma_code(&strm, LZMA_FINISH);
	lzma_end(&strm);

	if(res == LZMA_STREAM_END || (res == LZMA_OK &&
		strm.total_out >= uncompressed_size && strm.avail_in == 0))
		return uncompressed_size;

failed:
	*error = res;
	return -1;
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

