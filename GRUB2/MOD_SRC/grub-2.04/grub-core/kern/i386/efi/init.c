/* init.c - initialize an x86-based EFI system */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007  Free Software Foundation, Inc.
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

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/cache.h>
#include <grub/kernel.h>
#include <grub/efi/efi.h>
#include <grub/i386/tsc.h>
#include <grub/loader.h>
#include <grub/tpm.h>

void
grub_machine_init (void)
{
  grub_efi_init ();
  grub_tsc_init ();
}

void
grub_machine_fini (int flags)
{
  if (!(flags & GRUB_LOADER_FLAG_NORETURN))
    return;

  grub_efi_fini ();

  if (!(flags & GRUB_LOADER_FLAG_EFI_KEEP_ALLOCATED_MEMORY))
    grub_efi_memory_fini ();
}
