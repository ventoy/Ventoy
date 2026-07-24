/******************************************************************************
 * ventoy_http.c  ---- ventoy http
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
#include <linux/limits.h>
#include <dirent.h>
#include <pthread.h>
#include <ventoy_define.h>
#include <ventoy_json.h>
#include <ventoy_util.h>
#include <ventoy_disk.h>
#include <ventoy_http.h>
#include "fat_filelib.h"

static char *g_pub_out_buf = NULL;
static int g_pub_out_max = 0;

static pthread_mutex_t g_api_mutex;
static char g_cur_language[128];
static int  g_cur_part_style = 0;
static int  g_cur_show_all = 0;
static char g_cur_server_token[64];
static struct mg_context *g_ventoy_http_ctx = NULL;

static uint32_t g_efi_part_offset = 0;
static uint8_t *g_efi_part_raw_img = NULL;
static uint8_t *g_grub_stg1_raw_img = NULL;

static char g_cur_process_diskname[64];
static char g_cur_process_type[64];
static volatile int g_cur_process_result = 0;
static volatile PROGRESS_POINT g_current_progress = PT_FINISH;

static int ventoy_load_mbr_template(void)
{
    FILE *fp = NULL;

    fp = fopen("boot/boot.img", "rb");
    if (fp == NULL)
    {
        vlog("Failed to open file boot/boot.img\n");
        return 1;
    }

    fread(g_mbr_template, 1, 512, fp);
    fclose(fp);
    
    ventoy_gen_preudo_uuid(g_mbr_template + 0x180);    
    return 0;
}

static int ventoy_disk_xz_flush(void *src, unsigned int size)
{
    memcpy(g_efi_part_raw_img + g_efi_part_offset, src, size);
    g_efi_part_offset += size;

    g_current_progress = PT_LOAD_DISK_IMG + (g_efi_part_offset / SIZE_1MB);
    return (int)size;
}

static int ventoy_unxz_efipart_img(void)
{
    int rc;
    int inlen;
    int xzlen;
    void *xzbuf = NULL;
    uint8_t *buf = NULL;

    rc = ventoy_read_file_to_buf(VENTOY_FILE_DISK_IMG, 0, &xzbuf, &xzlen);
    vdebug("read disk.img.xz rc:%d len:%d\n", rc, xzlen);

    if (g_efi_part_raw_img)
    {
        buf = g_efi_part_raw_img;
    }
    else
    {
        buf = malloc(VTOYEFI_PART_BYTES);
        if (!buf)
        {
            check_free(xzbuf);
            return 1;
        }
    }

    g_efi_part_offset = 0;
    g_efi_part_raw_img = buf;
    
    rc = unxz(xzbuf, xzlen, NULL, ventoy_disk_xz_flush, buf, &inlen, NULL);
    vdebug("ventoy_unxz_efipart_img len:%d rc:%d unxzlen:%u\n", inlen, rc, g_efi_part_offset);

    check_free(xzbuf);
    return 0;
}

static int ventoy_unxz_stg1_img(void)
{
    int rc;
    int inlen;
    int xzlen;
    void *xzbuf = NULL;
    uint8_t *buf = NULL;

    rc = ventoy_read_file_to_buf(VENTOY_FILE_STG1_IMG, 0, &xzbuf, &xzlen);
    vdebug("read core.img.xz rc:%d len:%d\n", rc, xzlen);

    if (g_grub_stg1_raw_img)
    {
        buf = g_grub_stg1_raw_img;
    }
    else
    {
        buf = zalloc(SIZE_1MB);
        if (!buf)
        {
            check_free(xzbuf);
            return 1;
        }
    }
    
    rc = unxz(xzbuf, xzlen, NULL, NULL, buf, &inlen, NULL);
    vdebug("ventoy_unxz_stg1_img len:%d rc:%d\n", inlen, rc);

    g_grub_stg1_raw_img = buf;

    check_free(xzbuf);
    return 0;
}


static int ventoy_http_save_cfg(void)
{
    FILE *fp;

    fp = fopen(g_ini_file, "w");
    if (!fp)
    {
        vlog("Failed to open %s code:%d\n", g_ini_file, errno);
        return 0;
    }

    fprintf(fp, "[Ventoy]\nLanguage=%s\nPartStyle=%d\nShowAllDevice=%d\n", 
        g_cur_language, g_cur_part_style, g_cur_show_all);

    fclose(fp);
    return 0;
}

static int ventoy_http_load_cfg(void)
{
    int i;
    int len;
    char line[256];
    FILE *fp;

    fp = fopen(g_ini_file, "r");
    if (!fp)
    {
        return 0;
    }

    while (fgets(line, sizeof(line), fp))
    {
        len = (int)strlen(line);
        for (i = len - 1; i >= 0; i--)
        {
            if (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\n')
            {
                line[i] = 0;
            }
            else
            {
                break;
            }
        }
    
        len = (int)strlen("Language=");
        if (strncmp(line, "Language=", len) == 0)
        {
            scnprintf(g_cur_language, "%s", line + len);
        }
        else if (strncmp(line, "PartStyle=", strlen("PartStyle=")) == 0)
        {
            g_cur_part_style = (int)strtol(line + strlen("PartStyle="), NULL, 10);
        }
        else if (strncmp(line, "ShowAllDevice=", strlen("ShowAllDevice=")) == 0)
        {
            g_cur_show_all = (int)strtol(line + strlen("ShowAllDevice="), NULL, 10);
        }
    }

    fclose(fp);
    return 0;
}


static int ventoy_json_result(struct mg_connection *conn, const char *err)
{
    if (conn)
    {
        mg_printf(conn, 
                  "HTTP/1.1 200 OK \r\n"
                  "Content-Type: application/json\r\n"
                  "Content-Length: %d\r\n"
                  "\r\n%s",
                  (int)strlen(err), err);
    }
    else
    {
        memcpy(g_pub_out_buf, err, (int)strlen(err) + 1);
    }

    return 0;
}

static int ventoy_json_buffer(struct mg_connection *conn, const char *json_buf, int json_len)
{
    if (conn)
    {
        mg_printf(conn, 
                  "HTTP/1.1 200 OK \r\n"
                  "Content-Type: application/json\r\n"
                  "Content-Length: %d\r\n"
                  "\r\n%s",
                  json_len, json_buf);    
    }
    else
    {
        if (json_len >= g_pub_out_max)
        {
            vlog("json buffer overflow\n");
        }
        else
        {
            memcpy(g_pub_out_buf, json_buf, json_len);
            g_pub_out_buf[json_len] = 0;
        }
    }

    return 0;
}

static int ventoy_api_sysinfo(struct mg_connection *conn, VTOY_JSON *json)
{
    int busy = 0;
    int pos = 0;
    int buflen = 0;
    char buf[512];
    
    (void)json;

    busy = (g_current_progress == PT_FINISH) ? 0 : 1;

    buflen = sizeof(buf) - 1;
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_STRN("token", g_cur_server_token);
    VTOY_JSON_FMT_STRN("language", g_cur_language);
    VTOY_JSON_FMT_STRN("ventoy_ver", ventoy_get_local_version());
    VTOY_JSON_FMT_UINT("partstyle", g_cur_part_style);
    VTOY_JSON_FMT_BOOL("busy", busy);
    VTOY_JSON_FMT_STRN("process_disk", g_cur_process_diskname);
    VTOY_JSON_FMT_STRN("process_type", g_cur_process_type);
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    ventoy_json_buffer(conn, buf, pos);
    return 0;
}

static int ventoy_api_get_percent(struct mg_connection *conn, VTOY_JSON *json)
{
    int pos = 0;
    int buflen = 0;
    int percent = 0;
    char buf[128];
    
    (void)json;

    percent = g_current_progress * 100 / PT_FINISH;

    buflen = sizeof(buf) - 1;
    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_STRN("result", g_cur_process_result ? "failed" : "success");
    VTOY_JSON_FMT_STRN("process_disk", g_cur_process_diskname);
    VTOY_JSON_FMT_STRN("process_type", g_cur_process_type);
    VTOY_JSON_FMT_UINT("percent", percent);
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    ventoy_json_buffer(conn, buf, pos);
    return 0;
}

static int ventoy_api_set_language(struct mg_connection *conn, VTOY_JSON *json)
{
    const char *lang = NULL;
    
    lang = vtoy_json_get_string_ex(json, "language");
    if (lang)
    {
        scnprintf(g_cur_language, "%s", lang);
        ventoy_http_save_cfg();
    }

    ventoy_json_result(conn, VTOY_JSON_SUCCESS_RET);
    return 0;    
}

static int ventoy_api_set_partstyle(struct mg_connection *conn, VTOY_JSON *json)
{
    int ret;
    int style = 0;
    
    ret = vtoy_json_get_int(json, "partstyle", &style);
    if (JSON_SUCCESS == ret)
    {
        if ((style == 0) || (style == 1))
        {
            g_cur_part_style = style;
            ventoy_http_save_cfg();            
        }
    }

    ventoy_json_result(conn, VTOY_JSON_SUCCESS_RET);
    return 0;    
}

static int ventoy_clean_disk(int fd, uint64_t size)
{
    int zerolen;
    ssize_t len;
    off_t offset;
    void *buf = NULL;
    
    vdebug("ventoy_clean_disk fd:%d size:%llu\n", fd, (_ull)size);

    zerolen = 64 * 1024;
    buf = zalloc(zerolen);
    if (!buf)
    {
        vlog("failed to alloc clean buffer\n");
        return 1;
    }

    offset = lseek(fd, 0, SEEK_SET);
    len = write(fd, buf, zerolen);
    vdebug("write disk at off:%llu writelen:%lld datalen:%d\n", (_ull)offset, (_ll)len, zerolen);

    offset = lseek(fd, size - zerolen, SEEK_SET);
    len = write(fd, buf, zerolen);
    vdebug("write disk at off:%llu writelen:%lld datalen:%d\n", (_ull)offset, (_ll)len, zerolen);

    fsync(fd);

    free(buf);
    return 0;
}

static int ventoy_write_legacy_grub(int fd, int partstyle)
{
    ssize_t len;
    off_t offset;
    
    if (partstyle)
    {
        vlog("Write GPT stage1 ...\n");

        offset = lseek(fd, 512 * 34, SEEK_SET);

        g_grub_stg1_raw_img[500] = 35;//update blocklist
        len = write(fd, g_grub_stg1_raw_img, SIZE_1MB - 512 * 34);

        vlog("lseek offset:%llu(%u) writelen:%llu(%u)\n", (_ull)offset, 512 * 34, (_ull)len, SIZE_1MB - 512 * 34);
        if (SIZE_1MB - 512 * 34 != len)
        {
            vlog("write length error\n");
            return 1;
        }
    }
    else
    {
        vlog("Write MBR stage1 ...\n");
        offset = lseek(fd, 512, SEEK_SET);
        len = write(fd, g_grub_stg1_raw_img, SIZE_1MB - 512);
        
        vlog("lseek offset:%llu(%u) writelen:%llu(%u)\n", (_ull)offset, 512, (_ull)len, SIZE_1MB - 512);
        if (SIZE_1MB - 512 != len)
        {
            vlog("write length error\n");
            return 1;
        }
    }

    return 0;
}

static int VentoyFatMemRead(uint32 Sector, uint8 *Buffer, uint32 SectorCount)
{
	uint32 i;
	uint32 offset;

	for (i = 0; i < SectorCount; i++)
	{
		offset = (Sector + i) * 512;
        memcpy(Buffer + i * 512, g_efi_part_raw_img + offset, 512);
	}

	return 1;
}

static int VentoyFatMemWrite(uint32 Sector, uint8 *Buffer, uint32 SectorCount)
{
	uint32 i;
	uint32 offset;

	for (i = 0; i < SectorCount; i++)
	{
		offset = (Sector + i) * 512;
        memcpy(g_efi_part_raw_img + offset, Buffer + i * 512, 512);
	}

	return 1;
}

static int VentoyProcSecureBoot(int SecureBoot)
{
	int rc = 0;
	int size;
	char *filebuf = NULL;
	void *file = NULL;

	vlog("VentoyProcSecureBoot %d ...\n", SecureBoot);
	
	if (SecureBoot)
	{
		vlog("Secure boot is enabled ...\n");
		return 0;
	}

	fl_init();

	if (0 == fl_attach_media(VentoyFatMemRead, VentoyFatMemWrite))
	{
		file = fl_fopen("/EFI/BOOT/grubx64_real.efi", "rb");
		vlog("Open ventoy efi file %p \n", file);
		if (file)
		{
			fl_fseek(file, 0, SEEK_END);
			size = (int)fl_ftell(file);
			fl_fseek(file, 0, SEEK_SET);

			vlog("ventoy efi file size %d ...\n", size);

			filebuf = (char *)malloc(size);
			if (filebuf)
			{
				fl_fread(filebuf, 1, size, file);
			}

			fl_fclose(file);

			vlog("Now delete all efi files ...\n");
			fl_remove("/EFI/BOOT/BOOTX64.EFI");
			fl_remove("/EFI/BOOT/grubx64.efi");
			fl_remove("/EFI/BOOT/grubx64_real.efi");
			fl_remove("/EFI/BOOT/MokManager.efi");
			fl_remove("/EFI/BOOT/mmx64.efi");
            fl_remove("/ENROLL_THIS_KEY_IN_MOKMANAGER.cer");

			file = fl_fopen("/EFI/BOOT/BOOTX64.EFI", "wb");
			vlog("Open bootx64 efi file %p \n", file);
			if (file)
			{
				if (filebuf)
				{
					fl_fwrite(filebuf, 1, size, file);
				}
				
				fl_fflush(file);
				fl_fclose(file);
			}

			if (filebuf)
			{
				free(filebuf);
			}
		}

        file = fl_fopen("/EFI/BOOT/grubia32_real.efi", "rb");
        vlog("Open ventoy efi file %p\n", file);
        if (file)
        {
            fl_fseek(file, 0, SEEK_END);
            size = (int)fl_ftell(file);
            fl_fseek(file, 0, SEEK_SET);

            vlog("ventoy efi file size %d ...\n", size);

            filebuf = (char *)malloc(size);
            if (filebuf)
            {
                fl_fread(filebuf, 1, size, file);
            }

            fl_fclose(file);

            vlog("Now delete all efi files ...\n");
            fl_remove("/EFI/BOOT/BOOTIA32.EFI");
            fl_remove("/EFI/BOOT/grubia32.efi");
            fl_remove("/EFI/BOOT/grubia32_real.efi");
            fl_remove("/EFI/BOOT/mmia32.efi");            

            file = fl_fopen("/EFI/BOOT/BOOTIA32.EFI", "wb");
            vlog("Open bootia32 efi file %p\n", file);
            if (file)
            {
                if (filebuf)
                {
                    fl_fwrite(filebuf, 1, size, file);
                }

                fl_fflush(file);
                fl_fclose(file);
            }

            if (filebuf)
            {
                free(filebuf);
            }
        }

	}
	else
	{
		rc = 1;
	}

	fl_shutdown();

	return rc;
}

static int ventoy_check_efi_part_data(int fd, uint64_t offset)
{
    int i;
    ssize_t len;
    char *buf;

    buf = malloc(SIZE_1MB);
    if (!buf)
    {
        return 0;
    }
    
    lseek(fd, offset, SEEK_SET);
    for (i = 0; i < 32; i++)
    {
        len = read(fd, buf, SIZE_1MB);
        if (len != SIZE_1MB || memcmp(buf, g_efi_part_raw_img + i * SIZE_1MB, SIZE_1MB))
        {
            vlog("part2 data check failed i=%d len:%llu\n", i, (_ull)len);
            return 1;
        }

        g_current_progress = PT_CHECK_PART2 + (i / 4);
    }

    return 0;
}

static int ventoy_write_efipart(int fd, uint64_t offset, uint32_t secureboot)
{
    int i;
    ssize_t len;
    
    vlog("Formatting part2 EFI offset:%llu ...\n", (_ull)offset);
    lseek(fd, offset, SEEK_SET);

    VentoyProcSecureBoot((int)secureboot);

    g_current_progress = PT_WRITE_VENTOY_START;
    for (i = 0; i < 32; i++)
    {
        len = write(fd, g_efi_part_raw_img + i * SIZE_1MB, SIZE_1MB);
        vlog("write disk writelen:%lld datalen:%d [ %s ]\n", 
            (_ll)len, SIZE_1MB, (len == SIZE_1MB) ? "success" : "failed");
        
        if (len != SIZE_1MB)
        {
            vlog("failed to format part2 EFI\n");
            return 1;
        }
    
        g_current_progress = PT_WRITE_VENTOY_START + i / 4;
    }

    return 0;
}

static int VentoyFillBackupGptHead(VTOY_GPT_INFO *pInfo, VTOY_GPT_HDR *pHead)
{
    uint64_t LBA;
    uint64_t BackupLBA;

    memcpy(pHead, &pInfo->Head, sizeof(VTOY_GPT_HDR));

    LBA = pHead->EfiStartLBA;
    BackupLBA = pHead->EfiBackupLBA;
    
    pHead->EfiStartLBA = BackupLBA;
    pHead->EfiBackupLBA = LBA;
    pHead->PartTblStartLBA = BackupLBA + 1 - 33;

    pHead->Crc = 0;
    pHead->Crc = ventoy_crc32(pHead, pHead->Length);

    return 0;
}

static int ventoy_write_gpt_part_table(int fd, uint64_t disksize, VTOY_GPT_INFO *gpt)
{
    ssize_t len;
    off_t offset;
    VTOY_GPT_HDR BackupHead;
    
    VentoyFillBackupGptHead(gpt, &BackupHead);

    offset = lseek(fd, disksize - 512, SEEK_SET);
    len = write(fd, &BackupHead, sizeof(VTOY_GPT_HDR));
    vlog("write backup gpt part table off:%llu len:%llu\n", (_ull)offset, (_ull)len);
    if (offset != disksize - 512 || len != sizeof(VTOY_GPT_HDR))
    {
        return 1;
    }

    offset = lseek(fd, disksize - 512 * 33, SEEK_SET);
    len = write(fd, gpt->PartTbl, sizeof(gpt->PartTbl));
    vlog("write main gpt part table off:%llu len:%llu\n", (_ull)offset, (_ull)len);
    if (offset != disksize - 512 * 33 || len != sizeof(gpt->PartTbl))
    {
        return 1;
    }
    
    offset = lseek(fd, 0, SEEK_SET);
    len = write(fd, gpt, sizeof(VTOY_GPT_INFO));
    vlog("write gpt part head off:%llu len:%llu\n", (_ull)offset, (_ull)len);
    if (offset != 0 || len != sizeof(VTOY_GPT_INFO))
    {
        return 1;
    }
    
    return 0;
}

static int ventoy_mbr_need_update(ventoy_disk *disk, MBR_HEAD *mbr)
{
    int update = 0;
    int partition_style;
    MBR_HEAD LocalMBR;

    partition_style = disk->vtoydata.partition_style;
    memcpy(mbr, &(disk->vtoydata.gptinfo.MBR), 512);
    
    VentoyGetLocalBootImg(&LocalMBR);
    memcpy(LocalMBR.BootCode + 0x180, mbr->BootCode + 0x180, 16);
    if (partition_style)
    {
        LocalMBR.BootCode[92] = 0x22;        
    }

    if (memcmp(LocalMBR.BootCode, mbr->BootCode, 440))
    {
        memcpy(mbr->BootCode, LocalMBR.BootCode, 440);
        vlog("MBR boot code different, must update it.\n");
        update = 1;
    }

    if (partition_style == 0 && mbr->PartTbl[0].Active == 0)
    {
        mbr->PartTbl[0].Active = 0x80;
        mbr->PartTbl[1].Active = 0;
        mbr->PartTbl[2].Active = 0;
        mbr->PartTbl[3].Active = 0;
        vlog("set MBR partition 1 active flag enabled\n");
        update = 1;
    }

    return update;
}

static void * ventoy_update_thread(void *data)
{
    int fd;
    ssize_t len;
    off_t offset;
    MBR_HEAD MBR;
    ventoy_disk *disk = NULL;
    ventoy_thread_data *thread = (ventoy_thread_data *)data;
    VTOY_GPT_INFO *pstGPT = NULL;

    vdebug("ventoy_update_thread run ...\n");

    fd = thread->diskfd;
    disk = thread->disk;

    g_current_progress = PT_PRAPARE_FOR_CLEAN;
    vdebug("check disk %s\n", disk->disk_name);
    if (ventoy_is_disk_mounted(disk->disk_path))
    {
        vlog("disk is mounted, now try to unmount it ...\n");
        ventoy_try_umount_disk(disk->disk_path);
    }

    if (ventoy_is_disk_mounted(disk->disk_path))
    {
        vlog("%s is mounted and can't umount!\n", disk->disk_path);
        goto err;
    }
    else
    {
        vlog("disk is not mounted now, we can do continue ...\n");
    }

    g_current_progress = PT_LOAD_CORE_IMG;
    ventoy_unxz_stg1_img();
    
    g_current_progress = PT_LOAD_DISK_IMG;
    ventoy_unxz_efipart_img();

    g_current_progress = PT_FORMAT_PART2;

    vlog("Formatting part2 EFI ...\n");
    if (0 != ventoy_write_efipart(fd, disk->vtoydata.part2_start_sector * 512, thread->secure_boot))
    {
        vlog("Failed to format part2 efi ...\n");
        goto err;
    }

    g_current_progress = PT_WRITE_STG1_IMG;

    vlog("Writing legacy grub ...\n");
    if (0 != ventoy_write_legacy_grub(fd, disk->vtoydata.partition_style))
    {
        vlog("ventoy_write_legacy_grub failed ...\n");
        goto err;
    }

    offset = lseek(fd, 512 * 2040, SEEK_SET);
    len = write(fd, disk->vtoydata.rsvdata, sizeof(disk->vtoydata.rsvdata));
    vlog("Writing reserve data offset:%llu len:%llu ...\n", (_ull)offset, (_ull)len);

    if (ventoy_mbr_need_update(disk, &MBR))
    {
        offset = lseek(fd, 0, SEEK_SET);
        len = write(fd, &MBR, 512);
        vlog("update MBR offset:%llu len:%llu\n", (_ull)offset, (_ull)len);
    }
    else
    {
        vlog("No need to update MBR\n");
    }

    
    if (disk->vtoydata.partition_style)
    {
        pstGPT = (VTOY_GPT_INFO *)malloc(sizeof(VTOY_GPT_INFO));
        memset(pstGPT, 0, sizeof(VTOY_GPT_INFO));
            
        offset = lseek(fd, 0, SEEK_SET);
        len = read(fd, pstGPT, sizeof(VTOY_GPT_INFO));
        vlog("Read GPT table offset:%llu len:%llu ...\n", (_ull)offset, (_ull)len);

        if (pstGPT->PartTbl[1].Attr != 0x8000000000000000ULL)
        {
            vlog("Update EFI part attr from 0x%016llx to 0x%016llx\n", 
                 pstGPT->PartTbl[1].Attr, 0x8000000000000000ULL);

            pstGPT->PartTbl[1].Attr = 0x8000000000000000ULL;

            pstGPT->Head.PartTblCrc = ventoy_crc32(pstGPT->PartTbl, sizeof(pstGPT->PartTbl));
            pstGPT->Head.Crc = 0;
            pstGPT->Head.Crc = ventoy_crc32(&(pstGPT->Head), pstGPT->Head.Length);            
            ventoy_write_gpt_part_table(fd, disk->size_in_byte, pstGPT);
        }
        else
        {
            vlog("No need to update EFI part attr\n");
        }
        free(pstGPT);
    }
    

    g_current_progress = PT_SYNC_DATA1;

    vlog("fsync data1...\n");
    fsync(fd);
    vtoy_safe_close_fd(fd);

    g_current_progress = PT_SYNC_DATA2;

    vlog("====================================\n");
    vlog("====== ventoy update success ======\n");
    vlog("====================================\n");
    goto end;

err:
    g_cur_process_result = 1;
    vtoy_safe_close_fd(fd);        

end:
    g_current_progress = PT_FINISH;

    check_free(thread);
    
    return NULL;
}

static void * ventoy_install_thread(void *data)
{
    int fd;
    ssize_t len;
    off_t offset;
    MBR_HEAD MBR;
    ventoy_disk *disk = NULL;
    VTOY_GPT_INFO *gpt = NULL;
    ventoy_thread_data *thread = (ventoy_thread_data *)data;
    uint64_t Part1StartSector = 0;
    uint64_t Part1SectorCount = 0;
    uint64_t Part2StartSector = 0;

    vdebug("ventoy_install_thread run ...\n");

    fd = thread->diskfd;
    disk = thread->disk;

    g_current_progress = PT_PRAPARE_FOR_CLEAN;
    vdebug("check disk %s\n", disk->disk_name);
    if (ventoy_is_disk_mounted(disk->disk_path))
    {
        vlog("disk is mounted, now try to unmount it ...\n");
        ventoy_try_umount_disk(disk->disk_path);
    }

    if (ventoy_is_disk_mounted(disk->disk_path))
    {
        vlog("%s is mounted and can't umount!\n", disk->disk_path);
        goto err;
    }
    else
    {
        vlog("disk is not mounted now, we can do continue ...\n");
    }

    g_current_progress = PT_DEL_ALL_PART;
    ventoy_clean_disk(fd, disk->size_in_byte);
    
    g_current_progress = PT_LOAD_CORE_IMG;
    ventoy_unxz_stg1_img();
    
    g_current_progress = PT_LOAD_DISK_IMG;
    ventoy_unxz_efipart_img();

    if (thread->partstyle)
    {
        vdebug("Fill GPT part table\n");
        gpt = zalloc(sizeof(VTOY_GPT_INFO));
        ventoy_fill_gpt(disk->size_in_byte, thread->reserveBytes, thread->align4kb, gpt);
        Part1StartSector = gpt->PartTbl[0].StartLBA;
        Part1SectorCount = gpt->PartTbl[0].LastLBA - Part1StartSector + 1;
        Part2StartSector = gpt->PartTbl[1].StartLBA;
    }
    else
    {
        vdebug("Fill MBR part table\n");
        ventoy_fill_mbr(disk->size_in_byte, thread->reserveBytes, thread->align4kb, &MBR);
        Part1StartSector = MBR.PartTbl[0].StartSectorId;
        Part1SectorCount = MBR.PartTbl[0].SectorCount;
        Part2StartSector = MBR.PartTbl[1].StartSectorId;
    }

    vlog("Part1StartSector:%llu Part1SectorCount:%llu Part2StartSector:%llu\n", 
        (_ull)Part1StartSector, (_ull)Part1SectorCount, (_ull)Part2StartSector);

    if (thread->partstyle != disk->partstyle)
    {
        vlog("Wait for format part1 (partstyle changed) ...\n");
        sleep(1);
    }

    g_current_progress = PT_FORMAT_PART1;
    vlog("Formatting part1 exFAT %s ...\n", disk->disk_path);
    if (0 != mkexfat_main(disk->disk_path, fd, Part1SectorCount))
    {
        vlog("Failed to format exfat ...\n");
        goto err;
    }

    g_current_progress = PT_FORMAT_PART2;
    vlog("Formatting part2 EFI ...\n");
    if (0 != ventoy_write_efipart(fd, Part2StartSector * 512, thread->secure_boot))
    {
        vlog("Failed to format part2 efi ...\n");
        goto err;
    }

    g_current_progress = PT_WRITE_STG1_IMG;
    vlog("Writing legacy grub ...\n");
    if (0 != ventoy_write_legacy_grub(fd, thread->partstyle))
    {
        vlog("ventoy_write_legacy_grub failed ...\n");
        goto err;
    }

    g_current_progress = PT_SYNC_DATA1;
    vlog("fsync data1...\n");
    fsync(fd);
    vtoy_safe_close_fd(fd);

    /* reopen for check part2 data */
    vlog("Checking part2 efi data %s ...\n", disk->disk_path);
    g_current_progress = PT_CHECK_PART2;
    fd = open(disk->disk_path, O_RDONLY | O_BINARY);
    if (fd < 0)
    {
        vlog("failed to open %s for check fd:%d err:%d\n", disk->disk_path, fd, errno);
        goto err;
    }

    if (0 == ventoy_check_efi_part_data(fd, Part2StartSector * 512))
    {
        vlog("efi part data check success\n");
    }
    else
    {
        vlog("efi part data check failed\n");
        goto err;
    }

    vtoy_safe_close_fd(fd);
    
    /* reopen for write part table */
    g_current_progress = PT_WRITE_PART_TABLE;
    vlog("Writting Partition Table style:%d...\n", thread->partstyle);

    fd = open(disk->disk_path, O_RDWR | O_BINARY);
    if (fd < 0)
    {
        vlog("failed to open %s for part table fd:%d err:%d\n", disk->disk_path, fd, errno);
        goto err;
    }

    if (thread->partstyle)
    {
        ventoy_write_gpt_part_table(fd, disk->size_in_byte, gpt);
    }
    else
    {
        offset = lseek(fd, 0, SEEK_SET);
        len = write(fd, &MBR, 512);
        vlog("Writting MBR Partition Table %llu %llu\n", (_ull)offset, (_ull)len);
        if (offset != 0 || len != 512)
        {
            goto err;
        }
    }

    g_current_progress = PT_SYNC_DATA2;
    vlog("fsync data2...\n");
    fsync(fd);
    vtoy_safe_close_fd(fd);


    vlog("====================================\n");
    vlog("====== ventoy install success ======\n");
    vlog("====================================\n");
    goto end;

