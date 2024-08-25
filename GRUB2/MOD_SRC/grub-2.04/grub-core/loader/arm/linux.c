/* linux.c - boot Linux */
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

#include <grub/dl.h>
#include <grub/fdt.h>
#include <grub/file.h>
#include <grub/loader.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/command.h>
#include <grub/cache.h>
#include <grub/cpu/linux.h>
#include <grub/lib/cmdline.h>
#include <grub/linux.h>
#include <grub/verify.h>

GRUB_MOD_LICENSE ("GPLv3+");

static grub_dl_t my_mod;

static grub_addr_t initrd_start;
static grub_addr_t initrd_end;

static grub_addr_t linux_addr;
static grub_size_t linux_size;

static char *linux_args;

static grub_uint32_t machine_type;
static const void *current_fdt;

typedef void (*kernel_entry_t) (int, unsigned long, void *);

#define LINUX_PHYS_OFFSET        (0x00008000)
#define LINUX_INITRD_PHYS_OFFSET (LINUX_PHYS_OFFSET + 0x03000000)
#define LINUX_FDT_PHYS_OFFSET    (LINUX_INITRD_PHYS_OFFSET - 0x10000)

static grub_size_t
get_atag_size (const grub_uint32_t *atag)
{
  const grub_uint32_t *atag0 = atag;
  while (atag[0] && atag[1])
    atag += atag[0];
  return atag - atag0;
}

/*
 * linux_prepare_fdt():
 *   Prepares a loaded FDT for being passed to Linux.
 *   Merges in command line parameters and sets up initrd addresses.
 */
static grub_err_t
linux_prepare_atag (void *target_atag)
{
  const grub_uint32_t *atag_orig = (const grub_uint32_t *) current_fdt;
  grub_uint32_t *tmp_atag, *to;
  const grub_uint32_t *from;
  grub_size_t tmp_size;
  grub_size_t arg_size = grub_strlen (linux_args);
  char *cmdline_orig = NULL;
  grub_size_t cmdline_orig_len = 0;

  /* some place for cmdline, initrd and terminator.  */
  tmp_size = get_atag_size (atag_orig) + 20 + (arg_size) / 4;
  tmp_atag = grub_malloc (tmp_size * sizeof (grub_uint32_t));
  if (!tmp_atag)
    return grub_errno;

  for (from = atag_orig, to = tmp_atag; from[0] && from[1];
       from += from[0])
    switch (from[1])
      {
      case 0x54410004:
      case 0x54410005:
      case 0x54420005:
	break;
      case 0x54410009:
	if (*(char *) (from + 2))
	  {
	    cmdline_orig = (char *) (from + 2);
	    cmdline_orig_len = grub_strlen (cmdline_orig) + 1;
	  }
	break;
      default:
	grub_memcpy (to, from, sizeof (grub_uint32_t) * from[0]);
	to += from[0];
	break;
      }

  grub_dprintf ("linux", "linux inherited args: '%s'\n",
		cmdline_orig ? : "");
  grub_dprintf ("linux", "linux_args: '%s'\n", linux_args);

  /* Generate and set command line */
  to[0] = 3 + (arg_size + cmdline_orig_len) / 4;
  to[1] = 0x54410009;
  if (cmdline_orig)
    {
      grub_memcpy ((char *) to + 8, cmdline_orig, cmdline_orig_len - 1);
      *((char *) to + 8 + cmdline_orig_len - 1) = ' ';
    }
  grub_memcpy ((char *) to + 8 + cmdline_orig_len, linux_args, arg_size);
  grub_memset ((char *) to + 8 + cmdline_orig_len + arg_size, 0,
	       4 - ((arg_size + cmdline_orig_len) & 3));
  to += to[0];

  if (initrd_start && initrd_end)
    {
      /*
       * We're using physical addresses, so even if we have LPAE, we're
       * restricted to a 32-bit address space.
       */
      grub_dprintf ("loader", "Initrd @ 0x%08x-0x%08x\n",
		    initrd_start, initrd_end);

      to[0] = 4;
      to[1] = 0x54420005;
      to[2] = initrd_start;
      to[3] = initrd_end - initrd_start;
      to += 4;
    }

  to[0] = 0;
  to[1] = 0;
  to += 2;

  /* Copy updated FDT to its launch location */
  grub_memcpy (target_atag, tmp_atag, sizeof (grub_uint32_t) * (to - tmp_atag));
  grub_free (tmp_atag);

  grub_dprintf ("loader", "ATAG updated for Linux boot\n");

  return GRUB_ERR_NONE;
}

