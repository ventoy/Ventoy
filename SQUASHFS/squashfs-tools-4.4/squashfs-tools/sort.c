/*
 * Create a squashfs filesystem.  This is a highly compressed read only
 * filesystem.
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012,
 * 2013, 2014
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
 * sort.c
 */

#define TRUE 1
#define FALSE 0
#define MAX_LINE 16384

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "squashfs_fs.h"
#include "mksquashfs.h"
#include "sort.h"
#include "error.h"
#include "progressbar.h"

int mkisofs_style = -1;

struct sort_info {
	dev_t			st_dev;
	ino_t			st_ino;
	int			priority;
	struct sort_info	*next;
};

struct sort_info *sort_info_list[65536];

struct priority_entry *priority_list[65536];

extern int silent;
extern void write_file(squashfs_inode *inode, struct dir_ent *dir_ent,
	int *c_size);
extern char *pathname(struct dir_ent *dir_ent);


void add_priority_list(struct dir_ent *dir, int priority)
{
	struct priority_entry *new_priority_entry;

	priority += 32768;
	new_priority_entry = malloc(sizeof(struct priority_entry));
	if(new_priority_entry == NULL)
		MEM_ERROR();

	new_priority_entry->dir = dir;;
	new_priority_entry->next = priority_list[priority];
	priority_list[priority] = new_priority_entry;
}


int get_priority(char *filename, struct stat *buf, int priority)
{
	int hash = buf->st_ino & 0xffff;
	struct sort_info *s;

	for(s = sort_info_list[hash]; s; s = s->next)
		if((s->st_dev == buf->st_dev) && (s->st_ino == buf->st_ino)) {
			TRACE("returning priority %d (%s)\n", s->priority,
				filename);
			return s->priority;
		}
	TRACE("returning priority %d (%s)\n", priority, filename);
	return priority;
}


#define ADD_ENTRY(buf, priority) {\
	int hash = buf.st_ino & 0xffff;\
	struct sort_info *s;\
	if((s = malloc(sizeof(struct sort_info))) == NULL) \
		MEM_ERROR(); \
	s->st_dev = buf.st_dev;\
	s->st_ino = buf.st_ino;\
	s->priority = priority;\
	s->next = sort_info_list[hash];\
	sort_info_list[hash] = s;\
	}
int add_sort_list(char *path, int priority, int source, char *source_path[])
{
	int i, n;
	struct stat buf;

	TRACE("add_sort_list: filename %s, priority %d\n", path, priority);
	if(strlen(path) > 1 && strcmp(path + strlen(path) - 2, "/*") == 0)
		path[strlen(path) - 2] = '\0';

	TRACE("add_sort_list: filename %s, priority %d\n", path, priority);
re_read:
	if(path[0] == '/' || strncmp(path, "./", 2) == 0 ||
			strncmp(path, "../", 3) == 0 || mkisofs_style == 1) {
		if(lstat(path, &buf) == -1)
			goto error;
		TRACE("adding filename %s, priority %d, st_dev %d, st_ino "
			"%lld\n", path, priority, (int) buf.st_dev,
			(long long) buf.st_ino);
		ADD_ENTRY(buf, priority);
		return TRUE;
	}

	for(i = 0, n = 0; i < source; i++) {
		char *filename;
		int res = asprintf(&filename, "%s/%s", source_path[i], path);
		if(res == -1)
			BAD_ERROR("asprintf failed in add_sort_list\n");
		res = lstat(filename, &buf);
		free(filename);
		if(res == -1) {
			if(!(errno == ENOENT || errno == ENOTDIR))
				goto error;
			continue;
		}
		ADD_ENTRY(buf, priority);
		n ++;
	}

	if(n == 0 && mkisofs_style == -1 && lstat(path, &buf) != -1) {
		ERROR("WARNING: Mkisofs style sortlist detected! This is "
			"supported but please\n");
		ERROR("convert to mksquashfs style sortlist! A sortlist entry");
	        ERROR(" should be\neither absolute (starting with ");
		ERROR("'/') start with './' or '../' (taken to be\nrelative to "
			"$PWD), otherwise it ");
		ERROR("is assumed the entry is relative to one\nof the source "
			"directories, i.e. with ");
		ERROR("\"mksquashfs test test.sqsh\",\nthe sortlist ");
		ERROR("entry \"file\" is assumed to be inside the directory "
			"test.\n\n");
		mkisofs_style = 1;
		goto re_read;
	}

	mkisofs_style = 0;

	if(n == 1)
		return TRUE;
	if(n > 1) {
		ERROR(" Ambiguous sortlist entry \"%s\"\n\nIt maps to more "
			"than one source entry!  Please use an absolute path."
			"\n", path);
		return FALSE;
	}

error:
        ERROR_START("Cannot stat sortlist entry \"%s\"\n", path);
        ERROR("This is probably because you're using the wrong file\n");
        ERROR("path relative to the source directories.");
	ERROR_EXIT("  Ignoring");
	/*
	 * Historical note
	 * Failure to stat a sortlist entry is deliberately ignored, even
	 * though it is an error.  Squashfs release 2.2 changed the behaviour
	 * to treat it as a fatal error, but it was changed back to
	 * the original behaviour to ignore it in release 2.2-r2 following
	 * feedback from users at the time.
	 */
        return TRUE;
}


