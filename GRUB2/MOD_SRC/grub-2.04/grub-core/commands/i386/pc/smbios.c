/* smbios.c - get smbios tables. */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019  Free Software Foundation, Inc.
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

#include <grub/acpi.h>
#include <grub/smbios.h>
#include <grub/misc.h>

struct grub_smbios_eps *
grub_machine_smbios_get_eps (void)
{
  grub_uint8_t *ptr;

  grub_dprintf ("smbios", "Looking for SMBIOS EPS. Scanning BIOS\n");

  for (ptr = (grub_uint8_t *) 0xf0000; ptr < (grub_uint8_t *) 0x100000; ptr += 16)
    if (grub_memcmp (ptr, "_SM_", 4) == 0
	&& grub_byte_checksum (ptr, sizeof (struct grub_smbios_eps)) == 0)
      return (struct grub_smbios_eps *) ptr;

  return 0;
}

struct grub_smbios_eps3 *
grub_machine_smbios_get_eps3 (void)
{
  grub_uint8_t *ptr;

  grub_dprintf ("smbios", "Looking for SMBIOS3 EPS. Scanning BIOS\n");

  for (ptr = (grub_uint8_t *) 0xf0000; ptr < (grub_uint8_t *) 0x100000; ptr += 16)
    if (grub_memcmp (ptr, "_SM3_", 5) == 0
	&& grub_byte_checksum (ptr, sizeof (struct grub_smbios_eps3)) == 0)
      return (struct grub_smbios_eps3 *) ptr;

  return 0;
}
