/* cache.h - Flush the processor's cache.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2004,2007  Free Software Foundation, Inc.
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

#ifndef GRUB_CACHE_H
#define GRUB_CACHE_H	1

#include <grub/symbol.h>
#include <grub/types.h>

#if defined (__i386__) || defined (__x86_64__)
static inline void
grub_arch_sync_caches (void *address __attribute__ ((unused)),
		       grub_size_t len __attribute__ ((unused)))
{
}
#else
void EXPORT_FUNC(grub_arch_sync_caches) (void *address, grub_size_t len);
#endif

#ifndef GRUB_MACHINE_EMU
#if defined (__aarch64__) || defined (__ia64__) || defined (__powerpc__) || \
    defined (__sparc__)

#elif defined (__i386__) || defined (__x86_64__)
static inline void
grub_arch_sync_dma_caches (volatile void *address __attribute__ ((unused)),
			   grub_size_t len __attribute__ ((unused)))
{
}
#elif defined(__mips__) && (_MIPS_SIM != _ABI64)
void EXPORT_FUNC(grub_arch_sync_dma_caches) (volatile void *address, grub_size_t len);
#endif
#endif

#ifdef __arm__
void
grub_arm_cache_enable (void);
#endif

#endif /* ! GRUB_CACHE_HEADER */
