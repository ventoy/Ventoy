/*
 * Copyright (C) 2014 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * @file
 *
 * EFI block device
 *
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "wimboot.h"
#include "vdisk.h"
#include "efi.h"
#include "efipath.h"
#include "efiblock.h"

/** A block I/O device */
struct efi_block {
	/** EFI block I/O protocol */
	EFI_BLOCK_IO_PROTOCOL block;
	/** Device path */
	EFI_DEVICE_PATH_PROTOCOL *path;
	/** Starting LBA */
	uint64_t lba;
	/** Name */
	const char *name;
};

/**
 * Reset block I/O protocol
 *
 * @v this		Block I/O protocol
 * @v extended		Perform extended verification
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_reset_blocks ( EFI_BLOCK_IO_PROTOCOL *this, BOOLEAN extended ) {
	struct efi_block *block =
		container_of ( this, struct efi_block, block );
	void *retaddr = __builtin_return_address ( 0 );

	DBG2 ( "EFI %s %sreset -> %p\n",
	       block->name, ( extended ? "extended " : "" ), retaddr );
	return 0;
}

/**
 * Read blocks
 *
 * @v this		Block I/O protocol
 * @v media		Media ID
 * @v lba		Starting LBA
 * @v len		Length of data
 * @v data		Data buffer
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_read_blocks ( EFI_BLOCK_IO_PROTOCOL *this, UINT32 media, EFI_LBA lba,
		  UINTN len, VOID *data ) {
	struct efi_block *block =
		container_of ( this, struct efi_block, block );
	void *retaddr = __builtin_return_address ( 0 );

	DBG2 ( "EFI %s read media %08x LBA %#llx to %p+%zx -> %p\n",
	       block->name, media, lba, data, ( ( size_t ) len ), retaddr );
	vdisk_read ( ( lba + block->lba ), ( len / VDISK_SECTOR_SIZE ), data );
	return 0;
}

/**
 * Write blocks
 *
 * @v this		Block I/O protocol
 * @v media		Media ID
 * @v lba		Starting LBA
 * @v len		Length of data
 * @v data		Data buffer
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_write_blocks ( EFI_BLOCK_IO_PROTOCOL *this __unused,
		   UINT32 media __unused, EFI_LBA lba __unused,
		   UINTN len __unused, VOID *data __unused ) {
	struct efi_block *block =
		container_of ( this, struct efi_block, block );
	void *retaddr = __builtin_return_address ( 0 );

	DBG2 ( "EFI %s write media %08x LBA %#llx from %p+%zx -> %p\n",
	       block->name, media, lba, data, ( ( size_t ) len ), retaddr );
	return EFI_WRITE_PROTECTED;
}

/**
 * Flush block operations
 *
 * @v this		Block I/O protocol
 * @ret efirc		EFI status code
 */
static EFI_STATUS EFIAPI
efi_flush_blocks ( EFI_BLOCK_IO_PROTOCOL *this ) {
	struct efi_block *block =
		container_of ( this, struct efi_block, block );
	void *retaddr = __builtin_return_address ( 0 );

	DBG2 ( "EFI %s flush -> %p\n", block->name, retaddr );
	return 0;
}

/** GUID used in vendor device path */
#define EFIBLOCK_GUID {							\
	0x1322d197, 0x15dc, 0x4a45,					\
	{ 0xa6, 0xa4, 0xfa, 0x57, 0x05, 0x4e, 0xa6, 0x14 }		\
	}

/**
 * Initialise vendor device path
 *
 * @v name		Variable name
 */
#define EFIBLOCK_DEVPATH_VENDOR_INIT( name ) {				\
	.Header = EFI_DEVPATH_INIT ( name, HARDWARE_DEVICE_PATH,	\
				     HW_VENDOR_DP ),			\
	.Guid = EFIBLOCK_GUID,						\
	}

/**
 * Initialise ATA device path
 *
 * @v name		Variable name
 */
#define EFIBLOCK_DEVPATH_ATA_INIT( name ) {				\
	.Header = EFI_DEVPATH_INIT ( name, MESSAGING_DEVICE_PATH,	\
				     MSG_ATAPI_DP ),			\
	.PrimarySecondary = 0,						\
	.SlaveMaster = 0,						\
	.Lun = 0,							\
	}

/**
 * Initialise hard disk device path
 *
 * @v name		Variable name
 */
#define EFIBLOCK_DEVPATH_HD_INIT( name ) {				\
	.Header = EFI_DEVPATH_INIT ( name, MEDIA_DEVICE_PATH,		\
				     MEDIA_HARDDRIVE_DP ),		\
	.PartitionNumber = 1,						\
	.PartitionStart = VDISK_PARTITION_LBA,				\
	.PartitionSize = VDISK_PARTITION_COUNT,				\
	.Signature[0] = ( ( VDISK_MBR_SIGNATURE >> 0 ) & 0xff ),	\
	.Signature[1] = ( ( VDISK_MBR_SIGNATURE >> 8 ) & 0xff ),	\
	.Signature[2] = ( ( VDISK_MBR_SIGNATURE >> 16 ) & 0xff ),	\
	.Signature[3] = ( ( VDISK_MBR_SIGNATURE >> 24 ) & 0xff ),	\
	.MBRType = MBR_TYPE_PCAT,					\
	.SignatureType = SIGNATURE_TYPE_MBR,				\
	}

/** Virtual disk media */
static EFI_BLOCK_IO_MEDIA efi_vdisk_media = {
	.MediaId = VDISK_MBR_SIGNATURE,
	.MediaPresent = TRUE,
	.LogicalPartition = FALSE,
	.ReadOnly = TRUE,
	.BlockSize = VDISK_SECTOR_SIZE,
	.LastBlock = ( VDISK_COUNT - 1 ),
};

