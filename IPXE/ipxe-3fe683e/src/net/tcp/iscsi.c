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

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <byteswap.h>
#include <ipxe/vsprintf.h>
#include <ipxe/socket.h>
#include <ipxe/iobuf.h>
#include <ipxe/uri.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/scsi.h>
#include <ipxe/process.h>
#include <ipxe/uaccess.h>
#include <ipxe/tcpip.h>
#include <ipxe/settings.h>
#include <ipxe/features.h>
#include <ipxe/base16.h>
#include <ipxe/base64.h>
#include <ipxe/ibft.h>
#include <ipxe/iscsi.h>

/** @file
 *
 * iSCSI protocol
 *
 */

FEATURE ( FEATURE_PROTOCOL, "iSCSI", DHCP_EB_FEATURE_ISCSI, 1 );

/* Disambiguate the various error causes */
#define EACCES_INCORRECT_TARGET_USERNAME \
	__einfo_error ( EINFO_EACCES_INCORRECT_TARGET_USERNAME )
#define EINFO_EACCES_INCORRECT_TARGET_USERNAME \
	__einfo_uniqify ( EINFO_EACCES, 0x01, "Incorrect target username" )
#define EACCES_INCORRECT_TARGET_PASSWORD \
	__einfo_error ( EINFO_EACCES_INCORRECT_TARGET_PASSWORD )
#define EINFO_EACCES_INCORRECT_TARGET_PASSWORD \
	__einfo_uniqify ( EINFO_EACCES, 0x02, "Incorrect target password" )
#define EINVAL_ROOT_PATH_TOO_SHORT \
	__einfo_error ( EINFO_EINVAL_ROOT_PATH_TOO_SHORT )
#define EINFO_EINVAL_ROOT_PATH_TOO_SHORT \
	__einfo_uniqify ( EINFO_EINVAL, 0x01, "Root path too short" )
#define EINVAL_BAD_CREDENTIAL_MIX \
	__einfo_error ( EINFO_EINVAL_BAD_CREDENTIAL_MIX )
#define EINFO_EINVAL_BAD_CREDENTIAL_MIX \
	__einfo_uniqify ( EINFO_EINVAL, 0x02, "Bad credential mix" )
#define EINVAL_NO_ROOT_PATH \
	__einfo_error ( EINFO_EINVAL_NO_ROOT_PATH )
#define EINFO_EINVAL_NO_ROOT_PATH \
	__einfo_uniqify ( EINFO_EINVAL, 0x03, "No root path" )
#define EINVAL_NO_TARGET_IQN \
	__einfo_error ( EINFO_EINVAL_NO_TARGET_IQN )
#define EINFO_EINVAL_NO_TARGET_IQN \
	__einfo_uniqify ( EINFO_EINVAL, 0x04, "No target IQN" )
#define EINVAL_NO_INITIATOR_IQN \
	__einfo_error ( EINFO_EINVAL_NO_INITIATOR_IQN )
#define EINFO_EINVAL_NO_INITIATOR_IQN \
	__einfo_uniqify ( EINFO_EINVAL, 0x05, "No initiator IQN" )
#define EIO_TARGET_UNAVAILABLE \
	__einfo_error ( EINFO_EIO_TARGET_UNAVAILABLE )
#define EINFO_EIO_TARGET_UNAVAILABLE \
	__einfo_uniqify ( EINFO_EIO, 0x01, "Target not currently operational" )
#define EIO_TARGET_NO_RESOURCES \
	__einfo_error ( EINFO_EIO_TARGET_NO_RESOURCES )
#define EINFO_EIO_TARGET_NO_RESOURCES \
	__einfo_uniqify ( EINFO_EIO, 0x02, "Target out of resources" )
#define ENOTSUP_INITIATOR_STATUS \
	__einfo_error ( EINFO_ENOTSUP_INITIATOR_STATUS )
#define EINFO_ENOTSUP_INITIATOR_STATUS \
	__einfo_uniqify ( EINFO_ENOTSUP, 0x01, "Unsupported initiator status" )
#define ENOTSUP_OPCODE \
	__einfo_error ( EINFO_ENOTSUP_OPCODE )
#define EINFO_ENOTSUP_OPCODE \
	__einfo_uniqify ( EINFO_ENOTSUP, 0x02, "Unsupported opcode" )
#define ENOTSUP_DISCOVERY \
	__einfo_error ( EINFO_ENOTSUP_DISCOVERY )
#define EINFO_ENOTSUP_DISCOVERY \
	__einfo_uniqify ( EINFO_ENOTSUP, 0x03, "Discovery not supported" )
#define ENOTSUP_TARGET_STATUS \
	__einfo_error ( EINFO_ENOTSUP_TARGET_STATUS )
#define EINFO_ENOTSUP_TARGET_STATUS \
	__einfo_uniqify ( EINFO_ENOTSUP, 0x04, "Unsupported target status" )
#define EPERM_INITIATOR_AUTHENTICATION \
	__einfo_error ( EINFO_EPERM_INITIATOR_AUTHENTICATION )
#define EINFO_EPERM_INITIATOR_AUTHENTICATION \
	__einfo_uniqify ( EINFO_EPERM, 0x01, "Initiator authentication failed" )
#define EPERM_INITIATOR_AUTHORISATION \
	__einfo_error ( EINFO_EPERM_INITIATOR_AUTHORISATION )
#define EINFO_EPERM_INITIATOR_AUTHORISATION \
	__einfo_uniqify ( EINFO_EPERM, 0x02, "Initiator not authorised" )
#define EPROTO_INVALID_CHAP_ALGORITHM \
	__einfo_error ( EINFO_EPROTO_INVALID_CHAP_ALGORITHM )
#define EINFO_EPROTO_INVALID_CHAP_ALGORITHM \
	__einfo_uniqify ( EINFO_EPROTO, 0x01, "Invalid CHAP algorithm" )
#define EPROTO_INVALID_CHAP_IDENTIFIER \
	__einfo_error ( EINFO_EPROTO_INVALID_CHAP_IDENTIFIER )
#define EINFO_EPROTO_INVALID_CHAP_IDENTIFIER \
	__einfo_uniqify ( EINFO_EPROTO, 0x02, "Invalid CHAP identifier" )
#define EPROTO_INVALID_LARGE_BINARY \
	__einfo_error ( EINFO_EPROTO_INVALID_LARGE_BINARY )
#define EINFO_EPROTO_INVALID_LARGE_BINARY \
	__einfo_uniqify ( EINFO_EPROTO, 0x03, "Invalid large binary value" )
#define EPROTO_INVALID_CHAP_RESPONSE \
	__einfo_error ( EINFO_EPROTO_INVALID_CHAP_RESPONSE )
#define EINFO_EPROTO_INVALID_CHAP_RESPONSE \
	__einfo_uniqify ( EINFO_EPROTO, 0x04, "Invalid CHAP response" )
#define EPROTO_INVALID_KEY_VALUE_PAIR \
	__einfo_error ( EINFO_EPROTO_INVALID_KEY_VALUE_PAIR )
#define EINFO_EPROTO_INVALID_KEY_VALUE_PAIR \
	__einfo_uniqify ( EINFO_EPROTO, 0x05, "Invalid key/value pair" )
#define EPROTO_VALUE_REJECTED \
	__einfo_error ( EINFO_EPROTO_VALUE_REJECTED )
#define EINFO_EPROTO_VALUE_REJECTED					\
	__einfo_uniqify ( EINFO_EPROTO, 0x06, "Parameter rejected" )

static void iscsi_start_tx ( struct iscsi_session *iscsi );
static void iscsi_start_login ( struct iscsi_session *iscsi );
static void iscsi_start_data_out ( struct iscsi_session *iscsi,
				   unsigned int datasn );

/**
 * Finish receiving PDU data into buffer
 *
 * @v iscsi		iSCSI session
 */
static void iscsi_rx_buffered_data_done ( struct iscsi_session *iscsi ) {
	free ( iscsi->rx_buffer );
	iscsi->rx_buffer = NULL;
}

/**
 * Receive PDU data into buffer
 *
 * @v iscsi		iSCSI session
 * @v data		Data to receive
 * @v len		Length of data
 * @ret rc		Return status code
 *
 * This can be used when the RX PDU type handler wishes to buffer up
 * all received data and process the PDU as a single unit.  The caller
 * is repsonsible for calling iscsi_rx_buffered_data_done() after
 * processing the data.
 */
static int iscsi_rx_buffered_data ( struct iscsi_session *iscsi,
				    const void *data, size_t len ) {

	/* Allocate buffer on first call */
	if ( ! iscsi->rx_buffer ) {
		iscsi->rx_buffer = malloc ( iscsi->rx_len );
		if ( ! iscsi->rx_buffer )
			return -ENOMEM;
	}

	/* Copy data to buffer */
	assert ( ( iscsi->rx_offset + len ) <= iscsi->rx_len );
	memcpy ( ( iscsi->rx_buffer + iscsi->rx_offset ), data, len );

	return 0;
}

/**
 * Free iSCSI session
 *
 * @v refcnt		Reference counter
 */
static void iscsi_free ( struct refcnt *refcnt ) {
	struct iscsi_session *iscsi =
		container_of ( refcnt, struct iscsi_session, refcnt );

	free ( iscsi->initiator_iqn );
	free ( iscsi->target_address );
	free ( iscsi->target_iqn );
	free ( iscsi->initiator_username );
	free ( iscsi->initiator_password );
	free ( iscsi->target_username );
	free ( iscsi->target_password );
	chap_finish ( &iscsi->chap );
	iscsi_rx_buffered_data_done ( iscsi );
	free ( iscsi->command );
	free ( iscsi );
}

