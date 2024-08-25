/* linux.c - boot Linux */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2004,2005,2007,2009,2010  Free Software Foundation, Inc.
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

#include <grub/elf.h>
#include <grub/elfload.h>
#include <grub/loader.h>
#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/command.h>
#include <grub/mips/relocator.h>
#include <grub/memory.h>
#include <grub/i18n.h>
#include <grub/lib/cmdline.h>
#include <grub/linux.h>

GRUB_MOD_LICENSE ("GPLv3+");

#pragma GCC diagnostic ignored "-Wcast-align"

/* For frequencies.  */
#include <grub/machine/time.h>

#ifdef GRUB_MACHINE_MIPS_LOONGSON
#include <grub/pci.h>
#include <grub/machine/kernel.h>

const char loongson_machtypes[][60] =
  {
    [GRUB_ARCH_MACHINE_YEELOONG] = "machtype=lemote-yeeloong-2f-8.9inches",
    [GRUB_ARCH_MACHINE_FULOONG2F]  = "machtype=lemote-fuloong-2f-box",
    [GRUB_ARCH_MACHINE_FULOONG2E]  = "machtype=lemote-fuloong-2e-unknown"
  };
#endif

static grub_dl_t my_mod;

static int loaded;

static grub_size_t linux_size;

static struct grub_relocator *relocator;
static grub_uint8_t *playground;
static grub_addr_t target_addr, entry_addr;
#ifdef GRUB_MACHINE_MIPS_QEMU_MIPS
static char *params;
#else
static int linux_argc;
static grub_off_t argv_off;
#ifdef GRUB_MACHINE_MIPS_LOONGSON
static grub_off_t envp_off;
#endif
static grub_off_t rd_addr_arg_off, rd_size_arg_off;
#endif
static int initrd_loaded = 0;

