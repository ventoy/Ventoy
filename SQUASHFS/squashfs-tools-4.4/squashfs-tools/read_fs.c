/*
 * Read a squashfs filesystem.  This is a highly compressed read only
 * filesystem.
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010,
 * 2012, 2013, 2014, 2019
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
 * read_fs.c
 */

#define TRUE 1
#define FALSE 0
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <limits.h>
#include <dirent.h>

#ifndef linux
#define __BYTE_ORDER BYTE_ORDER
#define __BIG_ENDIAN BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#else
#include <endian.h>
#endif

#include <stdlib.h>

#include "squashfs_fs.h"
#include "squashfs_swap.h"
#include "compressor.h"
#include "xattr.h"
#include "error.h"
#include "mksquashfs.h"

int read_block(int fd, long long start, long long *next, int expected,
								void *block)
{
	unsigned short c_byte;
	int res, compressed;
	int outlen = expected ? expected : SQUASHFS_METADATA_SIZE;
	
	/* Read block size */
	res = read_fs_bytes(fd, start, 2, &c_byte);
	if(res == 0)
		return 0;

	SQUASHFS_INSWAP_SHORTS(&c_byte, 1);
	compressed = SQUASHFS_COMPRESSED(c_byte);
	c_byte = SQUASHFS_COMPRESSED_SIZE(c_byte);

	/*
	 * The block size should not be larger than
	 * the uncompressed size (or max uncompressed size if
	 * expected is 0)
	 */
	if (c_byte > outlen)
		return 0;

	if(compressed) {
		char buffer[c_byte];
		int error;

		res = read_fs_bytes(fd, start + 2, c_byte, buffer);
		if(res == 0)
			return 0;

		res = compressor_uncompress(comp, block, buffer, c_byte,
			outlen, &error);
		if(res == -1) {
			ERROR("%s uncompress failed with error code %d\n",
				comp->name, error);
			return 0;
		}
	} else {
		res = read_fs_bytes(fd, start + 2, c_byte, block);
		if(res == 0)
			return 0;
		res = c_byte;
	}

	if(next)
		*next = start + 2 + c_byte;

	/*
	 * if expected, then check the (uncompressed) return data
	 * is of the expected size
	 */
	if(expected && expected != res)
		return 0;
	else
		return res;
}


#define NO_BYTES(SIZE) \
	(bytes - (cur_ptr - inode_table) < (SIZE))

#define NO_INODE_BYTES(INODE) NO_BYTES(sizeof(struct INODE))

