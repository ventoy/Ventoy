/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008,2010  Free Software Foundation, Inc.
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

#include <grub/loader.h>
#include <grub/file.h>
#include <grub/disk.h>
#include <grub/err.h>
#include <grub/misc.h>
#include <grub/types.h>
#include <grub/command.h>
#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/cache.h>
#include <grub/kernel.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/elf.h>
#include <grub/i18n.h>
#include <grub/env.h>
#include <grub/linux.h>
#include <grub/verify.h>

GRUB_MOD_LICENSE ("GPLv3+");

#pragma GCC diagnostic ignored "-Wcast-align"

#define ALIGN_MIN (256*1024*1024)

#define GRUB_ELF_SEARCH 1024

#define BOOT_PARAM_SIZE	16384

struct ia64_boot_param
{
  grub_uint64_t command_line;	/* physical address of command line. */
  grub_uint64_t efi_systab;	/* physical address of EFI system table */
  grub_uint64_t efi_memmap;	/* physical address of EFI memory map */
  grub_uint64_t efi_memmap_size;	/* size of EFI memory map */
  grub_uint64_t efi_memdesc_size; /* size of an EFI memory map descriptor */
  grub_uint32_t efi_memdesc_version;	/* memory descriptor version */
  struct
  {
    grub_uint16_t num_cols;	/* number of columns on console output dev */
    grub_uint16_t num_rows;	/* number of rows on console output device */
    grub_uint16_t orig_x;	/* cursor's x position */
    grub_uint16_t orig_y;	/* cursor's y position */
  } console_info;
  grub_uint64_t fpswa;		/* physical address of the fpswa interface */
  grub_uint64_t initrd_start;
  grub_uint64_t initrd_size;
};

typedef struct
{
  grub_uint32_t	revision;
  grub_uint32_t	reserved;
  void *fpswa;
} fpswa_interface_t;
static fpswa_interface_t *fpswa;

#define NEXT_MEMORY_DESCRIPTOR(desc, size)      \
  ((grub_efi_memory_descriptor_t *) ((char *) (desc) + (size)))

static grub_dl_t my_mod;

static int loaded;

/* Kernel base and size.  */
static void *kernel_mem;
static grub_efi_uintn_t kernel_pages;
static grub_uint64_t entry;

/* Initrd base and size.  */
static void *initrd_mem;
static grub_efi_uintn_t initrd_pages;
static grub_efi_uintn_t initrd_size;

static struct ia64_boot_param *boot_param;
static grub_efi_uintn_t boot_param_pages;

static inline grub_size_t
page_align (grub_size_t size)
{
  return (size + (1 << 12) - 1) & (~((1 << 12) - 1));
}

static void
query_fpswa (void)
{
  grub_efi_handle_t fpswa_image;
  grub_efi_boot_services_t *bs;
  grub_efi_status_t status;
  grub_efi_uintn_t size;
  static const grub_efi_guid_t fpswa_protocol = 
    { 0xc41b6531, 0x97b9, 0x11d3,
      {0x9a, 0x29, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d} };

  if (fpswa != NULL)
    return;

  size = sizeof(grub_efi_handle_t);
  
  bs = grub_efi_system_table->boot_services;
  status = bs->locate_handle (GRUB_EFI_BY_PROTOCOL,
			      (void *) &fpswa_protocol,
			      NULL, &size, &fpswa_image);
  if (status != GRUB_EFI_SUCCESS)
    {
      grub_printf ("%s\n", _("Could not locate FPSWA driver"));
      return;
    }
  status = bs->handle_protocol (fpswa_image,
				(void *) &fpswa_protocol, (void *) &fpswa);
  if (status != GRUB_EFI_SUCCESS)
    {
      grub_printf ("%s\n",
		   _("FPSWA protocol wasn't able to find the interface"));
      return;
    } 
}

