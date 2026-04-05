/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
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
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <wchar.h>
#include <ipxe/vsprintf.h>

/** @file */

#define CHAR_LEN	0	/**< "hh" length modifier */
#define SHORT_LEN	1	/**< "h" length modifier */
#define INT_LEN		2	/**< no length modifier */
#define LONG_LEN	3	/**< "l" length modifier */
#define LONGLONG_LEN	4	/**< "ll" length modifier */
#define SIZE_T_LEN	5	/**< "z" length modifier */

static uint8_t type_sizes[] = {
	[CHAR_LEN]	= sizeof ( char ),
	[SHORT_LEN]	= sizeof ( short ),
	[INT_LEN]	= sizeof ( int ),
	[LONG_LEN]	= sizeof ( long ),
	[LONGLONG_LEN]	= sizeof ( long long ),
	[SIZE_T_LEN]	= sizeof ( size_t ),
};

/**
 * Use lower-case for hexadecimal digits
 *
 * Note that this value is set to 0x20 since that makes for very
 * efficient calculations.  (Bitwise-ORing with @c LCASE converts to a
 * lower-case character, for example.)
 */
#define LCASE 0x20

/**
 * Use "alternate form"
 *
 * For hexadecimal numbers, this means to add a "0x" or "0X" prefix to
 * the number.
 */
#define ALT_FORM 0x02

/**
 * Use zero padding
 *
 * Note that this value is set to 0x10 since that allows the pad
 * character to be calculated as @c 0x20|(flags&ZPAD)
 */
#define ZPAD 0x10

/**
 * Format a hexadecimal number
 *
 * @v end		End of buffer to contain number
 * @v num		Number to format
 * @v width		Minimum field width
 * @v flags		Format flags
 * @ret ptr		End of buffer
 *
 * Fills a buffer in reverse order with a formatted hexadecimal
 * number.  The number will be zero-padded to the specified width.
 * Lower-case and "alternate form" (i.e. "0x" prefix) flags may be
 * set.
 *
 * There must be enough space in the buffer to contain the largest
 * number that this function can format.
 */
static char * format_hex ( char *end, unsigned long long num, int width,
			   int flags ) {
	char *ptr = end;
	int case_mod = ( flags & LCASE );
	int pad = ( ( flags & ZPAD ) | ' ' );

	/* Generate the number */
	do {
		*(--ptr) = "0123456789ABCDEF"[ num & 0xf ] | case_mod;
		num >>= 4;
	} while ( num );

	/* Pad to width */
	while ( ( end - ptr ) < width )
		*(--ptr) = pad;

	/* Add "0x" or "0X" if alternate form specified */
	if ( flags & ALT_FORM ) {
		*(--ptr) = 'X' | case_mod;
		*(--ptr) = '0';
	}

	return ptr;
}

/**
 * Format a decimal number
 *
 * @v end		End of buffer to contain number
 * @v num		Number to format
 * @v width		Minimum field width
 * @v flags		Format flags
 * @ret ptr		End of buffer
 *
 * Fills a buffer in reverse order with a formatted decimal number.
 * The number will be space-padded to the specified width.
 *
 * There must be enough space in the buffer to contain the largest
 * number that this function can format.
 */
static char * format_decimal ( char *end, signed long num, int width,
			       int flags ) {
	char *ptr = end;
	int negative = 0;
	int zpad = ( flags & ZPAD );
	int pad = ( zpad | ' ' );

	/* Generate the number */
	if ( num < 0 ) {
		negative = 1;
		num = -num;
	}
	do {
		*(--ptr) = '0' + ( num % 10 );
		num /= 10;
	} while ( num );

	/* Add "-" if necessary */
	if ( negative && ( ! zpad ) )
		*(--ptr) = '-';

	/* Pad to width */
	while ( ( end - ptr ) < width )
		*(--ptr) = pad;

	/* Add "-" if necessary */
	if ( negative && zpad )
		*ptr = '-';

	return ptr;
}

/**
 * Print character via a printf context
 *
 * @v ctx		Context
 * @v c			Character
 *
 * Call's the printf_context::handler() method and increments
 * printf_context::len.
 */
static inline void cputchar ( struct printf_context *ctx, unsigned int c ) {
	ctx->handler ( ctx, c );
	++ctx->len;
}

