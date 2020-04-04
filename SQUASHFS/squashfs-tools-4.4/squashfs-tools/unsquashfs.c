/*
 * Unsquash a squashfs filesystem.  This is a highly compressed read only
 * filesystem.
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011,
 * 2012, 2013, 2014, 2017, 2019
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
 * unsquashfs.c
 */

#include "unsquashfs.h"
#include "squashfs_swap.h"
#include "squashfs_compat.h"
#include "compressor.h"
#include "xattr.h"
#include "unsquashfs_info.h"
#include "stdarg.h"
#include "fnmatch_compat.h"

#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <limits.h>
#include <ctype.h>


typedef unsigned long long uint64_t;
typedef unsigned int    uint32_t;

static int verbose = 0;
#define debug(fmt, ...) if(verbose) printf(fmt, ##__VA_ARGS__)

#pragma pack(1)

typedef struct fs_disk_region
{
    uint32_t sector;
    uint32_t count;
}fs_disk_region;

typedef struct fs_disk_map
{
    char diskname[32];
    uint64_t filesize;
    
    //fs_disk_region[N];
 }fs_disk_map;
#pragma pack()

int g_fs_region_num = 0;
fs_disk_region *g_fs_region_list = NULL;
fs_disk_map g_fs_disk_map;

struct cache *fragment_cache, *data_cache;
struct queue *to_reader, *to_inflate, *to_writer, *from_writer;
pthread_t *thread, *inflator_thread;
pthread_mutex_t	fragment_mutex;
static long long start_offset = 0;

/* user options that control parallelisation */
int processors = -1;

struct super_block sBlk;
squashfs_operations *s_ops;
squashfs_operations *(*read_filesystem_tables)();
struct compressor *comp;

int bytes = 0, swap, file_count = 0, dir_count = 0, sym_count = 0,
	dev_count = 0, fifo_count = 0;
struct hash_table_entry *inode_table_hash[65536], *directory_table_hash[65536];
int fd;
unsigned int cached_frag = SQUASHFS_INVALID_FRAG;
unsigned int block_size;
unsigned int block_log;
int lsonly = FALSE, info = FALSE, force = FALSE, short_ls = TRUE;
int concise = FALSE, quiet = FALSE, numeric = FALSE;
int use_regex = FALSE;
char **created_inode;
int root_process;
int columns;
int rotate = 0;
pthread_mutex_t	screen_mutex;
int progress = TRUE, progress_enabled = FALSE;
unsigned int total_blocks = 0, total_files = 0, total_inodes = 0;
unsigned int cur_blocks = 0;
int inode_number = 1;
int no_xattrs = XATTR_DEF;
int user_xattrs = FALSE;
int ignore_errors = FALSE;
int strict_errors = FALSE;
int use_localtime = TRUE;

int lookup_type[] = {
	0,
	S_IFDIR,
	S_IFREG,
	S_IFLNK,
	S_IFBLK,
	S_IFCHR,
	S_IFIFO,
	S_IFSOCK,
	S_IFDIR,
	S_IFREG,
	S_IFLNK,
	S_IFBLK,
	S_IFCHR,
	S_IFIFO,
	S_IFSOCK
};

struct test table[] = {
	{ S_IFMT, S_IFSOCK, 0, 's' },
	{ S_IFMT, S_IFLNK, 0, 'l' },
	{ S_IFMT, S_IFBLK, 0, 'b' },
	{ S_IFMT, S_IFDIR, 0, 'd' },
	{ S_IFMT, S_IFCHR, 0, 'c' },
	{ S_IFMT, S_IFIFO, 0, 'p' },
	{ S_IRUSR, S_IRUSR, 1, 'r' },
	{ S_IWUSR, S_IWUSR, 2, 'w' },
	{ S_IRGRP, S_IRGRP, 4, 'r' },
	{ S_IWGRP, S_IWGRP, 5, 'w' },
	{ S_IROTH, S_IROTH, 7, 'r' },
	{ S_IWOTH, S_IWOTH, 8, 'w' },
	{ S_IXUSR | S_ISUID, S_IXUSR | S_ISUID, 3, 's' },
	{ S_IXUSR | S_ISUID, S_ISUID, 3, 'S' },
	{ S_IXUSR | S_ISUID, S_IXUSR, 3, 'x' },
	{ S_IXGRP | S_ISGID, S_IXGRP | S_ISGID, 6, 's' },
	{ S_IXGRP | S_ISGID, S_ISGID, 6, 'S' },
	{ S_IXGRP | S_ISGID, S_IXGRP, 6, 'x' },
	{ S_IXOTH | S_ISVTX, S_IXOTH | S_ISVTX, 9, 't' },
	{ S_IXOTH | S_ISVTX, S_ISVTX, 9, 'T' },
	{ S_IXOTH | S_ISVTX, S_IXOTH, 9, 'x' },
	{ 0, 0, 0, 0}
};

void progress_bar(long long current, long long max, int columns);

#define MAX_LINE 16384

void prep_exit()
{
}


void sigwinch_handler()
{
	struct winsize winsize;

	if(ioctl(1, TIOCGWINSZ, &winsize) == -1) {
		if(isatty(STDOUT_FILENO))
			ERROR("TIOCGWINSZ ioctl failed, defaulting to 80 "
				"columns\n");
		columns = 80;
	} else
		columns = winsize.ws_col;
}


void sigalrm_handler()
{
	rotate = (rotate + 1) % 4;
}


int add_overflow(int a, int b)
{
	return (INT_MAX - a) < b;
}


int shift_overflow(int a, int shift)
{
	return (INT_MAX >> shift) < a;
}

 
int multiply_overflow(int a, int multiplier)
{
	return (INT_MAX / multiplier) < a;
}


struct queue *queue_init(int size)
{
	struct queue *queue = malloc(sizeof(struct queue));

	if(queue == NULL)
		EXIT_UNSQUASH("Out of memory in queue_init\n");

	if(add_overflow(size, 1) ||
				multiply_overflow(size + 1, sizeof(void *)))
		EXIT_UNSQUASH("Size too large in queue_init\n");

	queue->data = malloc(sizeof(void *) * (size + 1));
	if(queue->data == NULL)
		EXIT_UNSQUASH("Out of memory in queue_init\n");

	queue->size = size + 1;
	queue->readp = queue->writep = 0;
	pthread_mutex_init(&queue->mutex, NULL);
	pthread_cond_init(&queue->empty, NULL);
	pthread_cond_init(&queue->full, NULL);

	return queue;
}


void queue_put(struct queue *queue, void *data)
{
	int nextp;

	pthread_mutex_lock(&queue->mutex);

	while((nextp = (queue->writep + 1) % queue->size) == queue->readp)
		pthread_cond_wait(&queue->full, &queue->mutex);

	queue->data[queue->writep] = data;
	queue->writep = nextp;
	pthread_cond_signal(&queue->empty);
	pthread_mutex_unlock(&queue->mutex);
}


void *queue_get(struct queue *queue)
{
	void *data;
	pthread_mutex_lock(&queue->mutex);

	while(queue->readp == queue->writep)
		pthread_cond_wait(&queue->empty, &queue->mutex);

	data = queue->data[queue->readp];
	queue->readp = (queue->readp + 1) % queue->size;
	pthread_cond_signal(&queue->full);
	pthread_mutex_unlock(&queue->mutex);

	return data;
}


void dump_queue(struct queue *queue)
{
	pthread_mutex_lock(&queue->mutex);

	printf("Max size %d, size %d%s\n", queue->size - 1,  
		queue->readp <= queue->writep ? queue->writep - queue->readp :
			queue->size - queue->readp + queue->writep,
		queue->readp == queue->writep ? " (EMPTY)" :
			((queue->writep + 1) % queue->size) == queue->readp ?
			" (FULL)" : "");

	pthread_mutex_unlock(&queue->mutex);
}


/* Called with the cache mutex held */
void insert_hash_table(struct cache *cache, struct cache_entry *entry)
{
	int hash = CALCULATE_HASH(entry->block);

	entry->hash_next = cache->hash_table[hash];
	cache->hash_table[hash] = entry;
	entry->hash_prev = NULL;
	if(entry->hash_next)
		entry->hash_next->hash_prev = entry;
}


/* Called with the cache mutex held */
void remove_hash_table(struct cache *cache, struct cache_entry *entry)
{
	if(entry->hash_prev)
		entry->hash_prev->hash_next = entry->hash_next;
	else
		cache->hash_table[CALCULATE_HASH(entry->block)] =
			entry->hash_next;
	if(entry->hash_next)
		entry->hash_next->hash_prev = entry->hash_prev;

	entry->hash_prev = entry->hash_next = NULL;
}


/* Called with the cache mutex held */
void insert_free_list(struct cache *cache, struct cache_entry *entry)
{
	if(cache->free_list) {
		entry->free_next = cache->free_list;
		entry->free_prev = cache->free_list->free_prev;
		cache->free_list->free_prev->free_next = entry;
		cache->free_list->free_prev = entry;
	} else {
		cache->free_list = entry;
		entry->free_prev = entry->free_next = entry;
	}
}


/* Called with the cache mutex held */
void remove_free_list(struct cache *cache, struct cache_entry *entry)
{
	if(entry->free_prev == NULL || entry->free_next == NULL)
		/* not in free list */
		return;
	else if(entry->free_prev == entry && entry->free_next == entry) {
		/* only this entry in the free list */
		cache->free_list = NULL;
	} else {
		/* more than one entry in the free list */
		entry->free_next->free_prev = entry->free_prev;
		entry->free_prev->free_next = entry->free_next;
		if(cache->free_list == entry)
			cache->free_list = entry->free_next;
	}

	entry->free_prev = entry->free_next = NULL;
}


struct cache *cache_init(int buffer_size, int max_buffers)
{
	struct cache *cache = malloc(sizeof(struct cache));

	if(cache == NULL)
		EXIT_UNSQUASH("Out of memory in cache_init\n");

	cache->max_buffers = max_buffers;
	cache->buffer_size = buffer_size;
	cache->count = 0;
	cache->used = 0;
	cache->free_list = NULL;
	memset(cache->hash_table, 0, sizeof(struct cache_entry *) * 65536);
	cache->wait_free = FALSE;
	cache->wait_pending = FALSE;
	pthread_mutex_init(&cache->mutex, NULL);
	pthread_cond_init(&cache->wait_for_free, NULL);
	pthread_cond_init(&cache->wait_for_pending, NULL);

	return cache;
}


struct cache_entry *cache_get(struct cache *cache, long long block, int size)
{
	/*
	 * Get a block out of the cache.  If the block isn't in the cache
 	 * it is added and queued to the reader() and inflate() threads for
 	 * reading off disk and decompression.  The cache grows until max_blocks
 	 * is reached, once this occurs existing discarded blocks on the free
 	 * list are reused
 	 */
	int hash = CALCULATE_HASH(block);
	struct cache_entry *entry;

	pthread_mutex_lock(&cache->mutex);

	for(entry = cache->hash_table[hash]; entry; entry = entry->hash_next)
		if(entry->block == block)
			break;

	if(entry) {
		/*
 		 * found the block in the cache.  If the block is currently unused
		 * remove it from the free list and increment cache used count.
 		 */
		if(entry->used == 0) {
			cache->used ++;
			remove_free_list(cache, entry);
		}
		entry->used ++;
		pthread_mutex_unlock(&cache->mutex);
	} else {
		/*
 		 * not in the cache
		 *
		 * first try to allocate new block
		 */
		if(cache->count < cache->max_buffers) {
			entry = malloc(sizeof(struct cache_entry));
			if(entry == NULL)
				EXIT_UNSQUASH("Out of memory in cache_get\n");
			entry->data = malloc(cache->buffer_size);
			if(entry->data == NULL)
				EXIT_UNSQUASH("Out of memory in cache_get\n");
			entry->cache = cache;
			entry->free_prev = entry->free_next = NULL;
			cache->count ++;
		} else {
			/*
			 * try to get from free list
			 */
			while(cache->free_list == NULL) {
				cache->wait_free = TRUE;
				pthread_cond_wait(&cache->wait_for_free,
					&cache->mutex);
			}
			entry = cache->free_list;
			remove_free_list(cache, entry);
			remove_hash_table(cache, entry);
		}

		/*
		 * Initialise block and insert into the hash table.
		 * Increment used which tracks how many buffers in the
		 * cache are actively in use (the other blocks, count - used,
		 * are in the cache and available for lookup, but can also be
		 * re-used).
		 */
		entry->block = block;
		entry->size = size;
		entry->used = 1;
		entry->error = FALSE;
		entry->pending = TRUE;
		insert_hash_table(cache, entry);
		cache->used ++;

		/*
		 * queue to read thread to read and ultimately (via the
		 * decompress threads) decompress the buffer
 		 */
		pthread_mutex_unlock(&cache->mutex);
		queue_put(to_reader, entry);
	}

