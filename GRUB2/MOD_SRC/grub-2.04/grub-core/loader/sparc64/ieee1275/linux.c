/* linux.c - boot Linux */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003, 2004, 2005, 2007, 2009  Free Software Foundation, Inc.
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
#include <grub/ieee1275/ieee1275.h>
#include <grub/command.h>
#include <grub/i18n.h>
#include <grub/memory.h>
#include <grub/lib/cmdline.h>
#include <grub/linux.h>

GRUB_MOD_LICENSE ("GPLv3+");

static grub_dl_t my_mod;

static int loaded;

/* /virtual-memory/translations property layout  */
struct grub_ieee1275_translation {
  grub_uint64_t vaddr;
  grub_uint64_t size;
  grub_uint64_t data;
};

static struct grub_ieee1275_translation *of_trans;
static int of_num_trans;

static grub_addr_t phys_base;
static grub_addr_t grub_phys_start;
static grub_addr_t grub_phys_end;

static grub_addr_t initrd_addr;
static grub_addr_t initrd_paddr;
static grub_size_t initrd_size;

static Elf64_Addr linux_entry;
static grub_addr_t linux_addr;
static grub_addr_t linux_paddr;
static grub_size_t linux_size;

static char *linux_args;

struct linux_bootstr_info {
	int len, valid;
	char buf[];
};

struct linux_hdrs {
	/* All HdrS versions support these fields.  */
	unsigned int start_insns[2];
	char magic[4]; /* "HdrS" */
	unsigned int linux_kernel_version; /* LINUX_VERSION_CODE */
	unsigned short hdrs_version;
	unsigned short root_flags;
	unsigned short root_dev;
	unsigned short ram_flags;
	unsigned int __deprecated_ramdisk_image;
	unsigned int ramdisk_size;

	/* HdrS versions 0x0201 and higher only */
	char *reboot_command;

	/* HdrS versions 0x0202 and higher only */
	struct linux_bootstr_info *bootstr_info;

	/* HdrS versions 0x0301 and higher only */
	unsigned long ramdisk_image;
};

