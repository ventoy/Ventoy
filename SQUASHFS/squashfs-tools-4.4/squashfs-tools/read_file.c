/*
 * Create a squashfs filesystem.  This is a highly compressed read only
 * filesystem.
 *
 * Copyright (c) 2012
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
 * read_file.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "error.h"

#define TRUE 1
#define FALSE 0
#define MAX_LINE 16384

/*
 * Read file, passing each line to parse_line() for
 * parsing.
 *
 * Lines can be split across multiple lines using "\".
 * 
 * Blank lines and comment lines indicated by # are supported.
 */
int read_file(char *filename, char *type, int (parse_line)(char *))
{
	FILE *fd;
	char *def, *err, *line = NULL;
	int res, size = 0;

	fd = fopen(filename, "r");
	if(fd == NULL) {
		ERROR("Could not open %s device file \"%s\" because %s\n",
			type, filename, strerror(errno));
		return FALSE;
	}

	while(1) {
		int total = 0;

		while(1) {
			int len;

			if(total + (MAX_LINE + 1) > size) {
				line = realloc(line, size += (MAX_LINE + 1));
				if(line == NULL)
					MEM_ERROR();
			}

			err = fgets(line + total, MAX_LINE + 1, fd);
			if(err == NULL)
				break;

			len = strlen(line + total);
			total += len;

			if(len == MAX_LINE && line[total - 1] != '\n') {
				/* line too large */
				ERROR("Line too long when reading "
					"%s file \"%s\", larger than "
					"%d bytes\n", type, filename, MAX_LINE);
				goto failed;
			}

			/*
			 * Remove '\n' terminator if it exists (the last line
			 * in the file may not be '\n' terminated)
			 */
			if(len && line[total - 1] == '\n') {
				line[-- total] = '\0';
				len --;
			}

			/*
			 * If no line continuation then jump out to
			 * process line.  Note, we have to be careful to
			 * check for "\\" (backslashed backslash) and to
			 * ensure we don't look at the previous line
			 */
			if(len == 0 || line[total - 1] != '\\' || (len >= 2 &&
					strcmp(line + total - 2, "\\\\") == 0))
				break;
			else
				total --;
		}	

		if(err == NULL) {
			if(ferror(fd)) {
                		ERROR("Reading %s file \"%s\" failed "
					"because %s\n", type, filename,
					strerror(errno));
				goto failed;
			}

			/*
			 * At EOF, normally we'll be finished, but, have to
			 * check for special case where we had "\" line
			 * continuation and then hit EOF immediately afterwards
			 */
			if(total == 0)
				break;
			else
				line[total] = '\0';
		}

		/* Skip any leading whitespace */
		for(def = line; isspace(*def); def ++);

		/* if line is now empty after skipping characters, skip it */
		if(*def == '\0')
			continue;

		/* if comment line, skip */
		if(*def == '#')
			continue;

		res = parse_line(def);
		if(res == FALSE)
			goto failed;
	}

	fclose(fd);
	free(line);
	return TRUE;

failed:
	fclose(fd);
	free(line);
	return FALSE;
}
