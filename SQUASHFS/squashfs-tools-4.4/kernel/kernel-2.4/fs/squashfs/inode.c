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
 * inode.c
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
#include <linux/blkdev.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>

#include "squashfs.h"

static struct super_block *squashfs_read_super(struct super_block *, void *, int);
static void squashfs_put_super(struct super_block *);
static int squashfs_statfs(struct super_block *, struct statfs *);
static int squashfs_symlink_readpage(struct file *file, struct page *page);
static int squashfs_readpage(struct file *file, struct page *page);
static int squashfs_readpage4K(struct file *file, struct page *page);
static int squashfs_readdir(struct file *, void *, filldir_t);
static struct dentry *squashfs_lookup(struct inode *, struct dentry *);
static struct inode *squashfs_iget(struct super_block *s, squashfs_inode_t inode);
static long long read_blocklist(struct inode *inode, int index,
				int readahead_blks, char *block_list,
				unsigned short **block_p, unsigned int *bsize);

static DECLARE_FSTYPE_DEV(squashfs_fs_type, "squashfs", squashfs_read_super);

static unsigned char squashfs_filetype_table[] = {
	DT_UNKNOWN, DT_DIR, DT_REG, DT_LNK, DT_BLK, DT_CHR, DT_FIFO, DT_SOCK
};

static struct super_operations squashfs_ops = {
	.statfs = squashfs_statfs,
	.put_super = squashfs_put_super,
};

SQSH_EXTERN struct address_space_operations squashfs_symlink_aops = {
	.readpage = squashfs_symlink_readpage
};

SQSH_EXTERN struct address_space_operations squashfs_aops = {
	.readpage = squashfs_readpage
};

SQSH_EXTERN struct address_space_operations squashfs_aops_4K = {
	.readpage = squashfs_readpage4K
};

static struct file_operations squashfs_dir_ops = {
	.read = generic_read_dir,
	.readdir = squashfs_readdir
};

static struct inode_operations squashfs_dir_inode_ops = {
	.lookup = squashfs_lookup
};

static struct buffer_head *get_block_length(struct super_block *s,
				int *cur_index, int *offset, int *c_byte)
{
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	unsigned short temp;
	struct buffer_head *bh;

	if (!(bh = sb_bread(s, *cur_index)))
		goto out;

	if (msblk->devblksize - *offset == 1) {
		if (msblk->swap)
			((unsigned char *) &temp)[1] = *((unsigned char *)
				(bh->b_data + *offset));
		else
			((unsigned char *) &temp)[0] = *((unsigned char *)
				(bh->b_data + *offset));
		brelse(bh);
		if (!(bh = sb_bread(s, ++(*cur_index))))
			goto out;
		if (msblk->swap)
			((unsigned char *) &temp)[0] = *((unsigned char *)
				bh->b_data); 
		else
			((unsigned char *) &temp)[1] = *((unsigned char *)
				bh->b_data); 
		*c_byte = temp;
		*offset = 1;
	} else {
		if (msblk->swap) {
			((unsigned char *) &temp)[1] = *((unsigned char *)
				(bh->b_data + *offset));
			((unsigned char *) &temp)[0] = *((unsigned char *)
				(bh->b_data + *offset + 1)); 
		} else {
			((unsigned char *) &temp)[0] = *((unsigned char *)
				(bh->b_data + *offset));
			((unsigned char *) &temp)[1] = *((unsigned char *)
				(bh->b_data + *offset + 1)); 
		}
		*c_byte = temp;
		*offset += 2;
	}

	if (SQUASHFS_CHECK_DATA(msblk->sblk.flags)) {
		if (*offset == msblk->devblksize) {
			brelse(bh);
			if (!(bh = sb_bread(s, ++(*cur_index))))
				goto out;
			*offset = 0;
		}
		if (*((unsigned char *) (bh->b_data + *offset)) !=
						SQUASHFS_MARKER_BYTE) {
			ERROR("Metadata block marker corrupt @ %x\n",
						*cur_index);
			brelse(bh);
			goto out;
		}
		(*offset)++;
	}
	return bh;

out:
	return NULL;
}


SQSH_EXTERN unsigned int squashfs_read_data(struct super_block *s, char *buffer,
			long long index, unsigned int length,
			long long *next_index)
{
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	struct buffer_head *bh[((SQUASHFS_FILE_MAX_SIZE - 1) >>
			msblk->devblksize_log2) + 2];
	unsigned int offset = index & ((1 << msblk->devblksize_log2) - 1);
	unsigned int cur_index = index >> msblk->devblksize_log2;
	int bytes, avail_bytes, b = 0, k;
	char *c_buffer;
	unsigned int compressed;
	unsigned int c_byte = length;

	if (c_byte) {
		bytes = msblk->devblksize - offset;
		compressed = SQUASHFS_COMPRESSED_BLOCK(c_byte);
		c_buffer = compressed ? msblk->read_data : buffer;
		c_byte = SQUASHFS_COMPRESSED_SIZE_BLOCK(c_byte);

		TRACE("Block @ 0x%llx, %scompressed size %d\n", index, compressed
					? "" : "un", (unsigned int) c_byte);

		if (!(bh[0] = sb_getblk(s, cur_index)))
			goto block_release;

		for (b = 1; bytes < c_byte; b++) {
			if (!(bh[b] = sb_getblk(s, ++cur_index)))
				goto block_release;
			bytes += msblk->devblksize;
		}
		ll_rw_block(READ, b, bh);
	} else {
		if (!(bh[0] = get_block_length(s, &cur_index, &offset,
								&c_byte)))
			goto read_failure;

		bytes = msblk->devblksize - offset;
		compressed = SQUASHFS_COMPRESSED(c_byte);
		c_buffer = compressed ? msblk->read_data : buffer;
		c_byte = SQUASHFS_COMPRESSED_SIZE(c_byte);

		TRACE("Block @ 0x%llx, %scompressed size %d\n", index, compressed
					? "" : "un", (unsigned int) c_byte);

		for (b = 1; bytes < c_byte; b++) {
			if (!(bh[b] = sb_getblk(s, ++cur_index)))
				goto block_release;
			bytes += msblk->devblksize;
		}
		ll_rw_block(READ, b - 1, bh + 1);
	}

	if (compressed)
		down(&msblk->read_data_mutex);

	for (bytes = 0, k = 0; k < b; k++) {
		avail_bytes = (c_byte - bytes) > (msblk->devblksize - offset) ?
					msblk->devblksize - offset :
					c_byte - bytes;
		wait_on_buffer(bh[k]);
		if (!buffer_uptodate(bh[k]))
			goto block_release;
		memcpy(c_buffer + bytes, bh[k]->b_data + offset, avail_bytes);
		bytes += avail_bytes;
		offset = 0;
		brelse(bh[k]);
	}

	/*
	 * uncompress block
	 */
	if (compressed) {
		int zlib_err;

		msblk->stream.next_in = c_buffer;
		msblk->stream.avail_in = c_byte;
		msblk->stream.next_out = buffer;
		msblk->stream.avail_out = msblk->read_size;

		if (((zlib_err = zlib_inflateInit(&msblk->stream)) != Z_OK) ||
				((zlib_err = zlib_inflate(&msblk->stream, Z_FINISH))
				 != Z_STREAM_END) || ((zlib_err =
				zlib_inflateEnd(&msblk->stream)) != Z_OK)) {
			ERROR("zlib_fs returned unexpected result 0x%x\n",
				zlib_err);
			bytes = 0;
		} else
			bytes = msblk->stream.total_out;
		
		up(&msblk->read_data_mutex);
	}

	if (next_index)
		*next_index = index + c_byte + (length ? 0 :
				(SQUASHFS_CHECK_DATA(msblk->sblk.flags)
				 ? 3 : 2));
	return bytes;

block_release:
	while (--b >= 0)
		brelse(bh[b]);

read_failure:
	ERROR("sb_bread failed reading block 0x%x\n", cur_index);
	return 0;
}


