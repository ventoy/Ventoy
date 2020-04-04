/*
 * Copyright (c) 2013, 2019
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
 * lz4_wrapper.c
 *
 * Support for LZ4 compression http://fastcompression.blogspot.com/p/lz4.html
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <lz4.h>
#include <lz4hc.h>

#include "squashfs_fs.h"
#include "lz4_wrapper.h"
#include "compressor.h"

/* LZ4 1.7.0 introduced new functions, and since r131,
 * the older functions produce deprecated warnings.
 *
 * There are still too many distros using older versions
 * to switch to the newer functions, but, the deprecated
 * functions may completely disappear.  This is a mess.
 *
 * Support both by checking the library version and
 * using shadow definitions
 */

/* Earlier (but > 1.7.0) versions don't define this */
#ifndef LZ4HC_CLEVEL_MAX
#define LZ4HC_CLEVEL_MAX 12
#endif

#if LZ4_VERSION_NUMBER >= 10700
#define COMPRESS(src, dest, size, max)		 LZ4_compress_default(src, dest, size, max)
#define COMPRESS_HC(src, dest, size, max)	 LZ4_compress_HC(src, dest, size, max, LZ4HC_CLEVEL_MAX)
#else
#define COMPRESS(src, dest, size, max)		 LZ4_compress_limitedOutput(src, dest, size, max)
#define COMPRESS_HC(src, dest, size, max)	 LZ4_compressHC_limitedOutput(src, dest, size, max)
#endif

static int hc = 0;

/*
 * This function is called by the options parsing code in mksquashfs.c
 * to parse any -X compressor option.
 *
 * This function returns:
 *	>=0 (number of additional args parsed) on success
 *	-1 if the option was unrecognised, or
 *	-2 if the option was recognised, but otherwise bad in
 *	   some way (e.g. invalid parameter)
 *
 * Note: this function sets internal compressor state, but does not
 * pass back the results of the parsing other than success/failure.
 * The lz4_dump_options() function is called later to get the options in
 * a format suitable for writing to the filesystem.
 */
static int lz4_options(char *argv[], int argc)
{
	if(strcmp(argv[0], "-Xhc") == 0) {
		hc = 1;
		return 0;
	}

	return -1;
}


/*
 * This function is called by mksquashfs to dump the parsed
 * compressor options in a format suitable for writing to the
 * compressor options field in the filesystem (stored immediately
 * after the superblock).
 *
 * This function returns a pointer to the compression options structure
 * to be stored (and the size), or NULL if there are no compression
 * options
 *
 * Currently LZ4 always returns a comp_opts structure, with
 * the version indicating LZ4_LEGACY stream fomat.  This is to
 * easily accomodate changes in the kernel code to different
 * stream formats 
 */
static void *lz4_dump_options(int block_size, int *size)
{
	static struct lz4_comp_opts comp_opts;

	comp_opts.version = LZ4_LEGACY;
	comp_opts.flags = hc ? LZ4_HC : 0;
	SQUASHFS_INSWAP_COMP_OPTS(&comp_opts);

	*size = sizeof(comp_opts);
	return &comp_opts;
}


/*
 * This function is a helper specifically for the append mode of
 * mksquashfs.  Its purpose is to set the internal compressor state
 * to the stored compressor options in the passed compressor options
 * structure.
 *
 * In effect this function sets up the compressor options
 * to the same state they were when the filesystem was originally
 * generated, this is to ensure on appending, the compressor uses
 * the same compression options that were used to generate the
 * original filesystem.
 *
 * Note, even if there are no compressor options, this function is still
 * called with an empty compressor structure (size == 0), to explicitly
 * set the default options, this is to ensure any user supplied
 * -X options on the appending mksquashfs command line are over-ridden
 *
 * This function returns 0 on sucessful extraction of options, and
 *			-1 on error
 */