	return entry;
}

	
void cache_block_ready(struct cache_entry *entry, int error)
{
	/*
	 * mark cache entry as being complete, reading and (if necessary)
 	 * decompression has taken place, and the buffer is valid for use.
 	 * If an error occurs reading or decompressing, the buffer also 
 	 * becomes ready but with an error...
 	 */
	pthread_mutex_lock(&entry->cache->mutex);
	entry->pending = FALSE;
	entry->error = error;

	/*
	 * if the wait_pending flag is set, one or more threads may be waiting
	 * on this buffer
	 */
	if(entry->cache->wait_pending) {
		entry->cache->wait_pending = FALSE;
		pthread_cond_broadcast(&entry->cache->wait_for_pending);
	}

	pthread_mutex_unlock(&entry->cache->mutex);
}


void cache_block_wait(struct cache_entry *entry)
{
	/*
	 * wait for this cache entry to become ready, when reading and (if
	 * necessary) decompression has taken place
	 */
	pthread_mutex_lock(&entry->cache->mutex);

	while(entry->pending) {
		entry->cache->wait_pending = TRUE;
		pthread_cond_wait(&entry->cache->wait_for_pending,
			&entry->cache->mutex);
	}

	pthread_mutex_unlock(&entry->cache->mutex);
}


void cache_block_put(struct cache_entry *entry)
{
	/*
	 * finished with this cache entry, once the usage count reaches zero it
 	 * can be reused and is put onto the free list.  As it remains
 	 * accessible via the hash table it can be found getting a new lease of
 	 * life before it is reused.
 	 */
	pthread_mutex_lock(&entry->cache->mutex);

	entry->used --;
	if(entry->used == 0) {
		insert_free_list(entry->cache, entry);
		entry->cache->used --;

		/*
		 * if the wait_free flag is set, one or more threads may be
		 * waiting on this buffer
		 */
		if(entry->cache->wait_free) {
			entry->cache->wait_free = FALSE;
			pthread_cond_broadcast(&entry->cache->wait_for_free);
		}
	}

	pthread_mutex_unlock(&entry->cache->mutex);
}


void dump_cache(struct cache *cache)
{
	pthread_mutex_lock(&cache->mutex);

	printf("Max buffers %d, Current size %d, Used %d,  %s\n",
		cache->max_buffers, cache->count, cache->used,
		cache->free_list ?  "Free buffers" : "No free buffers");

	pthread_mutex_unlock(&cache->mutex);
}


char *modestr(char *str, int mode)
{
	int i;

	strcpy(str, "----------");

	for(i = 0; table[i].mask != 0; i++) {
		if((mode & table[i].mask) == table[i].value)
			str[table[i].position] = table[i].mode;
	}

	return str;
}


#define TOTALCHARS  25
int print_filename(char *pathname, struct inode *inode)
{
	char str[11], dummy[12], dummy2[12]; /* overflow safe */
	char *userstr, *groupstr;
	int padchars;
	struct passwd *user;
	struct group *group;
	struct tm *t;

	if(short_ls) {
		printf("%s\n", pathname);
		return 1;
	}

	user = numeric ? NULL : getpwuid(inode->uid);
	if(user == NULL) {
		int res = snprintf(dummy, 12, "%d", inode->uid);
		if(res < 0)
			EXIT_UNSQUASH("snprintf failed in print_filename()\n");
		else if(res >= 12)
			/* unsigned int shouldn't ever need more than 11 bytes
			 * (including terminating '\0') to print in base 10 */
			userstr = "*";
		else
			userstr = dummy;
	} else
		userstr = user->pw_name;
		 
	group = numeric ? NULL : getgrgid(inode->gid);
	if(group == NULL) {
		int res = snprintf(dummy2, 12, "%d", inode->gid);
		if(res < 0)
			EXIT_UNSQUASH("snprintf failed in print_filename()\n");
		else if(res >= 12)
			/* unsigned int shouldn't ever need more than 11 bytes
			 * (including terminating '\0') to print in base 10 */
			groupstr = "*";
		else
			groupstr = dummy2;
	} else
		groupstr = group->gr_name;

	printf("%s %s/%s ", modestr(str, inode->mode), userstr, groupstr);

	switch(inode->mode & S_IFMT) {
		case S_IFREG:
		case S_IFDIR:
		case S_IFSOCK:
		case S_IFIFO:
		case S_IFLNK:
			padchars = TOTALCHARS - strlen(userstr) -
				strlen(groupstr);

			printf("%*lld ", padchars > 0 ? padchars : 0,
				inode->data);
			break;
		case S_IFCHR:
		case S_IFBLK:
			padchars = TOTALCHARS - strlen(userstr) -
				strlen(groupstr) - 7; 

			printf("%*s%3d,%3d ", padchars > 0 ? padchars : 0, " ",
				(int) inode->data >> 8, (int) inode->data &
				0xff);
			break;
	}

	t = use_localtime ? localtime(&inode->time) : gmtime(&inode->time);

	printf("%d-%02d-%02d %02d:%02d %s", t->tm_year + 1900, t->tm_mon + 1,
		t->tm_mday, t->tm_hour, t->tm_min, pathname);
	if((inode->mode & S_IFMT) == S_IFLNK)
		printf(" -> %s", inode->symlink);
	printf("\n");
		
	return 1;
}
	

void add_entry(struct hash_table_entry *hash_table[], long long start,
	long long bytes)
{
	int hash = CALCULATE_HASH(start);
	struct hash_table_entry *hash_table_entry;

	hash_table_entry = malloc(sizeof(struct hash_table_entry));
	if(hash_table_entry == NULL)
		EXIT_UNSQUASH("Out of memory in add_entry\n");

	hash_table_entry->start = start;
	hash_table_entry->bytes = bytes;
	hash_table_entry->next = hash_table[hash];
	hash_table[hash] = hash_table_entry;
}


long long lookup_entry(struct hash_table_entry *hash_table[], long long start)
{
	int hash = CALCULATE_HASH(start);
	struct hash_table_entry *hash_table_entry;

	for(hash_table_entry = hash_table[hash]; hash_table_entry;
				hash_table_entry = hash_table_entry->next)

		if(hash_table_entry->start == start)
			return hash_table_entry->bytes;

	return -1;
}

int read_fs_sectors(int fd, uint32_t sector, uint32_t count, char *buf)
{
    int i;
    uint32_t total = 0;
    uint32_t left = 0;
    uint32_t offset = 0;
    uint32_t readcnt = 0;
    fs_disk_region *region;

    for (i = 0; i < g_fs_region_num && count > 0; i++)
    {
        region = g_fs_region_list + i;

        if (sector >= total && sector < total + region->count)
        {
            offset = sector - total;
            left = region->count - offset;
            readcnt = (count <= left) ? count : left;
            
            lseek(fd, (uint64_t)(offset + region->sector) * 512ULL, SEEK_SET);
            read(fd, buf, (uint64_t)readcnt * 512ULL);

            buf += (uint64_t)readcnt * 512ULL;
            count -= readcnt;
        }
        else
        {
            total += region->count;
        }
    }

    return 0;
}

#if 1
int read_fs_bytes(int fd, long long byte, int bytes, void *buff)
{
    uint32_t mod = 0;
    uint32_t align = 0;
    uint32_t sector = 0;
    uint32_t number = 0;
    uint32_t leftsize = 0;
	uint64_t offset = byte;
    char *buf = (char *)buff;
    char secbuf[512];

    if (offset >= g_fs_disk_map.filesize || offset + bytes > g_fs_disk_map.filesize)
    {
        return FALSE;
    }

    leftsize = bytes;
    sector = offset / 512;

    mod = offset % 512;
    if (mod > 0)
    {
        align = 512 - mod;
        read_fs_sectors(fd, sector, 1, secbuf);

        if (leftsize > align)
        {
            memcpy(buf, secbuf + mod, align);
            buf += align;
            offset += align;
            sector++;
            leftsize -= align;
        }
        else
        {
            memcpy(buf, secbuf + mod, leftsize);
            return TRUE;
        }
    }

    number = leftsize / 512;
    read_fs_sectors(fd, sector, number, buf);
    buf += number * 512;

    mod = leftsize % 512;
    if (mod > 0)
    {
        read_fs_sectors(fd, sector + number, 1, secbuf);
        memcpy(buf, secbuf, mod);
    }

	return TRUE;
}

#else

int read_fs_bytes(int fd, long long byte, int bytes, void *buff)
{
	off_t off = byte;
	int res, count;

	TRACE("read_bytes: reading from position 0x%llx, bytes %d\n", byte,
		bytes);

	if(lseek(fd, start_offset + off, SEEK_SET) == -1) {
		ERROR("Lseek failed because %s\n", strerror(errno));
		return FALSE;
	}

	for(count = 0; count < bytes; count += res) {
		res = read(fd, buff + count, bytes - count);
		if(res < 1) {
			if(res == 0) {
				ERROR("Read on filesystem failed because "
					"EOF\n");
				return FALSE;
			} else if(errno != EINTR) {
				ERROR("Read on filesystem failed because %s\n",
						strerror(errno));
				return FALSE;
			} else
				res = 0;
		}
	}

	return TRUE;
}
#endif

int read_block(int fd, long long start, long long *next, int expected,
								void *block)
{
	unsigned short c_byte;
	int offset = 2, res, compressed;
	int outlen = expected ? expected : SQUASHFS_METADATA_SIZE;
	static char *buffer = NULL;

	if(outlen > SQUASHFS_METADATA_SIZE)
		return FALSE;

	if(swap) {
		if(read_fs_bytes(fd, start, 2, &c_byte) == FALSE)
			goto failed;
		c_byte = (c_byte >> 8) | ((c_byte & 0xff) << 8);
	} else 
		if(read_fs_bytes(fd, start, 2, &c_byte) == FALSE)
			goto failed;

	TRACE("read_block: block @0x%llx, %d %s bytes\n", start,
		SQUASHFS_COMPRESSED_SIZE(c_byte), SQUASHFS_COMPRESSED(c_byte) ?
		"compressed" : "uncompressed");

	if(SQUASHFS_CHECK_DATA(sBlk.s.flags))
		offset = 3;

	compressed = SQUASHFS_COMPRESSED(c_byte);
	c_byte = SQUASHFS_COMPRESSED_SIZE(c_byte);

	/*
	 * The block size should not be larger than
	 * the uncompressed size (or max uncompressed size if
	 * expected is 0)
	 */
	if(c_byte > outlen)
		return FALSE;

	if(compressed) {
		int error;

		if(buffer == NULL) {
			buffer = malloc(SQUASHFS_METADATA_SIZE);

			if(buffer == NULL)
				EXIT_UNSQUASH("read_block: Failed to allocate buffer\n");
		}

		res = read_fs_bytes(fd, start + offset, c_byte, buffer);
		if(res == FALSE)
			goto failed;

		res = compressor_uncompress(comp, block, buffer, c_byte,
			outlen, &error);

		if(res == -1) {
			ERROR("%s uncompress failed with error code %d\n",
				comp->name, error);
			goto failed;
		}
	} else {
		res = read_fs_bytes(fd, start + offset, c_byte, block);
		if(res == FALSE)
			goto failed;
		res = c_byte;
	}

	if(next)
		*next = start + offset + c_byte;

	/*
	 * if expected, then check the (uncompressed) return data
	 * is of the expected size
	 */
	if(expected && expected != res)
		return FALSE;
	else
		return res;

failed:
	ERROR("read_block: failed to read block @0x%llx\n", start);
	return FALSE;
}


