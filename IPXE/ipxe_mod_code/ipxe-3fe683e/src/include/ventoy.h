
#ifndef __VENTOY_VDISK_H__
#define __VENTOY_VDISK_H__

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

//#define VTOY_DEBUG 1

#define grub_uint64_t  uint64_t
#define grub_uint32_t  uint32_t
#define grub_uint16_t  uint16_t
#define grub_uint8_t   uint8_t

#define COMPILE_ASSERT(expr)  extern char __compile_assert[(expr) ? 1 : -1]

#define VENTOY_GUID { 0x77772020, 0x2e77, 0x6576, { 0x6e, 0x74, 0x6f, 0x79, 0x2e, 0x6e, 0x65, 0x74 }}

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
    grub_uint32_t   image_sector_count; /* image sectors contained in this region */
    grub_uint32_t   image_start_sector; /* image sector start */
    grub_uint64_t   disk_start_sector;  /* disk sector start */
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
     * disk region data
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

    grub_uint8_t   vtoy_reserved[32];    // Internal use by ventoy

    grub_uint8_t   vtoy_disk_signature[4];
    
    grub_uint8_t   reserved[27];
}ventoy_os_param;

typedef struct ventoy_iso9660_override
{
    uint32_t first_sector;
    uint32_t first_sector_be;
    uint32_t size;
    uint32_t size_be;
}ventoy_iso9660_override;

#pragma pack()

// compile assert to check that size of ventoy_os_param must be 512
COMPILE_ASSERT(sizeof(ventoy_os_param) == 512);







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


typedef struct ventoy_img_chunk
{
    grub_uint32_t img_start_sector; //2KB
    grub_uint32_t img_end_sector;

    grub_uint64_t disk_start_sector; // in disk_sector_size
    grub_uint64_t disk_end_sector;
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


#pragma pack()


#define ventoy_debug_pause()    \
{\
    printf("\nPress Ctrl+C to continue......");\
    sleep(3600);\
    printf("\n");\
}

typedef struct ventoy_sector_flag
{
    uint8_t flag; // 0:init   1:mem  2:remap
    uint64_t remap_lba;    
}ventoy_sector_flag;

#define VENTOY_BIOS_FAKE_DRIVE  0xFE
#define VENTOY_BOOT_FIXBIN_DRIVE  0xFD

extern int g_debug;
extern int g_hddmode;
extern int g_bios_disk80;
extern char *g_cmdline_copy;
extern void *g_initrd_addr;
extern size_t g_initrd_len;
extern uint32_t g_disk_sector_size;
unsigned int ventoy_int13_hook (ventoy_chain_head *chain);
int ventoy_int13_boot ( unsigned int drive, void *imginfo, const char *cmdline);
void * ventoy_get_runtime_addr(void);
int ventoy_boot_vdisk(void *data);


uint32_t CalculateCrc32
(
    const void       *Buffer,
    uint32_t          Length,
    uint32_t          InitValue
);

struct smbios3_entry {
	
	uint8_t signature[5];

    /** Checksum */
	uint8_t checksum;

    /** Length */
	uint8_t len;

    /** Major version */
	uint8_t major;

    /** Minor version */
	uint8_t minor;
    
	uint8_t docrev;
    
	uint8_t revision;
    
	uint8_t reserved;

    uint32_t maxsize;

    uint64_t address;
} __attribute__ (( packed ));


typedef struct isolinux_boot_info
{
    uint32_t isolinux0;
    uint32_t isolinux1;
    uint32_t PvdLocation;
    uint32_t BootFileLocation;
    uint32_t BootFileLen;
    uint32_t BootFileChecksum;
    uint8_t  Reserved[40];
}isolinux_boot_info;

//#undef DBGLVL
//#define DBGLVL 7

#endif /* __VENTOY_VDISK_H__ */

