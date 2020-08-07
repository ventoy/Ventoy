/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
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

#ifndef GRUB_KERNEL_HEADER
#define GRUB_KERNEL_HEADER	1

#include <grub/types.h>
#include <grub/symbol.h>

enum
{
  OBJ_TYPE_ELF,
  OBJ_TYPE_MEMDISK,
  OBJ_TYPE_CONFIG,
  OBJ_TYPE_PREFIX,
  OBJ_TYPE_PUBKEY,
  OBJ_TYPE_DTB
};

/* The module header.  */
struct grub_module_header
{
  /* The type of object.  */
  grub_uint32_t type;
  /* The size of object (including this header).  */
  grub_uint32_t size;
};

/* "gmim" (GRUB Module Info Magic).  */
#define GRUB_MODULE_MAGIC 0x676d696d

struct grub_module_info32
{
  /* Magic number so we know we have modules present.  */
  grub_uint32_t magic;
  /* The offset of the modules.  */
  grub_uint32_t offset;
  /* The size of all modules plus this header.  */
  grub_uint32_t size;
};

struct grub_module_info64
{
  /* Magic number so we know we have modules present.  */
  grub_uint32_t magic;
  grub_uint32_t padding;
  /* The offset of the modules.  */
  grub_uint64_t offset;
  /* The size of all modules plus this header.  */
  grub_uint64_t size;
};

#ifndef GRUB_UTIL
/* Space isn't reusable on some platforms.  */
/* On Qemu the preload space is readonly.  */
/* On emu there is no preload space.  */
/* On ieee1275 our code assumes that heap is p=v which isn't guaranteed for module space.  */
#if defined (GRUB_MACHINE_QEMU) || defined (GRUB_MACHINE_EMU) \
  || defined (GRUB_MACHINE_EFI) \
  || (defined (GRUB_MACHINE_IEEE1275) && !defined (__sparc__))
#define GRUB_KERNEL_PRELOAD_SPACE_REUSABLE 0
#endif

#if defined (GRUB_MACHINE_PCBIOS) || defined (GRUB_MACHINE_COREBOOT) \
  || defined (GRUB_MACHINE_MULTIBOOT) || defined (GRUB_MACHINE_MIPS_QEMU_MIPS) \
  || defined (GRUB_MACHINE_MIPS_LOONGSON) || defined (GRUB_MACHINE_ARC) \
  || (defined (__sparc__) && defined (GRUB_MACHINE_IEEE1275)) \
  || defined (GRUB_MACHINE_UBOOT) || defined (GRUB_MACHINE_XEN) \
  || defined(GRUB_MACHINE_XEN_PVH)
/* FIXME: stack is between 2 heap regions. Move it.  */
#define GRUB_KERNEL_PRELOAD_SPACE_REUSABLE 1
#endif

#ifndef GRUB_KERNEL_PRELOAD_SPACE_REUSABLE
#error "Please check if preload space is reusable on this platform!"
#endif

#if GRUB_TARGET_SIZEOF_VOID_P == 8
#define grub_module_info grub_module_info64
#else
#define grub_module_info grub_module_info32
#endif

extern grub_addr_t EXPORT_VAR (grub_modbase);

void EXPORT_FUNC(ventoy_env_hook_root)(int hook);

#define FOR_MODULES(var)  for (\
  var = (grub_modbase && ((((struct grub_module_info *) grub_modbase)->magic) == GRUB_MODULE_MAGIC)) ? (struct grub_module_header *) \
    (grub_modbase + (((struct grub_module_info *) grub_modbase)->offset)) : 0;\
  var && (grub_addr_t) var \
    < (grub_modbase + (((struct grub_module_info *) grub_modbase)->size));    \
  var = (struct grub_module_header *)					\
    (((grub_uint32_t *) var) + ((((struct grub_module_header *) var)->size + sizeof (grub_addr_t) - 1) / sizeof (grub_addr_t)) * (sizeof (grub_addr_t) / sizeof (grub_uint32_t))))

grub_addr_t grub_modules_get_end (void);

#endif

/* The start point of the C code.  */
void grub_main (void) __attribute__ ((noreturn));

/* The machine-specific initialization. This must initialize memory.  */
void grub_machine_init (void);

/* The machine-specific finalization.  */
void EXPORT_FUNC(grub_machine_fini) (int flags);

/* The machine-specific prefix initialization.  */
void
grub_machine_get_bootlocation (char **device, char **path);

/* Register all the exported symbols. This is automatically generated.  */
void grub_register_exported_symbols (void);

extern void (*EXPORT_VAR(grub_net_poll_cards_idle)) (void);

#endif /* ! GRUB_KERNEL_HEADER */
