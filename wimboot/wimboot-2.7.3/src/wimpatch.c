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
 * WIM dynamic patching
 *
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include "wimboot.h"
#include "cmdline.h"
#include "vdisk.h"
#include "sha1.h"
#include "wim.h"
#include "wimpatch.h"

/** Directory into which files are injected */
#define WIM_INJECT_DIR "\\Windows\\System32"

struct wim_patch;

/** A region of a patched WIM file */
struct wim_patch_region {
	/** Name */
	const char *name;
	/** Opaque token */
	void *opaque;
	/** Starting offset of region */
	size_t offset;
	/** Length of region */
	size_t len;
	/** Patch region
	 *
	 * @v patch		WIM patch
	 * @v region		Patch region
	 * @v data		Data buffer
	 * @v offset		Relative offset
	 * @v len		Length
	 * @ret rc		Return status code
	 */
	int ( * patch ) ( struct wim_patch *patch,
			  struct wim_patch_region *region,
			  void *data, size_t offset, size_t len );
};

/** Regions of a patched WIM directory containing injected files */
struct wim_patch_dir_regions {
	/** Subdirectory offset within parent entry */
	struct wim_patch_region subdir;
	/** Copy of original directory entries */
	struct wim_patch_region copy;
	/** Injected file directory entries */
	struct wim_patch_region file[VDISK_MAX_FILES];
} __attribute__ (( packed ));

/** Regions of a patched WIM file */
union wim_patch_regions {
	/** Structured list of regions */
	struct {
		/** WIM header */
		struct wim_patch_region header;
		/** Injected file contents */
		struct wim_patch_region file[VDISK_MAX_FILES];
		/** Injected lookup table */
		struct {
			/** Uncompressed copy of original lookup table */
			struct wim_patch_region copy;
			/** Injected boot image metadata lookup table entry */
			struct wim_patch_region boot;
			/** Injected file lookup table entries */
			struct wim_patch_region file[VDISK_MAX_FILES];
		} __attribute__ (( packed )) lookup;
		/** Injected boot image metadata */
		struct {
			/** Uncompressed copy of original metadata */
			struct wim_patch_region copy;
			/** Patched directory containing injected files */
			struct wim_patch_dir_regions dir;
		} __attribute__ (( packed )) boot;
	} __attribute__ (( packed ));
	/** Unstructured list of regions */
	struct wim_patch_region region[0];
};

/** An injected directory entry */
struct wim_patch_dir_entry {
	/** Directory entry */
	struct wim_directory_entry dir;
	/** Name */
	wchar_t name[ VDISK_NAME_LEN + 1 /* wNUL */ ];
} __attribute__ (( packed ));

/** A directory containing injected files */
struct wim_patch_dir {
	/** Name */
	const char *name;
	/** Offset to parent directory entry */
	size_t parent;
	/** Offset to original directory entries */
	size_t offset;
	/** Length of original directory entries (excluding terminator) */
	size_t len;
	/** Offset to modified directory entries */
	size_t subdir;
};

/** A patched WIM file */
struct wim_patch {
	/** Virtual file */
	struct vdisk_file *file;
	/** Patched WIM header */
	struct wim_header header;
	/** Original lookup table */
	struct wim_resource_header lookup;
	/** Original boot image metadata */
	struct wim_resource_header boot;
	/** Original boot index */
	uint32_t boot_index;
	/** Directory containing injected files */
	struct wim_patch_dir dir;
	/** Patched regions */
	union wim_patch_regions regions;
};

/**
 * Align WIM offset to nearest qword
 *
 * @v len		Length
 * @ret len		Aligned length
 */
static size_t wim_align ( size_t len ) {
	return ( ( len + 0x07 ) & ~0x07 );
}

/**
 * Calculate WIM hash
 *
 * @v vfile		Virtual file
 * @v hash		Hash to fill in
 */
