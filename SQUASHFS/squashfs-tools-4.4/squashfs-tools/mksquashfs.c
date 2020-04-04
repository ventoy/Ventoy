/*
 * Create a squashfs filesystem.  This is a highly compressed read only
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
 * mksquashfs.c
 */

#define FALSE 0
#define TRUE 1
#define MAX_LINE 16384

#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <pthread.h>
#include <regex.h>
#include <sys/wait.h>
#include <limits.h>
#include <ctype.h>
#include <sys/sysinfo.h>

#ifndef linux
#define __BYTE_ORDER BYTE_ORDER
#define __BIG_ENDIAN BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#include <sys/sysctl.h>
#else
#include <endian.h>
#include <sys/sysinfo.h>
#endif

#include "squashfs_fs.h"
#include "squashfs_swap.h"
#include "mksquashfs.h"
#include "sort.h"
#include "pseudo.h"
#include "compressor.h"
#include "xattr.h"
#include "action.h"
#include "error.h"
#include "progressbar.h"
#include "info.h"
#include "caches-queues-lists.h"
#include "read_fs.h"
#include "restore.h"
#include "process_fragments.h"
#include "fnmatch_compat.h"

int delete = FALSE;
int quiet = FALSE;
int fd;
struct squashfs_super_block sBlk;

/* filesystem flags for building */
int comp_opts = FALSE;
int no_xattrs = XATTR_DEF;
int noX = FALSE;
int duplicate_checking = TRUE;
int noF = FALSE;
int no_fragments = FALSE;
int always_use_fragments = FALSE;
int noI = FALSE;
int noId = FALSE;
int noD = FALSE;
int silent = TRUE;
int exportable = TRUE;
int sparse_files = TRUE;
int old_exclude = TRUE;
int use_regex = FALSE;
int nopad = FALSE;
int exit_on_error = FALSE;
long long start_offset = 0;

long long global_uid = -1, global_gid = -1;

/* superblock attributes */
int block_size = SQUASHFS_FILE_SIZE, block_log;
unsigned int id_count = 0;
int file_count = 0, sym_count = 0, dev_count = 0, dir_count = 0, fifo_count = 0,
	sock_count = 0;

/* write position within data section */
long long bytes = 0, total_bytes = 0;

/* in memory directory table - possibly compressed */
char *directory_table = NULL;
unsigned int directory_bytes = 0, directory_size = 0, total_directory_bytes = 0;

/* cached directory table */
char *directory_data_cache = NULL;
unsigned int directory_cache_bytes = 0, directory_cache_size = 0;

/* in memory inode table - possibly compressed */
char *inode_table = NULL;
unsigned int inode_bytes = 0, inode_size = 0, total_inode_bytes = 0;

/* cached inode table */
char *data_cache = NULL;
unsigned int cache_bytes = 0, cache_size = 0, inode_count = 0;

/* inode lookup table */
squashfs_inode *inode_lookup_table = NULL;

/* in memory directory data */
#define I_COUNT_SIZE		128
#define DIR_ENTRIES		32
#define INODE_HASH_SIZE		65536
#define INODE_HASH_MASK		(INODE_HASH_SIZE - 1)
#define INODE_HASH(dev, ino)	(ino & INODE_HASH_MASK)

struct cached_dir_index {
	struct squashfs_dir_index	index;
	char				*name;
};

struct directory {
	unsigned int		start_block;
	unsigned int		size;
	unsigned char		*buff;
	unsigned char		*p;
	unsigned int		entry_count;
	unsigned char		*entry_count_p;
	unsigned int		i_count;
	unsigned int		i_size;
	struct cached_dir_index	*index;
	unsigned char		*index_count_p;
	unsigned int		inode_number;
};

struct inode_info *inode_info[INODE_HASH_SIZE];

/* hash tables used to do fast duplicate searches in duplicate check */
struct file_info *dupl[65536];
int dup_files = 0;

/* exclude file handling */
/* list of exclude dirs/files */
struct exclude_info {
	dev_t			st_dev;
	ino_t			st_ino;
};

#define EXCLUDE_SIZE 8192
int exclude = 0;
struct exclude_info *exclude_paths = NULL;
int old_excluded(char *filename, struct stat *buf);

struct path_entry {
	char *name;
	regex_t *preg;
	struct pathname *paths;
};

struct pathname {
	int names;
	struct path_entry *name;
};

struct pathnames {
	int count;
	struct pathname *path[0];
};
#define PATHS_ALLOC_SIZE 10

struct pathnames *paths = NULL;
struct pathname *path = NULL;
struct pathname *stickypath = NULL;
int excluded(char *name, struct pathnames *paths, struct pathnames **new);

int fragments = 0;

#define FRAG_SIZE 32768

struct squashfs_fragment_entry *fragment_table = NULL;
int fragments_outstanding = 0;

int fragments_locked = FALSE;

/* current inode number for directories and non directories */
unsigned int inode_no = 1;
unsigned int root_inode_number = 0;

/* list of source dirs/files */
int source = 0;
char **source_path;

/* list of root directory entries read from original filesystem */
int old_root_entries = 0;
struct old_root_entry_info {
	char			*name;
	struct inode_info	inode;
};
struct old_root_entry_info *old_root_entry;

/* restore orignal filesystem state if appending to existing filesystem is
 * cancelled */
int appending = FALSE;
char *sdata_cache, *sdirectory_data_cache, *sdirectory_compressed;

long long sbytes, stotal_bytes;

unsigned int sinode_bytes, scache_bytes, sdirectory_bytes,
	sdirectory_cache_bytes, sdirectory_compressed_bytes,
	stotal_inode_bytes, stotal_directory_bytes,
	sinode_count = 0, sfile_count, ssym_count, sdev_count,
	sdir_count, sfifo_count, ssock_count, sdup_files;
int sfragments;
int threads;

/* flag whether destination file is a block device */
int block_device = FALSE;

/* flag indicating whether files are sorted using sort list(s) */
int sorted = FALSE;

/* save destination file name for deleting on error */
char *destination_file = NULL;

/* recovery file for abnormal exit on appending */
char *recovery_file = NULL;
int recover = TRUE;

struct id *id_hash_table[ID_ENTRIES];
struct id *id_table[SQUASHFS_IDS], *sid_table[SQUASHFS_IDS];
unsigned int uid_count = 0, guid_count = 0;
unsigned int sid_count = 0, suid_count = 0, sguid_count = 0;

struct cache *reader_buffer, *fragment_buffer, *reserve_cache;
struct cache *bwriter_buffer, *fwriter_buffer;
struct queue *to_reader, *to_deflate, *to_writer, *from_writer,
	*to_frag, *locked_fragment, *to_process_frag;
struct seq_queue *to_main;
pthread_t reader_thread, writer_thread, main_thread;
pthread_t *deflator_thread, *frag_deflator_thread, *frag_thread;
pthread_t *restore_thread = NULL;
pthread_mutex_t	fragment_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	pos_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	dup_mutex = PTHREAD_MUTEX_INITIALIZER;

/* reproducible image queues and threads */
struct seq_queue *to_order;
pthread_t order_thread;
pthread_cond_t fragment_waiting = PTHREAD_COND_INITIALIZER;

int reproducible = REP_DEF;

/* Root mode option */
int root_mode_opt = FALSE;
mode_t root_mode;

/* Time value over-ride options */
unsigned int mkfs_time;
int mkfs_time_opt = FALSE;

unsigned int all_time;
int all_time_opt = FALSE;
int clamping = TRUE;

/* user options that control parallelisation */
int processors = -1;
int bwriter_size;

/* compression operations */
struct compressor *comp = NULL;
int compressor_opt_parsed = FALSE;
void *stream = NULL;

/* xattr stats */
unsigned int xattr_bytes = 0, total_xattr_bytes = 0;

/* fragment to file mapping used when appending */
int append_fragments = 0;
struct append_file **file_mapping;

/* root of the in-core directory structure */
struct dir_info *root_dir;

/* log file */
FILE *log_fd;
int logging=FALSE;

static char *read_from_disk(long long start, unsigned int avail_bytes);
void add_old_root_entry(char *name, squashfs_inode inode, int inode_number,
	int type);
struct file_info *duplicate(long long file_size, long long bytes,
	unsigned int **block_list, long long *start, struct fragment **fragment,
	struct file_buffer *file_buffer, int blocks, unsigned short checksum,
	int checksum_flag);
struct dir_info *dir_scan1(char *, char *, struct pathnames *,
	struct dir_ent *(_readdir)(struct dir_info *), int);
void dir_scan2(struct dir_info *dir, struct pseudo *pseudo);
void dir_scan3(struct dir_info *dir);
void dir_scan4(struct dir_info *dir);
void dir_scan5(struct dir_info *dir);
void dir_scan6(struct dir_info *dir);
void dir_scan7(squashfs_inode *inode, struct dir_info *dir_info);
struct file_info *add_non_dup(long long file_size, long long bytes,
	unsigned int *block_list, long long start, struct fragment *fragment,
	unsigned short checksum, unsigned short fragment_checksum,
	int checksum_flag, int checksum_frag_flag);
long long generic_write_table(int, void *, int, void *, int);
void restorefs();
struct dir_info *scan1_opendir(char *pathname, char *subpath, int depth);
void write_filesystem_tables(struct squashfs_super_block *sBlk, int nopad);
unsigned short get_checksum_mem(char *buff, int bytes);
void check_usable_phys_mem(int total_mem);


void prep_exit()
{
	if(restore_thread) {
		if(pthread_self() == *restore_thread) {
			/*
			 * Recursive failure when trying to restore filesystem!
			 * Nothing to do except to exit, otherwise we'll just
			 * appear to hang.  The user should be able to restore
			 * from the recovery file (which is why it was added, in
			 * case of catastrophic failure in Mksquashfs)
			 */
			exit(1);
		} else {
			/* signal the restore thread to restore */
			pthread_kill(*restore_thread, SIGUSR1);
			pthread_exit(NULL);
		}
	} else if(delete) {
		if(destination_file && !block_device)
			unlink(destination_file);
	} else if(recovery_file)
		unlink(recovery_file);
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


int multiply_overflowll(long long a, int multiplier)
{
	return (LLONG_MAX / multiplier) < a;
}


#define MKINODE(A)	((squashfs_inode)(((squashfs_inode) inode_bytes << 16) \
			+ (((char *)A) - data_cache)))


void restorefs()
{
	ERROR("Exiting - restoring original filesystem!\n\n");

	bytes = sbytes;
	memcpy(data_cache, sdata_cache, cache_bytes = scache_bytes);
	memcpy(directory_data_cache, sdirectory_data_cache,
		sdirectory_cache_bytes);
	directory_cache_bytes = sdirectory_cache_bytes;
	inode_bytes = sinode_bytes;
	directory_bytes = sdirectory_bytes;
 	memcpy(directory_table + directory_bytes, sdirectory_compressed,
		sdirectory_compressed_bytes);
 	directory_bytes += sdirectory_compressed_bytes;
	total_bytes = stotal_bytes;
	total_inode_bytes = stotal_inode_bytes;
	total_directory_bytes = stotal_directory_bytes;
	inode_count = sinode_count;
	file_count = sfile_count;
	sym_count = ssym_count;
	dev_count = sdev_count;
	dir_count = sdir_count;
	fifo_count = sfifo_count;
	sock_count = ssock_count;
	dup_files = sdup_files;
	fragments = sfragments;
	id_count = sid_count;
	restore_xattrs();
	write_filesystem_tables(&sBlk, nopad);
	exit(1);
}


void sighandler()
{
	EXIT_MKSQUASHFS();
}


int mangle2(void *strm, char *d, char *s, int size,
	int block_size, int uncompressed, int data_block)
{
	int error, c_byte = 0;

	if(!uncompressed) {
		c_byte = compressor_compress(comp, strm, d, s, size, block_size,
			 &error);
		if(c_byte == -1)
			BAD_ERROR("mangle2:: %s compress failed with error "
				"code %d\n", comp->name, error);
	}

	if(c_byte == 0 || c_byte >= size) {
		memcpy(d, s, size);
		return size | (data_block ? SQUASHFS_COMPRESSED_BIT_BLOCK :
			SQUASHFS_COMPRESSED_BIT);
	}

	return c_byte;
}


int mangle(char *d, char *s, int size, int block_size,
	int uncompressed, int data_block)
{
	return mangle2(stream, d, s, size, block_size, uncompressed,
		data_block);
}


void *get_inode(int req_size)
{
	int data_space;
	unsigned short c_byte;

	while(cache_bytes >= SQUASHFS_METADATA_SIZE) {
		if((inode_size - inode_bytes) <
				((SQUASHFS_METADATA_SIZE << 1)) + 2) {
			void *it = realloc(inode_table, inode_size +
				(SQUASHFS_METADATA_SIZE << 1) + 2);
			if(it == NULL)
				MEM_ERROR();
			inode_table = it;
			inode_size += (SQUASHFS_METADATA_SIZE << 1) + 2;
		}

		c_byte = mangle(inode_table + inode_bytes + BLOCK_OFFSET,
			data_cache, SQUASHFS_METADATA_SIZE,
			SQUASHFS_METADATA_SIZE, noI, 0);
		TRACE("Inode block @ 0x%x, size %d\n", inode_bytes, c_byte);
		SQUASHFS_SWAP_SHORTS(&c_byte, inode_table + inode_bytes, 1);
		inode_bytes += SQUASHFS_COMPRESSED_SIZE(c_byte) + BLOCK_OFFSET;
		total_inode_bytes += SQUASHFS_METADATA_SIZE + BLOCK_OFFSET;
		memmove(data_cache, data_cache + SQUASHFS_METADATA_SIZE,
			cache_bytes - SQUASHFS_METADATA_SIZE);
		cache_bytes -= SQUASHFS_METADATA_SIZE;
	}

	data_space = (cache_size - cache_bytes);
	if(data_space < req_size) {
			int realloc_size = cache_size == 0 ?
				((req_size + SQUASHFS_METADATA_SIZE) &
				~(SQUASHFS_METADATA_SIZE - 1)) : req_size -
				data_space;

			void *dc = realloc(data_cache, cache_size +
				realloc_size);
			if(dc == NULL)
				MEM_ERROR();
			cache_size += realloc_size;
			data_cache = dc;
	}

	cache_bytes += req_size;

	return data_cache + cache_bytes - req_size;
}


int read_bytes(int fd, void *buff, int bytes)
{
	int res, count;

	for(count = 0; count < bytes; count += res) {
		res = read(fd, buff + count, bytes - count);
		if(res < 1) {
			if(res == 0)
				goto bytes_read;
			else if(errno != EINTR) {
				ERROR("Read failed because %s\n",
						strerror(errno));
				return -1;
			} else
				res = 0;
		}
	}

bytes_read:
	return count;
}


int read_fs_bytes(int fd, long long byte, int bytes, void *buff)
{
	off_t off = byte;
	int res = 1;

	TRACE("read_fs_bytes: reading from position 0x%llx, bytes %d\n",
		byte, bytes);

	pthread_cleanup_push((void *) pthread_mutex_unlock, &pos_mutex);
	pthread_mutex_lock(&pos_mutex);
	if(lseek(fd, start_offset + off, SEEK_SET) == -1) {
		ERROR("read_fs_bytes: Lseek on destination failed because %s, "
			"offset=0x%llx\n", strerror(errno), start_offset + off);
		res = 0;
	} else if(read_bytes(fd, buff, bytes) < bytes) {
		ERROR("Read on destination failed\n");
		res = 0;
	}

	pthread_cleanup_pop(1);
	return res;
}


int write_bytes(int fd, void *buff, int bytes)
{
	int res, count;

	for(count = 0; count < bytes; count += res) {
		res = write(fd, buff + count, bytes - count);
		if(res == -1) {
			if(errno != EINTR) {
				ERROR("Write failed because %s\n",
						strerror(errno));
				return -1;
			}
			res = 0;
		}
	}

	return 0;
}


void write_destination(int fd, long long byte, int bytes, void *buff)
{
	off_t off = byte;

	pthread_cleanup_push((void *) pthread_mutex_unlock, &pos_mutex);
	pthread_mutex_lock(&pos_mutex);

	if(lseek(fd, start_offset + off, SEEK_SET) == -1) {
		ERROR("write_destination: Lseek on destination "
			"failed because %s, offset=0x%llx\n", strerror(errno),
			start_offset + off);
		BAD_ERROR("Probably out of space on output %s\n",
			block_device ? "block device" : "filesystem");
	}

	if(write_bytes(fd, buff, bytes) == -1)
		BAD_ERROR("Failed to write to output %s\n",
			block_device ? "block device" : "filesystem");

	pthread_cleanup_pop(1);
}


long long write_inodes()
{
	unsigned short c_byte;
	int avail_bytes;
	char *datap = data_cache;
	long long start_bytes = bytes;

	while(cache_bytes) {
		if(inode_size - inode_bytes <
				((SQUASHFS_METADATA_SIZE << 1) + 2)) {
			void *it = realloc(inode_table, inode_size +
				((SQUASHFS_METADATA_SIZE << 1) + 2));
			if(it == NULL)
				MEM_ERROR();
			inode_size += (SQUASHFS_METADATA_SIZE << 1) + 2;
			inode_table = it;
		}
		avail_bytes = cache_bytes > SQUASHFS_METADATA_SIZE ?
			SQUASHFS_METADATA_SIZE : cache_bytes;
		c_byte = mangle(inode_table + inode_bytes + BLOCK_OFFSET, datap,
			avail_bytes, SQUASHFS_METADATA_SIZE, noI, 0);
		TRACE("Inode block @ 0x%x, size %d\n", inode_bytes, c_byte);
		SQUASHFS_SWAP_SHORTS(&c_byte, inode_table + inode_bytes, 1); 
		inode_bytes += SQUASHFS_COMPRESSED_SIZE(c_byte) + BLOCK_OFFSET;
		total_inode_bytes += avail_bytes + BLOCK_OFFSET;
		datap += avail_bytes;
		cache_bytes -= avail_bytes;
	}

	write_destination(fd, bytes, inode_bytes,  inode_table);
	bytes += inode_bytes;

	return start_bytes;
}


long long write_directories()
{
	unsigned short c_byte;
	int avail_bytes;
	char *directoryp = directory_data_cache;
	long long start_bytes = bytes;

	while(directory_cache_bytes) {
		if(directory_size - directory_bytes <
				((SQUASHFS_METADATA_SIZE << 1) + 2)) {
			void *dt = realloc(directory_table,
				directory_size + ((SQUASHFS_METADATA_SIZE << 1)
				+ 2));
			if(dt == NULL)
				MEM_ERROR();
			directory_size += (SQUASHFS_METADATA_SIZE << 1) + 2;
			directory_table = dt;
		}
		avail_bytes = directory_cache_bytes > SQUASHFS_METADATA_SIZE ?
			SQUASHFS_METADATA_SIZE : directory_cache_bytes;
		c_byte = mangle(directory_table + directory_bytes +
			BLOCK_OFFSET, directoryp, avail_bytes,
			SQUASHFS_METADATA_SIZE, noI, 0);
		TRACE("Directory block @ 0x%x, size %d\n", directory_bytes,
			c_byte);
		SQUASHFS_SWAP_SHORTS(&c_byte,
			directory_table + directory_bytes, 1);
		directory_bytes += SQUASHFS_COMPRESSED_SIZE(c_byte) +
			BLOCK_OFFSET;
		total_directory_bytes += avail_bytes + BLOCK_OFFSET;
		directoryp += avail_bytes;
		directory_cache_bytes -= avail_bytes;
	}
	write_destination(fd, bytes, directory_bytes, directory_table);
	bytes += directory_bytes;

	return start_bytes;
}


long long write_id_table()
{
	unsigned int id_bytes = SQUASHFS_ID_BYTES(id_count);
	unsigned int p[id_count];
	int i;

	TRACE("write_id_table: ids %d, id_bytes %d\n", id_count, id_bytes);
	for(i = 0; i < id_count; i++) {
		TRACE("write_id_table: id index %d, id %d", i, id_table[i]->id);
		SQUASHFS_SWAP_INTS(&id_table[i]->id, p + i, 1);
	}

	return generic_write_table(id_bytes, p, 0, NULL, noI || noId);
}


struct id *get_id(unsigned int id)
{
	int hash = ID_HASH(id);
	struct id *entry = id_hash_table[hash];

	for(; entry; entry = entry->next)
		if(entry->id == id)
			break;

	return entry;
}


struct id *create_id(unsigned int id)
{
	int hash = ID_HASH(id);
	struct id *entry = malloc(sizeof(struct id));
	if(entry == NULL)
		MEM_ERROR();
	entry->id = id;
	entry->index = id_count ++;
	entry->flags = 0;
	entry->next = id_hash_table[hash];
	id_hash_table[hash] = entry;
	id_table[entry->index] = entry;
	return entry;
}


unsigned int get_uid(unsigned int uid)
{
	struct id *entry = get_id(uid);

	if(entry == NULL) {
		if(id_count == SQUASHFS_IDS)
			BAD_ERROR("Out of uids!\n");
		entry = create_id(uid);
	}

	if((entry->flags & ISA_UID) == 0) {
		entry->flags |= ISA_UID;
		uid_count ++;
	}

	return entry->index;
}


unsigned int get_guid(unsigned int guid)
{
	struct id *entry = get_id(guid);

	if(entry == NULL) {
		if(id_count == SQUASHFS_IDS)
			BAD_ERROR("Out of gids!\n");
		entry = create_id(guid);
	}

	if((entry->flags & ISA_GID) == 0) {
		entry->flags |= ISA_GID;
		guid_count ++;
	}

	return entry->index;
}


#define ALLOC_SIZE 128

char *_pathname(struct dir_ent *dir_ent, char *pathname, int *size)
{
	if(pathname == NULL) {
		pathname = malloc(ALLOC_SIZE);
		if(pathname == NULL)
			MEM_ERROR();
	}

	for(;;) {
		int res = snprintf(pathname, *size, "%s/%s", 
			dir_ent->our_dir->pathname,
			dir_ent->source_name ? : dir_ent->name);

		if(res < 0)
			BAD_ERROR("snprintf failed in pathname\n");
		else if(res >= *size) {
			/*
			 * pathname is too small to contain the result, so
			 * increase it and try again
			 */
			*size = (res + ALLOC_SIZE) & ~(ALLOC_SIZE - 1);
			pathname = realloc(pathname, *size);
			if(pathname == NULL)
				MEM_ERROR();
		} else
			break;
	}

	return pathname;
}


char *pathname(struct dir_ent *dir_ent)
{
	static char *pathname = NULL;
	static int size = ALLOC_SIZE;

	if (dir_ent->nonstandard_pathname)
		return dir_ent->nonstandard_pathname;

	return pathname = _pathname(dir_ent, pathname, &size);
}


char *pathname_reader(struct dir_ent *dir_ent)
{
	static char *pathname = NULL;
	static int size = ALLOC_SIZE;

	if (dir_ent->nonstandard_pathname)
		return dir_ent->nonstandard_pathname;

	return pathname = _pathname(dir_ent, pathname, &size);
}


char *subpathname(struct dir_ent *dir_ent)
{
	static char *subpath = NULL;
	static int size = ALLOC_SIZE;
	int res;

	if(subpath == NULL) {
		subpath = malloc(ALLOC_SIZE);
		if(subpath == NULL)
			MEM_ERROR();
	}

	for(;;) {
		if(dir_ent->our_dir->subpath[0] != '\0')
			res = snprintf(subpath, size, "%s/%s",
				dir_ent->our_dir->subpath, dir_ent->name);
		else
			res = snprintf(subpath, size, "/%s", dir_ent->name);

		if(res < 0)
			BAD_ERROR("snprintf failed in subpathname\n");
		else if(res >= size) {
			/*
			 * subpath is too small to contain the result, so
			 * increase it and try again
			 */
			size = (res + ALLOC_SIZE) & ~(ALLOC_SIZE - 1);
			subpath = realloc(subpath, size);
			if(subpath == NULL)
				MEM_ERROR();
		} else
			break;
	}

	return subpath;
}


static inline unsigned int get_inode_no(struct inode_info *inode)
{
	return inode->inode_number;
}


static inline unsigned int get_parent_no(struct dir_info *dir)
{
	return dir->depth ? get_inode_no(dir->dir_ent->inode) : inode_no;
}

	
static inline time_t get_time(time_t time)
{
	if(all_time_opt) {
		if(clamping)
			return time > all_time ? all_time : time;
		else
			return all_time;
	}

	return time;
}


