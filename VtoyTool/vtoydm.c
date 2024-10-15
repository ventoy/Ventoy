/******************************************************************************
 * vtoydm.c  ---- ventoy device mapper tool
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
#include <linux/fs.h>
#include <dirent.h>
#include "biso.h"
#include "biso_list.h"
#include "biso_util.h"
#include "biso_plat.h"
#include "biso_9660.h"
#include "vtoytool.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef USE_DIET_C
#ifndef __mips__
typedef unsigned long long uint64_t;
#endif
typedef unsigned int    uint32_t;
#endif

#pragma pack(4)
typedef struct ventoy_img_chunk
{
    uint32_t img_start_sector; // sector size: 2KB
    uint32_t img_end_sector;   // included

    uint64_t disk_start_sector; // in disk_sector_size
    uint64_t disk_end_sector;   // included
}ventoy_img_chunk;
#pragma pack()

static int verbose = 0;
#define debug(fmt, ...) if(verbose) printf(fmt, ##__VA_ARGS__)

#define CMD_PRINT_TABLE       1
#define CMD_CREATE_DM         2
#define CMD_DUMP_ISO_INFO     3
#define CMD_EXTRACT_ISO_FILE  4
#define CMD_PRINT_EXTRACT_ISO_FILE  5

static uint64_t g_iso_file_size;
static char g_disk_name[128];
static int g_img_chunk_num = 0;
static ventoy_img_chunk *g_img_chunk = NULL;
static unsigned char g_iso_sector_buf[2048];

ventoy_img_chunk * vtoydm_get_img_map_data(const char *img_map_file, int *plen)
{
    int len;
    int rc = 1;
    FILE *fp = NULL;
    ventoy_img_chunk *chunk = NULL;
    
    fp = fopen(img_map_file, "rb");
    if (NULL == fp)
    {
        fprintf(stderr, "Failed to open file %s err:%d\n", img_map_file, errno);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    len = (int)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    debug("File <%s> len:%d\n", img_map_file, len);

    chunk = (ventoy_img_chunk *)malloc(len);
    if (NULL == chunk)
    {
        fprintf(stderr, "Failed to malloc memory len:%d err:%d\n", len, errno);
        goto end;
    }

    if (fread(chunk, 1, len, fp) != len)
    {
        fprintf(stderr, "Failed to read file err:%d\n", errno);
        goto end;
    }

    if (len % sizeof(ventoy_img_chunk))
    {
        fprintf(stderr, "image map file size %d is not aligned with %d\n", 
                len, (int)sizeof(ventoy_img_chunk));
        goto end;
    }

    rc = 0;
end:
    fclose(fp);

    if (rc)
    {
        if (chunk)
        {
            free(chunk);
            chunk = NULL;
        }
    }

    *plen = len;
    return chunk;
}


UINT64 vtoydm_get_file_size(const char *pcFileName)
{
    (void)pcFileName;

    debug("vtoydm_get_file_size %s %lu\n", pcFileName, (unsigned long)g_iso_file_size);
    
    return g_iso_file_size;
}

BISO_FILE_S * vtoydm_open_file(const char *pcFileName)
{
    BISO_FILE_S *file;

    debug("vtoydm_open_file %s\n", pcFileName);

    file = malloc(sizeof(BISO_FILE_S));
    if (file)
    {
        memset(file, 0, sizeof(BISO_FILE_S));

        file->FileSize = g_iso_file_size;
        file->CurPos = 0;
    }
    
    return file;
}

void vtoydm_close_file(BISO_FILE_S *pstFile)
{
    debug("vtoydm_close_file\n");
    
    if (pstFile)
    {
        free(pstFile);
    }
}

INT64 vtoydm_seek_file(BISO_FILE_S *pstFile, INT64 i64Offset, INT iFromWhere)
{
    debug("vtoydm_seek_file %d\n", (int)i64Offset);

    if (iFromWhere == SEEK_SET)
    {
        pstFile->CurPos = (UINT64)i64Offset;
    }

    return 0;
}

UINT64 vtoydm_map_iso_sector(UINT64 sector)
{
    int i;
    UINT64 disk_sector = 0;
    
    for (i = 0; i < g_img_chunk_num; i++)
    {
        if (sector >= g_img_chunk[i].img_start_sector && sector <= g_img_chunk[i].img_end_sector)
        {
            disk_sector = ((sector - g_img_chunk[i].img_start_sector) << 2) + g_img_chunk[i].disk_start_sector;
            break;
        }
    }

    return disk_sector;
}

int vtoydm_read_iso_sector(UINT64 sector, void *buf)
{
    int i;
    int fd;
    UINT64 disk_sector = 0;
    
    for (i = 0; i < g_img_chunk_num; i++)
    {
        if (sector >= g_img_chunk[i].img_start_sector && sector <= g_img_chunk[i].img_end_sector)
        {
            disk_sector = ((sector - g_img_chunk[i].img_start_sector) << 2) + g_img_chunk[i].disk_start_sector;
            break;
        }
    }

    fd = open(g_disk_name, O_RDONLY | O_BINARY);
    if (fd < 0)
    {
        debug("Failed to open %s\n", g_disk_name);
        return 1;
    }

    lseek(fd, disk_sector * 512, SEEK_SET);

    read(fd, buf, 2048);

    close(fd);
    return 0;
}

UINT64 vtoydm_read_file
(
    BISO_FILE_S *pstFile, 
    UINT         uiBlkSize, 
    UINT         uiBlkNum, 
    VOID        *pBuf
)
{
    int pos = 0;
    int align = 0;
    UINT64 readlen = uiBlkSize * uiBlkNum;
    char *curbuf = (char *)pBuf;

    debug("vtoydm_read_file length:%u\n", uiBlkSize * uiBlkNum);

    pos = (int)(pstFile->CurPos % 2048);
    if (pos > 0)
    {
        align = 2048 - pos;
        
        vtoydm_read_iso_sector(pstFile->CurPos / 2048, g_iso_sector_buf);
        if (readlen > align)
        {
            memcpy(curbuf, g_iso_sector_buf + pos, align);
            curbuf  += align;
            readlen -= align;
            pstFile->CurPos += align;
        }
        else
        {
            memcpy(curbuf, g_iso_sector_buf + pos, readlen);
            pstFile->CurPos += readlen;
            return readlen;
        }
    }

    while (readlen > 2048)
    {
        vtoydm_read_iso_sector(pstFile->CurPos / 2048, curbuf);
        pstFile->CurPos += 2048;
        
        curbuf += 2048;
        readlen -= 2048;
    }

    if (readlen > 0)
    {
        vtoydm_read_iso_sector(pstFile->CurPos / 2048, g_iso_sector_buf);
        memcpy(curbuf, g_iso_sector_buf, readlen);
        pstFile->CurPos += readlen;
    }
    
    return uiBlkSize * uiBlkNum;
}

int vtoydm_dump_iso(const char *img_map_file, const char *diskname)
{
    int i = 0;
    int len = 0;
    uint64_t sector_num;
    unsigned long ret;
    ventoy_img_chunk *chunk = NULL;
    BISO_READ_S *iso;
    BISO_PARSER_S *parser = NULL;
    char label[64] = {0};
    
    chunk = vtoydm_get_img_map_data(img_map_file, &len);
    if (NULL == chunk)
    {
        return 1;
    }

    for (i = 0; i < len / sizeof(ventoy_img_chunk); i++)
    {
        sector_num = chunk[i].img_end_sector - chunk[i].img_start_sector + 1;
        g_iso_file_size += sector_num * 2048;
    }

    strncpy(g_disk_name, diskname, sizeof(g_disk_name) - 1);
    g_img_chunk = chunk;
    g_img_chunk_num = len / sizeof(ventoy_img_chunk);

    debug("iso file size : %llu\n", (unsigned long long)g_iso_file_size);

    iso = BISO_AllocReadHandle();
    if (iso == NULL)
    {
        free(chunk);
        return 1;
    }

    ret = BISO_OpenImage("XXX", iso);
    debug("open iso image ret=0x%lx\n", ret);

    parser = (BISO_PARSER_S *)iso;    
    memcpy(label, parser->pstPVD->szVolumeId, 32);
    for (i = 32; i >=0; i--)
    {
        if (label[i] != 0 && label[i] != ' ')
        {
            break;
        }
        else
        {
            label[i] = 0;
        }
    }

    if (label[0])
    {
        printf("VENTOY_ISO_LABEL %s\n", label);    
    }
    
    BISO_DumpFileTree(iso);
    
    BISO_FreeReadHandle(iso);

    free(chunk);
    return 0;
}

static int vtoydm_extract_iso
(
    const char *img_map_file, 
    const char *diskname,
    unsigned long first_sector,
    unsigned long long file_size,
    const char *outfile
)
{
    int len;
    FILE *fp = NULL;

    g_img_chunk = vtoydm_get_img_map_data(img_map_file, &len);
    if (NULL == g_img_chunk)
    {
        return 1;
    }

    strncpy(g_disk_name, diskname, sizeof(g_disk_name) - 1);
    g_img_chunk_num = len / sizeof(ventoy_img_chunk);

    fp = fopen(outfile, "wb");
    if (fp == NULL)
    {
        fprintf(stderr, "Failed to create file %s err:%d\n", outfile, errno);
        free(g_img_chunk);
        return 1;
    }

    while (file_size > 0)
    {
        vtoydm_read_iso_sector(first_sector++, g_iso_sector_buf);
        if (file_size > 2048)
        {
            fwrite(g_iso_sector_buf, 2048, 1, fp);
            file_size -= 2048;
        }
        else
        {
            fwrite(g_iso_sector_buf, 1, file_size, fp);
            file_size = 0;
        }
    }
    
    fclose(fp);
    free(g_img_chunk);
    return 0;
}


static int vtoydm_print_extract_iso
(
    const char *img_map_file, 
    const char *diskname,
    unsigned long first_sector,
    unsigned long long file_size,
    const char *outfile
)
{
    int len;
    uint32_t last = 0;
    uint32_t sector = 0;
    uint32_t disk_first = 0;
    uint32_t count = 0;
    uint32_t buf[2];
    uint64_t size = file_size;
    FILE *fp = NULL;

    g_img_chunk = vtoydm_get_img_map_data(img_map_file, &len);
    if (NULL == g_img_chunk)
    {
        return 1;
    }

    strncpy(g_disk_name, diskname, sizeof(g_disk_name) - 1);
    g_img_chunk_num = len / sizeof(ventoy_img_chunk);

    fp = fopen(outfile, "wb");
    if (fp == NULL)
    {
        fprintf(stderr, "Failed to create file %s err:%d\n", outfile, errno);
        free(g_img_chunk);
        return 1;
    }

    fwrite(g_disk_name, 1, 32, fp);
    fwrite(&size, 1, 8, fp);

    while (file_size > 0)
    {
        sector = vtoydm_map_iso_sector(first_sector++);

        if (count > 0 && sector == last + 4)
        {
            last += 4;
            count += 4;
        }
        else
        {
            if (count > 0)
            {
                buf[0] = disk_first;
                buf[1] = count;
                fwrite(buf, 1, sizeof(buf), fp);
            }

            disk_first = sector;
            last = sector;
            count = 4;
        }
        
        if (file_size > 2048)
        {
            file_size -= 2048;
        }
        else
        {
            file_size = 0;
        }
    }

    if (count > 0)
    {
        buf[0] = disk_first;
        buf[1] = count;
        fwrite(buf, 1, sizeof(buf), fp);
    }
    
    fclose(fp);
    free(g_img_chunk);
    return 0;
}




static int vtoydm_print_linear_table(const char *img_map_file, const char *diskname, int part, uint64_t offset)
{
    int i;
    int len;
    uint32_t disk_sector_num;
    uint32_t sector_start;
    ventoy_img_chunk *chunk = NULL;
    
    chunk = vtoydm_get_img_map_data(img_map_file, &len);
    if (NULL == chunk)
    {
        return 1;
    }

    for (i = 0; i < len / sizeof(ventoy_img_chunk); i++)
    {
        sector_start = chunk[i].img_start_sector;
        disk_sector_num = (uint32_t)(chunk[i].disk_end_sector + 1 - chunk[i].disk_start_sector);

        /* TBD: to be more flexible */
        #if 0
        printf("%u %u linear %s %llu\n", 
               (sector_start << 2), disk_sector_num, 
               diskname, (unsigned long long)chunk[i].disk_start_sector);
        #else
        if (strstr(diskname, "nvme") || strstr(diskname, "mmc") || strstr(diskname, "nbd"))
        {
            printf("%u %u linear %sp%d %llu\n", 
               (sector_start << 2), disk_sector_num, 
               diskname, part, (unsigned long long)chunk[i].disk_start_sector - offset);
        }
        else
        {
            printf("%u %u linear %s%d %llu\n", 
               (sector_start << 2), disk_sector_num, 
               diskname, part, (unsigned long long)chunk[i].disk_start_sector - offset);
        }
        #endif
    }

    free(chunk);
    return 0;
}