static void wim_hash ( struct vdisk_file *vfile, struct wim_hash *hash ) {
	uint8_t ctx[SHA1_CTX_SIZE];
	uint8_t buf[512];
	size_t offset;
	size_t len;

	/* Calculate SHA-1 digest */
	sha1_init ( ctx );
	for ( offset = 0 ; offset < vfile->len ; offset += len ) {

		/* Read block */
		len = ( vfile->len - offset );
		if ( len > sizeof ( buf ) )
			len = sizeof ( buf );
		vfile->read ( vfile, buf, offset, len );

		/* Update digest */
		sha1_update ( ctx, buf, len );
	}
	sha1_final ( ctx, hash->sha1 );
}

/**
 * Determine whether or not to inject file
 *
 * @v vfile		Virtual file
 * @ret inject		Inject this file
 */
static int wim_inject_file ( struct vdisk_file *vfile ) {
	size_t name_len;
	const char *ext;

	/* Ignore non-existent files */
	if ( ! vfile->read )
		return 0;

	/* Ignore wimboot itself */
	if ( strcasecmp ( vfile->name, "wimboot" ) == 0 )
		return 0;

	/* Ignore bootmgr files */
	if ( strcasecmp ( vfile->name, "bootmgr" ) == 0 )
		return 0;
	if ( strcasecmp ( vfile->name, "bootmgr.exe" ) == 0 )
		return 0;

	/* Ignore BCD files */
	if ( strcasecmp ( vfile->name, "BCD" ) == 0 )
		return 0;

	/* Locate file extension */
	name_len = strlen ( vfile->name );
	ext = ( ( name_len > 4 ) ? ( vfile->name + name_len - 4 ) : "" );

	/* Ignore .wim files */
	if ( strcasecmp ( ext, ".wim" ) == 0 )
		return 0;

	/* Ignore .sdi files */
	if ( strcasecmp ( ext, ".sdi" ) == 0 )
		return 0;

	/* Ignore .efi files */
	if ( strcasecmp ( ext, ".efi" ) == 0 )
		return 0;

	/* Ignore .ttf files */
	if ( strcasecmp ( ext, ".ttf" ) == 0 )
		return 0;

	return 1;
}

/**
 * Patch WIM header
 *
 * @v patch		WIM patch
 * @v region		Patch region
 * @v data		Data buffer
 * @v offset		Relative offset
 * @v len		Length
 * @ret rc		Return status code
 */
static int wim_patch_header ( struct wim_patch *patch,
			      struct wim_patch_region *region,
			      void *data, size_t offset, size_t len ) {
	struct wim_header *header = &patch->header;

	/* Sanity checks */
	assert ( offset < sizeof ( *header ) );
	assert ( len <= ( sizeof ( *header ) - offset ) );

	/* Copy patched header */
	if ( patch->lookup.offset != patch->header.lookup.offset ) {
		DBG2 ( "...patched WIM %s lookup table %#llx->%#llx\n",
		       region->name, patch->lookup.offset,
		       patch->header.lookup.offset );
	}
	if ( patch->boot.offset != patch->header.boot.offset ) {
		DBG2 ( "...patched WIM %s boot metadata %#llx->%#llx\n",
		       region->name, patch->boot.offset,
		       patch->header.boot.offset );
	}
	if ( patch->boot_index != patch->header.boot_index ) {
		DBG2 ( "...patched WIM %s boot index %d->%d\n", region->name,
		       patch->boot_index, patch->header.boot_index );
	}
	memcpy ( data, ( ( ( void * ) &patch->header ) + offset ), len );

	return 0;
}

/**
 * Patch injected file content
 *
 * @v patch		WIM patch
 * @v region		Patch region
 * @v data		Data buffer
 * @v offset		Relative offset
 * @v len		Length
 * @ret rc		Return status code
 */
static int wim_patch_file ( struct wim_patch *patch __unused,
			    struct wim_patch_region *region,
			    void *data, size_t offset, size_t len ) {
	struct vdisk_file *vfile = region->opaque;

	/* Read from file */
	vfile->read ( vfile, data, offset, len );

	return 0;
}

/**
 * Patch uncompressed copy of original lookup table
 *
 * @v patch		WIM patch
 * @v region		Patch region
 * @v data		Data buffer
 * @v offset		Relative offset
 * @v len		Length
 * @ret rc		Return status code
 */
