/* grub-mkimage.c - make a bootable image */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2003,2004,2005,2006,2007,2008,2009,2010  Free Software Foundation, Inc.
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

#include <config.h>
#include <grub/types.h>
#include <grub/elf.h>
#include <grub/aout.h>
#include <grub/i18n.h>
#include <grub/kernel.h>
#include <grub/disk.h>
#include <grub/emu/misc.h>
#include <grub/util/misc.h>
#include <grub/util/resolve.h>
#include <grub/misc.h>
#include <grub/offsets.h>
#include <grub/crypto.h>
#include <grub/dl.h>
#include <time.h>
#include <multiboot.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <grub/efi/pe32.h>
#include <grub/uboot/image.h>
#include <grub/arm/reloc.h>
#include <grub/arm64/reloc.h>
#include <grub/ia64/reloc.h>
#include <grub/osdep/hostfile.h>
#include <grub/util/install.h>
#include <grub/util/mkimage.h>

#include <xen/elfnote.h>

#pragma GCC diagnostic ignored "-Wcast-align"

#define GRUB_MKIMAGEXX
#if !defined(MKIMAGE_ELF32) && !defined(MKIMAGE_ELF64)
#if __SIZEOF_POINTER__ == 8
#include "grub-mkimage64.c"
#else
#include "grub-mkimage32.c"
#endif
#endif

/* These structures are defined according to the CHRP binding to IEEE1275,
   "Client Program Format" section.  */

struct grub_ieee1275_note_desc
{
  grub_uint32_t real_mode;
  grub_uint32_t real_base;
  grub_uint32_t real_size;
  grub_uint32_t virt_base;
  grub_uint32_t virt_size;
  grub_uint32_t load_base;
};

#define GRUB_IEEE1275_NOTE_NAME "PowerPC"
#define GRUB_IEEE1275_NOTE_TYPE 0x1275

struct grub_ieee1275_note
{
  Elf32_Nhdr header;
  char name[ALIGN_UP(sizeof (GRUB_IEEE1275_NOTE_NAME), 4)];
  struct grub_ieee1275_note_desc descriptor;
};

#define GRUB_XEN_NOTE_NAME "Xen"

struct fixup_block_list
{
  struct fixup_block_list *next;
  int state;
  struct grub_pe32_fixup_block b;
};

#define ALIGN_ADDR(x) (ALIGN_UP((x), image_target->voidp_sizeof))

struct section_metadata
{
  Elf_Half num_sections;
  Elf_Shdr *sections;
  Elf_Addr *addrs;
  Elf_Addr *vaddrs;
  Elf_Half section_entsize;
  Elf_Shdr *symtab;
  const char *strtab;
};

static int
is_relocatable (const struct grub_install_image_target_desc *image_target)
{
  return image_target->id == IMAGE_EFI || image_target->id == IMAGE_UBOOT
    || (image_target->id == IMAGE_COREBOOT && image_target->elf_target == EM_ARM);
}

#ifdef MKIMAGE_ELF32

/*
 * R_ARM_THM_CALL/THM_JUMP24
 *
 * Relocate Thumb (T32) instruction set relative branches:
 *   B.W, BL and BLX
 */
static grub_err_t
grub_arm_reloc_thm_call (grub_uint16_t *target, Elf32_Addr sym_addr)
{
  grub_int32_t offset;

  offset = grub_arm_thm_call_get_offset (target);

  grub_dprintf ("dl", "    sym_addr = 0x%08x", sym_addr);

  offset += sym_addr;

  grub_dprintf("dl", " BL*: target=%p, sym_addr=0x%08x, offset=%d\n",
	       target, sym_addr, offset);

  /* Keep traditional (pre-Thumb2) limits on blx. In any case if the kernel
     is bigger than 2M  (currently under 150K) then we probably have a problem
     somewhere else.  */
  if (offset < -0x200000 || offset >= 0x200000)
    return grub_error (GRUB_ERR_BAD_MODULE,
		       "THM_CALL Relocation out of range.");

  grub_dprintf ("dl", "    relative destination = %p",
		(char *) target + offset);

  return grub_arm_thm_call_set_offset (target, offset);
}

/*
 * R_ARM_THM_JUMP19
 *
 * Relocate conditional Thumb (T32) B<c>.W
 */
static grub_err_t
grub_arm_reloc_thm_jump19 (grub_uint16_t *target, Elf32_Addr sym_addr)
{
  grub_int32_t offset;

  if (!(sym_addr & 1))
    return grub_error (GRUB_ERR_BAD_MODULE,
		       "Relocation targeting wrong execution state");

  offset = grub_arm_thm_jump19_get_offset (target);

  /* Adjust and re-truncate offset */
  offset += sym_addr;

  if (!grub_arm_thm_jump19_check_offset (offset))
    return grub_error (GRUB_ERR_BAD_MODULE,
		       "THM_JUMP19 Relocation out of range.");

  grub_arm_thm_jump19_set_offset (target, offset);

  return GRUB_ERR_NONE;
}

/*
 * R_ARM_JUMP24
 *
 * Relocate ARM (A32) B
 */
static grub_err_t
grub_arm_reloc_jump24 (grub_uint32_t *target, Elf32_Addr sym_addr)
{
  grub_int32_t offset;

  if (sym_addr & 1)
    return grub_error (GRUB_ERR_BAD_MODULE,
		       "Relocation targeting wrong execution state");

  offset = grub_arm_jump24_get_offset (target);
  offset += sym_addr;

  if (!grub_arm_jump24_check_offset (offset))
    return grub_error (GRUB_ERR_BAD_MODULE,
		       "JUMP24 Relocation out of range.");


  grub_arm_jump24_set_offset (target, offset);

  return GRUB_ERR_NONE;
}

#endif

void
SUFFIX (grub_mkimage_generate_elf) (const struct grub_install_image_target_desc *image_target,
				    int note, char **core_img, size_t *core_size,
				    Elf_Addr target_addr,
				    struct grub_mkimage_layout *layout)
{
  char *elf_img;
  size_t program_size;
  Elf_Ehdr *ehdr;
  Elf_Phdr *phdr;
  Elf_Shdr *shdr;
  int header_size, footer_size = 0;
  int phnum = 1;
  int shnum = 4;
  int string_size = sizeof (".text") + sizeof ("mods") + 1;

  if (image_target->id != IMAGE_LOONGSON_ELF)
    phnum += 2;

  if (note)
    {
      phnum++;
      footer_size += sizeof (struct grub_ieee1275_note);
    }
  if (image_target->id == IMAGE_XEN || image_target->id == IMAGE_XEN_PVH)
    {
      phnum++;
      shnum++;
      string_size += sizeof (".xen");
      footer_size += (image_target->id == IMAGE_XEN) ? XEN_NOTE_SIZE : XEN_PVH_NOTE_SIZE;
    }
  header_size = ALIGN_UP (sizeof (*ehdr) + phnum * sizeof (*phdr)
			  + shnum * sizeof (*shdr) + string_size, layout->align);

  program_size = ALIGN_ADDR (*core_size);

  elf_img = xmalloc (program_size + header_size + footer_size);
  memset (elf_img, 0, program_size + header_size + footer_size);
  memcpy (elf_img  + header_size, *core_img, *core_size);
  ehdr = (void *) elf_img;
  phdr = (void *) (elf_img + sizeof (*ehdr));
  shdr = (void *) (elf_img + sizeof (*ehdr) + phnum * sizeof (*phdr));
  memcpy (ehdr->e_ident, ELFMAG, SELFMAG);
  ehdr->e_ident[EI_CLASS] = ELFCLASSXX;
  if (!image_target->bigendian)
    ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
  else
    ehdr->e_ident[EI_DATA] = ELFDATA2MSB;
  ehdr->e_ident[EI_VERSION] = EV_CURRENT;
  ehdr->e_ident[EI_OSABI] = ELFOSABI_NONE;
  ehdr->e_type = grub_host_to_target16 (ET_EXEC);
  ehdr->e_machine = grub_host_to_target16 (image_target->elf_target);
  ehdr->e_version = grub_host_to_target32 (EV_CURRENT);

  ehdr->e_phoff = grub_host_to_target32 ((char *) phdr - (char *) ehdr);
  ehdr->e_phentsize = grub_host_to_target16 (sizeof (*phdr));
  ehdr->e_phnum = grub_host_to_target16 (phnum);

  ehdr->e_shoff = grub_host_to_target32 ((grub_uint8_t *) shdr
					 - (grub_uint8_t *) ehdr);
  if (image_target->id == IMAGE_LOONGSON_ELF)
    ehdr->e_shentsize = grub_host_to_target16 (0);
  else
    ehdr->e_shentsize = grub_host_to_target16 (sizeof (Elf_Shdr));
  ehdr->e_shnum = grub_host_to_target16 (shnum);
  ehdr->e_shstrndx = grub_host_to_target16 (1);

  ehdr->e_ehsize = grub_host_to_target16 (sizeof (*ehdr));

  phdr->p_type = grub_host_to_target32 (PT_LOAD);
  phdr->p_offset = grub_host_to_target32 (header_size);
  phdr->p_flags = grub_host_to_target32 (PF_R | PF_W | PF_X);

  ehdr->e_entry = grub_host_to_target32 (target_addr);
  phdr->p_vaddr = grub_host_to_target32 (target_addr);
  phdr->p_paddr = grub_host_to_target32 (target_addr);
  phdr->p_align = grub_host_to_target32 (layout->align > image_target->link_align ?
					 layout->align : image_target->link_align);
  if (image_target->id == IMAGE_LOONGSON_ELF)
    ehdr->e_flags = grub_host_to_target32 (0x1000 | EF_MIPS_NOREORDER 
					   | EF_MIPS_PIC | EF_MIPS_CPIC);
  else
    ehdr->e_flags = 0;
  if (image_target->id == IMAGE_LOONGSON_ELF)
    {
      phdr->p_filesz = grub_host_to_target32 (*core_size);
      phdr->p_memsz = grub_host_to_target32 (*core_size);
    }
  else
    {
      grub_uint32_t target_addr_mods;
      phdr->p_filesz = grub_host_to_target32 (layout->kernel_size);
      if (image_target->id == IMAGE_COREBOOT && image_target->elf_target == EM_ARM)
	phdr->p_memsz = grub_host_to_target32 (layout->kernel_size);
      else
	phdr->p_memsz = grub_host_to_target32 (layout->kernel_size + layout->bss_size);

      phdr++;
      phdr->p_type = grub_host_to_target32 (PT_GNU_STACK);
      phdr->p_offset = grub_host_to_target32 (header_size + layout->kernel_size);
      phdr->p_paddr = phdr->p_vaddr = phdr->p_filesz = phdr->p_memsz = 0;
      phdr->p_flags = grub_host_to_target32 (PF_R | PF_W | PF_X);
      phdr->p_align = grub_host_to_target32 (image_target->link_align);

      phdr++;
      phdr->p_type = grub_host_to_target32 (PT_LOAD);
      phdr->p_offset = grub_host_to_target32 (header_size + layout->kernel_size);
      phdr->p_flags = grub_host_to_target32 (PF_R | PF_W | PF_X);
      phdr->p_filesz = phdr->p_memsz
	= grub_host_to_target32 (*core_size - layout->kernel_size);

      if (image_target->id == IMAGE_COREBOOT && image_target->elf_target == EM_386)
	target_addr_mods = GRUB_KERNEL_I386_COREBOOT_MODULES_ADDR;
      else if (image_target->id == IMAGE_COREBOOT && image_target->elf_target == EM_ARM)
	target_addr_mods = ALIGN_UP (target_addr + layout->end
				     + image_target->mod_gap,
				     image_target->mod_align);
      else
	target_addr_mods = ALIGN_UP (target_addr + layout->kernel_size + layout->bss_size
				     + image_target->mod_gap,
				     image_target->mod_align);
      phdr->p_vaddr = grub_host_to_target_addr (target_addr_mods);
      phdr->p_paddr = grub_host_to_target_addr (target_addr_mods);
      phdr->p_align = grub_host_to_target32 (image_target->link_align);
    }

