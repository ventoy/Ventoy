/** @file
  SimpleFileSystem protocol as defined in the UEFI 2.0 specification.

  The SimpleFileSystem protocol is the programmatic access to the FAT (12,16,32)
  file system specified in UEFI 2.0. It can also be used to abstract a file
  system other than FAT.

  UEFI 2.0 can boot from any valid EFI image contained in a SimpleFileSystem.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SIMPLE_FILE_SYSTEM_H__
#define __SIMPLE_FILE_SYSTEM_H__

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
  { \
    0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef struct _EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef struct _EFI_FILE_PROTOCOL *EFI_FILE_HANDLE;

///
/// Protocol GUID name defined in EFI1.1.
///
#define SIMPLE_FILE_SYSTEM_PROTOCOL  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID

///
/// Protocol name defined in EFI1.1.
///
typedef EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_FILE_IO_INTERFACE;
typedef EFI_FILE_PROTOCOL               EFI_FILE;

/**
  Open the root directory on a volume.

  @param  This A pointer to the volume to open the root directory.
  @param  Root A pointer to the location to return the opened file handle for the
               root directory.

  @retval EFI_SUCCESS          The device was opened.
  @retval EFI_UNSUPPORTED      This volume does not support the requested file system type.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_ACCESS_DENIED    The service denied access to the file.
  @retval EFI_OUT_OF_RESOURCES The volume was not opened due to lack of resources.
  @retval EFI_MEDIA_CHANGED    The device has a different medium in it or the medium is no
                               longer supported. Any existing file handles for this volume are
                               no longer valid. To access the files on the new medium, the
                               volume must be reopened with OpenVolume().

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME)(
  IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL    *This,
  OUT EFI_FILE_PROTOCOL                 **Root
  );

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION  0x00010000

///
/// Revision defined in EFI1.1
///
#define EFI_FILE_IO_INTERFACE_REVISION  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION

struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
  ///
  /// The version of the EFI_SIMPLE_FILE_SYSTEM_PROTOCOL. The version
  /// specified by this specification is 0x00010000. All future revisions
  /// must be backwards compatible.
  ///
  UINT64                                         Revision;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME    OpenVolume;
};

/**
  Opens a new file relative to the source file's location.

  @param  This       A pointer to the EFI_FILE_PROTOCOL instance that is the file
                     handle to the source location. This would typically be an open
                     handle to a directory.
  @param  NewHandle  A pointer to the location to return the opened handle for the new
                     file.
  @param  FileName   The Null-terminated string of the name of the file to be opened.
                     The file name may contain the following path modifiers: "\", ".",
                     and "..".
  @param  OpenMode   The mode to open the file. The only valid combinations that the
                     file may be opened with are: Read, Read/Write, or Create/Read/Write.
  @param  Attributes Only valid for EFI_FILE_MODE_CREATE, in which case these are the
                     attribute bits for the newly created file.

  @retval EFI_SUCCESS          The file was opened.
  @retval EFI_NOT_FOUND        The specified file could not be found on the device.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_MEDIA_CHANGED    The device has a different medium in it or the medium is no
                               longer supported.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  An attempt was made to create a file, or open a file for write
                               when the media is write-protected.
  @retval EFI_ACCESS_DENIED    The service denied access to the file.
  @retval EFI_OUT_OF_RESOURCES Not enough resources were available to open the file.
  @retval EFI_VOLUME_FULL      The volume is full.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_OPEN)(
  IN EFI_FILE_PROTOCOL        *This,
  OUT EFI_FILE_PROTOCOL       **NewHandle,
  IN CHAR16                   *FileName,
  IN UINT64                   OpenMode,
  IN UINT64                   Attributes
  );

//
// Open modes
//
#define EFI_FILE_MODE_READ    0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE   0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE  0x8000000000000000ULL

//
// File attributes
//
#define EFI_FILE_READ_ONLY   0x0000000000000001ULL
#define EFI_FILE_HIDDEN      0x0000000000000002ULL
#define EFI_FILE_SYSTEM      0x0000000000000004ULL
#define EFI_FILE_RESERVED    0x0000000000000008ULL
#define EFI_FILE_DIRECTORY   0x0000000000000010ULL
#define EFI_FILE_ARCHIVE     0x0000000000000020ULL
#define EFI_FILE_VALID_ATTR  0x0000000000000037ULL

/**
  Closes a specified file handle.

  @param  This          A pointer to the EFI_FILE_PROTOCOL instance that is the file
                        handle to close.

  @retval EFI_SUCCESS   The file was closed.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_CLOSE)(
  IN EFI_FILE_PROTOCOL  *This
  );

/**
  Close and delete the file handle.

  @param  This                     A pointer to the EFI_FILE_PROTOCOL instance that is the
                                   handle to the file to delete.

  @retval EFI_SUCCESS              The file was closed and deleted, and the handle was closed.
  @retval EFI_WARN_DELETE_FAILURE  The handle was closed, but the file was not deleted.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_DELETE)(
  IN EFI_FILE_PROTOCOL  *This
  );

/**
  Reads data from a file.

  @param  This       A pointer to the EFI_FILE_PROTOCOL instance that is the file
                     handle to read data from.
  @param  BufferSize On input, the size of the Buffer. On output, the amount of data
                     returned in Buffer. In both cases, the size is measured in bytes.
  @param  Buffer     The buffer into which the data is read.

  @retval EFI_SUCCESS          Data was read.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_DEVICE_ERROR     An attempt was made to read from a deleted file.
  @retval EFI_DEVICE_ERROR     On entry, the current file position is beyond the end of the file.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_BUFFER_TOO_SMALL The BufferSize is too small to read the current directory
                               entry. BufferSize has been updated with the size
                               needed to complete the request.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_READ)(
  IN EFI_FILE_PROTOCOL        *This,
  IN OUT UINTN                *BufferSize,
  OUT VOID                    *Buffer
  );

/**
  Writes data to a file.

  @param  This       A pointer to the EFI_FILE_PROTOCOL instance that is the file
                     handle to write data to.
  @param  BufferSize On input, the size of the Buffer. On output, the amount of data
                     actually written. In both cases, the size is measured in bytes.
  @param  Buffer     The buffer of data to write.

  @retval EFI_SUCCESS          Data was written.
  @retval EFI_UNSUPPORTED      Writes to open directory files are not supported.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_DEVICE_ERROR     An attempt was made to write to a deleted file.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  The file or medium is write-protected.
  @retval EFI_ACCESS_DENIED    The file was opened read only.
  @retval EFI_VOLUME_FULL      The volume is full.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_WRITE)(
  IN EFI_FILE_PROTOCOL        *This,
  IN OUT UINTN                *BufferSize,
  IN VOID                     *Buffer
  );

/**
  Sets a file's current position.

  @param  This            A pointer to the EFI_FILE_PROTOCOL instance that is the
                          file handle to set the requested position on.
  @param  Position        The byte position from the start of the file to set.

  @retval EFI_SUCCESS      The position was set.
  @retval EFI_UNSUPPORTED  The seek request for nonzero is not valid on open
                           directories.
  @retval EFI_DEVICE_ERROR An attempt was made to set the position of a deleted file.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_SET_POSITION)(
  IN EFI_FILE_PROTOCOL        *This,
  IN UINT64                   Position
  );

/**
  Returns a file's current position.

  @param  This            A pointer to the EFI_FILE_PROTOCOL instance that is the file
                          handle to get the current position on.
  @param  Position        The address to return the file's current position value.

  @retval EFI_SUCCESS      The position was returned.
  @retval EFI_UNSUPPORTED  The request is not valid on open directories.
  @retval EFI_DEVICE_ERROR An attempt was made to get the position from a deleted file.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_GET_POSITION)(
  IN EFI_FILE_PROTOCOL        *This,
  OUT UINT64                  *Position
  );

/**
  Returns information about a file.

  @param  This            A pointer to the EFI_FILE_PROTOCOL instance that is the file
                          handle the requested information is for.
  @param  InformationType The type identifier for the information being requested.
  @param  BufferSize      On input, the size of Buffer. On output, the amount of data
                          returned in Buffer. In both cases, the size is measured in bytes.
  @param  Buffer          A pointer to the data buffer to return. The buffer's type is
                          indicated by InformationType.

  @retval EFI_SUCCESS          The information was returned.
  @retval EFI_UNSUPPORTED      The InformationType is not known.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_BUFFER_TOO_SMALL The BufferSize is too small to read the current directory entry.
                               BufferSize has been updated with the size needed to complete
                               the request.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_GET_INFO)(
  IN EFI_FILE_PROTOCOL        *This,
  IN EFI_GUID                 *InformationType,
  IN OUT UINTN                *BufferSize,
  OUT VOID                    *Buffer
  );

/**
  Sets information about a file.

  @param  File            A pointer to the EFI_FILE_PROTOCOL instance that is the file
                          handle the information is for.
  @param  InformationType The type identifier for the information being set.
  @param  BufferSize      The size, in bytes, of Buffer.
  @param  Buffer          A pointer to the data buffer to write. The buffer's type is
                          indicated by InformationType.

  @retval EFI_SUCCESS          The information was set.
  @retval EFI_UNSUPPORTED      The InformationType is not known.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  InformationType is EFI_FILE_INFO_ID and the media is
                               read-only.
  @retval EFI_WRITE_PROTECTED  InformationType is EFI_FILE_PROTOCOL_SYSTEM_INFO_ID
                               and the media is read only.
  @retval EFI_WRITE_PROTECTED  InformationType is EFI_FILE_SYSTEM_VOLUME_LABEL_ID
                               and the media is read-only.
  @retval EFI_ACCESS_DENIED    An attempt is made to change the name of a file to a
                               file that is already present.
  @retval EFI_ACCESS_DENIED    An attempt is being made to change the EFI_FILE_DIRECTORY
                               Attribute.
  @retval EFI_ACCESS_DENIED    An attempt is being made to change the size of a directory.
  @retval EFI_ACCESS_DENIED    InformationType is EFI_FILE_INFO_ID and the file was opened
                               read-only and an attempt is being made to modify a field
                               other than Attribute.
  @retval EFI_VOLUME_FULL      The volume is full.
  @retval EFI_BAD_BUFFER_SIZE  BufferSize is smaller than the size of the type indicated
                               by InformationType.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_SET_INFO)(
  IN EFI_FILE_PROTOCOL        *This,
  IN EFI_GUID                 *InformationType,
  IN UINTN                    BufferSize,
  IN VOID                     *Buffer
  );

/**
  Flushes all modified data associated with a file to a device.

  @param  This A pointer to the EFI_FILE_PROTOCOL instance that is the file
               handle to flush.

  @retval EFI_SUCCESS          The data was flushed.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  The file or medium is write-protected.
  @retval EFI_ACCESS_DENIED    The file was opened read-only.
  @retval EFI_VOLUME_FULL      The volume is full.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_FLUSH)(
  IN EFI_FILE_PROTOCOL  *This
  );

typedef struct {
  //
  // If Event is NULL, then blocking I/O is performed.
  // If Event is not NULL and non-blocking I/O is supported, then non-blocking I/O is performed,
  // and Event will be signaled when the read request is completed.
  // The caller must be prepared to handle the case where the callback associated with Event
  // occurs before the original asynchronous I/O request call returns.
  //
  EFI_EVENT     Event;

  //
  // Defines whether or not the signaled event encountered an error.
  //
  EFI_STATUS    Status;

  //
  // For OpenEx():  Not Used, ignored.
  // For ReadEx():  On input, the size of the Buffer. On output, the amount of data returned in Buffer.
  //                In both cases, the size is measured in bytes.
  // For WriteEx(): On input, the size of the Buffer. On output, the amount of data actually written.
  //                In both cases, the size is measured in bytes.
  // For FlushEx(): Not used, ignored.
  //
  UINTN    BufferSize;

  //
  // For OpenEx():  Not Used, ignored.
  // For ReadEx():  The buffer into which the data is read.
  // For WriteEx(): The buffer of data to write.
  // For FlushEx(): Not Used, ignored.
  //
  VOID     *Buffer;
} EFI_FILE_IO_TOKEN;

/**
  Opens a new file relative to the source directory's location.

  @param  This       A pointer to the EFI_FILE_PROTOCOL instance that is the file
                     handle to the source location.
  @param  NewHandle  A pointer to the location to return the opened handle for the new
                     file.
  @param  FileName   The Null-terminated string of the name of the file to be opened.
                     The file name may contain the following path modifiers: "\", ".",
                     and "..".
  @param  OpenMode   The mode to open the file. The only valid combinations that the
                     file may be opened with are: Read, Read/Write, or Create/Read/Write.
  @param  Attributes Only valid for EFI_FILE_MODE_CREATE, in which case these are the
                     attribute bits for the newly created file.
  @param  Token      A pointer to the token associated with the transaction.

  @retval EFI_SUCCESS          If Event is NULL (blocking I/O): The data was read successfully.
                               If Event is not NULL (asynchronous I/O): The request was successfully
                                                                        queued for processing.
  @retval EFI_NOT_FOUND        The specified file could not be found on the device.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_MEDIA_CHANGED    The device has a different medium in it or the medium is no
                               longer supported.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  An attempt was made to create a file, or open a file for write
                               when the media is write-protected.
  @retval EFI_ACCESS_DENIED    The service denied access to the file.
  @retval EFI_OUT_OF_RESOURCES Not enough resources were available to open the file.
  @retval EFI_VOLUME_FULL      The volume is full.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_OPEN_EX)(
  IN EFI_FILE_PROTOCOL        *This,
  OUT EFI_FILE_PROTOCOL       **NewHandle,
  IN CHAR16                   *FileName,
  IN UINT64                   OpenMode,
  IN UINT64                   Attributes,
  IN OUT EFI_FILE_IO_TOKEN    *Token
  );

/**
  Reads data from a file.

  @param  This       A pointer to the EFI_FILE_PROTOCOL instance that is the file handle to read data from.
  @param  Token      A pointer to the token associated with the transaction.

  @retval EFI_SUCCESS          If Event is NULL (blocking I/O): The data was read successfully.
                               If Event is not NULL (asynchronous I/O): The request was successfully
                                                                        queued for processing.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_DEVICE_ERROR     An attempt was made to read from a deleted file.
  @retval EFI_DEVICE_ERROR     On entry, the current file position is beyond the end of the file.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_OUT_OF_RESOURCES Unable to queue the request due to lack of resources.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_READ_EX)(
  IN EFI_FILE_PROTOCOL        *This,
  IN OUT EFI_FILE_IO_TOKEN    *Token
  );

/**
  Writes data to a file.

  @param  This       A pointer to the EFI_FILE_PROTOCOL instance that is the file handle to write data to.
  @param  Token      A pointer to the token associated with the transaction.

  @retval EFI_SUCCESS          If Event is NULL (blocking I/O): The data was read successfully.
                               If Event is not NULL (asynchronous I/O): The request was successfully
                                                                        queued for processing.
  @retval EFI_UNSUPPORTED      Writes to open directory files are not supported.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_DEVICE_ERROR     An attempt was made to write to a deleted file.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  The file or medium is write-protected.
  @retval EFI_ACCESS_DENIED    The file was opened read only.
  @retval EFI_VOLUME_FULL      The volume is full.
  @retval EFI_OUT_OF_RESOURCES Unable to queue the request due to lack of resources.
**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_WRITE_EX)(
  IN EFI_FILE_PROTOCOL        *This,
  IN OUT EFI_FILE_IO_TOKEN    *Token
  );

/**
  Flushes all modified data associated with a file to a device.

  @param  This  A pointer to the EFI_FILE_PROTOCOL instance that is the file
                handle to flush.
  @param  Token A pointer to the token associated with the transaction.

  @retval EFI_SUCCESS          If Event is NULL (blocking I/O): The data was read successfully.
                               If Event is not NULL (asynchronous I/O): The request was successfully
                                                                        queued for processing.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  The file or medium is write-protected.
  @retval EFI_ACCESS_DENIED    The file was opened read-only.
  @retval EFI_VOLUME_FULL      The volume is full.
  @retval EFI_OUT_OF_RESOURCES Unable to queue the request due to lack of resources.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_FILE_FLUSH_EX)(
  IN EFI_FILE_PROTOCOL        *This,
  IN OUT EFI_FILE_IO_TOKEN    *Token
  );

#define EFI_FILE_PROTOCOL_REVISION         0x00010000
#define EFI_FILE_PROTOCOL_REVISION2        0x00020000
#define EFI_FILE_PROTOCOL_LATEST_REVISION  EFI_FILE_PROTOCOL_REVISION2

//
// Revision defined in EFI1.1.
//
#define EFI_FILE_REVISION  EFI_FILE_PROTOCOL_REVISION

///
/// The EFI_FILE_PROTOCOL provides file IO access to supported file systems.
/// An EFI_FILE_PROTOCOL provides access to a file's or directory's contents,
/// and is also a reference to a location in the directory tree of the file system
/// in which the file resides. With any given file handle, other files may be opened
/// relative to this file's location, yielding new file handles.
///
struct _EFI_FILE_PROTOCOL {
  ///
  /// The version of the EFI_FILE_PROTOCOL interface. The version specified
  /// by this specification is EFI_FILE_PROTOCOL_LATEST_REVISION.
  /// Future versions are required to be backward compatible to version 1.0.
  ///
  UINT64                   Revision;
  EFI_FILE_OPEN            Open;
  EFI_FILE_CLOSE           Close;
  EFI_FILE_DELETE          Delete;
  EFI_FILE_READ            Read;
  EFI_FILE_WRITE           Write;
  EFI_FILE_GET_POSITION    GetPosition;
  EFI_FILE_SET_POSITION    SetPosition;
  EFI_FILE_GET_INFO        GetInfo;
  EFI_FILE_SET_INFO        SetInfo;
  EFI_FILE_FLUSH           Flush;
  EFI_FILE_OPEN_EX         OpenEx;
  EFI_FILE_READ_EX         ReadEx;
  EFI_FILE_WRITE_EX        WriteEx;
  EFI_FILE_FLUSH_EX        FlushEx;
};

extern EFI_GUID  gEfiSimpleFileSystemProtocolGuid;

#endif
