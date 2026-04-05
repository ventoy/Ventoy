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
 * CPIO archives
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wimboot.h"
#include "cpio.h"

/**
 * Align CPIO length to nearest dword
 *
 * @v len		Length
 * @ret len		Aligned length
 */
static size_t cpio_align ( size_t len ) {
	return ( ( len + 0x03 ) & ~0x03 );
}

/**
 * Parse CPIO field value
 *
 * @v field		ASCII field
 * @ret value		Field value
 */
static unsigned long cpio_value ( const char *field ) {
	char buf[9];

	memcpy ( buf, field, ( sizeof ( buf ) - 1 ) );
	buf[ sizeof ( buf ) - 1 ] = '\0';
	return strtoul ( buf, NULL, 16 );
}

/**
 * Extract files from CPIO archive
 *
 * @v data		CPIO archive
 * @v len		Maximum length of CPIO archive
 * @v file		File handler
 * @ret rc		Return status code
 */
int cpio_extract ( void *data, size_t len,
		   int ( * file ) ( const char *name, void *data,
				    size_t len ) ) {
	const struct cpio_header *cpio;
	const uint32_t *pad;
	const char *file_name;
	void *file_data;
	size_t file_name_len;
	size_t file_len;
	size_t cpio_len;
	int rc;

	while ( 1 ) {

		/* Skip over any padding */
		while ( len >= sizeof ( *pad ) ) {
			pad = data;
			if ( *pad )
				break;
			data += sizeof ( *pad );
			len -= sizeof ( *pad );
		}

		/* Stop if we have reached the end of the archive */
		if ( ! len )
			return 0;

		/* Sanity check */
		if ( len < sizeof ( *cpio ) ) {
			DBG ( "Truncated CPIO header\n" );
			return -1;
		}
		cpio = data;

		/* Check magic */
		if ( memcmp ( cpio->c_magic, CPIO_MAGIC,
			      sizeof ( cpio->c_magic ) ) != 0 ) {
			DBG ( "Bad CPIO magic\n" );
			return -1;
		}

		/* Extract file parameters */
		file_name = ( ( void * ) ( cpio + 1 ) );
		file_name_len = cpio_value ( cpio->c_namesize );
		file_data = ( data + cpio_align ( sizeof ( *cpio ) +
						  file_name_len ) );
		file_len = cpio_value ( cpio->c_filesize );
		cpio_len = ( file_data + file_len - data );
		if ( cpio_len < len )
			cpio_len = cpio_align ( cpio_len );
		if ( cpio_len > len ) {
			DBG ( "Truncated CPIO file\n" );
			return -1;
		}

		/* If we reach the trailer, we're done */
		if ( strcmp ( file_name, CPIO_TRAILER ) == 0 )
			return 0;

		/* Process file */
		if ( ( rc = file ( file_name, file_data, file_len ) ) != 0 )
			return rc;

		/* Move to next file */
		data += cpio_len;
		len -= cpio_len;
	}
}