unsigned char *scan_inode_table(int fd, long long start, long long end,
	long long root_inode_start, int root_inode_offset,
	struct squashfs_super_block *sBlk, union squashfs_inode_header
	*dir_inode, unsigned int *root_inode_block, unsigned int
	*root_inode_size, long long *uncompressed_file, unsigned int
	*uncompressed_directory, int *file_count, int *sym_count, int
	*dev_count, int *dir_count, int *fifo_count, int *sock_count,
	unsigned int *id_table)
{
	unsigned char *cur_ptr;
	unsigned char *inode_table = NULL;
	int byte, files = 0;
	unsigned int directory_start_block, bytes = 0, size = 0;
	struct squashfs_base_inode_header base;

	TRACE("scan_inode_table: start 0x%llx, end 0x%llx, root_inode_start "
		"0x%llx\n", start, end, root_inode_start);

	*root_inode_block = UINT_MAX;
	while(start < end) {
		if(start == root_inode_start) {
			TRACE("scan_inode_table: read compressed block 0x%llx "
				"containing root inode\n", start);
			*root_inode_block = bytes;
		}
		if(size - bytes < SQUASHFS_METADATA_SIZE) {
			inode_table = realloc(inode_table, size
				+= SQUASHFS_METADATA_SIZE);
			if(inode_table == NULL)
				MEM_ERROR();
		}
		TRACE("scan_inode_table: reading block 0x%llx\n", start);
		byte = read_block(fd, start, &start, 0, inode_table + bytes);
		if(byte == 0)
			goto corrupted;

		bytes += byte;

		/* If this is not the last metadata block in the inode table
		 * then it should be SQUASHFS_METADATA_SIZE in size.
		 * Note, we can't use expected in read_block() above for this
		 * because we don't know if this is the last block until
		 * after reading.
		 */
		if(start != end && byte != SQUASHFS_METADATA_SIZE)
			goto corrupted;
	}

	/*
	 * We expect to have found the metadata block containing the
	 * root inode in the above inode_table metadata block scan.  If it
	 * hasn't been found then the filesystem is corrupted
	 */
	if(*root_inode_block == UINT_MAX)
		goto corrupted;

	/*
	 * The number of bytes available after the root inode medata block
	 * should be at least the root inode offset + the size of a
	 * regular directory inode, if not the filesystem is corrupted
	 *
	 *	+-----------------------+-----------------------+
	 *	| 			|        directory	|
	 *	|			|          inode	|
	 *	+-----------------------+-----------------------+
	 *	^			^			^
	 *	*root_inode_block	root_inode_offset	bytes
	 */
	if((bytes - *root_inode_block) < (root_inode_offset +
			sizeof(struct squashfs_dir_inode_header)))
		goto corrupted;

	/*
	 * Read last inode entry which is the root directory inode, and obtain
	 * the last directory start block index.  This is used when calculating
	 * the total uncompressed directory size.  The directory bytes in the
	 * last * block will be counted as normal.
	 *
	 * Note, the previous check ensures the following calculation won't
	 * underflow, and we won't access beyond the buffer
	 */
	*root_inode_size = bytes - (*root_inode_block + root_inode_offset);
	bytes = *root_inode_block + root_inode_offset;
	SQUASHFS_SWAP_DIR_INODE_HEADER(inode_table + bytes, &dir_inode->dir);
	
	if(dir_inode->base.inode_type == SQUASHFS_DIR_TYPE)
		directory_start_block = dir_inode->dir.start_block;
	else if(dir_inode->base.inode_type == SQUASHFS_LDIR_TYPE) {
		if(*root_inode_size < sizeof(struct squashfs_ldir_inode_header))
			/* corrupted filesystem */
			goto corrupted;
		SQUASHFS_SWAP_LDIR_INODE_HEADER(inode_table + bytes,
			&dir_inode->ldir);
		directory_start_block = dir_inode->ldir.start_block;
	} else
		/* bad type, corrupted filesystem */
		goto corrupted;

	get_uid(id_table[dir_inode->base.uid]);
	get_guid(id_table[dir_inode->base.guid]);

	/* allocate fragment to file mapping table */
	file_mapping = calloc(sBlk->fragments, sizeof(struct append_file *));
	if(file_mapping == NULL)
		MEM_ERROR();

	for(cur_ptr = inode_table; cur_ptr < inode_table + bytes; files ++) {
		if(NO_INODE_BYTES(squashfs_base_inode_header))
			/* corrupted filesystem */
			goto corrupted;

		SQUASHFS_SWAP_BASE_INODE_HEADER(cur_ptr, &base);

		TRACE("scan_inode_table: processing inode @ byte position "
			"0x%x, type 0x%x\n",
			(unsigned int) (cur_ptr - inode_table),
			base.inode_type);

		get_uid(id_table[base.uid]);
		get_guid(id_table[base.guid]);

		switch(base.inode_type) {
		case SQUASHFS_FILE_TYPE: {
			struct squashfs_reg_inode_header inode;
			int frag_bytes, blocks, i;
			long long start, file_bytes = 0;
			unsigned int *block_list;

			if(NO_INODE_BYTES(squashfs_reg_inode_header))
				/* corrupted filesystem */
				goto corrupted;

			SQUASHFS_SWAP_REG_INODE_HEADER(cur_ptr, &inode);

			frag_bytes = inode.fragment == SQUASHFS_INVALID_FRAG ?
				0 : inode.file_size % sBlk->block_size;
			blocks = inode.fragment == SQUASHFS_INVALID_FRAG ?
				(inode.file_size + sBlk->block_size - 1) >>
				sBlk->block_log : inode.file_size >>
				sBlk->block_log;
			start = inode.start_block;

			TRACE("scan_inode_table: regular file, file_size %d, "
				"blocks %d\n", inode.file_size, blocks);

			if(NO_BYTES(blocks * sizeof(unsigned int)))
				/* corrupted filesystem */
				goto corrupted;

			block_list = malloc(blocks * sizeof(unsigned int));
			if(block_list == NULL)
				MEM_ERROR();

			cur_ptr += sizeof(inode);
			SQUASHFS_SWAP_INTS(cur_ptr, block_list, blocks);

			*uncompressed_file += inode.file_size;
			(*file_count) ++;

			for(i = 0; i < blocks; i++)
				file_bytes +=
					SQUASHFS_COMPRESSED_SIZE_BLOCK
								(block_list[i]);

			if(inode.fragment != SQUASHFS_INVALID_FRAG &&
					inode.fragment >= sBlk->fragments) {
				free(block_list);
				goto corrupted;
			}

			add_file(start, inode.file_size, file_bytes,
				block_list, blocks, inode.fragment,
				inode.offset, frag_bytes);
				
			cur_ptr += blocks * sizeof(unsigned int);
			break;
		}	
		case SQUASHFS_LREG_TYPE: {
			struct squashfs_lreg_inode_header inode;
			int frag_bytes, blocks, i;
			long long start, file_bytes = 0;
			unsigned int *block_list;

			if(NO_INODE_BYTES(squashfs_lreg_inode_header))
				/* corrupted filesystem */
				goto corrupted;

			SQUASHFS_SWAP_LREG_INODE_HEADER(cur_ptr, &inode);

			frag_bytes = inode.fragment == SQUASHFS_INVALID_FRAG ?
				0 : inode.file_size % sBlk->block_size;
			blocks = inode.fragment == SQUASHFS_INVALID_FRAG ?
				(inode.file_size + sBlk->block_size - 1) >>
				sBlk->block_log : inode.file_size >>
				sBlk->block_log;
			start = inode.start_block;

			TRACE("scan_inode_table: extended regular "
				"file, file_size %lld, blocks %d\n",
				inode.file_size, blocks);

			if(NO_BYTES(blocks * sizeof(unsigned int)))
				/* corrupted filesystem */
				goto corrupted;

			block_list = malloc(blocks * sizeof(unsigned int));
			if(block_list == NULL)
				MEM_ERROR();

			cur_ptr += sizeof(inode);
			SQUASHFS_SWAP_INTS(cur_ptr, block_list, blocks);

			*uncompressed_file += inode.file_size;
			(*file_count) ++;

			for(i = 0; i < blocks; i++)
				file_bytes +=
					SQUASHFS_COMPRESSED_SIZE_BLOCK
								(block_list[i]);

			if(inode.fragment != SQUASHFS_INVALID_FRAG &&
					inode.fragment >= sBlk->fragments) {
				free(block_list);
				goto corrupted;
			}

			add_file(start, inode.file_size, file_bytes,
				block_list, blocks, inode.fragment,
				inode.offset, frag_bytes);

			cur_ptr += blocks * sizeof(unsigned int);
			break;
		}	
		case SQUASHFS_SYMLINK_TYPE:
		case SQUASHFS_LSYMLINK_TYPE: {
			struct squashfs_symlink_inode_header inode;

			if(NO_INODE_BYTES(squashfs_symlink_inode_header))
				/* corrupted filesystem */
				goto corrupted;

			SQUASHFS_SWAP_SYMLINK_INODE_HEADER(cur_ptr, &inode);

			(*sym_count) ++;

			if (inode.inode_type == SQUASHFS_LSYMLINK_TYPE) {
				if(NO_BYTES(inode.symlink_size +
							sizeof(unsigned int)))
					/* corrupted filesystem */
					goto corrupted;
				cur_ptr += sizeof(inode) + inode.symlink_size +
							sizeof(unsigned int);
			} else {
				if(NO_BYTES(inode.symlink_size))
					/* corrupted filesystem */
					goto corrupted;
				cur_ptr += sizeof(inode) + inode.symlink_size;
			}
			break;
		}
		case SQUASHFS_DIR_TYPE: {
			struct squashfs_dir_inode_header dir_inode;

			if(NO_INODE_BYTES(squashfs_dir_inode_header))
				/* corrupted filesystem */
				goto corrupted;
				
			SQUASHFS_SWAP_DIR_INODE_HEADER(cur_ptr, &dir_inode);

			if(dir_inode.start_block < directory_start_block)
				*uncompressed_directory += dir_inode.file_size;

			(*dir_count) ++;
			cur_ptr += sizeof(struct squashfs_dir_inode_header);
			break;
		}
		case SQUASHFS_LDIR_TYPE: {
			struct squashfs_ldir_inode_header dir_inode;
			int i;

			if(NO_INODE_BYTES(squashfs_ldir_inode_header))
				/* corrupted filesystem */
				goto corrupted;

			SQUASHFS_SWAP_LDIR_INODE_HEADER(cur_ptr, &dir_inode);

			if(dir_inode.start_block < directory_start_block)
				*uncompressed_directory += dir_inode.file_size;

			(*dir_count) ++;
			cur_ptr += sizeof(struct squashfs_ldir_inode_header);

			for(i = 0; i < dir_inode.i_count; i++) {
				struct squashfs_dir_index index;

				if(NO_BYTES(sizeof(index)))
					/* corrupted filesystem */
					goto corrupted;
			
				SQUASHFS_SWAP_DIR_INDEX(cur_ptr, &index);

				if(NO_BYTES(index.size + 1))
					/* corrupted filesystem */
					goto corrupted;

				cur_ptr += sizeof(index) + index.size + 1;
			}
			break;
		}
	 	case SQUASHFS_BLKDEV_TYPE:
	 	case SQUASHFS_CHRDEV_TYPE:
			if(NO_INODE_BYTES(squashfs_dev_inode_header))
				/* corrupted filesystem */
				goto corrupted;

			(*dev_count) ++;
			cur_ptr += sizeof(struct squashfs_dev_inode_header);
			break;
	 	case SQUASHFS_LBLKDEV_TYPE:
	 	case SQUASHFS_LCHRDEV_TYPE:
			if(NO_INODE_BYTES(squashfs_ldev_inode_header))
				/* corrupted filesystem */
				goto corrupted;

			(*dev_count) ++;
			cur_ptr += sizeof(struct squashfs_ldev_inode_header);
			break;
		case SQUASHFS_FIFO_TYPE:
			if(NO_INODE_BYTES(squashfs_ipc_inode_header))
				/* corrupted filesystem */
				goto corrupted;

			(*fifo_count) ++;
			cur_ptr += sizeof(struct squashfs_ipc_inode_header);
			break;
		case SQUASHFS_LFIFO_TYPE:
			if(NO_INODE_BYTES(squashfs_lipc_inode_header))
				/* corrupted filesystem */
				goto corrupted;

			(*fifo_count) ++;
			cur_ptr += sizeof(struct squashfs_lipc_inode_header);
			break;
		case SQUASHFS_SOCKET_TYPE:
			if(NO_INODE_BYTES(squashfs_ipc_inode_header))
				/* corrupted filesystem */
				goto corrupted;

			(*sock_count) ++;
			cur_ptr += sizeof(struct squashfs_ipc_inode_header);
			break;
		case SQUASHFS_LSOCKET_TYPE:
			if(NO_INODE_BYTES(squashfs_lipc_inode_header))
				/* corrupted filesystem */
				goto corrupted;

			(*sock_count) ++;
			cur_ptr += sizeof(struct squashfs_lipc_inode_header);
			break;
	 	default:
			ERROR("Unknown inode type %d in scan_inode_table!\n",
					base.inode_type);
			goto corrupted;
		}
	}
	
	printf("Read existing filesystem, %d inodes scanned\n", files);
	return inode_table;

corrupted:
	ERROR("scan_inode_table: filesystem corruption detected in "
		"scanning metadata\n");
	free(inode_table);
	return NULL;
}


