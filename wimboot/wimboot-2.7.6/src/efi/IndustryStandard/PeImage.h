/** @file
  EFI image format for PE32, PE32+ and TE. Please note some data structures are
  different for PE32 and PE32+. EFI_IMAGE_NT_HEADERS32 is for PE32 and
  EFI_IMAGE_NT_HEADERS64 is for PE32+.

  This file is coded to the Visual Studio, Microsoft Portable Executable and
  Common Object File Format Specification, Revision 8.3 - February 6, 2013.
  This file also includes some definitions in PI Specification, Revision 1.0.

Copyright (c) 2006 - 2010, Intel Corporation. All rights reserved.<BR>
Portions copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php.

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __PE_IMAGE_H__
#define __PE_IMAGE_H__

//
// PE32+ Subsystem type for EFI images
//
#define EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION         10
#define EFI_IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER 11
#define EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER      12
#define EFI_IMAGE_SUBSYSTEM_SAL_RUNTIME_DRIVER      13 ///< defined PI Specification, 1.0


//
// PE32+ Machine type for EFI images
//
#define IMAGE_FILE_MACHINE_I386            0x014c
#define IMAGE_FILE_MACHINE_IA64            0x0200
#define IMAGE_FILE_MACHINE_EBC             0x0EBC
#define IMAGE_FILE_MACHINE_X64             0x8664
#define IMAGE_FILE_MACHINE_ARMTHUMB_MIXED  0x01c2
#define IMAGE_FILE_MACHINE_ARM64           0xAA64

//
// EXE file formats
//
#define EFI_IMAGE_DOS_SIGNATURE     SIGNATURE_16('M', 'Z')
#define EFI_IMAGE_OS2_SIGNATURE     SIGNATURE_16('N', 'E')
#define EFI_IMAGE_OS2_SIGNATURE_LE  SIGNATURE_16('L', 'E')
#define EFI_IMAGE_NT_SIGNATURE      SIGNATURE_32('P', 'E', '\0', '\0')

///
/// PE images can start with an optional DOS header, so if an image is run
/// under DOS it can print an error message.
///
typedef struct {
  UINT16  e_magic;    ///< Magic number.
  UINT16  e_cblp;     ///< Bytes on last page of file.
  UINT16  e_cp;       ///< Pages in file.
  UINT16  e_crlc;     ///< Relocations.
  UINT16  e_cparhdr;  ///< Size of header in paragraphs.
  UINT16  e_minalloc; ///< Minimum extra paragraphs needed.
  UINT16  e_maxalloc; ///< Maximum extra paragraphs needed.
  UINT16  e_ss;       ///< Initial (relative) SS value.
  UINT16  e_sp;       ///< Initial SP value.
  UINT16  e_csum;     ///< Checksum.
  UINT16  e_ip;       ///< Initial IP value.
  UINT16  e_cs;       ///< Initial (relative) CS value.
  UINT16  e_lfarlc;   ///< File address of relocation table.
  UINT16  e_ovno;     ///< Overlay number.
  UINT16  e_res[4];   ///< Reserved words.
  UINT16  e_oemid;    ///< OEM identifier (for e_oeminfo).
  UINT16  e_oeminfo;  ///< OEM information; e_oemid specific.
  UINT16  e_res2[10]; ///< Reserved words.
  UINT32  e_lfanew;   ///< File address of new exe header.
} EFI_IMAGE_DOS_HEADER;

///
/// COFF File Header (Object and Image).
///
typedef struct {
  UINT16  Machine;
  UINT16  NumberOfSections;
  UINT32  TimeDateStamp;
  UINT32  PointerToSymbolTable;
  UINT32  NumberOfSymbols;
  UINT16  SizeOfOptionalHeader;
  UINT16  Characteristics;
} EFI_IMAGE_FILE_HEADER;

///
/// Size of EFI_IMAGE_FILE_HEADER.
///
#define EFI_IMAGE_SIZEOF_FILE_HEADER        20

//
// Characteristics
//
#define EFI_IMAGE_FILE_RELOCS_STRIPPED      BIT0     ///< 0x0001  Relocation info stripped from file.
#define EFI_IMAGE_FILE_EXECUTABLE_IMAGE     BIT1     ///< 0x0002  File is executable  (i.e. no unresolved externel references).
#define EFI_IMAGE_FILE_LINE_NUMS_STRIPPED   BIT2     ///< 0x0004  Line nunbers stripped from file.
#define EFI_IMAGE_FILE_LOCAL_SYMS_STRIPPED  BIT3     ///< 0x0008  Local symbols stripped from file.
#define EFI_IMAGE_FILE_BYTES_REVERSED_LO    BIT7     ///< 0x0080  Bytes of machine word are reversed.
#define EFI_IMAGE_FILE_32BIT_MACHINE        BIT8     ///< 0x0100  32 bit word machine.
#define EFI_IMAGE_FILE_DEBUG_STRIPPED       BIT9     ///< 0x0200  Debugging info stripped from file in .DBG file.
#define EFI_IMAGE_FILE_SYSTEM               BIT12    ///< 0x1000  System File.
#define EFI_IMAGE_FILE_DLL                  BIT13    ///< 0x2000  File is a DLL.
#define EFI_IMAGE_FILE_BYTES_REVERSED_HI    BIT15    ///< 0x8000  Bytes of machine word are reversed.

///
/// Header Data Directories.
///
typedef struct {
  UINT32  VirtualAddress;
  UINT32  Size;
} EFI_IMAGE_DATA_DIRECTORY;

//
// Directory Entries
//
#define EFI_IMAGE_DIRECTORY_ENTRY_EXPORT      0
#define EFI_IMAGE_DIRECTORY_ENTRY_IMPORT      1
#define EFI_IMAGE_DIRECTORY_ENTRY_RESOURCE    2
#define EFI_IMAGE_DIRECTORY_ENTRY_EXCEPTION   3
#define EFI_IMAGE_DIRECTORY_ENTRY_SECURITY    4
#define EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC   5
#define EFI_IMAGE_DIRECTORY_ENTRY_DEBUG       6
#define EFI_IMAGE_DIRECTORY_ENTRY_COPYRIGHT   7
#define EFI_IMAGE_DIRECTORY_ENTRY_GLOBALPTR   8
#define EFI_IMAGE_DIRECTORY_ENTRY_TLS         9
#define EFI_IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG 10

#define EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES 16

///
/// @attention
/// EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC means PE32 and
/// EFI_IMAGE_OPTIONAL_HEADER32 must be used. The data structures only vary
/// after NT additional fields.
///
#define EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC 0x10b

///
/// Optional Header Standard Fields for PE32.
///
typedef struct {
  ///
  /// Standard fields.
  ///
  UINT16                    Magic;
  UINT8                     MajorLinkerVersion;
  UINT8                     MinorLinkerVersion;
  UINT32                    SizeOfCode;
  UINT32                    SizeOfInitializedData;
  UINT32                    SizeOfUninitializedData;
  UINT32                    AddressOfEntryPoint;
  UINT32                    BaseOfCode;
  UINT32                    BaseOfData;  ///< PE32 contains this additional field, which is absent in PE32+.
  ///
  /// Optional Header Windows-Specific Fields.
  ///
  UINT32                    ImageBase;
  UINT32                    SectionAlignment;
  UINT32                    FileAlignment;
  UINT16                    MajorOperatingSystemVersion;
  UINT16                    MinorOperatingSystemVersion;
  UINT16                    MajorImageVersion;
  UINT16                    MinorImageVersion;
  UINT16                    MajorSubsystemVersion;
  UINT16                    MinorSubsystemVersion;
  UINT32                    Win32VersionValue;
  UINT32                    SizeOfImage;
  UINT32                    SizeOfHeaders;
  UINT32                    CheckSum;
  UINT16                    Subsystem;
  UINT16                    DllCharacteristics;
  UINT32                    SizeOfStackReserve;
  UINT32                    SizeOfStackCommit;
  UINT32                    SizeOfHeapReserve;
  UINT32                    SizeOfHeapCommit;
  UINT32                    LoaderFlags;
  UINT32                    NumberOfRvaAndSizes;
  EFI_IMAGE_DATA_DIRECTORY  DataDirectory[EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES];
} EFI_IMAGE_OPTIONAL_HEADER32;

///
/// @attention
/// EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC means PE32+ and
/// EFI_IMAGE_OPTIONAL_HEADER64 must be used. The data structures only vary
/// after NT additional fields.
///
#define EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC 0x20b

///
/// Optional Header Standard Fields for PE32+.
///
typedef struct {
  ///
  /// Standard fields.
  ///
  UINT16                    Magic;
  UINT8                     MajorLinkerVersion;
  UINT8                     MinorLinkerVersion;
  UINT32                    SizeOfCode;
  UINT32                    SizeOfInitializedData;
  UINT32                    SizeOfUninitializedData;
  UINT32                    AddressOfEntryPoint;
  UINT32                    BaseOfCode;
  ///
  /// Optional Header Windows-Specific Fields.
  ///
  UINT64                    ImageBase;
  UINT32                    SectionAlignment;
  UINT32                    FileAlignment;
  UINT16                    MajorOperatingSystemVersion;
  UINT16                    MinorOperatingSystemVersion;
  UINT16                    MajorImageVersion;
  UINT16                    MinorImageVersion;
  UINT16                    MajorSubsystemVersion;
  UINT16                    MinorSubsystemVersion;
  UINT32                    Win32VersionValue;
  UINT32                    SizeOfImage;
  UINT32                    SizeOfHeaders;
  UINT32                    CheckSum;
  UINT16                    Subsystem;
  UINT16                    DllCharacteristics;
  UINT64                    SizeOfStackReserve;
  UINT64                    SizeOfStackCommit;
  UINT64                    SizeOfHeapReserve;
  UINT64                    SizeOfHeapCommit;
  UINT32                    LoaderFlags;
  UINT32                    NumberOfRvaAndSizes;
  EFI_IMAGE_DATA_DIRECTORY  DataDirectory[EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES];
} EFI_IMAGE_OPTIONAL_HEADER64;


///
/// @attention
/// EFI_IMAGE_NT_HEADERS32 is for use ONLY by tools.
///
typedef struct {
  UINT32                      Signature;
  EFI_IMAGE_FILE_HEADER       FileHeader;
  EFI_IMAGE_OPTIONAL_HEADER32 OptionalHeader;
} EFI_IMAGE_NT_HEADERS32;

#define EFI_IMAGE_SIZEOF_NT_OPTIONAL32_HEADER sizeof (EFI_IMAGE_NT_HEADERS32)

///
/// @attention
/// EFI_IMAGE_HEADERS64 is for use ONLY by tools.
///
typedef struct {
  UINT32                      Signature;
  EFI_IMAGE_FILE_HEADER       FileHeader;
  EFI_IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} EFI_IMAGE_NT_HEADERS64;

#define EFI_IMAGE_SIZEOF_NT_OPTIONAL64_HEADER sizeof (EFI_IMAGE_NT_HEADERS64)

//
// Other Windows Subsystem Values
//
#define EFI_IMAGE_SUBSYSTEM_UNKNOWN     0
#define EFI_IMAGE_SUBSYSTEM_NATIVE      1
#define EFI_IMAGE_SUBSYSTEM_WINDOWS_GUI 2
#define EFI_IMAGE_SUBSYSTEM_WINDOWS_CUI 3
#define EFI_IMAGE_SUBSYSTEM_OS2_CUI     5
#define EFI_IMAGE_SUBSYSTEM_POSIX_CUI   7

///
/// Length of ShortName.
///
#define EFI_IMAGE_SIZEOF_SHORT_NAME 8

///
/// Section Table. This table immediately follows the optional header.
///
typedef struct {
  UINT8 Name[EFI_IMAGE_SIZEOF_SHORT_NAME];
  union {
    UINT32  PhysicalAddress;
    UINT32  VirtualSize;
  } Misc;
  UINT32  VirtualAddress;
  UINT32  SizeOfRawData;
  UINT32  PointerToRawData;
  UINT32  PointerToRelocations;
  UINT32  PointerToLinenumbers;
  UINT16  NumberOfRelocations;
  UINT16  NumberOfLinenumbers;
  UINT32  Characteristics;
} EFI_IMAGE_SECTION_HEADER;

///
/// Size of EFI_IMAGE_SECTION_HEADER.
///
#define EFI_IMAGE_SIZEOF_SECTION_HEADER       40

//
// Section Flags Values
//
#define EFI_IMAGE_SCN_TYPE_NO_PAD                  BIT3   ///< 0x00000008  ///< Reserved.
#define EFI_IMAGE_SCN_CNT_CODE                     BIT5   ///< 0x00000020
#define EFI_IMAGE_SCN_CNT_INITIALIZED_DATA         BIT6   ///< 0x00000040
#define EFI_IMAGE_SCN_CNT_UNINITIALIZED_DATA       BIT7   ///< 0x00000080

#define EFI_IMAGE_SCN_LNK_OTHER                    BIT8   ///< 0x00000100  ///< Reserved.
#define EFI_IMAGE_SCN_LNK_INFO                     BIT9   ///< 0x00000200  ///< Section contains comments or some other type of information.
#define EFI_IMAGE_SCN_LNK_REMOVE                   BIT11  ///< 0x00000800  ///< Section contents will not become part of image.
#define EFI_IMAGE_SCN_LNK_COMDAT                   BIT12  ///< 0x00001000

#define EFI_IMAGE_SCN_ALIGN_1BYTES                 BIT20  ///< 0x00100000
#define EFI_IMAGE_SCN_ALIGN_2BYTES                 BIT21  ///< 0x00200000
#define EFI_IMAGE_SCN_ALIGN_4BYTES          (BIT20|BIT21) ///< 0x00300000
#define EFI_IMAGE_SCN_ALIGN_8BYTES                 BIT22  ///< 0x00400000
#define EFI_IMAGE_SCN_ALIGN_16BYTES         (BIT20|BIT22) ///< 0x00500000
#define EFI_IMAGE_SCN_ALIGN_32BYTES         (BIT21|BIT22) ///< 0x00600000
#define EFI_IMAGE_SCN_ALIGN_64BYTES   (BIT20|BIT21|BIT22) ///< 0x00700000

#define EFI_IMAGE_SCN_MEM_DISCARDABLE              BIT25  ///< 0x02000000
#define EFI_IMAGE_SCN_MEM_NOT_CACHED               BIT26  ///< 0x04000000
#define EFI_IMAGE_SCN_MEM_NOT_PAGED                BIT27  ///< 0x08000000
#define EFI_IMAGE_SCN_MEM_SHARED                   BIT28  ///< 0x10000000
#define EFI_IMAGE_SCN_MEM_EXECUTE                  BIT29  ///< 0x20000000
#define EFI_IMAGE_SCN_MEM_READ                     BIT30  ///< 0x40000000
#define EFI_IMAGE_SCN_MEM_WRITE                    BIT31  ///< 0x80000000

///
/// Size of a Symbol Table Record.
///
#define EFI_IMAGE_SIZEOF_SYMBOL 18

//
// Symbols have a section number of the section in which they are
// defined. Otherwise, section numbers have the following meanings:
//
#define EFI_IMAGE_SYM_UNDEFINED (UINT16) 0  ///< Symbol is undefined or is common.
#define EFI_IMAGE_SYM_ABSOLUTE  (UINT16) -1 ///< Symbol is an absolute value.
#define EFI_IMAGE_SYM_DEBUG     (UINT16) -2 ///< Symbol is a special debug item.

//
// Symbol Type (fundamental) values.
//
#define EFI_IMAGE_SYM_TYPE_NULL   0   ///< no type.
#define EFI_IMAGE_SYM_TYPE_VOID   1   ///< no valid type.
#define EFI_IMAGE_SYM_TYPE_CHAR   2   ///< type character.
#define EFI_IMAGE_SYM_TYPE_SHORT  3   ///< type short integer.
#define EFI_IMAGE_SYM_TYPE_INT    4
#define EFI_IMAGE_SYM_TYPE_LONG   5
#define EFI_IMAGE_SYM_TYPE_FLOAT  6
#define EFI_IMAGE_SYM_TYPE_DOUBLE 7
#define EFI_IMAGE_SYM_TYPE_STRUCT 8
#define EFI_IMAGE_SYM_TYPE_UNION  9
#define EFI_IMAGE_SYM_TYPE_ENUM   10  ///< enumeration.
#define EFI_IMAGE_SYM_TYPE_MOE    11  ///< member of enumeration.
#define EFI_IMAGE_SYM_TYPE_BYTE   12
#define EFI_IMAGE_SYM_TYPE_WORD   13
#define EFI_IMAGE_SYM_TYPE_UINT   14
#define EFI_IMAGE_SYM_TYPE_DWORD  15

//
// Symbol Type (derived) values.
//
#define EFI_IMAGE_SYM_DTYPE_NULL      0 ///< no derived type.
#define EFI_IMAGE_SYM_DTYPE_POINTER   1
#define EFI_IMAGE_SYM_DTYPE_FUNCTION  2
#define EFI_IMAGE_SYM_DTYPE_ARRAY     3

//
// Storage classes.
//
#define EFI_IMAGE_SYM_CLASS_END_OF_FUNCTION   ((UINT8) -1)
#define EFI_IMAGE_SYM_CLASS_NULL              0
#define EFI_IMAGE_SYM_CLASS_AUTOMATIC         1
#define EFI_IMAGE_SYM_CLASS_EXTERNAL          2
#define EFI_IMAGE_SYM_CLASS_STATIC            3
#define EFI_IMAGE_SYM_CLASS_REGISTER          4
#define EFI_IMAGE_SYM_CLASS_EXTERNAL_DEF      5
#define EFI_IMAGE_SYM_CLASS_LABEL             6
#define EFI_IMAGE_SYM_CLASS_UNDEFINED_LABEL   7
#define EFI_IMAGE_SYM_CLASS_MEMBER_OF_STRUCT  8
#define EFI_IMAGE_SYM_CLASS_ARGUMENT          9
#define EFI_IMAGE_SYM_CLASS_STRUCT_TAG        10
#define EFI_IMAGE_SYM_CLASS_MEMBER_OF_UNION   11
#define EFI_IMAGE_SYM_CLASS_UNION_TAG         12
#define EFI_IMAGE_SYM_CLASS_TYPE_DEFINITION   13
#define EFI_IMAGE_SYM_CLASS_UNDEFINED_STATIC  14
#define EFI_IMAGE_SYM_CLASS_ENUM_TAG          15
#define EFI_IMAGE_SYM_CLASS_MEMBER_OF_ENUM    16
#define EFI_IMAGE_SYM_CLASS_REGISTER_PARAM    17
#define EFI_IMAGE_SYM_CLASS_BIT_FIELD         18
#define EFI_IMAGE_SYM_CLASS_BLOCK             100
#define EFI_IMAGE_SYM_CLASS_FUNCTION          101
#define EFI_IMAGE_SYM_CLASS_END_OF_STRUCT     102
#define EFI_IMAGE_SYM_CLASS_FILE              103
#define EFI_IMAGE_SYM_CLASS_SECTION           104
#define EFI_IMAGE_SYM_CLASS_WEAK_EXTERNAL     105

//
// type packing constants
//
#define EFI_IMAGE_N_BTMASK  017
#define EFI_IMAGE_N_TMASK   060
#define EFI_IMAGE_N_TMASK1  0300
#define EFI_IMAGE_N_TMASK2  0360
#define EFI_IMAGE_N_BTSHFT  4
#define EFI_IMAGE_N_TSHIFT  2

//
// Communal selection types.
//
#define EFI_IMAGE_COMDAT_SELECT_NODUPLICATES    1
#define EFI_IMAGE_COMDAT_SELECT_ANY             2
#define EFI_IMAGE_COMDAT_SELECT_SAME_SIZE       3
#define EFI_IMAGE_COMDAT_SELECT_EXACT_MATCH     4
#define EFI_IMAGE_COMDAT_SELECT_ASSOCIATIVE     5

//
// the following values only be referred in PeCoff, not defined in PECOFF.
//
#define EFI_IMAGE_WEAK_EXTERN_SEARCH_NOLIBRARY  1
#define EFI_IMAGE_WEAK_EXTERN_SEARCH_LIBRARY    2
#define EFI_IMAGE_WEAK_EXTERN_SEARCH_ALIAS      3

///
/// Relocation format.
///
typedef struct {
  UINT32  VirtualAddress;
  UINT32  SymbolTableIndex;
  UINT16  Type;
} EFI_IMAGE_RELOCATION;

///
/// Size of EFI_IMAGE_RELOCATION
///
#define EFI_IMAGE_SIZEOF_RELOCATION 10

//
// I386 relocation types.
//
#define EFI_IMAGE_REL_I386_ABSOLUTE 0x0000  ///< Reference is absolute, no relocation is necessary.
#define EFI_IMAGE_REL_I386_DIR16    0x0001  ///< Direct 16-bit reference to the symbols virtual address.
#define EFI_IMAGE_REL_I386_REL16    0x0002  ///< PC-relative 16-bit reference to the symbols virtual address.
#define EFI_IMAGE_REL_I386_DIR32    0x0006  ///< Direct 32-bit reference to the symbols virtual address.
#define EFI_IMAGE_REL_I386_DIR32NB  0x0007  ///< Direct 32-bit reference to the symbols virtual address, base not included.
#define EFI_IMAGE_REL_I386_SEG12    0x0009  ///< Direct 16-bit reference to the segment-selector bits of a 32-bit virtual address.
#define EFI_IMAGE_REL_I386_SECTION  0x000A
#define EFI_IMAGE_REL_I386_SECREL   0x000B
#define EFI_IMAGE_REL_I386_REL32    0x0014  ///< PC-relative 32-bit reference to the symbols virtual address.

//
// x64 processor relocation types.
//
#define IMAGE_REL_AMD64_ABSOLUTE  0x0000
#define IMAGE_REL_AMD64_ADDR64    0x0001
#define IMAGE_REL_AMD64_ADDR32    0x0002
#define IMAGE_REL_AMD64_ADDR32NB  0x0003
#define IMAGE_REL_AMD64_REL32     0x0004
#define IMAGE_REL_AMD64_REL32_1   0x0005
#define IMAGE_REL_AMD64_REL32_2   0x0006
#define IMAGE_REL_AMD64_REL32_3   0x0007
#define IMAGE_REL_AMD64_REL32_4   0x0008
#define IMAGE_REL_AMD64_REL32_5   0x0009
#define IMAGE_REL_AMD64_SECTION   0x000A
#define IMAGE_REL_AMD64_SECREL    0x000B
#define IMAGE_REL_AMD64_SECREL7   0x000C
#define IMAGE_REL_AMD64_TOKEN     0x000D
#define IMAGE_REL_AMD64_SREL32    0x000E
#define IMAGE_REL_AMD64_PAIR      0x000F
#define IMAGE_REL_AMD64_SSPAN32   0x0010

///
/// Based relocation format.
///
typedef struct {
  UINT32  VirtualAddress;
  UINT32  SizeOfBlock;
} EFI_IMAGE_BASE_RELOCATION;

///
/// Size of EFI_IMAGE_BASE_RELOCATION.
///
#define EFI_IMAGE_SIZEOF_BASE_RELOCATION  8

//
// Based relocation types.
//
#define EFI_IMAGE_REL_BASED_ABSOLUTE        0
#define EFI_IMAGE_REL_BASED_HIGH            1
#define EFI_IMAGE_REL_BASED_LOW             2
#define EFI_IMAGE_REL_BASED_HIGHLOW         3
#define EFI_IMAGE_REL_BASED_HIGHADJ         4
#define EFI_IMAGE_REL_BASED_MIPS_JMPADDR    5
#define EFI_IMAGE_REL_BASED_ARM_MOV32A      5
#define EFI_IMAGE_REL_BASED_ARM_MOV32T      7
#define EFI_IMAGE_REL_BASED_IA64_IMM64      9
#define EFI_IMAGE_REL_BASED_MIPS_JMPADDR16  9
#define EFI_IMAGE_REL_BASED_DIR64           10

///
/// Line number format.
///
typedef struct {
  union {
    UINT32  SymbolTableIndex; ///< Symbol table index of function name if Linenumber is 0.
    UINT32  VirtualAddress;   ///< Virtual address of line number.
  } Type;
  UINT16  Linenumber;         ///< Line number.
} EFI_IMAGE_LINENUMBER;

///
/// Size of EFI_IMAGE_LINENUMBER.
///
#define EFI_IMAGE_SIZEOF_LINENUMBER 6

//
// Archive format.
//
#define EFI_IMAGE_ARCHIVE_START_SIZE        8
#define EFI_IMAGE_ARCHIVE_START             "!<arch>\n"
#define EFI_IMAGE_ARCHIVE_END               "`\n"
#define EFI_IMAGE_ARCHIVE_PAD               "\n"
#define EFI_IMAGE_ARCHIVE_LINKER_MEMBER     "/               "
#define EFI_IMAGE_ARCHIVE_LONGNAMES_MEMBER  "//              "

///
/// Archive Member Headers
///
typedef struct {
  UINT8 Name[16];     ///< File member name - `/' terminated.
  UINT8 Date[12];     ///< File member date - decimal.
  UINT8 UserID[6];    ///< File member user id - decimal.
  UINT8 GroupID[6];   ///< File member group id - decimal.
  UINT8 Mode[8];      ///< File member mode - octal.
  UINT8 Size[10];     ///< File member size - decimal.
  UINT8 EndHeader[2]; ///< String to end header. (0x60 0x0A).
} EFI_IMAGE_ARCHIVE_MEMBER_HEADER;

///
/// Size of EFI_IMAGE_ARCHIVE_MEMBER_HEADER.
///
#define EFI_IMAGE_SIZEOF_ARCHIVE_MEMBER_HDR 60


//
// DLL Support
//

///
/// Export Directory Table.
///
typedef struct {
  UINT32  Characteristics;
  UINT32  TimeDateStamp;
  UINT16  MajorVersion;
  UINT16  MinorVersion;
  UINT32  Name;
  UINT32  Base;
  UINT32  NumberOfFunctions;
  UINT32  NumberOfNames;
  UINT32  AddressOfFunctions;
  UINT32  AddressOfNames;
  UINT32  AddressOfNameOrdinals;
} EFI_IMAGE_EXPORT_DIRECTORY;

///
/// Hint/Name Table.
///
typedef struct {
  UINT16  Hint;
  UINT8   Name[1];
} EFI_IMAGE_IMPORT_BY_NAME;

///
/// Import Address Table RVA (Thunk Table).
///
typedef struct {
  union {
    UINT32                    Function;
    UINT32                    Ordinal;
    EFI_IMAGE_IMPORT_BY_NAME  *AddressOfData;
  } u1;
} EFI_IMAGE_THUNK_DATA;

#define EFI_IMAGE_ORDINAL_FLAG              BIT31    ///< Flag for PE32.
#define EFI_IMAGE_SNAP_BY_ORDINAL(Ordinal)  ((Ordinal & EFI_IMAGE_ORDINAL_FLAG) != 0)
#define EFI_IMAGE_ORDINAL(Ordinal)          (Ordinal & 0xffff)

///
/// Import Directory Table
///
typedef struct {
  UINT32                Characteristics;
  UINT32                TimeDateStamp;
  UINT32                ForwarderChain;
  UINT32                Name;
  EFI_IMAGE_THUNK_DATA  *FirstThunk;
} EFI_IMAGE_IMPORT_DESCRIPTOR;


///
/// Debug Directory Format.
///
typedef struct {
  UINT32  Characteristics;
  UINT32  TimeDateStamp;
  UINT16  MajorVersion;
  UINT16  MinorVersion;
  UINT32  Type;
  UINT32  SizeOfData;
  UINT32  RVA;           ///< The address of the debug data when loaded, relative to the image base.
  UINT32  FileOffset;    ///< The file pointer to the debug data.
} EFI_IMAGE_DEBUG_DIRECTORY_ENTRY;

#define EFI_IMAGE_DEBUG_TYPE_CODEVIEW 2     ///< The Visual C++ debug information.

///
/// Debug Data Structure defined in Microsoft C++.
///
#define CODEVIEW_SIGNATURE_NB10  SIGNATURE_32('N', 'B', '1', '0')
typedef struct {
  UINT32  Signature;                        ///< "NB10"
  UINT32  Unknown;
  UINT32  Unknown2;
  UINT32  Unknown3;
  //
  // Filename of .PDB goes here
  //
} EFI_IMAGE_DEBUG_CODEVIEW_NB10_ENTRY;

///
/// Debug Data Structure defined in Microsoft C++.
///
#define CODEVIEW_SIGNATURE_RSDS  SIGNATURE_32('R', 'S', 'D', 'S')
typedef struct {
  UINT32  Signature;                        ///< "RSDS".
  UINT32  Unknown;
  UINT32  Unknown2;
  UINT32  Unknown3;
  UINT32  Unknown4;
  UINT32  Unknown5;
  //
  // Filename of .PDB goes here
  //
} EFI_IMAGE_DEBUG_CODEVIEW_RSDS_ENTRY;


///
/// Debug Data Structure defined by Apple Mach-O to Coff utility.
///
#define CODEVIEW_SIGNATURE_MTOC  SIGNATURE_32('M', 'T', 'O', 'C')
typedef struct {
  UINT32    Signature;                       ///< "MTOC".
  GUID      MachOUuid;
  //
  //  Filename of .DLL (Mach-O with debug info) goes here
  //
} EFI_IMAGE_DEBUG_CODEVIEW_MTOC_ENTRY;

///
/// Resource format.
///
typedef struct {
  UINT32  Characteristics;
  UINT32  TimeDateStamp;
  UINT16  MajorVersion;
  UINT16  MinorVersion;
  UINT16  NumberOfNamedEntries;
  UINT16  NumberOfIdEntries;
  //
  // Array of EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY entries goes here.
  //
} EFI_IMAGE_RESOURCE_DIRECTORY;

///
/// Resource directory entry format.
///
typedef struct {
  union {
    struct {
      UINT32  NameOffset:31;
      UINT32  NameIsString:1;
    } s;
    UINT32  Id;
  } u1;
  union {
    UINT32  OffsetToData;
    struct {
      UINT32  OffsetToDirectory:31;
      UINT32  DataIsDirectory:1;
    } s;
  } u2;
} EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY;

///
/// Resource directory entry for string.
///
typedef struct {
  UINT16  Length;
  CHAR16  String[1];
} EFI_IMAGE_RESOURCE_DIRECTORY_STRING;

///
/// Resource directory entry for data array.
///
typedef struct {
  UINT32  OffsetToData;
  UINT32  Size;
  UINT32  CodePage;
  UINT32  Reserved;
} EFI_IMAGE_RESOURCE_DATA_ENTRY;

///
/// Header format for TE images, defined in the PI Specification, 1.0.
///
typedef struct {
  UINT16                    Signature;            ///< The signature for TE format = "VZ".
  UINT16                    Machine;              ///< From the original file header.
  UINT8                     NumberOfSections;     ///< From the original file header.
  UINT8                     Subsystem;            ///< From original optional header.
  UINT16                    StrippedSize;         ///< Number of bytes we removed from the header.
  UINT32                    AddressOfEntryPoint;  ///< Offset to entry point -- from original optional header.
  UINT32                    BaseOfCode;           ///< From original image -- required for ITP debug.
  UINT64                    ImageBase;            ///< From original file header.
  EFI_IMAGE_DATA_DIRECTORY  DataDirectory[2];     ///< Only base relocation and debug directory.
} EFI_TE_IMAGE_HEADER;


#define EFI_TE_IMAGE_HEADER_SIGNATURE  SIGNATURE_16('V', 'Z')

//
// Data directory indexes in our TE image header
//
#define EFI_TE_IMAGE_DIRECTORY_ENTRY_BASERELOC  0
#define EFI_TE_IMAGE_DIRECTORY_ENTRY_DEBUG      1


///
/// Union of PE32, PE32+, and TE headers.
///
typedef union {
  EFI_IMAGE_NT_HEADERS32   Pe32;
  EFI_IMAGE_NT_HEADERS64   Pe32Plus;
  EFI_TE_IMAGE_HEADER      Te;
} EFI_IMAGE_OPTIONAL_HEADER_UNION;

typedef union {
  EFI_IMAGE_NT_HEADERS32            *Pe32;
  EFI_IMAGE_NT_HEADERS64            *Pe32Plus;
  EFI_TE_IMAGE_HEADER               *Te;
  EFI_IMAGE_OPTIONAL_HEADER_UNION   *Union;
} EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION;

#endif
