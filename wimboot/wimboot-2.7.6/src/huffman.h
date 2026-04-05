#ifndef _HUFFMAN_H
#define _HUFFMAN_H

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
 * Huffman alphabets
 *
 */

#include <stdint.h>

/** Maximum length of a Huffman symbol (in bits) */
#define HUFFMAN_BITS 16

/** Raw huffman symbol */
typedef uint16_t huffman_raw_symbol_t;

/** Quick lookup length for a Huffman symbol (in bits)
 *
 * This is a policy decision.
 */
#define HUFFMAN_QL_BITS 7

/** Quick lookup shift */
#define HUFFMAN_QL_SHIFT ( HUFFMAN_BITS - HUFFMAN_QL_BITS )

/** A Huffman-coded set of symbols of a given length */
struct huffman_symbols {
	/** Length of Huffman-coded symbols (in bits) */
	uint8_t bits;
	/** Shift to normalise symbols of this length to HUFFMAN_BITS bits */
	uint8_t shift;
	/** Number of Huffman-coded symbols having this length */
	uint16_t freq;
	/** First symbol of this length (normalised to HUFFMAN_BITS bits)
	 *
	 * Stored as a 32-bit value to allow the value
	 * (1<<HUFFMAN_BITS ) to be used for empty sets of symbols
	 * longer than the maximum utilised length.
	 */
	uint32_t start;
	/** Raw symbols having this length */
	huffman_raw_symbol_t *raw;
};

/** A Huffman-coded alphabet */
struct huffman_alphabet {
	/** Huffman-coded symbol set for each length */
	struct huffman_symbols huf[HUFFMAN_BITS];
	/** Quick lookup table */
	uint8_t lookup[ 1 << HUFFMAN_QL_BITS ];
	/** Raw symbols
	 *
	 * Ordered by Huffman-coded symbol length, then by symbol
	 * value.  This field has a variable length.
	 */
	huffman_raw_symbol_t raw[0];
};

/**
 * Get Huffman symbol length
 *
 * @v sym		Huffman symbol set
 * @ret len		Length (in bits)
 */
static inline __attribute__ (( always_inline )) unsigned int
huffman_len ( struct huffman_symbols *sym ) {

	return sym->bits;
}

/**
 * Get Huffman symbol value
 *
 * @v sym		Huffman symbol set
 * @v huf		Raw input value (normalised to HUFFMAN_BITS bits)
 * @ret raw		Raw symbol value
 */
static inline __attribute__ (( always_inline )) huffman_raw_symbol_t
huffman_raw ( struct huffman_symbols *sym, unsigned int huf ) {

	return sym->raw[ huf >> sym->shift ];
}

extern int huffman_alphabet ( struct huffman_alphabet *alphabet,
			      uint8_t *lengths, unsigned int count );
extern struct huffman_symbols *
huffman_sym ( struct huffman_alphabet *alphabet, unsigned int huf );

#endif /* _HUFFMAN_H */
