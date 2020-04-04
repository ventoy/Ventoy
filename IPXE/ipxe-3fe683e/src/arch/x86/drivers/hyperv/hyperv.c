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
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Hyper-V driver
 *
 */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <byteswap.h>
#include <pic8259.h>
#include <ipxe/malloc.h>
#include <ipxe/device.h>
#include <ipxe/timer.h>
#include <ipxe/quiesce.h>
#include <ipxe/cpuid.h>
#include <ipxe/msr.h>
#include <ipxe/hyperv.h>
#include <ipxe/vmbus.h>
#include "hyperv.h"

/** Maximum time to wait for a message response
 *
 * This is a policy decision.
 */
#define HV_MESSAGE_MAX_WAIT_MS 1000

/** Hyper-V timer frequency (fixed 10Mhz) */
#define HV_TIMER_HZ 10000000

/** Hyper-V timer scale factor (used to avoid 64-bit division) */
#define HV_TIMER_SHIFT 18

/**
 * Convert a Hyper-V status code to an iPXE status code
 *
 * @v status		Hyper-V status code
 * @ret rc		iPXE status code (before negation)
 */
#define EHV( status ) EPLATFORM ( EINFO_EPLATFORM, (status) )

/**
 * Allocate zeroed pages
 *
 * @v hv		Hyper-V hypervisor
 * @v ...		Page addresses to fill in, terminated by NULL
 * @ret rc		Return status code
 */
__attribute__ (( sentinel )) int
hv_alloc_pages ( struct hv_hypervisor *hv, ... ) {
	va_list args;
	void **page;
	int i;

	/* Allocate and zero pages */
	va_start ( args, hv );
	for ( i = 0 ; ( ( page = va_arg ( args, void ** ) ) != NULL ); i++ ) {
		*page = malloc_dma ( PAGE_SIZE, PAGE_SIZE );
		if ( ! *page )
			goto err_alloc;
		memset ( *page, 0, PAGE_SIZE );
	}
	va_end ( args );

	return 0;

 err_alloc:
	va_end ( args );
	va_start ( args, hv );
	for ( ; i >= 0 ; i-- ) {
		page = va_arg ( args, void ** );
		free_dma ( *page, PAGE_SIZE );
	}
	va_end ( args );
	return -ENOMEM;
}

/**
 * Free pages
 *
 * @v hv		Hyper-V hypervisor
 * @v ...		Page addresses, terminated by NULL
 */
__attribute__ (( sentinel )) void
hv_free_pages ( struct hv_hypervisor *hv, ... ) {
	va_list args;
	void *page;

	va_start ( args, hv );
	while ( ( page = va_arg ( args, void * ) ) != NULL )
		free_dma ( page, PAGE_SIZE );
	va_end ( args );
}

/**
 * Allocate message buffer
 *
 * @v hv		Hyper-V hypervisor
 * @ret rc		Return status code
 */
static int hv_alloc_message ( struct hv_hypervisor *hv ) {

	/* Allocate buffer.  Must be aligned to at least 8 bytes and
	 * must not cross a page boundary, so align on its own size.
	 */
	hv->message = malloc_dma ( sizeof ( *hv->message ),
				   sizeof ( *hv->message ) );
	if ( ! hv->message )
		return -ENOMEM;

	return 0;
}

/**
 * Free message buffer
 *
 * @v hv		Hyper-V hypervisor
 */
static void hv_free_message ( struct hv_hypervisor *hv ) {

	/* Free buffer */
	free_dma ( hv->message, sizeof ( *hv->message ) );
}

/**
 * Check whether or not we are running in Hyper-V
 *
 * @ret rc		Return status code
 */