static grub_err_t
grub_linux_boot (void)
{
  struct grub_relocator32_state state;

  grub_memset (&state, 0, sizeof (state));

  /* Boot the kernel.  */
  state.gpr[1] = entry_addr;

#ifdef GRUB_MACHINE_MIPS_QEMU_MIPS
  {
    grub_err_t err;
    grub_relocator_chunk_t ch;
    grub_uint32_t *memsize;
    grub_uint32_t *magic;
    char *str;

    err = grub_relocator_alloc_chunk_addr (relocator, &ch,
					   ((16 << 20) - 264),
					   grub_strlen (params) + 1 + 8);
    if (err)
      return err;
    memsize = get_virtual_current_address (ch);
    magic = memsize + 1;
    *memsize = grub_mmap_get_lower ();
    *magic = 0x12345678;
    str = (char *) (magic + 1);
    grub_strcpy (str, params);
  }
#endif  

#ifndef GRUB_MACHINE_MIPS_QEMU_MIPS
  state.gpr[4] = linux_argc;
  state.gpr[5] = target_addr + argv_off;
#ifdef GRUB_MACHINE_MIPS_LOONGSON
  state.gpr[6] = target_addr + envp_off;
#else
  state.gpr[6] = 0;
#endif
  state.gpr[7] = 0;
#endif
  state.jumpreg = 1;
  grub_relocator32_boot (relocator, state);

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_linux_unload (void)
{
  grub_relocator_unload (relocator);
  grub_dl_unref (my_mod);

#ifdef GRUB_MACHINE_MIPS_QEMU_MIPS
  grub_free (params);
  params = 0;
#endif

  loaded = 0;

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_linux_load32 (grub_elf_t elf, const char *filename,
		   void **extra_mem, grub_size_t extra_size)
{
  Elf32_Addr base;
  int extraoff;
  grub_err_t err;

  /* Linux's entry point incorrectly contains a virtual address.  */
  entry_addr = elf->ehdr.ehdr32.e_entry;

  linux_size = grub_elf32_size (elf, &base, 0);
  if (linux_size == 0)
    return grub_errno;
  target_addr = base;
  /* Pad it; the kernel scribbles over memory beyond its load address.  */
  linux_size += 0x100000;
  linux_size = ALIGN_UP (base + linux_size, 4) - base;
  extraoff = linux_size;
  linux_size += extra_size;

  relocator = grub_relocator_new ();
  if (!relocator)
    return grub_errno;

  {
    grub_relocator_chunk_t ch;
    err = grub_relocator_alloc_chunk_addr (relocator, &ch,
					   target_addr & 0x1fffffff,
					   linux_size);
    if (err)
      return err;
    playground = get_virtual_current_address (ch);
  }

  *extra_mem = playground + extraoff;

  /* Now load the segments into the area we claimed.  */
  return grub_elf32_load (elf, filename, playground - base, GRUB_ELF_LOAD_FLAGS_NONE, 0, 0);
}

static grub_err_t
grub_linux_load64 (grub_elf_t elf, const char *filename,
		   void **extra_mem, grub_size_t extra_size)
{
  Elf64_Addr base;
  int extraoff;
  grub_err_t err;

  /* Linux's entry point incorrectly contains a virtual address.  */
  entry_addr = elf->ehdr.ehdr64.e_entry;

  linux_size = grub_elf64_size (elf, &base, 0);
  if (linux_size == 0)
    return grub_errno;
  target_addr = base;
  /* Pad it; the kernel scribbles over memory beyond its load address.  */
  linux_size += 0x100000;
  linux_size = ALIGN_UP (base + linux_size, 4) - base;
  extraoff = linux_size;
  linux_size += extra_size;

  relocator = grub_relocator_new ();
  if (!relocator)
    return grub_errno;

  {
    grub_relocator_chunk_t ch;
    err = grub_relocator_alloc_chunk_addr (relocator, &ch,
					   target_addr & 0x1fffffff,
					   linux_size);
    if (err)
      return err;
    playground = get_virtual_current_address (ch);
  }

  *extra_mem = playground + extraoff;

  /* Now load the segments into the area we claimed.  */
  return grub_elf64_load (elf, filename, playground - base, GRUB_ELF_LOAD_FLAGS_NONE, 0, 0);
}

static grub_err_t
grub_cmd_linux (grub_command_t cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
  grub_elf_t elf = 0;
  int size;
  void *extra = NULL;
#ifndef GRUB_MACHINE_MIPS_QEMU_MIPS
  int i;
  grub_uint32_t *linux_argv;
  char *linux_args;
#endif
  grub_err_t err;
#ifdef GRUB_MACHINE_MIPS_LOONGSON
  char *linux_envs;
  grub_uint32_t *linux_envp;
#endif

  if (argc == 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

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

#ifdef GRUB_MACHINE_MIPS_QEMU_MIPS
  size = 0;
#else
  /* For arguments.  */
  linux_argc = argc;
#ifdef GRUB_MACHINE_MIPS_LOONGSON
  linux_argc++;
#endif
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
#ifdef GRUB_MACHINE_MIPS_LOONGSON
  size += ALIGN_UP (sizeof (loongson_machtypes[0]), 4);
#endif

  /* rd arguments.  */
  size += ALIGN_UP (sizeof ("rd_start=0xXXXXXXXXXXXXXXXX"), 4);
  size += ALIGN_UP (sizeof ("rd_size=0xXXXXXXXXXXXXXXXX"), 4);

  /* For the environment.  */
  size += sizeof (grub_uint32_t);
  size += 4 * sizeof (grub_uint32_t);
  size += ALIGN_UP (sizeof ("memsize=XXXXXXXXXXXXXXXXXXXX"), 4)
    + ALIGN_UP (sizeof ("highmemsize=XXXXXXXXXXXXXXXXXXXX"), 4)
    + ALIGN_UP (sizeof ("busclock=XXXXXXXXXX"), 4)
    + ALIGN_UP (sizeof ("cpuclock=XXXXXXXXXX"), 4);
#endif

  if (grub_elf_is_elf32 (elf))
    err = grub_linux_load32 (elf, argv[0], &extra, size);
  else
  if (grub_elf_is_elf64 (elf))
    err = grub_linux_load64 (elf, argv[0], &extra, size);
  else
    err = grub_error (GRUB_ERR_BAD_OS, N_("invalid arch-dependent ELF magic"));

  grub_elf_close (elf);

  if (err)
    return err;

#ifdef GRUB_MACHINE_MIPS_QEMU_MIPS
  /* Create kernel command line.  */
  size = grub_loader_cmdline_size(argc, argv);
  params = grub_malloc (size + sizeof (LINUX_IMAGE));
  if (! params)
    {
      grub_linux_unload ();
      return grub_errno;
    }

  grub_memcpy (params, LINUX_IMAGE, sizeof (LINUX_IMAGE));
  grub_create_loader_cmdline (argc, argv, params + sizeof (LINUX_IMAGE) - 1,
			      size, GRUB_VERIFY_KERNEL_CMDLINE);
#else
  linux_argv = extra;
  argv_off = (grub_uint8_t *) linux_argv - (grub_uint8_t *) playground;
  extra = linux_argv + (linux_argc + 1 + 2);
  linux_args = extra;

  grub_memcpy (linux_args, "a0", sizeof ("a0"));
  *linux_argv = (grub_uint8_t *) linux_args - (grub_uint8_t *) playground
    + target_addr;
  linux_argv++;
  linux_args += ALIGN_UP (sizeof ("a0"), 4);

  char *params = linux_args;

#ifdef GRUB_MACHINE_MIPS_LOONGSON
  {
    unsigned mtype = grub_arch_machine;
    if (mtype >= ARRAY_SIZE (loongson_machtypes))
      mtype = 0;
    /* In Loongson platform, it is the responsibility of the bootloader/firmware
       to supply the OS kernel with machine type information.  */
    grub_memcpy (linux_args, loongson_machtypes[mtype],
		 sizeof (loongson_machtypes[mtype]));
    *linux_argv = (grub_uint8_t *) linux_args - (grub_uint8_t *) playground
      + target_addr;
    linux_argv++;
    linux_args += ALIGN_UP (sizeof (loongson_machtypes[mtype]), 4);
  }
#endif

  for (i = 1; i < argc; i++)
    {
      grub_memcpy (linux_args, argv[i], grub_strlen (argv[i]) + 1);
      *linux_argv = (grub_uint8_t *) linux_args - (grub_uint8_t *) playground
	+ target_addr;
      linux_argv++;
      linux_args += ALIGN_UP (grub_strlen (argv[i]) + 1, 4);
    }

  *linux_args = '\0';

  err = grub_verify_string (params, GRUB_VERIFY_KERNEL_CMDLINE);
  if (err)
    return err;

  /* Reserve space for rd arguments.  */
  rd_addr_arg_off = (grub_uint8_t *) linux_args - (grub_uint8_t *) playground;
  linux_args += ALIGN_UP (sizeof ("rd_start=0xXXXXXXXXXXXXXXXX"), 4);
  *linux_argv = 0;
  linux_argv++;

  rd_size_arg_off = (grub_uint8_t *) linux_args - (grub_uint8_t *) playground;
  linux_args += ALIGN_UP (sizeof ("rd_size=0xXXXXXXXXXXXXXXXX"), 4);
  *linux_argv = 0;
  linux_argv++;

  *linux_argv = 0;

  extra = linux_args;

#ifdef GRUB_MACHINE_MIPS_LOONGSON
  linux_envp = extra;
  envp_off = (grub_uint8_t *) linux_envp - (grub_uint8_t *) playground;
  linux_envs = (char *) (linux_envp + 5);
  grub_snprintf (linux_envs, sizeof ("memsize=XXXXXXXXXXXXXXXXXXXX"),
		 "memsize=%lld",
		 (unsigned long long) grub_mmap_get_lower () >> 20);
  linux_envp[0] = (grub_uint8_t *) linux_envs - (grub_uint8_t *) playground
    + target_addr;
  linux_envs += ALIGN_UP (grub_strlen (linux_envs) + 1, 4);
  grub_snprintf (linux_envs, sizeof ("highmemsize=XXXXXXXXXXXXXXXXXXXX"),
		 "highmemsize=%lld",
		 (unsigned long long) grub_mmap_get_upper () >> 20);
  linux_envp[1] = (grub_uint8_t *) linux_envs - (grub_uint8_t *) playground
    + target_addr;
  linux_envs += ALIGN_UP (grub_strlen (linux_envs) + 1, 4);

  grub_snprintf (linux_envs, sizeof ("busclock=XXXXXXXXXX"),
		 "busclock=%d", grub_arch_busclock);
  linux_envp[2] = (grub_uint8_t *) linux_envs - (grub_uint8_t *) playground
    + target_addr;
  linux_envs += ALIGN_UP (grub_strlen (linux_envs) + 1, 4);
  grub_snprintf (linux_envs, sizeof ("cpuclock=XXXXXXXXXX"),
		 "cpuclock=%d", grub_arch_cpuclock);
  linux_envp[3] = (grub_uint8_t *) linux_envs - (grub_uint8_t *) playground
    + target_addr;
  linux_envs += ALIGN_UP (grub_strlen (linux_envs) + 1, 4);

  linux_envp[4] = 0;
#endif
#endif

  grub_loader_set (grub_linux_boot, grub_linux_unload, 1);
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
  void *initrd_src;
  grub_addr_t initrd_dest;
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
					    (target_addr & 0x1fffffff)
					    + linux_size + 0x10000,
					    (0x10000000 - size),
					    size, 0x10000,
					    GRUB_RELOCATOR_PREFERENCE_NONE, 0);

    if (err)
      goto fail;
    initrd_src = get_virtual_current_address (ch);
    initrd_dest = get_physical_target_address (ch) | 0x80000000;
  }

  if (grub_initrd_load (&initrd_ctx, argv, initrd_src))
    goto fail;

#ifdef GRUB_MACHINE_MIPS_QEMU_MIPS
  {
    char *tmp;
    tmp = grub_xasprintf ("%s rd_start=0x%" PRIxGRUB_ADDR
			  " rd_size=0x%" PRIxGRUB_ADDR, params,
			  initrd_dest, size);
    if (!tmp)
      goto fail;
    grub_free (params);
    params = tmp;
  }
#else
  grub_snprintf ((char *) playground + rd_addr_arg_off,
		 sizeof ("rd_start=0xXXXXXXXXXXXXXXXX"), "rd_start=0x%llx",
		(unsigned long long) initrd_dest);
  ((grub_uint32_t *) (playground + argv_off))[linux_argc]
    = target_addr + rd_addr_arg_off;
  linux_argc++;

  grub_snprintf ((char *) playground + rd_size_arg_off,
		sizeof ("rd_size=0xXXXXXXXXXXXXXXXXX"), "rd_size=0x%llx",
		(unsigned long long) size);
  ((grub_uint32_t *) (playground + argv_off))[linux_argc]
    = target_addr + rd_size_arg_off;
  linux_argc++;
#endif

  initrd_loaded = 1;

 fail:
  grub_initrd_close (&initrd_ctx);

  return grub_errno;
}

static grub_command_t cmd_linux, cmd_initrd;

GRUB_MOD_INIT(linux)
{
  cmd_linux = grub_register_command ("linux", grub_cmd_linux,
				     0, N_("Load Linux."));
  cmd_initrd = grub_register_command ("initrd", grub_cmd_initrd,
				      0, N_("Load initrd."));
  my_mod = mod;
}

GRUB_MOD_FINI(linux)
{
  grub_unregister_command (cmd_linux);
  grub_unregister_command (cmd_initrd);
}
