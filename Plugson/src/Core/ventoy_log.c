/******************************************************************************
 * ventoy_log.c  ---- ventoy log
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
#include <time.h>
#include <ventoy_define.h>
#include <ventoy_util.h>

extern char g_log_file[MAX_PATH];
static int g_ventoy_log_level = VLOG_DEBUG;
static pthread_mutex_t g_log_mutex;

int ventoy_log_init(void)
{
	if (ventoy_get_file_size(g_log_file) >= SIZE_1MB)
	{
		#if defined(_MSC_VER) || defined(WIN32)
			DeleteFileA(g_log_file);
		#else
			remove(g_log_file);
		#endif
	}

    pthread_mutex_init(&g_log_mutex, NULL);
    return 0;
}

void ventoy_log_exit(void)
{
    pthread_mutex_destroy(&g_log_mutex);
}

void ventoy_set_loglevel(int level)
{
    g_ventoy_log_level = level;
}



void ventoy_syslog_printf(const char *Fmt, ...)
{
    char log[512];
    va_list arg;
    time_t stamp;
    struct tm ttm;
    FILE *fp;
    
    time(&stamp);
    localtime_r(&stamp, &ttm);

    va_start(arg, Fmt);
#if defined(_MSC_VER) || defined(WIN32)
    vsnprintf_s(log, 512, _TRUNCATE, Fmt, arg);
#else
    vsnprintf(log, 512, Fmt, arg);
#endif
    va_end(arg);

    pthread_mutex_lock(&g_log_mutex);

#if defined(_MSC_VER) || defined(WIN32)
    fopen_s(&fp, g_log_file, "a+");
#else
    fp = fopen(g_log_file, "a+");
#endif

    if (fp)
    {
        fprintf(fp, "[%04u/%02u/%02u %02u:%02u:%02u] %s", 
			ttm.tm_year, ttm.tm_mon + 1, ttm.tm_mday,
           ttm.tm_hour, ttm.tm_min, ttm.tm_sec,
           log);
        fclose(fp);

        #ifdef VENTOY_SIM
        printf("[%04u/%02u/%02u %02u:%02u:%02u] %s", 
			ttm.tm_year, ttm.tm_mon + 1, ttm.tm_mday,
           ttm.tm_hour, ttm.tm_min, ttm.tm_sec,
           log);
        #endif
    }
    pthread_mutex_unlock(&g_log_mutex);
}

void ventoy_syslog(int level, const char *Fmt, ...)
{
    char log[512];
    va_list arg;
    time_t stamp;
    struct tm ttm;
    FILE *fp;
    
    if (level > g_ventoy_log_level)
    {
        return;
    }

    time(&stamp);
    localtime_r(&stamp, &ttm);

    va_start(arg, Fmt);
#if defined(_MSC_VER) || defined(WIN32)
    vsnprintf_s(log, 512, _TRUNCATE, Fmt, arg);
#else
    vsnprintf(log, 512, Fmt, arg);
#endif
    va_end(arg);

    pthread_mutex_lock(&g_log_mutex);
#if defined(_MSC_VER) || defined(WIN32)
    fopen_s(&fp, g_log_file, "a+");
#else
    fp = fopen(g_log_file, "a+");
#endif
    if (fp)
    {
        fprintf(fp, "[%04u/%02u/%02u %02u:%02u:%02u] %s", 
           ttm.tm_year + 1900, ttm.tm_mon + 1, ttm.tm_mday,
           ttm.tm_hour, ttm.tm_min, ttm.tm_sec,
           log);
        fclose(fp);

        #ifdef VENTOY_SIM
        printf("[%04u/%02u/%02u %02u:%02u:%02u] %s", 
			ttm.tm_year + 1900, ttm.tm_mon + 1, ttm.tm_mday,
           ttm.tm_hour, ttm.tm_min, ttm.tm_sec,
           log);
        #endif
    }
    pthread_mutex_unlock(&g_log_mutex);
}

