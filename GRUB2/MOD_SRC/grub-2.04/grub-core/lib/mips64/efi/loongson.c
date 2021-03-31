/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2017 Free Software Foundation, Inc.
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

#include <grub/mm.h>
#include <grub/cache.h>
#include <grub/efi/efi.h>
#include <grub/cpu/efi/memory.h>
#include <grub/cpu/memory.h>
#include <grub/machine/loongson.h>

#pragma GCC diagnostic ignored "-Wunused-function"

#define loongson_params (&loongson_boot_params->boot_params.efi.smbios.lp)
#define loongson_boot_params_size ALIGN_UP (sizeof (*loongson_boot_params), 8)
#define loongson_reset_code_size (&grub_efi_loongson_reset_end - &grub_efi_loongson_reset_start)

extern grub_uint8_t grub_efi_loongson_reset_start;
extern grub_uint8_t grub_efi_loongson_reset_end;

static struct
{
  grub_efi_loongson_boot_params boot_params;
  grub_efi_loongson_memory_map memory_map;
  grub_efi_loongson_cpu_info cpu_info;
  grub_efi_loongson_system_info system_info;
  grub_efi_loongson_irq_src_routing_table irq_src_routing_table;
  grub_efi_loongson_interface_info interface_info;
  grub_efi_loongson_special_attribute special_attribute;
  grub_efi_loongson_board_devices board_devices;
} GRUB_PACKED
* loongson_boot_params;

static void
grub_efi_loongson_init_reset_system (void)
{
  grub_efi_loongson_boot_params *boot_params;
  grub_uint8_t *reset_code_addr = (grub_uint8_t *) loongson_boot_params +
                                  loongson_boot_params_size;

  boot_params = &loongson_boot_params->boot_params;
  grub_efi_loongson_reset_system_addr =
                 (grub_uint64_t) grub_efi_system_table->runtime_services->reset_system;
  grub_memcpy (reset_code_addr, &grub_efi_loongson_reset_start, loongson_reset_code_size);
  grub_arch_sync_caches (reset_code_addr, loongson_reset_code_size);

  boot_params->reset_system.reset_cold = (grub_uint64_t) reset_code_addr +
                                         ((grub_uint64_t) &grub_efi_loongson_reset_cold -
                                          (grub_uint64_t) &grub_efi_loongson_reset_start);
  boot_params->reset_system.reset_warm = (grub_uint64_t) reset_code_addr +
                                         ((grub_uint64_t) &grub_efi_loongson_reset_warm -
                                          (grub_uint64_t) &grub_efi_loongson_reset_start);
  boot_params->reset_system.shutdown = (grub_uint64_t) reset_code_addr +
                                         ((grub_uint64_t) &grub_efi_loongson_reset_shutdown -
                                          (grub_uint64_t) &grub_efi_loongson_reset_start);
  boot_params->reset_system.do_suspend = (grub_uint64_t) reset_code_addr +
                                         ((grub_uint64_t) &grub_efi_loongson_reset_suspend -
                                          (grub_uint64_t) &grub_efi_loongson_reset_start);
}

static void
grub_efi_loongson_init_smbios (grub_efi_loongson_smbios_table *smbios_table)
{
  grub_efi_loongson_smbios_table *dst = &loongson_boot_params->boot_params.efi.smbios;

  dst->vers = smbios_table->vers;
  dst->vga_bios = smbios_table->vga_bios;
}

static void
grub_efi_loongson_init_cpu_info (grub_efi_loongson_smbios_table *smbios_table)
{
  grub_efi_loongson_cpu_info *src = (void *) smbios_table->lp.cpu_offset;
  grub_efi_loongson_cpu_info *dst = &loongson_boot_params->cpu_info;

  if (!src)
    return;

  grub_memcpy (dst, src, sizeof (grub_efi_loongson_cpu_info));
  loongson_params->cpu_offset = (grub_uint64_t) dst - (grub_uint64_t) loongson_params;
}

static void
grub_efi_loongson_init_system_info (grub_efi_loongson_smbios_table *smbios_table)
{
  grub_efi_loongson_system_info *src = (void *) smbios_table->lp.system_offset;
  grub_efi_loongson_system_info *dst = &loongson_boot_params->system_info;

  if (!src)
    return;

  grub_memcpy (dst, src, sizeof (grub_efi_loongson_system_info));
  loongson_params->system_offset = (grub_uint64_t) dst - (grub_uint64_t) loongson_params;
}

static void
grub_efi_loongson_init_irq_src_routing_table (grub_efi_loongson_smbios_table *smbios_table)
{
  grub_efi_loongson_irq_src_routing_table *src = (void *) smbios_table->lp.irq_offset;
  grub_efi_loongson_irq_src_routing_table *dst = &loongson_boot_params->irq_src_routing_table;

  if (!src)
    return;

  grub_memcpy (dst, src, sizeof (grub_efi_loongson_irq_src_routing_table));
  loongson_params->irq_offset = (grub_uint64_t) dst - (grub_uint64_t) loongson_params;
}

