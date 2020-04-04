/*
 * Copyright (c) 2013, 2014
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
 * lzo_wrapper.c
 *
 * Support for LZO compression http://www.oberhumer.com/opensource/lzo
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <lzo/lzoconf.h>
#include <lzo/lzo1x.h>

#include "squashfs_fs.h"
#include "lzo_wrapper.h"
#include "compressor.h"

static struct lzo_algorithm lzo[] = {
	{ "lzo1x_1", LZO1X_1_MEM_COMPRESS, lzo1x_1_compress },
	{ "lzo1x_1_11", LZO1X_1_11_MEM_COMPRESS, lzo1x_1_11_compress },
	{ "lzo1x_1_12", LZO1X_1_12_MEM_COMPRESS, lzo1x_1_12_compress },
	{ "lzo1x_1_15", LZO1X_1_15_MEM_COMPRESS, lzo1x_1_15_compress },
	{ "lzo1x_999", LZO1X_999_MEM_COMPRESS, lzo1x_999_wrapper },
	{ NULL, 0, NULL } 
};

/* default LZO compression algorithm and compression level */
static int algorithm = SQUASHFS_LZO1X_999;
static int compression_level = SQUASHFS_LZO1X_999_COMP_DEFAULT;

/* user specified compression level */
static int user_comp_level = -1;


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
 * The lzo_dump_options() function is called later to get the options in
 * a format suitable for writing to the filesystem.
 */
static int lzo_options(char *argv[], int argc)
{
    (void)argv;
    (void)argc;
	return 1;
}


/*
 * This function is called after all options have been parsed.
 * It is used to do post-processing on the compressor options using
 * values that were not expected to be known at option parse time.
 *
 * In this case the LZO algorithm may not be known until after the
 * compression level has been set (-Xalgorithm used after -Xcompression-level)
 *
 * This function returns 0 on successful post processing, or
 *			-1 on error
 */
