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
 * EFI relocations
 *
 * Derived from iPXE's elf2efi.c
 *
 */

#define PACKAGE "wimboot"
#define PACKAGE_VERSION VERSION
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <getopt.h>
#include <bfd.h>
#include "efi.h"
#include "efi/IndustryStandard/PeImage.h"
#include "wimboot.h"

#define eprintf(...) fprintf ( stderr, __VA_ARGS__ )

/* Maintain compatibility with binutils 2.34 */
#ifndef bfd_get_section_vma
#define bfd_get_section_vma(bfd, ptr) bfd_section_vma(ptr)
#endif
#ifndef bfd_get_section_flags
#define bfd_get_section_flags(bfd, ptr) bfd_section_flags(ptr)
#endif

/** PE header maximum length
 *
 * This maximum length is guaranteed by the fact that the PE headers
 * have to fit entirely before the start of the bzImage header.
 */
#define PE_HEADER_LEN 512

/** .reloc section index */
#define RELOC_SECTION_INDEX 3

/** PE relocations */
struct pe_relocs {
	struct pe_relocs *next;
	unsigned long start_rva;
	unsigned int used_relocs;
	unsigned int total_relocs;
	uint16_t *relocs;
};

/** Command-line options */
struct options {
	/** Verbosity */
	int verbosity;
};

/**
 * Allocate memory
 *
 * @v len		Length of memory to allocate
 * @ret ptr		Pointer to allocated memory
 */
static void * xmalloc ( size_t len ) {
	void *ptr;

	ptr = malloc ( len );
	if ( ! ptr ) {
		eprintf ( "Could not allocate %zd bytes\n", len );
		exit ( 1 );
	}

	return ptr;
}

/**
 * Write to file
 *
 * @v fd		File descriptor
 * @v data		Data
 * @v len		Length of data
 */
static void xwrite ( int fd, const void *data, size_t len ) {
	ssize_t written;

	written = write ( fd, data, len );
	if ( written < 0 ) {
		eprintf ( "Could not write %zd bytes: %s\n",
			  len, strerror ( errno ) );
		exit ( 1 );
	}
	if ( ( size_t ) written != len ) {
		eprintf ( "Wrote only %zd of %zd bytes\n", written, len );
		exit ( 1 );
	}
}

/**
 * Seek to file position
 *
 * @v fd		File descriptor
 * @v offset		Offset
 * @v whence		Origin
 */
static void xlseek ( int fd, off_t offset, int whence ) {
	off_t pos;

	pos = lseek ( fd, offset, whence );
	if ( pos < 0 ) {
		eprintf ( "Could not seek: %s\n", strerror ( errno ) );
		exit ( 1 );
	}
}

/**
 * Close file
 *
 * @v fd		File descriptor
 */
static void xclose ( int fd ) {

	if ( close ( fd ) < 0 ) {
		eprintf ( "Could not close: %s\n", strerror ( errno ) );
		exit ( 1 );
	}
}

/**
 * Open input BFD file
 *
 * @v filename		File name
 * @ret ibfd		BFD file
 */
static bfd * open_input_bfd ( const char *filename ) {
	bfd *bfd;

	/* Open the file */
	bfd = bfd_openr ( filename, NULL );
	if ( ! bfd ) {
		eprintf ( "Cannot open %s: ", filename );
		bfd_perror ( NULL );
		exit ( 1 );
	}

	/* The call to bfd_check_format() must be present, otherwise
	 * we get a segfault from later BFD calls.
	 */
	if ( ! bfd_check_format ( bfd, bfd_object ) ) {
		eprintf ( "%s is not an object file: ", filename );
		bfd_perror ( NULL );
		exit ( 1 );
	}

	return bfd;
}

/**
 * Read symbol table
 *
 * @v bfd		BFD file
 */
static asymbol ** read_symtab ( bfd *bfd ) {
	long symtab_size;
	asymbol **symtab;
	long symcount;

	/* Get symbol table size */
	symtab_size = bfd_get_symtab_upper_bound ( bfd );
	if ( symtab_size < 0 ) {
		bfd_perror ( "Could not get symbol table upper bound" );
		exit ( 1 );
	}

	/* Allocate and read symbol table */
	symtab = xmalloc ( symtab_size );
	symcount = bfd_canonicalize_symtab ( bfd, symtab );
	if ( symcount < 0 ) {
		bfd_perror ( "Cannot read symbol table" );
		exit ( 1 );
	}

	return symtab;
}