err:
    g_cur_process_result = 1;
    vtoy_safe_close_fd(fd);        

end:
    g_current_progress = PT_FINISH;

    check_free(gpt);
    check_free(thread);
    
    return NULL;
}

static int ventoy_api_clean(struct mg_connection *conn, VTOY_JSON *json)
{
    int i = 0;
    int fd = 0;
    ventoy_disk *disk = NULL;
    const char *diskname = NULL;
    char path[128];
    
    if (g_current_progress != PT_FINISH)
    {
        ventoy_json_result(conn, VTOY_JSON_BUSY_RET);
        return 0;  
    }
    
    diskname = vtoy_json_get_string_ex(json, "disk");
    if (diskname == NULL)
    {
        ventoy_json_result(conn, VTOY_JSON_INVALID_RET);
        return 0;
    }

    for (i = 0; i < g_disk_num; i++)
    {
        if (strcmp(g_disk_list[i].disk_name, diskname) == 0)
        {
            disk = g_disk_list + i;
            break;
        }
    }

    if (disk == NULL)
    {
        vlog("disk %s not found\n", diskname);
        ventoy_json_result(conn, VTOY_JSON_NOTFOUND_RET);
        return 0;
    }

    scnprintf(path, "/sys/block/%s", diskname);
    if (access(path, F_OK) < 0)
    {
        vlog("File %s not exist anymore\n", path);
        ventoy_json_result(conn, VTOY_JSON_NOTFOUND_RET);
        return 0;
    }

    vlog("==================================\n");
    vlog("===== ventoy clean %s =====\n", disk->disk_path);
    vlog("==================================\n");

    if (ventoy_is_disk_mounted(disk->disk_path))
    {
        vlog("disk is mounted, now try to unmount it ...\n");
        ventoy_try_umount_disk(disk->disk_path);
    }

    if (ventoy_is_disk_mounted(disk->disk_path))
    {
        vlog("%s is mounted and can't umount!\n", disk->disk_path);
        ventoy_json_result(conn, VTOY_JSON_FAILED_RET);
        return 0;
    }
    else
    {
        vlog("disk is not mounted now, we can do the clean ...\n");
    }

    fd = open(disk->disk_path, O_RDWR | O_BINARY);
    if (fd < 0)
    {
        vlog("failed to open %s fd:%d err:%d\n", disk->disk_path, fd, errno);
        ventoy_json_result(conn, VTOY_JSON_FAILED_RET);
        return 0;
    }

    vdebug("start clean %s ...\n", disk->disk_model);
    ventoy_clean_disk(fd, disk->size_in_byte);    

    vtoy_safe_close_fd(fd);
    
    ventoy_json_result(conn, VTOY_JSON_SUCCESS_RET);
    return 0;    
}