/*
 * linux_prepare_fdt():
 *   Prepares a loaded FDT for being passed to Linux.
 *   Merges in command line parameters and sets up initrd addresses.
 */
static grub_err_t
linux_prepare_fdt (void *target_fdt)
{
  int node;
  int retval;
  int tmp_size;
  void *tmp_fdt;

  tmp_size = grub_fdt_get_totalsize (current_fdt) + 0x100 + grub_strlen (linux_args);
  tmp_fdt = grub_malloc (tmp_size);
  if (!tmp_fdt)
    return grub_errno;

  grub_memcpy (tmp_fdt, current_fdt, grub_fdt_get_totalsize (current_fdt));
  grub_fdt_set_totalsize (tmp_fdt, tmp_size);

  /* Find or create '/chosen' node */
  node = grub_fdt_find_subnode (tmp_fdt, 0, "chosen");
  if (node < 0)
    {
      grub_dprintf ("linux", "No 'chosen' node in FDT - creating.\n");
      node = grub_fdt_add_subnode (tmp_fdt, 0, "chosen");
      if (node < 0)
	goto failure;
    }

  grub_dprintf ("linux", "linux_args: '%s'\n", linux_args);

  /* Generate and set command line */
  retval = grub_fdt_set_prop (tmp_fdt, node, "bootargs", linux_args,
			      grub_strlen (linux_args) + 1);
  if (retval)
    goto failure;

  if (initrd_start && initrd_end)
    {
      /*
       * We're using physical addresses, so even if we have LPAE, we're
       * restricted to a 32-bit address space.
       */
      grub_dprintf ("loader", "Initrd @ 0x%08x-0x%08x\n",
		    initrd_start, initrd_end);

      retval = grub_fdt_set_prop32 (tmp_fdt, node, "linux,initrd-start",
				    initrd_start);
      if (retval)
	goto failure;
      retval = grub_fdt_set_prop32 (tmp_fdt, node, "linux,initrd-end",
				    initrd_end);
      if (retval)
	goto failure;
    }

  /* Copy updated FDT to its launch location */
  grub_memcpy (target_fdt, tmp_fdt, tmp_size);
  grub_free (tmp_fdt);

  grub_dprintf ("loader", "FDT updated for Linux boot\n");

  return GRUB_ERR_NONE;

failure:
  grub_free (tmp_fdt);
  return grub_error (GRUB_ERR_BAD_ARGUMENT, "unable to prepare FDT");
}

