/******************************************************************************
 * vtoyloader.c  ---- ventoy loader (wapper for binary loader)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

static int verbose = 0;
#define debug(fmt, ...) if(verbose) printf(fmt, ##__VA_ARGS__)

static int vine_patch_loader(unsigned char *buf, int len, int major)
{
    int i;
    int ptrlen;
    unsigned int *data1;
    unsigned int *data2;

    /*
     * http://vinelinux.ime.cmc.osaka-u.ac.jp/Vine-6.5/SRPMS/SRPMS.main/anaconda-vine-11.0.2.1-1vl7.src.rpm
     * http://vinelinux.ime.cmc.osaka-u.ac.jp/Vine-6.5/SRPMS/SRPMS.main/kudzu-1.2.86-3vl6.src.rpm
     * anaconda-vine-11.0.2.1
     * isys/devnodes.c
     * static struct devnum devices[] = {
     *     { "aztcd",   29,	0,	0 },
     *     { "pcd",		46,	0,	0 },
     *
     * Patch 29 ---> 253
     */

    ptrlen = (buf[4] == 1) ? 4 : 8;
    debug("ELF %d bit major:%d ptrlen:%d\n", (buf[4] == 1) ? 32 : 64, major, ptrlen);    

    for (i = 0; i < len - 8 - 8 - ptrlen; i++)
    {
        data1 = (unsigned int *)(buf + i);
        data2 = (unsigned int *)(buf + i + 8 + ptrlen);

        if (data1[0] == 0x1D && data1[1] == 0x00 && data2[0] == 0x2E && data2[1] == 0x00)
        {
            debug("Find aztcd patch point at %d\n", i);
            data1[0] = major;
            break;
        }
    }

    for (i = 0; i < len; i++)
    {
        if (buf[i] != '/')
        {
            continue;
        }

        data1 = (unsigned int *)(buf + i + 1);

        /* /proc/ide */
        if (data1[0] == 0x636F7270 && data1[1] == 0x6564692F)
        {
            debug("patch string %s\n", (char *)(buf + i));
            buf[i + 1] = 'v';
            buf[i + 2] = 't';
            buf[i + 3] = 'o';
            buf[i + 4] = 'y';
        }
    }

    return 0;
}

int vtoyvine_main(int argc, char **argv)
{
    int i;
    int len;
    unsigned char *buf;
    FILE *fp;

    for (i = 0; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] == 'v')
        {
            verbose = 1;
            break;
        }
    }

    fp = fopen(argv[1], "rb");
    if (!fp)
    {
        fprintf(stderr, "Failed to open file %s err:%d\n", argv[1], errno);
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    len = (int)ftell(fp);
    debug("file length:%d\n", len);

    fseek(fp, 0, SEEK_SET);

    buf = (unsigned char *)malloc(len);
    if (!buf)
    {
        fclose(fp);
        return 1;
    }

    fread(buf, 1, len, fp);
    fclose(fp);

    vine_patch_loader(buf, len, (int)strtoul(argv[2], NULL, 10));

    fp = fopen(argv[1], "wb+");
    if (!fp)
    {
        fprintf(stderr, "Failed to open file %s err:%d\n", argv[1], errno);
        free(buf);
        return 1;
    }

    debug("write new data length:%d\n", len);
    fwrite(buf, 1, len, fp);
    fclose(fp);

    free(buf);
    
    return 0;
}

// wrapper main
#ifndef BUILD_VTOY_TOOL
int main(int argc, char **argv)
{
    return vtoyvine_main(argc, argv);
}
#endif

