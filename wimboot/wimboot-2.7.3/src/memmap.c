/*
 * Copyright (C) 2021 Michael Brown <mbrown@fensystems.co.uk>.
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
 * Memory map
 *
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "wimboot.h"
#include "memmap.h"

/** Buffer for INT 15,e820 calls */
static struct e820_entry e820_buf __attribute__ (( section ( ".bss16" ) ));

/** Continuation value for next INT 15,e820 call */
static uint32_t e820_ebx;

/**
 * Get system memory map entry
 *
 * @v prev		Previous system memory map entry, or NULL at start
 * @v next		Next system memory map entry, or NULL at end
 */
struct e820_entry * memmap_next ( struct e820_entry *prev ) {
	struct bootapp_callback_params params;

	/* Reset buffer and continuation value if restarting */
	if ( ! prev ) {
		memset ( &e820_buf, 0, sizeof ( e820_buf ) );
		e820_ebx = 0;
	} else if ( e820_ebx == 0 ) {
		/* Reach the end */
		return NULL;
	}

	/* Read system memory map */
	memset ( &params, 0, sizeof ( params ) );
	do {

		/* Call INT 15,e820 */
		params.vector.interrupt = 0x15;
		params.eax = 0xe820;
		params.ebx = e820_ebx;
		params.ecx = sizeof ( e820_buf );
		params.edx = E820_SMAP;
		params.es = BASE_SEG;
		params.edi = ( ( ( void * ) &e820_buf ) -
			       ( ( void * ) BASE_ADDRESS ) );
		call_interrupt ( &params );

		/* Record continuation value */
		e820_ebx = params.ebx;

		/* Check result */
		if ( params.eflags & CF ) {
			DBG ( "INT 15,e820 failed: error %02x\n", params.ah );
			break;
		}
		if ( params.eax != E820_SMAP ) {
			DBG ( "INT 15,e820 invalid SMAP signature %08x\n",
			      params.eax );
			break;
		}
		DBG2 ( "INT 15,e820 region [%llx,%llx) type %d\n",
		       e820_buf.start, ( e820_buf.start + e820_buf.len ),
		       e820_buf.type );

		/* Skip non-RAM regions */
		if ( e820_buf.type != E820_TYPE_RAM )
			continue;
		if ( params.ecx > offsetof ( typeof ( e820_buf ), attrs ) ) {
			if ( ! ( e820_buf.attrs & E820_ATTR_ENABLED ) )
				continue;
			if ( e820_buf.attrs & E820_ATTR_NONVOLATILE )
				continue;
		}

		/* Return this region */
		return &e820_buf;

	} while ( e820_ebx != 0 );

	return NULL;
}
