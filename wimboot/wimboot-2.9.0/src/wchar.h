#ifndef _WCHAR_H
#define _WCHAR_H

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
 * Wide characters
 *
 */

#include <stdint.h>

typedef void mbstate_t;

/**
 * Convert wide character to multibyte sequence
 *
 * @v buf		Buffer
 * @v wc		Wide character
 * @v ps		Shift state
 * @ret len		Number of characters written
 *
 * This is a stub implementation, sufficient to handle basic ASCII
 * characters.
 */
static inline size_t wcrtomb ( char *buf, wchar_t wc,
			       mbstate_t *ps __attribute__ (( unused )) ) {
	*buf = wc;
	return 1;
}

extern int wcscasecmp ( const wchar_t *str1, const wchar_t *str2 );
extern size_t wcslen ( const wchar_t *str );
extern wchar_t * wcschr ( const wchar_t *str, wchar_t c );

#endif /* _WCHAR_H */
