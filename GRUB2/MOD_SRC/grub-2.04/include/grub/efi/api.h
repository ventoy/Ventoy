/* efi.h - declare EFI types and functions */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007,2008,2009  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GRUB_EFI_API_HEADER
#define GRUB_EFI_API_HEADER	1

#include <grub/types.h>
#include <grub/symbol.h>

/* For consistency and safety, we name the EFI-defined types differently.
   All names are transformed into lower case, _t appended, and
   grub_efi_ prepended.  */

/* Constants.  */
#define GRUB_EFI_EVT_TIMER				0x80000000
#define GRUB_EFI_EVT_RUNTIME				0x40000000
#define GRUB_EFI_EVT_RUNTIME_CONTEXT			0x20000000
#define GRUB_EFI_EVT_NOTIFY_WAIT			0x00000100
#define GRUB_EFI_EVT_NOTIFY_SIGNAL			0x00000200
#define GRUB_EFI_EVT_SIGNAL_EXIT_BOOT_SERVICES		0x00000201
#define GRUB_EFI_EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE	0x60000202

#define GRUB_EFI_TPL_APPLICATION	4
#define GRUB_EFI_TPL_CALLBACK		8
#define GRUB_EFI_TPL_NOTIFY		16
#define GRUB_EFI_TPL_HIGH_LEVEL		31

#define GRUB_EFI_MEMORY_UC	0x0000000000000001LL
#define GRUB_EFI_MEMORY_WC	0x0000000000000002LL
#define GRUB_EFI_MEMORY_WT	0x0000000000000004LL
#define GRUB_EFI_MEMORY_WB	0x0000000000000008LL
#define GRUB_EFI_MEMORY_UCE	0x0000000000000010LL
#define GRUB_EFI_MEMORY_WP	0x0000000000001000LL
#define GRUB_EFI_MEMORY_RP	0x0000000000002000LL
#define GRUB_EFI_MEMORY_XP	0x0000000000004000LL
#define GRUB_EFI_MEMORY_NV	0x0000000000008000LL
#define GRUB_EFI_MEMORY_MORE_RELIABLE	0x0000000000010000LL
#define GRUB_EFI_MEMORY_RO	0x0000000000020000LL
#define GRUB_EFI_MEMORY_RUNTIME	0x8000000000000000LL

#define GRUB_EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL	0x00000001
#define GRUB_EFI_OPEN_PROTOCOL_GET_PROTOCOL		0x00000002
#define GRUB_EFI_OPEN_PROTOCOL_TEST_PROTOCOL		0x00000004
#define GRUB_EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER	0x00000008
#define GRUB_EFI_OPEN_PROTOCOL_BY_DRIVER		0x00000010
#define GRUB_EFI_OPEN_PROTOCOL_BY_EXCLUSIVE		0x00000020

#define GRUB_EFI_OS_INDICATIONS_BOOT_TO_FW_UI	0x0000000000000001ULL

#define GRUB_EFI_VARIABLE_NON_VOLATILE		0x0000000000000001
#define GRUB_EFI_VARIABLE_BOOTSERVICE_ACCESS	0x0000000000000002
#define GRUB_EFI_VARIABLE_RUNTIME_ACCESS	0x0000000000000004

#define GRUB_EFI_TIME_ADJUST_DAYLIGHT	0x01
#define GRUB_EFI_TIME_IN_DAYLIGHT	0x02

#define GRUB_EFI_UNSPECIFIED_TIMEZONE	0x07FF

#define GRUB_EFI_OPTIONAL_PTR	0x00000001