static int hv_check_hv ( void ) {
	struct x86_features features;
	uint32_t interface_id;
	uint32_t discard_ebx;
	uint32_t discard_ecx;
	uint32_t discard_edx;

	/* Check for presence of a hypervisor (not necessarily Hyper-V) */
	x86_features ( &features );
	if ( ! ( features.intel.ecx & CPUID_FEATURES_INTEL_ECX_HYPERVISOR ) ) {
		DBGC ( HV_INTERFACE_ID, "HV not running in a hypervisor\n" );
		return -ENODEV;
	}

	/* Check that hypervisor is Hyper-V */
	cpuid ( HV_CPUID_INTERFACE_ID, 0, &interface_id, &discard_ebx,
		&discard_ecx, &discard_edx );
	if ( interface_id != HV_INTERFACE_ID ) {
		DBGC ( HV_INTERFACE_ID, "HV not running in Hyper-V (interface "
		       "ID %#08x)\n", interface_id );
		return -ENODEV;
	}

	return 0;
}

/**
 * Check required features
 *
 * @v hv		Hyper-V hypervisor
 * @ret rc		Return status code
 */
static int hv_check_features ( struct hv_hypervisor *hv ) {
	uint32_t available;
	uint32_t permissions;
	uint32_t discard_ecx;
	uint32_t discard_edx;

	/* Check that required features and privileges are available */
	cpuid ( HV_CPUID_FEATURES, 0, &available, &permissions, &discard_ecx,
		&discard_edx );
	if ( ! ( available & HV_FEATURES_AVAIL_HYPERCALL_MSR ) ) {
		DBGC ( hv, "HV %p has no hypercall MSRs (features %08x:%08x)\n",
		       hv, available, permissions );
		return -ENODEV;
	}
	if ( ! ( available & HV_FEATURES_AVAIL_SYNIC_MSR ) ) {
		DBGC ( hv, "HV %p has no SynIC MSRs (features %08x:%08x)\n",
		       hv, available, permissions );
		return -ENODEV;
	}
	if ( ! ( permissions & HV_FEATURES_PERM_POST_MESSAGES ) ) {
		DBGC ( hv, "HV %p cannot post messages (features %08x:%08x)\n",
		       hv, available, permissions );
		return -EACCES;
	}
	if ( ! ( permissions & HV_FEATURES_PERM_SIGNAL_EVENTS ) ) {
		DBGC ( hv, "HV %p cannot signal events (features %08x:%08x)",
		       hv, available, permissions );
		return -EACCES;
	}

	return 0;
}

/**
 * Check that Gen 2 UEFI firmware is not running
 *
 * @v hv		Hyper-V hypervisor
 * @ret rc		Return status code
 *
 * We must not steal ownership from the Gen 2 UEFI firmware, since
 * doing so will cause an immediate crash.  Avoid this by checking for
 * the guest OS identity known to be used by the Gen 2 UEFI firmware.
 */
static int hv_check_uefi ( struct hv_hypervisor *hv ) {
	uint64_t guest_os_id;

	/* Check for UEFI firmware's guest OS identity */
	guest_os_id = rdmsr ( HV_X64_MSR_GUEST_OS_ID );
	if ( guest_os_id == HV_GUEST_OS_ID_UEFI ) {
		DBGC ( hv, "HV %p is owned by UEFI firmware\n", hv );
		return -ENOTSUP;
	}

	return 0;
}

/**
 * Map hypercall page
 *
 * @v hv		Hyper-V hypervisor
 */
