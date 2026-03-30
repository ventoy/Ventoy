/* ext2.c - Second Extended filesystem */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2004,2005,2007,2008,2009  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Magic value used to identify an ext2 filesystem.  */
#define	EXT2_MAGIC		0xEF53
/* Amount of indirect blocks in an inode.  */
#define INDIRECT_BLOCKS		12

/* The good old revision and the default inode size.  */
#define EXT2_GOOD_OLD_REVISION		0
#define EXT2_GOOD_OLD_INODE_SIZE	128

/* Filetype used in directory entry.  */
#define	FILETYPE_UNKNOWN	0
#define	FILETYPE_REG		1
#define	FILETYPE_DIRECTORY	2
#define	FILETYPE_SYMLINK	7

/* Filetype information as used in inodes.  */
#define FILETYPE_INO_MASK	0170000
#define FILETYPE_INO_REG	0100000
#define FILETYPE_INO_DIRECTORY	0040000
#define FILETYPE_INO_SYMLINK	0120000

#include <grub/err.h>
#include <grub/file.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/disk.h>
#include <grub/dl.h>
#include <grub/types.h>
#include <grub/fshelp.h>
#include <grub/ventoy.h>

GRUB_MOD_LICENSE ("GPLv3+");

/* Log2 size of ext2 block in 512 blocks.  */
#define LOG2_EXT2_BLOCK_SIZE(data)			\
	(grub_le_to_cpu32 (data->sblock.log2_block_size) + 1)

/* Log2 size of ext2 block in bytes.  */
#define LOG2_BLOCK_SIZE(data)					\
	(grub_le_to_cpu32 (data->sblock.log2_block_size) + 10)

/* The size of an ext2 block in bytes.  */
#define EXT2_BLOCK_SIZE(data)		(1U << LOG2_BLOCK_SIZE (data))

/* The revision level.  */
#define EXT2_REVISION(data)	grub_le_to_cpu32 (data->sblock.revision_level)

/* The inode size.  */
#define EXT2_INODE_SIZE(data)	\
  (data->sblock.revision_level \
   == grub_cpu_to_le32_compile_time (EXT2_GOOD_OLD_REVISION)	\
         ? EXT2_GOOD_OLD_INODE_SIZE \
         : grub_le_to_cpu16 (data->sblock.inode_size))

/* Superblock filesystem feature flags (RW compatible)
 * A filesystem with any of these enabled can be read and written by a driver
 * that does not understand them without causing metadata/data corruption.  */
#define EXT2_FEATURE_COMPAT_DIR_PREALLOC	0x0001
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES	0x0002
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL		0x0004
#define EXT2_FEATURE_COMPAT_EXT_ATTR		0x0008
#define EXT2_FEATURE_COMPAT_RESIZE_INODE	0x0010
#define EXT2_FEATURE_COMPAT_DIR_INDEX		0x0020
/* Superblock filesystem feature flags (RO compatible)
 * A filesystem with any of these enabled can be safely read by a driver that
 * does not understand them, but should not be written to, usually because
 * additional metadata is required.  */
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER	0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE	0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR	0x0004
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM		0x0010
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK	0x0020
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE	0x0040
/* Superblock filesystem feature flags (back-incompatible)
 * A filesystem with any of these enabled should not be attempted to be read
 * by a driver that does not understand them, since they usually indicate
 * metadata format changes that might confuse the reader.  */
#define EXT2_FEATURE_INCOMPAT_COMPRESSION	0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE		0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004 /* Needs recovery */
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008 /* Volume is journal device */
#define EXT2_FEATURE_INCOMPAT_META_BG		0x0010
#define EXT4_FEATURE_INCOMPAT_EXTENTS		0x0040 /* Extents used */
#define EXT4_FEATURE_INCOMPAT_64BIT		0x0080
#define EXT4_FEATURE_INCOMPAT_MMP		0x0100
#define EXT4_FEATURE_INCOMPAT_FLEX_BG		0x0200
#define EXT4_FEATURE_INCOMPAT_CSUM_SEED		0x2000
#define EXT4_FEATURE_INCOMPAT_ENCRYPT          0x10000

/* The set of back-incompatible features this driver DOES support. Add (OR)
 * flags here as the related features are implemented into the driver.  */
#define EXT2_DRIVER_SUPPORTED_INCOMPAT ( EXT2_FEATURE_INCOMPAT_FILETYPE \
                                       | EXT4_FEATURE_INCOMPAT_EXTENTS  \
                                       | EXT4_FEATURE_INCOMPAT_FLEX_BG \
                                       | EXT2_FEATURE_INCOMPAT_META_BG \
                                       | EXT4_FEATURE_INCOMPAT_64BIT \
                                       | EXT4_FEATURE_INCOMPAT_ENCRYPT)