int create_inode(squashfs_inode *i_no, struct dir_info *dir_info,
	struct dir_ent *dir_ent, int type, long long byte_size,
	long long start_block, unsigned int offset, unsigned int *block_list,
	struct fragment *fragment, struct directory *dir_in, long long sparse)
{
	struct stat *buf = &dir_ent->inode->buf;
	union squashfs_inode_header inode_header;
	struct squashfs_base_inode_header *base = &inode_header.base;
	void *inode;
	char *filename = pathname(dir_ent);
	int nlink = dir_ent->inode->nlink;
	int xattr = read_xattrs(dir_ent);

	switch(type) {
	case SQUASHFS_FILE_TYPE:
		if(dir_ent->inode->nlink > 1 ||
				byte_size >= (1LL << 32) ||
				start_block >= (1LL << 32) ||
				sparse || IS_XATTR(xattr))
			type = SQUASHFS_LREG_TYPE;
		break;
	case SQUASHFS_DIR_TYPE:
		if(dir_info->dir_is_ldir || IS_XATTR(xattr))
			type = SQUASHFS_LDIR_TYPE;
		break;
	case SQUASHFS_SYMLINK_TYPE:
		if(IS_XATTR(xattr))
			type = SQUASHFS_LSYMLINK_TYPE;
		break;
	case SQUASHFS_BLKDEV_TYPE:
		if(IS_XATTR(xattr))
			type = SQUASHFS_LBLKDEV_TYPE;
		break;
	case SQUASHFS_CHRDEV_TYPE:
		if(IS_XATTR(xattr))
			type = SQUASHFS_LCHRDEV_TYPE;
		break;
	case SQUASHFS_FIFO_TYPE:
		if(IS_XATTR(xattr))
			type = SQUASHFS_LFIFO_TYPE;
		break;
	case SQUASHFS_SOCKET_TYPE:
		if(IS_XATTR(xattr))
			type = SQUASHFS_LSOCKET_TYPE;
		break;
	}
			
	base->mode = SQUASHFS_MODE(buf->st_mode);
	base->uid = get_uid((unsigned int) global_uid == -1 ?
		buf->st_uid : global_uid);
	base->inode_type = type;
	base->guid = get_guid((unsigned int) global_gid == -1 ?
		buf->st_gid : global_gid);
	base->mtime = get_time(buf->st_mtime);
	base->inode_number = get_inode_no(dir_ent->inode);

	if(type == SQUASHFS_FILE_TYPE) {
		int i;
		struct squashfs_reg_inode_header *reg = &inode_header.reg;
		size_t off = offsetof(struct squashfs_reg_inode_header, block_list);

		inode = get_inode(sizeof(*reg) + offset * sizeof(unsigned int));
		reg->file_size = byte_size;
		reg->start_block = start_block;
		reg->fragment = fragment->index;
		reg->offset = fragment->offset;
		SQUASHFS_SWAP_REG_INODE_HEADER(reg, inode);
		SQUASHFS_SWAP_INTS(block_list, inode + off, offset);
		TRACE("File inode, file_size %lld, start_block 0x%llx, blocks "
			"%d, fragment %d, offset %d, size %d\n", byte_size,
			start_block, offset, fragment->index, fragment->offset,
			fragment->size);
		for(i = 0; i < offset; i++)
			TRACE("Block %d, size %d\n", i, block_list[i]);
	}
	else if(type == SQUASHFS_LREG_TYPE) {
		int i;
		struct squashfs_lreg_inode_header *reg = &inode_header.lreg;
		size_t off = offsetof(struct squashfs_lreg_inode_header, block_list);

		inode = get_inode(sizeof(*reg) + offset * sizeof(unsigned int));
		reg->nlink = nlink;
		reg->file_size = byte_size;
		reg->start_block = start_block;
		reg->fragment = fragment->index;
		reg->offset = fragment->offset;
		if(sparse && sparse >= byte_size)
			sparse = byte_size - 1;
		reg->sparse = sparse;
		reg->xattr = xattr;
		SQUASHFS_SWAP_LREG_INODE_HEADER(reg, inode);
		SQUASHFS_SWAP_INTS(block_list, inode + off, offset);
		TRACE("Long file inode, file_size %lld, start_block 0x%llx, "
			"blocks %d, fragment %d, offset %d, size %d, nlink %d"
			"\n", byte_size, start_block, offset, fragment->index,
			fragment->offset, fragment->size, nlink);
		for(i = 0; i < offset; i++)
			TRACE("Block %d, size %d\n", i, block_list[i]);
	}
	else if(type == SQUASHFS_LDIR_TYPE) {
		int i;
		unsigned char *p;
		struct squashfs_ldir_inode_header *dir = &inode_header.ldir;
		struct cached_dir_index *index = dir_in->index;
		unsigned int i_count = dir_in->i_count;
		unsigned int i_size = dir_in->i_size;

		if(byte_size >= 1 << 27)
			BAD_ERROR("directory greater than 2^27-1 bytes!\n");

		inode = get_inode(sizeof(*dir) + i_size);
		dir->inode_type = SQUASHFS_LDIR_TYPE;
		dir->nlink = dir_ent->dir->directory_count + 2;
		dir->file_size = byte_size;
		dir->offset = offset;
		dir->start_block = start_block;
		dir->i_count = i_count;
		dir->parent_inode = get_parent_no(dir_ent->our_dir);
		dir->xattr = xattr;

		SQUASHFS_SWAP_LDIR_INODE_HEADER(dir, inode);
		p = inode + offsetof(struct squashfs_ldir_inode_header, index);
		for(i = 0; i < i_count; i++) {
			SQUASHFS_SWAP_DIR_INDEX(&index[i].index, p);
			p += offsetof(struct squashfs_dir_index, name);
			memcpy(p, index[i].name, index[i].index.size + 1);
			p += index[i].index.size + 1;
		}
		TRACE("Long directory inode, file_size %lld, start_block "
			"0x%llx, offset 0x%x, nlink %d\n", byte_size,
			start_block, offset, dir_ent->dir->directory_count + 2);
	}
	else if(type == SQUASHFS_DIR_TYPE) {
		struct squashfs_dir_inode_header *dir = &inode_header.dir;

		inode = get_inode(sizeof(*dir));
		dir->nlink = dir_ent->dir->directory_count + 2;
		dir->file_size = byte_size;
		dir->offset = offset;
		dir->start_block = start_block;
		dir->parent_inode = get_parent_no(dir_ent->our_dir);
		SQUASHFS_SWAP_DIR_INODE_HEADER(dir, inode);
		TRACE("Directory inode, file_size %lld, start_block 0x%llx, "
			"offset 0x%x, nlink %d\n", byte_size, start_block,
			offset, dir_ent->dir->directory_count + 2);
	}
	else if(type == SQUASHFS_CHRDEV_TYPE || type == SQUASHFS_BLKDEV_TYPE) {
		struct squashfs_dev_inode_header *dev = &inode_header.dev;
		unsigned int major = major(buf->st_rdev);
		unsigned int minor = minor(buf->st_rdev);

		if(major > 0xfff) {
			ERROR("Major %d out of range in device node %s, "
				"truncating to %d\n", major, filename,
				major & 0xfff);
			major &= 0xfff;
		}
		if(minor > 0xfffff) {
			ERROR("Minor %d out of range in device node %s, "
				"truncating to %d\n", minor, filename,
				minor & 0xfffff);
			minor &= 0xfffff;
		}
		inode = get_inode(sizeof(*dev));
		dev->nlink = nlink;
		dev->rdev = (major << 8) | (minor & 0xff) |
				((minor & ~0xff) << 12);
		SQUASHFS_SWAP_DEV_INODE_HEADER(dev, inode);
		TRACE("Device inode, rdev 0x%x, nlink %d\n", dev->rdev, nlink);
	}
	else if(type == SQUASHFS_LCHRDEV_TYPE || type == SQUASHFS_LBLKDEV_TYPE) {
		struct squashfs_ldev_inode_header *dev = &inode_header.ldev;
		unsigned int major = major(buf->st_rdev);
		unsigned int minor = minor(buf->st_rdev);

		if(major > 0xfff) {
			ERROR("Major %d out of range in device node %s, "
				"truncating to %d\n", major, filename,
				major & 0xfff);
			major &= 0xfff;
		}
		if(minor > 0xfffff) {
			ERROR("Minor %d out of range in device node %s, "
				"truncating to %d\n", minor, filename,
				minor & 0xfffff);
			minor &= 0xfffff;
		}
		inode = get_inode(sizeof(*dev));
		dev->nlink = nlink;
		dev->rdev = (major << 8) | (minor & 0xff) |
				((minor & ~0xff) << 12);
		dev->xattr = xattr;
		SQUASHFS_SWAP_LDEV_INODE_HEADER(dev, inode);
		TRACE("Device inode, rdev 0x%x, nlink %d\n", dev->rdev, nlink);
	}
	else if(type == SQUASHFS_SYMLINK_TYPE) {
		struct squashfs_symlink_inode_header *symlink = &inode_header.symlink;
		int byte = strlen(dir_ent->inode->symlink);
		size_t off = offsetof(struct squashfs_symlink_inode_header, symlink);

		inode = get_inode(sizeof(*symlink) + byte);
		symlink->nlink = nlink;
		symlink->symlink_size = byte;
		SQUASHFS_SWAP_SYMLINK_INODE_HEADER(symlink, inode);
		strncpy(inode + off, dir_ent->inode->symlink, byte);
		TRACE("Symbolic link inode, symlink_size %d, nlink %d\n", byte,
			nlink);
	}
	else if(type == SQUASHFS_LSYMLINK_TYPE) {
		struct squashfs_symlink_inode_header *symlink = &inode_header.symlink;
		int byte = strlen(dir_ent->inode->symlink);
		size_t off = offsetof(struct squashfs_symlink_inode_header, symlink);

		inode = get_inode(sizeof(*symlink) + byte +
						sizeof(unsigned int));
		symlink->nlink = nlink;
		symlink->symlink_size = byte;
		SQUASHFS_SWAP_SYMLINK_INODE_HEADER(symlink, inode);
		strncpy(inode + off, dir_ent->inode->symlink, byte);
		SQUASHFS_SWAP_INTS(&xattr, inode + off + byte, 1);
		TRACE("Symbolic link inode, symlink_size %d, nlink %d\n", byte,
			nlink);
	}
	else if(type == SQUASHFS_FIFO_TYPE || type == SQUASHFS_SOCKET_TYPE) {
		struct squashfs_ipc_inode_header *ipc = &inode_header.ipc;

		inode = get_inode(sizeof(*ipc));
		ipc->nlink = nlink;
		SQUASHFS_SWAP_IPC_INODE_HEADER(ipc, inode);
		TRACE("ipc inode, type %s, nlink %d\n", type ==
			SQUASHFS_FIFO_TYPE ? "fifo" : "socket", nlink);
	}
	else if(type == SQUASHFS_LFIFO_TYPE || type == SQUASHFS_LSOCKET_TYPE) {
		struct squashfs_lipc_inode_header *ipc = &inode_header.lipc;

		inode = get_inode(sizeof(*ipc));
		ipc->nlink = nlink;
		ipc->xattr = xattr;
		SQUASHFS_SWAP_LIPC_INODE_HEADER(ipc, inode);
		TRACE("ipc inode, type %s, nlink %d\n", type ==
			SQUASHFS_FIFO_TYPE ? "fifo" : "socket", nlink);
	} else
		BAD_ERROR("Unrecognised inode %d in create_inode\n", type);

	*i_no = MKINODE(inode);
	inode_count ++;

	TRACE("Created inode 0x%llx, type %d, uid %d, guid %d\n", *i_no, type,
		base->uid, base->guid);

	return TRUE;
}


void add_dir(squashfs_inode inode, unsigned int inode_number, char *name,
	int type, struct directory *dir)
{
	unsigned char *buff;
	struct squashfs_dir_entry idir;
	unsigned int start_block = inode >> 16;
	unsigned int offset = inode & 0xffff;
	unsigned int size = strlen(name);
	size_t name_off = offsetof(struct squashfs_dir_entry, name);

	if(size > SQUASHFS_NAME_LEN) {
		size = SQUASHFS_NAME_LEN;
		ERROR("Filename is greater than %d characters, truncating! ..."
			"\n", SQUASHFS_NAME_LEN);
	}

	if(dir->p + sizeof(struct squashfs_dir_entry) + size +
			sizeof(struct squashfs_dir_header)
			>= dir->buff + dir->size) {
		buff = realloc(dir->buff, dir->size += SQUASHFS_METADATA_SIZE);
		if(buff == NULL)
			MEM_ERROR();

		dir->p = (dir->p - dir->buff) + buff;
		if(dir->entry_count_p) 
			dir->entry_count_p = (dir->entry_count_p - dir->buff +
			buff);
		dir->index_count_p = dir->index_count_p - dir->buff + buff;
		dir->buff = buff;
	}

	if(dir->entry_count == 256 || start_block != dir->start_block ||
			((dir->entry_count_p != NULL) &&
			((dir->p + sizeof(struct squashfs_dir_entry) + size -
			dir->index_count_p) > SQUASHFS_METADATA_SIZE)) ||
			((long long) inode_number - dir->inode_number) > 32767
			|| ((long long) inode_number - dir->inode_number)
			< -32768) {
		if(dir->entry_count_p) {
			struct squashfs_dir_header dir_header;

			if((dir->p + sizeof(struct squashfs_dir_entry) + size -
					dir->index_count_p) >
					SQUASHFS_METADATA_SIZE) {
				if(dir->i_count % I_COUNT_SIZE == 0) {
					dir->index = realloc(dir->index,
						(dir->i_count + I_COUNT_SIZE) *
						sizeof(struct cached_dir_index));
					if(dir->index == NULL)
						MEM_ERROR();
				}
				dir->index[dir->i_count].index.index =
					dir->p - dir->buff;
				dir->index[dir->i_count].index.size = size - 1;
				dir->index[dir->i_count++].name = name;
				dir->i_size += sizeof(struct squashfs_dir_index)
					+ size;
				dir->index_count_p = dir->p;
			}

			dir_header.count = dir->entry_count - 1;
			dir_header.start_block = dir->start_block;
			dir_header.inode_number = dir->inode_number;
			SQUASHFS_SWAP_DIR_HEADER(&dir_header,
				dir->entry_count_p);

		}


		dir->entry_count_p = dir->p;
		dir->start_block = start_block;
		dir->entry_count = 0;
		dir->inode_number = inode_number;
		dir->p += sizeof(struct squashfs_dir_header);
	}

	idir.offset = offset;
	idir.type = type;
	idir.size = size - 1;
	idir.inode_number = ((long long) inode_number - dir->inode_number);
	SQUASHFS_SWAP_DIR_ENTRY(&idir, dir->p);
	strncpy((char *) dir->p + name_off, name, size);
	dir->p += sizeof(struct squashfs_dir_entry) + size;
	dir->entry_count ++;
}


void write_dir(squashfs_inode *inode, struct dir_info *dir_info,
	struct directory *dir)
{
	unsigned int dir_size = dir->p - dir->buff;
	int data_space = directory_cache_size - directory_cache_bytes;
	unsigned int directory_block, directory_offset, i_count, index;
	unsigned short c_byte;

	if(data_space < dir_size) {
		int realloc_size = directory_cache_size == 0 ?
			((dir_size + SQUASHFS_METADATA_SIZE) &
			~(SQUASHFS_METADATA_SIZE - 1)) : dir_size - data_space;

		void *dc = realloc(directory_data_cache,
			directory_cache_size + realloc_size);
		if(dc == NULL)
			MEM_ERROR();
		directory_cache_size += realloc_size;
		directory_data_cache = dc;
	}

	if(dir_size) {
		struct squashfs_dir_header dir_header;

		dir_header.count = dir->entry_count - 1;
		dir_header.start_block = dir->start_block;
		dir_header.inode_number = dir->inode_number;
		SQUASHFS_SWAP_DIR_HEADER(&dir_header, dir->entry_count_p);
		memcpy(directory_data_cache + directory_cache_bytes, dir->buff,
			dir_size);
	}
	directory_offset = directory_cache_bytes;
	directory_block = directory_bytes;
	directory_cache_bytes += dir_size;
	i_count = 0;
	index = SQUASHFS_METADATA_SIZE - directory_offset;

	while(1) {
		while(i_count < dir->i_count &&
				dir->index[i_count].index.index < index)
			dir->index[i_count++].index.start_block =
				directory_bytes;
		index += SQUASHFS_METADATA_SIZE;

		if(directory_cache_bytes < SQUASHFS_METADATA_SIZE)
			break;

		if((directory_size - directory_bytes) <
					((SQUASHFS_METADATA_SIZE << 1) + 2)) {
			void *dt = realloc(directory_table,
				directory_size + (SQUASHFS_METADATA_SIZE << 1)
				+ 2);
			if(dt == NULL)
				MEM_ERROR();
			directory_size += SQUASHFS_METADATA_SIZE << 1;
			directory_table = dt;
		}

		c_byte = mangle(directory_table + directory_bytes +
				BLOCK_OFFSET, directory_data_cache,
				SQUASHFS_METADATA_SIZE, SQUASHFS_METADATA_SIZE,
				noI, 0);
		TRACE("Directory block @ 0x%x, size %d\n", directory_bytes,
			c_byte);
		SQUASHFS_SWAP_SHORTS(&c_byte,
			directory_table + directory_bytes, 1);
		directory_bytes += SQUASHFS_COMPRESSED_SIZE(c_byte) +
			BLOCK_OFFSET;
		total_directory_bytes += SQUASHFS_METADATA_SIZE + BLOCK_OFFSET;
		memmove(directory_data_cache, directory_data_cache +
			SQUASHFS_METADATA_SIZE, directory_cache_bytes -
			SQUASHFS_METADATA_SIZE);
		directory_cache_bytes -= SQUASHFS_METADATA_SIZE;
	}

	create_inode(inode, dir_info, dir_info->dir_ent, SQUASHFS_DIR_TYPE,
		dir_size + 3, directory_block, directory_offset, NULL, NULL,
		dir, 0);

#ifdef SQUASHFS_TRACE
	{
		unsigned char *dirp;
		int count;

		TRACE("Directory contents of inode 0x%llx\n", *inode);
		dirp = dir->buff;
		while(dirp < dir->p) {
			char buffer[SQUASHFS_NAME_LEN + 1];
			struct squashfs_dir_entry idir, *idirp;
			struct squashfs_dir_header dirh;
			SQUASHFS_SWAP_DIR_HEADER((struct squashfs_dir_header *) dirp,
				&dirh);
			count = dirh.count + 1;
			dirp += sizeof(struct squashfs_dir_header);

			TRACE("\tStart block 0x%x, count %d\n",
				dirh.start_block, count);

			while(count--) {
				idirp = (struct squashfs_dir_entry *) dirp;
				SQUASHFS_SWAP_DIR_ENTRY(idirp, &idir);
				strncpy(buffer, idirp->name, idir.size + 1);
				buffer[idir.size + 1] = '\0';
				TRACE("\t\tname %s, inode offset 0x%x, type "
					"%d\n", buffer, idir.offset, idir.type);
				dirp += sizeof(struct squashfs_dir_entry) + idir.size +
					1;
			}
		}
	}
#endif
	dir_count ++;
}


