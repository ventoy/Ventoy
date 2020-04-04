# makefile for vblade

# see README for others
PLATFORM=linux

prefix = /usr
sbindir = ${prefix}/sbin
sharedir = ${prefix}/share
mandir = ${sharedir}/man

O=aoe.o bpf.o ${PLATFORM}.o ata.o
CFLAGS += -Wall -g -O2
CC = gcc

vblade: $O
	${CC} -o vblade $O

aoe.o : aoe.c config.h dat.h fns.h makefile
	${CC} ${CFLAGS} -c $<

${PLATFORM}.o : ${PLATFORM}.c config.h dat.h fns.h makefile
	${CC} ${CFLAGS} -c $<

ata.o : ata.c config.h dat.h fns.h makefile
	${CC} ${CFLAGS} -c $<

bpf.o : bpf.c
	${CC} ${CFLAGS} -c $<

config.h : config/config.h.in makefile
	@if ${CC} ${CFLAGS} config/u64.c > /dev/null 2>&1; then \
	  sh -xc "cp config/config.h.in config.h"; \
	else \
	  sh -xc "sed 's!^//u64 !!' config/config.h.in > config.h"; \
	fi

clean :
	rm -f $O vblade

install : vblade vbladed
	install vblade ${sbindir}/
	install vbladed ${sbindir}/
	install vblade.8 ${mandir}/man8/