void *read_inode_table(long long start, long long end)
{
	int res;
	long long size = 0;
	long long bytes = 0;
	void *inode_table = NULL;

	TRACE("read_inode_table: start %lld, end %lld\n", start, end);

	while(start < end) {
		if(size - bytes < SQUASHFS_METADATA_SIZE) {
			inode_table = realloc(inode_table, size +=
				SQUASHFS_METADATA_SIZE);
			if(inode_table == NULL) {
				ERROR("Out of memory in read_inode_table");
				goto failed;
			}
		}

		add_entry(inode_table_hash, start, bytes);

		res = read_block(fd, start, &start, 0, inode_table + bytes);
		if(res == 0) {
			ERROR("read_inode_table: failed to read block\n");
			goto failed;
		}
		bytes += res;

		/*
		 * If this is not the last metadata block in the inode table
		 * then it should be SQUASHFS_METADATA_SIZE in size.
		 * Note, we can't use expected in read_block() above for this
		 * because we don't know if this is the last block until
		 * after reading.
		 */
		if(start != end && res != SQUASHFS_METADATA_SIZE) {
			ERROR("read_inode_table: metadata block should be %d "
				"bytes in length, it is %d bytes\n",
				SQUASHFS_METADATA_SIZE, res);
			
			goto failed;
		}
	}

	return inode_table;

failed:
	free(inode_table);
	return NULL;
}


int set_attributes(char *pathname, int mode, uid_t uid, gid_t guid, time_t time,
	unsigned int xattr, unsigned int set_mode)
{
	struct utimbuf times = { time, time };

	if(utime(pathname, &times) == -1) {
		EXIT_UNSQUASH_STRICT("set_attributes: failed to set time on %s, because %s\n",
			pathname, strerror(errno));
		return FALSE;
	}

	if(root_process) {
		if(chown(pathname, uid, guid) == -1) {
			EXIT_UNSQUASH_STRICT("set_attributes: failed to change uid and gids "
				"on %s, because %s\n", pathname,
				strerror(errno));
			return FALSE;
		}
	} else
		mode &= ~06000;

	if((set_mode || (mode & 07000)) && chmod(pathname, (mode_t) mode) == -1) {
		/*
		 * Some filesystems require root privileges to use the sticky
		 * bit. If we're not root and chmod() failed with EPERM when the
		 * sticky bit was included in the mode, try again without the
		 * sticky bit. Otherwise, fail with an error message.
		 */
		if (root_process || errno != EPERM || !(mode & 01000) ||
				chmod(pathname, (mode_t) (mode & ~01000)) == -1) {
			EXIT_UNSQUASH_STRICT("set_attributes: failed to change mode %s, because %s\n",
				pathname, strerror(errno));
			return FALSE;
		}
	}

	return write_xattr(pathname, xattr);
}


int write_bytes(int fd, char *buff, int bytes)
{
	int res, count;

	for(count = 0; count < bytes; count += res) {
		res = write(fd, buff + count, bytes - count);
		if(res == -1) {
			if(errno != EINTR) {
				ERROR("Write on output file failed because "
					"%s\n", strerror(errno));
				return -1;
			}
			res = 0;
		}
	}

	return 0;
}


int lseek_broken = FALSE;
char *zero_data = NULL;

int write_block(int file_fd, char *buffer, int size, long long hole, int sparse)
{
	off_t off = hole;

	if(hole) {
		if(sparse && lseek_broken == FALSE) {
			 int error = lseek(file_fd, off, SEEK_CUR);
			 if(error == -1)
				/* failed to seek beyond end of file */
				lseek_broken = TRUE;
		}

		if((sparse == FALSE || lseek_broken) && zero_data == NULL) {
			if((zero_data = malloc(block_size)) == NULL)
				EXIT_UNSQUASH("write_block: failed to alloc "
					"zero data block\n");
			memset(zero_data, 0, block_size);
		}

		if(sparse == FALSE || lseek_broken) {
			int blocks = (hole + block_size -1) / block_size;
			int avail_bytes, i;
			for(i = 0; i < blocks; i++, hole -= avail_bytes) {
				avail_bytes = hole > block_size ? block_size :
					hole;
				if(write_bytes(file_fd, zero_data, avail_bytes)
						== -1)
					goto failure;
			}
		}
	}

	if(write_bytes(file_fd, buffer, size) == -1)
		goto failure;

	return TRUE;

failure:
	return FALSE;
}


pthread_mutex_t open_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t open_empty = PTHREAD_COND_INITIALIZER;
int open_unlimited, open_count;
#define OPEN_FILE_MARGIN 10


void open_init(int count)
{
	open_count = count;
	open_unlimited = count == -1;
}


int open_wait(char *pathname, int flags, mode_t mode)
{
	if (!open_unlimited) {
		pthread_mutex_lock(&open_mutex);
		while (open_count == 0)
			pthread_cond_wait(&open_empty, &open_mutex);
		open_count --;
		pthread_mutex_unlock(&open_mutex);
	}

	return open(pathname, flags, mode);
}


void close_wake(int fd)
{
	close(fd);

	if (!open_unlimited) {
		pthread_mutex_lock(&open_mutex);
		open_count ++;
		pthread_cond_signal(&open_empty);
		pthread_mutex_unlock(&open_mutex);
	}
}


void queue_file(char *pathname, int file_fd, struct inode *inode)
{
	struct squashfs_file *file = malloc(sizeof(struct squashfs_file));
	if(file == NULL)
		EXIT_UNSQUASH("queue_file: unable to malloc file\n");

	file->fd = file_fd;
	file->file_size = inode->data;
	file->mode = inode->mode;
	file->gid = inode->gid;
	file->uid = inode->uid;
	file->time = inode->time;
	file->pathname = strdup(pathname);
	file->blocks = inode->blocks + (inode->frag_bytes > 0);
	file->sparse = inode->sparse;
	file->xattr = inode->xattr;
	queue_put(to_writer, file);
}


void queue_dir(char *pathname, struct dir *dir)
{
	struct squashfs_file *file = malloc(sizeof(struct squashfs_file));
	if(file == NULL)
		EXIT_UNSQUASH("queue_dir: unable to malloc file\n");

	file->fd = -1;
	file->mode = dir->mode;
	file->gid = dir->guid;
	file->uid = dir->uid;
	file->time = dir->mtime;
	file->pathname = strdup(pathname);
	file->xattr = dir->xattr;
	queue_put(to_writer, file);
}


int write_file(struct inode *inode, char *pathname)
{
	unsigned int file_fd, i;
	unsigned int *block_list;
	int file_end = inode->data / block_size;
	long long start = inode->start;

	TRACE("write_file: regular file, blocks %d\n", inode->blocks);

	file_fd = open_wait(pathname, O_CREAT | O_WRONLY |
		(force ? O_TRUNC : 0), (mode_t) inode->mode & 0777);
	if(file_fd == -1) {
		EXIT_UNSQUASH_IGNORE("write_file: failed to create file %s, because %s\n",
			pathname, strerror(errno));
		return FALSE;
	}

	block_list = malloc(inode->blocks * sizeof(unsigned int));
	if(block_list == NULL)
		EXIT_UNSQUASH("write_file: unable to malloc block list\n");

	s_ops->read_block_list(block_list, inode->block_ptr, inode->blocks);

	/*
	 * the writer thread is queued a squashfs_file structure describing the
 	 * file.  If the file has one or more blocks or a fragment they are
 	 * queued separately (references to blocks in the cache).
 	 */
	queue_file(pathname, file_fd, inode);

	for(i = 0; i < inode->blocks; i++) {
		int c_byte = SQUASHFS_COMPRESSED_SIZE_BLOCK(block_list[i]);
		struct file_entry *block = malloc(sizeof(struct file_entry));

		if(block == NULL)
			EXIT_UNSQUASH("write_file: unable to malloc file\n");
		block->offset = 0;
		block->size = i == file_end ? inode->data & (block_size - 1) :
			block_size;
		if(block_list[i] == 0) /* sparse block */
			block->buffer = NULL;
		else {
			block->buffer = cache_get(data_cache, start,
				block_list[i]);
			start += c_byte;
		}
		queue_put(to_writer, block);
	}

	if(inode->frag_bytes) {
		int size;
		long long start;
		struct file_entry *block = malloc(sizeof(struct file_entry));

		if(block == NULL)
			EXIT_UNSQUASH("write_file: unable to malloc file\n");
		s_ops->read_fragment(inode->fragment, &start, &size);
		block->buffer = cache_get(fragment_cache, start, size);
		block->offset = inode->offset;
		block->size = inode->frag_bytes;
		queue_put(to_writer, block);
	}

	free(block_list);
	return TRUE;
}


int create_inode(char *pathname, struct inode *i)
{
	int res;
	int failed = FALSE;

	TRACE("create_inode: pathname %s\n", pathname);

	if(created_inode[i->inode_number - 1]) {
		TRACE("create_inode: hard link\n");
		if(force)
			unlink(pathname);

		if(link(created_inode[i->inode_number - 1], pathname) == -1) {
			EXIT_UNSQUASH_IGNORE("create_inode: failed to create hardlink, "
				"because %s\n", strerror(errno));
			return FALSE;
		}

		return TRUE;
	}

	switch(i->type) {
		case SQUASHFS_FILE_TYPE:
		case SQUASHFS_LREG_TYPE:
			TRACE("create_inode: regular file, file_size %lld, "
				"blocks %d\n", i->data, i->blocks);

			res = write_file(i, pathname);
			if(res == FALSE)
				goto failed;

			file_count ++;
			break;
		case SQUASHFS_SYMLINK_TYPE:
		case SQUASHFS_LSYMLINK_TYPE: {
			struct timespec times[2] = {
				{ i->time, 0 },
				{ i->time, 0 }
			};

			TRACE("create_inode: symlink, symlink_size %lld\n",
				i->data);

			if(force)
				unlink(pathname);

			res = symlink(i->symlink, pathname);
			if(res == -1) {
				EXIT_UNSQUASH_STRICT("create_inode: failed to create symlink "
					"%s, because %s\n", pathname,
					strerror(errno));
				goto failed;
			}

			res = utimensat(AT_FDCWD, pathname, times,
					AT_SYMLINK_NOFOLLOW);
			if(res == -1) {
				EXIT_UNSQUASH_STRICT("create_inode: failed to set time on "
					"%s, because %s\n", pathname,
					strerror(errno));
			}

			res = write_xattr(pathname, i->xattr);
			if(res == FALSE)
				failed = TRUE;
	
			if(root_process) {
				res = lchown(pathname, i->uid, i->gid);
				if(res == -1) {
					EXIT_UNSQUASH_STRICT("create_inode: failed to change "
						"uid and gids on %s, because "
						"%s\n", pathname,
						strerror(errno));
					failed = TRUE;
				}
			}

			if(failed)
				goto failed;

			sym_count ++;
			break;
		}
 		case SQUASHFS_BLKDEV_TYPE:
	 	case SQUASHFS_CHRDEV_TYPE:
 		case SQUASHFS_LBLKDEV_TYPE:
	 	case SQUASHFS_LCHRDEV_TYPE: {
			int chrdev = 0;
			if ( i->type == SQUASHFS_CHRDEV_TYPE ||
					i->type == SQUASHFS_LCHRDEV_TYPE)
				chrdev = 1;

			TRACE("create_inode: dev, rdev 0x%llx\n", i->data);

			if(root_process) {
				if(force)
					unlink(pathname);

				res = mknod(pathname, chrdev ? S_IFCHR : S_IFBLK,
						makedev((i->data >> 8) & 0xff,
						i->data & 0xff));
				if(res == -1) {
					EXIT_UNSQUASH_STRICT("create_inode: failed to create "
						"%s device %s, because %s\n",
						chrdev ? "character" : "block",
						pathname, strerror(errno));
					goto failed;
				}
				res = set_attributes(pathname, i->mode, i->uid,
					i->gid, i->time, i->xattr, TRUE);
				if(res == FALSE)
					goto failed;

				dev_count ++;
			} else {
				EXIT_UNSQUASH_STRICT("create_inode: could not create %s "
					"device %s, because you're not "
					"superuser!\n", chrdev ? "character" :
					"block", pathname);
				goto failed;
			}
			break;
		}
		case SQUASHFS_FIFO_TYPE:
		case SQUASHFS_LFIFO_TYPE:
			TRACE("create_inode: fifo\n");

			if(force)
				unlink(pathname);

			res = mknod(pathname, S_IFIFO, 0);
			if(res == -1) {
				ERROR("create_inode: failed to create fifo %s, "
					"because %s\n", pathname,
					strerror(errno));
				goto failed;
			}
			res = set_attributes(pathname, i->mode, i->uid, i->gid,
				i->time, i->xattr, TRUE);
			if(res == FALSE)
				goto failed;

			fifo_count ++;
			break;
		case SQUASHFS_SOCKET_TYPE:
		case SQUASHFS_LSOCKET_TYPE:
			TRACE("create_inode: socket\n");
			ERROR("create_inode: socket %s ignored\n", pathname);
			break;
		default:
			EXIT_UNSQUASH_STRICT("Unknown inode type %d in create_inode_table!\n",
				i->type);
			return FALSE;
	}

	created_inode[i->inode_number - 1] = strdup(pathname);

	return TRUE;

failed:
	/*
	 * Mark the file as created (even though it may not have been), so
	 * any future hard links to it fail with a file not found, which
	 * is correct as the file *is* missing.
	 *
	 * If we don't mark it here as created, then any future hard links will try
	 * to create the file as a separate unlinked file.
	 * If we've had some transitory errors, this may produce files
	 * in various states, which should be hard-linked, but are not.
	 */
	created_inode[i->inode_number - 1] = strdup(pathname);

	return FALSE;
}


