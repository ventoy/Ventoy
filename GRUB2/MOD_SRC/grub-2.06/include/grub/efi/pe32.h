/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007,2008,2009  Free Software Foundation, Inc.
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

#ifndef GRUB_EFI_PE32_HEADER
#define GRUB_EFI_PE32_HEADER	1

#include <grub/types.h>
#include <grub/efi/memory.h>

/* The MSDOS compatibility stub. This was copied from the output of
   objcopy, and it is not necessary to care about what this means.  */
#define GRUB_PE32_MSDOS_STUB \
  { \
    0x4d, 0x5a, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00, \
    0x04, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, \
    0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
    0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, \
    0x0e, 0x1f, 0xba, 0x0e, 0x00, 0xb4, 0x09, 0xcd, \
    0x21, 0xb8, 0x01, 0x4c, 0xcd, 0x21, 0x54, 0x68, \
    0x69, 0x73, 0x20, 0x70, 0x72, 0x6f, 0x67, 0x72, \
    0x61, 0x6d, 0x20, 0x63, 0x61, 0x6e, 0x6e, 0x6f, \
    0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6e, \
    0x20, 0x69, 0x6e, 0x20, 0x44, 0x4f, 0x53, 0x20, \
    0x6d, 0x6f, 0x64, 0x65, 0x2e, 0x0d, 0x0d, 0x0a, \
    0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  \
  }

#define GRUB_PE32_MSDOS_STUB_SIZE	0x80

#define GRUB_PE32_MAGIC			0x5a4d

/* According to the spec, the minimal alignment is 512 bytes...
   But some examples (such as EFI drivers in the Intel
   Sample Implementation) use 32 bytes (0x20) instead, and it seems
   to be working.

   However, there is firmware showing up in the field now with
   page alignment constraints to guarantee that page protection
   bits take effect. Because currently existing GRUB code can not
   properly distinguish between in-memory and in-file layout, let's
   bump all alignment to GRUB_EFI_PAGE_SIZE. */
#define GRUB_PE32_SECTION_ALIGNMENT	GRUB_EFI_PAGE_SIZE
#define GRUB_PE32_FILE_ALIGNMENT	GRUB_PE32_SECTION_ALIGNMENT

struct grub_pe32_coff_header
{
  grub_uint16_t machine;
  grub_uint16_t num_sections;
  grub_uint32_t time;
  grub_uint32_t symtab_offset;
  grub_uint32_t num_symbols;
  grub_uint16_t optional_header_size;
  grub_uint16_t characteristics;
};

#define GRUB_PE32_MACHINE_I386			0x14c
#define GRUB_PE32_MACHINE_MIPS			0x166
#define GRUB_PE32_MACHINE_IA64			0x200
#define GRUB_PE32_MACHINE_X86_64		0x8664
#define GRUB_PE32_MACHINE_ARMTHUMB_MIXED	0x01c2
#define GRUB_PE32_MACHINE_ARM64			0xAA64
#define GRUB_PE32_MACHINE_RISCV32		0x5032
#define GRUB_PE32_MACHINE_RISCV64		0x5064

#define GRUB_PE32_RELOCS_STRIPPED		0x0001
#define GRUB_PE32_EXECUTABLE_IMAGE		0x0002
#define GRUB_PE32_LINE_NUMS_STRIPPED		0x0004
#define GRUB_PE32_LOCAL_SYMS_STRIPPED		0x0008
#define GRUB_PE32_AGGRESSIVE_WS_TRIM		0x0010
#define GRUB_PE32_LARGE_ADDRESS_AWARE		0x0020
#define GRUB_PE32_16BIT_MACHINE			0x0040
#define GRUB_PE32_BYTES_REVERSED_LO		0x0080
#define GRUB_PE32_32BIT_MACHINE			0x0100
#define GRUB_PE32_DEBUG_STRIPPED		0x0200
#define GRUB_PE32_REMOVABLE_RUN_FROM_SWAP	0x0400
#define GRUB_PE32_SYSTEM			0x1000
#define GRUB_PE32_DLL				0x2000
#define GRUB_PE32_UP_SYSTEM_ONLY		0x4000
#define GRUB_PE32_BYTES_REVERSED_HI		0x8000

struct grub_pe32_data_directory
{
  grub_uint32_t rva;
  grub_uint32_t size;
};

struct grub_pe32_optional_header
{
  grub_uint16_t magic;
  grub_uint8_t major_linker_version;
  grub_uint8_t minor_linker_version;
  grub_uint32_t code_size;
  grub_uint32_t data_size;
  grub_uint32_t bss_size;
  grub_uint32_t entry_addr;
  grub_uint32_t code_base;