SQSH_EXTERN int squashfs_get_cached_block(struct super_block *s, char *buffer,
				long long block, unsigned int offset,
				int length, long long *next_block,
				unsigned int *next_offset)
{
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	int n, i, bytes, return_length = length;
	long long next_index;

	TRACE("Entered squashfs_get_cached_block [%llx:%x]\n", block, offset);

	while ( 1 ) {
		for (i = 0; i < SQUASHFS_CACHED_BLKS; i++) 
			if (msblk->block_cache[i].block == block)
				break; 
		
		down(&msblk->block_cache_mutex);

		if (i == SQUASHFS_CACHED_BLKS) {
			/* read inode header block */
			for (i = msblk->next_cache, n = SQUASHFS_CACHED_BLKS;
					n ; n --, i = (i + 1) %
					SQUASHFS_CACHED_BLKS)
				if (msblk->block_cache[i].block !=
							SQUASHFS_USED_BLK)
					break;

			if (n == 0) {
				wait_queue_t wait;

				init_waitqueue_entry(&wait, current);
				add_wait_queue(&msblk->waitq, &wait);
				set_current_state(TASK_UNINTERRUPTIBLE);
 				up(&msblk->block_cache_mutex);
				schedule();
				set_current_state(TASK_RUNNING);
				remove_wait_queue(&msblk->waitq, &wait);
				continue;
			}
			msblk->next_cache = (i + 1) % SQUASHFS_CACHED_BLKS;

			if (msblk->block_cache[i].block ==
							SQUASHFS_INVALID_BLK) {
				if (!(msblk->block_cache[i].data =
						kmalloc(SQUASHFS_METADATA_SIZE,
						GFP_KERNEL))) {
					ERROR("Failed to allocate cache"
							"block\n");
					up(&msblk->block_cache_mutex);
					goto out;
				}
			}
	
			msblk->block_cache[i].block = SQUASHFS_USED_BLK;
			up(&msblk->block_cache_mutex);

			if (!(msblk->block_cache[i].length =
						squashfs_read_data(s,
						msblk->block_cache[i].data,
						block, 0, &next_index))) {
				ERROR("Unable to read cache block [%llx:%x]\n",
						block, offset);
				goto out;
			}

			down(&msblk->block_cache_mutex);
			wake_up(&msblk->waitq);
			msblk->block_cache[i].block = block;
			msblk->block_cache[i].next_index = next_index;
			TRACE("Read cache block [%llx:%x]\n", block, offset);
		}

		if (msblk->block_cache[i].block != block) {
			up(&msblk->block_cache_mutex);
			continue;
		}

		if ((bytes = msblk->block_cache[i].length - offset) >= length) {
			if (buffer)
				memcpy(buffer, msblk->block_cache[i].data +
						offset, length);
			if (msblk->block_cache[i].length - offset == length) {
				*next_block = msblk->block_cache[i].next_index;
				*next_offset = 0;
			} else {
				*next_block = block;
				*next_offset = offset + length;
			}
			up(&msblk->block_cache_mutex);
			goto finish;
		} else {
			if (buffer) {
				memcpy(buffer, msblk->block_cache[i].data +
						offset, bytes);
				buffer += bytes;
			}
			block = msblk->block_cache[i].next_index;
			up(&msblk->block_cache_mutex);
			length -= bytes;
			offset = 0;
		}
	}

finish:
	return return_length;
out:
	return 0;
}


