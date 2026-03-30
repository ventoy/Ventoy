/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2007,2009  Free Software Foundation, Inc.
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

#include <grub/symbol.h>
#include <grub/dl.h>
#include <grub/mips64/asm.h>

	.file	"setjmp.S"

GRUB_MOD_LICENSE "GPLv3+"

	.text

/*
 * int grub_setjmp (grub_jmp_buf env)
 */
FUNCTION(grub_setjmp)
	GRUB_ASM_REG_S $s0, 0($a0)
	GRUB_ASM_REG_S $s1, 8($a0)
	GRUB_ASM_REG_S $s2, 16($a0)
	GRUB_ASM_REG_S $s3, 24($a0)
	GRUB_ASM_REG_S $s4, 32($a0)
	GRUB_ASM_REG_S $s5, 40($a0)
	GRUB_ASM_REG_S $s6, 48($a0)
	GRUB_ASM_REG_S $s7, 56($a0)
	GRUB_ASM_REG_S $s8, 64($a0)
	GRUB_ASM_REG_S $gp, 72($a0)
	GRUB_ASM_REG_S $sp, 80($a0)
	GRUB_ASM_REG_S $ra, 88($a0)
	move $v0, $zero
	move $v1, $zero
	jr $ra
	 nop
/*
 * int grub_longjmp (grub_jmp_buf env, int val)
 */
FUNCTION(grub_longjmp)
	GRUB_ASM_REG_L $s0, 0($a0)
	GRUB_ASM_REG_L $s1, 8($a0)
	GRUB_ASM_REG_L $s2, 16($a0)
	GRUB_ASM_REG_L $s3, 24($a0)
	GRUB_ASM_REG_L $s4, 32($a0)
	GRUB_ASM_REG_L $s5, 40($a0)
	GRUB_ASM_REG_L $s6, 48($a0)
	GRUB_ASM_REG_L $s7, 56($a0)
	GRUB_ASM_REG_L $s8, 64($a0)
	GRUB_ASM_REG_L $gp, 72($a0)
	GRUB_ASM_REG_L $sp, 80($a0)
	GRUB_ASM_REG_L $ra, 88($a0)
	addiu $v0, $zero, 1
	movn $v0, $a1, $a1
	move $v1, $zero
	jr $ra
	 nop