static int vtoydm_print_help(FILE *fp)
{
    fprintf(fp, "Usage: \n"
            "   vtoydm -p -f img_map_file -d diskname [ -v ] \n"
            "   vtoydm -c -f img_map_file -d diskname [ -v ] \n"
            "   vtoydm -i -f img_map_file -d diskname [ -v ] \n"
            "   vtoydm -e -f img_map_file -d diskname -s sector -l len -o file [ -v ] \n"
            );
    return 0;        
}

static uint64_t vtoydm_get_part_start(const char *diskname, int part)
{
    int fd;
    unsigned long long size = 0;
    char diskpath[256] = {0};
    char sizebuf[64] = {0};

    if (strstr(diskname, "nvme") || strstr(diskname, "mmc") || strstr(diskname, "nbd"))
    {
        snprintf(diskpath, sizeof(diskpath) - 1, "/sys/class/block/%sp%d/start", diskname, part);
    }
    else
    {
        snprintf(diskpath, sizeof(diskpath) - 1, "/sys/class/block/%s%d/start", diskname, part);
    }

    if (access(diskpath, F_OK) >= 0)
    {
        debug("get part start from sysfs for %s %d\n", diskname, part);
        
        fd = open(diskpath, O_RDONLY | O_BINARY);
        if (fd >= 0)
        {
            read(fd, sizebuf, sizeof(sizebuf));
            size = strtoull(sizebuf, NULL, 10);
            close(fd);
            return size;
        }
    }
    else
    {
        debug("%s not exist \n", diskpath);
    }

    return size;
}