static grub_err_t
linux_boot (void)
{
  kernel_entry_t linuxmain;
  int fdt_valid, atag_valid;
  void *target_fdt = 0;

  fdt_valid = (current_fdt && grub_fdt_check_header_nosize (current_fdt) == 0);
  atag_valid = ((((const grub_uint16_t *) current_fdt)[3] & ~3) == 0x5440
		&& *((const grub_uint32_t *) current_fdt));
  grub_dprintf ("loader", "atag: %p, %x, %x, %s, %s\n",
		current_fdt,
		((const grub_uint16_t *) current_fdt)[3],
		*((const grub_uint32_t *) current_fdt),
		(const char *) current_fdt,
		(const char *) current_fdt + 1);

  if (!fdt_valid && machine_type == GRUB_ARM_MACHINE_TYPE_FDT)
    return grub_error (GRUB_ERR_FILE_NOT_FOUND,
		       N_("device tree must be supplied (see `devicetree' command)"));

  grub_arch_sync_caches ((void *) linux_addr, linux_size);

  grub_dprintf ("loader", "Kernel at: 0x%x\n", linux_addr);

  if (fdt_valid || atag_valid)
    {
#ifdef GRUB_MACHINE_EFI
      grub_size_t size;
      if (fdt_valid)
	size = grub_fdt_get_totalsize (current_fdt);
      else
	size = 4 * get_atag_size (current_fdt);
      size += grub_strlen (linux_args) + 256;
      target_fdt = grub_efi_allocate_loader_memory (LINUX_FDT_PHYS_OFFSET, size);
      if (!target_fdt)
	return grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
#else
      target_fdt = (void *) LINUX_FDT_ADDRESS;
#endif
    }

  if (fdt_valid)
    {
      grub_err_t err;

      err = linux_prepare_fdt (target_fdt);
      if (err)
	return err;
      grub_dprintf ("loader", "FDT @ %p\n", target_fdt);
    }
  else if (atag_valid)
    {
      grub_err_t err;

      err = linux_prepare_atag (target_fdt);
      if (err)
	return err;
      grub_dprintf ("loader", "ATAG @ %p\n", target_fdt);
    }

  grub_dprintf ("loader", "Jumping to Linux...\n");

  /* Boot the kernel.
   *   Arguments to kernel:
   *     r0 - 0
   *     r1 - machine type
   *     r2 - address of DTB
   */
  linuxmain = (kernel_entry_t) linux_addr;

  grub_arm_disable_caches_mmu ();

  linuxmain (0, machine_type, target_fdt);

  return grub_error (GRUB_ERR_BAD_OS, "Linux call returned");
}

/*
 * Only support zImage, so no relocations necessary
 */
static grub_err_t
linux_load (const char *filename, grub_file_t file)
{
  struct linux_arm_kernel_header *lh;
  int size;

  size = grub_file_size (file);

  linux_addr = LINUX_ADDRESS;
  grub_dprintf ("loader", "Loading Linux to 0x%08x\n",
		(grub_addr_t) linux_addr);

  if (grub_file_read (file, (void *) linux_addr, size) != size)
    {
      if (!grub_errno)
	grub_error (GRUB_ERR_BAD_OS, N_("premature end of file %s"),
		    filename);
      return grub_errno;
    }

  lh = (void *) linux_addr;

  if ((grub_size_t) size > sizeof (*lh) &&
      lh->magic == GRUB_LINUX_ARM_MAGIC_SIGNATURE)
    ;
  else if (size > 0x8000 && *(grub_uint32_t *) (linux_addr) == 0xea000006
	   && machine_type == GRUB_ARM_MACHINE_TYPE_RASPBERRY_PI)
    grub_memmove ((void *) linux_addr, (void *) (linux_addr + 0x8000),
		  size - 0x8000);
  else
    return grub_error (GRUB_ERR_BAD_FILE_TYPE, N_("invalid zImage"));

  linux_size = size;

  return GRUB_ERR_NONE;
}

