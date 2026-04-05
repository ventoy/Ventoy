#ifndef _EFI_H
#define _EFI_H

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
 * EFI definitions
 *
 */

/* EFIAPI definition */
#if __x86_64__
#define EFIAPI __attribute__ (( ms_abi ))
#else
#define EFIAPI
#endif

/* EFI headers rudely redefine NULL */
#undef NULL

#include "efi/Uefi.h"
#include "efi/Protocol/LoadedImage.h"

extern EFI_SYSTEM_TABLE *efi_systab;
extern EFI_HANDLE efi_image_handle;

extern EFI_GUID efi_block_io_protocol_guid;
extern EFI_GUID efi_device_path_protocol_guid;
extern EFI_GUID efi_graphics_output_protocol_guid;
extern EFI_GUID efi_loaded_image_protocol_guid;
extern EFI_GUID efi_simple_file_system_protocol_guid;

#endif /* _EFI_H */
