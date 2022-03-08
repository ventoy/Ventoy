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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bio.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <geom/geom.h>
#include <geom/ventoy/g_ventoy.h>

FEATURE(geom_ventoy, "GEOM ventoy support");

static MALLOC_DEFINE(M_VENTOY, "ventoy_data", "GEOM_VENTOY Data");

SYSCTL_DECL(_kern_geom);
static SYSCTL_NODE(_kern_geom, OID_AUTO, ventoy, CTLFLAG_RW, 0,
    "GEOM_VENTOY stuff");
static u_int g_ventoy_debug = 0;
SYSCTL_UINT(_kern_geom_ventoy, OID_AUTO, debug, CTLFLAG_RWTUN, &g_ventoy_debug, 0,
    "Debug level");

extern int resource_string_value(const char *name, int unit, const char *resname, const char **result);
extern int resource_int_value(const char *name, int unit, const char *resname, int *result);

static int g_ventoy_destroy(struct g_ventoy_softc *sc, boolean_t force);
static int g_ventoy_destroy_geom(struct gctl_req *req, struct g_class *mp,
    struct g_geom *gp);

static g_taste_t g_ventoy_taste;
static g_ctl_req_t g_ventoy_config;
static g_dumpconf_t g_ventoy_dumpconf;

static char g_ventoy_disk_uuid[64];
static bool g_ventoy_tasted = false;
static off_t g_ventoy_disk_size = 0;
static off_t g_disk_map_start = 0;
static off_t g_disk_map_end = 0;
static int g_ventoy_remount = 0;

struct g_ventoy_map g_ventoy_map_data __attribute__((aligned (4096))) = 
{
    { VENTOY_UNIX_SEG_MAGIC0, VENTOY_UNIX_SEG_MAGIC1, VENTOY_UNIX_SEG_MAGIC2, VENTOY_UNIX_SEG_MAGIC3 },
    { 0, 0, 0, 0 },
    0, 0,
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { { 0, 0 } },
    { VENTOY_UNIX_SEG_MAGIC0, VENTOY_UNIX_SEG_MAGIC1, VENTOY_UNIX_SEG_MAGIC2, VENTOY_UNIX_SEG_MAGIC3 }
};

struct g_class g_ventoy_class = {
	.name = G_VENTOY_CLASS_NAME,
	.version = G_VERSION,
	.ctlreq = g_ventoy_config,
	.taste = g_ventoy_taste,
	.destroy_geom = g_ventoy_destroy_geom
};


/*
 * Greatest Common Divisor.
 */
static u_int
gcd(u_int a, u_int b)
{
	u_int c;

	while (b != 0) {
		c = a;
		a = b;
		b = (c % b);
	}
	return (a);
}

/*
 * Least Common Multiple.
 */
static u_int
lcm(u_int a, u_int b)
{

	return ((a * b) / gcd(a, b));
}

/*
 * Return the number of valid disks.
 */
static u_int
g_ventoy_nvalid(struct g_ventoy_softc *sc)
{
	u_int i, no;

	no = 0;
	for (i = 0; i < sc->sc_ndisks; i++) {
		if (sc->sc_disks[i].d_consumer != NULL)
			no++;
	}

	return (no);
}

static void
g_ventoy_remove_disk(struct g_ventoy_disk *disk)
{
	struct g_consumer *cp;
	struct g_ventoy_softc *sc;

	g_topology_assert();
	KASSERT(disk->d_consumer != NULL, ("Non-valid disk in %s.", __func__));
	sc = disk->d_softc;
	cp = disk->d_consumer;

	if (!disk->d_removed) {
		G_VENTOY_DEBUG(0, "Disk %s removed from %s.",
		    cp->provider->name, sc->sc_name);
		disk->d_removed = 1;
	}

	if (sc->sc_provider != NULL) {
		sc->sc_provider->flags |= G_PF_WITHER;
		G_VENTOY_DEBUG(0, "Device %s deactivated.",
		    sc->sc_provider->name);
		g_orphan_provider(sc->sc_provider, ENXIO);
		sc->sc_provider = NULL;
	}

	if (cp->acr > 0 || cp->acw > 0 || cp->ace > 0)
		return;
	disk->d_consumer = NULL;
	g_detach(cp);
	g_destroy_consumer(cp);
	/* If there are no valid disks anymore, remove device. */
	if (LIST_EMPTY(&sc->sc_geom->consumer))
		g_ventoy_destroy(sc, 1);
}