/* List of rationales for the ignored "incompatible" features:
 * needs_recovery: Not really back-incompatible - was added as such to forbid
 *                 ext2 drivers from mounting an ext3 volume with a dirty
 *                 journal because they will ignore the journal, but the next
 *                 ext3 driver to mount the volume will find the journal and
 *                 replay it, potentially corrupting the metadata written by
 *                 the ext2 drivers. Safe to ignore for this RO driver.
 * mmp:            Not really back-incompatible - was added as such to
 *                 avoid multiple read-write mounts. Safe to ignore for this
 *                 RO driver.
 * checksum seed:  Not really back-incompatible - was added to allow tools
 *                 such as tune2fs to change the UUID on a mounted metadata
 *                 checksummed filesystem. Safe to ignore for now since the
 *                 driver doesn't support checksum verification. But it must
 *                 be removed from this list if that support is added later.
 *
 */
#define EXT2_DRIVER_IGNORED_INCOMPAT ( EXT3_FEATURE_INCOMPAT_RECOVER \
				     | EXT4_FEATURE_INCOMPAT_MMP \
				     | EXT4_FEATURE_INCOMPAT_CSUM_SEED)


#define EXT3_JOURNAL_MAGIC_NUMBER	0xc03b3998U

#define EXT3_JOURNAL_DESCRIPTOR_BLOCK	1
#define EXT3_JOURNAL_COMMIT_BLOCK	2
#define EXT3_JOURNAL_SUPERBLOCK_V1	3
#define EXT3_JOURNAL_SUPERBLOCK_V2	4
#define EXT3_JOURNAL_REVOKE_BLOCK	5

#define EXT3_JOURNAL_FLAG_ESCAPE	1
#define EXT3_JOURNAL_FLAG_SAME_UUID	2
#define EXT3_JOURNAL_FLAG_DELETED	4
#define EXT3_JOURNAL_FLAG_LAST_TAG	8

#define EXT4_ENCRYPT_FLAG              0x800
#define EXT4_EXTENTS_FLAG		0x80000

/* The ext2 superblock.  */
struct grub_ext2_sblock
{
  grub_uint32_t total_inodes;
  grub_uint32_t total_blocks;
  grub_uint32_t reserved_blocks;
  grub_uint32_t free_blocks;
  grub_uint32_t free_inodes;
  grub_uint32_t first_data_block;
  grub_uint32_t log2_block_size;
  grub_uint32_t log2_fragment_size;
  grub_uint32_t blocks_per_group;
  grub_uint32_t fragments_per_group;
  grub_uint32_t inodes_per_group;
  grub_uint32_t mtime;
  grub_uint32_t utime;
  grub_uint16_t mnt_count;
  grub_uint16_t max_mnt_count;
  grub_uint16_t magic;
  grub_uint16_t fs_state;
  grub_uint16_t error_handling;
  grub_uint16_t minor_revision_level;
  grub_uint32_t lastcheck;
  grub_uint32_t checkinterval;
  grub_uint32_t creator_os;
  grub_uint32_t revision_level;
  grub_uint16_t uid_reserved;
  grub_uint16_t gid_reserved;
  grub_uint32_t first_inode;
  grub_uint16_t inode_size;
  grub_uint16_t block_group_number;
  grub_uint32_t feature_compatibility;
  grub_uint32_t feature_incompat;
  grub_uint32_t feature_ro_compat;
  grub_uint16_t uuid[8];
  char volume_name[16];
  char last_mounted_on[64];
  grub_uint32_t compression_info;
  grub_uint8_t prealloc_blocks;
  grub_uint8_t prealloc_dir_blocks;
  grub_uint16_t reserved_gdt_blocks;
  grub_uint8_t journal_uuid[16];
  grub_uint32_t journal_inum;
  grub_uint32_t journal_dev;
  grub_uint32_t last_orphan;
  grub_uint32_t hash_seed[4];
  grub_uint8_t def_hash_version;
  grub_uint8_t jnl_backup_type;
  grub_uint16_t group_desc_size;
  grub_uint32_t default_mount_opts;
  grub_uint32_t first_meta_bg;
  grub_uint32_t mkfs_time;
  grub_uint32_t jnl_blocks[17];
};

/* The ext2 blockgroup.  */
struct grub_ext2_block_group
{
  grub_uint32_t block_id;
  grub_uint32_t inode_id;
  grub_uint32_t inode_table_id;
  grub_uint16_t free_blocks;
  grub_uint16_t free_inodes;
  grub_uint16_t used_dirs;
  grub_uint16_t pad;
  grub_uint32_t reserved[3];
  grub_uint32_t block_id_hi;
  grub_uint32_t inode_id_hi;
  grub_uint32_t inode_table_id_hi;
  grub_uint16_t free_blocks_hi;
  grub_uint16_t free_inodes_hi;
  grub_uint16_t used_dirs_hi;
  grub_uint16_t pad2;
  grub_uint32_t reserved2[3];
};

