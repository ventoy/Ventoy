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
 * String functions
 *
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ctype.h"
#include "wctype.h"

#if defined(__i386__) || defined(__x86_64__)

/**
 * Copy memory area
 *
 * @v dest		Destination address
 * @v src		Source address
 * @v len		Length
 * @ret dest		Destination address
 */
void * memcpy ( void *dest, const void *src, size_t len ) {
	void *edi = dest;
	const void *esi = src;
	int discard_ecx;

	/* Perform dword-based copy for bulk, then byte-based for remainder */
	__asm__ __volatile__ ( "rep movsl"
			       : "=&D" ( edi ), "=&S" ( esi ),
				 "=&c" ( discard_ecx )
			       : "0" ( edi ), "1" ( esi ), "2" ( len >> 2 )
			       : "memory" );
	__asm__ __volatile__ ( "rep movsb"
			       : "=&D" ( edi ), "=&S" ( esi ),
				 "=&c" ( discard_ecx )
			       : "0" ( edi ), "1" ( esi ), "2" ( len & 3 )
			       : "memory" );
	return dest;
}

/**
 * Copy memory area backwards
 *
 * @v dest		Destination address
 * @v src		Source address
 * @v len		Length
 * @ret dest		Destination address
 */
static void * memcpy_reverse ( void *dest, const void *src, size_t len ) {
	void *edi = ( dest + len - 1 );
	const void *esi = ( src + len - 1 );
	int discard_ecx;

	/* Assume memmove() is not performance-critical, and perform a
	 * bytewise copy for simplicity.
	 *
	 * Disable interrupts to avoid known problems on platforms
	 * that assume the direction flag always remains cleared.
	 */
	__asm__ __volatile__ ( "pushf\n\t"
			       "cli\n\t"
			       "std\n\t"
			       "rep movsb\n\t"
			       "popf\n\t"
			       : "=&D" ( edi ), "=&S" ( esi ),
				 "=&c" ( discard_ecx )
			       : "0" ( edi ), "1" ( esi ),
				 "2" ( len )
			       : "memory" );
	return dest;
}

/**
 * Copy (possibly overlapping) memory area
 *
 * @v dest		Destination address
 * @v src		Source address
 * @v len		Length
 * @ret dest		Destination address
 */
void * memmove ( void *dest, const void *src, size_t len ) {

	if ( dest <= src ) {
		return memcpy ( dest, src, len );
	} else {
		return memcpy_reverse ( dest, src, len );
	}
}

#elif defined(__aarch64__)

/**
 * Copy memory area
 *
 * @v dest		Destination address
 * @v src		Source address
 * @v len		Length
 * @ret dest		Destination address
 */
void * memcpy ( void *dest, const void *src, size_t len ) {
	void *discard_dest;
	void *discard_end;
	const void *discard_src;
	size_t discard_offset;
	unsigned long discard_data;
	unsigned long discard_low;
	unsigned long discard_high;

	/* If length is too short for an "ldp"/"stp" instruction pair,
	 * then just copy individual bytes.
	 */
	if ( len < 16 ) {
		__asm__ __volatile__ ( "cbz %0, 2f\n\t"
				       "\n1:\n\t"
				       "sub %0, %0, #1\n\t"
				       "ldrb %w1, [%3, %0]\n\t"
				       "strb %w1, [%2, %0]\n\t"
				       "cbnz %0, 1b\n\t"
				       "\n2:\n\t"
				       : "=&r" ( discard_offset ),
					 "=&r" ( discard_data )
				       : "r" ( dest ), "r" ( src ), "0" ( len )
				       : "memory" );
		return dest;
	}

	/* Use "ldp"/"stp" to copy 16 bytes at a time: one initial
	 * potentially unaligned access, multiple destination-aligned
	 * accesses, one final potentially unaligned access.
	 */
	__asm__ __volatile__ ( "ldp %3, %4, [%1], #16\n\t"
			       "stp %3, %4, [%0], #16\n\t"
			       "and %3, %0, #15\n\t"
			       "sub %0, %0, %3\n\t"
			       "sub %1, %1, %3\n\t"
			       "bic %2, %5, #15\n\t"
			       "b 2f\n\t"
			       "\n1:\n\t"
			       "ldp %3, %4, [%1], #16\n\t"
			       "stp %3, %4, [%0], #16\n\t"
			       "\n2:\n\t"
			       "cmp %0, %2\n\t"
			       "bne 1b\n\t"
			       "ldp %3, %4, [%6, #-16]\n\t"
			       "stp %3, %4, [%5, #-16]\n\t"
			       : "=&r" ( discard_dest ),
				 "=&r" ( discard_src ),
				 "=&r" ( discard_end ),
				 "=&r" ( discard_low ),
				 "=&r" ( discard_high )
			       : "r" ( dest + len ), "r" ( src + len ),
				 "0" ( dest ), "1" ( src )
			       : "memory", "cc" );

	return dest;
}

#endif