static int get_fragment_location(struct super_block *s, unsigned int fragment,
				long long *fragment_start_block,
				unsigned int *fragment_size)
{
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	long long start_block =
		msblk->fragment_index[SQUASHFS_FRAGMENT_INDEX(fragment)];
	int offset = SQUASHFS_FRAGMENT_INDEX_OFFSET(fragment);
	struct squashfs_fragment_entry fragment_entry;

	if (msblk->swap) {
		struct squashfs_fragment_entry sfragment_entry;

		if (!squashfs_get_cached_block(s, (char *) &sfragment_entry,
					start_block, offset,
					sizeof(sfragment_entry), &start_block,
					&offset))
			goto out;
		SQUASHFS_SWAP_FRAGMENT_ENTRY(&fragment_entry, &sfragment_entry);
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


SQSH_EXTERN void release_cached_fragment(struct squashfs_sb_info *msblk, struct
					squashfs_fragment_cache *fragment)
{
	down(&msblk->fragment_mutex);
	fragment->locked --;
	wake_up(&msblk->fragment_wait_queue);
	up(&msblk->fragment_mutex);
}


SQSH_EXTERN struct squashfs_fragment_cache *get_cached_fragment(struct super_block
					*s, long long start_block,
					int length)
{
	int i, n;
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;

	while ( 1 ) {
		down(&msblk->fragment_mutex);

		for (i = 0; i < SQUASHFS_CACHED_FRAGMENTS &&
				msblk->fragment[i].block != start_block; i++);

		if (i == SQUASHFS_CACHED_FRAGMENTS) {
			for (i = msblk->next_fragment, n =
				SQUASHFS_CACHED_FRAGMENTS; n &&
				msblk->fragment[i].locked; n--, i = (i + 1) %
				SQUASHFS_CACHED_FRAGMENTS);

			if (n == 0) {
				wait_queue_t wait;

				init_waitqueue_entry(&wait, current);
				add_wait_queue(&msblk->fragment_wait_queue,
									&wait);
				set_current_state(TASK_UNINTERRUPTIBLE);
				up(&msblk->fragment_mutex);
				schedule();
				set_current_state(TASK_RUNNING);
				remove_wait_queue(&msblk->fragment_wait_queue,
									&wait);
				continue;
			}
			msblk->next_fragment = (msblk->next_fragment + 1) %
				SQUASHFS_CACHED_FRAGMENTS;
			
			if (msblk->fragment[i].data == NULL)
				if (!(msblk->fragment[i].data = SQUASHFS_ALLOC
						(SQUASHFS_FILE_MAX_SIZE))) {
					ERROR("Failed to allocate fragment "
							"cache block\n");
					up(&msblk->fragment_mutex);
					goto out;
				}

			msblk->fragment[i].block = SQUASHFS_INVALID_BLK;
			msblk->fragment[i].locked = 1;
			up(&msblk->fragment_mutex);

			if (!(msblk->fragment[i].length = squashfs_read_data(s,
						msblk->fragment[i].data,
						start_block, length, NULL))) {
				ERROR("Unable to read fragment cache block "
							"[%llx]\n", start_block);
				msblk->fragment[i].locked = 0;
				goto out;
			}

			msblk->fragment[i].block = start_block;
			TRACE("New fragment %d, start block %lld, locked %d\n",
						i, msblk->fragment[i].block,
						msblk->fragment[i].locked);
			break;
		}

		msblk->fragment[i].locked++;
		up(&msblk->fragment_mutex);
		TRACE("Got fragment %d, start block %lld, locked %d\n", i,
						msblk->fragment[i].block,
						msblk->fragment[i].locked);
		break;
	}

	return &msblk->fragment[i];

out:
	return NULL;
}


static struct inode *squashfs_new_inode(struct super_block *s,
		struct squashfs_base_inode_header *inodeb)
{
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	struct inode *i = new_inode(s);

	if (i) {
		i->i_ino = inodeb->inode_number;
		i->i_mtime = inodeb->mtime;
		i->i_atime = inodeb->mtime;
		i->i_ctime = inodeb->mtime;
		i->i_uid = msblk->uid[inodeb->uid];
		i->i_mode = inodeb->mode;
		i->i_size = 0;
		if (inodeb->guid == SQUASHFS_GUIDS)
			i->i_gid = i->i_uid;
		else
			i->i_gid = msblk->guid[inodeb->guid];
	}

	return i;
}


static struct inode *squashfs_iget(struct super_block *s, squashfs_inode_t inode)
{
	struct inode *i;
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	struct squashfs_super_block *sblk = &msblk->sblk;
	long long block = SQUASHFS_INODE_BLK(inode) +
		sblk->inode_table_start;
	unsigned int offset = SQUASHFS_INODE_OFFSET(inode);
	long long next_block;
	unsigned int next_offset;
	union squashfs_inode_header id, sid;
	struct squashfs_base_inode_header *inodeb = &id.base,
					  *sinodeb = &sid.base;

	TRACE("Entered squashfs_iget\n");

	if (msblk->swap) {
		if (!squashfs_get_cached_block(s, (char *) sinodeb, block,
					offset, sizeof(*sinodeb), &next_block,
					&next_offset))
			goto failed_read;
		SQUASHFS_SWAP_BASE_INODE_HEADER(inodeb, sinodeb,
					sizeof(*sinodeb));
	} else
		if (!squashfs_get_cached_block(s, (char *) inodeb, block,
					offset, sizeof(*inodeb), &next_block,
					&next_offset))
			goto failed_read;

	switch(inodeb->inode_type) {
		case SQUASHFS_FILE_TYPE: {
			unsigned int frag_size;
			long long frag_blk;
			struct squashfs_reg_inode_header *inodep = &id.reg;
			struct squashfs_reg_inode_header *sinodep = &sid.reg;
				
			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, (char *)
						sinodep, block, offset,
						sizeof(*sinodep), &next_block,
						&next_offset))
					goto failed_read;
				SQUASHFS_SWAP_REG_INODE_HEADER(inodep, sinodep);
			} else
				if (!squashfs_get_cached_block(s, (char *)
						inodep, block, offset,
						sizeof(*inodep), &next_block,
						&next_offset))
					goto failed_read;

			frag_blk = SQUASHFS_INVALID_BLK;
			if (inodep->fragment != SQUASHFS_INVALID_FRAG &&
					!get_fragment_location(s,
					inodep->fragment, &frag_blk, &frag_size))
				goto failed_read;
				
			if((i = squashfs_new_inode(s, inodeb)) == NULL)
				goto failed_read1;

			i->i_nlink = 1;
			i->i_size = inodep->file_size;
			i->i_fop = &generic_ro_fops;
			i->i_mode |= S_IFREG;
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

			TRACE("File inode %x:%x, start_block %llx, "
					"block_list_start %llx, offset %x\n",
					SQUASHFS_INODE_BLK(inode), offset,
					inodep->start_block, next_block,
					next_offset);
			break;
		}
		case SQUASHFS_LREG_TYPE: {
			unsigned int frag_size;
			long long frag_blk;
			struct squashfs_lreg_inode_header *inodep = &id.lreg;
			struct squashfs_lreg_inode_header *sinodep = &sid.lreg;
				
			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, (char *)
						sinodep, block, offset,
						sizeof(*sinodep), &next_block,
						&next_offset))
					goto failed_read;
				SQUASHFS_SWAP_LREG_INODE_HEADER(inodep, sinodep);
			} else
				if (!squashfs_get_cached_block(s, (char *)
						inodep, block, offset,
						sizeof(*inodep), &next_block,
						&next_offset))
					goto failed_read;

			frag_blk = SQUASHFS_INVALID_BLK;
			if (inodep->fragment != SQUASHFS_INVALID_FRAG &&
					!get_fragment_location(s,
					inodep->fragment, &frag_blk, &frag_size))
				goto failed_read;
				
			if((i = squashfs_new_inode(s, inodeb)) == NULL)
				goto failed_read1;

			i->i_nlink = inodep->nlink;
			i->i_size = inodep->file_size;
			i->i_fop = &generic_ro_fops;
			i->i_mode |= S_IFREG;
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

			TRACE("File inode %x:%x, start_block %llx, "
					"block_list_start %llx, offset %x\n",
					SQUASHFS_INODE_BLK(inode), offset,
					inodep->start_block, next_block,
					next_offset);
			break;
		}
		case SQUASHFS_DIR_TYPE: {
			struct squashfs_dir_inode_header *inodep = &id.dir;
			struct squashfs_dir_inode_header *sinodep = &sid.dir;

			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, (char *)
						sinodep, block, offset,
						sizeof(*sinodep), &next_block,
						&next_offset))
					goto failed_read;
				SQUASHFS_SWAP_DIR_INODE_HEADER(inodep, sinodep);
			} else
				if (!squashfs_get_cached_block(s, (char *)
						inodep, block, offset,
						sizeof(*inodep), &next_block,
						&next_offset))
					goto failed_read;

			if((i = squashfs_new_inode(s, inodeb)) == NULL)
				goto failed_read1;

			i->i_nlink = inodep->nlink;
			i->i_size = inodep->file_size;
			i->i_op = &squashfs_dir_inode_ops;
			i->i_fop = &squashfs_dir_ops;
			i->i_mode |= S_IFDIR;
			SQUASHFS_I(i)->start_block = inodep->start_block;
			SQUASHFS_I(i)->offset = inodep->offset;
			SQUASHFS_I(i)->u.s2.directory_index_count = 0;
			SQUASHFS_I(i)->u.s2.parent_inode = inodep->parent_inode;

			TRACE("Directory inode %x:%x, start_block %x, offset "
					"%x\n", SQUASHFS_INODE_BLK(inode),
					offset, inodep->start_block,
					inodep->offset);
			break;
		}
		case SQUASHFS_LDIR_TYPE: {
			struct squashfs_ldir_inode_header *inodep = &id.ldir;
			struct squashfs_ldir_inode_header *sinodep = &sid.ldir;

			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, (char *)
						sinodep, block, offset,
						sizeof(*sinodep), &next_block,
						&next_offset))
					goto failed_read;
				SQUASHFS_SWAP_LDIR_INODE_HEADER(inodep,
						sinodep);
			} else
				if (!squashfs_get_cached_block(s, (char *)
						inodep, block, offset,
						sizeof(*inodep), &next_block,
						&next_offset))
					goto failed_read;

			if((i = squashfs_new_inode(s, inodeb)) == NULL)
				goto failed_read1;

			i->i_nlink = inodep->nlink;
			i->i_size = inodep->file_size;
			i->i_op = &squashfs_dir_inode_ops;
			i->i_fop = &squashfs_dir_ops;
			i->i_mode |= S_IFDIR;
			SQUASHFS_I(i)->start_block = inodep->start_block;
			SQUASHFS_I(i)->offset = inodep->offset;
			SQUASHFS_I(i)->u.s2.directory_index_start = next_block;
			SQUASHFS_I(i)->u.s2.directory_index_offset =
								next_offset;
			SQUASHFS_I(i)->u.s2.directory_index_count =
								inodep->i_count;
			SQUASHFS_I(i)->u.s2.parent_inode = inodep->parent_inode;

			TRACE("Long directory inode %x:%x, start_block %x, "
					"offset %x\n",
					SQUASHFS_INODE_BLK(inode), offset,
					inodep->start_block, inodep->offset);
			break;
		}
		case SQUASHFS_SYMLINK_TYPE: {
			struct squashfs_symlink_inode_header *inodep =
								&id.symlink;
			struct squashfs_symlink_inode_header *sinodep =
								&sid.symlink;
	
			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, (char *)
						sinodep, block, offset,
						sizeof(*sinodep), &next_block,
						&next_offset))
					goto failed_read;
				SQUASHFS_SWAP_SYMLINK_INODE_HEADER(inodep,
								sinodep);
			} else
				if (!squashfs_get_cached_block(s, (char *)
						inodep, block, offset,
						sizeof(*inodep), &next_block,
						&next_offset))
					goto failed_read;

			if((i = squashfs_new_inode(s, inodeb)) == NULL)
				goto failed_read1;

			i->i_nlink = inodep->nlink;
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
			struct squashfs_dev_inode_header *inodep = &id.dev;
			struct squashfs_dev_inode_header *sinodep = &sid.dev;

			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, (char *)
						sinodep, block, offset,
						sizeof(*sinodep), &next_block,
						&next_offset))
					goto failed_read;
				SQUASHFS_SWAP_DEV_INODE_HEADER(inodep, sinodep);
			} else	
				if (!squashfs_get_cached_block(s, (char *)
						inodep, block, offset,
						sizeof(*inodep), &next_block,
						&next_offset))
					goto failed_read;

			if ((i = squashfs_new_inode(s, inodeb)) == NULL)
				goto failed_read1;

			i->i_nlink = inodep->nlink;
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
			struct squashfs_ipc_inode_header *inodep = &id.ipc;
			struct squashfs_ipc_inode_header *sinodep = &sid.ipc;

			if (msblk->swap) {
				if (!squashfs_get_cached_block(s, (char *)
						sinodep, block, offset,
						sizeof(*sinodep), &next_block,
						&next_offset))
					goto failed_read;
				SQUASHFS_SWAP_IPC_INODE_HEADER(inodep, sinodep);
			} else	
				if (!squashfs_get_cached_block(s, (char *)
						inodep, block, offset,
						sizeof(*inodep), &next_block,
						&next_offset))
					goto failed_read;

			if ((i = squashfs_new_inode(s, inodeb)) == NULL)
				goto failed_read1;

			i->i_nlink = inodep->nlink;
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
	ERROR("Unable to read inode [%llx:%x]\n", block, offset);