static int ventoy_api_install(struct mg_connection *conn, VTOY_JSON *json)
{
    int i = 0;
    int ret = 0;
    int fd = 0;
    uint32_t align4kb = 0;
    uint32_t style = 0;
    uint32_t secure_boot = 0;
    uint64_t reserveBytes = 0;
    ventoy_disk *disk = NULL;
    const char *diskname = NULL;
    const char *reserve_space = NULL;
    ventoy_thread_data *thread = NULL;
    char path[128];
    
    if (g_current_progress != PT_FINISH)
    {
        ventoy_json_result(conn, VTOY_JSON_BUSY_RET);
        return 0;  
    }
    
    diskname = vtoy_json_get_string_ex(json, "disk");
    reserve_space = vtoy_json_get_string_ex(json, "reserve_space");
    ret += vtoy_json_get_uint(json, "partstyle", &style);
    ret += vtoy_json_get_uint(json, "secure_boot", &secure_boot);
    ret += vtoy_json_get_uint(json, "align_4kb", &align4kb);

    if (diskname == NULL || reserve_space == NULL || ret != JSON_SUCCESS)
    {
        ventoy_json_result(conn, VTOY_JSON_INVALID_RET);
        return 0;
    }

    reserveBytes = (uint64_t)strtoull(reserve_space, NULL, 10);

    for (i = 0; i < g_disk_num; i++)
    {
        if (strcmp(g_disk_list[i].disk_name, diskname) == 0)
        {
            disk = g_disk_list + i;
            break;
        }
    }

    if (disk == NULL)
    {
        vlog("disk %s not found\n", diskname);
        ventoy_json_result(conn, VTOY_JSON_NOTFOUND_RET);
        return 0;
    }

    if (disk->is4kn)
    {
        vlog("disk %s is 4k native, not supported.\n", diskname);
        ventoy_json_result(conn, VTOY_JSON_4KN_RET);
        return 0;
    }

    scnprintf(path, "/sys/block/%s", diskname);
    if (access(path, F_OK) < 0)
    {
        vlog("File %s not exist anymore\n", path);
        ventoy_json_result(conn, VTOY_JSON_NOTFOUND_RET);
        return 0;
    }

    if (disk->size_in_byte > 2199023255552ULL && style == 0)
    {
        vlog("disk %s is more than 2TB and GPT is needed\n", path);
        ventoy_json_result(conn, VTOY_JSON_MBR_2TB_RET);
        return 0;
    }

    if ((reserveBytes + VTOYEFI_PART_BYTES * 2) > disk->size_in_byte)
    {
        vlog("reserve space %llu is too big for disk %s %llu\n", (_ull)reserveBytes, path, (_ull)disk->size_in_byte);
        ventoy_json_result(conn, VTOY_JSON_INVALID_RSV_RET);
        return 0;
    }

    vlog("==================================================================================\n");
    vlog("===== ventoy install %s style:%s secureboot:%u align4K:%u reserve:%llu =========\n",
         disk->disk_path, (style ? "GPT" : "MBR"), secure_boot, align4kb, (_ull)reserveBytes);
    vlog("==================================================================================\n");

    if (ventoy_is_disk_mounted(disk->disk_path))
    {
        vlog("disk is mounted, now try to unmount it ...\n");
        ventoy_try_umount_disk(disk->disk_path);
    }

    if (ventoy_is_disk_mounted(disk->disk_path))
    {
        vlog("%s is mounted and can't umount!\n", disk->disk_path);
        ventoy_json_result(conn, VTOY_JSON_FAILED_RET);
        return 0;
    }
    else
    {
        vlog("disk is not mounted now, we can do the install ...\n");
    }

    fd = open(disk->disk_path, O_RDWR | O_BINARY);
    if (fd < 0)
    {
        vlog("failed to open %s fd:%d err:%d\n", disk->disk_path, fd, errno);
        ventoy_json_result(conn, VTOY_JSON_FAILED_RET);
        return 0;
    }

    vdebug("start install thread %s ...\n", disk->disk_model);
    thread = zalloc(sizeof(ventoy_thread_data));
    if (!thread)
    {
        vtoy_safe_close_fd(fd);
        vlog("failed to alloc thread data err:%d\n", errno);
        ventoy_json_result(conn, VTOY_JSON_FAILED_RET);
        return 0;
    }
    
    g_current_progress = PT_START;
    g_cur_process_result = 0;
    scnprintf(g_cur_process_type, "%s", "install");
    scnprintf(g_cur_process_diskname, "%s", disk->disk_name);

    thread->disk = disk;
    thread->diskfd = fd;
    thread->align4kb = align4kb;
    thread->partstyle = style;
    thread->secure_boot = secure_boot;
    thread->reserveBytes = reserveBytes;
    
    mg_start_thread(ventoy_install_thread, thread);
    
    ventoy_json_result(conn, VTOY_JSON_SUCCESS_RET);
    return 0;    
}

