// aoe.c: the ATA over Ethernet virtual EtherDrive (R) blade
#define _GNU_SOURCE
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include "dat.h"
#include "fns.h"

enum {
	Nmasks= 32,
	Nsrr= 256,
	Alen= 6,
};

uchar masks[Nmasks*Alen];
int nmasks;
uchar srr[Nsrr*Alen];
int nsrr;
char config[Nconfig];
int nconfig = 0;
int maxscnt = 2;
char *ifname;
int bufcnt = Bufcount;

#ifndef O_BINARY
#define O_BINARY 0
#endif

typedef unsigned long long u64_t;
typedef unsigned int    u32_t;

#pragma pack(4)
typedef struct ventoy_img_chunk
{
    u32_t img_start_sector; // sector size: 2KB
    u32_t img_end_sector;   // included

    u64_t disk_start_sector; // in disk_sector_size
    u64_t disk_end_sector;   // included
}ventoy_img_chunk;

typedef struct ventoy_disk_map
{
    u64_t img_start_sector;
    u64_t img_end_sector;
    u64_t disk_start_sector;
    u64_t disk_end_sector;
}ventoy_disk_map;
#pragma pack()

static int verbose = 0;
static u64_t g_iso_file_size = 0;
static int g_img_map_num = 0;
static ventoy_disk_map *g_img_map = NULL;

