#ifndef _IPXE_SANBOOT_H
#define _IPXE_SANBOOT_H

/** @file
 *
 * iPXE sanboot API
 *
 * The sanboot API provides methods for hooking, unhooking,
 * describing, and booting from SAN devices.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <ipxe/api.h>
#include <ipxe/refcnt.h>
#include <ipxe/list.h>
#include <ipxe/uri.h>
#include <ipxe/retry.h>
#include <ipxe/process.h>
#include <ipxe/blockdev.h>
#include <ipxe/acpi.h>
#include <config/sanboot.h>

/** A SAN path */
struct san_path {
	/** Containing SAN device */
	struct san_device *sandev;
	/** Path index */
	unsigned int index;
	/** SAN device URI */
	struct uri *uri;
	/** List of open/closed paths */
	struct list_head list;

	/** Underlying block device interface */
	struct interface block;
	/** Process */
	struct process process;
	/** Path status */
	int path_rc;

	/** ACPI descriptor (if applicable) */
	struct acpi_descriptor *desc;
};

/** A SAN device */
struct san_device {
	/** Reference count */
	struct refcnt refcnt;
	/** List of SAN devices */
	struct list_head list;

	/** Drive number */
	unsigned int drive;
	/** Flags */
	unsigned int flags;

	/** Command interface */
	struct interface command;
	/** Command timeout timer */
	struct retry_timer timer;
	/** Command status */
	int command_rc;

	/** Raw block device capacity */
	struct block_device_capacity capacity;
	/** Block size shift
	 *
	 * To allow for emulation of CD-ROM access, this represents
	 * the left-shift required to translate from exposed logical
	 * I/O blocks to underlying blocks.
	 */
	unsigned int blksize_shift;
	/** Drive is a CD-ROM */
	int is_cdrom;

	/** Driver private data */
	void *priv;

	/** Number of paths */
	unsigned int paths;
	/** Current active path */
	struct san_path *active;
	/** List of opened SAN paths */
	struct list_head opened;
	/** List of closed SAN paths */
	struct list_head closed;
	/** SAN paths */
	struct san_path path[0];

    unsigned int exdrive;
    int int13_command;
    void *x86_regptr;
    uint8_t boot_catalog_sector[2048];
};

/** SAN device flags */
enum san_device_flags {
	/** Device should not be included in description tables */
	SAN_NO_DESCRIBE = 0x0001,
};

/**
 * Calculate static inline sanboot API function name
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @ret _subsys_func	Subsystem API function
 */
#define SANBOOT_INLINE( _subsys, _api_func ) \
	SINGLE_API_INLINE ( SANBOOT_PREFIX_ ## _subsys, _api_func )

/**
 * Provide a sanboot API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 * @v _func		Implementing function
 */
#define PROVIDE_SANBOOT( _subsys, _api_func, _func ) \
	PROVIDE_SINGLE_API ( SANBOOT_PREFIX_ ## _subsys, _api_func, _func )

/**
 * Provide a static inline sanboot API implementation
 *
 * @v _prefix		Subsystem prefix
 * @v _api_func		API function
 */
#define PROVIDE_SANBOOT_INLINE( _subsys, _api_func ) \
	PROVIDE_SINGLE_API_INLINE ( SANBOOT_PREFIX_ ## _subsys, _api_func )

/* Include all architecture-independent sanboot API headers */
#include <ipxe/null_sanboot.h>
#include <ipxe/dummy_sanboot.h>
#include <ipxe/efi/efi_block.h>

/* Include all architecture-dependent sanboot API headers */
#include <bits/sanboot.h>

/**
 * Hook SAN device
 *
 * @v drive		Drive number
 * @v uris		List of URIs
 * @v count		Number of URIs
 * @v flags		Flags
 * @ret drive		Drive number, or negative error
 */
int san_hook ( unsigned int drive, struct uri **uris, unsigned int count,
	       unsigned int flags );

/**
 * Unhook SAN device
 *
 * @v drive		Drive number
 */
void san_unhook ( unsigned int drive );

/**
 * Attempt to boot from a SAN device
 *
 * @v drive		Drive number
 * @v filename		Filename (or NULL to use default)
 * @ret rc		Return status code
 */
int san_boot ( unsigned int drive, const char *filename );

/**
 * Describe SAN devices for SAN-booted operating system
 *
 * @ret rc		Return status code
 */
int san_describe ( void );

extern struct list_head san_devices;

/** Iterate over all SAN devices */
#define for_each_sandev( sandev ) \
	list_for_each_entry ( (sandev), &san_devices, list )

/** There exist some SAN devices
 *
 * @ret existence	Existence of SAN devices
 */
static inline int have_sandevs ( void ) {
	return ( ! list_empty ( &san_devices ) );
}

/**
 * Get reference to SAN device
 *
 * @v sandev		SAN device
 * @ret sandev		SAN device
 */
static inline __attribute__ (( always_inline )) struct san_device *
sandev_get ( struct san_device *sandev ) {
	ref_get ( &sandev->refcnt );
	return sandev;
}

/**
 * Drop reference to SAN device
 *
 * @v sandev		SAN device
 */
static inline __attribute__ (( always_inline )) void
sandev_put ( struct san_device *sandev ) {
	ref_put ( &sandev->refcnt );
}

/**
 * Calculate SAN device block size
 *
 * @v sandev		SAN device
 * @ret blksize		Sector size
 */
static inline size_t sandev_blksize ( struct san_device *sandev ) {
	return ( sandev->capacity.blksize << sandev->blksize_shift );
}

/**
 * Calculate SAN device capacity
 *
 * @v sandev		SAN device
 * @ret blocks		Number of blocks
 */
static inline uint64_t sandev_capacity ( struct san_device *sandev ) {
	return ( sandev->capacity.blocks >> sandev->blksize_shift );
}

/**
 * Check if SAN device needs to be reopened
 *
 * @v sandev		SAN device
 * @ret needs_reopen	SAN device needs to be reopened
 */
static inline int sandev_needs_reopen ( struct san_device *sandev ) {
	return ( sandev->active == NULL );
}

extern struct san_device * sandev_find ( unsigned int drive );
extern int sandev_reopen ( struct san_device *sandev );
extern int sandev_reset ( struct san_device *sandev );
extern int sandev_read ( struct san_device *sandev, uint64_t lba,
			 unsigned int count, userptr_t buffer );
extern int sandev_write ( struct san_device *sandev, uint64_t lba,
			  unsigned int count, userptr_t buffer );
extern struct san_device * alloc_sandev ( struct uri **uris, unsigned int count,
					  size_t priv_size );
extern int register_sandev ( struct san_device *sandev, unsigned int drive,
			     unsigned int flags );
extern void unregister_sandev ( struct san_device *sandev );
extern unsigned int san_default_drive ( void );

#endif /* _IPXE_SANBOOT_H */
