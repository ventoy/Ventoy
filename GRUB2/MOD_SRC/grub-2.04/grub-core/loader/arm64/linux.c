/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2013  Free Software Foundation, Inc.
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

#include <grub/charset.h>
#include <grub/command.h>
#include <grub/err.h>
#include <grub/file.h>
#include <grub/fdt.h>
#include <grub/linux.h>
#include <grub/loader.h>
#include <grub/mm.h>
#include <grub/types.h>
#include <grub/cpu/linux.h>
#include <grub/efi/efi.h>
#include <grub/efi/fdtload.h>
#include <grub/efi/memory.h>
#include <grub/efi/pe32.h>
#include <grub/i18n.h>
#include <grub/lib/cmdline.h>
#include <grub/verify.h>
#include <grub/term.h>
#include <grub/env.h>

GRUB_MOD_LICENSE ("GPLv3+");

static grub_dl_t my_mod;
static int loaded;

static void *kernel_addr;
static grub_uint64_t kernel_size;

static char *linux_args;
static grub_uint32_t cmdline_size;

static grub_addr_t initrd_start;
static grub_addr_t initrd_end;

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

grub_err_t
grub_arch_efi_linux_check_image (struct linux_arch_kernel_header * lh)
{
  if (lh->magic != GRUB_LINUX_ARMXX_MAGIC_SIGNATURE)
    return grub_error(GRUB_ERR_BAD_OS, "invalid magic number");

  if ((lh->code0 & 0xffff) != GRUB_PE32_MAGIC)
    return grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET,
		       N_("plain image kernel not supported - rebuild with CONFIG_(U)EFI_STUB enabled"));

  grub_dprintf ("linux", "UEFI stub kernel:\n");
  grub_dprintf ("linux", "PE/COFF header @ %08x\n", lh->hdr_offset);

  return GRUB_ERR_NONE;
}

static grub_err_t
finalize_params_linux (void)
{
  int node, retval;

  void *fdt;

  fdt = grub_fdt_load (GRUB_EFI_LINUX_FDT_EXTRA_SPACE);

  if (!fdt)
    goto failure;

  node = grub_fdt_find_subnode (fdt, 0, "chosen");
  if (node < 0)
    node = grub_fdt_add_subnode (fdt, 0, "chosen");

  if (node < 1)
    goto failure;

  /* Set initrd info */
  if (initrd_start && initrd_end > initrd_start)
    {
      grub_dprintf ("linux", "Initrd @ %p-%p\n",
		    (void *) initrd_start, (void *) initrd_end);

      retval = grub_fdt_set_prop64 (fdt, node, "linux,initrd-start",
				    initrd_start);
      if (retval)
	goto failure;
      retval = grub_fdt_set_prop64 (fdt, node, "linux,initrd-end",
				    initrd_end);
      if (retval)
	goto failure;
    }

  if (grub_fdt_install() != GRUB_ERR_NONE)
    goto failure;

  return GRUB_ERR_NONE;

failure:
  grub_fdt_unload();
  return grub_error(GRUB_ERR_BAD_OS, "failed to install/update FDT");
}

grub_err_t
grub_arch_efi_linux_boot_image (grub_addr_t addr, grub_size_t size, char *args)
{
  grub_efi_memory_mapped_device_path_t *mempath;
  grub_efi_handle_t image_handle;
  grub_efi_boot_services_t *b;
  grub_efi_status_t status;
  grub_efi_loaded_image_t *loaded_image;
  int len;

  mempath = grub_malloc (2 * sizeof (grub_efi_memory_mapped_device_path_t));
  if (!mempath)
    return grub_errno;

  mempath[0].header.type = GRUB_EFI_HARDWARE_DEVICE_PATH_TYPE;
  mempath[0].header.subtype = GRUB_EFI_MEMORY_MAPPED_DEVICE_PATH_SUBTYPE;
  mempath[0].header.length = grub_cpu_to_le16_compile_time (sizeof (*mempath));
  mempath[0].memory_type = GRUB_EFI_LOADER_DATA;
  mempath[0].start_address = addr;
  mempath[0].end_address = addr + size;

  mempath[1].header.type = GRUB_EFI_END_DEVICE_PATH_TYPE;
  mempath[1].header.subtype = GRUB_EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE;
  mempath[1].header.length = sizeof (grub_efi_device_path_t);

  b = grub_efi_system_table->boot_services;
  status = b->load_image (0, grub_efi_image_handle,
			  (grub_efi_device_path_t *) mempath,
			  (void *) addr, size, &image_handle);
  if (status != GRUB_EFI_SUCCESS)
    return grub_error (GRUB_ERR_BAD_OS, "cannot load image");

  grub_dprintf ("linux", "linux command line: '%s'\n", args);

  /* Convert command line to UCS-2 */
  loaded_image = grub_efi_get_loaded_image (image_handle);
  loaded_image->load_options_size = len =
    (grub_strlen (args) + 1) * sizeof (grub_efi_char16_t);
  loaded_image->load_options =
    grub_efi_allocate_any_pages (GRUB_EFI_BYTES_TO_PAGES (loaded_image->load_options_size));
  if (!loaded_image->load_options)
    return grub_errno;

  loaded_image->load_options_size =
    2 * grub_utf8_to_utf16 (loaded_image->load_options, len,
			    (grub_uint8_t *) args, len, NULL);

  grub_dprintf ("linux", "starting image %p\n", image_handle);
  status = b->start_image (image_handle, 0, NULL);

  /* When successful, not reached */
  b->unload_image (image_handle);
  grub_efi_free_pages ((grub_addr_t) loaded_image->load_options,
		       GRUB_EFI_BYTES_TO_PAGES (loaded_image->load_options_size));

  return grub_errno;
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
  ventoy_preboot();

  if (finalize_params_linux () != GRUB_ERR_NONE)
    return grub_errno;

  return (grub_arch_efi_linux_boot_image((grub_addr_t)kernel_addr,
                                          kernel_size, linux_args));
}