  grub_uint32_t data_base;
  grub_uint32_t image_base;

  grub_uint32_t section_alignment;
  grub_uint32_t file_alignment;
  grub_uint16_t major_os_version;
  grub_uint16_t minor_os_version;
  grub_uint16_t major_image_version;
  grub_uint16_t minor_image_version;
  grub_uint16_t major_subsystem_version;
  grub_uint16_t minor_subsystem_version;
  grub_uint32_t reserved;
  grub_uint32_t image_size;
  grub_uint32_t header_size;
  grub_uint32_t checksum;
  grub_uint16_t subsystem;
  grub_uint16_t dll_characteristics;

  grub_uint32_t stack_reserve_size;
  grub_uint32_t stack_commit_size;
  grub_uint32_t heap_reserve_size;
  grub_uint32_t heap_commit_size;

  grub_uint32_t loader_flags;
  grub_uint32_t num_data_directories;

  /* Data directories.  */
  struct grub_pe32_data_directory export_table;
  struct grub_pe32_data_directory import_table;
  struct grub_pe32_data_directory resource_table;
  struct grub_pe32_data_directory exception_table;
  struct grub_pe32_data_directory certificate_table;
  struct grub_pe32_data_directory base_relocation_table;
  struct grub_pe32_data_directory debug;
  struct grub_pe32_data_directory architecture;
  struct grub_pe32_data_directory global_ptr;
  struct grub_pe32_data_directory tls_table;
  struct grub_pe32_data_directory load_config_table;
  struct grub_pe32_data_directory bound_import;
  struct grub_pe32_data_directory iat;
  struct grub_pe32_data_directory delay_import_descriptor;
  struct grub_pe32_data_directory com_runtime_header;
  struct grub_pe32_data_directory reserved_entry;
};

struct grub_pe64_optional_header
{
  grub_uint16_t magic;
  grub_uint8_t major_linker_version;
  grub_uint8_t minor_linker_version;
  grub_uint32_t code_size;
  grub_uint32_t data_size;
  grub_uint32_t bss_size;
  grub_uint32_t entry_addr;
  grub_uint32_t code_base;

  grub_uint64_t image_base;

  grub_uint32_t section_alignment;
  grub_uint32_t file_alignment;
  grub_uint16_t major_os_version;
  grub_uint16_t minor_os_version;
  grub_uint16_t major_image_version;
  grub_uint16_t minor_image_version;
  grub_uint16_t major_subsystem_version;
  grub_uint16_t minor_subsystem_version;
  grub_uint32_t reserved;
  grub_uint32_t image_size;
  grub_uint32_t header_size;
  grub_uint32_t checksum;
  grub_uint16_t subsystem;
  grub_uint16_t dll_characteristics;

  grub_uint64_t stack_reserve_size;
  grub_uint64_t stack_commit_size;
  grub_uint64_t heap_reserve_size;
  grub_uint64_t heap_commit_size;

  grub_uint32_t loader_flags;
  grub_uint32_t num_data_directories;

  /* Data directories.  */
  struct grub_pe32_data_directory export_table;
  struct grub_pe32_data_directory import_table;
  struct grub_pe32_data_directory resource_table;
  struct grub_pe32_data_directory exception_table;
  struct grub_pe32_data_directory certificate_table;
  struct grub_pe32_data_directory base_relocation_table;
  struct grub_pe32_data_directory debug;
  struct grub_pe32_data_directory architecture;
  struct grub_pe32_data_directory global_ptr;
  struct grub_pe32_data_directory tls_table;
  struct grub_pe32_data_directory load_config_table;
  struct grub_pe32_data_directory bound_import;
  struct grub_pe32_data_directory iat;
  struct grub_pe32_data_directory delay_import_descriptor;
  struct grub_pe32_data_directory com_runtime_header;
  struct grub_pe32_data_directory reserved_entry;
};

#define GRUB_PE32_PE32_MAGIC	0x10b
#define GRUB_PE32_PE64_MAGIC	0x20b

#define GRUB_PE32_SUBSYSTEM_EFI_APPLICATION	10

#define GRUB_PE32_NUM_DATA_DIRECTORIES	16

struct grub_pe32_section_table
{
  char name[8];
  grub_uint32_t virtual_size;
  grub_uint32_t virtual_address;
  grub_uint32_t raw_data_size;
  grub_uint32_t raw_data_offset;
  grub_uint32_t relocations_offset;
  grub_uint32_t line_numbers_offset;
  grub_uint16_t num_relocations;
  grub_uint16_t num_line_numbers;
  grub_uint32_t characteristics;
};