static void
grub_efi_loongson_init_interface_info (grub_efi_loongson_smbios_table *smbios_table)
{
  grub_efi_loongson_interface_info *src = (void *) smbios_table->lp.interface_offset;
  grub_efi_loongson_interface_info *dst = &loongson_boot_params->interface_info;

  if (!src)
    return;

  grub_memcpy (dst, src, sizeof (grub_efi_loongson_interface_info));
  loongson_params->interface_offset = (grub_uint64_t) dst - (grub_uint64_t) loongson_params;
}

static void
grub_efi_loongson_init_special_attribute (grub_efi_loongson_smbios_table *smbios_table)
{
  grub_efi_loongson_special_attribute *src = (void *) smbios_table->lp.special_offset;
  grub_efi_loongson_special_attribute *dst = &loongson_boot_params->special_attribute;

  if (!src)
    return;

  grub_memcpy (dst, src, sizeof (grub_efi_loongson_special_attribute));
  loongson_params->special_offset = (grub_uint64_t) dst - (grub_uint64_t) loongson_params;
}

static void
grub_efi_loongson_init_board_devices (grub_efi_loongson_smbios_table *smbios_table)
{
  grub_efi_loongson_board_devices *src = (void *) smbios_table->lp.boarddev_table_offset;
  grub_efi_loongson_board_devices *dst = &loongson_boot_params->board_devices;

  if (!src)
    return;

  grub_memcpy (dst, src, sizeof (grub_efi_loongson_board_devices));
  loongson_params->boarddev_table_offset = (grub_uint64_t) dst - (grub_uint64_t) loongson_params;
}

#define ADD_MEMORY_DESCRIPTOR(desc, size)	\
  ((grub_efi_memory_descriptor_t *) ((char *) (desc) + (size)))

static void
grub_efi_loongson_init_memory_map (grub_efi_loongson_smbios_table *smbios_table,
                                   grub_efi_memory_descriptor_t *mmap_buf,
                                   grub_efi_uintn_t mmap_size,
                                   grub_efi_uintn_t desc_size)
{
  grub_efi_loongson_memory_map *src = (void *) smbios_table->lp.memory_offset;
  grub_efi_loongson_memory_map *dst = &loongson_boot_params->memory_map;
  grub_efi_memory_descriptor_t *mmap_end;
  grub_efi_memory_descriptor_t *desc;
  grub_efi_memory_descriptor_t *desc_next;
  grub_efi_uint32_t mem_types_reserved[] =
    {
      1, // GRUB_EFI_RESERVED_MEMORY_TYPE
      0, // GRUB_EFI_LOADER_CODE
      0, // GRUB_EFI_LOADER_DATA
      0, // GRUB_EFI_BOOT_SERVICES_CODE
      0, // GRUB_EFI_BOOT_SERVICES_DATA
      1, // GRUB_EFI_RUNTIME_SERVICES_CODE
      1, // GRUB_EFI_RUNTIME_SERVICES_DATA
      0, // GRUB_EFI_CONVENTIONAL_MEMORY
      1, // GRUB_EFI_UNUSABLE_MEMORY
      0, // GRUB_EFI_ACPI_RECLAIM_MEMORY
      0, // GRUB_EFI_ACPI_MEMORY_NVS
      1, // GRUB_EFI_MEMORY_MAPPED_IO
      1, // GRUB_EFI_MEMORY_MAPPED_IO_PORT_SPACE
      1, // GRUB_EFI_PAL_CODE
      1, // GRUB_EFI_PERSISTENT_MEMORY
    };
  grub_uint32_t need_sort = 1;

  if (!src)
    return;

  dst->vers = src->vers;
  dst->nr_map = 0;
  dst->mem_freq = src->mem_freq;
  loongson_params->memory_offset = (grub_uint64_t) dst - (grub_uint64_t) loongson_params;

  if (!mmap_buf || !mmap_size || !desc_size)
    return;

  mmap_end = ADD_MEMORY_DESCRIPTOR (mmap_buf, mmap_size);

  /* drop reserved */
  for (desc = mmap_buf,
       desc_next = desc;
       desc < mmap_end;
       desc = ADD_MEMORY_DESCRIPTOR (desc, desc_size))
    {
      desc->type = mem_types_reserved[desc->type];
      if (desc->type)
        continue;

      if (desc != desc_next)
        *desc_next = *desc;
      desc_next = ADD_MEMORY_DESCRIPTOR (desc_next, desc_size);
    }
  mmap_end = desc_next;

  /* sort: low->high */
  while (need_sort)
    {
      need_sort = 0;

      for (desc = mmap_buf,
           desc_next = ADD_MEMORY_DESCRIPTOR (desc, desc_size);
           (desc < mmap_end) && (desc_next < mmap_end);
           desc = desc_next,
           desc_next = ADD_MEMORY_DESCRIPTOR (desc, desc_size))
        {
          grub_efi_memory_descriptor_t tmp;

          if (desc->physical_start <= desc_next->physical_start)
            continue;

          tmp = *desc;
          *desc = *desc_next;
          *desc_next = tmp;
          need_sort = 1;
        }
    }

  /* combine continuous memory map */
  for (desc = mmap_buf,
       desc_next = ADD_MEMORY_DESCRIPTOR (desc, desc_size);
       desc_next < mmap_end;
       desc_next = ADD_MEMORY_DESCRIPTOR (desc_next, desc_size))
    {
      grub_efi_physical_address_t prev_end = desc->physical_start + (desc->num_pages << 12);

      if (prev_end == desc_next->physical_start)
        {
          desc->num_pages += desc_next->num_pages;
          continue;
        }

      desc = ADD_MEMORY_DESCRIPTOR (desc, desc_size);
      grub_memcpy (desc, desc_next, desc_size);
    }
  mmap_end = ADD_MEMORY_DESCRIPTOR (desc, desc_size);

  /* write to loongson memory map */
  for (desc = mmap_buf;
       desc < mmap_end;
       desc = ADD_MEMORY_DESCRIPTOR (desc, desc_size))
    {
      grub_efi_physical_address_t physical_start = grub_vtop ((void *) desc->physical_start);
      grub_efi_physical_address_t physical_end = physical_start + (desc->num_pages << 12);

      physical_start = ALIGN_UP (physical_start, 0x100000);
      physical_end = ALIGN_DOWN (physical_end, 0x100000);

      if (physical_start >= physical_end || (physical_end - physical_start) < 0x100000)
        continue;

      dst->map[dst->nr_map].node_id = (desc->physical_start >> 44) & 0xf;
      dst->map[dst->nr_map].mem_type = (physical_end <= 0x10000000) ?
                                        GRUB_EFI_LOONGSON_SYSTEM_RAM_LOW :
                                        GRUB_EFI_LOONGSON_SYSTEM_RAM_HIGH;
      dst->map[dst->nr_map].mem_start = physical_start;
      dst->map[dst->nr_map].mem_size = (physical_end - physical_start) >> 20;

      grub_dprintf ("loongson", "memory map %03u: 0x%016lx 0x%016lx @ %u\n",
                    dst->nr_map, physical_start, physical_end - physical_start,
                    dst->map[dst->nr_map].node_id);

      dst->nr_map ++;
    }
}

