/* linux.c - boot Linux */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2004,2005,2007,2009,2010,2017  Free Software Foundation, Inc.
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

#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/elf.h>
#include <grub/elfload.h>
#include <grub/loader.h>
#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/command.h>
#include <grub/cpu/relocator.h>
#include <grub/machine/loongson.h>
#include <grub/memory.h>
#include <grub/i18n.h>
#include <grub/lib/cmdline.h>
#include <grub/linux.h>
#include <grub/term.h>
#include <grub/env.h>


GRUB_MOD_LICENSE ("GPLv3+");

#define _ull unsigned long long
#pragma GCC diagnostic ignored "-Wcast-align"

typedef  unsigned long size_t;

static grub_dl_t my_mod;

static int loaded;

static grub_uint32_t tmp_index = 0;
static grub_size_t linux_size;

static struct grub_relocator *relocator;
static grub_addr_t target_addr, entry_addr;
static int linux_argc;
static grub_uint8_t *linux_args_addr;
static grub_off_t rd_addr_arg_off, rd_size_arg_off;
static int initrd_loaded = 0;


static grub_uint32_t j = 0;
static grub_uint32_t t = 0;
grub_uint64_t tempMemsize = 0;
grub_uint32_t free_index = 0;
grub_uint32_t reserve_index = 0;
grub_uint32_t acpi_table_index = 0;
grub_uint32_t acpi_nvs_index = 0;  

#define LINUX_MAX_ARGC  1024
static int ventoy_debug = 0;
static int ventoy_initrd_called = 0;
static int ventoy_linux_argc = 0;
static char **ventoy_linux_args = NULL;
static int ventoy_extra_initrd_num = 0;
static char *ventoy_extra_initrd_list[256];
static grub_err_t
grub_cmd_initrd (grub_command_t cmd __attribute__ ((unused)),
		 int argc, char *argv[]);



static inline grub_size_t
page_align (grub_size_t size)
{
  return (size + (1 << 12) - 1) & (~((1 << 12) - 1));
}

/* Find the optimal number of pages for the memory map. Is it better to
   move this code to efi/mm.c?  */
static grub_efi_uintn_t
find_mmap_size (void)
{
  static grub_efi_uintn_t mmap_size = 0;

  if (mmap_size != 0)
    return mmap_size;

  mmap_size = (1 << 12);
  while (1)
    {
      int ret;
      grub_efi_memory_descriptor_t *mmap;
      grub_efi_uintn_t desc_size;

      mmap = grub_malloc (mmap_size);
      if (! mmap)
	return 0;

      ret = grub_efi_get_memory_map (&mmap_size, mmap, 0, &desc_size, 0);
      grub_free (mmap);

      if (ret < 0)
	{
	  grub_error (GRUB_ERR_IO, "cannot get memory map");
	  return 0;
	}
      else if (ret > 0)
	break;

      mmap_size += (1 << 12);
    }


  /* Increase the size a bit for safety, because GRUB allocates more on
     later, and EFI itself may allocate more.  */
  mmap_size += (1 << 12);

  return page_align (mmap_size);
}


static void ventoy_debug_pause(void)
{
    char key;

    if (0 == ventoy_debug) 
    {
        return;
    }
    
    grub_printf("press Enter to continue ......\n");
    while (1)
    {
        key = grub_getkey();
        if (key == '\n' || key == '\r')
        {
            break;
        }
    }    
}

static int ventoy_preboot(void)
{
    int i;
    const char *file;
    char buf[128];

    if (ventoy_debug) 
    {
        grub_printf("ventoy_preboot %d %d\n", ventoy_linux_argc, ventoy_initrd_called);
        ventoy_debug_pause();
    }

    if (ventoy_linux_argc == 0)
    {
        return 0;
    }

    if (ventoy_initrd_called)
    {
        ventoy_initrd_called = 0;
        return 0;
    }

    grub_snprintf(buf, sizeof(buf), "mem:%s:size:%s", grub_env_get("ventoy_cpio_addr"), grub_env_get("ventoy_cpio_size"));

    ventoy_extra_initrd_list[ventoy_extra_initrd_num++] = grub_strdup(buf);

    file = grub_env_get("vtoy_img_part_file");
    if (file)
    {
        ventoy_extra_initrd_list[ventoy_extra_initrd_num++] = grub_strdup(file);
    }

    if (ventoy_debug) 
    {
        grub_printf("========== initrd list ==========\n");
        for (i = 0; i < ventoy_extra_initrd_num; i++)
        {
            grub_printf("%s\n", ventoy_extra_initrd_list[i]);
        }
        grub_printf("=================================\n");
        
        ventoy_debug_pause();
    }

    grub_cmd_initrd(NULL, ventoy_extra_initrd_num, ventoy_extra_initrd_list);

    return 0;
}