static int ventoy_api_update(struct mg_connection *conn, VTOY_JSON *json)
{
    int i = 0;
    int ret = 0;
    int fd = 0;
    uint32_t secure_boot = 0;
    ventoy_disk *disk = NULL;
    const char *diskname = NULL;
    ventoy_thread_data *thread = NULL;
    char path[128];
    
    if (g_current_progress != PT_FINISH)
    {
        ventoy_json_result(conn, VTOY_JSON_BUSY_RET);
        return 0;  
    }
    
    diskname = vtoy_json_get_string_ex(json, "disk");
    ret += vtoy_json_get_uint(json, "secure_boot", &secure_boot);
    if (diskname == NULL || ret != JSON_SUCCESS)
    {
        ventoy_json_result(conn, VTOY_JSON_INVALID_RET);
        return 0;
    }

    for (i = 0; i < g_disk_num; i++)
    {
        if (strcmp(g_disk_list[i].disk_name, diskname) == 0)
        {
            disk = g_disk_list + i;
            break;
        }
    }

    if (disk == NULL)
    {
        vlog("disk %s not found\n", diskname);
        ventoy_json_result(conn, VTOY_JSON_NOTFOUND_RET);
        return 0;
    }

    if (disk->vtoydata.ventoy_valid == 0)
    {
        vlog("disk %s is not ventoy disk\n", diskname);
        ventoy_json_result(conn, VTOY_JSON_FAILED_RET);
        return 0;
    }

    scnprintf(path, "/sys/block/%s", diskname);
    if (access(path, F_OK) < 0)
    {
        vlog("File %s not exist anymore\n", path);
        ventoy_json_result(conn, VTOY_JSON_NOTFOUND_RET);
        return 0;
    }

    vlog("==========================================================\n");
    vlog("===== ventoy update %s new_secureboot:%u =========\n", disk->disk_path, secure_boot);
    vlog("==========================================================\n");

    vlog("%s version:%s partstyle:%u oldsecureboot:%u reserve:%llu\n", 
        disk->disk_path, disk->vtoydata.ventoy_ver, 
        disk->vtoydata.partition_style,
        disk->vtoydata.secure_boot_flag,
        (_ull)(disk->vtoydata.preserved_space)
        );

    if (ventoy_is_disk_mounted(disk->disk_path))
    {
        vlog("disk is mounted, now try to unmount it ...\n");
        ventoy_try_umount_disk(disk->disk_path);
    }

    if (ventoy_is_disk_mounted(disk->disk_path))
    {
        vlog("%s is mounted and can't umount!\n", disk->disk_path);
        ventoy_json_result(conn, VTOY_JSON_FAILED_RET);
        return 0;
    }
    else
    {
        vlog("disk is not mounted now, we can do the update ...\n");
    }

    fd = open(disk->disk_path, O_RDWR | O_BINARY);
    if (fd < 0)
    {
        vlog("failed to open %s fd:%d err:%d\n", disk->disk_path, fd, errno);
        ventoy_json_result(conn, VTOY_JSON_FAILED_RET);
        return 0;
    }

    vdebug("start update thread %s ...\n", disk->disk_model);
    thread = zalloc(sizeof(ventoy_thread_data));
    if (!thread)
    {
        vtoy_safe_close_fd(fd);
        vlog("failed to alloc thread data err:%d\n", errno);
        ventoy_json_result(conn, VTOY_JSON_FAILED_RET);
        return 0;
    }
    
    g_current_progress = PT_START;
    g_cur_process_result = 0;
    scnprintf(g_cur_process_type, "%s", "update");
    scnprintf(g_cur_process_diskname, "%s", disk->disk_name);

    thread->disk = disk;
    thread->diskfd = fd;
    thread->secure_boot = secure_boot;
    
    mg_start_thread(ventoy_update_thread, thread);
    
    ventoy_json_result(conn, VTOY_JSON_SUCCESS_RET);
    return 0;    
}