failed_read1:
	return NULL;
}


int read_fragment_index_table(struct super_block *s)
{
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	struct squashfs_super_block *sblk = &msblk->sblk;

	if (!(msblk->fragment_index = kmalloc(SQUASHFS_FRAGMENT_INDEX_BYTES
					(sblk->fragments), GFP_KERNEL))) {
		ERROR("Failed to allocate uid/gid table\n");
		return 0;
	}
   
	if (SQUASHFS_FRAGMENT_INDEX_BYTES(sblk->fragments) &&
					!squashfs_read_data(s, (char *)
					msblk->fragment_index,
					sblk->fragment_table_start,
					SQUASHFS_FRAGMENT_INDEX_BYTES
					(sblk->fragments) |
					SQUASHFS_COMPRESSED_BIT_BLOCK, NULL)) {
		ERROR("unable to read fragment index table\n");
		return 0;
	}

	if (msblk->swap) {
		int i;
		long long fragment;

		for (i = 0; i < SQUASHFS_FRAGMENT_INDEXES(sblk->fragments);
									i++) {
			SQUASHFS_SWAP_FRAGMENT_INDEXES((&fragment),
						&msblk->fragment_index[i], 1);
			msblk->fragment_index[i] = fragment;
		}
	}

	return 1;
}


static int supported_squashfs_filesystem(struct squashfs_sb_info *msblk, int silent)
{
	struct squashfs_super_block *sblk = &msblk->sblk;

	msblk->iget = squashfs_iget;
	msblk->read_blocklist = read_blocklist;
	msblk->read_fragment_index_table = read_fragment_index_table;

	if (sblk->s_major == 1) {
		if (!squashfs_1_0_supported(msblk)) {
			SERROR("Major/Minor mismatch, Squashfs 1.0 filesystems "
				"are unsupported\n");
			SERROR("Please recompile with "
				"Squashfs 1.0 support enabled\n");
			return 0;
		}
	} else if (sblk->s_major == 2) {
		if (!squashfs_2_0_supported(msblk)) {
			SERROR("Major/Minor mismatch, Squashfs 2.0 filesystems "
				"are unsupported\n");
			SERROR("Please recompile with "
				"Squashfs 2.0 support enabled\n");
			return 0;
		}
	} else if(sblk->s_major != SQUASHFS_MAJOR || sblk->s_minor >
			SQUASHFS_MINOR) {
		SERROR("Major/Minor mismatch, trying to mount newer %d.%d "
				"filesystem\n", sblk->s_major, sblk->s_minor);
		SERROR("Please update your kernel\n");
		return 0;
	}

	return 1;
}