/**
 * Shut down iSCSI interface
 *
 * @v iscsi		iSCSI session
 * @v rc		Reason for close
 */
static void iscsi_close ( struct iscsi_session *iscsi, int rc ) {

	/* A TCP graceful close is still an error from our point of view */
	if ( rc == 0 )
		rc = -ECONNRESET;

	DBGC ( iscsi, "iSCSI %p closed: %s\n", iscsi, strerror ( rc ) );

	/* Stop transmission process */
	process_del ( &iscsi->process );

	/* Shut down interfaces */
	intfs_shutdown ( rc, &iscsi->socket, &iscsi->control, &iscsi->data,
			 NULL );
}

/**
 * Assign new iSCSI initiator task tag
 *
 * @v iscsi		iSCSI session
 */
static void iscsi_new_itt ( struct iscsi_session *iscsi ) {
	static uint16_t itt_idx;

	iscsi->itt = ( ISCSI_TAG_MAGIC | (++itt_idx) );
}

/**
 * Open iSCSI transport-layer connection
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
static int iscsi_open_connection ( struct iscsi_session *iscsi ) {
	struct sockaddr_tcpip target;
	int rc;

	assert ( iscsi->tx_state == ISCSI_TX_IDLE );
	assert ( iscsi->rx_state == ISCSI_RX_BHS );
	assert ( iscsi->rx_offset == 0 );

	/* Open socket */
	memset ( &target, 0, sizeof ( target ) );
	target.st_port = htons ( iscsi->target_port );
	if ( ( rc = xfer_open_named_socket ( &iscsi->socket, SOCK_STREAM,
					     ( struct sockaddr * ) &target,
					     iscsi->target_address,
					     NULL ) ) != 0 ) {
		DBGC ( iscsi, "iSCSI %p could not open socket: %s\n",
		       iscsi, strerror ( rc ) );
		return rc;
	}

	/* Enter security negotiation phase */
	iscsi->status = ( ISCSI_STATUS_SECURITY_NEGOTIATION_PHASE |
			  ISCSI_STATUS_STRINGS_SECURITY );
	if ( iscsi->target_username )
		iscsi->status |= ISCSI_STATUS_AUTH_REVERSE_REQUIRED;

	/* Assign new ISID */
	iscsi->isid_iana_qual = ( random() & 0xffff );

	/* Assign fresh initiator task tag */
	iscsi_new_itt ( iscsi );

	/* Initiate login */
	iscsi_start_login ( iscsi );

	return 0;
}

/**
 * Close iSCSI transport-layer connection
 *
 * @v iscsi		iSCSI session
 * @v rc		Reason for close
 *
 * Closes the transport-layer connection and resets the session state
 * ready to attempt a fresh login.
 */
static void iscsi_close_connection ( struct iscsi_session *iscsi, int rc ) {

	/* Close all data transfer interfaces */
	intf_restart ( &iscsi->socket, rc );

	/* Clear connection status */
	iscsi->status = 0;

	/* Reset TX and RX state machines */
	iscsi->tx_state = ISCSI_TX_IDLE;
	iscsi->rx_state = ISCSI_RX_BHS;
	iscsi->rx_offset = 0;

	/* Free any temporary dynamically allocated memory */
	chap_finish ( &iscsi->chap );
	iscsi_rx_buffered_data_done ( iscsi );
}

/**
 * Mark iSCSI SCSI operation as complete
 *
 * @v iscsi		iSCSI session
 * @v rc		Return status code
 * @v rsp		SCSI response, if any
 *
 * Note that iscsi_scsi_done() will not close the connection, and must
 * therefore be called only when the internal state machines are in an
 * appropriate state, otherwise bad things may happen on the next call
 * to iscsi_scsi_command().  The general rule is to call
 * iscsi_scsi_done() only at the end of receiving a PDU; at this point
 * the TX and RX engines should both be idle.
 */
static void iscsi_scsi_done ( struct iscsi_session *iscsi, int rc,
			      struct scsi_rsp *rsp ) {
	uint32_t itt = iscsi->itt;

	assert ( iscsi->tx_state == ISCSI_TX_IDLE );

	/* Clear command */
	free ( iscsi->command );
	iscsi->command = NULL;

	/* Send SCSI response, if any */
	if ( rsp )
		scsi_response ( &iscsi->data, rsp );

	/* Close SCSI command, if this is still the same command.  (It
	 * is possible that the command interface has already been
	 * closed as a result of the SCSI response we sent.)
	 */
	if ( iscsi->itt == itt )
		intf_restart ( &iscsi->data, rc );
}

/****************************************************************************
 *
 * iSCSI SCSI command issuing
 *
 */

/**
 * Build iSCSI SCSI command BHS
 *
 * @v iscsi		iSCSI session
 *
 * We don't currently support bidirectional commands (i.e. with both
 * Data-In and Data-Out segments); these would require providing code
 * to generate an AHS, and there doesn't seem to be any need for it at
 * the moment.
 */
static void iscsi_start_command ( struct iscsi_session *iscsi ) {
	struct iscsi_bhs_scsi_command *command = &iscsi->tx_bhs.scsi_command;

	assert ( ! ( iscsi->command->data_in && iscsi->command->data_out ) );

	/* Construct BHS and initiate transmission */
	iscsi_start_tx ( iscsi );
	command->opcode = ISCSI_OPCODE_SCSI_COMMAND;
	command->flags = ( ISCSI_FLAG_FINAL |
			   ISCSI_COMMAND_ATTR_SIMPLE );
	if ( iscsi->command->data_in )
		command->flags |= ISCSI_COMMAND_FLAG_READ;
	if ( iscsi->command->data_out )
		command->flags |= ISCSI_COMMAND_FLAG_WRITE;
	/* lengths left as zero */
	memcpy ( &command->lun, &iscsi->command->lun,
		 sizeof ( command->lun ) );
	command->itt = htonl ( iscsi->itt );
	command->exp_len = htonl ( iscsi->command->data_in_len |
				   iscsi->command->data_out_len );
	command->cmdsn = htonl ( iscsi->cmdsn );
	command->expstatsn = htonl ( iscsi->statsn + 1 );
	memcpy ( &command->cdb, &iscsi->command->cdb, sizeof ( command->cdb ));
	DBGC2 ( iscsi, "iSCSI %p start " SCSI_CDB_FORMAT " %s %#zx\n",
		iscsi, SCSI_CDB_DATA ( command->cdb ),
		( iscsi->command->data_in ? "in" : "out" ),
		( iscsi->command->data_in ?
		  iscsi->command->data_in_len :
		  iscsi->command->data_out_len ) );
}

/**
 * Receive data segment of an iSCSI SCSI response PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * @ret rc		Return status code
 */
static int iscsi_rx_scsi_response ( struct iscsi_session *iscsi,
				    const void *data, size_t len,
				    size_t remaining ) {
	struct iscsi_bhs_scsi_response *response
		= &iscsi->rx_bhs.scsi_response;
	struct scsi_rsp rsp;
	uint32_t residual_count;
	size_t data_len;
	int rc;

	/* Buffer up the PDU data */
	if ( ( rc = iscsi_rx_buffered_data ( iscsi, data, len ) ) != 0 ) {
		DBGC ( iscsi, "iSCSI %p could not buffer SCSI response: %s\n",
		       iscsi, strerror ( rc ) );
		return rc;
	}
	if ( remaining )
		return 0;

	/* Parse SCSI response and discard buffer */
	memset ( &rsp, 0, sizeof ( rsp ) );
	rsp.status = response->status;
	residual_count = ntohl ( response->residual_count );
	if ( response->flags & ISCSI_DATA_FLAG_OVERFLOW ) {
		rsp.overrun = residual_count;
	} else if ( response->flags & ISCSI_DATA_FLAG_UNDERFLOW ) {
		rsp.overrun = -(residual_count);
	}
	data_len = ISCSI_DATA_LEN ( response->lengths );
	if ( data_len ) {
		scsi_parse_sense ( ( iscsi->rx_buffer + 2 ), ( data_len - 2 ),
				   &rsp.sense );
	}
	iscsi_rx_buffered_data_done ( iscsi );

	/* Check for errors */
	if ( response->response != ISCSI_RESPONSE_COMMAND_COMPLETE )
		return -EIO;

	/* Mark as completed */
	iscsi_scsi_done ( iscsi, 0, &rsp );
	return 0;
}

/**
 * Receive data segment of an iSCSI data-in PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * @ret rc		Return status code
 */
static int iscsi_rx_data_in ( struct iscsi_session *iscsi,
			      const void *data, size_t len,
			      size_t remaining ) {
	struct iscsi_bhs_data_in *data_in = &iscsi->rx_bhs.data_in;
	unsigned long offset;

	/* Copy data to data-in buffer */
	offset = ntohl ( data_in->offset ) + iscsi->rx_offset;
	assert ( iscsi->command != NULL );
	assert ( iscsi->command->data_in );
	assert ( ( offset + len ) <= iscsi->command->data_in_len );
	copy_to_user ( iscsi->command->data_in, offset, data, len );

	/* Wait for whole SCSI response to arrive */
	if ( remaining )
		return 0;

	/* Mark as completed if status is present */
	if ( data_in->flags & ISCSI_DATA_FLAG_STATUS ) {
		assert ( ( offset + len ) == iscsi->command->data_in_len );
		assert ( data_in->flags & ISCSI_FLAG_FINAL );
		/* iSCSI cannot return an error status via a data-in */
		iscsi_scsi_done ( iscsi, 0, NULL );
	}

	return 0;
}

/**
 * Receive data segment of an iSCSI R2T PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * @ret rc		Return status code
 */
