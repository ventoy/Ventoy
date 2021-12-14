/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Donn Seeley at Berkeley Software Design, Inc.
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
 * @(#) Copyright (c) 1991, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)init.c	8.1 (Berkeley) 7/15/93
 * $FreeBSD: src/sbin/init/init.c,v 1.38.2.8 2001/10/22 11:27:32 des Exp $
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <db.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <ttyent.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/mount.h>
#include <err.h>

#include <stdarg.h>
#include <vtutil.h>

int boot_verbose = 0;

int prepare_dmtable(void);
int mount_cd9660(char *dev, char *dir);
int mount_null(const char *src, const char *dst);
int mount_tmpfs(int argc, char *argv[]);

static int setctty(const char *name)
{
	int fd;

	revoke(name);
	if ((fd = open(name, O_RDWR)) == -1) {
		exit(1);
	}

	if (login_tty(fd) == -1) {
		exit(1);
	}

	return fd;
}

static void ventoy_init(void)
{
	pid_t pid, wpid;
	int status, error;
    char arg0[MAXPATHLEN];
    char arg1[MAXPATHLEN];
    char arg2[MAXPATHLEN];
	char *argv[8];
	struct sigaction sa;

    /* step1: mount tmpfs */
    vdebug("[VTOY] step 1: mount tmpfs ...");
    strcpy(arg0, "mount_tmpfs");
    strcpy(arg1, "tmpfs");
    strcpy(arg2, "/tmp");
    argv[0] = arg0;
    argv[1] = arg1;
    argv[2] = arg2;
    argv[3] = NULL;
    error = mount_tmpfs(3, argv);
    vdebug(" %d\n", error);

    /* step 2: prepare dmtable */
    vdebug("[VTOY] step 2: prepare device-mapper table...\n");
    (void)prepare_dmtable();

    /* step 3: create device mapper */
    vdebug("[VTOY] step 3: create device-mapper ...\n");
	if ((pid = fork()) == 0) {
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_IGN;
		sigaction(SIGTSTP, &sa, NULL);
		sigaction(SIGHUP, &sa, NULL);

		argv[0] = "dmsetup";
		argv[1] = "create";
		argv[2] = "ventoy";
		argv[3] = "/tmp/dmtable";
		argv[4] = "--readonly";
		argv[5] = NULL;

		sigprocmask(SIG_SETMASK, &sa.sa_mask, NULL);
		execv("/sbin/dmsetup", __DECONST(char **, argv));
		exit(1); /* force single user mode */
	}

	do {
		wpid = waitpid(-1, &status, WUNTRACED);
	} while (wpid != pid);

    /* step 4: mount iso */
    vdebug("[VTOY] step 4: mount device-mapper ...");
    strcpy(arg0, "/dev/mapper/ventoy");
    strcpy(arg1, "/new_root");
    error = mount_cd9660(arg0, arg1);
    vdebug(" %d\n", error);

    /* step 5: mount devfs */
    vdebug("[VTOY] step 5: mount devfs ...");
    strcpy(arg0, "/dev");
    strcpy(arg1, "/new_root/dev");
    mount_null(arg0, arg1);
    vdebug(" %d\n", error);

    /* step 6: umount tmpfs */
    error = unmount("/tmp", 0);
    vdebug("[VTOY] step 6: unmount tmpfs %d\n", error);

    /* step 7: swich_root */
    vdebug("[VTOY] step 7: switch root ...\n");
}

int main(int argc __unused, char **argv)
{
	pid_t pid, wpid;
	int status, error;
    size_t varsize = sizeof(int);

	/* Dispose of random users. */
	if (getuid() != 0)
		errx(1, "%s", strerror(EPERM));

	/* Init is not allowed to die, it would make the kernel panic */
	signal(SIGTERM, SIG_IGN);

    if ((pid = fork()) == 0) {
    
        setctty(_PATH_CONSOLE);
        sysctlbyname("debug.bootverbose", &boot_verbose, &varsize, NULL, 0);

        vdebug("======= Ventoy Init Start ========\n");

    	ventoy_init();
        exit(1);	/* force single user mode */
	}

	do {
		wpid = waitpid(-1, &status, WUNTRACED);
	} while (wpid != pid);

    error = chdir("/new_root");
	if (error)
	    goto chroot_failed;

	error = chroot_kernel("/new_root");
	if (error)
	    goto chroot_failed;

	error = chroot("/new_root");
	if (error)
        goto chroot_failed;

	execv("/sbin/init", __DECONST(char **, argv));

	/* We failed to exec /sbin/init in the chroot, sleep forever */
chroot_failed:
	while(1) {
		sleep(3);
	};
	return 1;
}