static void
g_ventoy_orphan(struct g_consumer *cp)
{
	struct g_ventoy_softc *sc;
	struct g_ventoy_disk *disk;
	struct g_geom *gp;

	g_topology_assert();
	gp = cp->geom;
	sc = gp->softc;
	if (sc == NULL)
		return;

	disk = cp->private;
	if (disk == NULL)	/* Possible? */
		return;
	g_ventoy_remove_disk(disk);
}

static int
g_ventoy_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_consumer *cp1, *cp2, *tmp;
	struct g_ventoy_disk *disk;
	struct g_geom *gp;
	int error;

    if (dw > 0) /* readonly */
		return (EPERM);

	g_topology_assert();
	gp = pp->geom;

    if (g_ventoy_remount)
    {
        de = 0;
    }
    else
    {
        /* On first open, grab an extra "exclusive" bit */
    	if (pp->acr == 0 && pp->acw == 0 && pp->ace == 0)
    		de++;
    	/* ... and let go of it on last close */
    	if ((pp->acr + dr) == 0 && (pp->acw + dw) == 0 && (pp->ace + de) == 0)
    		de--;
    }

	LIST_FOREACH_SAFE(cp1, &gp->consumer, consumer, tmp) {
		error = g_access(cp1, dr, dw, de);
		if (error != 0)
			goto fail;
		disk = cp1->private;
		if (cp1->acr == 0 && cp1->acw == 0 && cp1->ace == 0 &&
		    disk->d_removed) {
			g_ventoy_remove_disk(disk); /* May destroy geom. */
		}
	}
	return (0);

fail:
	LIST_FOREACH(cp2, &gp->consumer, consumer) {
		if (cp1 == cp2)
			break;
		g_access(cp2, -dr, -dw, -de);
	}
	return (error);
}

static void
g_ventoy_kernel_dump(struct bio *bp)
{
	struct g_ventoy_softc *sc;
	struct g_ventoy_disk *disk;
	struct bio *cbp;
	struct g_kerneldump *gkd;
	u_int i;

	sc = bp->bio_to->geom->softc;
	gkd = (struct g_kerneldump *)bp->bio_data;
	for (i = 0; i < sc->sc_ndisks; i++) {
		if (sc->sc_disks[i].d_start <= gkd->offset &&
		    sc->sc_disks[i].d_end > gkd->offset)
			break;
	}
	if (i == sc->sc_ndisks)
		g_io_deliver(bp, EOPNOTSUPP);
	disk = &sc->sc_disks[i];
	gkd->offset -= disk->d_start;
	if (gkd->length > disk->d_end - disk->d_start - gkd->offset)
		gkd->length = disk->d_end - disk->d_start - gkd->offset;
	cbp = g_clone_bio(bp);
	if (cbp == NULL) {
		g_io_deliver(bp, ENOMEM);
		return;
	}
	cbp->bio_done = g_std_done;
	g_io_request(cbp, disk->d_consumer);
	G_VENTOY_DEBUG(1, "Kernel dump will go to %s.",
	    disk->d_consumer->provider->name);
}

static void
g_ventoy_flush(struct g_ventoy_softc *sc, struct bio *bp)
{
	struct bio_queue_head queue;
	struct g_consumer *cp;
	struct bio *cbp;
	u_int no;

	bioq_init(&queue);
	for (no = 0; no < sc->sc_ndisks; no++) {
		cbp = g_clone_bio(bp);
		if (cbp == NULL) {
			for (cbp = bioq_first(&queue); cbp != NULL;
			    cbp = bioq_first(&queue)) {
				bioq_remove(&queue, cbp);
				g_destroy_bio(cbp);
			}
			if (bp->bio_error == 0)
				bp->bio_error = ENOMEM;
			g_io_deliver(bp, bp->bio_error);
			return;
		}
		bioq_insert_tail(&queue, cbp);
		cbp->bio_done = g_std_done;
		cbp->bio_caller1 = sc->sc_disks[no].d_consumer;
		cbp->bio_to = sc->sc_disks[no].d_consumer->provider;
	}
	for (cbp = bioq_first(&queue); cbp != NULL; cbp = bioq_first(&queue)) {
		bioq_remove(&queue, cbp);
		G_VENTOY_LOGREQ(cbp, "Sending request.");
		cp = cbp->bio_caller1;
		cbp->bio_caller1 = NULL;
		g_io_request(cbp, cp);
	}
}