/**
 * Write a formatted string to a printf context
 *
 * @v ctx		Context
 * @v fmt		Format string
 * @v args		Arguments corresponding to the format string
 * @ret len		Length of formatted string
 */
size_t vcprintf ( struct printf_context *ctx, const char *fmt, va_list args ) {
	int flags;
	int width;
	uint8_t *length;
	char *ptr;
	char tmp_buf[32]; /* 32 is enough for all numerical formats.
			   * Insane width fields could overflow this buffer. */
	wchar_t *wptr;

	/* Initialise context */
	ctx->len = 0;

	for ( ; *fmt ; fmt++ ) {
		/* Pass through ordinary characters */
		if ( *fmt != '%' ) {
			cputchar ( ctx, *fmt );
			continue;
		}
		fmt++;
		/* Process flag characters */
		flags = 0;
		for ( ; ; fmt++ ) {
			if ( *fmt == '#' ) {
				flags |= ALT_FORM;
			} else if ( *fmt == '0' ) {
				flags |= ZPAD;
			} else {
				/* End of flag characters */
				break;
			}
		}
		/* Process field width */
		width = 0;
		for ( ; ; fmt++ ) {
			if ( ( ( unsigned ) ( *fmt - '0' ) ) < 10 ) {
				width = ( width * 10 ) + ( *fmt - '0' );
			} else {
				break;
			}
		}
		/* We don't do floating point */
		/* Process length modifier */
		length = &type_sizes[INT_LEN];
		for ( ; ; fmt++ ) {
			if ( *fmt == 'h' ) {
				length--;
			} else if ( *fmt == 'l' ) {
				length++;
			} else if ( *fmt == 'z' ) {
				length = &type_sizes[SIZE_T_LEN];
			} else {
				break;
			}
		}
		/* Process conversion specifier */
		ptr = tmp_buf + sizeof ( tmp_buf ) - 1;
		*ptr = '\0';
		wptr = NULL;
		if ( *fmt == 'c' ) {
			if ( length < &type_sizes[LONG_LEN] ) {
				cputchar ( ctx, va_arg ( args, unsigned int ) );
			} else {
				wchar_t wc;
				size_t len;

				wc = va_arg ( args, wint_t );
				len = wcrtomb ( tmp_buf, wc, NULL );
				tmp_buf[len] = '\0';
				ptr = tmp_buf;
			}
		} else if ( *fmt == 's' ) {
			if ( length < &type_sizes[LONG_LEN] ) {
				ptr = va_arg ( args, char * );
			} else {
				wptr = va_arg ( args, wchar_t * );
			}
			if ( ( ptr == NULL ) && ( wptr == NULL ) )
				ptr = "<NULL>";
		} else if ( *fmt == 'p' ) {
			intptr_t ptrval;

			ptrval = ( intptr_t ) va_arg ( args, void * );
			ptr = format_hex ( ptr, ptrval, width, 
					   ( ALT_FORM | LCASE ) );
		} else if ( ( *fmt & ~0x20 ) == 'X' ) {
			unsigned long long hex;

			flags |= ( *fmt & 0x20 ); /* LCASE */
			if ( *length >= sizeof ( unsigned long long ) ) {
				hex = va_arg ( args, unsigned long long );
			} else if ( *length >= sizeof ( unsigned long ) ) {
				hex = va_arg ( args, unsigned long );
			} else {
				hex = va_arg ( args, unsigned int );
			}
			ptr = format_hex ( ptr, hex, width, flags );
		} else if ( ( *fmt == 'd' ) || ( *fmt == 'i' ) ){
			signed long decimal;

			if ( *length >= sizeof ( signed long ) ) {
				decimal = va_arg ( args, signed long );
			} else {
				decimal = va_arg ( args, signed int );
			}
			ptr = format_decimal ( ptr, decimal, width, flags );
		} else {
			*(--ptr) = *fmt;
		}
		/* Write out conversion result */
		if ( wptr == NULL ) {
			for ( ; *ptr ; ptr++ ) {
				cputchar ( ctx, *ptr );
			}
		} else {
			for ( ; *wptr ; wptr++ ) {
				size_t len = wcrtomb ( tmp_buf, *wptr, NULL );
				for ( ptr = tmp_buf ; len-- ; ptr++ ) {
					cputchar ( ctx, *ptr );
				}
			}
		}
	}

	return ctx->len;
}

