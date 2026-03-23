/** @file
  Provides a GUID and a data structure that can be used with EFI_FILE_PROTOCOL.SetInfo()
  and EFI_FILE_PROTOCOL.GetInfo() to set or get generic file information.
  This GUID is defined in UEFI specification.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FILE_INFO_H__
#define __FILE_INFO_H__

#define EFI_FILE_INFO_ID \
  { \
    0x9576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

typedef struct {
  ///
  /// The size of the EFI_FILE_INFO structure, including the Null-terminated FileName string.
  ///
  UINT64      Size;
  ///
  /// The size of the file in bytes.
  ///
  UINT64      FileSize;
  ///
  /// PhysicalSize The amount of physical space the file consumes on the file system volume.
  ///
  UINT64      PhysicalSize;
  ///
  /// The time the file was created.
  ///
  EFI_TIME    CreateTime;
  ///
  /// The time when the file was last accessed.
  ///
  EFI_TIME    LastAccessTime;
  ///
  /// The time when the file's contents were last modified.
  ///
  EFI_TIME    ModificationTime;
  ///
  /// The attribute bits for the file.
  ///
  UINT64      Attribute;
  ///
  /// The Null-terminated name of the file.
  /// For a root directory, the name is an empty string.
  ///
  CHAR16      FileName[1];
} EFI_FILE_INFO;

///
/// The FileName field of the EFI_FILE_INFO data structure is variable length.
/// Whenever code needs to know the size of the EFI_FILE_INFO data structure, it needs to
/// be the size of the data structure without the FileName field.  The following macro
/// computes this size correctly no matter how big the FileName array is declared.
/// This is required to make the EFI_FILE_INFO data structure ANSI compilant.
///
#define SIZE_OF_EFI_FILE_INFO  OFFSET_OF (EFI_FILE_INFO, FileName)

extern EFI_GUID  gEfiFileInfoGuid;

#endif
