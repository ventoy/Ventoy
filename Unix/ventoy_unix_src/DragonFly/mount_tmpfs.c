/*	$NetBSD: mount_tmpfs.c,v 1.24 2008/08/05 20:57:45 pooka Exp $	*/

/*
 * Copyright (c) 2005, 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <vfs/tmpfs/tmpfs_mount.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <mntopts.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <inttypes.h>
#include <libutil.h>

//#include "defs.h"
#include "mount_tmpfs.h"

/* --------------------------------------------------------------------- */

#define MOPT_TMPFSOPTS	\
	{ "gid=",	0,	MNT_GID, 1},	\
	{ "uid=",	0,	MNT_UID, 1},	\
	{ "mode=",	0,	MNT_MODE, 1},	\
	{ "inodes=",	0,	MNT_INODES, 1},	\
	{ "size=",	0,	MNT_SIZE, 1},	\
	{ "maxfilesize=",	0,	MNT_MAXFSIZE, 1}


static const struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_TMPFSOPTS,
	MOPT_NULL
};

static int Cflag;

/* --------------------------------------------------------------------- */

static gid_t	a_gid(char *);
static uid_t	a_uid(char *);
static mode_t	a_mask(char *);
static int64_t a_number(char *s);
static void	usage(void) __dead2;

/* --------------------------------------------------------------------- */

void
mount_tmpfs_parseargs(int argc, char *argv[],
	struct tmpfs_mount_info *args, int *mntflags,
	char *canon_dev, char *canon_dir)
{
	int gidset, modeset, uidset; /* Ought to be 'bool'. */
	int ch;
	gid_t gid;
	uid_t uid;
	mode_t mode;
	struct stat sb;
	int extend_flags = 0;
	char *ptr, *delim;

	/* Set default values for mount point arguments. */
	memset(args, 0, sizeof(*args));
	args->ta_version = TMPFS_ARGS_VERSION;
	args->ta_size_max = 0;
	args->ta_nodes_max = 0;
	args->ta_maxfsize_max = 0;
	*mntflags = 0;

	gidset = 0; gid = 0;
	uidset = 0; uid = 0;
	modeset = 0; mode = 0;

	optind = optreset = 1;
	while ((ch = getopt(argc, argv, "Cf:g:m:n:o:s:u:")) != -1 ) {
		switch (ch) {
		case 'C':
			Cflag = 1;
			break;
		case 'f':
			args->ta_maxfsize_max = a_number(optarg);
			break;

		case 'g':
			gid = a_gid(optarg);
			gidset = 1;
			break;

		case 'm':
			mode = a_mask(optarg);
			modeset = 1;
			break;

		case 'n':
			args->ta_nodes_max = a_number(optarg);
			break;

		case 'o':
			getmntopts(optarg, mopts, mntflags, &extend_flags);
			if (extend_flags & MNT_GID) {
				ptr = strstr(optarg, "gid=");
				if(ptr) {
					delim = strstr(ptr, ",");
					if (delim) {
						*delim = '\0';
						gid = a_gid(ptr + 4);
						*delim = ',';
					} else
						gid = a_gid(ptr + 4);
					gidset = 1;
				}
				extend_flags ^= MNT_GID;
			}
			if (extend_flags & MNT_UID) {
				ptr = strstr(optarg, "uid=");
				if(ptr) {
					delim = strstr(ptr, ",");
					if (delim) {
						*delim = '\0';
						uid = a_uid(ptr + 4);
						*delim = ',';
					} else
						uid = a_uid(ptr + 4);
					uidset = 1;
				}
				extend_flags ^= MNT_UID;
			}
			if (extend_flags & MNT_MODE) {
				ptr = strstr(optarg, "mode=");
				if(ptr) {
					delim = strstr(ptr, ",");
					if (delim) {
						*delim = '\0';
						mode = a_mask(ptr + 5);
						*delim = ',';
					} else
						mode = a_mask(ptr + 5);
					modeset = 1;
				}
				extend_flags ^= MNT_MODE;
			}
			if (extend_flags & MNT_INODES) {
				ptr = strstr(optarg, "inodes=");
				if(ptr) {
					delim = strstr(ptr, ",");
					if (delim) {
						*delim = '\0';
						args->ta_nodes_max = a_number(ptr + 7);
						*delim = ',';
					} else
						args->ta_nodes_max = a_number(ptr + 7);
				}
				extend_flags ^= MNT_INODES;
			}
			if (extend_flags & MNT_SIZE) {
				ptr = strstr(optarg, "size=");
				if(ptr) {
					delim = strstr(ptr, ",");
					if (delim) {
						*delim = '\0';
						args->ta_size_max = a_number(ptr + 5);
						*delim = ',';
					} else
						args->ta_size_max = a_number(ptr + 5);
				}
				extend_flags ^= MNT_SIZE;
			}
			if (extend_flags & MNT_MAXFSIZE) {
				ptr = strstr(optarg, "maxfilesize=");
				if(ptr) {
					delim = strstr(ptr, ",");
					if (delim) {
						*delim = '\0';
						args->ta_maxfsize_max = a_number(ptr + 12);
						*delim = ',';
					} else
						args->ta_maxfsize_max = a_number(ptr + 12);
				}
				extend_flags ^= MNT_MAXFSIZE;
			}
			break;

		case 's':
			args->ta_size_max = a_number(optarg);
			break;

		case 'u':
			uid = a_uid(optarg);
			uidset = 1;
			break;

		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	strlcpy(canon_dev, argv[0], MAXPATHLEN);
	strlcpy(canon_dir, argv[1], MAXPATHLEN);

	if (stat(canon_dir, &sb) == -1)
		err(EXIT_FAILURE, "cannot stat `%s'", canon_dir);

	args->ta_root_uid = uidset ? uid : sb.st_uid;
	args->ta_root_gid = gidset ? gid : sb.st_gid;
	args->ta_root_mode = modeset ? mode : sb.st_mode;
}

/* --------------------------------------------------------------------- */

static gid_t
a_gid(char *s)
{
	struct group *gr;
	char *gname;
	gid_t gid;

	if ((gr = getgrnam(s)) != NULL)
		gid = gr->gr_gid;
	else {
		for (gname = s; *s && isdigit(*s); ++s);
		if (!*s)
			gid = atoi(gname);
		else
			errx(EX_NOUSER, "unknown group id: %s", gname);
	}
	return (gid);
}

static uid_t
a_uid(char *s)
{
	struct passwd *pw;
	char *uname;
	uid_t uid;

	if ((pw = getpwnam(s)) != NULL)
		uid = pw->pw_uid;
	else {
		for (uname = s; *s && isdigit(*s); ++s);
		if (!*s)
			uid = atoi(uname);
		else
			errx(EX_NOUSER, "unknown user id: %s", uname);
	}
	return (uid);
}

static mode_t
a_mask(char *s)
{
	int done, rv = 0;
	char *ep;

	done = 0;
	if (*s >= '0' && *s <= '7') {
		done = 1;
		rv = strtol(s, &ep, 8);
	}
	if (!done || rv < 0 || *ep)
		errx(EX_USAGE, "invalid file mode: %s", s);
	return (rv);
}

static int64_t
a_number(char *s)
{
	int64_t rv = 0;

	if (dehumanize_number(s, &rv) < 0 || rv < 0)
		errx(EX_USAGE, "bad number for option: %s", s);
	return (rv);
}

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: %s [-C] [-g group] [-m mode] [-n nodes] [-o options] [-s size]\n"
	    "           [-u user] [-f maxfilesize] tmpfs mountpoint\n", getprogname());
	exit(1);
}

