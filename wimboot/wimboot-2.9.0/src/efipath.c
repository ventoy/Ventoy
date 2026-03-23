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
 * EFI device paths
 *
 */

#include <stdio.h>
#include "wimboot.h"
#include "efi.h"
#include "efipath.h"

/**
 * Find end of device path
 *
 * @v path		Path to device
 * @ret path_end	End of device path
 */
EFI_DEVICE_PATH_PROTOCOL * efi_devpath_end ( EFI_DEVICE_PATH_PROTOCOL *path ) {

	while ( path->Type != END_DEVICE_PATH_TYPE ) {
		path = ( ( ( void * ) path ) +
			 /* There's this amazing new-fangled thing known as
			  * a UINT16, but who wants to use one of those? */
			 ( ( path->Length[1] << 8 ) | path->Length[0] ) );
	}
	return path;
}

/**
 * Get architecture-specific boot filename
 *
 * @ret wname		Architecture-specific boot filename
 */
const CHAR16 * efi_bootarch_wname ( void ) {
	static const CHAR16 bootarch_path[] = EFI_REMOVABLE_MEDIA_FILE_NAME;
	const CHAR16 *bootarch_wname = bootarch_path;
	const CHAR16 *tmp;

	for ( tmp = bootarch_path ; *tmp ; tmp++ ) {
		if ( *tmp == L'\\' )
			bootarch_wname = ( tmp + 1 );
	}
	return bootarch_wname;
}

/**
 * Get architecture-specific boot filename
 *
 * @ret name		Architecture-specific boot filename
 */
const char * efi_bootarch_name ( void ) {
	static char name[ sizeof ( EFI_REMOVABLE_MEDIA_FILE_NAME ) /
			  sizeof ( wchar_t ) ];

	snprintf ( name, sizeof ( name ), "%ls", efi_bootarch_wname() );
	return name;
}
