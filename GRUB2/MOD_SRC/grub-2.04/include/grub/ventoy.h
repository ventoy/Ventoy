/******************************************************************************
 * ventoy.h
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

#ifndef __VENTOY_H__
#define __VENTOY_H__

#define COMPILE_ASSERT(a, expr)  extern char __compile_assert##a[(expr) ? 1 : -1]

#define VENTOY_COMPATIBLE_STR      "VENTOY COMPATIBLE"
#define VENTOY_COMPATIBLE_STR_LEN  17

#define VENTOY_GUID { 0x77772020, 0x2e77, 0x6576, { 0x6e, 0x74, 0x6f, 0x79, 0x2e, 0x6e, 0x65, 0x74 }}

typedef enum ventoy_fs_type
{
    ventoy_fs_exfat = 0, /* 0: exfat */
    ventoy_fs_ntfs,      /* 1: NTFS */
    ventoy_fs_ext,       /* 2: ext2/ext3/ext4 */
    ventoy_fs_xfs,       /* 3: XFS */
    ventoy_fs_udf,       /* 4: UDF */
    ventoy_fs_fat,       /* 5: FAT */

    ventoy_fs_max
}ventoy_fs_type;

typedef enum ventoy_chain_type
{
    ventoy_chain_linux = 0, /* 0: linux */
    ventoy_chain_windows,   /* 1: windows */
    ventoy_chain_wim,       /* 2: wim */

    ventoy_chain_max
}ventoy_chain_type;

#pragma pack(1)

typedef struct ventoy_guid
{
    grub_uint32_t   data1;
    grub_uint16_t   data2;
    grub_uint16_t   data3;
    grub_uint8_t    data4[8];
}ventoy_guid;


typedef struct ventoy_image_disk_region
{
    grub_uint32_t   image_sector_count; /* image sectors contained in this region (in 2048) */
    grub_uint32_t   image_start_sector; /* image sector start (in 2048) */
    grub_uint64_t   disk_start_sector;  /* disk sector start (in 512) */
}ventoy_image_disk_region;

typedef struct ventoy_image_location
{
    ventoy_guid  guid;

    /* image sector size, 2048/512 */
    grub_uint32_t   image_sector_size;

    /* disk sector size, normally the value is 512 */
    grub_uint32_t   disk_sector_size;

    grub_uint32_t   region_count;

    /*
     * disk region data (region_count)
     * If the image file has more than one fragments in disk, 
     * there will be more than one region data here.
     *
     */
    ventoy_image_disk_region regions[1];

    /* ventoy_image_disk_region regions[2~region_count-1] */
}ventoy_image_location;


typedef struct ventoy_os_param
{
    ventoy_guid    guid;                  // VENTOY_GUID
    grub_uint8_t   chksum;                // checksum

    grub_uint8_t   vtoy_disk_guid[16];
    grub_uint64_t  vtoy_disk_size;       // disk size in bytes
    grub_uint16_t  vtoy_disk_part_id;    // begin with 1
    grub_uint16_t  vtoy_disk_part_type;  // 0:exfat   1:ntfs  other: reserved
    char           vtoy_img_path[384];   // It seems to be enough, utf-8 format
    grub_uint64_t  vtoy_img_size;        // image file size in bytes

    /* 
     * Ventoy will write a copy of ventoy_image_location data into runtime memory
     * this is the physically address and length of that memory.
     * Address 0 means no such data exist.
     * Address will be aligned by 4KB.
     *
     */
    grub_uint64_t  vtoy_img_location_addr;
    grub_uint32_t  vtoy_img_location_len;

    /* 
     * These 32 bytes are reserved by ventoy.
     *
     * vtoy_reserved[0]: vtoy_break_level
     * vtoy_reserved[1]: vtoy_debug_level
     * vtoy_reserved[2]: vtoy_chain_type     0:Linux    1:Windows  2:wimfile
     * vtoy_reserved[3]: vtoy_iso_format     0:iso9660  1:udf
     * vtoy_reserved[4]: vtoy_windows_cd_prompt
     * vtoy_reserved[5]: vtoy_linux_remount
     * vtoy_reserved[6]: vtoy_vlnk
     * vtoy_reserved[7~10]: vtoy_disk_sig[4] used for vlnk
     *
     */
    grub_uint8_t   vtoy_reserved[32];    // Internal use by ventoy

    grub_uint8_t   vtoy_disk_signature[4];

    grub_uint8_t   reserved[27];
}ventoy_os_param;


typedef struct ventoy_windows_data
{
    char auto_install_script[384];
    char injection_archive[384];
    grub_uint8_t windows11_bypass_check;
    grub_uint8_t reserved[255];
}ventoy_windows_data;


typedef struct ventoy_secure_data
{
    grub_uint8_t magic1[16];     /* VENTOY_GUID */
    grub_uint8_t diskuuid[16];   
    grub_uint8_t Checksum[16];    
    grub_uint8_t adminSHA256[32];
    grub_uint8_t reserved[4000];
    grub_uint8_t magic2[16];     /* VENTOY_GUID */
}ventoy_secure_data;


