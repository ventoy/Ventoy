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
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "wimboot.h"
#include "peloader.h"

/**
 * Load PE image into memory
 *
 * @v data		PE image
 * @v len		Length of PE image
 * @v pe		Loaded PE structure to fill in
 * @ret rc		Return status code
 */
int load_pe ( const void *data, size_t len, struct loaded_pe *pe ) {
	const struct mz_header *mzhdr;
	size_t pehdr_offset;
	const struct pe_header *pehdr;
	size_t opthdr_offset;
	const struct pe_optional_header *opthdr;
	size_t section_offset;
	const struct coff_section *section;
	char name[ sizeof ( section->name ) + 1 /* NUL */ ];
	unsigned int i;
	void *section_base;
	size_t filesz;
	size_t memsz;
	void *end;
	void *raw_base;

	DBG2 ( "Loading PE executable...\n" );

	/* Parse PE header */
	mzhdr = data;
	if ( mzhdr->magic != MZ_HEADER_MAGIC ) {
		DBG ( "Bad MZ magic %04x\n", mzhdr->magic );
		return -1;
	}
	pehdr_offset = mzhdr->lfanew;
	if ( pehdr_offset > len ) {
		DBG ( "PE header outside file\n" );
		return -1;
	}
	pehdr = ( data + pehdr_offset );
	if ( pehdr->magic != PE_HEADER_MAGIC ) {
		DBG ( "Bad PE magic %08x\n", pehdr->magic );
		return -1;
	}
	opthdr_offset = ( pehdr_offset + sizeof ( *pehdr ) );
	opthdr = ( data + opthdr_offset );
	pe->base = ( ( void * ) ( intptr_t ) ( opthdr->base ) );
	section_offset = ( opthdr_offset + pehdr->coff.opthdr_len );
	section = ( data + section_offset );

	/* Load header into memory */
	DBG2 ( "...headers to %p+%#x\n", pe->base, opthdr->header_len );
	memcpy ( pe->base, data, opthdr->header_len );
	end = ( pe->base + opthdr->header_len );

	/* Load each section into memory */
	for ( i = 0 ; i < pehdr->coff.num_sections ; i++, section++ ) {
		memset ( name, 0, sizeof ( name ) );
		memcpy ( name, section->name, sizeof ( section->name ) );
		section_base = ( pe->base + section->virtual );
		filesz = section->raw_len;
		memsz = section->misc.virtual_len;
		DBG2 ( "...from %#05x to %p+%#zx/%#zx (%s)\n",
		       section->raw, section_base, filesz, memsz, name );
		memset ( section_base, 0, memsz );
		memcpy ( section_base, ( data + section->raw ), filesz );
		if ( end < ( section_base + memsz ) )
			end = ( section_base + memsz );
	}
	pe->len = ( ( ( end - pe->base ) + opthdr->section_align - 1 )
		    & ~( opthdr->section_align - 1 ) );

	/* Load copy of raw image into memory immediately after loaded
	 * sections.  This seems to be used for verification of X.509
	 * signatures.
	 */
	raw_base = ( pe->base + pe->len );
	memcpy ( raw_base, data, len );
	pe->len += len;
	DBG2 ( "...raw copy to %p+%#zx\n", raw_base, len );

	/* Extract entry point */
	pe->entry = ( pe->base + opthdr->entry );
	DBG2 ( "...entry point %p\n", pe->entry );

	return 0;
}
