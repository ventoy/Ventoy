// ata.c:  ATA simulator for vblade
#include "config.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include "dat.h"
#include "fns.h"

enum {
	// err bits
	UNC =	1<<6,
	MC =	1<<5,
	IDNF =	1<<4,
	MCR =	1<<3,
	ABRT = 	1<<2,
	NM =	1<<1,

	// status bits
	BSY =	1<<7,
	DRDY =	1<<6,
	DF =	1<<5,
	DRQ =	1<<3,
	ERR =	1<<0,
};

static ushort ident[256];

static void
setfld(ushort *a, int idx, int len, char *str)	// set field in ident
{
	uchar *p;

	p = (uchar *)(a+idx);
	while (len > 0) {
		if (*str == 0)
			p[1] = ' ';
		else
			p[1] = *str++;
		if (*str == 0)
			p[0] = ' ';
		else
			p[0] = *str++;
		p += 2;
		len -= 2;
	}
}

static void
setlba28(ushort *ident, vlong lba)
{
	uchar *cp;

	cp = (uchar *) &ident[60];
	*cp++ = lba;
	*cp++ = lba >>= 8;
	*cp++ = lba >>= 8;
	*cp++ = (lba >>= 8) & 0xf;
}

static void
setlba48(ushort *ident, vlong lba)
{
	uchar *cp;

	cp = (uchar *) &ident[100];
	*cp++ = lba;
	*cp++ = lba >>= 8;
	*cp++ = lba >>= 8;
	*cp++ = lba >>= 8;
	*cp++ = lba >>= 8;
	*cp++ = lba >>= 8;
}

static void
setushort(ushort *a, int i, ushort n)
{
	uchar *p;

	p = (uchar *)(a+i);
	*p++ = n & 0xff;
	*p++ = n >> 8;
}

void
atainit(void)
{
	char buf[64];

	setushort(ident, 47, 0x8000);
	setushort(ident, 49, 0x0200);
	setushort(ident, 50, 0x4000);
	setushort(ident, 83, 0x5400);
	setushort(ident, 84, 0x4000);
	setushort(ident, 86, 0x1400);
	setushort(ident, 87, 0x4000);
	setushort(ident, 93, 0x400b);
	setfld(ident, 27, 40, "Coraid EtherDrive vblade");
	sprintf(buf, "V%d", VBLADE_VERSION);
	setfld(ident, 23, 8, buf);
	setfld(ident, 10, 20, serial);
}


/* The ATA spec is weird in that you specify the device size as number
 * of sectors and then address the sectors with an offset.  That means
 * with LBA 28 you shouldn't see an LBA of all ones.  Still, we don't
 * check for that.
 */
int
atacmd(Ataregs *p, uchar *dp, int ndp, int payload) // do the ata cmd
{
	vlong lba;
	ushort *ip;
	int n;
	enum { MAXLBA28SIZE = 0x0fffffff };
	extern int maxscnt;

	p->status = 0;
	switch (p->cmd) {
	default:
		p->status = DRDY | ERR;
		p->err = ABRT;
		return 0;
	case 0xe7:		// flush cache
		return 0;
	case 0xec:		// identify device
		if (p->sectors != 1 || ndp < 512)
			return -1;
		memmove(dp, ident, 512);
		ip = (ushort *)dp;
		if (size & ~MAXLBA28SIZE)
			setlba28(ip, MAXLBA28SIZE);
		else
			setlba28(ip, size);
		setlba48(ip, size);
		p->err = 0;
		p->status = DRDY;
		p->sectors = 0;
		return 0;
	case 0xe5:		// check power mode
		p->err = 0;
		p->sectors = 0xff; // the device is active or idle
		p->status = DRDY;
		return 0;
	case 0x20:		// read sectors
	case 0x30:		// write sectors
		lba = p->lba & MAXLBA28SIZE;
		break;
	case 0x24:		// read sectors ext
	case 0x34:		// write sectors ext
		lba = p->lba & 0x0000ffffffffffffLL;	// full 48
		break;
	}

	// we ought not be here unless we are a read/write

	if (p->sectors > maxscnt || p->sectors*512 > ndp)
		return -1;

	if (lba + p->sectors > size) {
		p->err = IDNF;
		p->status = DRDY | ERR;
		p->lba = lba;
		return 0;
	}
	if (p->cmd == 0x20 || p->cmd == 0x24)
		n = getsec(bfd, dp, lba+offset, p->sectors);
	else {
		// packet should be big enough to contain the data
		if (payload < 512 * p->sectors)
			return -1;
		n = putsec(bfd, dp, lba+offset, p->sectors);
	}
	n /= 512;
	if (n != p->sectors) {
		p->err = ABRT;
		p->status = ERR;
	} else
		p->err = 0;
	p->status |= DRDY;
	p->lba += n;
	p->sectors -= n;
	return 0;
}

