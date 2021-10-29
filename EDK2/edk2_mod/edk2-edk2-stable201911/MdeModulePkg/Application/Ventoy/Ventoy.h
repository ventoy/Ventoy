/******************************************************************************
 * Ventoy.h
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
    UINT32   data1;
    UINT16   data2;
    UINT16   data3;
    UINT8    data4[8];
}ventoy_guid;

typedef struct ventoy_image_disk_region
{
    UINT32   image_sector_count; /* image sectors contained in this region */
    UINT32   image_start_sector; /* image sector start */
    UINT64   disk_start_sector;  /* disk sector start */
}ventoy_image_disk_region;

typedef struct ventoy_image_location
{
    ventoy_guid  guid;
    
    /* image sector size, currently this value is always 2048 */
    UINT32   image_sector_size;

    /* disk sector size, normally the value is 512 */
    UINT32   disk_sector_size;

    UINT32   region_count;
    
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
    UINT8   chksum;                // checksum

    UINT8   vtoy_disk_guid[16];
    UINT64  vtoy_disk_size;       // disk size in bytes
    UINT16  vtoy_disk_part_id;    // begin with 1
    UINT16  vtoy_disk_part_type;  // 0:exfat   1:ntfs  other: reserved
    char    vtoy_img_path[384];   // It seems to be enough, utf-8 format
    UINT64  vtoy_img_size;        // image file size in bytes

    /* 
     * Ventoy will write a copy of ventoy_image_location data into runtime memory
     * this is the physically address and length of that memory.
     * Address 0 means no such data exist.
     * Address will be aligned by 4KB.
     *
     */
    UINT64  vtoy_img_location_addr;
    UINT32  vtoy_img_location_len;
    
    UINT64  vtoy_reserved[4];     // Internal use by ventoy

    UINT8   reserved[31];
}ventoy_os_param;

#pragma pack()

// compile assert to check that size of ventoy_os_param must be 512
COMPILE_ASSERT(sizeof(ventoy_os_param) == 512);



#pragma pack(4)

typedef struct ventoy_chain_head
{
    ventoy_os_param os_param;

    UINT32 disk_drive;
    UINT32 drive_map;
    UINT32 disk_sector_size;

    UINT64 real_img_size_in_bytes;
    UINT64 virt_img_size_in_bytes;
    UINT32 boot_catalog;
    UINT8  boot_catalog_sector[2048];
    
    UINT32 img_chunk_offset;
    UINT32 img_chunk_num;

    UINT32 override_chunk_offset;
    UINT32 override_chunk_num;

    UINT32 virt_chunk_offset;
    UINT32 virt_chunk_num;
}ventoy_chain_head;


typedef struct ventoy_img_chunk
{
    UINT32 img_start_sector; //2KB
    UINT32 img_end_sector;

    UINT64 disk_start_sector; // in disk_sector_size
    UINT64 disk_end_sector;
}ventoy_img_chunk;


typedef struct ventoy_override_chunk
{
    UINT64 img_offset;
    UINT32 override_size;
    UINT8  override_data[512];
}ventoy_override_chunk;

typedef struct ventoy_virt_chunk
{
    UINT32 mem_sector_start;
    UINT32 mem_sector_end;
    UINT32 mem_sector_offset;
    UINT32 remap_sector_start;
    UINT32 remap_sector_end;
    UINT32 org_sector_start;
}ventoy_virt_chunk;


#pragma pack()


#define VTOY_BLOCK_DEVICE_PATH_GUID					\
	{ 0x37b87ac6, 0xc180, 0x4583, { 0xa7, 0x05, 0x41, 0x4d, 0xa8, 0xf7, 0x7e, 0xd2 }}



#if   defined (MDE_CPU_IA32)
  #define VENTOY_UEFI_DESC   L"IA32 UEFI"
  #define ISO9660_EFI_DRIVER_PATH  L"\\ventoy\\iso9660_ia32.efi"
  #define UDF_EFI_DRIVER_PATH  L"\\ventoy\\udf_ia32.efi"
#elif defined (MDE_CPU_X64)
  #define VENTOY_UEFI_DESC   L"X64 UEFI"
  #define ISO9660_EFI_DRIVER_PATH  L"\\ventoy\\iso9660_x64.efi"
  #define UDF_EFI_DRIVER_PATH  L"\\ventoy\\udf_x64.efi"
#elif defined (MDE_CPU_EBC)
#elif defined (MDE_CPU_ARM)
  #define VENTOY_UEFI_DESC   L"ARM UEFI"
  #define ISO9660_EFI_DRIVER_PATH  L"\\ventoy\\iso9660_arm.efi"
  #define UDF_EFI_DRIVER_PATH  L"\\ventoy\\udf_arm.efi"