struct compressor *read_super(int fd, struct squashfs_super_block *sBlk, char *source)
{
	int res, bytes = 0;
	char buffer[SQUASHFS_METADATA_SIZE] __attribute__ ((aligned));

	res = read_fs_bytes(fd, SQUASHFS_START, sizeof(struct squashfs_super_block),
		sBlk);
	if(res == 0) {
		ERROR("Can't find a SQUASHFS superblock on %s\n",
				source);
		ERROR("Wrong filesystem or filesystem is corrupted!\n");
		goto failed_mount;
	}

	SQUASHFS_INSWAP_SUPER_BLOCK(sBlk);

	if(sBlk->s_magic != SQUASHFS_MAGIC) {
		if(sBlk->s_magic == SQUASHFS_MAGIC_SWAP)
			ERROR("Pre 4.0 big-endian filesystem on %s, appending"
				" to this is unsupported\n", source);
		else {
			ERROR("Can't find a SQUASHFS superblock on %s\n",
				source);
			ERROR("Wrong filesystem or filesystem is corrupted!\n");
		}
		goto failed_mount;
	}

	/* Check the MAJOR & MINOR versions */
	if(sBlk->s_major != SQUASHFS_MAJOR || sBlk->s_minor > SQUASHFS_MINOR) {
		if(sBlk->s_major < 4)
			ERROR("Filesystem on %s is a SQUASHFS %d.%d filesystem."
				"  Appending\nto SQUASHFS %d.%d filesystems is "
				"not supported.  Please convert it to a "
				"SQUASHFS 4 filesystem\n", source,
				sBlk->s_major,
				sBlk->s_minor, sBlk->s_major, sBlk->s_minor);
		else
			ERROR("Filesystem on %s is %d.%d, which is a later "
				"filesystem version than I support\n",
				source, sBlk->s_major, sBlk->s_minor);
		goto failed_mount;
	}

