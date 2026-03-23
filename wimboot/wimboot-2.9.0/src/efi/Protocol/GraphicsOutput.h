/** @file
  Graphics Output Protocol from the UEFI 2.0 specification.

  Abstraction of a very simple graphics device.

  Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __GRAPHICS_OUTPUT_H__
#define __GRAPHICS_OUTPUT_H__

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
  { \
    0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } \
  }

typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
  UINT32    RedMask;
  UINT32    GreenMask;
  UINT32    BlueMask;
  UINT32    ReservedMask;
} EFI_PIXEL_BITMASK;

typedef enum {
  ///
  /// A pixel is 32-bits and byte zero represents red, byte one represents green,
  /// byte two represents blue, and byte three is reserved. This is the definition
  /// for the physical frame buffer. The byte values for the red, green, and blue
  /// components represent the color intensity. This color intensity value range
  /// from a minimum intensity of 0 to maximum intensity of 255.
  ///
  PixelRedGreenBlueReserved8BitPerColor,
  ///
  /// A pixel is 32-bits and byte zero represents blue, byte one represents green,
  /// byte two represents red, and byte three is reserved. This is the definition
  /// for the physical frame buffer. The byte values for the red, green, and blue
  /// components represent the color intensity. This color intensity value range
  /// from a minimum intensity of 0 to maximum intensity of 255.
  ///
  PixelBlueGreenRedReserved8BitPerColor,
  ///
  /// The Pixel definition of the physical frame buffer.
  ///
  PixelBitMask,
  ///
  /// This mode does not support a physical frame buffer.
  ///
  PixelBltOnly,
  ///
  /// Valid EFI_GRAPHICS_PIXEL_FORMAT enum values are less than this value.
  ///
  PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

typedef struct {
  ///
  /// The version of this data structure. A value of zero represents the
  /// EFI_GRAPHICS_OUTPUT_MODE_INFORMATION structure as defined in this specification.
  ///
  UINT32                       Version;
  ///
  /// The size of video screen in pixels in the X dimension.
  ///
  UINT32                       HorizontalResolution;
  ///
  /// The size of video screen in pixels in the Y dimension.
  ///
  UINT32                       VerticalResolution;
  ///
  /// Enumeration that defines the physical format of the pixel. A value of PixelBltOnly
  /// implies that a linear frame buffer is not available for this mode.
  ///
  EFI_GRAPHICS_PIXEL_FORMAT    PixelFormat;
  ///
  /// This bit-mask is only valid if PixelFormat is set to PixelPixelBitMask.
  /// A bit being set defines what bits are used for what purpose such as Red, Green, Blue, or Reserved.
  ///
  EFI_PIXEL_BITMASK            PixelInformation;
  ///
  /// Defines the number of pixel elements per video memory line.
  ///
  UINT32                       PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

/**
  Returns information for an available graphics mode that the graphics device
  and the set of active video output devices supports.

  @param  This                  The EFI_GRAPHICS_OUTPUT_PROTOCOL instance.
  @param  ModeNumber            The mode number to return information on.
  @param  SizeOfInfo            A pointer to the size, in bytes, of the Info buffer.
  @param  Info                  A pointer to callee allocated buffer that returns information about ModeNumber.

  @retval EFI_SUCCESS           Valid mode information was returned.
  @retval EFI_DEVICE_ERROR      A hardware error occurred trying to retrieve the video mode.
  @retval EFI_INVALID_PARAMETER ModeNumber is not valid.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE)(
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  UINT32                                ModeNumber,
  OUT UINTN                                 *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
  );

/**
  Set the video device into the specified mode and clears the visible portions of
  the output display to black.

  @param  This              The EFI_GRAPHICS_OUTPUT_PROTOCOL instance.
  @param  ModeNumber        Abstraction that defines the current video mode.

  @retval EFI_SUCCESS       The graphics mode specified by ModeNumber was selected.
  @retval EFI_DEVICE_ERROR  The device had an error and could not complete the request.
  @retval EFI_UNSUPPORTED   ModeNumber is not supported by this device.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE)(
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
  IN  UINT32                       ModeNumber
  );

typedef struct {
  UINT8    Blue;
  UINT8    Green;
  UINT8    Red;
  UINT8    Reserved;
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL;

typedef union {
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    Pixel;
  UINT32                           Raw;
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION;

///
/// actions for BltOperations
///
typedef enum {
  ///
  /// Write data from the BltBuffer pixel (0, 0)
  /// directly to every pixel of the video display rectangle
  /// (DestinationX, DestinationY) (DestinationX + Width, DestinationY + Height).
  /// Only one pixel will be used from the BltBuffer. Delta is NOT used.
  ///
  EfiBltVideoFill,

  ///
  /// Read data from the video display rectangle
  /// (SourceX, SourceY) (SourceX + Width, SourceY + Height) and place it in
  /// the BltBuffer rectangle (DestinationX, DestinationY )
  /// (DestinationX + Width, DestinationY + Height). If DestinationX or
  /// DestinationY is not zero then Delta must be set to the length in bytes
  /// of a row in the BltBuffer.
  ///
  EfiBltVideoToBltBuffer,

  ///
  /// Write data from the BltBuffer rectangle
  /// (SourceX, SourceY) (SourceX + Width, SourceY + Height) directly to the
  /// video display rectangle (DestinationX, DestinationY)
  /// (DestinationX + Width, DestinationY + Height). If SourceX or SourceY is
  /// not zero then Delta must be set to the length in bytes of a row in the
  /// BltBuffer.
  ///
  EfiBltBufferToVideo,

  ///
  /// Copy from the video display rectangle (SourceX, SourceY)
  /// (SourceX + Width, SourceY + Height) to the video display rectangle
  /// (DestinationX, DestinationY) (DestinationX + Width, DestinationY + Height).
  /// The BltBuffer and Delta are not used in this mode.
  ///
  EfiBltVideoToVideo,

  EfiGraphicsOutputBltOperationMax
} EFI_GRAPHICS_OUTPUT_BLT_OPERATION;

/**
  Blt a rectangle of pixels on the graphics screen. Blt stands for BLock Transfer.

  @param  This         Protocol instance pointer.
  @param  BltBuffer    The data to transfer to the graphics screen.
                       Size is at least Width*Height*sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL).
  @param  BltOperation The operation to perform when copying BltBuffer on to the graphics screen.
  @param  SourceX      The X coordinate of source for the BltOperation.
  @param  SourceY      The Y coordinate of source for the BltOperation.
  @param  DestinationX The X coordinate of destination for the BltOperation.
  @param  DestinationY The Y coordinate of destination for the BltOperation.
  @param  Width        The width of a rectangle in the blt rectangle in pixels.
  @param  Height       The height of a rectangle in the blt rectangle in pixels.
  @param  Delta        Not used for EfiBltVideoFill or the EfiBltVideoToVideo operation.
                       If a Delta of zero is used, the entire BltBuffer is being operated on.
                       If a subrectangle of the BltBuffer is being used then Delta
                       represents the number of bytes in a row of the BltBuffer.

  @retval EFI_SUCCESS           BltBuffer was drawn to the graphics screen.
  @retval EFI_INVALID_PARAMETER BltOperation is not valid.
  @retval EFI_DEVICE_ERROR      The device had an error and could not complete the request.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT)(
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL            *This,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL           *BltBuffer    OPTIONAL,
  IN  EFI_GRAPHICS_OUTPUT_BLT_OPERATION       BltOperation,
  IN  UINTN                                   SourceX,
  IN  UINTN                                   SourceY,
  IN  UINTN                                   DestinationX,
  IN  UINTN                                   DestinationY,
  IN  UINTN                                   Width,
  IN  UINTN                                   Height,
  IN  UINTN                                   Delta         OPTIONAL
  );

typedef struct {
  ///
  /// The number of modes supported by QueryMode() and SetMode().
  ///
  UINT32                                  MaxMode;
  ///
  /// Current Mode of the graphics device. Valid mode numbers are 0 to MaxMode -1.
  ///
  UINT32                                  Mode;
  ///
  /// Pointer to read-only EFI_GRAPHICS_OUTPUT_MODE_INFORMATION data.
  ///
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION    *Info;
  ///
  /// Size of Info structure in bytes.
  ///
  UINTN                                   SizeOfInfo;
  ///
  /// Base address of graphics linear frame buffer.
  /// Offset zero in FrameBufferBase represents the upper left pixel of the display.
  ///
  EFI_PHYSICAL_ADDRESS                    FrameBufferBase;
  ///
  /// Amount of frame buffer needed to support the active mode as defined by
  /// PixelsPerScanLine xVerticalResolution x PixelElementSize.
  ///
  UINTN                                   FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

///
/// Provides a basic abstraction to set video modes and copy pixels to and from
/// the graphics controller's frame buffer. The linear address of the hardware
/// frame buffer is also exposed so software can write directly to the video hardware.
///
struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
  EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE    QueryMode;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE      SetMode;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT           Blt;
  ///
  /// Pointer to EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE data.
  ///
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE          *Mode;
};

extern EFI_GUID  gEfiGraphicsOutputProtocolGuid;

#endif