static int vtoydm_vlnk_convert(char *disk, int len, int *part, uint64_t *offset)
{
    int rc = 1;
    int cnt = 0;
    int rdlen;
    FILE *fp = NULL;
    ventoy_os_param param;
    char diskname[128] = {0};

    fp = fopen("/ventoy/ventoy_os_param", "rb");
    if (!fp)
    {
        debug("dm vlnk convert not exist %d\n", errno);
        goto end;
    }

    memset(&param, 0, sizeof(param));
    rdlen = (int)fread(&param, 1, sizeof(param), fp);
    if (rdlen != (int)sizeof(param))
    {
        debug("fread failed %d %d\n", rdlen, errno);
        goto end;
    }

    debug("dm vlnk convert vtoy_reserved=%d\n", param.vtoy_reserved[6]);

    if (param.vtoy_reserved[6])
    {
        cnt = vtoy_find_disk_by_guid(&param, diskname);
        debug("vtoy_find_disk_by_guid cnt=%d\n", cnt);        
        if (cnt == 1)
        {
            *part = param.vtoy_disk_part_id;
            *offset = vtoydm_get_part_start(diskname, *part);
            
            debug("VLNK <%s> <%s> <P%d> <%llu>\n", disk, diskname, *part, (unsigned long long)(*offset));

            snprintf(disk, len, "/dev/%s", diskname);

            rc = 0;
        }
    }

end:
    if (fp)
        fclose(fp);
    return rc;
}