void *read_directory_table(long long start, long long end)
{
	int res;
	long long bytes = 0;
	long long size = 0;
	void *directory_table = malloc(1);

	TRACE("read_directory_table: start %lld, end %lld\n", start, end);

	while(start < end) {
		if(size - bytes < SQUASHFS_METADATA_SIZE) {
			directory_table = realloc(directory_table, size +=
				SQUASHFS_METADATA_SIZE);
			if(directory_table == NULL) {
				ERROR("Out of memory in "
						"read_directory_table\n");
				goto failed;
			}
		}

		add_entry(directory_table_hash, start, bytes);

		res = read_block(fd, start, &start, 0, directory_table + bytes);
		if(res == 0) {
			ERROR("read_directory_table: failed to read block\n");
			goto failed;
		}

		bytes += res;

		/*
		 * If this is not the last metadata block in the directory table
		 * then it should be SQUASHFS_METADATA_SIZE in size.
		 * Note, we can't use expected in read_block() above for this
		 * because we don't know if this is the last block until
		 * after reading.
		 */
		if(start != end && res != SQUASHFS_METADATA_SIZE) {
			ERROR("read_directory_table: metadata block "
				"should be %d bytes in length, it is %d "
				"bytes\n", SQUASHFS_METADATA_SIZE, res);
			goto failed;
		}
	}

	return directory_table;

failed:
	free(directory_table);
	return NULL;
}


int squashfs_readdir(struct dir *dir, char **name, unsigned int *start_block,
unsigned int *offset, unsigned int *type)
{
	if(dir->cur_entry == dir->dir_count)
		return FALSE;

	*name = dir->dirs[dir->cur_entry].name;
	*start_block = dir->dirs[dir->cur_entry].start_block;
	*offset = dir->dirs[dir->cur_entry].offset;
	*type = dir->dirs[dir->cur_entry].type;
	dir->cur_entry ++;

	return TRUE;
}


void squashfs_closedir(struct dir *dir)
{
	free(dir->dirs);
	free(dir);
}


char *get_component(char *target, char **targname)
{
	char *start;

	while(*target == '/')
		target ++;

	start = target;
	while(*target != '/' && *target != '\0')
		target ++;

	*targname = strndup(start, target - start);

	while(*target == '/')
		target ++;

	return target;
}


void free_path(struct pathname *paths)
{
	int i;

	for(i = 0; i < paths->names; i++) {
		if(paths->name[i].paths)
			free_path(paths->name[i].paths);
		free(paths->name[i].name);
		if(paths->name[i].preg) {
			regfree(paths->name[i].preg);
			free(paths->name[i].preg);
		}
	}

	free(paths);
}


struct pathname *add_path(struct pathname *paths, char *target, char *alltarget)
{
	char *targname;
	int i, error;

	TRACE("add_path: adding \"%s\" extract file\n", target);

	target = get_component(target, &targname);

	if(paths == NULL) {
		paths = malloc(sizeof(struct pathname));
		if(paths == NULL)
			EXIT_UNSQUASH("failed to allocate paths\n");

		paths->names = 0;
		paths->name = NULL;
	}

	for(i = 0; i < paths->names; i++)
		if(strcmp(paths->name[i].name, targname) == 0)
			break;

	if(i == paths->names) {
		/*
		 * allocate new name entry
		 */
		paths->names ++;
		paths->name = realloc(paths->name, (i + 1) *
			sizeof(struct path_entry));
		if(paths->name == NULL)
			EXIT_UNSQUASH("Out of memory in add_path\n");	
		paths->name[i].name = targname;
		paths->name[i].paths = NULL;
		if(use_regex) {
			paths->name[i].preg = malloc(sizeof(regex_t));
			if(paths->name[i].preg == NULL)
				EXIT_UNSQUASH("Out of memory in add_path\n");
			error = regcomp(paths->name[i].preg, targname,
				REG_EXTENDED|REG_NOSUB);
			if(error) {
				char str[1024]; /* overflow safe */

				regerror(error, paths->name[i].preg, str, 1024);
				EXIT_UNSQUASH("invalid regex %s in export %s, "
					"because %s\n", targname, alltarget,
					str);
			}
		} else
			paths->name[i].preg = NULL;

		if(target[0] == '\0')
			/*
			 * at leaf pathname component
			*/
			paths->name[i].paths = NULL;
		else
			/*
			 * recurse adding child components
			 */
			paths->name[i].paths = add_path(NULL, target, alltarget);
	} else {
		/*
		 * existing matching entry
		 */
		free(targname);

		if(paths->name[i].paths == NULL) {
			/*
			 * No sub-directory which means this is the leaf
			 * component of a pre-existing extract which subsumes
			 * the extract currently being added, in which case stop
			 * adding components
			 */
		} else if(target[0] == '\0') {
			/*
			 * at leaf pathname component and child components exist
			 * from more specific extracts, delete as they're
			 * subsumed by this extract
			 */
			free_path(paths->name[i].paths);
			paths->name[i].paths = NULL;
		} else
			/*
			 * recurse adding child components
			 */
			add_path(paths->name[i].paths, target, alltarget);
	}

	return paths;
}


struct pathnames *init_subdir()
{
	struct pathnames *new = malloc(sizeof(struct pathnames));
	if(new == NULL)
		EXIT_UNSQUASH("Out of memory in init_subdir\n");
	new->count = 0;
	return new;
}


struct pathnames *add_subdir(struct pathnames *paths, struct pathname *path)
{
	if(paths->count % PATHS_ALLOC_SIZE == 0) {
		paths = realloc(paths, sizeof(struct pathnames *) +
			(paths->count + PATHS_ALLOC_SIZE) *
			sizeof(struct pathname *));
		if(paths == NULL)
			EXIT_UNSQUASH("Out of memory in add_subdir\n");
	}

	paths->path[paths->count++] = path;
	return paths;
}


void free_subdir(struct pathnames *paths)
{
	free(paths);
}


int matches(struct pathnames *paths, char *name, struct pathnames **new)
{
	int i, n;

	if(paths == NULL) {
		*new = NULL;
		return TRUE;
	}

	*new = init_subdir();

	for(n = 0; n < paths->count; n++) {
		struct pathname *path = paths->path[n];
		for(i = 0; i < path->names; i++) {
			int match = use_regex ?
				regexec(path->name[i].preg, name, (size_t) 0,
				NULL, 0) == 0 : fnmatch(path->name[i].name,
				name, FNM_PATHNAME|FNM_PERIOD|FNM_EXTMATCH) ==
				0;
			if(match && path->name[i].paths == NULL)
				/*
				 * match on a leaf component, any subdirectories
				 * will implicitly match, therefore return an
				 * empty new search set
				 */
				goto empty_set;

			if(match)
				/*
				 * match on a non-leaf component, add any
				 * subdirectories to the new set of
				 * subdirectories to scan for this name
				 */
				*new = add_subdir(*new, path->name[i].paths);
		}
	}

	if((*new)->count == 0) {
		/*
		 * no matching names found, delete empty search set, and return
		 * FALSE
		 */
		free_subdir(*new);
		*new = NULL;
		return FALSE;
	}

	/*
	 * one or more matches with sub-directories found (no leaf matches),
	 * return new search set and return TRUE
	 */
	return TRUE;

empty_set:
	/*
	 * found matching leaf exclude, return empty search set and return TRUE
	 */
	free_subdir(*new);
	*new = NULL;
	return TRUE;
}


int pre_scan(char *parent_name, unsigned int start_block, unsigned int offset,
	struct pathnames *paths)
{
	unsigned int type;
	int scan_res = TRUE;
	char *name;
	struct pathnames *new;
	struct inode *i;
	struct dir *dir = s_ops->opendir(start_block, offset, &i);

	if(dir == NULL)
		return FALSE;

	while(squashfs_readdir(dir, &name, &start_block, &offset, &type)) {
		struct inode *i;
		char *pathname;
		int res;

		TRACE("pre_scan: name %s, start_block %d, offset %d, type %d\n",
			name, start_block, offset, type);

		if(!matches(paths, name, &new))
			continue;

		res = asprintf(&pathname, "%s/%s", parent_name, name);
		if(res == -1)
			EXIT_UNSQUASH("asprintf failed in dir_scan\n");

		if(type == SQUASHFS_DIR_TYPE) {
			res = pre_scan(parent_name, start_block, offset, new);
			if(res == FALSE)
				scan_res = FALSE;
		} else if(new == NULL) {
			if(type == SQUASHFS_FILE_TYPE ||
					type == SQUASHFS_LREG_TYPE) {
				i = s_ops->read_inode(start_block, offset);
				if(created_inode[i->inode_number - 1] == NULL) {
					created_inode[i->inode_number - 1] =
						(char *) i;
					total_blocks += (i->data +
						(block_size - 1)) >> block_log;
				}
				total_files ++;
			}
			total_inodes ++;
		}

		free_subdir(new);
		free(pathname);
	}

	squashfs_closedir(dir);

	return scan_res;
}


int dir_scan(char *parent_name, unsigned int start_block, unsigned int offset,
	struct pathnames *paths)
{
	unsigned int type;
	int scan_res = TRUE;
	char *name;
	struct pathnames *new;
	struct inode *i;
	struct dir *dir = s_ops->opendir(start_block, offset, &i);

	if(dir == NULL) {
		EXIT_UNSQUASH_IGNORE("dir_scan: failed to read directory %s\n",
			parent_name);
		return FALSE;
	}

	if((lsonly || info) && (!concise || dir->dir_count ==0))
		print_filename(parent_name, i);

