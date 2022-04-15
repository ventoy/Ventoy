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

#ifndef GRUB_SMBIOS_HEADER
#define GRUB_SMBIOS_HEADER	1

#include <grub/symbol.h>
#include <grub/types.h>

#define GRUB_SMBIOS_TYPE_END_OF_TABLE ((grub_uint8_t)127)

struct grub_smbios_ieps
{
  grub_uint8_t anchor[5]; /* "_DMI_" */
  grub_uint8_t checksum;
  grub_uint16_t table_length;
  grub_uint32_t table_address;
  grub_uint16_t structures;
  grub_uint8_t revision;
} GRUB_PACKED;

struct grub_smbios_eps
{
  grub_uint8_t anchor[4]; /* "_SM_" */
  grub_uint8_t checksum;
  grub_uint8_t length; /* 0x1f */
  grub_uint8_t version_major;
  grub_uint8_t version_minor;
  grub_uint16_t maximum_structure_size;
  grub_uint8_t revision;
  grub_uint8_t formatted[5];
  struct grub_smbios_ieps intermediate;
} GRUB_PACKED;

struct grub_smbios_eps3
{
  grub_uint8_t anchor[5]; /* "_SM3_" */
  grub_uint8_t checksum;
  grub_uint8_t length; /* 0x18 */
  grub_uint8_t version_major;
  grub_uint8_t version_minor;
  grub_uint8_t docrev;
  grub_uint8_t revision;
  grub_uint8_t reserved;
  grub_uint32_t maximum_table_length;
  grub_uint64_t table_address;
} GRUB_PACKED;

extern struct grub_smbios_eps *grub_machine_smbios_get_eps (void);
extern struct grub_smbios_eps3 *grub_machine_smbios_get_eps3 (void);

extern struct grub_smbios_eps *EXPORT_FUNC (grub_smbios_get_eps) (void);

#endif /* ! GRUB_SMBIOS_HEADER */
