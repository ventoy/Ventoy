/******************************************************************************
 * ventoy_util.h
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
#ifndef __VENTOY_UTIL_H__
#define __VENTOY_UTIL_H__

#define PLUGSON_TXZ "plugson.tar.xz"

#define check_free(p) if (p) free(p)
#define vtoy_safe_close_fd(fd) \
{\
    if ((fd) >= 0) \
    { \
        close(fd);  \
        (fd) = -1; \
    }\
}

extern char g_cur_dir[MAX_PATH];
extern char g_ventoy_dir[MAX_PATH];

#if defined(_MSC_VER) || defined(WIN32)

typedef HANDLE pthread_mutex_t;

static __inline int pthread_mutex_init(pthread_mutex_t *mutex, void *unused)
{
	(void)unused;
	*mutex = CreateMutex(NULL, FALSE, NULL);
	return *mutex == NULL ? -1 : 0;
}

static __inline int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	return CloseHandle(*mutex) == 0 ? -1 : 0;
}

static __inline int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    return ReleaseMutex(*mutex) == 0 ? -1 : 0;
}

static __inline int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	return WaitForSingleObject(*mutex, INFINITE) == WAIT_OBJECT_0 ? 0 : -1;
}

int ventoy_path_case(char *path, int slash);

#else
int ventoy_get_sys_file_line(char *buffer, int buflen, const char *fmt, ...);

#define UINT8 uint8_t
#define UINT16 uint16_t
#define UINT32 uint32_t
#define UINT64 uint64_t

static inline int ventoy_path_case(char *path, int slash)
{
    (void)path;
    (void)slash;
    return 0;
}
#endif


#define LANGUAGE_EN 0
#define LANGUAGE_CN 1

typedef struct SYSINFO
{
    char buildtime[128];
    int syntax_error;
    int invalid_config;
    int config_save_error;
        
    int language;
    int pathcase;
    char cur_fsname[64];
    char cur_capacity[64];
    char cur_model[256];
    char cur_ventoy_ver[64];
    int cur_secureboot;
    int cur_part_style;

	char ip[32];
	char port[16];
}SYSINFO;

extern SYSINFO g_sysinfo;




#define TMAGIC "ustar"

#define REGTYPE  '0'
#define AREGTYPE  '\0'
#define LNKTYPE '1'
#define CHRTYPE '3'
#define BLKTYPE '4'
#define DIRTYPE '5'
#define FIFOTYPE '6'
#define CONTTYPE '7'

#pragma pack(1)

typedef struct tag_tar_head
{
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
}VENTOY_TAR_HEAD;


#pragma pack()

#define VENTOY_UP_ALIGN(N, align)  (((N) + ((align) - 1)) / (align) * (align))
#define VENTOY_FILE_MAX   2048


#if defined(_MSC_VER) || defined(WIN32)
#define million_sleep(a) Sleep(a)
#else
#define million_sleep(a) usleep((a) * 1000)
#endif


typedef struct ventoy_file
{
    int size;
    char path[MAX_PATH];
    int pathlen;
    void *addr;
}ventoy_file;



int ventoy_is_file_exist(const char *fmt, ...);
int ventoy_is_directory_exist(const char *fmt, ...);
void ventoy_gen_preudo_uuid(void *uuid);
uint64_t ventoy_get_human_readable_gb(uint64_t SizeBytes);
void ventoy_md5(const void *data, uint32_t len, uint8_t *md5);
int ventoy_is_disk_mounted(const char *devpath);
int unxz(unsigned char *in, int in_size,
	 int (*fill)(void *dest, unsigned int size),
	 int (*flush)(void *src, unsigned int size),
	 unsigned char *out, int *in_used,
	 void (*error)(char *x));
int ventoy_read_file_to_buf(const char *FileName, int ExtLen, void **Bufer, int *BufLen);
int ventoy_write_buf_to_file(const char *FileName, void *Bufer, int BufLen);
const char * ventoy_get_os_language(void);
int ventoy_get_file_size(const char *file);
int ventoy_www_init(void);
void ventoy_www_exit(void);
int ventoy_decompress_tar(char *tarbuf, int buflen, int *tarsize);
ventoy_file * ventoy_tar_find_file(const char *path);
void ventoy_get_json_path(char *path, char *backup);
int ventoy_copy_file(const char *a, const char *b);

typedef int (*ventoy_http_writeback_pf)(void);

int ventoy_start_writeback_thread(ventoy_http_writeback_pf callback);
void ventoy_stop_writeback_thread(void);
void ventoy_set_writeback_event(void);


extern unsigned char *g_unxz_buffer;
extern int g_unxz_len;
void unxz_error(char *x);
int unxz_flush(void *src, unsigned int size);

#endif /* __VENTOY_UTIL_H__ */

