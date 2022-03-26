/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2017  Free Software Foundation, Inc.
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

#ifndef GRUB_EFI_LOONGSON_HEADER
#define GRUB_EFI_LOONGSON_HEADER 1

#include <grub/types.h>

#include <grub/efi/api.h>

#define GRUB_EFI_LOONGSON_SMBIOS_TABLE_GUID	\
  { 0x4660f721, 0x2ec5, 0x416a, \
    { 0x89, 0x9a, 0x43, 0x18, 0x02, 0x50, 0xa0, 0xc9 } \
  }

#define GRUB_EFI_LOONGSON_MMAP_MAX 128
typedef enum
  {                   
    GRUB_EFI_LOONGSON_SYSTEM_RAM_LOW = 1,
    GRUB_EFI_LOONGSON_SYSTEM_RAM_HIGH,
    GRUB_EFI_LOONGSON_MEMORY_RESERVED,
    GRUB_EFI_LOONGSON_PCI_IO,
    GRUB_EFI_LOONGSON_PCI_MEM,
    GRUB_EFI_LOONGSON_CFG_REG,
    GRUB_EFI_LOONGSON_VIDEO_ROM,
    GRUB_EFI_LOONGSON_ADAPTER_ROM,
    GRUB_EFI_LOONGSON_ACPI_TABLE,
    GRUB_EFI_LOONGSON_SMBIOS_TABLE,
    GRUB_EFI_LOONGSON_UMA_VIDEO_RAM,
    GRUB_EFI_LOONGSON_VUMA_VIDEO_RAM,
    GRUB_EFI_LOONGSON_SYSTEM_RAM_LOW_DMA,
    GRUB_EFI_LOONGSON_SYSTEM_RAM_HIGH_DMA,
    GRUB_EFI_LOONGSON_ACPI_NVS,
    GRUB_EFI_LOONGSON_MAX_MEMORY_TYPE
  }
grub_efi_loongson_memory_type;

typedef struct
{
  grub_uint16_t vers;     /* version */
  grub_uint32_t nr_map;   /* number of memory_maps */
  grub_uint32_t mem_freq; /* memory frequence */
  struct mem_map {
    grub_uint32_t node_id;        /* node_id which memory attached to */
    grub_uint32_t mem_type;       /* system memory, pci memory, pci io, etc. */
    grub_uint64_t mem_start;      /* memory map start address */
    grub_uint32_t mem_size;       /* each memory_map size, not the total size */
  } map[GRUB_EFI_LOONGSON_MMAP_MAX];
} GRUB_PACKED
grub_efi_loongson_memory_map;

/*
 * Capability and feature descriptor structure for MIPS CPU
 */
typedef struct
{
  grub_uint16_t vers;         /* version */
  grub_uint32_t processor_id; /* PRID, e.g. 6305, 6306 */
  grub_uint32_t cputype;      /* Loongson_3A/3B, etc. */
  grub_uint32_t total_node;   /* num of total numa nodes */
  grub_uint16_t cpu_startup_core_id; /* Boot core id */
  grub_uint16_t reserved_cores_mask;
  grub_uint32_t cpu_clock_freq; /* cpu_clock */
  grub_uint32_t nr_cpus;
} GRUB_PACKED
grub_efi_loongson_cpu_info;

#define GRUB_EFI_LOONGSON_MAX_UARTS 64

typedef struct
{
  grub_uint32_t iotype; /* see include/linux/serial_core.h */
  grub_uint32_t uartclk;
  grub_uint32_t int_offset;
  grub_uint64_t uart_base;
} GRUB_PACKED
grub_efi_loongson_uart_device;

#define GRUB_EFI_LOONGSON_MAX_SENSORS 64

