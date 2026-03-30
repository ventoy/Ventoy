/* efi.c - generic EFI support */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007,2008,2009,2010  Free Software Foundation, Inc.
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

#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/kernel.h>
#include <grub/acpi.h>
#include <grub/loader.h>

void
grub_halt (void)
{
  grub_machine_fini (GRUB_LOADER_FLAG_NORETURN);
#if !defined(__ia64__) && !defined(__arm__) && !defined(__aarch64__) && !defined(__mips__) &&\
    !defined(__riscv)
  grub_acpi_halt ();
#endif
  efi_call_4 (grub_efi_system_table->runtime_services->reset_system,
              GRUB_EFI_RESET_SHUTDOWN, GRUB_EFI_SUCCESS, 0, NULL);

  while (1);
}