static int iscsi_rx_r2t ( struct iscsi_session *iscsi,
			  const void *data __unused, size_t len __unused,
			  size_t remaining __unused ) {
	struct iscsi_bhs_r2t *r2t = &iscsi->rx_bhs.r2t;

	/* Record transfer parameters and trigger first data-out */
	iscsi->ttt = ntohl ( r2t->ttt );
	iscsi->transfer_offset = ntohl ( r2t->offset );
	iscsi->transfer_len = ntohl ( r2t->len );
	iscsi_start_data_out ( iscsi, 0 );

	return 0;
}

/**
 * Build iSCSI data-out BHS
 *
 * @v iscsi		iSCSI session
 * @v datasn		Data sequence number within the transfer
 *
 */
static void iscsi_start_data_out ( struct iscsi_session *iscsi,
				   unsigned int datasn ) {
	struct iscsi_bhs_data_out *data_out = &iscsi->tx_bhs.data_out;
	unsigned long offset;
	unsigned long remaining;
	unsigned long len;

	/* We always send 512-byte Data-Out PDUs; this removes the
	 * need to worry about the target's MaxRecvDataSegmentLength.
	 */
	offset = datasn * 512;
	remaining = iscsi->transfer_len - offset;
	len = remaining;
	if ( len > 512 )
		len = 512;

	/* Construct BHS and initiate transmission */
	iscsi_start_tx ( iscsi );
	data_out->opcode = ISCSI_OPCODE_DATA_OUT;
	if ( len == remaining )
		data_out->flags = ( ISCSI_FLAG_FINAL );
	ISCSI_SET_LENGTHS ( data_out->lengths, 0, len );
	data_out->lun = iscsi->command->lun;
	data_out->itt = htonl ( iscsi->itt );
	data_out->ttt = htonl ( iscsi->ttt );
	data_out->expstatsn = htonl ( iscsi->statsn + 1 );
	data_out->datasn = htonl ( datasn );
	data_out->offset = htonl ( iscsi->transfer_offset + offset );
	DBGC ( iscsi, "iSCSI %p start data out DataSN %#x len %#lx\n",
	       iscsi, datasn, len );
}

/**
 * Complete iSCSI data-out PDU transmission
 *
 * @v iscsi		iSCSI session
 *
 */
static void iscsi_data_out_done ( struct iscsi_session *iscsi ) {
	struct iscsi_bhs_data_out *data_out = &iscsi->tx_bhs.data_out;

	/* If we haven't reached the end of the sequence, start
	 * sending the next data-out PDU.
	 */
	if ( ! ( data_out->flags & ISCSI_FLAG_FINAL ) )
		iscsi_start_data_out ( iscsi, ntohl ( data_out->datasn ) + 1 );
}

/**
 * Send iSCSI data-out data segment
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
static int iscsi_tx_data_out ( struct iscsi_session *iscsi ) {
	struct iscsi_bhs_data_out *data_out = &iscsi->tx_bhs.data_out;
	struct io_buffer *iobuf;
	unsigned long offset;
	size_t len;
	size_t pad_len;

	offset = ntohl ( data_out->offset );
	len = ISCSI_DATA_LEN ( data_out->lengths );
	pad_len = ISCSI_DATA_PAD_LEN ( data_out->lengths );

	assert ( iscsi->command != NULL );
	assert ( iscsi->command->data_out );
	assert ( ( offset + len ) <= iscsi->command->data_out_len );

	iobuf = xfer_alloc_iob ( &iscsi->socket, ( len + pad_len ) );
	if ( ! iobuf )
		return -ENOMEM;
	
	copy_from_user ( iob_put ( iobuf, len ),
			 iscsi->command->data_out, offset, len );
	memset ( iob_put ( iobuf, pad_len ), 0, pad_len );

	return xfer_deliver_iob ( &iscsi->socket, iobuf );
}

/**
 * Receive data segment of an iSCSI NOP-In
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * @ret rc		Return status code
 */
static int iscsi_rx_nop_in ( struct iscsi_session *iscsi,
			     const void *data __unused, size_t len __unused,
			     size_t remaining __unused ) {
	struct iscsi_nop_in *nop_in = &iscsi->rx_bhs.nop_in;

	DBGC2 ( iscsi, "iSCSI %p received NOP-In\n", iscsi );

	/* We don't currently have the ability to respond to NOP-Ins
	 * sent as ping requests, but we can happily accept NOP-Ins
	 * sent merely to update CmdSN.
	 */
	if ( nop_in->ttt == htonl ( ISCSI_TAG_RESERVED ) )
		return 0;

	/* Ignore any other NOP-Ins.  The target may eventually
	 * disconnect us for failing to respond, but this minimises
	 * unnecessary connection closures.
	 */
	DBGC ( iscsi, "iSCSI %p received unsupported NOP-In with TTT %08x\n",
	       iscsi, ntohl ( nop_in->ttt ) );
	return 0;
}

/****************************************************************************
 *
 * iSCSI login
 *
 */

/**
 * Build iSCSI login request strings
 *
 * @v iscsi		iSCSI session
 *
 * These are the initial set of strings sent in the first login
 * request PDU.  We want the following settings:
 *
 *     HeaderDigest=None
 *     DataDigest=None
 *     MaxConnections=1 (irrelevant; we make only one connection anyway) [4]
 *     InitialR2T=Yes [1]
 *     ImmediateData=No (irrelevant; we never send immediate data) [4]
 *     MaxRecvDataSegmentLength=8192 (default; we don't care) [3]
 *     MaxBurstLength=262144 (default; we don't care) [3]
 *     FirstBurstLength=65536 (irrelevant due to other settings) [5]
 *     DefaultTime2Wait=0 [2]
 *     DefaultTime2Retain=0 [2]
 *     MaxOutstandingR2T=1
 *     DataPDUInOrder=Yes
 *     DataSequenceInOrder=Yes
 *     ErrorRecoveryLevel=0
 *
 * [1] InitialR2T has an OR resolution function, so the target may
 * force us to use it.  We therefore simplify our logic by always
 * using it.
 *
 * [2] These ensure that we can safely start a new task once we have
 * reconnected after a failure, without having to manually tidy up
 * after the old one.
 *
 * [3] We are quite happy to use the RFC-defined default values for
 * these parameters, but some targets (notably OpenSolaris)
 * incorrectly assume a default value of zero, so we explicitly
 * specify the default values.
 *
 * [4] We are quite happy to use the RFC-defined default values for
 * these parameters, but some targets (notably a QNAP TS-639Pro) fail
 * unless they are supplied, so we explicitly specify the default
 * values.
 *
 * [5] FirstBurstLength is defined to be irrelevant since we already
 * force InitialR2T=Yes and ImmediateData=No, but some targets
 * (notably LIO as of kernel 4.11) fail unless it is specified, so we
 * explicitly specify the default value.
 */