  if (image_target->id == IMAGE_XEN)
    {
      char *note_start = (elf_img + program_size + header_size);
      Elf_Nhdr *note_ptr;
      char *ptr = (char *) note_start;

      grub_util_info ("adding XEN NOTE segment");

      /* Guest OS.  */
      note_ptr = (Elf_Nhdr *) ptr;
      note_ptr->n_namesz = grub_host_to_target32 (sizeof (GRUB_XEN_NOTE_NAME));
      note_ptr->n_descsz = grub_host_to_target32 (sizeof (PACKAGE_NAME));
      note_ptr->n_type = grub_host_to_target32 (XEN_ELFNOTE_GUEST_OS);
      ptr += sizeof (Elf_Nhdr);
      memcpy (ptr, GRUB_XEN_NOTE_NAME, sizeof (GRUB_XEN_NOTE_NAME));
      ptr += ALIGN_UP (sizeof (GRUB_XEN_NOTE_NAME), 4);
      memcpy (ptr, PACKAGE_NAME, sizeof (PACKAGE_NAME));
      ptr += ALIGN_UP (sizeof (PACKAGE_NAME), 4);

      /* Loader.  */
      note_ptr = (Elf_Nhdr *) ptr;
      note_ptr->n_namesz = grub_host_to_target32 (sizeof (GRUB_XEN_NOTE_NAME));
      note_ptr->n_descsz = grub_host_to_target32 (sizeof ("generic"));
      note_ptr->n_type = grub_host_to_target32 (XEN_ELFNOTE_LOADER);
      ptr += sizeof (Elf_Nhdr);
      memcpy (ptr, GRUB_XEN_NOTE_NAME, sizeof (GRUB_XEN_NOTE_NAME));
      ptr += ALIGN_UP (sizeof (GRUB_XEN_NOTE_NAME), 4);
      memcpy (ptr, "generic", sizeof ("generic"));
      ptr += ALIGN_UP (sizeof ("generic"), 4);

      /* Version.  */
      note_ptr = (Elf_Nhdr *) ptr;
      note_ptr->n_namesz = grub_host_to_target32 (sizeof (GRUB_XEN_NOTE_NAME));
      note_ptr->n_descsz = grub_host_to_target32 (sizeof ("xen-3.0"));
      note_ptr->n_type = grub_host_to_target32 (XEN_ELFNOTE_XEN_VERSION);
      ptr += sizeof (Elf_Nhdr);
      memcpy (ptr, GRUB_XEN_NOTE_NAME, sizeof (GRUB_XEN_NOTE_NAME));
      ptr += ALIGN_UP (sizeof (GRUB_XEN_NOTE_NAME), 4);
      memcpy (ptr, "xen-3.0", sizeof ("xen-3.0"));
      ptr += ALIGN_UP (sizeof ("xen-3.0"), 4);

      /* Entry.  */
      note_ptr = (Elf_Nhdr *) ptr;
      note_ptr->n_namesz = grub_host_to_target32 (sizeof (GRUB_XEN_NOTE_NAME));
      note_ptr->n_descsz = grub_host_to_target32 (image_target->voidp_sizeof);
      note_ptr->n_type = grub_host_to_target32 (XEN_ELFNOTE_ENTRY);
      ptr += sizeof (Elf_Nhdr);
      memcpy (ptr, GRUB_XEN_NOTE_NAME, sizeof (GRUB_XEN_NOTE_NAME));
      ptr += ALIGN_UP (sizeof (GRUB_XEN_NOTE_NAME), 4);
      memset (ptr, 0, image_target->voidp_sizeof);
      ptr += image_target->voidp_sizeof;

      /* Virt base.  */
      note_ptr = (Elf_Nhdr *) ptr;
      note_ptr->n_namesz = grub_host_to_target32 (sizeof (GRUB_XEN_NOTE_NAME));
      note_ptr->n_descsz = grub_host_to_target32 (image_target->voidp_sizeof);
      note_ptr->n_type = grub_host_to_target32 (XEN_ELFNOTE_VIRT_BASE);
      ptr += sizeof (Elf_Nhdr);
      memcpy (ptr, GRUB_XEN_NOTE_NAME, sizeof (GRUB_XEN_NOTE_NAME));
      ptr += ALIGN_UP (sizeof (GRUB_XEN_NOTE_NAME), 4);
      memset (ptr, 0, image_target->voidp_sizeof);
      ptr += image_target->voidp_sizeof;

      /* PAE.  */
      if (image_target->elf_target == EM_386)
	{
	  note_ptr = (Elf_Nhdr *) ptr;
	  note_ptr->n_namesz = grub_host_to_target32 (sizeof (GRUB_XEN_NOTE_NAME));
	  note_ptr->n_descsz = grub_host_to_target32 (sizeof ("yes,bimodal"));
	  note_ptr->n_type = grub_host_to_target32 (XEN_ELFNOTE_PAE_MODE);
	  ptr += sizeof (Elf_Nhdr);
	  memcpy (ptr, GRUB_XEN_NOTE_NAME, sizeof (GRUB_XEN_NOTE_NAME));
	  ptr += ALIGN_UP (sizeof (GRUB_XEN_NOTE_NAME), 4);
	  memcpy (ptr, "yes", sizeof ("yes"));
	  ptr += ALIGN_UP (sizeof ("yes"), 4);
	}

      assert (XEN_NOTE_SIZE == (ptr - note_start));

      phdr++;
      phdr->p_type = grub_host_to_target32 (PT_NOTE);
      phdr->p_flags = grub_host_to_target32 (PF_R);
      phdr->p_align = grub_host_to_target32 (image_target->voidp_sizeof);
      phdr->p_vaddr = 0;
      phdr->p_paddr = 0;
      phdr->p_filesz = grub_host_to_target32 (XEN_NOTE_SIZE);
      phdr->p_memsz = 0;
      phdr->p_offset = grub_host_to_target32 (header_size + program_size);
    }

  if (image_target->id == IMAGE_XEN_PVH)
    {
      char *note_start = (elf_img + program_size + header_size);
      Elf_Nhdr *note_ptr;
      char *ptr = (char *) note_start;

      grub_util_info ("adding XEN NOTE segment");

      /* Phys32 Entry.  */
      note_ptr = (Elf_Nhdr *) ptr;
      note_ptr->n_namesz = grub_host_to_target32 (sizeof (GRUB_XEN_NOTE_NAME));
      note_ptr->n_descsz = grub_host_to_target32 (image_target->voidp_sizeof);
      note_ptr->n_type = grub_host_to_target32 (XEN_ELFNOTE_PHYS32_ENTRY);
      ptr += sizeof (Elf_Nhdr);
      memcpy (ptr, GRUB_XEN_NOTE_NAME, sizeof (GRUB_XEN_NOTE_NAME));
      ptr += ALIGN_UP (sizeof (GRUB_XEN_NOTE_NAME), 4);
      memset (ptr, 0, image_target->voidp_sizeof);
      *(grub_uint32_t *) ptr = GRUB_KERNEL_I386_XEN_PVH_LINK_ADDR;
      ptr += image_target->voidp_sizeof;

      assert (XEN_PVH_NOTE_SIZE == (ptr - note_start));

      phdr++;
      phdr->p_type = grub_host_to_target32 (PT_NOTE);
      phdr->p_flags = grub_host_to_target32 (PF_R);
      phdr->p_align = grub_host_to_target32 (image_target->voidp_sizeof);
      phdr->p_vaddr = 0;
      phdr->p_paddr = 0;
      phdr->p_filesz = grub_host_to_target32 (XEN_PVH_NOTE_SIZE);
      phdr->p_memsz = 0;
      phdr->p_offset = grub_host_to_target32 (header_size + program_size);
    }

  if (note)
    {
      int note_size = sizeof (struct grub_ieee1275_note);
      struct grub_ieee1275_note *note_ptr = (struct grub_ieee1275_note *) 
	(elf_img + program_size + header_size);

      grub_util_info ("adding CHRP NOTE segment");

      note_ptr->header.n_namesz = grub_host_to_target32 (sizeof (GRUB_IEEE1275_NOTE_NAME));
      note_ptr->header.n_descsz = grub_host_to_target32 (note_size);
      note_ptr->header.n_type = grub_host_to_target32 (GRUB_IEEE1275_NOTE_TYPE);
      strcpy (note_ptr->name, GRUB_IEEE1275_NOTE_NAME);
      note_ptr->descriptor.real_mode = grub_host_to_target32 (0xffffffff);
      note_ptr->descriptor.real_base = grub_host_to_target32 (0x00c00000);
      note_ptr->descriptor.real_size = grub_host_to_target32 (0xffffffff);
      note_ptr->descriptor.virt_base = grub_host_to_target32 (0xffffffff);
      note_ptr->descriptor.virt_size = grub_host_to_target32 (0xffffffff);
      note_ptr->descriptor.load_base = grub_host_to_target32 (0x00004000);

      phdr++;
      phdr->p_type = grub_host_to_target32 (PT_NOTE);
      phdr->p_flags = grub_host_to_target32 (PF_R);
      phdr->p_align = grub_host_to_target32 (image_target->voidp_sizeof);
      phdr->p_vaddr = 0;
      phdr->p_paddr = 0;
      phdr->p_filesz = grub_host_to_target32 (note_size);
      phdr->p_memsz = 0;
      phdr->p_offset = grub_host_to_target32 (header_size + program_size);
    }

  {
    char *str_start = (elf_img + sizeof (*ehdr) + phnum * sizeof (*phdr)
		       + shnum * sizeof (*shdr));
    char *ptr = str_start + 1;

    shdr++;

    shdr->sh_name = grub_host_to_target32 (0);
    shdr->sh_type = grub_host_to_target32 (SHT_STRTAB);
    shdr->sh_addr = grub_host_to_target_addr (0);
    shdr->sh_offset = grub_host_to_target_addr (str_start - elf_img);
    shdr->sh_size = grub_host_to_target32 (string_size);
    shdr->sh_link = grub_host_to_target32 (0);
    shdr->sh_info = grub_host_to_target32 (0);
    shdr->sh_addralign = grub_host_to_target32 (layout->align);
    shdr->sh_entsize = grub_host_to_target32 (0);
    shdr++;

    memcpy (ptr, ".text", sizeof (".text"));

    shdr->sh_name = grub_host_to_target32 (ptr - str_start);
    ptr += sizeof (".text");
    shdr->sh_type = grub_host_to_target32 (SHT_PROGBITS);
    shdr->sh_addr = grub_host_to_target_addr (target_addr);
    shdr->sh_offset = grub_host_to_target_addr (header_size);
    shdr->sh_size = grub_host_to_target32 (layout->kernel_size);
    shdr->sh_link = grub_host_to_target32 (0);
    shdr->sh_info = grub_host_to_target32 (0);
    shdr->sh_addralign = grub_host_to_target32 (layout->align);
    shdr->sh_entsize = grub_host_to_target32 (0);
    shdr++;

    memcpy (ptr, "mods", sizeof ("mods"));
    shdr->sh_name = grub_host_to_target32 (ptr - str_start);
    ptr += sizeof ("mods");
    shdr->sh_type = grub_host_to_target32 (SHT_PROGBITS);
    shdr->sh_addr = grub_host_to_target_addr (target_addr + layout->kernel_size);
    shdr->sh_offset = grub_host_to_target_addr (header_size + layout->kernel_size);
    shdr->sh_size = grub_host_to_target32 (*core_size - layout->kernel_size);
    shdr->sh_link = grub_host_to_target32 (0);
    shdr->sh_info = grub_host_to_target32 (0);
    shdr->sh_addralign = grub_host_to_target32 (image_target->voidp_sizeof);
    shdr->sh_entsize = grub_host_to_target32 (0);
    shdr++;

    if (image_target->id == IMAGE_XEN || image_target->id == IMAGE_XEN_PVH)
      {
	memcpy (ptr, ".xen", sizeof (".xen"));
	shdr->sh_name = grub_host_to_target32 (ptr - str_start);
	ptr += sizeof (".xen");
	shdr->sh_type = grub_host_to_target32 (SHT_PROGBITS);
	shdr->sh_addr = grub_host_to_target_addr (target_addr + layout->kernel_size);
	shdr->sh_offset = grub_host_to_target_addr (program_size + header_size);
	if (image_target->id == IMAGE_XEN)
	  shdr->sh_size = grub_host_to_target32 (XEN_NOTE_SIZE);
	else
	  shdr->sh_size = grub_host_to_target32 (XEN_PVH_NOTE_SIZE);
	shdr->sh_link = grub_host_to_target32 (0);
	shdr->sh_info = grub_host_to_target32 (0);
	shdr->sh_addralign = grub_host_to_target32 (image_target->voidp_sizeof);
	shdr->sh_entsize = grub_host_to_target32 (0);
	shdr++;
      }
  }

  free (*core_img);
  *core_img = elf_img;
  *core_size = program_size + header_size + footer_size;
}

/* Relocate symbols; note that this function overwrites the symbol table.
   Return the address of a start symbol.  */