#define BYTES_TO_PAGES(bytes)	(((bytes) + 0xfff) >> 12)
#define SUB_MEMORY_DESCRIPTOR(desc, size)	\
  ((grub_efi_memory_descriptor_t *) ((char *) (desc) - (size)))

void
grub_efi_loongson_alloc_boot_params (void)
{
  grub_efi_memory_descriptor_t *mmap_buf;
  grub_efi_memory_descriptor_t *mmap_end;
  grub_efi_memory_descriptor_t *desc;
  grub_efi_uintn_t mmap_size;
  grub_efi_uintn_t desc_size;
  grub_efi_physical_address_t address;
  grub_efi_allocate_type_t type;
  grub_efi_uintn_t pages;
  grub_efi_status_t status;
  grub_efi_boot_services_t *b;
  int mm_status;

  type = GRUB_EFI_ALLOCATE_ADDRESS;
  pages = BYTES_TO_PAGES (loongson_boot_params_size + loongson_reset_code_size);

  mmap_size = (1 << 12);
  mmap_buf = grub_malloc (mmap_size);
  if (!mmap_buf)
    grub_fatal ("out of memory!");

  mm_status = grub_efi_get_memory_map (&mmap_size, mmap_buf, 0, &desc_size, 0);
  if (mm_status == 0)
    {
      grub_free (mmap_buf);
      mmap_size += desc_size * 32;

      mmap_buf = grub_malloc (mmap_size);
      if (!mmap_buf)
        grub_fatal ("out of memory!");

      mm_status = grub_efi_get_memory_map (&mmap_size, mmap_buf, 0, &desc_size, 0);
    }

  if (mm_status < 0)
    grub_fatal ("cannot get memory map!");

  mmap_end = ADD_MEMORY_DESCRIPTOR (mmap_buf, mmap_size);

  for (desc = SUB_MEMORY_DESCRIPTOR (mmap_end, desc_size);
       desc >= mmap_buf;
       desc = SUB_MEMORY_DESCRIPTOR (desc, desc_size))
    {
      if (desc->type != GRUB_EFI_CONVENTIONAL_MEMORY)
        continue;
      if (desc->physical_start >= GRUB_EFI_MAX_USABLE_ADDRESS)
        continue;
      if (desc->num_pages < pages)
        continue;

      address = desc->physical_start;
      break;
    }

  grub_free (mmap_buf);

  b = grub_efi_system_table->boot_services;
  status = efi_call_4 (b->allocate_pages, type, GRUB_EFI_RUNTIME_SERVICES_DATA, pages, &address);
  if (status != GRUB_EFI_SUCCESS)
    grub_fatal ("cannot allocate Loongson boot parameters!");

  loongson_boot_params = (void *) ((grub_addr_t) address);
}

