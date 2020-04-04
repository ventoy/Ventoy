#ifndef READ_FS_H
#define READ_FS_H
/*
 * Squashfs
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2013
 * Phillip Lougher <phillip@squashfs.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * read_fs.h
 *
 */
extern struct compressor *read_super(int, struct squashfs_super_block *,
	char *);
extern long long read_filesystem(char *, int, struct squashfs_super_block *,
char **, char **, char **, char **, unsigned int *, unsigned int *,
unsigned int *, unsigned int *, unsigned int *, int *, int *, int *, int *,
int *, int *, long long *, unsigned int *, unsigned int *, unsigned int *,
unsigned int *, void (push_directory_entry)(char *, squashfs_inode, int, int),
struct squashfs_fragment_entry **, squashfs_inode **);
#endif