static Elf_Addr
SUFFIX (relocate_symbols) (Elf_Ehdr *e, struct section_metadata *smd,
			   void *jumpers, Elf_Addr jumpers_addr,
			   Elf_Addr bss_start, Elf_Addr end,
			   const struct grub_install_image_target_desc *image_target)
{
  Elf_Word symtab_size, sym_size, num_syms;
  Elf_Off symtab_offset;
  Elf_Addr start_address = (Elf_Addr) -1;
  Elf_Sym *sym;
  Elf_Word i;
  Elf_Shdr *symtab_section;
  const char *symtab;
  grub_uint64_t *jptr = jumpers;

  symtab_section = (Elf_Shdr *) ((char *) smd->sections
				 + grub_target_to_host32 (smd->symtab->sh_link)
				   * smd->section_entsize);
  symtab = (char *) e + grub_target_to_host (symtab_section->sh_offset);

  symtab_size = grub_target_to_host (smd->symtab->sh_size);
  sym_size = grub_target_to_host (smd->symtab->sh_entsize);
  symtab_offset = grub_target_to_host (smd->symtab->sh_offset);
  num_syms = symtab_size / sym_size;

  for (i = 0, sym = (Elf_Sym *) ((char *) e + symtab_offset);
       i < num_syms;
       i++, sym = (Elf_Sym *) ((char *) sym + sym_size))
    {
      Elf_Section cur_index;
      const char *name;

      name = symtab + grub_target_to_host32 (sym->st_name);

      cur_index = grub_target_to_host16 (sym->st_shndx);
      if (cur_index == STN_ABS)
        {
          continue;
        }
      else if (cur_index == STN_UNDEF)
	{
	  if (sym->st_name && grub_strcmp (name, "__bss_start") == 0)
	    sym->st_value = bss_start;
	  else if (sym->st_name && grub_strcmp (name, "_end") == 0)
	    sym->st_value = end;
	  else if (sym->st_name)
	    grub_util_error ("undefined symbol %s", name);
	  else
	    continue;
	}
      else if (cur_index >= smd->num_sections)
	grub_util_error ("section %d does not exist", cur_index);
      else
	{
	  sym->st_value = (grub_target_to_host (sym->st_value)
			   + smd->vaddrs[cur_index]);
	}

      if (image_target->elf_target == EM_IA_64 && ELF_ST_TYPE (sym->st_info)
	  == STT_FUNC)
	{
	  *jptr = grub_host_to_target64 (sym->st_value);
	  sym->st_value = (char *) jptr - (char *) jumpers + jumpers_addr;
	  jptr++;
	  *jptr = 0;
	  jptr++;
	}
      grub_util_info ("locating %s at 0x%"  GRUB_HOST_PRIxLONG_LONG
		      " (0x%"  GRUB_HOST_PRIxLONG_LONG ")", name,
		      (unsigned long long) sym->st_value,
		      (unsigned long long) smd->vaddrs[cur_index]);

      if (start_address == (Elf_Addr)-1)
	if (strcmp (name, "_start") == 0 || strcmp (name, "start") == 0)
	  start_address = sym->st_value;
    }

  return start_address;
}

/* Return the address of a symbol at the index I in the section S.  */
static Elf_Addr
SUFFIX (get_symbol_address) (Elf_Ehdr *e, Elf_Shdr *s, Elf_Word i,
			     const struct grub_install_image_target_desc *image_target)
{
  Elf_Sym *sym;

  sym = (Elf_Sym *) ((char *) e
		       + grub_target_to_host (s->sh_offset)
		       + i * grub_target_to_host (s->sh_entsize));
  return sym->st_value;
}

/* Return the address of a modified value.  */
static Elf_Addr *
SUFFIX (get_target_address) (Elf_Ehdr *e, Elf_Shdr *s, Elf_Addr offset,
		    const struct grub_install_image_target_desc *image_target)
{
  return (Elf_Addr *) ((char *) e + grub_target_to_host (s->sh_offset) + offset);
}

#ifdef MKIMAGE_ELF64
static Elf_Addr
SUFFIX (count_funcs) (Elf_Ehdr *e, Elf_Shdr *symtab_section,
		      const struct grub_install_image_target_desc *image_target)
{
  Elf_Word symtab_size, sym_size, num_syms;
  Elf_Off symtab_offset;
  Elf_Sym *sym;
  Elf_Word i;
  int ret = 0;

  symtab_size = grub_target_to_host (symtab_section->sh_size);
  sym_size = grub_target_to_host (symtab_section->sh_entsize);
  symtab_offset = grub_target_to_host (symtab_section->sh_offset);
  num_syms = symtab_size / sym_size;

  for (i = 0, sym = (Elf_Sym *) ((char *) e + symtab_offset);
       i < num_syms;
       i++, sym = (Elf_Sym *) ((char *) sym + sym_size))
    if (ELF_ST_TYPE (sym->st_info) == STT_FUNC)
      ret++;

  return ret;
}
#endif

#ifdef MKIMAGE_ELF32
/* Deal with relocation information. This function relocates addresses
   within the virtual address space starting from 0. So only relative
   addresses can be fully resolved. Absolute addresses must be relocated
   again by a PE32 relocator when loaded.  */
static grub_size_t
arm_get_trampoline_size (Elf_Ehdr *e,
			 Elf_Shdr *sections,
			 Elf_Half section_entsize,
			 Elf_Half num_sections,
			 const struct grub_install_image_target_desc *image_target)
{
  Elf_Half i;
  Elf_Shdr *s;
  grub_size_t ret = 0;

  for (i = 0, s = sections;
       i < num_sections;
       i++, s = (Elf_Shdr *) ((char *) s + section_entsize))
    if ((s->sh_type == grub_host_to_target32 (SHT_REL)) ||
        (s->sh_type == grub_host_to_target32 (SHT_RELA)))
      {
	Elf_Rela *r;
	Elf_Word rtab_size, r_size, num_rs;
	Elf_Off rtab_offset;
	Elf_Shdr *symtab_section;
	Elf_Word j;

	symtab_section = (Elf_Shdr *) ((char *) sections
					 + (grub_target_to_host32 (s->sh_link)
					    * section_entsize));

	rtab_size = grub_target_to_host (s->sh_size);
	r_size = grub_target_to_host (s->sh_entsize);
	rtab_offset = grub_target_to_host (s->sh_offset);
	num_rs = rtab_size / r_size;

	for (j = 0, r = (Elf_Rela *) ((char *) e + rtab_offset);
	     j < num_rs;
	     j++, r = (Elf_Rela *) ((char *) r + r_size))
	  {
            Elf_Addr info;
	    Elf_Addr sym_addr;

	    info = grub_target_to_host (r->r_info);
	    sym_addr = SUFFIX (get_symbol_address) (e, symtab_section,
						    ELF_R_SYM (info), image_target);

            sym_addr += (s->sh_type == grub_target_to_host32 (SHT_RELA)) ?
	      grub_target_to_host (r->r_addend) : 0;

	    switch (ELF_R_TYPE (info))
	      {
	      case R_ARM_ABS32:
	      case R_ARM_V4BX:
		break;
	      case R_ARM_THM_CALL:
	      case R_ARM_THM_JUMP24:
	      case R_ARM_THM_JUMP19:
		if (!(sym_addr & 1))
		  ret += 8;
		break;

	      case R_ARM_CALL:
	      case R_ARM_JUMP24:
		if (sym_addr & 1)
		  ret += 16;
		break;

	      default:
		grub_util_error (_("relocation 0x%x is not implemented yet"),
				 (unsigned int) ELF_R_TYPE (info));
		break;
	      }
	  }
      }
  return ret;
}
#endif

static int
SUFFIX (is_kept_section) (Elf_Shdr *s, const struct grub_install_image_target_desc *image_target);
static int
SUFFIX (is_kept_reloc_section) (Elf_Shdr *s, const struct grub_install_image_target_desc *image_target,
				struct section_metadata *smd);

/* Deal with relocation information. This function relocates addresses
   within the virtual address space starting from 0. So only relative
   addresses can be fully resolved. Absolute addresses must be relocated
   again by a PE32 relocator when loaded.  */
