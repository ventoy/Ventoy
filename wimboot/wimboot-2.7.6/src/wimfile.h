#ifndef _WIMFILE_H
#define _WIMFILE_H

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
 * WIM virtual files
 *
 */

#include <wchar.h>

struct vdisk_file;

extern struct vdisk_file * wim_add_file ( struct vdisk_file *file,
					  unsigned int index,
					  const wchar_t *path,
					  const wchar_t *wname );
extern void wim_add_files ( struct vdisk_file *file, unsigned int index,
			    const wchar_t **paths );

#endif /* _WIMFILE_H */
