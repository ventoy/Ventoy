#ifndef _CPIO_H
#define _CPIO_H

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
 * CPIO archives
 *
 */

#include <stdint.h>

/** A CPIO archive header
 *
 * All field are hexadecimal ASCII numbers padded with '0' on the
 * left to the full width of the field.
 */
struct cpio_header {
	/** The string "070701" or "070702" */
	char c_magic[6];
	/** File inode number */
	char c_ino[8];
	/** File mode and permissions */
	char c_mode[8];
	/** File uid */
	char c_uid[8];
	/** File gid */
	char c_gid[8];
	/** Number of links */
	char c_nlink[8];
	/** Modification time */
	char c_mtime[8];
	/** Size of data field */
	char c_filesize[8];
	/** Major part of file device number */
	char c_maj[8];
	/** Minor part of file device number */
	char c_min[8];
	/** Major part of device node reference */
	char c_rmaj[8];
	/** Minor part of device node reference */
	char c_rmin[8];
	/** Length of filename, including final NUL */
	char c_namesize[8];
	/** Checksum of data field if c_magic is 070702, othersize zero */
	char c_chksum[8];
} __attribute__ (( packed ));

/** CPIO magic */
#define CPIO_MAGIC "070701"

/** CPIO trailer */
#define CPIO_TRAILER "TRAILER!!!"

extern int cpio_extract ( void *data, size_t len,
			  int ( * file ) ( const char *name, void *data,
					   size_t len ) );

#endif /* _CPIO_H */