static void
SUFFIX (relocate_addrs) (Elf_Ehdr *e, struct section_metadata *smd,
			     char *pe_target, Elf_Addr tramp_off, Elf_Addr got_off,
			     const struct grub_install_image_target_desc *image_target)
{
  Elf_Half i;
  Elf_Shdr *s;
#ifdef MKIMAGE_ELF64
  struct grub_ia64_trampoline *tr = (void *) (pe_target + tramp_off);
  grub_uint64_t *gpptr = (void *) (pe_target + got_off);
  unsigned unmatched_adr_got_page = 0;
#define MASK19 ((1 << 19) - 1)
#else
  grub_uint32_t *tr = (void *) (pe_target + tramp_off);
#endif

  for (i = 0, s = smd->sections;
       i < smd->num_sections;
       i++, s = (Elf_Shdr *) ((char *) s + smd->section_entsize))
    if ((s->sh_type == grub_host_to_target32 (SHT_REL)) ||
        (s->sh_type == grub_host_to_target32 (SHT_RELA)))
      {
	Elf_Rela *r;
	Elf_Word rtab_size, r_size, num_rs;
	Elf_Off rtab_offset;
	Elf_Word target_section_index;
	Elf_Addr target_section_addr;
	Elf_Shdr *target_section;
	Elf_Word j;

	if (!SUFFIX (is_kept_section) (s, image_target) &&
	    !SUFFIX (is_kept_reloc_section) (s, image_target, smd))
	  {
	    grub_util_info ("not translating relocations for omitted section %s",
			smd->strtab + grub_le_to_cpu32 (s->sh_name));
	    continue;
	  }

	target_section_index = grub_target_to_host32 (s->sh_info);
	target_section_addr = smd->addrs[target_section_index];
	target_section = (Elf_Shdr *) ((char *) smd->sections
					 + (target_section_index
					    * smd->section_entsize));

	grub_util_info ("dealing with the relocation section %s for %s",
			smd->strtab + grub_target_to_host32 (s->sh_name),
			smd->strtab + grub_target_to_host32 (target_section->sh_name));

	rtab_size = grub_target_to_host (s->sh_size);
	r_size = grub_target_to_host (s->sh_entsize);
	rtab_offset = grub_target_to_host (s->sh_offset);
	num_rs = rtab_size / r_size;

	for (j = 0, r = (Elf_Rela *) ((char *) e + rtab_offset);
	     j < num_rs;
	     j++, r = (Elf_Rela *) ((char *) r + r_size))
	  {
            Elf_Addr info;
	    Elf_Addr offset;
	    Elf_Addr sym_addr;
	    Elf_Addr *target;
	    Elf_Addr addend;

	    offset = grub_target_to_host (r->r_offset);
	    target = SUFFIX (get_target_address) (e, target_section,
						  offset, image_target);
	    if (image_target->elf_target == EM_MIPS && image_target->voidp_sizeof == 8)
	      info = ((grub_uint64_t) r->r_info << 32) |
		      (grub_uint32_t) grub_be_to_cpu64 (r->r_info);
	    else
	      info = grub_target_to_host (r->r_info);
	    sym_addr = SUFFIX (get_symbol_address) (e, smd->symtab,
						    ELF_R_SYM (info), image_target);

            addend = (s->sh_type == grub_target_to_host32 (SHT_RELA)) ?
	      grub_target_to_host (r->r_addend) : 0;

	   switch (image_target->elf_target)
	     {
	     case EM_386:
	      switch (ELF_R_TYPE (info))
		{
		case R_386_NONE:
		  break;

		case R_386_32:
		  /* This is absolute.  */
		  *target = grub_host_to_target32 (grub_target_to_host32 (*target)
						   + addend + sym_addr);
		  grub_util_info ("relocating an R_386_32 entry to 0x%"
				  GRUB_HOST_PRIxLONG_LONG " at the offset 0x%"
				  GRUB_HOST_PRIxLONG_LONG,
				  (unsigned long long) *target,
				  (unsigned long long) offset);
		  break;

		case R_386_PC32:
		  /* This is relative.  */
		  *target = grub_host_to_target32 (grub_target_to_host32 (*target)
						   + addend + sym_addr
						   - target_section_addr - offset
						   - image_target->vaddr_offset);
		  grub_util_info ("relocating an R_386_PC32 entry to 0x%"
				  GRUB_HOST_PRIxLONG_LONG " at the offset 0x%"
				  GRUB_HOST_PRIxLONG_LONG,
				  (unsigned long long) *target,
				  (unsigned long long) offset);
		  break;
		default:
		  grub_util_error (_("relocation 0x%x is not implemented yet"),
				   (unsigned int) ELF_R_TYPE (info));
		  break;
		}
	      break;
#ifdef MKIMAGE_ELF64
	     case EM_X86_64:
	      switch (ELF_R_TYPE (info))
		{

		case R_X86_64_NONE:
		  break;

		case R_X86_64_64:
		  *target = grub_host_to_target64 (grub_target_to_host64 (*target)
						   + addend + sym_addr);
		  grub_util_info ("relocating an R_X86_64_64 entry to 0x%"
				  GRUB_HOST_PRIxLONG_LONG " at the offset 0x%"
				  GRUB_HOST_PRIxLONG_LONG,
				  (unsigned long long) *target,
				  (unsigned long long) offset);
		  break;

		case R_X86_64_PC32:
		case R_X86_64_PLT32:
		  {
		    grub_uint32_t *t32 = (grub_uint32_t *) target;
		    *t32 = grub_host_to_target64 (grub_target_to_host32 (*t32)
						  + addend + sym_addr
						  - target_section_addr - offset
						  - image_target->vaddr_offset);
		    grub_util_info ("relocating an R_X86_64_PC32 entry to 0x%x at the offset 0x%"
				    GRUB_HOST_PRIxLONG_LONG,
				    *t32, (unsigned long long) offset);
		    break;
		  }

		case R_X86_64_PC64:
		  {
		    *target = grub_host_to_target64 (grub_target_to_host64 (*target)
						     + addend + sym_addr
						     - target_section_addr - offset
						     - image_target->vaddr_offset);
		    grub_util_info ("relocating an R_X86_64_PC64 entry to 0x%"
				    GRUB_HOST_PRIxLONG_LONG " at the offset 0x%"
				    GRUB_HOST_PRIxLONG_LONG,
				    (unsigned long long) *target,
				    (unsigned long long) offset);
		    break;
		  }

		case R_X86_64_32:
		case R_X86_64_32S:
		  {
		    grub_uint32_t *t32 = (grub_uint32_t *) target;
		    *t32 = grub_host_to_target64 (grub_target_to_host32 (*t32)
						  + addend + sym_addr);
		    grub_util_info ("relocating an R_X86_64_32(S) entry to 0x%x at the offset 0x%"
				    GRUB_HOST_PRIxLONG_LONG,
				    *t32, (unsigned long long) offset);
		    break;
		  }

		default:
		  grub_util_error (_("relocation 0x%x is not implemented yet"),
				   (unsigned int) ELF_R_TYPE (info));
		  break;
		}
	      break;
	     case EM_IA_64:
	      switch (ELF_R_TYPE (info))
		{
		case R_IA64_PCREL21B:
		  {
		    grub_uint64_t noff;
		    grub_ia64_make_trampoline (tr, addend + sym_addr);
		    noff = ((char *) tr - (char *) pe_target
			    - target_section_addr - (offset & ~3)) >> 4;
		    tr++;
		    if (noff & ~MASK19)
		      grub_util_error ("trampoline offset too big (%"
				       GRUB_HOST_PRIxLONG_LONG ")",
				       (unsigned long long) noff);
		    grub_ia64_add_value_to_slot_20b ((grub_addr_t) target, noff);
		  }
		  break;

		case R_IA64_LTOFF22X:
		case R_IA64_LTOFF22:
		  {
		    Elf_Sym *sym;

		    sym = (Elf_Sym *) ((char *) e
				       + grub_target_to_host (smd->symtab->sh_offset)
				       + ELF_R_SYM (info) * grub_target_to_host (smd->symtab->sh_entsize));
		    if (ELF_ST_TYPE (sym->st_info) == STT_FUNC)
		      sym_addr = grub_target_to_host64 (*(grub_uint64_t *) (pe_target
									    + sym->st_value
									    - image_target->vaddr_offset));
		  }
		/* FALLTHROUGH */
		case R_IA64_LTOFF_FPTR22:
		  *gpptr = grub_host_to_target64 (addend + sym_addr);
		  grub_ia64_add_value_to_slot_21 ((grub_addr_t) target,
						  (char *) gpptr - (char *) pe_target
						  + image_target->vaddr_offset);
		  gpptr++;
		  break;

		case R_IA64_GPREL22:
		  grub_ia64_add_value_to_slot_21 ((grub_addr_t) target,
						  addend + sym_addr);
		  break;
		case R_IA64_GPREL64I:
		  grub_ia64_set_immu64 ((grub_addr_t) target,
					addend + sym_addr);
		  break;
		case R_IA64_PCREL64LSB:
		  *target = grub_host_to_target64 (grub_target_to_host64 (*target)
						   + addend + sym_addr
						   - target_section_addr - offset
						   - image_target->vaddr_offset);
		  break;

		case R_IA64_SEGREL64LSB:
		  *target = grub_host_to_target64 (grub_target_to_host64 (*target)
						   + addend + sym_addr - target_section_addr);
		  break;
		case R_IA64_DIR64LSB:
		case R_IA64_FPTR64LSB:
		  *target = grub_host_to_target64 (grub_target_to_host64 (*target)
						   + addend + sym_addr);
		  grub_util_info ("relocating a direct entry to 0x%"
				  GRUB_HOST_PRIxLONG_LONG " at the offset 0x%"
				  GRUB_HOST_PRIxLONG_LONG,
				  (unsigned long long)
				  grub_target_to_host64 (*target),
				  (unsigned long long) offset);
		  break;

		  /* We treat LTOFF22X as LTOFF22, so we can ignore LDXMOV.  */
		case R_IA64_LDXMOV:
		  break;

		default:
		  grub_util_error (_("relocation 0x%x is not implemented yet"),
				   (unsigned int) ELF_R_TYPE (info));
		  break;
		}
	       break;
	     case EM_AARCH64:
	       {
		 sym_addr += addend;
		 switch (ELF_R_TYPE (info))
		   {
		   case R_AARCH64_ABS64:
		     {
		       *target = grub_host_to_target64 (grub_target_to_host64 (*target) + sym_addr);
		     }
		     break;
		   case R_AARCH64_PREL32:
		     {
		       grub_uint32_t *t32 = (grub_uint32_t *) target;
		       *t32 = grub_host_to_target64 (grub_target_to_host32 (*t32)
						     + sym_addr
						     - target_section_addr - offset
						     - image_target->vaddr_offset);
		       grub_util_info ("relocating an R_AARCH64_PREL32 entry to 0x%x at the offset 0x%"
				       GRUB_HOST_PRIxLONG_LONG,
				       *t32, (unsigned long long) offset);
		       break;
		     }
		   case R_AARCH64_ADD_ABS_LO12_NC:
		     grub_arm64_set_abs_lo12 ((grub_uint32_t *) target,
					      sym_addr);
		     break;
		   case R_AARCH64_LDST64_ABS_LO12_NC:
		     grub_arm64_set_abs_lo12_ldst64 ((grub_uint32_t *) target,
						     sym_addr);
		     break;
		   case R_AARCH64_JUMP26:
		   case R_AARCH64_CALL26:
		     {
		       sym_addr -= offset;
		       sym_addr -= target_section_addr + image_target->vaddr_offset;
		       if (!grub_arm_64_check_xxxx26_offset (sym_addr))
			 grub_util_error ("%s", "CALL26 Relocation out of range");

		       grub_arm64_set_xxxx26_offset((grub_uint32_t *)target,
						     sym_addr);
		     }
		     break;
		   case R_AARCH64_ADR_GOT_PAGE:
		     {
		       Elf64_Rela *rel2;
		       grub_int64_t gpoffset = (((char *) gpptr - (char *) pe_target + image_target->vaddr_offset) & ~0xfffULL)
			 - ((offset + target_section_addr + image_target->vaddr_offset) & ~0xfffULL);
		       unsigned k;
		       *gpptr = grub_host_to_target64 (sym_addr);
		       unmatched_adr_got_page++;
		       if (!grub_arm64_check_hi21_signed (gpoffset))
			 grub_util_error ("HI21 out of range");
		       grub_arm64_set_hi21((grub_uint32_t *)target,
					   gpoffset);
		       for (k = 0, rel2 = (Elf_Rela *) ((char *) r + r_size);
			    k < num_rs;
			    k++, rel2 = (Elf_Rela *) ((char *) rel2 + r_size))
			 if (ELF_R_SYM (rel2->r_info)
			     == ELF_R_SYM (r->r_info)
			     && r->r_addend == rel2->r_addend
			     && ELF_R_TYPE (rel2->r_info) == R_AARCH64_LD64_GOT_LO12_NC)
			   {
			     grub_arm64_set_abs_lo12_ldst64 ((grub_uint32_t *) SUFFIX (get_target_address) (e, target_section,
													    grub_target_to_host (rel2->r_offset), image_target),
							     ((char *) gpptr - (char *) pe_target + image_target->vaddr_offset));
			     break;
			   }
		       if (k >= num_rs)
			 grub_util_error ("ADR_GOT_PAGE without matching LD64_GOT_LO12_NC");
		       gpptr++;
	             }
		     break;
		   case R_AARCH64_LD64_GOT_LO12_NC:
		     if (unmatched_adr_got_page == 0)
		       grub_util_error ("LD64_GOT_LO12_NC without matching ADR_GOT_PAGE");
		     unmatched_adr_got_page--;
		     break;
		   case R_AARCH64_ADR_PREL_PG_HI21:
		     {
		       sym_addr &= ~0xfffULL;
		       sym_addr -= (offset + target_section_addr + image_target->vaddr_offset) & ~0xfffULL;
		       if (!grub_arm64_check_hi21_signed (sym_addr))
			 grub_util_error ("%s", "CALL26 Relocation out of range");

		       grub_arm64_set_hi21((grub_uint32_t *)target,
					   sym_addr);
		     }
		     break;
		   default:
		     grub_util_error (_("relocation 0x%x is not implemented yet"),
				      (unsigned int) ELF_R_TYPE (info));
		     break;
		   }
	       break;
	       }
	     case EM_MIPS:
	       {
		 sym_addr += addend;
		 switch (ELF_R_TYPE (info))
		   {
		   case R_MIPS_NONE:
		     break;
		   case R_MIPS_64:
		     {
		       *target = grub_host_to_target64 (grub_target_to_host64 (*target) + sym_addr);
		     }
		     break;
		   case R_MIPS_32:
		     {
		       grub_uint32_t *t32 = (grub_uint32_t *) target;
		       *t32 = grub_host_to_target64 (grub_target_to_host32 (*t32) + sym_addr);
		     }
		     break;
		   case R_MIPS_26:
		     {
		       grub_uint32_t *t32 = (grub_uint32_t *) target;
		       grub_addr_t addr = grub_host_to_target64 (sym_addr);
		       *t32 = ((*t32) & 0xfc000000U) | ((addr >> 2) & 0x3ffffffUL);
		     }
		     break;
		   case R_MIPS_LO16:
		     {
		       grub_int16_t *t16 = (grub_int16_t *) target;
		       grub_addr_t addr = grub_host_to_target64 (sym_addr);
		       *t16 = (grub_int16_t) addr;
		     }
		     break;
		   case R_MIPS_HI16:
		     {
		       grub_int16_t *t16 = (grub_int16_t *) target;
		       grub_addr_t addr = grub_host_to_target64 (sym_addr);
		       *t16 = (grub_int16_t) ((addr + 0x8000UL) >> 16);
		     }
		     break;
		   case R_MIPS_HIGHER:
		     {
		       grub_int16_t *t16 = (grub_int16_t *) target;
		       grub_addr_t addr = grub_host_to_target64 (sym_addr);
		       *t16 = (grub_int16_t) ((addr + 0x80008000UL) >> 32);
		     }
		     break;
		   case R_MIPS_HIGHEST:
		     {
		       grub_uint16_t *t16 = (grub_uint16_t *) target;
		       grub_addr_t addr = grub_host_to_target64 (sym_addr);
		       *t16 = (grub_uint16_t) ((addr + 0x800080008000UL) >> 48);
		     }
		     break;
		   default:
		     grub_util_error (_("relocation 0x%x is not implemented yet"),
				      (unsigned int) ELF_R_TYPE (info));
		     break;
		   }
	       break;
	       }
#endif
#if defined(MKIMAGE_ELF32)
	     case EM_ARM:
	       {
		 sym_addr += addend;
		 sym_addr -= image_target->vaddr_offset;
		 switch (ELF_R_TYPE (info))
		   {
		   case R_ARM_ABS32:
		     {
		       grub_util_info ("  ABS32:\toffset=%d\t(0x%08x)",
				       (int) sym_addr, (int) sym_addr);
		       /* Data will be naturally aligned */
		       if (image_target->id == IMAGE_EFI)
			 sym_addr += GRUB_PE32_SECTION_ALIGNMENT;
		       *target = grub_host_to_target32 (grub_target_to_host32 (*target) + sym_addr);
		     }
		     break;
		     /* Happens when compiled with -march=armv4.
			Since currently we need at least armv5, keep bx as-is.
		     */
		   case R_ARM_V4BX:
		     break;
		   case R_ARM_THM_CALL:
		   case R_ARM_THM_JUMP24:
		   case R_ARM_THM_JUMP19:
		     {
		       grub_err_t err;
		       Elf_Sym *sym;
		       grub_util_info ("  THM_JUMP24:\ttarget=0x%08lx\toffset=(0x%08x)",
				       (unsigned long) ((char *) target
							- (char *) e),
				       sym_addr);
		       sym = (Elf_Sym *) ((char *) e
					  + grub_target_to_host (smd->symtab->sh_offset)
					  + ELF_R_SYM (info) * grub_target_to_host (smd->symtab->sh_entsize));
		       if (ELF_ST_TYPE (sym->st_info) != STT_FUNC)
			 sym_addr |= 1;
		       if (!(sym_addr & 1))
			 {
			   grub_uint32_t tr_addr;
			   grub_int32_t new_offset;
			   tr_addr = (char *) tr - (char *) pe_target
			     - target_section_addr;
			   new_offset = sym_addr - tr_addr - 12;

			   if (!grub_arm_jump24_check_offset (new_offset))
			     return grub_util_error ("jump24 relocation out of range");

			   tr[0] = grub_host_to_target32 (0x46c04778); /* bx pc; nop  */
			   tr[1] = grub_host_to_target32 (((new_offset >> 2) & 0xffffff) | 0xea000000); /* b new_offset */
			   tr += 2;
			   sym_addr = tr_addr | 1;
			 }
		       sym_addr -= offset;
		       /* Thumb instructions can be 16-bit aligned */
		       if (ELF_R_TYPE (info) == R_ARM_THM_JUMP19)
			 err = grub_arm_reloc_thm_jump19 ((grub_uint16_t *) target, sym_addr);
		       else
			 err = grub_arm_reloc_thm_call ((grub_uint16_t *) target,
							sym_addr);
		       if (err)
			 grub_util_error ("%s", grub_errmsg);
		     }
		     break;

		   case R_ARM_CALL:
		   case R_ARM_JUMP24:
		     {
		       grub_err_t err;
		       grub_util_info ("  JUMP24:\ttarget=0x%08lx\toffset=(0x%08x)",  (unsigned long) ((char *) target - (char *) e), sym_addr);
		       if (sym_addr & 1)
			 {
			   grub_uint32_t tr_addr;
			   grub_int32_t new_offset;
			   tr_addr = (char *) tr - (char *) pe_target
			     - target_section_addr;
			   new_offset = sym_addr - tr_addr - 12;

			   /* There is no immediate version of bx, only register one...  */
			   tr[0] = grub_host_to_target32 (0xe59fc004); /* ldr	ip, [pc, #4] */
			   tr[1] = grub_host_to_target32 (0xe08cc00f); /* add	ip, ip, pc */
			   tr[2] = grub_host_to_target32 (0xe12fff1c); /* bx	ip */
			   tr[3] = grub_host_to_target32 (new_offset | 1);
			   tr += 4;
			   sym_addr = tr_addr;
			 }
		       sym_addr -= offset;
		       err = grub_arm_reloc_jump24 (target,
						    sym_addr);
		       if (err)
			 grub_util_error ("%s", grub_errmsg);
		     }
		     break;

		   default:
		     grub_util_error (_("relocation 0x%x is not implemented yet"),
				      (unsigned int) ELF_R_TYPE (info));
		     break;
		   }
		 break;
	       }
#endif /* MKIMAGE_ELF32 */
	     case EM_RISCV:
	       {
		 grub_uint64_t *t64 = (grub_uint64_t *) target;
		 grub_uint32_t *t32 = (grub_uint32_t *) target;
		 grub_uint16_t *t16 = (grub_uint16_t *) target;
		 grub_uint8_t *t8 = (grub_uint8_t *) target;
		 grub_int64_t off = (long)sym_addr - target_section_addr - offset
				    - image_target->vaddr_offset;

		 /*
		  * Instructions and instruction encoding are documented in the RISC-V
		  * specification. This file is based on version 2.2:
		  *
		  * https://github.com/riscv/riscv-isa-manual/blob/master/release/riscv-spec-v2.2.pdf
		  */

		 sym_addr += addend;

		 switch (ELF_R_TYPE (info))
		   {
		   case R_RISCV_ADD8:
		     *t8 = *t8 + sym_addr;
		     break;
		   case R_RISCV_ADD16:
		     *t16 = grub_host_to_target16 (grub_target_to_host16 (*t16) + sym_addr);
		     break;
		   case R_RISCV_32:
		   case R_RISCV_ADD32:
		     *t32 = grub_host_to_target32 (grub_target_to_host32 (*t32) + sym_addr);
		     break;
		   case R_RISCV_64:
		   case R_RISCV_ADD64:
		     *t64 = grub_host_to_target64 (grub_target_to_host64 (*t64) + sym_addr);
		     break;

		   case R_RISCV_SUB8:
		     *t8 = sym_addr - *t8;
		     break;
		   case R_RISCV_SUB16:
		     *t16 = grub_host_to_target16 (grub_target_to_host16 (*t16) - sym_addr);
		     break;
		   case R_RISCV_SUB32:
		     *t32 = grub_host_to_target32 (grub_target_to_host32 (*t32) - sym_addr);
		     break;
		   case R_RISCV_SUB64:
		     *t64 = grub_host_to_target64 (grub_target_to_host64 (*t64) - sym_addr);
		     break;
		   case R_RISCV_BRANCH:
		     {
		       grub_uint32_t imm12 = (off & 0x1000) << (31 - 12);
		       grub_uint32_t imm11 = (off & 0x800) >> (11 - 7);
		       grub_uint32_t imm10_5 = (off & 0x7e0) << (30 - 10);
		       grub_uint32_t imm4_1 = (off & 0x1e) << (11 - 4);
		       *t32 = grub_host_to_target32 ((grub_target_to_host32 (*t32) & 0x1fff07f)
						     | imm12 | imm11 | imm10_5 | imm4_1);
		     }
		     break;
		   case R_RISCV_JAL:
		     {
		       grub_uint32_t imm20 = (off & 0x100000) << (31 - 20);
		       grub_uint32_t imm19_12 = (off & 0xff000);
		       grub_uint32_t imm11 = (off & 0x800) << (20 - 11);
		       grub_uint32_t imm10_1 = (off & 0x7fe) << (30 - 10);
		       *t32 = grub_host_to_target32 ((grub_target_to_host32 (*t32) & 0xfff)
						     | imm20 | imm19_12 | imm11 | imm10_1);
		     }
		     break;
		   case R_RISCV_CALL:
		     {
		       grub_uint32_t hi20, lo12;

		       if (off != (grub_int32_t)off)
			 grub_util_error ("target %lx not reachable from pc=%lx", (long)sym_addr, (long)((char *)target - (char *)e));

		       hi20 = (off + 0x800) & 0xfffff000;
		       lo12 = (off - hi20) & 0xfff;
		       t32[0] = grub_host_to_target32 ((grub_target_to_host32 (t32[0]) & 0xfff) | hi20);
		       t32[1] = grub_host_to_target32 ((grub_target_to_host32 (t32[1]) & 0xfffff) | (lo12 << 20));
		     }
		     break;
		   case R_RISCV_RVC_BRANCH:
		     {
		       grub_uint16_t imm8 = (off & 0x100) << (12 - 8);
		       grub_uint16_t imm7_6 = (off & 0xc0) >> (6 - 5);
		       grub_uint16_t imm5 = (off & 0x20) >> (5 - 2);
		       grub_uint16_t imm4_3 = (off & 0x18) << (12 - 5);
		       grub_uint16_t imm2_1 = (off & 0x6) << (12 - 10);
		       *t16 = grub_host_to_target16 ((grub_target_to_host16 (*t16) & 0xe383)
						     | imm8 | imm7_6 | imm5 | imm4_3 | imm2_1);
		     }
		     break;
		   case R_RISCV_RVC_JUMP:
		     {
		       grub_uint16_t imm11 = (off & 0x800) << (12 - 11);
		       grub_uint16_t imm10 = (off & 0x400) >> (10 - 8);
		       grub_uint16_t imm9_8 = (off & 0x300) << (12 - 11);
		       grub_uint16_t imm7 = (off & 0x80) >> (7 - 6);
		       grub_uint16_t imm6 = (off & 0x40) << (12 - 11);
		       grub_uint16_t imm5 = (off & 0x20) >> (5 - 2);
		       grub_uint16_t imm4 = (off & 0x10) << (12 - 5);
		       grub_uint16_t imm3_1 = (off & 0xe) << (12 - 10);
		       *t16 = grub_host_to_target16 ((grub_target_to_host16 (*t16) & 0xe003)
						     | imm11 | imm10 | imm9_8 | imm7 | imm6
						     | imm5 | imm4 | imm3_1);
		     }
		     break;
		   case R_RISCV_PCREL_HI20:
		     {
		       grub_int32_t hi20;

		       if (off != (grub_int32_t)off)
			 grub_util_error ("target %lx not reachable from pc=%lx", (long)sym_addr, (long)((char *)target - (char *)e));

		       hi20 = (off + 0x800) & 0xfffff000;
		       *t32 = grub_host_to_target32 ((grub_target_to_host32 (*t32) & 0xfff) | hi20);
		     }
		     break;
		   case R_RISCV_PCREL_LO12_I:
		   case R_RISCV_PCREL_LO12_S:
		     {
		       Elf_Rela *rel2;
		       Elf_Word k;
		       /* Search backwards for matching HI20 reloc.  */
		       for (k = j, rel2 = (Elf_Rela *) ((char *) r - r_size);
			    k > 0;
			    k--, rel2 = (Elf_Rela *) ((char *) rel2 - r_size))
			 {
			   Elf_Addr rel2_info;
			   Elf_Addr rel2_offset;
			   Elf_Addr rel2_sym_addr;
			   Elf_Addr rel2_addend;
			   Elf_Addr rel2_loc;
			   grub_int64_t rel2_off;

			   rel2_offset = grub_target_to_host (rel2->r_offset);
			   rel2_info = grub_target_to_host (rel2->r_info);
			   rel2_loc = target_section_addr + rel2_offset + image_target->vaddr_offset;

			   if (ELF_R_TYPE (rel2_info) == R_RISCV_PCREL_HI20
			       && rel2_loc == sym_addr)
			     {
			       rel2_sym_addr = SUFFIX (get_symbol_address)
				 (e, smd->symtab, ELF_R_SYM (rel2_info),
				  image_target);
			       rel2_addend = (s->sh_type == grub_target_to_host32 (SHT_RELA)) ?
				 grub_target_to_host (rel2->r_addend) : 0;
			       rel2_off = rel2_sym_addr + rel2_addend - rel2_loc;
			       off = rel2_off - ((rel2_off + 0x800) & 0xfffff000);

			       if (ELF_R_TYPE (info) == R_RISCV_PCREL_LO12_I)
				 *t32 = grub_host_to_target32 ((grub_target_to_host32 (*t32) & 0xfffff) | (off & 0xfff) << 20);
			       else
				 {
				   grub_uint32_t imm11_5 = (off & 0xfe0) << (31 - 11);
				   grub_uint32_t imm4_0 = (off & 0x1f) << (11 - 4);
				   *t32 = grub_host_to_target32 ((grub_target_to_host32 (*t32) & 0x1fff07f) | imm11_5 | imm4_0);
				 }
			       break;
			     }
			 }
		       if (k == 0)
			 grub_util_error ("cannot find matching HI20 relocation");
		     }
		     break;
		   case R_RISCV_HI20:
		     *t32 = grub_host_to_target32 ((grub_target_to_host32 (*t32) & 0xfff) | (((grub_int32_t) sym_addr + 0x800) & 0xfffff000));
		     break;
		   case R_RISCV_LO12_I:
		     {
		       grub_int32_t lo12 = (grub_int32_t) sym_addr - (((grub_int32_t) sym_addr + 0x800) & 0xfffff000);
		       *t32 = grub_host_to_target32 ((grub_target_to_host32 (*t32) & 0xfffff) | ((lo12 & 0xfff) << 20));
		     }
		     break;
		   case R_RISCV_LO12_S:
		     {
		       grub_int32_t lo12 = (grub_int32_t) sym_addr - (((grub_int32_t) sym_addr + 0x800) & 0xfffff000);
		       grub_uint32_t imm11_5 = (lo12 & 0xfe0) << (31 - 11);
		       grub_uint32_t imm4_0 = (lo12 & 0x1f) << (11 - 4);
		       *t32 = grub_host_to_target32 ((grub_target_to_host32 (*t32) & 0x1fff07f) | imm11_5 | imm4_0);
		     }
		     break;
		   case R_RISCV_RELAX:
		     break;
		   default:
		     grub_util_error (_("relocation 0x%x is not implemented yet"),
				      (unsigned int) ELF_R_TYPE (info));
		     break;
		   }
	       break;
	       }
	     default:
	       grub_util_error ("unknown architecture type %d",
				image_target->elf_target);
	     }
	  }
      }
}

