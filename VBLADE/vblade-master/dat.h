/* dat.h: include file for vblade AoE target */

#define	nil	((void *)0)
/*
 *	tunable variables
 */

enum {
	VBLADE_VERSION		= 24,

	// Firmware version
	FWV			= 0x4000 + VBLADE_VERSION,
};

#undef major
#undef minor
#undef makedev

#define	major(x)		((x) >> 24 & 0xFF)
#define	minor(x)		((x) & 0xffffff)
#define	makedev(x, y)	((x) << 24 | (y))

typedef unsigned char uchar;
//typedef unsigned short ushort;
#ifdef __FreeBSD__
typedef unsigned long ulong;
#else
//typedef unsigned long ulong;
#endif
typedef long long vlong;

typedef struct Aoehdr Aoehdr;
typedef struct Ata Ata;
typedef struct Conf Conf;
typedef struct Ataregs Ataregs;
typedef struct Mdir Mdir;
typedef struct Aoemask Aoemask;
typedef struct Aoesrr Aoesrr;

struct Ataregs
{
	vlong	lba;
	uchar	cmd;
	uchar	status;
	uchar	err;
	uchar	feature;
	uchar	sectors;
};

struct Aoehdr
{
	uchar	dst[6];
	uchar	src[6];
	ushort	type;
	uchar	flags;
	uchar	error;
	ushort	maj;
	uchar	min;
	uchar	cmd;
	uchar	tag[4];
};

struct Ata
{
	Aoehdr	h;
	uchar	aflag;
	uchar	err;
	uchar	sectors;
	uchar	cmd;
	uchar	lba[6];
	uchar	resvd[2];
};

struct Conf
{
	Aoehdr	h;
	ushort	bufcnt;
	ushort	firmware;
	uchar	scnt;
	uchar	vercmd;
	ushort	len;
	uchar	data[1024];
};

// mask directive
struct Mdir {
	uchar res;
	uchar cmd;
	uchar mac[6];
};

struct Aoemask {
	Aoehdr h;
	uchar res;
	uchar cmd;
	uchar merror;
	uchar nmacs;
//	struct Mdir m[0];
};

struct Aoesrr {
	Aoehdr h;
	uchar rcmd;
	uchar nmacs;
//	uchar mac[6][nmacs];
};

enum {
	AoEver = 1,

	ATAcmd = 0,		// command codes
	Config,
	Mask,
	Resrel,

	Resp = (1<<3),		// flags
	Error = (1<<2),

	BadCmd = 1,
	BadArg,
	DevUnavailable,
	ConfigErr,
	BadVersion,
	Res,

	Write = (1<<0),
	Async = (1<<1),
	Device = (1<<4),
	Extend = (1<<6),

	Qread = 0,
	Qtest,
	Qprefix,
	Qset,
	Qfset,

	Nretries = 3,
	Nconfig = 1024,

	Bufcount = 16,

	/* mask commands */
	Mread= 0,
	Medit,

	/* mask directives */
	MDnop= 0,
	MDadd,
	MDdel,

	/* mask errors */
	MEunspec= 1,
	MEbaddir,
	MEfull,

	/* header sizes, including aoe hdr */
	Naoehdr= 24,
	Natahdr= Naoehdr + 12,
	Ncfghdr= Naoehdr + 8,
	Nmaskhdr= Naoehdr + 4,
	Nsrrhdr= Naoehdr + 2,

	Nserial= 20,
};

int	shelf, slot;
ulong	aoetag;
uchar	mac[6];
int	bfd;		// block file descriptor
int	sfd;		// socket file descriptor
vlong	size;		// size of vblade
vlong	offset;
char	*progname;
char	serial[Nserial+1];
