/* dl-mips64.c - arch-dependent part of loadable module support */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2005,2007,2009,2017  Free Software Foundation, Inc.
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
#include <grub/elf.h>
#include <grub/misc.h>
#include <grub/err.h>
#include <grub/cpu/types.h>
#include <grub/mm.h>
#include <grub/i18n.h>

/* Check if EHDR is a valid ELF header.  */
grub_err_t
grub_arch_dl_check_header (void *ehdr)
{
  Elf_Ehdr *e = ehdr;

  /* Check the magic numbers.  */
#ifdef GRUB_CPU_WORDS_BIGENDIAN
  if (e->e_ident[EI_CLASS] != ELFCLASS64
      || e->e_ident[EI_DATA] != ELFDATA2MSB
      || e->e_machine != EM_MIPS)
#else
  if (e->e_ident[EI_CLASS] != ELFCLASS64
      || e->e_ident[EI_DATA] != ELFDATA2LSB
      || e->e_machine != EM_MIPS)
#endif
    return grub_error (GRUB_ERR_BAD_OS, N_("invalid arch-dependent ELF magic"));

  return GRUB_ERR_NONE;
}

#pragma GCC diagnostic ignored "-Wcast-align"

grub_err_t
grub_arch_dl_get_tramp_got_size (const void *ehdr __attribute__ ((unused)),
				 grub_size_t *tramp, grub_size_t *got)
{
  *tramp = 0;
  *got = 0;
  return GRUB_ERR_NONE;
}

/* Relocate symbols.  */
grub_err_t
grub_arch_dl_relocate_symbols (grub_dl_t mod, void *ehdr,
			       Elf_Shdr *s, grub_dl_segment_t seg)
{
  Elf_Ehdr *e = ehdr;
  Elf_Rel *rel, *max;

  for (rel = (Elf_Rel *) ((char *) e + s->sh_offset),
	 max = (Elf_Rel *) ((char *) rel + s->sh_size);
       rel < max;
       rel = (Elf_Rel *) ((char *) rel + s->sh_entsize))
    {
      grub_uint8_t *addr;
      Elf_Sym *sym;
      Elf_Addr r_info;
      grub_uint64_t sym_value;

      if (seg->size < rel->r_offset)
	return grub_error (GRUB_ERR_BAD_MODULE,
			   "reloc offset is out of the segment");

      r_info = ((grub_uint64_t) rel->r_info << 32) |
              (grub_uint32_t) grub_be_to_cpu64 (rel->r_info);

      addr = (grub_uint8_t *) ((char *) seg->addr + rel->r_offset);
      sym = (Elf_Sym *) ((char *) mod->symtab
			 + mod->symsize * ELF_R_SYM (r_info));
      sym_value = sym->st_value;
      if (s->sh_type == SHT_RELA)
	{
	  sym_value += ((Elf_Rela *) rel)->r_addend;
	}
      switch (ELF_R_TYPE (r_info))
	{
	case R_MIPS_64:
	  *(grub_uint64_t *) addr += sym_value;
	  break;
	case R_MIPS_32:
	  *(grub_uint32_t *) addr += sym_value;
	  break;
	case R_MIPS_26:
	  {
	    grub_uint32_t value;
	    grub_uint32_t raw;
	    raw = (*(grub_uint32_t *) addr) & 0x3ffffff;
	    value = raw << 2;
	    value += sym_value;
	    raw = (value >> 2) & 0x3ffffff;

	    *(grub_uint32_t *) addr =
	      raw | ((*(grub_uint32_t *) addr) & 0xfc000000);
	  }
	  break;
	case R_MIPS_LO16:
#ifdef GRUB_CPU_WORDS_BIGENDIAN
	  addr += 2;
#endif
	  *(grub_uint16_t *) addr = (grub_int16_t) sym_value;
	  break;
	case R_MIPS_HI16:
#ifdef GRUB_CPU_WORDS_BIGENDIAN
	  addr += 2;
#endif
	  *(grub_uint16_t *) addr = (grub_int16_t) ((sym_value + 0x8000UL) >> 16);
	  break;
	case R_MIPS_HIGHER:
#ifdef GRUB_CPU_WORDS_BIGENDIAN
	  addr += 2;
#endif
	  *(grub_uint16_t *) addr = (grub_int16_t) ((sym_value + 0x80008000UL) >> 32);
	  break;
	case R_MIPS_HIGHEST:
#ifdef GRUB_CPU_WORDS_BIGENDIAN
	  addr += 2;
#endif
	  *(grub_uint16_t *) addr = (grub_uint16_t) ((sym_value + 0x800080008000UL) >> 48);
	  break;
	default:
	  {
	    return grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET,
			       N_("relocation 0x%x is not implemented yet"),
			       ELF_R_TYPE (r_info));
	  }
	  break;
	}
    }

  return GRUB_ERR_NONE;
}