static ventoy_disk_map * vtoydm_get_img_map_data(const char *img_map_file, int *plen)
{
    int i;
    int len;
    int rc = 1;
    u64_t sector_num;
    FILE *fp = NULL;
    ventoy_img_chunk *chunk = NULL;
    ventoy_disk_map *map = NULL;
    
    fp = fopen(img_map_file, "rb");
    if (NULL == fp)
    {
        fprintf(stderr, "Failed to open file %s\n", img_map_file);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    len = (int)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    chunk = (ventoy_img_chunk *)malloc(len);
    if (NULL == chunk)
    {
        fprintf(stderr, "Failed to malloc memory len:%d\n", len);
        goto end;
    }

    if (fread(chunk, 1, len, fp) != len)
    {
        fprintf(stderr, "Failed to read file\n");
        goto end;
    }

    if (len % sizeof(ventoy_img_chunk))
    {
        fprintf(stderr, "image map file size %d is not aligned with %d\n", 
                len, (int)sizeof(ventoy_img_chunk));
        goto end;
    }

    map = (ventoy_disk_map *)malloc((len / sizeof(ventoy_img_chunk)) * sizeof(ventoy_disk_map));
    if (NULL == map)
    {
        fprintf(stderr, "Failed to malloc memory\n");
        goto end;
    }
    
    for (i = 0; i < len / sizeof(ventoy_img_chunk); i++)
    {
        sector_num = chunk[i].img_end_sector - chunk[i].img_start_sector + 1;
        g_iso_file_size += sector_num * 2048;
        
        map[i].img_start_sector = chunk[i].img_start_sector << 2;
        map[i].img_end_sector = (chunk[i].img_end_sector << 2) + 3;
        map[i].disk_start_sector = chunk[i].disk_start_sector;
        map[i].disk_end_sector = chunk[i].disk_end_sector;
    }

    rc = 0;
end:
    fclose(fp);

    if (chunk)
    {
        free(chunk);
        chunk = NULL;
    }

    *plen = len;
    return map;
}

static void parse_img_chunk(const char *img_map_file)
{
    int len;

    g_img_map = vtoydm_get_img_map_data(img_map_file, &len);
    if (g_img_map)
    {
        g_img_map_num = len / sizeof(ventoy_img_chunk);
    }
}

static u64_t get_disk_sector(u64_t lba)
{
    int i;
    ventoy_disk_map *cur = g_img_map;
    
    for (i = 0; i < g_img_map_num; i++, cur++)
    {
        if (lba >= cur->img_start_sector && lba <= cur->img_end_sector)
        {
            return (lba - cur->img_start_sector) + cur->disk_start_sector;
        }
    }

    return 0;
}

int getsec(int fd, uchar *place, vlong lba, int nsec)
{
    int i;
    int count = 0;
    u64_t last_sector;
    u64_t sector;

    count = 1;
    last_sector = get_disk_sector((u64_t)lba);
    
    for (i = 1; i < nsec; i++)
    {
        sector = get_disk_sector((u64_t)(lba + i));
        if (sector == (last_sector + count))
        {
            count++;
        }
        else
        {
            lseek(fd, last_sector * 512, SEEK_SET);
            read(fd, place, count * 512);

            last_sector = sector;
            count = 1;
        }
    }

    lseek(fd, last_sector * 512, SEEK_SET);
    read(fd, place, count * 512);

	return nsec * 512;
}
// read only
int putsec(int fd, uchar *place, vlong lba, int nsec)
{
    return nsec * 512;
}


void
aoead(int fd)	// advertise the virtual blade
{
	uchar buf[2000];
	Conf *p;
	int i;

	p = (Conf *)buf;
	memset(p, 0, sizeof *p);
	memset(p->h.dst, 0xff, 6);
	memmove(p->h.src, mac, 6);
	p->h.type = htons(0x88a2);
	p->h.flags = Resp;
	p->h.maj = htons(shelf);
	p->h.min = slot;
	p->h.cmd = Config;
	p->bufcnt = htons(bufcnt);
	p->scnt = maxscnt = (getmtu(sfd, ifname) - sizeof (Ata)) / 512;
	p->firmware = htons(FWV);
	p->vercmd = 0x10 | Qread;
	memcpy(p->data, config, nconfig);
	p->len = htons(nconfig);
	if (nmasks == 0)
	if (putpkt(fd, buf, sizeof *p - sizeof p->data + nconfig) == -1) {
		perror("putpkt aoe id");
		return;
	}
	for (i=0; i<nmasks; i++) {
		memcpy(p->h.dst, &masks[i*Alen], Alen);
		if (putpkt(fd, buf, sizeof *p - sizeof p->data + nconfig) == -1)
			perror("putpkt aoe id");
	}
}

int
isbcast(uchar *ea)
{
	uchar *b = (uchar *)"\377\377\377\377\377\377";

	return memcmp(ea, b, 6) == 0;
}

long long
getlba(uchar *p)
{
	vlong v;
	int i;

	v = 0;
	for (i = 0; i < 6; i++)
		v |= (vlong)(*p++) << i * 8;
	return v;
}

int
aoeata(Ata *p, int pktlen)	// do ATA reqeust
{
	Ataregs r;
	int len = 60;
	int n;

	r.lba = getlba(p->lba);
	r.sectors = p->sectors;
	r.feature = p->err;
	r.cmd = p->cmd;
	if (r.cmd != 0xec)
	if (!rrok(p->h.src)) {
		p->h.flags |= Error;
		p->h.error = Res;
		return len;
	}
	if (atacmd(&r, (uchar *)(p+1), maxscnt*512, pktlen - sizeof(*p)) < 0) {
		p->h.flags |= Error;
		p->h.error = BadArg;
		return len;
	}
	if (!(p->aflag & Write))
	if ((n = p->sectors)) {
		n -= r.sectors;
		len = sizeof (Ata) + (n*512);
	}
	p->sectors = r.sectors;
	p->err = r.err;
	p->cmd = r.status;
	return len;
}

#define QCMD(x) ((x)->vercmd & 0xf)

// yes, this makes unnecessary copies.

int
confcmd(Conf *p, int payload)	// process conf request
{
	int len;

	len = ntohs(p->len);
	if (QCMD(p) != Qread)
	if (len > Nconfig || len > payload)
		return 0;	// if you can't play nice ...
	switch (QCMD(p)) {
	case Qtest:
		if (len != nconfig)
			return 0;
		// fall thru
	case Qprefix:
		if (len > nconfig)
			return 0;
		if (memcmp(config, p->data, len))
			return 0;
		// fall thru
	case Qread:
		break;
	case Qset:
		if (nconfig)
		if (nconfig != len || memcmp(config, p->data, len)) {
			p->h.flags |= Error;
			p->h.error = ConfigErr;
			break;
		}
		// fall thru
	case Qfset:
		nconfig = len;
		memcpy(config, p->data, nconfig);
		break;
	default:
		p->h.flags |= Error;
		p->h.error = BadArg;
	}
	memmove(p->data, config, nconfig);
	p->len = htons(nconfig);
	p->bufcnt = htons(bufcnt);
	p->scnt = maxscnt = (getmtu(sfd, ifname) - sizeof (Ata)) / 512;
	p->firmware = htons(FWV);
	p->vercmd = 0x10 | QCMD(p);	// aoe v.1
	return nconfig + sizeof *p - sizeof p->data;
}

static int
aoesrr(Aoesrr *sh, int len)
{
	uchar *m, *e;
	int n;

	e = (uchar *) sh + len;
	m = (uchar *) sh + Nsrrhdr;
	switch (sh->rcmd) {
	default:
e:		sh->h.error = BadArg;
		sh->h.flags |= Error;
		break;
	case 1:	// set
		if (!rrok(sh->h.src)) {
			sh->h.error = Res;
			sh->h.flags |= Error;
			break;
		}
	case 2:	// force set
		n = sh->nmacs * 6;
		if (e < m + n)
			goto e;
		nsrr = sh->nmacs;
		memmove(srr, m, n);
	case 0:	// read
		break;
	}
	sh->nmacs = nsrr;
	n = nsrr * 6;
	memmove(m, srr, n);
	return Nsrrhdr + n;
}

static int
addmask(uchar *ea)
{

	uchar *p, *e;

	p = masks;
	e = p + nmasks;
	for (; p<e; p += 6)
		if (!memcmp(p, ea, 6))
			return 2;
	if (nmasks >= Nmasks)
		return 0;
	memmove(p, ea, 6);
	nmasks++;
	return 1;
}

static void
rmmask(uchar *ea)
{
	uchar *p, *e;

	p = masks;
	e = p + nmasks;
	for (; p<e; p+=6)
		if (!memcmp(p, ea, 6)) {
			memmove(p, p+6, e-p-6);
			nmasks--;
			return;
		}
}

static int
aoemask(Aoemask *mh, int len)
{
	Mdir *md, *mdi, *mde;
	int i, n;

	n = 0;
	md = mdi = (Mdir *) ((uchar *)mh + Nmaskhdr);
	switch (mh->cmd) {
	case Medit:
		mde = md + mh->nmacs;
		for (; md<mde; md++) {
			switch (md->cmd) {
			case MDdel:
				rmmask(md->mac);
				continue;
			case MDadd:
				if (addmask(md->mac))
					continue;
				mh->merror = MEfull;
				mh->nmacs = md - mdi;
				goto e;
			case MDnop:
				continue;
			default:
				mh->merror = MEbaddir;
				mh->nmacs = md - mdi;
				goto e;
			}
		}
		// success.  fall thru to return list
	case Mread:
		md = mdi;
		for (i=0; i<nmasks; i++) {
			md->res = md->cmd = 0;
			memmove(md->mac, &masks[i*6], 6);
			md++;
		}
		mh->merror = 0;
		mh->nmacs = nmasks;
		n = sizeof *md * nmasks;
		break;
	default:
		mh->h.flags |= Error;
		mh->h.error = BadArg;
	}
e:	return n + Nmaskhdr;
}

void
doaoe(Aoehdr *p, int n)
{
	int len;

	switch (p->cmd) {
	case ATAcmd:
		if (n < Natahdr)
			return;
		len = aoeata((Ata*)p, n);
		break;
	case Config:
		if (n < Ncfghdr)
			return;
		len = confcmd((Conf *)p, n);
		break;
	case Mask:
		if (n < Nmaskhdr)
			return;
		len = aoemask((Aoemask *)p, n);
		break;
	case Resrel:
		if (n < Nsrrhdr)
			return;
		len = aoesrr((Aoesrr *)p, n);
		break;
	default:
		p->error = BadCmd;
		p->flags |= Error;
		len = n;
		break;
	}
	if (len <= 0)
		return;
	memmove(p->dst, p->src, 6);
	memmove(p->src, mac, 6);
	p->maj = htons(shelf);
	p->min = slot;
	p->flags |= Resp;
	if (putpkt(sfd, (uchar *) p, len) == -1) {
		perror("write to network");
		exit(1);
	}
}

void
aoe(void)
{
	Aoehdr *p;
	uchar *buf;
	int n, sh;
	long pagesz;
	enum { bufsz = 1<<16, };

	if ((pagesz = sysconf(_SC_PAGESIZE)) < 0) {
		perror("sysconf");
		exit(1);
	}        
	if ((buf = malloc(bufsz + pagesz)) == NULL) {
		perror("malloc");
		exit(1);
	}
	n = (size_t) buf + sizeof(Ata);
	if (n & (pagesz - 1))
		buf += pagesz - (n & (pagesz - 1));

	aoead(sfd);

	for (;;) {
		n = getpkt(sfd, buf, bufsz);
		if (n < 0) {
			perror("read network");
			exit(1);
		}
		if (n < sizeof(Aoehdr))
			continue;
		p = (Aoehdr *) buf;
		if (ntohs(p->type) != 0x88a2)
			continue;
		if (p->flags & Resp)
			continue;
		sh = ntohs(p->maj);
		if (sh != shelf && sh != (ushort)~0)
			continue;
		if (p->min != slot && p->min != (uchar)~0)
			continue;
		if (nmasks && !maskok(p->src))
			continue;
		doaoe(p, n);
	}
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-b bufcnt] [-o offset] [-l length] [-d ] [-s] [-r] [ -m mac[,mac...] ] shelf slot netif filename\n", 
		progname);
	exit(1);
}

