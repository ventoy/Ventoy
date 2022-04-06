/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 longpanda <admin@ventoy.net>
 * Copyright (c) 2004-2005 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_G_VENTOY_H_
#define	_G_VENTOY_H_

#include <sys/endian.h>

#define	G_VENTOY_CLASS_NAME	"VENTOY"

#define	G_VENTOY_MAGIC		"GEOM::VENTOY"
/*
 * Version history:
 * 1 - Initial version number.
 * 2 - Added 'stop' command to gconcat(8).
 * 3 - Added md_provider field to metadata and '-h' option to gconcat(8).
 * 4 - Added md_provsize field to metadata.
 */
#define	G_VENTOY_VERSION	4

#ifdef _KERNEL
#define	G_VENTOY_TYPE_MANUAL	0
#define	G_VENTOY_TYPE_AUTOMATIC	1

#define G_DEBUG(...) if (bootverbose) printf(__VA_ARGS__)
#define G_VENTOY_DEBUG(lvl, ...) if (g_ventoy_debug) printf(__VA_ARGS__)
#define G_VENTOY_LOGREQ(bp, ...) if (g_ventoy_debug) printf(__VA_ARGS__)

struct g_ventoy_disk {
	struct g_consumer	*d_consumer;
	struct g_ventoy_softc	*d_softc;
	off_t			 d_start;
	off_t			 d_end;
    off_t			 d_map_start;
    off_t			 d_map_end;
	int			 d_candelete;
	int			 d_removed;
};

struct g_ventoy_softc {
	u_int		 sc_type;	/* provider type */
	struct g_geom	*sc_geom;
	struct g_provider *sc_provider;
	uint32_t	 sc_id;		/* concat unique ID */

	struct g_ventoy_disk *sc_disks;
	uint16_t	 sc_ndisks;
	struct mtx	 sc_lock;
};
#define	sc_name	sc_geom->name


#pragma pack(1)
#define VENTOY_UNIX_SEG_MAGIC0    0x11223344
#define VENTOY_UNIX_SEG_MAGIC1    0x55667788
#define VENTOY_UNIX_SEG_MAGIC2    0x99aabbcc
#define VENTOY_UNIX_SEG_MAGIC3    0xddeeff00
#define VENTOY_UNIX_MAX_SEGNUM   40960
struct g_ventoy_seg {
    uint64_t seg_start_bytes;
    uint64_t seg_end_bytes;
};

struct g_ventoy_map{
    uint32_t magic1[4];
    uint32_t magic2[4];
    uint64_t segnum;
    uint64_t disksize;
    uint8_t diskuuid[16];
    struct g_ventoy_seg seglist[VENTOY_UNIX_MAX_SEGNUM];
    uint32_t magic3[4];
};
#pragma pack()

#define VENTOY_MAP_VALID(magic2) \
    (magic2[0] == VENTOY_UNIX_SEG_MAGIC0 && magic2[1] == VENTOY_UNIX_SEG_MAGIC1 && magic2[2] == VENTOY_UNIX_SEG_MAGIC2 && magic2[3] == VENTOY_UNIX_SEG_MAGIC3)

#endif	/* _KERNEL */

struct g_ventoy_metadata {
	char		md_magic[16];	/* Magic value. */
	uint32_t	md_version;	/* Version number. */
	char		md_name[16];	/* Concat name. */
	uint32_t	md_id;		/* Unique ID. */
	uint16_t	md_no;		/* Disk number. */
	uint16_t	md_all;		/* Number of all disks. */
	char		md_provider[16]; /* Hardcoded provider. */
	uint64_t	md_provsize;	/* Provider's size. */
};
static __inline void
ventoy_metadata_encode(const struct g_ventoy_metadata *md, u_char *data)
{

	bcopy(md->md_magic, data, sizeof(md->md_magic));
	le32enc(data + 16, md->md_version);
	bcopy(md->md_name, data + 20, sizeof(md->md_name));
	le32enc(data + 36, md->md_id);
	le16enc(data + 40, md->md_no);
	le16enc(data + 42, md->md_all);
	bcopy(md->md_provider, data + 44, sizeof(md->md_provider));
	le64enc(data + 60, md->md_provsize);
}
static __inline void
ventoy_metadata_decode(const u_char *data, struct g_ventoy_metadata *md)
{

	bcopy(data, md->md_magic, sizeof(md->md_magic));
	md->md_version = le32dec(data + 16);
	bcopy(data + 20, md->md_name, sizeof(md->md_name));
	md->md_id = le32dec(data + 36);
	md->md_no = le16dec(data + 40);
	md->md_all = le16dec(data + 42);
	bcopy(data + 44, md->md_provider, sizeof(md->md_provider));
	md->md_provsize = le64dec(data + 60);
}
#endif	/* _G_VENTOY_H_ */