#elif defined (MDE_CPU_AARCH64)
  #define VENTOY_UEFI_DESC   L"ARM64 UEFI"
  #define ISO9660_EFI_DRIVER_PATH  L"\\ventoy\\iso9660_aa64.efi"
  #define UDF_EFI_DRIVER_PATH  L"\\ventoy\\udf_aa64.efi"
#else
  #error Unknown Processor Type
#endif

#define VENTOY_DEVICE_WARN 1
#define VTOY_WARNING  L"!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!"

typedef struct ventoy_sector_flag
{
    UINT8 flag; // 0:init   1:mem  2:remap
    UINT64 remap_lba;    
}ventoy_sector_flag;


typedef struct vtoy_block_data 
{
	EFI_HANDLE Handle;
	EFI_BLOCK_IO_MEDIA Media;       /* Media descriptor */
	EFI_BLOCK_IO_PROTOCOL BlockIo;	/* Block I/O protocol */

    UINTN DevicePathCompareLen;
	EFI_DEVICE_PATH_PROTOCOL *Path;	/* Device path protocol */

    EFI_HANDLE RawBlockIoHandle;
    EFI_BLOCK_IO_PROTOCOL *pRawBlockIo;
    EFI_DEVICE_PATH_PROTOCOL *pDiskDevPath;

    /* ventoy disk part2 ESP */
    EFI_HANDLE DiskFsHandle;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *pDiskFs;
    EFI_DEVICE_PATH_PROTOCOL *pDiskFsDevPath;

    EFI_HANDLE IsoDriverImage;
}vtoy_block_data;


#define debug(expr, ...) if (gDebugPrint) VtoyDebug("[VTOY] "expr"\r\n", ##__VA_ARGS__)
#define trace(expr, ...) VtoyDebug("[VTOY] "expr"\r\n", ##__VA_ARGS__)
#define sleep(sec) gBS->Stall(1000000 * (sec))

#define ventoy_debug_pause() \
if (gDebugPrint) \
{ \
    UINTN __Index = 0; \
    gST->ConOut->OutputString(gST->ConOut, L"[VTOY] ###### Press Enter to continue... ######\r\n");\
    gST->ConIn->Reset(gST->ConIn, FALSE); \
    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &__Index);\
}

typedef int (*grub_env_set_pf)(const char *name, const char *val);
typedef const char * (*grub_env_get_pf)(const char *name);
typedef int (*grub_env_printf_pf)(const char *fmt, ...);

#pragma pack(1)

#define GRUB_FILE_REPLACE_MAGIC  0x1258BEEF
#define GRUB_IMG_REPLACE_MAGIC   0x1259BEEF

typedef struct ventoy_efi_file_replace
{
    UINT64 BlockIoSectorStart;

    UINT64 CurPos;
    UINT64 FileSizeBytes;

    EFI_FILE_PROTOCOL  WrapperHandle;
}ventoy_efi_file_replace;

typedef struct ventoy_grub_param_file_replace
{
    UINT32 magic;
    char   old_file_name[4][256];
    UINT32 old_file_cnt;
    UINT32 new_file_virtual_id;
}ventoy_grub_param_file_replace;

typedef struct ventoy_grub_param
{
    grub_env_get_pf grub_env_get;
    grub_env_set_pf grub_env_set;
    ventoy_grub_param_file_replace file_replace;
    ventoy_grub_param_file_replace img_replace;
    grub_env_printf_pf grub_env_printf;    
}ventoy_grub_param;

typedef struct ventoy_ram_disk
{
    UINT64 PhyAddr;
    UINT64 DiskSize;
}ventoy_ram_disk;

typedef struct ventoy_iso9660_override
{
    UINT32 first_sector;
    UINT32 first_sector_be;
    UINT32 size;
    UINT32 size_be;
}ventoy_iso9660_override;

typedef struct PART_TABLE
{
    UINT8  Active; // 0x00  0x80

    UINT8  StartHead;
    UINT16 StartSector : 6;
    UINT16 StartCylinder : 10;

    UINT8  FsFlag;

    UINT8  EndHead;
    UINT16 EndSector : 6;
    UINT16 EndCylinder : 10;

    UINT32 StartSectorId;
    UINT32 SectorCount;
}PART_TABLE;

typedef struct MBR_HEAD
{
    UINT8 BootCode[446];
    PART_TABLE PartTbl[4];
    UINT8 Byte55;
    UINT8 ByteAA;
}MBR_HEAD;

#pragma pack()


typedef struct well_known_guid 
{
	EFI_GUID *guid;
	const char *name;
}well_known_guid;