void generate_file_priorities(struct dir_info *dir, int priority,
	struct stat *buf)
{
	struct dir_ent *dir_ent = dir->list;

	priority = get_priority(dir->pathname, buf, priority);

	for(; dir_ent; dir_ent = dir_ent->next) {
		struct stat *buf = &dir_ent->inode->buf;
		if(dir_ent->inode->root_entry)
			continue;

		switch(buf->st_mode & S_IFMT) {
			case S_IFREG:
				add_priority_list(dir_ent,
					get_priority(pathname(dir_ent), buf,
					priority));
				break;
			case S_IFDIR:
				generate_file_priorities(dir_ent->dir,
					priority, buf);
				break;
		}
	}
}


int read_sort_file(char *filename, int source, char *source_path[])
{
	FILE *fd;
	char line_buffer[MAX_LINE + 1]; /* overflow safe */
	char sort_filename[MAX_LINE + 1]; /* overflow safe */
	char *line, *name;
	int n, priority, res;

	if((fd = fopen(filename, "r")) == NULL) {
		ERROR("Failed to open sort file \"%s\" because %s\n",
			filename, strerror(errno));
		return FALSE;
	}

	while(fgets(line = line_buffer, MAX_LINE + 1, fd) != NULL) {
		int len = strlen(line);

		if(len == MAX_LINE && line[len - 1] != '\n') {
			/* line too large */
			ERROR("Line too long when reading "
				"sort file \"%s\", larger than %d "
				"bytes\n", filename, MAX_LINE);
			goto failed;
		}

		/*
		 * Remove '\n' terminator if it exists (the last line
		 * in the file may not be '\n' terminated)
		 */
		if(len && line[len - 1] == '\n')
			line[len - 1] = '\0';

		/* Skip any leading whitespace */
		while(isspace(*line))
			line ++;

		/* if comment line, skip */
		if(*line == '#')
			continue;

		/*
		 * Scan for filename, don't use sscanf() and "%s" because
		 * that can't handle filenames with spaces
		 */
		for(name = sort_filename; !isspace(*line) && *line != '\0';) {
			if(*line == '\\') {
				line ++;
				if (*line == '\0')
					break;
			}
			*name ++ = *line ++;
		}
		*name = '\0';

		/*
		 * if filename empty, then line was empty of anything but
		 * whitespace or a backslash character.  Skip empy lines
		 */
		if(sort_filename[0] == '\0')
			continue;

		/*
		 * Scan the rest of the line, we expect a decimal number
		 * which is the filename priority
		 */
		errno = 0;
		res = sscanf(line, "%d%n", &priority, &n);

		if((res < 1 || errno) && errno != ERANGE) {
			if(errno == 0)
				/* No error, assume EOL or match failure */
				ERROR("Sort file \"%s\", can't find priority "
					"in entry \"%s\", EOL or match "
					"failure\n", filename, line_buffer);
			else
				/* Some other failure not ERANGE */
				ERROR("Sscanf failed reading sort file \"%s\" "
					"because %s\n", filename,
					strerror(errno));
			goto failed;
		} else if((errno == ERANGE) ||
				(priority < -32768 || priority > 32767)) {
			ERROR("Sort file \"%s\", entry \"%s\" has priority "
				"outside range of -32767:32768.\n", filename,
				line_buffer);
			goto failed;
		}

		/* Skip any trailing whitespace */
		line += n;
		while(isspace(*line))
			line ++;

		if(*line != '\0') {
			ERROR("Sort file \"%s\", trailing characters after "
				"priority in entry \"%s\"\n", filename,
				line_buffer);
			goto failed;
		}

		res = add_sort_list(sort_filename, priority, source,
			source_path);
		if(res == FALSE)
			goto failed;
	}

	if(ferror(fd)) {
		ERROR("Reading sort file \"%s\" failed because %s\n", filename,
			strerror(errno));
		goto failed;
	}

	fclose(fd);
	return TRUE;

failed:
	fclose(fd);
	return FALSE;
}


void sort_files_and_write(struct dir_info *dir)
{
	int i;
	struct priority_entry *entry;
	squashfs_inode inode;
	int duplicate_file;

	for(i = 65535; i >= 0; i--)
		for(entry = priority_list[i]; entry; entry = entry->next) {
			TRACE("%d: %s\n", i - 32768, pathname(entry->dir));
			if(entry->dir->inode->inode == SQUASHFS_INVALID_BLK) {
				write_file(&inode, entry->dir, &duplicate_file);
				INFO("file %s, uncompressed size %lld bytes %s"
					"\n", pathname(entry->dir),
					(long long)
					entry->dir->inode->buf.st_size,
					duplicate_file ? "DUPLICATE" : "");
				entry->dir->inode->inode = inode;
				entry->dir->inode->type = SQUASHFS_FILE_TYPE;
			} else
				INFO("file %s, uncompressed size %lld bytes "
					"LINK\n", pathname(entry->dir),
					(long long)
					entry->dir->inode->buf.st_size);
		}
}
