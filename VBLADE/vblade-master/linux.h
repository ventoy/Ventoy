// linux.h: header for linux.c

typedef unsigned char uchar;
typedef long long vlong;

int	dial(char *);
int	getindx(int, char *);
int	getea(int, char *, uchar *);
int	getsec(int, uchar *, vlong, int);
int	putsec(int, uchar *, vlong, int);
vlong	getsize(int);