static void hv_map_hypercall ( struct hv_hypervisor *hv ) {
	union {
		struct {
			uint32_t ebx;
			uint32_t ecx;
			uint32_t edx;
		} __attribute__ (( packed ));
		char text[ 13 /* "bbbbccccdddd" + NUL */ ];
	} vendor_id;
	uint32_t build;
	uint32_t version;
	uint32_t discard_eax;
	uint32_t discard_ecx;
	uint32_t discard_edx;
	uint64_t guest_os_id;
	uint64_t hypercall;

	/* Report guest OS identity */
	guest_os_id = rdmsr ( HV_X64_MSR_GUEST_OS_ID );
	if ( guest_os_id != 0 ) {
		DBGC ( hv, "HV %p guest OS ID MSR was %#08llx\n",
		       hv, guest_os_id );
	}
	guest_os_id = HV_GUEST_OS_ID_IPXE;
	DBGC2 ( hv, "HV %p guest OS ID MSR is %#08llx\n", hv, guest_os_id );
	wrmsr ( HV_X64_MSR_GUEST_OS_ID, guest_os_id );

	/* Get hypervisor system identity (for debugging) */
	cpuid ( HV_CPUID_VENDOR_ID, 0, &discard_eax, &vendor_id.ebx,
		&vendor_id.ecx, &vendor_id.edx );
	vendor_id.text[ sizeof ( vendor_id.text ) - 1 ] = '\0';
	cpuid ( HV_CPUID_HYPERVISOR_ID, 0, &build, &version, &discard_ecx,
		&discard_edx );
	DBGC ( hv, "HV %p detected \"%s\" version %d.%d build %d\n", hv,
	       vendor_id.text, ( version >> 16 ), ( version & 0xffff ), build );

	/* Map hypercall page */
	hypercall = rdmsr ( HV_X64_MSR_HYPERCALL );
	hypercall &= ( PAGE_SIZE - 1 );
	hypercall |= ( virt_to_phys ( hv->hypercall ) | HV_HYPERCALL_ENABLE );
	DBGC2 ( hv, "HV %p hypercall MSR is %#08llx\n", hv, hypercall );
	wrmsr ( HV_X64_MSR_HYPERCALL, hypercall );
}

/**
 * Unmap hypercall page
 *
 * @v hv		Hyper-V hypervisor
 */
static void hv_unmap_hypercall ( struct hv_hypervisor *hv ) {
	uint64_t hypercall;
	uint64_t guest_os_id;

	/* Unmap the hypercall page */
	hypercall = rdmsr ( HV_X64_MSR_HYPERCALL );
	hypercall &= ( ( PAGE_SIZE - 1 ) & ~HV_HYPERCALL_ENABLE );
	DBGC2 ( hv, "HV %p hypercall MSR is %#08llx\n", hv, hypercall );
	wrmsr ( HV_X64_MSR_HYPERCALL, hypercall );

	/* Reset the guest OS identity */
	guest_os_id = 0;
	DBGC2 ( hv, "HV %p guest OS ID MSR is %#08llx\n", hv, guest_os_id );
	wrmsr ( HV_X64_MSR_GUEST_OS_ID, guest_os_id );
}

/**
 * Map synthetic interrupt controller
 *
 * @v hv		Hyper-V hypervisor
 */
static void hv_map_synic ( struct hv_hypervisor *hv ) {
	uint64_t simp;
	uint64_t siefp;
	uint64_t scontrol;

	/* Zero SynIC message and event pages */
	memset ( hv->synic.message, 0, PAGE_SIZE );
	memset ( hv->synic.event, 0, PAGE_SIZE );

	/* Map SynIC message page */
	simp = rdmsr ( HV_X64_MSR_SIMP );
	simp &= ( PAGE_SIZE - 1 );
	simp |= ( virt_to_phys ( hv->synic.message ) | HV_SIMP_ENABLE );
	DBGC2 ( hv, "HV %p SIMP MSR is %#08llx\n", hv, simp );
	wrmsr ( HV_X64_MSR_SIMP, simp );

	/* Map SynIC event page */
	siefp = rdmsr ( HV_X64_MSR_SIEFP );
	siefp &= ( PAGE_SIZE - 1 );
	siefp |= ( virt_to_phys ( hv->synic.event ) | HV_SIEFP_ENABLE );
	DBGC2 ( hv, "HV %p SIEFP MSR is %#08llx\n", hv, siefp );
	wrmsr ( HV_X64_MSR_SIEFP, siefp );

	/* Enable SynIC */
	scontrol = rdmsr ( HV_X64_MSR_SCONTROL );
	scontrol |= HV_SCONTROL_ENABLE;
	DBGC2 ( hv, "HV %p SCONTROL MSR is %#08llx\n", hv, scontrol );
	wrmsr ( HV_X64_MSR_SCONTROL, scontrol );
}

