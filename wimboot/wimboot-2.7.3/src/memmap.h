#ifndef _MEMMAP_H
#define _MEMMAP_H

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

#include <stdint.h>

/** Magic value for INT 15,e820 calls */
#define E820_SMAP 0x534d4150

/** An INT 15,e820 memory map entry */
struct e820_entry {
	/** Start of region */
	uint64_t start;
	/** Length of region */
	uint64_t len;
	/** Type of region */
	uint32_t type;
	/** Extended attributes (optional) */
	uint32_t attrs;
} __attribute__ (( packed ));

/** Normal RAM */
#define E820_TYPE_RAM 1

/** Region is enabled (if extended attributes are present) */
#define E820_ATTR_ENABLED 0x00000001UL

/** Region is non-volatile memory (if extended attributes are present) */
#define E820_ATTR_NONVOLATILE 0x00000002UL

extern struct e820_entry * memmap_next ( struct e820_entry *prev );

#endif /* _MEMMAP_H */
