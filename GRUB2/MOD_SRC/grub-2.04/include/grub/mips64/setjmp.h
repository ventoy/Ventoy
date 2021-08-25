/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2004,2006,2007,2009,2017  Free Software Foundation, Inc.
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

#ifndef GRUB_SETJMP_CPU_HEADER
#define GRUB_SETJMP_CPU_HEADER	1

typedef grub_uint64_t grub_jmp_buf[12];

int grub_setjmp (grub_jmp_buf env) RETURNS_TWICE;
void grub_longjmp (grub_jmp_buf env, int val) __attribute__ ((noreturn));

#endif /* ! GRUB_SETJMP_CPU_HEADER */