static void
g_ventoy_start(struct bio *bp)
{
	struct bio_queue_head queue;
	struct g_ventoy_softc *sc;
	struct g_ventoy_disk *disk;
	struct g_provider *pp;
	off_t offset, end, length, off, len;
	struct bio *cbp;
	char *addr;
	u_int no;

	pp = bp->bio_to;
	sc = pp->geom->softc;
	/*
	 * If sc == NULL, provider's error should be set and g_ventoy_start()
	 * should not be called at all.
	 */
	KASSERT(sc != NULL,
	    ("Provider's error should be set (error=%d)(device=%s).",
	    bp->bio_to->error, bp->bio_to->name));

	G_VENTOY_LOGREQ(bp, "Request received.");

	switch (bp->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
	case BIO_DELETE:
		break;
	case BIO_FLUSH:
		g_ventoy_flush(sc, bp);
		return;
	case BIO_GETATTR:
		if (strcmp("GEOM::kerneldump", bp->bio_attribute) == 0) {
			g_ventoy_kernel_dump(bp);
			return;
		}
		/* To which provider it should be delivered? */
		/* FALLTHROUGH */
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}

	offset = bp->bio_offset;
	length = bp->bio_length;
	addr = bp->bio_data;
	end = offset + length;

	bioq_init(&queue);
	for (no = 0; no < sc->sc_ndisks; no++) {
		disk = &sc->sc_disks[no];
		if (disk->d_end <= offset)
			continue;
		if (disk->d_start >= end)
			break;

		off = offset - disk->d_start;
		len = MIN(length, disk->d_end - offset);
		length -= len;
		offset += len;

		cbp = g_clone_bio(bp);
		if (cbp == NULL) {
			for (cbp = bioq_first(&queue); cbp != NULL;
			    cbp = bioq_first(&queue)) {
				bioq_remove(&queue, cbp);
				g_destroy_bio(cbp);
			}
			if (bp->bio_error == 0)
				bp->bio_error = ENOMEM;
			g_io_deliver(bp, bp->bio_error);
			return;
		}
		bioq_insert_tail(&queue, cbp);
		/*
		 * Fill in the component buf structure.
		 */
        cbp->bio_done = g_std_done;
		cbp->bio_offset = off + disk->d_map_start;		
        cbp->bio_data = addr;
		addr += len;
        cbp->bio_length = len;
		cbp->bio_to = disk->d_consumer->provider;
		cbp->bio_caller1 = disk;

		if (length == 0)
			break;
	}
	KASSERT(length == 0,
	    ("Length is still greater than 0 (class=%s, name=%s).",
	    bp->bio_to->geom->class->name, bp->bio_to->geom->name));
	for (cbp = bioq_first(&queue); cbp != NULL; cbp = bioq_first(&queue)) {
		bioq_remove(&queue, cbp);
		G_VENTOY_LOGREQ(cbp, "Sending request.");
		disk = cbp->bio_caller1;
		cbp->bio_caller1 = NULL;
		g_io_request(cbp, disk->d_consumer);
	}
}

static void
g_ventoy_check_and_run(struct g_ventoy_softc *sc)
{
	struct g_ventoy_disk *disk;
	struct g_provider *dp, *pp;
	u_int no, sectorsize = 0;
	off_t start;

	g_topology_assert();
	if (g_ventoy_nvalid(sc) != sc->sc_ndisks)
		return;

	pp = g_new_providerf(sc->sc_geom, "ventoy/%s", sc->sc_name);
	start = 0;
	for (no = 0; no < sc->sc_ndisks; no++) {
		disk = &sc->sc_disks[no];
		dp = disk->d_consumer->provider;
		disk->d_start = start;
		disk->d_end = disk->d_start + (disk->d_map_end - disk->d_map_start);
		if (sc->sc_type == G_VENTOY_TYPE_AUTOMATIC)
			disk->d_end -= dp->sectorsize;
		start = disk->d_end;
		if (no == 0)
			sectorsize = dp->sectorsize;
		else {
			sectorsize = lcm(sectorsize, dp->sectorsize);
		
		}
	}
	pp->sectorsize = sectorsize;
	/* We have sc->sc_disks[sc->sc_ndisks - 1].d_end in 'start'. */
	pp->mediasize = start;
	pp->stripesize = sc->sc_disks[0].d_consumer->provider->stripesize;
	pp->stripeoffset = sc->sc_disks[0].d_consumer->provider->stripeoffset;
	sc->sc_provider = pp;
	g_error_provider(pp, 0);

	G_VENTOY_DEBUG(0, "Device %s activated.", sc->sc_provider->name);
}