static int lzo_options_post(int block_size)
{
	/*
	 * Use of compression level only makes sense for
	 * LZO1X_999 algorithm
	 */
	if(user_comp_level != -1) {
		if(algorithm != SQUASHFS_LZO1X_999) {
			fprintf(stderr, "lzo: -Xcompression-level not "
				"supported by selected %s algorithm\n",
				lzo[algorithm].name);
			fprintf(stderr, "lzo: -Xcompression-level is only "
				"applicable for the lzo1x_999 algorithm\n");
			goto failed;
		}
		compression_level = user_comp_level;
	}

	return 0;

failed:
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
 */
static void *lzo_dump_options(int block_size, int *size)
{
	static struct lzo_comp_opts comp_opts;

	/*
	 * If default compression options of SQUASHFS_LZO1X_999 and
	 * compression level of SQUASHFS_LZO1X_999_COMP_DEFAULT then
	 * don't store a compression options structure (this is compatible
	 * with the legacy implementation of LZO for Squashfs)
	 */
	if(algorithm == SQUASHFS_LZO1X_999 &&
			compression_level == SQUASHFS_LZO1X_999_COMP_DEFAULT)
		return NULL;

	comp_opts.algorithm = algorithm;
	comp_opts.compression_level = algorithm == SQUASHFS_LZO1X_999 ?
		compression_level : 0;

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
static int lzo_extract_options(int block_size, void *buffer, int size)
{
	struct lzo_comp_opts *comp_opts = buffer;

	if(size == 0) {
		/* Set default values */
		algorithm = SQUASHFS_LZO1X_999;
		compression_level = SQUASHFS_LZO1X_999_COMP_DEFAULT;
		return 0;
	}

	/* we expect a comp_opts structure of sufficient size to be present */
	if(size < sizeof(*comp_opts))
		goto failed;

	SQUASHFS_INSWAP_COMP_OPTS(comp_opts);

	/* Check comp_opts structure for correctness */
	switch(comp_opts->algorithm) {
	case SQUASHFS_LZO1X_1:
	case SQUASHFS_LZO1X_1_11:
	case SQUASHFS_LZO1X_1_12:
	case SQUASHFS_LZO1X_1_15:
		if(comp_opts->compression_level != 0) {
			fprintf(stderr, "lzo: bad compression level in "
				"compression options structure\n");
			goto failed;
		}
		break;
	case SQUASHFS_LZO1X_999:
		if(comp_opts->compression_level < 1 ||
				comp_opts->compression_level > 9) {
			fprintf(stderr, "lzo: bad compression level in "
				"compression options structure\n");
			goto failed;
		}
		compression_level = comp_opts->compression_level;
		break;
	default:
		fprintf(stderr, "lzo: bad algorithm in compression options "
				"structure\n");
			goto failed;
	}

	algorithm = comp_opts->algorithm;

	return 0;

failed:
	fprintf(stderr, "lzo: error reading stored compressor options from "
		"filesystem!\n");

	return -1;
}


static void lzo_display_options(void *buffer, int size)
{
	struct lzo_comp_opts *comp_opts = buffer;

	/* we expect a comp_opts structure of sufficient size to be present */
	if(size < sizeof(*comp_opts))
		goto failed;

	SQUASHFS_INSWAP_COMP_OPTS(comp_opts);

	/* Check comp_opts structure for correctness */
	switch(comp_opts->algorithm) {
	case SQUASHFS_LZO1X_1:
	case SQUASHFS_LZO1X_1_11:
	case SQUASHFS_LZO1X_1_12:
	case SQUASHFS_LZO1X_1_15:
		printf("\talgorithm %s\n", lzo[comp_opts->algorithm].name);
		break;
	case SQUASHFS_LZO1X_999:
		if(comp_opts->compression_level < 1 ||
				comp_opts->compression_level > 9) {
			fprintf(stderr, "lzo: bad compression level in "
				"compression options structure\n");
			goto failed;
		}
		printf("\talgorithm %s\n", lzo[comp_opts->algorithm].name);
		printf("\tcompression level %d\n",
						comp_opts->compression_level);
		break;
	default:
		fprintf(stderr, "lzo: bad algorithm in compression options "
				"structure\n");
			goto failed;
	}

	return;

failed:
	fprintf(stderr, "lzo: error reading stored compressor options from "
		"filesystem!\n");
}	


/*
 * This function is called by mksquashfs to initialise the
 * compressor, before compress() is called.
 *
 * This function returns 0 on success, and
 *			-1 on error
 */
static int squashfs_lzo_init(void **strm, int block_size, int datablock)
{
	struct lzo_stream *stream;

	stream = *strm = malloc(sizeof(struct lzo_stream));
	if(stream == NULL)
		goto failed;

	stream->workspace = malloc(lzo[algorithm].size);
	if(stream->workspace == NULL)
		goto failed2;

	stream->buffer = malloc(LZO_MAX_EXPANSION(block_size));
	if(stream->buffer != NULL)
		return 0;

	free(stream->workspace);
failed2:
	free(stream);
failed:
	return -1;
}


static int lzo_compress(void *strm, void *dest, void *src,  int size,
	int block_size, int *error)
{
    
	return 0;
}


static int lzo_uncompress(void *dest, void *src, int size, int outsize,
	int *error)
{
	int res;
	lzo_uint outlen = outsize;

	res = lzo1x_decompress_safe(src, size, dest, &outlen, NULL);
	if(res != LZO_E_OK) {
		*error = res;
		return -1;
	}

	return outlen;
}


static void lzo_usage()
{
	int i;

	fprintf(stderr, "\t  -Xalgorithm <algorithm>\n");
	fprintf(stderr, "\t\tWhere <algorithm> is one of:\n");

	for(i = 0; lzo[i].name; i++)
		fprintf(stderr, "\t\t\t%s%s\n", lzo[i].name,
				i == SQUASHFS_LZO1X_999 ? " (default)" : "");

	fprintf(stderr, "\t  -Xcompression-level <compression-level>\n");
	fprintf(stderr, "\t\t<compression-level> should be 1 .. 9 (default "
		"%d)\n", SQUASHFS_LZO1X_999_COMP_DEFAULT);
	fprintf(stderr, "\t\tOnly applies to lzo1x_999 algorithm\n");
}


/*
 * Helper function for lzo1x_999 compression algorithm.
 * All other lzo1x_xxx compressors do not take a compression level,
 * so we need to wrap lzo1x_999 to pass the compression level which
 * is applicable to it
 */
int lzo1x_999_wrapper(const lzo_bytep src, lzo_uint src_len, lzo_bytep dst,
	lzo_uintp compsize, lzo_voidp workspace)
{
	return lzo1x_999_compress_level(src, src_len, dst, compsize,
		workspace, NULL, 0, 0, compression_level);
}


struct compressor lzo_comp_ops = {
	.init = squashfs_lzo_init,
	.compress = lzo_compress,
	.uncompress = lzo_uncompress,
	.options = lzo_options,
	.options_post = lzo_options_post,
	.dump_options = lzo_dump_options,
	.extract_options = lzo_extract_options,
	.display_options = lzo_display_options,
	.usage = lzo_usage,
	.id = LZO_COMPRESSION,
	.name = "lzo",
	.supported = 1
};