/* Add a PE32's fixup entry for a relocation. Return the resulting address
   after having written to the file OUT.  */
static Elf_Addr
add_fixup_entry (struct fixup_block_list **cblock, grub_uint16_t type,
		 Elf_Addr addr, int flush, Elf_Addr current_address,
		 const struct grub_install_image_target_desc *image_target)
{
  struct grub_pe32_fixup_block *b;

  b = &((*cblock)->b);

  /* First, check if it is necessary to write out the current block.  */
  if ((*cblock)->state)
    {
      if (flush || (type && (addr < b->page_rva || b->page_rva + 0x1000 <= addr)))
	{
	  grub_uint32_t size;

	  if (flush)
	    {
	      /* Add as much padding as necessary to align the address
		 with a section boundary.  */
	      Elf_Addr next_address;
	      unsigned padding_size;
              size_t cur_index;

	      next_address = current_address + b->block_size;
	      padding_size = ((ALIGN_UP (next_address, image_target->section_align)
			       - next_address)
			      >> 1);
              cur_index = ((b->block_size - sizeof (*b)) >> 1);
              grub_util_info ("adding %d padding fixup entries", padding_size);
	      while (padding_size--)
		{
		  b->entries[cur_index++] = 0;
		  b->block_size += 2;
		}
	    }
          else while (b->block_size & (8 - 1))
            {
	      /* If not aligned with a 32-bit boundary, add
		 a padding entry.  */
              size_t cur_index;

              grub_util_info ("adding a padding fixup entry");
              cur_index = ((b->block_size - sizeof (*b)) >> 1);
              b->entries[cur_index] = 0;
              b->block_size += 2;
            }

          /* Flush it.  */
          grub_util_info ("writing %d bytes of a fixup block starting at 0x%x",
                          b->block_size, b->page_rva);
          size = b->block_size;
	  current_address += size;
	  b->page_rva = grub_host_to_target32 (b->page_rva);
	  b->block_size = grub_host_to_target32 (b->block_size);
	  (*cblock)->next = xmalloc (sizeof (**cblock) + 2 * 0x1000);
	  memset ((*cblock)->next, 0, sizeof (**cblock) + 2 * 0x1000);
	  *cblock = (*cblock)->next;
	}
    }

  b = &((*cblock)->b);

  if (! flush)
    {
      grub_uint16_t entry;
      size_t cur_index;

      /* If not allocated yet, allocate a block with enough entries.  */
      if (! (*cblock)->state)
	{
	  (*cblock)->state = 1;

	  /* The spec does not mention the requirement of a Page RVA.
	     Here, align the address with a 4K boundary for safety.  */
	  if (type)
	    b->page_rva = (addr & ~(0x1000 - 1));
	  b->block_size = sizeof (*b);
	}

      /* Sanity check.  */
      if (b->block_size >= sizeof (*b) + 2 * 0x1000)
	grub_util_error ("too many fixup entries");

      /* Add a new entry.  */
      cur_index = ((b->block_size - sizeof (*b)) >> 1);
      entry = GRUB_PE32_FIXUP_ENTRY (type, type ? (addr - b->page_rva) : addr);
      b->entries[cur_index] = grub_host_to_target16 (entry);
      b->block_size += 2;
    }

  return current_address;
}