static struct super_block *squashfs_read_super(struct super_block *s,
		void *data, int silent)
{
	kdev_t dev = s->s_dev;
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	struct squashfs_super_block *sblk = &msblk->sblk;
	int i;
	struct inode *root;

	if (!(msblk->stream.workspace = vmalloc(zlib_inflate_workspacesize()))) {
		ERROR("Failed to allocate zlib workspace\n");
		goto failed_mount;
	}

	msblk->devblksize = get_hardsect_size(dev);
	if(msblk->devblksize < BLOCK_SIZE)
		msblk->devblksize = BLOCK_SIZE;
	msblk->devblksize_log2 = ffz(~msblk->devblksize);
        set_blocksize(dev, msblk->devblksize);
	s->s_blocksize = msblk->devblksize;
	s->s_blocksize_bits = msblk->devblksize_log2;
	
	init_MUTEX(&msblk->read_data_mutex);
	init_MUTEX(&msblk->read_page_mutex);
	init_MUTEX(&msblk->block_cache_mutex);
	init_MUTEX(&msblk->fragment_mutex);
	
	init_waitqueue_head(&msblk->waitq);
	init_waitqueue_head(&msblk->fragment_wait_queue);

	if (!squashfs_read_data(s, (char *) sblk, SQUASHFS_START,
					sizeof(struct squashfs_super_block) |
					SQUASHFS_COMPRESSED_BIT_BLOCK, NULL)) {
		SERROR("unable to read superblock\n");
		goto failed_mount;
	}

	/* Check it is a SQUASHFS superblock */
	msblk->swap = 0;
	if ((s->s_magic = sblk->s_magic) != SQUASHFS_MAGIC) {
		if (sblk->s_magic == SQUASHFS_MAGIC_SWAP) {
			struct squashfs_super_block ssblk;

			WARNING("Mounting a different endian SQUASHFS "
				"filesystem on %s\n", bdevname(dev));

			SQUASHFS_SWAP_SUPER_BLOCK(&ssblk, sblk);
			memcpy(sblk, &ssblk, sizeof(struct squashfs_super_block));
			msblk->swap = 1;
		} else  {
			SERROR("Can't find a SQUASHFS superblock on %s\n",
							bdevname(dev));
			goto failed_mount;
		}
	}

	/* Check the MAJOR & MINOR versions */
	if(!supported_squashfs_filesystem(msblk, silent))
		goto failed_mount;

	TRACE("Found valid superblock on %s\n", bdevname(dev));
	TRACE("Inodes are %scompressed\n",
					SQUASHFS_UNCOMPRESSED_INODES
					(sblk->flags) ? "un" : "");
	TRACE("Data is %scompressed\n",
					SQUASHFS_UNCOMPRESSED_DATA(sblk->flags)
					? "un" : "");
	TRACE("Check data is %s present in the filesystem\n",
					SQUASHFS_CHECK_DATA(sblk->flags) ?
					"" : "not");
	TRACE("Filesystem size %lld bytes\n", sblk->bytes_used);
	TRACE("Block size %d\n", sblk->block_size);
	TRACE("Number of inodes %d\n", sblk->inodes);
	if (sblk->s_major > 1)
		TRACE("Number of fragments %d\n", sblk->fragments);
	TRACE("Number of uids %d\n", sblk->no_uids);
	TRACE("Number of gids %d\n", sblk->no_guids);
	TRACE("sblk->inode_table_start %llx\n", sblk->inode_table_start);
	TRACE("sblk->directory_table_start %llx\n", sblk->directory_table_start);
	if (sblk->s_major > 1)
		TRACE("sblk->fragment_table_start %llx\n",
					sblk->fragment_table_start);
	TRACE("sblk->uid_start %llx\n", sblk->uid_start);

	s->s_flags |= MS_RDONLY;
	s->s_op = &squashfs_ops;

	/* Init inode_table block pointer array */
	if (!(msblk->block_cache = kmalloc(sizeof(struct squashfs_cache) *
					SQUASHFS_CACHED_BLKS, GFP_KERNEL))) {
		ERROR("Failed to allocate block cache\n");
		goto failed_mount;
	}

	for (i = 0; i < SQUASHFS_CACHED_BLKS; i++)
		msblk->block_cache[i].block = SQUASHFS_INVALID_BLK;

	msblk->next_cache = 0;

	/* Allocate read_data block */
	msblk->read_size = (sblk->block_size < SQUASHFS_METADATA_SIZE) ?
					SQUASHFS_METADATA_SIZE :
					sblk->block_size;

	if (!(msblk->read_data = kmalloc(msblk->read_size, GFP_KERNEL))) {
		ERROR("Failed to allocate read_data block\n");
		goto failed_mount;
	}

	/* Allocate read_page block */
	if (!(msblk->read_page = kmalloc(sblk->block_size, GFP_KERNEL))) {
		ERROR("Failed to allocate read_page block\n");
		goto failed_mount;
	}

	/* Allocate uid and gid tables */
	if (!(msblk->uid = kmalloc((sblk->no_uids + sblk->no_guids) *
					sizeof(unsigned int), GFP_KERNEL))) {
		ERROR("Failed to allocate uid/gid table\n");
		goto failed_mount;
	}
	msblk->guid = msblk->uid + sblk->no_uids;
   
	if (msblk->swap) {
		unsigned int suid[sblk->no_uids + sblk->no_guids];

		if (!squashfs_read_data(s, (char *) &suid, sblk->uid_start,
					((sblk->no_uids + sblk->no_guids) *
					 sizeof(unsigned int)) |
					SQUASHFS_COMPRESSED_BIT_BLOCK, NULL)) {
			ERROR("unable to read uid/gid table\n");
			goto failed_mount;
		}

		SQUASHFS_SWAP_DATA(msblk->uid, suid, (sblk->no_uids +
			sblk->no_guids), (sizeof(unsigned int) * 8));
	} else
		if (!squashfs_read_data(s, (char *) msblk->uid, sblk->uid_start,
					((sblk->no_uids + sblk->no_guids) *
					 sizeof(unsigned int)) |
					SQUASHFS_COMPRESSED_BIT_BLOCK, NULL)) {
			ERROR("unable to read uid/gid table\n");
			goto failed_mount;
		}


	if (sblk->s_major == 1 && squashfs_1_0_supported(msblk))
		goto allocate_root;

	if (!(msblk->fragment = kmalloc(sizeof(struct squashfs_fragment_cache) *
				SQUASHFS_CACHED_FRAGMENTS, GFP_KERNEL))) {
		ERROR("Failed to allocate fragment block cache\n");
		goto failed_mount;
	}

	for (i = 0; i < SQUASHFS_CACHED_FRAGMENTS; i++) {
		msblk->fragment[i].locked = 0;
		msblk->fragment[i].block = SQUASHFS_INVALID_BLK;
		msblk->fragment[i].data = NULL;
	}

	msblk->next_fragment = 0;

	/* Allocate fragment index table */
	if(msblk->read_fragment_index_table(s) == 0)
		goto failed_mount;

allocate_root:
	if ((root = (msblk->iget)(s, sblk->root_inode)) == NULL)
		goto failed_mount;

	if ((s->s_root = d_alloc_root(root)) == NULL) {
		ERROR("Root inode create failed\n");
		iput(root);
		goto failed_mount;
	}

	TRACE("Leaving squashfs_read_super\n");
	return s;

failed_mount:
	kfree(msblk->fragment_index);
	kfree(msblk->fragment);
	kfree(msblk->uid);
	kfree(msblk->read_page);
	kfree(msblk->read_data);
	kfree(msblk->block_cache);
	kfree(msblk->fragment_index_2);
	vfree(msblk->stream.workspace);
	return NULL;
}


static int squashfs_statfs(struct super_block *s, struct statfs *buf)
{
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	struct squashfs_super_block *sblk = &msblk->sblk;

	TRACE("Entered squashfs_statfs\n");

	buf->f_type = SQUASHFS_MAGIC;
	buf->f_bsize = sblk->block_size;
	buf->f_blocks = ((sblk->bytes_used - 1) >> sblk->block_log) + 1;
	buf->f_bfree = buf->f_bavail = 0;
	buf->f_files = sblk->inodes;
	buf->f_ffree = 0;
	buf->f_namelen = SQUASHFS_NAME_LEN;

	return 0;
}


static int squashfs_symlink_readpage(struct file *file, struct page *page)
{
	struct inode *inode = page->mapping->host;
	int index = page->index << PAGE_CACHE_SHIFT, length, bytes;
	long long block = SQUASHFS_I(inode)->start_block;
	int offset = SQUASHFS_I(inode)->offset;
	void *pageaddr = kmap(page);

	TRACE("Entered squashfs_symlink_readpage, page index %ld, start block "
				"%llx, offset %x\n", page->index,
				SQUASHFS_I(inode)->start_block,
				SQUASHFS_I(inode)->offset);

	for (length = 0; length < index; length += bytes) {
		if (!(bytes = squashfs_get_cached_block(inode->i_sb, NULL,
				block, offset, PAGE_CACHE_SIZE, &block,
				&offset))) {
			ERROR("Unable to read symbolic link [%llx:%x]\n", block,
					offset);
			goto skip_read;
		}
	}

	if (length != index) {
		ERROR("(squashfs_symlink_readpage) length != index\n");
		bytes = 0;
		goto skip_read;
	}

	bytes = (i_size_read(inode) - length) > PAGE_CACHE_SIZE ? PAGE_CACHE_SIZE :
					i_size_read(inode) - length;

	if (!(bytes = squashfs_get_cached_block(inode->i_sb, pageaddr, block,
					offset, bytes, &block, &offset)))
		ERROR("Unable to read symbolic link [%llx:%x]\n", block, offset);

skip_read:
	memset(pageaddr + bytes, 0, PAGE_CACHE_SIZE - bytes);
	kunmap(page);
	SetPageUptodate(page);
	UnlockPage(page);

	return 0;
}


struct meta_index *locate_meta_index(struct inode *inode, int index, int offset)
{
	struct meta_index *meta = NULL;
	struct squashfs_sb_info *msblk = &inode->i_sb->u.squashfs_sb;
	int i;

	down(&msblk->meta_index_mutex);

	TRACE("locate_meta_index: index %d, offset %d\n", index, offset);

	if(msblk->meta_index == NULL)
		goto not_allocated;

	for (i = 0; i < SQUASHFS_META_NUMBER; i ++)
		if (msblk->meta_index[i].inode_number == inode->i_ino &&
				msblk->meta_index[i].offset >= offset &&
				msblk->meta_index[i].offset <= index &&
				msblk->meta_index[i].locked == 0) {
			TRACE("locate_meta_index: entry %d, offset %d\n", i,
					msblk->meta_index[i].offset);
			meta = &msblk->meta_index[i];
			offset = meta->offset;
		}

