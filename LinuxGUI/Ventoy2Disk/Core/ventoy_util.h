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

extern char g_log_file[PATH_MAX];
extern char g_ini_file[PATH_MAX];

#define check_free(p) if (p) free(p)
#define vtoy_safe_close_fd(fd) \
{\
    if ((fd) >= 0) \
    { \
        close(fd);  \
        (fd) = -1; \
    }\
}

extern uint8_t g_mbr_template[512];
void ventoy_gen_preudo_uuid(void *uuid);
int ventoy_get_disk_part_name(const char *dev, int part, char *partbuf, int bufsize);
int ventoy_get_sys_file_line(char *buffer, int buflen, const char *fmt, ...);
uint64_t ventoy_get_human_readable_gb(uint64_t SizeBytes);
void ventoy_md5(const void *data, uint32_t len, uint8_t *md5);
int ventoy_is_disk_mounted(const char *devpath);
int ventoy_try_umount_disk(const char *devpath);
int unxz(unsigned char *in, int in_size,
	 int (*fill)(void *dest, unsigned int size),
	 int (*flush)(void *src, unsigned int size),
	 unsigned char *out, int *in_used,
	 void (*error)(char *x));
int ventoy_read_file_to_buf(const char *FileName, int ExtLen, void **Bufer, int *BufLen);
const char * ventoy_get_local_version(void);
int ventoy_fill_gpt(uint64_t size, uint64_t reserve, int align4k, VTOY_GPT_INFO *gpt);
int ventoy_fill_mbr(uint64_t size, uint64_t reserve, int align4k, MBR_HEAD *pMBR);

#endif /* __VENTOY_UTIL_H__ */