struct raw_reloc
{
  struct raw_reloc *next;
  grub_uint32_t offset;
  enum raw_reloc_type {
    RAW_RELOC_NONE = -1,
    RAW_RELOC_32 = 0,
    RAW_RELOC_MAX = 1,
  } type;
};

struct translate_context
{
  /* PE */
  struct fixup_block_list *lst, *lst0;
  Elf_Addr current_address;

  /* Raw */
  struct raw_reloc *raw_relocs;
};

static void
translate_reloc_start (struct translate_context *ctx,
		       const struct grub_install_image_target_desc *image_target)
{
  grub_memset (ctx, 0, sizeof (*ctx));
  if (image_target->id == IMAGE_EFI)
    {
      ctx->lst = ctx->lst0 = xmalloc (sizeof (*ctx->lst) + 2 * 0x1000);
      memset (ctx->lst, 0, sizeof (*ctx->lst) + 2 * 0x1000);
      ctx->current_address = 0;
    }
}

static void
translate_relocation_pe (struct translate_context *ctx,
			 Elf_Addr addr,
			 Elf_Addr info,
			 Elf_Addr sym_addr,
			 Elf_Addr addend,
			 const struct grub_install_image_target_desc *image_target)
{
  /* Necessary to relocate only absolute addresses.  */
  switch (image_target->elf_target)
    {
    case EM_386:
      if (ELF_R_TYPE (info) == R_386_32)
	{
	  grub_util_info ("adding a relocation entry for 0x%"
			  GRUB_HOST_PRIxLONG_LONG,
			  (unsigned long long) addr);
	  ctx->current_address
	    = add_fixup_entry (&ctx->lst,
			       GRUB_PE32_REL_BASED_HIGHLOW,
			       addr, 0, ctx->current_address,
			       image_target);
	}
      break;
    case EM_X86_64:
      if ((ELF_R_TYPE (info) == R_X86_64_32) ||
	  (ELF_R_TYPE (info) == R_X86_64_32S))
	{
	  grub_util_error ("can\'t add fixup entry for R_X86_64_32(S)");
	}
      else if (ELF_R_TYPE (info) == R_X86_64_64)
	{
	  grub_util_info ("adding a relocation entry for 0x%"
			  GRUB_HOST_PRIxLONG_LONG,
			  (unsigned long long) addr);
	  ctx->current_address
	    = add_fixup_entry (&ctx->lst,
			       GRUB_PE32_REL_BASED_DIR64,
			       addr,
			       0, ctx->current_address,
			       image_target);
	}
      break;
    case EM_IA_64:
      switch (ELF_R_TYPE (info))
	{
	case R_IA64_PCREL64LSB:
	case R_IA64_LDXMOV:
	case R_IA64_PCREL21B:
	case R_IA64_LTOFF_FPTR22:
	case R_IA64_LTOFF22X:
	case R_IA64_LTOFF22:
	case R_IA64_GPREL22:
	case R_IA64_GPREL64I:
	case R_IA64_SEGREL64LSB:
	  break;

	case R_IA64_FPTR64LSB:
	case R_IA64_DIR64LSB:
#if 1
	  {
	    grub_util_info ("adding a relocation entry for 0x%"
			    GRUB_HOST_PRIxLONG_LONG,
			    (unsigned long long) addr);
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst,
				 GRUB_PE32_REL_BASED_DIR64,
				 addr,
				 0, ctx->current_address,
				 image_target);
	  }
#endif
	  break;
	default:
	  grub_util_error (_("relocation 0x%x is not implemented yet"),
			   (unsigned int) ELF_R_TYPE (info));
	  break;
	}
      break;
    case EM_AARCH64:
      switch (ELF_R_TYPE (info))
	{
	case R_AARCH64_ABS64:
	  {
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst,
				 GRUB_PE32_REL_BASED_DIR64,
				 addr, 0, ctx->current_address,
				 image_target);
	  }
	  break;
	  /* Relative relocations do not require fixup entries. */
	case R_AARCH64_CALL26:
	case R_AARCH64_JUMP26:
	case R_AARCH64_PREL32:
	  break;
	  /* Page-relative relocations do not require fixup entries. */
	case R_AARCH64_ADR_PREL_PG_HI21:
	  /* We page-align the whole kernel, so no need
	     for fixup entries.
	  */
	case R_AARCH64_ADD_ABS_LO12_NC:
	case R_AARCH64_LDST64_ABS_LO12_NC:
	  break;

	  /* GOT is relocated separately.  */
	case R_AARCH64_ADR_GOT_PAGE:
	case R_AARCH64_LD64_GOT_LO12_NC:
	  break;

	default:
	  grub_util_error (_("relocation 0x%x is not implemented yet"),
			   (unsigned int) ELF_R_TYPE (info));
	  break;
	}
      break;
#if defined(MKIMAGE_ELF64)
    case EM_MIPS:
      switch (ELF_R_TYPE (info))
	{
	case R_MIPS_64:
	  {
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst,
				 GRUB_PE32_REL_BASED_DIR64,
				 addr, 0, ctx->current_address,
				 image_target);
	  }
	  break;
	case R_MIPS_32:
#if 0
	  {
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst,
				 GRUB_PE32_REL_BASED_HIGHLOW,
				 addr, 0, ctx->current_address,
				 image_target);
	  }
#endif
	  break;
	case R_MIPS_26:
	  {
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst,
				 GRUB_PE32_REL_BASED_MIPS_JMPADDR,
				 addr, 0, ctx->current_address,
				 image_target);
	  }
	  break;
	case R_MIPS_LO16:
	  {
	    Elf_Addr target = sym_addr + addend;

	    ctx->current_address
	      = add_fixup_entry (&ctx->lst,
				 GRUB_PE32_REL_BASED_MIPS_LOW,
				 addr, 0, ctx->current_address,
				 image_target);
	    /* Hi */
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_int16_t) ((target & 0x8000UL) >> 16),
				 0, ctx->current_address,
				 image_target);
	    /* Higher */
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_int16_t) ((target & 0x80008000UL) >> 32),
				 0, ctx->current_address,
				 image_target);
	    /* Highest */
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_uint16_t) ((target & 0x800080008000UL) >> 48),
				 0, ctx->current_address,
				 image_target);
	  }
	  break;
	case R_MIPS_HI16:
	  {
	    Elf_Addr target = sym_addr + addend;

	    ctx->current_address
	      = add_fixup_entry (&ctx->lst,
				 GRUB_PE32_REL_BASED_MIPS_HIGH,
				 addr, 0, ctx->current_address,
				 image_target);
	    /* Lo */
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_int16_t) target,
				 0, ctx->current_address,
				 image_target);
	    /* Higher */
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_int16_t) ((target & 0x80008000UL) >> 32),
				 0, ctx->current_address,
				 image_target);
	    /* Highest */
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_uint16_t) ((target & 0x800080008000UL) >> 48),
				 0, ctx->current_address,
				 image_target);
	  }
	  break;
	case R_MIPS_HIGHER:
	  {
	    Elf_Addr target = sym_addr + addend;

	    ctx->current_address
	      = add_fixup_entry (&ctx->lst,
				 GRUB_PE32_REL_BASED_MIPS_HIGHER,
				 addr, 0, ctx->current_address,
				 image_target);
	    /* Lo */
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_int16_t) target,
				 0, ctx->current_address,
				 image_target);
	    /* Hi */
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_int16_t) ((target & 0x8000UL) >> 16),
				 0, ctx->current_address,
				 image_target);
	    /* Highest */
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_uint16_t) ((target & 0x800080008000UL) >> 48),
				 0, ctx->current_address,
				 image_target);
	  }
	  break;
	case R_MIPS_HIGHEST:
	  {
	    Elf_Addr target = sym_addr + addend;

	    ctx->current_address
	      = add_fixup_entry (&ctx->lst,
				 GRUB_PE32_REL_BASED_MIPS_HIGHEST,
				 addr, 0, ctx->current_address,
				 image_target);
	    /* Lo */
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_int16_t) target,
				 0, ctx->current_address,
				 image_target);
	    /* Hi */
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_int16_t) ((target & 0x8000UL) >> 16),
				 0, ctx->current_address,
				 image_target);
	    /* Higher */
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_int16_t) ((target & 0x80008000UL) >> 32),
				 0, ctx->current_address,
				 image_target);
	  }
	  break;
	default:
	  grub_util_error (_("relocation 0x%x is not implemented yet"),
			   (unsigned int) ELF_R_TYPE (info));
	  break;
	}
      break;
#endif
#if defined(MKIMAGE_ELF32)
    case EM_ARM:
      switch (ELF_R_TYPE (info))
	{
	case R_ARM_V4BX:
	  /* Relative relocations do not require fixup entries. */
	case R_ARM_JUMP24:
	case R_ARM_THM_CALL:
	case R_ARM_THM_JUMP19:
	case R_ARM_THM_JUMP24:
	case R_ARM_CALL:
	  {
	    grub_util_info ("  %s:  not adding fixup: 0x%08x : 0x%08x", __FUNCTION__, (unsigned int) addr, (unsigned int) ctx->current_address);
	  }
	  break;
	  /* Create fixup entry for PE/COFF loader */
	case R_ARM_ABS32:
	  {
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst,
				 GRUB_PE32_REL_BASED_HIGHLOW,
				 addr, 0, ctx->current_address,
				 image_target);
	  }
	  break;
	default:
	  grub_util_error (_("relocation 0x%x is not implemented yet"),
			   (unsigned int) ELF_R_TYPE (info));
	  break;
	}
      break;
#endif /* defined(MKIMAGE_ELF32) */
    case EM_RISCV:
      switch (ELF_R_TYPE (info))
	{
	case R_RISCV_32:
	  {
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst,
				 GRUB_PE32_REL_BASED_HIGHLOW,
				 addr, 0, ctx->current_address,
				 image_target);
	  }
	  break;
	case R_RISCV_64:
	  {
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst,
				 GRUB_PE32_REL_BASED_DIR64,
				 addr, 0, ctx->current_address,
				 image_target);
	  }
	  break;
	  /* Relative relocations do not require fixup entries. */
	case R_RISCV_BRANCH:
	case R_RISCV_JAL:
	case R_RISCV_CALL:
	case R_RISCV_PCREL_HI20:
	case R_RISCV_PCREL_LO12_I:
	case R_RISCV_PCREL_LO12_S:
	case R_RISCV_RVC_BRANCH:
	case R_RISCV_RVC_JUMP:
	case R_RISCV_ADD32:
	case R_RISCV_SUB32:
	  grub_util_info ("  %s:  not adding fixup: 0x%08x : 0x%08x", __FUNCTION__, (unsigned int) addr, (unsigned int) ctx->current_address);
	  break;
	case R_RISCV_HI20:
	  {
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst,
				 GRUB_PE32_REL_BASED_RISCV_HI20,
				 addr, 0, ctx->current_address,
				 image_target);
	  }
	  break;
	case R_RISCV_LO12_I:
	  {
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst,
				 GRUB_PE32_REL_BASED_RISCV_LOW12I,
				 addr, 0, ctx->current_address,
				 image_target);
	  }
	  break;
	case R_RISCV_LO12_S:
	  {
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst,
				 GRUB_PE32_REL_BASED_RISCV_LOW12S,
				 addr, 0, ctx->current_address,
				 image_target);
	  }
	  break;
	case R_RISCV_RELAX:
	  break;
	default:
	  grub_util_error (_("relocation 0x%x is not implemented yet"),
			   (unsigned int) ELF_R_TYPE (info));
	  break;
	}
      break;
    default:
      grub_util_error ("unknown machine type 0x%x", image_target->elf_target);
    }
}

