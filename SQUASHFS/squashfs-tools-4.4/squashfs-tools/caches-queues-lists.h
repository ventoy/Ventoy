#ifndef CACHES_QUEUES_LISTS_H
#define CACHES_QUEUES_LISTS_H
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
 * caches-queues-lists.h
 */

#define INSERT_LIST(NAME, TYPE) \
void insert_##NAME##_list(TYPE **list, TYPE *entry) { \
	if(*list) { \
		entry->NAME##_next = *list; \
		entry->NAME##_prev = (*list)->NAME##_prev; \
		(*list)->NAME##_prev->NAME##_next = entry; \
		(*list)->NAME##_prev = entry; \
	} else { \
		*list = entry; \
		entry->NAME##_prev = entry->NAME##_next = entry; \
	} \
}


#define REMOVE_LIST(NAME, TYPE) \
void remove_##NAME##_list(TYPE **list, TYPE *entry) { \
	if(entry->NAME##_prev == entry && entry->NAME##_next == entry) { \
		/* only this entry in the list */ \
		*list = NULL; \
	} else if(entry->NAME##_prev != NULL && entry->NAME##_next != NULL) { \
		/* more than one entry in the list */ \
		entry->NAME##_next->NAME##_prev = entry->NAME##_prev; \
		entry->NAME##_prev->NAME##_next = entry->NAME##_next; \
		if(*list == entry) \
			*list = entry->NAME##_next; \
	} \
	entry->NAME##_prev = entry->NAME##_next = NULL; \
}


#define INSERT_HASH_TABLE(NAME, TYPE, HASH_FUNCTION, FIELD, LINK) \
void insert_##NAME##_hash_table(TYPE *container, struct file_buffer *entry) \
{ \
	int hash = HASH_FUNCTION(entry->FIELD); \
\
	entry->LINK##_next = container->hash_table[hash]; \
	container->hash_table[hash] = entry; \
	entry->LINK##_prev = NULL; \
	if(entry->LINK##_next) \
		entry->LINK##_next->LINK##_prev = entry; \
}


#define REMOVE_HASH_TABLE(NAME, TYPE, HASH_FUNCTION, FIELD, LINK) \
void remove_##NAME##_hash_table(TYPE *container, struct file_buffer *entry) \
{ \
	if(entry->LINK##_prev) \
		entry->LINK##_prev->LINK##_next = entry->LINK##_next; \
	else \
		container->hash_table[HASH_FUNCTION(entry->FIELD)] = \
			entry->LINK##_next; \
	if(entry->LINK##_next) \
		entry->LINK##_next->LINK##_prev = entry->LINK##_prev; \
\
	entry->LINK##_prev = entry->LINK##_next = NULL; \
}

#define HASH_SIZE 65536
#define CALCULATE_HASH(n) ((n) & 0xffff)


/* struct describing a cache entry passed between threads */
struct file_buffer {
	long long index;
	long long sequence;
	long long file_size;
	union {
		long long block;
		unsigned short checksum;
	};
	struct cache *cache;
	union {
		struct file_info *dupl_start;
		struct file_buffer *hash_next;
	};
	union {
		int duplicate;
		struct file_buffer *hash_prev;
	};
	union {
		struct {
			struct file_buffer *free_next;
			struct file_buffer *free_prev;
		};
		struct {
			struct file_buffer *seq_next;
			struct file_buffer *seq_prev;
		};
	};
	int size;
	int c_byte;
	char used;
	char fragment;
	char error;
	char locked;
	char wait_on_unlock;
	char noD;
	char data[0] __attribute__((aligned));
};


/* struct describing queues used to pass data between threads */
struct queue {
	int			size;
	int			readp;
	int			writep;
	pthread_mutex_t		mutex;
	pthread_cond_t		empty;
	pthread_cond_t		full;
	void			**data;
};


/*
 * struct describing seq_queues used to pass data between the read
 * thread and the deflate and main threads
 */
struct seq_queue {
	int			fragment_count;
	int			block_count;
	long long		sequence;
	struct file_buffer	*hash_table[HASH_SIZE];
	pthread_mutex_t		mutex;
	pthread_cond_t		wait;
};


/* Cache status struct.  Caches are used to keep
  track of memory buffers passed between different threads */
struct cache {
	int	max_buffers;
	int	count;
	int	buffer_size;
	int	noshrink_lookup;
	int	first_freelist;
	union {
		int	used;
		int	max_count;
	};
	pthread_mutex_t	mutex;
	pthread_cond_t wait_for_free;
	pthread_cond_t wait_for_unlock;
	struct file_buffer *free_list;
	struct file_buffer *hash_table[HASH_SIZE];
};


extern struct queue *queue_init(int);
extern void queue_put(struct queue *, void *);
extern void *queue_get(struct queue *);
extern int queue_empty(struct queue *);
extern void queue_flush(struct queue *);
extern void dump_queue(struct queue *);
extern struct seq_queue *seq_queue_init();
extern void seq_queue_put(struct seq_queue *, struct file_buffer *);
extern void dump_seq_queue(struct seq_queue *, int);
extern struct file_buffer *seq_queue_get(struct seq_queue *);
extern void seq_queue_flush(struct seq_queue *);
extern struct cache *cache_init(int, int, int, int);
extern struct file_buffer *cache_lookup(struct cache *, long long);
extern struct file_buffer *cache_get(struct cache *, long long);
extern struct file_buffer *cache_get_nohash(struct cache *);
extern void cache_hash(struct file_buffer *, long long);
extern void cache_block_put(struct file_buffer *);
extern void dump_cache(struct cache *);
extern struct file_buffer *cache_get_nowait(struct cache *, long long);
extern struct file_buffer *cache_lookup_nowait(struct cache *, long long,
	char *);
extern void cache_wait_unlock(struct file_buffer *);
extern void cache_unlock(struct file_buffer *);

extern int first_freelist;
#endif