static int iscsi_build_login_request_strings ( struct iscsi_session *iscsi,
					       void *data, size_t len ) {
	unsigned int used = 0;
	const char *auth_method;

	if ( iscsi->status & ISCSI_STATUS_STRINGS_SECURITY ) {
		/* Default to allowing no authentication */
		auth_method = "None";
		/* If we have a credential to supply, permit CHAP */
		if ( iscsi->initiator_username )
			auth_method = "CHAP,None";
		/* If we have a credential to check, force CHAP */
		if ( iscsi->target_username )
			auth_method = "CHAP";
		used += ssnprintf ( data + used, len - used,
				    "InitiatorName=%s%c"
				    "TargetName=%s%c"
				    "SessionType=Normal%c"
				    "AuthMethod=%s%c",
				    iscsi->initiator_iqn, 0,
				    iscsi->target_iqn, 0, 0,
				    auth_method, 0 );
	}

	if ( iscsi->status & ISCSI_STATUS_STRINGS_CHAP_ALGORITHM ) {
		used += ssnprintf ( data + used, len - used, "CHAP_A=5%c", 0 );
	}
	
	if ( ( iscsi->status & ISCSI_STATUS_STRINGS_CHAP_RESPONSE ) ) {
		char buf[ base16_encoded_len ( iscsi->chap.response_len ) + 1 ];
		assert ( iscsi->initiator_username != NULL );
		base16_encode ( iscsi->chap.response, iscsi->chap.response_len,
				buf, sizeof ( buf ) );
		used += ssnprintf ( data + used, len - used,
				    "CHAP_N=%s%cCHAP_R=0x%s%c",
				    iscsi->initiator_username, 0, buf, 0 );
	}

	if ( ( iscsi->status & ISCSI_STATUS_STRINGS_CHAP_CHALLENGE ) ) {
		size_t challenge_len = ( sizeof ( iscsi->chap_challenge ) - 1 );
		char buf[ base16_encoded_len ( challenge_len ) + 1 ];
		base16_encode ( ( iscsi->chap_challenge + 1 ), challenge_len,
				buf, sizeof ( buf ) );
		used += ssnprintf ( data + used, len - used,
				    "CHAP_I=%d%cCHAP_C=0x%s%c",
				    iscsi->chap_challenge[0], 0, buf, 0 );
	}

	if ( iscsi->status & ISCSI_STATUS_STRINGS_OPERATIONAL ) {
		used += ssnprintf ( data + used, len - used,
				    "HeaderDigest=None%c"
				    "DataDigest=None%c"
				    "MaxConnections=1%c"
				    "InitialR2T=Yes%c"
				    "ImmediateData=No%c"
				    "MaxRecvDataSegmentLength=8192%c"
				    "MaxBurstLength=262144%c"
				    "FirstBurstLength=65536%c"
				    "DefaultTime2Wait=0%c"
				    "DefaultTime2Retain=0%c"
				    "MaxOutstandingR2T=1%c"
				    "DataPDUInOrder=Yes%c"
				    "DataSequenceInOrder=Yes%c"
				    "ErrorRecoveryLevel=0%c",
				    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	}

	return used;
}

/**
 * Build iSCSI login request BHS
 *
 * @v iscsi		iSCSI session
 */
static void iscsi_start_login ( struct iscsi_session *iscsi ) {
	struct iscsi_bhs_login_request *request = &iscsi->tx_bhs.login_request;
	int len;

	switch ( iscsi->status & ISCSI_LOGIN_CSG_MASK ) {
	case ISCSI_LOGIN_CSG_SECURITY_NEGOTIATION:
		DBGC ( iscsi, "iSCSI %p entering security negotiation\n",
		       iscsi );
		break;
	case ISCSI_LOGIN_CSG_OPERATIONAL_NEGOTIATION:
		DBGC ( iscsi, "iSCSI %p entering operational negotiation\n",
		       iscsi );
		break;
	default:
		assert ( 0 );
	}

	/* Construct BHS and initiate transmission */
	iscsi_start_tx ( iscsi );
	request->opcode = ( ISCSI_OPCODE_LOGIN_REQUEST |
			    ISCSI_FLAG_IMMEDIATE );
	request->flags = ( ( iscsi->status & ISCSI_STATUS_PHASE_MASK ) |
			   ISCSI_LOGIN_FLAG_TRANSITION );
	/* version_max and version_min left as zero */
	len = iscsi_build_login_request_strings ( iscsi, NULL, 0 );
	ISCSI_SET_LENGTHS ( request->lengths, 0, len );
	request->isid_iana_en = htonl ( ISCSI_ISID_IANA |
					IANA_EN_FEN_SYSTEMS );
	request->isid_iana_qual = htons ( iscsi->isid_iana_qual );
	/* tsih left as zero */
	request->itt = htonl ( iscsi->itt );
	/* cid left as zero */
	request->cmdsn = htonl ( iscsi->cmdsn );
	request->expstatsn = htonl ( iscsi->statsn + 1 );
}

/**
 * Complete iSCSI login request PDU transmission
 *
 * @v iscsi		iSCSI session
 *
 */
static void iscsi_login_request_done ( struct iscsi_session *iscsi ) {

	/* Clear any "strings to send" flags */
	iscsi->status &= ~ISCSI_STATUS_STRINGS_MASK;

	/* Free any dynamically allocated storage used for login */
	chap_finish ( &iscsi->chap );
}

/**
 * Transmit data segment of an iSCSI login request PDU
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 *
 * For login requests, the data segment consists of the login strings.
 */
static int iscsi_tx_login_request ( struct iscsi_session *iscsi ) {
	struct iscsi_bhs_login_request *request = &iscsi->tx_bhs.login_request;
	struct io_buffer *iobuf;
	size_t len;
	size_t pad_len;

	len = ISCSI_DATA_LEN ( request->lengths );
	pad_len = ISCSI_DATA_PAD_LEN ( request->lengths );
	iobuf = xfer_alloc_iob ( &iscsi->socket, ( len + pad_len ) );
	if ( ! iobuf )
		return -ENOMEM;
	iob_put ( iobuf, len );
	iscsi_build_login_request_strings ( iscsi, iobuf->data, len );
	memset ( iob_put ( iobuf, pad_len ), 0, pad_len );

	return xfer_deliver_iob ( &iscsi->socket, iobuf );
}

/**
 * Decode large binary value
 *
 * @v encoded		Encoded large binary value
 * @v raw		Raw data
 * @v len		Length of data buffer
 * @ret len		Length of raw data, or negative error
 */
static int iscsi_large_binary_decode ( const char *encoded, uint8_t *raw,
				       size_t len ) {

	/* Check for initial '0x' or '0b' and decode as appropriate */
	if ( *(encoded++) == '0' ) {
		switch ( tolower ( *(encoded++) ) ) {
		case 'x' :
			return base16_decode ( encoded, raw, len );
		case 'b' :
			return base64_decode ( encoded, raw, len );
		}
	}

	return -EPROTO_INVALID_LARGE_BINARY;
}

/**
 * Handle iSCSI TargetAddress text value
 *
 * @v iscsi		iSCSI session
 * @v value		TargetAddress value
 * @ret rc		Return status code
 */
static int iscsi_handle_targetaddress_value ( struct iscsi_session *iscsi,
					      const char *value ) {
	char *separator;

	DBGC ( iscsi, "iSCSI %p will redirect to %s\n", iscsi, value );

	/* Replace target address */
	free ( iscsi->target_address );
	iscsi->target_address = strdup ( value );
	if ( ! iscsi->target_address )
		return -ENOMEM;

	/* Replace target port */
	iscsi->target_port = htons ( ISCSI_PORT );
	separator = strchr ( iscsi->target_address, ':' );
	if ( separator ) {
		*separator = '\0';
		iscsi->target_port = strtoul ( ( separator + 1 ), NULL, 0 );
	}

	return 0;
}

/**
 * Handle iSCSI AuthMethod text value
 *
 * @v iscsi		iSCSI session
 * @v value		AuthMethod value
 * @ret rc		Return status code
 */
static int iscsi_handle_authmethod_value ( struct iscsi_session *iscsi,
					   const char *value ) {

	/* If server requests CHAP, send the CHAP_A string */
	if ( strcmp ( value, "CHAP" ) == 0 ) {
		DBGC ( iscsi, "iSCSI %p initiating CHAP authentication\n",
		       iscsi );
		iscsi->status |= ( ISCSI_STATUS_STRINGS_CHAP_ALGORITHM |
				   ISCSI_STATUS_AUTH_FORWARD_REQUIRED );
	}

	return 0;
}

/**
 * Handle iSCSI CHAP_A text value
 *
 * @v iscsi		iSCSI session
 * @v value		CHAP_A value
 * @ret rc		Return status code
 */
static int iscsi_handle_chap_a_value ( struct iscsi_session *iscsi,
				       const char *value ) {

	/* We only ever offer "5" (i.e. MD5) as an algorithm, so if
	 * the server responds with anything else it is a protocol
	 * violation.
	 */
	if ( strcmp ( value, "5" ) != 0 ) {
		DBGC ( iscsi, "iSCSI %p got invalid CHAP algorithm \"%s\"\n",
		       iscsi, value );
		return -EPROTO_INVALID_CHAP_ALGORITHM;
	}

	return 0;
}

/**
 * Handle iSCSI CHAP_I text value
 *
 * @v iscsi		iSCSI session
 * @v value		CHAP_I value
 * @ret rc		Return status code
 */
static int iscsi_handle_chap_i_value ( struct iscsi_session *iscsi,
				       const char *value ) {
	unsigned int identifier;
	char *endp;
	int rc;

	/* The CHAP identifier is an integer value */
	identifier = strtoul ( value, &endp, 0 );
	if ( *endp != '\0' ) {
		DBGC ( iscsi, "iSCSI %p saw invalid CHAP identifier \"%s\"\n",
		       iscsi, value );
		return -EPROTO_INVALID_CHAP_IDENTIFIER;
	}

	/* Prepare for CHAP with MD5 */
	chap_finish ( &iscsi->chap );
	if ( ( rc = chap_init ( &iscsi->chap, &md5_algorithm ) ) != 0 ) {
		DBGC ( iscsi, "iSCSI %p could not initialise CHAP: %s\n",
		       iscsi, strerror ( rc ) );
		return rc;
	}

	/* Identifier and secret are the first two components of the
	 * challenge.
	 */
	chap_set_identifier ( &iscsi->chap, identifier );
	if ( iscsi->initiator_password ) {
		chap_update ( &iscsi->chap, iscsi->initiator_password,
			      strlen ( iscsi->initiator_password ) );
	}

	return 0;
}

/**
 * Handle iSCSI CHAP_C text value
 *
 * @v iscsi		iSCSI session
 * @v value		CHAP_C value
 * @ret rc		Return status code
 */
static int iscsi_handle_chap_c_value ( struct iscsi_session *iscsi,
				       const char *value ) {
	uint8_t buf[ strlen ( value ) ]; /* Decoding never expands data */
	unsigned int i;
	int len;
	int rc;

	/* Process challenge */
	len = iscsi_large_binary_decode ( value, buf, sizeof ( buf ) );
	if ( len < 0 ) {
		rc = len;
		DBGC ( iscsi, "iSCSI %p invalid CHAP challenge \"%s\": %s\n",
		       iscsi, value, strerror ( rc ) );
		return rc;
	}
	chap_update ( &iscsi->chap, buf, len );

	/* Build CHAP response */
	DBGC ( iscsi, "iSCSI %p sending CHAP response\n", iscsi );
	chap_respond ( &iscsi->chap );
	iscsi->status |= ISCSI_STATUS_STRINGS_CHAP_RESPONSE;

	/* Send CHAP challenge, if applicable */
	if ( iscsi->target_username ) {
		iscsi->status |= ISCSI_STATUS_STRINGS_CHAP_CHALLENGE;
		/* Generate CHAP challenge data */
		for ( i = 0 ; i < sizeof ( iscsi->chap_challenge ) ; i++ ) {
			iscsi->chap_challenge[i] = random();
		}
	}

	return 0;
}

/**
 * Handle iSCSI CHAP_N text value
 *
 * @v iscsi		iSCSI session
 * @v value		CHAP_N value
 * @ret rc		Return status code
 */
static int iscsi_handle_chap_n_value ( struct iscsi_session *iscsi,
				       const char *value ) {

	/* The target username isn't actually involved at any point in
	 * the authentication process; it merely serves to identify
	 * which password the target is using to generate the CHAP
	 * response.  We unnecessarily verify that the username is as
	 * expected, in order to provide mildly helpful diagnostics if
	 * the target is supplying the wrong username/password
	 * combination.
	 */
	if ( iscsi->target_username &&
	     ( strcmp ( iscsi->target_username, value ) != 0 ) ) {
		DBGC ( iscsi, "iSCSI %p target username \"%s\" incorrect "
		       "(wanted \"%s\")\n",
		       iscsi, value, iscsi->target_username );
		return -EACCES_INCORRECT_TARGET_USERNAME;
	}

	return 0;
}

/**
 * Handle iSCSI CHAP_R text value
 *
 * @v iscsi		iSCSI session
 * @v value		CHAP_R value
 * @ret rc		Return status code
 */
static int iscsi_handle_chap_r_value ( struct iscsi_session *iscsi,
				       const char *value ) {
	uint8_t buf[ strlen ( value ) ]; /* Decoding never expands data */
	int len;
	int rc;

	/* Generate CHAP response for verification */
	chap_finish ( &iscsi->chap );
	if ( ( rc = chap_init ( &iscsi->chap, &md5_algorithm ) ) != 0 ) {
		DBGC ( iscsi, "iSCSI %p could not initialise CHAP: %s\n",
		       iscsi, strerror ( rc ) );
		return rc;
	}
	chap_set_identifier ( &iscsi->chap, iscsi->chap_challenge[0] );
	if ( iscsi->target_password ) {
		chap_update ( &iscsi->chap, iscsi->target_password,
			      strlen ( iscsi->target_password ) );
	}
	chap_update ( &iscsi->chap, &iscsi->chap_challenge[1],
		      ( sizeof ( iscsi->chap_challenge ) - 1 ) );
	chap_respond ( &iscsi->chap );

	/* Process response */
	len = iscsi_large_binary_decode ( value, buf, sizeof ( buf ) );
	if ( len < 0 ) {
		rc = len;
		DBGC ( iscsi, "iSCSI %p invalid CHAP response \"%s\": %s\n",
		       iscsi, value, strerror ( rc ) );
		return rc;
	}

	/* Check CHAP response */
	if ( len != ( int ) iscsi->chap.response_len ) {
		DBGC ( iscsi, "iSCSI %p invalid CHAP response length\n",
		       iscsi );
		return -EPROTO_INVALID_CHAP_RESPONSE;
	}
	if ( memcmp ( buf, iscsi->chap.response, len ) != 0 ) {
		DBGC ( iscsi, "iSCSI %p incorrect CHAP response \"%s\"\n",
		       iscsi, value );
		return -EACCES_INCORRECT_TARGET_PASSWORD;
	}

	/* Mark session as authenticated */
	iscsi->status |= ISCSI_STATUS_AUTH_REVERSE_OK;

	return 0;
}

/** An iSCSI text string that we want to handle */
struct iscsi_string_type {
	/** String key
	 *
	 * This is the portion preceding the "=" sign,
	 * e.g. "InitiatorName", "CHAP_A", etc.
	 */
	const char *key;
	/** Handle iSCSI string value
	 *
	 * @v iscsi		iSCSI session
	 * @v value		iSCSI string value
	 * @ret rc		Return status code
	 */
	int ( * handle ) ( struct iscsi_session *iscsi, const char *value );
};

/** iSCSI text strings that we want to handle */
static struct iscsi_string_type iscsi_string_types[] = {
	{ "TargetAddress", iscsi_handle_targetaddress_value },
	{ "AuthMethod", iscsi_handle_authmethod_value },
	{ "CHAP_A", iscsi_handle_chap_a_value },
	{ "CHAP_I", iscsi_handle_chap_i_value },
	{ "CHAP_C", iscsi_handle_chap_c_value },
	{ "CHAP_N", iscsi_handle_chap_n_value },
	{ "CHAP_R", iscsi_handle_chap_r_value },
	{ NULL, NULL }
};

/**
 * Handle iSCSI string
 *
 * @v iscsi		iSCSI session
 * @v string		iSCSI string (in "key=value" format)
 * @ret rc		Return status code
 */
static int iscsi_handle_string ( struct iscsi_session *iscsi,
				 const char *string ) {
	struct iscsi_string_type *type;
	const char *separator;
	const char *value;
	size_t key_len;
	int rc;

	/* Find separator */
	separator = strchr ( string, '=' );
	if ( ! separator ) {
		DBGC ( iscsi, "iSCSI %p malformed string %s\n",
		       iscsi, string );
		return -EPROTO_INVALID_KEY_VALUE_PAIR;
	}
	key_len = ( separator - string );
	value = ( separator + 1 );

	/* Check for rejections.  Since we send only non-rejectable
	 * values, any rejection is a fatal protocol error.
	 */
	if ( strcmp ( value, "Reject" ) == 0 ) {
		DBGC ( iscsi, "iSCSI %p rejection: %s\n", iscsi, string );
		return -EPROTO_VALUE_REJECTED;
	}

	/* Handle key/value pair */
	for ( type = iscsi_string_types ; type->key ; type++ ) {
		if ( strncmp ( string, type->key, key_len ) != 0 )
			continue;
		DBGC ( iscsi, "iSCSI %p handling %s\n", iscsi, string );
		if ( ( rc = type->handle ( iscsi, value ) ) != 0 ) {
			DBGC ( iscsi, "iSCSI %p could not handle %s: %s\n",
			       iscsi, string, strerror ( rc ) );
			return rc;
		}
		return 0;
	}
	DBGC ( iscsi, "iSCSI %p ignoring %s\n", iscsi, string );
	return 0;
}

/**
 * Handle iSCSI strings
 *
 * @v iscsi		iSCSI session
 * @v string		iSCSI string buffer
 * @v len		Length of string buffer
 * @ret rc		Return status code
 */
static int iscsi_handle_strings ( struct iscsi_session *iscsi,
				  const char *strings, size_t len ) {
	size_t string_len;
	int rc;

	/* Handle each string in turn, taking care not to overrun the
	 * data buffer in case of badly-terminated data.
	 */
	while ( 1 ) {
		string_len = ( strnlen ( strings, len ) + 1 );
		if ( string_len > len )
			break;
		if ( ( rc = iscsi_handle_string ( iscsi, strings ) ) != 0 )
			return rc;
		strings += string_len;
		len -= string_len;
	}
	return 0;
}

/**
 * Convert iSCSI response status to return status code
 *
 * @v status_class	iSCSI status class
 * @v status_detail	iSCSI status detail
 * @ret rc		Return status code
 */
static int iscsi_status_to_rc ( unsigned int status_class,
				unsigned int status_detail ) {
	switch ( status_class ) {
	case ISCSI_STATUS_INITIATOR_ERROR :
		switch ( status_detail ) {
		case ISCSI_STATUS_INITIATOR_ERROR_AUTHENTICATION :
			return -EPERM_INITIATOR_AUTHENTICATION;
		case ISCSI_STATUS_INITIATOR_ERROR_AUTHORISATION :
			return -EPERM_INITIATOR_AUTHORISATION;
		case ISCSI_STATUS_INITIATOR_ERROR_NOT_FOUND :
		case ISCSI_STATUS_INITIATOR_ERROR_REMOVED :
			return -ENODEV;
		default :
			return -ENOTSUP_INITIATOR_STATUS;
		}
	case ISCSI_STATUS_TARGET_ERROR :
		switch ( status_detail ) {
		case ISCSI_STATUS_TARGET_ERROR_UNAVAILABLE:
			return -EIO_TARGET_UNAVAILABLE;
		case ISCSI_STATUS_TARGET_ERROR_NO_RESOURCES:
			return -EIO_TARGET_NO_RESOURCES;
		default:
			return -ENOTSUP_TARGET_STATUS;
		}
	default :
		return -EINVAL;
	}
}

/**
 * Receive data segment of an iSCSI login response PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * @ret rc		Return status code
 */
static int iscsi_rx_login_response ( struct iscsi_session *iscsi,
				     const void *data, size_t len,
				     size_t remaining ) {
	struct iscsi_bhs_login_response *response
		= &iscsi->rx_bhs.login_response;
	int rc;

	/* Buffer up the PDU data */
	if ( ( rc = iscsi_rx_buffered_data ( iscsi, data, len ) ) != 0 ) {
		DBGC ( iscsi, "iSCSI %p could not buffer login response: %s\n",
		       iscsi, strerror ( rc ) );
		return rc;
	}
	if ( remaining )
		return 0;

	/* Process string data and discard string buffer */
	if ( ( rc = iscsi_handle_strings ( iscsi, iscsi->rx_buffer,
					   iscsi->rx_len ) ) != 0 )
		return rc;
	iscsi_rx_buffered_data_done ( iscsi );

	/* Check for login redirection */
	if ( response->status_class == ISCSI_STATUS_REDIRECT ) {
		DBGC ( iscsi, "iSCSI %p redirecting to new server\n", iscsi );
		iscsi_close_connection ( iscsi, 0 );
		if ( ( rc = iscsi_open_connection ( iscsi ) ) != 0 ) {
			DBGC ( iscsi, "iSCSI %p could not redirect: %s\n ",
			       iscsi, strerror ( rc ) );
			return rc;
		}
		return 0;
	}

	/* Check for fatal errors */
	if ( response->status_class != 0 ) {
		DBGC ( iscsi, "iSCSI login failure: class %02x detail %02x\n",
		       response->status_class, response->status_detail );
		rc = iscsi_status_to_rc ( response->status_class,
					  response->status_detail );
		return rc;
	}

	/* Handle login transitions */
	if ( response->flags & ISCSI_LOGIN_FLAG_TRANSITION ) {
		iscsi->status &= ~( ISCSI_STATUS_PHASE_MASK |
				    ISCSI_STATUS_STRINGS_MASK );
		switch ( response->flags & ISCSI_LOGIN_NSG_MASK ) {
		case ISCSI_LOGIN_NSG_OPERATIONAL_NEGOTIATION:
			iscsi->status |=
				( ISCSI_STATUS_OPERATIONAL_NEGOTIATION_PHASE |
				  ISCSI_STATUS_STRINGS_OPERATIONAL );
			break;
		case ISCSI_LOGIN_NSG_FULL_FEATURE_PHASE:
			iscsi->status |= ISCSI_STATUS_FULL_FEATURE_PHASE;
			break;
		default:
			DBGC ( iscsi, "iSCSI %p got invalid response flags "
			       "%02x\n", iscsi, response->flags );
			return -EIO;
		}
	}

	/* Send next login request PDU if we haven't reached the full
	 * feature phase yet.
	 */
	if ( ( iscsi->status & ISCSI_STATUS_PHASE_MASK ) !=
	     ISCSI_STATUS_FULL_FEATURE_PHASE ) {
		iscsi_start_login ( iscsi );
		return 0;
	}

	/* Check that target authentication was successful (if required) */
	if ( ( iscsi->status & ISCSI_STATUS_AUTH_REVERSE_REQUIRED ) &&
	     ! ( iscsi->status & ISCSI_STATUS_AUTH_REVERSE_OK ) ) {
		DBGC ( iscsi, "iSCSI %p nefarious target tried to bypass "
		       "authentication\n", iscsi );
		return -EPROTO;
	}

	/* Notify SCSI layer of window change */
	DBGC ( iscsi, "iSCSI %p entering full feature phase\n", iscsi );
	xfer_window_changed ( &iscsi->control );

	return 0;
}

/****************************************************************************
 *
 * iSCSI to socket interface
 *
 */

/**
 * Pause TX engine
 *
 * @v iscsi		iSCSI session
 */
static void iscsi_tx_pause ( struct iscsi_session *iscsi ) {
	process_del ( &iscsi->process );
}

/**
 * Resume TX engine
 *
 * @v iscsi		iSCSI session
 */
static void iscsi_tx_resume ( struct iscsi_session *iscsi ) {
	process_add ( &iscsi->process );
}

/**
 * Start up a new TX PDU
 *
 * @v iscsi		iSCSI session
 *
 * This initiates the process of sending a new PDU.  Only one PDU may
 * be in transit at any one time.
 */
static void iscsi_start_tx ( struct iscsi_session *iscsi ) {

	assert ( iscsi->tx_state == ISCSI_TX_IDLE );

	/* Initialise TX BHS */
	memset ( &iscsi->tx_bhs, 0, sizeof ( iscsi->tx_bhs ) );

	/* Flag TX engine to start transmitting */
	iscsi->tx_state = ISCSI_TX_BHS;

	/* Start transmission process */
	iscsi_tx_resume ( iscsi );
}

/**
 * Transmit nothing
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
static int iscsi_tx_nothing ( struct iscsi_session *iscsi __unused ) {
	return 0;
}

/**
 * Transmit basic header segment of an iSCSI PDU
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
static int iscsi_tx_bhs ( struct iscsi_session *iscsi ) {
	return xfer_deliver_raw ( &iscsi->socket,  &iscsi->tx_bhs,
				  sizeof ( iscsi->tx_bhs ) );
}

/**
 * Transmit data segment of an iSCSI PDU
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 * 
 * Handle transmission of part of a PDU data segment.  iscsi::tx_bhs
 * will be valid when this is called.
 */
static int iscsi_tx_data ( struct iscsi_session *iscsi ) {
	struct iscsi_bhs_common *common = &iscsi->tx_bhs.common;

	switch ( common->opcode & ISCSI_OPCODE_MASK ) {
	case ISCSI_OPCODE_DATA_OUT:
		return iscsi_tx_data_out ( iscsi );
	case ISCSI_OPCODE_LOGIN_REQUEST:
		return iscsi_tx_login_request ( iscsi );
	default:
		/* Nothing to send in other states */
		return 0;
	}
}

/**
 * Complete iSCSI PDU transmission
 *
 * @v iscsi		iSCSI session
 *
 * Called when a PDU has been completely transmitted and the TX state
 * machine is about to enter the idle state.  iscsi::tx_bhs will be
 * valid for the just-completed PDU when this is called.
 */
static void iscsi_tx_done ( struct iscsi_session *iscsi ) {
	struct iscsi_bhs_common *common = &iscsi->tx_bhs.common;

	/* Stop transmission process */
	iscsi_tx_pause ( iscsi );

	switch ( common->opcode & ISCSI_OPCODE_MASK ) {
	case ISCSI_OPCODE_DATA_OUT:
		iscsi_data_out_done ( iscsi );
		break;
	case ISCSI_OPCODE_LOGIN_REQUEST:
		iscsi_login_request_done ( iscsi );
		break;
	default:
		/* No action */
		break;
	}
}

/**
 * Transmit iSCSI PDU
 *
 * @v iscsi		iSCSI session
 * @v buf		Temporary data buffer
 * @v len		Length of temporary data buffer
 * 
 * Constructs data to be sent for the current TX state
 */
static void iscsi_tx_step ( struct iscsi_session *iscsi ) {
	struct iscsi_bhs_common *common = &iscsi->tx_bhs.common;
	int ( * tx ) ( struct iscsi_session *iscsi );
	enum iscsi_tx_state next_state;
	size_t tx_len;
	int rc;

	/* Select fragment to transmit */
	while ( 1 ) {
		switch ( iscsi->tx_state ) {
		case ISCSI_TX_BHS:
			tx = iscsi_tx_bhs;
			tx_len = sizeof ( iscsi->tx_bhs );
			next_state = ISCSI_TX_AHS;
			break;
		case ISCSI_TX_AHS:
			tx = iscsi_tx_nothing;
			tx_len = 0;
			next_state = ISCSI_TX_DATA;
			break;
		case ISCSI_TX_DATA:
			tx = iscsi_tx_data;
			tx_len = ISCSI_DATA_LEN ( common->lengths );
			next_state = ISCSI_TX_IDLE;
			break;
		case ISCSI_TX_IDLE:
			/* Nothing to do; pause processing */
			iscsi_tx_pause ( iscsi );
			return;
		default:
			assert ( 0 );
			return;
		}

		/* Check for window availability, if needed */
		if ( tx_len && ( xfer_window ( &iscsi->socket ) == 0 ) ) {
			/* Cannot transmit at this point; pause
			 * processing and wait for window to reopen
			 */
			iscsi_tx_pause ( iscsi );
			return;
		}

		/* Transmit data */
		if ( ( rc = tx ( iscsi ) ) != 0 ) {
			DBGC ( iscsi, "iSCSI %p could not transmit: %s\n",
			       iscsi, strerror ( rc ) );
			/* Transmission errors are fatal */
			iscsi_close ( iscsi, rc );
			return;
		}

		/* Move to next state */
		iscsi->tx_state = next_state;

		/* If we have moved to the idle state, mark
		 * transmission as complete
		 */
		if ( iscsi->tx_state == ISCSI_TX_IDLE )
			iscsi_tx_done ( iscsi );
	}
}

/** iSCSI TX process descriptor */
static struct process_descriptor iscsi_process_desc =
	PROC_DESC ( struct iscsi_session, process, iscsi_tx_step );

/**
 * Receive basic header segment of an iSCSI PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * @ret rc		Return status code
 *
 * This fills in iscsi::rx_bhs with the data from the BHS portion of
 * the received PDU.
 */
static int iscsi_rx_bhs ( struct iscsi_session *iscsi, const void *data,
			  size_t len, size_t remaining __unused ) {
	memcpy ( &iscsi->rx_bhs.bytes[iscsi->rx_offset], data, len );
	if ( ( iscsi->rx_offset + len ) >= sizeof ( iscsi->rx_bhs ) ) {
		DBGC2 ( iscsi, "iSCSI %p received PDU opcode %#x len %#x\n",
			iscsi, iscsi->rx_bhs.common.opcode,
			ISCSI_DATA_LEN ( iscsi->rx_bhs.common.lengths ) );
	}
	return 0;
}

/**
 * Discard portion of an iSCSI PDU.
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * @ret rc		Return status code
 *
 * This discards data from a portion of a received PDU.
 */
static int iscsi_rx_discard ( struct iscsi_session *iscsi __unused,
			      const void *data __unused, size_t len __unused,
			      size_t remaining __unused ) {
	/* Do nothing */
	return 0;
}

/**
 * Receive data segment of an iSCSI PDU
 *
 * @v iscsi		iSCSI session
 * @v data		Received data
 * @v len		Length of received data
 * @v remaining		Data remaining after this data
 * @ret rc		Return status code
 *
 * Handle processing of part of a PDU data segment.  iscsi::rx_bhs
 * will be valid when this is called.
 */
static int iscsi_rx_data ( struct iscsi_session *iscsi, const void *data,
			   size_t len, size_t remaining ) {
	struct iscsi_bhs_common_response *response
		= &iscsi->rx_bhs.common_response;

	/* Update cmdsn and statsn */
	iscsi->cmdsn = ntohl ( response->expcmdsn );
	iscsi->statsn = ntohl ( response->statsn );

	switch ( response->opcode & ISCSI_OPCODE_MASK ) {
	case ISCSI_OPCODE_LOGIN_RESPONSE:
		return iscsi_rx_login_response ( iscsi, data, len, remaining );
	case ISCSI_OPCODE_SCSI_RESPONSE:
		return iscsi_rx_scsi_response ( iscsi, data, len, remaining );
	case ISCSI_OPCODE_DATA_IN:
		return iscsi_rx_data_in ( iscsi, data, len, remaining );
	case ISCSI_OPCODE_R2T:
		return iscsi_rx_r2t ( iscsi, data, len, remaining );
	case ISCSI_OPCODE_NOP_IN:
		return iscsi_rx_nop_in ( iscsi, data, len, remaining );
	default:
		if ( remaining )
			return 0;
		DBGC ( iscsi, "iSCSI %p unknown opcode %02x\n", iscsi,
		       response->opcode );
		return -ENOTSUP_OPCODE;
	}
}

/**
 * Receive new data
 *
 * @v iscsi		iSCSI session
 * @v iobuf		I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 *
 * This handles received PDUs.  The receive strategy is to fill in
 * iscsi::rx_bhs with the contents of the BHS portion of the PDU,
 * throw away any AHS portion, and then process each part of the data
 * portion as it arrives.  The data processing routine therefore
 * always has a full copy of the BHS available, even for portions of
 * the data in different packets to the BHS.
 */
static int iscsi_socket_deliver ( struct iscsi_session *iscsi,
				  struct io_buffer *iobuf,
				  struct xfer_metadata *meta __unused ) {
	struct iscsi_bhs_common *common = &iscsi->rx_bhs.common;
	int ( * rx ) ( struct iscsi_session *iscsi, const void *data,
		       size_t len, size_t remaining );
	enum iscsi_rx_state next_state;
	size_t frag_len;
	size_t remaining;
	int rc;

	while ( 1 ) {
		switch ( iscsi->rx_state ) {
		case ISCSI_RX_BHS:
			rx = iscsi_rx_bhs;
			iscsi->rx_len = sizeof ( iscsi->rx_bhs );
			next_state = ISCSI_RX_AHS;			
			break;
		case ISCSI_RX_AHS:
			rx = iscsi_rx_discard;
			iscsi->rx_len = 4 * ISCSI_AHS_LEN ( common->lengths );
			next_state = ISCSI_RX_DATA;
			break;
		case ISCSI_RX_DATA:
			rx = iscsi_rx_data;
			iscsi->rx_len = ISCSI_DATA_LEN ( common->lengths );
			next_state = ISCSI_RX_DATA_PADDING;
			break;
		case ISCSI_RX_DATA_PADDING:
			rx = iscsi_rx_discard;
			iscsi->rx_len = ISCSI_DATA_PAD_LEN ( common->lengths );
			next_state = ISCSI_RX_BHS;
			break;
		default:
			assert ( 0 );
			rc = -EINVAL;
			goto done;
		}

		frag_len = iscsi->rx_len - iscsi->rx_offset;
		if ( frag_len > iob_len ( iobuf ) )
			frag_len = iob_len ( iobuf );
		remaining = iscsi->rx_len - iscsi->rx_offset - frag_len;
		if ( ( rc = rx ( iscsi, iobuf->data, frag_len,
				 remaining ) ) != 0 ) {
			DBGC ( iscsi, "iSCSI %p could not process received "
			       "data: %s\n", iscsi, strerror ( rc ) );
			goto done;
		}

		iscsi->rx_offset += frag_len;
		iob_pull ( iobuf, frag_len );

		/* If all the data for this state has not yet been
		 * received, stay in this state for now.
		 */
		if ( iscsi->rx_offset != iscsi->rx_len ) {
			rc = 0;
			goto done;
		}

		iscsi->rx_state = next_state;
		iscsi->rx_offset = 0;
	}

 done:
	/* Free I/O buffer */
	free_iob ( iobuf );

	/* Destroy session on error */
	if ( rc != 0 )
		iscsi_close ( iscsi, rc );

	return rc;
}

/**
 * Handle redirection event
 *
 * @v iscsi		iSCSI session
 * @v type		Location type
 * @v args		Remaining arguments depend upon location type
 * @ret rc		Return status code
 */
static int iscsi_vredirect ( struct iscsi_session *iscsi, int type,
			     va_list args ) {
	va_list tmp;
	struct sockaddr *peer;
	int rc;

	/* Intercept redirects to a LOCATION_SOCKET and record the IP
	 * address for the iBFT.  This is a bit of a hack, but avoids
	 * inventing an ioctl()-style call to retrieve the socket
	 * address from a data-xfer interface.
	 */
	if ( type == LOCATION_SOCKET ) {
		va_copy ( tmp, args );
		( void ) va_arg ( tmp, int ); /* Discard "semantics" */
		peer = va_arg ( tmp, struct sockaddr * );
		memcpy ( &iscsi->target_sockaddr, peer,
			 sizeof ( iscsi->target_sockaddr ) );
		va_end ( tmp );
	}

	/* Redirect to new location */
	if ( ( rc = xfer_vreopen ( &iscsi->socket, type, args ) ) != 0 )
		goto err;

	return 0;

 err:
	iscsi_close ( iscsi, rc );
	return rc;
}

/** iSCSI socket interface operations */
static struct interface_operation iscsi_socket_operations[] = {
	INTF_OP ( xfer_deliver, struct iscsi_session *, iscsi_socket_deliver ),
	INTF_OP ( xfer_window_changed, struct iscsi_session *,
		  iscsi_tx_resume ),
	INTF_OP ( xfer_vredirect, struct iscsi_session *, iscsi_vredirect ),
	INTF_OP ( intf_close, struct iscsi_session *, iscsi_close ),
};

/** iSCSI socket interface descriptor */
static struct interface_descriptor iscsi_socket_desc =
	INTF_DESC ( struct iscsi_session, socket, iscsi_socket_operations );

/****************************************************************************
 *
 * iSCSI command issuing
 *
 */

/**
 * Check iSCSI flow-control window
 *
 * @v iscsi		iSCSI session
 * @ret len		Length of window
 */
static size_t iscsi_scsi_window ( struct iscsi_session *iscsi ) {

	if ( ( ( iscsi->status & ISCSI_STATUS_PHASE_MASK ) ==
	       ISCSI_STATUS_FULL_FEATURE_PHASE ) &&
	     ( iscsi->command == NULL ) ) {
		/* We cannot handle concurrent commands */
		return 1;
	} else {
		return 0;
	}
}

/**
 * Issue iSCSI SCSI command
 *
 * @v iscsi		iSCSI session
 * @v parent		Parent interface
 * @v command		SCSI command
 * @ret tag		Command tag, or negative error
 */
static int iscsi_scsi_command ( struct iscsi_session *iscsi,
				struct interface *parent,
				struct scsi_cmd *command ) {

	/* This iSCSI implementation cannot handle multiple concurrent
	 * commands or commands arriving before login is complete.
	 */
	if ( iscsi_scsi_window ( iscsi ) == 0 ) {
		DBGC ( iscsi, "iSCSI %p cannot handle concurrent commands\n",
		       iscsi );
		return -EOPNOTSUPP;
	}

	/* Store command */
	iscsi->command = malloc ( sizeof ( *command ) );
	if ( ! iscsi->command )
		return -ENOMEM;
	memcpy ( iscsi->command, command, sizeof ( *command ) );

	/* Assign new ITT */
	iscsi_new_itt ( iscsi );

	/* Start sending command */
	iscsi_start_command ( iscsi );

	/* Attach to parent interface and return */
	intf_plug_plug ( &iscsi->data, parent );
	return iscsi->itt;
}

/**
 * Get iSCSI ACPI descriptor
 *
 * @v iscsi		iSCSI session
 * @ret desc		ACPI descriptor
 */
static struct acpi_descriptor * iscsi_describe ( struct iscsi_session *iscsi ) {

	return &iscsi->desc;
}

/** iSCSI SCSI command-issuing interface operations */
static struct interface_operation iscsi_control_op[] = {
	INTF_OP ( scsi_command, struct iscsi_session *, iscsi_scsi_command ),
	INTF_OP ( xfer_window, struct iscsi_session *, iscsi_scsi_window ),
	INTF_OP ( intf_close, struct iscsi_session *, iscsi_close ),
	INTF_OP ( acpi_describe, struct iscsi_session *, iscsi_describe ),
};

/** iSCSI SCSI command-issuing interface descriptor */
static struct interface_descriptor iscsi_control_desc =
	INTF_DESC ( struct iscsi_session, control, iscsi_control_op );

/**
 * Close iSCSI command
 *
 * @v iscsi		iSCSI session
 * @v rc		Reason for close
 */
static void iscsi_command_close ( struct iscsi_session *iscsi, int rc ) {

	/* Restart interface */
	intf_restart ( &iscsi->data, rc );

	/* Treat unsolicited command closures mid-command as fatal,
	 * because we have no code to handle partially-completed PDUs.
	 */
	if ( iscsi->command != NULL )
		iscsi_close ( iscsi, ( ( rc == 0 ) ? -ECANCELED : rc ) );
}

/** iSCSI SCSI command interface operations */
static struct interface_operation iscsi_data_op[] = {
	INTF_OP ( intf_close, struct iscsi_session *, iscsi_command_close ),
};

/** iSCSI SCSI command interface descriptor */
static struct interface_descriptor iscsi_data_desc =
	INTF_DESC ( struct iscsi_session, data, iscsi_data_op );

/****************************************************************************
 *
 * Instantiator
 *
 */

/** iSCSI root path components (as per RFC4173) */
enum iscsi_root_path_component {
	RP_SERVERNAME = 0,
	RP_PROTOCOL,
	RP_PORT,
	RP_LUN,
	RP_TARGETNAME,
	NUM_RP_COMPONENTS
};

/** iSCSI initiator IQN setting */
const struct setting initiator_iqn_setting __setting ( SETTING_SANBOOT_EXTRA,
						       initiator-iqn ) = {
	.name = "initiator-iqn",
	.description = "iSCSI initiator name",
	.tag = DHCP_ISCSI_INITIATOR_IQN,
	.type = &setting_type_string,
};

/** iSCSI reverse username setting */
const struct setting reverse_username_setting __setting ( SETTING_AUTH_EXTRA,
							  reverse-username ) = {
	.name = "reverse-username",
	.description = "Reverse user name",
	.tag = DHCP_EB_REVERSE_USERNAME,
	.type = &setting_type_string,
};

/** iSCSI reverse password setting */
const struct setting reverse_password_setting __setting ( SETTING_AUTH_EXTRA,
							  reverse-password ) = {
	.name = "reverse-password",
	.description = "Reverse password",
	.tag = DHCP_EB_REVERSE_PASSWORD,
	.type = &setting_type_string,
};

/**
 * Parse iSCSI root path
 *
 * @v iscsi		iSCSI session
 * @v root_path		iSCSI root path (as per RFC4173)
 * @ret rc		Return status code
 */
static int iscsi_parse_root_path ( struct iscsi_session *iscsi,
				   const char *root_path ) {
	char rp_copy[ strlen ( root_path ) + 1 ];
	char *rp_comp[NUM_RP_COMPONENTS];
	char *rp = rp_copy;
	int skip = 0;
	int i = 0;
	int rc;

	/* Split root path into component parts */
	strcpy ( rp_copy, root_path );
	while ( 1 ) {
		rp_comp[i++] = rp;
		if ( i == NUM_RP_COMPONENTS )
			break;
		for ( ; ( ( *rp != ':' ) || skip ) ; rp++ ) {
			if ( ! *rp ) {
				DBGC ( iscsi, "iSCSI %p root path \"%s\" "
				       "too short\n", iscsi, root_path );
				return -EINVAL_ROOT_PATH_TOO_SHORT;
			} else if ( *rp == '[' ) {
				skip = 1;
			} else if ( *rp == ']' ) {
				skip = 0;
			}
		}
		*(rp++) = '\0';
	}

	/* Use root path components to configure iSCSI session */
	iscsi->target_address = strdup ( rp_comp[RP_SERVERNAME] );
	if ( ! iscsi->target_address )
		return -ENOMEM;
	iscsi->target_port = strtoul ( rp_comp[RP_PORT], NULL, 10 );
	if ( ! iscsi->target_port )
		iscsi->target_port = ISCSI_PORT;
	if ( ( rc = scsi_parse_lun ( rp_comp[RP_LUN], &iscsi->lun ) ) != 0 ) {
		DBGC ( iscsi, "iSCSI %p invalid LUN \"%s\"\n",
		       iscsi, rp_comp[RP_LUN] );
		return rc;
	}
	iscsi->target_iqn = strdup ( rp_comp[RP_TARGETNAME] );
	if ( ! iscsi->target_iqn )
		return -ENOMEM;

	return 0;
}

/**
 * Fetch iSCSI settings
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
static int iscsi_fetch_settings ( struct iscsi_session *iscsi ) {
	char *hostname;
	union uuid uuid;
	int len;

	/* Fetch relevant settings.  Don't worry about freeing on
	 * error, since iscsi_free() will take care of that anyway.
	 */
	fetch_string_setting_copy ( NULL, &username_setting,
				    &iscsi->initiator_username );
	fetch_string_setting_copy ( NULL, &password_setting,
				    &iscsi->initiator_password );
	fetch_string_setting_copy ( NULL, &reverse_username_setting,
				    &iscsi->target_username );
	fetch_string_setting_copy ( NULL, &reverse_password_setting,
				    &iscsi->target_password );

	/* Use explicit initiator IQN if provided */
	fetch_string_setting_copy ( NULL, &initiator_iqn_setting,
				    &iscsi->initiator_iqn );
	if ( iscsi->initiator_iqn )
		return 0;

	/* Otherwise, try to construct an initiator IQN from the hostname */
	fetch_string_setting_copy ( NULL, &hostname_setting, &hostname );
	if ( hostname ) {
		len = asprintf ( &iscsi->initiator_iqn,
				 ISCSI_DEFAULT_IQN_PREFIX ":%s", hostname );
		free ( hostname );
		if ( len < 0 ) {
			DBGC ( iscsi, "iSCSI %p could not allocate initiator "
			       "IQN\n", iscsi );
			return -ENOMEM;
		}
		assert ( iscsi->initiator_iqn );
		return 0;
	}

	/* Otherwise, try to construct an initiator IQN from the UUID */
	if ( ( len = fetch_uuid_setting ( NULL, &uuid_setting, &uuid ) ) < 0 ) {
		DBGC ( iscsi, "iSCSI %p has no suitable initiator IQN\n",
		       iscsi );
		return -EINVAL_NO_INITIATOR_IQN;
	}
	if ( ( len = asprintf ( &iscsi->initiator_iqn,
				ISCSI_DEFAULT_IQN_PREFIX ":%s",
				uuid_ntoa ( &uuid ) ) ) < 0 ) {
		DBGC ( iscsi, "iSCSI %p could not allocate initiator IQN\n",
		       iscsi );
		return -ENOMEM;
	}
	assert ( iscsi->initiator_iqn );

	return 0;
}