static int
g_ventoy_read_metadata(struct g_consumer *cp, struct g_ventoy_metadata *md)
{
	struct g_provider *pp;
	u_char *buf;
	int error;

	g_topology_assert();

	error = g_access(cp, 1, 0, 0);
	if (error != 0)
		return (error);
	pp = cp->provider;
	g_topology_unlock();
	buf = g_read_data(cp, pp->mediasize - pp->sectorsize, pp->sectorsize,
	    &error);
	g_topology_lock();
	g_access(cp, -1, 0, 0);
	if (buf == NULL)
		return (error);

	/* Decode metadata. */
	ventoy_metadata_decode(buf, md);
	g_free(buf);

	return (0);
}

/*
 * Add disk to given device.
 */
static int
g_ventoy_add_disk(struct g_ventoy_softc *sc, struct g_provider *pp, u_int no)
{
	struct g_ventoy_disk *disk;
	struct g_consumer *cp, *fcp;
	struct g_geom *gp;
	int error;

	g_topology_assert();
	/* Metadata corrupted? */
	if (no >= sc->sc_ndisks)
		return (EINVAL);

	disk = &sc->sc_disks[no];
	/* Check if disk is not already attached. */
	if (disk->d_consumer != NULL)
		return (EEXIST);

	gp = sc->sc_geom;
	fcp = LIST_FIRST(&gp->consumer);

	cp = g_new_consumer(gp);
	error = g_attach(cp, pp);
	if (error != 0) {
		g_destroy_consumer(cp);
		return (error);
	}

	if (fcp != NULL && (fcp->acr > 0 || fcp->acw > 0 || fcp->ace > 0)) {
		error = g_access(cp, fcp->acr, fcp->acw, fcp->ace);
		if (error != 0) {
			g_detach(cp);
			g_destroy_consumer(cp);
			return (error);
		}
	}
	if (sc->sc_type == G_VENTOY_TYPE_AUTOMATIC) {
		struct g_ventoy_metadata md;

		/* Re-read metadata. */
		error = g_ventoy_read_metadata(cp, &md);
		if (error != 0)
			goto fail;

		if (strcmp(md.md_magic, G_VENTOY_MAGIC) != 0 ||
		    strcmp(md.md_name, sc->sc_name) != 0 ||
		    md.md_id != sc->sc_id) {
			G_VENTOY_DEBUG(0, "Metadata on %s changed.", pp->name);
			goto fail;
		}
	}

	cp->private = disk;
	disk->d_consumer = cp;
	disk->d_softc = sc;
	disk->d_start = 0;	/* not yet */
	disk->d_end = 0;	/* not yet */
	disk->d_removed = 0;
    
	disk->d_map_start = g_disk_map_start;
	disk->d_map_end = g_disk_map_end;

	G_VENTOY_DEBUG(0, "Disk %s attached to %s.", pp->name, sc->sc_name);

	g_ventoy_check_and_run(sc);

	return (0);
fail:
	if (fcp != NULL && (fcp->acr > 0 || fcp->acw > 0 || fcp->ace > 0))
		g_access(cp, -fcp->acr, -fcp->acw, -fcp->ace);
	g_detach(cp);
	g_destroy_consumer(cp);
	return (error);
}

