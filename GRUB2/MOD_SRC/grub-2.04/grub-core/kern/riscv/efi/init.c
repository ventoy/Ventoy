/* init.c - initialize a riscv-based EFI system */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2018 Free Software Foundation, Inc.
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

static grub_uint64_t timer_frequency_in_khz;

static grub_uint64_t
grub_efi_get_time_ms (void)
{
  grub_uint64_t tmr;

#if __riscv_xlen == 64
  asm volatile ("rdcycle %0" : "=r" (tmr));
#else
  grub_uint32_t lo, hi, tmp;
  asm volatile (
    "1:\n"
    "rdcycleh %0\n"
    "rdcycle %1\n"
    "rdcycleh %2\n"
    "bne %0, %2, 1b"
    : "=&r" (hi), "=&r" (lo), "=&r" (tmp));
  tmr = ((grub_uint64_t)hi << 32) | lo;
#endif

  return tmr / timer_frequency_in_khz;
}

void
grub_machine_init (void)
{
  grub_uint64_t time_before, time_after;

  grub_efi_init ();

  /* Calculate timer frequency */
  timer_frequency_in_khz = 1;
  time_before = grub_efi_get_time_ms();
  grub_efi_stall(1000);
  time_after = grub_efi_get_time_ms();
  timer_frequency_in_khz = time_after - time_before;

  grub_install_get_time_ms (grub_efi_get_time_ms);
}

void
grub_machine_fini (int flags)
{
  if (!(flags & GRUB_LOADER_FLAG_NORETURN))
    return;

  grub_efi_fini ();
}