static struct file_buffer *get_fragment(struct fragment *fragment)
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

	if(fragment->index == SQUASHFS_INVALID_FRAG)
		return NULL;

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
			data = read_from_disk(start_block, size);
			if(data == NULL) {
				ERROR("Failed to read fragment from output"
					" filesystem\n");
				BAD_ERROR("Output filesystem corrupted?\n");
			}
		}

		res = compressor_uncompress(comp, buffer->data, data, size,
			block_size, &error);
		if(res == -1)
			BAD_ERROR("%s uncompress failed with error code %d\n",
				comp->name, error);
	} else if(compressed_buffer)
		memcpy(buffer->data, compressed_buffer->data, size);
	else {
		res = read_fs_bytes(fd, start_block, size, buffer->data);
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


unsigned short get_fragment_checksum(struct file_info *file)
{
	struct file_buffer *frag_buffer;
	struct append_file *append;
	int res, index = file->fragment->index;
	unsigned short checksum;

	if(index == SQUASHFS_INVALID_FRAG)
		return 0;

	pthread_cleanup_push((void *) pthread_mutex_unlock, &dup_mutex);
	pthread_mutex_lock(&dup_mutex);
	res = file->have_frag_checksum;
	checksum = file->fragment_checksum;
	pthread_cleanup_pop(1);

	if(res)
		return checksum;

	frag_buffer = get_fragment(file->fragment);

	pthread_cleanup_push((void *) pthread_mutex_unlock, &dup_mutex);

	for(append = file_mapping[index]; append; append = append->next) {
		int offset = append->file->fragment->offset;
		int size = append->file->fragment->size;
		unsigned short cksum =
			get_checksum_mem(frag_buffer->data + offset, size);

		if(file == append->file)
			checksum = cksum;

		pthread_mutex_lock(&dup_mutex);
		append->file->fragment_checksum = cksum;
		append->file->have_frag_checksum = TRUE;
		pthread_mutex_unlock(&dup_mutex);
	}

	cache_block_put(frag_buffer);
	pthread_cleanup_pop(0);

	return checksum;
}


void ensure_fragments_flushed()
{
	pthread_cleanup_push((void *) pthread_mutex_unlock, &fragment_mutex);
	pthread_mutex_lock(&fragment_mutex);

	while(fragments_outstanding)
		pthread_cond_wait(&fragment_waiting, &fragment_mutex);

	pthread_cleanup_pop(1);
}


void lock_fragments()
{
	pthread_cleanup_push((void *) pthread_mutex_unlock, &fragment_mutex);
	pthread_mutex_lock(&fragment_mutex);
	fragments_locked = TRUE;
	pthread_cleanup_pop(1);
}


void log_fragment(unsigned int fragment, long long start)
{
	if(logging)
		fprintf(log_fd, "Fragment %u, %lld\n", fragment, start);
}


void unlock_fragments()
{
	int frg, size;
	struct file_buffer *write_buffer;

	pthread_cleanup_push((void *) pthread_mutex_unlock, &fragment_mutex);
	pthread_mutex_lock(&fragment_mutex);

	/*
	 * Note queue_empty() is inherently racy with respect to concurrent
	 * queue get and pushes.  We avoid this because we're holding the
	 * fragment_mutex which ensures no other threads can be using the
	 * queue at this time.
	 */
	while(!queue_empty(locked_fragment)) {
		write_buffer = queue_get(locked_fragment);
		frg = write_buffer->block;	
		size = SQUASHFS_COMPRESSED_SIZE_BLOCK(fragment_table[frg].size);
		fragment_table[frg].start_block = bytes;
		write_buffer->block = bytes;
		bytes += size;
		fragments_outstanding --;
		queue_put(to_writer, write_buffer);
		log_fragment(frg, fragment_table[frg].start_block);
		TRACE("fragment_locked writing fragment %d, compressed size %d"
			"\n", frg, size);
	}
	fragments_locked = FALSE;
	pthread_cleanup_pop(1);
}

/* Called with the fragment_mutex locked */
void add_pending_fragment(struct file_buffer *write_buffer, int c_byte,
	int fragment)
{
	fragment_table[fragment].size = c_byte;
	write_buffer->block = fragment;

	queue_put(locked_fragment, write_buffer);
}


void write_fragment(struct file_buffer *fragment)
{
	static long long sequence = 0;

	if(fragment == NULL)
		return;

	pthread_cleanup_push((void *) pthread_mutex_unlock, &fragment_mutex);
	pthread_mutex_lock(&fragment_mutex);
	fragment_table[fragment->block].unused = 0;
	fragment->sequence = sequence ++;
	fragments_outstanding ++;
	queue_put(to_frag, fragment);
	pthread_cleanup_pop(1);
}


struct file_buffer *allocate_fragment()
{
	struct file_buffer *fragment = cache_get(fragment_buffer, fragments);

	pthread_cleanup_push((void *) pthread_mutex_unlock, &fragment_mutex);
	pthread_mutex_lock(&fragment_mutex);

	if(fragments % FRAG_SIZE == 0) {
		void *ft = realloc(fragment_table, (fragments +
			FRAG_SIZE) * sizeof(struct squashfs_fragment_entry));
		if(ft == NULL)
			MEM_ERROR();
		fragment_table = ft;
	}

	fragment->size = 0;
	fragment->block = fragments ++;

	pthread_cleanup_pop(1);

	return fragment;
}


static struct fragment empty_fragment = {SQUASHFS_INVALID_FRAG, 0, 0};


void free_fragment(struct fragment *fragment)
{
	if(fragment != &empty_fragment)
		free(fragment);
}


struct fragment *get_and_fill_fragment(struct file_buffer *file_buffer,
	struct dir_ent *dir_ent)
{
	struct fragment *ffrg;
	struct file_buffer **fragment;

	if(file_buffer == NULL || file_buffer->size == 0)
		return &empty_fragment;

	fragment = eval_frag_actions(root_dir, dir_ent);

	if((*fragment) && (*fragment)->size + file_buffer->size > block_size) {
		write_fragment(*fragment);
		*fragment = NULL;
	}

	ffrg = malloc(sizeof(struct fragment));
	if(ffrg == NULL)
		MEM_ERROR();

	if(*fragment == NULL)
		*fragment = allocate_fragment();

	ffrg->index = (*fragment)->block;
	ffrg->offset = (*fragment)->size;
	ffrg->size = file_buffer->size;
	memcpy((*fragment)->data + (*fragment)->size, file_buffer->data,
		file_buffer->size);
	(*fragment)->size += file_buffer->size;

	return ffrg;
}


long long generic_write_table(int length, void *buffer, int length2,
	void *buffer2, int uncompressed)
{
	int meta_blocks = (length + SQUASHFS_METADATA_SIZE - 1) /
		SQUASHFS_METADATA_SIZE;
	long long *list, start_bytes;
	int compressed_size, i, list_size = meta_blocks * sizeof(long long);
	unsigned short c_byte;
	char cbuffer[(SQUASHFS_METADATA_SIZE << 2) + 2];
	
#ifdef SQUASHFS_TRACE
	long long obytes = bytes;
	int olength = length;
#endif

	list = malloc(list_size);
	if(list == NULL)
		MEM_ERROR();

	for(i = 0; i < meta_blocks; i++) {
		int avail_bytes = length > SQUASHFS_METADATA_SIZE ?
			SQUASHFS_METADATA_SIZE : length;
		c_byte = mangle(cbuffer + BLOCK_OFFSET, buffer + i *
			SQUASHFS_METADATA_SIZE , avail_bytes,
			SQUASHFS_METADATA_SIZE, uncompressed, 0);
		SQUASHFS_SWAP_SHORTS(&c_byte, cbuffer, 1);
		list[i] = bytes;
		compressed_size = SQUASHFS_COMPRESSED_SIZE(c_byte) +
			BLOCK_OFFSET;
		TRACE("block %d @ 0x%llx, compressed size %d\n", i, bytes,
			compressed_size);
		write_destination(fd, bytes, compressed_size, cbuffer);
		bytes += compressed_size;
		total_bytes += avail_bytes;
		length -= avail_bytes;
	}

	start_bytes = bytes;
	if(length2) {
		write_destination(fd, bytes, length2, buffer2);
		bytes += length2;
		total_bytes += length2;
	}
		
	SQUASHFS_INSWAP_LONG_LONGS(list, meta_blocks);
	write_destination(fd, bytes, list_size, list);
	bytes += list_size;
	total_bytes += list_size;

	TRACE("generic_write_table: total uncompressed %d compressed %lld\n",
		olength, bytes - obytes);

	free(list);

	return start_bytes;
}


long long write_fragment_table()
{
	unsigned int frag_bytes = SQUASHFS_FRAGMENT_BYTES(fragments);
	int i;

	TRACE("write_fragment_table: fragments %d, frag_bytes %d\n", fragments,
		frag_bytes);
	for(i = 0; i < fragments; i++) {
		TRACE("write_fragment_table: fragment %d, start_block 0x%llx, "
			"size %d\n", i, fragment_table[i].start_block,
			fragment_table[i].size);
		SQUASHFS_INSWAP_FRAGMENT_ENTRY(&fragment_table[i]);
	}

	return generic_write_table(frag_bytes, fragment_table, 0, NULL, noF);
}


char read_from_file_buffer[SQUASHFS_FILE_MAX_SIZE];
static char *read_from_disk(long long start, unsigned int avail_bytes)
{
	int res;

	res = read_fs_bytes(fd, start, avail_bytes, read_from_file_buffer);
	if(res == 0)
		return NULL;

	return read_from_file_buffer;
}


char read_from_file_buffer2[SQUASHFS_FILE_MAX_SIZE];
char *read_from_disk2(long long start, unsigned int avail_bytes)
{
	int res;

	res = read_fs_bytes(fd, start, avail_bytes, read_from_file_buffer2);
	if(res == 0)
		return NULL;

	return read_from_file_buffer2;
}


/*
 * Compute 16 bit BSD checksum over the data
 */
unsigned short get_checksum(char *buff, int bytes, unsigned short chksum)
{
	unsigned char *b = (unsigned char *) buff;

	while(bytes --) {
		chksum = (chksum & 1) ? (chksum >> 1) | 0x8000 : chksum >> 1;
		chksum += *b++;
	}

	return chksum;
}


unsigned short get_checksum_disk(long long start, long long l,
	unsigned int *blocks)
{
	unsigned short chksum = 0;
	unsigned int bytes;
	struct file_buffer *write_buffer;
	int i;

	for(i = 0; l; i++)  {
		bytes = SQUASHFS_COMPRESSED_SIZE_BLOCK(blocks[i]);
		if(bytes == 0) /* sparse block */
			continue;
		write_buffer = cache_lookup(bwriter_buffer, start);
		if(write_buffer) {
			chksum = get_checksum(write_buffer->data, bytes,
				chksum);
			cache_block_put(write_buffer);
		} else {
			void *data = read_from_disk(start, bytes);
			if(data == NULL) {	
				ERROR("Failed to checksum data from output"
					" filesystem\n");
				BAD_ERROR("Output filesystem corrupted?\n");
			}

			chksum = get_checksum(data, bytes, chksum);
		}

		l -= bytes;
		start += bytes;
	}

	return chksum;
}


unsigned short get_checksum_mem(char *buff, int bytes)
{
	return get_checksum(buff, bytes, 0);
}


unsigned short get_checksum_mem_buffer(struct file_buffer *file_buffer)
{
	if(file_buffer == NULL)
		return 0;
	else
		return get_checksum(file_buffer->data, file_buffer->size, 0);
}


#define DUP_HASH(a) (a & 0xffff)
void add_file(long long start, long long file_size, long long file_bytes,
	unsigned int *block_listp, int blocks, unsigned int fragment,
	int offset, int bytes)
{
	struct fragment *frg;
	unsigned int *block_list = block_listp;
	struct file_info *dupl_ptr = dupl[DUP_HASH(file_size)];
	struct append_file *append_file;
	struct file_info *file;

	if(!duplicate_checking || file_size == 0)
		return;

	for(; dupl_ptr; dupl_ptr = dupl_ptr->next) {
		if(file_size != dupl_ptr->file_size)
			continue;
		if(blocks != 0 && start != dupl_ptr->start)
			continue;
		if(fragment != dupl_ptr->fragment->index)
			continue;
		if(fragment != SQUASHFS_INVALID_FRAG && (offset !=
				dupl_ptr->fragment->offset || bytes !=
				dupl_ptr->fragment->size))
			continue;
		return;
	}

	frg = malloc(sizeof(struct fragment));
	if(frg == NULL)
		MEM_ERROR();

	frg->index = fragment;
	frg->offset = offset;
	frg->size = bytes;

	file = add_non_dup(file_size, file_bytes, block_list, start, frg, 0, 0,
		FALSE, FALSE);

	if(fragment == SQUASHFS_INVALID_FRAG)
		return;

	append_file = malloc(sizeof(struct append_file));
	if(append_file == NULL)
		MEM_ERROR();

	append_file->file = file;
	append_file->next = file_mapping[fragment];
	file_mapping[fragment] = append_file;
}


int pre_duplicate(long long file_size)
{
	struct file_info *dupl_ptr = dupl[DUP_HASH(file_size)];

	for(; dupl_ptr; dupl_ptr = dupl_ptr->next)
		if(dupl_ptr->file_size == file_size)
			return TRUE;

	return FALSE;
}


struct file_info *add_non_dup(long long file_size, long long bytes,
	unsigned int *block_list, long long start, struct fragment *fragment,
	unsigned short checksum, unsigned short fragment_checksum,
	int checksum_flag, int checksum_frag_flag)
{
	struct file_info *dupl_ptr = malloc(sizeof(struct file_info));

	if(dupl_ptr == NULL)
		MEM_ERROR();

	dupl_ptr->file_size = file_size;
	dupl_ptr->bytes = bytes;
	dupl_ptr->block_list = block_list;
	dupl_ptr->start = start;
	dupl_ptr->fragment = fragment;
	dupl_ptr->checksum = checksum;
	dupl_ptr->fragment_checksum = fragment_checksum;
	dupl_ptr->have_frag_checksum = checksum_frag_flag;
	dupl_ptr->have_checksum = checksum_flag;

	pthread_cleanup_push((void *) pthread_mutex_unlock, &dup_mutex);
        pthread_mutex_lock(&dup_mutex);
	dupl_ptr->next = dupl[DUP_HASH(file_size)];
	dupl[DUP_HASH(file_size)] = dupl_ptr;
	dup_files ++;
	pthread_cleanup_pop(1);

	return dupl_ptr;
}


struct fragment *frag_duplicate(struct file_buffer *file_buffer, char *dont_put)
{
	struct file_info *dupl_ptr;
	struct file_buffer *buffer;
	struct file_info *dupl_start = file_buffer->dupl_start;
	long long file_size = file_buffer->file_size;
	unsigned short checksum = file_buffer->checksum;
	int res;

	if(file_buffer->duplicate) {
		TRACE("Found duplicate file, fragment %d, size %d, offset %d, "
			"checksum 0x%x\n", dupl_start->fragment->index,
			file_size, dupl_start->fragment->offset, checksum);
		*dont_put = TRUE;
		return dupl_start->fragment;
	} else {
		*dont_put = FALSE;
		dupl_ptr = dupl[DUP_HASH(file_size)];
	}

	for(; dupl_ptr && dupl_ptr != dupl_start; dupl_ptr = dupl_ptr->next) {
		if(file_size == dupl_ptr->file_size && file_size ==
				dupl_ptr->fragment->size) {
			if(get_fragment_checksum(dupl_ptr) == checksum) {
				buffer = get_fragment(dupl_ptr->fragment);
				res = memcmp(file_buffer->data, buffer->data +
					dupl_ptr->fragment->offset, file_size);
				cache_block_put(buffer);
				if(res == 0)
					break;
			}
		}
	}

	if(!dupl_ptr || dupl_ptr == dupl_start)
		return NULL;

	TRACE("Found duplicate file, fragment %d, size %d, offset %d, "
		"checksum 0x%x\n", dupl_ptr->fragment->index, file_size,
		dupl_ptr->fragment->offset, checksum);

	return dupl_ptr->fragment;
}


struct file_info *duplicate(long long file_size, long long bytes,
	unsigned int **block_list, long long *start, struct fragment **fragment,
	struct file_buffer *file_buffer, int blocks, unsigned short checksum,
	int checksum_flag)
{
	struct file_info *dupl_ptr = dupl[DUP_HASH(file_size)];
	int frag_bytes = file_buffer ? file_buffer->size : 0;
	unsigned short fragment_checksum = file_buffer ?
		file_buffer->checksum : 0;

	for(; dupl_ptr; dupl_ptr = dupl_ptr->next)
		if(file_size == dupl_ptr->file_size && bytes == dupl_ptr->bytes
				 && frag_bytes == dupl_ptr->fragment->size) {
			long long target_start, dup_start = dupl_ptr->start;
			int block;

			if(memcmp(*block_list, dupl_ptr->block_list, blocks *
					sizeof(unsigned int)) != 0)
				continue;

			if(checksum_flag == FALSE) {
				checksum = get_checksum_disk(*start, bytes,
					*block_list);
				checksum_flag = TRUE;
			}

			if(!dupl_ptr->have_checksum) {
				dupl_ptr->checksum =
					get_checksum_disk(dupl_ptr->start,
					dupl_ptr->bytes, dupl_ptr->block_list);
				dupl_ptr->have_checksum = TRUE;
			}

			if(checksum != dupl_ptr->checksum ||
					fragment_checksum !=
					get_fragment_checksum(dupl_ptr))
				continue;

			target_start = *start;
			for(block = 0; block < blocks; block ++) {
				int size = SQUASHFS_COMPRESSED_SIZE_BLOCK
					((*block_list)[block]);
				struct file_buffer *target_buffer = NULL;
				struct file_buffer *dup_buffer = NULL;
				char *target_data, *dup_data;
				int res;

				if(size == 0)
					continue;
				target_buffer = cache_lookup(bwriter_buffer,
					target_start);
				if(target_buffer)
					target_data = target_buffer->data;
				else {
					target_data =
						read_from_disk(target_start,
						size);
					if(target_data == NULL) {
						ERROR("Failed to read data from"
							" output filesystem\n");
						BAD_ERROR("Output filesystem"
							" corrupted?\n");
					}
				}

				dup_buffer = cache_lookup(bwriter_buffer,
					dup_start);
				if(dup_buffer)
					dup_data = dup_buffer->data;
				else {
					dup_data = read_from_disk2(dup_start,
						size);
					if(dup_data == NULL) {
						ERROR("Failed to read data from"
							" output filesystem\n");
						BAD_ERROR("Output filesystem"
							" corrupted?\n");
					}
				}

				res = memcmp(target_data, dup_data, size);
				cache_block_put(target_buffer);
				cache_block_put(dup_buffer);
				if(res != 0)
					break;
				target_start += size;
				dup_start += size;
			}
			if(block == blocks) {
				struct file_buffer *frag_buffer =
					get_fragment(dupl_ptr->fragment);

				if(frag_bytes == 0 ||
						memcmp(file_buffer->data,
						frag_buffer->data +
						dupl_ptr->fragment->offset,
						frag_bytes) == 0) {
					TRACE("Found duplicate file, start "
						"0x%llx, size %lld, checksum "
						"0x%x, fragment %d, size %d, "
						"offset %d, checksum 0x%x\n",
						dupl_ptr->start,
						dupl_ptr->bytes,
						dupl_ptr->checksum,
						dupl_ptr->fragment->index,
						frag_bytes,
						dupl_ptr->fragment->offset,
						fragment_checksum);
					*block_list = dupl_ptr->block_list;
					*start = dupl_ptr->start;
					*fragment = dupl_ptr->fragment;
					cache_block_put(frag_buffer);
					return 0;
				}
				cache_block_put(frag_buffer);
			}
		}


	return add_non_dup(file_size, bytes, *block_list, *start, *fragment,
		checksum, fragment_checksum, checksum_flag, TRUE);
}


static inline int is_fragment(struct inode_info *inode)
{
	off_t file_size = inode->buf.st_size;

	/*
	 * If this block is to be compressed differently to the
	 * fragment compression then it cannot be a fragment
	 */
	if(inode->noF != noF)
		return FALSE;

	return !inode->no_fragments && file_size && (file_size < block_size ||
		(inode->always_use_fragments && file_size & (block_size - 1)));
}


void put_file_buffer(struct file_buffer *file_buffer)
{
	/*
	 * Decide where to send the file buffer:
	 * - compressible non-fragment blocks go to the deflate threads,
	 * - fragments go to the process fragment threads,
	 * - all others go directly to the main thread
	 */
	if(file_buffer->error) {
		file_buffer->fragment = 0;
		seq_queue_put(to_main, file_buffer);
	} else if (file_buffer->file_size == 0)
		seq_queue_put(to_main, file_buffer);
 	else if(file_buffer->fragment)
		queue_put(to_process_frag, file_buffer);
	else
		queue_put(to_deflate, file_buffer);
}


static int seq = 0;
void reader_read_process(struct dir_ent *dir_ent)
{
	long long bytes = 0;
	struct inode_info *inode = dir_ent->inode;
	struct file_buffer *prev_buffer = NULL, *file_buffer;
	int status, byte, res, child;
	int file = pseudo_exec_file(get_pseudo_file(inode->pseudo_id), &child);

	if(!file) {
		file_buffer = cache_get_nohash(reader_buffer);
		file_buffer->sequence = seq ++;
		goto read_err;
	}

	while(1) {
		file_buffer = cache_get_nohash(reader_buffer);
		file_buffer->sequence = seq ++;
		file_buffer->noD = inode->noD;

		byte = read_bytes(file, file_buffer->data, block_size);
		if(byte == -1)
			goto read_err2;

		file_buffer->size = byte;
		file_buffer->file_size = -1;
		file_buffer->error = FALSE;
		file_buffer->fragment = FALSE;
		bytes += byte;

		if(byte == 0)
			break;

		/*
		 * Update progress bar size.  This is done
		 * on every block rather than waiting for all blocks to be
		 * read incase write_file_process() is running in parallel
		 * with this.  Otherwise the current progress bar position
		 * may get ahead of the progress bar size.
		 */ 
		progress_bar_size(1);

		if(prev_buffer)
			put_file_buffer(prev_buffer);
		prev_buffer = file_buffer;
	}

	/*
 	 * Update inode file size now that the size of the dynamic pseudo file
	 * is known.  This is needed for the -info option.
	 */
	inode->buf.st_size = bytes;

	res = waitpid(child, &status, 0);
	close(file);

	if(res == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
		goto read_err;

	if(prev_buffer == NULL)
		prev_buffer = file_buffer;
	else {
		cache_block_put(file_buffer);
		seq --;
	}
	prev_buffer->file_size = bytes;
	prev_buffer->fragment = is_fragment(inode);
	put_file_buffer(prev_buffer);

	return;

read_err2:
	close(file);
read_err:
	if(prev_buffer) {
		cache_block_put(file_buffer);
		seq --;
		file_buffer = prev_buffer;
	}
	file_buffer->error = TRUE;
	put_file_buffer(file_buffer);
}


void reader_read_file(struct dir_ent *dir_ent)
{
	struct stat *buf = &dir_ent->inode->buf, buf2;
	struct file_buffer *file_buffer;
	int blocks, file, res;
	long long bytes, read_size;
	struct inode_info *inode = dir_ent->inode;

	if(inode->read)
		return;

	inode->read = TRUE;
again:
	bytes = 0;
	read_size = buf->st_size;
	blocks = (read_size + block_size - 1) >> block_log;

	file = open(pathname_reader(dir_ent), O_RDONLY);
	if(file == -1) {
		file_buffer = cache_get_nohash(reader_buffer);
		file_buffer->sequence = seq ++;
		goto read_err2;
	}

	do {
		file_buffer = cache_get_nohash(reader_buffer);
		file_buffer->file_size = read_size;
		file_buffer->sequence = seq ++;
		file_buffer->noD = inode->noD;
		file_buffer->error = FALSE;

		/*
		 * Always try to read block_size bytes from the file rather
		 * than expected bytes (which will be less than the block_size
		 * at the file tail) to check that the file hasn't grown
		 * since being stated.  If it is longer (or shorter) than
		 * expected, then restat, and try again.  Note the special
		 * case where the file is an exact multiple of the block_size
		 * is dealt with later.
		 */
		file_buffer->size = read_bytes(file, file_buffer->data,
			block_size);
		if(file_buffer->size == -1)
			goto read_err;

		bytes += file_buffer->size;

		if(blocks > 1) {
			/* non-tail block should be exactly block_size */
			if(file_buffer->size < block_size)
				goto restat;

			file_buffer->fragment = FALSE;
			put_file_buffer(file_buffer);
		}
	} while(-- blocks > 0);

	/* Overall size including tail should match */
	if(read_size != bytes)
		goto restat;

	if(read_size && read_size % block_size == 0) {
		/*
		 * Special case where we've not tried to read past the end of
		 * the file.  We expect to get EOF, i.e. the file isn't larger
		 * than we expect.
		 */
		char buffer;
		int res;

		res = read_bytes(file, &buffer, 1);
		if(res == -1)
			goto read_err;

		if(res != 0)
			goto restat;
	}

	file_buffer->fragment = is_fragment(inode);
	put_file_buffer(file_buffer);

	close(file);

	return;

restat:
	res = fstat(file, &buf2);
	if(res == -1) {
		ERROR("Cannot stat dir/file %s because %s\n",
			pathname_reader(dir_ent), strerror(errno));
		goto read_err;
	}

	if(read_size != buf2.st_size) {
		close(file);
		memcpy(buf, &buf2, sizeof(struct stat));
		file_buffer->error = 2;
		put_file_buffer(file_buffer);
		goto again;
	}
read_err:
	close(file);
read_err2:
	file_buffer->error = TRUE;
	put_file_buffer(file_buffer);
}


void reader_scan(struct dir_info *dir) {
	struct dir_ent *dir_ent = dir->list;

	for(; dir_ent; dir_ent = dir_ent->next) {
		struct stat *buf = &dir_ent->inode->buf;
		if(dir_ent->inode->root_entry)
			continue;

		if(IS_PSEUDO_PROCESS(dir_ent->inode)) {
			reader_read_process(dir_ent);
			continue;
		}

		switch(buf->st_mode & S_IFMT) {
			case S_IFREG:
				reader_read_file(dir_ent);
				break;
			case S_IFDIR:
				reader_scan(dir_ent->dir);
				break;
		}
	}
}


void *reader(void *arg)
{
	if(!sorted)
		reader_scan(queue_get(to_reader));
	else {
		int i;
		struct priority_entry *entry;

		queue_get(to_reader);
		for(i = 65535; i >= 0; i--)
			for(entry = priority_list[i]; entry;
							entry = entry->next)
				reader_read_file(entry->dir);
	}

	pthread_exit(NULL);
}


void *writer(void *arg)
{
	while(1) {
		struct file_buffer *file_buffer = queue_get(to_writer);
		off_t off;

		if(file_buffer == NULL) {
			queue_put(from_writer, NULL);
			continue;
		}

		off = file_buffer->block;

		pthread_cleanup_push((void *) pthread_mutex_unlock, &pos_mutex);
		pthread_mutex_lock(&pos_mutex);

		if(lseek(fd, start_offset + off, SEEK_SET) == -1) {
			ERROR("writer: Lseek on destination failed because "
				"%s, offset=0x%llx\n", strerror(errno), start_offset + off);
			BAD_ERROR("Probably out of space on output "
				"%s\n", block_device ? "block device" :
				"filesystem");
		}

		if(write_bytes(fd, file_buffer->data,
				file_buffer->size) == -1)
			BAD_ERROR("Failed to write to output %s\n",
				block_device ? "block device" : "filesystem");

		pthread_cleanup_pop(1);

		cache_block_put(file_buffer);
	}
}


int all_zero(struct file_buffer *file_buffer)
{
	int i;
	long entries = file_buffer->size / sizeof(long);
	long *p = (long *) file_buffer->data;

	for(i = 0; i < entries && p[i] == 0; i++);

	if(i == entries) {
		for(i = file_buffer->size & ~(sizeof(long) - 1);
			i < file_buffer->size && file_buffer->data[i] == 0;
			i++);

		return i == file_buffer->size;
	}

	return 0;
}


void *deflator(void *arg)
{
	struct file_buffer *write_buffer = cache_get_nohash(bwriter_buffer);
	void *stream = NULL;
	int res;

	res = compressor_init(comp, &stream, block_size, 1);
	if(res)
		BAD_ERROR("deflator:: compressor_init failed\n");

	while(1) {
		struct file_buffer *file_buffer = queue_get(to_deflate);

		if(sparse_files && all_zero(file_buffer)) { 
			file_buffer->c_byte = 0;
			seq_queue_put(to_main, file_buffer);
		} else {
			write_buffer->c_byte = mangle2(stream,
				write_buffer->data, file_buffer->data,
				file_buffer->size, block_size,
				file_buffer->noD, 1);
			write_buffer->sequence = file_buffer->sequence;
			write_buffer->file_size = file_buffer->file_size;
			write_buffer->block = file_buffer->block;
			write_buffer->size = SQUASHFS_COMPRESSED_SIZE_BLOCK
				(write_buffer->c_byte);
			write_buffer->fragment = FALSE;
			write_buffer->error = FALSE;
			cache_block_put(file_buffer);
			seq_queue_put(to_main, write_buffer);
			write_buffer = cache_get_nohash(bwriter_buffer);
		}
	}
}


void *frag_deflator(void *arg)
{
	void *stream = NULL;
	int res;

	res = compressor_init(comp, &stream, block_size, 1);
	if(res)
		BAD_ERROR("frag_deflator:: compressor_init failed\n");

	pthread_cleanup_push((void *) pthread_mutex_unlock, &fragment_mutex);

	while(1) {
		int c_byte, compressed_size;
		struct file_buffer *file_buffer = queue_get(to_frag);
		struct file_buffer *write_buffer =
			cache_get(fwriter_buffer, file_buffer->block);

		c_byte = mangle2(stream, write_buffer->data, file_buffer->data,
			file_buffer->size, block_size, noF, 1);
		compressed_size = SQUASHFS_COMPRESSED_SIZE_BLOCK(c_byte);
		write_buffer->size = compressed_size;
		pthread_mutex_lock(&fragment_mutex);
		if(fragments_locked == FALSE) {
			fragment_table[file_buffer->block].size = c_byte;
			fragment_table[file_buffer->block].start_block = bytes;
			write_buffer->block = bytes;
			bytes += compressed_size;
			fragments_outstanding --;
			queue_put(to_writer, write_buffer);
			log_fragment(file_buffer->block, fragment_table[file_buffer->block].start_block);
			pthread_mutex_unlock(&fragment_mutex);
			TRACE("Writing fragment %lld, uncompressed size %d, "
				"compressed size %d\n", file_buffer->block,
				file_buffer->size, compressed_size);
		} else {
				add_pending_fragment(write_buffer, c_byte,
					file_buffer->block);
				pthread_mutex_unlock(&fragment_mutex);
		}
		cache_block_put(file_buffer);
	}

	pthread_cleanup_pop(0);
}


void *frag_order_deflator(void *arg)
{
	void *stream = NULL;
	int res;

	res = compressor_init(comp, &stream, block_size, 1);
	if(res)
		BAD_ERROR("frag_deflator:: compressor_init failed\n");

	while(1) {
		int c_byte;
		struct file_buffer *file_buffer = queue_get(to_frag);
		struct file_buffer *write_buffer =
			cache_get(fwriter_buffer, file_buffer->block);

		c_byte = mangle2(stream, write_buffer->data, file_buffer->data,
			file_buffer->size, block_size, noF, 1);
		write_buffer->block = file_buffer->block;
		write_buffer->sequence = file_buffer->sequence;
		write_buffer->size = c_byte;
		write_buffer->fragment = FALSE;
		seq_queue_put(to_order, write_buffer);
		TRACE("Writing fragment %lld, uncompressed size %d, "
			"compressed size %d\n", file_buffer->block,
			file_buffer->size, SQUASHFS_COMPRESSED_SIZE_BLOCK(c_byte));
		cache_block_put(file_buffer);
	}
}


void *frag_orderer(void *arg)
{
	pthread_cleanup_push((void *) pthread_mutex_unlock, &fragment_mutex);

	while(1) {
		struct file_buffer *write_buffer = seq_queue_get(to_order);
		int block = write_buffer->block;

		pthread_mutex_lock(&fragment_mutex);
		fragment_table[block].size = write_buffer->size;
		fragment_table[block].start_block = bytes;
		write_buffer->block = bytes;
		bytes += SQUASHFS_COMPRESSED_SIZE_BLOCK(write_buffer->size);
		write_buffer->size = SQUASHFS_COMPRESSED_SIZE_BLOCK(write_buffer->size);
		fragments_outstanding --;
		log_fragment(block, write_buffer->block);
		queue_put(to_writer, write_buffer);
		pthread_cond_signal(&fragment_waiting);
		pthread_mutex_unlock(&fragment_mutex);
	}

	pthread_cleanup_pop(0);
}


struct file_buffer *get_file_buffer()
{
	struct file_buffer *file_buffer = seq_queue_get(to_main);

	return file_buffer;
}


void write_file_empty(squashfs_inode *inode, struct dir_ent *dir_ent,
	struct file_buffer *file_buffer, int *duplicate_file)
{
	file_count ++;
	*duplicate_file = FALSE;
	cache_block_put(file_buffer);
	create_inode(inode, NULL, dir_ent, SQUASHFS_FILE_TYPE, 0, 0, 0,
		 NULL, &empty_fragment, NULL, 0);
}


void write_file_frag(squashfs_inode *inode, struct dir_ent *dir_ent,
	struct file_buffer *file_buffer, int *duplicate_file)
{
	int size = file_buffer->file_size;
	struct fragment *fragment;
	unsigned short checksum = file_buffer->checksum;
	char dont_put;

	fragment = frag_duplicate(file_buffer, &dont_put);
	*duplicate_file = !fragment;
	if(!fragment) {
		fragment = get_and_fill_fragment(file_buffer, dir_ent);
		if(duplicate_checking)
			add_non_dup(size, 0, NULL, 0, fragment, 0, checksum,
				TRUE, TRUE);
	}

	if(dont_put)
		free(file_buffer);
	else
		cache_block_put(file_buffer);

	total_bytes += size;
	file_count ++;

	inc_progress_bar();

	create_inode(inode, NULL, dir_ent, SQUASHFS_FILE_TYPE, size, 0,
			0, NULL, fragment, NULL, 0);

	if(!duplicate_checking)
		free_fragment(fragment);
}


void log_file(struct dir_ent *dir_ent, long long start)
{
	if(logging && start)
		fprintf(log_fd, "%s, %lld\n", pathname(dir_ent), start);
}


int write_file_process(squashfs_inode *inode, struct dir_ent *dir_ent,
	struct file_buffer *read_buffer, int *duplicate_file)
{
	long long read_size, file_bytes, start;
	struct fragment *fragment;
	unsigned int *block_list = NULL;
	int block = 0, status;
	long long sparse = 0;
	struct file_buffer *fragment_buffer = NULL;

	*duplicate_file = FALSE;

	if(reproducible)
		ensure_fragments_flushed();
	else
		lock_fragments();

	file_bytes = 0;
	start = bytes;
	while (1) {
		read_size = read_buffer->file_size;
		if(read_buffer->fragment) {
			fragment_buffer = read_buffer;
			if(block == 0)
				start=0;
		} else {
			block_list = realloc(block_list, (block + 1) *
				sizeof(unsigned int));
			if(block_list == NULL)
				MEM_ERROR();
			block_list[block ++] = read_buffer->c_byte;
			if(read_buffer->c_byte) {
				read_buffer->block = bytes;
				bytes += read_buffer->size;
				cache_hash(read_buffer, read_buffer->block);
				file_bytes += read_buffer->size;
				queue_put(to_writer, read_buffer);
			} else {
				sparse += read_buffer->size;
				cache_block_put(read_buffer);
			}
		}
		inc_progress_bar();

		if(read_size != -1)
			break;

		read_buffer = get_file_buffer();
		if(read_buffer->error)
			goto read_err;
	}

	if(!reproducible)
		unlock_fragments();

	fragment = get_and_fill_fragment(fragment_buffer, dir_ent);

	if(duplicate_checking)
		add_non_dup(read_size, file_bytes, block_list, start, fragment,
			0, fragment_buffer ? fragment_buffer->checksum : 0,
			FALSE, TRUE);
	cache_block_put(fragment_buffer);
	file_count ++;
	total_bytes += read_size;

	create_inode(inode, NULL, dir_ent, SQUASHFS_FILE_TYPE, read_size, start,
		 block, block_list, fragment, NULL, sparse);
	log_file(dir_ent, start);

	if(duplicate_checking == FALSE) {
		free(block_list);
		free_fragment(fragment);
	}

	return 0;

read_err:
	dec_progress_bar(block);
	status = read_buffer->error;
	bytes = start;
	if(!block_device) {
		int res;

		queue_put(to_writer, NULL);
		if(queue_get(from_writer) != 0)
			EXIT_MKSQUASHFS();
		res = ftruncate(fd, bytes);
		if(res != 0)
			BAD_ERROR("Failed to truncate dest file because %s\n",
				strerror(errno));
	}
	if(!reproducible)
		unlock_fragments();
	free(block_list);
	cache_block_put(read_buffer);
	return status;
}


int write_file_blocks_dup(squashfs_inode *inode, struct dir_ent *dir_ent,
	struct file_buffer *read_buffer, int *duplicate_file)
{
	int block, thresh;
	long long read_size = read_buffer->file_size;
	long long file_bytes, dup_start, start;
	struct fragment *fragment;
	struct file_info *dupl_ptr;
	int blocks = (read_size + block_size - 1) >> block_log;
	unsigned int *block_list, *block_listp;
	struct file_buffer **buffer_list;
	int status;
	long long sparse = 0;
	struct file_buffer *fragment_buffer = NULL;

	block_list = malloc(blocks * sizeof(unsigned int));
	if(block_list == NULL)
		MEM_ERROR();
	block_listp = block_list;

	buffer_list = malloc(blocks * sizeof(struct file_buffer *));
	if(buffer_list == NULL)
		MEM_ERROR();

	if(reproducible)
		ensure_fragments_flushed();
	else
		lock_fragments();

	file_bytes = 0;
	start = dup_start = bytes;
	thresh = blocks > bwriter_size ? blocks - bwriter_size : 0;

	for(block = 0; block < blocks;) {
		if(read_buffer->fragment) {
			block_list[block] = 0;
			buffer_list[block] = NULL;
			fragment_buffer = read_buffer;
			blocks = read_size >> block_log;
		} else {
			block_list[block] = read_buffer->c_byte;

			if(read_buffer->c_byte) {
				read_buffer->block = bytes;
				bytes += read_buffer->size;
				file_bytes += read_buffer->size;
				cache_hash(read_buffer, read_buffer->block);
				if(block < thresh) {
					buffer_list[block] = NULL;
					queue_put(to_writer, read_buffer);
				} else
					buffer_list[block] = read_buffer;
			} else {
				buffer_list[block] = NULL;
				sparse += read_buffer->size;
				cache_block_put(read_buffer);
			}
		}
		inc_progress_bar();

		if(++block < blocks) {
			read_buffer = get_file_buffer();
			if(read_buffer->error)
				goto read_err;
		}
	}

	dupl_ptr = duplicate(read_size, file_bytes, &block_listp, &dup_start,
		&fragment, fragment_buffer, blocks, 0, FALSE);

	if(dupl_ptr) {
		*duplicate_file = FALSE;
		for(block = thresh; block < blocks; block ++)
			if(buffer_list[block])
				queue_put(to_writer, buffer_list[block]);
		fragment = get_and_fill_fragment(fragment_buffer, dir_ent);
		dupl_ptr->fragment = fragment;
	} else {
		*duplicate_file = TRUE;
		for(block = thresh; block < blocks; block ++)
			cache_block_put(buffer_list[block]);
		bytes = start;
		if(thresh && !block_device) {
			int res;

			queue_put(to_writer, NULL);
			if(queue_get(from_writer) != 0)
				EXIT_MKSQUASHFS();
			res = ftruncate(fd, bytes);
			if(res != 0)
				BAD_ERROR("Failed to truncate dest file because"
					"  %s\n", strerror(errno));
		}
	}

	if(!reproducible)
		unlock_fragments();
	cache_block_put(fragment_buffer);
	free(buffer_list);
	file_count ++;
	total_bytes += read_size;

	/*
	 * sparse count is needed to ensure squashfs correctly reports a
 	 * a smaller block count on stat calls to sparse files.  This is
 	 * to ensure intelligent applications like cp correctly handle the
 	 * file as a sparse file.  If the file in the original filesystem isn't
 	 * stored as a sparse file then still store it sparsely in squashfs, but
 	 * report it as non-sparse on stat calls to preserve semantics
 	 */
	if(sparse && (dir_ent->inode->buf.st_blocks << 9) >= read_size)
		sparse = 0;

	create_inode(inode, NULL, dir_ent, SQUASHFS_FILE_TYPE, read_size,
		dup_start, blocks, block_listp, fragment, NULL, sparse);

	if(*duplicate_file == TRUE)
		free(block_list);
	else
		log_file(dir_ent, dup_start);

	return 0;

read_err:
	dec_progress_bar(block);
	status = read_buffer->error;
	bytes = start;
	if(thresh && !block_device) {
		int res;

		queue_put(to_writer, NULL);
		if(queue_get(from_writer) != 0)
			EXIT_MKSQUASHFS();
		res = ftruncate(fd, bytes);
		if(res != 0)
			BAD_ERROR("Failed to truncate dest file because %s\n",
				strerror(errno));
	}
	if(!reproducible)
		unlock_fragments();
	for(blocks = thresh; blocks < block; blocks ++)
		cache_block_put(buffer_list[blocks]);
	free(buffer_list);
	free(block_list);
	cache_block_put(read_buffer);
	return status;
}


int write_file_blocks(squashfs_inode *inode, struct dir_ent *dir_ent,
	struct file_buffer *read_buffer, int *dup)
{
	long long read_size = read_buffer->file_size;
	long long file_bytes, start;
	struct fragment *fragment;
	unsigned int *block_list;
	int block, status;
	int blocks = (read_size + block_size - 1) >> block_log;
	long long sparse = 0;
	struct file_buffer *fragment_buffer = NULL;

	if(pre_duplicate(read_size))
		return write_file_blocks_dup(inode, dir_ent, read_buffer, dup);

	*dup = FALSE;

	block_list = malloc(blocks * sizeof(unsigned int));
	if(block_list == NULL)
		MEM_ERROR();

	if(reproducible)
		ensure_fragments_flushed();
	else
		lock_fragments();

	file_bytes = 0;
	start = bytes;
	for(block = 0; block < blocks;) {
		if(read_buffer->fragment) {
			block_list[block] = 0;
			fragment_buffer = read_buffer;
			blocks = read_size >> block_log;
		} else {
			block_list[block] = read_buffer->c_byte;
			if(read_buffer->c_byte) {
				read_buffer->block = bytes;
				bytes += read_buffer->size;
				cache_hash(read_buffer, read_buffer->block);
				file_bytes += read_buffer->size;
				queue_put(to_writer, read_buffer);
			} else {
				sparse += read_buffer->size;
				cache_block_put(read_buffer);
			}
		}
		inc_progress_bar();

		if(++block < blocks) {
			read_buffer = get_file_buffer();
			if(read_buffer->error)
				goto read_err;
		}
	}

	if(!reproducible)
		unlock_fragments();
	fragment = get_and_fill_fragment(fragment_buffer, dir_ent);

	if(duplicate_checking)
		add_non_dup(read_size, file_bytes, block_list, start, fragment,
			0, fragment_buffer ? fragment_buffer->checksum : 0,
			FALSE, TRUE);
	cache_block_put(fragment_buffer);
	file_count ++;
	total_bytes += read_size;

	/*
	 * sparse count is needed to ensure squashfs correctly reports a
 	 * a smaller block count on stat calls to sparse files.  This is
 	 * to ensure intelligent applications like cp correctly handle the
 	 * file as a sparse file.  If the file in the original filesystem isn't
 	 * stored as a sparse file then still store it sparsely in squashfs, but
 	 * report it as non-sparse on stat calls to preserve semantics
 	 */
	if(sparse && (dir_ent->inode->buf.st_blocks << 9) >= read_size)
		sparse = 0;

	create_inode(inode, NULL, dir_ent, SQUASHFS_FILE_TYPE, read_size, start,
		 blocks, block_list, fragment, NULL, sparse);
	log_file(dir_ent, start);

	if(duplicate_checking == FALSE) {
		free(block_list);
		free_fragment(fragment);
	}

	return 0;

read_err:
	dec_progress_bar(block);
	status = read_buffer->error;
	bytes = start;
	if(!block_device) {
		int res;

		queue_put(to_writer, NULL);
		if(queue_get(from_writer) != 0)
			EXIT_MKSQUASHFS();
		res = ftruncate(fd, bytes);
		if(res != 0)
			BAD_ERROR("Failed to truncate dest file because %s\n",
				strerror(errno));
	}
	if(!reproducible)
		unlock_fragments();
	free(block_list);
	cache_block_put(read_buffer);
	return status;
}


void write_file(squashfs_inode *inode, struct dir_ent *dir, int *dup)
{
	int status;
	struct file_buffer *read_buffer;

again:
	read_buffer = get_file_buffer();
	status = read_buffer->error;

	if(status)
		cache_block_put(read_buffer);
	else if(read_buffer->file_size == -1)
		status = write_file_process(inode, dir, read_buffer, dup);
	else if(read_buffer->file_size == 0)
		write_file_empty(inode, dir, read_buffer, dup);
	else if(read_buffer->fragment && read_buffer->c_byte)
		write_file_frag(inode, dir, read_buffer, dup);
	else
		status = write_file_blocks(inode, dir, read_buffer, dup);

	if(status == 2) {
		ERROR("File %s changed size while reading filesystem, "
			"attempting to re-read\n", pathname(dir));
		goto again;
	} else if(status == 1) {
		ERROR_START("Failed to read file %s", pathname(dir));
		ERROR_EXIT(", creating empty file\n");
		write_file_empty(inode, dir, NULL, dup);
	}
}


#define BUFF_SIZE 512
char *name;
char *basename_r();

char *getbase(char *pathname)
{
	static char *b_buffer = NULL;
	static int b_size = BUFF_SIZE;
	char *result;

	if(b_buffer == NULL) {
		b_buffer = malloc(b_size);
		if(b_buffer == NULL)
			MEM_ERROR();
	}

	while(1) {
		if(*pathname != '/') {
			result = getcwd(b_buffer, b_size);
			if(result == NULL && errno != ERANGE)
				BAD_ERROR("Getcwd failed in getbase\n");

			/* enough room for pathname + "/" + '\0' terminator? */
			if(result && strlen(pathname) + 2 <=
						b_size - strlen(b_buffer)) {
				strcat(strcat(b_buffer, "/"), pathname);
				break;
			}
		} else if(strlen(pathname) < b_size) {
			strcpy(b_buffer, pathname);
			break;
		}

		/* Buffer not large enough, realloc and try again */
		b_buffer = realloc(b_buffer, b_size += BUFF_SIZE);
		if(b_buffer == NULL)
			MEM_ERROR();
	}

	name = b_buffer;
	if(((result = basename_r()) == NULL) || (strcmp(result, "..") == 0))
		return NULL;
	else
		return result;
}


char *basename_r()
{
	char *s;
	char *p;
	int n = 1;

	for(;;) {
		s = name;
		if(*name == '\0')
			return NULL;
		if(*name != '/') {
			while(*name != '\0' && *name != '/') name++;
			n = name - s;
		}
		while(*name == '/') name++;
		if(strncmp(s, ".", n) == 0)
			continue;
		if((*name == '\0') || (strncmp(s, "..", n) == 0) ||
				((p = basename_r()) == NULL)) {
			s[n] = '\0';
			return s;
		}
		if(strcmp(p, "..") == 0)
			continue;
		return p;
	}
}


struct inode_info *lookup_inode3(struct stat *buf, int pseudo, int id,
	char *symlink, int bytes)
{
	int ino_hash = INODE_HASH(buf->st_dev, buf->st_ino);
	struct inode_info *inode;

	/*
	 * Look-up inode in hash table, if it already exists we have a
	 * hard-link, so increment the nlink count and return it.
	 * Don't do the look-up for directories because we don't hard-link
	 * directories.
	 */
	if ((buf->st_mode & S_IFMT) != S_IFDIR) {
		for(inode = inode_info[ino_hash]; inode; inode = inode->next) {
			if(memcmp(buf, &inode->buf, sizeof(struct stat)) == 0) {
				inode->nlink ++;
				return inode;
			}
		}
	}

	inode = malloc(sizeof(struct inode_info) + bytes);
	if(inode == NULL)
		MEM_ERROR();

	if(bytes)
		memcpy(&inode->symlink, symlink, bytes);
	memcpy(&inode->buf, buf, sizeof(struct stat));
	inode->read = FALSE;
	inode->root_entry = FALSE;
	inode->pseudo_file = pseudo;
	inode->pseudo_id = id;
	inode->inode = SQUASHFS_INVALID_BLK;
	inode->nlink = 1;
	inode->inode_number = 0;

	/*
	 * Copy filesystem wide defaults into inode, these filesystem
	 * wide defaults may be altered on an individual inode basis by
	 * user specified actions
	 *
	*/
	inode->no_fragments = no_fragments;
	inode->always_use_fragments = always_use_fragments;
	inode->noD = noD;
	inode->noF = noF;

	inode->next = inode_info[ino_hash];
	inode_info[ino_hash] = inode;

	return inode;
}


struct inode_info *lookup_inode2(struct stat *buf, int pseudo, int id)
{
	return lookup_inode3(buf, pseudo, id, NULL, 0);
}


static inline struct inode_info *lookup_inode(struct stat *buf)
{
	return lookup_inode2(buf, 0, 0);
}


static inline void alloc_inode_no(struct inode_info *inode, unsigned int use_this)
{
	if (inode->inode_number == 0) {
		inode->inode_number = use_this ? : inode_no ++;
		if((inode->buf.st_mode & S_IFMT) == S_IFREG)
			progress_bar_size((inode->buf.st_size + block_size - 1)
								 >> block_log);
	}
}


static inline struct dir_ent *create_dir_entry(char *name, char *source_name,
	char *nonstandard_pathname, struct dir_info *dir)
{
	struct dir_ent *dir_ent = malloc(sizeof(struct dir_ent));
	if(dir_ent == NULL)
		MEM_ERROR();

	dir_ent->name = name;
	dir_ent->source_name = source_name;
	dir_ent->nonstandard_pathname = nonstandard_pathname;
	dir_ent->our_dir = dir;
	dir_ent->inode = NULL;
	dir_ent->next = NULL;

	return dir_ent;
}


static inline void add_dir_entry(struct dir_ent *dir_ent, struct dir_info *sub_dir,
	struct inode_info *inode_info)
{
	struct dir_info *dir = dir_ent->our_dir;

	if(sub_dir)
		sub_dir->dir_ent = dir_ent;
	dir_ent->inode = inode_info;
	dir_ent->dir = sub_dir;

	dir_ent->next = dir->list;
	dir->list = dir_ent;
	dir->count++;
}


static inline void add_dir_entry2(char *name, char *source_name,
	char *nonstandard_pathname, struct dir_info *sub_dir,
	struct inode_info *inode_info, struct dir_info *dir)
{
	struct dir_ent *dir_ent = create_dir_entry(name, source_name,
		nonstandard_pathname, dir);


	add_dir_entry(dir_ent, sub_dir, inode_info);
}


static inline void free_dir_entry(struct dir_ent *dir_ent)
{
	if(dir_ent->name)
		free(dir_ent->name);

	if(dir_ent->source_name)
		free(dir_ent->source_name);

	if(dir_ent->nonstandard_pathname)
		free(dir_ent->nonstandard_pathname);

	/* if this entry has been associated with an inode, then we need
	 * to update the inode nlink count.  Orphaned inodes are harmless, and
	 * is easier to leave them than go to the bother of deleting them */
	if(dir_ent->inode && !dir_ent->inode->root_entry)
		dir_ent->inode->nlink --;

	free(dir_ent);
}


static inline void add_excluded(struct dir_info *dir)
{
	dir->excluded ++;
}


void dir_scan(squashfs_inode *inode, char *pathname,
	struct dir_ent *(_readdir)(struct dir_info *), int progress)
{
	struct stat buf;
	struct dir_ent *dir_ent;
	
	root_dir = dir_scan1(pathname, "", paths, _readdir, 1);
	if(root_dir == NULL)
		return;

	/* Create root directory dir_ent and associated inode, and connect
	 * it to the root directory dir_info structure */
	dir_ent = create_dir_entry("", NULL, pathname,
						scan1_opendir("", "", 0));

	if(pathname[0] == '\0') {
		/*
 		 * dummy top level directory, if multiple sources specified on
		 * command line
		 */
		memset(&buf, 0, sizeof(buf));
		buf.st_mode = (root_mode_opt) ? root_mode | S_IFDIR : S_IRWXU | S_IRWXG | S_IRWXO | S_IFDIR;
		buf.st_uid = getuid();
		buf.st_gid = getgid();
		buf.st_mtime = time(NULL);
		buf.st_dev = 0;
		buf.st_ino = 0;
		dir_ent->inode = lookup_inode2(&buf, PSEUDO_FILE_OTHER, 0);
	} else {
		if(lstat(pathname, &buf) == -1)
			/* source directory has disappeared? */
			BAD_ERROR("Cannot stat source directory %s because %s\n",
				pathname, strerror(errno));
		if(root_mode_opt)
			buf.st_mode = root_mode | S_IFDIR;

		dir_ent->inode = lookup_inode(&buf);
	}

	dir_ent->dir = root_dir;
	root_dir->dir_ent = dir_ent;

	/*
	 * Process most actions and any pseudo files
	 */
	if(actions() || get_pseudo())
		dir_scan2(root_dir, get_pseudo());

	/*
	 * Process move actions
	 */
	if(move_actions()) {
		dir_scan3(root_dir);
		do_move_actions();
	}

	/*
	 * Process prune actions
	 */
	if(prune_actions())
		dir_scan4(root_dir);

	/*
	 * Process empty actions
	 */
	if(empty_actions())
		dir_scan5(root_dir);

 	/*
	 * Sort directories and compute the inode numbers
	 */
	dir_scan6(root_dir);

	alloc_inode_no(dir_ent->inode, root_inode_number);

	eval_actions(root_dir, dir_ent);

	if(sorted)
		generate_file_priorities(root_dir, 0,
			&root_dir->dir_ent->inode->buf);

	if(appending) {
		sigset_t sigmask;

		restore_thread = init_restore_thread();
		sigemptyset(&sigmask);
		sigaddset(&sigmask, SIGINT);
		sigaddset(&sigmask, SIGTERM);
		sigaddset(&sigmask, SIGUSR1);
		if(pthread_sigmask(SIG_BLOCK, &sigmask, NULL) != 0)
			BAD_ERROR("Failed to set signal mask\n");
		write_destination(fd, SQUASHFS_START, 4, "\0\0\0\0");
	}

	queue_put(to_reader, root_dir);

	set_progressbar_state(progress);

	if(sorted)
		sort_files_and_write(root_dir);

	dir_scan7(inode, root_dir);
	dir_ent->inode->inode = *inode;
	dir_ent->inode->type = SQUASHFS_DIR_TYPE;
}


/*
 * dir_scan1 routines...
 * These scan the source directories into memory for processing.
 * Exclude actions are processed here (in contrast to the other actions)
 * because they affect what is scanned.
 */
struct dir_info *scan1_opendir(char *pathname, char *subpath, int depth)
{
	struct dir_info *dir;

	dir = malloc(sizeof(struct dir_info));
	if(dir == NULL)
		MEM_ERROR();

	if(pathname[0] != '\0') {
		dir->linuxdir = opendir(pathname);
		if(dir->linuxdir == NULL) {
			free(dir);
			return NULL;
		}
	}

	dir->pathname = strdup(pathname);
	dir->subpath = strdup(subpath);
	dir->count = 0;
	dir->directory_count = 0;
	dir->dir_is_ldir = TRUE;
	dir->list = NULL;
	dir->depth = depth;
	dir->excluded = 0;

	return dir;
}


struct dir_ent *scan1_encomp_readdir(struct dir_info *dir)
{
	static int index = 0;

	if(dir->count < old_root_entries) {
		int i;

		for(i = 0; i < old_root_entries; i++) {
			if(old_root_entry[i].inode.type == SQUASHFS_DIR_TYPE)
				dir->directory_count ++;
			add_dir_entry2(old_root_entry[i].name, NULL, NULL, NULL,
				&old_root_entry[i].inode, dir);
		}
	}

	while(index < source) {
		char *basename = NULL;
		char *dir_name = getbase(source_path[index]);
		int pass = 1, res;

		if(dir_name == NULL) {
			ERROR_START("Bad source directory %s",
				source_path[index]);
			ERROR_EXIT(" - skipping ...\n");
			index ++;
			continue;
		}
		dir_name = strdup(dir_name);
		for(;;) {
			struct dir_ent *dir_ent = dir->list;

			for(; dir_ent && strcmp(dir_ent->name, dir_name) != 0;
				dir_ent = dir_ent->next);
			if(dir_ent == NULL)
				break;
			ERROR("Source directory entry %s already used! - trying"
				" ", dir_name);
			if(pass == 1)
				basename = dir_name;
			else
				free(dir_name);
			res = asprintf(&dir_name, "%s_%d", basename, pass++);
			if(res == -1)
				BAD_ERROR("asprintf failed in "
					"scan1_encomp_readdir\n");
			ERROR("%s\n", dir_name);
		}
		return create_dir_entry(dir_name, basename,
			strdup(source_path[index ++]), dir);
	}
	return NULL;
}


struct dir_ent *scan1_single_readdir(struct dir_info *dir)
{
	struct dirent *d_name;
	int i;

	if(dir->count < old_root_entries) {
		for(i = 0; i < old_root_entries; i++) {
			if(old_root_entry[i].inode.type == SQUASHFS_DIR_TYPE)
				dir->directory_count ++;
			add_dir_entry2(old_root_entry[i].name, NULL, NULL, NULL,
				&old_root_entry[i].inode, dir);
		}
	}

	if((d_name = readdir(dir->linuxdir)) != NULL) {
		char *basename = NULL;
		char *dir_name = strdup(d_name->d_name);
		int pass = 1, res;

		for(;;) {
			struct dir_ent *dir_ent = dir->list;

			for(; dir_ent && strcmp(dir_ent->name, dir_name) != 0;
				dir_ent = dir_ent->next);
			if(dir_ent == NULL)
				break;
			ERROR("Source directory entry %s already used! - trying"
				" ", dir_name);
			if (pass == 1)
				basename = dir_name;
			else
				free(dir_name);
			res = asprintf(&dir_name, "%s_%d", d_name->d_name, pass++);
			if(res == -1)
				BAD_ERROR("asprintf failed in "
					"scan1_single_readdir\n");
			ERROR("%s\n", dir_name);
		}
		return create_dir_entry(dir_name, basename, NULL, dir);
	}

	return NULL;
}


struct dir_ent *scan1_readdir(struct dir_info *dir)
{
	struct dirent *d_name = readdir(dir->linuxdir);

	return d_name ?
		create_dir_entry(strdup(d_name->d_name), NULL, NULL, dir) :
		NULL;
}


void scan1_freedir(struct dir_info *dir)
{
	if(dir->pathname[0] != '\0')
		closedir(dir->linuxdir);
}


struct dir_info *dir_scan1(char *filename, char *subpath,
	struct pathnames *paths,
	struct dir_ent *(_readdir)(struct dir_info *), int depth)
{
	struct dir_info *dir = scan1_opendir(filename, subpath, depth);
	struct dir_ent *dir_ent;

	if(dir == NULL) {
		ERROR_START("Could not open %s", filename);
		ERROR_EXIT(", skipping...\n");
		return NULL;
	}

	while((dir_ent = _readdir(dir))) {
		struct dir_info *sub_dir;
		struct stat buf;
		struct pathnames *new = NULL;
		char *filename = pathname(dir_ent);
		char *subpath = NULL;
		char *dir_name = dir_ent->name;

		if(strcmp(dir_name, ".") == 0 || strcmp(dir_name, "..") == 0) {
			free_dir_entry(dir_ent);
			continue;
		}

		if(lstat(filename, &buf) == -1) {
			ERROR_START("Cannot stat dir/file %s because %s",
				filename, strerror(errno));
			ERROR_EXIT(", ignoring\n");
			free_dir_entry(dir_ent);
			continue;
		}

		if((buf.st_mode & S_IFMT) != S_IFREG &&
					(buf.st_mode & S_IFMT) != S_IFDIR &&
					(buf.st_mode & S_IFMT) != S_IFLNK &&
					(buf.st_mode & S_IFMT) != S_IFCHR &&
					(buf.st_mode & S_IFMT) != S_IFBLK &&
					(buf.st_mode & S_IFMT) != S_IFIFO &&
					(buf.st_mode & S_IFMT) != S_IFSOCK) {
			ERROR_START("File %s has unrecognised filetype %d",
				filename, buf.st_mode & S_IFMT);
			ERROR_EXIT(", ignoring\n");
			free_dir_entry(dir_ent);
			continue;
		}

		if((old_exclude && old_excluded(filename, &buf)) ||
			(!old_exclude && excluded(dir_name, paths, &new))) {
			add_excluded(dir);
			free_dir_entry(dir_ent);
			continue;
		}

		if(exclude_actions()) {
			subpath = subpathname(dir_ent);
			
			if(eval_exclude_actions(dir_name, filename, subpath,
							&buf, depth, dir_ent)) {
				add_excluded(dir);
				free_dir_entry(dir_ent);
				continue;
			}
		}

		switch(buf.st_mode & S_IFMT) {
		case S_IFDIR:
			if(subpath == NULL)
				subpath = subpathname(dir_ent);

			sub_dir = dir_scan1(filename, subpath, new,
					scan1_readdir, depth + 1);
			if(sub_dir) {
				dir->directory_count ++;
				add_dir_entry(dir_ent, sub_dir,
							lookup_inode(&buf));
			} else
				free_dir_entry(dir_ent);
			break;
		case S_IFLNK: {
			int byte;
			static char buff[65536]; /* overflow safe */

			byte = readlink(filename, buff, 65536);
			if(byte == -1) {
				ERROR_START("Failed to read symlink %s",
								filename);
				ERROR_EXIT(", ignoring\n");
			} else if(byte == 65536) {
				ERROR_START("Symlink %s is greater than 65536 "
							"bytes!", filename);
				ERROR_EXIT(", ignoring\n");
			} else {
				/* readlink doesn't 0 terminate the returned
				 * path */
				buff[byte] = '\0';
				add_dir_entry(dir_ent, NULL, lookup_inode3(&buf,
							 0, 0, buff, byte + 1));
			}
			break;
		}
		default:
			add_dir_entry(dir_ent, NULL, lookup_inode(&buf));
		}

		free(new);
	}

	scan1_freedir(dir);

	return dir;
}


/*
 * dir_scan2 routines...
 * This processes most actions and any pseudo files
 */
struct dir_ent *scan2_readdir(struct dir_info *dir, struct dir_ent *dir_ent)
{
	if (dir_ent == NULL)
		dir_ent = dir->list;
	else
		dir_ent = dir_ent->next;

	for(; dir_ent && dir_ent->inode->root_entry; dir_ent = dir_ent->next);

	return dir_ent;	
}


struct dir_ent *scan2_lookup(struct dir_info *dir, char *name)
{
	struct dir_ent *dir_ent = dir->list;

	for(; dir_ent && strcmp(dir_ent->name, name) != 0;
					dir_ent = dir_ent->next);

	return dir_ent;
}


void dir_scan2(struct dir_info *dir, struct pseudo *pseudo)
{
	struct dir_ent *dir_ent = NULL;
	struct pseudo_entry *pseudo_ent;
	struct stat buf;
	static int pseudo_ino = 1;
	
	while((dir_ent = scan2_readdir(dir, dir_ent)) != NULL) {
		struct inode_info *inode_info = dir_ent->inode;
		struct stat *buf = &inode_info->buf;
		char *name = dir_ent->name;

		eval_actions(root_dir, dir_ent);

		if((buf->st_mode & S_IFMT) == S_IFDIR)
			dir_scan2(dir_ent->dir, pseudo_subdir(name, pseudo));
	}

	while((pseudo_ent = pseudo_readdir(pseudo)) != NULL) {
		dir_ent = scan2_lookup(dir, pseudo_ent->name);
		if(pseudo_ent->dev->type == 'm') {
			struct stat *buf;
			if(dir_ent == NULL) {
				ERROR_START("Pseudo modify file \"%s\" does "
					"not exist in source filesystem.",
					pseudo_ent->pathname);
				ERROR_EXIT("  Ignoring.\n");
				continue;
			}
			if(dir_ent->inode->root_entry) {
				ERROR_START("Pseudo modify file \"%s\" is a "
					"pre-existing file in the filesystem "
					"being appended to.  It cannot be "\
					"modified.", pseudo_ent->pathname);
				ERROR_EXIT("  Ignoring.\n");
				continue;
			}
			buf = &dir_ent->inode->buf;
			buf->st_mode = (buf->st_mode & S_IFMT) |
				pseudo_ent->dev->mode;
			buf->st_uid = pseudo_ent->dev->uid;
			buf->st_gid = pseudo_ent->dev->gid;
			continue;
		}

		if(dir_ent) {
			if(dir_ent->inode->root_entry) {
				ERROR_START("Pseudo file \"%s\" is a "
					"pre-existing file in the filesystem "
					"being appended to.",
					pseudo_ent->pathname);
				ERROR_EXIT("  Ignoring.\n");
			} else {
				ERROR_START("Pseudo file \"%s\" exists in "
					"source filesystem \"%s\".",
					pseudo_ent->pathname,
					pathname(dir_ent));
				ERROR_EXIT("\nIgnoring, exclude it (-e/-ef) to "
					"override.\n");
			}
			continue;
		}

		memset(&buf, 0, sizeof(buf));
		buf.st_mode = pseudo_ent->dev->mode;
		buf.st_uid = pseudo_ent->dev->uid;
		buf.st_gid = pseudo_ent->dev->gid;
		buf.st_rdev = makedev(pseudo_ent->dev->major,
			pseudo_ent->dev->minor);
		buf.st_mtime = time(NULL);
		buf.st_ino = pseudo_ino ++;

		if(pseudo_ent->dev->type == 'd') {
			struct dir_ent *dir_ent =
				create_dir_entry(pseudo_ent->name, NULL,
						pseudo_ent->pathname, dir);
			char *subpath = subpathname(dir_ent);
			struct dir_info *sub_dir = scan1_opendir("", subpath,
						dir->depth + 1);
			if(sub_dir == NULL) {
				ERROR_START("Could not create pseudo directory "
					"\"%s\"", pseudo_ent->pathname);
				ERROR_EXIT(", skipping...\n");
				pseudo_ino --;
				continue;
			}
			dir_scan2(sub_dir, pseudo_ent->pseudo);
			dir->directory_count ++;
			add_dir_entry(dir_ent, sub_dir,
				lookup_inode2(&buf, PSEUDO_FILE_OTHER, 0));
		} else if(pseudo_ent->dev->type == 'f') {
			add_dir_entry2(pseudo_ent->name, NULL,
				pseudo_ent->pathname, NULL,
				lookup_inode2(&buf, PSEUDO_FILE_PROCESS,
				pseudo_ent->dev->pseudo_id), dir);
		} else if(pseudo_ent->dev->type == 's') {
			add_dir_entry2(pseudo_ent->name, NULL,
				pseudo_ent->pathname, NULL,
				lookup_inode3(&buf, PSEUDO_FILE_OTHER, 0,
				pseudo_ent->dev->symlink,
				strlen(pseudo_ent->dev->symlink) + 1), dir);
		} else {
			add_dir_entry2(pseudo_ent->name, NULL,
				pseudo_ent->pathname, NULL,
				lookup_inode2(&buf, PSEUDO_FILE_OTHER, 0), dir);
		}
	}
}


/*
 * dir_scan3 routines...
 * This processes the move action
 */
void dir_scan3(struct dir_info *dir)
{
	struct dir_ent *dir_ent = NULL;

	while((dir_ent = scan2_readdir(dir, dir_ent)) != NULL) {

		eval_move_actions(root_dir, dir_ent);

		if((dir_ent->inode->buf.st_mode & S_IFMT) == S_IFDIR)
			dir_scan3(dir_ent->dir);
	}
}


/*
 * dir_scan4 routines...
 * This processes the prune action.  This action is designed to do fine
 * grained tuning of the in-core directory structure after the exclude,
 * move and pseudo actions have been performed.  This allows complex
 * tests to be performed which are impossible at exclude time (i.e.
 * tests which rely on the in-core directory structure)
 */
void free_dir(struct dir_info *dir)
{
	struct dir_ent *dir_ent = dir->list;

	while(dir_ent) {
		struct dir_ent *tmp = dir_ent;

		if((dir_ent->inode->buf.st_mode & S_IFMT) == S_IFDIR)
			free_dir(dir_ent->dir);

		dir_ent = dir_ent->next;
		free_dir_entry(tmp);
	}

	free(dir->pathname);
	free(dir->subpath);
	free(dir);
}
	

void dir_scan4(struct dir_info *dir)
{
	struct dir_ent *dir_ent = dir->list, *prev = NULL;

	while(dir_ent) {
		if(dir_ent->inode->root_entry) {
			prev = dir_ent;
			dir_ent = dir_ent->next;
			continue;
		}

		if((dir_ent->inode->buf.st_mode & S_IFMT) == S_IFDIR)
			dir_scan4(dir_ent->dir);

		if(eval_prune_actions(root_dir, dir_ent)) {
			struct dir_ent *tmp = dir_ent;

			if((dir_ent->inode->buf.st_mode & S_IFMT) == S_IFDIR) {
				free_dir(dir_ent->dir);
				dir->directory_count --;
			}

			dir->count --;

			/* remove dir_ent from list */
			dir_ent = dir_ent->next;
			if(prev)
				prev->next = dir_ent;
			else
				dir->list = dir_ent;
			
			/* free it */
			free_dir_entry(tmp);

			add_excluded(dir);
			continue;
		}

		prev = dir_ent;
		dir_ent = dir_ent->next;
	}
}


/*
 * dir_scan5 routines...
 * This processes the empty action.  This action has to be processed after
 * all other actions because the previous exclude and move actions and the
 * pseudo actions affect whether a directory is empty
 */
void dir_scan5(struct dir_info *dir)
{
	struct dir_ent *dir_ent = dir->list, *prev = NULL;

	while(dir_ent) {
		if(dir_ent->inode->root_entry) {
			prev = dir_ent;
			dir_ent = dir_ent->next;
			continue;
		}

		if((dir_ent->inode->buf.st_mode & S_IFMT) == S_IFDIR) {
			dir_scan5(dir_ent->dir);

			if(eval_empty_actions(root_dir, dir_ent)) {
				struct dir_ent *tmp = dir_ent;

				/*
				 * delete sub-directory, this is by definition
				 * empty
				 */
				free(dir_ent->dir->pathname);
				free(dir_ent->dir->subpath);
				free(dir_ent->dir);

				/* remove dir_ent from list */
				dir_ent = dir_ent->next;
				if(prev)
					prev->next = dir_ent;
				else
					dir->list = dir_ent;
			
				/* free it */
				free_dir_entry(tmp);

				/* update counts */
				dir->directory_count --;
				dir->count --;
				add_excluded(dir);
				continue;
			}
		}

		prev = dir_ent;
		dir_ent = dir_ent->next;
	}
}


/*
 * dir_scan6 routines...
 * This sorts every directory and computes the inode numbers
 */

/*
 * Bottom up linked list merge sort.
 *
 * Qsort and other O(n log n) algorithms work well with arrays but not
 * linked lists.  Merge sort another O(n log n) sort algorithm on the other hand
 * is not ideal for arrays (as it needs an additonal n storage locations
 * as sorting is not done in place), but it is ideal for linked lists because
 * it doesn't require any extra storage,
 */ 
void sort_directory(struct dir_info *dir)
{
	struct dir_ent *cur, *l1, *l2, *next;
	int len1, len2, stride = 1;

	if(dir->list == NULL || dir->count < 2)
		return;

	/*
	 * We can consider our linked-list to be made up of stride length
	 * sublists.  Eacn iteration around this loop merges adjacent
	 * stride length sublists into larger 2*stride sublists.  We stop
	 * when stride becomes equal to the entire list.
	 *
	 * Initially stride = 1 (by definition a sublist of 1 is sorted), and
	 * these 1 element sublists are merged into 2 element sublists,  which
	 * are then merged into 4 element sublists and so on.
	 */
	do {
		l2 = dir->list; /* head of current linked list */
		cur = NULL; /* empty output list */

		/*
		 * Iterate through the linked list, merging adjacent sublists.
		 * On each interation l2 points to the next sublist pair to be
		 * merged (if there's only one sublist left this is simply added
		 * to the output list)
		 */
		while(l2) {
			l1 = l2;
			for(len1 = 0; l2 && len1 < stride; len1 ++, l2 = l2->next);
			len2 = stride;

			/*
			 * l1 points to first sublist.
			 * l2 points to second sublist.
			 * Merge them onto the output list
			 */
			while(len1 && l2 && len2) {
				if(strcmp(l1->name, l2->name) <= 0) {
					next = l1;
					l1 = l1->next;
					len1 --;
				} else {
					next = l2;
					l2 = l2->next;
					len2 --;
				}

				if(cur) {
					cur->next = next;
					cur = next;
				} else
					dir->list = cur = next;
			}
			/*
			 * One sublist is now empty, copy the other one onto the
			 * output list
			 */
			for(; len1; len1 --, l1 = l1->next) {
				if(cur) {
					cur->next = l1;
					cur = l1;
				} else
					dir->list = cur = l1;
			}
			for(; l2 && len2; len2 --, l2 = l2->next) {
				if(cur) {
					cur->next = l2;
					cur = l2;
				} else
					dir->list = cur = l2;
			}
		}
		cur->next = NULL;
		stride = stride << 1;
	} while(stride < dir->count);
}


void dir_scan6(struct dir_info *dir)
{
	struct dir_ent *dir_ent;
	unsigned int byte_count = 0;

	sort_directory(dir);

	for(dir_ent = dir->list; dir_ent; dir_ent = dir_ent->next) {
		byte_count += strlen(dir_ent->name) +
			sizeof(struct squashfs_dir_entry);

		if(dir_ent->inode->root_entry)
			continue;

		alloc_inode_no(dir_ent->inode, 0);

		if((dir_ent->inode->buf.st_mode & S_IFMT) == S_IFDIR)
			dir_scan6(dir_ent->dir);
	}

	if((dir->count < 257 && byte_count < SQUASHFS_METADATA_SIZE))
		dir->dir_is_ldir = FALSE;
}


/*
 * dir_scan6 routines...
 * This generates the filesystem metadata and writes it out to the destination
 */
void scan7_init_dir(struct directory *dir)
{
	dir->buff = malloc(SQUASHFS_METADATA_SIZE);
	if(dir->buff == NULL)
		MEM_ERROR();

	dir->size = SQUASHFS_METADATA_SIZE;
	dir->p = dir->index_count_p = dir->buff;
	dir->entry_count = 256;
	dir->entry_count_p = NULL;
	dir->index = NULL;
	dir->i_count = dir->i_size = 0;
}


struct dir_ent *scan7_readdir(struct directory *dir, struct dir_info *dir_info,
	struct dir_ent *dir_ent)
{
	if (dir_ent == NULL)
		dir_ent = dir_info->list;
	else
		dir_ent = dir_ent->next;

	for(; dir_ent && dir_ent->inode->root_entry; dir_ent = dir_ent->next)
		add_dir(dir_ent->inode->inode, dir_ent->inode->inode_number,
			dir_ent->name, dir_ent->inode->type, dir);

	return dir_ent;	
}


void scan7_freedir(struct directory *dir)
{
	if(dir->index)
		free(dir->index);
	free(dir->buff);
}


void dir_scan7(squashfs_inode *inode, struct dir_info *dir_info)
{
	int squashfs_type;
	int duplicate_file;
	struct directory dir;
	struct dir_ent *dir_ent = NULL;
	
	scan7_init_dir(&dir);
	
	while((dir_ent = scan7_readdir(&dir, dir_info, dir_ent)) != NULL) {
		struct stat *buf = &dir_ent->inode->buf;

		update_info(dir_ent);

		if(dir_ent->inode->inode == SQUASHFS_INVALID_BLK) {
			switch(buf->st_mode & S_IFMT) {
				case S_IFREG:
					squashfs_type = SQUASHFS_FILE_TYPE;
					write_file(inode, dir_ent,
						&duplicate_file);
					INFO("file %s, uncompressed size %lld "
						"bytes %s\n",
						subpathname(dir_ent),
						(long long) buf->st_size,
						duplicate_file ?  "DUPLICATE" :
						 "");
					break;

				case S_IFDIR:
					squashfs_type = SQUASHFS_DIR_TYPE;
					dir_scan7(inode, dir_ent->dir);
					break;

				case S_IFLNK:
					squashfs_type = SQUASHFS_SYMLINK_TYPE;
					create_inode(inode, NULL, dir_ent,
						squashfs_type, 0, 0, 0, NULL,
						NULL, NULL, 0);
					INFO("symbolic link %s inode 0x%llx\n",
						subpathname(dir_ent), *inode);
					sym_count ++;
					break;

				case S_IFCHR:
					squashfs_type = SQUASHFS_CHRDEV_TYPE;
					create_inode(inode, NULL, dir_ent,
						squashfs_type, 0, 0, 0, NULL,
						NULL, NULL, 0);
					INFO("character device %s inode 0x%llx"
						"\n", subpathname(dir_ent),
						*inode);
					dev_count ++;
					break;

				case S_IFBLK:
					squashfs_type = SQUASHFS_BLKDEV_TYPE;
					create_inode(inode, NULL, dir_ent,
						squashfs_type, 0, 0, 0, NULL,
						NULL, NULL, 0);
					INFO("block device %s inode 0x%llx\n",
						subpathname(dir_ent), *inode);
					dev_count ++;
					break;

				case S_IFIFO:
					squashfs_type = SQUASHFS_FIFO_TYPE;
					create_inode(inode, NULL, dir_ent,
						squashfs_type, 0, 0, 0, NULL,
						NULL, NULL, 0);
					INFO("fifo %s inode 0x%llx\n",
						subpathname(dir_ent), *inode);
					fifo_count ++;
					break;

				case S_IFSOCK:
					squashfs_type = SQUASHFS_SOCKET_TYPE;
					create_inode(inode, NULL, dir_ent,
						squashfs_type, 0, 0, 0, NULL,
						NULL, NULL, 0);
					INFO("unix domain socket %s inode "
						"0x%llx\n",
						subpathname(dir_ent), *inode);
					sock_count ++;
					break;

				default:
					BAD_ERROR("%s unrecognised file type, "
						"mode is %x\n",
						subpathname(dir_ent),
						buf->st_mode);
			}
			dir_ent->inode->inode = *inode;
			dir_ent->inode->type = squashfs_type;
		 } else {
			*inode = dir_ent->inode->inode;
			squashfs_type = dir_ent->inode->type;
			switch(squashfs_type) {
				case SQUASHFS_FILE_TYPE:
					if(!sorted)
						INFO("file %s, uncompressed "
							"size %lld bytes LINK"
							"\n",
							subpathname(dir_ent),
							(long long)
							buf->st_size);
					break;
				case SQUASHFS_SYMLINK_TYPE:
					INFO("symbolic link %s inode 0x%llx "
						"LINK\n", subpathname(dir_ent),
						 *inode);
					break;
				case SQUASHFS_CHRDEV_TYPE:
					INFO("character device %s inode 0x%llx "
						"LINK\n", subpathname(dir_ent),
						*inode);
					break;
				case SQUASHFS_BLKDEV_TYPE:
					INFO("block device %s inode 0x%llx "
						"LINK\n", subpathname(dir_ent),
						*inode);
					break;
				case SQUASHFS_FIFO_TYPE:
					INFO("fifo %s inode 0x%llx LINK\n",
						subpathname(dir_ent), *inode);
					break;
				case SQUASHFS_SOCKET_TYPE:
					INFO("unix domain socket %s inode "
						"0x%llx LINK\n",
						subpathname(dir_ent), *inode);
					break;
			}
		}
		
		add_dir(*inode, get_inode_no(dir_ent->inode), dir_ent->name,
			squashfs_type, &dir);
	}

	write_dir(inode, dir_info, &dir);
	INFO("directory %s inode 0x%llx\n", subpathname(dir_info->dir_ent),
		*inode);

	scan7_freedir(&dir);
}


unsigned int slog(unsigned int block)
{
	int i;

	for(i = 12; i <= 20; i++)
		if(block == (1 << i))
			return i;
	return 0;
}


int old_excluded(char *filename, struct stat *buf)
{
	int i;

	for(i = 0; i < exclude; i++)
		if((exclude_paths[i].st_dev == buf->st_dev) &&
				(exclude_paths[i].st_ino == buf->st_ino))
			return TRUE;
	return FALSE;
}


#define ADD_ENTRY(buf) \
	if(exclude % EXCLUDE_SIZE == 0) { \
		exclude_paths = realloc(exclude_paths, (exclude + EXCLUDE_SIZE) \
			* sizeof(struct exclude_info)); \
		if(exclude_paths == NULL) \
			MEM_ERROR(); \
	} \
	exclude_paths[exclude].st_dev = buf.st_dev; \
	exclude_paths[exclude++].st_ino = buf.st_ino;
int old_add_exclude(char *path)
{
	int i;
	char *filename;
	struct stat buf;

	if(path[0] == '/' || strncmp(path, "./", 2) == 0 ||
			strncmp(path, "../", 3) == 0) {
		if(lstat(path, &buf) == -1) {
			ERROR_START("Cannot stat exclude dir/file %s because "
				"%s", path, strerror(errno));
			ERROR_EXIT(", ignoring\n");
			return TRUE;
		}
		ADD_ENTRY(buf);
		return TRUE;
	}

	for(i = 0; i < source; i++) {
		int res = asprintf(&filename, "%s/%s", source_path[i], path);
		if(res == -1)
			BAD_ERROR("asprintf failed in old_add_exclude\n");
		if(lstat(filename, &buf) == -1) {
			if(!(errno == ENOENT || errno == ENOTDIR)) {
				ERROR_START("Cannot stat exclude dir/file %s "
					"because %s", filename, strerror(errno));
				ERROR_EXIT(", ignoring\n");
			}
			free(filename);
			continue;
		}
		free(filename);
		ADD_ENTRY(buf);
	}
	return TRUE;
}


void add_old_root_entry(char *name, squashfs_inode inode, int inode_number,
	int type)
{
	old_root_entry = realloc(old_root_entry,
		sizeof(struct old_root_entry_info) * (old_root_entries + 1));
	if(old_root_entry == NULL)
		MEM_ERROR();

	old_root_entry[old_root_entries].name = strdup(name);
	old_root_entry[old_root_entries].inode.inode = inode;
	old_root_entry[old_root_entries].inode.inode_number = inode_number;
	old_root_entry[old_root_entries].inode.type = type;
	old_root_entry[old_root_entries++].inode.root_entry = TRUE;
}


void initialise_threads(int readq, int fragq, int bwriteq, int fwriteq,
	int freelst, char *destination_file)
{
	int i;
	sigset_t sigmask, old_mask;
	int total_mem = readq;
	int reader_size;
	int fragment_size;
	int fwriter_size;
	/*
	 * bwriter_size is global because it is needed in
	 * write_file_blocks_dup()
	 */

	/*
	 * Never allow the total size of the queues to be larger than
	 * physical memory
	 *
	 * When adding together the possibly user supplied values, make
	 * sure they've not been deliberately contrived to overflow an int
	 */
	if(add_overflow(total_mem, fragq))
		BAD_ERROR("Queue sizes rediculously too large\n");
	total_mem += fragq;
	if(add_overflow(total_mem, bwriteq))
		BAD_ERROR("Queue sizes rediculously too large\n");
	total_mem += bwriteq;
	if(add_overflow(total_mem, fwriteq))
		BAD_ERROR("Queue sizes rediculously too large\n");
	total_mem += fwriteq;

	check_usable_phys_mem(total_mem);

	/*
	 * convert from queue size in Mbytes to queue size in
	 * blocks.
	 *
	 * This isn't going to overflow an int unless there exists
	 * systems with more than 8 Petabytes of RAM!
	 */
	reader_size = readq << (20 - block_log);
	fragment_size = fragq << (20 - block_log);
	bwriter_size = bwriteq << (20 - block_log);
	fwriter_size = fwriteq << (20 - block_log);

	/*
	 * setup signal handlers for the main thread, these cleanup
	 * deleting the destination file, if appending the
	 * handlers for SIGTERM and SIGINT will be replaced with handlers
	 * allowing the user to press ^C twice to restore the existing
	 * filesystem.
	 *
	 * SIGUSR1 is an internal signal, which is used by the sub-threads
	 * to tell the main thread to terminate, deleting the destination file,
	 * or if necessary restoring the filesystem on appending
	 */
	signal(SIGTERM, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGUSR1, sighandler);

	/* block SIGQUIT and SIGHUP, these are handled by the info thread */
	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGQUIT);
	sigaddset(&sigmask, SIGHUP);
	if(pthread_sigmask(SIG_BLOCK, &sigmask, NULL) != 0)
		BAD_ERROR("Failed to set signal mask in intialise_threads\n");

	/*
	 * temporarily block these signals, so the created sub-threads
	 * will ignore them, ensuring the main thread handles them
	 */
	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGTERM);
	sigaddset(&sigmask, SIGUSR1);
	if(pthread_sigmask(SIG_BLOCK, &sigmask, &old_mask) != 0)
		BAD_ERROR("Failed to set signal mask in intialise_threads\n");

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
			ERROR_START("Failed to get number of available "
				"processors.");
			ERROR_EXIT("  Defaulting to 1\n");
			processors = 1;
		}