static void
free_pages (void)
{
  if (kernel_mem)
    {
      grub_efi_free_pages ((grub_addr_t) kernel_mem, kernel_pages);
      kernel_mem = 0;
    }

  if (initrd_mem)
    {
      grub_efi_free_pages ((grub_addr_t) initrd_mem, initrd_pages);
      initrd_mem = 0;
    }

  if (boot_param)
    {
      /* Free bootparam.  */
      grub_efi_free_pages ((grub_efi_physical_address_t) boot_param,
			   boot_param_pages);
      boot_param = 0;
    }
}

static void *
allocate_pages (grub_uint64_t align, grub_uint64_t size_pages,
		grub_uint64_t nobase)
{
  grub_uint64_t size;
  grub_efi_uintn_t desc_size;
  grub_efi_memory_descriptor_t *mmap, *mmap_end;
  grub_efi_uintn_t mmap_size, tmp_mmap_size;
  grub_efi_memory_descriptor_t *desc;
  void *mem = NULL;

  size = size_pages << 12;

  mmap_size = grub_efi_find_mmap_size ();
  if (!mmap_size)
    return 0;

    /* Read the memory map temporarily, to find free space.  */
  mmap = grub_malloc (mmap_size);
  if (! mmap)
    return 0;

  tmp_mmap_size = mmap_size;
  if (grub_efi_get_memory_map (&tmp_mmap_size, mmap, 0, &desc_size, 0) <= 0)
    {
      grub_error (GRUB_ERR_IO, "cannot get memory map");
      goto fail;
    }

  mmap_end = NEXT_MEMORY_DESCRIPTOR (mmap, tmp_mmap_size);
  
  /* First, find free pages for the real mode code
     and the memory map buffer.  */
  for (desc = mmap;
       desc < mmap_end;
       desc = NEXT_MEMORY_DESCRIPTOR (desc, desc_size))
    {
      grub_uint64_t start, end;
      grub_uint64_t aligned_start;

      if (desc->type != GRUB_EFI_CONVENTIONAL_MEMORY)
	continue;

      start = desc->physical_start;
      end = start + (desc->num_pages << 12);
      /* Align is a power of 2.  */
      aligned_start = (start + align - 1) & ~(align - 1);
      if (aligned_start + size > end)
	continue;
      if (aligned_start == nobase)
	aligned_start += align;
      if (aligned_start + size > end)
	continue;
      mem = grub_efi_allocate_fixed (aligned_start, size_pages);
      if (! mem)
	{
	  grub_error (GRUB_ERR_OUT_OF_MEMORY, "cannot allocate memory");
	  goto fail;
	}
      break;
    }

  if (! mem)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, "cannot allocate memory");
      goto fail;
    }

  grub_free (mmap);
  return mem;

 fail:
  grub_free (mmap);
  free_pages ();
  return 0;
}

static void
set_boot_param_console (void)
{
  grub_efi_simple_text_output_interface_t *conout;
  grub_efi_uintn_t cols, rows;
  
  conout = grub_efi_system_table->con_out;
  if (conout->query_mode (conout, conout->mode->mode, &cols, &rows)
      != GRUB_EFI_SUCCESS)
    return;

  grub_dprintf ("linux",
		"Console info: cols=%lu rows=%lu x=%u y=%u\n",
		cols, rows,
		conout->mode->cursor_column, conout->mode->cursor_row);
  
  boot_param->console_info.num_cols = cols;
  boot_param->console_info.num_rows = rows;
  boot_param->console_info.orig_x = conout->mode->cursor_column;
  boot_param->console_info.orig_y = conout->mode->cursor_row;
}

