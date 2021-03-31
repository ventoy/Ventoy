/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009,2017  Free Software Foundation, Inc.
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

#ifndef	GRUB_IO_H
#define	GRUB_IO_H	1

#include <grub/types.h>

typedef grub_addr_t grub_port_t;

static __inline unsigned char
grub_inb (grub_port_t port)
{
  return *(volatile grub_uint8_t *) port;
}

static __inline unsigned short int
grub_inw (grub_port_t port)
{
  return *(volatile grub_uint16_t *) port;
}

static __inline unsigned int
grub_inl (grub_port_t port)
{
  return *(volatile grub_uint32_t *) port;
}

static __inline void
grub_outb (unsigned char value, grub_port_t port)
{
  *(volatile grub_uint8_t *) port = value;
}

static __inline void
grub_outw (unsigned short int value, grub_port_t port)
{
  *(volatile grub_uint16_t *) port = value;
}

static __inline void
grub_outl (unsigned int value, grub_port_t port)
{
  *(volatile grub_uint32_t *) port = value;
}

#endif /* _SYS_IO_H */