/**
 * Unmap synthetic interrupt controller, leaving SCONTROL untouched
 *
 * @v hv		Hyper-V hypervisor
 */
static void hv_unmap_synic_no_scontrol ( struct hv_hypervisor *hv ) {
	uint64_t siefp;
	uint64_t simp;

	/* Unmap SynIC event page */
	siefp = rdmsr ( HV_X64_MSR_SIEFP );
	siefp &= ( ( PAGE_SIZE - 1 ) & ~HV_SIEFP_ENABLE );
	DBGC2 ( hv, "HV %p SIEFP MSR is %#08llx\n", hv, siefp );
	wrmsr ( HV_X64_MSR_SIEFP, siefp );

	/* Unmap SynIC message page */
	simp = rdmsr ( HV_X64_MSR_SIMP );
	simp &= ( ( PAGE_SIZE - 1 ) & ~HV_SIMP_ENABLE );
	DBGC2 ( hv, "HV %p SIMP MSR is %#08llx\n", hv, simp );
	wrmsr ( HV_X64_MSR_SIMP, simp );
}

/**
 * Unmap synthetic interrupt controller
 *
 * @v hv		Hyper-V hypervisor
 */
static void hv_unmap_synic ( struct hv_hypervisor *hv ) {
	uint64_t scontrol;

	/* Disable SynIC */
	scontrol = rdmsr ( HV_X64_MSR_SCONTROL );
	scontrol &= ~HV_SCONTROL_ENABLE;
	DBGC2 ( hv, "HV %p SCONTROL MSR is %#08llx\n", hv, scontrol );
	wrmsr ( HV_X64_MSR_SCONTROL, scontrol );

	/* Unmap SynIC event and message pages */
	hv_unmap_synic_no_scontrol ( hv );
}

/**
 * Enable synthetic interrupt
 *
 * @v hv		Hyper-V hypervisor
 * @v sintx		Synthetic interrupt number
 */
void hv_enable_sint ( struct hv_hypervisor *hv, unsigned int sintx ) {
	unsigned long msr = HV_X64_MSR_SINT ( sintx );
	uint64_t sint;

	/* Enable synthetic interrupt
	 *
	 * We have to enable the interrupt, otherwise messages will
	 * not be delivered (even though the documentation implies
	 * that polling for messages is possible).  We enable AutoEOI
	 * and hook the interrupt to the obsolete IRQ13 (FPU
	 * exception) vector, which will be implemented as a no-op.
	 */
	sint = rdmsr ( msr );
	sint &= ~( HV_SINT_MASKED | HV_SINT_VECTOR_MASK );
	sint |= ( HV_SINT_AUTO_EOI |
		  HV_SINT_VECTOR ( IRQ_INT ( 13 /* See comment above */ ) ) );
	DBGC2 ( hv, "HV %p SINT%d MSR is %#08llx\n", hv, sintx, sint );
	wrmsr ( msr, sint );
}

/**
 * Disable synthetic interrupt
 *
 * @v hv		Hyper-V hypervisor
 * @v sintx		Synthetic interrupt number
 */
void hv_disable_sint ( struct hv_hypervisor *hv, unsigned int sintx ) {
	unsigned long msr = HV_X64_MSR_SINT ( sintx );
	uint64_t sint;

	/* Do nothing if interrupt is already disabled */
	sint = rdmsr ( msr );
	if ( sint & HV_SINT_MASKED )
		return;

	/* Disable synthetic interrupt */
	sint &= ~HV_SINT_AUTO_EOI;
	sint |= HV_SINT_MASKED;
	DBGC2 ( hv, "HV %p SINT%d MSR is %#08llx\n", hv, sintx, sint );
	wrmsr ( msr, sint );
}

/**
 * Post message
 *
 * @v hv		Hyper-V hypervisor
 * @v id		Connection ID
 * @v type		Message type
 * @v data		Message
 * @v len		Length of message
 * @ret rc		Return status code
 */