/**
 * Read relocation table
 *
 * @v bfd		BFD file
 * @v symtab		Symbol table
 * @v section		Section
 * @v symtab		Symbol table
 * @ret reltab		Relocation table
 */
static arelent ** read_reltab ( bfd *bfd, asymbol **symtab,
				asection *section ) {
	long reltab_size;
	arelent **reltab;
	long numrels;

	/* Get relocation table size */
	reltab_size = bfd_get_reloc_upper_bound ( bfd, section );
	if ( reltab_size < 0 ) {
		bfd_perror ( "Could not get relocation table upper bound" );
		exit ( 1 );
	}

	/* Allocate and read relocation table */
	reltab = xmalloc ( reltab_size );
	numrels = bfd_canonicalize_reloc ( bfd, section, reltab, symtab );
	if ( numrels < 0 ) {
		bfd_perror ( "Cannot read relocation table" );
		exit ( 1 );
	}

	return reltab;
}

/**
 * Generate entry in PE relocation table
 *
 * @v pe_reltab		PE relocation table
 * @v rva		RVA
 * @v size		Size of relocation entry
 */
static void generate_pe_reloc ( struct pe_relocs **pe_reltab,
				unsigned long rva, size_t size ) {
	unsigned long start_rva;
	uint16_t reloc;
	struct pe_relocs *pe_rel;
	uint16_t *relocs;

	/* Construct */
	start_rva = ( rva & ~0xfff );
	reloc = ( rva & 0xfff );
	switch ( size ) {
	case 8:
		reloc |= 0xa000;
		break;
	case 4:
		reloc |= 0x3000;
		break;
	case 2:
		reloc |= 0x2000;
		break;
	default:
		eprintf ( "Unsupported relocation size %zd\n", size );
		exit ( 1 );
	}

	/* Locate or create PE relocation table */
	for ( pe_rel = *pe_reltab ; pe_rel ; pe_rel = pe_rel->next ) {
		if ( pe_rel->start_rva == start_rva )
			break;
	}
	if ( ! pe_rel ) {
		pe_rel = xmalloc ( sizeof ( *pe_rel ) );
		memset ( pe_rel, 0, sizeof ( *pe_rel ) );
		pe_rel->next = *pe_reltab;
		*pe_reltab = pe_rel;
		pe_rel->start_rva = start_rva;
	}

	/* Expand relocation list if necessary */
	if ( pe_rel->used_relocs < pe_rel->total_relocs ) {
		relocs = pe_rel->relocs;
	} else {
		pe_rel->total_relocs = ( pe_rel->total_relocs ?
					 ( pe_rel->total_relocs * 2 ) : 256 );
		relocs = xmalloc ( pe_rel->total_relocs *
				   sizeof ( pe_rel->relocs[0] ) );
		memset ( relocs, 0,
			 pe_rel->total_relocs * sizeof ( pe_rel->relocs[0] ) );
		memcpy ( relocs, pe_rel->relocs,
			 pe_rel->used_relocs * sizeof ( pe_rel->relocs[0] ) );
		free ( pe_rel->relocs );
		pe_rel->relocs = relocs;
	}

	/* Store relocation */
	pe_rel->relocs[ pe_rel->used_relocs++ ] = reloc;
}

/**
 * Process relocation record
 *
 * @v bfd		BFD file
 * @v section		Section
 * @v rel		Relocation entry
 * @v pe_reltab		PE relocation table to fill in
 */