	if(!lsonly) {
		/*
		 * Make directory with default User rwx permissions rather than
		 * the permissions from the filesystem, as these may not have
		 * write/execute permission.  These are fixed up later in
		 * set_attributes().
		 */
		int res = mkdir(parent_name, S_IRUSR|S_IWUSR|S_IXUSR);
		if(res == -1) {
			/*
			 * Skip directory if mkdir fails, unless we're
			 * forcing and the error is -EEXIST
			 */
			if(!force || errno != EEXIST) {
				EXIT_UNSQUASH_IGNORE("dir_scan: failed to make directory %s, "
					"because %s\n", parent_name,
					strerror(errno));
				squashfs_closedir(dir);
				return FALSE;
			} 

			/*
			 * Try to change permissions of existing directory so
			 * that we can write to it
			 */
			res = chmod(parent_name, S_IRUSR|S_IWUSR|S_IXUSR);
			if (res == -1) {
				EXIT_UNSQUASH_IGNORE("dir_scan: failed to change permissions "
					"for directory %s, because %s\n",
					parent_name, strerror(errno));
				squashfs_closedir(dir);
				return FALSE;
			}
		}
	}

	while(squashfs_readdir(dir, &name, &start_block, &offset, &type)) {
		char *pathname;
		int res;

		TRACE("dir_scan: name %s, start_block %d, offset %d, type %d\n",
			name, start_block, offset, type);


		if(!matches(paths, name, &new))
			continue;

		res = asprintf(&pathname, "%s/%s", parent_name, name);
		if(res == -1)
			EXIT_UNSQUASH("asprintf failed in dir_scan\n");

		if(type == SQUASHFS_DIR_TYPE) {
			res = dir_scan(pathname, start_block, offset, new);
			if(res == FALSE)
				scan_res = FALSE;
			free(pathname);
		} else if(new == NULL) {
			update_info(pathname);

			i = s_ops->read_inode(start_block, offset);

			if(lsonly || info)
				print_filename(pathname, i);

			if(!lsonly) {
				res = create_inode(pathname, i);
				if(res == FALSE)
					scan_res = FALSE;
			}

			if(i->type == SQUASHFS_SYMLINK_TYPE ||
					i->type == SQUASHFS_LSYMLINK_TYPE)
				free(i->symlink);
		} else
			free(pathname);

		free_subdir(new);
	}

	if(!lsonly)
		queue_dir(parent_name, dir);

	squashfs_closedir(dir);
	dir_count ++;

	return scan_res;
}


void squashfs_stat(char *source)
{
	time_t mkfs_time = (time_t) sBlk.s.mkfs_time;
	struct tm *t = use_localtime ? localtime(&mkfs_time) : gmtime(&mkfs_time);
	char *mkfs_str = asctime(t);

#if __BYTE_ORDER == __BIG_ENDIAN
	printf("Found a valid %sSQUASHFS %d:%d superblock on %s.\n",
		sBlk.s.s_major == 4 ? "" : swap ? "little endian " :
		"big endian ", sBlk.s.s_major, sBlk.s.s_minor, source);
#else
	printf("Found a valid %sSQUASHFS %d:%d superblock on %s.\n",
		sBlk.s.s_major == 4 ? "" : swap ? "big endian " :
		"little endian ", sBlk.s.s_major, sBlk.s.s_minor, source);
#endif

	printf("Creation or last append time %s", mkfs_str ? mkfs_str :
		"failed to get time\n");
	printf("Filesystem size %llu bytes (%.2f Kbytes / %.2f Mbytes)\n",
		sBlk.s.bytes_used, sBlk.s.bytes_used / 1024.0,
		sBlk.s.bytes_used / (1024.0 * 1024.0));

	if(sBlk.s.s_major == 4) {
		printf("Compression %s\n", comp->name);

		if(SQUASHFS_COMP_OPTS(sBlk.s.flags)) {
			char buffer[SQUASHFS_METADATA_SIZE] __attribute__ ((aligned));
			int bytes;

			if(!comp->supported)
				printf("\tCould not display compressor options, because %s compression is not supported\n",
						comp->name);
			else {
				bytes = read_block(fd, sizeof(sBlk.s), NULL, 0, buffer);
				if(bytes == 0) {
					ERROR("Failed to read compressor options\n");
					return;
				}

				compressor_display_options(comp, buffer, bytes);
			}
		}
	}

	printf("Block size %d\n", sBlk.s.block_size);
	printf("Filesystem is %sexportable via NFS\n",
		SQUASHFS_EXPORTABLE(sBlk.s.flags) ? "" : "not ");
	printf("Inodes are %scompressed\n",
		SQUASHFS_UNCOMPRESSED_INODES(sBlk.s.flags) ? "un" : "");
	printf("Data is %scompressed\n",
		SQUASHFS_UNCOMPRESSED_DATA(sBlk.s.flags) ? "un" : "");

	if(sBlk.s.s_major >= 4)
		printf("Uids/Gids (Id table) are %scompressed\n",
			SQUASHFS_UNCOMPRESSED_INODES(sBlk.s.flags) ||
			SQUASHFS_UNCOMPRESSED_IDS(sBlk.s.flags) ? "un" : "");

	if(sBlk.s.s_major > 1) {
		if(SQUASHFS_NO_FRAGMENTS(sBlk.s.flags))
			printf("Fragments are not stored\n");
		else {
			printf("Fragments are %scompressed\n",
				SQUASHFS_UNCOMPRESSED_FRAGMENTS(sBlk.s.flags) ?
				"un" : "");
			printf("Always-use-fragments option is %sspecified\n",
				SQUASHFS_ALWAYS_FRAGMENTS(sBlk.s.flags) ? "" :
				"not ");
		}
	}

	if(sBlk.s.s_major == 4) {
		if(SQUASHFS_NO_XATTRS(sBlk.s.flags))
			printf("Xattrs are not stored\n");
		else
			printf("Xattrs are %scompressed\n",
				SQUASHFS_UNCOMPRESSED_XATTRS(sBlk.s.flags) ?
				"un" : "");
	}

	if(sBlk.s.s_major < 4)
			printf("Check data is %spresent in the filesystem\n",
				SQUASHFS_CHECK_DATA(sBlk.s.flags) ? "" :
				"not ");

	if(sBlk.s.s_major > 1)
		printf("Duplicates are %sremoved\n",
			SQUASHFS_DUPLICATES(sBlk.s.flags) ? "" : "not ");
	else
		printf("Duplicates are removed\n");

	if(sBlk.s.s_major > 1)
		printf("Number of fragments %d\n", sBlk.s.fragments);

	printf("Number of inodes %d\n", sBlk.s.inodes);

	if(sBlk.s.s_major == 4)
		printf("Number of ids %d\n", sBlk.s.no_ids);
	else {
		printf("Number of uids %d\n", sBlk.no_uids);
		printf("Number of gids %d\n", sBlk.no_guids);
	}

	TRACE("sBlk.s.inode_table_start 0x%llx\n", sBlk.s.inode_table_start);
	TRACE("sBlk.s.directory_table_start 0x%llx\n",
		sBlk.s.directory_table_start);

	if(sBlk.s.s_major > 1)
		TRACE("sBlk.s.fragment_table_start 0x%llx\n\n",
			sBlk.s.fragment_table_start);

	if(sBlk.s.s_major > 2)
		TRACE("sBlk.s.lookup_table_start 0x%llx\n\n",
			sBlk.s.lookup_table_start);

	if(sBlk.s.s_major == 4) {
		TRACE("sBlk.s.id_table_start 0x%llx\n", sBlk.s.id_table_start);
		TRACE("sBlk.s.xattr_id_table_start 0x%llx\n",
			sBlk.s.xattr_id_table_start);
	} else {
		TRACE("sBlk.uid_start 0x%llx\n", sBlk.uid_start);
		TRACE("sBlk.guid_start 0x%llx\n", sBlk.guid_start);
	}
}


int check_compression(struct compressor *comp)
{
	int res, bytes = 0;
	char buffer[SQUASHFS_METADATA_SIZE] __attribute__ ((aligned));

	if(!comp->supported) {
		ERROR("Filesystem uses %s compression, this is "
			"unsupported by this version\n", comp->name);
		ERROR("Decompressors available:\n");
		display_compressors("", "");
		return FALSE;
	}

	/*
	 * Read compression options from disk if present, and pass to
	 * the compressor to ensure we know how to decompress a filesystem
	 * compressed with these compression options.
	 *
	 * Note, even if there is no compression options we still call the
	 * compressor because some compression options may be mandatory
	 * for some compressors.
	 */
	if(SQUASHFS_COMP_OPTS(sBlk.s.flags)) {
		bytes = read_block(fd, sizeof(sBlk.s), NULL, 0, buffer);
		if(bytes == 0) {
			ERROR("Failed to read compressor options\n");
			return FALSE;
		}
	}

	res = compressor_check_options(comp, sBlk.s.block_size, buffer, bytes);

	return res != -1;
}


int read_super(char *source)
{
	squashfs_super_block_3 sBlk_3;
	struct squashfs_super_block sBlk_4;

	/*
	 * Try to read a Squashfs 4 superblock
	 */
	read_fs_bytes(fd, SQUASHFS_START, sizeof(struct squashfs_super_block),
		&sBlk_4);
	swap = sBlk_4.s_magic != SQUASHFS_MAGIC;
	SQUASHFS_INSWAP_SUPER_BLOCK(&sBlk_4);

	if(sBlk_4.s_magic == SQUASHFS_MAGIC && sBlk_4.s_major == 4 &&
			sBlk_4.s_minor == 0) {
		read_filesystem_tables = read_filesystem_tables_4;
		memcpy(&sBlk, &sBlk_4, sizeof(sBlk_4));

		/*
		 * Check the compression type
		 */
		comp = lookup_compressor_id(sBlk.s.compression);
		return TRUE;
	}

	/*
 	 * Not a Squashfs 4 superblock, try to read a squashfs 3 superblock
 	 * (compatible with 1 and 2 filesystems)
 	 */
	read_fs_bytes(fd, SQUASHFS_START, sizeof(squashfs_super_block_3),
		&sBlk_3);

	/*
	 * Check it is a SQUASHFS superblock
	 */
	swap = 0;
	if(sBlk_3.s_magic != SQUASHFS_MAGIC) {
		if(sBlk_3.s_magic == SQUASHFS_MAGIC_SWAP) {
			squashfs_super_block_3 sblk;
			ERROR("Reading a different endian SQUASHFS filesystem "
				"on %s\n", source);
			SQUASHFS_SWAP_SUPER_BLOCK_3(&sblk, &sBlk_3);
			memcpy(&sBlk_3, &sblk, sizeof(squashfs_super_block_3));
			swap = 1;
		} else  {
			ERROR("Can't find a SQUASHFS superblock on %s\n",
				source);
			goto failed_mount;
		}
	}

	sBlk.s.s_magic = sBlk_3.s_magic;
	sBlk.s.inodes = sBlk_3.inodes;
	sBlk.s.mkfs_time = sBlk_3.mkfs_time;
	sBlk.s.block_size = sBlk_3.block_size;
	sBlk.s.fragments = sBlk_3.fragments;
	sBlk.s.block_log = sBlk_3.block_log;
	sBlk.s.flags = sBlk_3.flags;
	sBlk.s.s_major = sBlk_3.s_major;
	sBlk.s.s_minor = sBlk_3.s_minor;
	sBlk.s.root_inode = sBlk_3.root_inode;
	sBlk.s.bytes_used = sBlk_3.bytes_used;
	sBlk.s.inode_table_start = sBlk_3.inode_table_start;
	sBlk.s.directory_table_start = sBlk_3.directory_table_start;
	sBlk.s.fragment_table_start = sBlk_3.fragment_table_start;
	sBlk.s.lookup_table_start = sBlk_3.lookup_table_start;
	sBlk.no_uids = sBlk_3.no_uids;
	sBlk.no_guids = sBlk_3.no_guids;
	sBlk.uid_start = sBlk_3.uid_start;
	sBlk.guid_start = sBlk_3.guid_start;
	sBlk.s.xattr_id_table_start = SQUASHFS_INVALID_BLK;

	/* Check the MAJOR & MINOR versions */
	if(sBlk.s.s_major == 1 || sBlk.s.s_major == 2) {
		sBlk.s.bytes_used = sBlk_3.bytes_used_2;
		sBlk.uid_start = sBlk_3.uid_start_2;
		sBlk.guid_start = sBlk_3.guid_start_2;
		sBlk.s.inode_table_start = sBlk_3.inode_table_start_2;
		sBlk.s.directory_table_start = sBlk_3.directory_table_start_2;
		
		if(sBlk.s.s_major == 1) {
			sBlk.s.block_size = sBlk_3.block_size_1;
			sBlk.s.fragment_table_start = sBlk.uid_start;
			read_filesystem_tables = read_filesystem_tables_1;
		} else {
			sBlk.s.fragment_table_start =
				sBlk_3.fragment_table_start_2;
			read_filesystem_tables = read_filesystem_tables_2;
		}
	} else if(sBlk.s.s_major == 3) {
		read_filesystem_tables = read_filesystem_tables_3;
	} else {
		ERROR("Filesystem on %s is (%d:%d), ", source, sBlk.s.s_major,
			sBlk.s.s_minor);
		ERROR("which is a later filesystem version than I support!\n");
		goto failed_mount;
	}

	/*
	 * 1.x, 2.x and 3.x filesystems use gzip compression.
	 */
	comp = lookup_compressor("gzip");
	return TRUE;

failed_mount:
	return FALSE;
}