static struct g_geom *
g_ventoy_create(struct g_class *mp, const struct g_ventoy_metadata *md,
    u_int type)
{
	struct g_ventoy_softc *sc;
	struct g_geom *gp;
	u_int no;

	G_VENTOY_DEBUG(1, "Creating device %s (id=%u).", md->md_name,
	    md->md_id);

	/* One disks is minimum. */
	if (md->md_all < 1)
		return (NULL);

	/* Check for duplicate unit */
	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc != NULL && strcmp(sc->sc_name, md->md_name) == 0) {
			G_VENTOY_DEBUG(0, "Device %s already configured.",
			    gp->name);
			return (NULL);
		}
	}
	gp = g_new_geomf(mp, "%s", md->md_name);
	sc = malloc(sizeof(*sc), M_VENTOY, M_WAITOK | M_ZERO);
	gp->start = g_ventoy_start;
	gp->spoiled = g_ventoy_orphan;
	gp->orphan = g_ventoy_orphan;
	gp->access = g_ventoy_access;
	gp->dumpconf = g_ventoy_dumpconf;

	sc->sc_id = md->md_id;
	sc->sc_ndisks = md->md_all;
	sc->sc_disks = malloc(sizeof(struct g_ventoy_disk) * sc->sc_ndisks,
	    M_VENTOY, M_WAITOK | M_ZERO);
	for (no = 0; no < sc->sc_ndisks; no++)
		sc->sc_disks[no].d_consumer = NULL;
	sc->sc_type = type;

	gp->softc = sc;
	sc->sc_geom = gp;
	sc->sc_provider = NULL;

	G_VENTOY_DEBUG(0, "Device %s created (id=%u).", sc->sc_name, sc->sc_id);

	return (gp);
}

static int
g_ventoy_destroy(struct g_ventoy_softc *sc, boolean_t force)
{
	struct g_provider *pp;
	struct g_consumer *cp, *cp1;
	struct g_geom *gp;

	g_topology_assert();

	if (sc == NULL)
		return (ENXIO);

	pp = sc->sc_provider;
	if (pp != NULL && (pp->acr != 0 || pp->acw != 0 || pp->ace != 0)) {
		if (force) {
			G_VENTOY_DEBUG(0, "Device %s is still open, so it "
			    "can't be definitely removed.", pp->name);
		} else {
			G_VENTOY_DEBUG(1,
			    "Device %s is still open (r%dw%de%d).", pp->name,
			    pp->acr, pp->acw, pp->ace);
			return (EBUSY);
		}
	}

	gp = sc->sc_geom;
	LIST_FOREACH_SAFE(cp, &gp->consumer, consumer, cp1) {
		g_ventoy_remove_disk(cp->private);
		if (cp1 == NULL)
			return (0);	/* Recursion happened. */
	}
	if (!LIST_EMPTY(&gp->consumer))
		return (EINPROGRESS);

	gp->softc = NULL;
	KASSERT(sc->sc_provider == NULL, ("Provider still exists? (device=%s)",
	    gp->name));
	free(sc->sc_disks, M_VENTOY);
	free(sc, M_VENTOY);

	G_VENTOY_DEBUG(0, "Device %s destroyed.", gp->name);
	g_wither_geom(gp, ENXIO);
	return (0);
}

static int
g_ventoy_destroy_geom(struct gctl_req *req __unused,
    struct g_class *mp __unused, struct g_geom *gp)
{
	struct g_ventoy_softc *sc;

	sc = gp->softc;
	return (g_ventoy_destroy(sc, 0));
}