/** Virtual disk device path */
static struct {
	VENDOR_DEVICE_PATH vendor;
	ATAPI_DEVICE_PATH ata;
	EFI_DEVICE_PATH_PROTOCOL end;
} __attribute__ (( packed )) efi_vdisk_path = {
	.vendor = EFIBLOCK_DEVPATH_VENDOR_INIT ( efi_vdisk_path.vendor ),
	.ata = EFIBLOCK_DEVPATH_ATA_INIT ( efi_vdisk_path.ata ),
	.end = EFI_DEVPATH_END_INIT ( efi_vdisk_path.end ),
};

/** Virtual disk device */
static struct efi_block efi_vdisk = {
	.block = {
		.Revision = EFI_BLOCK_IO_PROTOCOL_REVISION,
		.Media = &efi_vdisk_media,
		.Reset = efi_reset_blocks,
		.ReadBlocks = efi_read_blocks,
		.WriteBlocks = efi_write_blocks,
		.FlushBlocks = efi_flush_blocks,
	},
	.path = &efi_vdisk_path.vendor.Header,
	.lba = 0,
	.name = "vdisk",
};

/** Virtual partition media */
static EFI_BLOCK_IO_MEDIA efi_vpartition_media = {
	.MediaId = VDISK_MBR_SIGNATURE,
	.MediaPresent = TRUE,
	.LogicalPartition = TRUE,
	.ReadOnly = TRUE,
	.BlockSize = VDISK_SECTOR_SIZE,
	.LastBlock = ( VDISK_PARTITION_COUNT - 1 ),
};

/** Virtual partition device path */
static struct {
	VENDOR_DEVICE_PATH vendor;
	ATAPI_DEVICE_PATH ata;
	HARDDRIVE_DEVICE_PATH hd;
	EFI_DEVICE_PATH_PROTOCOL end;
} __attribute__ (( packed )) efi_vpartition_path = {
	.vendor = EFIBLOCK_DEVPATH_VENDOR_INIT ( efi_vpartition_path.vendor ),
	.ata = EFIBLOCK_DEVPATH_ATA_INIT ( efi_vpartition_path.ata ),
	.hd = EFIBLOCK_DEVPATH_HD_INIT ( efi_vpartition_path.hd ),
	.end = EFI_DEVPATH_END_INIT ( efi_vpartition_path.end ),
};

/** Virtual partition device */
static struct efi_block efi_vpartition = {
	.block = {
		.Revision = EFI_BLOCK_IO_PROTOCOL_REVISION,
		.Media = &efi_vpartition_media,
		.Reset = efi_reset_blocks,
		.ReadBlocks = efi_read_blocks,
		.WriteBlocks = efi_write_blocks,
		.FlushBlocks = efi_flush_blocks,
	},
	.path = &efi_vpartition_path.vendor.Header,
	.lba = VDISK_PARTITION_LBA,
	.name = "vpartition",
};

/**
 * Install block I/O protocols
 *
 * @ret vdisk		New virtual disk handle
 * @ret vpartition	New virtual partition handle
 */
void efi_install ( EFI_HANDLE *vdisk, EFI_HANDLE *vpartition ) {
	EFI_BOOT_SERVICES *bs = efi_systab->BootServices;
	EFI_STATUS efirc;

	/* Install virtual disk */
	if ( ( efirc = bs->InstallMultipleProtocolInterfaces (
		vdisk,
		&efi_block_io_protocol_guid, &efi_vdisk.block,
		&efi_device_path_protocol_guid, efi_vdisk.path,
		NULL ) ) != 0 ) {
		die ( "Could not install disk block I/O protocols: %#lx\n",
		      ( ( unsigned long ) efirc ) );
	}

	/* Install virtual partition */
	if ( ( efirc = bs->InstallMultipleProtocolInterfaces (
		vpartition,
		&efi_block_io_protocol_guid, &efi_vpartition.block,
		&efi_device_path_protocol_guid, efi_vpartition.path,
		NULL ) ) != 0 ) {
		die ( "Could not install partition block I/O protocols: %#lx\n",
		      ( ( unsigned long ) efirc ) );
	}
}

/** Boot image path */
static struct {
	VENDOR_DEVICE_PATH vendor;
	ATAPI_DEVICE_PATH ata;
	HARDDRIVE_DEVICE_PATH hd;
	struct {
		EFI_DEVICE_PATH header;
		CHAR16 name[ sizeof ( EFI_REMOVABLE_MEDIA_FILE_NAME ) /
			     sizeof ( CHAR16 ) ];
	} __attribute__ (( packed )) file;
	EFI_DEVICE_PATH_PROTOCOL end;
} __attribute__ (( packed )) efi_bootmgfw_path = {
	.vendor = EFIBLOCK_DEVPATH_VENDOR_INIT ( efi_bootmgfw_path.vendor ),
	.ata = EFIBLOCK_DEVPATH_ATA_INIT ( efi_bootmgfw_path.ata ),
	.hd = EFIBLOCK_DEVPATH_HD_INIT ( efi_bootmgfw_path.hd ),
	.file = {
		.header = EFI_DEVPATH_INIT ( efi_bootmgfw_path.file,
					     MEDIA_DEVICE_PATH,
					     MEDIA_FILEPATH_DP ),
		.name = EFI_REMOVABLE_MEDIA_FILE_NAME,
	},
	.end = EFI_DEVPATH_END_INIT ( efi_bootmgfw_path.end ),
};

/** Boot image path */
EFI_DEVICE_PATH_PROTOCOL *bootmgfw_path = &efi_bootmgfw_path.vendor.Header;