static grub_err_t
grub_linux_unload (void)
{
  grub_dl_unref (my_mod);
  loaded = 0;
  if (initrd_start)
    grub_efi_free_pages ((grub_efi_physical_address_t) initrd_start,
			 GRUB_EFI_BYTES_TO_PAGES (initrd_end - initrd_start));
  initrd_start = initrd_end = 0;
  grub_free (linux_args);
  if (kernel_addr)
    grub_efi_free_pages ((grub_addr_t) kernel_addr,
			 GRUB_EFI_BYTES_TO_PAGES (kernel_size));
  grub_fdt_unload ();
  return GRUB_ERR_NONE;
}

/*
 * As per linux/Documentation/arm/Booting
 * ARM initrd needs to be covered by kernel linear mapping,
 * so place it in the first 512MB of DRAM.
 *
 * As per linux/Documentation/arm64/booting.txt
 * ARM64 initrd needs to be contained entirely within a 1GB aligned window
 * of up to 32GB of size that covers the kernel image as well.
 * Since the EFI stub loader will attempt to load the kernel near start of
 * RAM, place the buffer in the first 32GB of RAM.
 */
#ifdef __arm__
#define INITRD_MAX_ADDRESS_OFFSET (512U * 1024 * 1024)
#else /* __aarch64__ */
#define INITRD_MAX_ADDRESS_OFFSET (32ULL * 1024 * 1024 * 1024)
#endif

/*
 * This function returns a pointer to a legally allocated initrd buffer,
 * or NULL if unsuccessful
 */
static void *
allocate_initrd_mem (int initrd_pages)
{
  grub_addr_t max_addr;

  if (grub_efi_get_ram_base (&max_addr) != GRUB_ERR_NONE)
    return NULL;

  max_addr += INITRD_MAX_ADDRESS_OFFSET - 1;

  return grub_efi_allocate_pages_real (max_addr, initrd_pages,
				       GRUB_EFI_ALLOCATE_MAX_ADDRESS,
				       GRUB_EFI_LOADER_DATA);
}

static grub_err_t
grub_cmd_initrd (grub_command_t cmd __attribute__ ((unused)),
		 int argc, char *argv[])
{
  struct grub_linux_initrd_context initrd_ctx = { 0, 0, 0 };
  int initrd_size, initrd_pages;
  void *initrd_mem = NULL;

  if (argc == 0)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
      goto fail;
    }

  if (!loaded)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT,
		  N_("you need to load the kernel first"));
      goto fail;
    }

  if (grub_initrd_init (argc, argv, &initrd_ctx))
    goto fail;

  initrd_size = grub_get_initrd_size (&initrd_ctx);
  grub_dprintf ("linux", "Loading initrd\n");

  initrd_pages = (GRUB_EFI_BYTES_TO_PAGES (initrd_size));
  initrd_mem = allocate_initrd_mem (initrd_pages);

  if (!initrd_mem)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
      goto fail;
    }

  if (grub_initrd_load (&initrd_ctx, argv, initrd_mem))
    goto fail;

  initrd_start = (grub_addr_t) initrd_mem;
  initrd_end = initrd_start + initrd_size;
  grub_dprintf ("linux", "[addr=%p, size=0x%x]\n",
		(void *) initrd_start, initrd_size);

 fail:
  grub_initrd_close (&initrd_ctx);
  if (initrd_mem && !initrd_start)
    grub_efi_free_pages ((grub_addr_t) initrd_mem, initrd_pages);

  return grub_errno;
}

