/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * @file
 *
 * Xpress Compression Algorithm (MS-XCA) decompression
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "wimboot.h"
#include "huffman.h"
#include "xca.h"

/**
 * Decompress XCA-compressed data
 *
 * @v data		Compressed data
 * @v len		Length of compressed data
 * @v buf		Decompression buffer, or NULL
 * @ret out_len		Length of decompressed data, or negative error
 */
ssize_t xca_decompress ( const void *data, size_t len, void *buf ) {
	const void *src = data;
	const void *end = ( src + len );
	uint8_t *out = buf;
	size_t out_len = 0;
	size_t out_len_threshold = 0;
	const struct xca_huf_len *lengths;
	struct xca xca;
	uint32_t accum = 0;
	int extra_bits = 0;
	unsigned int huf;
	struct huffman_symbols *sym;
	unsigned int raw;
	unsigned int match_len;
	unsigned int match_offset_bits;
	unsigned int match_offset;
	const uint8_t *copy;
	int rc;

	/* Process data stream */
	while ( src < end ) {

		/* (Re)initialise decompressor if applicable */
		if ( out_len >= out_len_threshold ) {

			/* Construct symbol lengths */
			lengths = src;
			src += sizeof ( *lengths );
			if ( src > end ) {
				DBG ( "XCA too short to hold Huffman lengths "
				      "table at input offset %#zx\n",
				      ( src - data ) );
				return -1;
			}
			for ( raw = 0 ; raw < XCA_CODES ; raw++ )
				xca.lengths[raw] = xca_huf_len ( lengths, raw );

			/* Construct Huffman alphabet */
			if ( ( rc = huffman_alphabet ( &xca.alphabet,
						       xca.lengths,
						       XCA_CODES ) ) != 0 )
				return rc;

			/* Initialise state */
			accum = XCA_GET16 ( src );
			accum <<= 16;
			accum |= XCA_GET16 ( src );
			extra_bits = 16;

			/* Determine next threshold */
			out_len_threshold = ( out_len + XCA_BLOCK_SIZE );
		}

		/* Determine symbol */
		huf = ( accum >> ( 32 - HUFFMAN_BITS ) );
		sym = huffman_sym ( &xca.alphabet, huf );
		raw = huffman_raw ( sym, huf );
		accum <<= huffman_len ( sym );
		extra_bits -= huffman_len ( sym );
		if ( extra_bits < 0 ) {
			accum |= ( XCA_GET16 ( src ) << ( -extra_bits ) );
			extra_bits += 16;
		}

		/* Process symbol */
		if ( raw < XCA_END_MARKER ) {

			/* Literal symbol - add to output stream */
			if ( buf )
				*(out++) = raw;
			out_len++;

		} else if ( ( raw == XCA_END_MARKER ) &&
			    ( src >= ( end - 1 ) ) ) {

			/* End marker symbol */
			return out_len;

		} else {

			/* LZ77 match symbol */
			raw -= XCA_END_MARKER;
			match_offset_bits = ( raw >> 4 );
			match_len = ( raw & 0x0f );
			if ( match_len == 0x0f ) {
				match_len = XCA_GET8 ( src );
				if ( match_len == 0xff ) {
					match_len = XCA_GET16 ( src );
				} else {
					match_len += 0x0f;
				}
			}
			match_len += 3;
			if ( match_offset_bits ) {
				match_offset =
					( ( accum >> ( 32 - match_offset_bits ))
					  + ( 1 << match_offset_bits ) );
			} else {
				match_offset = 1;
			}
			accum <<= match_offset_bits;
			extra_bits -= match_offset_bits;
			if ( extra_bits < 0 ) {
				accum |= ( XCA_GET16 ( src ) << (-extra_bits) );
				extra_bits += 16;
			}

			/* Copy data */
			out_len += match_len;
			if ( buf ) {
				copy = ( out - match_offset );
				while ( match_len-- )
					*(out++) = *(copy++);
			}
		}
	}

	DBG ( "XCA input overrun at output length %#zx\n", out_len );
	return -1;
}
