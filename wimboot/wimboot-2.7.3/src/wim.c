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
 * WIM images
 *
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <assert.h>
#include "wimboot.h"
#include "vdisk.h"
#include "lzx.h"
#include "xca.h"
#include "wim.h"

/** WIM chunk buffer */
static struct wim_chunk_buffer wim_chunk_buffer;

/**
 * Get WIM header
 *
 * @v file		Virtual file
 * @v header		WIM header to fill in
 * @ret rc		Return status code
 */
int wim_header ( struct vdisk_file *file, struct wim_header *header ) {

	/* Sanity check */
	if ( sizeof ( *header ) > file->len ) {
		DBG ( "WIM file too short (%#zx bytes)\n", file->len );
		return -1;
	}

	/* Read WIM header */
	file->read ( file, header, 0, sizeof ( *header ) );

	return 0;
}

/**
 * Get compressed chunk offset
 *
 * @v file		Virtual file
 * @v resource		Resource
 * @v chunk		Chunk number
 * @v offset		Offset to fill in
 * @ret rc		Return status code
 */
static int wim_chunk_offset ( struct vdisk_file *file,
			      struct wim_resource_header *resource,
			      unsigned int chunk, size_t *offset ) {
	size_t zlen = ( resource->zlen__flags & WIM_RESHDR_ZLEN_MASK );
	unsigned int chunks;
	size_t offset_offset;
	size_t offset_len;
	size_t chunks_len;
	union {
		uint32_t offset_32;
		uint64_t offset_64;
	} u;

	/* Special case: zero-length files have no chunks */
	if ( ! resource->len ) {
		*offset = 0;
		return 0;
	}

	/* Calculate chunk parameters */
	chunks = ( ( resource->len + WIM_CHUNK_LEN - 1 ) / WIM_CHUNK_LEN );
	offset_len = ( ( resource->len > 0xffffffffULL ) ?
		       sizeof ( u.offset_64 ) : sizeof ( u.offset_32 ) );
	chunks_len = ( ( chunks - 1 ) * offset_len );

	/* Sanity check */
	if ( chunks_len > zlen ) {
		DBG ( "Resource too short for %d chunks\n", chunks );
		return -1;
	}

	/* Special case: chunk 0 has no offset field */
	if ( ! chunk ) {
		*offset = chunks_len;
		return 0;
	}

	/* Treat out-of-range chunks as being at the end of the
	 * resource, to allow for length calculation on the final
	 * chunk.
	 */
	if ( chunk >= chunks ) {
		*offset = zlen;
		return 0;
	}

	/* Otherwise, read the chunk offset */
	offset_offset = ( ( chunk - 1 ) * offset_len );
	file->read ( file, &u, ( resource->offset + offset_offset ),
		     offset_len );
	*offset = ( chunks_len + ( ( offset_len == sizeof ( u.offset_64 ) ) ?
				   u.offset_64 : u.offset_32 ) );
	if ( *offset > zlen ) {
		DBG ( "Chunk %d offset lies outside resource\n", chunk );
		return -1;
	}
	return 0;
}

/**
 * Read chunk from a compressed resource
 *
 * @v file		Virtual file
 * @v header		WIM header
 * @v resource		Resource
 * @v chunk		Chunk number
 * @v buf		Chunk buffer
 * @ret rc		Return status code
 */