	if (meta)
		meta->locked = 1;

not_allocated:
	up(&msblk->meta_index_mutex);

	return meta;
}


struct meta_index *empty_meta_index(struct inode *inode, int offset, int skip)
{
	struct squashfs_sb_info *msblk = &inode->i_sb->u.squashfs_sb;
	struct meta_index *meta = NULL;
	int i;

	down(&msblk->meta_index_mutex);

	TRACE("empty_meta_index: offset %d, skip %d\n", offset, skip);

	if(msblk->meta_index == NULL) {
		if (!(msblk->meta_index = kmalloc(sizeof(struct meta_index) *
					SQUASHFS_META_NUMBER, GFP_KERNEL))) {
			ERROR("Failed to allocate meta_index\n");
			goto failed;
		}
		for(i = 0; i < SQUASHFS_META_NUMBER; i++) {
			msblk->meta_index[i].inode_number = 0;
			msblk->meta_index[i].locked = 0;
		}
		msblk->next_meta_index = 0;
	}

	for(i = SQUASHFS_META_NUMBER; i &&
			msblk->meta_index[msblk->next_meta_index].locked; i --)
		msblk->next_meta_index = (msblk->next_meta_index + 1) %
			SQUASHFS_META_NUMBER;

	if(i == 0) {
		TRACE("empty_meta_index: failed!\n");
		goto failed;
	}

	TRACE("empty_meta_index: returned meta entry %d, %p\n",
			msblk->next_meta_index,
			&msblk->meta_index[msblk->next_meta_index]);

	meta = &msblk->meta_index[msblk->next_meta_index];
	msblk->next_meta_index = (msblk->next_meta_index + 1) %
			SQUASHFS_META_NUMBER;

	meta->inode_number = inode->i_ino;
	meta->offset = offset;
	meta->skip = skip;
	meta->entries = 0;
	meta->locked = 1;

failed:
	up(&msblk->meta_index_mutex);
	return meta;
}


void release_meta_index(struct inode *inode, struct meta_index *meta)
{
	meta->locked = 0;
}


static int read_block_index(struct super_block *s, int blocks, char *block_list,
		long long *start_block, int *offset)
{
	struct squashfs_sb_info *msblk = &s->u.squashfs_sb;
	unsigned int *block_listp;
	int block = 0;
	
	if (msblk->swap) {
		char sblock_list[blocks << 2];

		if (!squashfs_get_cached_block(s, sblock_list, *start_block,
				*offset, blocks << 2, start_block, offset)) {
			ERROR("Unable to read block list [%llx:%x]\n",
				*start_block, *offset);
			goto failure;
		}
		SQUASHFS_SWAP_INTS(((unsigned int *)block_list),
				((unsigned int *)sblock_list), blocks);
	} else
		if (!squashfs_get_cached_block(s, block_list, *start_block,
				*offset, blocks << 2, start_block, offset)) {
			ERROR("Unable to read block list [%llx:%x]\n",
				*start_block, *offset);
			goto failure;
		}

	for (block_listp = (unsigned int *) block_list; blocks;
				block_listp++, blocks --)
		block += SQUASHFS_COMPRESSED_SIZE_BLOCK(*block_listp);

	return block;

failure:
	return -1;
}


#define SIZE 256

static inline int calculate_skip(int blocks) {
	int skip = (blocks - 1) / ((SQUASHFS_SLOTS * SQUASHFS_META_ENTRIES + 1) * SQUASHFS_META_INDEXES);
	return skip >= 7 ? 7 : skip + 1;
}


static int get_meta_index(struct inode *inode, int index,
		long long *index_block, int *index_offset,
		long long *data_block, char *block_list)
{
	struct squashfs_sb_info *msblk = &inode->i_sb->u.squashfs_sb;
	struct squashfs_super_block *sblk = &msblk->sblk;
	int skip = calculate_skip(i_size_read(inode) >> sblk->block_log);
	int offset = 0;
	struct meta_index *meta;
	struct meta_entry *meta_entry;
	long long cur_index_block = SQUASHFS_I(inode)->u.s1.block_list_start;
	int cur_offset = SQUASHFS_I(inode)->offset;
	long long cur_data_block = SQUASHFS_I(inode)->start_block;
	int i;
 
	index /= SQUASHFS_META_INDEXES * skip;

	while ( offset < index ) {
		meta = locate_meta_index(inode, index, offset + 1);

		if (meta == NULL) {
			if ((meta = empty_meta_index(inode, offset + 1,
							skip)) == NULL)
				goto all_done;
		} else {
			offset = index < meta->offset + meta->entries ? index :
				meta->offset + meta->entries - 1;
			meta_entry = &meta->meta_entry[offset - meta->offset];
			cur_index_block = meta_entry->index_block + sblk->inode_table_start;
			cur_offset = meta_entry->offset;
			cur_data_block = meta_entry->data_block;
			TRACE("get_meta_index: offset %d, meta->offset %d, "
				"meta->entries %d\n", offset, meta->offset,
				meta->entries);
			TRACE("get_meta_index: index_block 0x%llx, offset 0x%x"
				" data_block 0x%llx\n", cur_index_block,
				cur_offset, cur_data_block);
		}

		for (i = meta->offset + meta->entries; i <= index &&
				i < meta->offset + SQUASHFS_META_ENTRIES; i++) {
			int blocks = skip * SQUASHFS_META_INDEXES;

			while (blocks) {
				int block = blocks > (SIZE >> 2) ? (SIZE >> 2) :
					blocks;
				int res = read_block_index(inode->i_sb, block,
					block_list, &cur_index_block,
					&cur_offset);

				if (res == -1)
					goto failed;

				cur_data_block += res;
				blocks -= block;
			}

			meta_entry = &meta->meta_entry[i - meta->offset];
			meta_entry->index_block = cur_index_block - sblk->inode_table_start;
			meta_entry->offset = cur_offset;
			meta_entry->data_block = cur_data_block;
			meta->entries ++;
			offset ++;
		}

		TRACE("get_meta_index: meta->offset %d, meta->entries %d\n",
				meta->offset, meta->entries);

		release_meta_index(inode, meta);
	}

all_done:
	*index_block = cur_index_block;
	*index_offset = cur_offset;
	*data_block = cur_data_block;

	return offset * SQUASHFS_META_INDEXES * skip;

failed:
	release_meta_index(inode, meta);
	return -1;
}


static long long read_blocklist(struct inode *inode, int index,
				int readahead_blks, char *block_list,
				unsigned short **block_p, unsigned int *bsize)
{
	long long block_ptr;
	int offset;
	long long block;
	int res = get_meta_index(inode, index, &block_ptr, &offset, &block,
		block_list);

	TRACE("read_blocklist: res %d, index %d, block_ptr 0x%llx, offset"
		       " 0x%x, block 0x%llx\n", res, index, block_ptr, offset,
		       block);

	if(res == -1)
		goto failure;

	index -= res;

	while ( index ) {
		int blocks = index > (SIZE >> 2) ? (SIZE >> 2) : index;
		int res = read_block_index(inode->i_sb, blocks, block_list,
			&block_ptr, &offset);
		if (res == -1)
			goto failure;
		block += res;
		index -= blocks;
	}

	if (read_block_index(inode->i_sb, 1, block_list,
			&block_ptr, &offset) == -1)
		goto failure;
	*bsize = *((unsigned int *) block_list);

	return block;

failure:
	return 0;
}


