#ifndef _IPXE_VSPRINTF_H
#define _IPXE_VSPRINTF_H

/** @file
 *
 * printf() and friends
 *
 * Etherboot's printf() functions understand the following subset of
 * the standard C printf()'s format specifiers:
 *
 *	- Flag characters
 *		- '#'		- Alternate form (i.e. "0x" prefix)
 *		- '0'		- Zero-pad
 *	- Field widths
 *	- Length modifiers
 *		- 'hh'		- Signed / unsigned char
 *		- 'h'		- Signed / unsigned short
 *		- 'l'		- Signed / unsigned long
 *		- 'll'		- Signed / unsigned long long
 *		- 'z'		- Signed / unsigned size_t
 *	- Conversion specifiers
 *		- 'd'		- Signed decimal
 *		- 'x','X'	- Unsigned hexadecimal
 *		- 'c'		- Character
 *		- 's'		- String
 *		- 'p'		- Pointer
 *
 * Hexadecimal numbers are always zero-padded to the specified field
 * width (if any); decimal numbers are always space-padded.  Decimal
 * long longs are not supported.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

/**
 * A printf context
 *
 * Contexts are used in order to be able to share code between
 * vprintf() and vsnprintf(), without requiring the allocation of a
 * buffer for vprintf().
 */
struct printf_context {
	/**
	 * Character handler
	 *
	 * @v ctx	Context
	 * @v c		Character
	 *
	 * This method is called for each character written to the
	 * formatted string.
	 */
	void ( * handler ) ( struct printf_context *ctx, unsigned int c );
	/** Length of formatted string
	 *
	 * When handler() is called, @len will be set to the number of
	 * characters written so far (i.e. zero for the first call to
	 * handler()).
	 */
	size_t len;
};

extern size_t vcprintf ( struct printf_context *ctx, const char *fmt,
			 va_list args );
extern int vssnprintf ( char *buf, ssize_t ssize, const char *fmt,
			va_list args );
extern int __attribute__ (( format ( printf, 3, 4 ) ))
ssnprintf ( char *buf, ssize_t ssize, const char *fmt, ... );

#endif /* _IPXE_VSPRINTF_H */
