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

#include <grub/symbol.h>

	.p2align	4	/* force 16-byte alignment */

	.set push
	.set noreorder
	.set nomacro

VARIABLE (grub_relocator_forward_start)

copycont1:
	ld $11,0($8)
	sd $11,0($9)
	daddiu $8, $8, 8
	daddiu $10, $10, -8
	bne $10, $0, copycont1
	 daddiu $9, $9, 8

VARIABLE (grub_relocator_forward_end)

VARIABLE (grub_relocator_backward_start)

	daddu $9, $9, $10
	daddu $8, $8, $10
	/* Backward movsl is implicitly off-by-one.  compensate that.  */
	daddiu $9, $9, -8
	daddiu $8, $8, -8
copycont2:
	ld $11,0($8)
	sd $11,0($9)
	daddiu $8, $8, -8
	daddiu $10, $10, -8
	bne $10, $0, copycont2
	 daddiu $9, $9, -8

VARIABLE (grub_relocator_backward_end)

	.set pop