	/* Check the compression type */
	comp = lookup_compressor_id(sBlk->compression);
	if(!comp->supported) {
		ERROR("Filesystem on %s uses %s compression, this is "
			"unsupported by this version\n", source, comp->name);
		ERROR("Compressors available:\n");
		display_compressors("", "");
		goto failed_mount;
	}

	/*
	 * Read extended superblock information from disk.
	 *
	 * Read compressor specific options from disk if present, and pass
	 * to compressor to set compressor options.
	 *
	 * Note, if there's no compressor options present, the compressor
	 * is still called to set the default options (the defaults may have
	 * been changed by the user specifying options on the command
	 * line which need to be over-ridden).
	 *
	 * Compressor_extract_options is also used to ensure that 
	 * we know how decompress a filesystem compressed with these
	 * compression options.
	 */
	if(SQUASHFS_COMP_OPTS(sBlk->flags)) {
		bytes = read_block(fd, sizeof(*sBlk), NULL, 0, buffer);

		if(bytes == 0) {
			ERROR("Failed to read compressor options from append "
				"filesystem\n");
			ERROR("Filesystem corrupted?\n");
			goto failed_mount;
		}
	}

	res = compressor_extract_options(comp, sBlk->block_size, buffer, bytes);
	if(res == -1) {
		ERROR("Compressor failed to set compressor options\n");
		goto failed_mount;
	}