static int ventoy_boot_opt_filter(char *opt)
{
    if (grub_strcmp(opt, "noinitrd") == 0)
    {
        return 1;
    }

    if (grub_strcmp(opt, "vga=current") == 0)
    {
        return 1;
    }

    if (grub_strncmp(opt, "rdinit=", 7) == 0)
    {
        if (grub_strcmp(opt, "rdinit=/vtoy/vtoy") != 0)
        {
            opt[0] = 'v';
            opt[1] = 't';
        }
        return 0;
    }
    
    if (grub_strncmp(opt, "init=", 5) == 0)
    {
        opt[0] = 'v';
        opt[1] = 't';
        return 0;
    }

    if (grub_strncmp(opt, "dm=", 3) == 0)
    {
        opt[0] = 'D';
        opt[1] = 'M';
        return 0;
    }

    if (ventoy_debug)
    {
        if (grub_strcmp(opt, "quiet") == 0)
        {
            return 1;
        }
        
        if (grub_strncmp(opt, "loglevel=", 9) == 0)
        {
            return 1;
        }
        
        if (grub_strcmp(opt, "splash") == 0)
        {
            return 1;
        }
    }

    return 0;
}

static int ventoy_bootopt_hook(int argc, char *argv[])
{
    int i;
    int TmpIdx;
    int count = 0;
    const char *env;
    char c;
    char *newenv;
    char *last, *pos;

    //grub_printf("ventoy_bootopt_hook: %d %d\n", argc, ventoy_linux_argc);

    if (ventoy_linux_argc == 0)
    {
        return 0;
    }

    /* To avoid --- parameter, we split two parts */
    for (TmpIdx = 0; TmpIdx < argc; TmpIdx++)
    {
        if (ventoy_boot_opt_filter(argv[TmpIdx]))
        {
            continue;
        }

        if (grub_strncmp(argv[TmpIdx], "--", 2) == 0)
        {
            break;
        }

        ventoy_linux_args[count++] = grub_strdup(argv[TmpIdx]);
    }

    for (i = 0; i < ventoy_linux_argc; i++)
    {
        ventoy_linux_args[count] = ventoy_linux_args[i + (LINUX_MAX_ARGC / 2)];
        ventoy_linux_args[i + (LINUX_MAX_ARGC / 2)] = NULL;
        
        if (ventoy_linux_args[count][0] == '@')
        {
            env = grub_env_get(ventoy_linux_args[count] + 1);
            if (env)
            {
                grub_free(ventoy_linux_args[count]);

                newenv = grub_strdup(env);
                last =  newenv;

                while (*last)
                {
                    while (*last)
                    {
                        if (*last != ' ' && *last != '\t')
                        {
                            break;
                        }
                        last++;
                    }

                    if (*last == 0)
                    {
                        break;
                    }

                    for (pos = last; *pos; pos++)
                    {
                        if (*pos == ' ' || *pos == '\t')
                        {
                            c = *pos;
                            *pos = 0;
                            if (0 == ventoy_boot_opt_filter(last))
                            {
                                ventoy_linux_args[count++] = grub_strdup(last);
                            }
                            *pos = c;
                            break;
                        }
                    }

                    if (*pos == 0)
                    {
                        if (0 == ventoy_boot_opt_filter(last))
                        {
                            ventoy_linux_args[count++] = grub_strdup(last);                            
                        }
                        break;
                    }

                    last = pos + 1;
                }
            }
            else
            {
                count++;
            }
        }
        else
        {
            count++;            
        }
    }

    while (TmpIdx < argc)
    {
        if (ventoy_boot_opt_filter(argv[TmpIdx]))
        {
            continue;
        }

        ventoy_linux_args[count++] = grub_strdup(argv[TmpIdx]);
        TmpIdx++;
    }

    if (ventoy_debug)
    {
        ventoy_linux_args[count++] = grub_strdup("loglevel=7");
    }

    ventoy_linux_argc = count;

    if (ventoy_debug)
    {
        grub_printf("========== bootoption ==========\n");
        for (i = 0; i < count; i++)
        {
            grub_printf("%s ", ventoy_linux_args[i]);
        }  
        grub_printf("\n================================\n");
    }
    
    return 0;
}