static grub_err_t
linux_unload (void)
{
  grub_dl_unref (my_mod);

  grub_free (linux_args);
  linux_args = NULL;

  initrd_start = initrd_end = 0;

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_linux (grub_command_t cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
  int size;
  grub_err_t err;
  grub_file_t file;
  grub_dl_ref (my_mod);

  if (argc == 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  file = grub_file_open (argv[0], GRUB_FILE_TYPE_LINUX_KERNEL);
  if (!file)
    goto fail;

  err = linux_load (argv[0], file);
  grub_file_close (file);
  if (err)
    goto fail;

  grub_loader_set (linux_boot, linux_unload, 0);

  size = grub_loader_cmdline_size (argc, argv);
  linux_args = grub_malloc (size + sizeof (LINUX_IMAGE));
  if (!linux_args)
    {
      grub_loader_unset();
      goto fail;
    }

  /* Create kernel command line.  */
  grub_memcpy (linux_args, LINUX_IMAGE, sizeof (LINUX_IMAGE));
  err = grub_create_loader_cmdline (argc, argv,
				    linux_args + sizeof (LINUX_IMAGE) - 1, size,
				    GRUB_VERIFY_KERNEL_CMDLINE);
  if (err)
    goto fail;

  return GRUB_ERR_NONE;

fail:
  grub_dl_unref (my_mod);
  return grub_errno;
}

static grub_err_t
grub_cmd_initrd (grub_command_t cmd __attribute__ ((unused)),
		 int argc, char *argv[])
{
  grub_file_t file;
  grub_size_t size = 0;
  struct grub_linux_initrd_context initrd_ctx = { 0, 0, 0 };

  if (argc == 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  file = grub_file_open (argv[0], GRUB_FILE_TYPE_LINUX_INITRD);
  if (!file)
    return grub_errno;

  if (grub_initrd_init (argc, argv, &initrd_ctx))
    goto fail;

  size = grub_get_initrd_size (&initrd_ctx);

  initrd_start = LINUX_INITRD_ADDRESS;

  grub_dprintf ("loader", "Loading initrd to 0x%08x\n",
		(grub_addr_t) initrd_start);

  if (grub_initrd_load (&initrd_ctx, argv, (void *) initrd_start))
    goto fail;

  initrd_end = initrd_start + size;

  return GRUB_ERR_NONE;

fail:
  grub_file_close (file);

  return grub_errno;
}

static grub_err_t
load_dtb (grub_file_t dtb, int size)
{
  void *new_fdt = grub_zalloc (size);
  if (!new_fdt)
    return grub_errno;
  grub_dprintf ("loader", "Loading device tree to %p\n",
		new_fdt);
  if ((grub_file_read (dtb, new_fdt, size) != size)
      || (grub_fdt_check_header (new_fdt, size) != 0))
    {
      grub_free (new_fdt);
      return grub_error (GRUB_ERR_BAD_OS, N_("invalid device tree"));
    }

  grub_fdt_set_totalsize (new_fdt, size);
  current_fdt = new_fdt;
  /* 
   * We've successfully loaded an FDT, so any machine type passed
   * from firmware is now obsolete.
   */
  machine_type = GRUB_ARM_MACHINE_TYPE_FDT;

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_devicetree (grub_command_t cmd __attribute__ ((unused)),
		     int argc, char *argv[])
{
  grub_file_t dtb;
  int size;

  if (argc != 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  dtb = grub_file_open (argv[0], GRUB_FILE_TYPE_DEVICE_TREE_IMAGE);
  if (!dtb)
    return grub_errno;

  size = grub_file_size (dtb);
  if (size == 0)
    grub_error (GRUB_ERR_BAD_OS, "empty file");
  else
    load_dtb (dtb, size);
  grub_file_close (dtb);

  return grub_errno;
}

static grub_command_t cmd_linux, cmd_initrd, cmd_devicetree;

GRUB_MOD_INIT (linux)
{
  cmd_linux = grub_register_command ("linux", grub_cmd_linux,
				     0, N_("Load Linux."));
  cmd_initrd = grub_register_command ("initrd", grub_cmd_initrd,
				      0, N_("Load initrd."));
  cmd_devicetree = grub_register_command ("devicetree", grub_cmd_devicetree,
					  /* TRANSLATORS: DTB stands for device tree blob.  */
					  0, N_("Load DTB file."));
  my_mod = mod;
  current_fdt = (const void *) grub_arm_firmware_get_boot_data ();
  machine_type = grub_arm_firmware_get_machine_type ();
}

GRUB_MOD_FINI (linux)
{
  grub_unregister_command (cmd_linux);
  grub_unregister_command (cmd_initrd);
  grub_unregister_command (cmd_devicetree);
}
