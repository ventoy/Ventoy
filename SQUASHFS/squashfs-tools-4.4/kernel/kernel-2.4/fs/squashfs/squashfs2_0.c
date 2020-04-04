/*
 * Squashfs - a compressed read only filesystem for Linux
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006
 * Phillip Lougher <phillip@lougher.demon.co.uk>
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
 * squashfs2_0.c
 */

#include <linux/types.h>
#include <linux/squashfs_fs.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/zlib.h>
#include <linux/fs.h>
#include <linux/smp_lock.h>
#include <linux/locks.h>
#include <linux/init.h>
#include <linux/dcache.h>
#include <linux/wait.h>
#include <linux/zlib.h>
#include <linux/blkdev.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include "squashfs.h"

static int squashfs_readdir_2(struct file *file, void *dirent, filldir_t filldir);
static struct dentry *squashfs_lookup_2(struct inode *i, struct dentry *dentry);

static struct file_operations squashfs_dir_ops_2 = {
	.read = generic_read_dir,
	.readdir = squashfs_readdir_2
};

static struct inode_operations squashfs_dir_inode_ops_2 = {
	.lookup = squashfs_lookup_2
};

static unsigned char squashfs_filetype_table[] = {
	DT_UNKNOWN, DT_DIR, DT_REG, DT_LNK, DT_BLK, DT_CHR, DT_FIFO, DT_SOCK
};

static int read_fragment_index_table_2(struct super_block *s)
{
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	struct squashfs_super_block *sblk = &msblk->sblk;

	if (!(msblk->fragment_index_2 = kmalloc(SQUASHFS_FRAGMENT_INDEX_BYTES_2
					(sblk->fragments), GFP_KERNEL))) {
		ERROR("Failed to allocate uid/gid table\n");
		return 0;
	}
   
	if (SQUASHFS_FRAGMENT_INDEX_BYTES_2(sblk->fragments) &&
					!squashfs_read_data(s, (char *)
					msblk->fragment_index_2,
					sblk->fragment_table_start,
					SQUASHFS_FRAGMENT_INDEX_BYTES_2
					(sblk->fragments) |
					SQUASHFS_COMPRESSED_BIT_BLOCK, NULL)) {
		ERROR("unable to read fragment index table\n");
		return 0;
	}

	if (msblk->swap) {
		int i;
		unsigned int fragment;

		for (i = 0; i < SQUASHFS_FRAGMENT_INDEXES_2(sblk->fragments);
									i++) {
			SQUASHFS_SWAP_FRAGMENT_INDEXES_2((&fragment),
						&msblk->fragment_index_2[i], 1);
			msblk->fragment_index_2[i] = fragment;
		}
	}

	return 1;
}


static int get_fragment_location_2(struct super_block *s, unsigned int fragment,
				long long *fragment_start_block,
				unsigned int *fragment_size)
{
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	long long start_block =
		msblk->fragment_index_2[SQUASHFS_FRAGMENT_INDEX_2(fragment)];
	int offset = SQUASHFS_FRAGMENT_INDEX_OFFSET_2(fragment);
	struct squashfs_fragment_entry_2 fragment_entry;

	if (msblk->swap) {
		struct squashfs_fragment_entry_2 sfragment_entry;

		if (!squashfs_get_cached_block(s, (char *) &sfragment_entry,
					start_block, offset,
					sizeof(sfragment_entry), &start_block,
					&offset))
			goto out;
		SQUASHFS_SWAP_FRAGMENT_ENTRY_2(&fragment_entry, &sfragment_entry);
	} else
		if (!squashfs_get_cached_block(s, (char *) &fragment_entry,
					start_block, offset,
					sizeof(fragment_entry), &start_block,
					&offset))
			goto out;

	*fragment_start_block = fragment_entry.start_block;
	*fragment_size = fragment_entry.size;

	return 1;

out:
	return 0;
}


static struct inode *squashfs_new_inode(struct super_block *s,
		struct squashfs_base_inode_header_2 *inodeb, unsigned int ino)
{
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	struct squashfs_super_block *sblk = &msblk->sblk;
	struct inode *i = new_inode(s);

