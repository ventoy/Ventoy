#ifndef _BITS_BYTESWAP_H
#define _BITS_BYTESWAP_H

/** @file
 *
 * Byte-order swapping functions
 *
 */

#include <stdint.h>

static inline __attribute__ (( always_inline, const )) uint16_t
__bswap_variable_16 ( uint16_t x ) {
	__asm__ ( "xchgb %b0,%h0" : "=Q" ( x ) : "0" ( x ) );
	return x;
}

static inline __attribute__ (( always_inline )) void
__bswap_16s ( uint16_t *x ) {
	__asm__ ( "rorw $8, %0" : "+m" ( *x ) );
}

static inline __attribute__ (( always_inline, const )) uint32_t
__bswap_variable_32 ( uint32_t x ) {
	__asm__ ( "bswapl %k0" : "=r" ( x ) : "0" ( x ) );
	return x;
}

static inline __attribute__ (( always_inline )) void
__bswap_32s ( uint32_t *x ) {
	__asm__ ( "bswapl %k0" : "=r" ( *x ) : "0" ( *x ) );
}

#ifdef __x86_64__

static inline __attribute__ (( always_inline, const )) uint64_t
__bswap_variable_64 ( uint64_t x ) {
	__asm__ ( "bswapq %q0" : "=r" ( x ) : "0" ( x ) );
	return x;
}

#else /* __x86_64__ */

static inline __attribute__ (( always_inline, const )) uint64_t
__bswap_variable_64 ( uint64_t x ) {
	uint32_t in_high = ( x >> 32 );
	uint32_t in_low = ( x & 0xffffffffUL );
	uint32_t out_high;
	uint32_t out_low;

	__asm__ ( "bswapl %0\n\t"
		  "bswapl %1\n\t"
		  "xchgl %0,%1\n\t"
		  : "=r" ( out_high ), "=r" ( out_low )
		  : "0" ( in_high ), "1" ( in_low ) );

	return ( ( ( ( uint64_t ) out_high ) << 32 ) |
		 ( ( uint64_t ) out_low ) );
}

#endif /* __x86_64__ */

static inline __attribute__ (( always_inline )) void
__bswap_64s ( uint64_t *x ) {
	*x = __bswap_variable_64 ( *x );
}

/**
 * Byte-swap a 16-bit constant
 *
 * @v value		Constant value
 * @ret swapped		Byte-swapped value
 */
#define __bswap_constant_16( value )					\
	( ( ( (value) & 0x00ff ) << 8 ) |				\
	  ( ( (value) & 0xff00 ) >> 8 ) )

/**
 * Byte-swap a 32-bit constant
 *
 * @v value		Constant value
 * @ret swapped		Byte-swapped value
 */
#define __bswap_constant_32( value ) \
	( ( ( (value) & 0x000000ffUL ) << 24 ) |			\
	  ( ( (value) & 0x0000ff00UL ) <<  8 ) |			\
	  ( ( (value) & 0x00ff0000UL ) >>  8 ) |			\
	  ( ( (value) & 0xff000000UL ) >> 24 ) )

/**
 * Byte-swap a 64-bit constant
 *
 * @v value		Constant value
 * @ret swapped		Byte-swapped value
 */
#define __bswap_constant_64( value )					\
	( ( ( (value) & 0x00000000000000ffULL ) << 56 ) |		\
	  ( ( (value) & 0x000000000000ff00ULL ) << 40 ) |		\
	  ( ( (value) & 0x0000000000ff0000ULL ) << 24 ) |		\
	  ( ( (value) & 0x00000000ff000000ULL ) <<  8 ) |		\
	  ( ( (value) & 0x000000ff00000000ULL ) >>  8 ) |		\
	  ( ( (value) & 0x0000ff0000000000ULL ) >> 24 ) |		\
	  ( ( (value) & 0x00ff000000000000ULL ) >> 40 ) |		\
	  ( ( (value) & 0xff00000000000000ULL ) >> 56 ) )

/**
 * Byte-swap a 16-bit value
 *
 * @v value		Value
 * @ret swapped		Byte-swapped value
 */