typedef struct
{
  char name[32];   /* a formal name */
  char label[64];  /* a flexible description */
  grub_uint32_t type;        /* SENSOR_* */
  grub_uint32_t id;          /* instance id of a sensor-class */
  grub_uint32_t fan_policy;
  grub_uint32_t fan_percent; /* only for constant speed policy */
  grub_uint64_t base_addr;   /* base address of device registers */
} GRUB_PACKED
grub_efi_loongson_sensor_device;

typedef struct
{
  grub_uint16_t vers;     /* version */
  grub_uint32_t ccnuma_smp; /* 0: no numa; 1: has numa */
  grub_uint32_t sing_double_channel; /* 1:single; 2:double */
  grub_uint32_t nr_uarts;
  grub_efi_loongson_uart_device uarts[GRUB_EFI_LOONGSON_MAX_UARTS];
  grub_uint32_t nr_sensors;
  grub_efi_loongson_sensor_device sensors[GRUB_EFI_LOONGSON_MAX_SENSORS];
  char has_ec;
  char ec_name[32];
  grub_uint64_t ec_base_addr;
  char has_tcm;
  char tcm_name[32];
  grub_uint64_t tcm_base_addr;
  grub_uint64_t workarounds; /* see workarounds.h */
} GRUB_PACKED
grub_efi_loongson_system_info;

typedef struct
{
  grub_uint16_t vers;
  grub_uint16_t size;
  grub_uint16_t rtr_bus;
  grub_uint16_t rtr_devfn;
  grub_uint32_t vendor;
  grub_uint32_t device;
  grub_uint32_t PIC_type;   /* conform use HT or PCI to route to CPU-PIC */
  grub_uint64_t ht_int_bit; /* 3A: 1<<24; 3B: 1<<16 */
  grub_uint64_t ht_enable;  /* irqs used in this PIC */
  grub_uint32_t node_id;    /* node id: 0x0-0; 0x1-1; 0x10-2; 0x11-3 */
  grub_uint64_t pci_mem_start_addr;
  grub_uint64_t pci_mem_end_addr;
  grub_uint64_t pci_io_start_addr;
  grub_uint64_t pci_io_end_addr;
  grub_uint64_t pci_config_addr;
  grub_uint32_t dma_mask_bits;
} GRUB_PACKED
grub_efi_loongson_irq_src_routing_table;

typedef struct
{
  grub_uint16_t vers; /* version */
  grub_uint16_t size;
  grub_uint8_t  flag;
  char description[64];
} GRUB_PACKED
grub_efi_loongson_interface_info;

#define GRUB_EFI_LOONGSON_MAX_RESOURCE_NUMBER 128

typedef struct
{
  grub_uint64_t start; /* resource start address */
  grub_uint64_t end;   /* resource end address */
  char name[64];
  grub_uint32_t flags;
}
grub_efi_loongson_resource;

/* arch specific additions */
typedef struct
{
}
grub_efi_loongson_archdev_data;

typedef struct
{
  char name[64];    /* hold the device name */
  grub_uint32_t num_resources; /* number of device_resource */
  /* for each device's resource */
  grub_efi_loongson_resource resource[GRUB_EFI_LOONGSON_MAX_RESOURCE_NUMBER];
  /* arch specific additions */
  grub_efi_loongson_archdev_data archdata;
}
grub_efi_loongson_board_devices;

typedef struct
{
  grub_uint16_t vers;     /* version */
  char special_name[64]; /* special_atribute_name */
  grub_uint32_t loongson_special_type; /* type of special device */
  /* for each device's resource */
  grub_efi_loongson_resource resource[GRUB_EFI_LOONGSON_MAX_RESOURCE_NUMBER];
}
grub_efi_loongson_special_attribute;

typedef struct
{
  grub_uint64_t memory_offset;    /* efi_loongson_memory_map struct offset */
  grub_uint64_t cpu_offset;       /* efi_loongson_cpuinfo struct offset */
  grub_uint64_t system_offset;    /* efi_loongson_system_info struct offset */
  grub_uint64_t irq_offset;       /* efi_loongson_irq_src_routing_table struct offset */
  grub_uint64_t interface_offset; /* interface_info struct offset */
  grub_uint64_t special_offset;   /* efi_loongson_special_attribute struct offset */
  grub_uint64_t boarddev_table_offset;  /* efi_loongson_board_devices offset */
}
grub_efi_loongson_params;

