/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
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
 * WIM virtual files
 *
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <wchar.h>
#include "wimboot.h"
#include "vdisk.h"
#include "wim.h"
#include "wimfile.h"

/** A WIM virtual file */
struct wim_file {
	/** Underlying virtual file */
	struct vdisk_file *file;
	/** WIM header */
	struct wim_header header;
	/** Resource */
	struct wim_resource_header resource;
};

/** Maximum number of WIM virtual files */
#define WIM_MAX_FILES 8

/** WIM virtual files */
static struct wim_file wim_files[WIM_MAX_FILES];

/**
 * Read from WIM virtual file
 *
 * @v file		Virtual file
 * @v data		Data buffer
 * @v offset		Offset
 * @v len		Length
 */
static void wim_read_file ( struct vdisk_file *file, void *data,
			    size_t offset, size_t len ) {
	struct wim_file *wfile = file->opaque;
	int rc;

	/* Read from resource */
	if ( ( rc = wim_read ( wfile->file, &wfile->header, &wfile->resource,
			       data, offset, len ) ) != 0 ) {
		die ( "Could not read from WIM virtual file\n" );
	}
}

/**
 * Add WIM virtual file
 *
 * @v file		Underlying virtual file
 * @v index		Image index, or 0 to use boot image
 * @v path		Path to file within WIM
 * @v wname		New virtual file name
 * @ret file		Virtual file, or NULL if not found
 */
struct vdisk_file * wim_add_file ( struct vdisk_file *file, unsigned int index,
				   const wchar_t *path, const wchar_t *wname ) {
	static unsigned int wim_file_idx = 0;
	struct wim_resource_header meta;
	struct wim_file *wfile;
	char name[ VDISK_NAME_LEN + 1 /* NUL */ ];
	unsigned int i;
	int rc;

	/* Sanity check */
	if ( wim_file_idx >= WIM_MAX_FILES )
		die ( "Too many WIM files\n" );
	wfile = &wim_files[wim_file_idx];

	/* Construct ASCII file name */
	snprintf ( name, sizeof ( name ), "%ls", wname );

	/* Skip files already added explicitly */
	for ( i = 0 ; i < VDISK_MAX_FILES ; i++ ) {
		if ( strcasecmp ( name, vdisk_files[i].name ) == 0 )
			return NULL;
	}

	/* Get WIM header */
	if ( ( rc = wim_header ( file, &wfile->header ) ) != 0 )
		return NULL;

	/* Get image metadata */
	if ( ( rc = wim_metadata ( file, &wfile->header, index, &meta ) ) != 0 )
		return NULL;

	/* Get file resource */
	if ( ( rc = wim_file ( file, &wfile->header, &meta, path,
			       &wfile->resource ) ) != 0 )
		return NULL;

	/* Add virtual file */
	wim_file_idx++;
	wfile->file = file;
	return vdisk_add_file ( name, wfile, wfile->resource.len,
				wim_read_file );
}

/**
 * Add WIM virtual files
 *
 * @v file		Underlying virtual file
 * @v index		Image index, or 0 to use boot image
 * @v paths		List of paths to files within WIM
 */
void wim_add_files ( struct vdisk_file *file, unsigned int index,
		     const wchar_t **paths ) {
	const wchar_t **path;
	const wchar_t *wname;
	const wchar_t *tmp;

	/* Add any existent files within the list */
	for ( path = paths ; *path ; path++ ) {

		/* Construct file name */
		wname = *path;
		for ( tmp = wname ; *tmp ; tmp++ ) {
			if ( *tmp == L'\\' )
				wname = ( tmp + 1 );
		}

		/* Add virtual file, if existent */
		wim_add_file ( file, index, *path, wname );
	}
}
