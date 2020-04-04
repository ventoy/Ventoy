#ifndef SORT_H 
#define SORT_H

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
 * sort.h
 */

struct priority_entry {
	struct dir_ent *dir;
	struct priority_entry *next;
};

extern int read_sort_file(char *, int, char *[]);
extern void sort_files_and_write(struct dir_info *);
extern void generate_file_priorities(struct dir_info *, int priority,
	struct stat *);
extern struct  priority_entry *priority_list[65536];
#endif