static int ventoy_api_refresh_device(struct mg_connection *conn, VTOY_JSON *json)
{
    (void)json;

    if (g_current_progress == PT_FINISH)
    {
        g_disk_num = 0;
        ventoy_disk_enumerate_all();
    }

    ventoy_json_result(conn, VTOY_JSON_SUCCESS_RET);
    return 0;
}

static int ventoy_api_get_dev_list(struct mg_connection *conn, VTOY_JSON *json)
{
    int i = 0;
    int rc = 0;
    int pos = 0;
    int buflen = 0;
    uint32_t alldev = 0;
    char *buf = NULL;
    ventoy_disk *cur = NULL;
    
    rc = vtoy_json_get_uint(json, "alldev", &alldev);
    if (JSON_SUCCESS != rc)
    {
        alldev = 0;
    }

    buflen = g_disk_num * 1024;
    buf = (char *)malloc(buflen + 1024);
    if (!buf)
    {
        ventoy_json_result(conn, VTOY_JSON_FAILED_RET);
        return 0;
    }

    VTOY_JSON_FMT_BEGIN(pos, buf, buflen);
    VTOY_JSON_FMT_OBJ_BEGIN();
    VTOY_JSON_FMT_KEY("list");
    VTOY_JSON_FMT_ARY_BEGIN();

    for (i = 0; i < g_disk_num; i++)
    {
        cur = g_disk_list + i;

        if (alldev == 0 && cur->type != VTOY_DEVICE_USB)
        {
            continue;
        }
        
        VTOY_JSON_FMT_OBJ_BEGIN();
        VTOY_JSON_FMT_STRN("name", cur->disk_name);
        VTOY_JSON_FMT_STRN("model", cur->disk_model);
        VTOY_JSON_FMT_STRN("size", cur->human_readable_size);
        VTOY_JSON_FMT_UINT("vtoy_valid", cur->vtoydata.ventoy_valid);
        VTOY_JSON_FMT_STRN("vtoy_ver", cur->vtoydata.ventoy_ver);
        VTOY_JSON_FMT_UINT("vtoy_secure_boot", cur->vtoydata.secure_boot_flag);
        VTOY_JSON_FMT_UINT("vtoy_partstyle", cur->vtoydata.partition_style);
        VTOY_JSON_FMT_OBJ_ENDEX();
    }
    
    VTOY_JSON_FMT_ARY_END();
    VTOY_JSON_FMT_OBJ_END();
    VTOY_JSON_FMT_END(pos);

    ventoy_json_buffer(conn, buf, pos);
    return 0;
}