static int wim_chunk ( struct vdisk_file *file, struct wim_header *header,
		       struct wim_resource_header *resource,
		       unsigned int chunk, struct wim_chunk_buffer *buf ) {
	ssize_t ( * decompress ) ( const void *data, size_t len, void *buf );
	unsigned int chunks;
	size_t offset;
	size_t next_offset;
	size_t len;
	size_t expected_out_len;
	ssize_t out_len;
	int rc;

	/* Get chunk compressed data offset and length */
	if ( ( rc = wim_chunk_offset ( file, resource, chunk,
				       &offset ) ) != 0 )
		return rc;
	if ( ( rc = wim_chunk_offset ( file, resource, ( chunk + 1 ),
				       &next_offset ) ) != 0 )
		return rc;
	len = ( next_offset - offset );

	/* Calculate uncompressed length */
	assert ( resource->len > 0 );
	chunks = ( ( resource->len + WIM_CHUNK_LEN - 1 ) / WIM_CHUNK_LEN );
	expected_out_len = WIM_CHUNK_LEN;
	if ( chunk >= ( chunks - 1 ) )
		expected_out_len -= ( -resource->len & ( WIM_CHUNK_LEN - 1 ) );

	/* Read possibly-compressed data */
	if ( len == expected_out_len ) {

		/* Chunk did not compress; read raw data */
		file->read ( file, buf->data, ( resource->offset + offset ),
			     len );

	} else {
		uint8_t zbuf[len];

		/* Read compressed data into a temporary buffer */
		file->read ( file, zbuf, ( resource->offset + offset ), len );

		/* Identify decompressor */
		if ( header->flags & WIM_HDR_LZX ) {
			decompress = lzx_decompress;
        } else if (header->flags & WIM_HDR_XPRESS) {
            decompress = xca_decompress;
		} else {
			DBG ( "Can't handle unknown compression scheme %#08x "
			      "for %#llx chunk %d at [%#llx+%#llx)\n",
			      header->flags, resource->offset,
			      chunk, ( resource->offset + offset ),
			      ( resource->offset + offset + len ) );
			return -1;
		}

		/* Decompress data */
		out_len = decompress ( zbuf, len, NULL );
		if ( out_len < 0 )
			return out_len;
		if ( ( ( size_t ) out_len ) != expected_out_len ) {
			DBG ( "Unexpected output length %#lx (expected %#zx)\n",
			      out_len, expected_out_len );
			return -1;
		}
		decompress ( zbuf, len, buf->data );
	}

	return 0;
}

/**
 * Read from a (possibly compressed) resource
 *
 * @v file		Virtual file
 * @v header		WIM header
 * @v resource		Resource
 * @v data		Data buffer
 * @v offset		Starting offset
 * @v len		Length
 * @ret rc		Return status code
 */
int wim_read ( struct vdisk_file *file, struct wim_header *header,
	       struct wim_resource_header *resource, void *data,
	       size_t offset, size_t len ) {
	static struct vdisk_file *cached_file;
	static size_t cached_resource_offset;
	static unsigned int cached_chunk;
	size_t zlen = ( resource->zlen__flags & WIM_RESHDR_ZLEN_MASK );
	unsigned int chunk;
	size_t skip_len;
	size_t frag_len;
	int rc;

	/* Sanity checks */
	if ( ( offset + len ) > resource->len ) {
		DBG ( "Resource too short (%#llx bytes)\n", resource->len );
		return -1;
	}
	if ( ( resource->offset + zlen ) > file->len ) {
		DBG ( "Resource exceeds length of file\n" );
		return -1;
	}

	/* If resource is uncompressed, just read the raw data */
	if ( ! ( resource->zlen__flags & ( WIM_RESHDR_COMPRESSED |
					   WIM_RESHDR_PACKED_STREAMS ) ) ) {
		file->read ( file, data, ( resource->offset + offset ), len );
		return 0;
	}

	/* Read from each chunk overlapping the target region */
	while ( len ) {

		/* Calculate chunk number */
		chunk = ( offset / WIM_CHUNK_LEN );

		/* Read chunk, if not already cached */
		if ( ( file != cached_file ) ||
		     ( resource->offset != cached_resource_offset ) ||
		     ( chunk != cached_chunk ) ) {

			/* Read chunk */
			if ( ( rc = wim_chunk ( file, header, resource, chunk,
						&wim_chunk_buffer ) ) != 0 )
				return rc;

			/* Update cache */
			cached_file = file;
			cached_resource_offset = resource->offset;
			cached_chunk = chunk;
		}

		/* Copy fragment from this chunk */
		skip_len = ( offset % WIM_CHUNK_LEN );
		frag_len = ( WIM_CHUNK_LEN - skip_len );
		if ( frag_len > len )
			frag_len = len;
		memcpy ( data, ( wim_chunk_buffer.data + skip_len ), frag_len );

		/* Move to next chunk */
		data += frag_len;
		offset += frag_len;
		len -= frag_len;
	}

	return 0;
}

