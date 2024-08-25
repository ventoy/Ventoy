/* init.c - initialize an arm-based EFI system */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2013 Free Software Foundation, Inc.
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

#include <grub/env.h>
#include <grub/kernel.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/time.h>
#include <grub/efi/efi.h>
#include <grub/loader.h>

static grub_uint64_t tmr;
static grub_efi_event_t tmr_evt;

static grub_uint64_t
grub_efi_get_time_ms (void)
{
  return tmr;
}

static void 
increment_timer (grub_efi_event_t event __attribute__ ((unused)),
		 void *context __attribute__ ((unused)))
{
  tmr += 10;
}

void
grub_machine_init (void)
{
  grub_efi_boot_services_t *b;

  grub_efi_init ();

  b = grub_efi_system_table->boot_services;

  efi_call_5 (b->create_event, GRUB_EFI_EVT_TIMER | GRUB_EFI_EVT_NOTIFY_SIGNAL,
	      GRUB_EFI_TPL_CALLBACK, increment_timer, NULL, &tmr_evt);
  efi_call_3 (b->set_timer, tmr_evt, GRUB_EFI_TIMER_PERIODIC, 100000);

  grub_install_get_time_ms (grub_efi_get_time_ms);
}

void
grub_machine_fini (int flags)
{
  grub_efi_boot_services_t *b;

  if (!(flags & GRUB_LOADER_FLAG_NORETURN))
    return;

  b = grub_efi_system_table->boot_services;

  efi_call_3 (b->set_timer, tmr_evt, GRUB_EFI_TIMER_CANCEL, 0);
  efi_call_1 (b->close_event, tmr_evt);

  grub_efi_fini ();
}
