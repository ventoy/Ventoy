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
 * INT 13 emulation
 *
 */

#include <string.h>
#include <stdio.h>
#include "wimboot.h"
#include "int13.h"
#include "vdisk.h"

/** Emulated drive number */
static int vdisk_drive;

/**
 * Initialise emulation
 *
 * @ret drive		Emulated drive number
 */
int initialise_int13 ( void ) {

	/* Determine drive number */
	vdisk_drive = ( 0x80 | INT13_DRIVE_COUNT++ );
	DBG ( "Emulating drive %#02x\n", vdisk_drive );

	return vdisk_drive;
}

/**
 * INT 13, 08 - Get drive parameters
 *
 * @ret ch		Low bits of maximum cylinder number
 * @ret cl (bits 7:6)	High bits of maximum cylinder number
 * @ret cl (bits 5:0)	Maximum sector number
 * @ret dh		Maximum head number
 * @ret dl		Number of drives
 * @ret ah		Status code
 */
static void int13_get_parameters ( struct bootapp_callback_params *params ) {
	unsigned int max_cylinder = ( VDISK_CYLINDERS - 1 );
	unsigned int max_head = ( VDISK_HEADS - 1 );
	unsigned int max_sector = ( VDISK_SECTORS_PER_TRACK - 0 /* sic */ );
	unsigned int num_drives;
	unsigned int min_num_drives;

	/* Calculate number of drives to report */
	num_drives = INT13_DRIVE_COUNT;
	min_num_drives = ( ( vdisk_drive & 0x7f ) + 1 );
	if ( num_drives < min_num_drives )
		num_drives = min_num_drives;

	/* Fill in drive parameters */
	params->ch = ( max_cylinder & 0xff );
	params->cl = ( ( ( max_cylinder >> 8 ) << 6 ) | max_sector );
	params->dh = max_head;
	params->dl = num_drives;
	DBG2 ( "Get parameters: C/H/S = %d/%d/%d, drives = %d\n",
	       ( max_cylinder + 1 ), ( max_head + 1 ), max_sector, num_drives );

	/* Success */
	params->ah = 0;
}

/**
 * INT 13, 15 - Get disk type
 *
 * @ret cx:dx		Sector count
 * @ret ah		Type code
 */
static void int13_get_disk_type ( struct bootapp_callback_params *params ) {
	uint32_t sector_count = VDISK_COUNT;
	uint8_t drive_type = INT13_DISK_TYPE_HDD;

	/* Fill in disk type */
	params->cx = ( sector_count >> 16 );
	params->dx = ( sector_count & 0xffff );
	params->ah = drive_type;
	DBG2 ( "Get disk type: sectors = %#08x, type = %d\n",
	       sector_count, drive_type );
}

/**
 * INT 13, 41 - Extensions installation check
 *
 * @v bx		0x55aa
 * @ret bx		0xaa55
 * @ret cx		Extensions API support bitmap
 * @ret ah		API version
 */
static void int13_extension_check ( struct bootapp_callback_params *params ) {

	/* Fill in extension information */
	params->bx = 0xaa55;
	params->cx = INT13_EXTENSION_LINEAR;
	params->ah = INT13_EXTENSION_VER_1_X;
	DBG2 ( "Extensions installation check\n" );
}

/**
 * INT 13, 48 - Get extended parameters
 *
 * @v ds:si		Drive parameter table
 * @ret ah		Status code
 */
static void
int13_get_extended_parameters ( struct bootapp_callback_params *params ) {
	struct int13_disk_parameters *disk_params;

	/* Fill in extended parameters */
	disk_params = REAL_PTR ( params->ds, params->si );
	memset ( disk_params, 0, sizeof ( *disk_params ) );
	disk_params->bufsize = sizeof ( *disk_params );
	disk_params->flags = INT13_FL_DMA_TRANSPARENT;
	disk_params->cylinders = VDISK_CYLINDERS;
	disk_params->heads = VDISK_HEADS;
	disk_params->sectors_per_track = VDISK_SECTORS_PER_TRACK;
	disk_params->sectors = VDISK_COUNT;
	disk_params->sector_size = VDISK_SECTOR_SIZE;
	DBG2 ( "Get extended parameters: C/H/S = %d/%d/%d, sectors = %#08llx "
	       "(%d bytes)\n", disk_params->cylinders, disk_params->heads,
	       disk_params->sectors_per_track, disk_params->sectors,
	       disk_params->sector_size );

	/* Success */
	params->ah = 0;
}

/**
 * INT 13, 42 - Extended read
 *
 * @v ds:si		Disk address packet
 * @ret ah		Status code
 */
static void int13_extended_read ( struct bootapp_callback_params *params ) {
	struct int13_disk_address *disk_address;
	void *data;

	/* Read from emulated disk */
	disk_address = REAL_PTR ( params->ds, params->si );
	data = REAL_PTR ( disk_address->buffer.segment,
			  disk_address->buffer.offset );
	vdisk_read ( disk_address->lba, disk_address->count, data );

	/* Success */
	params->ah = 0;
}

/**
 * Emulate INT 13 drive
 *
 * @v params		Parameters
 */
void emulate_int13 ( struct bootapp_callback_params *params ) {
	int command = params->ah;
	int drive = params->dl;
	int min_num_drives;
	unsigned long eflags;

	if ( drive == vdisk_drive ) {

		/* Emulated drive - handle internally */

		/* Populate eflags with a sensible starting value */
		__asm__ ( "pushf\n\t"
			  "pop %0\n\t"
			  : "=r" ( eflags ) );
		params->eflags = ( eflags & ~CF );

		/* Handle command */
		switch ( command ) {
		case INT13_GET_PARAMETERS:
			int13_get_parameters ( params );
			break;
		case INT13_GET_DISK_TYPE:
			int13_get_disk_type ( params );
			break;
		case INT13_EXTENSION_CHECK:
			int13_extension_check ( params );
			break;
		case INT13_GET_EXTENDED_PARAMETERS:
			int13_get_extended_parameters ( params );
			break;
		case INT13_EXTENDED_READ:
			int13_extended_read ( params );
			break;
		default:
			DBG ( "Unrecognised INT 13,%02x\n", command );
			params->eflags |= CF;
			break;
		}

	} else {

		/* Pass through command to underlying INT 13 */
		call_interrupt ( params );

		/* Modify drive count, if applicable */
		if ( command == INT13_GET_PARAMETERS ) {
			min_num_drives = ( ( vdisk_drive & 0x7f ) + 1 );
			if ( params->dl < min_num_drives )
				params->dl = min_num_drives;
		}
	}
}
