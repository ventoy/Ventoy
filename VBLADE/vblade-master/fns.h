// fns.h: function prototypes

// aoe.c

void	aoe(void);
void	aoeinit(void);
void	aoequery(void);
void	aoeconfig(void);
void	aoead(int);
void	aoeflush(int, int);
void	aoetick(void);
void	aoerequest(int, int, vlong, int, uchar *, int);
int	maskok(uchar *);
int	rrok(uchar *);

// ata.c

void	atainit(void);
int	atacmd(Ataregs *, uchar *, int, int);

// bpf.c

void *	create_bpf_program(int, int);
void	free_bpf_program(void *);

// os specific

int	dial(char *, int);
int	getea(int, char *, uchar *);
int	putsec(int, uchar *, vlong, int);
int	getsec(int, uchar *, vlong, int);
int	putpkt(int, uchar *, int);
int	getpkt(int, uchar *, int);
vlong	getsize(int);
int	getmtu(int, char *);
