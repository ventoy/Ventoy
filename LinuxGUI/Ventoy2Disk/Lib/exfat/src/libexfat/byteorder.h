/*
	byteorder.h (12.01.10)
	Endianness stuff. exFAT uses little-endian byte order.

	Free exFAT implementation.
	Copyright (C) 2010-2018  Andrew Nayenko

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef BYTEORDER_H_INCLUDED
#define BYTEORDER_H_INCLUDED

#include "platform.h"
#include <stdint.h>
#include <stddef.h>

typedef struct { uint16_t __u16; } le16_t;
typedef struct { uint32_t __u32; } le32_t;
typedef struct { uint64_t __u64; } le64_t;

#if EXFAT_BYTE_ORDER == EXFAT_LITTLE_ENDIAN

static inline uint16_t le16_to_cpu(le16_t v) { return v.__u16; }
static inline uint32_t le32_to_cpu(le32_t v) { return v.__u32; }
static inline uint64_t le64_to_cpu(le64_t v) { return v.__u64; }

static inline le16_t cpu_to_le16(uint16_t v) { le16_t t = {v}; return t; }
static inline le32_t cpu_to_le32(uint32_t v) { le32_t t = {v}; return t; }
static inline le64_t cpu_to_le64(uint64_t v) { le64_t t = {v}; return t; }

typedef size_t bitmap_t;

#elif EXFAT_BYTE_ORDER == EXFAT_BIG_ENDIAN

static inline uint16_t le16_to_cpu(le16_t v)
	{ return exfat_bswap16(v.__u16); }
static inline uint32_t le32_to_cpu(le32_t v)
	{ return exfat_bswap32(v.__u32); }
static inline uint64_t le64_to_cpu(le64_t v)
	{ return exfat_bswap64(v.__u64); }

static inline le16_t cpu_to_le16(uint16_t v)
	{ le16_t t = {exfat_bswap16(v)}; return t; }
static inline le32_t cpu_to_le32(uint32_t v)
	{ le32_t t = {exfat_bswap32(v)}; return t; }
static inline le64_t cpu_to_le64(uint64_t v)
	{ le64_t t = {exfat_bswap64(v)}; return t; }

typedef unsigned char bitmap_t;

#else
#error Wow! You have a PDP machine?!
#endif

#endif /* ifndef BYTEORDER_H_INCLUDED */