typedef struct ventoy_vlnk
{
    ventoy_guid   guid;         // VENTOY_GUID
    grub_uint32_t crc32;        // crc32
    grub_uint32_t disk_signature;
    grub_uint64_t part_offset; // in bytes
    char filepath[384];
    grub_uint8_t reserved[96];
}ventoy_vlnk;

#pragma pack()

// compile assert check : sizeof(ventoy_os_param) must be 512
COMPILE_ASSERT(1,sizeof(ventoy_os_param) == 512);
COMPILE_ASSERT(2,sizeof(ventoy_secure_data) == 4096);
COMPILE_ASSERT(3,sizeof(ventoy_vlnk) == 512);







#pragma pack(4)

typedef struct ventoy_chain_head
{
    ventoy_os_param os_param;

    grub_uint32_t disk_drive;
    grub_uint32_t drive_map;
    grub_uint32_t disk_sector_size;

    grub_uint64_t real_img_size_in_bytes;
    grub_uint64_t virt_img_size_in_bytes;
    grub_uint32_t boot_catalog;
    grub_uint8_t  boot_catalog_sector[2048];
    
    grub_uint32_t img_chunk_offset;
    grub_uint32_t img_chunk_num;

    grub_uint32_t override_chunk_offset;
    grub_uint32_t override_chunk_num;

    grub_uint32_t virt_chunk_offset;
    grub_uint32_t virt_chunk_num;
}ventoy_chain_head;

typedef struct ventoy_image_desc
{
    grub_uint64_t disk_size;
    grub_uint64_t part1_size;
    grub_uint8_t  disk_uuid[16];
    grub_uint8_t  disk_signature[4];
    grub_uint32_t img_chunk_count;
    /* ventoy_img_chunk list */
}ventoy_image_desc;



typedef struct ventoy_img_chunk
{
    grub_uint32_t img_start_sector; // sector size: 2KB
    grub_uint32_t img_end_sector;   // included

    grub_uint64_t disk_start_sector; // in disk_sector_size
    grub_uint64_t disk_end_sector;   // included
}ventoy_img_chunk;


typedef struct ventoy_override_chunk
{
    grub_uint64_t img_offset;
    grub_uint32_t override_size;
    grub_uint8_t  override_data[512];
}ventoy_override_chunk;

typedef struct ventoy_virt_chunk
{
    grub_uint32_t mem_sector_start;
    grub_uint32_t mem_sector_end;
    grub_uint32_t mem_sector_offset;
    grub_uint32_t remap_sector_start;
    grub_uint32_t remap_sector_end;
    grub_uint32_t org_sector_start;
}ventoy_virt_chunk;

#define DEFAULT_CHUNK_NUM   1024
typedef struct ventoy_img_chunk_list
{
    grub_uint32_t max_chunk;
    grub_uint32_t cur_chunk;
    ventoy_img_chunk *chunk;
}ventoy_img_chunk_list;


#pragma pack()

#define ventoy_filt_register grub_file_filter_register

#pragma pack(1)

#define GRUB_FILE_REPLACE_MAGIC  0x1258BEEF
#define GRUB_IMG_REPLACE_MAGIC   0x1259BEEF

typedef const char * (*grub_env_get_pf)(const char *name);
typedef int (*grub_env_set_pf)(const char *name, const char *val);
typedef int (*grub_env_printf_pf)(const char *fmt, ...);

typedef struct ventoy_grub_param_file_replace
{
    grub_uint32_t magic;
    char old_file_name[4][256];
    grub_uint32_t old_name_cnt;
    grub_uint32_t new_file_virtual_id;
}ventoy_grub_param_file_replace;

typedef struct ventoy_grub_param
{
    grub_env_get_pf grub_env_get;
    grub_env_set_pf grub_env_set;
    ventoy_grub_param_file_replace file_replace;
    ventoy_grub_param_file_replace img_replace;
    grub_env_printf_pf grub_env_printf;
}ventoy_grub_param;

#pragma pack()

int grub_ext_get_file_chunk(grub_uint64_t part_start, grub_file_t file, ventoy_img_chunk_list *chunk_list);
int grub_fat_get_file_chunk(grub_uint64_t part_start, grub_file_t file, ventoy_img_chunk_list *chunk_list);
void grub_iso9660_set_nojoliet(int nojoliet);
int grub_iso9660_is_joliet(void);
grub_uint64_t grub_iso9660_get_last_read_pos(grub_file_t file);
grub_uint64_t grub_iso9660_get_last_file_dirent_pos(grub_file_t file);
grub_uint64_t grub_udf_get_file_offset(grub_file_t file);
grub_uint64_t grub_udf_get_last_pd_size_offset(void);
grub_uint64_t grub_udf_get_last_file_attr_offset
(
    grub_file_t file, 
    grub_uint32_t *startBlock,
    grub_uint64_t *fe_entry_size_offset
);
int ventoy_is_efi_os(void);

#endif /* __VENTOY_H__ */