static enum raw_reloc_type
classify_raw_reloc (Elf_Addr info,
		    const struct grub_install_image_target_desc *image_target)
{
    /* Necessary to relocate only absolute addresses.  */
  switch (image_target->elf_target)
    {
    case EM_ARM:
      switch (ELF_R_TYPE (info))
	{
	case R_ARM_V4BX:
	case R_ARM_JUMP24:
	case R_ARM_THM_CALL:
	case R_ARM_THM_JUMP19:
	case R_ARM_THM_JUMP24:
	case R_ARM_CALL:
	  return RAW_RELOC_NONE;
	case R_ARM_ABS32:
	  return RAW_RELOC_32;
	default:
	  grub_util_error (_("relocation 0x%x is not implemented yet"),
			   (unsigned int) ELF_R_TYPE (info));
	  break;
	}
      break;
    default:
      grub_util_error ("unknown machine type 0x%x", image_target->elf_target);
    }
}

static void
translate_relocation_raw (struct translate_context *ctx,
			  Elf_Addr addr,
			  Elf_Addr info,
			  const struct grub_install_image_target_desc *image_target)
{
  enum raw_reloc_type class = classify_raw_reloc (info, image_target);
  struct raw_reloc *rel;
  if (class == RAW_RELOC_NONE)
    return;
  rel = xmalloc (sizeof (*rel));
  rel->next = ctx->raw_relocs;
  rel->type = class;
  rel->offset = addr;
  ctx->raw_relocs = rel;
}

static void
translate_relocation (struct translate_context *ctx,
		      Elf_Addr addr,
		      Elf_Addr info,
		      Elf_Addr sym_addr,
		      Elf_Addr addend,
		      const struct grub_install_image_target_desc *image_target)
{
  if (image_target->id == IMAGE_EFI)
    translate_relocation_pe (ctx, addr, info, sym_addr, addend, image_target);
  else
    translate_relocation_raw (ctx, addr, info, image_target);
}

static void
finish_reloc_translation_pe (struct translate_context *ctx, struct grub_mkimage_layout *layout,
			     const struct grub_install_image_target_desc *image_target)
{
  ctx->current_address = add_fixup_entry (&ctx->lst, 0, 0, 1, ctx->current_address, image_target);

  {
    grub_uint8_t *ptr;
    layout->reloc_section = ptr = xmalloc (ctx->current_address);
    for (ctx->lst = ctx->lst0; ctx->lst; ctx->lst = ctx->lst->next)
      if (ctx->lst->state)
	{
	  memcpy (ptr, &ctx->lst->b, grub_target_to_host32 (ctx->lst->b.block_size));
	  ptr += grub_target_to_host32 (ctx->lst->b.block_size);
	}
    assert ((ctx->current_address + (grub_uint8_t *) layout->reloc_section) == ptr);
  }

  for (ctx->lst = ctx->lst0; ctx->lst; )
    {
      struct fixup_block_list *next;
      next = ctx->lst->next;
      free (ctx->lst);
      ctx->lst = next;
    }

  layout->reloc_size = ctx->current_address;
  if (image_target->elf_target == EM_ARM && layout->reloc_size > GRUB_KERNEL_ARM_STACK_SIZE)
    grub_util_error ("Reloc section (%d) is bigger than stack size (%d). "
		     "This breaks assembly assumptions. Please increase stack size",
		     (int) layout->reloc_size,
		     (int) GRUB_KERNEL_ARM_STACK_SIZE);
}

/*
  Layout:
  <type 0 relocations>
  <fffffffe>
  <type 1 relocations>
  <fffffffe>
  ...
  <type n relocations>
  <ffffffff>
  each relocation starts with 32-bit offset. Rest depends on relocation.
  mkimage stops when it sees first unknown type or end marker.
  This allows images to be created with mismatched mkimage and
  kernel as long as no relocations are present in kernel that mkimage
  isn't aware of (in which case mkimage aborts).
  This also allows simple assembly to do the relocs.
*/

#define RAW_SEPARATOR 0xfffffffe
#define RAW_END_MARKER 0xffffffff

static void
finish_reloc_translation_raw (struct translate_context *ctx, struct grub_mkimage_layout *layout,
			      const struct grub_install_image_target_desc *image_target)
{
  size_t count = 0, sz;
  enum raw_reloc_type highest = RAW_RELOC_NONE;
  enum raw_reloc_type curtype;
  struct raw_reloc *cur;
  grub_uint32_t *p;
  if (!ctx->raw_relocs)
    {
      layout->reloc_section = p = xmalloc (sizeof (grub_uint32_t));
      p[0] = RAW_END_MARKER;
      layout->reloc_size = sizeof (grub_uint32_t);
      return;
    }
  for (cur = ctx->raw_relocs; cur; cur = cur->next)
    {
      count++;
      if (cur->type > highest)
	highest = cur->type;
    }
  /* highest separators, count relocations and one end marker.  */
  sz = (highest + count + 1) * sizeof (grub_uint32_t);
  layout->reloc_section = p = xmalloc (sz);
  for (curtype = 0; curtype <= highest; curtype++)
    {
      /* Support for special cases would go here.  */
      for (cur = ctx->raw_relocs; cur; cur = cur->next)
	if (cur->type == curtype)
	  {
	    *p++ = cur->offset;
	  }
      *p++ = RAW_SEPARATOR;
    }
  *--p = RAW_END_MARKER;
  layout->reloc_size = sz;
}

static void
finish_reloc_translation (struct translate_context *ctx, struct grub_mkimage_layout *layout,
			  const struct grub_install_image_target_desc *image_target)
{
  if (image_target->id == IMAGE_EFI)
    finish_reloc_translation_pe (ctx, layout, image_target);
  else
    finish_reloc_translation_raw (ctx, layout, image_target);
}


static void
create_u64_fixups (struct translate_context *ctx,
		   Elf_Addr jumpers, grub_size_t njumpers,
		   const struct grub_install_image_target_desc *image_target)
{
  unsigned i;
  assert (image_target->id == IMAGE_EFI);
  for (i = 0; i < njumpers; i++)
    ctx->current_address = add_fixup_entry (&ctx->lst,
					    GRUB_PE32_REL_BASED_DIR64,
					    jumpers + 8 * i,
					    0, ctx->current_address,
					    image_target);
}

/* Make a .reloc section.  */
static void
make_reloc_section (Elf_Ehdr *e, struct grub_mkimage_layout *layout,
		    struct section_metadata *smd,
		    const struct grub_install_image_target_desc *image_target)
{
  unsigned i;
  Elf_Shdr *s;
  struct translate_context ctx;

  translate_reloc_start (&ctx, image_target);

  for (i = 0, s = smd->sections; i < smd->num_sections;
       i++, s = (Elf_Shdr *) ((char *) s + smd->section_entsize))
    if ((grub_target_to_host32 (s->sh_type) == SHT_REL) ||
        (grub_target_to_host32 (s->sh_type) == SHT_RELA))
      {
	Elf_Rela *r;
	Elf_Word rtab_size, r_size, num_rs;
	Elf_Off rtab_offset;
	Elf_Shdr *symtab_section;
	Elf_Addr section_address;
	Elf_Word j;

	if (!SUFFIX (is_kept_reloc_section) (s, image_target, smd))
	  {
	    grub_util_info ("not translating the skipped relocation section %s",
			    smd->strtab + grub_le_to_cpu32 (s->sh_name));
	    continue;
	  }

	symtab_section = (Elf_Shdr *) ((char *) smd->sections
					+ (grub_target_to_host32 (s->sh_link)
						* smd->section_entsize));

	grub_util_info ("translating the relocation section %s",
			smd->strtab + grub_le_to_cpu32 (s->sh_name));

	rtab_size = grub_target_to_host (s->sh_size);
	r_size = grub_target_to_host (s->sh_entsize);
	rtab_offset = grub_target_to_host (s->sh_offset);
	num_rs = rtab_size / r_size;

	section_address = smd->vaddrs[grub_le_to_cpu32 (s->sh_info)];

	for (j = 0, r = (Elf_Rela *) ((char *) e + rtab_offset);
	     j < num_rs;
	     j++, r = (Elf_Rela *) ((char *) r + r_size))
	  {
	    Elf_Addr info;
	    Elf_Addr offset;
	    Elf_Addr addr;
	    Elf_Addr sym_addr;
	    Elf_Addr addend;

	    offset = grub_target_to_host (r->r_offset);
	    if (image_target->elf_target == EM_MIPS && image_target->voidp_sizeof == 8)
	      info = ((grub_uint64_t) r->r_info << 32) |
		      (grub_uint32_t) grub_be_to_cpu64 (r->r_info);
	    else
	      info = grub_target_to_host (r->r_info);

	    sym_addr = SUFFIX (get_symbol_address) (e, symtab_section,
						    ELF_R_SYM (info), image_target);
	    addend = (s->sh_type == grub_target_to_host32 (SHT_RELA)) ?
		grub_target_to_host (r->r_addend) : 0;
	    addr = section_address + offset;

	    translate_relocation (&ctx, addr, info, sym_addr, addend, image_target);
	  }
      }

  if (image_target->elf_target == EM_IA_64)
    create_u64_fixups (&ctx,
		       layout->ia64jmp_off
		       + image_target->vaddr_offset,
		       2 * layout->ia64jmpnum,
		       image_target);
  if (image_target->elf_target == EM_IA_64 || image_target->elf_target == EM_AARCH64)
    create_u64_fixups (&ctx,
		       layout->got_off
		       + image_target->vaddr_offset,
		       (layout->got_size / 8),
		       image_target);

  finish_reloc_translation (&ctx, layout, image_target);
}

/* Determine if this section is a text section. Return false if this
   section is not allocated.  */
static int
SUFFIX (is_text_section) (Elf_Shdr *s, const struct grub_install_image_target_desc *image_target)
{
  if (!is_relocatable (image_target)
      && grub_target_to_host32 (s->sh_type) != SHT_PROGBITS)
    return 0;
  return ((grub_target_to_host (s->sh_flags) & (SHF_EXECINSTR | SHF_ALLOC))
	  == (SHF_EXECINSTR | SHF_ALLOC));
}

/* Determine if this section is a data section.  */
static int
SUFFIX (is_data_section) (Elf_Shdr *s, const struct grub_install_image_target_desc *image_target)
{
  if (!is_relocatable (image_target) 
      && grub_target_to_host32 (s->sh_type) != SHT_PROGBITS)
    return 0;
  return ((grub_target_to_host (s->sh_flags) & (SHF_EXECINSTR | SHF_ALLOC))
	  == SHF_ALLOC) && !(grub_target_to_host32 (s->sh_type) == SHT_NOBITS);
}

static int
SUFFIX (is_bss_section) (Elf_Shdr *s, const struct grub_install_image_target_desc *image_target)
{
  if (!is_relocatable (image_target))
    return 0;
  return ((grub_target_to_host (s->sh_flags) & (SHF_EXECINSTR | SHF_ALLOC))
	  == SHF_ALLOC) && (grub_target_to_host32 (s->sh_type) == SHT_NOBITS);
}

/* Determine if a section is going to be in the final output */
static int
SUFFIX (is_kept_section) (Elf_Shdr *s, const struct grub_install_image_target_desc *image_target)
{
  /* We keep .text and .data */
  if (SUFFIX (is_text_section) (s, image_target)
      || SUFFIX (is_data_section) (s, image_target))
    return 1;

  /*
   * And we keep .bss if we're producing PE binaries or the target doesn't
   * have a relocating loader.  Platforms other than EFI and U-boot shouldn't
   * have .bss in their binaries as we build with -Wl,-Ttext.
   */
  if (SUFFIX (is_bss_section) (s, image_target)
      && (image_target->id == IMAGE_EFI || !is_relocatable (image_target)))
    return 1;

  /* Otherwise this is not a section we're keeping in the final output. */
  return 0;
}

static int
SUFFIX (is_kept_reloc_section) (Elf_Shdr *s, const struct grub_install_image_target_desc *image_target,
				struct section_metadata *smd)
{
  int i;
  int r = 0;
  const char *name = smd->strtab + grub_host_to_target32 (s->sh_name);

  if (!strncmp (name, ".rela.", 6))
    name += 5;
  else if (!strncmp (name, ".rel.", 5))
    name += 4;
  else
    return 1;

  for (i = 0, s = smd->sections; i < smd->num_sections;
       i++, s = (Elf_Shdr *) ((char *) s + smd->section_entsize))
    {
      const char *sname = smd->strtab + grub_host_to_target32 (s->sh_name);
      if (strcmp (sname, name))
	continue;

      return SUFFIX (is_kept_section) (s, image_target);
    }

  return r;
}

