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

#include <grub/smbios.h>
#include <grub/misc.h>
#include <grub/efi/efi.h>
#include <grub/efi/api.h>

struct grub_smbios_eps *
grub_machine_smbios_get_eps (void)
{
  unsigned i;
  static grub_efi_packed_guid_t smbios_guid = GRUB_EFI_SMBIOS_TABLE_GUID;

  for (i = 0; i < grub_efi_system_table->num_table_entries; i++)
    {
      grub_efi_packed_guid_t *guid =
	&grub_efi_system_table->configuration_table[i].vendor_guid;

      if (! grub_memcmp (guid, &smbios_guid, sizeof (grub_efi_packed_guid_t)))
	return (struct grub_smbios_eps *)
	  grub_efi_system_table->configuration_table[i].vendor_table;
    }

  return 0;
}

struct grub_smbios_eps3 *
grub_machine_smbios_get_eps3 (void)
{
  unsigned i;
  static grub_efi_packed_guid_t smbios3_guid = GRUB_EFI_SMBIOS3_TABLE_GUID;

  for (i = 0; i < grub_efi_system_table->num_table_entries; i++)
    {
      grub_efi_packed_guid_t *guid =
	&grub_efi_system_table->configuration_table[i].vendor_guid;

      if (! grub_memcmp (guid, &smbios3_guid, sizeof (grub_efi_packed_guid_t)))
	return (struct grub_smbios_eps3 *)
	  grub_efi_system_table->configuration_table[i].vendor_table;
    }

  return 0;
}
