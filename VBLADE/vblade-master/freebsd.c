/*
 * Copyright (c) 2005, Stacey Son <sson (at) verio (dot) net>
 * All rights reserved.
 */

// freebsd.c: low level access routines for FreeBSD
#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <net/ethernet.h>
#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <net/if.h>
#include <sys/stat.h>
#include <sys/disk.h>
#include <sys/select.h>
#include <sys/sysctl.h>

#include <fcntl.h>
#include <errno.h>

#include "dat.h"
#include "fns.h"

#define BPF_DEV "/dev/bpf0"

/* Packet buffer for getpkt() */
static uchar *pktbuf = NULL;
static int pktbufsz = 0;

int
dial(char *eth, int bufcnt)
{
	char m;
	int fd = -1;
	struct bpf_version bv;
	u_int v;
	unsigned bufsize, linktype;
	char device[sizeof BPF_DEV];
	struct ifreq ifr;

	struct bpf_program *bpf_program = create_bpf_program(shelf, slot);
	
	strncpy(device, BPF_DEV, sizeof BPF_DEV);

	/* find a bpf device we can use, check /dev/bpf[0-9] */
	for (m = '0'; m <= '9'; m++) {
		device[sizeof(BPF_DEV)-2] = m;

		if ((fd = open(device, O_RDWR)) > 0)
			break;
	}

	if (fd < 0) {
		perror("open");
		return -1;
	}

	if (ioctl(fd, BIOCVERSION, &bv) < 0) {
		perror("BIOCVERSION");
		goto bad;
	}

	if (bv.bv_major != BPF_MAJOR_VERSION ||
	    bv.bv_minor < BPF_MINOR_VERSION) {
		fprintf(stderr,
			"kernel bpf filter out of date\n");
		goto bad;
	}

	/*
	 * Try finding a good size for the buffer; 65536 may be too
	 * big, so keep cutting it in half until we find a size
	 * that works, or run out of sizes to try.
	 *
	 */
	for (v = 65536; v != 0; v >>= 1) {
		(void) ioctl(fd, BIOCSBLEN, (caddr_t)&v);

		(void)strncpy(ifr.ifr_name, eth,
			sizeof(ifr.ifr_name));
		if (ioctl(fd, BIOCSETIF, (caddr_t)&ifr) >= 0)
			break;  /* that size worked; we're done */

		if (errno != ENOBUFS) {
			fprintf(stderr, "BIOCSETIF: %s: %s\n",
					eth, strerror(errno));
			goto bad;
		}
	}
	if (v == 0) {
		fprintf(stderr, 
			"BIOCSBLEN: %s: No buffer size worked\n", eth);
		goto bad;
	}

	/* Allocate memory for the packet buffer */
	pktbufsz = v;
	if ((pktbuf = malloc(pktbufsz)) == NULL) {
		perror("malloc");
		goto bad;
	}

	/* Don't wait for buffer to be full or timeout */
	v = 1;
	if (ioctl(fd, BIOCIMMEDIATE, &v) < 0) {
		perror("BIOCIMMEDIATE");
		goto bad;
	}

	/* Only read incoming packets */
	v = 0;
	if (ioctl(fd, BIOCSSEESENT, &v) < 0) {
		perror("BIOCSSEESENT");
		goto bad;
	}

	/* Don't complete ethernet hdr */
	v = 1;
	if (ioctl(fd, BIOCSHDRCMPLT, &v) < 0) {
		perror("BIOCSHDRCMPLT");
		goto bad;
	}

	/* Get the data link layer type. */
	if (ioctl(fd, BIOCGDLT, (caddr_t)&v) < 0) {
		perror("BIOCGDLT");
		goto bad;
	}
	linktype = v;

	/* Get the filter buf size */
	if (ioctl(fd, BIOCGBLEN, (caddr_t)&v) < 0) {
		perror("BIOCGBLEN");
		goto bad;
	}
	bufsize = v;

	if (ioctl(fd, BIOCSETF, (caddr_t)bpf_program) < 0) {
		perror("BIOSETF");
		goto bad;
	} 

	free_bpf_program(bpf_program);
	return(fd);

bad:
	free_bpf_program(bpf_program);
	close(fd);
	return(-1);
}

