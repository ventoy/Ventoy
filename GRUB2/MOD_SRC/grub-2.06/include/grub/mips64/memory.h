/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2017 Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GRUB_MEMORY_CPU_HEADER
#define GRUB_MEMORY_CPU_HEADER	1

#ifndef ASM_FILE
#include <grub/symbol.h>
#include <grub/err.h>
#include <grub/types.h>
#endif

#ifndef ASM_FILE

typedef grub_addr_t grub_phys_addr_t;

static inline grub_phys_addr_t
grub_vtop (void *a)
{
  if (-1 == ((grub_int64_t) a >> 32))
    return ((grub_phys_addr_t) a) & 0x1fffffffUL;
  return ((grub_phys_addr_t) a) & 0xffffffffffffUL;
}

static inline void *
grub_map_memory (grub_phys_addr_t a, grub_size_t size)
{
  if ((a + size) < 0x20000000UL)
    return (void *) (a | 0xffffffff80000000UL);
//  return (void *) (a | 0x9800000000000000UL);
    return (void *) ((a&0x8fffffff) | 0xffffffff00000000UL);
}

static inline void
grub_unmap_memory (void *a __attribute__ ((unused)),
		   grub_size_t size __attribute__ ((unused)))
{
}

#endif

#endif
