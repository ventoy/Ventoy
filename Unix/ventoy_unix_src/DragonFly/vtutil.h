
#ifndef __UTIL_H__
#define __UTIL_H__

extern int boot_verbose;
//#define vdebug(fmt, ...) 
//#define verror
#define vdebug(fmt, ...) if (boot_verbose) { printf(fmt, ##__VA_ARGS__); usleep(500000); }
#define verror printf


#pragma pack(4)
typedef struct ventoy_image_desc
{
    uint64_t disk_size;
    uint64_t part1_size;
    uint8_t  disk_uuid[16];
    uint8_t  disk_signature[4];
    uint32_t img_chunk_count;
    /* ventoy_img_chunk list */
}ventoy_image_desc;

typedef struct ventoy_img_chunk
{
    uint32_t img_start_sector; // sector size: 2KB
    uint32_t img_end_sector;   // included

    uint64_t disk_start_sector; // in disk_sector_size
    uint64_t disk_end_sector;   // included
}ventoy_img_chunk;
#pragma pack()


#endif