int vtoydm_main(int argc, char **argv)
{
    int ch;
    int cmd = 0;
    int part = 1;
    uint64_t offset = 2048;
    unsigned long first_sector = 0;
    unsigned long long file_size = 0;
    char diskname[128] = {0};
    char filepath[300] = {0};
    char outfile[300] = {0};

    while ((ch = getopt(argc, argv, "s:l:o:d:f:v::i::p::c::h::e::E::")) != -1)
    {
        if (ch == 'd')
        {
            strncpy(diskname, optarg, sizeof(diskname) - 1);
        }
        else if (ch == 'f')
        {
            strncpy(filepath, optarg, sizeof(filepath) - 1);
        }
        else if (ch == 'p')
        {
            cmd = CMD_PRINT_TABLE;
        }
        else if (ch == 'c')
        {
            cmd = CMD_CREATE_DM;
        }
        else if (ch == 'i')
        {
            cmd = CMD_DUMP_ISO_INFO;
        }
        else if (ch == 'e')
        {
            cmd = CMD_EXTRACT_ISO_FILE;
        }
        else if (ch == 'E')
        {
            cmd = CMD_PRINT_EXTRACT_ISO_FILE;
        }
        else if (ch == 's')
        {
            first_sector = strtoul(optarg, NULL, 10);
        }
        else if (ch == 'l')
        {
            file_size = strtoull(optarg, NULL, 10);
        }
        else if (ch == 'o')
        {
            strncpy(outfile, optarg, sizeof(outfile) - 1);
        }
        else if (ch == 'v')
        {
            verbose = 1;
        }
        else if (ch == 'h')
        {
            return vtoydm_print_help(stdout);
        }
        else
        {
            vtoydm_print_help(stderr);
            return 1;
        }
    }

    if (filepath[0] == 0 || diskname[0] == 0)
    {
        fprintf(stderr, "Must input file and disk\n");
        return 1;
    }

    debug("cmd=%d file=<%s> disk=<%s> first_sector=%lu file_size=%llu\n", 
          cmd, filepath, diskname, first_sector, file_size);

    vtoydm_vlnk_convert(diskname, sizeof(diskname), &part, &offset);
    
    switch (cmd)
    {
        case CMD_PRINT_TABLE:
        {
            return vtoydm_print_linear_table(filepath, diskname, part, offset);
        }
        case CMD_CREATE_DM:
        {
            break;
        }
        case CMD_DUMP_ISO_INFO:
        {
            return vtoydm_dump_iso(filepath, diskname);
        }
        case CMD_EXTRACT_ISO_FILE:
        {
            return vtoydm_extract_iso(filepath, diskname, first_sector, file_size, outfile);
        }
        case CMD_PRINT_EXTRACT_ISO_FILE:
        {
            return vtoydm_print_extract_iso(filepath, diskname, first_sector, file_size, outfile);
        }
        default :
        {
            fprintf(stderr, "Invalid cmd \n");
            return 1;
        }
    }

	return 0;
}

// wrapper main
#ifndef BUILD_VTOY_TOOL
int main(int argc, char **argv)
{
    return vtoydm_main(argc, argv);
}
#endif

