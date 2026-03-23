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
 * EFI file system access
 *
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <wchar.h>
#include "wimboot.h"
#include "vdisk.h"
#include "cmdline.h"
#include "wimpatch.h"
#include "wimfile.h"
#include "efi.h"
#include "efipath.h"
#include "efifile.h"

/** bootmgfw.efi path within WIM */
static const wchar_t bootmgfw_path[] = L"\\Windows\\Boot\\EFI\\bootmgfw.efi";

/** bootmgfw_EX.efi path within WIM */
static const wchar_t bootmgfw_ex_path[] =
	L"\\Windows\\Boot\\EFI_EX\\bootmgfw_EX.efi";

/** Other paths within WIM */
static const wchar_t *efi_wim_paths[] = {
	L"\\Windows\\Boot\\DVD\\EFI\\boot.sdi",
	L"\\Windows\\Boot\\DVD\\EFI\\BCD",
	L"\\Windows\\Boot\\EFI\\boot.stl",
	L"\\Windows\\Boot\\Fonts\\segmono_boot.ttf",
	L"\\Windows\\Boot\\Fonts\\segoen_slboot.ttf",
	L"\\Windows\\Boot\\Fonts\\segoe_slboot.ttf",
	L"\\Windows\\Boot\\Fonts\\wgl4_boot.ttf",
	L"\\sms\\boot\\boot.sdi",
	NULL
};

/** bootmgfw.efi file */
struct vdisk_file *bootmgfw;

/** bootmgfw_EX.efi file */
struct vdisk_file *bootmgfw_ex;

/**
 * Read from EFI file
 *
 * @v vfile		Virtual file
 * @v data		Data buffer
 * @v offset		Offset
 * @v len		Length
 */
static void efi_read_file ( struct vdisk_file *vfile, void *data,
			    size_t offset, size_t len ) {
	pfventoy_file_read ( ( ( const char * ) vfile->opaque ),
			     ( ( int ) offset ), ( ( int ) len ), data );
}

/**
 * Patch BCD file
 *
 * @v vfile		Virtual file
 * @v data		Data buffer
 * @v offset		Offset
 * @v len		Length
 */
static void efi_patch_bcd ( struct vdisk_file *vfile __unused, void *data,
			    size_t offset, size_t len ) {
	static const wchar_t search[] = L".exe";
	static const wchar_t replace[] = L".efi";
	size_t i;

	/* Do nothing if BCD patching is disabled */
	if ( cmdline_rawbcd )
		return;

	/* Patch any occurrences of ".exe" to ".efi".  In the common
	 * simple cases, this allows the same BCD file to be used for
	 * both BIOS and UEFI systems.
	 */
	for ( i = 0 ; ( i + sizeof ( search ) ) < len ; i++ ) {
		if ( wcscasecmp ( ( data + i ), search ) == 0 ) {
			memcpy ( ( data + i ), replace, sizeof ( replace ) );
			DBG ( "...patched BCD at %#zx: \"%ls\" to \"%ls\"\n",
			      ( offset + i ), search, replace );
		}
	}
}

/**
 * Extract files from EFI file system
 *
 * @v handle		Device handle
 */
void efi_extract ( EFI_HANDLE handle ) {
	char name[ VDISK_NAME_LEN + 1 /* NUL */ ];
	struct vdisk_file *wim = NULL;
	struct vdisk_file *bootarch = NULL;
	struct vdisk_file *vfile;
	CHAR16 wname[ VDISK_NAME_LEN + 1 /* WNUL */ ];
	char *path;
	int file_len;
	size_t len = 0;
	int i;
	int j;

	( void ) handle;

	if ( ( ! pfventoy_file_size ) || ( ! pfventoy_file_read ) )
		die ( "FATAL: no Ventoy file callbacks\n" );

	/* Read root directory */
	for ( i = 0 ; i < cmdline_vf_num ; i++ ) {
		path = strchr ( cmdline_vf_path[i], ':' );
		if ( ! path )
			die ( "Invalid vf \"%s\"\n", cmdline_vf_path[i] );
		*(path++) = '\0';
		if ( ! path[0] )
			die ( "Invalid vf \"%s\"\n", cmdline_vf_path[i] );

		/* Add file */
		snprintf ( name, sizeof ( name ), "%s", cmdline_vf_path[i] );
		memset ( wname, 0, sizeof ( wname ) );
		for ( j = 0 ; cmdline_vf_path[i][j] && ( j < VDISK_NAME_LEN ) ; j++ )
			wname[j] = cmdline_vf_path[i][j];
		file_len = pfventoy_file_size ( path );
		if ( file_len < 0 )
			die ( "Could not get size for \"%s\"\n", path );
		len = file_len;
		vfile = vdisk_add_file ( name, path, len, efi_read_file );

		/* Check for special-case files */
		if ( wcscasecmp ( wname, efi_bootarch_wname() ) == 0 ) {
			DBG ( "...found bootloader file %ls\n", wname );
			bootarch = vfile;
		} else if ( wcscasecmp ( wname, L"bootmgfw.efi" ) == 0 ) {
			DBG ( "...found bootloader file %ls\n", wname );
			bootmgfw = vfile;
		} else if ( wcscasecmp ( wname, L"bootmgfw_EX.efi" ) == 0 ) {
			DBG ( "...found bootloader file %ls\n", wname );
			bootmgfw_ex = vfile;
		} else if ( wcscasecmp ( wname, L"BCD" ) == 0 ) {
			DBG ( "...found BCD\n" );
			vdisk_patch_file ( vfile, efi_patch_bcd );
		} else if ( wcscasecmp ( ( wname + ( wcslen ( wname ) - 4 ) ),
					 L".wim" ) == 0 ) {
			DBG ( "...found WIM file %ls\n", wname );
			wim = vfile;
		}
	}

	/* Use only boot<arch>.efi if provided */
	if ( bootarch ) {
		if ( bootmgfw )
			DBG ( "...ignoring %s\n", bootmgfw->name );
		if ( bootmgfw_ex )
			DBG ( "...ignoring %s\n", bootmgfw_ex->name );
		bootmgfw = bootarch;
		bootmgfw_ex = NULL;
	}

	/* Extract bootloader(s) from WIM if none are explicitly provided */
	if ( wim && ( ! bootmgfw ) && ( ! bootmgfw_ex ) ) {
		if ( ( bootmgfw = wim_add_file ( wim, cmdline_index,
						 bootmgfw_path ) ) ) {
			DBG ( "...extracted %ls\n", bootmgfw_path );
		}
		if ( ( bootmgfw_ex = wim_add_file ( wim, cmdline_index,
						    bootmgfw_ex_path ) ) ) {
			DBG ( "...extracted %ls\n", bootmgfw_ex_path );
		}
	}

	/* Process WIM image */
	if ( wim ) {
		vdisk_patch_file ( wim, patch_wim );
		wim_add_files ( wim, cmdline_index, efi_wim_paths );
	}

	/* Check that we have a boot file */
	if ( ( ! bootmgfw ) && ( ! bootmgfw_ex ) ) {
		die ( "FATAL: no bootloader file found\n" );
	}
}