	if (i) {
		i->i_ino = ino;
		i->i_mtime = sblk->mkfs_time;
		i->i_atime = sblk->mkfs_time;
		i->i_ctime = sblk->mkfs_time;
		i->i_uid = msblk->uid[inodeb->uid];
		i->i_mode = inodeb->mode;
		i->i_nlink = 1;
		i->i_size = 0;
		if (inodeb->guid == SQUASHFS_GUIDS)
			i->i_gid = i->i_uid;
		else
			i->i_gid = msblk->guid[inodeb->guid];
	}

	return i;
}


static struct inode *squashfs_iget_2(struct super_block *s, squashfs_inode_t inode)
{
	struct inode *i;
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	struct squashfs_super_block *sblk = &msblk->sblk;
	unsigned int block = SQUASHFS_INODE_BLK(inode) +
		sblk->inode_table_start;
	unsigned int offset = SQUASHFS_INODE_OFFSET(inode);
	unsigned int ino = SQUASHFS_MK_VFS_INODE(block
		- sblk->inode_table_start, offset);
	long long next_block;
	unsigned int next_offset;
	union squashfs_inode_header_2 id, sid;
	struct squashfs_base_inode_header_2 *inodeb = &id.base,
					  *sinodeb = &sid.base;

	TRACE("Entered squashfs_iget\n");

	if (msblk->swap) {
		if (!squashfs_get_cached_block(s, (char *) sinodeb, block,
					offset, sizeof(*sinodeb), &next_block,
					&next_offset))
			goto failed_read;
		SQUASHFS_SWAP_BASE_INODE_HEADER_2(inodeb, sinodeb,
					sizeof(*sinodeb));
	} else
		if (!squashfs_get_cached_block(s, (char *) inodeb, block,
					offset, sizeof(*inodeb), &next_block,
					&next_offset))
			goto failed_read;