/* parseether from plan 9 */
int
parseether(uchar *to, char *from)
{
	char nip[4];
	char *p;
	int i;

	p = from;
	for(i = 0; i < 6; i++){
		if(*p == 0)
			return -1;
		nip[0] = *p++;
		if(*p == 0)
			return -1;
		nip[1] = *p++;
		nip[2] = 0;
		to[i] = strtoul(nip, 0, 16);
		if(*p == ':')
			p++;
	}
	return 0;
}

void
setmask(char *ml)
{
	char *p;
	int n;

	for (; ml; ml=p) {
		p = strchr(ml, ',');
		if (p)
			*p++ = '\0';
		n = parseether(&masks[nmasks*Alen], ml);
		if (n < 0)
			fprintf(stderr, "ignoring mask %s, parseether failure\n", ml);
		else
			nmasks++;
	}
}

int
maskok(uchar *ea)
{
	int i, ok = 0;

	for (i=0; !ok && i<nmasks; i++)
		ok = memcmp(ea, &masks[i*Alen], Alen) == 0;
	return ok;
}

int
rrok(uchar *ea)
{
	int i, ok = 0;

	if (nsrr == 0)
		return 1;
	for (i=0; !ok && i<nsrr; i++)
		ok = memcmp(ea, &srr[i*Alen], Alen) == 0;
	return ok;
}

