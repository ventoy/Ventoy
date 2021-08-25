#ifndef _STRING_H
#define _STRING_H

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
 * String operations
 *
 */

#include <stdint.h>

extern void * memcpy ( void *dest, const void *src, size_t len );
extern void * memmove ( void *dest, const void *src, size_t len );
extern void * memset ( void *dest, int c, size_t len );
extern int memcmp ( const void *src1, const void *src2, size_t len );
extern int strcmp ( const char *str1, const char *str2 );
extern size_t strlen ( const char *str );

#endif /* _STRING_H */