	switch(inodeb->inode_type) {
		case SQUASHFS_FILE_TYPE: {
			struct squashfs_reg_inode_header_2 *inodep = &id.reg;
			struct squashfs_reg_inode_header_2 *sinodep = &sid.reg;
			long long frag_blk;
			unsigned int frag_size;
				
			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, (char *)
						sinodep, block, offset,
						sizeof(*sinodep), &next_block,
						&next_offset))
					goto failed_read;
				SQUASHFS_SWAP_REG_INODE_HEADER_2(inodep, sinodep);
			} else
				if (!squashfs_get_cached_block(s, (char *)
						inodep, block, offset,
						sizeof(*inodep), &next_block,
						&next_offset))
					goto failed_read;

			frag_blk = SQUASHFS_INVALID_BLK;
			if (inodep->fragment != SQUASHFS_INVALID_FRAG &&
					!get_fragment_location_2(s,
					inodep->fragment, &frag_blk, &frag_size))
				goto failed_read;
				
			if((i = squashfs_new_inode(s, inodeb, ino)) == NULL)
				goto failed_read1;

			i->i_size = inodep->file_size;
			i->i_fop = &generic_ro_fops;
			i->i_mode |= S_IFREG;
			i->i_mtime = inodep->mtime;
			i->i_atime = inodep->mtime;
			i->i_ctime = inodep->mtime;
			i->i_blocks = ((i->i_size - 1) >> 9) + 1;
			i->i_blksize = PAGE_CACHE_SIZE;
			SQUASHFS_I(i)->u.s1.fragment_start_block = frag_blk;
			SQUASHFS_I(i)->u.s1.fragment_size = frag_size;
			SQUASHFS_I(i)->u.s1.fragment_offset = inodep->offset;
			SQUASHFS_I(i)->start_block = inodep->start_block;
			SQUASHFS_I(i)->u.s1.block_list_start = next_block;
			SQUASHFS_I(i)->offset = next_offset;
			if (sblk->block_size > 4096)
				i->i_data.a_ops = &squashfs_aops;
			else
				i->i_data.a_ops = &squashfs_aops_4K;

			TRACE("File inode %x:%x, start_block %x, "
					"block_list_start %llx, offset %x\n",
					SQUASHFS_INODE_BLK(inode), offset,
					inodep->start_block, next_block,
					next_offset);
			break;
		}
		case SQUASHFS_DIR_TYPE: {
			struct squashfs_dir_inode_header_2 *inodep = &id.dir;
			struct squashfs_dir_inode_header_2 *sinodep = &sid.dir;

			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, (char *)
						sinodep, block, offset,
						sizeof(*sinodep), &next_block,
						&next_offset))
					goto failed_read;
				SQUASHFS_SWAP_DIR_INODE_HEADER_2(inodep, sinodep);
			} else
				if (!squashfs_get_cached_block(s, (char *)
						inodep, block, offset,
						sizeof(*inodep), &next_block,
						&next_offset))
					goto failed_read;

			if((i = squashfs_new_inode(s, inodeb, ino)) == NULL)
				goto failed_read1;

			i->i_size = inodep->file_size;
			i->i_op = &squashfs_dir_inode_ops_2;
			i->i_fop = &squashfs_dir_ops_2;
			i->i_mode |= S_IFDIR;
			i->i_mtime = inodep->mtime;
			i->i_atime = inodep->mtime;
			i->i_ctime = inodep->mtime;
			SQUASHFS_I(i)->start_block = inodep->start_block;
			SQUASHFS_I(i)->offset = inodep->offset;
			SQUASHFS_I(i)->u.s2.directory_index_count = 0;
			SQUASHFS_I(i)->u.s2.parent_inode = 0;

			TRACE("Directory inode %x:%x, start_block %x, offset "
					"%x\n", SQUASHFS_INODE_BLK(inode),
					offset, inodep->start_block,
					inodep->offset);
			break;
		}
		case SQUASHFS_LDIR_TYPE: {
			struct squashfs_ldir_inode_header_2 *inodep = &id.ldir;
			struct squashfs_ldir_inode_header_2 *sinodep = &sid.ldir;

			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, (char *)
						sinodep, block, offset,
						sizeof(*sinodep), &next_block,
						&next_offset))
					goto failed_read;
				SQUASHFS_SWAP_LDIR_INODE_HEADER_2(inodep,
						sinodep);
			} else
				if (!squashfs_get_cached_block(s, (char *)
						inodep, block, offset,
						sizeof(*inodep), &next_block,
						&next_offset))
					goto failed_read;

			if((i = squashfs_new_inode(s, inodeb, ino)) == NULL)
				goto failed_read1;

			i->i_size = inodep->file_size;
			i->i_op = &squashfs_dir_inode_ops_2;
			i->i_fop = &squashfs_dir_ops_2;
			i->i_mode |= S_IFDIR;
			i->i_mtime = inodep->mtime;
			i->i_atime = inodep->mtime;
			i->i_ctime = inodep->mtime;
			SQUASHFS_I(i)->start_block = inodep->start_block;
			SQUASHFS_I(i)->offset = inodep->offset;
			SQUASHFS_I(i)->u.s2.directory_index_start = next_block;
			SQUASHFS_I(i)->u.s2.directory_index_offset =
								next_offset;
			SQUASHFS_I(i)->u.s2.directory_index_count =
								inodep->i_count;
			SQUASHFS_I(i)->u.s2.parent_inode = 0;

			TRACE("Long directory inode %x:%x, start_block %x, "
					"offset %x\n",
					SQUASHFS_INODE_BLK(inode), offset,
					inodep->start_block, inodep->offset);
			break;
		}
		case SQUASHFS_SYMLINK_TYPE: {
			struct squashfs_symlink_inode_header_2 *inodep =
								&id.symlink;
			struct squashfs_symlink_inode_header_2 *sinodep =
								&sid.symlink;
	
			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, (char *)
						sinodep, block, offset,
						sizeof(*sinodep), &next_block,
						&next_offset))
					goto failed_read;
				SQUASHFS_SWAP_SYMLINK_INODE_HEADER_2(inodep,
								sinodep);
			} else
				if (!squashfs_get_cached_block(s, (char *)
						inodep, block, offset,
						sizeof(*inodep), &next_block,
						&next_offset))
					goto failed_read;

			if((i = squashfs_new_inode(s, inodeb, ino)) == NULL)
				goto failed_read1;

			i->i_size = inodep->symlink_size;
			i->i_op = &page_symlink_inode_operations;
			i->i_data.a_ops = &squashfs_symlink_aops;
			i->i_mode |= S_IFLNK;
			SQUASHFS_I(i)->start_block = next_block;
			SQUASHFS_I(i)->offset = next_offset;

			TRACE("Symbolic link inode %x:%x, start_block %llx, "
					"offset %x\n",
					SQUASHFS_INODE_BLK(inode), offset,
					next_block, next_offset);
			break;
		 }
		 case SQUASHFS_BLKDEV_TYPE:
		 case SQUASHFS_CHRDEV_TYPE: {
			struct squashfs_dev_inode_header_2 *inodep = &id.dev;
			struct squashfs_dev_inode_header_2 *sinodep = &sid.dev;

			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, (char *)
						sinodep, block, offset,
						sizeof(*sinodep), &next_block,
						&next_offset))
					goto failed_read;
				SQUASHFS_SWAP_DEV_INODE_HEADER_2(inodep, sinodep);
			} else	
				if (!squashfs_get_cached_block(s, (char *)
						inodep, block, offset,
						sizeof(*inodep), &next_block,
						&next_offset))
					goto failed_read;

			if ((i = squashfs_new_inode(s, inodeb, ino)) == NULL)
				goto failed_read1;

			i->i_mode |= (inodeb->inode_type ==
					SQUASHFS_CHRDEV_TYPE) ?  S_IFCHR :
					S_IFBLK;
			init_special_inode(i, i->i_mode, inodep->rdev);

			TRACE("Device inode %x:%x, rdev %x\n",
					SQUASHFS_INODE_BLK(inode), offset,
					inodep->rdev);
			break;
		 }
		 case SQUASHFS_FIFO_TYPE:
		 case SQUASHFS_SOCKET_TYPE: {
			if ((i = squashfs_new_inode(s, inodeb, ino)) == NULL)
				goto failed_read1;

			i->i_mode |= (inodeb->inode_type == SQUASHFS_FIFO_TYPE)
							? S_IFIFO : S_IFSOCK;
			init_special_inode(i, i->i_mode, 0);
			break;
		 }
		 default:
			ERROR("Unknown inode type %d in squashfs_iget!\n",
					inodeb->inode_type);
			goto failed_read1;
	}
	
	insert_inode_hash(i);
	return i;