static bool g_vtoy_check_disk(struct g_class *mp, struct g_provider *pp)
{
    int i;
    int vlnk = 0;    
    bool ret = true;
    uint8_t *buf;
    char uuid[64];
    const char *value;
    struct g_consumer *cp;
	struct g_geom *gp;
    uint8_t mbrdata[] = {
        0xEB, 0x63, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,    
        0x56, 0x54, 0x00, 0x47, 0x65, 0x00, 0x48, 0x44, 0x00, 0x52, 0x64, 0x00, 0x20, 0x45, 0x72, 0x0D,
    };
    
    if (g_ventoy_disk_size == 0)
    {
        if (VENTOY_MAP_VALID(g_ventoy_map_data.magic2))
        {
            G_DEBUG("ventoy map data is valid. [OK]\n");

            for (i = 0; i < 16; i++)
            {
                sprintf(uuid + i * 2, "%02x", g_ventoy_map_data.diskuuid[i]);
            }
            snprintf(g_ventoy_disk_uuid, sizeof(g_ventoy_disk_uuid), "%s", uuid);
            g_ventoy_disk_size = g_ventoy_map_data.disksize;
            
            G_DEBUG("ventoy.disksize: %llu\n", (unsigned long long)g_ventoy_disk_size);
            G_DEBUG("ventoy.diskuuid: <%s>\n", g_ventoy_disk_uuid);
        }
        else
        {
            G_DEBUG("ventoy map data is invalid, get from resource\n");
        
            if (resource_string_value("ventoy", 0, "disksize", &value) == 0)
            {
                G_DEBUG("ventoy.disksize: %s\n", value);
                g_ventoy_disk_size = strtouq(value, NULL, 0);
            }

            if (resource_string_value("ventoy", 0, "diskuuid", &value) == 0)
            {
                snprintf(g_ventoy_disk_uuid, sizeof(g_ventoy_disk_uuid), "%s", value);
                G_DEBUG("ventoy.diskuuid: <%s>\n", value);
            }
        }
    }

    if (g_ventoy_disk_size != pp->mediasize)
    {
        return false;
    }

    if (strncmp(pp->name, "cd", 2) == 0 || strchr(pp->name, '/'))
    {
        return false;
    }

    /* read UUID from disk */
    gp = g_new_geomf(mp, "ventoy:taste");
	gp->start = NULL;
	gp->access = g_ventoy_access;
	gp->orphan = g_ventoy_orphan;
	cp = g_new_consumer(gp);
	g_attach(cp, pp);

    g_access(cp, 1, 0, 0);
	g_topology_unlock();
	buf = g_read_data(cp, 0, pp->sectorsize, NULL); 
	g_topology_lock();
	g_access(cp, -1, 0, 0);
    
    g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	gp = NULL;

    if (!buf)
    {
        return false;
    }

    for (i = 0; i < 16; i++)
    {
        sprintf(uuid + i * 2, "%02x", buf[0x180 + i]);
    }

    if (strncmp(g_ventoy_disk_uuid, uuid, 32))
    {
        ret = false;
    }

    if (resource_int_value("ventoy", 0, "vlnk", &vlnk) || (vlnk != 1))
    {
        if (memcmp(mbrdata, buf, 0x30) || memcmp(mbrdata + 0x30, buf + 0x190, 16))
        {
            ret = false;
        }
    }

    g_free(buf);

    if (ret)
    {
        G_DEBUG("ventoy disk check OK\n");
    }

    return ret;
}