struct pathname *process_extract_files(struct pathname *path, char *filename)
{
	FILE *fd;
	char buffer[MAX_LINE + 1]; /* overflow safe */
	char *name;

	fd = fopen(filename, "r");
	if(fd == NULL)
		EXIT_UNSQUASH("Failed to open extract file \"%s\" because %s\n",
			filename, strerror(errno));

	while(fgets(name = buffer, MAX_LINE + 1, fd) != NULL) {
		int len = strlen(name);

		if(len == MAX_LINE && name[len - 1] != '\n')
			/* line too large */
			EXIT_UNSQUASH("Line too long when reading "
				"extract file \"%s\", larger than %d "
				"bytes\n", filename, MAX_LINE);

		/*
		 * Remove '\n' terminator if it exists (the last line
		 * in the file may not be '\n' terminated)
		 */
		if(len && name[len - 1] == '\n')
			name[len - 1] = '\0';

		/* Skip any leading whitespace */
		while(isspace(*name))
			name ++;

		/* if comment line, skip */
		if(*name == '#')
			continue;

		/* check for initial backslash, to accommodate
		 * filenames with leading space or leading # character
		 */
		if(*name == '\\')
			name ++;

		/* if line is now empty after skipping characters, skip it */
		if(*name == '\0')
			continue;

		path = add_path(path, name, name);
	}

	if(ferror(fd))
		EXIT_UNSQUASH("Reading extract file \"%s\" failed because %s\n",
			filename, strerror(errno));

	fclose(fd);
	return path;
}
		

/*
 * reader thread.  This thread processes read requests queued by the
 * cache_get() routine.
 */
void *reader(void *arg)
{
	while(1) {
		struct cache_entry *entry = queue_get(to_reader);
		int res = read_fs_bytes(fd, entry->block,
			SQUASHFS_COMPRESSED_SIZE_BLOCK(entry->size),
			entry->data);

		if(res && SQUASHFS_COMPRESSED_BLOCK(entry->size))
			/*
			 * queue successfully read block to the inflate
			 * thread(s) for further processing
 			 */
			queue_put(to_inflate, entry);
		else
			/*
			 * block has either been successfully read and is
			 * uncompressed, or an error has occurred, clear pending
			 * flag, set error appropriately, and wake up any
			 * threads waiting on this buffer
			 */
			cache_block_ready(entry, !res);
	}
}


/*
 * writer thread.  This processes file write requests queued by the
 * write_file() routine.
 */
void *writer(void *arg)
{
	int i;
	long failed = FALSE;

	while(1) {
		struct squashfs_file *file = queue_get(to_writer);
		int file_fd;
		long long hole = 0;
		int res;

		if(file == NULL) {
			queue_put(from_writer, (void *) failed);
			continue;
		} else if(file->fd == -1) {
			/* write attributes for directory file->pathname */
			res = set_attributes(file->pathname, file->mode, file->uid,
				file->gid, file->time, file->xattr, TRUE);
			if(res == FALSE)
				failed = TRUE;
			free(file->pathname);
			free(file);
			continue;
		}

		TRACE("writer: regular file, blocks %d\n", file->blocks);

		file_fd = file->fd;

		for(i = 0; i < file->blocks; i++, cur_blocks ++) {
			struct file_entry *block = queue_get(to_writer);

			if(block->buffer == 0) { /* sparse file */
				hole += block->size;
				free(block);
				continue;
			}

			cache_block_wait(block->buffer);

			if(block->buffer->error) {
				EXIT_UNSQUASH_IGNORE("writer: failed to read/uncompress file %s\n", file->pathname);
				failed = TRUE;
			}

			if(failed)
				continue;

			res = write_block(file_fd, block->buffer->data +
				block->offset, block->size, hole, file->sparse);

			if(res == FALSE) {
				EXIT_UNSQUASH_IGNORE("writer: failed to write file %s\n", file->pathname);
				failed = TRUE;
			}

			hole = 0;
			cache_block_put(block->buffer);
			free(block);
		}

		if(hole && failed == FALSE) {
			/*
			 * corner case for hole extending to end of file
			 */
			if(file->sparse == FALSE ||
					lseek(file_fd, hole, SEEK_CUR) == -1) {
				/*
				 * for files which we don't want to write
				 * sparsely, or for broken lseeks which cannot
				 * seek beyond end of file, write_block will do
				 * the right thing
				 */
				hole --;
				if(write_block(file_fd, "\0", 1, hole,
						file->sparse) == FALSE) {
					EXIT_UNSQUASH_IGNORE("writer: failed to write sparse "
						"data block for file %s\n", file->pathname);
					failed = TRUE;
				}
			} else if(ftruncate(file_fd, file->file_size) == -1) {
				EXIT_UNSQUASH_IGNORE("writer: failed to write sparse data "
					"block for file %s\n", file->pathname);
				failed = TRUE;
			}
		}

		close_wake(file_fd);
		if(failed == FALSE) {
			res = set_attributes(file->pathname, file->mode, file->uid,
				file->gid, file->time, file->xattr, force);
			if(res == FALSE)
				failed = TRUE;
		} else
			unlink(file->pathname);
		free(file->pathname);
		free(file);

	}
}


/*
 * decompress thread.  This decompresses buffers queued by the read thread
 */
void *inflator(void *arg)
{
	char *tmp = malloc(block_size);

	if(tmp == NULL)
		EXIT_UNSQUASH("inflator: Failed to allocate block buffer\n");

	while(1) {
		struct cache_entry *entry = queue_get(to_inflate);
		int error, res;

		res = compressor_uncompress(comp, tmp, entry->data,
			SQUASHFS_COMPRESSED_SIZE_BLOCK(entry->size), block_size,
			&error);

		if(res == -1)
			ERROR("%s uncompress failed with error code %d\n",
				comp->name, error);
		else
			memcpy(entry->data, tmp, res);

		/*
		 * block has been either successfully decompressed, or an error
 		 * occurred, clear pending flag, set error appropriately and
 		 * wake up any threads waiting on this block
 		 */ 
		cache_block_ready(entry, res == -1);
	}
}


void *progress_thread(void *arg)
{
	struct timespec requested_time, remaining;
	struct itimerval itimerval;
	struct winsize winsize;

	if(ioctl(1, TIOCGWINSZ, &winsize) == -1) {
		if(isatty(STDOUT_FILENO))
			ERROR("TIOCGWINSZ ioctl failed, defaulting to 80 "
				"columns\n");
		columns = 80;
	} else
		columns = winsize.ws_col;
	signal(SIGWINCH, sigwinch_handler);
	signal(SIGALRM, sigalrm_handler);

	itimerval.it_value.tv_sec = 0;
	itimerval.it_value.tv_usec = 250000;
	itimerval.it_interval.tv_sec = 0;
	itimerval.it_interval.tv_usec = 250000;
	setitimer(ITIMER_REAL, &itimerval, NULL);

	requested_time.tv_sec = 0;
	requested_time.tv_nsec = 250000000;

	while(1) {
		int res = nanosleep(&requested_time, &remaining);

		if(res == -1 && errno != EINTR)
			EXIT_UNSQUASH("nanosleep failed in progress thread\n");

		if(progress_enabled) {
			pthread_mutex_lock(&screen_mutex);
			progress_bar(sym_count + dev_count +
				fifo_count + cur_blocks, total_inodes -
				total_files + total_blocks, columns);
			pthread_mutex_unlock(&screen_mutex);
		}
	}
}


