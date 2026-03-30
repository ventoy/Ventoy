/******************************************************************************
 * wimboot.h
 *
 * Copyright (c) 2020, longpanda <admin@ventoy.net>
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
#ifndef __WIMBOOT_H__
#define __WIMBOOT_H__

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/disk.h>
#include <grub/device.h>
#include <grub/term.h>
#include <grub/partition.h>
#include <grub/file.h>
#include <grub/normal.h>
#include <grub/extcmd.h>
#include <grub/datetime.h>
#include <grub/i18n.h>
#include <grub/net.h>
#include <grub/time.h>
#include <grub/crypto.h>
#include <grub/ventoy.h>
#include "ventoy_def.h"


#define size_t grub_size_t
#define ssize_t grub_ssize_t
#define memset grub_memset
#define memcpy grub_memcpy

#define uint8_t   grub_uint8_t
#define uint16_t  grub_uint16_t
#define uint32_t  grub_uint32_t
#define uint64_t  grub_uint64_t
#define int32_t   grub_int32_t



#define assert(exp)

//#define DBG grub_printf
#define DBG(fmt, ...)

const char * huffman_bin ( unsigned long value, unsigned int bits );

#endif