failed_read:
	ERROR("Unable to read inode [%x:%x]\n", block, offset);

failed_read1:
	return NULL;
}


static int get_dir_index_using_offset(struct super_block *s, long long 
				*next_block, unsigned int *next_offset,
				long long index_start,
				unsigned int index_offset, int i_count,
				long long f_pos)
{
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	struct squashfs_super_block *sblk = &msblk->sblk;
	int i, length = 0;
	struct squashfs_dir_index_2 index;

	TRACE("Entered get_dir_index_using_offset, i_count %d, f_pos %d\n",
					i_count, (unsigned int) f_pos);

	if (f_pos == 0)
		goto finish;

	for (i = 0; i < i_count; i++) {
		if (msblk->swap) {
			struct squashfs_dir_index_2 sindex;
			squashfs_get_cached_block(s, (char *) &sindex,
					index_start, index_offset,
					sizeof(sindex), &index_start,
					&index_offset);
			SQUASHFS_SWAP_DIR_INDEX_2(&index, &sindex);
		} else
			squashfs_get_cached_block(s, (char *) &index,
					index_start, index_offset,
					sizeof(index), &index_start,
					&index_offset);

		if (index.index > f_pos)
			break;

		squashfs_get_cached_block(s, NULL, index_start, index_offset,
					index.size + 1, &index_start,
					&index_offset);

		length = index.index;
		*next_block = index.start_block + sblk->directory_table_start;
	}

	*next_offset = (length + *next_offset) % SQUASHFS_METADATA_SIZE;

finish:
	return length;
}