void initialise_threads(int fragment_buffer_size, int data_buffer_size)
{
	struct rlimit rlim;
	int i, max_files, res;
	sigset_t sigmask, old_mask;

	/* block SIGQUIT and SIGHUP, these are handled by the info thread */
	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGQUIT);
	sigaddset(&sigmask, SIGHUP);
	if(pthread_sigmask(SIG_BLOCK, &sigmask, NULL) != 0)
		EXIT_UNSQUASH("Failed to set signal mask in initialise_threads"
			"\n");

	/*
	 * temporarily block these signals so the created sub-threads will
	 * ignore them, ensuring the main thread handles them
	 */
	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGTERM);
	if(pthread_sigmask(SIG_BLOCK, &sigmask, &old_mask) != 0)
		EXIT_UNSQUASH("Failed to set signal mask in initialise_threads"
			"\n");

	if(processors == -1) {
#ifndef linux
		int mib[2];
		size_t len = sizeof(processors);

		mib[0] = CTL_HW;
#ifdef HW_AVAILCPU
		mib[1] = HW_AVAILCPU;
#else
		mib[1] = HW_NCPU;
#endif

		if(sysctl(mib, 2, &processors, &len, NULL, 0) == -1) {
			ERROR("Failed to get number of available processors.  "
				"Defaulting to 1\n");
			processors = 1;
		}
#else
		processors = sysconf(_SC_NPROCESSORS_ONLN);
#endif
	}

	if(add_overflow(processors, 3) ||
			multiply_overflow(processors + 3, sizeof(pthread_t)))
		EXIT_UNSQUASH("Processors too large\n");

	thread = malloc((3 + processors) * sizeof(pthread_t));
	if(thread == NULL)
		EXIT_UNSQUASH("Out of memory allocating thread descriptors\n");
	inflator_thread = &thread[3];

	/*
	 * dimensioning the to_reader and to_inflate queues.  The size of
	 * these queues is directly related to the amount of block
	 * read-ahead possible.  To_reader queues block read requests to
	 * the reader thread and to_inflate queues block decompression
	 * requests to the inflate thread(s) (once the block has been read by
	 * the reader thread).  The amount of read-ahead is determined by
	 * the combined size of the data_block and fragment caches which
	 * determine the total number of blocks which can be "in flight"
	 * at any one time (either being read or being decompressed)
	 *
	 * The maximum file open limit, however, affects the read-ahead
	 * possible, in that for normal sizes of the fragment and data block
	 * caches, where the incoming files have few data blocks or one fragment
	 * only, the file open limit is likely to be reached before the
	 * caches are full.  This means the worst case sizing of the combined
	 * sizes of the caches is unlikely to ever be necessary.  However, is is
	 * obvious read-ahead up to the data block cache size is always possible
	 * irrespective of the file open limit, because a single file could
	 * contain that number of blocks.
	 *
	 * Choosing the size as "file open limit + data block cache size" seems
	 * to be a reasonable estimate.  We can reasonably assume the maximum
	 * likely read-ahead possible is data block cache size + one fragment
	 * per open file.
	 *
	 * dimensioning the to_writer queue.  The size of this queue is
	 * directly related to the amount of block read-ahead possible.
	 * However, unlike the to_reader and to_inflate queues, this is
	 * complicated by the fact the to_writer queue not only contains
	 * entries for fragments and data_blocks but it also contains
	 * file entries, one per open file in the read-ahead.
	 *
	 * Choosing the size as "2 * (file open limit) +
	 * data block cache size" seems to be a reasonable estimate.
	 * We can reasonably assume the maximum likely read-ahead possible
	 * is data block cache size + one fragment per open file, and then
	 * we will have a file_entry for each open file.
	 */
	res = getrlimit(RLIMIT_NOFILE, &rlim);
	if (res == -1) {
		ERROR("failed to get open file limit!  Defaulting to 1\n");
		rlim.rlim_cur = 1;
	}

	if (rlim.rlim_cur != RLIM_INFINITY) {
		/*
		 * leave OPEN_FILE_MARGIN free (rlim_cur includes fds used by
		 * stdin, stdout, stderr and filesystem fd
		 */
		if (rlim.rlim_cur <= OPEN_FILE_MARGIN)
			/* no margin, use minimum possible */
			max_files = 1;
		else
			max_files = rlim.rlim_cur - OPEN_FILE_MARGIN;
	} else
		max_files = -1;

	/* set amount of available files for use by open_wait and close_wake */
	open_init(max_files);

	/*
	 * allocate to_reader, to_inflate and to_writer queues.  Set based on
	 * open file limit and cache size, unless open file limit is unlimited,
	 * in which case set purely based on cache limits
	 *
	 * In doing so, check that the user supplied values do not overflow
	 * a signed int
	 */
	if (max_files != -1) {
		if(add_overflow(data_buffer_size, max_files) ||
				add_overflow(data_buffer_size, max_files * 2))
			EXIT_UNSQUASH("Data queue size is too large\n");

		to_reader = queue_init(max_files + data_buffer_size);
		to_inflate = queue_init(max_files + data_buffer_size);
		to_writer = queue_init(max_files * 2 + data_buffer_size);
	} else {
		int all_buffers_size;

		if(add_overflow(fragment_buffer_size, data_buffer_size))
			EXIT_UNSQUASH("Data and fragment queues combined are"
							" too large\n");

		all_buffers_size = fragment_buffer_size + data_buffer_size;

		if(add_overflow(all_buffers_size, all_buffers_size))
			EXIT_UNSQUASH("Data and fragment queues combined are"
							" too large\n");

		to_reader = queue_init(all_buffers_size);
		to_inflate = queue_init(all_buffers_size);
		to_writer = queue_init(all_buffers_size * 2);
	}

	from_writer = queue_init(1);

	fragment_cache = cache_init(block_size, fragment_buffer_size);
	data_cache = cache_init(block_size, data_buffer_size);
	pthread_create(&thread[0], NULL, reader, NULL);
	pthread_create(&thread[1], NULL, writer, NULL);
	pthread_create(&thread[2], NULL, progress_thread, NULL);
	init_info();
	pthread_mutex_init(&fragment_mutex, NULL);

	for(i = 0; i < processors; i++) {
		if(pthread_create(&inflator_thread[i], NULL, inflator, NULL) !=
				 0)
			EXIT_UNSQUASH("Failed to create thread\n");
	}

	if(pthread_sigmask(SIG_SETMASK, &old_mask, NULL) != 0)
		EXIT_UNSQUASH("Failed to set signal mask in initialise_threads"
			"\n");
}


void enable_progress_bar()
{
	pthread_mutex_lock(&screen_mutex);
	progress_enabled = progress;
	pthread_mutex_unlock(&screen_mutex);
}


void disable_progress_bar()
{
	pthread_mutex_lock(&screen_mutex);
	if(progress_enabled) {
		progress_bar(sym_count + dev_count + fifo_count + cur_blocks,
			total_inodes - total_files + total_blocks, columns);
		printf("\n");
	}
	progress_enabled = FALSE;
	pthread_mutex_unlock(&screen_mutex);
}


void progressbar_error(char *fmt, ...)
{
	va_list ap;

	pthread_mutex_lock(&screen_mutex);

	if(progress_enabled)
		fprintf(stderr, "\n");

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	pthread_mutex_unlock(&screen_mutex);
}


void progressbar_info(char *fmt, ...)
{
	va_list ap;

	pthread_mutex_lock(&screen_mutex);

	if(progress_enabled)
		printf("\n");

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

	pthread_mutex_unlock(&screen_mutex);
}
static int get_max_digits(long long max)
{
    int digits = 0;
    
    while (max > 10) {
        max /= 10;
        digits++;
    }

    return digits;
}

void progress_bar(long long current, long long max, int columns)
{
	char rotate_list[] = { '|', '/', '-', '\\' };
	int max_digits, used, hashes, spaces;
	static int tty = -1;

	if(max == 0)
		return;

	//max_digits = floor(log10(max)) + 1;
	max_digits = get_max_digits(max) + 1;
	used = max_digits * 2 + 11;
	hashes = (current * (columns - used)) / max;
	spaces = columns - used - hashes;

	if((current > max) || (columns - used < 0))
		return;

	if(tty == -1)
		tty = isatty(STDOUT_FILENO);
	if(!tty) {
		static long long previous = -1;

		/*
		 * Updating much more frequently than this results in huge
		 * log files.
		 */
		if((current % 100) != 0 && current != max)
			return;
		/* Don't update just to rotate the spinner. */
		if(current == previous)
			return;
		previous = current;
	}

	printf("\r[");

	while (hashes --)
		putchar('=');

	putchar(rotate_list[rotate]);

	while(spaces --)
		putchar(' ');

	printf("] %*lld/%*lld", max_digits, current, max_digits, max);
	printf(" %3lld%%", current * 100 / max);
	fflush(stdout);
}


int multiply_overflowll(long long a, int multiplier)
{
	return (LLONG_MAX / multiplier) < a;
}


int parse_numberll(char *start, long long *res, int size)
{
	char *end;
	long long number;

	errno = 0; /* To distinguish success/failure after call */

	number = strtoll(start, &end, 10);

	/*
	 * check for strtoll underflow or overflow in conversion, and other
	 * errors.
	 */
	if((errno == ERANGE && (number == LLONG_MIN || number == LLONG_MAX)) ||
			(errno != 0 && number == 0))
		return 0;

	/* reject negative numbers as invalid */
	if(number < 0)
		return 0;

	if(size) {
		/*
		 * Check for multiplier and trailing junk.
		 * But first check that a number exists before the
		 * multiplier
		 */
		if(end == start)
			return 0;

		switch(end[0]) {
		case 'g':
		case 'G':
			if(multiply_overflowll(number, 1073741824))
				return 0;
			number *= 1073741824;

			if(end[1] != '\0')
				/* trailing junk after multiplier, but
				 * allow it to be "bytes" */
				if(strcmp(end + 1, "bytes"))
					return 0;

			break;
		case 'm':
		case 'M':
			if(multiply_overflowll(number, 1048576))
				return 0;
			number *= 1048576;

			if(end[1] != '\0')
				/* trailing junk after multiplier, but
				 * allow it to be "bytes" */
				if(strcmp(end + 1, "bytes"))
					return 0;

			break;
		case 'k':
		case 'K':
			if(multiply_overflowll(number, 1024))
				return 0;
			number *= 1024;

			if(end[1] != '\0')
				/* trailing junk after multiplier, but
				 * allow it to be "bytes" */
				if(strcmp(end + 1, "bytes"))
					return 0;

			break;
		case '\0':
			break;
		default:
			/* trailing junk after number */
			return 0;
		}
	} else if(end[0] != '\0')
		/* trailing junk after number */
		return 0;

	*res = number;
	return 1;
}


int parse_number(char *start, int *res)
{
	long long number;

	if(!parse_numberll(start, &number, 0))
		return 0;

	/* check if long result will overflow signed int */
	if(number > INT_MAX)
		return 0;

	*res = (int) number;
	return 1;
}


int ventoy_parse_disk_map(void)
{
    int len = 0;

    debug("ventoy_parse_disk_map\n");

    len = (int)lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    if (len < sizeof(fs_disk_map) + sizeof(fs_disk_region))
    {
        return 1;
    }

    read(fd, &g_fs_disk_map, sizeof(fs_disk_map));

    debug("diskname=<%s> filesize=<%llu> region_num=<%u>\n", 
          g_fs_disk_map.diskname, g_fs_disk_map.filesize, g_fs_region_num);

    g_fs_region_num = (len - sizeof(fs_disk_map)) / sizeof(fs_disk_region);
    g_fs_region_list = malloc(g_fs_region_num * sizeof(fs_disk_region));
    read(fd, g_fs_region_list, g_fs_region_num * sizeof(fs_disk_region));

    close(fd);    
    
    fd = open(g_fs_disk_map.diskname, O_RDONLY);
    debug("ventoy_parse_disk_map end fd=%d\n", fd);
    
    return 0;
}


#define VERSION() \
	printf("unsquashfs version 4.4 (2019/08/29)\n");\
	printf("copyright (C) 2019 Phillip Lougher "\
		"<phillip@squashfs.org.uk>\n\n");\
    	printf("This program is free software; you can redistribute it and/or"\
		"\n");\
	printf("modify it under the terms of the GNU General Public License"\
		"\n");\
	printf("as published by the Free Software Foundation; either version "\
		"2,\n");\
	printf("or (at your option) any later version.\n\n");\
	printf("This program is distributed in the hope that it will be "\
		"useful,\n");\
	printf("but WITHOUT ANY WARRANTY; without even the implied warranty of"\
		"\n");\
	printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the"\
		"\n");\
	printf("GNU General Public License for more details.\n");
