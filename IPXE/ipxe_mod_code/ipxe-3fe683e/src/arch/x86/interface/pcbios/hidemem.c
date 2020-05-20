/* Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <assert.h>
#include <realmode.h>
#include <biosint.h>
#include <basemem.h>
#include <fakee820.h>
#include <ipxe/init.h>
#include <ipxe/io.h>
#include <ipxe/hidemem.h>

/** Set to true if you want to test a fake E820 map */
#define FAKE_E820 0

/** Alignment for hidden memory regions */
#define ALIGN_HIDDEN 4096   /* 4kB page alignment should be enough */

/**
 * A hidden region of iPXE
 *
 * This represents a region that will be edited out of the system's
 * memory map.
 *
 * This structure is accessed by assembly code, so must not be
 * changed.
 */
struct hidden_region {
	/** Physical start address */
	uint64_t start;
	/** Physical end address */
	uint64_t end;
};

/** Hidden base memory */
extern struct hidden_region __data16 ( hidemem_base );
#define hidemem_base __use_data16 ( hidemem_base )

/** Hidden umalloc memory */
extern struct hidden_region __data16 ( hidemem_umalloc );
#define hidemem_umalloc __use_data16 ( hidemem_umalloc )

/** Hidden text memory */
extern struct hidden_region __data16 ( hidemem_textdata );
#define hidemem_textdata __use_data16 ( hidemem_textdata )

/** Assembly routine in e820mangler.S */
extern void int15();

/** Vector for storing original INT 15 handler */
extern struct segoff __text16 ( int15_vector );
#define int15_vector __use_text16 ( int15_vector )

/* The linker defines these symbols for us */
extern char _textdata[];
extern char _etextdata[];
extern char _text16_memsz[];
#define _text16_memsz ( ( size_t ) _text16_memsz )
extern char _data16_memsz[];
#define _data16_memsz ( ( size_t ) _data16_memsz )

/**
 * Hide region of memory from system memory map
 *
 * @v region		Hidden memory region
 * @v start		Start of region
 * @v end		End of region
 */
static void hide_region ( struct hidden_region *region,
			  physaddr_t start, physaddr_t end ) {

	/* Some operating systems get a nasty shock if a region of the
	 * E820 map seems to start on a non-page boundary.  Make life
	 * safer by rounding out our edited region.
	 */
	region->start = ( start & ~( ALIGN_HIDDEN - 1 ) );
	region->end = ( ( end + ALIGN_HIDDEN - 1 ) & ~( ALIGN_HIDDEN - 1 ) );

	DBG ( "Hiding region [%llx,%llx)\n", region->start, region->end );
}

/**
 * Hide used base memory
 *
 */
void hide_basemem ( void ) {
	/* Hide from the top of free base memory to 640kB.  Don't use
	 * hide_region(), because we don't want this rounded to the
	 * nearest page boundary.
	 */
	hidemem_base.start = ( get_fbms() * 1024 );
}

/**
 * Hide umalloc() region
 *
 */
void hide_umalloc ( physaddr_t start, physaddr_t end ) {
	assert ( end <= virt_to_phys ( _textdata ) );
	hide_region ( &hidemem_umalloc, start, end );
}

/**
 * Hide .text and .data
 *
 */
void hide_textdata ( void ) {
	hide_region ( &hidemem_textdata, virt_to_phys ( _textdata ),
		      virt_to_phys ( _etextdata ) );
}

/**
 * Hide Etherboot
 *
 * Installs an INT 15 handler to edit Etherboot out of the memory map
 * returned by the BIOS.
 */
static void hide_etherboot ( void ) {
	struct memory_map memmap;
	unsigned int rm_ds_top;
	unsigned int rm_cs_top;
	unsigned int fbms;

	/* Dump memory map before mangling */
	DBG ( "Hiding iPXE from system memory map\n" );
	get_memmap ( &memmap );

	/* Hook in fake E820 map, if we're testing one */
	if ( FAKE_E820 ) {
		DBG ( "Hooking in fake E820 map\n" );
		fake_e820();
		get_memmap ( &memmap );
	}

	/* Initialise the hidden regions */
	hide_basemem();
	hide_umalloc ( virt_to_phys ( _textdata ), virt_to_phys ( _textdata ) );
	hide_textdata();

	/* Some really moronic BIOSes bring up the PXE stack via the
	 * UNDI loader entry point and then don't bother to unload it
	 * before overwriting the code and data segments.  If this
	 * happens, we really don't want to leave INT 15 hooked,
	 * because that will cause any loaded OS to die horribly as
	 * soon as it attempts to fetch the system memory map.
	 *
	 * We use a heuristic to guess whether or not we are being
	 * loaded sensibly.
	 */
	rm_cs_top = ( ( ( rm_cs << 4 ) + _text16_memsz + 1024 - 1 ) >> 10 );
	rm_ds_top = ( ( ( rm_ds << 4 ) + _data16_memsz + 1024 - 1 ) >> 10 );
	fbms = get_fbms();
	if ( ( rm_cs_top < fbms ) && ( rm_ds_top < fbms ) ) {
		DBG ( "Detected potentially unsafe UNDI load at CS=%04x "
		      "DS=%04x FBMS=%dkB\n", rm_cs, rm_ds, fbms );
		DBG ( "Disabling INT 15 memory hiding\n" );
		return;
	}

	/* Hook INT 15 */
	hook_bios_interrupt ( 0x15, ( intptr_t ) int15, &int15_vector );

	/* Dump memory map after mangling */
	DBG ( "Hidden iPXE from system memory map\n" );
	get_memmap ( &memmap );
}

/**
 * Unhide Etherboot
 *
 * Uninstalls the INT 15 handler installed by hide_etherboot(), if
 * possible.
 */
static void unhide_etherboot ( int flags __unused ) {
	struct memory_map memmap;
	int rc;

	/* If we have more than one hooked interrupt at this point, it
	 * means that some other vector is still hooked, in which case
	 * we can't safely unhook INT 15 because we need to keep our
	 * memory protected.  (We expect there to be at least one
	 * hooked interrupt, because INT 15 itself is still hooked).
	 */
	if ( hooked_bios_interrupts > 1 ) {
		DBG ( "Cannot unhide: %d interrupt vectors still hooked\n",
		      hooked_bios_interrupts );
		return;
	}

	/* Try to unhook INT 15 */
	if ( ( rc = unhook_bios_interrupt ( 0x15, ( intptr_t ) int15,
					    &int15_vector ) ) != 0 ) {
		DBG ( "Cannot unhook INT15: %s\n", strerror ( rc ) );
		/* Leave it hooked; there's nothing else we can do,
		 * and it should be intrinsically safe (though
		 * wasteful of RAM).
		 */
	}

	/* Unhook fake E820 map, if used */
	if ( FAKE_E820 )
		unfake_e820();

	/* Dump memory map after unhiding */
	DBG ( "Unhidden iPXE from system memory map\n" );
	get_memmap ( &memmap );
}

/** Hide Etherboot startup function */
struct startup_fn hide_etherboot_startup_fn __startup_fn ( STARTUP_EARLY ) = {
	.name = "hidemem",
	.startup = hide_etherboot,
	.shutdown = unhide_etherboot,
};
