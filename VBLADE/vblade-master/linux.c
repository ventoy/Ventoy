// linux.c: low level access routines for Linux
#define _GNU_SOURCE
#include "config.h"
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <features.h>    /* for the glibc version number */
#if __GLIBC__ >= 2 && __GLIBC_MINOR >= 1
#include <netpacket/packet.h>
#include <net/ethernet.h>     /* the L2 protocols */
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>   /* The L2 protocols */
#endif

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/fs.h>
#include <sys/stat.h>

#include "dat.h"
#include "fns.h"

int	getindx(int, char *);
int	getea(int, char *, uchar *);



int
dial(char *eth, int bufcnt)		// get us a raw connection to an interface
{
	int i, n, s;
	struct sockaddr_ll sa;
	enum { aoe_type = 0x88a2 };

	memset(&sa, 0, sizeof sa);
	s = socket(PF_PACKET, SOCK_RAW, htons(aoe_type));
	if (s == -1) {
		perror("got bad socket");
		return -1;
	}
	i = getindx(s, eth);
	if (i < 0) {
		perror(eth);
		return -1;
	}
	sa.sll_family = AF_PACKET;
	sa.sll_protocol = htons(0x88a2);
	sa.sll_ifindex = i;
	n = bind(s, (struct sockaddr *)&sa, sizeof sa);
	if (n == -1) {
		perror("bind funky");
		return -1;
	}

	struct bpf_program {
		ulong bf_len;
		void *bf_insns;
	} *bpf_program = create_bpf_program(shelf, slot);
	setsockopt(s, SOL_SOCKET, SO_ATTACH_FILTER, bpf_program, sizeof(*bpf_program));
	free_bpf_program(bpf_program);

	n = bufcnt * getmtu(s, eth);
	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &n, sizeof(n)) < 0)
		perror("setsockopt SOL_SOCKET, SO_SNDBUF");
	if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n)) < 0)
		perror("setsockopt SOL_SOCKET, SO_RCVBUF");

	return s;
}

int
getindx(int s, char *name)	// return the index of device 'name'
{
	struct ifreq xx;
	int n;

	snprintf(xx.ifr_name, sizeof xx.ifr_name, "%s", name);
	n = ioctl(s, SIOCGIFINDEX, &xx);
	if (n == -1)
		return -1;
	return xx.ifr_ifindex;
}

int
getea(int s, char *name, uchar *ea)
{
	struct ifreq xx;
	int n;

        snprintf(xx.ifr_name, sizeof xx.ifr_name, "%s", name);
	n = ioctl(s, SIOCGIFHWADDR, &xx);
	if (n == -1) {
		perror("Can't get hw addr");
		return 0;
	}
	memmove(ea, xx.ifr_hwaddr.sa_data, 6);
	return 1;
}

int
getmtu(int s, char *name)
{
	struct ifreq xx;
	int n;

	snprintf(xx.ifr_name, sizeof xx.ifr_name, "%s", name);
	n = ioctl(s, SIOCGIFMTU, &xx);
	if (n == -1) {
		perror("Can't get mtu");
		return 1500;
	}
	return xx.ifr_mtu;
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

int
getpkt(int fd, uchar *buf, int sz)
{
	return read(fd, buf, sz);
}

int
putpkt(int fd, uchar *buf, int sz)
{
	return write(fd, buf, sz);
}

vlong
getsize(int fd)
{
	vlong size;
	struct stat s;
	int n;

	n = ioctl(fd, BLKGETSIZE64, &size);
	if (n == -1) {	// must not be a block special
		n = fstat(fd, &s);
		if (n == -1) {
			perror("getsize");
			exit(1);
		}
		size = s.st_size;
	}
	return size;
}