static JSON_CB g_ventoy_json_cb[] = 
{
    { "sysinfo",        ventoy_api_sysinfo        },
    { "sel_language",   ventoy_api_set_language   },
    { "sel_partstyle",  ventoy_api_set_partstyle  },
    { "refresh_device", ventoy_api_refresh_device },
    { "get_dev_list",   ventoy_api_get_dev_list   },
    { "install",        ventoy_api_install        },
    { "update",         ventoy_api_update         },
    { "clean",          ventoy_api_clean          },
    { "get_percent",    ventoy_api_get_percent    },
};

static int ventoy_json_handler(struct mg_connection *conn, VTOY_JSON *json)
{
    int i;
    const char *token = NULL;
    const char *method = NULL;

    method = vtoy_json_get_string_ex(json, "method");
    if (!method)
    {
        ventoy_json_result(conn, VTOY_JSON_SUCCESS_RET);
        return 0;
    }

    if (strcmp(method, "sysinfo"))
    {
        token = vtoy_json_get_string_ex(json, "token");
        if (token == NULL || strcmp(token, g_cur_server_token))
        {
            ventoy_json_result(conn, VTOY_JSON_TOKEN_ERR_RET);
            return 0;
        }
    }

    for (i = 0; i < (int)(sizeof(g_ventoy_json_cb) / sizeof(g_ventoy_json_cb[0])); i++)
    {
        if (strcmp(method, g_ventoy_json_cb[i].method) == 0)
        {
            g_ventoy_json_cb[i].callback(conn, json);
            break;
        }
    }

    return 0;
}

