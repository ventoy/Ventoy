#ifndef _LZX_H
#define _LZX_H

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
 */

#include <stdint.h>
#include "huffman.h"

/** Number of aligned offset codes */
#define LZX_ALIGNOFFSET_CODES 8

/** Aligned offset code length (in bits) */
#define LZX_ALIGNOFFSET_BITS 3

/** Number of pretree codes */
#define LZX_PRETREE_CODES 20

/** Pretree code length (in bits) */
#define LZX_PRETREE_BITS 4

/** Number of literal main codes */
#define LZX_MAIN_LIT_CODES 256

/** Number of position slots */
#define LZX_POSITION_SLOTS 30

/** Number of main codes */
#define LZX_MAIN_CODES ( LZX_MAIN_LIT_CODES + ( 8 * LZX_POSITION_SLOTS ) )

/** Number of length codes */
#define LZX_LENGTH_CODES 249

/** Block type length (in bits) */
#define LZX_BLOCK_TYPE_BITS 3

/** Default block length */
#define LZX_DEFAULT_BLOCK_LEN 32768

/** Number of repeated offsets */
#define LZX_REPEATED_OFFSETS 3

/** Don't ask */
#define LZX_WIM_MAGIC_FILESIZE 12000000

/** Block types */
enum lzx_block_type {
	/** Verbatim block */
	LZX_BLOCK_VERBATIM = 1,
	/** Aligned offset block */
	LZX_BLOCK_ALIGNOFFSET = 2,
	/** Uncompressed block */
	LZX_BLOCK_UNCOMPRESSED = 3,
};

/** An LZX input stream */
struct lzx_input_stream {
	/** Data */
	const uint8_t *data;
	/** Length */
	size_t len;
	/** Offset within stream */
	size_t offset;
};

/** An LZX output stream */
struct lzx_output_stream {
	/** Data, or NULL */
	uint8_t *data;
	/** Offset within stream */
	size_t offset;
	/** End of current block within stream */
	size_t threshold;
};

/** LZX decompressor */
struct lzx {
	/** Input stream */
	struct lzx_input_stream input;
	/** Output stream */
	struct lzx_output_stream output;
	/** Accumulator */
	uint32_t accumulator;
	/** Number of bits in accumulator */
	unsigned int bits;
	/** Block type */
	enum lzx_block_type block_type;
	/** Repeated offsets */
	unsigned int repeated_offset[LZX_REPEATED_OFFSETS];

	/** Aligned offset Huffman alphabet */
	struct huffman_alphabet alignoffset;
	/** Aligned offset raw symbols
	 *
	 * Must immediately follow the aligned offset Huffman
	 * alphabet.
	 */
	huffman_raw_symbol_t alignoffset_raw[LZX_ALIGNOFFSET_CODES];
	/** Aligned offset code lengths */
	uint8_t alignoffset_lengths[LZX_ALIGNOFFSET_CODES];

	/** Pretree Huffman alphabet */
	struct huffman_alphabet pretree;
	/** Pretree raw symbols
	 *
	 * Must immediately follow the pretree Huffman alphabet.
	 */
	huffman_raw_symbol_t pretree_raw[LZX_PRETREE_CODES];
	/** Preetree code lengths */
	uint8_t pretree_lengths[LZX_PRETREE_CODES];

	/** Main Huffman alphabet */
	struct huffman_alphabet main;
	/** Main raw symbols
	 *
	 * Must immediately follow the main Huffman alphabet.
	 */
	huffman_raw_symbol_t main_raw[LZX_MAIN_CODES];
	/** Main code lengths */
	struct {
		/** Literals */
		uint8_t literals[LZX_MAIN_LIT_CODES];
		/** Remaining symbols */
		uint8_t remainder[ LZX_MAIN_CODES - LZX_MAIN_LIT_CODES ];
	} __attribute__ (( packed )) main_lengths;

	/** Length Huffman alphabet */
	struct huffman_alphabet length;
	/** Length raw symbols
	 *
	 * Must immediately follow the length Huffman alphabet.
	 */
	huffman_raw_symbol_t length_raw[LZX_LENGTH_CODES];
	/** Length code lengths */
	uint8_t length_lengths[LZX_LENGTH_CODES];
};

/**
 * Calculate number of footer bits for a given position slot
 *
 * @v position_slot	Position slot
 * @ret footer_bits 	Number of footer bits
 */
static inline unsigned int lzx_footer_bits ( unsigned int position_slot ) {

	if ( position_slot < 2 ) {
		return 0;
	} else if ( position_slot < 38 ) {
		return ( ( position_slot / 2 ) - 1 );
	} else {
		return 17;
	}
}

extern ssize_t lzx_decompress ( const void *data, size_t len, void *buf );

#endif /* _LZX_H */
