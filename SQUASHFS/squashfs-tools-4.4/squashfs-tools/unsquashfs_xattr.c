/*
 * Unsquash a squashfs filesystem.  This is a highly compressed read only
 * filesystem.
 *
 * Copyright (c) 2010, 2012, 2019
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
 * unsquashfs_xattr.c
 */

#include "unsquashfs.h"
#include "xattr.h"

#include <sys/xattr.h>

#define NOSPACE_MAX 10

extern int root_process;
extern int user_xattrs;
extern int ignore_errors;
extern int strict_errors;

int write_xattr(char *pathname, unsigned int xattr)
{
	unsigned int count;
	struct xattr_list *xattr_list;
	int i;
	static int nonsuper_error = FALSE;
	static int ignore_xattrs = FALSE;
	static int nospace_error = 0;
	int failed;

	if(ignore_xattrs || xattr == SQUASHFS_INVALID_XATTR ||
			sBlk.s.xattr_id_table_start == SQUASHFS_INVALID_BLK)
		return TRUE;

	xattr_list = get_xattr(xattr, &count, &failed);
	if(failed)
		EXIT_UNSQUASH_STRICT("write_xattr: Failed to read one or more xattrs for %s\n", pathname);

	for(i = 0; i < count; i++) {
		int prefix = xattr_list[i].type & SQUASHFS_XATTR_PREFIX_MASK;

		if(ignore_xattrs || (user_xattrs && prefix != SQUASHFS_XATTR_USER))
			continue;

		if(root_process || prefix == SQUASHFS_XATTR_USER) {
			int res = lsetxattr(pathname, xattr_list[i].full_name,
				xattr_list[i].value, xattr_list[i].vsize, 0);

			if(res == -1) {
				if(errno == ENOTSUP) {
					/*
					 * If the destination filesystem cannot
					 * suppport xattrs, print error, and
					 * disable xattr output as this error is
					 * unlikely to go away, and printing
					 * screenfulls of the same error message
					 * is rather annoying
					 */
					ERROR("write_xattr: failed to write "
						"xattr %s for file %s because " 
						"extended attributes are not "
						"supported by the destination "
						"filesystem\n",
						xattr_list[i].full_name,
						pathname);
					ERROR("Ignoring xattrs in "
								"filesystem\n");
					EXIT_UNSQUASH_STRICT("To avoid this error message, "
						"specify -no-xattrs\n");
					ignore_xattrs = TRUE;
				} else if((errno == ENOSPC || errno == EDQUOT)
						&& nospace_error < NOSPACE_MAX) {
					/*
					 * Many filesystems like ext2/3/4 have
					 * limits on the amount of xattr
					 * data that can be stored per file
					 * (typically one block or 4K), so
					 * we shouldn't disable xattr ouput,
					 * as the error may be restriced to one
					 * file only.  If we get a lot of these
					 * then suppress the error messsage
					 */
					EXIT_UNSQUASH_IGNORE("write_xattr: failed to write "
						"xattr %s for file %s because " 
						"no extended attribute space "
						"remaining (per file or "
						"filesystem limit)\n",
						xattr_list[i].full_name,
						pathname);
					if(++ nospace_error == NOSPACE_MAX)
						ERROR("%d of these errors "
							"printed, further error "
							"messages of this type "
							"are suppressed!\n",
							NOSPACE_MAX);
				} else
					EXIT_UNSQUASH_IGNORE("write_xattr: failed to write "
						"xattr %s for file %s because "
						"%s\n", xattr_list[i].full_name,
						pathname, strerror(errno));
				failed = TRUE;
			}
		} else if(nonsuper_error == FALSE) {
			/*
			 * if extract user xattrs only then
			 * error message is suppressed, if not
			 * print error, and then suppress further error
			 * messages to avoid possible screenfulls of the
			 * same error message!
			 */
			ERROR("write_xattr: could not write xattr %s "
					"for file %s because you're not "
					"superuser!\n",
					xattr_list[i].full_name, pathname);
			EXIT_UNSQUASH_STRICT("write_xattr: to avoid this error message, either"
				" specify -user-xattrs, -no-xattrs, or run as "
				"superuser!\n");
			ERROR("Further error messages of this type are "
				"suppressed!\n");
			nonsuper_error = TRUE;
			failed = TRUE;
		}
	}

	free_xattr(xattr_list, count);

	return !failed;
}
