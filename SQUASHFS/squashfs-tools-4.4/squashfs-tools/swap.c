/*
 * Copyright (c) 2009, 2010
 * Phillip Lougher <phillip@squashfs.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * swap.c
 */

#ifndef linux
#define __BYTE_ORDER BYTE_ORDER
#define __BIG_ENDIAN BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#else
#include <endian.h>
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
void swap_le16(void *src, void *dest)
{
	unsigned char *s = src;
	unsigned char *d = dest;

	d[0] = s[1];
	d[1] = s[0];
}


void swap_le32(void *src, void *dest)
{
	unsigned char *s = src;
	unsigned char *d = dest;

	d[0] = s[3];
	d[1] = s[2];
	d[2] = s[1];
	d[3] = s[0];
}


void swap_le64(void *src, void *dest)
{
	unsigned char *s = src;
	unsigned char *d = dest;

	d[0] = s[7];
	d[1] = s[6];
	d[2] = s[5];
	d[3] = s[4];
	d[4] = s[3];
	d[5] = s[2];
	d[6] = s[1];
	d[7] = s[0];
}


unsigned short inswap_le16(unsigned short num)
{
	return (num >> 8) |
		((num & 0xff) << 8);
}


unsigned int inswap_le32(unsigned int num)
{
	return (num >> 24) |
		((num & 0xff0000) >> 8) |
		((num & 0xff00) << 8) |
		((num & 0xff) << 24);
}


long long inswap_le64(long long n)
{
	unsigned long long num = n;

	return (num >> 56) |
		((num & 0xff000000000000LL) >> 40) |
		((num & 0xff0000000000LL) >> 24) |
		((num & 0xff00000000LL) >> 8) |
		((num & 0xff000000) << 8) |
		((num & 0xff0000) << 24) |
		((num & 0xff00) << 40) |
		((num & 0xff) << 56);
}


#define SWAP_LE_NUM(BITS) \
void swap_le##BITS##_num(void *s, void *d, int n) \
{\
	int i;\
	for(i = 0; i < n; i++, s += BITS / 8, d += BITS / 8)\
		swap_le##BITS(s, d);\
}

SWAP_LE_NUM(16)
SWAP_LE_NUM(32)
SWAP_LE_NUM(64)

#define INSWAP_LE_NUM(BITS, TYPE) \
void inswap_le##BITS##_num(TYPE *s, int n) \
{\
	int i;\
	for(i = 0; i < n; i++)\
		s[i] = inswap_le##BITS(s[i]);\
}

INSWAP_LE_NUM(16, unsigned short)
INSWAP_LE_NUM(32, unsigned int)
INSWAP_LE_NUM(64, long long)
#endif