int main(int argc, char *argv[])
{
	char *dest = "squashfs-root";
	int i, stat_sys = FALSE, version = FALSE, mkfs_time_opt = FALSE;
	int n;
	struct pathnames *paths = NULL;
	struct pathname *path = NULL;
	int fragment_buffer_size = FRAGMENT_BUFFER_DEFAULT;
	int data_buffer_size = DATA_BUFFER_DEFAULT;
	long res;
	int exit_code = 0;

	pthread_mutex_init(&screen_mutex, NULL);
	root_process = geteuid() == 0;
	if(root_process)
		umask(0);
	
	for(i = 1; i < argc; i++) {
		if(*argv[i] != '-')
			break;
		if(strcmp(argv[i], "-UTC") == 0)
			use_localtime = FALSE;
        else if (strcmp(argv[i], "-t") == 0)
            return 0;
        else if (strcmp(argv[i], "-v") == 0)
            verbose = 1;
		else if(strcmp(argv[i], "-strict-errors") == 0 ||
				strcmp(argv[i], "-st") == 0)
			strict_errors = TRUE;
		else if(strcmp(argv[i], "-ignore-errors") == 0 ||
				strcmp(argv[i], "-ig") == 0)
			ignore_errors = TRUE;
		else if(strcmp(argv[i], "-quiet") == 0 ||
				strcmp(argv[i], "-q") == 0)
			quiet = TRUE;
		else if(strcmp(argv[i], "-version") == 0 ||
				strcmp(argv[i], "-v") == 0) {
			VERSION();
			version = TRUE;
		} else if(strcmp(argv[i], "-info") == 0 ||
				strcmp(argv[i], "-i") == 0)
			info = TRUE;
		else if(strcmp(argv[i], "-ls") == 0 ||
				strcmp(argv[i], "-l") == 0)
			lsonly = TRUE;
		else if(strcmp(argv[i], "-lc") == 0) {
			lsonly = TRUE;
			concise = TRUE;
		} else if(strcmp(argv[i], "-no-progress") == 0 ||
				strcmp(argv[i], "-n") == 0)
			progress = FALSE;
		else if(strcmp(argv[i], "-no-xattrs") == 0 ||
				strcmp(argv[i], "-no") == 0)
			no_xattrs = TRUE;
		else if(strcmp(argv[i], "-xattrs") == 0 ||
				strcmp(argv[i], "-x") == 0)
			no_xattrs = FALSE;
		else if(strcmp(argv[i], "-user-xattrs") == 0 ||
				strcmp(argv[i], "-u") == 0) {
			user_xattrs = TRUE;
			no_xattrs = FALSE;
		} else if(strcmp(argv[i], "-dest") == 0 ||
				strcmp(argv[i], "-d") == 0) {
			if(++i == argc) {
				fprintf(stderr, "%s: -dest missing filename\n",
					argv[0]);
				exit(1);
			}
			dest = argv[i];
		} else if(strcmp(argv[i], "-processors") == 0 ||
				strcmp(argv[i], "-p") == 0) {
			if((++i == argc) || 
					!parse_number(argv[i],
						&processors)) {
				ERROR("%s: -processors missing or invalid "
					"processor number\n", argv[0]);
				exit(1);
			}
			if(processors < 1) {
				ERROR("%s: -processors should be 1 or larger\n",
					argv[0]);
				exit(1);
			}
		} else if(strcmp(argv[i], "-data-queue") == 0 ||
					 strcmp(argv[i], "-da") == 0) {
			if((++i == argc) ||
					!parse_number(argv[i],
						&data_buffer_size)) {
				ERROR("%s: -data-queue missing or invalid "
					"queue size\n", argv[0]);
				exit(1);
			}
			if(data_buffer_size < 1) {
				ERROR("%s: -data-queue should be 1 Mbyte or "
					"larger\n", argv[0]);
				exit(1);
			}
		} else if(strcmp(argv[i], "-frag-queue") == 0 ||
					strcmp(argv[i], "-fr") == 0) {
			if((++i == argc) ||
					!parse_number(argv[i],
						&fragment_buffer_size)) {
				ERROR("%s: -frag-queue missing or invalid "
					"queue size\n", argv[0]);
				exit(1);
			}
			if(fragment_buffer_size < 1) {
				ERROR("%s: -frag-queue should be 1 Mbyte or "
					"larger\n", argv[0]);
				exit(1);
			}
		} else if(strcmp(argv[i], "-force") == 0 ||
				strcmp(argv[i], "-f") == 0)
			force = TRUE;
		else if(strcmp(argv[i], "-stat") == 0 ||
				strcmp(argv[i], "-s") == 0)
			stat_sys = TRUE;
		else if(strcmp(argv[i], "-mkfs-time") == 0 ||
				strcmp(argv[i], "-fstime") == 0)
			mkfs_time_opt = TRUE;
		else if(strcmp(argv[i], "-lls") == 0 ||
				strcmp(argv[i], "-ll") == 0) {
			lsonly = TRUE;
			short_ls = FALSE;
		} else if(strcmp(argv[i], "-llnumeric") == 0 ||
				strcmp(argv[i], "-lln") == 0) {
			lsonly = TRUE;
			short_ls = FALSE;
			numeric = TRUE;
		} else if(strcmp(argv[i], "-llc") == 0) {
			lsonly = TRUE;
			short_ls = FALSE;
			concise = TRUE;
		} else if(strcmp(argv[i], "-linfo") == 0 ||
				strcmp(argv[i], "-li") == 0) {
			info = TRUE;
			short_ls = FALSE;
		} else if(strcmp(argv[i], "-ef") == 0 ||
				strcmp(argv[i], "-e") == 0) {
			if(++i == argc) {
				fprintf(stderr, "%s: -ef missing filename\n",
					argv[0]);
				exit(1);
			}
			path = process_extract_files(path, argv[i]);
		} else if(strcmp(argv[i], "-regex") == 0 ||
				strcmp(argv[i], "-r") == 0)
			use_regex = TRUE;
		else if(strcmp(argv[i], "-offset") == 0 || strcmp(argv[i], "-o") == 0) {
			if((++i == argc) || !parse_numberll(argv[i], &start_offset, 1)) {
				ERROR("%s: %s missing or invalid offset size\n", argv[0], argv[i - 1]);
				exit(1);
			}
		} else
			goto options;
	}

	if(lsonly || info)
		progress = FALSE;

	if(strict_errors && ignore_errors)
		EXIT_UNSQUASH("Both -strict-errors and -ignore-errors should not be set\n");

#ifdef SQUASHFS_TRACE
	/*
	 * Disable progress bar if full debug tracing is enabled.
	 * The progress bar in this case just gets in the way of the
	 * debug trace output
	 */
	progress = FALSE;
#endif

	if(i == argc) {
		if(!version) {
options:
			ERROR("SYNTAX: %s [options] filesystem [directories or "
				"files to extract]\n", argv[0]);
			ERROR("\t-v[ersion]\t\tprint version, licence and "
				"copyright information\n");
			ERROR("\t-d[est] <pathname>\tunsquash to <pathname>, "
				"default \"squashfs-root\"\n");
			ERROR("\t-q[uiet]\t\tno verbose output\n");
			ERROR("\t-n[o-progress]\t\tdon't display the progress "
				"bar\n");
			ERROR("\t-no[-xattrs]\t\tdon't extract xattrs in file system"
				NOXOPT_STR"\n");
			ERROR("\t-x[attrs]\t\textract xattrs in file system"
				XOPT_STR "\n");
			ERROR("\t-u[ser-xattrs]\t\tonly extract user xattrs in "
				"file system.\n\t\t\t\tEnables extracting "
				"xattrs\n");
			ERROR("\t-p[rocessors] <number>\tuse <number> "
				"processors.  By default will use\n");
			ERROR("\t\t\t\tnumber of processors available\n");
			ERROR("\t-i[nfo]\t\t\tprint files as they are "
				"unsquashed\n");
			ERROR("\t-li[nfo]\t\tprint files as they are "
				"unsquashed with file\n");
			ERROR("\t\t\t\tattributes (like ls -l output)\n");
			ERROR("\t-l[s]\t\t\tlist filesystem, but don't unsquash"
				"\n");
			ERROR("\t-ll[s]\t\t\tlist filesystem with file "
				"attributes (like\n");
			ERROR("\t\t\t\tls -l output), but don't unsquash\n");
			ERROR("\t-lln[umeric]\t\t-lls but with numeric uids and gids\n");
			ERROR("\t-lc\t\t\tlist filesystem concisely, displaying only"
				" files\n\t\t\t\tand empty directories.  Don't unsquash\n");
			ERROR("\t-llc\t\t\tlist filesystem concisely with file attributes,"
				"\n\t\t\t\tdisplaying only files and empty directories.\n\t\t\t\tDon't unsquash\n");
			ERROR("\t-o[ffset] <bytes>\tskip <bytes> at start of <dest>\n");
			ERROR("\t\t\t\tOptionally a suffix of K, M or G can be"
				" given to specify\n\t\t\t\tKbytes, Mbytes or"
				" Gbytes respectively.\n");
			ERROR("\t\t\t\tDefault 0 bytes.\n");
			ERROR("\t-f[orce]\t\tif file already exists then "
				"overwrite\n");
			ERROR("\t-ig[nore-errors]\tTreat errors writing files "
				"to output as non-fatal\n");
			ERROR("\t-st[rict-errors]\tTreat all errors as fatal\n");
			ERROR("\t-s[tat]\t\t\tdisplay filesystem superblock "
				"information\n");
			ERROR("\t-UTC\t\t\tUse UTC rather than local time zone when displaying time\n");
			ERROR("\t-mkfs-time\t\tdisplay filesystem superblock time\n");
			ERROR("\t-fstime\t\t\tsynonym for -mkfs-time\n");
			ERROR("\t-e[f] <extract file>\tlist of directories or "
				"files to extract.\n\t\t\t\tOne per line\n");
			ERROR("\t-da[ta-queue] <size>\tSet data queue to "
				"<size> Mbytes.  Default %d\n\t\t\t\tMbytes\n",
				DATA_BUFFER_DEFAULT);
			ERROR("\t-fr[ag-queue] <size>\tSet fragment queue to "
				"<size> Mbytes.  Default\n\t\t\t\t%d Mbytes\n",
				FRAGMENT_BUFFER_DEFAULT);
			ERROR("\t-r[egex]\t\ttreat extract names as POSIX "
				"regular expressions\n");
			ERROR("\t\t\t\trather than use the default shell "
				"wildcard\n\t\t\t\texpansion (globbing)\n");
			ERROR("\nDecompressors available:\n");
			display_compressors("", "");
		}
		exit(1);
	}

	for(n = i + 1; n < argc; n++)
		path = add_path(path, argv[n], argv[n]);

	if((fd = open(argv[i], O_RDONLY)) == -1) {
		ERROR("Could not open %s, because %s\n", argv[i],
			strerror(errno));
		exit(1);
	}

    ventoy_parse_disk_map();

	if(read_super(argv[i]) == FALSE)
		exit(1);

	if(mkfs_time_opt) {
		printf("%u\n", sBlk.s.mkfs_time);
		exit(0);
	}

	if(stat_sys) {
		squashfs_stat(argv[i]);
		exit(0);
	}

	if(!check_compression(comp))
		exit(1);

	block_size = sBlk.s.block_size;
	block_log = sBlk.s.block_log;

	/*
	 * Sanity check block size and block log.
	 *
	 * Check they're within correct limits
	 */
	if(block_size > SQUASHFS_FILE_MAX_SIZE ||
					block_log > SQUASHFS_FILE_MAX_LOG)
		EXIT_UNSQUASH("Block size or block_log too large."
			"  File system is corrupt.\n");

	/*
	 * Check block_size and block_log match
	 */
	if(block_size != (1 << block_log))
		EXIT_UNSQUASH("Block size and block_log do not match."
			"  File system is corrupt.\n");

	/*
	 * convert from queue size in Mbytes to queue size in
	 * blocks.
	 *
	 * In doing so, check that the user supplied values do not
	 * overflow a signed int
	 */
	if(shift_overflow(fragment_buffer_size, 20 - block_log))
		EXIT_UNSQUASH("Fragment queue size is too large\n");
	else
		fragment_buffer_size <<= 20 - block_log;

	if(shift_overflow(data_buffer_size, 20 - block_log))
		EXIT_UNSQUASH("Data queue size is too large\n");
	else
		data_buffer_size <<= 20 - block_log;

	initialise_threads(fragment_buffer_size, data_buffer_size);

	created_inode = malloc(sBlk.s.inodes * sizeof(char *));
	if(created_inode == NULL)
		EXIT_UNSQUASH("failed to allocate created_inode\n");

	memset(created_inode, 0, sBlk.s.inodes * sizeof(char *));

	s_ops = read_filesystem_tables();
	if(s_ops == NULL)
		EXIT_UNSQUASH("failed to read file system tables\n");

	if(path) {
		paths = init_subdir();
		paths = add_subdir(paths, path);
	}

	if(!quiet || progress) {
		res = pre_scan(dest, SQUASHFS_INODE_BLK(sBlk.s.root_inode),
			SQUASHFS_INODE_OFFSET(sBlk.s.root_inode), paths);
		if(res == FALSE)
			exit_code = 1;

		memset(created_inode, 0, sBlk.s.inodes * sizeof(char *));
		inode_number = 1;

		if(!quiet)  {
			printf("Parallel unsquashfs: Using %d processor%s\n", processors,
					processors == 1 ? "" : "s");

			printf("%d inodes (%d blocks) to write\n\n", total_inodes,
					total_inodes - total_files + total_blocks);
		}

		enable_progress_bar();
	}

	res = dir_scan(dest, SQUASHFS_INODE_BLK(sBlk.s.root_inode),
		SQUASHFS_INODE_OFFSET(sBlk.s.root_inode), paths);
	if(res == FALSE)
		exit_code = 1;

	queue_put(to_writer, NULL);
	res = (long) queue_get(from_writer);
	if(res == TRUE)
		exit_code = 1;

	disable_progress_bar();

	if(!quiet && !lsonly) {
		printf("\n");
		printf("created %d files\n", file_count);
		printf("created %d directories\n", dir_count);
		printf("created %d symlinks\n", sym_count);
		printf("created %d devices\n", dev_count);
		printf("created %d fifos\n", fifo_count);
	}

    close(fd);    

	return exit_code;
}
