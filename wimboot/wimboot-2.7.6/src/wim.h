#ifndef _WIM_H
#define _WIM_H

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
 * The file format is documented in the document "Windows Imaging File
 * Format (WIM)", available from
 * 
 *   http://www.microsoft.com/en-us/download/details.aspx?id=13096
 *
 * The wimlib source code is also a useful reference.
 *
 */

#include <stdint.h>

/** A WIM resource header */
struct wim_resource_header {
	/** Compressed length and flags */
	uint64_t zlen__flags;
	/** Offset */
	uint64_t offset;
	/** Uncompressed length */
	uint64_t len;
} __attribute__ (( packed ));

/** WIM resource header length mask */
#define WIM_RESHDR_ZLEN_MASK 0x00ffffffffffffffULL

/** WIM resource header flags */
enum wim_resource_header_flags {
	/** Resource contains metadata */
	WIM_RESHDR_METADATA = ( 0x02ULL << 56 ),
	/** Resource is compressed */
	WIM_RESHDR_COMPRESSED = ( 0x04ULL << 56 ),
	/** Resource is compressed using packed streams */
	WIM_RESHDR_PACKED_STREAMS = ( 0x10ULL << 56 ),
};

/** A WIM header */
struct wim_header {
	/** Signature */
	uint8_t signature[8];
	/** Header length */
	uint32_t header_len;
	/** Verson */
	uint32_t version;
	/** Flags */
	uint32_t flags;
	/** Chunk length */
	uint32_t chunk_len;
	/** GUID */
	uint8_t guid[16];
	/** Part number */
	uint16_t part;
	/** Total number of parts */
	uint16_t parts;
	/** Number of images */
	uint32_t images;
	/** Lookup table */
	struct wim_resource_header lookup;
	/** XML data */
	struct wim_resource_header xml;
	/** Boot metadata */
	struct wim_resource_header boot;
	/** Boot index */
	uint32_t boot_index;
	/** Integrity table */
	struct wim_resource_header integrity;
	/** Reserved */
	uint8_t reserved[60];
} __attribute__ (( packed ));;

/** WIM header flags */
enum wim_header_flags {
	/** WIM uses Xpress compresson */
	WIM_HDR_XPRESS = 0x00020000,
	/** WIM uses LZX compression */
	WIM_HDR_LZX = 0x00040000,
};

/** A WIM file hash */
struct wim_hash {
	/** SHA-1 hash */
	uint8_t sha1[20];
} __attribute__ (( packed ));

/** A WIM lookup table entry */
struct wim_lookup_entry {
	/** Resource header */
	struct wim_resource_header resource;
	/** Part number */
	uint16_t part;
	/** Reference count */
	uint32_t refcnt;
	/** Hash */
	struct wim_hash hash;
} __attribute__ (( packed ));

/** WIM chunk length */
#define WIM_CHUNK_LEN 32768

/** A WIM chunk buffer */
struct wim_chunk_buffer {
	/** Data */
	uint8_t data[WIM_CHUNK_LEN];
};

/** Security data */
struct wim_security_header {
	/** Length */
	uint32_t len;
	/** Number of entries */
	uint32_t count;
} __attribute__ (( packed ));

/** Directory entry */
struct wim_directory_entry {
	/** Length */
	uint64_t len;
	/** Attributes */
	uint32_t attributes;
	/** Security ID */
	uint32_t security;
	/** Subdirectory offset */
	uint64_t subdir;
	/** Reserved */
	uint8_t reserved1[16];
	/** Creation time */
	uint64_t created;
	/** Last access time */
	uint64_t accessed;
	/** Last written time */
	uint64_t written;
	/** Hash */
	struct wim_hash hash;
	/** Reserved */
	uint8_t reserved2[12];
	/** Streams */
	uint16_t streams;
	/** Short name length */
	uint16_t short_name_len;
	/** Name length */
	uint16_t name_len;
} __attribute__ (( packed ));

/** Normal file */
#define WIM_ATTR_NORMAL 0x00000080UL

/** No security information exists for this file */
#define WIM_NO_SECURITY 0xffffffffUL

/** Windows complains if the time fields are left at zero */
#define WIM_MAGIC_TIME 0x1a7b83d2ad93000ULL

extern int wim_header ( struct vdisk_file *file, struct wim_header *header );
extern int wim_count ( struct vdisk_file *file, struct wim_header *header,
		       unsigned int *count );
extern int wim_metadata ( struct vdisk_file *file, struct wim_header *header,
			  unsigned int index, struct wim_resource_header *meta);
extern int wim_read ( struct vdisk_file *file, struct wim_header *header,
		      struct wim_resource_header *resource, void *data,
		      size_t offset, size_t len );
extern int wim_path ( struct vdisk_file *file, struct wim_header *header,
		      struct wim_resource_header *meta, const wchar_t *path,
		      size_t *offset, struct wim_directory_entry *direntry );
extern int wim_file ( struct vdisk_file *file, struct wim_header *header,
		      struct wim_resource_header *meta, const wchar_t *path,
		      struct wim_resource_header *resource );
extern int wim_dir_len ( struct vdisk_file *file, struct wim_header *header,
			 struct wim_resource_header *meta, size_t offset,
			 size_t *len );

#endif /* _WIM_H */
