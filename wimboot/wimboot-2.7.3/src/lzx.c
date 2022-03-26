/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
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
 * LZX decompression
 *
 * This algorithm is derived jointly from the document "[MS-PATCH]:
 * LZX DELTA Compression and Decompression", available from
 *
 *     http://msdn.microsoft.com/en-us/library/cc483133.aspx
 *
 * and from the file lzx-decompress.c in the wimlib source code.
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "wimboot.h"
#include "huffman.h"
#include "lzx.h"

/** Base positions, indexed by position slot */
static unsigned int lzx_position_base[LZX_POSITION_SLOTS];

/**
 * Attempt to accumulate bits from LZX bitstream
 *
 * @v lzx		Decompressor
 * @v bits		Number of bits to accumulate
 * @v norm_value	Accumulated value (normalised to 16 bits)
 *
 * Note that there may not be sufficient accumulated bits in the
 * bitstream; callers must check that sufficient bits are available
 * before using the value.
 */
static int lzx_accumulate ( struct lzx *lzx, unsigned int bits ) {
	const uint16_t *src16;

	/* Accumulate more bits if required */
	if ( ( lzx->bits < bits ) &&
	     ( lzx->input.offset < lzx->input.len ) ) {
		src16 = ( ( void * ) lzx->input.data + lzx->input.offset );
		lzx->input.offset += sizeof ( *src16 );
		lzx->accumulator |= ( *src16 << ( 16 - lzx->bits ) );
		lzx->bits += 16;
	}

	return ( lzx->accumulator >> 16 );
}

/**
 * Consume accumulated bits from LZX bitstream
 *
 * @v lzx		Decompressor
 * @v bits		Number of bits to consume
 * @ret rc		Return status code
 */
static int lzx_consume ( struct lzx *lzx, unsigned int bits ) {

	/* Fail if insufficient bits are available */
	if ( lzx->bits < bits ) {
		DBG ( "LZX input overrun in %#zx/%#zx out %#zx)\n",
		      lzx->input.offset, lzx->input.len, lzx->output.offset );
		return -1;
	}

	/* Consume bits */
	lzx->accumulator <<= bits;
	lzx->bits -= bits;

	return 0;
}

/**
 * Get bits from LZX bitstream
 *
 * @v lzx		Decompressor
 * @v bits		Number of bits to fetch
 * @ret value		Value, or negative error
 */
static int lzx_getbits ( struct lzx *lzx, unsigned int bits ) {
	int norm_value;
	int rc;

	/* Accumulate more bits if required */
	norm_value = lzx_accumulate ( lzx, bits );

	/* Consume bits */
	if ( ( rc = lzx_consume ( lzx, bits ) ) != 0 )
		return rc;

	return ( norm_value >> ( 16 - bits ) );
}

/**
 * Align LZX bitstream for byte access
 *
 * @v lzx		Decompressor
 * @v bits		Minimum number of padding bits
 * @ret rc		Return status code
 */
static int lzx_align ( struct lzx *lzx, unsigned int bits ) {
	int pad;

	/* Get padding bits */
	pad = lzx_getbits ( lzx, bits );
	if ( pad < 0 )
		return pad;

	/* Consume all accumulated bits */
	lzx_consume ( lzx, lzx->bits );

	return 0;
}

/**
 * Get bytes from LZX bitstream
 *
 * @v lzx		Decompressor
 * @v data		Data buffer, or NULL
 * @v len		Length of data buffer
 * @ret rc		Return status code
 */
static int lzx_getbytes ( struct lzx *lzx, void *data, size_t len ) {

	/* Sanity check */
	if ( ( lzx->input.offset + len ) > lzx->input.len ) {
		DBG ( "LZX input overrun in %#zx/%#zx out %#zx)\n",
		      lzx->input.offset, lzx->input.len, lzx->output.offset );
		return -1;
	}

	/* Copy data */
	if ( data )
		memcpy ( data, ( lzx->input.data + lzx->input.offset ), len );
	lzx->input.offset += len;

	return 0;
}