/* The ext2 inode.  */
struct grub_ext2_inode
{
  grub_uint16_t mode;
  grub_uint16_t uid;
  grub_uint32_t size;
  grub_uint32_t atime;
  grub_uint32_t ctime;
  grub_uint32_t mtime;
  grub_uint32_t dtime;
  grub_uint16_t gid;
  grub_uint16_t nlinks;
  grub_uint32_t blockcnt;  /* Blocks of 512 bytes!! */
  grub_uint32_t flags;
  grub_uint32_t osd1;
  union
  {
    struct datablocks
    {
      grub_uint32_t dir_blocks[INDIRECT_BLOCKS];
      grub_uint32_t indir_block;
      grub_uint32_t double_indir_block;
      grub_uint32_t triple_indir_block;
    } blocks;
    char symlink[60];
  };
  grub_uint32_t version;
  grub_uint32_t acl;
  grub_uint32_t size_high;
  grub_uint32_t fragment_addr;
  grub_uint32_t osd2[3];
};

/* The header of an ext2 directory entry.  */
struct ext2_dirent
{
  grub_uint32_t inode;
  grub_uint16_t direntlen;
#define MAX_NAMELEN 255
  grub_uint8_t namelen;
  grub_uint8_t filetype;
};

struct grub_ext3_journal_header
{
  grub_uint32_t magic;
  grub_uint32_t block_type;
  grub_uint32_t sequence;
};

struct grub_ext3_journal_revoke_header
{
  struct grub_ext3_journal_header header;
  grub_uint32_t count;
  grub_uint32_t data[0];
};

struct grub_ext3_journal_block_tag
{
  grub_uint32_t block;
  grub_uint32_t flags;
};

struct grub_ext3_journal_sblock
{
  struct grub_ext3_journal_header header;
  grub_uint32_t block_size;
  grub_uint32_t maxlen;
  grub_uint32_t first;
  grub_uint32_t sequence;
  grub_uint32_t start;
};

#define EXT4_EXT_MAGIC		0xf30a

struct grub_ext4_extent_header
{
  grub_uint16_t magic;
  grub_uint16_t entries;
  grub_uint16_t max;
  grub_uint16_t depth;
  grub_uint32_t generation;
};

struct grub_ext4_extent
{
  grub_uint32_t block;
  grub_uint16_t len;
  grub_uint16_t start_hi;
  grub_uint32_t start;
};

struct grub_ext4_extent_idx
{
  grub_uint32_t block;
  grub_uint32_t leaf;
  grub_uint16_t leaf_hi;
  grub_uint16_t unused;
};

struct grub_fshelp_node
{
  struct grub_ext2_data *data;
  struct grub_ext2_inode inode;
  int ino;
  int inode_read;
};

/* Information about a "mounted" ext2 filesystem.  */
struct grub_ext2_data
{
  struct grub_ext2_sblock sblock;
  int log_group_desc_size;
  grub_disk_t disk;
  struct grub_ext2_inode *inode;
  struct grub_fshelp_node diropen;
};

static grub_dl_t my_mod;

static int g_ventoy_block_count;

/* Check is a = b^x for some x.  */
static inline int
is_power_of (grub_uint64_t a, grub_uint32_t b)
{
  grub_uint64_t c;
  /* Prevent overflow assuming b < 8.  */
  if (a >= (1LL << 60))
    return 0;
  for (c = 1; c <= a; c *= b);
  return (c == a);
}


static inline int
group_has_super_block (struct grub_ext2_data *data, grub_uint64_t group)
{
  if (!(data->sblock.feature_ro_compat
	& grub_cpu_to_le32_compile_time(EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER)))
    return 1;
  /* Algorithm looked up in Linux source.  */
  if (group <= 1)
    return 1;
  /* Even number is never a power of odd number.  */
  if (!(group & 1))
    return 0;
  return (is_power_of(group, 7) || is_power_of(group, 5) ||
	  is_power_of(group, 3));
}

/* Read into BLKGRP the blockgroup descriptor of blockgroup GROUP of
   the mounted filesystem DATA.  */
inline static grub_err_t
grub_ext2_blockgroup (struct grub_ext2_data *data, grub_uint64_t group,
		      struct grub_ext2_block_group *blkgrp)
{
  grub_uint64_t full_offset = (group << data->log_group_desc_size);
  grub_uint64_t block, offset;
  block = (full_offset >> LOG2_BLOCK_SIZE (data));
  offset = (full_offset & ((1 << LOG2_BLOCK_SIZE (data)) - 1));
  if ((data->sblock.feature_incompat
       & grub_cpu_to_le32_compile_time (EXT2_FEATURE_INCOMPAT_META_BG))
      && block >= grub_le_to_cpu32(data->sblock.first_meta_bg))
    {
      grub_uint64_t first_block_group;
      /* Find the first block group for which a descriptor
	 is stored in given block. */
      first_block_group = (block << (LOG2_BLOCK_SIZE (data)
				     - data->log_group_desc_size));

      block = (first_block_group
	       * grub_le_to_cpu32(data->sblock.blocks_per_group));

      if (group_has_super_block (data, first_block_group))
	block++;
    }
  else
    /* Superblock.  */
    block++;
  return grub_disk_read (data->disk,
                         ((grub_le_to_cpu32 (data->sblock.first_data_block)
			   + block)
                          << LOG2_EXT2_BLOCK_SIZE (data)), offset,
			 sizeof (struct grub_ext2_block_group), blkgrp);
}