static int get_dir_index_using_name(struct super_block *s, long long
				*next_block, unsigned int *next_offset,
				long long index_start,
				unsigned int index_offset, int i_count,
				const char *name, int size)
{
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	struct squashfs_super_block *sblk = &msblk->sblk;
	int i, length = 0;
	char buffer[sizeof(struct squashfs_dir_index_2) + SQUASHFS_NAME_LEN + 1];
	struct squashfs_dir_index_2 *index = (struct squashfs_dir_index_2 *) buffer;
	char str[SQUASHFS_NAME_LEN + 1];

	TRACE("Entered get_dir_index_using_name, i_count %d\n", i_count);

	strncpy(str, name, size);
	str[size] = '\0';

	for (i = 0; i < i_count; i++) {
		if (msblk->swap) {
			struct squashfs_dir_index_2 sindex;
			squashfs_get_cached_block(s, (char *) &sindex,
					index_start, index_offset,
					sizeof(sindex), &index_start,
					&index_offset);
			SQUASHFS_SWAP_DIR_INDEX_2(index, &sindex);
		} else
			squashfs_get_cached_block(s, (char *) index,
					index_start, index_offset,
					sizeof(struct squashfs_dir_index_2),
					&index_start, &index_offset);

		squashfs_get_cached_block(s, index->name, index_start,
					index_offset, index->size + 1,
					&index_start, &index_offset);

		index->name[index->size + 1] = '\0';

		if (strcmp(index->name, str) > 0)
			break;

		length = index->index;
		*next_block = index->start_block + sblk->directory_table_start;
	}

	*next_offset = (length + *next_offset) % SQUASHFS_METADATA_SIZE;
	return length;
}

		
static int squashfs_readdir_2(struct file *file, void *dirent, filldir_t filldir)
{
	struct inode *i = file->f_dentry->d_inode;
	struct squashfs_sb_info *msblk = &i->i_sb->u.squashfs_sb;
	struct squashfs_super_block *sblk = &msblk->sblk;
	long long next_block = SQUASHFS_I(i)->start_block +
		sblk->directory_table_start;
	int next_offset = SQUASHFS_I(i)->offset, length = 0,
		dir_count;
	struct squashfs_dir_header_2 dirh;
	char buffer[sizeof(struct squashfs_dir_entry_2) + SQUASHFS_NAME_LEN + 1];
	struct squashfs_dir_entry_2 *dire = (struct squashfs_dir_entry_2 *) buffer;

	TRACE("Entered squashfs_readdir_2 [%llx:%x]\n", next_block, next_offset);

	length = get_dir_index_using_offset(i->i_sb, &next_block, &next_offset,
				SQUASHFS_I(i)->u.s2.directory_index_start,
				SQUASHFS_I(i)->u.s2.directory_index_offset,
				SQUASHFS_I(i)->u.s2.directory_index_count,
				file->f_pos);

	while (length < i_size_read(i)) {
		/* read directory header */
		if (msblk->swap) {
			struct squashfs_dir_header_2 sdirh;
			
			if (!squashfs_get_cached_block(i->i_sb, (char *) &sdirh,
					next_block, next_offset, sizeof(sdirh),
					&next_block, &next_offset))
				goto failed_read;

			length += sizeof(sdirh);
			SQUASHFS_SWAP_DIR_HEADER_2(&dirh, &sdirh);
		} else {
			if (!squashfs_get_cached_block(i->i_sb, (char *) &dirh,
					next_block, next_offset, sizeof(dirh),
					&next_block, &next_offset))
				goto failed_read;

			length += sizeof(dirh);
		}

		dir_count = dirh.count + 1;
		while (dir_count--) {
			if (msblk->swap) {
				struct squashfs_dir_entry_2 sdire;
				if (!squashfs_get_cached_block(i->i_sb, (char *)
						&sdire, next_block, next_offset,
						sizeof(sdire), &next_block,
						&next_offset))
					goto failed_read;
				
				length += sizeof(sdire);
				SQUASHFS_SWAP_DIR_ENTRY_2(dire, &sdire);
			} else {
				if (!squashfs_get_cached_block(i->i_sb, (char *)
						dire, next_block, next_offset,
						sizeof(*dire), &next_block,
						&next_offset))
					goto failed_read;

				length += sizeof(*dire);
			}

			if (!squashfs_get_cached_block(i->i_sb, dire->name,
						next_block, next_offset,
						dire->size + 1, &next_block,
						&next_offset))
				goto failed_read;

			length += dire->size + 1;

			if (file->f_pos >= length)
				continue;

			dire->name[dire->size + 1] = '\0';

			TRACE("Calling filldir(%x, %s, %d, %d, %x:%x, %d)\n",
					(unsigned int) dirent, dire->name,
					dire->size + 1, (int) file->f_pos,
					dirh.start_block, dire->offset,
					squashfs_filetype_table[dire->type]);

			if (filldir(dirent, dire->name, dire->size + 1,
					file->f_pos, SQUASHFS_MK_VFS_INODE(
					dirh.start_block, dire->offset),
					squashfs_filetype_table[dire->type])
					< 0) {
				TRACE("Filldir returned less than 0\n");
				goto finish;
			}
			file->f_pos = length;
		}
	}

finish:
	return 0;

failed_read:
	ERROR("Unable to read directory block [%llx:%x]\n", next_block,
		next_offset);
	return 0;
}