static int squashfs_readpage(struct file *file, struct page *page)
{
	struct inode *inode = page->mapping->host;
	struct squashfs_sb_info *msblk = &inode->i_sb->u.squashfs_sb;
	struct squashfs_super_block *sblk = &msblk->sblk;
	unsigned char block_list[SIZE];
	long long block;
	unsigned int bsize, i = 0, bytes = 0, byte_offset = 0;
	int index = page->index >> (sblk->block_log - PAGE_CACHE_SHIFT);
 	void *pageaddr;
	struct squashfs_fragment_cache *fragment = NULL;
	char *data_ptr = msblk->read_page;
	
	int mask = (1 << (sblk->block_log - PAGE_CACHE_SHIFT)) - 1;
	int start_index = page->index & ~mask;
	int end_index = start_index | mask;

	TRACE("Entered squashfs_readpage, page index %lx, start block %llx\n",
					page->index,
					SQUASHFS_I(inode)->start_block);

	if (page->index >= ((i_size_read(inode) + PAGE_CACHE_SIZE - 1) >>
					PAGE_CACHE_SHIFT))
		goto skip_read;

	if (SQUASHFS_I(inode)->u.s1.fragment_start_block == SQUASHFS_INVALID_BLK
					|| index < (i_size_read(inode) >>
					sblk->block_log)) {
		if ((block = (msblk->read_blocklist)(inode, index, 1,
					block_list, NULL, &bsize)) == 0)
			goto skip_read;

		down(&msblk->read_page_mutex);
		
		if (!(bytes = squashfs_read_data(inode->i_sb, msblk->read_page,
					block, bsize, NULL))) {
			ERROR("Unable to read page, block %llx, size %x\n", block,
					bsize);
			up(&msblk->read_page_mutex);
			goto skip_read;
		}
	} else {
		if ((fragment = get_cached_fragment(inode->i_sb,
					SQUASHFS_I(inode)->
					u.s1.fragment_start_block,
					SQUASHFS_I(inode)->u.s1.fragment_size))
					== NULL) {
			ERROR("Unable to read page, block %llx, size %x\n",
					SQUASHFS_I(inode)->
					u.s1.fragment_start_block,
					(int) SQUASHFS_I(inode)->
					u.s1.fragment_size);
			goto skip_read;
		}
		bytes = SQUASHFS_I(inode)->u.s1.fragment_offset +
					(i_size_read(inode) & (sblk->block_size
					- 1));
		byte_offset = SQUASHFS_I(inode)->u.s1.fragment_offset;
		data_ptr = fragment->data;
	}

	for (i = start_index; i <= end_index && byte_offset < bytes;
					i++, byte_offset += PAGE_CACHE_SIZE) {
		struct page *push_page;
		int available_bytes = (bytes - byte_offset) > PAGE_CACHE_SIZE ?
					PAGE_CACHE_SIZE : bytes - byte_offset;

		TRACE("bytes %d, i %d, byte_offset %d, available_bytes %d\n",
					bytes, i, byte_offset, available_bytes);

		if (i == page->index)  {
			pageaddr = kmap_atomic(page, KM_USER0);
			memcpy(pageaddr, data_ptr + byte_offset,
					available_bytes);
			memset(pageaddr + available_bytes, 0,
					PAGE_CACHE_SIZE - available_bytes);
			kunmap_atomic(pageaddr, KM_USER0);
			flush_dcache_page(page);
			SetPageUptodate(page);
			UnlockPage(page);
		} else if ((push_page =
				grab_cache_page_nowait(page->mapping, i))) {
 			pageaddr = kmap_atomic(push_page, KM_USER0);

			memcpy(pageaddr, data_ptr + byte_offset,
					available_bytes);
			memset(pageaddr + available_bytes, 0,
					PAGE_CACHE_SIZE - available_bytes);
			kunmap_atomic(pageaddr, KM_USER0);
			flush_dcache_page(push_page);
			SetPageUptodate(push_page);
			UnlockPage(push_page);
			page_cache_release(push_page);
		}
	}

	if (SQUASHFS_I(inode)->u.s1.fragment_start_block == SQUASHFS_INVALID_BLK
					|| index < (i_size_read(inode) >>
					sblk->block_log))
		up(&msblk->read_page_mutex);
	else
		release_cached_fragment(msblk, fragment);

	return 0;

skip_read:
	pageaddr = kmap_atomic(page, KM_USER0);
	memset(pageaddr + bytes, 0, PAGE_CACHE_SIZE - bytes);
	kunmap_atomic(pageaddr, KM_USER0);
	flush_dcache_page(page);
	SetPageUptodate(page);
	UnlockPage(page);

	return 0;
}


