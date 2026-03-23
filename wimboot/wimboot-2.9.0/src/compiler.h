#ifndef _COMPILER_H
#define _COMPILER_H

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
 * Global compiler definitions
 *
 */

/* Force visibility of all symbols to "hidden", i.e. inform gcc that
 * all symbol references resolve strictly within our final binary.
 * This avoids unnecessary PLT/GOT entries on x86_64.
 *
 * This is a stronger claim than specifying "-fvisibility=hidden",
 * since it also affects symbols marked with "extern".
 */
#ifndef ASSEMBLY
#if __GNUC__ >= 4
#pragma GCC visibility push(hidden)
#endif
#endif /* ASSEMBLY */

#endif /* _COMPILER_H */