int ventoy_func_handler(const char *jsonstr, char *jsonbuf, int buflen)
{
    int i;
    const char *method = NULL;
    VTOY_JSON *json = NULL;

    g_pub_out_buf = jsonbuf;
    g_pub_out_max = buflen;

    json = vtoy_json_create();
    if (JSON_SUCCESS == vtoy_json_parse(json, jsonstr))
    {
        pthread_mutex_lock(&g_api_mutex);

        method = vtoy_json_get_string_ex(json->pstChild, "method");
        for (i = 0; i < (int)(sizeof(g_ventoy_json_cb) / sizeof(g_ventoy_json_cb[0])); i++)
        {
            if (method && strcmp(method, g_ventoy_json_cb[i].method) == 0)
            {
                g_ventoy_json_cb[i].callback(NULL, json->pstChild);
                break;
            }
        }

        pthread_mutex_unlock(&g_api_mutex);
    }
    else
    {
        ventoy_json_result(NULL, VTOY_JSON_INVALID_RET);
    }

    vtoy_json_destroy(json);
    return 0;
}

static int ventoy_request_handler(struct mg_connection *conn)
{
    int post_data_len;
    int post_buf_len;
    VTOY_JSON *json = NULL;
    char *post_data_buf = NULL;
    const struct mg_request_info *ri = NULL;
    char stack_buf[512];
    
    ri = mg_get_request_info(conn);    

    if (strcmp(ri->uri, "/vtoy/json") == 0)
    {
        if (ri->content_length > 500)
        {
            post_data_buf = malloc(ri->content_length + 4);
            post_buf_len  = ri->content_length + 1;
        }
        else
        {
            post_data_buf = stack_buf;
            post_buf_len = sizeof(stack_buf);
        }
        
        post_data_len = mg_read(conn, post_data_buf, post_buf_len);
        post_data_buf[post_data_len] = 0;

        json = vtoy_json_create();
        if (JSON_SUCCESS == vtoy_json_parse(json, post_data_buf))
        {
            pthread_mutex_lock(&g_api_mutex);
            ventoy_json_handler(conn, json->pstChild);
            pthread_mutex_unlock(&g_api_mutex);
        }
        else
        {
            ventoy_json_result(conn, VTOY_JSON_INVALID_RET);
        }

        vtoy_json_destroy(json);

        if (post_data_buf != stack_buf)
        {
            free(post_data_buf);
        }
        return 1;
    }
    else
    {
        return 0;
    }
}