/**
 * Get number of images
 *
 * @v file		Virtual file
 * @v header		WIM header
 * @v count		Count of images to fill in
 * @ret rc		Return status code
 */
int wim_count ( struct vdisk_file *file, struct wim_header *header,
		unsigned int *count ) {
	struct wim_lookup_entry entry;
	size_t offset;
	int rc;

	/* Count metadata entries */
	for ( offset = 0 ; ( offset + sizeof ( entry ) ) <= header->lookup.len ;
	      offset += sizeof ( entry ) ) {

		/* Read entry */
		if ( ( rc = wim_read ( file, header, &header->lookup, &entry,
				       offset, sizeof ( entry ) ) ) != 0 )
			return rc;

		/* Check for metadata entries */
		if ( entry.resource.zlen__flags & WIM_RESHDR_METADATA ) {
			(*count)++;
			DBG2 ( "...found image %d metadata at +%#zx\n",
			       *count, offset );
		}
	}

	return 0;
}

/**
 * Get WIM image metadata
 *
 * @v file		Virtual file
 * @v header		WIM header
 * @v index		Image index, or 0 to use boot image
 * @v meta		Metadata to fill in
 * @ret rc		Return status code
 */
int wim_metadata ( struct vdisk_file *file, struct wim_header *header,
		   unsigned int index, struct wim_resource_header *meta ) {
	struct wim_lookup_entry entry;
	size_t offset;
	unsigned int found = 0;
	int rc;

	/* If no image index is specified, just use the boot metadata */
	if ( index == 0 ) {
		memcpy ( meta, &header->boot, sizeof ( *meta ) );
		return 0;
	}

	/* Look for metadata entry */
	for ( offset = 0 ; ( offset + sizeof ( entry ) ) <= header->lookup.len ;
	      offset += sizeof ( entry ) ) {

		/* Read entry */
		if ( ( rc = wim_read ( file, header, &header->lookup, &entry,
				       offset, sizeof ( entry ) ) ) != 0 )
			return rc;

		/* Look for our target entry */
		if ( entry.resource.zlen__flags & WIM_RESHDR_METADATA ) {
			found++;
			DBG2 ( "...found image %d metadata at +%#zx\n",
			       found, offset );
			if ( found == index ) {
				memcpy ( meta, &entry.resource,
					 sizeof ( *meta ) );
				return 0;
			}
		}
	}

	/* Fail if index was not found */
	DBG ( "Cannot find WIM image index %d in %s\n", index, file->name );
	return -1;
}

/**
 * Get directory entry
 *
 * @v file		Virtual file
 * @v header		WIM header
 * @v meta		Metadata
 * @v name		Name
 * @v offset		Directory offset (will be updated)
 * @v direntry		Directory entry to fill in
 * @ret rc		Return status code
 */
static int wim_direntry ( struct vdisk_file *file, struct wim_header *header,
			  struct wim_resource_header *meta,
			  const wchar_t *name, size_t *offset,
			  struct wim_directory_entry *direntry ) {
	wchar_t name_buf[ wcslen ( name ) + 1 /* NUL */ ];
	int rc;

	/* Search directory */
	for ( ; ; *offset += direntry->len ) {

		/* Read length field */
		if ( ( rc = wim_read ( file, header, meta, direntry, *offset,
				       sizeof ( direntry->len ) ) ) != 0 )
			return rc;

		/* Check for end of this directory */
		if ( ! direntry->len ) {
			DBG ( "...directory entry \"%ls\" not found\n", name );
			return -1;
		}

		/* Read fixed-length portion of directory entry */
		if ( ( rc = wim_read ( file, header, meta, direntry, *offset,
				       sizeof ( *direntry ) ) ) != 0 )
			return rc;

		/* Check name length */
		if ( direntry->name_len > sizeof ( name_buf ) )
			continue;

		/* Read name */
		if ( ( rc = wim_read ( file, header, meta, &name_buf,
				       ( *offset + sizeof ( *direntry ) ),
				       sizeof ( name_buf ) ) ) != 0 )
			return rc;

		/* Check name */
		if ( wcscasecmp ( name, name_buf ) != 0 )
			continue;

		DBG2 ( "...found entry \"%ls\"\n", name );
		return 0;
	}
}

