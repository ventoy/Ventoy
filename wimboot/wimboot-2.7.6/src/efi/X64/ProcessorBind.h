/** @file
  Processor or Compiler specific defines and types x64 (Intel 64, AMD64).

  Copyright (c) 2006 - 2013, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __PROCESSOR_BIND_H__
#define __PROCESSOR_BIND_H__

///
/// Define the processor type so other code can make processor based choices
///
#define MDE_CPU_X64

//
// Make sure we are using the correct packing rules per EFI specification
//
#if !defined(__GNUC__)
#pragma pack()
#endif


#if defined(__INTEL_COMPILER)
//
// Disable ICC's remark #869: "Parameter" was never referenced warning.
// This is legal ANSI C code so we disable the remark that is turned on with -Wall
//
#pragma warning ( disable : 869 )

//
// Disable ICC's remark #1418: external function definition with no prior declaration.
// This is legal ANSI C code so we disable the remark that is turned on with /W4
//
#pragma warning ( disable : 1418 )

//
// Disable ICC's remark #1419: external declaration in primary source file
// This is legal ANSI C code so we disable the remark that is turned on with /W4
//
#pragma warning ( disable : 1419 )

//
// Disable ICC's remark #593: "Variable" was set but never used.
// This is legal ANSI C code so we disable the remark that is turned on with /W4
//
#pragma warning ( disable : 593 )

#endif


#if defined(_MSC_EXTENSIONS)

//
// Disable warning that make it impossible to compile at /W4
// This only works for Microsoft* tools
//

//
// Disabling bitfield type checking warnings.
//
#pragma warning ( disable : 4214 )

//
// Disabling the unreferenced formal parameter warnings.
//
#pragma warning ( disable : 4100 )

//
// Disable slightly different base types warning as CHAR8 * can not be set
// to a constant string.
//
#pragma warning ( disable : 4057 )

//
// ASSERT(FALSE) or while (TRUE) are legal constructes so supress this warning
//
#pragma warning ( disable : 4127 )

//
// This warning is caused by functions defined but not used. For precompiled header only.
//
#pragma warning ( disable : 4505 )

//
// This warning is caused by empty (after preprocessing) source file. For precompiled header only.
//
#pragma warning ( disable : 4206 )

#endif


#if defined(_MSC_EXTENSIONS)
  //
  // use Microsoft C complier dependent integer width types
  //

  ///
  /// 8-byte unsigned value
  ///
  typedef unsigned __int64    UINT64;
  ///
  /// 8-byte signed value
  ///
  typedef __int64             INT64;
  ///
  /// 4-byte unsigned value
  ///
  typedef unsigned __int32    UINT32;
  ///
  /// 4-byte signed value
  ///
  typedef __int32             INT32;
  ///
  /// 2-byte unsigned value
  ///
  typedef unsigned short      UINT16;
  ///
  /// 2-byte Character.  Unless otherwise specified all strings are stored in the
  /// UTF-16 encoding format as defined by Unicode 2.1 and ISO/IEC 10646 standards.
  ///
  typedef unsigned short      CHAR16;
  ///
  /// 2-byte signed value
  ///
  typedef short               INT16;
  ///
  /// Logical Boolean.  1-byte value containing 0 for FALSE or a 1 for TRUE.  Other
  /// values are undefined.
  ///
  typedef unsigned char       BOOLEAN;
  ///
  /// 1-byte unsigned value
  ///
  typedef unsigned char       UINT8;
  ///
  /// 1-byte Character
  ///
  typedef char                CHAR8;
  ///
  /// 1-byte signed value
  ///
  typedef signed char         INT8;
#else
  ///
  /// 8-byte unsigned value
  ///
  typedef unsigned long long  UINT64;
  ///
  /// 8-byte signed value
  ///
  typedef long long           INT64;
  ///
  /// 4-byte unsigned value
  ///
  typedef unsigned int        UINT32;
  ///
  /// 4-byte signed value
  ///
  typedef int                 INT32;
  ///
  /// 2-byte unsigned value
  ///
  typedef unsigned short      UINT16;
  ///
  /// 2-byte Character.  Unless otherwise specified all strings are stored in the
  /// UTF-16 encoding format as defined by Unicode 2.1 and ISO/IEC 10646 standards.
  ///
  typedef unsigned short      CHAR16;
  ///
  /// 2-byte signed value
  ///
  typedef short               INT16;
  ///
  /// Logical Boolean.  1-byte value containing 0 for FALSE or a 1 for TRUE.  Other
  /// values are undefined.
  ///
  typedef unsigned char       BOOLEAN;
  ///
  /// 1-byte unsigned value
  ///
  typedef unsigned char       UINT8;
  ///
  /// 1-byte Character
  ///
  typedef char                CHAR8;
  ///
  /// 1-byte signed value
  ///
  typedef signed char         INT8;
#endif

///
/// Unsigned value of native width.  (4 bytes on supported 32-bit processor instructions,
/// 8 bytes on supported 64-bit processor instructions)
///
typedef UINT64  UINTN;
///
/// Signed value of native width.  (4 bytes on supported 32-bit processor instructions,
/// 8 bytes on supported 64-bit processor instructions)
///
typedef INT64   INTN;


//
// Processor specific defines
//

///
/// A value of native width with the highest bit set.
///
#define MAX_BIT     0x8000000000000000ULL
///
/// A value of native width with the two highest bits set.
///
#define MAX_2_BITS  0xC000000000000000ULL

///
/// Maximum legal x64 address
///
#define MAX_ADDRESS   0xFFFFFFFFFFFFFFFFULL

///
/// Maximum legal x64 INTN and UINTN values.
///
#define MAX_INTN   ((INTN)0x7FFFFFFFFFFFFFFFULL)
#define MAX_UINTN  ((UINTN)0xFFFFFFFFFFFFFFFFULL)

///
/// The stack alignment required for x64
///
#define CPU_STACK_ALIGNMENT   16

//
// Modifier to ensure that all protocol member functions and EFI intrinsics
// use the correct C calling convention. All protocol member functions and
// EFI intrinsics are required to modify their member functions with EFIAPI.
//
#ifdef EFIAPI
  ///
  /// If EFIAPI is already defined, then we use that definition.
  ///
#elif defined(_MSC_EXTENSIONS)
  ///
  /// Microsoft* compiler specific method for EFIAPI calling convension
  ///
  #define EFIAPI __cdecl
#elif defined(__GNUC__)
  ///
  /// Define the standard calling convention reguardless of optimization level.
  /// The GCC support assumes a GCC compiler that supports the EFI ABI. The EFI
  /// ABI is much closer to the x64 Microsoft* ABI than standard x64 (x86-64)
  /// GCC ABI. Thus a standard x64 (x86-64) GCC compiler can not be used for
  /// x64. Warning the assembly code in the MDE x64 does not follow the correct
  /// ABI for the standard x64 (x86-64) GCC.
  ///
  #define EFIAPI
#else
  ///
  /// The default for a non Microsoft* or GCC compiler is to assume the EFI ABI
  /// is the standard.
  ///
  #define EFIAPI
#endif

#if defined(__GNUC__)
  ///
  /// For GNU assembly code, .global or .globl can declare global symbols.
  /// Define this macro to unify the usage.
  ///
  #define ASM_GLOBAL .globl
#endif

/**
  Return the pointer to the first instruction of a function given a function pointer.
  On x64 CPU architectures, these two pointer values are the same,
  so the implementation of this macro is very simple.

  @param  FunctionPointer   A pointer to a function.

  @return The pointer to the first instruction of a function given a function pointer.

**/
#define FUNCTION_ENTRY_POINT(FunctionPointer) (VOID *)(UINTN)(FunctionPointer)

#endif