/* Return if the ELF header is valid.  */
static int
SUFFIX (check_elf_header) (Elf_Ehdr *e, size_t size, const struct grub_install_image_target_desc *image_target)
{
  if (size < sizeof (*e)
      || e->e_ident[EI_MAG0] != ELFMAG0
      || e->e_ident[EI_MAG1] != ELFMAG1
      || e->e_ident[EI_MAG2] != ELFMAG2
      || e->e_ident[EI_MAG3] != ELFMAG3
      || e->e_ident[EI_VERSION] != EV_CURRENT
      || e->e_ident[EI_CLASS] != ELFCLASSXX
      || e->e_version != grub_host_to_target32 (EV_CURRENT))
    return 0;

  return 1;
}

static Elf_Addr
SUFFIX (put_section) (Elf_Shdr *s, int i,
		      Elf_Addr current_address,
		      struct section_metadata *smd,
		      const struct grub_install_image_target_desc *image_target)
{
	Elf_Word align = grub_host_to_target_addr (s->sh_addralign);
	const char *name = smd->strtab + grub_host_to_target32 (s->sh_name);

	if (align)
	  current_address = ALIGN_UP (current_address + image_target->vaddr_offset,
				      align)
	    - image_target->vaddr_offset;

	grub_util_info ("locating the section %s at 0x%"
			GRUB_HOST_PRIxLONG_LONG,
			name, (unsigned long long) current_address);
	if (!is_relocatable (image_target))
	  current_address = grub_host_to_target_addr (s->sh_addr)
			    - image_target->link_addr;
	smd->addrs[i] = current_address;
	current_address += grub_host_to_target_addr (s->sh_size);
	return current_address;
}

/*
 * Locate section addresses by merging code sections and data sections
 * into .text and .data, respectively.
 */
static void
SUFFIX (locate_sections) (Elf_Ehdr *e, const char *kernel_path,
			  struct section_metadata *smd,
			  struct grub_mkimage_layout *layout,
			  const struct grub_install_image_target_desc *image_target)
{
  int i;
  Elf_Shdr *s;

  layout->align = 1;
  /* Page-aligning simplifies relocation handling.  */
  if (image_target->elf_target == EM_AARCH64)
    layout->align = 4096;

  layout->kernel_size = 0;

  for (i = 0, s = smd->sections;
       i < smd->num_sections;
       i++, s = (Elf_Shdr *) ((char *) s + smd->section_entsize))
    if ((grub_target_to_host (s->sh_flags) & SHF_ALLOC)
	&& grub_host_to_target32 (s->sh_addralign) > layout->align)
      layout->align = grub_host_to_target32 (s->sh_addralign);

  /* .text */
  for (i = 0, s = smd->sections;
       i < smd->num_sections;
       i++, s = (Elf_Shdr *) ((char *) s + smd->section_entsize))
    if (SUFFIX (is_text_section) (s, image_target))
      {
	layout->kernel_size = SUFFIX (put_section) (s, i, layout->kernel_size,
						smd, image_target);
	if (!is_relocatable (image_target) &&
	    grub_host_to_target_addr (s->sh_addr) != image_target->link_addr)
	  {
	    char *msg
	      = grub_xasprintf (_("`%s' is miscompiled: its start address is 0x%llx"
				  " instead of 0x%llx: ld.gold bug?"),
				kernel_path,
				(unsigned long long) grub_host_to_target_addr (s->sh_addr),
				(unsigned long long) image_target->link_addr);
	    grub_util_error ("%s", msg);
	  }
      }

#ifdef MKIMAGE_ELF32
  if (image_target->elf_target == EM_ARM)
    {
      grub_size_t tramp;

      layout->kernel_size = ALIGN_UP (layout->kernel_size, 16);

      tramp = arm_get_trampoline_size (e, smd->sections, smd->section_entsize,
				       smd->num_sections, image_target);

      layout->tramp_off = layout->kernel_size;
      layout->kernel_size += ALIGN_UP (tramp, 16);
    }
#endif

  layout->kernel_size = ALIGN_UP (layout->kernel_size + image_target->vaddr_offset,
			      image_target->section_align)
    - image_target->vaddr_offset;
  layout->exec_size = layout->kernel_size;

  /* .data */
  for (i = 0, s = smd->sections;
       i < smd->num_sections;
       i++, s = (Elf_Shdr *) ((char *) s + smd->section_entsize))
    if (SUFFIX (is_data_section) (s, image_target))
      layout->kernel_size = SUFFIX (put_section) (s, i, layout->kernel_size, smd,
						  image_target);

  layout->bss_start = layout->kernel_size;
  layout->end = layout->kernel_size;
  
  /* .bss */
  for (i = 0, s = smd->sections;
       i < smd->num_sections;
       i++, s = (Elf_Shdr *) ((char *) s + smd->section_entsize))
    {
      if (SUFFIX (is_bss_section) (s, image_target))
	layout->end = SUFFIX (put_section) (s, i, layout->end, smd, image_target);

      /*
       * This must to be in the last time this function passes through the loop.
       */
      smd->vaddrs[i] = smd->addrs[i] + image_target->vaddr_offset;
    }

  layout->end = ALIGN_UP (layout->end + image_target->vaddr_offset,
			      image_target->section_align) - image_target->vaddr_offset;
  /* Explicitly initialize BSS
     when producing PE32 to avoid a bug in EFI implementations.
     Platforms other than EFI and U-boot shouldn't have .bss in
     their binaries as we build with -Wl,-Ttext.
  */
  if (image_target->id == IMAGE_EFI || !is_relocatable (image_target))
    layout->kernel_size = layout->end;
}

char *
SUFFIX (grub_mkimage_load_image) (const char *kernel_path,
				  size_t total_module_size,
				  struct grub_mkimage_layout *layout,
				  const struct grub_install_image_target_desc *image_target)
{
  char *kernel_img, *out_img;
  struct section_metadata smd = { 0, 0, 0, 0, 0, 0, 0 };
  Elf_Ehdr *e;
  int i;
  Elf_Shdr *s;
  Elf_Off section_offset;
  grub_size_t kernel_size;

  grub_memset (layout, 0, sizeof (*layout));

  layout->start_address = 0;

  kernel_size = grub_util_get_image_size (kernel_path);
  kernel_img = xmalloc (kernel_size);
  grub_util_load_image (kernel_path, kernel_img);

  e = (Elf_Ehdr *) kernel_img;
  if (! SUFFIX (check_elf_header) (e, kernel_size, image_target))
    grub_util_error ("invalid ELF header");

  section_offset = grub_target_to_host (e->e_shoff);
  smd.section_entsize = grub_target_to_host16 (e->e_shentsize);
  smd.num_sections = grub_target_to_host16 (e->e_shnum);

  if (kernel_size < section_offset
		    + (grub_uint32_t) smd.section_entsize * smd.num_sections)
    grub_util_error (_("premature end of file %s"), kernel_path);

  smd.sections = (Elf_Shdr *) (kernel_img + section_offset);

  /* Relocate sections then symbols in the virtual address space.  */
  s = (Elf_Shdr *) ((char *) smd.sections
		      + grub_host_to_target16 (e->e_shstrndx) * smd.section_entsize);
  smd.strtab = (char *) e + grub_host_to_target_addr (s->sh_offset);

  smd.addrs = xmalloc (sizeof (*smd.addrs) * smd.num_sections);
  memset (smd.addrs, 0, sizeof (*smd.addrs) * smd.num_sections);
  smd.vaddrs = xmalloc (sizeof (*smd.vaddrs) * smd.num_sections);
  memset (smd.vaddrs, 0, sizeof (*smd.vaddrs) * smd.num_sections);

  SUFFIX (locate_sections) (e, kernel_path, &smd, layout, image_target);

  if (!is_relocatable (image_target))
    {
      Elf_Addr current_address = layout->kernel_size;

      for (i = 0, s = smd.sections;
	   i < smd.num_sections;
	   i++, s = (Elf_Shdr *) ((char *) s + smd.section_entsize))
	if (grub_target_to_host32 (s->sh_type) == SHT_NOBITS)
	  {
	    Elf_Word sec_align = grub_host_to_target_addr (s->sh_addralign);
	    const char *name = smd.strtab + grub_host_to_target32 (s->sh_name);

	    if (sec_align)
	      current_address = ALIGN_UP (current_address
					  + image_target->vaddr_offset,
					  sec_align)
		- image_target->vaddr_offset;

	    grub_util_info ("locating the section %s at 0x%"
			    GRUB_HOST_PRIxLONG_LONG,
			    name, (unsigned long long) current_address);
	    if (!is_relocatable (image_target))
	      current_address = grub_host_to_target_addr (s->sh_addr)
		- image_target->link_addr;

	    smd.vaddrs[i] = current_address
	      + image_target->vaddr_offset;
	    current_address += grub_host_to_target_addr (s->sh_size);
	  }
      current_address = ALIGN_UP (current_address + image_target->vaddr_offset,
				  image_target->section_align)
	- image_target->vaddr_offset;
      layout->bss_size = current_address - layout->kernel_size;
    }
  else
    layout->bss_size = 0;

  if (image_target->id == IMAGE_SPARC64_AOUT
      || image_target->id == IMAGE_SPARC64_RAW
      || image_target->id == IMAGE_UBOOT
      || image_target->id == IMAGE_COREBOOT
      || image_target->id == IMAGE_SPARC64_CDCORE)
    layout->kernel_size = ALIGN_UP (layout->kernel_size, image_target->mod_align);

  if (is_relocatable (image_target))
    {
      smd.symtab = NULL;
      for (i = 0, s = smd.sections;
	   i < smd.num_sections;
	   i++, s = (Elf_Shdr *) ((char *) s + smd.section_entsize))
	if (s->sh_type == grub_host_to_target32 (SHT_SYMTAB))
	  {
	    smd.symtab = s;
	    break;
	  }
      if (! smd.symtab)
	grub_util_error ("%s", _("no symbol table"));
#ifdef MKIMAGE_ELF64
      if (image_target->elf_target == EM_IA_64)
	{
	  grub_size_t tramp;

	  layout->kernel_size = ALIGN_UP (layout->kernel_size, 16);

	  grub_ia64_dl_get_tramp_got_size (e, &tramp, &layout->got_size);

	  layout->tramp_off = layout->kernel_size;
	  layout->kernel_size += ALIGN_UP (tramp, 16);

	  layout->ia64jmp_off = layout->kernel_size;
	  layout->ia64jmpnum = SUFFIX (count_funcs) (e, smd.symtab,
						     image_target);
	  layout->kernel_size += 16 * layout->ia64jmpnum;

	  layout->got_off = layout->kernel_size;
	  layout->kernel_size += ALIGN_UP (layout->got_size, 16);
	}
      if (image_target->elf_target == EM_AARCH64)
	{
	  grub_size_t tramp;

	  layout->kernel_size = ALIGN_UP (layout->kernel_size, 16);

	  grub_arm64_dl_get_tramp_got_size (e, &tramp, &layout->got_size);

	  layout->got_off = layout->kernel_size;
	  layout->kernel_size += ALIGN_UP (layout->got_size, 16);
	}
#endif
    }
  else
    {
      layout->reloc_size = 0;
      layout->reloc_section = NULL;
    }

  out_img = xmalloc (layout->kernel_size + total_module_size);
  memset (out_img, 0, layout->kernel_size + total_module_size);

  if (is_relocatable (image_target))
    {
      layout->start_address = SUFFIX (relocate_symbols) (e, &smd,
				  (char *) out_img + layout->ia64jmp_off,
				  layout->ia64jmp_off + image_target->vaddr_offset,
				  layout->bss_start, layout->end, image_target);

      if (layout->start_address == (Elf_Addr) -1)
	grub_util_error ("start symbol is not defined");

      /* Resolve addrs in the virtual address space.  */
      SUFFIX (relocate_addrs) (e, &smd, out_img, layout->tramp_off,
				   layout->got_off, image_target);

      make_reloc_section (e, layout, &smd, image_target);
      if (image_target->id != IMAGE_EFI)
	{
	  out_img = xrealloc (out_img, layout->kernel_size + total_module_size
			      + ALIGN_UP (layout->reloc_size, image_target->mod_align));
	  memcpy (out_img + layout->kernel_size, layout->reloc_section, layout->reloc_size);
	  memset (out_img + layout->kernel_size + layout->reloc_size, 0,
		  total_module_size + ALIGN_UP (layout->reloc_size, image_target->mod_align) - layout->reloc_size);
	  layout->kernel_size += ALIGN_UP (layout->reloc_size, image_target->mod_align);
	}
    }

  for (i = 0, s = smd.sections;
       i < smd.num_sections;
       i++, s = (Elf_Shdr *) ((char *) s + smd.section_entsize))
    if (SUFFIX (is_kept_section) (s, image_target))
      {
	if (grub_target_to_host32 (s->sh_type) == SHT_NOBITS)
	  memset (out_img + smd.addrs[i], 0,
		  grub_host_to_target_addr (s->sh_size));
	else
	  memcpy (out_img + smd.addrs[i],
		  kernel_img + grub_host_to_target_addr (s->sh_offset),
		  grub_host_to_target_addr (s->sh_size));
      }
  free (kernel_img);

  free (smd.vaddrs);
  smd.vaddrs = NULL;
  free (smd.addrs);
  smd.addrs = NULL;

  return out_img;
}