static grub_err_t
grub_cmd_linux (grub_command_t cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
  grub_file_t file = 0;
  struct linux_arch_kernel_header lh;
  grub_err_t err;

  grub_dl_ref (my_mod);

  if (argc == 0)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
      goto fail;
    }

  file = grub_file_open (argv[0], GRUB_FILE_TYPE_LINUX_KERNEL);
  if (!file)
    goto fail;

  kernel_size = grub_file_size (file);

  if (grub_file_read (file, &lh, sizeof (lh)) < (long) sizeof (lh))
    return grub_errno;

  if (grub_arch_efi_linux_check_image (&lh) != GRUB_ERR_NONE)
    goto fail;

  grub_loader_unset();

  grub_dprintf ("linux", "kernel file size: %lld\n", (long long) kernel_size);
  kernel_addr = grub_efi_allocate_any_pages (GRUB_EFI_BYTES_TO_PAGES (kernel_size));
  grub_dprintf ("linux", "kernel numpages: %lld\n",
		(long long) GRUB_EFI_BYTES_TO_PAGES (kernel_size));
  if (!kernel_addr)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
      goto fail;
    }

  grub_file_seek (file, 0);
  if (grub_file_read (file, kernel_addr, kernel_size)
      < (grub_int64_t) kernel_size)
    {
      if (!grub_errno)
	grub_error (GRUB_ERR_BAD_OS, N_("premature end of file %s"), argv[0]);
      goto fail;
    }

  grub_dprintf ("linux", "kernel @ %p\n", kernel_addr);

  cmdline_size = grub_loader_cmdline_size (argc, argv) + sizeof (LINUX_IMAGE);
  linux_args = grub_malloc (cmdline_size);
  if (!linux_args)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
      goto fail;
    }
  grub_memcpy (linux_args, LINUX_IMAGE, sizeof (LINUX_IMAGE));

  if (ventoy_linux_argc)
  {
      ventoy_bootopt_hook(argc, argv);
      err = grub_create_loader_cmdline (ventoy_linux_argc, ventoy_linux_args,
    				    linux_args + sizeof (LINUX_IMAGE) - 1,
    				    cmdline_size,
    				    GRUB_VERIFY_KERNEL_CMDLINE);  }
  else
  {      
      err = grub_create_loader_cmdline (argc, argv,
    				    linux_args + sizeof (LINUX_IMAGE) - 1,
    				    cmdline_size,
    				    GRUB_VERIFY_KERNEL_CMDLINE);
  }

  if (err)
    goto fail;

  if (grub_errno == GRUB_ERR_NONE)
    {
      grub_loader_set (grub_linux_boot, grub_linux_unload, 0);
      loaded = 1;
    }

fail:
  if (file)
    grub_file_close (file);

  if (grub_errno != GRUB_ERR_NONE)
    {
      grub_dl_unref (my_mod);
      loaded = 0;
    }

  if (linux_args && !loaded)
    grub_free (linux_args);

  if (kernel_addr && !loaded)
    grub_efi_free_pages ((grub_addr_t) kernel_addr,
			 GRUB_EFI_BYTES_TO_PAGES (kernel_size));

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


static grub_command_t cmd_linux, cmd_initrd, cmd_linuxefi, cmd_initrdefi;
static grub_command_t cmd_set_bootopt, cmd_unset_bootopt, cmd_extra_initrd_append, cmd_extra_initrd_reset;


GRUB_MOD_INIT (linux)
{
  cmd_linux = grub_register_command ("linux", grub_cmd_linux, 0,
				     N_("Load Linux."));
  cmd_initrd = grub_register_command ("initrd", ventoy_cmd_initrd, 0,
				      N_("Load initrd."));

  cmd_linuxefi = grub_register_command ("linuxefi", grub_cmd_linux,
				     0, N_("Load Linux."));
  cmd_initrdefi = grub_register_command ("initrdefi", ventoy_cmd_initrd,
				      0, N_("Load initrd."));

  cmd_set_bootopt = grub_register_command ("vt_set_boot_opt", grub_cmd_set_boot_opt, 0, N_("set ext boot opt"));
  cmd_unset_bootopt = grub_register_command ("vt_unset_boot_opt", grub_cmd_unset_boot_opt, 0, N_("unset ext boot opt"));
  
  cmd_extra_initrd_append = grub_register_command ("vt_img_extra_initrd_append", grub_cmd_extra_initrd_append, 0, N_(""));
  cmd_extra_initrd_reset = grub_register_command ("vt_img_extra_initrd_reset", grub_cmd_extra_initrd_reset, 0, N_(""));

  ventoy_linux_args = grub_zalloc(sizeof(char *) * LINUX_MAX_ARGC);
                      
  my_mod = mod;
}

GRUB_MOD_FINI (linux)
{
  grub_unregister_command (cmd_linux);
  grub_unregister_command (cmd_initrd);
}