#else
		processors = sysconf(_SC_NPROCESSORS_ONLN);
#endif
	}

	if(multiply_overflow(processors, 3) ||
			multiply_overflow(processors * 3, sizeof(pthread_t)))
		BAD_ERROR("Processors too large\n");

	deflator_thread = malloc(processors * 3 * sizeof(pthread_t));
	if(deflator_thread == NULL)
		MEM_ERROR();

	frag_deflator_thread = &deflator_thread[processors];
	frag_thread = &frag_deflator_thread[processors];

	to_reader = queue_init(1);
	to_deflate = queue_init(reader_size);
	to_process_frag = queue_init(reader_size);
	to_writer = queue_init(bwriter_size + fwriter_size);
	from_writer = queue_init(1);
	to_frag = queue_init(fragment_size);
	to_main = seq_queue_init();
	if(reproducible)
		to_order = seq_queue_init();
	else
		locked_fragment = queue_init(fragment_size);
	reader_buffer = cache_init(block_size, reader_size, 0, 0);
	bwriter_buffer = cache_init(block_size, bwriter_size, 1, freelst);
	fwriter_buffer = cache_init(block_size, fwriter_size, 1, freelst);
	fragment_buffer = cache_init(block_size, fragment_size, 1, 0);
	reserve_cache = cache_init(block_size, processors + 1, 1, 0);
	pthread_create(&reader_thread, NULL, reader, NULL);
	pthread_create(&writer_thread, NULL, writer, NULL);
	init_progress_bar();
	init_info();

	for(i = 0; i < processors; i++) {
		if(pthread_create(&deflator_thread[i], NULL, deflator, NULL))
			BAD_ERROR("Failed to create thread\n");
		if(pthread_create(&frag_deflator_thread[i], NULL, reproducible ?
				frag_order_deflator : frag_deflator, NULL) != 0)
			BAD_ERROR("Failed to create thread\n");
		if(pthread_create(&frag_thread[i], NULL, frag_thrd,
				(void *) destination_file) != 0)
			BAD_ERROR("Failed to create thread\n");
	}

	main_thread = pthread_self();

	if(reproducible)
		pthread_create(&order_thread, NULL, frag_orderer, NULL);

	if(!quiet)
		printf("Parallel mksquashfs: Using %d processor%s\n", processors,
			processors == 1 ? "" : "s");

	/* Restore the signal mask for the main thread */
	if(pthread_sigmask(SIG_SETMASK, &old_mask, NULL) != 0)
		BAD_ERROR("Failed to set signal mask in intialise_threads\n");
}


