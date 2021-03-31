/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2017  Free Software Foundation, Inc.
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

#ifndef GRUB_RELOCATOR_CPU_HEADER
#define GRUB_RELOCATOR_CPU_HEADER	1

#include <grub/types.h>
#include <grub/err.h>
#include <grub/relocator.h>

struct grub_relocator64_state
{
  /* gpr[0] is ignored since it's hardwired to 0.  */
  grub_uint64_t gpr[32];
  /* Register holding target $pc.  */
  int jumpreg;
};

grub_err_t
grub_relocator64_boot (struct grub_relocator *rel,
		       struct grub_relocator64_state state);

#endif /* ! GRUB_RELOCATOR_CPU_HEADER */