typedef struct
{
  grub_uint16_t vers;     /* version */
  grub_uint64_t vga_bios; /* vga_bios address */
  grub_efi_loongson_params lp;
}
grub_efi_loongson_smbios_table;

typedef struct
{
  grub_uint64_t reset_cold;
  grub_uint64_t reset_warm;
  grub_uint64_t reset_type;
  grub_uint64_t shutdown;
  grub_uint64_t do_suspend; /* NULL if not support */
}
grub_efi_loongson_reset_system;

typedef struct
{
  grub_uint64_t mps;    /* MPS table */
  grub_uint64_t acpi;   /* ACPI table (IA64 ext 0.71) */
  grub_uint64_t acpi20; /* ACPI table (ACPI 2.0) */
  grub_efi_loongson_smbios_table smbios; /* SM BIOS table */
  grub_uint64_t sal_systab; /* SAL system table */
  grub_uint64_t boot_info;  /* boot info table */
}
grub_efi_loongson;

typedef struct
{
  grub_efi_loongson efi;
  grub_efi_loongson_reset_system reset_system;
}
grub_efi_loongson_boot_params;

extern grub_uint64_t grub_efi_loongson_reset_system_addr;

extern void grub_efi_loongson_reset_cold (void);
extern void grub_efi_loongson_reset_warm (void);
extern void grub_efi_loongson_reset_shutdown (void);
extern void grub_efi_loongson_reset_suspend (void);

void grub_efi_loongson_init (void);
void grub_efi_loongson_fini (void);
void grub_efi_loongson_alloc_boot_params (void);
void grub_efi_loongson_free_boot_params (void);
void * grub_efi_loongson_get_smbios_table (void);

int EXPORT_FUNC(grub_efi_is_loongson) (void);

grub_uint8_t
EXPORT_FUNC(grub_efi_loongson_calculatesum8) (const grub_uint8_t *Buffer, grub_efi_uintn_t Length);

grub_uint8_t
EXPORT_FUNC(grub_efi_loongson_grub_calculatechecksum8) (const grub_uint8_t *Buffer, grub_efi_uintn_t Length);


void *
EXPORT_FUNC(grub_efi_loongson_get_boot_params) (void);

typedef struct _extention_list_hdr{
  grub_uint64_t  signature;
  grub_uint32_t  length;
  grub_uint8_t   revision;
  grub_uint8_t   checksum;
  struct  _extention_list_hdr *next;
}GRUB_PACKED
ext_list;

typedef struct bootparamsinterface {
  grub_uint64_t           signature;    //{'B', 'P', 'I', '_', '0', '_', '1'}
  grub_efi_system_table_t *systemtable;
  ext_list         *extlist;
}GRUB_PACKED
bootparamsinterface;

typedef struct {
  ext_list  header;         //  {'M', 'E', 'M'}
  grub_uint8_t mapcount;
  struct GRUB_PACKED memmap {
    grub_uint32_t memtype;
    grub_uint64_t memstart;
    grub_uint64_t memsize;
  } map[GRUB_EFI_LOONGSON_MMAP_MAX];
}GRUB_PACKED
mem_map;

typedef struct {
  ext_list header;          // {VBIOS}
  grub_uint64_t  vbiosaddr;
}GRUB_PACKED
vbios;

grub_uint32_t 
EXPORT_FUNC (grub_efi_loongson_memmap_sort) (struct memmap array[], grub_uint32_t length, mem_map * bpmem, grub_uint32_t index, grub_uint32_t memtype);
#endif /* ! GRUB_EFI_LOONGSON_HEADER */