long long write_inode_lookup_table()
{
	int i, inode_number, lookup_bytes = SQUASHFS_LOOKUP_BYTES(inode_count);
	void *it;

	if(inode_count == sinode_count)
		goto skip_inode_hash_table;

	it = realloc(inode_lookup_table, lookup_bytes);
	if(it == NULL)
		MEM_ERROR();
	inode_lookup_table = it;

	for(i = 0; i < INODE_HASH_SIZE; i ++) {
		struct inode_info *inode;

		for(inode = inode_info[i]; inode; inode = inode->next) {

			inode_number = get_inode_no(inode);

			/* The empty action will produce orphaned inode
			 * entries in the inode_info[] table.  These
			 * entries because they are orphaned will not be
			 * allocated an inode number in dir_scan5(), so
			 * skip any entries with the default dummy inode
			 * number of 0 */
			if(inode_number == 0)
				continue;

			SQUASHFS_SWAP_LONG_LONGS(&inode->inode,
				&inode_lookup_table[inode_number - 1], 1);

		}
	}

skip_inode_hash_table:
	return generic_write_table(lookup_bytes, inode_lookup_table, 0, NULL,
		noI);
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

	target = get_component(target, &targname);

	if(paths == NULL) {
		paths = malloc(sizeof(struct pathname));
		if(paths == NULL)
			MEM_ERROR();

		paths->names = 0;
		paths->name = NULL;
	}

	for(i = 0; i < paths->names; i++)
		if(strcmp(paths->name[i].name, targname) == 0)
			break;

	if(i == paths->names) {
		/* allocate new name entry */
		paths->names ++;
		paths->name = realloc(paths->name, (i + 1) *
			sizeof(struct path_entry));
		if(paths->name == NULL)
			MEM_ERROR();
		paths->name[i].name = targname;
		paths->name[i].paths = NULL;
		if(use_regex) {
			paths->name[i].preg = malloc(sizeof(regex_t));
			if(paths->name[i].preg == NULL)
				MEM_ERROR();
			error = regcomp(paths->name[i].preg, targname,
				REG_EXTENDED|REG_NOSUB);
			if(error) {
				char str[1024]; /* overflow safe */

				regerror(error, paths->name[i].preg, str, 1024);
				BAD_ERROR("invalid regex %s in export %s, "
					"because %s\n", targname, alltarget,
					str);
			}
		} else
			paths->name[i].preg = NULL;

		if(target[0] == '\0')
			/* at leaf pathname component */
			paths->name[i].paths = NULL;
		else
			/* recurse adding child components */
			paths->name[i].paths = add_path(NULL, target,
				alltarget);
	} else {
		/* existing matching entry */
		free(targname);

		if(paths->name[i].paths == NULL) {
			/* No sub-directory which means this is the leaf
			 * component of a pre-existing exclude which subsumes
			 * the exclude currently being added, in which case stop
			 * adding components */
		} else if(target[0] == '\0') {
			/* at leaf pathname component and child components exist
			 * from more specific excludes, delete as they're
			 * subsumed by this exclude */
			free_path(paths->name[i].paths);
			paths->name[i].paths = NULL;
		} else
			/* recurse adding child components */
			add_path(paths->name[i].paths, target, alltarget);
	}

	return paths;
}


