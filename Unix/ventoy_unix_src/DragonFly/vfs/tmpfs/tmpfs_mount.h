/*	$NetBSD: tmpfs_args.h,v 1.3 2008/07/29 09:10:09 pooka Exp $	*/
/*	$DragonFly: tmpfs_mount.h,v 1.3 2017/10/29 09:10:09 markuspf Exp $	*/

/*
 * Copyright (c) 2005, 2006, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VFS_TMPFS_TMPFS_MOUNT_H_
#define VFS_TMPFS_TMPFS_MOUNT_H_

/*
 * This structure is used to communicate mount parameters between userland
 * and kernel space.
 */
#define TMPFS_ARGS_VERSION	2
struct tmpfs_mount_info {
	int			ta_version;

	/* Size counters. */
	ino_t			ta_nodes_max;
	off_t			ta_size_max;
	size_t			ta_maxfsize_max;

	/* Root node attributes. */
	uid_t			ta_root_uid;
	gid_t			ta_root_gid;
	mode_t			ta_root_mode;
};

#define MNT_GID		0x00000001
#define MNT_UID		0x00000002
#define MNT_MODE	0x00000004
#define MNT_INODES	0x00000008
#define MNT_SIZE	0x00000010
#define MNT_MAXFSIZE	0x00000020

#endif /* VFS_TMPFS_TMPFS_MOUNT_H_ */
