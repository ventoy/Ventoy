/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Donn Seeley at Berkeley Software Design, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) Copyright (c) 1991, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)init.c	8.1 (Berkeley) 7/15/93
 * $FreeBSD: src/sbin/init/init.c,v 1.38.2.8 2001/10/22 11:27:32 des Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vtutil.h>

static int find_disk_by_signature(uint8_t *uuid, uint8_t *sig, uint64_t size, int *count, char *name)
{
    int fd;
    int len;
    int cnt = 0;
    FTS *ftsp;
	FTSENT *p;
    uint8_t mbr[512];
    char devname[MAXPATHLEN];
    static char dev[] = "/dev", *devav[] = {dev, NULL};

    vdebug("[VTOY] find_disk_by_signature %llu\n", size);

    ftsp = fts_open(devav, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
    while ((p = fts_read(ftsp)) != NULL)
    {
        if (p->fts_level == 1 && p->fts_statp && p->fts_name && p->fts_statp->st_size == size)
        {
            sprintf(devname, "/dev/%s", p->fts_name);

            fd = open(devname, O_RDONLY);
            if (fd < 0)
            {
                continue;
            }

            memset(mbr, 0, 512);
            read(fd, mbr, 512);
            close(fd);

            if (memcmp(mbr + 0x180, uuid, 16) == 0 && memcmp(mbr + 0x1B8, sig, 4) == 0)
            {
                cnt++;
                strcpy(name, p->fts_name);
                break;
            }
        }
    }

    *count = cnt;
    fts_close(ftsp);

    return 0;
}

static int find_disk_by_size(uint64_t size, const char *prefix, int *count, char *name)
{
    int len;
    int cnt = 0;
    FTS *ftsp;
	FTSENT *p;
    static char dev[] = "/dev", *devav[] = {dev, NULL};

    if (prefix)
    {
        len = strlen(prefix);
    }

    name[0] = 0;
    ftsp = fts_open(devav, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
    while ((p = fts_read(ftsp)) != NULL)
    {
        if (p->fts_level == 1 && p->fts_statp && p->fts_name && p->fts_statp->st_size == size)
        {
            if (prefix)
            {
                if (strncmp(p->fts_name, prefix, len) == 0)
                {
                    cnt++;
                    if (name[0] == 0)
                        strcpy(name, p->fts_name);
                }
            }
            else
            {
                cnt++;
                if (name[0] == 0)
                    strcpy(name, p->fts_name);
            }
        }
    }

    *count = cnt;
    fts_close(ftsp);

    return 0;
}

int prepare_dmtable(void)
{
    int count = 0;
    uint32_t i = 0;
    uint32_t sector_start = 0;
    uint32_t disk_sector_num = 0;
    FILE *fIn, *fOut;
    char disk[MAXPATHLEN];
    char prefix[MAXPATHLEN];
    ventoy_image_desc desc;
    ventoy_img_chunk chunk;
    
    fIn = fopen("/dmtable", "rb");
    if (!fIn)
    {
        printf("Failed to open dmtable\n");
        return 1;
    }
    
    fOut = fopen("/tmp/dmtable", "w+"); 
    if (!fOut)
    {
        printf("Failed to create /tmp/dmtable %d\n", errno);
        fclose(fIn);
        return 1;
    }

    fread(&desc, 1, sizeof(desc), fIn);
    
    vdebug("[VTOY] disksize:%lu part1size:%lu chunkcount:%u\n", desc.disk_size, desc.part1_size, desc.img_chunk_count);
    
    for (i = 0; count <= 0 && i < 10; i++)
    {
        sleep(2);
        find_disk_by_size(desc.part1_size, NULL, &count, disk);
        vdebug("[VTOY] find disk by part1 size, i=%d, count=%d, %s\n", i, count, disk);
    }

    if (count == 0)
    {
        goto end;
    }
    else if (count > 1)
    {
        find_disk_by_signature(desc.disk_uuid, desc.disk_signature, desc.disk_size, &count, prefix);
        vdebug("[VTOY] find disk by signature: %d %s\n", count, prefix);

        if (count != 1)
        {
            printf("[VTOY] Failed to find disk by signature\n");
            goto end;
        }

        find_disk_by_size(desc.part1_size, prefix, &count, disk);
        vdebug("[VTOY] find disk by part1 size with prefix %s : %d %s\n", prefix, count, disk);
    }

    for (i = 0; i < desc.img_chunk_count; i++)
    {
        fread(&chunk, 1, sizeof(chunk), fIn);

        sector_start = chunk.img_start_sector;
        disk_sector_num = (uint32_t)(chunk.disk_end_sector + 1 - chunk.disk_start_sector);

        fprintf(fOut, "%u %u linear /dev/%s %llu\n", 
               (sector_start << 2), disk_sector_num, 
               disk, (unsigned long long)chunk.disk_start_sector - 2048);
        
        vdebug("%u %u linear /dev/%s %llu\n", 
               (sector_start << 2), disk_sector_num, 
               disk, (unsigned long long)chunk.disk_start_sector - 2048);
    }

end:    
    fclose(fIn);   
    fclose(fOut);   
    return 0;
}

