/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010,2017  Free Software Foundation, Inc.
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

#ifndef GRUB_REGISTERS_CPU_HEADER
#define GRUB_REGISTERS_CPU_HEADER	1

#ifdef ASM_FILE
#define GRUB_CPU_REGISTER_WRAP(x) x
#else
#define GRUB_CPU_REGISTER_WRAP(x) #x
#endif

#define GRUB_CPU_MIPS_COP0_TIMER_COUNT GRUB_CPU_REGISTER_WRAP($9)

#endif
