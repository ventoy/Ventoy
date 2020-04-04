/*
 * Create a squashfs filesystem.  This is a highly compressed read only
 * filesystem.
 *
 * Copyright (c) 2014
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
 * process_fragments.c
 */

#include <pthread.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "caches-queues-lists.h"
#include "squashfs_fs.h"
#include "mksquashfs.h"
#include "error.h"
#include "progressbar.h"
#include "info.h"
#include "compressor.h"
#include "process_fragments.h"

#define FALSE 0
#define TRUE 1

extern struct queue *to_process_frag;
extern struct seq_queue *to_main;
extern int sparse_files;
extern long long start_offset;

/*
 * Compute 16 bit BSD checksum over the data, and check for sparseness
 */
static int checksum_sparse(struct file_buffer *file_buffer)
{
	unsigned char *b = (unsigned char *) file_buffer->data;
	unsigned short chksum = 0;
	int bytes = file_buffer->size, sparse = TRUE, value;

	while(bytes --) {
		chksum = (chksum & 1) ? (chksum >> 1) | 0x8000 : chksum >> 1;
		value = *b++;
		if(value) {
			sparse = FALSE;
			chksum += value;
		}
	}

	file_buffer->checksum = chksum;
	return sparse;
}


static int read_filesystem(int fd, long long byte, int bytes, void *buff)
{
	off_t off = byte;

	TRACE("read_filesystem: reading from position 0x%llx, bytes %d\n",
		byte, bytes);

	if(lseek(fd, start_offset + off, SEEK_SET) == -1) {
		ERROR("read_filesystem: Lseek on destination failed because %s, "
			"offset=0x%llx\n", strerror(errno), start_offset + off);
		return 0;
	} else if(read_bytes(fd, buff, bytes) < bytes) {
		ERROR("Read on destination failed\n");
		return 0;
	}

	return 1;
}


static struct file_buffer *get_fragment(struct fragment *fragment,
	char *data_buffer, int fd)
{
	struct squashfs_fragment_entry *disk_fragment;
	struct file_buffer *buffer, *compressed_buffer;
	long long start_block;
	int res, size, index = fragment->index;
	char locked;

	/*
	 * Lookup fragment block in cache.
	 * If the fragment block doesn't exist, then get the compressed version
	 * from the writer cache or off disk, and decompress it.
	 *
	 * This routine has two things which complicate the code:
	 *
	 *	1. Multiple threads can simultaneously lookup/create the
	 *	   same buffer.  This means a buffer needs to be "locked"
	 *	   when it is being filled in, to prevent other threads from
	 *	   using it when it is not ready.  This is because we now do
	 *	   fragment duplicate checking in parallel.
	 *	2. We have two caches which need to be checked for the
	 *	   presence of fragment blocks: the normal fragment cache
	 *	   and a "reserve" cache.  The reserve cache is used to
	 *	   prevent an unnecessary pipeline stall when the fragment cache
	 *	   is full of fragments waiting to be compressed.
	 */
	pthread_cleanup_push((void *) pthread_mutex_unlock, &dup_mutex);
	pthread_mutex_lock(&dup_mutex);

again:
	buffer = cache_lookup_nowait(fragment_buffer, index, &locked);
	if(buffer) {
		pthread_mutex_unlock(&dup_mutex);
		if(locked)
			/* got a buffer being filled in.  Wait for it */
			cache_wait_unlock(buffer);
		goto finished;
	}

	/* not in fragment cache, is it in the reserve cache? */
	buffer = cache_lookup_nowait(reserve_cache, index, &locked);
	if(buffer) {
		pthread_mutex_unlock(&dup_mutex);
		if(locked)
			/* got a buffer being filled in.  Wait for it */
			cache_wait_unlock(buffer);
		goto finished;
	}

	/* in neither cache, try to get it from the fragment cache */
	buffer = cache_get_nowait(fragment_buffer, index);
	if(!buffer) {
		/*
		 * no room, get it from the reserve cache, this is
		 * dimensioned so it will always have space (no more than
		 * processors + 1 can have an outstanding reserve buffer)
		 */
		buffer = cache_get_nowait(reserve_cache, index);
		if(!buffer) {
			/* failsafe */
			ERROR("no space in reserve cache\n");
			goto again;
		}
	}

	pthread_mutex_unlock(&dup_mutex);

	compressed_buffer = cache_lookup(fwriter_buffer, index);

	pthread_cleanup_push((void *) pthread_mutex_unlock, &fragment_mutex);
	pthread_mutex_lock(&fragment_mutex);
	disk_fragment = &fragment_table[index];
	size = SQUASHFS_COMPRESSED_SIZE_BLOCK(disk_fragment->size);
	start_block = disk_fragment->start_block;
	pthread_cleanup_pop(1);

	if(SQUASHFS_COMPRESSED_BLOCK(disk_fragment->size)) {
		int error;
		char *data;

		if(compressed_buffer)
			data = compressed_buffer->data;
		else {
			res = read_filesystem(fd, start_block, size, data_buffer);
			if(res == 0) {
				ERROR("Failed to read fragment from output"
					" filesystem\n");
				BAD_ERROR("Output filesystem corrupted?\n");
			}
			data = data_buffer;
		}

		res = compressor_uncompress(comp, buffer->data, data, size,
			block_size, &error);
		if(res == -1)
			BAD_ERROR("%s uncompress failed with error code %d\n",
				comp->name, error);
	} else if(compressed_buffer)
		memcpy(buffer->data, compressed_buffer->data, size);
	else {
		res = read_filesystem(fd, start_block, size, buffer->data);
		if(res == 0) {
			ERROR("Failed to read fragment from output "
				"filesystem\n");
			BAD_ERROR("Output filesystem corrupted?\n");
		}
	}

	cache_unlock(buffer);
	cache_block_put(compressed_buffer);

finished:
	pthread_cleanup_pop(0);

	return buffer;
}