int
getea(int s, char *eth, uchar *ea)
{
	int mib[6];
	size_t len;
	char *buf, *next, *end;
	struct if_msghdr *ifm;
	struct sockaddr_dl *sdl;
	

	mib[0] = CTL_NET; 	mib[1] = AF_ROUTE;
	mib[2] = 0; 		mib[3] = AF_LINK;
	mib[4] = NET_RT_IFLIST;	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0) {
		return (-1);
	}

	if (!(buf = (char *) malloc(len))) {
		return (-1);
	}
	
	if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) {
		free(buf);
		return (-1);
	}
	end = buf + len;

	for (next = buf; next < end; next += ifm->ifm_msglen) {
		ifm = (struct if_msghdr *)next;
		if (ifm->ifm_type == RTM_IFINFO) {
			sdl = (struct sockaddr_dl *)(ifm + 1);
			if (strncmp(&sdl->sdl_data[0], eth, 
					sdl->sdl_nlen) == 0) {
				memcpy(ea, LLADDR(sdl), ETHER_ADDR_LEN);
				break;
			}

		}

	}

	free(buf);
	return(0);
}


#if 0
int
getsec(int fd, uchar *place, vlong lba, int nsec)
{
	return pread(fd, place, nsec * 512, lba * 512);
}

int
putsec(int fd, uchar *place, vlong lba, int nsec)
{
	return pwrite(fd, place, nsec * 512, lba * 512);
}
#endif

static int pktn = 0;
static uchar *pktbp = NULL;

int
getpkt(int fd, uchar *buf, int sz)
{
	register struct bpf_hdr *bh;
	register int pktlen, retlen;
	
	if (pktn <= 0) { 
		if ((pktn = read(fd, pktbuf, pktbufsz)) < 0) {
			perror("read");
			exit(1);
		}
		pktbp = pktbuf;
	}

	bh = (struct bpf_hdr *) pktbp;
	retlen = (int) bh->bh_caplen;
	/* This memcpy() is currently needed */ 
	memcpy(buf, (void *)(pktbp + bh->bh_hdrlen),
		retlen > sz ? sz : retlen);
	pktlen = bh->bh_hdrlen + bh->bh_caplen; 
	
	pktbp = pktbp + BPF_WORDALIGN(pktlen);
	pktn  -= (int) BPF_WORDALIGN(pktlen);

	return retlen; 
}

int
putpkt(int fd, uchar *buf, int sz)
{
	return write(fd, buf, sz);
}

int
getmtu(int fd, char *name)
{
	struct ifreq xx;
	int s, n, p;

	s = socket(AF_INET, SOCK_RAW, 0);
	if (s == -1) {
		perror("Can't get mtu");
		return 1500;
	}
	xx.ifr_addr.sa_family = AF_INET;
	snprintf(xx.ifr_name, sizeof xx.ifr_name, "%s", name);
	n = ioctl(s, SIOCGIFMTU, &xx);
	if (n == -1) {
		perror("Can't get mtu");
		return 1500;
	}
	close(s);
	// FreeBSD bpf writes are capped at one PAGESIZE'd mbuf. As such we must
	// limit our sector count. See FreeBSD PR 205164, OpenAoE/vblade #7.
	p = getpagesize();
	if (xx.ifr_mtu > p) {
		return p;
	}
	return xx.ifr_mtu;
}

vlong
getsize(int fd)
{
	off_t media_size;
	vlong size;
	struct stat s;
	int n;

	// Try getting disklabel from block dev
	if ((n = ioctl(fd, DIOCGMEDIASIZE, &media_size)) != -1) {
		size = media_size;
	} else {
		// must not be a block special dev
		if (fstat(fd, &s) == -1) {
			perror("getsize");
			exit(1);
		}
		size = s.st_size;
	}
	printf("ioctl returned %d\n", n);
	printf("%lld bytes\n", size);
	return size;
}