	printf("Found a valid %sSQUASHFS superblock on %s.\n",
		SQUASHFS_EXPORTABLE(sBlk->flags) ? "exportable " : "", source);
	printf("\tCompression used %s\n", comp->name);
	printf("\tInodes are %scompressed\n",
		SQUASHFS_UNCOMPRESSED_INODES(sBlk->flags) ? "un" : "");
	printf("\tData is %scompressed\n",
		SQUASHFS_UNCOMPRESSED_DATA(sBlk->flags) ? "un" : "");
	printf("\tFragments are %scompressed\n",
		SQUASHFS_UNCOMPRESSED_FRAGMENTS(sBlk->flags) ? "un" : "");
	printf("\tXattrs are %scompressed\n",
		SQUASHFS_UNCOMPRESSED_XATTRS(sBlk->flags) ? "un" : "");
	printf("\tFragments are %spresent in the filesystem\n",
		SQUASHFS_NO_FRAGMENTS(sBlk->flags) ? "not " : "");
	printf("\tAlways-use-fragments option is %sspecified\n",
		SQUASHFS_ALWAYS_FRAGMENTS(sBlk->flags) ? "" : "not ");
	printf("\tDuplicates are %sremoved\n",
		SQUASHFS_DUPLICATES(sBlk->flags) ? "" : "not ");
	printf("\tXattrs are %sstored\n",
		SQUASHFS_NO_XATTRS(sBlk->flags) ? "not " : "");
	printf("\tFilesystem size %.2f Kbytes (%.2f Mbytes)\n",
		sBlk->bytes_used / 1024.0, sBlk->bytes_used
		/ (1024.0 * 1024.0));
	printf("\tBlock size %d\n", sBlk->block_size);
	printf("\tNumber of fragments %d\n", sBlk->fragments);
	printf("\tNumber of inodes %d\n", sBlk->inodes);
	printf("\tNumber of ids %d\n", sBlk->no_ids);
	TRACE("sBlk->inode_table_start %llx\n", sBlk->inode_table_start);
	TRACE("sBlk->directory_table_start %llx\n",
		sBlk->directory_table_start);
	TRACE("sBlk->id_table_start %llx\n", sBlk->id_table_start);
	TRACE("sBlk->fragment_table_start %llx\n", sBlk->fragment_table_start);
	TRACE("sBlk->lookup_table_start %llx\n", sBlk->lookup_table_start);
	TRACE("sBlk->xattr_id_table_start %llx\n", sBlk->xattr_id_table_start);
	printf("\n");

	return comp;

failed_mount:
	return NULL;
}


