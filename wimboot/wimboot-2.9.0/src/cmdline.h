#ifndef _CMDLINE_H
#define _CMDLINE_H

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
 * Command line
 *
 */

extern int cmdline_rawbcd;
extern int cmdline_rawwim;
extern int cmdline_quiet;
extern int cmdline_gui;
extern int cmdline_pause;
extern int cmdline_pause_quiet;
extern int cmdline_linear;
extern unsigned int cmdline_index;
extern void process_cmdline ( char *cmdline );

typedef int (*file_size_pf)(const char *path);
typedef int (*file_read_pf)(const char *path, int offset, int len, void *buf);
extern file_size_pf pfventoy_file_size;
extern file_read_pf pfventoy_file_read;
#define MAX_VF 16
extern char cmdline_vf_path[MAX_VF][64];
extern int cmdline_vf_num;

#endif /* _CMDLINE_H */
