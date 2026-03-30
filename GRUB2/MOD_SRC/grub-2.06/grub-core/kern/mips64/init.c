/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009,2017  Free Software Foundation, Inc.
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

#include <grub/kernel.h>
#include <grub/env.h>
#include <grub/time.h>
#include <grub/cpu/mips.h>

grub_uint32_t grub_arch_cpuclock;

/* FIXME: use interrupt to count high.  */
grub_uint64_t
grub_get_rtc (void)
{
  static grub_uint32_t high = 0;
  static grub_uint32_t last = 0;
  grub_uint32_t low;

  asm volatile ("mfc0 %0, " GRUB_CPU_MIPS_COP0_TIMER_COUNT : "=r" (low));
  if (low < last)
    high++;
  last = low;

  return (((grub_uint64_t) high) << 32) | low;
}

void
grub_timer_init (grub_uint32_t cpuclock)
{
  grub_arch_cpuclock = cpuclock;
  grub_install_get_time_ms (grub_rtc_get_time_ms);
}