unsigned char *squashfs_readdir(int fd, int root_entries,
	unsigned int directory_start_block, int offset, int size,
	unsigned int *last_directory_block, struct squashfs_super_block *sBlk,
	void (push_directory_entry)(char *, squashfs_inode, int, int))
{
	struct squashfs_dir_header dirh;
	char buffer[sizeof(struct squashfs_dir_entry) + SQUASHFS_NAME_LEN + 1]
		__attribute__ ((aligned));
	struct squashfs_dir_entry *dire = (struct squashfs_dir_entry *) buffer;
	unsigned char *directory_table = NULL;
	int byte, bytes = 0, dir_count;
	long long start = sBlk->directory_table_start + directory_start_block,
		last_start_block = start; 

	size += offset;
	directory_table = malloc((size + SQUASHFS_METADATA_SIZE * 2 - 1) &
		~(SQUASHFS_METADATA_SIZE - 1));
	if(directory_table == NULL)
		MEM_ERROR();

	while(bytes < size) {
		int expected = (size - bytes) >= SQUASHFS_METADATA_SIZE ?
			SQUASHFS_METADATA_SIZE : 0;

		TRACE("squashfs_readdir: reading block 0x%llx, bytes read so "
			"far %d\n", start, bytes);

		last_start_block = start;
		byte = read_block(fd, start, &start, expected, directory_table + bytes);
		if(byte == 0) {
			ERROR("Failed to read directory\n");
			ERROR("Filesystem corrupted?\n");
			free(directory_table);
			return NULL;
		}
		bytes += byte;
	}

	if(!root_entries)
		goto all_done;

	bytes = offset;
 	while(bytes < size) {			
		SQUASHFS_SWAP_DIR_HEADER(directory_table + bytes, &dirh);

		dir_count = dirh.count + 1;

		/* dir_count should never be larger than SQUASHFS_DIR_COUNT */
		if(dir_count > SQUASHFS_DIR_COUNT) {
			ERROR("File system corrupted: too many entries in directory\n");
			free(directory_table);
			return NULL;
		}

		TRACE("squashfs_readdir: Read directory header @ byte position "
			"0x%x, 0x%x directory entries\n", bytes, dir_count);
		bytes += sizeof(dirh);

		while(dir_count--) {
			SQUASHFS_SWAP_DIR_ENTRY(directory_table + bytes, dire);
			bytes += sizeof(*dire);

			/* size should never be SQUASHFS_NAME_LEN or larger */
			if(dire->size >= SQUASHFS_NAME_LEN) {
				ERROR("File system corrupted: filename too long\n");
				free(directory_table);
				return NULL;
			}

			memcpy(dire->name, directory_table + bytes,
				dire->size + 1);
			dire->name[dire->size + 1] = '\0';
			TRACE("squashfs_readdir: pushing directory entry %s, "
				"inode %x:%x, type 0x%x\n", dire->name,
				dirh.start_block, dire->offset, dire->type);
			push_directory_entry(dire->name,
				SQUASHFS_MKINODE(dirh.start_block,
				dire->offset), dirh.inode_number +
				dire->inode_number, dire->type);
			bytes += dire->size + 1;
		}
	}

all_done:
	*last_directory_block = (unsigned int) last_start_block -
		sBlk->directory_table_start;
	return directory_table;
}


unsigned int *read_id_table(int fd, struct squashfs_super_block *sBlk)
{
	int indexes = SQUASHFS_ID_BLOCKS(sBlk->no_ids);
	long long index[indexes];
	int bytes = SQUASHFS_ID_BYTES(sBlk->no_ids);
	unsigned int *id_table;
	int res, i;

	id_table = malloc(bytes);
	if(id_table == NULL)
		MEM_ERROR();

	res = read_fs_bytes(fd, sBlk->id_table_start,
		SQUASHFS_ID_BLOCK_BYTES(sBlk->no_ids), index);
	if(res == 0) {
		ERROR("Failed to read id table index\n");
		ERROR("Filesystem corrupted?\n");
		free(id_table);
		return NULL;
	}

	SQUASHFS_INSWAP_ID_BLOCKS(index, indexes);

	for(i = 0; i < indexes; i++) {
		int expected = (i + 1) != indexes ? SQUASHFS_METADATA_SIZE :
					bytes & (SQUASHFS_METADATA_SIZE - 1);
		int length = read_block(fd, index[i], NULL, expected,
			((unsigned char *) id_table) +
			(i * SQUASHFS_METADATA_SIZE));
		TRACE("Read id table block %d, from 0x%llx, length %d\n", i,
			index[i], length);
		if(length == 0) {
			ERROR("Failed to read id table block %d, from 0x%llx, "
				"length %d\n", i, index[i], length);
			ERROR("Filesystem corrupted?\n");
			free(id_table);
			return NULL;
		}
	}

	SQUASHFS_INSWAP_INTS(id_table, sBlk->no_ids);

	for(i = 0; i < sBlk->no_ids; i++) {
		TRACE("Adding id %d to id tables\n", id_table[i]);
		create_id(id_table[i]);
	}

	return id_table;
}