void add_exclude(char *target)
{

	if(target[0] == '/' || strncmp(target, "./", 2) == 0 ||
			strncmp(target, "../", 3) == 0)
		BAD_ERROR("/, ./ and ../ prefixed excludes not supported with "
			"-wildcards or -regex options\n");	
	else if(strncmp(target, "... ", 4) == 0)
		stickypath = add_path(stickypath, target + 4, target + 4);
	else	
		path = add_path(path, target, target);
}


void display_path(int depth, struct pathname *paths)
{
	int i, n;

	if(paths == NULL)
		return;

	for(i = 0; i < paths->names; i++) {
		for(n = 0; n < depth; n++)
			printf("\t");
		printf("%d: %s\n", depth, paths->name[i].name);
		display_path(depth + 1, paths->name[i].paths);
	}
}


void display_path2(struct pathname *paths, char *string)
{
	int i;
	char *path;

	if(paths == NULL) {
		printf("%s\n", string);
		return;
	}

	for(i = 0; i < paths->names; i++) {
		int res = asprintf(&path, "%s/%s", string, paths->name[i].name);
		if(res == -1)
			BAD_ERROR("asprintf failed in display_path2\n");
		display_path2(paths->name[i].paths, path);
		free(path);
	}
}


struct pathnames *add_subdir(struct pathnames *paths, struct pathname *path)
{
	int count = paths == NULL ? 0 : paths->count;

	if(count % PATHS_ALLOC_SIZE == 0) {
		paths = realloc(paths, sizeof(struct pathnames) +
			(count + PATHS_ALLOC_SIZE) * sizeof(struct pathname *));
		if(paths == NULL)
			MEM_ERROR();
	}

	paths->path[count] = path;
	paths->count = count  + 1;
	return paths;
}


int excluded_match(char *name, struct pathname *path, struct pathnames **new)
{
	int i;

	for(i = 0; i < path->names; i++) {
		int match = use_regex ?
			regexec(path->name[i].preg, name, (size_t) 0,
					NULL, 0) == 0 :
			fnmatch(path->name[i].name, name,
				FNM_PATHNAME|FNM_PERIOD|FNM_EXTMATCH) == 0;

		if(match) {
			 if(path->name[i].paths == NULL || new == NULL)
				/* match on a leaf component, any subdirectories
			 	* in the filesystem should be excluded */
				return TRUE;
			else
				/* match on a non-leaf component, add any
				 * subdirectories to the new set of
				 * subdirectories to scan for this name */
				*new = add_subdir(*new, path->name[i].paths);
		}
	}

	return FALSE;
}


int excluded(char *name, struct pathnames *paths, struct pathnames **new)
{
	int n;
		
	if(stickypath && excluded_match(name, stickypath, NULL))
		return TRUE;

	for(n = 0; paths && n < paths->count; n++) {
		int res = excluded_match(name, paths->path[n], new);
		if(res) {
			free(*new);
			*new = NULL;
			return TRUE;
		}
	}

	/*
	 * Either:
	 * -  no matching names found, return empty new search set, or
	 * -  one or more matches with sub-directories found (no leaf matches),
	 *    in which case return new search set.
	 *
	 * In either case return FALSE as we don't want to exclude this entry
	 */
	return FALSE;
}


void process_exclude_file(char *argv)
{
	FILE *fd;
	char buffer[MAX_LINE + 1]; /* overflow safe */
	char *filename;

	fd = fopen(argv, "r");
	if(fd == NULL)
		BAD_ERROR("Failed to open exclude file \"%s\" because %s\n",
			argv, strerror(errno));

	while(fgets(filename = buffer, MAX_LINE + 1, fd) != NULL) {
		int len = strlen(filename);

		if(len == MAX_LINE && filename[len - 1] != '\n')
			/* line too large */
			BAD_ERROR("Line too long when reading "
				"exclude file \"%s\", larger than %d "
				"bytes\n", argv, MAX_LINE);

		/*
		 * Remove '\n' terminator if it exists (the last line
		 * in the file may not be '\n' terminated)
		 */
		if(len && filename[len - 1] == '\n')
			filename[len - 1] = '\0';

		/* Skip any leading whitespace */
		while(isspace(*filename))
			filename ++;

		/* if comment line, skip */
		if(*filename == '#')
			continue;

		/*
		 * check for initial backslash, to accommodate
		 * filenames with leading space or leading # character
		 */
		if(*filename == '\\')
			filename ++;

		/* if line is now empty after skipping characters, skip it */
		if(*filename == '\0')
			continue;

		if(old_exclude)
			old_add_exclude(filename);
		else
			add_exclude(filename);
	}

	if(ferror(fd))
		BAD_ERROR("Reading exclude file \"%s\" failed because %s\n",
			argv, strerror(errno));

	fclose(fd);
}


#define RECOVER_ID "Squashfs recovery file v1.0\n"
#define RECOVER_ID_SIZE 28

void write_recovery_data(struct squashfs_super_block *sBlk)
{
	int res, recoverfd, bytes = sBlk->bytes_used - sBlk->inode_table_start;
	pid_t pid = getpid();
	char *metadata;
	char header[] = RECOVER_ID;

	if(recover == FALSE) {
		printf("No recovery data option specified.\n");
		printf("Skipping saving recovery file.\n\n");
		return;
	}

	metadata = malloc(bytes);
	if(metadata == NULL)
		MEM_ERROR();

	res = read_fs_bytes(fd, sBlk->inode_table_start, bytes, metadata);
	if(res == 0) {
		ERROR("Failed to read append filesystem metadata\n");
		BAD_ERROR("Filesystem corrupted?\n");
	}

	res = asprintf(&recovery_file, "squashfs_recovery_%s_%d",
		getbase(destination_file), pid);
	if(res == -1)
		MEM_ERROR();

	recoverfd = open(recovery_file, O_CREAT | O_TRUNC | O_RDWR, S_IRWXU);
	if(recoverfd == -1)
		BAD_ERROR("Failed to create recovery file, because %s.  "
			"Aborting\n", strerror(errno));
		
	if(write_bytes(recoverfd, header, RECOVER_ID_SIZE) == -1)
		BAD_ERROR("Failed to write recovery file, because %s\n",
			strerror(errno));

	if(write_bytes(recoverfd, sBlk, sizeof(struct squashfs_super_block)) == -1)
		BAD_ERROR("Failed to write recovery file, because %s\n",
			strerror(errno));

	if(write_bytes(recoverfd, metadata, bytes) == -1)
		BAD_ERROR("Failed to write recovery file, because %s\n",
			strerror(errno));

	close(recoverfd);
	free(metadata);
	
	printf("Recovery file \"%s\" written\n", recovery_file);
	printf("If Mksquashfs aborts abnormally (i.e. power failure), run\n");
	printf("mksquashfs dummy %s -recover %s\n", destination_file,
		recovery_file);
	printf("to restore filesystem\n\n");
}


void read_recovery_data(char *recovery_file, char *destination_file)
{
	int fd, recoverfd, bytes;
	struct squashfs_super_block orig_sBlk, sBlk;
	char *metadata;
	int res;
	struct stat buf;
	char header[] = RECOVER_ID;
	char header2[RECOVER_ID_SIZE];

	recoverfd = open(recovery_file, O_RDONLY);
	if(recoverfd == -1)
		BAD_ERROR("Failed to open recovery file because %s\n",
			strerror(errno));

	if(stat(destination_file, &buf) == -1)
		BAD_ERROR("Failed to stat destination file, because %s\n",
			strerror(errno));

	fd = open(destination_file, O_RDWR);
	if(fd == -1)
		BAD_ERROR("Failed to open destination file because %s\n",
			strerror(errno));

	res = read_bytes(recoverfd, header2, RECOVER_ID_SIZE);
	if(res == -1)
		BAD_ERROR("Failed to read recovery file, because %s\n",
			strerror(errno));
	if(res < RECOVER_ID_SIZE)
		BAD_ERROR("Recovery file appears to be truncated\n");
	if(strncmp(header, header2, RECOVER_ID_SIZE) !=0 )
		BAD_ERROR("Not a recovery file\n");

	res = read_bytes(recoverfd, &sBlk, sizeof(struct squashfs_super_block));
	if(res == -1)
		BAD_ERROR("Failed to read recovery file, because %s\n",
			strerror(errno));
	if(res < sizeof(struct squashfs_super_block))
		BAD_ERROR("Recovery file appears to be truncated\n");

	res = read_fs_bytes(fd, 0, sizeof(struct squashfs_super_block), &orig_sBlk);
	if(res == 0) {
		ERROR("Failed to read superblock from output filesystem\n");
		BAD_ERROR("Output filesystem is empty!\n");
	}

	if(memcmp(((char *) &sBlk) + 4, ((char *) &orig_sBlk) + 4,
			sizeof(struct squashfs_super_block) - 4) != 0)
		BAD_ERROR("Recovery file and destination file do not seem to "
			"match\n");

	bytes = sBlk.bytes_used - sBlk.inode_table_start;

	metadata = malloc(bytes);
	if(metadata == NULL)
		MEM_ERROR();

	res = read_bytes(recoverfd, metadata, bytes);
	if(res == -1)
		BAD_ERROR("Failed to read recovery file, because %s\n",
			strerror(errno));
	if(res < bytes)
		BAD_ERROR("Recovery file appears to be truncated\n");

	write_destination(fd, 0, sizeof(struct squashfs_super_block), &sBlk);

	write_destination(fd, sBlk.inode_table_start, bytes, metadata);

	close(recoverfd);
	close(fd);

	printf("Successfully wrote recovery file \"%s\".  Exiting\n",
		recovery_file);
	
	exit(0);
}


