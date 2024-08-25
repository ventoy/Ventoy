/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2023 Free Software Foundation, Inc.
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

#include <grub/cache.h>
#include <grub/misc.h>

/* Prototypes for asm functions. */
void grub_arch_clean_dcache_range (void);
void grub_arch_invalidate_icache_range (void);

void
grub_arch_sync_caches (void *address __attribute__((unused)),
		       grub_size_t len __attribute__((unused)))
{
  grub_arch_clean_dcache_range ();
  grub_arch_invalidate_icache_range ();
}

void
grub_arch_sync_dma_caches (volatile void *address __attribute__((unused)),
			   grub_size_t len __attribute__((unused)))
{
  /* DMA non-coherent devices not supported yet */
}
