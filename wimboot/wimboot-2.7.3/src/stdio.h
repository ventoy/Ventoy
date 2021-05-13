#ifndef _STDIO_H
#define _STDIO_H

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
 * Standard Input/Output
 *
 */

#include <stdint.h>
#include <stdarg.h>

extern int putchar ( int character );
extern int getchar ( void );

extern int __attribute__ (( format ( printf, 1, 2 ) ))
printf ( const char *fmt, ... );

extern int __attribute__ (( format ( printf, 3, 4 ) ))
snprintf ( char *buf, size_t size, const char *fmt, ... );

extern int vprintf ( const char *fmt, va_list args );

extern int vsnprintf ( char *buf, size_t size, const char *fmt, va_list args );

#endif /* _STDIO_H */
