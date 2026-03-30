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

	.file 	"loongson.S"
	.text

	.set		push
	.set		noreorder
	.align		4

VARIABLE (grub_efi_loongson_reset_start)

VARIABLE (grub_efi_loongson_reset_system_addr)
	.dword		0

reset_system:
	bal		1f
	move		$a1, $zero
1:
	ld		$t9, -16($ra)
	move		$a2, $zero
	jalr		$t9
	move		$a3, $zero

FUNCTION(grub_efi_loongson_reset_cold)
	b		reset_system
	li		$a0, 0

FUNCTION(grub_efi_loongson_reset_warm)
	b		reset_system
	li		$a0, 1

FUNCTION(grub_efi_loongson_reset_shutdown)
	b		reset_system
	li		$a0, 2

FUNCTION(grub_efi_loongson_reset_suspend)
	b		reset_system
	li		$a0, 3

VARIABLE (grub_efi_loongson_reset_end)

	.set		pop

