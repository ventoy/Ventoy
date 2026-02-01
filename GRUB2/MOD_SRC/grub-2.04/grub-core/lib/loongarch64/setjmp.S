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

#include <grub/dl.h>

	.file	"setjmp.S"

GRUB_MOD_LICENSE "GPLv3+"

	.text

/*
 * int grub_setjmp (jmp_buf env)
 */
FUNCTION(grub_setjmp)
	st.d $s0, $a0, 0x0
	st.d $s1, $a0, 0x8
	st.d $s2, $a0, 0x10
	st.d $s3, $a0, 0x18
	st.d $s4, $a0, 0x20
	st.d $s5, $a0, 0x28
	st.d $s6, $a0, 0x30
	st.d $s7, $a0, 0x38
	st.d $s8, $a0, 0x40
	st.d $fp, $a0, 0x48
	st.d $sp, $a0, 0x50
	st.d $ra, $a0, 0x58

	move $a0, $zero
	jr   $ra

/*
 * void grub_longjmp (jmp_buf env, int val)
 */
FUNCTION(grub_longjmp)
	ld.d $s0, $a0, 0x0
	ld.d $s1, $a0, 0x8
	ld.d $s2, $a0, 0x10
	ld.d $s3, $a0, 0x18
	ld.d $s4, $a0, 0x20
	ld.d $s5, $a0, 0x28
	ld.d $s6, $a0, 0x30
	ld.d $s7, $a0, 0x38
	ld.d $s8, $a0, 0x40
	ld.d $fp, $a0, 0x48
	ld.d $sp, $a0, 0x50
	ld.d $ra, $a0, 0x58

	/* Return 1 if passed 0, otherwise returns the value in place. */
	li.w $a0, 1
	beqz $a1, 1f
	move $a0, $a1
1:
	jr   $ra