void
setserial(int sh, int sl)
{
	char h[32];

	h[0] = 0;
	gethostname(h, sizeof h);
	snprintf(serial, Nserial, "%d.%d:%.*s", sh, sl, (int) sizeof h, h);
}

int
main(int argc, char **argv)
{
	int ch, omode = 0, readonly = 0;
	vlong length = 0;
	char *end;
    char filepath[300] = {0};

    /* Avoid to be killed by systemd */
    if (access("/etc/initrd-release", F_OK) >= 0)
	{		
		argv[0][0] = '@';
	}

	bufcnt = Bufcount;
	offset = 0;
	setbuf(stdin, NULL);
	progname = *argv;
	while ((ch = getopt(argc, argv, "b:dsrm:f:tv::o:l:")) != -1) {
		switch (ch) {
		case 'b':
			bufcnt = atoi(optarg);
			break;
		case 'd':
#ifdef O_DIRECT
			omode |= O_DIRECT;
#endif
			break;
		case 's':
			omode |= O_SYNC;
			break;
		case 'r':
			readonly = 1;
			break;
		case 'm':
			setmask(optarg);
			break;
        case 't':
            return 0;
        case 'v':
            verbose = 1;
            break;
        case 'f':
            strncpy(filepath, optarg, sizeof(filepath) - 1);
            break;
		case 'o':
			offset = strtoll(optarg, &end, 0);
			if (end == optarg || offset < 0)
				usage();
			break;
		case 'l':
			length = strtoll(optarg, &end, 0);
			if (end == optarg || length < 1)
				usage();
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 4 || bufcnt <= 0)
		usage();
	omode |= readonly ? O_RDONLY : O_RDWR;
    parse_img_chunk(filepath);
	bfd = open(argv[3], omode);
	if (bfd == -1) {
		perror("open");
		exit(1);
	}
	shelf = atoi(argv[0]);
	slot = atoi(argv[1]);
	setserial(shelf, slot);
	size = g_iso_file_size; //getsize(bfd);
	size /= 512;
	if (size <= offset) {
                if (offset)
                        fprintf(stderr,
                                "Offset %lld too large for %lld-sector export\n",
                                offset,
                                size);
                else
                        fputs("0-sector file size is too small\n", stderr);
		exit(1);
	}
	size -= offset;
	if (length) {
		if (length > size) {
			fprintf(stderr, "Length %llu too big - exceeds size of file!\n", offset);
			exit(1);
		}
		size = length;
	}
	ifname = argv[2];
	sfd = dial(ifname, bufcnt);
	if (sfd < 0)
		return 1;
	getea(sfd, ifname, mac);

    if (verbose) {
        printf("pid %ld: e%d.%d, %lld sectors %s\n",
		    (long) getpid(), shelf, slot, size,
		    readonly ? "O_RDONLY" : "O_RDWR");
    }
    
	fflush(stdout);
	atainit();
	aoe();
	return 0;
}

