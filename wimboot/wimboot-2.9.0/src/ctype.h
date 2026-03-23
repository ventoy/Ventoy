#ifndef _CTYPE_H
#define _CTYPE_H

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
 * Character types
 *
 */

static inline int islower ( int c ) {
	return ( ( c >= 'a' ) && ( c <= 'z' ) );
}

static inline int isupper ( int c ) {
	return ( ( c >= 'A' ) && ( c <= 'Z' ) );
}

static inline int toupper ( int c ) {

	if ( islower ( c ) )
		c -= ( 'a' - 'A' );
	return c;
}

extern int isspace ( int c );

#endif /* _CTYPE_H */
