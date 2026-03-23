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
 * EFI boot manager invocation
 *
 */

#include <stdio.h>
#include <string.h>
#include "wimboot.h"
#include "cmdline.h"
#include "vdisk.h"
#include "pause.h"
#include "efi.h"
#include "efi/Protocol/GraphicsOutput.h"
#include "efi/Guid/GlobalVariable.h"
#include "efipath.h"
#include "efiblock.h"
#include "efifile.h"
#include "efiboot.h"

/** Original OpenProtocol() method */
static EFI_OPEN_PROTOCOL orig_open_protocol;

/** Dummy "opening graphics output protocol blocked once" protocol GUID */
static EFI_GUID efi_graphics_output_protocol_blocked_guid = {
	0xbd1598bf, 0x8e65, 0x47e0,
	{ 0x80, 0x01, 0xe4, 0x62, 0x4c, 0xab, 0xa4, 0x7f }
};

/** Dummy "opening graphics output protocol blocked once" protocol instance */
static uint8_t efi_graphics_output_protocol_blocked;

/**
 * Intercept OpenProtocol()
 *
 * @v handle		EFI handle
 * @v protocol		Protocol GUID
 * @v interface		Opened interface
 * @v agent_handle	Agent handle
 * @v controller_handle	Controller handle
 * @v attributes	Attributes
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_open_protocol_wrapper ( EFI_HANDLE handle, EFI_GUID *protocol,
			    VOID **interface, EFI_HANDLE agent_handle,
			    EFI_HANDLE controller_handle, UINT32 attributes ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;

	/* Open the protocol */
	if ( ( efirc = orig_open_protocol ( handle, protocol, interface,
					    agent_handle, controller_handle,
					    attributes ) ) != 0 ) {
		return efirc;
	}

	/* Block first attempt by bootmgfw.efi to open each
	 * EFI_GRAPHICS_OUTPUT_PROTOCOL.  This forces error messages
	 * to be displayed in text mode (thereby avoiding the totally
	 * blank error screen if the fonts are missing).  We must
	 * allow subsequent attempts to succeed, otherwise the OS will
	 * fail to boot.
	 */
	if ( ( memcmp ( protocol, &efi_graphics_output_protocol_guid,
			sizeof ( *protocol ) ) == 0 ) &&
	     ( bs->InstallMultipleProtocolInterfaces (
			&handle,
			&efi_graphics_output_protocol_blocked_guid,
			&efi_graphics_output_protocol_blocked,
			NULL ) == 0 ) &&
	     ( ! cmdline_gui ) ) {
		DBG ( "Forcing text mode output\n" );
		return EFI_INVALID_PARAMETER;
	}

	return 0;
}

/**
 * Show Secure Boot status
 *
 */
static void efi_show_sb ( void ) {
	EFI_RUNTIME_SERVICES *rs = efi_systab->RuntimeServices;
	char secureboot;
	UINTN size;
	EFI_STATUS efirc;

	/* Get Secure Boot status, if known */
	size = sizeof ( secureboot );
	efirc = rs->GetVariable ( EFI_SECURE_BOOT_MODE_NAME,
				  &efi_global_variable_guid, NULL,
				  &size, &secureboot );
	switch ( efirc ) {
	case 0:
		DBG ( "Secure Boot is %sabled\n",
		      ( secureboot ? "en" : "dis" ) );
		break;
	case EFI_NOT_FOUND:
		DBG ( "Secure Boot is not supported\n" );
		break;
	default:
		DBG ( "Secure Boot status unknown: %#lx\n",
		      ( ( unsigned long ) efirc ) );
		return;
	}
}

/**
 * Try loading EFI image
 *
 * @v file		Virtual file
 * @ret handle		Image handle, or NULL on failure
 */
static EFI_HANDLE efi_load ( struct vdisk_file *file ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_PHYSICAL_ADDRESS phys;
	void *data;
	unsigned int pages;
	EFI_HANDLE handle;
	EFI_STATUS efirc;

	/* Temporarily rename file */
	file->filename = efi_bootarch_name();

	/* Allocate memory */
	pages = ( ( file->len + PAGE_SIZE - 1 ) / PAGE_SIZE );
	if ( ( efirc = bs->AllocatePages ( AllocateAnyPages,
					   EfiBootServicesData, pages,
					   &phys ) ) != 0 ) {
		die ( "Could not allocate %d pages: %#lx\n",
		      pages, ( ( unsigned long ) efirc ) );
	}
	data = ( ( void * ) ( intptr_t ) phys );

	/* Read image */
	file->read ( file, data, 0, file->len );
	DBG ( "Read %s\n", file->name );

	/* Load image */
	if ( ( efirc = bs->LoadImage ( FALSE, efi_image_handle, bootarch_path,
				       data, file->len, &handle ) ) != 0 ) {
		DBG ( "Could not load %s: %#lx\n",
		      file->name, ( ( unsigned long ) efirc ) );
		bs->FreePages ( phys, pages );
		file->filename = file->name;
		return NULL;
	}
	DBG ( "Loaded %s as %s\n", file->name, file->filename );

	return handle;
}

/**
 * Boot from EFI device
 *
 * @v device		Device handle
 */
void efi_boot ( EFI_HANDLE device ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	union {
		EFI_LOADED_IMAGE_PROTOCOL *image;
		void *intf;
	} loaded;
	struct vdisk_file *file;
	EFI_HANDLE handle;
	EFI_STATUS efirc;

	/* Show Secure Boot status */
	efi_show_sb();

	/* Load an image */
	if ( bootmgfw_ex && ( handle = efi_load ( bootmgfw_ex ) ) ) {
		file = bootmgfw_ex;
	} else if ( bootmgfw && ( handle = efi_load ( bootmgfw ) ) ) {
		file = bootmgfw;
	} else {
		die ( "Could not load any bootloaders\n" );
	}

	/* Get loaded image protocol */
	if ( ( efirc = bs->OpenProtocol ( handle,
					  &efi_loaded_image_protocol_guid,
					  &loaded.intf, efi_image_handle, NULL,
					  EFI_OPEN_PROTOCOL_GET_PROTOCOL ))!=0){
		die ( "Could not get loaded image protocol for %s: %#lx\n",
		      file->name, ( ( unsigned long ) efirc ) );
	}

	/* Force correct device handle */
	if ( loaded.image->DeviceHandle != device ) {
		DBG ( "Forcing correct DeviceHandle (%p->%p)\n",
		      loaded.image->DeviceHandle, device );
		loaded.image->DeviceHandle = device;
	}

	/* Intercept calls to OpenProtocol() */
	orig_open_protocol =
		loaded.image->SystemTable->BootServices->OpenProtocol;
	loaded.image->SystemTable->BootServices->OpenProtocol =
		efi_open_protocol_wrapper;

	/* Start image */
	if ( cmdline_pause )
		pause();
	if ( ( efirc = bs->StartImage ( handle, NULL, NULL ) ) != 0 ) {
		die ( "Could not start %s: %#lx\n",
		      file->name, ( ( unsigned long ) efirc ) );
	}

	die ( "%s returned\n", file->name );
}
