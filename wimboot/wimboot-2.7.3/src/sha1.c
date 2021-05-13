/*
 * Copyright (C) 2012 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

/**
 * @file
 *
 * SHA-1 algorithm
 *
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <byteswap.h>
#include <assert.h>
#include "rotate.h"
#include "sha1.h"

/** SHA-1 variables */
struct sha1_variables {
	/* This layout matches that of struct sha1_digest_data,
	 * allowing for efficient endianness-conversion,
	 */
	uint32_t a;
	uint32_t b;
	uint32_t c;
	uint32_t d;
	uint32_t e;
	uint32_t w[80];
} __attribute__ (( packed ));

/**
 * f(a,b,c,d) for steps 0 to 19
 *
 * @v v		SHA-1 variables
 * @ret f	f(a,b,c,d)
 */
static uint32_t sha1_f_0_19 ( struct sha1_variables *v ) {
	return ( ( v->b & v->c ) | ( (~v->b) & v->d ) );
}

/**
 * f(a,b,c,d) for steps 20 to 39 and 60 to 79
 *
 * @v v		SHA-1 variables
 * @ret f	f(a,b,c,d)
 */
static uint32_t sha1_f_20_39_60_79 ( struct sha1_variables *v ) {
	return ( v->b ^ v->c ^ v->d );
}

/**
 * f(a,b,c,d) for steps 40 to 59
 *
 * @v v		SHA-1 variables
 * @ret f	f(a,b,c,d)
 */
static uint32_t sha1_f_40_59 ( struct sha1_variables *v ) {
	return ( ( v->b & v->c ) | ( v->b & v->d ) | ( v->c & v->d ) );
}

/** An SHA-1 step function */
struct sha1_step {
	/**
	 * Calculate f(a,b,c,d)
	 *
	 * @v v		SHA-1 variables
	 * @ret f	f(a,b,c,d)
	 */
	uint32_t ( * f ) ( struct sha1_variables *v );
	/** Constant k */
	uint32_t k;
};

/** SHA-1 steps */
static struct sha1_step sha1_steps[4] = {
	/** 0 to 19 */
	{ .f = sha1_f_0_19,		.k = 0x5a827999 },
	/** 20 to 39 */
	{ .f = sha1_f_20_39_60_79,	.k = 0x6ed9eba1 },
	/** 40 to 59 */
	{ .f = sha1_f_40_59,		.k = 0x8f1bbcdc },
	/** 60 to 79 */
	{ .f = sha1_f_20_39_60_79,	.k = 0xca62c1d6 },
};

/**
 * Initialise SHA-1 algorithm
 *
 * @v ctx		SHA-1 context
 */
void sha1_init ( void *ctx ) {
	struct sha1_context *context = ctx;

	context->ddd.dd.digest.h[0] = cpu_to_be32 ( 0x67452301 );
	context->ddd.dd.digest.h[1] = cpu_to_be32 ( 0xefcdab89 );
	context->ddd.dd.digest.h[2] = cpu_to_be32 ( 0x98badcfe );
	context->ddd.dd.digest.h[3] = cpu_to_be32 ( 0x10325476 );
	context->ddd.dd.digest.h[4] = cpu_to_be32 ( 0xc3d2e1f0 );
	context->len = 0;
}

/**
 * Calculate SHA-1 digest of accumulated data
 *
 * @v context		SHA-1 context
 */
static void sha1_digest ( struct sha1_context *context ) {
        union {
		union sha1_digest_data_dwords ddd;
		struct sha1_variables v;
	} u;
	uint32_t *a = &u.v.a;
	uint32_t *b = &u.v.b;
	uint32_t *c = &u.v.c;
	uint32_t *d = &u.v.d;
	uint32_t *e = &u.v.e;
	uint32_t *w = u.v.w;
	uint32_t f;
	uint32_t k;
	uint32_t temp;
	struct sha1_step *step;
	unsigned int i;

	/* Convert h[0..4] to host-endian, and initialise a, b, c, d,
	 * e, and w[0..15]
	 */
	for ( i = 0 ; i < ( sizeof ( u.ddd.dword ) /
			    sizeof ( u.ddd.dword[0] ) ) ; i++ ) {
		be32_to_cpus ( &context->ddd.dword[i] );
		u.ddd.dword[i] = context->ddd.dword[i];
	}

	/* Initialise w[16..79] */
	for ( i = 16 ; i < 80 ; i++ )
		w[i] = rol32 ( ( w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16] ), 1 );

	/* Main loop */
	for ( i = 0 ; i < 80 ; i++ ) {
		step = &sha1_steps[ i / 20 ];
		f = step->f ( &u.v );
		k = step->k;
		temp = ( rol32 ( *a, 5 ) + f + *e + k + w[i] );
		*e = *d;
		*d = *c;
		*c = rol32 ( *b, 30 );
		*b = *a;
		*a = temp;
	}

	/* Add chunk to hash and convert back to big-endian */
	for ( i = 0 ; i < 5 ; i++ ) {
		context->ddd.dd.digest.h[i] =
			cpu_to_be32 ( context->ddd.dd.digest.h[i] +
				      u.ddd.dd.digest.h[i] );
	}
}

/**
 * Accumulate data with SHA-1 algorithm
 *
 * @v ctx		SHA-1 context
 * @v data		Data
 * @v len		Length of data
 */
void sha1_update ( void *ctx, const void *data, size_t len ) {
	struct sha1_context *context = ctx;
	const uint8_t *byte = data;
	size_t offset;

	/* Accumulate data a byte at a time, performing the digest
	 * whenever we fill the data buffer
	 */
	while ( len-- ) {
		offset = ( context->len % sizeof ( context->ddd.dd.data ) );
		context->ddd.dd.data.byte[offset] = *(byte++);
		context->len++;
		if ( ( context->len % sizeof ( context->ddd.dd.data ) ) == 0 )
			sha1_digest ( context );
	}
}

/**
 * Generate SHA-1 digest
 *
 * @v ctx		SHA-1 context
 * @v out		Output buffer
 */
void sha1_final ( void *ctx, void *out ) {
	struct sha1_context *context = ctx;
	uint64_t len_bits;
	uint8_t pad;

	/* Record length before pre-processing */
	len_bits = cpu_to_be64 ( ( ( uint64_t ) context->len ) * 8 );

	/* Pad with a single "1" bit followed by as many "0" bits as required */
	pad = 0x80;
	do {
		sha1_update ( ctx, &pad, sizeof ( pad ) );
		pad = 0x00;
	} while ( ( context->len % sizeof ( context->ddd.dd.data ) ) !=
		  offsetof ( typeof ( context->ddd.dd.data ), final.len ) );

	/* Append length (in bits) */
	sha1_update ( ctx, &len_bits, sizeof ( len_bits ) );
	assert ( ( context->len % sizeof ( context->ddd.dd.data ) ) == 0 );

	/* Copy out final digest */
	memcpy ( out, &context->ddd.dd.digest,
		 sizeof ( context->ddd.dd.digest ) );
}
