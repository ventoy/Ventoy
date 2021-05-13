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
 * LZNT1 decompression
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "wimboot.h"
#include "lznt1.h"

/**
 * Decompress LZNT1-compressed data block
 *
 * @v data		Compressed data
 * @v limit		Length of compressed data up to end of block
 * @v offset		Starting offset within compressed data
 * @v block		Decompression buffer for this block, or NULL
 * @ret out_len		Length of decompressed block, or negative error
 */
static ssize_t lznt1_block ( const void *data, size_t limit, size_t offset,
			     void *block ) {
	const uint16_t *tuple;
	const uint8_t *copy_src;
	uint8_t *copy_dest = block;
	size_t copy_len;
	size_t block_out_len = 0;
	unsigned int split = 12;
	unsigned int next_threshold = 16;
	unsigned int tag_bit = 0;
	unsigned int tag = 0;

	while ( offset != limit ) {

		/* Extract tag */
		if ( tag_bit == 0 ) {
			tag = *( ( uint8_t * ) ( data + offset ) );
			offset++;
			if ( offset == limit )
				break;
		}

		/* Calculate copy source and length */
		if ( tag & 1 ) {

			/* Compressed value */
			if ( offset + sizeof ( *tuple ) > limit ) {
				DBG ( "LZNT1 compressed value overrun at "
				      "%#zx\n", offset );
				return -1;
			}
			tuple = ( data + offset );
			offset += sizeof ( *tuple );
			copy_len = LZNT1_VALUE_LEN ( *tuple, split );
			block_out_len += copy_len;
			if ( copy_dest ) {
				copy_src = ( copy_dest -
					     LZNT1_VALUE_OFFSET ( *tuple,
								  split ) );
				while ( copy_len-- )
					*(copy_dest++) = *(copy_src++);
			}

		} else {

			/* Uncompressed value */
			copy_src = ( data + offset );
			if ( copy_dest )
				*(copy_dest++) = *copy_src;
			offset++;
			block_out_len++;
		}

		/* Update split, if applicable */
		while ( block_out_len > next_threshold ) {
			split--;
			next_threshold <<= 1;
		}

		/* Move to next value */
		tag >>= 1;
		tag_bit = ( ( tag_bit + 1 ) % 8 );
	}

	return block_out_len;
}

/**
 * Decompress LZNT1-compressed data
 *
 * @v data		Compressed data
 * @v len		Length of compressed data
 * @v buf		Decompression buffer, or NULL
 * @ret out_len		Length of decompressed data, or negative error
 */
ssize_t lznt1_decompress ( const void *data, size_t len, void *buf ) {
	const uint16_t *header;
	const uint8_t *end;
	size_t offset = 0;
	ssize_t out_len = 0;
	size_t block_len;
	size_t limit;
	void *block;
	ssize_t block_out_len;

	while ( offset != len ) {

		/* Check for end marker */
		if ( ( offset + sizeof ( *end ) ) == len ) {
			end = ( data + offset );
			if ( *end == 0 )
				break;
		}

		/* Extract block header */
		if ( ( offset + sizeof ( *header ) ) > len ) {
			DBG ( "LZNT1 block header overrun at %#zx\n", offset );
			return -1;
		}
		header = ( data + offset );
		offset += sizeof ( *header );

		/* Process block */
		block_len = LZNT1_BLOCK_LEN ( *header );
		if ( LZNT1_BLOCK_COMPRESSED ( *header ) ) {

			/* Compressed block */
			DBG2 ( "LZNT1 compressed block %#zx+%#zx\n",
			       offset, block_len );
			limit = ( offset + block_len );
			block = ( buf ? ( buf + out_len ) : NULL );
			block_out_len = lznt1_block ( data, limit, offset,
						      block );
			if ( block_out_len < 0 )
				return block_out_len;
			offset += block_len;
			out_len += block_out_len;

		} else {

			/* Uncompressed block */
			if ( ( offset + block_len ) > len ) {
				DBG ( "LZNT1 uncompressed block overrun at "
				      "%#zx+%#zx\n", offset, block_len );
				return -1;
			}
			DBG2 ( "LZNT1 uncompressed block %#zx+%#zx\n",
			       offset, block_len );
			if ( buf ) {
				memcpy ( ( buf + out_len ), ( data + offset ),
					 block_len );
			}
			offset += block_len;
			out_len += block_len;
		}
	}

	return out_len;
}
