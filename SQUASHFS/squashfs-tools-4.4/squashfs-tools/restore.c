/*
 * Create a squashfs filesystem.  This is a highly compressed read only
 * filesystem.
 *
 * Copyright (c) 2013, 2014, 2019
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
 * restore.c
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

#include "caches-queues-lists.h"
#include "squashfs_fs.h"
#include "mksquashfs.h"
#include "error.h"
#include "progressbar.h"
#include "info.h"

#define FALSE 0
#define TRUE 1

extern pthread_t reader_thread, writer_thread, main_thread, order_thread;
extern pthread_t *deflator_thread, *frag_deflator_thread, *frag_thread;
extern struct queue *to_deflate, *to_writer, *to_frag, *to_process_frag;
extern struct seq_queue *to_main, *to_order;
extern void restorefs();
extern int processors;
extern int reproducible;

static int interrupted = 0;
static pthread_t restore_thread;

void *restore_thrd(void *arg)
{
	sigset_t sigmask, old_mask;
	int i, sig;

	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGTERM);
	sigaddset(&sigmask, SIGUSR1);
	pthread_sigmask(SIG_BLOCK, &sigmask, &old_mask);

	while(1) {
		sigwait(&sigmask, &sig);

		if((sig == SIGINT || sig == SIGTERM) && !interrupted) {
			ERROR("Interrupting will restore original "
				"filesystem!\n");
                	ERROR("Interrupt again to quit\n");
			interrupted = TRUE;
			continue;
		}

		/* kill main thread/worker threads and restore */
		set_progressbar_state(FALSE);
		disable_info();

		/* first kill the reader thread */
		pthread_cancel(reader_thread);
		pthread_join(reader_thread, NULL);

		/*
		 * then flush the reader to deflator thread(s) output queue.
		 * The deflator thread(s) will idle
		 */
		queue_flush(to_deflate);

		/* now kill the deflator thread(s) */
		for(i = 0; i < processors; i++)
			pthread_cancel(deflator_thread[i]);
		for(i = 0; i < processors; i++)
			pthread_join(deflator_thread[i], NULL);

		/*
		 * then flush the reader to process fragment thread(s) output
		 * queue.  The process fragment thread(s) will idle
		 */
		queue_flush(to_process_frag);

		/* now kill the process fragment thread(s) */
		for(i = 0; i < processors; i++)
			pthread_cancel(frag_thread[i]);
		for(i = 0; i < processors; i++)
			pthread_join(frag_thread[i], NULL);

		/*
		 * then flush the reader/deflator/process fragment to main
		 * thread output queue.  The main thread will idle
		 */
		seq_queue_flush(to_main);

		/* now kill the main thread */
		pthread_cancel(main_thread);
		pthread_join(main_thread, NULL);

		/* then flush the main thread to fragment deflator thread(s)
		 * queue.  The fragment deflator thread(s) will idle
		 */
		queue_flush(to_frag);

		/* now kill the fragment deflator thread(s) */
		for(i = 0; i < processors; i++)
			pthread_cancel(frag_deflator_thread[i]);
		for(i = 0; i < processors; i++)
			pthread_join(frag_deflator_thread[i], NULL);

		if(reproducible) {
			/* then flush the fragment deflator_threads(s)
			 * to frag orderer thread.  The frag orderer
			 * thread will idle
			 */
			seq_queue_flush(to_order);

			/* now kill the frag orderer thread */
			pthread_cancel(order_thread);
			pthread_join(order_thread, NULL);
		}

		/*
		 * then flush the main thread/fragment deflator thread(s)
		 * to writer thread queue.  The writer thread will idle
		 */
		queue_flush(to_writer);

		/* now kill the writer thread */
		pthread_cancel(writer_thread);
		pthread_join(writer_thread, NULL);

		TRACE("All threads cancelled\n");

		restorefs();
	}
}


pthread_t *init_restore_thread()
{
	pthread_create(&restore_thread, NULL, restore_thrd, NULL);
	return &restore_thread;
}