static int lz4_extract_options(int block_size, void *buffer, int size)
{
	struct lz4_comp_opts *comp_opts = buffer;

	/* we expect a comp_opts structure to be present */
	if(size < sizeof(*comp_opts))
		goto failed;

	SQUASHFS_INSWAP_COMP_OPTS(comp_opts);

	/* we expect the stream format to be LZ4_LEGACY */
	if(comp_opts->version != LZ4_LEGACY) {
		fprintf(stderr, "lz4: unknown LZ4 version\n");
		goto failed;
	}

	/*
	 * Check compression flags, currently only LZ4_HC ("high compression")
	 * can be set.
	 */
	if(comp_opts->flags == LZ4_HC)
		hc = 1;
	else if(comp_opts->flags != 0) {
		fprintf(stderr, "lz4: unknown LZ4 flags\n");
		goto failed;
	}

	return 0;

failed:
	fprintf(stderr, "lz4: error reading stored compressor options from "
		"filesystem!\n");

	return -1;
}


/*
 * This function is a helper specifically for unsquashfs.
 * Its purpose is to check that the compression options are
 * understood by this version of LZ4.
 *
 * This is important for LZ4 because the format understood by the
 * Linux kernel may change from the already obsolete legacy format
 * currently supported.
 *
 * If this does happen, then this version of LZ4 will not be able to decode
 * the newer format.  So we need to check for this.
 *
 * This function returns 0 on sucessful checking of options, and
 *			-1 on error
 */
static int lz4_check_options(int block_size, void *buffer, int size)
{
	struct lz4_comp_opts *comp_opts = buffer;

	/* we expect a comp_opts structure to be present */
	if(size < sizeof(*comp_opts))
		goto failed;

	SQUASHFS_INSWAP_COMP_OPTS(comp_opts);

	/* we expect the stream format to be LZ4_LEGACY */
	if(comp_opts->version != LZ4_LEGACY) {
		fprintf(stderr, "lz4: unknown LZ4 version\n");
		goto failed;
	}

	return 0;

failed:
	fprintf(stderr, "lz4: error reading stored compressor options from "
		"filesystem!\n");
	return -1;
}


static void lz4_display_options(void *buffer, int size)
{
	struct lz4_comp_opts *comp_opts = buffer;

	/* check passed comp opts struct is of the correct length */
	if(size < sizeof(*comp_opts))
		goto failed;

	SQUASHFS_INSWAP_COMP_OPTS(comp_opts);

	/* we expect the stream format to be LZ4_LEGACY */
	if(comp_opts->version != LZ4_LEGACY) {
		fprintf(stderr, "lz4: unknown LZ4 version\n");
		goto failed;
	}

	/*
	 * Check compression flags, currently only LZ4_HC ("high compression")
	 * can be set.
	 */
	if(comp_opts->flags & ~LZ4_FLAGS_MASK) {
		fprintf(stderr, "lz4: unknown LZ4 flags\n");
		goto failed;
	}

	if(comp_opts->flags & LZ4_HC)
		printf("\tHigh Compression option specified (-Xhc)\n");

	return;

failed:
	fprintf(stderr, "lz4: error reading stored compressor options from "
		"filesystem!\n");
}	


static int lz4_compress(void *strm, void *dest, void *src,  int size,
	int block_size, int *error)
{
	return 0;
}


static int lz4_uncompress(void *dest, void *src, int size, int outsize,
	int *error)
{
	int res = LZ4_decompress_safe(src, dest, size, outsize);
	if(res < 0) {
		*error = res;
		return -1;
	}

	return res;
}


static void lz4_usage()
{
	fprintf(stderr, "\t  -Xhc\n");
	fprintf(stderr, "\t\tCompress using LZ4 High Compression\n");
}


struct compressor lz4_comp_ops = {
	.compress = lz4_compress,
	.uncompress = lz4_uncompress,
	.options = lz4_options,
	.dump_options = lz4_dump_options,
	.extract_options = lz4_extract_options,
	.check_options = lz4_check_options,
	.display_options = lz4_display_options,
	.usage = lz4_usage,
	.id = LZ4_COMPRESSION,
	.name = "lz4",
	.supported = 1
};
