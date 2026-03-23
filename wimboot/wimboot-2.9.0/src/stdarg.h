#ifndef _STDARG_H
#define _STDARG_H

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
 * Standard arguments
 *
 */

typedef __builtin_va_list va_list;
#define va_start( ap, last ) __builtin_va_start ( ap, last )
#define va_arg( ap, type ) __builtin_va_arg ( ap, type )
#define va_end( ap ) __builtin_va_end ( ap )
#define va_copy( dest, src ) __builtin_va_copy ( dest, src )

#endif /* _STDARG_H */
