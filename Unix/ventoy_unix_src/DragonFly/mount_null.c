/*
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) Copyright (c) 1992, 1993, 1994 The Regents of the University of California.  All rights reserved.
 * @(#)mount_null.c	8.6 (Berkeley) 4/26/95
 * $FreeBSD: src/sbin/mount_null/mount_null.c,v 1.13 1999/10/09 11:54:11 phk Exp $
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <vfs/nullfs/null.h>

#include <err.h>
#include <mntopts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_UPDATE,
	MOPT_NULL
};

static void	usage(void) __dead2;

int
mount_null(const char *src, const char *dst)
{
	struct null_args args;
	int ch, mntflags;
	char source[MAXPATHLEN];
	char target[MAXPATHLEN];
	struct vfsconf vfc;
	int error;

	bzero(&args, sizeof(args));
	mntflags = 0;

    args.target = target;
	checkpath(src, target);
	checkpath(dst, source);

	/*
	 * Mount points that did not use distinct paths (e.g. / on /mnt)
	 * used to be disallowed because mount linkages were stored in
	 * vnodes and would lead to endlessly recursive trees.  DragonFly
	 * stores mount linkages in the namecache topology and does not
	 * have this problem, so paths no longer need to be distinct.
	 */

	error = getvfsbyname("null", &vfc);
	if (error && vfsisloadable("null")) {
		if(vfsload("null"))
			err(EX_OSERR, "vfsload(null)");
		endvfsent();
		error = getvfsbyname("null", &vfc);
	}
	if (error)
		errx(EX_OSERR, "null/loopback filesystem is not available");

	if (mount(vfc.vfc_name, source, mntflags, &args))
		err(1, NULL);
    
	return 0;
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: mount_null [-o options] target_fs mount_point\n");
	fprintf(stderr,
	    "       mount_null -u [-o options] mount_point\n");
	exit(1);
}