static struct dentry *squashfs_lookup_2(struct inode *i, struct dentry *dentry)
{
	const unsigned char *name = dentry->d_name.name;
	int len = dentry->d_name.len;
	struct inode *inode = NULL;
	struct squashfs_sb_info *msblk = &i->i_sb->u.squashfs_sb;
	struct squashfs_super_block *sblk = &msblk->sblk;
	long long next_block = SQUASHFS_I(i)->start_block +
				sblk->directory_table_start;
	int next_offset = SQUASHFS_I(i)->offset, length = 0,
				dir_count;
	struct squashfs_dir_header_2 dirh;
	char buffer[sizeof(struct squashfs_dir_entry_2) + SQUASHFS_NAME_LEN];
	struct squashfs_dir_entry_2 *dire = (struct squashfs_dir_entry_2 *) buffer;
	int sorted = sblk->s_major == 2 && sblk->s_minor >= 1;

	TRACE("Entered squashfs_lookup [%llx:%x]\n", next_block, next_offset);

	if (len > SQUASHFS_NAME_LEN)
		goto exit_loop;

	length = get_dir_index_using_name(i->i_sb, &next_block, &next_offset,
				SQUASHFS_I(i)->u.s2.directory_index_start,
				SQUASHFS_I(i)->u.s2.directory_index_offset,
				SQUASHFS_I(i)->u.s2.directory_index_count, name,
				len);

	while (length < i_size_read(i)) {
		/* read directory header */
		if (msblk->swap) {
			struct squashfs_dir_header_2 sdirh;
			if (!squashfs_get_cached_block(i->i_sb, (char *) &sdirh,
					next_block, next_offset, sizeof(sdirh),
					&next_block, &next_offset))
				goto failed_read;

			length += sizeof(sdirh);
			SQUASHFS_SWAP_DIR_HEADER_2(&dirh, &sdirh);
		} else {
			if (!squashfs_get_cached_block(i->i_sb, (char *) &dirh,
					next_block, next_offset, sizeof(dirh),
					&next_block, &next_offset))
				goto failed_read;

			length += sizeof(dirh);
		}

		dir_count = dirh.count + 1;
		while (dir_count--) {
			if (msblk->swap) {
				struct squashfs_dir_entry_2 sdire;
				if (!squashfs_get_cached_block(i->i_sb, (char *)
						&sdire, next_block,next_offset,
						sizeof(sdire), &next_block,
						&next_offset))
					goto failed_read;
				
				length += sizeof(sdire);
				SQUASHFS_SWAP_DIR_ENTRY_2(dire, &sdire);
			} else {
				if (!squashfs_get_cached_block(i->i_sb, (char *)
						dire, next_block,next_offset,
						sizeof(*dire), &next_block,
						&next_offset))
					goto failed_read;

				length += sizeof(*dire);
			}

			if (!squashfs_get_cached_block(i->i_sb, dire->name,
					next_block, next_offset, dire->size + 1,
					&next_block, &next_offset))
				goto failed_read;

			length += dire->size + 1;

			if (sorted && name[0] < dire->name[0])
				goto exit_loop;

			if ((len == dire->size + 1) && !strncmp(name,
						dire->name, len)) {
				squashfs_inode_t ino =
					SQUASHFS_MKINODE(dirh.start_block,
					dire->offset);

				TRACE("calling squashfs_iget for directory "
					"entry %s, inode %x:%x, %d\n", name,
					dirh.start_block, dire->offset, ino);

				inode = (msblk->iget)(i->i_sb, ino);

				goto exit_loop;
			}
		}
	}

exit_loop:
	d_add(dentry, inode);
	return ERR_PTR(0);

failed_read:
	ERROR("Unable to read directory block [%llx:%x]\n", next_block,
		next_offset);
	goto exit_loop;
}


int squashfs_2_0_supported(struct squashfs_sb_info *msblk)
{
	struct squashfs_super_block *sblk = &msblk->sblk;

	msblk->iget = squashfs_iget_2;
	msblk->read_fragment_index_table = read_fragment_index_table_2;

	sblk->bytes_used = sblk->bytes_used_2;
	sblk->uid_start = sblk->uid_start_2;
	sblk->guid_start = sblk->guid_start_2;
	sblk->inode_table_start = sblk->inode_table_start_2;
	sblk->directory_table_start = sblk->directory_table_start_2;
	sblk->fragment_table_start = sblk->fragment_table_start_2;

	return 1;
}