static void process_reloc ( bfd *bfd __unused, asection *section, arelent *rel,
			    struct pe_relocs **pe_reltab ) {
	reloc_howto_type *howto = rel->howto;
	asymbol *sym = *(rel->sym_ptr_ptr);
	unsigned long offset = ( bfd_get_section_vma ( bfd, section ) +
				 rel->address - BASE_ADDRESS );

	if ( bfd_is_abs_section ( sym->section ) ) {
		/* Skip absolute symbols; the symbol value won't
		 * change when the object is loaded.
		 */
	} else if ( strcmp ( howto->name, "R_X86_64_64" ) == 0 ) {
		/* Generate an 8-byte PE relocation */
		generate_pe_reloc ( pe_reltab, offset, 8 );
	} else if ( ( strcmp ( howto->name, "R_386_32" ) == 0 ) ||
		    ( strcmp ( howto->name, "R_X86_64_32" ) == 0 ) ||
		    ( strcmp ( howto->name, "R_X86_64_32S" ) == 0 ) ) {
		/* Generate a 4-byte PE relocation */
		generate_pe_reloc ( pe_reltab, offset, 4 );
	} else if ( ( strcmp ( howto->name, "R_386_16" ) == 0 ) ||
		    ( strcmp ( howto->name, "R_X86_64_16" ) == 0 ) ) {
		/* Generate a 2-byte PE relocation */
		generate_pe_reloc ( pe_reltab, offset, 2 );
	} else if ( ( strcmp ( howto->name, "R_386_PC32" ) == 0 ) ||
		    ( strcmp ( howto->name, "R_X86_64_PC32" ) == 0 ) ||
		    ( strcmp ( howto->name, "R_X86_64_PLT32" ) == 0 ) ) {
		/* Skip PC-relative relocations; all relative offsets
		 * remain unaltered when the object is loaded.
		 */
	} else {
		eprintf ( "Unrecognised relocation type %s\n", howto->name );
		exit ( 1 );
	}
}

/**
 * Calculate size of binary PE relocation table
 *
 * @v fh		File handle
 * @v pe_reltab		PE relocation table
 * @ret size		Size of binary table
 */
static size_t output_pe_reltab ( int fd, struct pe_relocs *pe_reltab ) {
	EFI_IMAGE_BASE_RELOCATION header;
	struct pe_relocs *pe_rel;
	static uint8_t pad[16];
	unsigned int num_relocs;
	size_t size;
	size_t pad_size;
	size_t total_size = 0;

	for ( pe_rel = pe_reltab ; pe_rel ; pe_rel = pe_rel->next ) {
		num_relocs = ( ( pe_rel->used_relocs + 1 ) & ~1 );
		size = ( sizeof ( header ) +
			 ( num_relocs * sizeof ( uint16_t ) ) );
		pad_size = ( ( -size ) & ( sizeof ( pad ) - 1 ) );
		size += pad_size;
		header.VirtualAddress = pe_rel->start_rva;
		header.SizeOfBlock = size;
		xwrite ( fd, &header, sizeof ( header ) );
		xwrite ( fd, pe_rel->relocs,
			 ( num_relocs * sizeof ( uint16_t ) ) );
		xwrite ( fd, pad, pad_size );
		total_size += size;
	}

	return total_size;
}

/**
 * Add relocation information
 *
 * @v elf_name		ELF file name
 * @v pe_name		PE file name
 */
