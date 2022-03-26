/*
	platform.h (14.05.13)
	OS-specific code (libc-specific in fact). Note that systems with the
	same kernel can use different libc implementations.

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

#ifndef PLATFORM_H_INCLUDED
#define PLATFORM_H_INCLUDED

#if defined(__linux__) || defined(__GLIBC__) || defined(__GNU__)

#include <endian.h>
#include <byteswap.h>
#define exfat_bswap16(x) bswap_16(x)
#define exfat_bswap32(x) bswap_32(x)
#define exfat_bswap64(x) bswap_64(x)
#define EXFAT_BYTE_ORDER __BYTE_ORDER
#define EXFAT_LITTLE_ENDIAN __LITTLE_ENDIAN
#define EXFAT_BIG_ENDIAN __BIG_ENDIAN

#elif defined(__APPLE__)

#include <machine/endian.h>
#include <libkern/OSByteOrder.h>
#define exfat_bswap16(x) OSSwapInt16(x)
#define exfat_bswap32(x) OSSwapInt32(x)
#define exfat_bswap64(x) OSSwapInt64(x)
#define EXFAT_BYTE_ORDER BYTE_ORDER
#define EXFAT_LITTLE_ENDIAN LITTLE_ENDIAN
#define EXFAT_BIG_ENDIAN BIG_ENDIAN

#elif defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__) || defined(__OpenBSD__)

#include <sys/endian.h>
#define exfat_bswap16(x) bswap16(x)
#define exfat_bswap32(x) bswap32(x)
#define exfat_bswap64(x) bswap64(x)
#define EXFAT_BYTE_ORDER _BYTE_ORDER
#define EXFAT_LITTLE_ENDIAN _LITTLE_ENDIAN
#define EXFAT_BIG_ENDIAN _BIG_ENDIAN

#else 
#error Unknown platform
#endif

#endif /* ifndef PLATFORM_H_INCLUDED */