/**
 * Get directory entry for a path
 *
 * @v file		Virtual file
 * @v header		WIM header
 * @v meta		Metadata
 * @v path		Path to file/directory
 * @v offset		Directory entry offset to fill in
 * @v direntry		Directory entry to fill in
 * @ret rc		Return status code
 */
int wim_path ( struct vdisk_file *file, struct wim_header *header,
	       struct wim_resource_header *meta, const wchar_t *path,
	       size_t *offset, struct wim_directory_entry *direntry ) {
	wchar_t path_copy[ wcslen ( path ) + 1 /* WNUL */ ];
	struct wim_security_header security;
	wchar_t *name;
	wchar_t *next;
	int rc;

	/* Read security data header */
	if ( ( rc = wim_read ( file, header, meta, &security, 0,
			       sizeof ( security ) ) ) != 0 )
		return rc;

	/* Get root directory offset */
    if (security.len > 0)
    	direntry->subdir = ( ( security.len + sizeof ( uint64_t ) - 1 ) & ~( sizeof ( uint64_t ) - 1 ) );
    else
        direntry->subdir = security.len + 8;

	/* Find directory entry */
	name = memcpy ( path_copy, path, sizeof ( path_copy ) );
	do {
		next = wcschr ( name, L'\\' );
		if ( next )
			*next = L'\0';
		*offset = direntry->subdir;
		if ( ( rc = wim_direntry ( file, header, meta, name, offset,
					   direntry ) ) != 0 )
			return rc;
		name = ( next + 1 );
	} while ( next );

	return 0;
}

/**
 * Get file resource
 *
 * @v file		Virtual file
 * @v header		WIM header
 * @v meta		Metadata
 * @v path		Path to file
 * @v resource		File resource to fill in
 * @ret rc		Return status code
 */
int wim_file ( struct vdisk_file *file, struct wim_header *header,
	       struct wim_resource_header *meta, const wchar_t *path,
	       struct wim_resource_header *resource ) {
	struct wim_directory_entry direntry;
	struct wim_lookup_entry entry;
	size_t offset;
	int rc;

	/* Find directory entry */
	if ( ( rc = wim_path ( file, header, meta, path, &offset,
			       &direntry ) ) != 0 )
		return rc;

	/* File matching file entry */
	for ( offset = 0 ; ( offset + sizeof ( entry ) ) <= header->lookup.len ;
	      offset += sizeof ( entry ) ) {

		/* Read entry */
		if ( ( rc = wim_read ( file, header, &header->lookup, &entry,
				       offset, sizeof ( entry ) ) ) != 0 )
			return rc;

		/* Look for our target entry */
		if ( memcmp ( &entry.hash, &direntry.hash,
			      sizeof ( entry.hash ) ) == 0 ) {
			DBG ( "...found file \"%ls\"\n", path );
			memcpy ( resource, &entry.resource,
				 sizeof ( *resource ) );
			return 0;
		}
	}

	DBG ( "Cannot find file %ls\n", path );
	return -1;
}

/**
 * Get length of a directory
 *
 * @v file		Virtual file
 * @v header		WIM header
 * @v meta		Metadata
 * @v offset		Directory offset
 * @v len		Directory length to fill in (excluding terminator)
 * @ret rc		Return status code
 */
int wim_dir_len ( struct vdisk_file *file, struct wim_header *header,
		  struct wim_resource_header *meta, size_t offset,
		  size_t *len ) {
	struct wim_directory_entry direntry;
	int rc;

	/* Search directory */
	for ( *len = 0 ; ; *len += direntry.len ) {

		/* Read length field */
		if ( ( rc = wim_read ( file, header, meta, &direntry,
				       ( offset + *len ),
				       sizeof ( direntry.len ) ) ) != 0 )
			return rc;

		/* Check for end of this directory */
		if ( ! direntry.len )
			return 0;
	}
}