static struct grub_ext4_extent_header *
grub_ext4_find_leaf (struct grub_ext2_data *data,
                     struct grub_ext4_extent_header *ext_block,
                     grub_uint32_t fileblock)
{
  struct grub_ext4_extent_idx *index;
  void *buf = NULL;

  while (1)
    {
      int i;
      grub_disk_addr_t block;

      index = (struct grub_ext4_extent_idx *) (ext_block + 1);

      if (ext_block->magic != grub_cpu_to_le16_compile_time (EXT4_EXT_MAGIC))
	goto fail;

      if (ext_block->depth == 0)
        return ext_block;

      for (i = 0; i < grub_le_to_cpu16 (ext_block->entries); i++)
        {
          if (fileblock < grub_le_to_cpu32(index[i].block))
            break;
        }

      if (--i < 0)
	goto fail;

      block = grub_le_to_cpu16 (index[i].leaf_hi);
      block = (block << 32) | grub_le_to_cpu32 (index[i].leaf);
      if (!buf)
	buf = grub_malloc (EXT2_BLOCK_SIZE(data));
      if (!buf)
	goto fail;
      if (grub_disk_read (data->disk,
                          block << LOG2_EXT2_BLOCK_SIZE (data),
                          0, EXT2_BLOCK_SIZE(data), buf))
	goto fail;

      ext_block = buf;
    }
 fail:
  grub_free (buf);
  return 0;
}

static grub_disk_addr_t
grub_ext2_read_block (grub_fshelp_node_t node, grub_disk_addr_t fileblock)
{
  struct grub_ext2_data *data = node->data;
  struct grub_ext2_inode *inode = &node->inode;
  unsigned int blksz = EXT2_BLOCK_SIZE (data);
  grub_disk_addr_t blksz_quarter = blksz / 4;
  int log2_blksz = LOG2_EXT2_BLOCK_SIZE (data);
  int log_perblock = log2_blksz + 9 - 2;
  grub_uint32_t indir;
  int shift;

  if (inode->flags & grub_cpu_to_le32_compile_time (EXT4_EXTENTS_FLAG))
    {
      struct grub_ext4_extent_header *leaf;
      struct grub_ext4_extent *ext;
      int i;
      grub_disk_addr_t ret;

      leaf = grub_ext4_find_leaf (data, (struct grub_ext4_extent_header *) inode->blocks.dir_blocks, fileblock);
      if (! leaf)
        {
          grub_error (GRUB_ERR_BAD_FS, "invalid extent");
          return -1;
        }

      ext = (struct grub_ext4_extent *) (leaf + 1);
      for (i = 0; i < grub_le_to_cpu16 (leaf->entries); i++)
        {
          if (fileblock < grub_le_to_cpu32 (ext[i].block))
            break;
        }

      if (--i >= 0)
        {
          fileblock -= grub_le_to_cpu32 (ext[i].block);
          if (fileblock >= grub_le_to_cpu16 (ext[i].len))
	    ret = 0;
          else
            {
              grub_disk_addr_t start;

              start = grub_le_to_cpu16 (ext[i].start_hi);
              start = (start << 32) + grub_le_to_cpu32 (ext[i].start);

              g_ventoy_block_count = (int)(grub_le_to_cpu16 (ext[i].len) - fileblock);
              ret = fileblock + start;
            }
        }
      else
        {
          grub_error (GRUB_ERR_BAD_FS, "something wrong with extent");
	  ret = -1;
        }

      if (leaf != (struct grub_ext4_extent_header *) inode->blocks.dir_blocks)
	grub_free (leaf);

      return ret;
    }

  /* Direct blocks.  */
  if (fileblock < INDIRECT_BLOCKS)
    return grub_le_to_cpu32 (inode->blocks.dir_blocks[fileblock]);
  fileblock -= INDIRECT_BLOCKS;
  /* Indirect.  */
  if (fileblock < blksz_quarter)
    {
      indir = inode->blocks.indir_block;
      shift = 0;
      goto indirect;
    }
  fileblock -= blksz_quarter;
  /* Double indirect.  */
  if (fileblock < blksz_quarter * blksz_quarter)
    {
      indir = inode->blocks.double_indir_block;
      shift = 1;
      goto indirect;
    }
  fileblock -= blksz_quarter * blksz_quarter;
  /* Triple indirect.  */
  if (fileblock < blksz_quarter * blksz_quarter * (blksz_quarter + 1))
    {
      indir = inode->blocks.triple_indir_block;
      shift = 2;
      goto indirect;
    }
  grub_error (GRUB_ERR_BAD_FS,
	      "ext2fs doesn't support quadruple indirect blocks");
  return -1;

indirect:
  do {
    /* If the indirect block is zero, all child blocks are absent
       (i.e. filled with zeros.) */
    if (indir == 0)
      return 0;
    if (grub_disk_read (data->disk,
			((grub_disk_addr_t) grub_le_to_cpu32 (indir))
			<< log2_blksz,
			((fileblock >> (log_perblock * shift))
			 & ((1 << log_perblock) - 1))
			* sizeof (indir),
			sizeof (indir), &indir))
      return -1;
  } while (shift--);

  return grub_le_to_cpu32 (indir);
}