/**
 * Decode LZX Huffman-coded symbol
 *
 * @v lzx		Decompressor
 * @v alphabet		Huffman alphabet
 * @ret raw		Raw symbol, or negative error
 */
static int lzx_decode ( struct lzx *lzx, struct huffman_alphabet *alphabet ) {
	struct huffman_symbols *sym;
	int huf;
	int rc;

	/* Accumulate sufficient bits */
	huf = lzx_accumulate ( lzx, HUFFMAN_BITS );
	if ( huf < 0 )
		return huf;

	/* Decode symbol */
	sym = huffman_sym ( alphabet, huf );

	/* Consume bits */
	if ( ( rc = lzx_consume ( lzx, huffman_len ( sym ) ) ) != 0 )
		return rc;

	return huffman_raw ( sym, huf );
}

/**
 * Generate Huffman alphabet from raw length table
 *
 * @v lzx		Decompressor
 * @v count		Number of symbols
 * @v bits		Length of each length (in bits)
 * @v lengths		Lengths table to fill in
 * @v alphabet		Huffman alphabet to fill in
 * @ret rc		Return status code
 */
static int lzx_raw_alphabet ( struct lzx *lzx, unsigned int count,
			      unsigned int bits, uint8_t *lengths,
			      struct huffman_alphabet *alphabet ) {
	unsigned int i;
	int len;
	int rc;

	/* Read lengths */
	for ( i = 0 ; i < count ; i++ ) {
		len = lzx_getbits ( lzx, bits );
		if ( len < 0 )
			return len;
		lengths[i] = len;
	}

	/* Generate Huffman alphabet */
	if ( ( rc = huffman_alphabet ( alphabet, lengths, count ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Generate pretree
 *
 * @v lzx		Decompressor
 * @v count		Number of symbols
 * @v lengths		Lengths table to fill in
 * @ret rc		Return status code
 */
static int lzx_pretree ( struct lzx *lzx, unsigned int count,
			 uint8_t *lengths ) {
	unsigned int i;
	unsigned int length;
	int dup = 0;
	int code;
	int rc;

	/* Generate pretree alphabet */
	if ( ( rc =  lzx_raw_alphabet ( lzx, LZX_PRETREE_CODES,
					LZX_PRETREE_BITS, lzx->pretree_lengths,
					&lzx->pretree ) ) != 0 )
		return rc;

	/* Read lengths */
	for ( i = 0 ; i < count ; i++ ) {

		if ( dup ) {

			/* Duplicate previous length */
			lengths[i] = lengths[ i - 1 ];
			dup--;

		} else {

			/* Get next code */
			code = lzx_decode ( lzx, &lzx->pretree );
			if ( code < 0 )
				return code;

			/* Interpret code */
			if ( code <= 16 ) {
				length = ( ( lengths[i] - code + 17 ) % 17 );
			} else if ( code == 17 ) {
				length = 0;
				dup = lzx_getbits ( lzx, 4 );
				if ( dup < 0 )
					return dup;
				dup += 3;
			} else if ( code == 18 ) {
				length = 0;
				dup = lzx_getbits ( lzx, 5 );
				if ( dup < 0 )
					return dup;
				dup += 19;
			} else if ( code == 19 ) {
				length = 0;
				dup = lzx_getbits ( lzx, 1 );
				if ( dup < 0 )
					return dup;
				dup += 3;
				code = lzx_decode ( lzx, &lzx->pretree );
				if ( code < 0 )
					return code;
				length = ( ( lengths[i] - code + 17 ) % 17 );
			} else {
				DBG ( "Unrecognised pretree code %d\n", code );
				return -1;
			}
			lengths[i] = length;
		}
	}

	/* Sanity check */
	if ( dup ) {
		DBG ( "Pretree duplicate overrun\n" );
		return -1;
	}

	return 0;
}

/**
 * Generate aligned offset Huffman alphabet
 *
 * @v lzx		Decompressor
 * @ret rc		Return status code
 */
static int lzx_alignoffset_alphabet ( struct lzx *lzx ) {
	int rc;

	/* Generate aligned offset alphabet */
	if ( ( rc = lzx_raw_alphabet ( lzx, LZX_ALIGNOFFSET_CODES,
				       LZX_ALIGNOFFSET_BITS,
				       lzx->alignoffset_lengths,
				       &lzx->alignoffset ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Generate main Huffman alphabet
 *
 * @v lzx		Decompressor
 * @ret rc		Return status code
 */
static int lzx_main_alphabet ( struct lzx *lzx ) {
	int rc;

	/* Generate literal symbols pretree */
	if ( ( rc = lzx_pretree ( lzx, LZX_MAIN_LIT_CODES,
				  lzx->main_lengths.literals ) ) != 0 ) {
		DBG ( "Could not construct main literal pretree\n" );
		return rc;
	}

	/* Generate remaining symbols pretree */
	if ( ( rc = lzx_pretree ( lzx, ( LZX_MAIN_CODES - LZX_MAIN_LIT_CODES ),
				  lzx->main_lengths.remainder ) ) != 0 ) {
		DBG ( "Could not construct main remainder pretree\n" );
		return rc;
	}

	/* Generate Huffman alphabet */
	if ( ( rc = huffman_alphabet ( &lzx->main, lzx->main_lengths.literals,
				       LZX_MAIN_CODES ) ) != 0 ) {
		DBG ( "Could not generate main alphabet\n" );
		return rc;
	}

	return 0;
}

/**
 * Generate length Huffman alphabet
 *
 * @v lzx		Decompressor
 * @ret rc		Return status code
 */
static int lzx_length_alphabet ( struct lzx *lzx ) {
	int rc;

	/* Generate pretree */
	if ( ( rc = lzx_pretree ( lzx, LZX_LENGTH_CODES,
				  lzx->length_lengths ) ) != 0 ) {
		DBG ( "Could not generate length pretree\n" );
		return rc;
	}

	/* Generate Huffman alphabet */
	if ( ( rc = huffman_alphabet ( &lzx->length, lzx->length_lengths,
				       LZX_LENGTH_CODES ) ) != 0 ) {
		DBG ( "Could not generate length alphabet\n" );
		return rc;
	}

	return 0;
}

/**
 * Process LZX block header
 *
 * @v lzx		Decompressor
 * @ret rc		Return status code
 */
static int lzx_block_header ( struct lzx *lzx ) {
	size_t block_len;
	int block_type;
	int default_len;
	int len_high;
	int len_low;
	int rc;

	/* Get block type */
	block_type = lzx_getbits ( lzx, LZX_BLOCK_TYPE_BITS );
	if ( block_type < 0 )
		return block_type;
	lzx->block_type = block_type;

	/* Check block length */
	default_len = lzx_getbits ( lzx, 1 );
	if ( default_len < 0 )
		return default_len;
	if ( default_len ) {
		block_len = LZX_DEFAULT_BLOCK_LEN;
	} else {
		len_high = lzx_getbits ( lzx, 8 );
		if ( len_high < 0 )
			return len_high;
		len_low = lzx_getbits ( lzx, 8 );
		if ( len_low < 0 )
			return len_low;
		block_len = ( ( len_high << 8 ) | len_low );
	}
	lzx->output.threshold = ( lzx->output.offset + block_len );

	/* Handle block type */
	switch ( block_type ) {
	case LZX_BLOCK_ALIGNOFFSET :
		/* Generated aligned offset alphabet */
		if ( ( rc = lzx_alignoffset_alphabet ( lzx ) ) != 0 )
			return rc;
		/* Fall through */
	case LZX_BLOCK_VERBATIM :
		/* Generate main alphabet */
		if ( ( rc = lzx_main_alphabet ( lzx ) ) != 0 )
			return rc;
		/* Generate lengths alphabet */
		if ( ( rc = lzx_length_alphabet ( lzx ) ) != 0 )
			return rc;
		break;
	case LZX_BLOCK_UNCOMPRESSED :
		/* Align input stream */
		if ( ( rc = lzx_align ( lzx, 1 ) ) != 0 )
			return rc;
		/* Read new repeated offsets */
		if ( ( rc = lzx_getbytes ( lzx, &lzx->repeated_offset,
					   sizeof ( lzx->repeated_offset )))!=0)
			return rc;
		break;
	default:
		DBG ( "Unrecognised block type %d\n", block_type );
		return -1;
	}

	return 0;
}

/**
 * Process uncompressed data
 *
 * @v lzx		Decompressor
 * @ret rc		Return status code
 */
static int lzx_uncompressed ( struct lzx *lzx ) {
	void *data;
	size_t len;
	int rc;

	/* Copy bytes */
	data = ( lzx->output.data ?
		 ( lzx->output.data + lzx->output.offset ) : NULL );
	len = ( lzx->output.threshold - lzx->output.offset );
	if ( ( rc = lzx_getbytes ( lzx, data, len ) ) != 0 )
		return rc;

	/* Align input stream */
	if ( len % 2 )
		lzx->input.offset++;

	return 0;
}

/**
 * Process an LZX token
 *
 * @v lzx		Decompressor
 * @ret rc		Return status code
 *
 * Variable names are chosen to match the LZX specification
 * pseudo-code.
 */
static int lzx_token ( struct lzx *lzx ) {
	unsigned int length_header;
	unsigned int position_slot;
	unsigned int offset_bits;
	unsigned int i;
	size_t match_offset;
	size_t match_length;
	int verbatim_bits;
	int aligned_bits;
	int main;
	int length;
	uint8_t *copy;

	/* Get main symelse*/
	main = lzx_decode ( lzx, &lzx->main );
	if ( main < 0 )
		return main;

	/* Check for literals */
	if ( main < LZX_MAIN_LIT_CODES ) {
		if ( lzx->output.data )
			lzx->output.data[lzx->output.offset] = main;
		lzx->output.offset++;
		return 0;
	}
	main -= LZX_MAIN_LIT_CODES;

	/* Calculate the match length */
	length_header = ( main & 7 );
	if ( length_header == 7 ) {
		length = lzx_decode ( lzx, &lzx->length );
		if ( length < 0 )
			return length;
	} else {
		length = 0;
	}
	match_length = ( length_header + 2 + length );

	/* Calculate the position slot */
	position_slot = ( main >> 3 );
	if ( position_slot < LZX_REPEATED_OFFSETS ) {

		/* Repeated offset */
		match_offset = lzx->repeated_offset[position_slot];
		lzx->repeated_offset[position_slot] = lzx->repeated_offset[0];
		lzx->repeated_offset[0] = match_offset;

	} else {

		/* Non-repeated offset */
		offset_bits = lzx_footer_bits ( position_slot );
		if ( ( lzx->block_type == LZX_BLOCK_ALIGNOFFSET ) &&
		     ( offset_bits >= 3 ) ) {
			verbatim_bits = lzx_getbits ( lzx, ( offset_bits - 3 ));
			if ( verbatim_bits < 0 )
				return verbatim_bits;
			verbatim_bits <<= 3;
			aligned_bits = lzx_decode ( lzx, &lzx->alignoffset );
			if ( aligned_bits < 0 )
				return aligned_bits;
		} else {
			verbatim_bits = lzx_getbits ( lzx, offset_bits );
			if ( verbatim_bits < 0 )
				return verbatim_bits;
			aligned_bits = 0;
		}
		match_offset = ( lzx_position_base[position_slot] +
				 verbatim_bits + aligned_bits - 2 );

		/* Update repeated offset list */
		for ( i = ( LZX_REPEATED_OFFSETS - 1 ) ; i > 0 ; i-- )
			lzx->repeated_offset[i] = lzx->repeated_offset[ i - 1 ];
		lzx->repeated_offset[0] = match_offset;
	}

	/* Copy data */
	if ( match_offset > lzx->output.offset ) {
		DBG ( "LZX match underrun out %#zx offset %#zx len %#zx\n",
		      lzx->output.offset, match_offset, match_length );
		return -1;
	}
	if ( lzx->output.data ) {
		copy = &lzx->output.data[lzx->output.offset];
		for ( i = 0 ; i < match_length ; i++ )
			copy[i] = copy[ i - match_offset ];
	}
	lzx->output.offset += match_length;

	return 0;
}

/**
 * Translate E8 jump addresses
 *
 * @v lzx		Decompressor
 */
static void lzx_translate_jumps ( struct lzx *lzx ) {
	size_t offset;
	int32_t *target;

	/* Sanity check */
	if ( lzx->output.offset < 10 )
		return;

	/* Scan for jump instructions */
	for ( offset = 0 ; offset < ( lzx->output.offset - 10 ) ; offset++ ) {

		/* Check for jump instruction */
		if ( lzx->output.data[offset] != 0xe8 )
			continue;

		/* Translate jump target */
		target = ( ( int32_t * ) &lzx->output.data[ offset + 1 ] );
		if ( *target >= 0 ) {
			if ( *target < LZX_WIM_MAGIC_FILESIZE )
				*target -= offset;
		} else {
			if ( *target >= -( ( int32_t ) offset ) )
				*target += LZX_WIM_MAGIC_FILESIZE;
		}
		offset += sizeof ( *target );
	}
}

/**
 * Decompress LZX-compressed data
 *
 * @v data		Compressed data
 * @v len		Length of compressed data
 * @v buf		Decompression buffer, or NULL
 * @ret out_len		Length of decompressed data, or negative error
 */
ssize_t lzx_decompress ( const void *data, size_t len, void *buf ) {
	struct lzx lzx;
	unsigned int i;
	int rc;

	/* Sanity check */
	if ( len % 2 ) {
		DBG ( "LZX cannot handle odd-length input data\n" );
		return -1;
	}

	/* Initialise global state, if required */
	if ( ! lzx_position_base[ LZX_POSITION_SLOTS - 1 ] ) {
		for ( i = 1 ; i < LZX_POSITION_SLOTS ; i++ ) {
			lzx_position_base[i] =
				( lzx_position_base[i-1] +
				  ( 1 << lzx_footer_bits ( i - 1 ) ) );
		}
	}

	/* Initialise decompressor */
	memset ( &lzx, 0, sizeof ( lzx ) );
	lzx.input.data = data;
	lzx.input.len = len;
	lzx.output.data = buf;
	for ( i = 0 ; i < LZX_REPEATED_OFFSETS ; i++ )
		lzx.repeated_offset[i] = 1;

	/* Process blocks */
	while ( lzx.input.offset < lzx.input.len ) {

		/* Process block header */
		if ( ( rc = lzx_block_header ( &lzx ) ) != 0 )
			return rc;

		/* Process block contents */
		if ( lzx.block_type == LZX_BLOCK_UNCOMPRESSED ) {

			/* Copy uncompressed data */
			if ( ( rc = lzx_uncompressed ( &lzx ) ) != 0 )
				return rc;

		} else {

			/* Process token stream */
			while ( lzx.output.offset < lzx.output.threshold ) {
				if ( ( rc = lzx_token ( &lzx ) ) != 0 )
					return rc;
			}
		}
	}

	/* Postprocess to undo E8 jump compression */
	if ( lzx.output.data )
		lzx_translate_jumps ( &lzx );

	return lzx.output.offset;
}