static int wim_patch_lookup_copy ( struct wim_patch *patch,
				   struct wim_patch_region *region __unused,
				   void *data, size_t offset, size_t len ) {
	int rc;

	/* Read original lookup table */
	if ( ( rc = wim_read ( patch->file, &patch->header, &patch->lookup,
			       data, offset, len ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Patch injected boot image metadata lookup table entry
 *
 * @v patch		WIM patch
 * @v region		Patch region
 * @v data		Data buffer
 * @v offset		Relative offset
 * @v len		Length
 * @ret rc		Return status code
 */
static int wim_patch_lookup_boot ( struct wim_patch *patch,
				   struct wim_patch_region *region __unused,
				   void *data, size_t offset, size_t len ) {
	struct wim_lookup_entry entry;

	/* Sanity checks */
	assert ( offset < sizeof ( entry ) );
	assert ( len <= ( sizeof ( entry ) - offset ) );

	/* Construct lookup table entry */
	memset ( &entry, 0, sizeof ( entry ) );
	memcpy ( &entry.resource, &patch->header.boot,
		 sizeof ( entry.resource ) );

	/* Copy lookup table entry */
	memcpy ( data, ( ( ( void * ) &entry ) + offset ), len );

	return 0;
}

/**
 * Patch injected file lookup table entry
 *
 * @v patch		WIM patch
 * @v region		Patch region
 * @v data		Data buffer
 * @v offset		Relative offset
 * @v len		Length
 * @ret rc		Return status code
 */
static int wim_patch_lookup_file ( struct wim_patch *patch __unused,
				   struct wim_patch_region *region,
				   void *data, size_t offset, size_t len ) {
	struct wim_patch_region *rfile = region->opaque;
	struct vdisk_file *vfile = rfile->opaque;
	struct wim_lookup_entry entry;

	/* Sanity checks */
	assert ( offset < sizeof ( entry ) );
	assert ( len <= ( sizeof ( entry ) - offset ) );

	/* Construct lookup table entry */
	memset ( &entry, 0, sizeof ( entry ) );
	entry.resource.offset = rfile->offset;
	entry.resource.len = vfile->len;
	entry.resource.zlen__flags = entry.resource.len;
	entry.refcnt = 1;
	wim_hash ( vfile, &entry.hash );

	/* Copy lookup table entry */
	memcpy ( data, ( ( ( void * ) &entry ) + offset ), len );
	DBG2 ( "...patched WIM %s %s\n", region->name, vfile->name );

	return 0;
}

/**
 * Patch uncompressed copy of original boot metadata
 *
 * @v patch		WIM patch
 * @v region		Patch region
 * @v data		Data buffer
 * @v offset		Relative offset
 * @v len		Length
 * @ret rc		Return status code
 */
static int wim_patch_boot_copy ( struct wim_patch *patch,
				 struct wim_patch_region *region __unused,
				 void *data, size_t offset, size_t len ) {
	int rc;

	/* Read original boot metadata */
	if ( ( rc = wim_read ( patch->file, &patch->header, &patch->boot,
			       data, offset, len ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Patch subdirectory offset within parent directory entry
 *
 * @v patch		WIM patch
 * @v region		Patch region
 * @v data		Data buffer
 * @v offset		Relative offset
 * @v len		Length
 * @ret rc		Return status code
 */
static int wim_patch_dir_subdir ( struct wim_patch *patch,
				  struct wim_patch_region *region,
				  void *data, size_t offset, size_t len ) {
	struct wim_patch_dir *dir = region->opaque;
	uint64_t subdir = dir->subdir;

	/* Sanity checks */
	assert ( offset < sizeof ( subdir ) );
	assert ( len <= ( sizeof ( subdir ) - offset ) );

	/* Copy subdirectory offset */
	memcpy ( data, ( ( ( void * ) &subdir ) + offset ), len );
	DBG2 ( "...patched WIM %s %s %#llx\n", region->name, dir->name,
	       ( patch->header.boot.offset + subdir ) );

	return 0;
}

/**
 * Patch copy of original directory entries
 *
 * @v patch		WIM patch
 * @v region		Patch region
 * @v data		Data buffer
 * @v offset		Relative offset
 * @v len		Length
 * @ret rc		Return status code
 */
static int wim_patch_dir_copy ( struct wim_patch *patch,
				struct wim_patch_region *region,
				void *data, size_t offset, size_t len ) {
	struct wim_patch_dir *dir = region->opaque;
	int rc;

	/* Read portion of original boot metadata */
	if ( ( rc = wim_read ( patch->file, &patch->header, &patch->boot,
			       data, ( dir->offset + offset ), len ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Patch injected directory entries
 *
 * @v patch		WIM patch
 * @v region		Patch region
 * @v data		Data buffer
 * @v offset		Relative offset
 * @v len		Length
 * @ret rc		Return status code
 */
static int wim_patch_dir_file ( struct wim_patch *patch __unused,
				struct wim_patch_region *region,
				void *data, size_t offset, size_t len ) {
	struct wim_patch_region *rfile = region->opaque;
	struct vdisk_file *vfile = rfile->opaque;
	struct wim_patch_dir_entry entry;
	size_t name_len = strlen ( vfile->name );
	unsigned int i;

	/* Sanity checks */
	assert ( offset < sizeof ( entry ) );
	assert ( len <= ( sizeof ( entry ) - offset ) );

	/* Construct directory entry */
	memset ( &entry, 0, sizeof ( entry ) );
	entry.dir.len = wim_align ( sizeof ( entry ) );
	entry.dir.attributes = WIM_ATTR_NORMAL;
	entry.dir.security = WIM_NO_SECURITY;
	entry.dir.created = WIM_MAGIC_TIME;
	entry.dir.accessed = WIM_MAGIC_TIME;
	entry.dir.written = WIM_MAGIC_TIME;
	wim_hash ( vfile, &entry.dir.hash );
	entry.dir.name_len = ( name_len * sizeof ( entry.name[0] ) );
	for ( i = 0 ; i < name_len ; i++ )
		entry.name[i] = vfile->name[i];

	/* Copy directory entry */
	memcpy ( data, ( ( ( void * ) &entry ) + offset ), len );
	DBG2 ( "...patched WIM %s %s\n", region->name, vfile->name );

	return 0;
}

/**
 * Patch WIM region
 *
 * @v patch		WIM patch
 * @v region		Patch region
 * @v data		Data buffer
 * @v offset		Relative offset
 * @v len		Length
 * @ret rc		Return status code
 */
static int wim_patch_region ( struct wim_patch *patch,
			      struct wim_patch_region *region,
			      void *data, size_t offset, size_t len ) {
	size_t skip;
	int rc;

	/* Skip unused regions */
	if ( ! region->patch )
		return 0;

	/* Skip any data before this region */
	skip = ( ( region->offset > offset ) ?
		 ( region->offset - offset ) : 0 );
	if ( skip >= len )
		return 0;
	data += skip;
	offset += skip;
	len -= skip;

	/* Convert to relative offset within this region */
	offset -= region->offset;

	/* Skip any data after this region */
	if ( offset >= region->len )
		return 0;
	if ( len > ( region->len - offset ) )
		len = ( region->len - offset );

	/* Patch this region */
	if ( ( rc = region->patch ( patch, region, data, offset, len ) ) != 0 )
		return rc;
	DBG2 ( "...patched WIM %s at [%#zx,%#zx)\n", region->name,
	       ( region->offset + offset ), ( region->offset + offset + len ) );

	return 0;
}

/**
 * Construct patched WIM region
 *
 * @v region		Patched region to fill in
 * @v name		Name
 * @v opaque		Opaque data
 * @v offset		Offset
 * @v len		Length
 * @v patch		Patch method
 * @ret offset		Next offset
 */
static inline __attribute__ (( always_inline )) size_t
wim_construct_region ( struct wim_patch_region *region, const char *name,
		       void *opaque, size_t offset, size_t len,
		       int ( * patch ) ( struct wim_patch *patch,
					 struct wim_patch_region *region,
					 void *data, size_t offset,
					 size_t len ) ) {

	DBG ( "...patching WIM %s at [%#zx,%#zx)\n",
	      name, offset, ( offset + len ) );
	region->name = name;
	region->opaque = opaque;
	region->offset = offset;
	region->len = len;
	region->patch = patch;
	return ( offset + len );
}

/**
 * Construct patch WIM directory regions
 *
 * @v patch		WIM patch
 * @v dir		Patched directory
 * @v offset		Offset
 * @v regions		Patched directory regions to fill in
 * @ret offset		Next offset
 */
static size_t wim_construct_dir ( struct wim_patch *patch,
				  struct wim_patch_dir *dir, size_t offset,
				  struct wim_patch_dir_regions *regions ) {
	struct wim_patch_dir_entry *entry;
	struct wim_patch_region *rfile;
	size_t boot_offset = patch->header.boot.offset;
	unsigned int i;

	DBG ( "...patching WIM directory at %#zx from [%#zx,%#zx)\n",
	      ( boot_offset + dir->parent ), ( boot_offset + dir->offset ),
	      ( boot_offset + dir->offset + dir->len ) );

	/* Align directory entries */
	offset = wim_align ( offset );
	dir->subdir = ( offset - patch->header.boot.offset );

	/* Construct injected file directory entries */
	for ( i = 0 ; i < VDISK_MAX_FILES ; i++ ) {
		rfile = &patch->regions.file[i];
		if ( ! rfile->patch )
			continue;
		offset = wim_construct_region ( &regions->file[i], "dir.file",
						rfile, offset,
						sizeof ( *entry ),
						wim_patch_dir_file );
		offset = wim_align ( offset );
	}

	/* Construct copy of original directory entries */
	offset = wim_construct_region ( &regions->copy, dir->name, dir, offset,
					dir->len, wim_patch_dir_copy );

	/* Allow space for directory terminator */
	offset += sizeof ( entry->dir.len );

	/* Construct subdirectory offset within parent directory entry */
	wim_construct_region ( &regions->subdir, "dir.subdir", dir,
			       ( boot_offset + dir->parent +
				 offsetof ( typeof ( entry->dir ), subdir ) ),
			       sizeof ( entry->dir.subdir ),
			       wim_patch_dir_subdir );

	return offset;
}

/**
 * Construct WIM patch
 *
 * @v file		Virtual file
 * @v boot_index	New boot index (or zero)
 * @v inject		Inject files into WIM
 * @v patch		Patch to fill in
 * @ret rc		Return status code
 */
static int wim_construct_patch ( struct vdisk_file *file,
				 unsigned int boot_index, int inject,
				 struct wim_patch *patch ) {
	union wim_patch_regions *regions = &patch->regions;
	struct wim_patch_region *rfile;
	struct wim_resource_header *lookup;
	struct wim_resource_header *boot;
	struct wim_directory_entry direntry;
	struct wim_lookup_entry *entry;
	struct vdisk_file *vfile;
	size_t offset;
	unsigned int injected = 0;
	unsigned int i;
	int rc;

	/* Initialise patch */
	memset ( patch, 0, sizeof ( *patch ) );
	patch->file = file;
	DBG ( "...patching WIM %s\n", file->name );

	/* Reset file length */
	file->xlen = file->len;
	offset = file->len;

	/* Read WIM header */
	if ( ( rc = wim_header ( file, &patch->header ) ) != 0 )
		return rc;
	lookup = &patch->header.lookup;
	boot = &patch->header.boot;

	/* Patch header within original image body */
	wim_construct_region ( &regions->header, "header", NULL, 0,
			       sizeof ( patch->header ), wim_patch_header );

	/* Record original lookup table */
	memcpy ( &patch->lookup, lookup, sizeof ( patch->lookup ) );

	/* Record original metadata for selected boot image (which may
	 * not be the originally selected boot image).
	 */
	if ( ( rc = wim_metadata ( file, &patch->header, boot_index,
				   &patch->boot ) ) != 0 )
		return rc;

	/* Record original boot index */
	patch->boot_index = patch->header.boot_index;

	/* Update boot index in patched header, if applicable */
	if ( boot_index )
		patch->header.boot_index = boot_index;

	/* Do nothing more if injection is disabled */
	if ( ! inject )
		return 0;

	/* Construct injected files */
	for ( i = 0 ; i < VDISK_MAX_FILES ; i++ ) {
		vfile = &vdisk_files[i];
		if ( ! wim_inject_file ( vfile ) )
			continue;
		offset = wim_construct_region ( &regions->file[i], vfile->name,
						vfile, offset, vfile->len,
						wim_patch_file );
		injected++;
	}

	/* Do nothing more if no files are injected */
	if ( injected == 0 )
		return 0;

	/* Calculate boot index for injected image */
	if ( ( rc = wim_count ( file, &patch->header, &boot_index ) ) != 0 )
		return rc;
	patch->header.images = ( boot_index + 1 );
	patch->header.boot_index = patch->header.images;

	/* Construct injected lookup table */
	lookup->offset = offset = wim_align ( offset );
	offset = wim_construct_region ( &regions->lookup.copy, "lookup.copy",
					NULL, offset, patch->lookup.len,
					wim_patch_lookup_copy );
	offset = wim_construct_region ( &regions->lookup.boot, "lookup.boot",
					NULL, offset, sizeof ( *entry ),
					wim_patch_lookup_boot );
	for ( i = 0 ; i < VDISK_MAX_FILES ; i++ ) {
		rfile = &regions->file[i];
		if ( ! rfile->patch )
			continue;
		offset = wim_construct_region ( &regions->lookup.file[i],
						"lookup.file", rfile,
						offset, sizeof ( *entry ),
						wim_patch_lookup_file );
	}
	lookup->offset = regions->lookup.copy.offset;
	lookup->len = ( offset - lookup->offset );
	lookup->zlen__flags = lookup->len;

	/* Locate directory containing injected files */
	patch->dir.name = WIM_INJECT_DIR;
	if ( ( rc = wim_path ( file, &patch->header, &patch->boot,
			       L(WIM_INJECT_DIR), &patch->dir.parent,
			       &direntry ) ) != 0 )
		return rc;
	patch->dir.offset = direntry.subdir;
	if ( ( rc = wim_dir_len ( file, &patch->header, &patch->boot,
				  patch->dir.offset, &patch->dir.len ) ) != 0 )
		return rc;

	/* Construct injected boot image metadata */
	boot->offset = offset = wim_align ( offset );
	offset = wim_construct_region ( &regions->boot.copy, "boot.copy",
					NULL, offset, patch->boot.len,
					wim_patch_boot_copy );
	offset = wim_construct_dir ( patch, &patch->dir, offset,
				     &regions->boot.dir );
	boot->len = ( offset - boot->offset );
	boot->zlen__flags = ( boot->len | WIM_RESHDR_METADATA );

	/* Record patched length */
	file->xlen = offset;
	DBG ( "...patching WIM length %#zx->%#zx\n", file->len, file->xlen );

	return 0;
}

/**
 * Patch WIM file
 *
 * @v file		Virtual file
 * @v data		Data buffer
 * @v offset		Offset
 * @v len		Length
 */
void patch_wim ( struct vdisk_file *file, void *data, size_t offset,
		 size_t len ) {
	static struct wim_patch cached_patch;
	struct wim_patch *patch = &cached_patch;
	struct wim_patch_region *region;
	unsigned int boot_index;
	unsigned int i;
	int inject;
	int rc;

	/* Do nothing unless patching is required */
	boot_index = cmdline_index;
	inject = ( ! cmdline_rawwim );
	if ( ( boot_index == 0 ) && ( ! inject ) )
		return;

	/* Update cached patch if required */
	if ( file != patch->file ) {
		if ( ( rc = wim_construct_patch ( file, boot_index, inject,
						  patch ) ) != 0 ) {
			die ( "Could not patch WIM %s\n", file->name );
		}
	}
	patch = &cached_patch;

	/* Patch regions */
	for ( i = 0 ; i < ( sizeof ( patch->regions ) /
			    sizeof ( patch->regions.region[0] ) ) ; i++ ) {
		region = &patch->regions.region[i];
		if ( ( rc = wim_patch_region ( patch, region, data, offset,
					       len ) ) != 0 ) {
			die ( "Could not patch WIM %s %s at [%#zx,%#zx)\n",
			      file->name, region->name, offset,
			      ( offset + len ) );
		}
	}
}