struct squashfs_fragment_entry *read_fragment_table(int fd, struct squashfs_super_block *sBlk)
{
	int res, i;
	int bytes = SQUASHFS_FRAGMENT_BYTES(sBlk->fragments);
	int indexes = SQUASHFS_FRAGMENT_INDEXES(sBlk->fragments);
	long long fragment_table_index[indexes];
	struct squashfs_fragment_entry *fragment_table;

	TRACE("read_fragment_table: %d fragments, reading %d fragment indexes "
		"from 0x%llx\n", sBlk->fragments, indexes,
		sBlk->fragment_table_start);

	fragment_table = malloc(bytes);
	if(fragment_table == NULL)
		MEM_ERROR();

	res = read_fs_bytes(fd, sBlk->fragment_table_start,
		SQUASHFS_FRAGMENT_INDEX_BYTES(sBlk->fragments),
		fragment_table_index);
	if(res == 0) {
		ERROR("Failed to read fragment table index\n");
		ERROR("Filesystem corrupted?\n");
		free(fragment_table);
		return NULL;
	}

	SQUASHFS_INSWAP_FRAGMENT_INDEXES(fragment_table_index, indexes);

	for(i = 0; i < indexes; i++) {
		int expected = (i + 1) != indexes ? SQUASHFS_METADATA_SIZE :
					bytes & (SQUASHFS_METADATA_SIZE - 1);
		int length = read_block(fd, fragment_table_index[i], NULL,
			expected, ((unsigned char *) fragment_table) +
			(i * SQUASHFS_METADATA_SIZE));
		TRACE("Read fragment table block %d, from 0x%llx, length %d\n",
			i, fragment_table_index[i], length);
		if(length == 0) {
			ERROR("Failed to read fragment table block %d, from "
				"0x%llx, length %d\n", i,
				fragment_table_index[i], length);
			ERROR("Filesystem corrupted?\n");
			free(fragment_table);
			return NULL;
		}
	}

	for(i = 0; i < sBlk->fragments; i++)
		SQUASHFS_INSWAP_FRAGMENT_ENTRY(&fragment_table[i]);

	return fragment_table;
}


squashfs_inode *read_inode_lookup_table(int fd, struct squashfs_super_block *sBlk)
{
	int lookup_bytes = SQUASHFS_LOOKUP_BYTES(sBlk->inodes);
	int indexes = SQUASHFS_LOOKUP_BLOCKS(sBlk->inodes);
	long long index[indexes];
	int res, i;
	squashfs_inode *inode_lookup_table;

	inode_lookup_table = malloc(lookup_bytes);
	if(inode_lookup_table == NULL)
		MEM_ERROR();

	res = read_fs_bytes(fd, sBlk->lookup_table_start,
		SQUASHFS_LOOKUP_BLOCK_BYTES(sBlk->inodes), index);
	if(res == 0) {
		ERROR("Failed to read inode lookup table index\n");
		ERROR("Filesystem corrupted?\n");
		free(inode_lookup_table);
		return NULL;
	}

	SQUASHFS_INSWAP_LONG_LONGS(index, indexes);

	for(i = 0; i <  indexes; i++) {
		int expected = (i + 1) != indexes ? SQUASHFS_METADATA_SIZE :
				lookup_bytes & (SQUASHFS_METADATA_SIZE - 1);
		int length = read_block(fd, index[i], NULL, expected,
			((unsigned char *) inode_lookup_table) +
			(i * SQUASHFS_METADATA_SIZE));
		TRACE("Read inode lookup table block %d, from 0x%llx, length "
			"%d\n", i, index[i], length);
		if(length == 0) {
			ERROR("Failed to read inode lookup table block %d, "
				"from 0x%llx, length %d\n", i, index[i],
				length);
			ERROR("Filesystem corrupted?\n");
			free(inode_lookup_table);
			return NULL;
		}
	}

	SQUASHFS_INSWAP_LONG_LONGS(inode_lookup_table, sBlk->inodes);

	return inode_lookup_table;
}


