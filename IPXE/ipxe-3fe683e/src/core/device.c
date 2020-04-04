/*
 * Copyright (C) 2006 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <string.h>
#include <ipxe/list.h>
#include <ipxe/tables.h>
#include <ipxe/init.h>
#include <ipxe/interface.h>
#include <ipxe/device.h>

/**
 * @file
 *
 * Device model
 *
 */

/** Registered root devices */
static LIST_HEAD ( devices );

/** Device removal inhibition counter */
int device_keep_count = 0;

/**
 * Probe a root device
 *
 * @v rootdev		Root device
 * @ret rc		Return status code
 */
static int rootdev_probe ( struct root_device *rootdev ) {
	int rc;

	DBG ( "Adding %s root bus\n", rootdev->dev.name );
	if ( ( rc = rootdev->driver->probe ( rootdev ) ) != 0 ) {
		DBG ( "Failed to add %s root bus: %s\n",
		      rootdev->dev.name, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Remove a root device
 *
 * @v rootdev		Root device
 */
static void rootdev_remove ( struct root_device *rootdev ) {
	rootdev->driver->remove ( rootdev );
	DBG ( "Removed %s root bus\n", rootdev->dev.name );
}

/**
 * Probe all devices
 *
 * This initiates probing for all devices in the system.  After this
 * call, the device hierarchy will be populated, and all hardware
 * should be ready to use.
 */
static void probe_devices ( void ) {
	struct root_device *rootdev;
	int rc;

	for_each_table_entry ( rootdev, ROOT_DEVICES ) {
		list_add ( &rootdev->dev.siblings, &devices );
		INIT_LIST_HEAD ( &rootdev->dev.children );
		if ( ( rc = rootdev_probe ( rootdev ) ) != 0 )
			list_del ( &rootdev->dev.siblings );
	}
}

/**
 * Remove all devices
 *
 */
static void remove_devices ( int booting __unused ) {
	struct root_device *rootdev;
	struct root_device *tmp;

	if ( device_keep_count != 0 ) {
		DBG ( "Refusing to remove devices on shutdown\n" );
		return;
	}

	list_for_each_entry_safe ( rootdev, tmp, &devices, dev.siblings ) {
		rootdev_remove ( rootdev );
		list_del ( &rootdev->dev.siblings );
	}
}

//struct startup_fn startup_devices __startup_fn ( STARTUP_NORMAL ) = {
struct startup_fn startup_devices  = {
	.name = "devices",
	.startup = probe_devices,
	.shutdown = remove_devices,
};

/**
 * Identify a device behind an interface
 *
 * @v intf		Interface
 * @ret device		Device, or NULL
 */
struct device * identify_device ( struct interface *intf ) {
	struct interface *dest;
	identify_device_TYPE ( void * ) *op =
		intf_get_dest_op ( intf, identify_device, &dest );
	void *object = intf_object ( dest );
	void *device;

	if ( op ) {
		device = op ( object );
	} else {
		/* Default is to return NULL */
		device = NULL;
	}

	intf_put ( dest );
	return device;
}