void
grub_efi_loongson_free_boot_params (void)
{
  grub_efi_free_pages ((grub_addr_t) loongson_boot_params,
                       BYTES_TO_PAGES (loongson_boot_params_size + loongson_reset_code_size));
}

void *
grub_efi_loongson_get_smbios_table (void)
{
  static grub_efi_loongson_smbios_table *smbios_table; 
  grub_efi_loongson_boot_params *old_boot_params;
  struct bootparamsinterface* boot_params;
  void * tmp_boot_params = NULL;	
  char * p = NULL;
  if(smbios_table)
    return smbios_table;

  tmp_boot_params = grub_efi_loongson_get_boot_params();
  if(tmp_boot_params == NULL)
  {
    grub_dprintf("loongson", "tmp_boot_params is NULL\n");
    return tmp_boot_params;
  }
 
  boot_params = (struct bootparamsinterface *)tmp_boot_params;
  p = (char *)&(boot_params->signature);
  if(grub_strncmp(p, "BPI", 3) == 0)
  {
    grub_dprintf("loongson", "find new bpi\n");
    return boot_params ? boot_params : 0;
  }
  else
  {
    old_boot_params = (grub_efi_loongson_boot_params *)tmp_boot_params;
    return old_boot_params ? &old_boot_params->efi.smbios : 0;
  }

}

int
grub_efi_is_loongson (void)
{
  return grub_efi_loongson_get_smbios_table () ? 1 : 0;
}

void *
grub_efi_loongson_get_boot_params (void)
{
  static void * boot_params = NULL;
  grub_efi_configuration_table_t *tables;
  grub_efi_guid_t smbios_guid = GRUB_EFI_LOONGSON_SMBIOS_TABLE_GUID;
  unsigned int i;

  if (boot_params)
    return boot_params;

  /* Look for Loongson SMBIOS in UEFI config tables. */
  tables = grub_efi_system_table->configuration_table;

  for (i = 0; i < grub_efi_system_table->num_table_entries; i++)
    if (grub_memcmp (&tables[i].vendor_guid, &smbios_guid, sizeof (smbios_guid)) == 0)
      {
        boot_params= tables[i].vendor_table;
        grub_dprintf ("loongson", "found registered SMBIOS @ %p\n", boot_params);
        break;
      }
  return boot_params;
}

grub_uint8_t
grub_efi_loongson_calculatesum8 (const grub_uint8_t *buffer, grub_efi_uintn_t length)
{
  grub_uint8_t sum;
  grub_efi_uintn_t count;

  for (sum = 0, count = 0; count < length; count++)
  {
    sum = (grub_uint8_t) (sum + *(buffer + count));
  }
  return sum;
}

grub_uint8_t
grub_efi_loongson_grub_calculatechecksum8 (const grub_uint8_t *buffer, grub_efi_uintn_t length)
{
  grub_uint8_t checksum;

  checksum = grub_efi_loongson_calculatesum8(buffer, length);

  return (grub_uint8_t) (0x100 - checksum);
}


grub_uint32_t 
grub_efi_loongson_memmap_sort(struct memmap array[], grub_uint32_t length, mem_map * bpmem, grub_uint32_t index, grub_uint32_t memtype)
{
  grub_uint64_t tempmemsize = 0;
  grub_uint32_t j = 0;
  grub_uint32_t t = 0;

  for(j = 0; j < length;)
  {
    tempmemsize = array[j].memsize;
    for(t = j + 1; t < length; t++)
    {
      if(array[j].memstart + tempmemsize == array[t].memstart) 
      {
        tempmemsize += array[t].memsize;
      }
      else
      {            
        break;
      }
   }
   bpmem->map[index].memtype = memtype;
   bpmem->map[index].memstart = array[j].memstart;
   bpmem->map[index].memsize = tempmemsize;
   grub_dprintf("loongson", "map[%d]:type %x, start 0x%llx, end 0x%llx\n",
		   index,
		   bpmem->map[index].memtype,
		   (unsigned long long)bpmem->map[index].memstart,
		   (unsigned long long)bpmem->map[index].memstart+ bpmem->map[index].memsize
	       );
   j = t;
   index++;
  }
  return index;
}