static void efireloc ( const char *elf_name, const char *pe_name ) {
	struct pe_relocs *pe_reltab = NULL;
	int fd;
	EFI_IMAGE_DOS_HEADER *dos;
	EFI_IMAGE_OPTIONAL_HEADER_UNION *nt;
	EFI_IMAGE_DATA_DIRECTORY *data_dir;
	EFI_IMAGE_SECTION_HEADER *pe_sections;
	UINT32 *image_size;
	bfd *bfd;
	asymbol **symtab;
	asection *section;
	arelent **reltab;
	arelent **rel;
	size_t reloc_len;

	/* Open the output file */
	fd = open ( pe_name, O_RDWR );
	if ( fd < 0 ) {
		eprintf ( "Could not open %s: %s\n",
			  pe_name, strerror ( errno ) );
		exit ( 1 );
	}

	/* Map the output file header */
	dos = mmap ( NULL, PE_HEADER_LEN, ( PROT_READ | PROT_WRITE ),
		     MAP_SHARED, fd, 0 );
	if ( ! dos ) {
		eprintf ( "Could not mmap %s: %s\n",
			  pe_name, strerror ( errno ) );
		exit ( 1 );
	}

	/* Locate the modifiable fields within the output file header */
	nt = ( ( ( void * ) dos ) + dos->e_lfanew );
	if ( nt->Pe32.FileHeader.Machine == EFI_IMAGE_MACHINE_IA32 ) {
		image_size = &nt->Pe32.OptionalHeader.SizeOfImage;
		data_dir = nt->Pe32.OptionalHeader.DataDirectory;
		pe_sections = ( ( ( void * ) nt ) + sizeof ( nt->Pe32 ) );
	} else if ( nt->Pe32Plus.FileHeader.Machine == EFI_IMAGE_MACHINE_X64 ) {
		image_size = &nt->Pe32Plus.OptionalHeader.SizeOfImage;
		data_dir = nt->Pe32Plus.OptionalHeader.DataDirectory;
		pe_sections = ( ( ( void * ) nt ) + sizeof ( nt->Pe32Plus ) );
	} else {
		eprintf ( "Unrecognised machine type\n" );
		exit ( 1 );
	}

	/* Open the input file */
	bfd = open_input_bfd ( elf_name );
	symtab = read_symtab ( bfd );

	/* For each input section, create the appropriate relocation records */
	for ( section = bfd->sections ; section ; section = section->next ) {
		/* Discard non-allocatable sections */
		if ( ! ( bfd_get_section_flags ( bfd, section ) & SEC_ALLOC ) )
			continue;
		/* Add relocations from this section */
		reltab = read_reltab ( bfd, symtab, section );
		for ( rel = reltab ; *rel ; rel++ )
			process_reloc ( bfd, section, *rel, &pe_reltab );
		free ( reltab );
	}

	/* Close input file */
	bfd_close ( bfd );

	/* Generate relocation section */
	xlseek ( fd, 0, SEEK_END );
	reloc_len = output_pe_reltab ( fd, pe_reltab );

	/* Modify image header */
	*image_size += reloc_len;
	data_dir[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC].Size = reloc_len;
	pe_sections[RELOC_SECTION_INDEX].Misc.VirtualSize = reloc_len;
	pe_sections[RELOC_SECTION_INDEX].SizeOfRawData = reloc_len;

	/* Unmap output file header */
	munmap ( dos, PE_HEADER_LEN );

	/* Close output file */
	xclose ( fd );
}

/**
 * Print help
 *
 * @v program_name	Program name
 */
static void print_help ( const char *program_name ) {
	eprintf ( "Syntax: %s [-v|-q] infile outfile\n", program_name );
}

/**
 * Parse command-line options
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @v opts		Options structure to populate
 */
static int parse_options ( const int argc, char **argv,
			   struct options *opts ) {
	int c;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{ "help", 0, NULL, 'h' },
			{ "verbose", 0, NULL, 'v' },
			{ "quiet", 0, NULL, 'q' },
			{ 0, 0, 0, 0 }
		};

		if ( ( c = getopt_long ( argc, argv, "hvq",
					 long_options,
					 &option_index ) ) == -1 ) {
			break;
		}

		switch ( c ) {
		case 'v':
			opts->verbosity++;
			break;
		case 'q':
			if ( opts->verbosity )
				opts->verbosity--;
			break;
		case 'h':
			print_help ( argv[0] );
			exit ( 0 );
		case '?':
		default:
			exit ( 2 );
		}
	}
	return optind;
}

/**
 * Main program
 *
 * @v argc		Number of arguments
 * @v argv		Command-line arguments
 * @ret rc		Return status code
 */
int main ( int argc, char **argv ) {
	struct options opts = {
		.verbosity = 0,
	};
	int infile_index;
	const char *infile;
	const char *outfile;

	/* Initialise libbfd */
	bfd_init();

	/* Parse command-line arguments */
	infile_index = parse_options ( argc, argv, &opts );
	if ( argc != ( infile_index + 2 ) ) {
		print_help ( argv[0] );
		exit ( 2 );
	}
	infile = argv[infile_index];
	outfile = argv[infile_index + 1];

	/* Add relocation information */
	efireloc ( infile, outfile );

	return 0;
}