static grub_err_t
grub_linux_boot (void)
{
  grub_efi_uintn_t mmap_size;
  grub_efi_uintn_t map_key;
  grub_efi_uintn_t desc_size;
  grub_efi_uint32_t desc_version;
  grub_efi_memory_descriptor_t *mmap_buf;
  grub_err_t err;

  /* FPSWA.  */
  query_fpswa ();
  boot_param->fpswa = (grub_uint64_t)fpswa;

  /* Initrd.  */
  boot_param->initrd_start = (grub_uint64_t)initrd_mem;
  boot_param->initrd_size = (grub_uint64_t)initrd_size;

  set_boot_param_console ();

  grub_dprintf ("linux", "Jump to %016lx\n", entry);

  /* MDT.
     Must be done after grub_machine_fini because map_key is used by
     exit_boot_services.  */
  mmap_size = grub_efi_find_mmap_size ();
  if (! mmap_size)
    return grub_errno;
  mmap_buf = grub_efi_allocate_any_pages (page_align (mmap_size) >> 12);
  if (! mmap_buf)
    return grub_error (GRUB_ERR_IO, "cannot allocate memory map");
  err = grub_efi_finish_boot_services (&mmap_size, mmap_buf, &map_key,
				       &desc_size, &desc_version);
  if (err)
    return err;

  boot_param->efi_memmap = (grub_uint64_t)mmap_buf;
  boot_param->efi_memmap_size = mmap_size;
  boot_param->efi_memdesc_size = desc_size;
  boot_param->efi_memdesc_version = desc_version;

  /* See you next boot.  */
  asm volatile ("mov r28=%1; br.sptk.few %0" :: "b"(entry),"r"(boot_param));
  
  /* Never reach here.  */
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_linux_unload (void)
{
  free_pages ();
  grub_dl_unref (my_mod);
  loaded = 0;
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_load_elf64 (grub_file_t file, void *buffer, const char *filename)
{
  Elf64_Ehdr *ehdr = (Elf64_Ehdr *) buffer;
  Elf64_Phdr *phdr;
  int i;
  grub_uint64_t low_addr;
  grub_uint64_t high_addr;
  grub_uint64_t align;
  grub_uint64_t reloc_offset;
  const char *relocate;

  if (ehdr->e_ident[EI_MAG0] != ELFMAG0
      || ehdr->e_ident[EI_MAG1] != ELFMAG1
      || ehdr->e_ident[EI_MAG2] != ELFMAG2
      || ehdr->e_ident[EI_MAG3] != ELFMAG3
      || ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
    return grub_error(GRUB_ERR_UNKNOWN_OS,
		      N_("invalid arch-independent ELF magic"));

  if (ehdr->e_ident[EI_CLASS] != ELFCLASS64
      || ehdr->e_version != EV_CURRENT
      || ehdr->e_machine != EM_IA_64)
    return grub_error (GRUB_ERR_UNKNOWN_OS,
		       N_("invalid arch-dependent ELF magic"));

  if (ehdr->e_type != ET_EXEC)
    return grub_error (GRUB_ERR_UNKNOWN_OS,
		       N_("this ELF file is not of the right type"));

  /* FIXME: Should we support program headers at strange locations?  */
  if (ehdr->e_phoff + ehdr->e_phnum * ehdr->e_phentsize > GRUB_ELF_SEARCH)
    return grub_error (GRUB_ERR_BAD_OS, "program header at a too high offset");

  entry = ehdr->e_entry;

  /* Compute low, high and align addresses.  */
  low_addr = ~0UL;
  high_addr = 0;
  align = 0;
  for (i = 0; i < ehdr->e_phnum; i++)
    {
      phdr = (Elf64_Phdr *) ((char *) buffer + ehdr->e_phoff
			     + i * ehdr->e_phentsize);
      if (phdr->p_type == PT_LOAD)
	{
	  if (phdr->p_paddr < low_addr)
	    low_addr = phdr->p_paddr;
	  if (phdr->p_paddr + phdr->p_memsz > high_addr)
	    high_addr = phdr->p_paddr + phdr->p_memsz;
	  if (phdr->p_align > align)
	    align = phdr->p_align;
	}
    }

  if (align < ALIGN_MIN)
    align = ALIGN_MIN;

  if (high_addr == 0)
    return grub_error (GRUB_ERR_BAD_OS, "no program entries");

  kernel_pages = page_align (high_addr - low_addr) >> 12;

  /* Undocumented on purpose.  */
  relocate = grub_env_get ("linux_relocate");
  if (!relocate || grub_strcmp (relocate, "force") != 0)
    {
      kernel_mem = grub_efi_allocate_fixed (low_addr, kernel_pages);
      reloc_offset = 0;
    }
  /* Try to relocate.  */
  if (! kernel_mem && (!relocate || grub_strcmp (relocate, "off") != 0))
    {
      kernel_mem = allocate_pages (align, kernel_pages, low_addr);
      if (kernel_mem)
	{
	  reloc_offset = (grub_uint64_t)kernel_mem - low_addr;
	  grub_dprintf ("linux", "  Relocated at %p (offset=%016lx)\n",
			kernel_mem, reloc_offset);
	  entry += reloc_offset;
	}
    }
  if (! kernel_mem)
    return grub_error (GRUB_ERR_OUT_OF_MEMORY,
		       "cannot allocate memory for OS");

  /* Load every loadable segment in memory.  */
  for (i = 0; i < ehdr->e_phnum; i++)
    {
      phdr = (Elf64_Phdr *) ((char *) buffer + ehdr->e_phoff
			     + i * ehdr->e_phentsize);
      if (phdr->p_type == PT_LOAD)
        {
	  grub_dprintf ("linux", "  [paddr=%lx load=%lx memsz=%08lx "
			"off=%lx flags=%x]\n",
			phdr->p_paddr, phdr->p_paddr + reloc_offset,
			phdr->p_memsz, phdr->p_offset, phdr->p_flags);
	  
	  if (grub_file_seek (file, phdr->p_offset) == (grub_off_t)-1)
	    return grub_errno;

	  if (grub_file_read (file, (void *) (phdr->p_paddr + reloc_offset),
			      phdr->p_filesz)
              != (grub_ssize_t) phdr->p_filesz)
	    {
	      if (!grub_errno)
		grub_error (GRUB_ERR_BAD_OS, N_("premature end of file %s"),
			    filename);
	      return grub_errno;
	    }
	  
          if (phdr->p_filesz < phdr->p_memsz)
	    grub_memset
	      ((char *)(phdr->p_paddr + reloc_offset + phdr->p_filesz),
	       0, phdr->p_memsz - phdr->p_filesz);

	  /* Sync caches if necessary.  */
	  if (phdr->p_flags & PF_X)
	    grub_arch_sync_caches
	      ((void *)(phdr->p_paddr + reloc_offset), phdr->p_memsz);
        }
    }
  loaded = 1;
  return 0;
}

static grub_err_t
grub_cmd_linux (grub_command_t cmd __attribute__ ((unused)),
		int argc, char *argv[])
{
  grub_file_t file = 0;
  char buffer[GRUB_ELF_SEARCH];
  char *cmdline, *p;
  grub_ssize_t len;
  int i;

  grub_dl_ref (my_mod);

  grub_loader_unset ();
    
  if (argc == 0)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
      goto fail;
    }

  file = grub_file_open (argv[0], GRUB_FILE_TYPE_LINUX_KERNEL);
  if (! file)
    goto fail;

  len = grub_file_read (file, buffer, sizeof (buffer));
  if (len < (grub_ssize_t) sizeof (Elf64_Ehdr))
    {
      if (!grub_errno)
	grub_error (GRUB_ERR_BAD_OS, N_("premature end of file %s"),
		    argv[0]);
      goto fail;
    }

  grub_dprintf ("linux", "Loading linux: %s\n", argv[0]);

  if (grub_load_elf64 (file, buffer, argv[0]))
    goto fail;

  len = sizeof("BOOT_IMAGE=") + 8;
  for (i = 0; i < argc; i++)
    len += grub_strlen (argv[i]) + 1;
  len += sizeof (struct ia64_boot_param) + 512; /* Room for extensions.  */
  boot_param_pages = page_align (len) >> 12;
  boot_param = grub_efi_allocate_any_pages (boot_param_pages);
  if (boot_param == 0)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY,
		  "cannot allocate memory for bootparams");
      goto fail;
    }

  grub_memset (boot_param, 0, len);
  cmdline = ((char *)(boot_param + 1)) + 256;

  /* Build cmdline.  */
  p = grub_stpcpy (cmdline, "BOOT_IMAGE");
  for (i = 0; i < argc; i++)
    {
      *p++ = ' ';
      p = grub_stpcpy (p, argv[i]);
    }
  cmdline[10] = '=';

  *p = '\0';

  if (grub_verify_string (cmdline, GRUB_VERIFY_KERNEL_CMDLINE))
    goto fail;
  
  boot_param->command_line = (grub_uint64_t) cmdline;
  boot_param->efi_systab = (grub_uint64_t) grub_efi_system_table;

  grub_errno = GRUB_ERR_NONE;

  grub_loader_set (grub_linux_boot, grub_linux_unload, 0);

 fail:
  if (file)
    grub_file_close (file);

  if (grub_errno != GRUB_ERR_NONE)
    {
      grub_efi_free_pages ((grub_efi_physical_address_t) boot_param,
			   boot_param_pages);
      grub_dl_unref (my_mod);
    }
  return grub_errno;
}

