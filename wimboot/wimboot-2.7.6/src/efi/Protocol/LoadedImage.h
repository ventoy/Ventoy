/** @file
  UEFI 2.0 Loaded image protocol definition.

  Every EFI driver and application is passed an image handle when it is loaded.
  This image handle will contain a Loaded Image Protocol.

  Copyright (c) 2006 - 2008, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __LOADED_IMAGE_PROTOCOL_H__
#define __LOADED_IMAGE_PROTOCOL_H__

#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
  { \
    0x5B1B31A1, 0x9562, 0x11d2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } \
  }

#define EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID \
  { \
    0xbc62157e, 0x3e33, 0x4fec, {0x99, 0x20, 0x2d, 0x3b, 0x36, 0xd7, 0x50, 0xdf } \
  }

///
/// Protocol GUID defined in EFI1.1.
///
#define LOADED_IMAGE_PROTOCOL   EFI_LOADED_IMAGE_PROTOCOL_GUID

///
/// EFI_SYSTEM_TABLE & EFI_IMAGE_UNLOAD are defined in EfiApi.h
///
#define EFI_LOADED_IMAGE_PROTOCOL_REVISION  0x1000

///
/// Revision defined in EFI1.1.
///
#define EFI_LOADED_IMAGE_INFORMATION_REVISION    EFI_LOADED_IMAGE_PROTOCOL_REVISION

///
/// Can be used on any image handle to obtain information about the loaded image.
///
typedef struct {
  UINT32            Revision;       ///< Defines the revision of the EFI_LOADED_IMAGE_PROTOCOL structure.
                                    ///< All future revisions will be backward compatible to the current revision.
  EFI_HANDLE        ParentHandle;   ///< Parent image's image handle. NULL if the image is loaded directly from
                                    ///< the firmware's boot manager.
  EFI_SYSTEM_TABLE  *SystemTable;   ///< the image's EFI system table pointer.

  //
  // Source location of image
  //
  EFI_HANDLE        DeviceHandle;   ///< The device handle that the EFI Image was loaded from.
  EFI_DEVICE_PATH_PROTOCOL  *FilePath;  ///< A pointer to the file path portion specific to DeviceHandle
                                        ///< that the EFI Image was loaded from.
  VOID              *Reserved;      ///< Reserved. DO NOT USE.

  //
  // Images load options
  //
  UINT32            LoadOptionsSize;///< The size in bytes of LoadOptions.
  VOID              *LoadOptions;   ///< A pointer to the image's binary load options.

  //
  // Location of where image was loaded
  //
  VOID              *ImageBase;     ///< The base address at which the image was loaded.
  UINT64            ImageSize;      ///< The size in bytes of the loaded image.
  EFI_MEMORY_TYPE   ImageCodeType;  ///< The memory type that the code sections were loaded as.
  EFI_MEMORY_TYPE   ImageDataType;  ///< The memory type that the data sections were loaded as.
  EFI_IMAGE_UNLOAD  Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

//
// For backward-compatible with EFI1.1.
//
typedef EFI_LOADED_IMAGE_PROTOCOL EFI_LOADED_IMAGE;

extern EFI_GUID gEfiLoadedImageProtocolGuid;
extern EFI_GUID gEfiLoadedImageDevicePathProtocolGuid;

#endif