static struct g_geom *
g_ventoy_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
    int i;
	int error;
    int disknum;
    int remount = 0;
    char *endpos;
    const char *value;
    const char *alias = NULL;
	struct g_geom *gp;
	struct g_ventoy_metadata md;
	struct g_ventoy_softc *sc;

    if (g_ventoy_tasted)
    {
        return NULL;
    }

	G_DEBUG("%s(%s, %s)\n", __func__, mp->name, pp->name);
	g_topology_assert();

	/* Skip providers that are already open for writing. */
	if (pp->acw > 0)
		return (NULL);

    if (!g_vtoy_check_disk(mp, pp))
    {
        return NULL;
    }

    g_ventoy_tasted = true;

    G_DEBUG("###### ventoy disk <%s> ######\n", pp->name);

    /* hint.ventoy.0.remount=1 */
    if (resource_int_value("ventoy", 0, "remount", &remount) == 0 && remount == 1)
    {
        g_ventoy_remount = 1;
        G_DEBUG("###### ventoy remount enabled ######\n");
    }
    
    /* hint.ventoy.0.alias=xxx */
    if (resource_string_value("ventoy", 0, "alias", &alias) == 0 && alias)
    {
        G_DEBUG("###### ventoy alias <%s> ######\n", alias);
        setenv("kern.devalias.ventoy/IMAGE", alias);
    }

    if (VENTOY_MAP_VALID(g_ventoy_map_data.magic2))
    {
        disknum = (int)g_ventoy_map_data.segnum;
        G_DEBUG("segnum from map data is:<%d>\n", disknum);
    }
    else
    {
        resource_int_value("ventoy", 0, "segnum", &disknum);
        G_DEBUG("segnum from resource is:<%d>\n", disknum);
    }

    strlcpy(md.md_magic, G_VENTOY_MAGIC, sizeof(md.md_magic));
	md.md_version = G_VENTOY_VERSION;
	strlcpy(md.md_name, "IMAGE", sizeof(md.md_name));
	md.md_id = arc4random();
	md.md_no = 0;
	md.md_all = (uint16_t)disknum;
	bzero(md.md_provider, sizeof(md.md_provider));
	/* This field is not important here. */
	md.md_provsize = 0;

	gp = g_ventoy_create(mp, &md, G_VENTOY_TYPE_MANUAL);
	if (gp == NULL) {
		G_VENTOY_DEBUG(0, "Cannot create device %s.",
		    md.md_name);
		return (NULL);
	}
	sc = gp->softc;

    for (i = 0; i < disknum; i ++)
    {
        if (VENTOY_MAP_VALID(g_ventoy_map_data.magic2))
        {
            G_DEBUG("[map] ventoy segment%d: 0x%llx@0x%llx\n", i, 
                (long long)g_ventoy_map_data.seglist[i].seg_start_bytes,
                (long long)g_ventoy_map_data.seglist[i].seg_end_bytes);
            
            g_disk_map_start = (off_t)g_ventoy_map_data.seglist[i].seg_start_bytes;
            g_disk_map_end = (off_t)g_ventoy_map_data.seglist[i].seg_end_bytes;
        }
        else
        {
            if (resource_string_value("ventoy", i, "seg", &value) == 0)
            {
                g_disk_map_start = strtouq(value, &endpos, 0);
                g_disk_map_end = strtouq(endpos + 1, NULL, 0);
            }
            else
            {
                printf("Failed to parse ventoy seg %d\n", i);
                continue;
            }
            G_DEBUG("[resource] ventoy segment%d: %s\n", i, value);
        }
        
        G_VENTOY_DEBUG(1, "Adding disk %s to %s.", pp->name, gp->name);
    	error = g_ventoy_add_disk(sc, pp, i);
    	if (error != 0) {
    		G_VENTOY_DEBUG(0,
    		    "Cannot add disk %s to %s (error=%d).", pp->name,
    		    gp->name, error);
    		g_ventoy_destroy(sc, 1);
    		return (NULL);
    	}
        
        g_disk_map_start = 0;
        g_disk_map_end = 0;
    }

    return (gp);
}

static void
g_ventoy_ctl_create(struct gctl_req *req, struct g_class *mp)
{
	u_int attached, no;
	struct g_ventoy_metadata md;
	struct g_provider *pp;
	struct g_ventoy_softc *sc;
	struct g_geom *gp;
	struct sbuf *sb;
	const char *name;
	char param[16];
	int *nargs;

	g_topology_assert();
	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 2) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	strlcpy(md.md_magic, G_VENTOY_MAGIC, sizeof(md.md_magic));
	md.md_version = G_VENTOY_VERSION;
	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	strlcpy(md.md_name, name, sizeof(md.md_name));
	md.md_id = arc4random();
	md.md_no = 0;
	md.md_all = *nargs - 1;
	bzero(md.md_provider, sizeof(md.md_provider));
	/* This field is not important here. */
	md.md_provsize = 0;

	/* Check all providers are valid */
	for (no = 1; no < *nargs; no++) {
		snprintf(param, sizeof(param), "arg%u", no);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", no);
			return;
		}
		if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
			name += strlen("/dev/");
		pp = g_provider_by_name(name);
		if (pp == NULL) {
			G_VENTOY_DEBUG(1, "Disk %s is invalid.", name);
			gctl_error(req, "Disk %s is invalid.", name);
			return;
		}
	}

	gp = g_ventoy_create(mp, &md, G_VENTOY_TYPE_MANUAL);
	if (gp == NULL) {
		gctl_error(req, "Can't configure %s.", md.md_name);
		return;
	}

	sc = gp->softc;
	sb = sbuf_new_auto();
	sbuf_printf(sb, "Can't attach disk(s) to %s:", gp->name);
	for (attached = 0, no = 1; no < *nargs; no++) {
		snprintf(param, sizeof(param), "arg%u", no);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%d' argument.", no);
			return;
		}
		if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
			name += strlen("/dev/");
		pp = g_provider_by_name(name);
		KASSERT(pp != NULL, ("Provider %s disappear?!", name));
		if (g_ventoy_add_disk(sc, pp, no - 1) != 0) {
			G_VENTOY_DEBUG(1, "Disk %u (%s) not attached to %s.",
			    no, pp->name, gp->name);
			sbuf_printf(sb, " %s", pp->name);
			continue;
		}
		attached++;
	}
	sbuf_finish(sb);
	if (md.md_all != attached) {
		g_ventoy_destroy(gp->softc, 1);
		gctl_error(req, "%s", sbuf_data(sb));
	}
	sbuf_delete(sb);
}

