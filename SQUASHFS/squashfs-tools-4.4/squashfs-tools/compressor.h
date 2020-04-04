#ifndef COMPRESSOR_H
#define COMPRESSOR_H
/*
 *
 * Copyright (c) 2009, 2010, 2011, 2012, 2013, 2014
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
 * compressor.h
 */

struct compressor {
	int id;
	char *name;
	int supported;
	int (*init)(void **, int, int);
	int (*compress)(void *, void *, void *, int, int, int *);
	int (*uncompress)(void *, void *, int, int, int *);
	int (*options)(char **, int);
	int (*options_post)(int);
	void *(*dump_options)(int, int *);
	int (*extract_options)(int, void *, int);
	int (*check_options)(int, void *, int);
	void (*display_options)(void *, int);
	void (*usage)();
};

extern struct compressor *lookup_compressor(char *);
extern struct compressor *lookup_compressor_id(int);
extern void display_compressors(char *, char *);
extern void display_compressor_usage(char *);

static inline int compressor_init(struct compressor *comp, void **stream,
	int block_size, int datablock)
{
	if(comp->init == NULL)
		return 0;
	return comp->init(stream, block_size, datablock);
}


static inline int compressor_compress(struct compressor *comp, void *strm,
	void *dest, void *src, int size, int block_size, int *error)
{
	return comp->compress(strm, dest, src, size, block_size, error);
}


static inline int compressor_uncompress(struct compressor *comp, void *dest,
	void *src, int size, int block_size, int *error)
{
	return comp->uncompress(dest, src, size, block_size, error);
}


/*
 * For the following functions please see the lzo, lz4 or xz
 * compressors for commented examples of how they are used.
 */
static inline int compressor_options(struct compressor *comp, char *argv[],
	int argc)
{
	if(comp->options == NULL)
		return -1;

	return comp->options(argv, argc);
}


static inline int compressor_options_post(struct compressor *comp, int block_size)
{
	if(comp->options_post == NULL)
		return 0;
	return comp->options_post(block_size);
}


static inline void *compressor_dump_options(struct compressor *comp,
	int block_size, int *size)
{
	if(comp->dump_options == NULL)
		return NULL;
	return comp->dump_options(block_size, size);
}


static inline int compressor_extract_options(struct compressor *comp,
	int block_size, void *buffer, int size)
{
	if(comp->extract_options == NULL)
		return size ? -1 : 0;
	return comp->extract_options(block_size, buffer, size);
}


static inline int compressor_check_options(struct compressor *comp,
	int block_size, void *buffer, int size)
{
	if(comp->check_options == NULL)
		return 0;
	return comp->check_options(block_size, buffer, size);
}


static inline void compressor_display_options(struct compressor *comp,
	void *buffer, int size)
{
	if(comp->display_options != NULL)
		comp->display_options(buffer, size);
}
#endif