static grub_err_t
grub_cmd_set_boot_opt (grub_command_t cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
    int i;
    const char *vtdebug;

    for (i = 0; i < argc; i++)
    {
        ventoy_linux_args[ventoy_linux_argc + (LINUX_MAX_ARGC / 2) ] = grub_strdup(argv[i]);
        ventoy_linux_argc++;
    }

    vtdebug = grub_env_get("vtdebug_flag");
    if (vtdebug && vtdebug[0])
    {
        ventoy_debug = 1;
    }

    if (ventoy_debug) grub_printf("ventoy set boot opt %d\n", ventoy_linux_argc);

    return 0;
}

static grub_err_t
grub_cmd_unset_boot_opt (grub_command_t cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
    int i;
    
    (void)argc;
    (void)argv;

    for (i = 0; i < LINUX_MAX_ARGC; i++)
    {
        if (ventoy_linux_args[i])
        {
            grub_free(ventoy_linux_args[i]);
        }
    }

    ventoy_debug = 0;
    ventoy_linux_argc = 0;
    ventoy_initrd_called = 0;
    grub_memset(ventoy_linux_args, 0, sizeof(char *) * LINUX_MAX_ARGC);
    return 0;
}

static grub_err_t
grub_cmd_extra_initrd_append (grub_command_t cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
    int newclen = 0;
    char *pos = NULL;
    char *end = NULL;
    char buf[256] = {0};
    
    if (argc != 1)
    {
        return 1;
    }

    for (pos = argv[0]; *pos; pos++)
    {
        if (*pos == '/')
        {
            end = pos;
        }
    }

    if (end)
    {
        /* grub2 newc bug workaround */
        newclen = (int)grub_strlen(end + 1);
        if ((110 + newclen) % 4 == 0)
        {
            grub_snprintf(buf, sizeof(buf), "newc:.%s:%s", end + 1, argv[0]);
        }
        else
        {
            grub_snprintf(buf, sizeof(buf), "newc:%s:%s", end + 1, argv[0]);
        }
    
        if (ventoy_extra_initrd_num < 256)
        {
            ventoy_extra_initrd_list[ventoy_extra_initrd_num++] = grub_strdup(buf);        
        }
    }

    return 0;
}

static grub_err_t
grub_cmd_extra_initrd_reset (grub_command_t cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
    int i;
    
    (void)argc;
    (void)argv;

    for (i = 0; i < ventoy_extra_initrd_num; i++)
    {
        if (ventoy_extra_initrd_list[i])
        {
            grub_free(ventoy_extra_initrd_list[i]);
        }
    }

    grub_memset(ventoy_extra_initrd_list, 0, sizeof(ventoy_extra_initrd_list));

    return 0;
}


