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
 * Paging
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "wimboot.h"
#include "memmap.h"
#include "paging.h"

/** Virtual address used as a 2MB window during relocation */
#define COPY_WINDOW 0x200000

/** Paging is available */
int paging;

/** Page directory pointer table */
static uint64_t pdpt[4] __attribute__ (( aligned ( PAGE_SIZE ) ));

/** Page directories */
static uint64_t pd[2048] __attribute__ (( aligned ( PAGE_SIZE ) ));

/**
 * Check that paging can be supported
 *
 * @ret supported	Paging can be supported on this CPU
 */
static int paging_supported ( void ) {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;

	/* Get CPU features */
	__asm__ ( "cpuid"
		  : "=a" ( eax ), "=b" ( ebx ), "=c" ( ecx ), "=d" ( edx )
		  : "0" ( CPUID_FEATURES ) );

	return ( edx & CPUID_FEATURE_EDX_PAE );
}

/**
 * Map 2MB page directory entry containing address
 *
 * @v vaddr		Virtual address
 * @v paddr		Physical address
 */
static void map_page ( uint32_t vaddr, uint64_t paddr ) {
	char *byte = ( ( char * ) ( intptr_t ) vaddr );
	unsigned int index;

	/* Sanity checks */
	assert ( ( vaddr & ( PAGE_SIZE_2MB - 1 ) ) == 0 );
	assert ( ( paddr & ( PAGE_SIZE_2MB - 1 ) ) == 0 );

	/* Populate page directory entry */
	index = ( vaddr / PAGE_SIZE_2MB );
	pd[index] = ( paddr | PG_P | PG_RW | PG_US | PG_PS );

	/* Invalidate TLB */
	__asm__ __volatile__ ( "invlpg %0" : : "m" ( *byte ) );
}

/**
 * Initialise paging
 *
 */
void init_paging ( void ) {
	uint32_t addr;
	unsigned int i;

	/* Do nothing if paging is disabled */
	if ( cmdline_linear ) {
		DBG ( "Paging disabled\n" );
		return;
	}

	/* Check for PAE */
	if ( ! paging_supported() ) {
		DBG ( "Paging not possible on this CPU\n" );
		return;
	}

	/* Initialise page directory entries */
	addr = 0;
	do {
		map_page ( addr, addr );
		addr += PAGE_SIZE_2MB;
	} while ( addr );

	/* Initialise page directory pointer table */
	for ( i = 0 ; i < ( sizeof ( pdpt ) / sizeof ( pdpt[0] ) ) ; i++ ) {
		addr = ( ( intptr_t ) &pd[ i * PAGE_SIZE / sizeof ( pd[0] ) ] );
		pdpt[i] = ( addr | PG_P );
	}

	/* Mark paging as available */
	paging = 1;
}

/**
 * Enable paging
 *
 * @v state		Saved paging state to fill in
 */
void enable_paging ( struct paging_state *state ) {
	unsigned long cr0;
	unsigned long cr3;
	unsigned long cr4;

	/* Do nothing if paging is unavailable */
	if ( ! paging )
		return;

	/* Save paging state */
	__asm__ __volatile__ ( "mov %%cr0, %0\n\t"
			       "mov %%cr3, %1\n\t"
			       "mov %%cr4, %2\n\t"
			       : "=r" ( cr0 ), "=r" ( cr3 ), "=r" ( cr4 ) );
	state->cr0 = cr0;
	state->cr3 = cr3;
	state->cr4 = cr4;

	/* Disable any existing paging */
	__asm__ __volatile__ ( "mov %0, %%cr0" : : "r" ( cr0 & ~CR0_PG ) );

	/* Enable PAE */
	__asm__ __volatile__ ( "mov %0, %%cr4" : : "r" ( cr4 | CR4_PAE ) );

	/* Load page directory pointer table */
	__asm__ __volatile__ ( "mov %0, %%cr3" : : "r" ( pdpt ) );

	/* Enable paging */
	__asm__ __volatile__ ( "mov %0, %%cr0" : : "r" ( cr0 | CR0_PG ) );
}

/**
 * Disable paging
 *
 * @v state		Previously saved paging state
 */
void disable_paging ( struct paging_state *state ) {
	unsigned long cr0 = state->cr0;
	unsigned long cr3 = state->cr3;
	unsigned long cr4 = state->cr4;

	/* Do nothing if paging is unavailable */
	if ( ! paging )
		return;

	/* Disable paging */
	__asm__ __volatile__ ( "mov %0, %%cr0" : : "r" ( cr0 & ~CR0_PG ) );

	/* Restore saved paging state */
	__asm__ __volatile__ ( "mov %2, %%cr4\n\t"
			       "mov %1, %%cr3\n\t"
			       "mov %0, %%cr0\n\t"
			       : : "r" ( cr0 ), "r" ( cr3 ), "r" ( cr4 ) );
}

/**
 * Relocate data out of 32-bit address space, if possible
 *
 * @v data		Start of data
 * @v len		Length of data
 * @ret start		Physical start address
 */
uint64_t relocate_memory_high ( void *data, size_t len ) {
	intptr_t end = ( ( ( intptr_t ) data ) + len );
	struct e820_entry *e820 = NULL;
	uint64_t start;
	uint64_t dest;
	size_t offset;
	size_t frag_len;

	/* Do nothing if paging is unavailable */
	if ( ! paging )
		return ( ( intptr_t ) data );

	/* Read system memory map */
	while ( ( e820 = memmap_next ( e820 ) ) != NULL ) {

		/* Find highest compatible placement within this region */
		start = ( e820->start + e820->len );
		if ( start < ADDR_4GB )
			continue;
		start = ( ( ( start - end ) & ~( PAGE_SIZE_2MB - 1 ) ) + end );
		start -= len;
		if ( start < e820->start )
			continue;
		if ( start < ADDR_4GB )
			continue;

		/* Relocate to this region */
		dest = start;
		while ( len ) {

			/* Calculate length within this 2MB page */
			offset = ( ( ( intptr_t ) data ) &
				   ( PAGE_SIZE_2MB - 1 ) );
			frag_len = ( PAGE_SIZE_2MB - offset );
			if ( frag_len > len )
				frag_len = len;

			/* Map copy window to destination */
			map_page ( COPY_WINDOW,
				   ( dest & ~( PAGE_SIZE_2MB - 1 ) ) );

			/* Copy data through copy window */
			memcpy ( ( ( ( void * ) COPY_WINDOW ) + offset ),
				 data, frag_len );

			/* Map original page to destination */
			map_page ( ( ( ( intptr_t ) data ) - offset ),
				   ( dest & ~( PAGE_SIZE_2MB - 1 ) ) );

			/* Move to next 2MB page */
			data += frag_len;
			dest += frag_len;
			len -= frag_len;
		}

		/* Remap copy window */
		map_page ( COPY_WINDOW, COPY_WINDOW );

		return start;
	}

	/* Leave at original location */
	return ( ( intptr_t ) data );
}
