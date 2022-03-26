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

/**
 * Set memory area
 *
 * @v dest		Destination address
 * @v src		Source address
 * @v len		Length
 * @ret dest		Destination address
 */
void * memset ( void *dest, int c, size_t len ) {
	void *edi = dest;
	int eax = c;
	int discard_ecx;

	/* Expand byte to whole dword */
	eax |= ( eax << 8 );
	eax |= ( eax << 16 );

	/* Perform dword-based set for bulk, then byte-based for remainder */
	__asm__ __volatile__ ( "rep stosl"
			       : "=&D" ( edi ), "=&a" ( eax ),
				 "=&c" ( discard_ecx )
			       : "0" ( edi ), "1" ( eax ), "2" ( len >> 2 )
			       : "memory" );
	__asm__ __volatile__ ( "rep stosb"
			       : "=&D" ( edi ), "=&a" ( eax ),
				 "=&c" ( discard_ecx )
			       : "0" ( edi ), "1" ( eax ), "2" ( len & 3 )
			       : "memory" );
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

char *strchr(const char *str, char c) {
    for ( ; *str ; str++ ) {
		if ( *str == c )
			return ( ( char * )str );
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