#define GRUB_PE32_SCN_CNT_CODE			0x00000020
#define GRUB_PE32_SCN_CNT_INITIALIZED_DATA	0x00000040
#define GRUB_PE32_SCN_MEM_DISCARDABLE		0x02000000
#define GRUB_PE32_SCN_MEM_EXECUTE		0x20000000
#define GRUB_PE32_SCN_MEM_READ			0x40000000
#define GRUB_PE32_SCN_MEM_WRITE			0x80000000

#define GRUB_PE32_SCN_ALIGN_1BYTES		0x00100000
#define GRUB_PE32_SCN_ALIGN_2BYTES		0x00200000
#define GRUB_PE32_SCN_ALIGN_4BYTES		0x00300000
#define GRUB_PE32_SCN_ALIGN_8BYTES		0x00400000
#define GRUB_PE32_SCN_ALIGN_16BYTES		0x00500000
#define GRUB_PE32_SCN_ALIGN_32BYTES		0x00600000
#define GRUB_PE32_SCN_ALIGN_64BYTES		0x00700000

#define GRUB_PE32_SCN_ALIGN_SHIFT		20
#define GRUB_PE32_SCN_ALIGN_MASK		7

#define GRUB_PE32_SIGNATURE_SIZE 4

struct grub_pe32_header
{
  /* This should be filled in with GRUB_PE32_MSDOS_STUB.  */
  grub_uint8_t msdos_stub[GRUB_PE32_MSDOS_STUB_SIZE];

  /* This is always PE\0\0.  */
  char signature[GRUB_PE32_SIGNATURE_SIZE];

  /* The COFF file header.  */
  struct grub_pe32_coff_header coff_header;

#if GRUB_TARGET_SIZEOF_VOID_P == 8
  /* The Optional header.  */
  struct grub_pe64_optional_header optional_header;
#else
  /* The Optional header.  */
  struct grub_pe32_optional_header optional_header;
#endif
};

struct grub_pe32_fixup_block
{
  grub_uint32_t page_rva;
  grub_uint32_t block_size;
  grub_uint16_t entries[0];
};

#define GRUB_PE32_FIXUP_ENTRY(type, offset)	(((type) << 12) | (offset))

#define GRUB_PE32_REL_BASED_ABSOLUTE	0
#define GRUB_PE32_REL_BASED_HIGH	1
#define GRUB_PE32_REL_BASED_LOW		2
#define GRUB_PE32_REL_BASED_HIGHLOW	3
#define GRUB_PE32_REL_BASED_HIGHADJ	4
#define GRUB_PE32_REL_BASED_MIPS_JMPADDR 5
#define GRUB_PE32_REL_BASED_MIPS_LOW    6
#define GRUB_PE32_REL_BASED_MIPS_HIGH   4
#define GRUB_PE32_REL_BASED_MIPS_HIGHER 7
#define GRUB_PE32_REL_BASED_MIPS_HIGHEST 8
#define GRUB_PE32_REL_BASED_ARM_MOV32A  5
#define GRUB_PE32_REL_BASED_RISCV_HI20	5
#define GRUB_PE32_REL_BASED_SECTION	6
#define GRUB_PE32_REL_BASED_REL		7
#define GRUB_PE32_REL_BASED_ARM_MOV32T  7
#define GRUB_PE32_REL_BASED_RISCV_LOW12I 7
#define GRUB_PE32_REL_BASED_RISCV_LOW12S 8
#define GRUB_PE32_REL_BASED_IA64_IMM64	9
#define GRUB_PE32_REL_BASED_DIR64	10
#define GRUB_PE32_REL_BASED_HIGH3ADJ	11

struct grub_pe32_symbol
{
  union
  {
    char short_name[8];
    grub_uint32_t long_name[2];
  };

  grub_uint32_t value;
  grub_uint16_t section;
  grub_uint16_t type;
  grub_uint8_t storage_class;
  grub_uint8_t num_aux;
} GRUB_PACKED;

#define GRUB_PE32_SYM_CLASS_EXTERNAL	2
#define GRUB_PE32_SYM_CLASS_STATIC	3
#define GRUB_PE32_SYM_CLASS_FILE	0x67

#define GRUB_PE32_DT_FUNCTION		0x20

struct grub_pe32_reloc
{
  grub_uint32_t offset;
  grub_uint32_t symtab_index;
  grub_uint16_t type;
} GRUB_PACKED;

#define GRUB_PE32_REL_I386_DIR32	0x6
#define GRUB_PE32_REL_I386_REL32	0x14

#endif /* ! GRUB_EFI_PE32_HEADER */