static grub_err_t
grub_linux_boot (void)
{
  struct linux_bootstr_info *bp;
  struct linux_hdrs *hp;
  grub_addr_t addr;

  hp = (struct linux_hdrs *) linux_addr;

  /* Any pointer we dereference in the kernel image must be relocated
     to where we actually loaded the kernel.  */
  addr = (grub_addr_t) hp->bootstr_info;
  addr += (linux_addr - linux_entry);
  bp = (struct linux_bootstr_info *) addr;

  /* Set the command line arguments, unless the kernel has been
     built with a fixed CONFIG_CMDLINE.  */
  if (!bp->valid)
    {
      int len = grub_strlen (linux_args) + 1;
      if (bp->len < len)
	len = bp->len;
      grub_memcpy(bp->buf, linux_args, len);
      bp->buf[len-1] = '\0';
      bp->valid = 1;
    }

  if (initrd_addr)
    {
      /* The kernel expects the physical address, adjusted relative
	 to the lowest address advertised in "/memory"'s available
	 property.

	 The history of this is that back when the kernel only supported
	 specifying a 32-bit ramdisk address, this was the way to still
	 be able to specify the ramdisk physical address even if memory
	 started at some place above 4GB.

	 The magic 0x400000 is KERNBASE, I have no idea why SILO adds
	 that term into the address, but it does and thus we have to do
	 it too as this is what the kernel expects.  */
      hp->ramdisk_image = initrd_paddr - phys_base + 0x400000;
      hp->ramdisk_size = initrd_size;
    }

  grub_dprintf ("loader", "Entry point: 0x%lx\n", linux_addr);
  grub_dprintf ("loader", "Initrd at: 0x%lx, size 0x%lx\n", initrd_addr,
		initrd_size);
  grub_dprintf ("loader", "Boot arguments: %s\n", linux_args);
  grub_dprintf ("loader", "Jumping to Linux...\n");

  /* Boot the kernel.  */
  asm volatile ("ldx	%0, %%o4\n"
		"ldx	%1, %%o6\n"
		"ldx	%2, %%o5\n"
		"mov    %%g0, %%o0\n"
		"mov    %%g0, %%o2\n"
		"mov    %%g0, %%o3\n"
		"jmp    %%o5\n"
	        "mov    %%g0, %%o1\n": :
		"m"(grub_ieee1275_entry_fn),
		"m"(grub_ieee1275_original_stack),
		"m"(linux_addr));

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_linux_release_mem (void)
{
  grub_free (linux_args);
  linux_args = 0;
  linux_addr = 0;
  initrd_addr = 0;

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_linux_unload (void)
{
  grub_err_t err;

  err = grub_linux_release_mem ();
  grub_dl_unref (my_mod);

  loaded = 0;

  return err;
}

#define FOUR_MB	(4 * 1024 * 1024)

/* Context for alloc_phys.  */
struct alloc_phys_ctx
{
  grub_addr_t size;
  grub_addr_t ret;
};

/* Helper for alloc_phys.  */
static int
alloc_phys_choose (grub_uint64_t addr, grub_uint64_t len,
		   grub_memory_type_t type, void *data)
{
  struct alloc_phys_ctx *ctx = data;
  grub_addr_t end = addr + len;

  if (type != GRUB_MEMORY_AVAILABLE)
    return 0;

  addr = ALIGN_UP (addr, FOUR_MB);
  if (addr + ctx->size >= end)
    return 0;

  /* OBP available region contains grub. Start at grub_phys_end. */
  /* grub_phys_start does not start at the beginning of the memory region */
  if ((grub_phys_start >= addr && grub_phys_end < end) ||
      (addr > grub_phys_start && addr < grub_phys_end))
    {
      addr = ALIGN_UP (grub_phys_end, FOUR_MB);
      if (addr + ctx->size >= end)
	return 0;
    }

  grub_dprintf("loader",
    "addr = 0x%lx grub_phys_start = 0x%lx grub_phys_end = 0x%lx\n",
    addr, grub_phys_start, grub_phys_end);

  if (loaded)
    {
      grub_addr_t linux_end = ALIGN_UP (linux_paddr + linux_size, FOUR_MB);

      if (addr >= linux_paddr && addr < linux_end)
	{
	  addr = linux_end;
	  if (addr + ctx->size >= end)
	    return 0;
	}
      if ((addr + ctx->size) >= linux_paddr
	  && (addr + ctx->size) < linux_end)
	{
	  addr = linux_end;
	  if (addr + ctx->size >= end)
	    return 0;
	}
    }

  ctx->ret = addr;
  return 1;
}

static grub_addr_t
alloc_phys (grub_addr_t size)
{
  struct alloc_phys_ctx ctx = {
    .size = size,
    .ret = (grub_addr_t) -1
  };

  grub_machine_mmap_iterate (alloc_phys_choose, &ctx);

  return ctx.ret;
}

static grub_err_t
grub_linux_load64 (grub_elf_t elf, const char *filename)
{
  grub_addr_t off, paddr, base;
  int ret;

  linux_entry = elf->ehdr.ehdr64.e_entry;
  linux_addr = 0x40004000;
  off = 0x4000;
  linux_size = grub_elf64_size (elf, 0, 0);
  if (linux_size == 0)
    return grub_errno;

  grub_dprintf ("loader", "Attempting to claim at 0x%lx, size 0x%lx.\n",
		linux_addr, linux_size);

  paddr = alloc_phys (linux_size + off);
  if (paddr == (grub_addr_t) -1)
    return grub_error (GRUB_ERR_OUT_OF_MEMORY,
		       "couldn't allocate physical memory");
  ret = grub_ieee1275_map (paddr, linux_addr - off,
			   linux_size + off, IEEE1275_MAP_DEFAULT);
  if (ret)
    return grub_error (GRUB_ERR_OUT_OF_MEMORY,
		       "couldn't map physical memory");

  grub_dprintf ("loader", "Loading Linux at vaddr 0x%lx, paddr 0x%lx, size 0x%lx\n",
		linux_addr, paddr, linux_size);

  linux_paddr = paddr;

  base = linux_entry - off;

  /* Now load the segments into the area we claimed.  */
  return grub_elf64_load (elf, filename, (void *) (linux_addr - off - base), GRUB_ELF_LOAD_FLAGS_NONE, 0, 0);
}

static grub_err_t
grub_cmd_linux (grub_command_t cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
  grub_file_t file = 0;
  grub_elf_t elf = 0;
  int size;

  grub_dl_ref (my_mod);

  if (argc == 0)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
      goto out;
    }

  file = grub_file_open (argv[0], GRUB_FILE_TYPE_LINUX_KERNEL);
  if (!file)
    goto out;

  elf = grub_elf_file (file, argv[0]);
  if (! elf)
    goto out;

  if (elf->ehdr.ehdr32.e_type != ET_EXEC)
    {
      grub_error (GRUB_ERR_UNKNOWN_OS,
		  N_("this ELF file is not of the right type"));
      goto out;
    }

  /* Release the previously used memory.  */
  grub_loader_unset ();

  if (grub_elf_is_elf64 (elf))
    grub_linux_load64 (elf, argv[0]);
  else
    {
      grub_error (GRUB_ERR_BAD_FILE_TYPE, N_("invalid arch-dependent ELF magic"));
      goto out;
    }

  size = grub_loader_cmdline_size(argc, argv);

  linux_args = grub_malloc (size + sizeof (LINUX_IMAGE));
  if (! linux_args)
    goto out;

  /* Create kernel command line.  */
  grub_memcpy (linux_args, LINUX_IMAGE, sizeof (LINUX_IMAGE));
  if (grub_create_loader_cmdline (argc, argv, linux_args + sizeof (LINUX_IMAGE) - 1,
				  size, GRUB_VERIFY_KERNEL_CMDLINE))
    goto out;

out:
  if (elf)
    grub_elf_close (elf);
  else if (file)
    grub_file_close (file);

  if (grub_errno != GRUB_ERR_NONE)
    {
      grub_linux_release_mem ();
      grub_dl_unref (my_mod);
      loaded = 0;
    }
  else
    {
      grub_loader_set (grub_linux_boot, grub_linux_unload, 1);
      initrd_addr = 0;
      loaded = 1;
    }

  return grub_errno;
}

static grub_err_t
grub_cmd_initrd (grub_command_t cmd __attribute__ ((unused)),
		 int argc, char *argv[])
{
  grub_size_t size = 0;
  grub_addr_t paddr;
  grub_addr_t addr;
  int ret;
  struct grub_linux_initrd_context initrd_ctx = { 0, 0, 0 };

  if (argc == 0)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
      goto fail;
    }

  if (!loaded)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("you need to load the kernel first"));
      goto fail;
    }

  if (grub_initrd_init (argc, argv, &initrd_ctx))
    goto fail;

  size = grub_get_initrd_size (&initrd_ctx);

  addr = 0x60000000;

  paddr = alloc_phys (size);
  if (paddr == (grub_addr_t) -1)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY,
		  "couldn't allocate physical memory");
      goto fail;
    }
  ret = grub_ieee1275_map (paddr, addr, size, IEEE1275_MAP_DEFAULT);
  if (ret)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY,
		  "couldn't map physical memory");
      goto fail;
    }

  grub_dprintf ("loader", "Loading initrd at vaddr 0x%lx, paddr 0x%lx, size 0x%lx\n",
		addr, paddr, size);

  if (grub_initrd_load (&initrd_ctx, (void *) addr))
    goto fail;

  initrd_addr = addr;
  initrd_paddr = paddr;
  initrd_size = size;

 fail:
  grub_initrd_close (&initrd_ctx);

  return grub_errno;
}

