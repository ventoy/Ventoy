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
 * Fatal errors
 *
 */

#include <stdarg.h>
#include <stdio.h>
#include "wimboot.h"
#include "efi.h"

/**
 * Handle fatal errors
 *
 * @v fmt	Error message format string
 * @v ...	Arguments
 */
void die ( const char *fmt, ... ) {
	EFI_BOOT_SERVICES *bs;
	EFI_RUNTIME_SERVICES *rs;
	va_list args;

	/* Print message */
	va_start ( args, fmt );
	vprintf ( fmt, args );
	va_end ( args );

	/* Reboot or exit as applicable */
	if ( efi_systab ) {

		/* Exit */
		bs = efi_systab->BootServices;
		bs->Exit ( efi_image_handle, EFI_LOAD_ERROR, 0, NULL );
		printf ( "Failed to exit\n" );
		rs = efi_systab->RuntimeServices;
		rs->ResetSystem ( EfiResetWarm, 0, 0, NULL );
		printf ( "Failed to reboot\n" );

	} else {

		/* Wait for keypress */
		printf ( "Press a key to reboot..." );
		getchar();
		printf ( "\n" );

		/* Reboot system */
		reboot();
	}

	/* Should be impossible to reach this */
	__builtin_unreachable();
}