/**
 * Check iSCSI authentication details
 *
 * @v iscsi		iSCSI session
 * @ret rc		Return status code
 */
static int iscsi_check_auth ( struct iscsi_session *iscsi ) {

	/* Check for invalid authentication combinations */
	if ( ( /* Initiator username without password (or vice-versa) */
		( !! iscsi->initiator_username ) ^
		( !! iscsi->initiator_password ) ) ||
	     ( /* Target username without password (or vice-versa) */
		( !! iscsi->target_username ) ^
		( !! iscsi->target_password ) ) ||
	     ( /* Target (reverse) without initiator (forward) */
		( iscsi->target_username &&
		  ( ! iscsi->initiator_username ) ) ) ) {
		DBGC ( iscsi, "iSCSI %p invalid credentials: initiator "
		       "%sname,%spw, target %sname,%spw\n", iscsi,
		       ( iscsi->initiator_username ? "" : "no " ),
		       ( iscsi->initiator_password ? "" : "no " ),
		       ( iscsi->target_username ? "" : "no " ),
		       ( iscsi->target_password ? "" : "no " ) );
		return -EINVAL_BAD_CREDENTIAL_MIX;
	}

	return 0;
}

/**
 * Open iSCSI URI
 *
 * @v parent		Parent interface
 * @v uri		URI
 * @ret rc		Return status code
 */