/* Helper for determine_phys_base.  */
static int
get_physbase (grub_uint64_t addr, grub_uint64_t len __attribute__ ((unused)),
	      grub_memory_type_t type, void *data __attribute__ ((unused)))
{
  if (type != GRUB_MEMORY_AVAILABLE)
    return 0;
  if (addr < phys_base)
    phys_base = addr;
  return 0;
}

static void
determine_phys_base (void)
{
  phys_base = ~(grub_uint64_t) 0;
  grub_machine_mmap_iterate (get_physbase, NULL);
}

static void
fetch_translations (void)
{
  grub_ieee1275_phandle_t node;
  grub_ssize_t actual;
  int i;

  if (grub_ieee1275_finddevice ("/virtual-memory", &node))
    {
      grub_printf ("Cannot find /virtual-memory node.\n");
      return;
    }

  if (grub_ieee1275_get_property_length (node, "translations", &actual))
    {
      grub_printf ("Cannot find /virtual-memory/translations size.\n");
      return;
    }

  of_trans = grub_malloc (actual);
  if (!of_trans)
    {
      grub_printf ("Cannot allocate translations buffer.\n");
      return;
    }

  if (grub_ieee1275_get_property (node, "translations", of_trans, actual, &actual))
    {
      grub_printf ("Cannot fetch /virtual-memory/translations property.\n");
      return;
    }

  of_num_trans = actual / sizeof(struct grub_ieee1275_translation);

  for (i = 0; i < of_num_trans; i++)
    {
      struct grub_ieee1275_translation *p = &of_trans[i];

      if (p->vaddr == 0x2000)
	{
	  grub_addr_t phys, tte = p->data;

	  phys = tte & ~(0xff00000000001fffULL);

	  grub_phys_start = phys;
	  grub_phys_end = grub_phys_start + p->size;
	  grub_dprintf ("loader", "Grub lives at phys_start[%lx] phys_end[%lx]\n",
			(unsigned long) grub_phys_start,
			(unsigned long) grub_phys_end);
	  break;
	}
    }
}


static grub_command_t cmd_linux, cmd_initrd;

GRUB_MOD_INIT(linux)
{
  determine_phys_base ();
  fetch_translations ();

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