static int squashfs_readpage4K(struct file *file, struct page *page)
{
	struct inode *inode = page->mapping->host;
	struct squashfs_sb_info *msblk = &inode->i_sb->u.squashfs_sb;
	struct squashfs_super_block *sblk = &msblk->sblk;
	unsigned char block_list[SIZE];
	long long block;
	unsigned int bsize, bytes = 0;
 	void *pageaddr;
	
	TRACE("Entered squashfs_readpage4K, page index %lx, start block %llx\n",
					page->index,
					SQUASHFS_I(inode)->start_block);

	if (page->index >= ((i_size_read(inode) + PAGE_CACHE_SIZE - 1) >>
					PAGE_CACHE_SHIFT)) {
		pageaddr = kmap_atomic(page, KM_USER0);
		goto skip_read;
	}

	if (SQUASHFS_I(inode)->u.s1.fragment_start_block == SQUASHFS_INVALID_BLK
					|| page->index < (i_size_read(inode) >>
					sblk->block_log)) {
		block = (msblk->read_blocklist)(inode, page->index, 1,
					block_list, NULL, &bsize);

		down(&msblk->read_page_mutex);
		bytes = squashfs_read_data(inode->i_sb, msblk->read_page, block,
					bsize, NULL);
		pageaddr = kmap_atomic(page, KM_USER0);
		if (bytes)
			memcpy(pageaddr, msblk->read_page, bytes);
		else
			ERROR("Unable to read page, block %llx, size %x\n",
					block, bsize);
		up(&msblk->read_page_mutex);
	} else {
		struct squashfs_fragment_cache *fragment =
			get_cached_fragment(inode->i_sb,
					SQUASHFS_I(inode)->
					u.s1.fragment_start_block,
					SQUASHFS_I(inode)-> u.s1.fragment_size);
		pageaddr = kmap_atomic(page, KM_USER0);
		if (fragment) {
			bytes = i_size_read(inode) & (sblk->block_size - 1);
			memcpy(pageaddr, fragment->data + SQUASHFS_I(inode)->
					u.s1.fragment_offset, bytes);
			release_cached_fragment(msblk, fragment);
		} else
			ERROR("Unable to read page, block %llx, size %x\n",
					SQUASHFS_I(inode)->
					u.s1.fragment_start_block, (int)
					SQUASHFS_I(inode)-> u.s1.fragment_size);
	}

skip_read:
	memset(pageaddr + bytes, 0, PAGE_CACHE_SIZE - bytes);
	kunmap_atomic(pageaddr, KM_USER0);
	flush_dcache_page(page);
	SetPageUptodate(page);
	UnlockPage(page);

	return 0;
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
	struct squashfs_dir_index index;

	TRACE("Entered get_dir_index_using_offset, i_count %d, f_pos %d\n",
					i_count, (unsigned int) f_pos);

	f_pos -= 3;
	if (f_pos == 0)
		goto finish;

	for (i = 0; i < i_count; i++) {
		if (msblk->swap) {
			struct squashfs_dir_index sindex;
			squashfs_get_cached_block(s, (char *) &sindex,
					index_start, index_offset,
					sizeof(sindex), &index_start,
					&index_offset);
			SQUASHFS_SWAP_DIR_INDEX(&index, &sindex);
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
	return length + 3;
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
	char buffer[sizeof(struct squashfs_dir_index) + SQUASHFS_NAME_LEN + 1];
	struct squashfs_dir_index *index = (struct squashfs_dir_index *) buffer;
	char str[SQUASHFS_NAME_LEN + 1];

	TRACE("Entered get_dir_index_using_name, i_count %d\n", i_count);

	strncpy(str, name, size);
	str[size] = '\0';

	for (i = 0; i < i_count; i++) {
		if (msblk->swap) {
			struct squashfs_dir_index sindex;
			squashfs_get_cached_block(s, (char *) &sindex,
					index_start, index_offset,
					sizeof(sindex), &index_start,
					&index_offset);
			SQUASHFS_SWAP_DIR_INDEX(index, &sindex);
		} else
			squashfs_get_cached_block(s, (char *) index,
					index_start, index_offset,
					sizeof(struct squashfs_dir_index),
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
	return length + 3;
}

		
static int squashfs_readdir(struct file *file, void *dirent, filldir_t filldir)
{
	struct inode *i = file->f_dentry->d_inode;
	struct squashfs_sb_info *msblk = &i->i_sb->u.squashfs_sb;
	struct squashfs_super_block *sblk = &msblk->sblk;
	long long next_block = SQUASHFS_I(i)->start_block +
		sblk->directory_table_start;
	int next_offset = SQUASHFS_I(i)->offset, length = 0,
		dir_count;
	struct squashfs_dir_header dirh;
	char buffer[sizeof(struct squashfs_dir_entry) + SQUASHFS_NAME_LEN + 1];
	struct squashfs_dir_entry *dire = (struct squashfs_dir_entry *) buffer;

	TRACE("Entered squashfs_readdir [%llx:%x]\n", next_block, next_offset);

	while(file->f_pos < 3) {
		char *name;
		int size, i_ino;

		if(file->f_pos == 0) {
			name = ".";
			size = 1;
			i_ino = i->i_ino;
		} else {
			name = "..";
			size = 2;
			i_ino = SQUASHFS_I(i)->u.s2.parent_inode;
		}
		TRACE("Calling filldir(%x, %s, %d, %d, %d, %d)\n",
				(unsigned int) dirent, name, size, (int)
				file->f_pos, i_ino,
				squashfs_filetype_table[1]);

		if (filldir(dirent, name, size,
				file->f_pos, i_ino,
				squashfs_filetype_table[1]) < 0) {
				TRACE("Filldir returned less than 0\n");
				goto finish;
		}
		file->f_pos += size;
	}

	length = get_dir_index_using_offset(i->i_sb, &next_block, &next_offset,
				SQUASHFS_I(i)->u.s2.directory_index_start,
				SQUASHFS_I(i)->u.s2.directory_index_offset,
				SQUASHFS_I(i)->u.s2.directory_index_count,
				file->f_pos);

	while (length < i_size_read(i)) {
		/* read directory header */
		if (msblk->swap) {
			struct squashfs_dir_header sdirh;
			
			if (!squashfs_get_cached_block(i->i_sb, (char *) &sdirh,
					next_block, next_offset, sizeof(sdirh),
					&next_block, &next_offset))
				goto failed_read;

			length += sizeof(sdirh);
			SQUASHFS_SWAP_DIR_HEADER(&dirh, &sdirh);
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
				struct squashfs_dir_entry sdire;
				if (!squashfs_get_cached_block(i->i_sb, (char *)
						&sdire, next_block, next_offset,
						sizeof(sdire), &next_block,
						&next_offset))
					goto failed_read;
				
				length += sizeof(sdire);
				SQUASHFS_SWAP_DIR_ENTRY(dire, &sdire);
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

			TRACE("Calling filldir(%x, %s, %d, %d, %x:%x, %d, %d)\n",
					(unsigned int) dirent, dire->name,
					dire->size + 1, (int) file->f_pos,
					dirh.start_block, dire->offset,
					dirh.inode_number + dire->inode_number,
					squashfs_filetype_table[dire->type]);

			if (filldir(dirent, dire->name, dire->size + 1,
					file->f_pos,
					dirh.inode_number + dire->inode_number,
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


static struct dentry *squashfs_lookup(struct inode *i, struct dentry *dentry)
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
	struct squashfs_dir_header dirh;
	char buffer[sizeof(struct squashfs_dir_entry) + SQUASHFS_NAME_LEN];
	struct squashfs_dir_entry *dire = (struct squashfs_dir_entry *) buffer;

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
			struct squashfs_dir_header sdirh;
			if (!squashfs_get_cached_block(i->i_sb, (char *) &sdirh,
					next_block, next_offset, sizeof(sdirh),
					&next_block, &next_offset))
				goto failed_read;

			length += sizeof(sdirh);
			SQUASHFS_SWAP_DIR_HEADER(&dirh, &sdirh);
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
				struct squashfs_dir_entry sdire;
				if (!squashfs_get_cached_block(i->i_sb, (char *)
						&sdire, next_block,next_offset,
						sizeof(sdire), &next_block,
						&next_offset))
					goto failed_read;
				
				length += sizeof(sdire);
				SQUASHFS_SWAP_DIR_ENTRY(dire, &sdire);
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

			if (name[0] < dire->name[0])
				goto exit_loop;

			if ((len == dire->size + 1) && !strncmp(name,
						dire->name, len)) {
				squashfs_inode_t ino =
					SQUASHFS_MKINODE(dirh.start_block,
					dire->offset);

				TRACE("calling squashfs_iget for directory "
					"entry %s, inode %x:%x, %d\n", name,
					dirh.start_block, dire->offset,
					dirh.inode_number + dire->inode_number);

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


static void squashfs_put_super(struct super_block *s)
{
	int i;

		struct squashfs_sb_info *sbi = &s->u.squashfs_sb;
		if (sbi->block_cache)
			for (i = 0; i < SQUASHFS_CACHED_BLKS; i++)
				if (sbi->block_cache[i].block !=
							SQUASHFS_INVALID_BLK)
					kfree(sbi->block_cache[i].data);
		if (sbi->fragment)
			for (i = 0; i < SQUASHFS_CACHED_FRAGMENTS; i++) 
				SQUASHFS_FREE(sbi->fragment[i].data);
		kfree(sbi->fragment);
		kfree(sbi->block_cache);
		kfree(sbi->read_data);
		kfree(sbi->read_page);
		kfree(sbi->uid);
		kfree(sbi->fragment_index);
		kfree(sbi->fragment_index_2);
		kfree(sbi->meta_index);
		vfree(sbi->stream.workspace);
		sbi->block_cache = NULL;
		sbi->uid = NULL;
		sbi->read_data = NULL;
		sbi->read_page = NULL;
		sbi->fragment = NULL;
		sbi->fragment_index = NULL;
		sbi->fragment_index_2 = NULL;
		sbi->meta_index = NULL;
		sbi->stream.workspace = NULL;
}


static int __init init_squashfs_fs(void)
{

	printk(KERN_INFO "squashfs: version 3.1 (2006/08/15) "
		"Phillip Lougher\n");

	return register_filesystem(&squashfs_fs_type);
}


static void __exit exit_squashfs_fs(void)
{
	unregister_filesystem(&squashfs_fs_type);
}


EXPORT_NO_SYMBOLS;

module_init(init_squashfs_fs);
module_exit(exit_squashfs_fs);
MODULE_DESCRIPTION("squashfs 3.1, a compressed read-only filesystem");
MODULE_AUTHOR("Phillip Lougher <phillip@lougher.demon.co.uk>");
MODULE_LICENSE("GPL");