/**
 * Set memory area
 *
 * @v dest		Destination address
 * @v src		Source address
 * @v len		Length
 * @ret dest		Destination address
 */
void * memset ( void *dest, int c, size_t len ) {
	uint8_t *bytes = dest;

	while ( len-- )
		*(bytes++) = c;
	return dest;
}

/**
 * Compare memory areas
 *
 * @v src1		First source area
 * @v src2		Second source area
 * @v len		Length
 * @ret diff		Difference
 */
int memcmp ( const void *src1, const void *src2, size_t len ) {
	const uint8_t *bytes1 = src1;
	const uint8_t *bytes2 = src2;
	int diff;

	while ( len-- ) {
		if ( ( diff = ( *(bytes1++) - *(bytes2++) ) ) )
			return diff;
	}
	return 0;
}

/**
 * Compare two strings
 *
 * @v str1		First string
 * @v str2		Second string
 * @ret diff		Difference
 */
int strcmp ( const char *str1, const char *str2 ) {
	int c1;
	int c2;

	do {
		c1 = *(str1++);
		c2 = *(str2++);
	} while ( ( c1 != '\0' ) && ( c1 == c2 ) );

	return ( c1 - c2 );
}

/**
 * Compare two strings, case-insensitively
 *
 * @v str1		First string
 * @v str2		Second string
 * @ret diff		Difference
 */
int strcasecmp ( const char *str1, const char *str2 ) {
	int c1;
	int c2;

	do {
		c1 = toupper ( *(str1++) );
		c2 = toupper ( *(str2++) );
	} while ( ( c1 != '\0' ) && ( c1 == c2 ) );

	return ( c1 - c2 );
}

/**
 * Compare two wide-character strings, case-insensitively
 *
 * @v str1		First string
 * @v str2		Second string
 * @ret diff		Difference
 */
int wcscasecmp ( const wchar_t *str1, const wchar_t *str2 ) {
	int c1;
	int c2;

	do {
		c1 = towupper ( *(str1++) );
		c2 = towupper ( *(str2++) );
	} while ( ( c1 != L'\0' ) && ( c1 == c2 ) );

	return ( c1 - c2 );
}

/**
 * Get length of string
 *
 * @v str		String
 * @ret len		Length
 */
size_t strlen ( const char *str ) {
	size_t len = 0;

	while ( *(str++) )
		len++;
	return len;
}

/**
 * Get length of wide-character string
 *
 * @v str		String
 * @ret len		Length (in characters)
 */
size_t wcslen ( const wchar_t *str ) {
	size_t len = 0;

	while ( *(str++) )
		len++;
	return len;
}

/**
 * Find character in wide-character string
 *
 * @v str		String
 * @v c			Wide character
 * @ret first		First occurrence of wide character in string, or NULL
 */
wchar_t * wcschr ( const wchar_t *str, wchar_t c ) {

	for ( ; *str ; str++ ) {
		if ( *str == c )
			return ( ( wchar_t * )str );
	}
	return NULL;
}

/**
 * Check to see if character is a space
 *
 * @v c                 Character
 * @ret isspace         Character is a space
 */
int isspace ( int c ) {
        switch ( c ) {
        case ' ' :
        case '\f' :
        case '\n' :
        case '\r' :
        case '\t' :
        case '\v' :
                return 1;
        default:
                return 0;
        }
}

/**
 * Convert a string to an unsigned integer
 *
 * @v nptr		String
 * @v endptr		End pointer to fill in (or NULL)
 * @v base		Numeric base
 * @ret val		Value
 */
unsigned long strtoul ( const char *nptr, char **endptr, int base ) {
	unsigned long val = 0;
	int negate = 0;
	unsigned int digit;

	/* Skip any leading whitespace */
	while ( isspace ( *nptr ) )
		nptr++;

	/* Parse sign, if present */
	if ( *nptr == '+' ) {
		nptr++;
	} else if ( *nptr == '-' ) {
		nptr++;
		negate = 1;
	}

	/* Parse base */
	if ( base == 0 ) {

		/* Default to decimal */
		base = 10;

		/* Check for octal or hexadecimal markers */
		if ( *nptr == '0' ) {
			nptr++;
			base = 8;
			if ( ( *nptr | 0x20 ) == 'x' ) {
				nptr++;
				base = 16;
			}
		}
	}

	/* Parse digits */
	for ( ; ; nptr++ ) {
		digit = *nptr;
		if ( digit >= 'a' ) {
			digit = ( digit - 'a' + 10 );
		} else if ( digit >= 'A' ) {
			digit = ( digit - 'A' + 10 );
		} else if ( digit <= '9' ) {
			digit = ( digit - '0' );
		}
		if ( digit >= ( unsigned int ) base )
			break;
		val = ( ( val * base ) + digit );
	}

	/* Record end marker, if applicable */
	if ( endptr )
		*endptr = ( ( char * ) nptr );

	/* Return value */
	return ( negate ? -val : val );
}