static grub_err_t
grub_linux_boot (void)
{
  struct grub_relocator64_state state;
  grub_int8_t checksum = 0;
  grub_efi_memory_descriptor_t * lsdesc = NULL;

  ventoy_preboot();

  grub_memset (&state, 0, sizeof (state));

  /* Boot the kernel.  */
  state.gpr[1] = entry_addr;
  grub_dprintf("loongson", "entry_addr is 0x%llx\n", (_ull)state.gpr[1]);
  state.gpr[4] = linux_argc;
  grub_dprintf("loongson", "linux_argc is %lld\n", (_ull)state.gpr[4]);
  state.gpr[5] = (grub_addr_t) linux_args_addr;
  grub_dprintf("loongson", "args_addr is 0x%llx\n", (_ull)state.gpr[5]);

  if(grub_efi_is_loongson ())
  {
    grub_efi_uintn_t mmap_size;
    grub_efi_uintn_t desc_size;
    grub_efi_memory_descriptor_t *mmap_buf;
    grub_err_t err;
    struct bootparamsinterface * boot_params;
    void * tmp_boot_params = NULL;
    grub_efi_uint8_t new_interface_flag = 0;
    mem_map * new_interface_mem = NULL;
    char *p = NULL;

    struct memmap reserve_mem[GRUB_EFI_LOONGSON_MMAP_MAX];
    struct memmap free_mem[GRUB_EFI_LOONGSON_MMAP_MAX];
    struct memmap acpi_table_mem[GRUB_EFI_LOONGSON_MMAP_MAX];
    struct memmap acpi_nvs_mem[GRUB_EFI_LOONGSON_MMAP_MAX];
  
    grub_memset(reserve_mem, 0, sizeof(struct memmap) * GRUB_EFI_LOONGSON_MMAP_MAX);
    grub_memset(free_mem, 0, sizeof(struct memmap) * GRUB_EFI_LOONGSON_MMAP_MAX);
    grub_memset(acpi_table_mem, 0, sizeof(struct memmap) * GRUB_EFI_LOONGSON_MMAP_MAX);
    grub_memset(acpi_nvs_mem, 0, sizeof(struct memmap) * GRUB_EFI_LOONGSON_MMAP_MAX);

    tmp_boot_params = grub_efi_loongson_get_boot_params();
    if(tmp_boot_params == NULL)
    {
      grub_printf("not find param\n");
      return -1;
    }

    boot_params = (struct bootparamsinterface *)tmp_boot_params;
    p = (char *)&(boot_params->signature);
    if(grub_strncmp(p, "BPI", 3) == 0)
    {
      /* Check extlist headers */
      ext_list * listpointer = NULL;
      listpointer = boot_params->extlist;
      for( ;listpointer != NULL; listpointer = listpointer->next)
      {
        char *pl= (char *)&(listpointer->signature);
	if(grub_strncmp(pl, "MEM", 3) == 0)
	{
           new_interface_mem = (mem_map *)listpointer;
	}
      }

      new_interface_flag = 1;
      grub_dprintf("loongson", "get new parameter interface\n");
    }else{
      new_interface_flag = 0;
      grub_dprintf("loongson", "get old parameter interface\n");
      
    }
    state.gpr[6] = (grub_uint64_t)tmp_boot_params;
    grub_dprintf("loongson", "boot_params is 0x%llx\n", (_ull)state.gpr[6]);

    mmap_size = find_mmap_size ();
    if (! mmap_size)
      return grub_errno;
    mmap_buf = grub_efi_allocate_any_pages (page_align (mmap_size) >> 12);
    if (! mmap_buf)
      return grub_error (GRUB_ERR_IO, "cannot allocate memory map");
    err = grub_efi_finish_boot_services (&mmap_size, mmap_buf, NULL,
                                         &desc_size, NULL);
    //grub_printf("%s-%d\n", __func__, __LINE__);
    if (err)
     return err;

    if(new_interface_flag)
    {
      if (!mmap_buf || !mmap_size || !desc_size)
        return -1;

      tmp_index = new_interface_mem -> mapcount;
      //grub_printf("%s-%d mapcount %d\n", __func__, __LINE__, tmp_index);

      /*
       According to UEFI SPEC,mmap_buf is the accurate Memory Map array \
       now we can fill platform specific memory structure. 
       */
      for(lsdesc = mmap_buf; lsdesc < (grub_efi_memory_descriptor_t *)((char *)mmap_buf + mmap_size); lsdesc = (grub_efi_memory_descriptor_t *)((char *)lsdesc + desc_size))
      {
        /* Recovery */
        if((lsdesc->type != GRUB_EFI_ACPI_RECLAIM_MEMORY) && \
			(lsdesc->type != GRUB_EFI_ACPI_MEMORY_NVS) && \
			(lsdesc->type != GRUB_EFI_RUNTIME_SERVICES_DATA) && \
			(lsdesc->type != GRUB_EFI_RUNTIME_SERVICES_CODE) && \
			(lsdesc->type != GRUB_EFI_RESERVED_MEMORY_TYPE) && \
			(lsdesc->type != GRUB_EFI_PAL_CODE)) 
	{

	  free_mem[free_index].memtype = GRUB_EFI_LOONGSON_SYSTEM_RAM_LOW;
	  free_mem[free_index].memstart = (lsdesc->physical_start) & 0xffffffffffff;
	  free_mem[free_index].memsize = lsdesc->num_pages * 4096;
	  free_index++;
	  /*ACPI*/
	}else if((lsdesc->type == GRUB_EFI_ACPI_RECLAIM_MEMORY)){
	  acpi_table_mem[acpi_table_index].memtype = GRUB_EFI_LOONGSON_ACPI_TABLE;
	  acpi_table_mem[acpi_table_index].memstart = (lsdesc->physical_start) & 0xffffffffffff;
	  acpi_table_mem[acpi_table_index].memsize = lsdesc->num_pages * 4096;
	  acpi_table_index++;

	}else if((lsdesc->type == GRUB_EFI_ACPI_MEMORY_NVS)){
	  acpi_nvs_mem[acpi_nvs_index].memtype = GRUB_EFI_LOONGSON_ACPI_NVS;
	  acpi_nvs_mem[acpi_nvs_index].memstart = (lsdesc->physical_start) & 0xffffffffffff;
	  acpi_nvs_mem[acpi_nvs_index].memsize = lsdesc->num_pages * 4096;
	  acpi_nvs_index++;

	  /* Reserve */
	}else{ 
	  reserve_mem[reserve_index].memtype = GRUB_EFI_LOONGSON_MEMORY_RESERVED;
	  reserve_mem[reserve_index].memstart = (lsdesc->physical_start) & 0xffffffffffff;
	  reserve_mem[reserve_index].memsize = lsdesc->num_pages * 4096;
	  reserve_index++;
	}
      }

      /* Recovery sort */
      for(j = 0; j < free_index;)
      {
        tempMemsize = free_mem[j].memsize;
        for(t = j + 1; t < free_index; t++)
        {
          if((free_mem[j].memstart + tempMemsize == free_mem[t].memstart) && (free_mem[j].memtype == free_mem[t].memtype)) 
          {
		  tempMemsize += free_mem[t].memsize;
	  }else{            
		  break;
	  }      
	}
	if(free_mem[j].memstart >= 0x10000000) /*HIGH MEM*/
	  new_interface_mem->map[tmp_index].memtype = GRUB_EFI_LOONGSON_SYSTEM_RAM_HIGH;
	else
	  new_interface_mem->map[tmp_index].memtype = GRUB_EFI_LOONGSON_SYSTEM_RAM_LOW;
	new_interface_mem->map[tmp_index].memstart = free_mem[j].memstart;
	new_interface_mem->map[tmp_index].memsize = tempMemsize;
	grub_dprintf("loongson", "map[%d]:type %x, start 0x%llx, end 0x%llx\n",
			tmp_index,
			new_interface_mem->map[tmp_index].memtype,
			(_ull)new_interface_mem->map[tmp_index].memstart,
			(_ull)new_interface_mem->map[tmp_index].memstart+ new_interface_mem->map[tmp_index].memsize
		    );
	j = t;
	tmp_index++;
      }

      /*ACPI Sort*/
      tmp_index = grub_efi_loongson_memmap_sort(acpi_table_mem, acpi_table_index, new_interface_mem, tmp_index, GRUB_EFI_LOONGSON_ACPI_TABLE);
      tmp_index = grub_efi_loongson_memmap_sort(acpi_nvs_mem, acpi_nvs_index, new_interface_mem, tmp_index, GRUB_EFI_LOONGSON_ACPI_NVS);
      /*Reserve Sort*/
      tmp_index = grub_efi_loongson_memmap_sort(reserve_mem, reserve_index, new_interface_mem, tmp_index, GRUB_EFI_LOONGSON_MEMORY_RESERVED);

      new_interface_mem->mapcount = tmp_index;
      new_interface_mem->header.checksum = 0;
      //grub_printf("%s-%d mapcount %d\n", __func__, __LINE__, tmp_index);

      checksum = grub_efi_loongson_grub_calculatechecksum8((grub_uint8_t *)new_interface_mem, new_interface_mem->header.length);
      new_interface_mem->header.checksum = checksum;
    }
  }

  state.jumpreg = 1;
  grub_relocator64_boot (relocator, state);

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_linux_unload (void)
{
  grub_relocator_unload (relocator);
  grub_dl_unref (my_mod);

  loaded = 0;

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_linux_load32 (grub_elf_t elf, const char *filename)
{
  Elf32_Addr base;
  grub_err_t err;
  grub_uint8_t *playground;

  /* Linux's entry point incorrectly contains a virtual address.  */
  entry_addr = elf->ehdr.ehdr32.e_entry;

  linux_size = grub_elf32_size (elf, &base, 0);
  if (linux_size == 0)
    return grub_errno;
  target_addr = base;
  linux_size = ALIGN_UP (base + linux_size - base, 8);

  relocator = grub_relocator_new ();
  if (!relocator)
    return grub_errno;

  {
    grub_relocator_chunk_t ch;
    err = grub_relocator_alloc_chunk_addr (relocator, &ch,
					   grub_vtop ((void *) target_addr),
					   linux_size);
    if (err)
      return err;
    playground = get_virtual_current_address (ch);
  }

  /* Now load the segments into the area we claimed.  */
  return grub_elf32_load (elf, filename, playground - base, GRUB_ELF_LOAD_FLAGS_NONE, 0, 0);
}

static grub_err_t
grub_linux_load64 (grub_elf_t elf, const char *filename)
{
  Elf64_Addr base;
  grub_err_t err;
  grub_uint8_t *playground;

  /* Linux's entry point incorrectly contains a virtual address.  */
  entry_addr = elf->ehdr.ehdr64.e_entry;
  grub_dprintf("loongson", "entry address = 0x%llx\n", (_ull)entry_addr);

  linux_size = grub_elf64_size (elf, &base, 0);
  grub_dprintf("loongson", "base = 0x%llx\n", (_ull)base);

  if (linux_size == 0)
    return grub_errno;
  target_addr = base;
  linux_size = ALIGN_UP (base + linux_size - base, 8);

  relocator = grub_relocator_new ();
  if (!relocator)
    return grub_errno;

  {
    grub_relocator_chunk_t ch;
    err = grub_relocator_alloc_chunk_addr (relocator, &ch,
					   grub_vtop ((void *) target_addr),
					   linux_size);
    if (err)
      return err;
    playground = get_virtual_current_address (ch);
    //playground = 0xffffffff81ee0000; //将内核直接load到elf头指定内存，而非grub分配的空间
    //playground = 0xffffffff80200000;
  }

  grub_printf("playground:0x%llx\n", (_ull)playground);

  /* Now load the segments into the area we claimed.  */
  return grub_elf64_load (elf, filename, playground - base, GRUB_ELF_LOAD_FLAGS_NONE, 0, 0);
}

static grub_err_t
grub_cmd_linux (grub_command_t cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
  grub_elf_t elf = 0;
  int size;
  int i;
  grub_uint32_t *linux_argv;
  char *linux_args;
  grub_err_t err;

  if (argc == 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  if (ventoy_linux_argc)
  {
      ventoy_bootopt_hook(argc, argv);
      argc = ventoy_linux_argc;
      argv = ventoy_linux_args;
  }

  elf = grub_elf_open (argv[0], GRUB_FILE_TYPE_LINUX_KERNEL);
  if (! elf)
    return grub_errno;

  if (elf->ehdr.ehdr32.e_type != ET_EXEC)
    {
      grub_elf_close (elf);
      return grub_error (GRUB_ERR_UNKNOWN_OS,
			 N_("this ELF file is not of the right type"));
    }

  /* Release the previously used memory.  */
  grub_loader_unset ();
  loaded = 0;

  /* For arguments.  */
  linux_argc = argc;
  /* Main arguments.  */
  size = (linux_argc) * sizeof (grub_uint32_t);
  /* Initrd address and size.  */
  size += 2 * sizeof (grub_uint32_t);
  /* NULL terminator.  */
  size += sizeof (grub_uint32_t);
  /* First argument is always "a0".  */
  size += ALIGN_UP (sizeof ("a0"), 4);
  /* Normal arguments.  */
  for (i = 1; i < argc; i++)
    size += ALIGN_UP (grub_strlen (argv[i]) + 1, 4);

  /* rd arguments.  */
  size += ALIGN_UP (sizeof ("rd_start=0xXXXXXXXXXXXXXXXX"), 4);
  size += ALIGN_UP (sizeof ("rd_size=0xXXXXXXXXXXXXXXXX"), 4);

  size = ALIGN_UP (size, 8);

  if (grub_elf_is_elf32 (elf))
    err = grub_linux_load32 (elf, argv[0]);
  else
  if (grub_elf_is_elf64 (elf))
    err = grub_linux_load64 (elf, argv[0]);
  else
    err = grub_error (GRUB_ERR_BAD_OS, N_("invalid arch-dependent ELF magic"));

  grub_elf_close (elf);

  if (err)
    return err;

  {
    grub_relocator_chunk_t ch;
    err = grub_relocator_alloc_chunk_align (relocator, &ch,
					    0, (0xffffffff - size) + 1,
					    size, 8,
					    GRUB_RELOCATOR_PREFERENCE_HIGH, 0);
    if (err)
      return err;
    linux_args_addr = get_virtual_current_address (ch);
  }

  linux_argv = (grub_uint32_t *) linux_args_addr;
  linux_args = (char *) (linux_argv + (linux_argc + 1 + 2));

  grub_memcpy (linux_args, "a0", sizeof ("a0"));
  *linux_argv = (grub_uint32_t) (grub_addr_t) linux_args;
  linux_argv++;
  linux_args += ALIGN_UP (sizeof ("a0"), 4);

  for (i = 1; i < argc; i++)
    {
      grub_memcpy (linux_args, argv[i], grub_strlen (argv[i]) + 1);
      *linux_argv = (grub_uint32_t) (grub_addr_t) linux_args;
      linux_argv++;
      linux_args += ALIGN_UP (grub_strlen (argv[i]) + 1, 4);
    }

  /* Reserve space for rd arguments.  */
  rd_addr_arg_off = (grub_uint8_t *) linux_args - linux_args_addr;
  linux_args += ALIGN_UP (sizeof ("rd_start=0xXXXXXXXXXXXXXXXX"), 4);
  *linux_argv = 0;
  linux_argv++;

  rd_size_arg_off = (grub_uint8_t *) linux_args - linux_args_addr;
  linux_args += ALIGN_UP (sizeof ("rd_size=0xXXXXXXXXXXXXXXXX"), 4);
  *linux_argv = 0;
  linux_argv++;

  *linux_argv = 0;

  //wake up other cores
  {
	  __asm__(
			  "dli  $8, 0x900000003ff01000\n\t"
			  "dli  $11, 0      \n\t"
			  "dsll $11, 8      \n\t"
			  "or   $8, $8,$11  \n\t"
			  "li   $9, 0x5a5a \n\t"
			  "sw   $9, 32($8) \n\t"
			  "nop             \n\t"
			  :
			  :
			 );
  }
  grub_loader_set (grub_linux_boot, grub_linux_unload, 0);
  initrd_loaded = 0;
  loaded = 1;
  grub_dl_ref (my_mod);

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_initrd (grub_command_t cmd __attribute__ ((unused)),
		 int argc, char *argv[])
{
  grub_size_t size = 0;
  void *initrd_dest;
  grub_err_t err;
  struct grub_linux_initrd_context initrd_ctx = { 0, 0, 0 };

  if (argc == 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  if (!loaded)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("you need to load the kernel first"));

  if (initrd_loaded)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "only one initrd command can be issued.");

  if (grub_initrd_init (argc, argv, &initrd_ctx))
    goto fail;

  size = grub_get_initrd_size (&initrd_ctx);

  {
    grub_relocator_chunk_t ch;
    err = grub_relocator_alloc_chunk_align (relocator, &ch,
					    0, (0xffffffff - size) + 1,
					    size, 0x10000,
					    GRUB_RELOCATOR_PREFERENCE_HIGH, 0);

    if (err)
      goto fail;
    initrd_dest = get_virtual_current_address (ch);
  }

  if (grub_initrd_load (&initrd_ctx, argv, initrd_dest))
    goto fail;

  grub_snprintf ((char *) linux_args_addr + rd_addr_arg_off,
		 sizeof ("rd_start=0xXXXXXXXXXXXXXXXX"), "rd_start=0x%lx",
		(grub_uint64_t) initrd_dest);
  ((grub_uint32_t *) linux_args_addr)[linux_argc]
    = (grub_uint32_t) ((grub_addr_t) linux_args_addr + rd_addr_arg_off);
  linux_argc++;

  grub_snprintf ((char *) linux_args_addr + rd_size_arg_off,
		sizeof ("rd_size=0xXXXXXXXXXXXXXXXXX"), "rd_size=0x%lx",
		(grub_uint64_t) size);
  ((grub_uint32_t *) linux_args_addr)[linux_argc]
    = (grub_uint32_t) ((grub_addr_t) linux_args_addr + rd_size_arg_off);
  linux_argc++;

  initrd_loaded = 1;

 fail:
  grub_initrd_close (&initrd_ctx);

  return grub_errno;
}

static grub_err_t
ventoy_cmd_initrd (grub_command_t cmd __attribute__ ((unused)),
		 int argc, char *argv[])
{
    int i;
    const char *file;
    char buf[64];

    if (ventoy_debug) grub_printf("ventoy_cmd_initrd %d\n", ventoy_linux_argc);

    if (ventoy_linux_argc == 0)
    {
        return grub_cmd_initrd(cmd, argc, argv);        
    }

    grub_snprintf(buf, sizeof(buf), "mem:%s:size:%s", grub_env_get("ventoy_cpio_addr"), grub_env_get("ventoy_cpio_size"));

    if (ventoy_debug) grub_printf("membuf=%s\n", buf);

    ventoy_extra_initrd_list[ventoy_extra_initrd_num++] = grub_strdup(buf);

    file = grub_env_get("vtoy_img_part_file");
    if (file)
    {
        ventoy_extra_initrd_list[ventoy_extra_initrd_num++] = grub_strdup(file);
    }

    for (i = 0; i < argc; i++)
    {
        ventoy_extra_initrd_list[ventoy_extra_initrd_num++] = grub_strdup(argv[i]);
    }

    ventoy_initrd_called = 1;

    if (ventoy_debug)
    {
        grub_printf("========== initrd list ==========\n");
        for (i = 0; i < ventoy_extra_initrd_num; i++)
        {
            grub_printf("%s\n", ventoy_extra_initrd_list[i]);
        }
        grub_printf("=================================\n");
    }
    
    return grub_cmd_initrd(cmd, ventoy_extra_initrd_num, ventoy_extra_initrd_list);
}
         
static grub_command_t cmd_linux, cmd_initrd;
static grub_command_t cmd_set_bootopt, cmd_unset_bootopt, cmd_extra_initrd_append, cmd_extra_initrd_reset;

GRUB_MOD_INIT(linux)
{
  cmd_linux = grub_register_command ("linux", grub_cmd_linux,
				     0, N_("Load Linux."));
  cmd_initrd = grub_register_command ("initrd", ventoy_cmd_initrd,
				      0, N_("Load initrd."));

  cmd_set_bootopt = grub_register_command ("vt_set_boot_opt", grub_cmd_set_boot_opt, 0, N_("set ext boot opt"));
  cmd_unset_bootopt = grub_register_command ("vt_unset_boot_opt", grub_cmd_unset_boot_opt, 0, N_("unset ext boot opt"));
  
  cmd_extra_initrd_append = grub_register_command ("vt_img_extra_initrd_append", grub_cmd_extra_initrd_append, 0, N_(""));
  cmd_extra_initrd_reset = grub_register_command ("vt_img_extra_initrd_reset", grub_cmd_extra_initrd_reset, 0, N_(""));

  ventoy_linux_args = grub_zalloc(sizeof(char *) * LINUX_MAX_ARGC);

  my_mod = mod;
}

GRUB_MOD_FINI(linux)
{
  grub_unregister_command (cmd_linux);
  grub_unregister_command (cmd_initrd);
}