#define GRUB_EFI_LOADED_IMAGE_GUID	\
  { 0x5b1b31a1, 0x9562, 0x11d2, \
    { 0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

#define GRUB_EFI_DISK_IO_GUID	\
  { 0xce345171, 0xba0b, 0x11d2, \
    { 0x8e, 0x4f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

#define GRUB_EFI_BLOCK_IO_GUID	\
  { 0x964e5b21, 0x6459, 0x11d2, \
    { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

#define GRUB_EFI_SERIAL_IO_GUID \
  { 0xbb25cf6f, 0xf1d4, 0x11d2, \
    { 0x9a, 0x0c, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0xfd } \
  }

#define GRUB_EFI_SIMPLE_NETWORK_GUID	\
  { 0xa19832b9, 0xac25, 0x11d3, \
    { 0x9a, 0x2d, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

#define GRUB_EFI_PXE_GUID	\
  { 0x03c4e603, 0xac28, 0x11d3, \
    { 0x9a, 0x2d, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

#define GRUB_EFI_DEVICE_PATH_GUID	\
  { 0x09576e91, 0x6d3f, 0x11d2, \
    { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

#define GRUB_EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID \
  { 0x387477c1, 0x69c7, 0x11d2, \
    { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

#define GRUB_EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID \
  { 0xdd9e7534, 0x7762, 0x4698, \
    { 0x8c, 0x14, 0xf5, 0x85, 0x17, 0xa6, 0x25, 0xaa } \
  }

#define GRUB_EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID \
  { 0x387477c2, 0x69c7, 0x11d2, \
    { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

#define GRUB_EFI_SIMPLE_POINTER_PROTOCOL_GUID \
  { 0x31878c87, 0xb75, 0x11d5, \
    { 0x9a, 0x4f, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

#define GRUB_EFI_ABSOLUTE_POINTER_PROTOCOL_GUID \
  { 0x8D59D32B, 0xC655, 0x4AE9, \
    { 0x9B, 0x15, 0xF2, 0x59, 0x04, 0x99, 0x2A, 0x43 } \
  }

#define GRUB_EFI_DRIVER_BINDING_PROTOCOL_GUID \
  { 0x18A031AB, 0xB443, 0x4D1A, \
    { 0xA5, 0xC0, 0x0C, 0x09, 0x26, 0x1E, 0x9F, 0x71 } \
  }

#define GRUB_EFI_LOADED_IMAGE_PROTOCOL_GUID \
  { 0x5B1B31A1, 0x9562, 0x11d2, \
    { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } \
  }

#define GRUB_EFI_LOAD_FILE_PROTOCOL_GUID \
  { 0x56EC3091, 0x954C, 0x11d2, \
    { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } \
  }

#define GRUB_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
  { 0x0964e5b22, 0x6459, 0x11d2, \
    { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

#define GRUB_EFI_TAPE_IO_PROTOCOL_GUID \
  { 0x1e93e633, 0xd65a, 0x459e, \
    { 0xab, 0x84, 0x93, 0xd9, 0xec, 0x26, 0x6d, 0x18 } \
  }

#define GRUB_EFI_UNICODE_COLLATION_PROTOCOL_GUID \
  { 0x1d85cd7f, 0xf43d, 0x11d2, \
    { 0x9a, 0x0c, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

#define GRUB_EFI_SCSI_IO_PROTOCOL_GUID \
  { 0x932f47e6, 0x2362, 0x4002, \
    { 0x80, 0x3e, 0x3c, 0xd5, 0x4b, 0x13, 0x8f, 0x85 } \
  }

#define GRUB_EFI_USB2_HC_PROTOCOL_GUID \
  { 0x3e745226, 0x9818, 0x45b6, \
    { 0xa2, 0xac, 0xd7, 0xcd, 0x0e, 0x8b, 0xa2, 0xbc } \
  }

#define GRUB_EFI_DEBUG_SUPPORT_PROTOCOL_GUID \
  { 0x2755590C, 0x6F3C, 0x42FA, \
    { 0x9E, 0xA4, 0xA3, 0xBA, 0x54, 0x3C, 0xDA, 0x25 } \
  }

#define GRUB_EFI_DEBUGPORT_PROTOCOL_GUID \
  { 0xEBA4E8D2, 0x3858, 0x41EC, \
    { 0xA2, 0x81, 0x26, 0x47, 0xBA, 0x96, 0x60, 0xD0 } \
  }

#define GRUB_EFI_DECOMPRESS_PROTOCOL_GUID \
  { 0xd8117cfe, 0x94a6, 0x11d4, \
    { 0x9a, 0x3a, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

#define GRUB_EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID \
  { 0x8b843e20, 0x8132, 0x4852, \
    { 0x90, 0xcc, 0x55, 0x1a, 0x4e, 0x4a, 0x7f, 0x1c } \
  }

#define GRUB_EFI_DEVICE_PATH_UTILITIES_PROTOCOL_GUID \
  { 0x379be4e, 0xd706, 0x437d, \
    { 0xb0, 0x37, 0xed, 0xb8, 0x2f, 0xb7, 0x72, 0xa4 } \
  }

#define GRUB_EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL_GUID \
  { 0x5c99a21, 0xc70f, 0x4ad2, \
    { 0x8a, 0x5f, 0x35, 0xdf, 0x33, 0x43, 0xf5, 0x1e } \
  }

#define GRUB_EFI_ACPI_TABLE_PROTOCOL_GUID \
  { 0xffe06bdd, 0x6107, 0x46a6, \
    { 0x7b, 0xb2, 0x5a, 0x9c, 0x7e, 0xc5, 0x27, 0x5c} \
  }

#define GRUB_EFI_HII_CONFIG_ROUTING_PROTOCOL_GUID \
  { 0x587e72d7, 0xcc50, 0x4f79, \
    { 0x82, 0x09, 0xca, 0x29, 0x1f, 0xc1, 0xa1, 0x0f } \
  }

#define GRUB_EFI_HII_DATABASE_PROTOCOL_GUID \
  { 0xef9fc172, 0xa1b2, 0x4693, \
    { 0xb3, 0x27, 0x6d, 0x32, 0xfc, 0x41, 0x60, 0x42 } \
  }

#define GRUB_EFI_HII_STRING_PROTOCOL_GUID \
  { 0xfd96974, 0x23aa, 0x4cdc, \
    { 0xb9, 0xcb, 0x98, 0xd1, 0x77, 0x50, 0x32, 0x2a } \
  }

#define GRUB_EFI_HII_IMAGE_PROTOCOL_GUID \
  { 0x31a6406a, 0x6bdf, 0x4e46, \
    { 0xb2, 0xa2, 0xeb, 0xaa, 0x89, 0xc4, 0x9, 0x20 } \
  }

#define GRUB_EFI_HII_FONT_PROTOCOL_GUID \
  { 0xe9ca4775, 0x8657, 0x47fc, \
    { 0x97, 0xe7, 0x7e, 0xd6, 0x5a, 0x8, 0x43, 0x24 } \
  }

#define GRUB_EFI_HII_CONFIGURATION_ACCESS_PROTOCOL_GUID \
  { 0x330d4706, 0xf2a0, 0x4e4f, \
    { 0xa3, 0x69, 0xb6, 0x6f, 0xa8, 0xd5, 0x43, 0x85 } \
  }

#define GRUB_EFI_COMPONENT_NAME2_PROTOCOL_GUID \
  { 0x6a7a5cff, 0xe8d9, 0x4f70, \
    { 0xba, 0xda, 0x75, 0xab, 0x30, 0x25, 0xce, 0x14} \
  }

#define GRUB_EFI_USB_IO_PROTOCOL_GUID \
  { 0x2B2F68D6, 0x0CD2, 0x44cf, \
    { 0x8E, 0x8B, 0xBB, 0xA2, 0x0B, 0x1B, 0x5B, 0x75 } \
  }

#define GRUB_EFI_TIANO_CUSTOM_DECOMPRESS_GUID \
  { 0xa31280ad, 0x481e, 0x41b6, \
    { 0x95, 0xe8, 0x12, 0x7f, 0x4c, 0x98, 0x47, 0x79 } \
  }

#define GRUB_EFI_CRC32_GUIDED_SECTION_EXTRACTION_GUID \
  { 0xfc1bcdb0, 0x7d31, 0x49aa, \
    { 0x93, 0x6a, 0xa4, 0x60, 0x0d, 0x9d, 0xd0, 0x83 } \
  }

#define GRUB_EFI_LZMA_CUSTOM_DECOMPRESS_GUID \
  { 0xee4e5898, 0x3914, 0x4259, \
    { 0x9d, 0x6e, 0xdc, 0x7b, 0xd7, 0x94, 0x03, 0xcf } \
  }

#define GRUB_EFI_TSC_FREQUENCY_GUID \
  { 0xdba6a7e3, 0xbb57, 0x4be7, \
    { 0x8a, 0xf8, 0xd5, 0x78, 0xdb, 0x7e, 0x56, 0x87 } \
  }

#define GRUB_EFI_SYSTEM_RESOURCE_TABLE_GUID \
  { 0xb122a263, 0x3661, 0x4f68, \
    { 0x99, 0x29, 0x78, 0xf8, 0xb0, 0xd6, 0x21, 0x80 } \
  }

#define GRUB_EFI_DXE_SERVICES_TABLE_GUID \
  { 0x05ad34ba, 0x6f02, 0x4214, \
    { 0x95, 0x2e, 0x4d, 0xa0, 0x39, 0x8e, 0x2b, 0xb9 } \
  }

#define GRUB_EFI_HOB_LIST_GUID \
  { 0x7739f24c, 0x93d7, 0x11d4, \
    { 0x9a, 0x3a, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

#define GRUB_EFI_MEMORY_TYPE_INFORMATION_GUID \
  { 0x4c19049f, 0x4137, 0x4dd3, \
    { 0x9c, 0x10, 0x8b, 0x97, 0xa8, 0x3f, 0xfd, 0xfa } \
  }

#define GRUB_EFI_DEBUG_IMAGE_INFO_TABLE_GUID \
  { 0x49152e77, 0x1ada, 0x4764, \
    { 0xb7, 0xa2, 0x7a, 0xfe, 0xfe, 0xd9, 0x5e, 0x8b } \
  }

#define GRUB_EFI_MPS_TABLE_GUID	\
  { 0xeb9d2d2f, 0x2d88, 0x11d3, \
    { 0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

#define GRUB_EFI_ACPI_TABLE_GUID	\
  { 0xeb9d2d30, 0x2d88, 0x11d3, \
    { 0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

#define GRUB_EFI_ACPI_20_TABLE_GUID	\
  { 0x8868e871, 0xe4f1, 0x11d3, \
    { 0xbc, 0x22, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 } \
  }

#define GRUB_EFI_SMBIOS_TABLE_GUID	\
  { 0xeb9d2d31, 0x2d88, 0x11d3, \
    { 0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

#define GRUB_EFI_SMBIOS3_TABLE_GUID	\
  { 0xf2fd1544, 0x9794, 0x4a2c, \
    { 0x99, 0x2e, 0xe5, 0xbb, 0xcf, 0x20, 0xe3, 0x94 } \
  }

#define GRUB_EFI_SAL_TABLE_GUID \
  { 0xeb9d2d32, 0x2d88, 0x11d3, \
      { 0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

#define GRUB_EFI_HCDP_TABLE_GUID \
  { 0xf951938d, 0x620b, 0x42ef, \
      { 0x82, 0x79, 0xa8, 0x4b, 0x79, 0x61, 0x78, 0x98 } \
  }

#define GRUB_EFI_DEVICE_TREE_GUID \
  { 0xb1b621d5, 0xf19c, 0x41a5, \
      { 0x83, 0x0b, 0xd9, 0x15, 0x2c, 0x69, 0xaa, 0xe0 } \
  }

#define GRUB_EFI_VENDOR_APPLE_GUID \
  { 0x2B0585EB, 0xD8B8, 0x49A9,	\
      { 0x8B, 0x8C, 0xE2, 0x1B, 0x01, 0xAE, 0xF2, 0xB7 } \
  }

struct grub_efi_sal_system_table
{
  grub_uint32_t signature;
  grub_uint32_t total_table_len;
  grub_uint16_t sal_rev;
  grub_uint16_t entry_count;
  grub_uint8_t checksum;
  grub_uint8_t reserved1[7];
  grub_uint16_t sal_a_version;
  grub_uint16_t sal_b_version;
  grub_uint8_t oem_id[32];
  grub_uint8_t product_id[32];
  grub_uint8_t reserved2[8];
  grub_uint8_t entries[0];
};

enum
  {
    GRUB_EFI_SAL_SYSTEM_TABLE_TYPE_ENTRYPOINT_DESCRIPTOR = 0,
    GRUB_EFI_SAL_SYSTEM_TABLE_TYPE_MEMORY_DESCRIPTOR = 1,
    GRUB_EFI_SAL_SYSTEM_TABLE_TYPE_PLATFORM_FEATURES = 2,
    GRUB_EFI_SAL_SYSTEM_TABLE_TYPE_TRANSLATION_REGISTER_DESCRIPTOR = 3,
    GRUB_EFI_SAL_SYSTEM_TABLE_TYPE_PURGE_TRANSLATION_COHERENCE = 4,
    GRUB_EFI_SAL_SYSTEM_TABLE_TYPE_AP_WAKEUP = 5
  };

struct grub_efi_sal_system_table_entrypoint_descriptor
{
  grub_uint8_t type;
  grub_uint8_t pad[7];
  grub_uint64_t pal_proc_addr;
  grub_uint64_t sal_proc_addr;
  grub_uint64_t global_data_ptr;
  grub_uint64_t reserved[2];
};

struct grub_efi_sal_system_table_memory_descriptor
{
  grub_uint8_t type;
  grub_uint8_t sal_used;
  grub_uint8_t attr;
  grub_uint8_t ar;
  grub_uint8_t attr_mask;
  grub_uint8_t mem_type;
  grub_uint8_t usage;
  grub_uint8_t unknown;
  grub_uint64_t addr;
  grub_uint64_t len;
  grub_uint64_t unknown2;
};

struct grub_efi_sal_system_table_platform_features
{
  grub_uint8_t type;
  grub_uint8_t flags;
  grub_uint8_t reserved[14];
};

struct grub_efi_sal_system_table_translation_register_descriptor
{
  grub_uint8_t type;
  grub_uint8_t register_type;
  grub_uint8_t register_number;
  grub_uint8_t reserved[5];
  grub_uint64_t addr;
  grub_uint64_t page_size;
  grub_uint64_t reserver;
};

struct grub_efi_sal_system_table_purge_translation_coherence
{
  grub_uint8_t type;
  grub_uint8_t reserved[3];  
  grub_uint32_t ndomains;
  grub_uint64_t coherence;
};

struct grub_efi_sal_system_table_ap_wakeup
{
  grub_uint8_t type;
  grub_uint8_t mechanism;
  grub_uint8_t reserved[6];
  grub_uint64_t vector;
};

enum
  {
    GRUB_EFI_SAL_SYSTEM_TABLE_PLATFORM_FEATURE_BUSLOCK = 1,
    GRUB_EFI_SAL_SYSTEM_TABLE_PLATFORM_FEATURE_IRQREDIRECT = 2,
    GRUB_EFI_SAL_SYSTEM_TABLE_PLATFORM_FEATURE_IPIREDIRECT = 4,
    GRUB_EFI_SAL_SYSTEM_TABLE_PLATFORM_FEATURE_ITCDRIFT = 8,
  };

typedef enum grub_efi_parity_type
  {
    GRUB_EFI_SERIAL_DEFAULT_PARITY,
    GRUB_EFI_SERIAL_NO_PARITY,
    GRUB_EFI_SERIAL_EVEN_PARITY,
    GRUB_EFI_SERIAL_ODD_PARITY
  }
grub_efi_parity_type_t;

typedef enum grub_efi_stop_bits
  {
    GRUB_EFI_SERIAL_DEFAULT_STOP_BITS,
    GRUB_EFI_SERIAL_1_STOP_BIT,
    GRUB_EFI_SERIAL_1_5_STOP_BITS,
    GRUB_EFI_SERIAL_2_STOP_BITS
  }
  grub_efi_stop_bits_t;

/* Enumerations.  */
enum grub_efi_timer_delay
  {
    GRUB_EFI_TIMER_CANCEL,
    GRUB_EFI_TIMER_PERIODIC,
    GRUB_EFI_TIMER_RELATIVE
  };
typedef enum grub_efi_timer_delay grub_efi_timer_delay_t;

enum grub_efi_allocate_type
  {
    GRUB_EFI_ALLOCATE_ANY_PAGES,
    GRUB_EFI_ALLOCATE_MAX_ADDRESS,
    GRUB_EFI_ALLOCATE_ADDRESS,
    GRUB_EFI_MAX_ALLOCATION_TYPE
  };
typedef enum grub_efi_allocate_type grub_efi_allocate_type_t;

enum grub_efi_memory_type
  {
    GRUB_EFI_RESERVED_MEMORY_TYPE,
    GRUB_EFI_LOADER_CODE,
    GRUB_EFI_LOADER_DATA,
    GRUB_EFI_BOOT_SERVICES_CODE,
    GRUB_EFI_BOOT_SERVICES_DATA,
    GRUB_EFI_RUNTIME_SERVICES_CODE,
    GRUB_EFI_RUNTIME_SERVICES_DATA,
    GRUB_EFI_CONVENTIONAL_MEMORY,
    GRUB_EFI_UNUSABLE_MEMORY,
    GRUB_EFI_ACPI_RECLAIM_MEMORY,
    GRUB_EFI_ACPI_MEMORY_NVS,
    GRUB_EFI_MEMORY_MAPPED_IO,
    GRUB_EFI_MEMORY_MAPPED_IO_PORT_SPACE,
    GRUB_EFI_PAL_CODE,
    GRUB_EFI_PERSISTENT_MEMORY,
    GRUB_EFI_MAX_MEMORY_TYPE
  };
typedef enum grub_efi_memory_type grub_efi_memory_type_t;

enum grub_efi_interface_type
  {
    GRUB_EFI_NATIVE_INTERFACE
  };
typedef enum grub_efi_interface_type grub_efi_interface_type_t;

enum grub_efi_locate_search_type
  {
    GRUB_EFI_ALL_HANDLES,
    GRUB_EFI_BY_REGISTER_NOTIFY,
    GRUB_EFI_BY_PROTOCOL
  };
typedef enum grub_efi_locate_search_type grub_efi_locate_search_type_t;

enum grub_efi_reset_type
  {
    GRUB_EFI_RESET_COLD,
    GRUB_EFI_RESET_WARM,
    GRUB_EFI_RESET_SHUTDOWN
  };
typedef enum grub_efi_reset_type grub_efi_reset_type_t;

/* Types.  */
typedef char grub_efi_boolean_t;
#if GRUB_CPU_SIZEOF_VOID_P == 8
typedef grub_int64_t grub_efi_intn_t;
typedef grub_uint64_t grub_efi_uintn_t;
#else
typedef grub_int32_t grub_efi_intn_t;
typedef grub_uint32_t grub_efi_uintn_t;
#endif
typedef grub_int8_t grub_efi_int8_t;
typedef grub_uint8_t grub_efi_uint8_t;
typedef grub_int16_t grub_efi_int16_t;
typedef grub_uint16_t grub_efi_uint16_t;
typedef grub_int32_t grub_efi_int32_t;
typedef grub_uint32_t grub_efi_uint32_t;
typedef grub_int64_t grub_efi_int64_t;
typedef grub_uint64_t grub_efi_uint64_t;
typedef grub_uint8_t grub_efi_char8_t;
typedef grub_uint16_t grub_efi_char16_t;

typedef grub_efi_intn_t grub_efi_status_t;

#define GRUB_EFI_ERROR_CODE(value)	\
  ((((grub_efi_status_t) 1) << (sizeof (grub_efi_status_t) * 8 - 1)) | (value))

#define GRUB_EFI_WARNING_CODE(value)	(value)

#define GRUB_EFI_SUCCESS		0

#define GRUB_EFI_LOAD_ERROR		GRUB_EFI_ERROR_CODE (1)
#define GRUB_EFI_INVALID_PARAMETER	GRUB_EFI_ERROR_CODE (2)
#define GRUB_EFI_UNSUPPORTED		GRUB_EFI_ERROR_CODE (3)
#define GRUB_EFI_BAD_BUFFER_SIZE	GRUB_EFI_ERROR_CODE (4)
#define GRUB_EFI_BUFFER_TOO_SMALL	GRUB_EFI_ERROR_CODE (5)
#define GRUB_EFI_NOT_READY		GRUB_EFI_ERROR_CODE (6)
#define GRUB_EFI_DEVICE_ERROR		GRUB_EFI_ERROR_CODE (7)
#define GRUB_EFI_WRITE_PROTECTED	GRUB_EFI_ERROR_CODE (8)
#define GRUB_EFI_OUT_OF_RESOURCES	GRUB_EFI_ERROR_CODE (9)
#define GRUB_EFI_VOLUME_CORRUPTED	GRUB_EFI_ERROR_CODE (10)
#define GRUB_EFI_VOLUME_FULL		GRUB_EFI_ERROR_CODE (11)
#define GRUB_EFI_NO_MEDIA		GRUB_EFI_ERROR_CODE (12)
#define GRUB_EFI_MEDIA_CHANGED		GRUB_EFI_ERROR_CODE (13)
#define GRUB_EFI_NOT_FOUND		GRUB_EFI_ERROR_CODE (14)
#define GRUB_EFI_ACCESS_DENIED		GRUB_EFI_ERROR_CODE (15)
#define GRUB_EFI_NO_RESPONSE		GRUB_EFI_ERROR_CODE (16)
#define GRUB_EFI_NO_MAPPING		GRUB_EFI_ERROR_CODE (17)
#define GRUB_EFI_TIMEOUT		GRUB_EFI_ERROR_CODE (18)
#define GRUB_EFI_NOT_STARTED		GRUB_EFI_ERROR_CODE (19)
#define GRUB_EFI_ALREADY_STARTED	GRUB_EFI_ERROR_CODE (20)
#define GRUB_EFI_ABORTED		GRUB_EFI_ERROR_CODE (21)
#define GRUB_EFI_ICMP_ERROR		GRUB_EFI_ERROR_CODE (22)
#define GRUB_EFI_TFTP_ERROR		GRUB_EFI_ERROR_CODE (23)
#define GRUB_EFI_PROTOCOL_ERROR		GRUB_EFI_ERROR_CODE (24)
#define GRUB_EFI_INCOMPATIBLE_VERSION	GRUB_EFI_ERROR_CODE (25)
#define GRUB_EFI_SECURITY_VIOLATION	GRUB_EFI_ERROR_CODE (26)
#define GRUB_EFI_CRC_ERROR		GRUB_EFI_ERROR_CODE (27)

#define GRUB_EFI_WARN_UNKNOWN_GLYPH	GRUB_EFI_WARNING_CODE (1)
#define GRUB_EFI_WARN_DELETE_FAILURE	GRUB_EFI_WARNING_CODE (2)
#define GRUB_EFI_WARN_WRITE_FAILURE	GRUB_EFI_WARNING_CODE (3)
#define GRUB_EFI_WARN_BUFFER_TOO_SMALL	GRUB_EFI_WARNING_CODE (4)

typedef void *grub_efi_handle_t;
typedef void *grub_efi_event_t;
typedef grub_efi_uint64_t grub_efi_lba_t;
typedef grub_efi_uintn_t grub_efi_tpl_t;
typedef grub_uint8_t grub_efi_mac_address_t[32];
typedef grub_uint8_t grub_efi_ipv4_address_t[4];
typedef grub_uint16_t grub_efi_ipv6_address_t[8];
typedef grub_uint8_t grub_efi_ip_address_t[8] __attribute__ ((aligned(4)));
typedef grub_efi_uint64_t grub_efi_physical_address_t;
typedef grub_efi_uint64_t grub_efi_virtual_address_t;

struct grub_efi_guid
{
  grub_uint32_t data1;
  grub_uint16_t data2;
  grub_uint16_t data3;
  grub_uint8_t data4[8];
} __attribute__ ((aligned(8)));
typedef struct grub_efi_guid grub_efi_guid_t;

struct grub_efi_packed_guid
{
  grub_uint32_t data1;
  grub_uint16_t data2;
  grub_uint16_t data3;
  grub_uint8_t data4[8];
} GRUB_PACKED;
typedef struct grub_efi_packed_guid grub_efi_packed_guid_t;

/* XXX although the spec does not specify the padding, this actually
   must have the padding!  */
struct grub_efi_memory_descriptor
{
  grub_efi_uint32_t type;
  grub_efi_uint32_t padding;
  grub_efi_physical_address_t physical_start;
  grub_efi_virtual_address_t virtual_start;
  grub_efi_uint64_t num_pages;
  grub_efi_uint64_t attribute;
} GRUB_PACKED;
typedef struct grub_efi_memory_descriptor grub_efi_memory_descriptor_t;

/* Device Path definitions.  */
struct grub_efi_device_path
{
  grub_efi_uint8_t type;
  grub_efi_uint8_t subtype;
  grub_efi_uint16_t length;
} GRUB_PACKED;
typedef struct grub_efi_device_path grub_efi_device_path_t;
/* XXX EFI does not define EFI_DEVICE_PATH_PROTOCOL but uses it.
   It seems to be identical to EFI_DEVICE_PATH.  */
typedef struct grub_efi_device_path grub_efi_device_path_protocol_t;

#define GRUB_EFI_DEVICE_PATH_TYPE(dp)		((dp)->type & 0x7f)
#define GRUB_EFI_DEVICE_PATH_SUBTYPE(dp)	((dp)->subtype)
#define GRUB_EFI_DEVICE_PATH_LENGTH(dp)		((dp)->length)

/* The End of Device Path nodes.  */
#define GRUB_EFI_END_DEVICE_PATH_TYPE			(0xff & 0x7f)

#define GRUB_EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE		0xff
#define GRUB_EFI_END_THIS_DEVICE_PATH_SUBTYPE		0x01

#define GRUB_EFI_END_ENTIRE_DEVICE_PATH(dp)	\
  (GRUB_EFI_DEVICE_PATH_TYPE (dp) == GRUB_EFI_END_DEVICE_PATH_TYPE \
   && (GRUB_EFI_DEVICE_PATH_SUBTYPE (dp) \
       == GRUB_EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE))

#define GRUB_EFI_NEXT_DEVICE_PATH(dp)	\
  ((grub_efi_device_path_t *) ((char *) (dp) \
                               + GRUB_EFI_DEVICE_PATH_LENGTH (dp)))

/* Hardware Device Path.  */
#define GRUB_EFI_HARDWARE_DEVICE_PATH_TYPE		1

#define GRUB_EFI_PCI_DEVICE_PATH_SUBTYPE		1

struct grub_efi_pci_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint8_t function;
  grub_efi_uint8_t device;
} GRUB_PACKED;
typedef struct grub_efi_pci_device_path grub_efi_pci_device_path_t;

#define GRUB_EFI_PCCARD_DEVICE_PATH_SUBTYPE		2

struct grub_efi_pccard_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint8_t function;
} GRUB_PACKED;
typedef struct grub_efi_pccard_device_path grub_efi_pccard_device_path_t;

#define GRUB_EFI_MEMORY_MAPPED_DEVICE_PATH_SUBTYPE	3

struct grub_efi_memory_mapped_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint32_t memory_type;
  grub_efi_physical_address_t start_address;
  grub_efi_physical_address_t end_address;
} GRUB_PACKED;
typedef struct grub_efi_memory_mapped_device_path grub_efi_memory_mapped_device_path_t;

#define GRUB_EFI_VENDOR_DEVICE_PATH_SUBTYPE		4

struct grub_efi_vendor_device_path
{
  grub_efi_device_path_t header;
  grub_efi_packed_guid_t vendor_guid;
  grub_efi_uint8_t vendor_defined_data[0];
} GRUB_PACKED;
typedef struct grub_efi_vendor_device_path grub_efi_vendor_device_path_t;

#define GRUB_EFI_CONTROLLER_DEVICE_PATH_SUBTYPE		5

struct grub_efi_controller_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint32_t controller_number;
} GRUB_PACKED;
typedef struct grub_efi_controller_device_path grub_efi_controller_device_path_t;

/* ACPI Device Path.  */
#define GRUB_EFI_ACPI_DEVICE_PATH_TYPE			2

#define GRUB_EFI_ACPI_DEVICE_PATH_SUBTYPE		1

struct grub_efi_acpi_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint32_t hid;
  grub_efi_uint32_t uid;
} GRUB_PACKED;
typedef struct grub_efi_acpi_device_path grub_efi_acpi_device_path_t;

#define GRUB_EFI_EXPANDED_ACPI_DEVICE_PATH_SUBTYPE	2

struct grub_efi_expanded_acpi_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint32_t hid;
  grub_efi_uint32_t uid;
  grub_efi_uint32_t cid;
  char hidstr[0];
} GRUB_PACKED;
typedef struct grub_efi_expanded_acpi_device_path grub_efi_expanded_acpi_device_path_t;

#define GRUB_EFI_EXPANDED_ACPI_HIDSTR(dp)	\
  (((grub_efi_expanded_acpi_device_path_t *) dp)->hidstr)
#define GRUB_EFI_EXPANDED_ACPI_UIDSTR(dp)	\
  (GRUB_EFI_EXPANDED_ACPI_HIDSTR(dp) \
   + grub_strlen (GRUB_EFI_EXPANDED_ACPI_HIDSTR(dp)) + 1)
#define GRUB_EFI_EXPANDED_ACPI_CIDSTR(dp)	\
  (GRUB_EFI_EXPANDED_ACPI_UIDSTR(dp) \
   + grub_strlen (GRUB_EFI_EXPANDED_ACPI_UIDSTR(dp)) + 1)

/* Messaging Device Path.  */
#define GRUB_EFI_MESSAGING_DEVICE_PATH_TYPE		3

#define GRUB_EFI_ATAPI_DEVICE_PATH_SUBTYPE		1

struct grub_efi_atapi_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint8_t primary_secondary;
  grub_efi_uint8_t slave_master;
  grub_efi_uint16_t lun;
} GRUB_PACKED;
typedef struct grub_efi_atapi_device_path grub_efi_atapi_device_path_t;

#define GRUB_EFI_SCSI_DEVICE_PATH_SUBTYPE		2

struct grub_efi_scsi_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint16_t pun;
  grub_efi_uint16_t lun;
} GRUB_PACKED;
typedef struct grub_efi_scsi_device_path grub_efi_scsi_device_path_t;

#define GRUB_EFI_FIBRE_CHANNEL_DEVICE_PATH_SUBTYPE	3

struct grub_efi_fibre_channel_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint32_t reserved;
  grub_efi_uint64_t wwn;
  grub_efi_uint64_t lun;
} GRUB_PACKED;
typedef struct grub_efi_fibre_channel_device_path grub_efi_fibre_channel_device_path_t;

#define GRUB_EFI_1394_DEVICE_PATH_SUBTYPE		4

struct grub_efi_1394_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint32_t reserved;
  grub_efi_uint64_t guid;
} GRUB_PACKED;
typedef struct grub_efi_1394_device_path grub_efi_1394_device_path_t;

#define GRUB_EFI_USB_DEVICE_PATH_SUBTYPE		5

struct grub_efi_usb_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint8_t parent_port_number;
  grub_efi_uint8_t usb_interface;
} GRUB_PACKED;
typedef struct grub_efi_usb_device_path grub_efi_usb_device_path_t;

#define GRUB_EFI_USB_CLASS_DEVICE_PATH_SUBTYPE		15

struct grub_efi_usb_class_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint16_t vendor_id;
  grub_efi_uint16_t product_id;
  grub_efi_uint8_t device_class;
  grub_efi_uint8_t device_subclass;
  grub_efi_uint8_t device_protocol;
} GRUB_PACKED;
typedef struct grub_efi_usb_class_device_path grub_efi_usb_class_device_path_t;

#define GRUB_EFI_I2O_DEVICE_PATH_SUBTYPE		6

struct grub_efi_i2o_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint32_t tid;
} GRUB_PACKED;
typedef struct grub_efi_i2o_device_path grub_efi_i2o_device_path_t;

#define GRUB_EFI_MAC_ADDRESS_DEVICE_PATH_SUBTYPE	11

struct grub_efi_mac_address_device_path
{
  grub_efi_device_path_t header;
  grub_efi_mac_address_t mac_address;
  grub_efi_uint8_t if_type;
} GRUB_PACKED;
typedef struct grub_efi_mac_address_device_path grub_efi_mac_address_device_path_t;

#define GRUB_EFI_IPV4_DEVICE_PATH_SUBTYPE		12

struct grub_efi_ipv4_device_path
{
  grub_efi_device_path_t header;
  grub_efi_ipv4_address_t local_ip_address;
  grub_efi_ipv4_address_t remote_ip_address;
  grub_efi_uint16_t local_port;
  grub_efi_uint16_t remote_port;
  grub_efi_uint16_t protocol;
  grub_efi_uint8_t static_ip_address;
} GRUB_PACKED;
typedef struct grub_efi_ipv4_device_path grub_efi_ipv4_device_path_t;

#define GRUB_EFI_IPV6_DEVICE_PATH_SUBTYPE		13

struct grub_efi_ipv6_device_path
{
  grub_efi_device_path_t header;
  grub_efi_ipv6_address_t local_ip_address;
  grub_efi_ipv6_address_t remote_ip_address;
  grub_efi_uint16_t local_port;
  grub_efi_uint16_t remote_port;
  grub_efi_uint16_t protocol;
  grub_efi_uint8_t static_ip_address;
} GRUB_PACKED;
typedef struct grub_efi_ipv6_device_path grub_efi_ipv6_device_path_t;

#define GRUB_EFI_INFINIBAND_DEVICE_PATH_SUBTYPE		9

struct grub_efi_infiniband_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint32_t resource_flags;
  grub_efi_uint8_t port_gid[16];
  grub_efi_uint64_t remote_id;
  grub_efi_uint64_t target_port_id;
  grub_efi_uint64_t device_id;
} GRUB_PACKED;
typedef struct grub_efi_infiniband_device_path grub_efi_infiniband_device_path_t;

#define GRUB_EFI_UART_DEVICE_PATH_SUBTYPE		14

struct grub_efi_uart_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint32_t reserved;
  grub_efi_uint64_t baud_rate;
  grub_efi_uint8_t data_bits;
  grub_efi_uint8_t parity;
  grub_efi_uint8_t stop_bits;
} GRUB_PACKED;
typedef struct grub_efi_uart_device_path grub_efi_uart_device_path_t;

#define GRUB_EFI_SATA_DEVICE_PATH_SUBTYPE		18

struct grub_efi_sata_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint16_t hba_port;
  grub_efi_uint16_t multiplier_port;
  grub_efi_uint16_t lun;
} GRUB_PACKED;
typedef struct grub_efi_sata_device_path grub_efi_sata_device_path_t;

#define GRUB_EFI_VENDOR_MESSAGING_DEVICE_PATH_SUBTYPE	10

/* Media Device Path.  */
#define GRUB_EFI_MEDIA_DEVICE_PATH_TYPE			4

#define GRUB_EFI_HARD_DRIVE_DEVICE_PATH_SUBTYPE		1

struct grub_efi_hard_drive_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint32_t partition_number;
  grub_efi_lba_t partition_start;
  grub_efi_lba_t partition_size;
  grub_efi_uint8_t partition_signature[16];
  grub_efi_uint8_t partmap_type;
  grub_efi_uint8_t signature_type;
} GRUB_PACKED;
typedef struct grub_efi_hard_drive_device_path grub_efi_hard_drive_device_path_t;

#define GRUB_EFI_CDROM_DEVICE_PATH_SUBTYPE		2

struct grub_efi_cdrom_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint32_t boot_entry;
  grub_efi_lba_t partition_start;
  grub_efi_lba_t partition_size;
} GRUB_PACKED;
typedef struct grub_efi_cdrom_device_path grub_efi_cdrom_device_path_t;

#define GRUB_EFI_VENDOR_MEDIA_DEVICE_PATH_SUBTYPE	3

struct grub_efi_vendor_media_device_path
{
  grub_efi_device_path_t header;
  grub_efi_packed_guid_t vendor_guid;
  grub_efi_uint8_t vendor_defined_data[0];
} GRUB_PACKED;
typedef struct grub_efi_vendor_media_device_path grub_efi_vendor_media_device_path_t;

#define GRUB_EFI_FILE_PATH_DEVICE_PATH_SUBTYPE		4

struct grub_efi_file_path_device_path
{
  grub_efi_device_path_t header;
  grub_efi_char16_t path_name[0];
} GRUB_PACKED;
typedef struct grub_efi_file_path_device_path grub_efi_file_path_device_path_t;

#define GRUB_EFI_PROTOCOL_DEVICE_PATH_SUBTYPE		5

struct grub_efi_protocol_device_path
{
  grub_efi_device_path_t header;
  grub_efi_packed_guid_t guid;
} GRUB_PACKED;
typedef struct grub_efi_protocol_device_path grub_efi_protocol_device_path_t;

#define GRUB_EFI_PIWG_DEVICE_PATH_SUBTYPE		6

struct grub_efi_piwg_device_path
{
  grub_efi_device_path_t header;
  grub_efi_packed_guid_t guid;
} GRUB_PACKED;
typedef struct grub_efi_piwg_device_path grub_efi_piwg_device_path_t;


/* BIOS Boot Specification Device Path.  */
#define GRUB_EFI_BIOS_DEVICE_PATH_TYPE			5

#define GRUB_EFI_BIOS_DEVICE_PATH_SUBTYPE		1

struct grub_efi_bios_device_path
{
  grub_efi_device_path_t header;
  grub_efi_uint16_t device_type;
  grub_efi_uint16_t status_flags;
  char description[0];
} GRUB_PACKED;
typedef struct grub_efi_bios_device_path grub_efi_bios_device_path_t;

struct grub_efi_open_protocol_information_entry
{
  grub_efi_handle_t agent_handle;
  grub_efi_handle_t controller_handle;
  grub_efi_uint32_t attributes;
  grub_efi_uint32_t open_count;
};
typedef struct grub_efi_open_protocol_information_entry grub_efi_open_protocol_information_entry_t;

struct grub_efi_time
{
  grub_efi_uint16_t year;
  grub_efi_uint8_t month;
  grub_efi_uint8_t day;
  grub_efi_uint8_t hour;
  grub_efi_uint8_t minute;
  grub_efi_uint8_t second;
  grub_efi_uint8_t pad1;
  grub_efi_uint32_t nanosecond;
  grub_efi_int16_t time_zone;
  grub_efi_uint8_t daylight;
  grub_efi_uint8_t pad2;
} GRUB_PACKED;
typedef struct grub_efi_time grub_efi_time_t;

struct grub_efi_time_capabilities
{
  grub_efi_uint32_t resolution;
  grub_efi_uint32_t accuracy;
  grub_efi_boolean_t sets_to_zero;
};
typedef struct grub_efi_time_capabilities grub_efi_time_capabilities_t;

struct grub_efi_input_key
{
  grub_efi_uint16_t scan_code;
  grub_efi_char16_t unicode_char;
};
typedef struct grub_efi_input_key grub_efi_input_key_t;

typedef grub_efi_uint8_t grub_efi_key_toggle_state_t;
struct grub_efi_key_state
{
	grub_efi_uint32_t key_shift_state;
	grub_efi_key_toggle_state_t key_toggle_state;
};
typedef struct grub_efi_key_state grub_efi_key_state_t;

#define GRUB_EFI_SHIFT_STATE_VALID     0x80000000
#define GRUB_EFI_RIGHT_SHIFT_PRESSED   0x00000001
#define GRUB_EFI_LEFT_SHIFT_PRESSED    0x00000002
#define GRUB_EFI_RIGHT_CONTROL_PRESSED 0x00000004
#define GRUB_EFI_LEFT_CONTROL_PRESSED  0x00000008
#define GRUB_EFI_RIGHT_ALT_PRESSED     0x00000010
#define GRUB_EFI_LEFT_ALT_PRESSED      0x00000020
#define GRUB_EFI_RIGHT_LOGO_PRESSED    0x00000040
#define GRUB_EFI_LEFT_LOGO_PRESSED     0x00000080
#define GRUB_EFI_MENU_KEY_PRESSED      0x00000100
#define GRUB_EFI_SYS_REQ_PRESSED       0x00000200

#define GRUB_EFI_TOGGLE_STATE_VALID 0x80
#define GRUB_EFI_KEY_STATE_EXPOSED  0x40
#define GRUB_EFI_SCROLL_LOCK_ACTIVE 0x01
#define GRUB_EFI_NUM_LOCK_ACTIVE    0x02
#define GRUB_EFI_CAPS_LOCK_ACTIVE   0x04

struct grub_efi_simple_text_output_mode
{
  grub_efi_int32_t max_mode;
  grub_efi_int32_t mode;
  grub_efi_int32_t attribute;
  grub_efi_int32_t cursor_column;
  grub_efi_int32_t cursor_row;
  grub_efi_boolean_t cursor_visible;
};
typedef struct grub_efi_simple_text_output_mode grub_efi_simple_text_output_mode_t;

/* Tables.  */
struct grub_efi_table_header
{
  grub_efi_uint64_t signature;
  grub_efi_uint32_t revision;
  grub_efi_uint32_t header_size;
  grub_efi_uint32_t crc32;
  grub_efi_uint32_t reserved;
};
typedef struct grub_efi_table_header grub_efi_table_header_t;

struct grub_efi_boot_services
{
  grub_efi_table_header_t hdr;

  grub_efi_tpl_t
  (*raise_tpl) (grub_efi_tpl_t new_tpl);

  void
  (*restore_tpl) (grub_efi_tpl_t old_tpl);

  grub_efi_status_t
  (*allocate_pages) (grub_efi_allocate_type_t type,
		     grub_efi_memory_type_t memory_type,
		     grub_efi_uintn_t pages,
		     grub_efi_physical_address_t *memory);

  grub_efi_status_t
  (*free_pages) (grub_efi_physical_address_t memory,
		 grub_efi_uintn_t pages);

  grub_efi_status_t
  (*get_memory_map) (grub_efi_uintn_t *memory_map_size,
		     grub_efi_memory_descriptor_t *memory_map,
		     grub_efi_uintn_t *map_key,
		     grub_efi_uintn_t *descriptor_size,
		     grub_efi_uint32_t *descriptor_version);

  grub_efi_status_t
  (*allocate_pool) (grub_efi_memory_type_t pool_type,
		    grub_efi_uintn_t size,
		    void **buffer);

  grub_efi_status_t
  (*free_pool) (void *buffer);

  grub_efi_status_t
  (*create_event) (grub_efi_uint32_t type,
		   grub_efi_tpl_t notify_tpl,
		   void (*notify_function) (grub_efi_event_t event,
					    void *context),
		   void *notify_context,
		   grub_efi_event_t *event);

  grub_efi_status_t
  (*set_timer) (grub_efi_event_t event,
		grub_efi_timer_delay_t type,
		grub_efi_uint64_t trigger_time);

   grub_efi_status_t
   (*wait_for_event) (grub_efi_uintn_t num_events,
		      grub_efi_event_t *event,
		      grub_efi_uintn_t *index);

  grub_efi_status_t
  (*signal_event) (grub_efi_event_t event);

  grub_efi_status_t
  (*close_event) (grub_efi_event_t event);

  grub_efi_status_t
  (*check_event) (grub_efi_event_t event);

   grub_efi_status_t
   (*install_protocol_interface) (grub_efi_handle_t *handle,
				  grub_efi_guid_t *protocol,
				  grub_efi_interface_type_t protocol_interface_type,
				  void *protocol_interface);

  grub_efi_status_t
  (*reinstall_protocol_interface) (grub_efi_handle_t handle,
				   grub_efi_guid_t *protocol,
				   void *old_interface,
				   void *new_interface);

  grub_efi_status_t
  (*uninstall_protocol_interface) (grub_efi_handle_t handle,
				   grub_efi_guid_t *protocol,
				   void *protocol_interface);

  grub_efi_status_t
  (*handle_protocol) (grub_efi_handle_t handle,
		      grub_efi_guid_t *protocol,
		      void **protocol_interface);

  void *reserved;

  grub_efi_status_t
  (*register_protocol_notify) (grub_efi_guid_t *protocol,
			       grub_efi_event_t event,
			       void **registration);

  grub_efi_status_t
  (*locate_handle) (grub_efi_locate_search_type_t search_type,
		    grub_efi_guid_t *protocol,
		    void *search_key,
		    grub_efi_uintn_t *buffer_size,
		    grub_efi_handle_t *buffer);

  grub_efi_status_t
  (*locate_device_path) (grub_efi_guid_t *protocol,
			 grub_efi_device_path_t **device_path,
			 grub_efi_handle_t *device);

  grub_efi_status_t
  (*install_configuration_table) (grub_efi_guid_t *guid, void *table);

  grub_efi_status_t
  (*load_image) (grub_efi_boolean_t boot_policy,
		 grub_efi_handle_t parent_image_handle,
		 grub_efi_device_path_t *file_path,
		 void *source_buffer,
		 grub_efi_uintn_t source_size,
		 grub_efi_handle_t *image_handle);

  grub_efi_status_t
  (*start_image) (grub_efi_handle_t image_handle,
		  grub_efi_uintn_t *exit_data_size,
		  grub_efi_char16_t **exit_data);

  grub_efi_status_t
  (*exit) (grub_efi_handle_t image_handle,
	   grub_efi_status_t exit_status,
	   grub_efi_uintn_t exit_data_size,
	   grub_efi_char16_t *exit_data) __attribute__((noreturn));

  grub_efi_status_t
  (*unload_image) (grub_efi_handle_t image_handle);

  grub_efi_status_t
  (*exit_boot_services) (grub_efi_handle_t image_handle,
			 grub_efi_uintn_t map_key);

  grub_efi_status_t
  (*get_next_monotonic_count) (grub_efi_uint64_t *count);

  grub_efi_status_t
  (*stall) (grub_efi_uintn_t microseconds);

  grub_efi_status_t
  (*set_watchdog_timer) (grub_efi_uintn_t timeout,
			 grub_efi_uint64_t watchdog_code,
			 grub_efi_uintn_t data_size,
			 grub_efi_char16_t *watchdog_data);

  grub_efi_status_t
  (*connect_controller) (grub_efi_handle_t controller_handle,
			 grub_efi_handle_t *driver_image_handle,
			 grub_efi_device_path_protocol_t *remaining_device_path,
			 grub_efi_boolean_t recursive);

  grub_efi_status_t
  (*disconnect_controller) (grub_efi_handle_t controller_handle,
			    grub_efi_handle_t driver_image_handle,
			    grub_efi_handle_t child_handle);

  grub_efi_status_t
  (*open_protocol) (grub_efi_handle_t handle,
		    grub_efi_guid_t *protocol,
		    void **protocol_interface,
		    grub_efi_handle_t agent_handle,
		    grub_efi_handle_t controller_handle,
		    grub_efi_uint32_t attributes);

  grub_efi_status_t
  (*close_protocol) (grub_efi_handle_t handle,
		     grub_efi_guid_t *protocol,
		     grub_efi_handle_t agent_handle,
		     grub_efi_handle_t controller_handle);

  grub_efi_status_t
  (*open_protocol_information) (grub_efi_handle_t handle,
				grub_efi_guid_t *protocol,
				grub_efi_open_protocol_information_entry_t **entry_buffer,
				grub_efi_uintn_t *entry_count);

  grub_efi_status_t
  (*protocols_per_handle) (grub_efi_handle_t handle,
			   grub_efi_packed_guid_t ***protocol_buffer,
			   grub_efi_uintn_t *protocol_buffer_count);

  grub_efi_status_t
  (*locate_handle_buffer) (grub_efi_locate_search_type_t search_type,
			   grub_efi_guid_t *protocol,
			   void *search_key,
			   grub_efi_uintn_t *no_handles,
			   grub_efi_handle_t **buffer);

  grub_efi_status_t
  (*locate_protocol) (grub_efi_guid_t *protocol,
		      void *registration,
		      void **protocol_interface);

  grub_efi_status_t
  (*install_multiple_protocol_interfaces) (grub_efi_handle_t *handle, ...);

  grub_efi_status_t
  (*uninstall_multiple_protocol_interfaces) (grub_efi_handle_t handle, ...);

  grub_efi_status_t
  (*calculate_crc32) (void *data,
		      grub_efi_uintn_t data_size,
		      grub_efi_uint32_t *crc32);

  void
  (*copy_mem) (void *destination, void *source, grub_efi_uintn_t length);

  void
  (*set_mem) (void *buffer, grub_efi_uintn_t size, grub_efi_uint8_t value);
};
typedef struct grub_efi_boot_services grub_efi_boot_services_t;

struct grub_efi_runtime_services
{
  grub_efi_table_header_t hdr;

  grub_efi_status_t
  (*get_time) (grub_efi_time_t *time,
	       grub_efi_time_capabilities_t *capabilities);

  grub_efi_status_t
  (*set_time) (grub_efi_time_t *time);

  grub_efi_status_t
  (*get_wakeup_time) (grub_efi_boolean_t *enabled,
		      grub_efi_boolean_t *pending,
		      grub_efi_time_t *time);

  grub_efi_status_t
  (*set_wakeup_time) (grub_efi_boolean_t enabled,
		      grub_efi_time_t *time);

  grub_efi_status_t
  (*set_virtual_address_map) (grub_efi_uintn_t memory_map_size,
			      grub_efi_uintn_t descriptor_size,
			      grub_efi_uint32_t descriptor_version,
			      grub_efi_memory_descriptor_t *virtual_map);

  grub_efi_status_t
  (*convert_pointer) (grub_efi_uintn_t debug_disposition, void **address);

#define GRUB_EFI_GLOBAL_VARIABLE_GUID \
  { 0x8BE4DF61, 0x93CA, 0x11d2, { 0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B,0x8C }}


  grub_efi_status_t
  (*get_variable) (grub_efi_char16_t *variable_name,
		   const grub_efi_guid_t *vendor_guid,
		   grub_efi_uint32_t *attributes,
		   grub_efi_uintn_t *data_size,
		   void *data);

  grub_efi_status_t
  (*get_next_variable_name) (grub_efi_uintn_t *variable_name_size,
			     grub_efi_char16_t *variable_name,
			     grub_efi_guid_t *vendor_guid);

  grub_efi_status_t
  (*set_variable) (grub_efi_char16_t *variable_name,
		   const grub_efi_guid_t *vendor_guid,
		   grub_efi_uint32_t attributes,
		   grub_efi_uintn_t data_size,
		   void *data);

  grub_efi_status_t
  (*get_next_high_monotonic_count) (grub_efi_uint32_t *high_count);

  void
  (*reset_system) (grub_efi_reset_type_t reset_type,
		   grub_efi_status_t reset_status,
		   grub_efi_uintn_t data_size,
		   grub_efi_char16_t *reset_data);
};
typedef struct grub_efi_runtime_services grub_efi_runtime_services_t;

struct grub_efi_configuration_table
{
  grub_efi_packed_guid_t vendor_guid;
  void *vendor_table;
} GRUB_PACKED;
typedef struct grub_efi_configuration_table grub_efi_configuration_table_t;

#define GRUB_EFIEMU_SYSTEM_TABLE_SIGNATURE 0x5453595320494249LL
#define GRUB_EFIEMU_RUNTIME_SERVICES_SIGNATURE 0x56524553544e5552LL

struct grub_efi_serial_io_interface
{
  grub_efi_uint32_t revision;
  void (*reset) (void);
  grub_efi_status_t (*set_attributes) (struct grub_efi_serial_io_interface *this,
				       grub_efi_uint64_t speed,
				       grub_efi_uint32_t fifo_depth,
				       grub_efi_uint32_t timeout,
				       grub_efi_parity_type_t parity,
				       grub_uint8_t word_len,
				       grub_efi_stop_bits_t stop_bits);
  grub_efi_status_t (*set_control_bits) (struct grub_efi_serial_io_interface *this,
					 grub_efi_uint32_t flags);
  void (*get_control_bits) (void);
  grub_efi_status_t (*write) (struct grub_efi_serial_io_interface *this,
			      grub_efi_uintn_t *buf_size,
			      void *buffer);
  grub_efi_status_t (*read) (struct grub_efi_serial_io_interface *this,
			     grub_efi_uintn_t *buf_size,
			     void *buffer);
};

struct grub_efi_simple_input_interface
{
  grub_efi_status_t
  (*reset) (struct grub_efi_simple_input_interface *this,
	    grub_efi_boolean_t extended_verification);

  grub_efi_status_t
  (*read_key_stroke) (struct grub_efi_simple_input_interface *this,
		      grub_efi_input_key_t *key);

  grub_efi_event_t wait_for_key;
};
typedef struct grub_efi_simple_input_interface grub_efi_simple_input_interface_t;

struct grub_efi_key_data {
	grub_efi_input_key_t key;
	grub_efi_key_state_t key_state;
};
typedef struct grub_efi_key_data grub_efi_key_data_t;

typedef grub_efi_status_t (*grub_efi_key_notify_function_t) (
	grub_efi_key_data_t *key_data
	);

struct grub_efi_simple_text_input_ex_interface
{
	grub_efi_status_t
	(*reset) (struct grub_efi_simple_text_input_ex_interface *this,
		  grub_efi_boolean_t extended_verification);

	grub_efi_status_t
	(*read_key_stroke) (struct grub_efi_simple_text_input_ex_interface *this,
			    grub_efi_key_data_t *key_data);

	grub_efi_event_t wait_for_key;

	grub_efi_status_t
	(*set_state) (struct grub_efi_simple_text_input_ex_interface *this,
		      grub_efi_key_toggle_state_t *key_toggle_state);

	grub_efi_status_t
	(*register_key_notify) (struct grub_efi_simple_text_input_ex_interface *this,
				grub_efi_key_data_t *key_data,
				grub_efi_key_notify_function_t key_notification_function);

	grub_efi_status_t
	(*unregister_key_notify) (struct grub_efi_simple_text_input_ex_interface *this,
				  void *notification_handle);
};
typedef struct grub_efi_simple_text_input_ex_interface grub_efi_simple_text_input_ex_interface_t;

struct grub_efi_simple_text_output_interface
{
  grub_efi_status_t
  (*reset) (struct grub_efi_simple_text_output_interface *this,
	    grub_efi_boolean_t extended_verification);

  grub_efi_status_t
  (*output_string) (struct grub_efi_simple_text_output_interface *this,
		    grub_efi_char16_t *string);

  grub_efi_status_t
  (*test_string) (struct grub_efi_simple_text_output_interface *this,
		  grub_efi_char16_t *string);

  grub_efi_status_t
  (*query_mode) (struct grub_efi_simple_text_output_interface *this,
		 grub_efi_uintn_t mode_number,
		 grub_efi_uintn_t *columns,
		 grub_efi_uintn_t *rows);

  grub_efi_status_t
  (*set_mode) (struct grub_efi_simple_text_output_interface *this,
	       grub_efi_uintn_t mode_number);

  grub_efi_status_t
  (*set_attributes) (struct grub_efi_simple_text_output_interface *this,
		     grub_efi_uintn_t attribute);

  grub_efi_status_t
  (*clear_screen) (struct grub_efi_simple_text_output_interface *this);

  grub_efi_status_t
  (*set_cursor_position) (struct grub_efi_simple_text_output_interface *this,
			  grub_efi_uintn_t column,
			  grub_efi_uintn_t row);

  grub_efi_status_t
  (*enable_cursor) (struct grub_efi_simple_text_output_interface *this,
		    grub_efi_boolean_t visible);

  grub_efi_simple_text_output_mode_t *mode;
};
typedef struct grub_efi_simple_text_output_interface grub_efi_simple_text_output_interface_t;

typedef grub_uint8_t grub_efi_pxe_packet_t[1472];

typedef struct grub_efi_pxe_mode
{
  grub_uint8_t unused[52];
  grub_efi_pxe_packet_t dhcp_discover;
  grub_efi_pxe_packet_t dhcp_ack;
  grub_efi_pxe_packet_t proxy_offer;
  grub_efi_pxe_packet_t pxe_discover;
  grub_efi_pxe_packet_t pxe_reply;
} grub_efi_pxe_mode_t;

typedef struct grub_efi_pxe
{
  grub_uint64_t rev;
  void (*start) (void);
  void (*stop) (void);
  void (*dhcp) (void);
  void (*discover) (void);
  void (*mftp) (void);
  void (*udpwrite) (void);
  void (*udpread) (void);
  void (*setipfilter) (void);
  void (*arp) (void);
  void (*setparams) (void);
  void (*setstationip) (void);
  void (*setpackets) (void);
  struct grub_efi_pxe_mode *mode;
} grub_efi_pxe_t;

#define GRUB_EFI_BLACK		0x00
#define GRUB_EFI_BLUE		0x01
#define GRUB_EFI_GREEN		0x02
#define GRUB_EFI_CYAN		0x03
#define GRUB_EFI_RED		0x04
#define GRUB_EFI_MAGENTA	0x05
#define GRUB_EFI_BROWN		0x06
#define GRUB_EFI_LIGHTGRAY	0x07
#define GRUB_EFI_BRIGHT		0x08
#define GRUB_EFI_DARKGRAY	0x08
#define GRUB_EFI_LIGHTBLUE	0x09
#define GRUB_EFI_LIGHTGREEN	0x0A
#define GRUB_EFI_LIGHTCYAN	0x0B
#define GRUB_EFI_LIGHTRED	0x0C
#define GRUB_EFI_LIGHTMAGENTA	0x0D
#define GRUB_EFI_YELLOW		0x0E
#define GRUB_EFI_WHITE		0x0F

#define GRUB_EFI_BACKGROUND_BLACK	0x00
#define GRUB_EFI_BACKGROUND_BLUE	0x10
#define GRUB_EFI_BACKGROUND_GREEN	0x20
#define GRUB_EFI_BACKGROUND_CYAN	0x30
#define GRUB_EFI_BACKGROUND_RED		0x40
#define GRUB_EFI_BACKGROUND_MAGENTA	0x50
#define GRUB_EFI_BACKGROUND_BROWN	0x60
#define GRUB_EFI_BACKGROUND_LIGHTGRAY	0x70

#define GRUB_EFI_TEXT_ATTR(fg, bg)	((fg) | ((bg)))

struct grub_efi_system_table
{
  grub_efi_table_header_t hdr;
  grub_efi_char16_t *firmware_vendor;
  grub_efi_uint32_t firmware_revision;
  grub_efi_handle_t console_in_handler;
  grub_efi_simple_input_interface_t *con_in;
  grub_efi_handle_t console_out_handler;
  grub_efi_simple_text_output_interface_t *con_out;
  grub_efi_handle_t standard_error_handle;
  grub_efi_simple_text_output_interface_t *std_err;
  grub_efi_runtime_services_t *runtime_services;
  grub_efi_boot_services_t *boot_services;
  grub_efi_uintn_t num_table_entries;
  grub_efi_configuration_table_t *configuration_table;
};
typedef struct grub_efi_system_table  grub_efi_system_table_t;

struct grub_efi_loaded_image
{
  grub_efi_uint32_t revision;
  grub_efi_handle_t parent_handle;
  grub_efi_system_table_t *system_table;

  grub_efi_handle_t device_handle;
  grub_efi_device_path_t *file_path;
  void *reserved;

  grub_efi_uint32_t load_options_size;
  void *load_options;

  void *image_base;
  grub_efi_uint64_t image_size;
  grub_efi_memory_type_t image_code_type;
  grub_efi_memory_type_t image_data_type;

  grub_efi_status_t (*unload) (grub_efi_handle_t image_handle);
};
typedef struct grub_efi_loaded_image grub_efi_loaded_image_t;

struct grub_efi_disk_io
{
  grub_efi_uint64_t revision;
  grub_efi_status_t (*read) (struct grub_efi_disk_io *this,
			     grub_efi_uint32_t media_id,
			     grub_efi_uint64_t offset,
			     grub_efi_uintn_t buffer_size,
			     void *buffer);
  grub_efi_status_t (*write) (struct grub_efi_disk_io *this,
			     grub_efi_uint32_t media_id,
			     grub_efi_uint64_t offset,
			     grub_efi_uintn_t buffer_size,
			     void *buffer);
};
typedef struct grub_efi_disk_io grub_efi_disk_io_t;

struct grub_efi_block_io_media
{
  grub_efi_uint32_t media_id;
  grub_efi_boolean_t removable_media;
  grub_efi_boolean_t media_present;
  grub_efi_boolean_t logical_partition;
  grub_efi_boolean_t read_only;
  grub_efi_boolean_t write_caching;
  grub_efi_uint8_t pad[3];
  grub_efi_uint32_t block_size;
  grub_efi_uint32_t io_align;
  grub_efi_uint8_t pad2[4];
  grub_efi_lba_t last_block;
};
typedef struct grub_efi_block_io_media grub_efi_block_io_media_t;

typedef grub_uint8_t grub_efi_mac_t[32];

struct grub_efi_simple_network_mode
{
  grub_uint32_t state;
  grub_uint32_t hwaddr_size;
  grub_uint32_t media_header_size;
  grub_uint32_t max_packet_size;
  grub_uint32_t nvram_size;
  grub_uint32_t nvram_access_size;
  grub_uint32_t receive_filter_mask;
  grub_uint32_t receive_filter_setting;
  grub_uint32_t max_mcast_filter_count;
  grub_uint32_t mcast_filter_count;
  grub_efi_mac_t mcast_filter[16];
  grub_efi_mac_t current_address;
  grub_efi_mac_t broadcast_address;
  grub_efi_mac_t permanent_address;
  grub_uint8_t if_type;
  grub_uint8_t mac_changeable;
  grub_uint8_t multitx_supported;
  grub_uint8_t media_present_supported;
  grub_uint8_t media_present;
};

enum
  {
    GRUB_EFI_NETWORK_STOPPED,
    GRUB_EFI_NETWORK_STARTED,
    GRUB_EFI_NETWORK_INITIALIZED,
  };

enum
  {
    GRUB_EFI_SIMPLE_NETWORK_RECEIVE_UNICAST		  = 0x01,
    GRUB_EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST		  = 0x02,
    GRUB_EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST		  = 0x04,
    GRUB_EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS		  = 0x08,
    GRUB_EFI_SIMPLE_NETWORK_RECEIVE_PROMISCUOUS_MULTICAST = 0x10,
  };

struct grub_efi_simple_network
{
  grub_uint64_t revision;
  grub_efi_status_t (*start) (struct grub_efi_simple_network *this);
  grub_efi_status_t (*stop) (struct grub_efi_simple_network *this);
  grub_efi_status_t (*initialize) (struct grub_efi_simple_network *this,
				   grub_efi_uintn_t extra_rx,
				   grub_efi_uintn_t extra_tx);
  void (*reset) (void);
  grub_efi_status_t (*shutdown) (struct grub_efi_simple_network *this);
  grub_efi_status_t (*receive_filters) (struct grub_efi_simple_network *this,
					grub_uint32_t enable,
					grub_uint32_t disable,
					grub_efi_boolean_t reset_mcast_filter,
					grub_efi_uintn_t mcast_filter_count,
					grub_efi_mac_address_t *mcast_filter);
  void (*station_address) (void);
  void (*statistics) (void);
  void (*mcastiptomac) (void);
  void (*nvdata) (void);
  grub_efi_status_t (*get_status) (struct grub_efi_simple_network *this,
				   grub_uint32_t *int_status,
				   void **txbuf);
  grub_efi_status_t (*transmit) (struct grub_efi_simple_network *this,
				 grub_efi_uintn_t header_size,
				 grub_efi_uintn_t buffer_size,
				 void *buffer,
				 grub_efi_mac_t *src_addr,
				 grub_efi_mac_t *dest_addr,
				 grub_efi_uint16_t *protocol);
  grub_efi_status_t (*receive) (struct grub_efi_simple_network *this,
				grub_efi_uintn_t *header_size,
				grub_efi_uintn_t *buffer_size,
				void *buffer,
				grub_efi_mac_t *src_addr,
				grub_efi_mac_t *dest_addr,
				grub_uint16_t *protocol);
  void (*waitforpacket) (void);
  struct grub_efi_simple_network_mode *mode;
};
typedef struct grub_efi_simple_network grub_efi_simple_network_t;


struct grub_efi_block_io
{
  grub_efi_uint64_t revision;
  grub_efi_block_io_media_t *media;
  grub_efi_status_t (*reset) (struct grub_efi_block_io *this,
			      grub_efi_boolean_t extended_verification);
  grub_efi_status_t (*read_blocks) (struct grub_efi_block_io *this,
				    grub_efi_uint32_t media_id,
				    grub_efi_lba_t lba,
				    grub_efi_uintn_t buffer_size,
				    void *buffer);
  grub_efi_status_t (*write_blocks) (struct grub_efi_block_io *this,
				     grub_efi_uint32_t media_id,
				     grub_efi_lba_t lba,
				     grub_efi_uintn_t buffer_size,
				     void *buffer);
  grub_efi_status_t (*flush_blocks) (struct grub_efi_block_io *this);
};
typedef struct grub_efi_block_io grub_efi_block_io_t;

#if (GRUB_TARGET_SIZEOF_VOID_P == 4) || defined (__ia64__) \
  || defined (__aarch64__) || defined(__mips__) || defined (__MINGW64__) || defined (__CYGWIN__) \
  || defined(__riscv)

#define efi_call_0(func)		func()
#define efi_call_1(func, a)		func(a)
#define efi_call_2(func, a, b)		func(a, b)
#define efi_call_3(func, a, b, c)	func(a, b, c)
#define efi_call_4(func, a, b, c, d)	func(a, b, c, d)
#define efi_call_5(func, a, b, c, d, e)	func(a, b, c, d, e)
#define efi_call_6(func, a, b, c, d, e, f) func(a, b, c, d, e, f)
#define efi_call_7(func, a, b, c, d, e, f, g) func(a, b, c, d, e, f, g)
#define efi_call_10(func, a, b, c, d, e, f, g, h, i, j)	func(a, b, c, d, e, f, g, h, i, j)

#else

#define efi_call_0(func) \
  efi_wrap_0(func)
#define efi_call_1(func, a) \
  efi_wrap_1(func, (grub_uint64_t) (a))
#define efi_call_2(func, a, b) \
  efi_wrap_2(func, (grub_uint64_t) (a), (grub_uint64_t) (b))
#define efi_call_3(func, a, b, c) \
  efi_wrap_3(func, (grub_uint64_t) (a), (grub_uint64_t) (b), \
	     (grub_uint64_t) (c))
#define efi_call_4(func, a, b, c, d) \
  efi_wrap_4(func, (grub_uint64_t) (a), (grub_uint64_t) (b), \
	     (grub_uint64_t) (c), (grub_uint64_t) (d))
#define efi_call_5(func, a, b, c, d, e)	\
  efi_wrap_5(func, (grub_uint64_t) (a), (grub_uint64_t) (b), \
	     (grub_uint64_t) (c), (grub_uint64_t) (d), (grub_uint64_t) (e))
#define efi_call_6(func, a, b, c, d, e, f) \
  efi_wrap_6(func, (grub_uint64_t) (a), (grub_uint64_t) (b), \
	     (grub_uint64_t) (c), (grub_uint64_t) (d), (grub_uint64_t) (e), \
	     (grub_uint64_t) (f))
#define efi_call_7(func, a, b, c, d, e, f, g) \
  efi_wrap_7(func, (grub_uint64_t) (a), (grub_uint64_t) (b), \
	     (grub_uint64_t) (c), (grub_uint64_t) (d), (grub_uint64_t) (e), \
	     (grub_uint64_t) (f), (grub_uint64_t) (g))
#define efi_call_10(func, a, b, c, d, e, f, g, h, i, j) \
  efi_wrap_10(func, (grub_uint64_t) (a), (grub_uint64_t) (b), \
	      (grub_uint64_t) (c), (grub_uint64_t) (d), (grub_uint64_t) (e), \
	      (grub_uint64_t) (f), (grub_uint64_t) (g),	(grub_uint64_t) (h), \
	      (grub_uint64_t) (i), (grub_uint64_t) (j))

grub_uint64_t EXPORT_FUNC(efi_wrap_0) (void *func);
grub_uint64_t EXPORT_FUNC(efi_wrap_1) (void *func, grub_uint64_t arg1);
grub_uint64_t EXPORT_FUNC(efi_wrap_2) (void *func, grub_uint64_t arg1,
                                       grub_uint64_t arg2);
grub_uint64_t EXPORT_FUNC(efi_wrap_3) (void *func, grub_uint64_t arg1,
                                       grub_uint64_t arg2, grub_uint64_t arg3);
grub_uint64_t EXPORT_FUNC(efi_wrap_4) (void *func, grub_uint64_t arg1,
                                       grub_uint64_t arg2, grub_uint64_t arg3,
                                       grub_uint64_t arg4);
grub_uint64_t EXPORT_FUNC(efi_wrap_5) (void *func, grub_uint64_t arg1,
                                       grub_uint64_t arg2, grub_uint64_t arg3,
                                       grub_uint64_t arg4, grub_uint64_t arg5);
grub_uint64_t EXPORT_FUNC(efi_wrap_6) (void *func, grub_uint64_t arg1,
                                       grub_uint64_t arg2, grub_uint64_t arg3,
                                       grub_uint64_t arg4, grub_uint64_t arg5,
                                       grub_uint64_t arg6);
grub_uint64_t EXPORT_FUNC(efi_wrap_7) (void *func, grub_uint64_t arg1,
                                       grub_uint64_t arg2, grub_uint64_t arg3,
                                       grub_uint64_t arg4, grub_uint64_t arg5,
                                       grub_uint64_t arg6, grub_uint64_t arg7);
grub_uint64_t EXPORT_FUNC(efi_wrap_10) (void *func, grub_uint64_t arg1,
                                        grub_uint64_t arg2, grub_uint64_t arg3,
                                        grub_uint64_t arg4, grub_uint64_t arg5,
                                        grub_uint64_t arg6, grub_uint64_t arg7,
                                        grub_uint64_t arg8, grub_uint64_t arg9,
                                        grub_uint64_t arg10);
#endif

#endif /* ! GRUB_EFI_API_HEADER */
