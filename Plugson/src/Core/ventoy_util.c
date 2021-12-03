/******************************************************************************
 * ventoy_util.c  ---- ventoy util
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
#include <sys/stat.h>
#include <ventoy_define.h>
#include <ventoy_util.h>


static int g_tar_filenum = 0;
static char *g_tar_buffer = NULL;
static ventoy_file *g_tar_filelist = NULL;

SYSINFO g_sysinfo;

unsigned char *g_unxz_buffer = NULL;
int g_unxz_len = 0;

void unxz_error(char *x)
{
	vlog("%s\n", x);
}

int unxz_flush(void *src, unsigned int size)
{
	memcpy(g_unxz_buffer + g_unxz_len, src, size);
	g_unxz_len += (int)size;

	return (int)size;
}


uint64_t ventoy_get_human_readable_gb(uint64_t SizeBytes)
{
    int i;
    int Pow2 = 1;
    double Delta;
    double GB = SizeBytes * 1.0 / 1000 / 1000 / 1000;

    if ((SizeBytes % SIZE_1GB) == 0)
    {
        return (uint64_t)(SizeBytes / SIZE_1GB);
    }

    for (i = 0; i < 12; i++)
    {
        if (Pow2 > GB)
        {
            Delta = (Pow2 - GB) / Pow2;
        }
        else
        {
            Delta = (GB - Pow2) / Pow2;
        }

        if (Delta < 0.05)
        {
            return Pow2;
        }

        Pow2 <<= 1;
    }

    return (uint64_t)GB;
}

ventoy_file * ventoy_tar_find_file(const char *path)
{
    int i;
    int len;
    ventoy_file *node = g_tar_filelist;

    len = (int)strlen(path);
    
    for (i = 0; i < g_tar_filenum; i++, node++)
    {
        if (node->pathlen == len && memcmp(node->path, path, len) == 0)
        {
            return node;
        }

        if (node->pathlen > len)
        {
            break;
        }
    }

    return NULL;
}


int ventoy_decompress_tar(char *tarbuf, int buflen, int *tarsize)
{
    int rc = 1;
	int inused = 0;
	int BufLen = 0;
	unsigned char *buffer = NULL;
    char tarxz[MAX_PATH];

#if defined(_MSC_VER) || defined(WIN32)
    scnprintf(tarxz, sizeof(tarxz), "%s\\ventoy\\%s", g_ventoy_dir, PLUGSON_TXZ);
#else
    scnprintf(tarxz, sizeof(tarxz), "%s/tool/%s", g_ventoy_dir, PLUGSON_TXZ);
#endif

    if (ventoy_read_file_to_buf(tarxz, 0, (void **)&buffer, &BufLen))
    {
        vlog("Failed to read file <%s>\n", tarxz);
        return 1;
    }

    g_unxz_buffer = (unsigned char *)tarbuf;
    g_unxz_len = 0;

    unxz(buffer, BufLen, NULL, unxz_flush, NULL, &inused, unxz_error);
    vlog("xzlen:%u rawdata size:%d\n", BufLen, g_unxz_len);

    if (inused != BufLen)
    {
        vlog("Failed to unxz data %d %d\n", inused, BufLen);
        rc = 1;
    }
    else
    {
        *tarsize = g_unxz_len;
        rc = 0;        
    }

	free(buffer);

    return rc;
}

int ventoy_www_init(void)
{
    int i = 0;
    int j = 0;
    int size = 0;
    int tarsize = 0;
    int offset = 0;
    ventoy_file *node = NULL;
    ventoy_file *node2 = NULL;
    VENTOY_TAR_HEAD *pHead = NULL;
    ventoy_file tmpnode;

    if (!g_tar_filelist)
    {
        g_tar_filelist = malloc(VENTOY_FILE_MAX * sizeof(ventoy_file));
        g_tar_buffer = malloc(TAR_BUF_MAX);
        g_tar_filenum = 0;
    }

    if ((!g_tar_filelist) || (!g_tar_buffer))
    {
        return 1;
    }

    if (ventoy_decompress_tar(g_tar_buffer, TAR_BUF_MAX, &tarsize))
    {
        vlog("Failed to decompress tar\n");
        return 1;
    }

    pHead = (VENTOY_TAR_HEAD *)g_tar_buffer;
    node = g_tar_filelist;

    while (g_tar_filenum < VENTOY_FILE_MAX && size < tarsize && memcmp(pHead->magic, TMAGIC, 5) == 0)
    {
        if (pHead->typeflag == REGTYPE)
        {
            node->size = (int)strtol(pHead->size, NULL, 8);
            node->pathlen = (int)scnprintf(node->path, MAX_PATH, "%s", pHead->name);
            node->addr = pHead + 1;

            if (node->pathlen == 13 && strcmp(pHead->name, "www/buildtime") == 0)
            {
                scnprintf(g_sysinfo.buildtime, sizeof(g_sysinfo.buildtime), "%s", (char *)node->addr);
                vlog("Plugson buildtime %s\n", g_sysinfo.buildtime);
            }

			offset = 512 + VENTOY_UP_ALIGN(node->size, 512);

            node++;
            g_tar_filenum++;
        }
        else
        {
            offset = 512;
        }

        pHead = (VENTOY_TAR_HEAD *)((char *)pHead + offset);
        size += offset;
    }


    //sort
    for (i = 0; i < g_tar_filenum; i++)
    for (j = i + 1; j < g_tar_filenum; j++)
    {
        node = g_tar_filelist + i;
        node2 = g_tar_filelist + j;

        if (node->pathlen > node2->pathlen)
        {
            memcpy(&tmpnode, node, sizeof(ventoy_file));
            memcpy(node, node2, sizeof(ventoy_file));
            memcpy(node2, &tmpnode, sizeof(ventoy_file));
        }
    }

    vlog("Total extract %d files from tar file.\n", g_tar_filenum);
    
    return 0;
}

void ventoy_www_exit(void)
{
    check_free(g_tar_filelist);
    check_free(g_tar_buffer);
    g_tar_filelist = NULL;
    g_tar_buffer = NULL;
    g_tar_filenum = 0;
}


void ventoy_get_json_path(char *path, char *backup)
{
#if defined(_MSC_VER) || defined(WIN32)
    scnprintf(path, 64, "%C:\\ventoy\\ventoy.json", g_cur_dir[0]);
if (backup)
{
    scnprintf(backup, 64, "%C:\\ventoy\\ventoy_backup.json", g_cur_dir[0]);
}
#else
    scnprintf(path, 64, "%s/ventoy/ventoy.json", g_cur_dir);
if (backup)
{
    scnprintf(backup, 64, "%s/ventoy/ventoy_backup.json", g_cur_dir);
}

#endif    
}


