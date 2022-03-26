/******************************************************************************
 * ventoy_md5.c  ---- ventoy md5
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
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
#include <errno.h>

const static uint32_t k[64] = 
{
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 
};

const static uint32_t r[] = 
{
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

#define LEFTROTATE(x, c) (((x) << (c)) | ((x) >> (32 - (c))))
#define to_bytes(val, bytes) *((uint32_t *)(bytes)) = (val)

#define ROTATE_CALC() \
{\
    temp = d; \
    d = c; \
    c = b; \
    b = b + LEFTROTATE((a + f + k[i] + w[g]), r[i]); \
    a = temp; \
}

void ventoy_md5(const void *data, uint32_t len, uint8_t *md5)
{ 
    uint32_t h0, h1, h2, h3;
    uint32_t w[16];
    uint32_t a, b, c, d, i, f, g, temp;
    uint32_t offset, mod, delta;
    uint8_t postbuf[128] = {0};
    
    // Initialize variables - simple count in nibbles:
    h0 = 0x67452301;
    h1 = 0xefcdab89;
    h2 = 0x98badcfe;
    h3 = 0x10325476;
 
    //Pre-processing:
    //append "1" bit to message    
    //append "0" bits until message length in bits กิ 448 (mod 512)
    //append length mod (2^64) to message

    mod = len % 64;
    if (mod)
    {
        memcpy(postbuf, (const uint8_t *)data + len - mod, mod);            
    }
    
    postbuf[mod] = 0x80;
    if (mod < 56)
    {
        to_bytes(len * 8, postbuf + 56);
        to_bytes(len >> 29, postbuf + 60);
        delta = 64;
    }
    else
    {
        to_bytes(len * 8, postbuf + 120);
        to_bytes(len >> 29, postbuf + 124);
        delta = 128;
    }

    len -= mod;
 
    for (offset = 0; offset < len + delta; offset += 64)
    {
        if (offset < len)
        {
            memcpy(w, (const uint8_t *)data + offset, 64);
        }
        else
        {
            memcpy(w, postbuf + offset - len, 64);
        }

        // Initialize hash value for this chunk:
        a = h0;
        b = h1;
        c = h2;
        d = h3;
 
        // Main loop:
        for (i = 0; i < 16; i++)
        {
            f = (b & c) | ((~b) & d);
            g = i;            
            ROTATE_CALC();
        }
        
        for (i = 16; i < 32; i++)
        {
            f = (d & b) | ((~d) & c);
            g = (5 * i + 1) % 16;
            ROTATE_CALC();
        }
        
        for (i = 32; i < 48; i++)
        {
            f = b ^ c ^ d;
            g = (3 * i + 5) % 16;
            ROTATE_CALC();
        }
        
        for (i = 48; i < 64; i++)
        {
            f = c ^ (b | (~d));
            g = (7 * i) % 16;
            ROTATE_CALC();
        }
        
        // Add this chunk's hash to result so far:
        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
    }
 
    //var char md5[16] := h0 append h1 append h2 append h3 //(Output is in little-endian)
    to_bytes(h0, md5);
    to_bytes(h1, md5 + 4);
    to_bytes(h2, md5 + 8);
    to_bytes(h3, md5 + 12);
}