/* --------------------------------------------------------------------- */

int
mount_tmpfs(int argc, char *argv[])
{
	struct tmpfs_mount_info args;
	char canon_dev[MAXPATHLEN], canon_dir[MAXPATHLEN];
	int mntflags;
	struct vfsconf vfc;
	int error;
	//fsnode_t copyroot = NULL;
	//fsnode_t copyhlinks = NULL;

	mount_tmpfs_parseargs(argc, argv, &args, &mntflags,
	    canon_dev, canon_dir);

	error = getvfsbyname("tmpfs", &vfc);
	if (error && vfsisloadable("tmpfs")) {
		if(vfsload("tmpfs"))
			err(EX_OSERR, "vfsload(%s)", "tmpfs");
		endvfsent();
		error = getvfsbyname("tmpfs", &vfc);
	}
	if (error)
		errx(EX_OSERR, "%s filesystem not available", "tmpfs");

	//if (Cflag)
	//	copyroot = FSCopy(&copyhlinks, canon_dir);

	if (mount(vfc.vfc_name, canon_dir, mntflags, &args) == -1)
		err(EXIT_FAILURE, "tmpfs on %s", canon_dir);

	//if (Cflag)
	//	FSPaste(canon_dir, copyroot, copyhlinks);

	return EXIT_SUCCESS;
}

#ifndef MOUNT_NOMAIN
int
main(int argc, char *argv[])
{
	setprogname(argv[0]);
	return mount_tmpfs(argc, argv);
}
#endif
