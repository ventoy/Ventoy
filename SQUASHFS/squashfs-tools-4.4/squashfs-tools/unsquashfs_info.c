/*
 * Create a squashfs filesystem.  This is a highly compressed read only
 * filesystem.
 *
 * Copyright (c) 2013
 * Phillip Lougher <phillip@squashfs.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * unsquashfs_info.c
 */

#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "squashfs_fs.h"
#include "unsquashfs.h"
#include "error.h"

static int silent = 0;
char *pathname = NULL;

pthread_t info_thread;


void disable_info()
{
	if(pathname)
		free(pathname);

	pathname = NULL;
}


void update_info(char *name)
{
	if(pathname)
		free(pathname);

	pathname = name;
}


void dump_state()
{
	disable_progress_bar();

	printf("Queue and cache status dump\n");
	printf("===========================\n");

	printf("file buffer read queue (main thread -> reader thread)\n");
	dump_queue(to_reader);

	printf("file buffer decompress queue (reader thread -> inflate"
							" thread(s))\n");
	dump_queue(to_inflate);

	printf("file buffer write queue (main thread -> writer thread)\n");
	dump_queue(to_writer);

	printf("\nbuffer cache (uncompressed blocks and compressed blocks "
							"'in flight')\n");
	dump_cache(data_cache);

	printf("fragment buffer cache (uncompressed frags and compressed"
						" frags 'in flight')\n");
	dump_cache(fragment_cache);

	enable_progress_bar();
}


void *info_thrd(void *arg)
{
	sigset_t sigmask;
	struct timespec timespec = { .tv_sec = 1, .tv_nsec = 0 };
	int sig, waiting = 0;

	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGQUIT);
	sigaddset(&sigmask, SIGHUP);

	while(1) {
		if(waiting)
			sig = sigtimedwait(&sigmask, NULL, &timespec);
		else
			sig = sigwaitinfo(&sigmask, NULL);

		if(sig == -1) {
			switch(errno) {
			case EAGAIN:
				/* interval timed out */
				waiting = 0;
				/* FALLTHROUGH */
			case EINTR:
				/* if waiting, the wait will be longer, but
				   that's OK */
				continue;
			default:
				BAD_ERROR("sigtimedwait/sigwaitinfo failed "
					"because %s\n", strerror(errno));
			}
		}

		if(sig == SIGQUIT && !waiting) {
			if(pathname)
				INFO("%s\n", pathname);

			/* set one second interval period, if ^\ received
			   within then, dump queue and cache status */
			waiting = 1;
		} else
			dump_state();
	}
}


void init_info()
{
	pthread_create(&info_thread, NULL, info_thrd, NULL);
}