/* Read LEN bytes from the file described by DATA starting with byte
   POS.  Return the amount of read bytes in READ.  */
static grub_ssize_t
grub_ext2_read_file (grub_fshelp_node_t node,
		     grub_disk_read_hook_t read_hook, void *read_hook_data,
		     grub_off_t pos, grub_size_t len, char *buf)
{
  return grub_fshelp_read_file (node->data->disk, node,
				read_hook, read_hook_data,
				pos, len, buf, grub_ext2_read_block,
				grub_cpu_to_le32 (node->inode.size)
				| (((grub_off_t) grub_cpu_to_le32 (node->inode.size_high)) << 32),
				LOG2_EXT2_BLOCK_SIZE (node->data), 0);

}


/* Read the inode INO for the file described by DATA into INODE.  */
static grub_err_t
grub_ext2_read_inode (struct grub_ext2_data *data,
		      int ino, struct grub_ext2_inode *inode)
{
  struct grub_ext2_block_group blkgrp;
  struct grub_ext2_sblock *sblock = &data->sblock;
  int inodes_per_block;
  unsigned int blkno;
  unsigned int blkoff;
  grub_disk_addr_t base;

  /* It is easier to calculate if the first inode is 0.  */
  ino--;

  grub_ext2_blockgroup (data,
                        ino / grub_le_to_cpu32 (sblock->inodes_per_group),
			&blkgrp);
  if (grub_errno)
    return grub_errno;

  inodes_per_block = EXT2_BLOCK_SIZE (data) / EXT2_INODE_SIZE (data);
  blkno = (ino % grub_le_to_cpu32 (sblock->inodes_per_group))
    / inodes_per_block;
  blkoff = (ino % grub_le_to_cpu32 (sblock->inodes_per_group))
    % inodes_per_block;

  base = grub_le_to_cpu32 (blkgrp.inode_table_id);
  if (data->log_group_desc_size >= 6)
    base |= (((grub_disk_addr_t) grub_le_to_cpu32 (blkgrp.inode_table_id_hi))
	     << 32);

  /* Read the inode.  */
  if (grub_disk_read (data->disk,
		      ((base + blkno) << LOG2_EXT2_BLOCK_SIZE (data)),
		      EXT2_INODE_SIZE (data) * blkoff,
		      sizeof (struct grub_ext2_inode), inode))
    return grub_errno;

  return 0;
}

static struct grub_ext2_data *
grub_ext2_mount (grub_disk_t disk)
{
  struct grub_ext2_data *data;

  data = grub_malloc (sizeof (struct grub_ext2_data));
  if (!data)
    return 0;

  /* Read the superblock.  */
  grub_disk_read (disk, 1 * 2, 0, sizeof (struct grub_ext2_sblock),
                  &data->sblock);
  if (grub_errno)
    goto fail;

  /* Make sure this is an ext2 filesystem.  */
  if (data->sblock.magic != grub_cpu_to_le16_compile_time (EXT2_MAGIC)
      || grub_le_to_cpu32 (data->sblock.log2_block_size) >= 16
      || data->sblock.inodes_per_group == 0
      /* 20 already means 1GiB blocks. We don't want to deal with blocks overflowing int32. */
      || grub_le_to_cpu32 (data->sblock.log2_block_size) > 20
      || EXT2_INODE_SIZE (data) == 0
      || EXT2_BLOCK_SIZE (data) / EXT2_INODE_SIZE (data) == 0)
    {
      grub_error (GRUB_ERR_BAD_FS, "not an ext2 filesystem");
      goto fail;
    }

  /* Check the FS doesn't have feature bits enabled that we don't support. */
  if (data->sblock.revision_level != grub_cpu_to_le32_compile_time (EXT2_GOOD_OLD_REVISION)
      && (data->sblock.feature_incompat
	  & grub_cpu_to_le32_compile_time (~(EXT2_DRIVER_SUPPORTED_INCOMPAT
					     | EXT2_DRIVER_IGNORED_INCOMPAT))))
    {
      grub_error (GRUB_ERR_BAD_FS, "filesystem has unsupported incompatible features");
      goto fail;
    }

  if (data->sblock.revision_level != grub_cpu_to_le32_compile_time (EXT2_GOOD_OLD_REVISION)
      && (data->sblock.feature_incompat
	  & grub_cpu_to_le32_compile_time (EXT4_FEATURE_INCOMPAT_64BIT))
      && data->sblock.group_desc_size != 0
      && ((data->sblock.group_desc_size & (data->sblock.group_desc_size - 1))
	  == 0)
      && (data->sblock.group_desc_size & grub_cpu_to_le16_compile_time (0x1fe0)))
    {
      grub_uint16_t b = grub_le_to_cpu16 (data->sblock.group_desc_size);
      for (data->log_group_desc_size = 0; b != (1 << data->log_group_desc_size);
	   data->log_group_desc_size++);
    }
  else
    data->log_group_desc_size = 5;

  data->disk = disk;

  data->diropen.data = data;
  data->diropen.ino = 2;
  data->diropen.inode_read = 1;

  data->inode = &data->diropen.inode;

  grub_ext2_read_inode (data, 2, data->inode);
  if (grub_errno)
    goto fail;

  return data;

 fail:
  if (grub_errno == GRUB_ERR_OUT_OF_RANGE)
    grub_error (GRUB_ERR_BAD_FS, "not an ext2 filesystem");

  grub_free (data);
  return 0;
}