void write_filesystem_tables(struct squashfs_super_block *sBlk, int nopad)
{
	int i;

	sBlk->fragments = fragments;
	sBlk->no_ids = id_count;
	sBlk->inode_table_start = write_inodes();
	sBlk->directory_table_start = write_directories();
	sBlk->fragment_table_start = write_fragment_table();
	sBlk->lookup_table_start = exportable ? write_inode_lookup_table() :
		SQUASHFS_INVALID_BLK;
	sBlk->id_table_start = write_id_table();
	sBlk->xattr_id_table_start = write_xattrs();

	TRACE("sBlk->inode_table_start 0x%llx\n", sBlk->inode_table_start);
	TRACE("sBlk->directory_table_start 0x%llx\n",
		sBlk->directory_table_start);
	TRACE("sBlk->fragment_table_start 0x%llx\n", sBlk->fragment_table_start);
	if(exportable)
		TRACE("sBlk->lookup_table_start 0x%llx\n",
			sBlk->lookup_table_start);

	sBlk->bytes_used = bytes;

	sBlk->compression = comp->id;

	SQUASHFS_INSWAP_SUPER_BLOCK(sBlk); 
	write_destination(fd, SQUASHFS_START, sizeof(*sBlk), sBlk);

	if(!nopad && (i = bytes & (4096 - 1))) {
		char temp[4096] = {0};
		write_destination(fd, bytes, 4096 - i, temp);
	}

	close(fd);

	if(recovery_file)
		unlink(recovery_file);

	total_bytes += total_inode_bytes + total_directory_bytes +
		sizeof(struct squashfs_super_block) + total_xattr_bytes;

	if(quiet)
		return;

	printf("\n%sSquashfs %d.%d filesystem, %s compressed, data block size"
		" %d\n", exportable ? "Exportable " : "", SQUASHFS_MAJOR,
		SQUASHFS_MINOR, comp->name, block_size);
	printf("\t%s data, %s metadata, %s fragments,\n\t%s xattrs, %s ids\n",
		noD ? "uncompressed" : "compressed", noI ?  "uncompressed" :
		"compressed", no_fragments ? "no" : noF ? "uncompressed" :
		"compressed", no_xattrs ? "no" : noX ? "uncompressed" :
		"compressed", noI || noId ? "uncompressed" : "compressed");
	printf("\tduplicates are %sremoved\n", duplicate_checking ? "" :
		"not ");
	printf("Filesystem size %.2f Kbytes (%.2f Mbytes)\n", bytes / 1024.0,
		bytes / (1024.0 * 1024.0));
	printf("\t%.2f%% of uncompressed filesystem size (%.2f Kbytes)\n",
		((float) bytes / total_bytes) * 100.0, total_bytes / 1024.0);
	printf("Inode table size %d bytes (%.2f Kbytes)\n",
		inode_bytes, inode_bytes / 1024.0);
	printf("\t%.2f%% of uncompressed inode table size (%d bytes)\n",
		((float) inode_bytes / total_inode_bytes) * 100.0,
		total_inode_bytes);
	printf("Directory table size %d bytes (%.2f Kbytes)\n",
		directory_bytes, directory_bytes / 1024.0);
	printf("\t%.2f%% of uncompressed directory table size (%d bytes)\n",
		((float) directory_bytes / total_directory_bytes) * 100.0,
		total_directory_bytes);
	if(total_xattr_bytes) {
		printf("Xattr table size %d bytes (%.2f Kbytes)\n",
			xattr_bytes, xattr_bytes / 1024.0);
		printf("\t%.2f%% of uncompressed xattr table size (%d bytes)\n",
			((float) xattr_bytes / total_xattr_bytes) * 100.0,
			total_xattr_bytes);
	}
	if(duplicate_checking)
		printf("Number of duplicate files found %d\n", file_count -
			dup_files);
	else
		printf("No duplicate files removed\n");
	printf("Number of inodes %d\n", inode_count);
	printf("Number of files %d\n", file_count);
	if(!no_fragments)
		printf("Number of fragments %d\n", fragments);
	printf("Number of symbolic links  %d\n", sym_count);
	printf("Number of device nodes %d\n", dev_count);
	printf("Number of fifo nodes %d\n", fifo_count);
	printf("Number of socket nodes %d\n", sock_count);
	printf("Number of directories %d\n", dir_count);
	printf("Number of ids (unique uids + gids) %d\n", id_count);
	printf("Number of uids %d\n", uid_count);

	for(i = 0; i < id_count; i++) {
		if(id_table[i]->flags & ISA_UID) {
			struct passwd *user = getpwuid(id_table[i]->id);
			printf("\t%s (%d)\n", user == NULL ? "unknown" :
				user->pw_name, id_table[i]->id);
		}
	}

	printf("Number of gids %d\n", guid_count);

	for(i = 0; i < id_count; i++) {
		if(id_table[i]->flags & ISA_GID) {
			struct group *group = getgrgid(id_table[i]->id);
			printf("\t%s (%d)\n", group == NULL ? "unknown" :
				group->gr_name, id_table[i]->id);
		}
	}
}