/** Context used by vsnprintf() and friends */
struct sputc_context {
	struct printf_context ctx;
	/** Buffer for formatted string (used by printf_sputc()) */
	char *buf;
	/** Buffer length (used by printf_sputc()) */
	size_t max_len;	
};

/**
 * Write character to buffer
 *
 * @v ctx		Context
 * @v c			Character
 */
static void printf_sputc ( struct printf_context *ctx, unsigned int c ) {
	struct sputc_context * sctx =
		container_of ( ctx, struct sputc_context, ctx );

	if ( ctx->len < sctx->max_len )
		sctx->buf[ctx->len] = c;
}

/**
 * Write a formatted string to a buffer
 *
 * @v buf		Buffer into which to write the string
 * @v size		Size of buffer
 * @v fmt		Format string
 * @v args		Arguments corresponding to the format string
 * @ret len		Length of formatted string
 *
 * If the buffer is too small to contain the string, the returned
 * length is the length that would have been written had enough space
 * been available.
 */
int vsnprintf ( char *buf, size_t size, const char *fmt, va_list args ) {
	struct sputc_context sctx;
	size_t len;
	size_t end;

	/* Hand off to vcprintf */
	sctx.ctx.handler = printf_sputc;
	sctx.buf = buf;
	sctx.max_len = size;
	len = vcprintf ( &sctx.ctx, fmt, args );

	/* Add trailing NUL */
	if ( size ) {
		end = size - 1;
		if ( len < end )
			end = len;
		buf[end] = '\0';
	}

	return len;
}

/**
 * Write a formatted string to a buffer
 *
 * @v buf		Buffer into which to write the string
 * @v size		Size of buffer
 * @v fmt		Format string
 * @v ...		Arguments corresponding to the format string
 * @ret len		Length of formatted string
 */
int snprintf ( char *buf, size_t size, const char *fmt, ... ) {
	va_list args;
	int i;

	va_start ( args, fmt );
	i = vsnprintf ( buf, size, fmt, args );
	va_end ( args );
	return i;
}

/**
 * Version of vsnprintf() that accepts a signed buffer size
 *
 * @v buf		Buffer into which to write the string
 * @v size		Size of buffer
 * @v fmt		Format string
 * @v args		Arguments corresponding to the format string
 * @ret len		Length of formatted string
 */
int vssnprintf ( char *buf, ssize_t ssize, const char *fmt, va_list args ) {

	/* Treat negative buffer size as zero buffer size */
	if ( ssize < 0 )
		ssize = 0;

	/* Hand off to vsnprintf */
	return vsnprintf ( buf, ssize, fmt, args );
}

/**
 * Version of vsnprintf() that accepts a signed buffer size
 *
 * @v buf		Buffer into which to write the string
 * @v size		Size of buffer
 * @v fmt		Format string
 * @v ...		Arguments corresponding to the format string
 * @ret len		Length of formatted string
 */
int ssnprintf ( char *buf, ssize_t ssize, const char *fmt, ... ) {
	va_list args;
	int len;

	/* Hand off to vssnprintf */
	va_start ( args, fmt );
	len = vssnprintf ( buf, ssize, fmt, args );
	va_end ( args );
	return len;
}

/**
 * Write character to console
 *
 * @v ctx		Context
 * @v c			Character
 */
static void printf_putchar ( struct printf_context *ctx __unused,
			     unsigned int c ) {
	putchar ( c );
}

/**
 * Write a formatted string to the console
 *
 * @v fmt		Format string
 * @v args		Arguments corresponding to the format string
 * @ret len		Length of formatted string
 */
int vprintf ( const char *fmt, va_list args ) {
	struct printf_context ctx;

	/* Hand off to vcprintf */
	ctx.handler = printf_putchar;	
	return vcprintf ( &ctx, fmt, args );	
}

/**
 * Write a formatted string to the console.
 *
 * @v fmt		Format string
 * @v ...		Arguments corresponding to the format string
 * @ret	len		Length of formatted string
 */
int printf ( const char *fmt, ... ) {
	va_list args;
	int i;

	va_start ( args, fmt );
	i = vprintf ( fmt, args );
	va_end ( args );
	return i;
}
