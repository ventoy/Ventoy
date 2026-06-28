/******************************************************************************
 * VtoyShim.c
 *
 * Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved.<BR>
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 */

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>


static const UINT32 sha256_initial_h[8]
    = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

static const UINT32 sha256_round_k[64]
    = { 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
        0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
        0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
        0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
        0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

static void sha256_endian_reverse64(UINT64 input, UINT8 *output) {
    output[7] = (input >> 0) & 0xff;
    output[6] = (input >> 8) & 0xff;
    output[5] = (input >> 16) & 0xff;
    output[4] = (input >> 24) & 0xff;
    output[3] = (input >> 32) & 0xff;
    output[2] = (input >> 40) & 0xff;
    output[1] = (input >> 48) & 0xff;
    output[0] = (input >> 56) & 0xff;
}

static UINT32 sha256_endian_read32(UINT8 *input) {
    UINT32 output = 0;
    output |= (input[0] << 24);
    output |= (input[1] << 16);
    output |= (input[2] << 8);
    output |= (input[3] << 0);

    return output;
}

static void sha256_endian_reverse32(UINT32 input, UINT8 *output) {
    output[3] = (input >> 0) & 0xff;
    output[2] = (input >> 8) & 0xff;
    output[1] = (input >> 16) & 0xff;
    output[0] = (input >> 24) & 0xff;
}

static UINT32 sha256_ror(UINT32 input, UINT32 by) {
    return (input >> by) | (((input & ((1 << by) - 1))) << (32 - by));
}

void calc_sha256(const void *data, UINT64 len, void *output)
{
    int i = 0;
    int j = 0;
    UINT8 padding[80];
    UINT64 current = (len + 1) % 64;
    // want to be == 56 % 64.
    UINT64 needed = (64 + 56 - current) % 64;
    UINT64 extra = needed + 9;
    UINT64 total = len + extra;
    UINT32 v[8];
    UINT64 cursor = 0;

    for(i = 1; i < 80; i++)
    {
        padding[i] = 0;
    }
    padding[0] = 0x80;

    sha256_endian_reverse64(len * 8, padding + total - len - 8);

    for(i = 0; i < 8; i++)
    {
        v[i] = sha256_initial_h[i];
    }

    for(cursor = 0; cursor * 64 < total; cursor++)
    {
        UINT32 t[8];
        UINT32 w[64];

        for(i = 0; i < 8; i++)
        {
            t[i] = v[i];
        }


        if(cursor * 64 + 64 <= len)
        {
            for(j = 0; j < 16; j++)
            {
                w[j] = sha256_endian_read32((UINT8 *)data + cursor * 64 + j * 4);
            }
        }
        else
        {
            if(cursor * 64 < len)
            {
                UINT64 size = len - cursor * 64;
                if(size > 0)
                {
                    CopyMem(w, (UINT8 *)data + cursor * 64, size);
                }
                CopyMem((UINT8 *)w + size, padding, 64 - size);
            }
            else
            {
                UINT64 off = (cursor * 64 - len) % 64;
                CopyMem((UINT8 *)w, padding + off, 64);
            }

            for(j = 0; j < 16; j++)
            {
                w[j] = sha256_endian_read32((UINT8 *)&w[j]);
            }
        }

        for(j = 16; j < 64; j++)
        {
            UINT32 s1 = sha256_ror(w[j - 2], 17) ^ sha256_ror(w[j - 2], 19) ^ (w[j - 2] >> 10);
            UINT32 s0 = sha256_ror(w[j - 15], 7) ^ sha256_ror(w[j - 15], 18) ^ (w[j - 15] >> 3);
            w[j] = s1 + w[j - 7] + s0 + w[j - 16];
        }

        for(j = 0; j < 64; j++)
        {
            UINT32 ch = (t[4] & t[5]) ^ (~t[4] & t[6]);
            UINT32 maj = (t[0] & t[1]) ^ (t[0] & t[2]) ^ (t[1] & t[2]);
            UINT32 S0 = sha256_ror(t[0], 2) ^ sha256_ror(t[0], 13) ^ sha256_ror(t[0], 22);
            UINT32 S1 = sha256_ror(t[4], 6) ^ sha256_ror(t[4], 11) ^ sha256_ror(t[4], 25);
            UINT32 t1 = t[7] + S1 + ch + sha256_round_k[j] + w[j];
            UINT32 t2 = S0 + maj;

            t[7] = t[6];
            t[6] = t[5];
            t[5] = t[4];
            t[4] = t[3] + t1;
            t[3] = t[2];
            t[2] = t[1];
            t[1] = t[0];
            t[0] = t1 + t2;
        }

        for(i = 0; i < 8; i++)
        {
            v[i] += t[i];
        }
    }

    for(i = 0; i < 8; i++)
    {
        sha256_endian_reverse32(v[i], (UINT8 *)output + i * 4);
    }
}