typedef struct ventoy_system_wrapper
{
    EFI_LOCATE_PROTOCOL NewLocateProtocol;
    EFI_LOCATE_PROTOCOL OriLocateProtocol;

    EFI_HANDLE_PROTOCOL NewHandleProtocol;
    EFI_HANDLE_PROTOCOL OriHandleProtocol;
    
    EFI_OPEN_PROTOCOL NewOpenProtocol;
    EFI_OPEN_PROTOCOL OriOpenProtocol;

    EFI_LOCATE_HANDLE_BUFFER NewLocateHandleBuffer;
    EFI_LOCATE_HANDLE_BUFFER OriLocateHandleBuffer;

    EFI_PROTOCOLS_PER_HANDLE NewProtocolsPerHandle;
    EFI_PROTOCOLS_PER_HANDLE OriProtocolsPerHandle;

    EFI_LOCATE_HANDLE NewLocateHandle;
    EFI_LOCATE_HANDLE OriLocateHandle;

    EFI_LOCATE_DEVICE_PATH NewLocateDevicePath;
    EFI_LOCATE_DEVICE_PATH OriLocateDevicePath;
} ventoy_system_wrapper;


#define MAX_DRIVER_BIND_WRAPPER  64
typedef struct DriverBindWrapper
{
    EFI_DRIVER_BINDING_PROTOCOL *DriverBinding;
    EFI_DRIVER_BINDING_SUPPORTED pfOldSupport;
}DRIVER_BIND_WRAPPER;

#define ventoy_wrapper(bs, wrapper, func, newfunc) \
{\
    wrapper.Ori##func = bs->func;\
    wrapper.New##func = newfunc;\
    bs->func = wrapper.New##func;\
}


#define VENTOY_GET_COMPONENT_NAME(Protocol, DriverName) \
{\
    DriverName = NULL;\
    Status = Protocol->GetDriverName(Protocol, "en", &DriverName);\
    if (EFI_ERROR(Status) || NULL == DriverName) \
    {\
        DriverName = NULL;\
        Status = Protocol->GetDriverName(Protocol, "eng", &DriverName);\
        if (EFI_ERROR(Status) || NULL == DriverName) \
        {\
            continue;\
        }\
    }\
}

extern BOOLEAN gDebugPrint;
VOID EFIAPI VtoyDebug(IN CONST CHAR8  *Format, ...);
EFI_STATUS EFIAPI ventoy_wrapper_system(VOID);
EFI_STATUS EFIAPI ventoy_block_io_read 
(
    IN EFI_BLOCK_IO_PROTOCOL          *This,
    IN UINT32                          MediaId,
    IN EFI_LBA                         Lba,
    IN UINTN                           BufferSize,
    OUT VOID                          *Buffer
);


extern ventoy_chain_head *g_chain;
extern ventoy_img_chunk *g_chunk;
extern UINT32 g_img_chunk_num;
extern ventoy_override_chunk *g_override_chunk;
extern UINT32 g_override_chunk_num;
extern ventoy_virt_chunk *g_virt_chunk;
extern UINT32 g_virt_chunk_num;
extern vtoy_block_data gBlockData;
extern ventoy_efi_file_replace g_efi_file_replace;
extern ventoy_efi_file_replace g_img_file_replace;
extern ventoy_sector_flag *g_sector_flag;
extern UINT32 g_sector_flag_num;
extern BOOLEAN gMemdiskMode;
extern BOOLEAN gSector512Mode;
extern UINTN g_iso_buf_size;
extern UINT8 *g_iso_data_buf;
extern ventoy_grub_param_file_replace *g_file_replace_list;
extern ventoy_grub_param_file_replace *g_img_replace_list;
extern BOOLEAN g_fixup_iso9660_secover_enable;
extern EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *g_con_simple_input_ex;
extern BOOLEAN g_fix_windows_1st_cdrom_issue;

EFI_STATUS EFIAPI ventoy_wrapper_open_volume
(
    IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL     *This,
    OUT EFI_FILE_PROTOCOL                 **Root
);
EFI_STATUS EFIAPI ventoy_install_blockio(IN EFI_HANDLE ImageHandle, IN UINT64 ImgSize);
EFI_STATUS EFIAPI ventoy_wrapper_push_openvolume(IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME OpenVolume);
EFI_STATUS ventoy_hook_keyboard_start(VOID);
EFI_STATUS ventoy_hook_keyboard_stop(VOID);
BOOLEAN ventoy_is_cdrom_dp_exist(VOID);
EFI_STATUS ventoy_hook_1st_cdrom_start(VOID);
EFI_STATUS ventoy_hook_1st_cdrom_stop(VOID);
EFI_STATUS ventoy_disable_ex_filesystem(VOID);
EFI_STATUS ventoy_enable_ex_filesystem(VOID);

#endif