long long read_filesystem(char *root_name, int fd, struct squashfs_super_block *sBlk,
	char **cinode_table, char **data_cache, char **cdirectory_table,
	char **directory_data_cache, unsigned int *last_directory_block,
	unsigned int *inode_dir_offset, unsigned int *inode_dir_file_size,
	unsigned int *root_inode_size, unsigned int *inode_dir_start_block,
	int *file_count, int *sym_count, int *dev_count, int *dir_count,
	int *fifo_count, int *sock_count, long long *uncompressed_file,
	unsigned int *uncompressed_inode, unsigned int *uncompressed_directory,
	unsigned int *inode_dir_inode_number,
	unsigned int *inode_dir_parent_inode,
	void (push_directory_entry)(char *, squashfs_inode, int, int),
	struct squashfs_fragment_entry **fragment_table,
	squashfs_inode **inode_lookup_table)
{
	unsigned char *inode_table = NULL, *directory_table = NULL;
	long long start = sBlk->inode_table_start;
	long long end = sBlk->directory_table_start;
	long long root_inode_start = start +
		SQUASHFS_INODE_BLK(sBlk->root_inode);
	unsigned int root_inode_offset =
		SQUASHFS_INODE_OFFSET(sBlk->root_inode);
	unsigned int root_inode_block;
	union squashfs_inode_header inode;
	unsigned int *id_table = NULL;
	int res;

	printf("Scanning existing filesystem...\n");

	if(get_xattrs(fd, sBlk) == 0)
		goto error;

	if(sBlk->fragments > 0) {
		*fragment_table = read_fragment_table(fd, sBlk);
		if(*fragment_table == NULL)
			goto error;
	}

	if(sBlk->lookup_table_start != SQUASHFS_INVALID_BLK) {
		*inode_lookup_table = read_inode_lookup_table(fd, sBlk);
		if(*inode_lookup_table == NULL)
			goto error;
	}

	id_table = read_id_table(fd, sBlk);
	if(id_table == NULL)
		goto error;

	inode_table = scan_inode_table(fd, start, end, root_inode_start,
		root_inode_offset, sBlk, &inode, &root_inode_block,
		root_inode_size, uncompressed_file, uncompressed_directory,
		file_count, sym_count, dev_count, dir_count, fifo_count,
		sock_count, id_table);
	if(inode_table == NULL)
		goto error;

	*uncompressed_inode = root_inode_block;

	if(inode.base.inode_type == SQUASHFS_DIR_TYPE ||
			inode.base.inode_type == SQUASHFS_LDIR_TYPE) {
		if(inode.base.inode_type == SQUASHFS_DIR_TYPE) {
			*inode_dir_start_block = inode.dir.start_block;
			*inode_dir_offset = inode.dir.offset;
			*inode_dir_file_size = inode.dir.file_size - 3;
			*inode_dir_inode_number = inode.dir.inode_number;
			*inode_dir_parent_inode = inode.dir.parent_inode;
		} else {
			*inode_dir_start_block = inode.ldir.start_block;
			*inode_dir_offset = inode.ldir.offset;
			*inode_dir_file_size = inode.ldir.file_size - 3;
			*inode_dir_inode_number = inode.ldir.inode_number;
			*inode_dir_parent_inode = inode.ldir.parent_inode;
		}

		directory_table = squashfs_readdir(fd, !root_name,
			*inode_dir_start_block, *inode_dir_offset,
			*inode_dir_file_size, last_directory_block, sBlk,
			push_directory_entry);
		if(directory_table == NULL) 
			goto error;

		root_inode_start -= start;
		*cinode_table = malloc(root_inode_start);
		if(*cinode_table == NULL)
			MEM_ERROR();

	       	res = read_fs_bytes(fd, start, root_inode_start, *cinode_table);
		if(res == 0) {
			ERROR("Failed to read inode table\n");
			ERROR("Filesystem corrupted?\n");
			goto error;
		}

		*cdirectory_table = malloc(*last_directory_block);
		if(*cdirectory_table == NULL)
			MEM_ERROR();

		res = read_fs_bytes(fd, sBlk->directory_table_start,
			*last_directory_block, *cdirectory_table);
		if(res == 0) {
			ERROR("Failed to read directory table\n");
			ERROR("Filesystem corrupted?\n");
			goto error;
		}

		*data_cache = malloc(root_inode_offset + *root_inode_size);
		if(*data_cache == NULL)
			MEM_ERROR();

		memcpy(*data_cache, inode_table + root_inode_block,
			root_inode_offset + *root_inode_size);

		*directory_data_cache = malloc(*inode_dir_offset +
			*inode_dir_file_size);
		if(*directory_data_cache == NULL)
			MEM_ERROR();

		memcpy(*directory_data_cache, directory_table,
			*inode_dir_offset + *inode_dir_file_size);

		free(id_table);
		free(inode_table);
		free(directory_table);
		return sBlk->inode_table_start;
	}

error:
	free(id_table);
	free(inode_table);
	free(directory_table);
	return 0;
}