struct file_buffer *get_fragment_cksum(struct file_info *file,
	char *data_buffer, int fd, unsigned short *checksum)
{
	struct file_buffer *frag_buffer;
	struct append_file *append;
	int index = file->fragment->index;

	frag_buffer = get_fragment(file->fragment, data_buffer, fd);

	pthread_cleanup_push((void *) pthread_mutex_unlock, &dup_mutex);

	for(append = file_mapping[index]; append; append = append->next) {
		int offset = append->file->fragment->offset;
		int size = append->file->fragment->size;
		char *data = frag_buffer->data + offset;
		unsigned short cksum = get_checksum_mem(data, size);

		if(file == append->file)
			*checksum = cksum;

		pthread_mutex_lock(&dup_mutex);
		append->file->fragment_checksum = cksum;
		append->file->have_frag_checksum = TRUE;
		pthread_mutex_unlock(&dup_mutex);
	}

	pthread_cleanup_pop(0);

	return frag_buffer;
}


void *frag_thrd(void *destination_file)
{
	sigset_t sigmask, old_mask;
	char *data_buffer;
	int fd;

	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGTERM);
	sigaddset(&sigmask, SIGUSR1);
	pthread_sigmask(SIG_BLOCK, &sigmask, &old_mask);

	fd = open(destination_file, O_RDONLY);
	if(fd == -1)
		BAD_ERROR("frag_thrd: can't open destination for reading\n");

	data_buffer = malloc(SQUASHFS_FILE_MAX_SIZE);
	if(data_buffer == NULL)
		MEM_ERROR();

	pthread_cleanup_push((void *) pthread_mutex_unlock, &dup_mutex);

	while(1) {
		struct file_buffer *file_buffer = queue_get(to_process_frag);
		struct file_buffer *buffer;
		int sparse = checksum_sparse(file_buffer);
		struct file_info *dupl_ptr;
		long long file_size;
		unsigned short checksum;
		char flag;
		int res;

		if(sparse_files && sparse) {
			file_buffer->c_byte = 0;
			file_buffer->fragment = FALSE;
		} else
			file_buffer->c_byte = file_buffer->size;

		/*
		 * Specutively pull into the fragment cache any fragment blocks
		 * which contain fragments which *this* fragment may be
		 * be a duplicate.
		 *
		 * By ensuring the fragment block is in cache ahead of time
		 * should eliminate the parallelisation stall when the
		 * main thread needs to read the fragment block to do a
		 * duplicate check on it.
		 *
		 * If this is a fragment belonging to a larger file
		 * (with additional blocks) then ignore it.  Here we're
		 * interested in the "low hanging fruit" of files which
		 * consist of only a fragment
		 */
		if(file_buffer->file_size != file_buffer->size) {
			seq_queue_put(to_main, file_buffer);
			continue;
		}

		file_size = file_buffer->file_size;

		pthread_mutex_lock(&dup_mutex);
		dupl_ptr = dupl[DUP_HASH(file_size)];
		pthread_mutex_unlock(&dup_mutex);

		file_buffer->dupl_start = dupl_ptr;
		file_buffer->duplicate = FALSE;

		for(; dupl_ptr; dupl_ptr = dupl_ptr->next) {
			if(file_size != dupl_ptr->file_size ||
					file_size != dupl_ptr->fragment->size)
				continue;

			pthread_mutex_lock(&dup_mutex);
			flag = dupl_ptr->have_frag_checksum;
			checksum = dupl_ptr->fragment_checksum;
			pthread_mutex_unlock(&dup_mutex);

			/*
			 * If we have the checksum and it matches then
			 * read in the fragment block.
			 *
			 * If we *don't* have the checksum, then we are
			 * appending, and the fragment block is on the
			 * "old" filesystem.  Read it in and checksum
			 * the entire fragment buffer
			 */
			if(!flag) {
				buffer = get_fragment_cksum(dupl_ptr,
					data_buffer, fd, &checksum);
				if(checksum != file_buffer->checksum) {
					cache_block_put(buffer);
					continue;
				}
			} else if(checksum == file_buffer->checksum)
				buffer = get_fragment(dupl_ptr->fragment,
					data_buffer, fd);
			else
				continue;

			res = memcmp(file_buffer->data, buffer->data +
				dupl_ptr->fragment->offset, file_size);
			cache_block_put(buffer);
			if(res == 0) {
				struct file_buffer *dup = malloc(sizeof(*dup));
				if(dup == NULL)
					MEM_ERROR();
				memcpy(dup, file_buffer, sizeof(*dup));
				cache_block_put(file_buffer);
				dup->dupl_start = dupl_ptr;
				dup->duplicate = TRUE;
				file_buffer = dup;
				break;
			}
		}

		seq_queue_put(to_main, file_buffer);
	}

	pthread_cleanup_pop(0);
}
