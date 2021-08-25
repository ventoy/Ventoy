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
#include "efifile.h"

/** bootmgfw.efi path within WIM */
static const wchar_t bootmgfw_path[] = L"\\Windows\\Boot\\EFI\\bootmgfw.efi";

/** Other paths within WIM */
static const wchar_t *efi_wim_paths[] = {
	L"\\Windows\\Boot\\DVD\\EFI\\boot.sdi",
	L"\\Windows\\Boot\\DVD\\EFI\\BCD",
	L"\\Windows\\Boot\\Fonts\\segmono_boot.ttf",
	L"\\Windows\\Boot\\Fonts\\segoen_slboot.ttf",
	L"\\Windows\\Boot\\Fonts\\segoe_slboot.ttf",
	L"\\Windows\\Boot\\Fonts\\wgl4_boot.ttf",
	L"\\sms\\boot\\boot.sdi",
	NULL
};

/** bootmgfw.efi file */
struct vdisk_file *bootmgfw;

/**
 * Get architecture-specific boot filename
 *
 * @ret bootarch	Architecture-specific boot filename
 */
static const CHAR16 * efi_bootarch ( void ) {
	static const CHAR16 bootarch_full[] = EFI_REMOVABLE_MEDIA_FILE_NAME;
	const CHAR16 *tmp;
	const CHAR16 *bootarch = bootarch_full;

	for ( tmp = bootarch_full ; *tmp ; tmp++ ) {
		if ( *tmp == L'\\' )
			bootarch = ( tmp + 1 );
	}
	return bootarch;
}

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
#if 0
	EFI_FILE_PROTOCOL *file = vfile->opaque;
	UINTN size = len;
	EFI_STATUS efirc;

	/* Set file position */
	if ( ( efirc = file->SetPosition ( file, offset ) ) != 0 ) {
		die ( "Could not set file position: %#lx\n",
		      ( ( unsigned long ) efirc ) );
	}

	/* Read from file */
	if ( ( efirc = file->Read ( file, &size, data ) ) != 0 ) {
		die ( "Could not read from file: %#lx\n",
		      ( ( unsigned long ) efirc ) );
	}
#endif /* #if 0 */

    (void)vfile;

    pfventoy_file_read((const char *)vfile->opaque, (int)offset, (int)len, data);
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
	struct vdisk_file *wim = NULL;
	struct vdisk_file *vfile;
	CHAR16 wname[64];
    int i, j, k;
    char *pos;
    size_t len = 0;

    (void)handle;

	/* Read root directory */
    for (i = 0; i < cmdline_vf_num; i++) {
        pos = strchr(cmdline_vf_path[i], ':');

        *pos = 0;
        k = (int)strlen(cmdline_vf_path[i]);
    
        memset(wname, 0, sizeof(wname));
        for (j = 0; j < k; j++)
        {
            wname[j] = cmdline_vf_path[i][j];
        }

        len = pfventoy_file_size(pos + 1);
		vfile = vdisk_add_file (cmdline_vf_path[i], pos + 1, len, efi_read_file);

		/* Check for special-case files */
		if ( ( wcscasecmp ( wname, efi_bootarch() ) == 0 ) ||
		     ( wcscasecmp ( wname, L"bootmgfw.efi" ) == 0 ) ) {
			DBG ( "...found bootmgfw.efi file %ls\n", wname );
			bootmgfw = vfile;
		} else if ( wcscasecmp ( wname, L"BCD" ) == 0 ) {
			DBG ( "...found BCD\n" );
			vdisk_patch_file ( vfile, efi_patch_bcd );
		} else if ( wcscasecmp ( ( wname + ( wcslen ( wname ) - 4 ) ),
					 L".wim" ) == 0 ) {
			DBG ( "...found WIM file %ls\n", wname );
			wim = vfile;
		}
	}

	/* Process WIM image */
	if ( wim ) {
		vdisk_patch_file ( wim, patch_wim );
		if ( ( ! bootmgfw ) &&
		     ( bootmgfw = wim_add_file ( wim, cmdline_index,
						 bootmgfw_path,
						 efi_bootarch() ) ) ) {
			DBG ( "...extracted %ls\n", bootmgfw_path );
		}
		wim_add_files ( wim, cmdline_index, efi_wim_paths );
	}

	/* Check that we have a boot file */
	if ( ! bootmgfw ) {
		die ( "FATAL: no %ls or bootmgfw.efi found\n",
		      efi_bootarch() );
	}
}
