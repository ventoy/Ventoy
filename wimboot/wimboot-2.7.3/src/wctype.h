#ifndef _WCTYPE_H
#define _WCTYPE_H

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
 * Wide character types
 *
 * We don't actually care about wide characters.  Internationalisation
 * is a user interface concern, and has absolutely no place in the
 * boot process.  However, UEFI uses wide characters and so we have to
 * at least be able to handle the ASCII subset of UCS-2.
 *
 */

#include <ctype.h>

static inline int iswlower ( wint_t c ) {
	return islower ( c );
}

static inline int iswupper ( wint_t c ) {
	return isupper ( c );
}

static inline int towupper ( wint_t c ) {
	return toupper ( c );
}

static inline int iswspace ( wint_t c ) {
	return isspace ( c );
}

#endif /* _WCTYPE_H */