static struct g_ventoy_softc *
g_ventoy_find_device(struct g_class *mp, const char *name)
{
	struct g_ventoy_softc *sc;
	struct g_geom *gp;

	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc == NULL)
			continue;
		if (strcmp(sc->sc_name, name) == 0)
			return (sc);
	}
	return (NULL);
}

static void
g_ventoy_ctl_destroy(struct gctl_req *req, struct g_class *mp)
{
	struct g_ventoy_softc *sc;
	int *force, *nargs, error;
	const char *name;
	char param[16];
	u_int i;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs <= 0) {
		gctl_error(req, "Missing device(s).");
		return;
	}
	force = gctl_get_paraml(req, "force", sizeof(*force));
	if (force == NULL) {
		gctl_error(req, "No '%s' argument.", "force");
		return;
	}

	for (i = 0; i < (u_int)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", i);
			return;
		}
		sc = g_ventoy_find_device(mp, name);
		if (sc == NULL) {
			gctl_error(req, "No such device: %s.", name);
			return;
		}
		error = g_ventoy_destroy(sc, *force);
		if (error != 0) {
			gctl_error(req, "Cannot destroy device %s (error=%d).",
			    sc->sc_name, error);
			return;
		}
	}
}

static void
g_ventoy_config(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	uint32_t *version;

    return;
    
	g_topology_assert();
    
	version = gctl_get_paraml(req, "version", sizeof(*version));
	if (version == NULL) {
		gctl_error(req, "No '%s' argument.", "version");
		return;
	}
	if (*version != G_VENTOY_VERSION) {
		gctl_error(req, "Userland and kernel parts are out of sync.");
		return;
	}

	if (strcmp(verb, "create") == 0) {
		g_ventoy_ctl_create(req, mp);
		return;
	} else if (strcmp(verb, "destroy") == 0 ||
	    strcmp(verb, "stop") == 0) {
		g_ventoy_ctl_destroy(req, mp);
		return;
	}
	gctl_error(req, "Unknown verb.");
}

static void
g_ventoy_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
    struct g_consumer *cp, struct g_provider *pp)
{
	struct g_ventoy_softc *sc;

	g_topology_assert();
	sc = gp->softc;
	if (sc == NULL)
		return;
	if (pp != NULL) {
		/* Nothing here. */
	} else if (cp != NULL) {
		struct g_ventoy_disk *disk;

		disk = cp->private;
		if (disk == NULL)
			return;
		sbuf_printf(sb, "%s<End>%jd</End>\n", indent,
		    (intmax_t)disk->d_end);
		sbuf_printf(sb, "%s<Start>%jd</Start>\n", indent,
		    (intmax_t)disk->d_start);
	} else {
		sbuf_printf(sb, "%s<ID>%u</ID>\n", indent, (u_int)sc->sc_id);
		sbuf_printf(sb, "%s<Type>", indent);
		switch (sc->sc_type) {
		case G_VENTOY_TYPE_AUTOMATIC:
			sbuf_cat(sb, "AUTOMATIC");
			break;
		case G_VENTOY_TYPE_MANUAL:
			sbuf_cat(sb, "MANUAL");
			break;
		default:
			sbuf_cat(sb, "UNKNOWN");
			break;
		}
		sbuf_cat(sb, "</Type>\n");
		sbuf_printf(sb, "%s<Status>Total=%u, Online=%u</Status>\n",
		    indent, sc->sc_ndisks, g_ventoy_nvalid(sc));
		sbuf_printf(sb, "%s<State>", indent);
		if (sc->sc_provider != NULL && sc->sc_provider->error == 0)
			sbuf_cat(sb, "UP");
		else
			sbuf_cat(sb, "DOWN");
		sbuf_cat(sb, "</State>\n");
	}
}

DECLARE_GEOM_CLASS(g_ventoy_class, g_ventoy);