static char *
grub_ext2_read_symlink (grub_fshelp_node_t node)
{
  char *symlink;
  struct grub_fshelp_node *diro = node;

  if (! diro->inode_read)
    {
      grub_ext2_read_inode (diro->data, diro->ino, &diro->inode);
      if (grub_errno)
	return 0;

      if (diro->inode.flags & grub_cpu_to_le32_compile_time (EXT4_ENCRYPT_FLAG))
       {
         grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET, "symlink is encrypted");
         return 0;
       }
    }

  symlink = grub_malloc (grub_le_to_cpu32 (diro->inode.size) + 1);
  if (! symlink)
    return 0;

  /*
   * If the filesize of the symlink is equal to or bigger than 60 the symlink
   * is stored in a separate block, otherwise it is stored in the inode.
   */
  if (grub_le_to_cpu32 (diro->inode.size) < sizeof (diro->inode.symlink))
    grub_memcpy (symlink,
		 diro->inode.symlink,
		 grub_le_to_cpu32 (diro->inode.size));
  else
    {
      grub_ext2_read_file (diro, 0, 0, 0,
			   grub_le_to_cpu32 (diro->inode.size),
			   symlink);
      if (grub_errno)
	{
	  grub_free (symlink);
	  return 0;
	}
    }

  symlink[grub_le_to_cpu32 (diro->inode.size)] = '\0';
  return symlink;
}

static int
grub_ext2_iterate_dir (grub_fshelp_node_t dir,
		       grub_fshelp_iterate_dir_hook_t hook, void *hook_data)
{
  unsigned int fpos = 0;
  struct grub_fshelp_node *diro = (struct grub_fshelp_node *) dir;

  if (! diro->inode_read)
    {
      grub_ext2_read_inode (diro->data, diro->ino, &diro->inode);
      if (grub_errno)
	return 0;
    }

  if (diro->inode.flags & grub_cpu_to_le32_compile_time (EXT4_ENCRYPT_FLAG))
    {
      grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET, "directory is encrypted");
      return 0;
    }

  /* Search the file.  */
  while (fpos < grub_le_to_cpu32 (diro->inode.size))
    {
      struct ext2_dirent dirent;

      grub_ext2_read_file (diro, 0, 0, fpos, sizeof (struct ext2_dirent),
			   (char *) &dirent);
      if (grub_errno)
	return 0;

      if (dirent.direntlen == 0)
        return 0;

      if (dirent.inode != 0 && dirent.namelen != 0)
	{
	  char filename[MAX_NAMELEN + 1];
	  struct grub_fshelp_node *fdiro;
	  enum grub_fshelp_filetype type = GRUB_FSHELP_UNKNOWN;

	  grub_ext2_read_file (diro, 0, 0, fpos + sizeof (struct ext2_dirent),
			       dirent.namelen, filename);
	  if (grub_errno)
	    return 0;

	  fdiro = grub_malloc (sizeof (struct grub_fshelp_node));
	  if (! fdiro)
	    return 0;

	  fdiro->data = diro->data;
	  fdiro->ino = grub_le_to_cpu32 (dirent.inode);

	  filename[dirent.namelen] = '\0';

	  if (dirent.filetype != FILETYPE_UNKNOWN)
	    {
	      fdiro->inode_read = 0;

	      if (dirent.filetype == FILETYPE_DIRECTORY)
		type = GRUB_FSHELP_DIR;
	      else if (dirent.filetype == FILETYPE_SYMLINK)
		type = GRUB_FSHELP_SYMLINK;
	      else if (dirent.filetype == FILETYPE_REG)
		type = GRUB_FSHELP_REG;
	    }
	  else
	    {
	      /* The filetype can not be read from the dirent, read
		 the inode to get more information.  */
	      grub_ext2_read_inode (diro->data,
                                    grub_le_to_cpu32 (dirent.inode),
				    &fdiro->inode);
	      if (grub_errno)
		{
		  grub_free (fdiro);
		  return 0;
		}

	      fdiro->inode_read = 1;

	      if ((grub_le_to_cpu16 (fdiro->inode.mode)
		   & FILETYPE_INO_MASK) == FILETYPE_INO_DIRECTORY)
		type = GRUB_FSHELP_DIR;
	      else if ((grub_le_to_cpu16 (fdiro->inode.mode)
			& FILETYPE_INO_MASK) == FILETYPE_INO_SYMLINK)
		type = GRUB_FSHELP_SYMLINK;
	      else if ((grub_le_to_cpu16 (fdiro->inode.mode)
			& FILETYPE_INO_MASK) == FILETYPE_INO_REG)
		type = GRUB_FSHELP_REG;
	    }

	  if (hook (filename, type, fdiro, hook_data))
	    return 1;
	}

      fpos += grub_le_to_cpu16 (dirent.direntlen);
    }

  return 0;
}