int hv_post_message ( struct hv_hypervisor *hv, unsigned int id,
		      unsigned int type, const void *data, size_t len ) {
	struct hv_post_message *msg = &hv->message->posted;
	int status;
	int rc;

	/* Sanity check */
	assert ( len <= sizeof ( msg->data ) );

	/* Construct message */
	memset ( msg, 0, sizeof ( *msg ) );
	msg->id = cpu_to_le32 ( id );
	msg->type = cpu_to_le32 ( type );
	msg->len = cpu_to_le32 ( len );
	memcpy ( msg->data, data, len );
	DBGC2 ( hv, "HV %p connection %d posting message type %#08x:\n",
		hv, id, type );
	DBGC2_HDA ( hv, 0, msg->data, len );

	/* Post message */
	if ( ( status = hv_call ( hv, HV_POST_MESSAGE, msg, NULL ) ) != 0 ) {
		rc = -EHV ( status );
		DBGC ( hv, "HV %p could not post message to %#08x: %s\n",
		       hv, id, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Wait for received message
 *
 * @v hv		Hyper-V hypervisor
 * @v sintx		Synthetic interrupt number
 * @ret rc		Return status code
 */
int hv_wait_for_message ( struct hv_hypervisor *hv, unsigned int sintx ) {
	struct hv_message *msg = &hv->message->received;
	struct hv_message *src = &hv->synic.message[sintx];
	unsigned int retries;
	size_t len;

	/* Wait for message to arrive */
	for ( retries = 0 ; retries < HV_MESSAGE_MAX_WAIT_MS ; retries++ ) {

		/* Check for message */
		if ( src->type ) {

			/* Copy message */
			memset ( msg, 0, sizeof ( *msg ) );
			len = src->len;
			assert ( len <= sizeof ( *msg ) );
			memcpy ( msg, src,
				 ( offsetof ( typeof ( *msg ), data ) + len ) );
			DBGC2 ( hv, "HV %p SINT%d received message type "
				"%#08x:\n", hv, sintx,
				le32_to_cpu ( msg->type ) );
			DBGC2_HDA ( hv, 0, msg->data, len );

			/* Consume message */
			src->type = 0;

			return 0;
		}

		/* Trigger message delivery */
		wrmsr ( HV_X64_MSR_EOM, 0 );

		/* Delay */
		mdelay ( 1 );
	}

	DBGC ( hv, "HV %p SINT%d timed out waiting for message\n",
	       hv, sintx );
	return -ETIMEDOUT;
}

/**
 * Signal event
 *
 * @v hv		Hyper-V hypervisor
 * @v id		Connection ID
 * @v flag		Flag number
 * @ret rc		Return status code
 */
int hv_signal_event ( struct hv_hypervisor *hv, unsigned int id,
		      unsigned int flag ) {
	struct hv_signal_event *event = &hv->message->signalled;
	int status;
	int rc;

	/* Construct event */
	memset ( event, 0, sizeof ( *event ) );
	event->id = cpu_to_le32 ( id );
	event->flag = cpu_to_le16 ( flag );

	/* Signal event */
	if ( ( status = hv_call ( hv, HV_SIGNAL_EVENT, event, NULL ) ) != 0 ) {
		rc = -EHV ( status );
		DBGC ( hv, "HV %p could not signal event to %#08x: %s\n",
		       hv, id, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Probe root device
 *
 * @v rootdev		Root device
 * @ret rc		Return status code
 */
static int hv_probe ( struct root_device *rootdev ) {
	struct hv_hypervisor *hv;
	int rc;

	/* Check we are running in Hyper-V */
	if ( ( rc = hv_check_hv() ) != 0 )
		goto err_check_hv;

	/* Allocate and initialise structure */
	hv = zalloc ( sizeof ( *hv ) );
	if ( ! hv ) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	/* Check features */
	if ( ( rc = hv_check_features ( hv ) ) != 0 )
		goto err_check_features;

	/* Check that Gen 2 UEFI firmware is not running */
	if ( ( rc = hv_check_uefi ( hv ) ) != 0 )
		goto err_check_uefi;

	/* Allocate pages */
	if ( ( rc = hv_alloc_pages ( hv, &hv->hypercall, &hv->synic.message,
				     &hv->synic.event, NULL ) ) != 0 )
		goto err_alloc_pages;

	/* Allocate message buffer */
	if ( ( rc = hv_alloc_message ( hv ) ) != 0 )
		goto err_alloc_message;

	/* Map hypercall page */
	hv_map_hypercall ( hv );

	/* Map synthetic interrupt controller */
	hv_map_synic ( hv );

	/* Probe Hyper-V devices */
	if ( ( rc = vmbus_probe ( hv, &rootdev->dev ) ) != 0 )
		goto err_vmbus_probe;

	rootdev_set_drvdata ( rootdev, hv );
	return 0;

	vmbus_remove ( hv, &rootdev->dev );
 err_vmbus_probe:
	hv_unmap_synic ( hv );
	hv_unmap_hypercall ( hv );
	hv_free_message ( hv );
 err_alloc_message:
	hv_free_pages ( hv, hv->hypercall, hv->synic.message, hv->synic.event,
			NULL );
 err_alloc_pages:
 err_check_uefi:
 err_check_features:
	free ( hv );
 err_alloc:
 err_check_hv:
	return rc;
}

/**
 * Remove root device
 *
 * @v rootdev		Root device
 */
static void hv_remove ( struct root_device *rootdev ) {
	struct hv_hypervisor *hv = rootdev_get_drvdata ( rootdev );

	vmbus_remove ( hv, &rootdev->dev );
	hv_unmap_synic ( hv );
	hv_unmap_hypercall ( hv );
	hv_free_message ( hv );
	hv_free_pages ( hv, hv->hypercall, hv->synic.message, hv->synic.event,
			NULL );
	free ( hv );
	rootdev_set_drvdata ( rootdev, NULL );
}

/** Hyper-V root device driver */
static struct root_driver hv_root_driver = {
	.probe = hv_probe,
	.remove = hv_remove,
};

/** Hyper-V root device */
struct root_device hv_root_device __root_device = {
	.dev = { .name = "Hyper-V" },
	.driver = &hv_root_driver,
};

/**
 * Quiesce system
 *
 */
static void hv_quiesce ( void ) {
	struct hv_hypervisor *hv = rootdev_get_drvdata ( &hv_root_device );
	unsigned int i;

	/* Do nothing if we are not running in Hyper-V */
	if ( ! hv )
		return;

	/* The "enlightened" portions of the Windows Server 2016 boot
	 * process will not cleanly take ownership of an active
	 * Hyper-V connection.  Experimentation shows that the minimum
	 * requirement is that we disable the SynIC message page
	 * (i.e. zero the SIMP MSR).
	 *
	 * We cannot perform a full shutdown of the Hyper-V
	 * connection.  Experimentation shows that if we disable the
	 * SynIC (i.e. zero the SCONTROL MSR) then Windows Server 2016
	 * will enter an indefinite wait loop.
	 *
	 * Attempt to create a safe handover environment by resetting
	 * all MSRs except for SCONTROL.
	 *
	 * Note that we do not shut down our VMBus devices, since we
	 * may need to unquiesce the system and continue operation.
	 */

	/* Disable all synthetic interrupts */
	for ( i = 0 ; i <= HV_SINT_MAX ; i++ )
		hv_disable_sint ( hv, i );

	/* Unmap synthetic interrupt controller, leaving SCONTROL
	 * enabled (see above).
	 */
	hv_unmap_synic_no_scontrol ( hv );

	/* Unmap hypercall page */
	hv_unmap_hypercall ( hv );

	DBGC ( hv, "HV %p quiesced\n", hv );
}

/**
 * Unquiesce system
 *
 */
static void hv_unquiesce ( void ) {
	struct hv_hypervisor *hv = rootdev_get_drvdata ( &hv_root_device );
	uint64_t simp;
	int rc;

	/* Do nothing if we are not running in Hyper-V */
	if ( ! hv )
		return;

	/* Experimentation shows that the "enlightened" portions of
	 * Windows Server 2016 will break our Hyper-V connection at
	 * some point during a SAN boot.  Surprisingly it does not
	 * change the guest OS ID MSR, but it does leave the SynIC
	 * message page disabled.
	 *
	 * Our own explicit quiescing procedure will also disable the
	 * SynIC message page.  We can therefore use the SynIC message
	 * page enable bit as a heuristic to determine when we need to
	 * reestablish our Hyper-V connection.
	 */
	simp = rdmsr ( HV_X64_MSR_SIMP );
	if ( simp & HV_SIMP_ENABLE )
		return;

	/* Remap hypercall page */
	hv_map_hypercall ( hv );

	/* Remap synthetic interrupt controller */
	hv_map_synic ( hv );

	/* Reset Hyper-V devices */
	if ( ( rc = vmbus_reset ( hv, &hv_root_device.dev ) ) != 0 ) {
		DBGC ( hv, "HV %p could not unquiesce: %s\n",
		       hv, strerror ( rc ) );
		/* Nothing we can do */
		return;
	}
}

/** Hyper-V quiescer */
struct quiescer hv_quiescer __quiescer = {
	.quiesce = hv_quiesce,
	.unquiesce = hv_unquiesce,
};

/**
 * Probe timer
 *
 * @ret rc		Return status code
 */
static int hv_timer_probe ( void ) {
	uint32_t available;
	uint32_t discard_ebx;
	uint32_t discard_ecx;
	uint32_t discard_edx;
	int rc;

	/* Check we are running in Hyper-V */
	if ( ( rc = hv_check_hv() ) != 0 )
		return rc;

	/* Check for available reference counter */
	cpuid ( HV_CPUID_FEATURES, 0, &available, &discard_ebx, &discard_ecx,
		&discard_edx );
	if ( ! ( available & HV_FEATURES_AVAIL_TIME_REF_COUNT_MSR ) ) {
		DBGC ( HV_INTERFACE_ID, "HV has no time reference counter\n" );
		return -ENODEV;
	}

	return 0;
}

/**
 * Get current system time in ticks
 *
 * @ret ticks		Current time, in ticks
 */
static unsigned long hv_currticks ( void ) {

	/* Calculate time using a combination of bit shifts and
	 * multiplication (to avoid a 64-bit division).
	 */
	return ( ( rdmsr ( HV_X64_MSR_TIME_REF_COUNT ) >> HV_TIMER_SHIFT ) *
		 ( TICKS_PER_SEC / ( HV_TIMER_HZ >> HV_TIMER_SHIFT ) ) );
}

/**
 * Delay for a fixed number of microseconds
 *
 * @v usecs		Number of microseconds for which to delay
 */
static void hv_udelay ( unsigned long usecs ) {
	uint32_t start;
	uint32_t elapsed;
	uint32_t threshold;

	/* Spin until specified number of 10MHz ticks have elapsed */
	start = rdmsr ( HV_X64_MSR_TIME_REF_COUNT );
	threshold = ( usecs * ( HV_TIMER_HZ / 1000000 ) );
	do {
		elapsed = ( rdmsr ( HV_X64_MSR_TIME_REF_COUNT ) - start );
	} while ( elapsed < threshold );
}

/** Hyper-V timer */
struct timer hv_timer __timer ( TIMER_PREFERRED ) = {
	.name = "Hyper-V",
	.probe = hv_timer_probe,
	.currticks = hv_currticks,
	.udelay = hv_udelay,
};

/* Drag in objects via hv_root_device */
REQUIRING_SYMBOL ( hv_root_device );

/* Drag in netvsc driver */
//REQUIRE_OBJECT ( netvsc );