static int iscsi_open ( struct interface *parent, struct uri *uri ) {
	struct iscsi_session *iscsi;
	int rc;

	/* Sanity check */
	if ( ! uri->opaque ) {
		rc = -EINVAL_NO_ROOT_PATH;
		goto err_sanity_uri;
	}

	/* Allocate and initialise structure */
	iscsi = zalloc ( sizeof ( *iscsi ) );
	if ( ! iscsi ) {
		rc = -ENOMEM;
		goto err_zalloc;
	}
	ref_init ( &iscsi->refcnt, iscsi_free );
	intf_init ( &iscsi->control, &iscsi_control_desc, &iscsi->refcnt );
	intf_init ( &iscsi->data, &iscsi_data_desc, &iscsi->refcnt );
	intf_init ( &iscsi->socket, &iscsi_socket_desc, &iscsi->refcnt );
	process_init_stopped ( &iscsi->process, &iscsi_process_desc,
			       &iscsi->refcnt );
//	acpi_init ( &iscsi->desc, &ibft_model, &iscsi->refcnt );

	/* Parse root path */
	if ( ( rc = iscsi_parse_root_path ( iscsi, uri->opaque ) ) != 0 )
		goto err_parse_root_path;
	/* Set fields not specified by root path */
	if ( ( rc = iscsi_fetch_settings ( iscsi ) ) != 0 )
		goto err_fetch_settings;
	/* Validate authentication */
	if ( ( rc = iscsi_check_auth ( iscsi ) ) != 0 )
		goto err_check_auth;

	/* Sanity checks */
	if ( ! iscsi->target_address ) {
		DBGC ( iscsi, "iSCSI %p does not yet support discovery\n",
		       iscsi );
		rc = -ENOTSUP_DISCOVERY;
		goto err_sanity_address;
	}
	if ( ! iscsi->target_iqn ) {
		DBGC ( iscsi, "iSCSI %p no target address supplied in %s\n",
		       iscsi, uri->opaque );
		rc = -EINVAL_NO_TARGET_IQN;
		goto err_sanity_iqn;
	}
	DBGC ( iscsi, "iSCSI %p initiator %s\n",iscsi, iscsi->initiator_iqn );
	DBGC ( iscsi, "iSCSI %p target %s %s\n",
	       iscsi, iscsi->target_address, iscsi->target_iqn );

	/* Open socket */
	if ( ( rc = iscsi_open_connection ( iscsi ) ) != 0 )
		goto err_open_connection;

	/* Attach SCSI device to parent interface */
	if ( ( rc = scsi_open ( parent, &iscsi->control,
				&iscsi->lun ) ) != 0 ) {
		DBGC ( iscsi, "iSCSI %p could not create SCSI device: %s\n",
		       iscsi, strerror ( rc ) );
		goto err_scsi_open;
	}

	/* Mortalise self, and return */
	ref_put ( &iscsi->refcnt );
	return 0;
	
 err_scsi_open:
 err_open_connection:
 err_sanity_iqn:
 err_sanity_address:
 err_check_auth:
 err_fetch_settings:
 err_parse_root_path:
	iscsi_close ( iscsi, rc );
	ref_put ( &iscsi->refcnt );
 err_zalloc:
 err_sanity_uri:
	return rc;
}

/** iSCSI URI opener */
struct uri_opener iscsi_uri_opener __uri_opener = {
	.scheme = "iscsi",
	.open = iscsi_open,
};