/* Open a file named NAME and initialize FILE.  */
static grub_err_t
grub_ext2_open (struct grub_file *file, const char *name)
{
  struct grub_ext2_data *data;
  struct grub_fshelp_node *fdiro = 0;
  grub_err_t err;

  grub_dl_ref (my_mod);

  data = grub_ext2_mount (file->device->disk);
  if (! data)
    {
      err = grub_errno;
      goto fail;
    }

  err = grub_fshelp_find_file (name, &data->diropen, &fdiro,
			       grub_ext2_iterate_dir,
			       grub_ext2_read_symlink, GRUB_FSHELP_REG);
  if (err)
    goto fail;

  if (! fdiro->inode_read)
    {
      err = grub_ext2_read_inode (data, fdiro->ino, &fdiro->inode);
      if (err)
	goto fail;
    }

  if (fdiro->inode.flags & grub_cpu_to_le32_compile_time (EXT4_ENCRYPT_FLAG))
    {
      err = grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET, "file is encrypted");
      goto fail;
    }

  grub_memcpy (data->inode, &fdiro->inode, sizeof (struct grub_ext2_inode));
  grub_free (fdiro);

  file->size = grub_le_to_cpu32 (data->inode->size);
  file->size |= ((grub_off_t) grub_le_to_cpu32 (data->inode->size_high)) << 32;
  file->data = data;
  file->offset = 0;

  return 0;

 fail:
  if (fdiro != &data->diropen)
    grub_free (fdiro);
  grub_free (data);

  grub_dl_unref (my_mod);

  return err;
}

static grub_err_t
grub_ext2_close (grub_file_t file)
{
  grub_free (file->data);

  grub_dl_unref (my_mod);

  return GRUB_ERR_NONE;
}

/* Read LEN bytes data from FILE into BUF.  */
static grub_ssize_t
grub_ext2_read (grub_file_t file, char *buf, grub_size_t len)
{
  struct grub_ext2_data *data = (struct grub_ext2_data *) file->data;

  return grub_ext2_read_file (&data->diropen,
			      file->read_hook, file->read_hook_data,
			      file->offset, len, buf);
}


/* Context for grub_ext2_dir.  */
struct grub_ext2_dir_ctx
{
  grub_fs_dir_hook_t hook;
  void *hook_data;
  struct grub_ext2_data *data;
};

/* Helper for grub_ext2_dir.  */
static int
grub_ext2_dir_iter (const char *filename, enum grub_fshelp_filetype filetype,
		    grub_fshelp_node_t node, void *data)
{
  struct grub_ext2_dir_ctx *ctx = data;
  struct grub_dirhook_info info;

  grub_memset (&info, 0, sizeof (info));
  if (! node->inode_read)
    {
      grub_ext2_read_inode (ctx->data, node->ino, &node->inode);
      if (!grub_errno)
	node->inode_read = 1;
      grub_errno = GRUB_ERR_NONE;
    }
  if (node->inode_read)
    {
      info.mtimeset = 1;
      info.mtime = grub_le_to_cpu32 (node->inode.mtime);
    }

  info.dir = ((filetype & GRUB_FSHELP_TYPE_MASK) == GRUB_FSHELP_DIR);
  if (!info.dir)
    info.size = (((grub_off_t) grub_le_to_cpu32 (node->inode.size_high)) << 32) | grub_le_to_cpu32 (node->inode.size);
  grub_free (node);
  return ctx->hook (filename, &info, ctx->hook_data);
}