int _parse_numberll(char *start, long long *res, int size, int base)
{
	char *end;
	long long number;

	errno = 0; /* To distinguish success/failure after call */

	number = strtoll(start, &end, base);

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


int parse_numberll(char *start, long long *res, int size)
{
	return _parse_numberll(start, res, size, 10);
}


int parse_number(char *start, int *res, int size)
{
	long long number;

	if(!_parse_numberll(start, &number, size, 10))
		return 0;
	
	/* check if long result will overflow signed int */
	if(number > INT_MAX)
		return 0;

	*res = (int) number;
	return 1;
}


int parse_number_unsigned(char *start, unsigned int *res, int size)
{
	long long number;

	if(!_parse_numberll(start, &number, size, 10))
		return 0;
	
	/* check if long result will overflow unsigned int */
	if(number > UINT_MAX)
		return 0;

	*res = (unsigned int) number;
	return 1;
}


int parse_num(char *arg, int *res)
{
	return parse_number(arg, res, 0);
}


int parse_num_unsigned(char *arg, unsigned int *res)
{
	return parse_number_unsigned(arg, res, 0);
}


int parse_mode(char *arg, mode_t *res)
{
	long long number;

	if(!_parse_numberll(arg, &number, 0, 8))
		return 0;

	if(number > 07777)
		return 0;

	*res = (mode_t) number;
	return 1;
}


int get_physical_memory()
{
	/*
	 * Long longs are used here because with PAE, a 32-bit
	 * machine can have more than 4GB of physical memory
	 *
	 * sysconf(_SC_PHYS_PAGES) relies on /proc being mounted.
	 * If it fails use sysinfo, if that fails return 0
	 */
	long long num_pages = sysconf(_SC_PHYS_PAGES);
	long long page_size = sysconf(_SC_PAGESIZE);
	int phys_mem;

	if(num_pages == -1 || page_size == -1) {
		struct sysinfo sys;
		int res = sysinfo(&sys);

		if(res == -1)
			return 0;

		num_pages = sys.totalram;
		page_size = sys.mem_unit;
	}

	phys_mem = num_pages * page_size >> 20;

	if(phys_mem < SQUASHFS_LOWMEM)
		BAD_ERROR("Mksquashfs requires more physical memory than is "
			"available!\n");

	return phys_mem;
}


void check_usable_phys_mem(int total_mem)
{
	/*
	 * We want to allow users to use as much of their physical
	 * memory as they wish.  However, for practical reasons there are
	 * limits which need to be imposed, to protect users from themselves
	 * and to prevent people from using Mksquashfs as a DOS attack by using
	 * all physical memory.   Mksquashfs uses memory to cache data from disk
	 * to optimise performance.  It is pointless to ask it to use more
	 * than 75% of physical memory, as this causes thrashing and it is thus
	 * self-defeating.
	 */
	int mem = get_physical_memory();

	mem = (mem >> 1) + (mem >> 2); /* 75% */
						
	if(total_mem > mem && mem) {
		ERROR("Total memory requested is more than 75%% of physical "
						"memory.\n");
		ERROR("Mksquashfs uses memory to cache data from disk to "
						"optimise performance.\n");
		ERROR("It is pointless to ask it to use more than this amount "
						"of memory, as this\n");
		ERROR("causes thrashing and it is thus self-defeating.\n");
		BAD_ERROR("Requested memory size too large\n");
	}

	if(sizeof(void *) == 4 && total_mem > 2048) {
		/*
		 * If we're running on a kernel with PAE or on a 64-bit kernel,
		 * then the 75% physical memory limit can still easily exceed
		 * the addressable memory by this process.
		 *
		 * Due to the typical kernel/user-space split (1GB/3GB, or
		 * 2GB/2GB), we have to conservatively assume the 32-bit
		 * processes can only address 2-3GB.  So refuse if the user
		 * tries to allocate more than 2GB.
		 */
		ERROR("Total memory requested may exceed maximum "
				"addressable memory by this process\n");
		BAD_ERROR("Requested memory size too large\n");
	}
}


int get_default_phys_mem()
{
	/*
	 * get_physical_memory() relies on /proc being mounted.
	 * If it fails, issue a warning, and use
	 * SQUASHFS_LOWMEM / SQUASHFS_TAKE as default,
	 * and allow a larger value to be set with -mem.
	 */
	int mem = get_physical_memory();

	if(mem == 0) {
		mem = SQUASHFS_LOWMEM / SQUASHFS_TAKE;

		ERROR("Warning: Cannot get size of physical memory, probably "
				"because /proc is missing.\n");
		ERROR("Warning: Defaulting to minimal use of %d Mbytes, use "
				"-mem to set a better value,\n", mem);
		ERROR("Warning: or fix /proc.\n");
	} else
		mem /= SQUASHFS_TAKE;

	if(sizeof(void *) == 4 && mem > 640) {
		/*
		 * If we're running on a kernel with PAE or on a 64-bit kernel,
		 * the default memory usage can exceed the addressable
		 * memory by this process.
		 * Due to the typical kernel/user-space split (1GB/3GB, or
		 * 2GB/2GB), we have to conservatively assume the 32-bit
		 * processes can only address 2-3GB.  So limit the  default
		 * usage to 640M, which gives room for other data.
		 */
		mem = 640;
	}

	return mem;
}


void calculate_queue_sizes(int mem, int *readq, int *fragq, int *bwriteq,
							int *fwriteq)
{
	*readq = mem / SQUASHFS_READQ_MEM;
	*bwriteq = mem / SQUASHFS_BWRITEQ_MEM;
	*fwriteq = mem / SQUASHFS_FWRITEQ_MEM;
	*fragq = mem - *readq - *bwriteq - *fwriteq;
}


void open_log_file(char *filename)
{
	log_fd=fopen(filename, "w");
	if(log_fd == NULL)
		BAD_ERROR("Failed to open log file \"%s\" because %s\n", filename, strerror(errno));

	logging=TRUE;
}


void check_env_var()
{
	char *time_string = getenv("SOURCE_DATE_EPOCH");
	unsigned int time;

	if(time_string != NULL) {
		/*
		 * We cannot have both command-line options and environment
		 * variable trying to set the timestamp(s) at the same
		 * time.  Semantically both are FORCE options which cannot be
		 * over-ridden elsewhere (otherwise they can't be relied on).
		 *
		 * So refuse to continue if both are set.
		 */
		if(mkfs_time_opt || all_time_opt)
			BAD_ERROR("SOURCE_DATE_EPOCH and command line options "
				"can't be used at the same time to set "
				"timestamp(s)\n");

		if(!parse_num_unsigned(time_string, &time)) {
			ERROR("Env Var SOURCE_DATE_EPOCH has invalid time value\n");
			EXIT_MKSQUASHFS();
		}

		all_time = mkfs_time = time;
		all_time_opt = mkfs_time_opt = TRUE;
	}
}


#define VERSION() \
	printf("mksquashfs version 4.4 (2019/08/29)\n");\
	printf("copyright (C) 2019 Phillip Lougher "\
		"<phillip@squashfs.org.uk>\n\n"); \
	printf("This program is free software; you can redistribute it and/or"\
		"\n");\
	printf("modify it under the terms of the GNU General Public License"\
		"\n");\
	printf("as published by the Free Software Foundation; either version "\
		"2,\n");\
	printf("or (at your option) any later version.\n\n");\
	printf("This program is distributed in the hope that it will be "\
		"useful,\n");\
	printf("but WITHOUT ANY WARRANTY; without even the implied warranty "\
		"of\n");\
	printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the"\
		"\n");\
	printf("GNU General Public License for more details.\n");
int main(int argc, char *argv[])
{
	struct stat buf, source_buf;
	int res, i;
	char *b, *root_name = NULL;
	int keep_as_directory = FALSE;
	squashfs_inode inode;
	int readq;
	int fragq;
	int bwriteq;
	int fwriteq;
	int total_mem = get_default_phys_mem();
	int progress = TRUE;
	int force_progress = FALSE;
	struct file_buffer **fragment = NULL;

	if(argc > 1 && strcmp(argv[1], "-version") == 0) {
		VERSION();
		exit(0);
	}

	block_log = slog(block_size);
	calculate_queue_sizes(total_mem, &readq, &fragq, &bwriteq, &fwriteq);

        for(i = 1; i < argc && argv[i][0] != '-'; i++);
	if(i < 3)
		goto printOptions;
	source_path = argv + 1;
	source = i - 2;

	/*
	 * Scan the command line for -comp xxx option, this is to ensure
	 * any -X compressor specific options are passed to the
	 * correct compressor
	 */
	for(; i < argc; i++) {
		struct compressor *prev_comp = comp;
		
		if(strcmp(argv[i], "-comp") == 0) {
			if(++i == argc) {
				ERROR("%s: -comp missing compression type\n",
					argv[0]);
				exit(1);
			}
			comp = lookup_compressor(argv[i]);
			if(!comp->supported) {
				ERROR("%s: Compressor \"%s\" is not supported!"
					"\n", argv[0], argv[i]);
				ERROR("%s: Compressors available:\n", argv[0]);
				display_compressors("", COMP_DEFAULT);
				exit(1);
			}
			if(prev_comp != NULL && prev_comp != comp) {
				ERROR("%s: -comp multiple conflicting -comp"
					" options specified on command line"
					", previously %s, now %s\n", argv[0],
					prev_comp->name, comp->name);
				exit(1);
			}
			compressor_opt_parsed = 1;

		} else if(strcmp(argv[i], "-e") == 0)
			break;
		else if(strcmp(argv[i], "-root-becomes") == 0 ||
				strcmp(argv[i], "-ef") == 0 ||
				strcmp(argv[i], "-pf") == 0 ||
				strcmp(argv[i], "-vaf") == 0 ||
				strcmp(argv[i], "-log") == 0)
			i++;
	}

	/*
	 * if no -comp option specified lookup default compressor.  Note the
	 * Makefile ensures the default compressor has been built, and so we
	 * don't need to to check for failure here
	 */
	if(comp == NULL)
		comp = lookup_compressor(COMP_DEFAULT);

	for(i = source + 2; i < argc; i++) {
		if(strcmp(argv[i], "-mkfs-time") == 0 ||
				strcmp(argv[i], "-fstime") == 0) {
			if((++i == argc) || !parse_num_unsigned(argv[i], &mkfs_time)) {
				ERROR("%s: %s missing or invalid time value\n", argv[0], argv[i - 1]);
				exit(1);
			}
			mkfs_time_opt = TRUE;
		} else if(strcmp(argv[i], "-all-time") == 0) {
			if((++i == argc) || !parse_num_unsigned(argv[i], &all_time)) {
				ERROR("%s: %s missing or invalid time value\n", argv[0], argv[i - 1]);
				exit(1);
			}
			all_time_opt = TRUE;
			clamping = FALSE;
		} else if(strcmp(argv[i], "-reproducible") == 0)
			reproducible = TRUE;
		else if(strcmp(argv[i], "-not-reproducible") == 0)
			reproducible = FALSE;
		else if(strcmp(argv[i], "-root-mode") == 0) {
			if((++i == argc) || !parse_mode(argv[i], &root_mode)) {
				ERROR("%s: -root-mode missing or invalid mode,"
					" octal number <= 07777 expected\n", argv[0]);
				exit(1);
			}
			root_mode_opt = TRUE;
		} else if(strcmp(argv[i], "-log") == 0) {
			if(++i == argc) {
				ERROR("%s: %s missing log file\n",
					argv[0], argv[i - 1]);
				exit(1);
			}
			open_log_file(argv[i]);

		} else if(strcmp(argv[i], "-action") == 0 ||
				strcmp(argv[i], "-a") ==0) {
			if(++i == argc) {
				ERROR("%s: %s missing action\n",
					argv[0], argv[i - 1]);
				exit(1);
			}
			res = parse_action(argv[i], ACTION_LOG_NONE);
			if(res == 0)
				exit(1);

		} else if(strcmp(argv[i], "-verbose-action") == 0 ||
				strcmp(argv[i], "-va") ==0) {
			if(++i == argc) {
				ERROR("%s: %s missing action\n",
					argv[0], argv[i - 1]);
				exit(1);
			}
			res = parse_action(argv[i], ACTION_LOG_VERBOSE);
			if(res == 0)
				exit(1);

		} else if(strcmp(argv[i], "-true-action") == 0 ||
				strcmp(argv[i], "-ta") ==0) {
			if(++i == argc) {
				ERROR("%s: %s missing action\n",
					argv[0], argv[i - 1]);
				exit(1);
			}
			res = parse_action(argv[i], ACTION_LOG_TRUE);
			if(res == 0)
				exit(1);

		} else if(strcmp(argv[i], "-false-action") == 0 ||
				strcmp(argv[i], "-fa") ==0) {
			if(++i == argc) {
				ERROR("%s: %s missing action\n",
					argv[0], argv[i - 1]);
				exit(1);
			}
			res = parse_action(argv[i], ACTION_LOG_FALSE);
			if(res == 0)
				exit(1);

		} else if(strcmp(argv[i], "-action-file") == 0 ||
				strcmp(argv[i], "-af") ==0) {
			if(++i == argc) {
				ERROR("%s: %s missing filename\n", argv[0],
							argv[i - 1]);
				exit(1);
			}
			if(read_action_file(argv[i], ACTION_LOG_NONE) == FALSE)
				exit(1);

		} else if(strcmp(argv[i], "-verbose-action-file") == 0 ||
				strcmp(argv[i], "-vaf") ==0) {
			if(++i == argc) {
				ERROR("%s: %s missing filename\n", argv[0],
							argv[i - 1]);
				exit(1);
			}
			if(read_action_file(argv[i], ACTION_LOG_VERBOSE) == FALSE)
				exit(1);

		} else if(strcmp(argv[i], "-true-action-file") == 0 ||
				strcmp(argv[i], "-taf") ==0) {
			if(++i == argc) {
				ERROR("%s: %s missing filename\n", argv[0],
							argv[i - 1]);
				exit(1);
			}
			if(read_action_file(argv[i], ACTION_LOG_TRUE) == FALSE)
				exit(1);

		} else if(strcmp(argv[i], "-false-action-file") == 0 ||
				strcmp(argv[i], "-faf") ==0) {
			if(++i == argc) {
				ERROR("%s: %s missing filename\n", argv[0],
							argv[i - 1]);
				exit(1);
			}
			if(read_action_file(argv[i], ACTION_LOG_FALSE) == FALSE)
				exit(1);

		} else if(strcmp(argv[i], "-comp") == 0)
			/* parsed previously */
			i++;

		else if(strncmp(argv[i], "-X", 2) == 0) {
			int args;

			if(strcmp(argv[i] + 2, "help") == 0)
				goto print_compressor_options;

			args = compressor_options(comp, argv + i, argc - i);
			if(args < 0) {
				if(args == -1) {
					ERROR("%s: Unrecognised compressor"
						" option %s\n", argv[0],
						argv[i]);
					if(!compressor_opt_parsed)
						ERROR("%s: Did you forget to"
							" specify -comp?\n",
							argv[0]);
print_compressor_options:
					ERROR("%s: selected compressor \"%s\""
						".  Options supported: %s\n",
						argv[0], comp->name,
						comp->usage ? "" : "none");
					if(comp->usage)
						comp->usage();
				}
				exit(1);
			}
			i += args;

		} else if(strcmp(argv[i], "-pf") == 0) {
			if(++i == argc) {
				ERROR("%s: -pf missing filename\n", argv[0]);
				exit(1);
			}
			if(read_pseudo_file(argv[i]) == FALSE)
				exit(1);
		} else if(strcmp(argv[i], "-p") == 0) {
			if(++i == argc) {
				ERROR("%s: -p missing pseudo file definition\n",
					argv[0]);
				exit(1);
			}
			if(read_pseudo_def(argv[i]) == FALSE)
				exit(1);
		} else if(strcmp(argv[i], "-recover") == 0) {
			if(++i == argc) {
				ERROR("%s: -recover missing recovery file\n",
					argv[0]);
				exit(1);
			}
			read_recovery_data(argv[i], argv[source + 1]);
		} else if(strcmp(argv[i], "-no-recovery") == 0)
			recover = FALSE;
		else if(strcmp(argv[i], "-wildcards") == 0) {
			old_exclude = FALSE;
			use_regex = FALSE;
		} else if(strcmp(argv[i], "-regex") == 0) {
			old_exclude = FALSE;
			use_regex = TRUE;
		} else if(strcmp(argv[i], "-no-sparse") == 0)
			sparse_files = FALSE;
		else if(strcmp(argv[i], "-no-progress") == 0)
			progress = FALSE;
		else if(strcmp(argv[i], "-progress") == 0)
			force_progress = TRUE;
		else if(strcmp(argv[i], "-no-exports") == 0)
			exportable = FALSE;
		else if(strcmp(argv[i], "-offset") == 0 || strcmp(argv[i], "-o") == 0) {
			if((++i == argc) || !parse_numberll(argv[i], &start_offset, 1)) {
				ERROR("%s: %s missing or invalid offset size\n", argv[0], argv[i - 1]);
				exit(1);
			}
		} else if(strcmp(argv[i], "-processors") == 0) {
			if((++i == argc) || !parse_num(argv[i], &processors)) {
				ERROR("%s: -processors missing or invalid "
					"processor number\n", argv[0]);
				exit(1);
			}
			if(processors < 1) {
				ERROR("%s: -processors should be 1 or larger\n",
					argv[0]);
				exit(1);
			}
		} else if(strcmp(argv[i], "-read-queue") == 0) {
			if((++i == argc) || !parse_num(argv[i], &readq)) {
				ERROR("%s: -read-queue missing or invalid "
					"queue size\n", argv[0]);
				exit(1);
			}
			if(readq < 1) {
				ERROR("%s: -read-queue should be 1 megabyte or "
					"larger\n", argv[0]);
				exit(1);
			}
		} else if(strcmp(argv[i], "-write-queue") == 0) {
			if((++i == argc) || !parse_num(argv[i], &bwriteq)) {
				ERROR("%s: -write-queue missing or invalid "
					"queue size\n", argv[0]);
				exit(1);
			}
			if(bwriteq < 2) {
				ERROR("%s: -write-queue should be 2 megabytes "
					"or larger\n", argv[0]);
				exit(1);
			}
			fwriteq = bwriteq >> 1;
			bwriteq -= fwriteq;
		} else if(strcmp(argv[i], "-fragment-queue") == 0) {
			if((++i == argc) || !parse_num(argv[i], &fragq)) {
				ERROR("%s: -fragment-queue missing or invalid "
					"queue size\n", argv[0]);
				exit(1);
			}
			if(fragq < 1) {
				ERROR("%s: -fragment-queue should be 1 "
					"megabyte or larger\n", argv[0]);
				exit(1);
			}
		} else if(strcmp(argv[i], "-mem") == 0) {
			long long number;

			if((++i == argc) ||
					!parse_numberll(argv[i], &number, 1)) {
				ERROR("%s: -mem missing or invalid mem size\n",
					 argv[0]);
				exit(1);
			}

			/*
			 * convert from bytes to Mbytes, ensuring the value
			 * does not overflow a signed int
			 */
			if(number >= (1LL << 51)) {
				ERROR("%s: -mem invalid mem size\n", argv[0]);
				exit(1);
			}

			total_mem = number / 1048576;
			if(total_mem < (SQUASHFS_LOWMEM / SQUASHFS_TAKE)) {
				ERROR("%s: -mem should be %d Mbytes or "
					"larger\n", argv[0],
					SQUASHFS_LOWMEM / SQUASHFS_TAKE);
				exit(1);
			}
			calculate_queue_sizes(total_mem, &readq, &fragq,
				&bwriteq, &fwriteq);
		} else if(strcmp(argv[i], "-b") == 0) {
			if(++i == argc) {
				ERROR("%s: -b missing block size\n", argv[0]);
				exit(1);
			}
			if(!parse_number(argv[i], &block_size, 1)) {
				ERROR("%s: -b invalid block size\n", argv[0]);
				exit(1);
			}
			if((block_log = slog(block_size)) == 0) {
				ERROR("%s: -b block size not power of two or "
					"not between 4096 and 1Mbyte\n",
					argv[0]);
				exit(1);
			}
		} else if(strcmp(argv[i], "-ef") == 0) {
			if(++i == argc) {
				ERROR("%s: -ef missing filename\n", argv[0]);
				exit(1);
			}
		} else if(strcmp(argv[i], "-no-duplicates") == 0)
			duplicate_checking = FALSE;

		else if(strcmp(argv[i], "-no-fragments") == 0)
			no_fragments = TRUE;

		 else if(strcmp(argv[i], "-always-use-fragments") == 0)
			always_use_fragments = TRUE;

		 else if(strcmp(argv[i], "-sort") == 0) {
			if(++i == argc) {
				ERROR("%s: -sort missing filename\n", argv[0]);
				exit(1);
			}
		} else if(strcmp(argv[i], "-all-root") == 0 ||
				strcmp(argv[i], "-root-owned") == 0)
			global_uid = global_gid = 0;

		else if(strcmp(argv[i], "-force-uid") == 0) {
			if(++i == argc) {
				ERROR("%s: -force-uid missing uid or user\n",
					argv[0]);
				exit(1);
			}
			if((global_uid = strtoll(argv[i], &b, 10)), *b =='\0') {
				if(global_uid < 0 || global_uid >
						(((long long) 1 << 32) - 1)) {
					ERROR("%s: -force-uid uid out of range"
						"\n", argv[0]);
					exit(1);
				}
			} else {
				struct passwd *uid = getpwnam(argv[i]);
				if(uid)
					global_uid = uid->pw_uid;
				else {
					ERROR("%s: -force-uid invalid uid or "
						"unknown user\n", argv[0]);
					exit(1);
				}
			}
		} else if(strcmp(argv[i], "-force-gid") == 0) {
			if(++i == argc) {
				ERROR("%s: -force-gid missing gid or group\n",
					argv[0]);
				exit(1);
			}
			if((global_gid = strtoll(argv[i], &b, 10)), *b =='\0') {
				if(global_gid < 0 || global_gid >
						(((long long) 1 << 32) - 1)) {
					ERROR("%s: -force-gid gid out of range"
						"\n", argv[0]);
					exit(1);
				}
			} else {
				struct group *gid = getgrnam(argv[i]);
				if(gid)
					global_gid = gid->gr_gid;
				else {
					ERROR("%s: -force-gid invalid gid or "
						"unknown group\n", argv[0]);
					exit(1);
				}
			}
		} else if(strcmp(argv[i], "-noI") == 0 ||
				strcmp(argv[i], "-noInodeCompression") == 0)
			noI = TRUE;

		else if(strcmp(argv[i], "-noId") == 0 ||
				strcmp(argv[i], "-noIdTableCompression") == 0)
			noId = TRUE;

		else if(strcmp(argv[i], "-noD") == 0 ||
				strcmp(argv[i], "-noDataCompression") == 0)
			noD = TRUE;

		else if(strcmp(argv[i], "-noF") == 0 ||
				strcmp(argv[i], "-noFragmentCompression") == 0)
			noF = TRUE;

		else if(strcmp(argv[i], "-noX") == 0 ||
				strcmp(argv[i], "-noXattrCompression") == 0)
			noX = TRUE;

		else if(strcmp(argv[i], "-no-xattrs") == 0)
			no_xattrs = TRUE;

		else if(strcmp(argv[i], "-xattrs") == 0)
			no_xattrs = FALSE;

		else if(strcmp(argv[i], "-nopad") == 0)
			nopad = TRUE;

		else if(strcmp(argv[i], "-info") == 0)
			silent = FALSE;

		else if(strcmp(argv[i], "-e") == 0)
			break;

		else if(strcmp(argv[i], "-noappend") == 0)
			delete = TRUE;

		else if(strcmp(argv[i], "-quiet") == 0)
			quiet = TRUE;

		else if(strcmp(argv[i], "-keep-as-directory") == 0)
			keep_as_directory = TRUE;

		else if(strcmp(argv[i], "-exit-on-error") == 0)
			exit_on_error = TRUE;

		else if(strcmp(argv[i], "-root-becomes") == 0) {
			if(++i == argc) {
				ERROR("%s: -root-becomes: missing name\n",
					argv[0]);
				exit(1);
			}	
			root_name = argv[i];
		} else if(strcmp(argv[i], "-version") == 0) {
			VERSION();
		} else {
			ERROR("%s: invalid option\n\n", argv[0]);
printOptions:
			ERROR("SYNTAX:%s source1 source2 ...  dest [options] "
				"[-e list of exclude\ndirs/files]\n", argv[0]);
			ERROR("\nFilesystem build options:\n");
			ERROR("-comp <comp>\t\tselect <comp> compression\n");
			ERROR("\t\t\tCompressors available:\n");
			display_compressors("\t\t\t", COMP_DEFAULT);
			ERROR("-b <block_size>\t\tset data block to "
				"<block_size>.  Default 128 Kbytes\n");
			ERROR("\t\t\tOptionally a suffix of K or M can be"
				" given to specify\n\t\t\tKbytes or Mbytes"
				" respectively\n");
			ERROR("-reproducible\t\tbuild images that are reproducible"
				REP_STR "\n");
			ERROR("-not-reproducible\tbuild images that are not reproducible"
				NOREP_STR "\n");
			ERROR("-mkfs-time <time>\tset mkfs time to <time> which is an unsigned int\n");
			ERROR("-fstime <time>\t\tsynonym for mkfs-time\n");
			ERROR("-all-time <time>\tset all inode times to <time> which is an unsigned int\n");
			ERROR("-no-exports\t\tdon't make the filesystem "
				"exportable via NFS\n");
			ERROR("-no-sparse\t\tdon't detect sparse files\n");
			ERROR("-no-xattrs\t\tdon't store extended attributes"
				NOXOPT_STR "\n");
			ERROR("-xattrs\t\t\tstore extended attributes" XOPT_STR
				"\n");
			ERROR("-noI\t\t\tdo not compress inode table\n");
			ERROR("-noId\t\t\tdo not compress the uid/gid table"
				" (implied by -noI)\n");
			ERROR("-noD\t\t\tdo not compress data blocks\n");
			ERROR("-noF\t\t\tdo not compress fragment blocks\n");
			ERROR("-noX\t\t\tdo not compress extended "
				"attributes\n");
			ERROR("-no-fragments\t\tdo not use fragments\n");
			ERROR("-always-use-fragments\tuse fragment blocks for "
				"files larger than block size\n");
			ERROR("-no-duplicates\t\tdo not perform duplicate "
				"checking\n");
			ERROR("-all-root\t\tmake all files owned by root\n");
			ERROR("-root-mode <mode>\tset root directory permissions to octal <mode>\n");
			ERROR("-force-uid <uid>\tset all file uids to <uid>\n");
			ERROR("-force-gid <gid>\tset all file gids to <gid>\n");
			ERROR("-nopad\t\t\tdo not pad filesystem to a multiple "
				"of 4K\n");
			ERROR("-keep-as-directory\tif one source directory is "
				"specified, create a root\n");
			ERROR("\t\t\tdirectory containing that directory, "
				"rather than the\n");
			ERROR("\t\t\tcontents of the directory\n");
			ERROR("\nFilesystem filter options:\n");
			ERROR("-p <pseudo-definition>\tAdd pseudo file "
				"definition\n");
			ERROR("-pf <pseudo-file>\tAdd list of pseudo file "
				"definitions\n");
			ERROR("\t\t\tPseudo definitions should be of the "
				"format\n");
			ERROR("\t\t\t\tfilename d mode uid gid\n");
			ERROR("\t\t\t\tfilename m mode uid gid\n");
			ERROR("\t\t\t\tfilename b mode uid gid major minor\n");
			ERROR("\t\t\t\tfilename c mode uid gid major minor\n");
			ERROR("\t\t\t\tfilename f mode uid gid command\n");
			ERROR("\t\t\t\tfilename s mode uid gid symlink\n");
			ERROR("-sort <sort_file>\tsort files according to "
				"priorities in <sort_file>.  One\n");
			ERROR("\t\t\tfile or dir with priority per line.  "
				"Priority -32768 to\n");
			ERROR("\t\t\t32767, default priority 0\n");
			ERROR("-ef <exclude_file>\tlist of exclude dirs/files."
				"  One per line\n");
			ERROR("-wildcards\t\tAllow extended shell wildcards "
				"(globbing) to be used in\n\t\t\texclude "
				"dirs/files\n");
			ERROR("-regex\t\t\tAllow POSIX regular expressions to "
				"be used in exclude\n\t\t\tdirs/files\n");
			ERROR("\nFilesystem append options:\n");
			ERROR("-noappend\t\tdo not append to existing "
				"filesystem\n");
			ERROR("-root-becomes <name>\twhen appending source "
				"files/directories, make the\n");
			ERROR("\t\t\toriginal root become a subdirectory in "
				"the new root\n");
			ERROR("\t\t\tcalled <name>, rather than adding the new "
				"source items\n");
			ERROR("\t\t\tto the original root\n");
			ERROR("\nMksquashfs runtime options:\n");
			ERROR("-version\t\tprint version, licence and "
				"copyright message\n");
			ERROR("-exit-on-error\t\ttreat normally ignored errors "
				"as fatal\n");
			ERROR("-recover <name>\t\trecover filesystem data "
				"using recovery file <name>\n");
			ERROR("-no-recovery\t\tdon't generate a recovery "
				"file\n");
			ERROR("-quiet\t\t\tno verbose output\n");
			ERROR("-info\t\t\tprint files written to filesystem\n");
			ERROR("-no-progress\t\tdon't display the progress "
				"bar\n");
			ERROR("-progress\t\tdisplay progress bar when using "
				"the -info option\n");
			ERROR("-processors <number>\tUse <number> processors."
				"  By default will use number of\n");
			ERROR("\t\t\tprocessors available\n");
			ERROR("-mem <size>\t\tUse <size> physical memory.  "
				"Currently set to %dM\n", total_mem);
			ERROR("\t\t\tOptionally a suffix of K, M or G can be"
				" given to specify\n\t\t\tKbytes, Mbytes or"
				" Gbytes respectively\n");
			ERROR("\nMiscellaneous options:\n");
			ERROR("-root-owned\t\talternative name for -all-root"
				"\n");
			ERROR("-offset <offset>\tSkip <offset> bytes at the "
				"beginning of <dest>.\n");
			ERROR("\t\t\tOptionally a suffix of K, M or G can be"
				" given to specify\n\t\t\tKbytes, Mbytes or"
				" Gbytes respectively.\n");
			ERROR("\t\t\tDefault 0 bytes.\n");
			ERROR("-o <offset>\t\tsynonym for -offset\n");
			ERROR("-noInodeCompression\talternative name for -noI"
				"\n");
			ERROR("-noIdTableCompression\talternative name for -noId"
				"\n");
			ERROR("-noDataCompression\talternative name for -noD"
				"\n");
			ERROR("-noFragmentCompression\talternative name for "
				"-noF\n");
			ERROR("-noXattrCompression\talternative name for "
				"-noX\n");
			ERROR("\n-Xhelp\t\t\tprint compressor options for"
				" selected compressor\n");
			ERROR("\nCompressors available and compressor specific "
				"options:\n");
			display_compressor_usage(COMP_DEFAULT);
			exit(1);
		}
	}

	check_env_var();

	/*
	 * The -noI option implies -noId for backwards compatibility, so reset noId
	 * if both have been specified
	 */
	if(noI && noId)
		noId = FALSE;

	/*
	 * Some compressors may need the options to be checked for validity
	 * once all the options have been processed
	 */
	res = compressor_options_post(comp, block_size);
	if(res)
		EXIT_MKSQUASHFS();

	/*
	 * If the -info option has been selected then disable the
	 * progress bar unless it has been explicitly enabled with
	 * the -progress option
	 */
	if(!silent)
		progress = force_progress;
		
#ifdef SQUASHFS_TRACE
	/*
	 * Disable progress bar if full debug tracing is enabled.
	 * The progress bar in this case just gets in the way of the
	 * debug trace output
	 */
	progress = FALSE;
#endif

	for(i = 0; i < source; i++)
		if(lstat(source_path[i], &source_buf) == -1) {
			fprintf(stderr, "Cannot stat source directory \"%s\" "
				"because %s\n", source_path[i],
				strerror(errno));
			EXIT_MKSQUASHFS();
		}

	destination_file = argv[source + 1];
	if(stat(argv[source + 1], &buf) == -1) {
		if(errno == ENOENT) { /* Does not exist */
			fd = open(argv[source + 1], O_CREAT | O_TRUNC | O_RDWR,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			if(fd == -1) {
				perror("Could not create destination file");
				exit(1);
			}
			delete = TRUE;
		} else {
			perror("Could not stat destination file");
			exit(1);
		}

	} else {
		if(S_ISBLK(buf.st_mode)) {
			if((fd = open(argv[source + 1], O_RDWR)) == -1) {
				perror("Could not open block device as "
					"destination");
				exit(1);
			}
			block_device = 1;

		} else if(S_ISREG(buf.st_mode))	 {
			fd = open(argv[source + 1], (delete ? O_TRUNC : 0) |
				O_RDWR);
			if(fd == -1) {
				perror("Could not open regular file for "
					"writing as destination");
				exit(1);
			}
		}
		else {
			ERROR("Destination not block device or regular file\n");
			exit(1);
		}

	}

	/*
	 * process the exclude files - must be done afer destination file has
	 * been possibly created
	 */
	for(i = source + 2; i < argc; i++)
		if(strcmp(argv[i], "-ef") == 0)
			/*
			 * Note presence of filename arg has already
			 * been checked
			 */
			process_exclude_file(argv[++i]);
		else if(strcmp(argv[i], "-e") == 0)
			break;
		else if(strcmp(argv[i], "-root-becomes") == 0 ||
				strcmp(argv[i], "-sort") == 0 ||
				strcmp(argv[i], "-pf") == 0 ||
				strcmp(argv[i], "-af") == 0 ||
				strcmp(argv[i], "-vaf") == 0 ||
				strcmp(argv[i], "-comp") == 0 ||
				strcmp(argv[i], "-log") == 0)
			i++;

	if(i != argc) {
		if(++i == argc) {
			ERROR("%s: -e missing arguments\n", argv[0]);
			EXIT_MKSQUASHFS();
		}
		while(i < argc)
			if(old_exclude)
				old_add_exclude(argv[i++]);
			else
				add_exclude(argv[i++]);
	}

	/* process the sort files - must be done afer the exclude files  */
	for(i = source + 2; i < argc; i++)
		if(strcmp(argv[i], "-sort") == 0) {
			int res = read_sort_file(argv[++i], source,
								source_path);
			if(res == FALSE)
				BAD_ERROR("Failed to read sort file\n");
			sorted ++;
		} else if(strcmp(argv[i], "-e") == 0)
			break;
		else if(strcmp(argv[i], "-root-becomes") == 0 ||
				strcmp(argv[i], "-ef") == 0 ||
				strcmp(argv[i], "-pf") == 0 ||
				strcmp(argv[i], "-af") == 0 ||
				strcmp(argv[i], "-vaf") == 0 ||
				strcmp(argv[i], "-comp") == 0 ||
				strcmp(argv[i], "-log") == 0)
			i++;

	if(!delete) {
	        comp = read_super(fd, &sBlk, argv[source + 1]);
	        if(comp == NULL) {
			ERROR("Failed to read existing filesystem - will not "
				"overwrite - ABORTING!\n");
			ERROR("To force Mksquashfs to write to this block "
				"device or file use -noappend\n");
			EXIT_MKSQUASHFS();
		}

		block_log = slog(block_size = sBlk.block_size);
		noI = SQUASHFS_UNCOMPRESSED_INODES(sBlk.flags);
		noD = SQUASHFS_UNCOMPRESSED_DATA(sBlk.flags);
		noF = SQUASHFS_UNCOMPRESSED_FRAGMENTS(sBlk.flags);
		noX = SQUASHFS_UNCOMPRESSED_XATTRS(sBlk.flags);
		noId = SQUASHFS_UNCOMPRESSED_IDS(sBlk.flags);
		no_fragments = SQUASHFS_NO_FRAGMENTS(sBlk.flags);
		always_use_fragments = SQUASHFS_ALWAYS_FRAGMENTS(sBlk.flags);
		duplicate_checking = SQUASHFS_DUPLICATES(sBlk.flags);
		exportable = SQUASHFS_EXPORTABLE(sBlk.flags);
		no_xattrs = SQUASHFS_NO_XATTRS(sBlk.flags);
		comp_opts = SQUASHFS_COMP_OPTS(sBlk.flags);
	}

	initialise_threads(readq, fragq, bwriteq, fwriteq, delete,
		destination_file);

	res = compressor_init(comp, &stream, SQUASHFS_METADATA_SIZE, 0);
	if(res)
		BAD_ERROR("compressor_init failed\n");

	if(delete) {
		int size;
		void *comp_data = compressor_dump_options(comp, block_size,
			&size);

		if(!quiet)
			printf("Creating %d.%d filesystem on %s, block size %d.\n",
				SQUASHFS_MAJOR, SQUASHFS_MINOR,
				argv[source + 1], block_size);

		/*
		 * store any compressor specific options after the superblock,
		 * and set the COMP_OPT flag to show that the filesystem has
		 * compressor specfic options
		 */
		if(comp_data) {
			unsigned short c_byte = size | SQUASHFS_COMPRESSED_BIT;
	
			SQUASHFS_INSWAP_SHORTS(&c_byte, 1);
			write_destination(fd, sizeof(struct squashfs_super_block),
				sizeof(c_byte), &c_byte);
			write_destination(fd, sizeof(struct squashfs_super_block) +
				sizeof(c_byte), size, comp_data);
			bytes = sizeof(struct squashfs_super_block) + sizeof(c_byte)
				+ size;
			comp_opts = TRUE;
		} else			
			bytes = sizeof(struct squashfs_super_block);
	} else {
		unsigned int last_directory_block, inode_dir_offset,
			inode_dir_file_size, root_inode_size,
			inode_dir_start_block, uncompressed_data,
			compressed_data, inode_dir_inode_number,
			inode_dir_parent_inode;
		unsigned int root_inode_start =
			SQUASHFS_INODE_BLK(sBlk.root_inode),
			root_inode_offset =
			SQUASHFS_INODE_OFFSET(sBlk.root_inode);

		if((bytes = read_filesystem(root_name, fd, &sBlk, &inode_table,
				&data_cache, &directory_table,
				&directory_data_cache, &last_directory_block,
				&inode_dir_offset, &inode_dir_file_size,
				&root_inode_size, &inode_dir_start_block,
				&file_count, &sym_count, &dev_count, &dir_count,
				&fifo_count, &sock_count, &total_bytes,
				&total_inode_bytes, &total_directory_bytes,
				&inode_dir_inode_number,
				&inode_dir_parent_inode, add_old_root_entry,
				&fragment_table, &inode_lookup_table)) == 0) {
			ERROR("Failed to read existing filesystem - will not "
				"overwrite - ABORTING!\n");
			ERROR("To force Mksquashfs to write to this block "
				"device or file use -noappend\n");
			EXIT_MKSQUASHFS();
		}
		if((append_fragments = fragments = sBlk.fragments)) {
			fragment_table = realloc((char *) fragment_table,
				((fragments + FRAG_SIZE - 1) & ~(FRAG_SIZE - 1))
				 * sizeof(struct squashfs_fragment_entry)); 
			if(fragment_table == NULL)
				BAD_ERROR("Out of memory in save filesystem state\n");
		}

		printf("Appending to existing %d.%d filesystem on %s, block "
			"size %d\n", SQUASHFS_MAJOR, SQUASHFS_MINOR, argv[source + 1],
			block_size);
		printf("All -b, -noI, -noD, -noF, -noX, -noId, -no-duplicates, "
			"-no-fragments,\n-always-use-fragments, -exportable and "
			"-comp options ignored\n");
		printf("\nIf appending is not wanted, please re-run with "
			"-noappend specified!\n\n");

		compressed_data = (inode_dir_offset + inode_dir_file_size) &
			~(SQUASHFS_METADATA_SIZE - 1);
		uncompressed_data = (inode_dir_offset + inode_dir_file_size) &
			(SQUASHFS_METADATA_SIZE - 1);
		
		/* save original filesystem state for restoring ... */
		sfragments = fragments;
		sbytes = bytes;
		sinode_count = sBlk.inodes;
		scache_bytes = root_inode_offset + root_inode_size;
		sdirectory_cache_bytes = uncompressed_data;
		sdata_cache = malloc(scache_bytes);
		if(sdata_cache == NULL)
			BAD_ERROR("Out of memory in save filesystem state\n");
		sdirectory_data_cache = malloc(sdirectory_cache_bytes);
		if(sdirectory_data_cache == NULL)
			BAD_ERROR("Out of memory in save filesystem state\n");
		memcpy(sdata_cache, data_cache, scache_bytes);
		memcpy(sdirectory_data_cache, directory_data_cache +
			compressed_data, sdirectory_cache_bytes);
		sinode_bytes = root_inode_start;
		stotal_bytes = total_bytes;
		stotal_inode_bytes = total_inode_bytes;
		stotal_directory_bytes = total_directory_bytes +
			compressed_data;
		sfile_count = file_count;
		ssym_count = sym_count;
		sdev_count = dev_count;
		sdir_count = dir_count + 1;
		sfifo_count = fifo_count;
		ssock_count = sock_count;
		sdup_files = dup_files;
		sid_count = id_count;
		write_recovery_data(&sBlk);
		save_xattrs();
		appending = TRUE;

		/*
		 * set the filesystem state up to be able to append to the
		 * original filesystem.  The filesystem state differs depending
		 * on whether we're appending to the original root directory, or
		 * if the original root directory becomes a sub-directory
		 * (root-becomes specified on command line, here root_name !=
		 * NULL)
		 */
		inode_bytes = inode_size = root_inode_start;
		directory_size = last_directory_block;
		cache_size = root_inode_offset + root_inode_size;
		directory_cache_size = inode_dir_offset + inode_dir_file_size;
		if(root_name) {
			sdirectory_bytes = last_directory_block;
			sdirectory_compressed_bytes = 0;
			root_inode_number = inode_dir_parent_inode;
			inode_no = sBlk.inodes + 2;
			directory_bytes = last_directory_block;
			directory_cache_bytes = uncompressed_data;
			memmove(directory_data_cache, directory_data_cache +
				compressed_data, uncompressed_data);
			cache_bytes = root_inode_offset + root_inode_size;
			add_old_root_entry(root_name, sBlk.root_inode,
				inode_dir_inode_number, SQUASHFS_DIR_TYPE);
			total_directory_bytes += compressed_data;
			dir_count ++;
		} else {
			sdirectory_compressed_bytes = last_directory_block -
				inode_dir_start_block;
			sdirectory_compressed =
				malloc(sdirectory_compressed_bytes);
			if(sdirectory_compressed == NULL)
				BAD_ERROR("Out of memory in save filesystem "
					"state\n");
			memcpy(sdirectory_compressed, directory_table +
				inode_dir_start_block,
				sdirectory_compressed_bytes); 
			sdirectory_bytes = inode_dir_start_block;
			root_inode_number = inode_dir_inode_number;
			inode_no = sBlk.inodes + 1;
			directory_bytes = inode_dir_start_block;
			directory_cache_bytes = inode_dir_offset;
			cache_bytes = root_inode_offset;
		}

		inode_count = file_count + dir_count + sym_count + dev_count +
			fifo_count + sock_count;
	}

	if(path)
		paths = add_subdir(paths, path);

	dump_actions(); 
	dump_pseudos();

	if(delete && !keep_as_directory && source == 1 &&
			S_ISDIR(source_buf.st_mode))
		dir_scan(&inode, source_path[0], scan1_readdir, progress);
	else if(!keep_as_directory && source == 1 &&
			S_ISDIR(source_buf.st_mode))
		dir_scan(&inode, source_path[0], scan1_single_readdir, progress);
	else
		dir_scan(&inode, "", scan1_encomp_readdir, progress);
	sBlk.root_inode = inode;
	sBlk.inodes = inode_count;
	sBlk.s_magic = SQUASHFS_MAGIC;
	sBlk.s_major = SQUASHFS_MAJOR;
	sBlk.s_minor = SQUASHFS_MINOR;
	sBlk.block_size = block_size;
	sBlk.block_log = block_log;
	sBlk.flags = SQUASHFS_MKFLAGS(noI, noD, noF, noX, noId, no_fragments,
		always_use_fragments, duplicate_checking, exportable,
		no_xattrs, comp_opts);
	sBlk.mkfs_time = mkfs_time_opt ? mkfs_time : time(NULL);

	disable_info();

	while((fragment = get_frag_action(fragment)))
		write_fragment(*fragment);
	if(!reproducible)
		unlock_fragments();
	pthread_cleanup_push((void *) pthread_mutex_unlock, &fragment_mutex);
	pthread_mutex_lock(&fragment_mutex);
	while(fragments_outstanding) {
		pthread_mutex_unlock(&fragment_mutex);
		pthread_testcancel();
		sched_yield();
		pthread_mutex_lock(&fragment_mutex);
	}
	pthread_cleanup_pop(1);

	queue_put(to_writer, NULL);
	if(queue_get(from_writer) != 0)
		EXIT_MKSQUASHFS();

	set_progressbar_state(FALSE);
	write_filesystem_tables(&sBlk, nopad);

	if(logging)
		fclose(log_fd);

	return 0;
}
