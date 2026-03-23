#ifndef _PELOADER_H
#define _PELOADER_H

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
 * PE image loader
 *
 */

#include <stdint.h>
#include "wimboot.h"

/** DOS MZ header */
struct mz_header {
	/** Magic number */
	uint16_t magic;
	/** Bytes on last page of file */
	uint16_t cblp;
	/** Pages in file */
	uint16_t cp;
	/** Relocations */
	uint16_t crlc;
	/** Size of header in paragraphs */
	uint16_t cparhdr;
	/** Minimum extra paragraphs needed */
	uint16_t minalloc;
	/** Maximum extra paragraphs needed */
	uint16_t maxalloc;
	/** Initial (relative) SS value */
	uint16_t ss;
	/** Initial SP value */
	uint16_t sp;
	/** Checksum */
	uint16_t csum;
	/** Initial IP value */
	uint16_t ip;
	/** Initial (relative) CS value */
	uint16_t cs;
	/** File address of relocation table */
	uint16_t lfarlc;
	/** Overlay number */
	uint16_t ovno;
	/** Reserved words */
	uint16_t res[4];
	/** OEM identifier (for oeminfo) */
	uint16_t oemid;
	/** OEM information; oemid specific */
	uint16_t oeminfo;
	/** Reserved words */
	uint16_t res2[10];
	/** File address of new exe header */
	uint32_t lfanew;
} __attribute__ (( packed ));

/** MZ header magic */
#define MZ_HEADER_MAGIC 0x5a4d

/** COFF file header */
struct coff_header {
	/** Magic number */
	uint16_t magic;
	/** Number of sections */
	uint16_t num_sections;
	/** Timestamp (seconds since the Epoch) */
	uint32_t timestamp;
	/** Offset to symbol table */
	uint32_t symtab;
	/** Number of symbol table entries */
	uint32_t num_syms;
	/** Length of optional header */
	uint16_t opthdr_len;
	/** Flags */
	uint16_t flags;
} __attribute__ (( packed ));

/** COFF section */
struct coff_section {
	/** Section name */
	char name[8];
	/** Physical address or virtual length */
	union {
		/** Physical address */
		uint32_t physical;
		/** Virtual length */
		uint32_t virtual_len;
	} misc;
	/** Virtual address */
	uint32_t virtual;
	/** Length of raw data */
	uint32_t raw_len;
	/** Offset to raw data */
	uint32_t raw;
	/** Offset to relocations */
	uint32_t relocations;
	/** Offset to line numbers */
	uint32_t line_numbers;
	/** Number of relocations */
	uint16_t num_relocations;
	/** Number of line numbers */
	uint16_t num_line_numbers;
	/** Flags */
	uint32_t flags;
} __attribute__ (( packed ));

/** PE file header */
struct pe_header {
	/** Magic number */
	uint32_t magic;
	/** COFF header */
	struct coff_header coff;
} __attribute__ (( packed ));

/** PE header magic */
#define PE_HEADER_MAGIC 0x00004550

/** PE optional header */
struct pe_optional_header {
	/** Magic number */
	uint16_t magic;
	/** Major linker version */
	uint8_t linker_major;
	/** Minor linker version */
	uint8_t linker_minor;
	/** Length of code */
	uint32_t text_len;
	/** Length of initialised data */
	uint32_t data_len;
	/** Length of uninitialised data */
	uint32_t bss_len;
	/** Entry point */
	uint32_t entry;
	/** Base of code */
	uint32_t text;
	/** Base of data */
	uint32_t data;
	/** Image base address */
	uint32_t base;
	/** Section alignment */
	uint32_t section_align;
	/** File alignment */
	uint32_t file_align;
	/** Major operating system version */
	uint16_t os_major;
	/** Minor operating system version */
	uint16_t os_minor;
	/** Major image version */
	uint16_t image_major;
	/** Minor image version */
	uint16_t image_minor;
	/** Major subsystem version */
	uint16_t subsystem_major;
	/** Minor subsystem version */
	uint16_t subsystem_minor;
	/** Win32 version */
	uint32_t win32_version;
	/** Size of image */
	uint32_t len;
	/** Size of headers */
	uint32_t header_len;
	/* Plus extra fields that we don't care about */
} __attribute__ (( packed ));

/** A loaded PE image */
struct loaded_pe {
	/** Base address */
	void *base;
	/** Length */
	size_t len;
	/** Entry point */
	void ( * entry ) ( struct bootapp_descriptor *bootapp );
};

extern int load_pe ( const void *data, size_t len, struct loaded_pe *pe );

#endif /* _PELOADER_H */