int ventoy_http_start(const char *ip, const char *port)
{
    uint8_t uuid[16];
    char addr[128];
    struct mg_callbacks callbacks;
    const char *options[] = 
    {
	    "listening_ports",    "24680",
        "document_root",      "WebUI",
        "error_log_file",     g_log_file,
	    "request_timeout_ms", "10000",
	     NULL
    };

    /* unique token */
    ventoy_gen_preudo_uuid(uuid);
    scnprintf(g_cur_server_token, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
        uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
        uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

    /* option */
    scnprintf(addr, "%s:%s", ip, port);
    options[1] = addr;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.begin_request = ventoy_request_handler;
    g_ventoy_http_ctx = mg_start(&callbacks, NULL, options);

    return g_ventoy_http_ctx ? 0 : 1;
}

int ventoy_http_stop(void)
{
    if (g_ventoy_http_ctx)
    {
        mg_stop(g_ventoy_http_ctx);        
    }
    return 0;
}

int ventoy_http_init(void)
{
    pthread_mutex_init(&g_api_mutex, NULL);

    ventoy_http_load_cfg();

    ventoy_load_mbr_template();
    
    return 0;
}

void ventoy_http_exit(void)
{
    pthread_mutex_destroy(&g_api_mutex);

    check_free(g_efi_part_raw_img);
    g_efi_part_raw_img = NULL;    
}


const char * ventoy_code_get_cur_language(void)
{
    return g_cur_language;
}

int ventoy_code_get_cur_part_style(void)
{
    return g_cur_part_style;
}

void ventoy_code_set_cur_part_style(int style)
{
    pthread_mutex_lock(&g_api_mutex);
    
    g_cur_part_style = style;
    ventoy_http_save_cfg();

    pthread_mutex_unlock(&g_api_mutex);
}

int ventoy_code_get_cur_show_all(void)
{
    return g_cur_show_all;
}

void ventoy_code_set_cur_show_all(int show_all)
{
    pthread_mutex_lock(&g_api_mutex);
    
    g_cur_show_all = show_all;
    ventoy_http_save_cfg();

    pthread_mutex_unlock(&g_api_mutex);
}

void ventoy_code_set_cur_language(const char *lang)
{
    pthread_mutex_lock(&g_api_mutex);
    
    scnprintf(g_cur_language, "%s", lang);
    ventoy_http_save_cfg();
    
    pthread_mutex_unlock(&g_api_mutex);
}

void ventoy_code_refresh_device(void)
{
    if (g_current_progress == PT_FINISH)
    {
        g_disk_num = 0;
        ventoy_disk_enumerate_all();
    }
}

int ventoy_code_is_busy(void)
{
    return (g_current_progress == PT_FINISH) ? 0 : 1;
}

int ventoy_code_get_percent(void)
{
    return g_current_progress * 100 / PT_FINISH;
}

int ventoy_code_get_result(void)
{
    return g_cur_process_result;
}

void ventoy_code_save_cfg(void)
{
    ventoy_http_save_cfg();
}