static grub_err_t
grub_cmd_initrd (grub_command_t cmd __attribute__ ((unused)),
		 int argc, char *argv[])
{
  struct grub_linux_initrd_context initrd_ctx = { 0, 0, 0 };

  if (argc == 0)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
      goto fail;
    }
  
  if (! loaded)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("you need to load the kernel first"));
      goto fail;
    }

  if (grub_initrd_init (argc, argv, &initrd_ctx))
    goto fail;

  initrd_size = grub_get_initrd_size (&initrd_ctx);
  grub_dprintf ("linux", "Loading initrd\n");

  initrd_pages = (page_align (initrd_size) >> 12);
  initrd_mem = grub_efi_allocate_any_pages (initrd_pages);
  if (! initrd_mem)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, "cannot allocate pages");
      goto fail;
    }
  
  grub_dprintf ("linux", "[addr=0x%lx, size=0x%lx]\n",
		(grub_uint64_t) initrd_mem, initrd_size);

  if (grub_initrd_load (&initrd_ctx, argv, initrd_mem))
    goto fail;
 fail:
  grub_initrd_close (&initrd_ctx);
  return grub_errno;
}

static grub_err_t
grub_cmd_fpswa (grub_command_t cmd __attribute__ ((unused)),
		int argc __attribute__((unused)),
		char *argv[] __attribute__((unused)))
{
  query_fpswa ();
  if (fpswa == NULL)
    grub_puts_ (N_("No FPSWA found"));
  else
    grub_printf (_("FPSWA revision: %x\n"), fpswa->revision);
  return GRUB_ERR_NONE;
}

static grub_command_t cmd_linux, cmd_initrd, cmd_fpswa;

GRUB_MOD_INIT(linux)
{
  cmd_linux = grub_register_command ("linux", grub_cmd_linux,
				     N_("FILE [ARGS...]"), N_("Load Linux."));
  
  cmd_initrd = grub_register_command ("initrd", grub_cmd_initrd,
				      N_("FILE"), N_("Load initrd."));

  cmd_fpswa = grub_register_command ("fpswa", grub_cmd_fpswa,
				     "", N_("Display FPSWA version."));

  my_mod = mod;
}

GRUB_MOD_FINI(linux)
{
  grub_unregister_command (cmd_linux);
  grub_unregister_command (cmd_initrd);
  grub_unregister_command (cmd_fpswa);
}
