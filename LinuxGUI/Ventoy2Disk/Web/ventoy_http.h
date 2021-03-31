/******************************************************************************
 * ventoy_http.h
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
#ifndef __VENTOY_HTTP_H__
#define __VENTOY_HTTP_H__

#include <civetweb.h>

typedef enum PROGRESS_POINT
{
    PT_START = 1,

    PT_PRAPARE_FOR_CLEAN,
    PT_DEL_ALL_PART,

    PT_LOAD_CORE_IMG,
    PT_LOAD_DISK_IMG,
    PT_UNXZ_DISK_IMG_FINISH = PT_LOAD_DISK_IMG + 32,

    PT_FORMAT_PART1, //10
    
    PT_FORMAT_PART2,

    PT_WRITE_VENTOY_START,
    PT_WRITE_VENTOY_FINISH = PT_WRITE_VENTOY_START + 8,

    PT_WRITE_STG1_IMG,//45
    PT_SYNC_DATA1,
    
    PT_CHECK_PART2,
    PT_CHECK_PART2_FINISH = PT_CHECK_PART2 + 8,

    PT_WRITE_PART_TABLE,//52
    PT_SYNC_DATA2,
    
    PT_FINISH
}PROGRESS_POINT;

typedef int (*ventoy_json_callback)(struct mg_connection *conn, VTOY_JSON *json);
typedef struct JSON_CB
{
    const char *method;
    ventoy_json_callback callback;
}JSON_CB;

typedef struct ventoy_thread_data
{
    int diskfd;
    uint32_t align4kb;
    uint32_t partstyle;
    uint32_t secure_boot;
    uint64_t reserveBytes;
    ventoy_disk *disk;
}ventoy_thread_data;

extern int g_vtoy_exfat_disk_fd;
extern uint64_t g_vtoy_exfat_part_size;

int ventoy_http_init(void);
void ventoy_http_exit(void);
int ventoy_http_start(const char *ip, const char *port);
int ventoy_http_stop(void);
int mkexfat_main(const char *devpath, int fd, uint64_t part_sector_count);

#endif /* __VENTOY_HTTP_H__ */

