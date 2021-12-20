/******************************************************************************
 * ventoy_util_linux.c  ---- ventoy util
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
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <linux/fs.h>
#include <dirent.h>
#include <time.h>
#include <ventoy_define.h>
#include <ventoy_util.h>

void ventoy_gen_preudo_uuid(void *uuid)
{
    int i;
    int fd;

    fd = open("/dev/urandom", O_RDONLY | O_BINARY);
    if (fd < 0)
    {
        srand(time(NULL));
        for (i = 0; i < 8; i++)
        {
            *((uint16_t *)uuid + i) = (uint16_t)(rand() & 0xFFFF);
        }
    }
    else
    {
        read(fd, uuid, 16);
        close(fd);
    }
}

int ventoy_get_sys_file_line(char *buffer, int buflen, const char *fmt, ...)
{
    int len;
    char c;
    char path[256];
    va_list arg;

    va_start(arg, fmt);
    vsnprintf(path, 256, fmt, arg);
    va_end(arg);

    if (access(path, F_OK) >= 0)
    {
        FILE *fp = fopen(path, "r");
        memset(buffer, 0, buflen);
        len = (int)fread(buffer, 1, buflen - 1, fp);
        fclose(fp);

        while (len > 0)
        {
            c = buffer[len - 1];
            if (c == '\r' || c == '\n' || c == ' ' || c == '\t')
            {
                buffer[len - 1] = 0;
                len--;
            }
            else
            {
                break;
            }
        }
        
        return 0;
    }
    else
    {
        vdebug("%s not exist \n", path);
        return 1;
    }
}

int ventoy_is_disk_mounted(const char *devpath)
{
    int len;
    int mount = 0;
    char line[512];
    FILE *fp = NULL;

    fp = fopen("/proc/mounts", "r");
    if (!fp)
    {
        return 0;
    }

    len = (int)strlen(devpath);
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, devpath, len) == 0)
        {
            mount = 1;
            vdebug("%s mounted <%s>\n", devpath, line);
            goto end;
        }
    }

end:
    fclose(fp);
    return mount;
}

const char * ventoy_get_os_language(void)
{
    const char *env = getenv("LANG");

    if (env && strncasecmp(env, "zh_CN", 5) == 0)
    {
        return "cn";
    }
    else
    {
        return "en";
    }
}

int ventoy_is_file_exist(const char *fmt, ...)
{
    va_list ap;
    struct stat sb;
    char fullpath[MAX_PATH];
    
    va_start (ap, fmt);
    vsnprintf(fullpath, MAX_PATH, fmt, ap);
    va_end (ap);

    if (stat(fullpath, &sb))
    {
        return 0;
    }

    if (S_ISREG(sb.st_mode))
    {
        return 1;
    }

    return 0;
}

int ventoy_is_directory_exist(const char *fmt, ...)
{
    va_list ap;
    struct stat sb;
    char fullpath[MAX_PATH];
    
    va_start (ap, fmt);
    vsnprintf(fullpath, MAX_PATH, fmt, ap);
    va_end (ap);

    if (stat(fullpath, &sb))
    {
        return 0;
    }

    if (S_ISDIR(sb.st_mode))
    {
        return 1;
    }

    return 0;
}

int ventoy_get_file_size(const char *file)
{
	int Size = -1;
    struct stat stStat;
    
	if (stat(file, &stStat) >= 0)
    {
        Size = (int)(stStat.st_size);
    }

	return Size;
}


int ventoy_write_buf_to_file(const char *FileName, void *Bufer, int BufLen)
{
    int fd;
    int rc;
    ssize_t size;

    fd = open(FileName, O_CREAT | O_RDWR | O_TRUNC, 0755);
    if (fd < 0)
    {
        vlog("Failed to open file %s %d\n", FileName, errno);
        return 1;
    }

    rc = fchmod(fd, 0755);
    if (rc)
    {
        vlog("Failed to chmod <%s> %d\n", FileName, errno);
    }
    
    size = write(fd, Bufer, BufLen);
    if ((int)size != BufLen)
    {
        close(fd);
        vlog("write file %s failed %d err:%d\n", FileName, (int)size, errno);
        return 1;
    }
    
    fsync(fd);
    close(fd);

    return 0;
}


static volatile int g_thread_stop = 0;
static pthread_t g_writeback_thread;
static pthread_mutex_t g_writeback_mutex;
static pthread_cond_t g_writeback_cond;
static void * ventoy_local_thread_run(void* data)
{
    ventoy_http_writeback_pf callback = (ventoy_http_writeback_pf)data;

    while (1)
    {
        pthread_mutex_lock(&g_writeback_mutex);
        pthread_cond_wait(&g_writeback_cond, &g_writeback_mutex);
        
        if (g_thread_stop)
        {
            pthread_mutex_unlock(&g_writeback_mutex);
            break;
        }
        else
        {
            callback();
            pthread_mutex_unlock(&g_writeback_mutex);
        }
    }    

    return NULL;
}

void ventoy_set_writeback_event(void)
{
    pthread_cond_signal(&g_writeback_cond);
}

int ventoy_start_writeback_thread(ventoy_http_writeback_pf callback)
{
    g_thread_stop = 0;
    pthread_mutex_init(&g_writeback_mutex, NULL);
    pthread_cond_init(&g_writeback_cond, NULL);

    pthread_create(&g_writeback_thread, NULL, ventoy_local_thread_run, callback);

    return 0;
}

void ventoy_stop_writeback_thread(void)
{
    g_thread_stop = 1;
    pthread_cond_signal(&g_writeback_cond);
    
    pthread_join(g_writeback_thread, NULL);


    pthread_cond_destroy(&g_writeback_cond);
    pthread_mutex_destroy(&g_writeback_mutex);
}



int ventoy_read_file_to_buf(const char *FileName, int ExtLen, void **Bufer, int *BufLen)
{
    int FileSize;
    FILE *fp = NULL;
    void *Data = NULL;

    fp = fopen(FileName, "rb");
    if (fp == NULL)
    {
        vlog("Failed to open file %s", FileName);
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    FileSize = (int)ftell(fp);

    Data = malloc(FileSize + ExtLen);
    if (!Data)
    {
        fclose(fp);
        return 1;
    }

    fseek(fp, 0, SEEK_SET);
    fread(Data, 1, FileSize, fp);

    fclose(fp);

    *Bufer = Data;
    *BufLen = FileSize;

    return 0;
}

int ventoy_copy_file(const char *a, const char *b)
{
    int len = 0;
    char *buf = NULL;
    
    if (0 == ventoy_read_file_to_buf(a, 0, (void **)&buf, &len))
    {
        ventoy_write_buf_to_file(b, buf, len);        
        free(buf);
    }
    
    return 0;
}