#define __bswap_16( value )						\
	( __builtin_constant_p (value) ?				\
	  ( ( uint16_t ) __bswap_constant_16 ( ( uint16_t ) (value) ) ) \
	  : __bswap_variable_16 (value) )
#define bswap_16( value ) __bswap_16 (value)

/**
 * Byte-swap a 32-bit value
 *
 * @v value		Value
 * @ret swapped		Byte-swapped value
 */
#define __bswap_32( value )						\
	( __builtin_constant_p (value) ?				\
	  ( ( uint32_t ) __bswap_constant_32 ( ( uint32_t ) (value) ) ) \
	  : __bswap_variable_32 (value) )
#define bswap_32( value ) __bswap_32 (value)

/**
 * Byte-swap a 64-bit value
 *
 * @v value		Value
 * @ret swapped		Byte-swapped value
 */
#define __bswap_64( value )						\
	( __builtin_constant_p (value) ?				\
	  ( ( uint64_t ) __bswap_constant_64 ( ( uint64_t ) (value) ) ) \
          : __bswap_variable_64 (value) )
#define bswap_64( value ) __bswap_64 (value)

#define __cpu_to_leNN( bits, value ) (value)
#define __cpu_to_beNN( bits, value ) __bswap_ ## bits (value)
#define __leNN_to_cpu( bits, value ) (value)
#define __beNN_to_cpu( bits, value ) __bswap_ ## bits (value)
#define __cpu_to_leNNs( bits, ptr ) do { } while ( 0 )
#define __cpu_to_beNNs( bits, ptr ) __bswap_ ## bits ## s (ptr)
#define __leNN_to_cpus( bits, ptr ) do { } while ( 0 )
#define __beNN_to_cpus( bits, ptr ) __bswap_ ## bits ## s (ptr)

#define cpu_to_le16( value ) __cpu_to_leNN ( 16, value )
#define cpu_to_le32( value ) __cpu_to_leNN ( 32, value )
#define cpu_to_le64( value ) __cpu_to_leNN ( 64, value )
#define cpu_to_be16( value ) __cpu_to_beNN ( 16, value )
#define cpu_to_be32( value ) __cpu_to_beNN ( 32, value )
#define cpu_to_be64( value ) __cpu_to_beNN ( 64, value )
#define le16_to_cpu( value ) __leNN_to_cpu ( 16, value )
#define le32_to_cpu( value ) __leNN_to_cpu ( 32, value )
#define le64_to_cpu( value ) __leNN_to_cpu ( 64, value )
#define be16_to_cpu( value ) __beNN_to_cpu ( 16, value )
#define be32_to_cpu( value ) __beNN_to_cpu ( 32, value )
#define be64_to_cpu( value ) __beNN_to_cpu ( 64, value )
#define cpu_to_le16s( ptr ) __cpu_to_leNNs ( 16, ptr )
#define cpu_to_le32s( ptr ) __cpu_to_leNNs ( 32, ptr )
#define cpu_to_le64s( ptr ) __cpu_to_leNNs ( 64, ptr )
#define cpu_to_be16s( ptr ) __cpu_to_beNNs ( 16, ptr )
#define cpu_to_be32s( ptr ) __cpu_to_beNNs ( 32, ptr )
#define cpu_to_be64s( ptr ) __cpu_to_beNNs ( 64, ptr )
#define le16_to_cpus( ptr ) __leNN_to_cpus ( 16, ptr )
#define le32_to_cpus( ptr ) __leNN_to_cpus ( 32, ptr )
#define le64_to_cpus( ptr ) __leNN_to_cpus ( 64, ptr )
#define be16_to_cpus( ptr ) __beNN_to_cpus ( 16, ptr )
#define be32_to_cpus( ptr ) __beNN_to_cpus ( 32, ptr )
#define be64_to_cpus( ptr ) __beNN_to_cpus ( 64, ptr )

#define htonll( value ) cpu_to_be64 (value)
#define ntohll( value ) be64_to_cpu (value)
#define htonl( value ) cpu_to_be32 (value)
#define ntohl( value ) be32_to_cpu (value)
#define htons( value ) cpu_to_be16 (value)
#define ntohs( value ) be16_to_cpu (value)

#endif /* _BITS_BYTESWAP_H */