static grub_err_t
grub_ext2_dir (grub_device_t device, const char *path, grub_fs_dir_hook_t hook,
	       void *hook_data)
{
  struct grub_ext2_dir_ctx ctx = {
    .hook = hook,
    .hook_data = hook_data
  };
  struct grub_fshelp_node *fdiro = 0;

  grub_dl_ref (my_mod);

  ctx.data = grub_ext2_mount (device->disk);
  if (! ctx.data)
    goto fail;

  grub_fshelp_find_file (path, &ctx.data->diropen, &fdiro,
			 grub_ext2_iterate_dir, grub_ext2_read_symlink,
			 GRUB_FSHELP_DIR);
  if (grub_errno)
    goto fail;

  grub_ext2_iterate_dir (fdiro, grub_ext2_dir_iter, &ctx);

 fail:
  if (fdiro != &ctx.data->diropen)
    grub_free (fdiro);
  grub_free (ctx.data);

  grub_dl_unref (my_mod);

  return grub_errno;
}

static grub_err_t
grub_ext2_label (grub_device_t device, char **label)
{
  struct grub_ext2_data *data;
  grub_disk_t disk = device->disk;

  grub_dl_ref (my_mod);

  data = grub_ext2_mount (disk);
  if (data)
    *label = grub_strndup (data->sblock.volume_name,
			   sizeof (data->sblock.volume_name));
  else
    *label = NULL;

  grub_dl_unref (my_mod);

  grub_free (data);

  return grub_errno;
}

static grub_err_t
grub_ext2_uuid (grub_device_t device, char **uuid)
{
  struct grub_ext2_data *data;
  grub_disk_t disk = device->disk;

  grub_dl_ref (my_mod);

  data = grub_ext2_mount (disk);
  if (data)
    {
      *uuid = grub_xasprintf ("%04x%04x-%04x-%04x-%04x-%04x%04x%04x",
			     grub_be_to_cpu16 (data->sblock.uuid[0]),
			     grub_be_to_cpu16 (data->sblock.uuid[1]),
			     grub_be_to_cpu16 (data->sblock.uuid[2]),
			     grub_be_to_cpu16 (data->sblock.uuid[3]),
			     grub_be_to_cpu16 (data->sblock.uuid[4]),
			     grub_be_to_cpu16 (data->sblock.uuid[5]),
			     grub_be_to_cpu16 (data->sblock.uuid[6]),
			     grub_be_to_cpu16 (data->sblock.uuid[7]));
    }
  else
    *uuid = NULL;

  grub_dl_unref (my_mod);

  grub_free (data);

  return grub_errno;
}

/* Get mtime.  */
static grub_err_t
grub_ext2_mtime (grub_device_t device, grub_int32_t *tm)
{
  struct grub_ext2_data *data;
  grub_disk_t disk = device->disk;

  grub_dl_ref (my_mod);

  data = grub_ext2_mount (disk);
  if (!data)
    *tm = 0;
  else
    *tm = grub_le_to_cpu32 (data->sblock.utime);

  grub_dl_unref (my_mod);

  grub_free (data);

  return grub_errno;

}

int grub_ext_get_file_chunk(grub_uint64_t part_start, grub_file_t file, ventoy_img_chunk_list *chunk_list)
{
    int blocksize;
    int log2blocksize;
    grub_disk_t disk;
    grub_disk_addr_t i = 0;
    grub_disk_addr_t blockcnt;
    grub_disk_addr_t blknr;
    grub_fshelp_node_t node = NULL;

    disk = file->device->disk;
    node = &(((struct grub_ext2_data *)file->data)->diropen);

    log2blocksize = LOG2_EXT2_BLOCK_SIZE (node->data);
    blocksize = 1 << (log2blocksize + GRUB_DISK_SECTOR_BITS);
    blockcnt = (file->size + blocksize - 1) >> (log2blocksize + GRUB_DISK_SECTOR_BITS);

    while (i < blockcnt)
    {
        g_ventoy_block_count = 1;
        blknr = grub_ext2_read_block(node, i);
        if (blknr == 0)
        {
            return 0;
        }
        
        i += g_ventoy_block_count;        
        blknr = blknr << log2blocksize;
        grub_disk_blocklist_read(chunk_list, blknr, g_ventoy_block_count * blocksize, disk->log_sector_size);
    }

    for (i = 0; i < chunk_list->cur_chunk; i++)
    {
        chunk_list->chunk[i].disk_start_sector += part_start;
        chunk_list->chunk[i].disk_end_sector += part_start;
    }

    return 0;
}

static struct grub_fs grub_ext2_fs =
  {
    .name = "ext2",
    .fs_dir = grub_ext2_dir,
    .fs_open = grub_ext2_open,
    .fs_read = grub_ext2_read,
    .fs_close = grub_ext2_close,
    .fs_label = grub_ext2_label,
    .fs_uuid = grub_ext2_uuid,
    .fs_mtime = grub_ext2_mtime,
#ifdef GRUB_UTIL
    .reserved_first_sector = 1,
    .blocklist_install = 1,
#endif
    .next = 0
  };

GRUB_MOD_INIT(ext2)
{
  grub_fs_register (&grub_ext2_fs);
  my_mod = mod;
}

GRUB_MOD_FINI(ext2)
{
  grub_fs_unregister (&grub_ext2_fs);
}
