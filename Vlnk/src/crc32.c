/******************************************************************************
 * crc32.c  ---- 
 *
 * Copyright (c) 2022, longpanda <admin@ventoy.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "vlnk.h"

static uint32_t crc32c_table [256];

/* Helper for init_crc32c_table.  */
static uint32_t reflect (uint32_t ref, int len)
{
  uint32_t result = 0;
  int i;

  for (i = 1; i <= len; i++)
    {
      if (ref & 1)
	result |= 1 << (len - i);
      ref >>= 1;
    }

  return result;
}

static void init_crc32c_table (void)
{
  uint32_t polynomial = 0x1edc6f41;
  int i, j;

  for(i = 0; i < 256; i++)
    {
      crc32c_table[i] = reflect(i, 8) << 24;
      for (j = 0; j < 8; j++)
        crc32c_table[i] = (crc32c_table[i] << 1) ^
            (crc32c_table[i] & (1 << 31) ? polynomial : 0);
      crc32c_table[i] = reflect(crc32c_table[i], 32);
    }
}

uint32_t ventoy_getcrc32c (uint32_t crc, const void *buf, int size)
{
  int i;
  const uint8_t *data = buf;

  if (! crc32c_table[1])
    init_crc32c_table ();

  crc^= 0xffffffff;

  for (i = 0; i < size; i++)
    {
      crc = (crc >> 8) ^ crc32c_table[(crc & 0xFF) ^ *data];
      data++;
    }

  return crc ^ 0xffffffff;
}

