/* xfs.c - XFS.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2005,2006,2007,2008,2009  Free Software Foundation, Inc.
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

#include <grub/err.h>
#include <grub/file.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/disk.h>
#include <grub/dl.h>
#include <grub/time.h>
#include <grub/types.h>
#include <grub/fshelp.h>

GRUB_MOD_LICENSE ("GPLv3+");

#define NSEC_PER_SEC ((grub_int64_t) 1000000000)

// GRUB 2.04 doesn't have safemath.h
// #include <grub/safemath.h>

// gcc < 5.1 doesn't support __builtin_add_overflow and __builtin_mul_overflow
// #define grub_add(a, b, res) __builtin_add_overflow(a, b, res)
// #define grub_mul(a, b, res) __builtin_mul_overflow(a, b, res)
// Warning: This is unsafe!
#define grub_add(a, b, res) ({ *(res) = (a) + (b); 0; })

#define grub_mul(a, b, res) ({ *(res) = (a) * (b); 0; })

#define	XFS_INODE_EXTENTS	9

#define XFS_INODE_FORMAT_INO	1
#define XFS_INODE_FORMAT_EXT	2
#define XFS_INODE_FORMAT_BTREE	3

/* Superblock version field flags */
#define XFS_SB_VERSION_NUMBITS		0x000f
#define	XFS_SB_VERSION_ATTRBIT		0x0010
#define	XFS_SB_VERSION_NLINKBIT		0x0020
#define	XFS_SB_VERSION_QUOTABIT		0x0040
#define	XFS_SB_VERSION_ALIGNBIT		0x0080
#define	XFS_SB_VERSION_DALIGNBIT	0x0100
#define	XFS_SB_VERSION_LOGV2BIT		0x0400
#define	XFS_SB_VERSION_SECTORBIT	0x0800
#define	XFS_SB_VERSION_EXTFLGBIT	0x1000
#define	XFS_SB_VERSION_DIRV2BIT		0x2000
#define XFS_SB_VERSION_MOREBITSBIT	0x8000
#define XFS_SB_VERSION_BITS_SUPPORTED \
	(XFS_SB_VERSION_NUMBITS | \
	 XFS_SB_VERSION_ATTRBIT | \
	 XFS_SB_VERSION_NLINKBIT | \
	 XFS_SB_VERSION_QUOTABIT | \
	 XFS_SB_VERSION_ALIGNBIT | \
	 XFS_SB_VERSION_DALIGNBIT | \
	 XFS_SB_VERSION_LOGV2BIT | \
	 XFS_SB_VERSION_SECTORBIT | \
	 XFS_SB_VERSION_EXTFLGBIT | \
	 XFS_SB_VERSION_DIRV2BIT | \
	 XFS_SB_VERSION_MOREBITSBIT)

/* Recognized xfs format versions */
#define XFS_SB_VERSION_4		4	/* Good old XFS filesystem */
#define XFS_SB_VERSION_5		5	/* CRC enabled filesystem */

/* features2 field flags */
#define XFS_SB_VERSION2_LAZYSBCOUNTBIT	0x00000002	/* Superblk counters */
#define XFS_SB_VERSION2_ATTR2BIT	0x00000008	/* Inline attr rework */
#define XFS_SB_VERSION2_PROJID32BIT	0x00000080	/* 32-bit project ids */
#define XFS_SB_VERSION2_FTYPE		0x00000200	/* inode type in dir */
#define XFS_SB_VERSION2_BITS_SUPPORTED \
	(XFS_SB_VERSION2_LAZYSBCOUNTBIT | \
	 XFS_SB_VERSION2_ATTR2BIT | \
	 XFS_SB_VERSION2_PROJID32BIT | \
	 XFS_SB_VERSION2_FTYPE)

/* Inode flags2 flags */
#define XFS_DIFLAG2_BIGTIME_BIT	3
#define XFS_DIFLAG2_BIGTIME		(1 << XFS_DIFLAG2_BIGTIME_BIT)
#define XFS_DIFLAG2_NREXT64_BIT	4
#define XFS_DIFLAG2_NREXT64		(1 << XFS_DIFLAG2_NREXT64_BIT)

/* incompat feature flags */
#define XFS_SB_FEAT_INCOMPAT_FTYPE      (1 << 0)        /* filetype in dirent */
#define XFS_SB_FEAT_INCOMPAT_SPINODES   (1 << 1)        /* sparse inode chunks */
#define XFS_SB_FEAT_INCOMPAT_META_UUID  (1 << 2)        /* metadata UUID */
#define XFS_SB_FEAT_INCOMPAT_BIGTIME    (1 << 3)        /* large timestamps */
#define XFS_SB_FEAT_INCOMPAT_NEEDSREPAIR (1 << 4)       /* needs xfs_repair */
#define XFS_SB_FEAT_INCOMPAT_NREXT64    (1 << 5)        /* large extent counters */
#define XFS_SB_FEAT_INCOMPAT_EXCHRANGE  (1 << 6)        /* exchangerange supported */
#define XFS_SB_FEAT_INCOMPAT_PARENT     (1 << 7)        /* parent pointers */
#define XFS_SB_FEAT_INCOMPAT_METADIR    (1 << 8)        /* metadata dir tree */

/*
 * Directory entries with ftype are explicitly handled by GRUB code.
 *
 * We do not currently read the inode btrees, so it is safe to read filesystems
 * with the XFS_SB_FEAT_INCOMPAT_SPINODES feature.
 *
 * We do not currently verify metadata UUID, so it is safe to read filesystems
 * with the XFS_SB_FEAT_INCOMPAT_META_UUID feature.
 *
 * We do not currently replay the log, so it is safe to read filesystems
 * with the XFS_SB_FEAT_INCOMPAT_EXCHRANGE feature.
 *
 * We do not currently read directory parent pointers, so it is safe to read
 * filesystems with the XFS_SB_FEAT_INCOMPAT_PARENT feature.
 *
 * We do not currently look at realtime or quota metadata, so it is safe to
 * read filesystems with the XFS_SB_FEAT_INCOMPAT_METADIR feature.
 */
#define XFS_SB_FEAT_INCOMPAT_SUPPORTED \
	(XFS_SB_FEAT_INCOMPAT_FTYPE | \
	 XFS_SB_FEAT_INCOMPAT_SPINODES | \
	 XFS_SB_FEAT_INCOMPAT_META_UUID | \
	 XFS_SB_FEAT_INCOMPAT_BIGTIME | \
	 XFS_SB_FEAT_INCOMPAT_NEEDSREPAIR | \
	 XFS_SB_FEAT_INCOMPAT_NREXT64 | \
	 XFS_SB_FEAT_INCOMPAT_EXCHRANGE | \
	 XFS_SB_FEAT_INCOMPAT_PARENT | \
	 XFS_SB_FEAT_INCOMPAT_METADIR)

struct grub_xfs_sblock
{
  grub_uint8_t magic[4];
  grub_uint32_t bsize;
  grub_uint8_t unused1[24];
  grub_uint16_t uuid[8];
  grub_uint8_t unused2[8];
  grub_uint64_t rootino;
  grub_uint8_t unused3[20];
  grub_uint32_t agsize;
  grub_uint8_t unused4[12];
  grub_uint16_t version;
  grub_uint8_t unused5[6];
  grub_uint8_t label[12];
  grub_uint8_t log2_bsize;
  grub_uint8_t log2_sect;
  grub_uint8_t log2_inode;
  grub_uint8_t log2_inop;
  grub_uint8_t log2_agblk;
  grub_uint8_t unused6[67];
  grub_uint8_t log2_dirblk;
  grub_uint8_t unused7[7];
  grub_uint32_t features2;
  grub_uint8_t unused8[4];
  grub_uint32_t sb_features_compat;
  grub_uint32_t sb_features_ro_compat;
  grub_uint32_t sb_features_incompat;
  grub_uint32_t sb_features_log_incompat;
} GRUB_PACKED;

struct grub_xfs_dir_header
{
  grub_uint8_t count;
  grub_uint8_t largeino;
  union
  {
    grub_uint32_t i4;
    grub_uint64_t i8;
  } GRUB_PACKED parent;
} GRUB_PACKED;

/* Structure for directory entry inlined in the inode */
struct grub_xfs_dir_entry
{
  grub_uint8_t len;
  grub_uint16_t offset;
  char name[1];
  /* Inode number follows, 32 / 64 bits.  */
} GRUB_PACKED;

/* Structure for directory entry in a block */
struct grub_xfs_dir2_entry
{
  grub_uint64_t inode;
  grub_uint8_t len;
} GRUB_PACKED;

struct grub_xfs_extent
{
  /* This should be a bitfield but bietfields are unportable, so just have
     a raw array and functions extracting useful info from it.
   */
  grub_uint32_t raw[4];
} GRUB_PACKED;

struct grub_xfs_btree_node
{
  grub_uint8_t magic[4];
  grub_uint16_t level;
  grub_uint16_t numrecs;
  grub_uint64_t left;
  grub_uint64_t right;
  /* In V5 here follow crc, uuid, etc. */
  /* Then follow keys and block pointers */
} GRUB_PACKED;

struct grub_xfs_btree_root
{
  grub_uint16_t level;
  grub_uint16_t numrecs;
  grub_uint64_t keys[1];
} GRUB_PACKED;

struct grub_xfs_time_legacy
{
  grub_uint32_t sec;
  grub_uint32_t nanosec;
} GRUB_PACKED;

/*
 * The struct grub_xfs_inode layout was taken from the
 * struct xfs_dinode_core which is described here:
 * https://mirrors.edge.kernel.org/pub/linux/utils/fs/xfs/docs/xfs_filesystem_structure.pdf
 */
struct grub_xfs_inode
{
  grub_uint8_t magic[2];
  grub_uint16_t mode;
  grub_uint8_t version;
  grub_uint8_t format;
  grub_uint8_t unused2[18];
  grub_uint64_t nextents_big;
  grub_uint64_t atime;
  grub_uint64_t mtime;
  grub_uint64_t ctime;
  grub_uint64_t size;
  grub_uint64_t nblocks;
  grub_uint32_t extsize;
  grub_uint32_t nextents;
  grub_uint16_t unused3;
  grub_uint8_t fork_offset;
  grub_uint8_t unused4[17]; /* Last member of inode v2. */
  grub_uint8_t unused5[20]; /* First member of inode v3. */
  grub_uint64_t flags2;
  grub_uint8_t unused6[48]; /* Last member of inode v3. */
} GRUB_PACKED;

#define XFS_V3_INODE_SIZE	sizeof(struct grub_xfs_inode)
/* Size of struct grub_xfs_inode v2, up to unused4 member included. */
#define XFS_V2_INODE_SIZE	(XFS_V3_INODE_SIZE - 76)

struct grub_xfs_dir_leaf_entry
{
  grub_uint32_t hashval;
  grub_uint32_t address;
} GRUB_PACKED;

struct grub_xfs_dirblock_tail
{
  grub_uint32_t leaf_count;
  grub_uint32_t leaf_stale;
} GRUB_PACKED;

struct grub_fshelp_node
{
  struct grub_xfs_data *data;
  grub_uint64_t ino;
  int inode_read;
  struct grub_xfs_inode inode;
};

struct grub_xfs_data
{
  grub_size_t data_size;
  struct grub_xfs_sblock sblock;
  grub_disk_t disk;
  int pos;
  int bsize;
  grub_uint32_t agsize;
  unsigned int hasftype:1;
  unsigned int hascrc:1;
  struct grub_fshelp_node diropen;
};

static grub_dl_t my_mod;

static int grub_xfs_sb_hascrc(struct grub_xfs_data *data)
{
  return (data->sblock.version & grub_cpu_to_be16_compile_time(XFS_SB_VERSION_NUMBITS)) ==
	  grub_cpu_to_be16_compile_time(XFS_SB_VERSION_5);
}

static int grub_xfs_sb_hasftype(struct grub_xfs_data *data)
{
  if ((data->sblock.version & grub_cpu_to_be16_compile_time(XFS_SB_VERSION_NUMBITS)) ==
	grub_cpu_to_be16_compile_time(XFS_SB_VERSION_5) &&
      data->sblock.sb_features_incompat & grub_cpu_to_be32_compile_time(XFS_SB_FEAT_INCOMPAT_FTYPE))
    return 1;
  if (data->sblock.version & grub_cpu_to_be16_compile_time(XFS_SB_VERSION_MOREBITSBIT) &&
      data->sblock.features2 & grub_cpu_to_be32_compile_time(XFS_SB_VERSION2_FTYPE))
    return 1;
  return 0;
}

static int grub_xfs_sb_valid(struct grub_xfs_data *data)
{
  grub_dprintf("xfs", "Validating superblock\n");
  if (grub_strncmp ((char *) (data->sblock.magic), "XFSB", 4)
      || data->sblock.log2_bsize < GRUB_DISK_SECTOR_BITS
      || ((int) data->sblock.log2_bsize
	  + (int) data->sblock.log2_dirblk) >= 27)
    {
      grub_error (GRUB_ERR_BAD_FS, "not a XFS filesystem");
      return 0;
    }
  if ((data->sblock.version & grub_cpu_to_be16_compile_time(XFS_SB_VERSION_NUMBITS)) ==
       grub_cpu_to_be16_compile_time(XFS_SB_VERSION_5))
    {
      grub_dprintf("xfs", "XFS v5 superblock detected\n");
      if (data->sblock.sb_features_incompat &
          grub_cpu_to_be32_compile_time(~XFS_SB_FEAT_INCOMPAT_SUPPORTED))
        {
	  grub_error (GRUB_ERR_BAD_FS, "XFS filesystem has unsupported "
		      "incompatible features");
	  return 0;
        }
      return 1;
    }
  else if ((data->sblock.version & grub_cpu_to_be16_compile_time(XFS_SB_VERSION_NUMBITS)) ==
	   grub_cpu_to_be16_compile_time(XFS_SB_VERSION_4))
    {
      grub_dprintf("xfs", "XFS v4 superblock detected\n");
      if (!(data->sblock.version & grub_cpu_to_be16_compile_time(XFS_SB_VERSION_DIRV2BIT)))
	{
	  grub_error (GRUB_ERR_BAD_FS, "XFS filesystem without V2 directories "
		      "is unsupported");
	  return 0;
	}
      if (data->sblock.version & grub_cpu_to_be16_compile_time(~XFS_SB_VERSION_BITS_SUPPORTED) ||
	  (data->sblock.version & grub_cpu_to_be16_compile_time(XFS_SB_VERSION_MOREBITSBIT) &&
	   data->sblock.features2 & grub_cpu_to_be16_compile_time(~XFS_SB_VERSION2_BITS_SUPPORTED)))
	{
	  grub_error (GRUB_ERR_BAD_FS, "XFS filesystem has unsupported version "
		      "bits");
	  return 0;
	}
      return 1;
    }

  grub_error (GRUB_ERR_BAD_FS, "unsupported XFS filesystem version");
  return 0;
}

static int
grub_xfs_sb_needs_repair (struct grub_xfs_data *data)
{
  return ((data->sblock.version &
           grub_cpu_to_be16_compile_time (XFS_SB_VERSION_NUMBITS)) ==
          grub_cpu_to_be16_compile_time (XFS_SB_VERSION_5) &&
          (data->sblock.sb_features_incompat &
           grub_cpu_to_be32_compile_time (XFS_SB_FEAT_INCOMPAT_NEEDSREPAIR)));
}

/* Filetype information as used in inodes.  */
#define FILETYPE_INO_MASK	0170000
#define FILETYPE_INO_REG	0100000
#define FILETYPE_INO_DIRECTORY	0040000
#define FILETYPE_INO_SYMLINK	0120000

static inline int
GRUB_XFS_INO_AGBITS(struct grub_xfs_data *data)
{
  return ((data)->sblock.log2_agblk + (data)->sblock.log2_inop);
}

static inline grub_uint64_t
GRUB_XFS_INO_INOINAG (struct grub_xfs_data *data,
		      grub_uint64_t ino)
{
  return (ino & ((1LL << GRUB_XFS_INO_AGBITS (data)) - 1));
}

static inline grub_uint64_t
GRUB_XFS_INO_AG (struct grub_xfs_data *data,
		 grub_uint64_t ino)
{
  return (ino >> GRUB_XFS_INO_AGBITS (data));
}

static inline grub_disk_addr_t
GRUB_XFS_FSB_TO_BLOCK (struct grub_xfs_data *data, grub_disk_addr_t fsb)
{
  return ((fsb >> data->sblock.log2_agblk) * data->agsize
	  + (fsb & ((1LL << data->sblock.log2_agblk) - 1)));
}

static inline grub_uint64_t
GRUB_XFS_EXTENT_OFFSET (struct grub_xfs_extent *exts, int ex)
{
  return ((grub_be_to_cpu32 (exts[ex].raw[0]) & ~(1 << 31)) << 23
	  | grub_be_to_cpu32 (exts[ex].raw[1]) >> 9);
}

static inline grub_uint64_t
GRUB_XFS_EXTENT_BLOCK (struct grub_xfs_extent *exts, int ex)
{
  return ((grub_uint64_t) (grub_be_to_cpu32 (exts[ex].raw[1])
			   & (0x1ff)) << 43
	  | (grub_uint64_t) grub_be_to_cpu32 (exts[ex].raw[2]) << 11
	  | grub_be_to_cpu32 (exts[ex].raw[3]) >> 21);
}

static inline grub_uint64_t
GRUB_XFS_EXTENT_SIZE (struct grub_xfs_extent *exts, int ex)
{
  return (grub_be_to_cpu32 (exts[ex].raw[3]) & ((1 << 21) - 1));
}

static inline grub_uint64_t
grub_xfs_inode_block (struct grub_xfs_data *data,
		      grub_uint64_t ino)
{
  long long int inoinag = GRUB_XFS_INO_INOINAG (data, ino);
  long long ag = GRUB_XFS_INO_AG (data, ino);
  long long block;

  block = (inoinag >> data->sblock.log2_inop) + ag * data->agsize;
  block <<= (data->sblock.log2_bsize - GRUB_DISK_SECTOR_BITS);
  return block;
}


static inline int
grub_xfs_inode_offset (struct grub_xfs_data *data,
		       grub_uint64_t ino)
{
  int inoag = GRUB_XFS_INO_INOINAG (data, ino);
  return ((inoag & ((1 << data->sblock.log2_inop) - 1)) <<
	  data->sblock.log2_inode);
}

static inline grub_size_t
grub_xfs_inode_size(struct grub_xfs_data *data)
{
  return (grub_size_t)1 << data->sblock.log2_inode;
}

/*
 * Returns size occupied by XFS inode stored in memory - we store struct
 * grub_fshelp_node there but on disk inode size may be actually larger than
 * struct grub_xfs_inode so we need to account for that so that we can read
 * from disk directly into in-memory structure.
 */
static inline grub_size_t
grub_xfs_fshelp_size(struct grub_xfs_data *data)
{
  return sizeof (struct grub_fshelp_node) - sizeof (struct grub_xfs_inode)
	       + grub_xfs_inode_size(data);
}

/* This should return void * but XFS code is error-prone with alignment, so
   return char to retain cast-align.
 */
static char *
grub_xfs_inode_data(struct grub_xfs_inode *inode)
{
	if (inode->version <= 2)
		return ((char *)inode) + XFS_V2_INODE_SIZE;
	return ((char *)inode) + XFS_V3_INODE_SIZE;
}

static struct grub_xfs_dir_entry *
grub_xfs_inline_de(struct grub_xfs_dir_header *head)
{
  /*
    With small inode numbers the header is 4 bytes smaller because of
    smaller parent pointer
  */
  return (struct grub_xfs_dir_entry *)
    (((char *) head) + sizeof(struct grub_xfs_dir_header) -
     (head->largeino ? 0 : sizeof(grub_uint32_t)));
}

static grub_uint8_t *
grub_xfs_inline_de_inopos(struct grub_xfs_data *data,
			  struct grub_xfs_dir_entry *de)
{
  return ((grub_uint8_t *)(de + 1)) + de->len - 1 + (data->hasftype ? 1 : 0);
}

static struct grub_xfs_dir_entry *
grub_xfs_inline_next_de(struct grub_xfs_data *data,
			struct grub_xfs_dir_header *head,
			struct grub_xfs_dir_entry *de)
{
  char *p = (char *)de + sizeof(struct grub_xfs_dir_entry) - 1 + de->len;

  p += head->largeino ? sizeof(grub_uint64_t) : sizeof(grub_uint32_t);
  if (data->hasftype)
    p++;

  return (struct grub_xfs_dir_entry *)p;
}

static struct grub_xfs_dirblock_tail *
grub_xfs_dir_tail(struct grub_xfs_data *data, void *dirblock)
{
  int dirblksize = 1 << (data->sblock.log2_bsize + data->sblock.log2_dirblk);

  return (struct grub_xfs_dirblock_tail *)
    ((char *)dirblock + dirblksize - sizeof (struct grub_xfs_dirblock_tail));
}

static struct grub_xfs_dir2_entry *
grub_xfs_first_de(struct grub_xfs_data *data, void *dirblock)
{
  if (data->hascrc)
    return (struct grub_xfs_dir2_entry *)((char *)dirblock + 64);
  return (struct grub_xfs_dir2_entry *)((char *)dirblock + 16);
}

static struct grub_xfs_dir2_entry *
grub_xfs_next_de(struct grub_xfs_data *data, struct grub_xfs_dir2_entry *de)
{
  int size = sizeof (struct grub_xfs_dir2_entry) + de->len + 2 /* Tag */;

  if (data->hasftype)
    size++;		/* File type */
  return (struct grub_xfs_dir2_entry *)(((char *)de) + ALIGN_UP(size, 8));
}

/* This should return void * but XFS code is error-prone with alignment, so
   return char to retain cast-align.
 */
static char *
grub_xfs_btree_keys(struct grub_xfs_data *data,
		    struct grub_xfs_btree_node *leaf)
{
  char *keys = (char *)(leaf + 1);

  if (data->hascrc)
    keys += 48;	/* skip crc, uuid, ... */
  return keys;
}

static grub_err_t
grub_xfs_read_inode (struct grub_xfs_data *data, grub_uint64_t ino,
		     struct grub_xfs_inode *inode)
{
  grub_uint64_t block = grub_xfs_inode_block (data, ino);
  int offset = grub_xfs_inode_offset (data, ino);

  grub_dprintf("xfs", "Reading inode (%" PRIuGRUB_UINT64_T ") - %" PRIuGRUB_UINT64_T ", %d\n",
	       ino, block, offset);
  /* Read the inode.  */
  if (grub_disk_read (data->disk, block, offset, grub_xfs_inode_size(data),
		      inode))
    return grub_errno;

  if (grub_strncmp ((char *) inode->magic, "IN", 2))
    return grub_error (GRUB_ERR_BAD_FS, "not a correct XFS inode");

  return 0;
}

static grub_uint64_t
get_fsb (const void *keys, int idx)
{
  const char *p = (const char *) keys + sizeof(grub_uint64_t) * idx;
  return grub_be_to_cpu64 (grub_get_unaligned64 (p));
}

static int
grub_xfs_inode_has_large_extent_counts (const struct grub_xfs_inode *inode)
{
  return inode->version >= 3 &&
	 (inode->flags2 & grub_cpu_to_be64_compile_time (XFS_DIFLAG2_NREXT64));
}

static grub_uint64_t
grub_xfs_get_inode_nextents (struct grub_xfs_inode *inode)
{
  return (grub_xfs_inode_has_large_extent_counts (inode)) ?
	  grub_be_to_cpu64 (inode->nextents_big) :
	  grub_be_to_cpu32 (inode->nextents);
}

static grub_disk_addr_t
grub_xfs_read_block (grub_fshelp_node_t node, grub_disk_addr_t fileblock)
{
  struct grub_xfs_btree_node *leaf = 0;
  grub_uint64_t ex, nrec;
  struct grub_xfs_extent *exts;
  grub_uint64_t ret = 0;

  if (node->inode.format == XFS_INODE_FORMAT_BTREE)
    {
      struct grub_xfs_btree_root *root;
      const char *keys;
      int recoffset;

      leaf = grub_malloc (node->data->bsize);
      if (leaf == 0)
        return 0;

      root = (struct grub_xfs_btree_root *) grub_xfs_inode_data(&node->inode);
      nrec = grub_be_to_cpu16 (root->numrecs);
      keys = (char *) &root->keys[0];
      if (node->inode.fork_offset)
	recoffset = (node->inode.fork_offset - 1) / 2;
      else
	recoffset = (grub_xfs_inode_size(node->data)
		     - ((char *) keys - (char *) &node->inode))
				/ (2 * sizeof (grub_uint64_t));
      do
        {
          grub_uint64_t i;
	  grub_addr_t keys_end, data_end;

	  if (grub_mul (sizeof (grub_uint64_t), nrec, &keys_end) ||
	      grub_add ((grub_addr_t) keys, keys_end, &keys_end) ||
	      grub_add ((grub_addr_t) node->data, node->data->data_size, &data_end) ||
	      keys_end > data_end)
	    {
	      grub_error (GRUB_ERR_BAD_FS, "invalid number of XFS root keys");
	      grub_free (leaf);
	      return 0;
	    }

          for (i = 0; i < nrec; i++)
            {
              if (fileblock < get_fsb(keys, i))
                break;
            }

          /* Sparse block.  */
          if (i == 0)
            {
              grub_free (leaf);
              return 0;
            }

          if (grub_disk_read (node->data->disk,
                              GRUB_XFS_FSB_TO_BLOCK (node->data, get_fsb (keys, i - 1 + recoffset)) << (node->data->sblock.log2_bsize - GRUB_DISK_SECTOR_BITS),
                              0, node->data->bsize, leaf))
            {
              grub_free (leaf);
              return 0;
            }

	  if ((!node->data->hascrc &&
	       grub_strncmp ((char *) leaf->magic, "BMAP", 4)) ||
	      (node->data->hascrc &&
	       grub_strncmp ((char *) leaf->magic, "BMA3", 4)))
            {
              grub_free (leaf);
              grub_error (GRUB_ERR_BAD_FS, "not a correct XFS BMAP node");
              return 0;
            }

          nrec = grub_be_to_cpu16 (leaf->numrecs);
          keys = grub_xfs_btree_keys(node->data, leaf);
	  recoffset = ((node->data->bsize - ((char *) keys
					     - (char *) leaf))
		       / (2 * sizeof (grub_uint64_t)));
	}
      while (leaf->level);
      exts = (struct grub_xfs_extent *) keys;
    }
  else if (node->inode.format == XFS_INODE_FORMAT_EXT)
    {
      grub_addr_t exts_end = 0;
      grub_addr_t data_end = 0;

      nrec = grub_xfs_get_inode_nextents (&node->inode);
      exts = (struct grub_xfs_extent *) grub_xfs_inode_data(&node->inode);

      if (grub_mul (sizeof (struct grub_xfs_extent), nrec, &exts_end) ||
	  grub_add ((grub_addr_t) node->data, exts_end, &exts_end) ||
	  grub_add ((grub_addr_t) node->data, node->data->data_size, &data_end) ||
	  exts_end > data_end)
	{
	  grub_error (GRUB_ERR_BAD_FS, "invalid number of XFS extents");
	  return 0;
	}
    }
  else
    {
      grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET,
		  "XFS does not support inode format %d yet",
		  node->inode.format);
      return 0;
    }

  /* Iterate over each extent to figure out which extent has
     the block we are looking for.  */
  for (ex = 0; ex < nrec; ex++)
    {
      grub_uint64_t start = GRUB_XFS_EXTENT_BLOCK (exts, ex);
      grub_uint64_t offset = GRUB_XFS_EXTENT_OFFSET (exts, ex);
      grub_uint64_t size = GRUB_XFS_EXTENT_SIZE (exts, ex);

      /* Sparse block.  */
      if (fileblock < offset)
        break;
      else if (fileblock < offset + size)
        {
          ret = (fileblock - offset + start);
          break;
        }
    }

  grub_free (leaf);

  return GRUB_XFS_FSB_TO_BLOCK(node->data, ret);
}


/* Read LEN bytes from the file described by DATA starting with byte
   POS.  Return the amount of read bytes in READ.  */
static grub_ssize_t
grub_xfs_read_file (grub_fshelp_node_t node,
		    grub_disk_read_hook_t read_hook, void *read_hook_data,
		    grub_off_t pos, grub_size_t len, char *buf, grub_uint32_t header_size)
{
  return grub_fshelp_read_file (node->data->disk, node,
				read_hook, read_hook_data,
				pos, len, buf, grub_xfs_read_block,
				grub_be_to_cpu64 (node->inode.size) + header_size,
				node->data->sblock.log2_bsize
				- GRUB_DISK_SECTOR_BITS, 0);
}


static char *
grub_xfs_read_symlink (grub_fshelp_node_t node)
{
  grub_ssize_t size = grub_be_to_cpu64 (node->inode.size);
  grub_size_t sz;

  if (size < 0)
    {
      grub_error (GRUB_ERR_BAD_FS, "invalid symlink");
      return 0;
    }

  switch (node->inode.format)
    {
    case XFS_INODE_FORMAT_INO:
      return grub_strndup (grub_xfs_inode_data(&node->inode), size);

    case XFS_INODE_FORMAT_EXT:
      {
	char *symlink;
	grub_ssize_t numread;
	int off = 0;

	if (node->data->hascrc)
	  off = 56;

	if (grub_add (size, 1, &sz))
	  {
	    grub_error (GRUB_ERR_OUT_OF_RANGE, N_("symlink size overflow"));
	    return 0;
	  }
	symlink = grub_malloc (sz);
	if (!symlink)
	  return 0;

	node->inode.size = grub_be_to_cpu64 (size + off);
	numread = grub_xfs_read_file (node, 0, 0, off, size, symlink, off);
	if (numread != size)
	  {
	    grub_free (symlink);
	    return 0;
	  }
	symlink[size] = '\0';
	return symlink;
      }
    }

  return 0;
}


static enum grub_fshelp_filetype
grub_xfs_mode_to_filetype (grub_uint16_t mode)
{
  if ((grub_be_to_cpu16 (mode)
       & FILETYPE_INO_MASK) == FILETYPE_INO_DIRECTORY)
    return GRUB_FSHELP_DIR;
  else if ((grub_be_to_cpu16 (mode)
	    & FILETYPE_INO_MASK) == FILETYPE_INO_SYMLINK)
    return GRUB_FSHELP_SYMLINK;
  else if ((grub_be_to_cpu16 (mode)
	    & FILETYPE_INO_MASK) == FILETYPE_INO_REG)
    return GRUB_FSHELP_REG;
  return GRUB_FSHELP_UNKNOWN;
}


/* Context for grub_xfs_iterate_dir.  */
struct grub_xfs_iterate_dir_ctx
{
  grub_fshelp_iterate_dir_hook_t hook;
  void *hook_data;
  struct grub_fshelp_node *diro;
};

/* Helper for grub_xfs_iterate_dir.  */
static int iterate_dir_call_hook (grub_uint64_t ino, const char *filename,
				  struct grub_xfs_iterate_dir_ctx *ctx)
{
  struct grub_fshelp_node *fdiro;
  grub_err_t err;
  grub_size_t sz;

  if (grub_add (grub_xfs_fshelp_size(ctx->diro->data), 1, &sz))
    {
      grub_error (GRUB_ERR_OUT_OF_RANGE, N_("directory data size overflow"));
      grub_print_error ();
      return 0;
    }
  fdiro = grub_malloc (sz);
  if (!fdiro)
    {
      grub_print_error ();
      return 0;
    }

  /* The inode should be read, otherwise the filetype can
     not be determined.  */
  fdiro->ino = ino;
  fdiro->inode_read = 1;
  fdiro->data = ctx->diro->data;
  err = grub_xfs_read_inode (ctx->diro->data, ino, &fdiro->inode);
  if (err)
    {
      grub_print_error ();
      grub_free (fdiro);
      return 0;
    }

  return ctx->hook (filename, grub_xfs_mode_to_filetype (fdiro->inode.mode),
		    fdiro, ctx->hook_data);
}

static int
grub_xfs_iterate_dir (grub_fshelp_node_t dir,
		      grub_fshelp_iterate_dir_hook_t hook, void *hook_data)
{
  struct grub_fshelp_node *diro = (struct grub_fshelp_node *) dir;
  struct grub_xfs_iterate_dir_ctx ctx = {
    .hook = hook,
    .hook_data = hook_data,
    .diro = diro
  };

  switch (diro->inode.format)
    {
    case XFS_INODE_FORMAT_INO:
      {
	struct grub_xfs_dir_header *head = (struct grub_xfs_dir_header *) grub_xfs_inode_data(&diro->inode);
	struct grub_xfs_dir_entry *de = grub_xfs_inline_de(head);
	int smallino = !head->largeino;
	int i;
	grub_uint64_t parent;

	/* If small inode numbers are used to pack the direntry, the
	   parent inode number is small too.  */
	if (smallino)
	  parent = grub_be_to_cpu32 (head->parent.i4);
	else
	  parent = grub_be_to_cpu64 (head->parent.i8);

	/* Synthesize the direntries for `.' and `..'.  */
	if (iterate_dir_call_hook (diro->ino, ".", &ctx))
	  return 1;

	if (iterate_dir_call_hook (parent, "..", &ctx))
	  return 1;

	for (i = 0; i < head->count &&
	     (grub_uint8_t *) de < ((grub_uint8_t *) dir + grub_xfs_fshelp_size (dir->data)); i++)
	  {
	    grub_uint64_t ino;
	    grub_uint8_t *inopos = grub_xfs_inline_de_inopos(dir->data, de);
	    grub_uint8_t c;

	    if ((inopos + (smallino ? 4 : 8)) > (grub_uint8_t *) dir + grub_xfs_fshelp_size (dir->data))
	      {
		grub_error (GRUB_ERR_BAD_FS, "invalid XFS inode");
		return 0;
	      }


	    /* inopos might be unaligned.  */
	    if (smallino)
	      ino = (((grub_uint32_t) inopos[0]) << 24)
		| (((grub_uint32_t) inopos[1]) << 16)
		| (((grub_uint32_t) inopos[2]) << 8)
		| (((grub_uint32_t) inopos[3]) << 0);
	    else
	      ino = (((grub_uint64_t) inopos[0]) << 56)
		| (((grub_uint64_t) inopos[1]) << 48)
		| (((grub_uint64_t) inopos[2]) << 40)
		| (((grub_uint64_t) inopos[3]) << 32)
		| (((grub_uint64_t) inopos[4]) << 24)
		| (((grub_uint64_t) inopos[5]) << 16)
		| (((grub_uint64_t) inopos[6]) << 8)
		| (((grub_uint64_t) inopos[7]) << 0);

	    c = de->name[de->len];
	    de->name[de->len] = '\0';
	    if (iterate_dir_call_hook (ino, de->name, &ctx))
	      {
		de->name[de->len] = c;
		return 1;
	      }
	    de->name[de->len] = c;

	    de = grub_xfs_inline_next_de(dir->data, head, de);
	  }
	break;
      }

    case XFS_INODE_FORMAT_BTREE:
    case XFS_INODE_FORMAT_EXT:
      {
	grub_ssize_t numread;
	char *dirblock;
	grub_uint64_t blk;
        int dirblk_size, dirblk_log2;

        dirblk_log2 = (dir->data->sblock.log2_bsize
                       + dir->data->sblock.log2_dirblk);
        dirblk_size = 1 << dirblk_log2;

	dirblock = grub_malloc (dirblk_size);
	if (! dirblock)
	  return 0;

	/* Iterate over every block the directory has.  */
	for (blk = 0;
	     blk < (grub_be_to_cpu64 (dir->inode.size)
		    >> dirblk_log2);
	     blk++)
	  {
	    struct grub_xfs_dir2_entry *direntry =
					grub_xfs_first_de(dir->data, dirblock);
	    int entries = -1;
	    char *end = dirblock + dirblk_size;
	    grub_uint32_t magic;

	    numread = grub_xfs_read_file (dir, 0, 0,
					  blk << dirblk_log2,
					  dirblk_size, dirblock, 0);
	    if (numread != dirblk_size)
	      {
	        grub_free (dirblock);
	        return 0;
	      }

	    /*
	     * If this data block isn't actually part of the extent list then
	     * grub_xfs_read_file() returns a block of zeros. So, if the magic
	     * number field is all zeros then this block should be skipped.
	     */
	    magic = *(grub_uint32_t *)(void *) dirblock;
	    if (!magic)
	      continue;

	    /*
	     * Leaf and tail information are only in the data block if the number
	     * of extents is 1.
	     */
	    if (grub_xfs_get_inode_nextents (&dir->inode) == 1)
	      {
		struct grub_xfs_dirblock_tail *tail = grub_xfs_dir_tail (dir->data, dirblock);

		end = (char *) tail;

		/* Subtract the space used by leaf nodes. */
		end -= grub_be_to_cpu32 (tail->leaf_count) * sizeof (struct grub_xfs_dir_leaf_entry);

		entries = grub_be_to_cpu32 (tail->leaf_count) - grub_be_to_cpu32 (tail->leaf_stale);

		if (!entries)
		  continue;
	      }

	    /* Iterate over all entries within this block.  */
	    while ((char *) direntry < (char *) end)
	      {
		grub_uint8_t *freetag;
		char *filename;

		freetag = (grub_uint8_t *) direntry;

		if (grub_get_unaligned16 (freetag) == 0XFFFF)
		  {
		    grub_uint8_t *skip = (freetag + sizeof (grub_uint16_t));

		    /* This entry is not used, go to the next one.  */
		    direntry = (struct grub_xfs_dir2_entry *)
				(((char *)direntry) +
				grub_be_to_cpu16 (grub_get_unaligned16 (skip)));

		    continue;
		  }

		filename = (char *)(direntry + 1);
		if (filename + direntry->len + 1 > (char *) end)
		  {
		    grub_error (GRUB_ERR_BAD_FS, "invalid XFS directory entry");
		    return 0;
		  }

		/* The byte after the filename is for the filetype, padding, or
		   tag, which is not used by GRUB.  So it can be overwritten. */
		filename[direntry->len] = '\0';

		if (iterate_dir_call_hook (grub_be_to_cpu64(direntry->inode),
					   filename, &ctx))
		  {
		    grub_free (dirblock);
		    return 1;
		  }

		/*
		 * The expected number of directory entries is only tracked for the
		 * single extent case.
		 */
		if (grub_xfs_get_inode_nextents (&dir->inode) == 1)
		  {
		    /* Check if last direntry in this block is reached. */
		    entries--;
		    if (!entries)
		      break;
		  }

		/* Select the next directory entry.  */
		direntry = grub_xfs_next_de(dir->data, direntry);
	      }
	  }
	grub_free (dirblock);
	break;
      }

    default:
      grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET,
		  "XFS does not support inode format %d yet",
		  diro->inode.format);
    }
  return 0;
}


static struct grub_xfs_data *
grub_xfs_mount (grub_disk_t disk)
{
  struct grub_xfs_data *data = 0;
  grub_size_t sz;

  data = grub_zalloc (sizeof (struct grub_xfs_data));
  if (!data)
    return 0;

  data->data_size = sizeof (struct grub_xfs_data);

  grub_dprintf("xfs", "Reading sb\n");
  /* Read the superblock.  */
  if (grub_disk_read (disk, 0, 0,
		      sizeof (struct grub_xfs_sblock), &data->sblock))
    goto fail;

  if (!grub_xfs_sb_valid(data))
    goto fail;

  if (grub_xfs_sb_needs_repair (data))
    grub_dprintf ("xfs", "XFS filesystem needs repair, boot may fail\n");

  if (grub_add (grub_xfs_inode_size (data),
      sizeof (struct grub_xfs_data) - sizeof (struct grub_xfs_inode) + 1, &sz))
    goto fail;

  data = grub_realloc (data, sz);

  if (! data)
    goto fail;

  data->data_size = sz;
  data->diropen.data = data;
  data->diropen.ino = grub_be_to_cpu64(data->sblock.rootino);
  data->diropen.inode_read = 1;
  data->bsize = grub_be_to_cpu32 (data->sblock.bsize);
  data->agsize = grub_be_to_cpu32 (data->sblock.agsize);
  data->hasftype = grub_xfs_sb_hasftype(data);
  data->hascrc = grub_xfs_sb_hascrc(data);

  data->disk = disk;
  data->pos = 0;
  grub_dprintf("xfs", "Reading root ino %" PRIuGRUB_UINT64_T "\n",
	       grub_cpu_to_be64(data->sblock.rootino));

  grub_xfs_read_inode (data, data->diropen.ino, &data->diropen.inode);

  return data;
 fail:

  if (grub_errno == GRUB_ERR_OUT_OF_RANGE || grub_errno == GRUB_ERR_NONE)
    grub_error (GRUB_ERR_BAD_FS, "not an XFS filesystem");

  grub_free (data);

  return 0;
}

/* Context for grub_xfs_dir.  */
struct grub_xfs_dir_ctx
{
  grub_fs_dir_hook_t hook;
  void *hook_data;
};

/* Bigtime inodes helpers. */
#define XFS_BIGTIME_EPOCH_OFFSET	(-(grub_int64_t) GRUB_INT32_MIN)

static int grub_xfs_inode_has_bigtime (const struct grub_xfs_inode *inode)
{
  return inode->version >= 3 &&
	 (inode->flags2 & grub_cpu_to_be64_compile_time (XFS_DIFLAG2_BIGTIME));
}

static grub_int64_t
grub_xfs_get_inode_time (struct grub_xfs_inode *inode)
{
  struct grub_xfs_time_legacy *lts;

  if (grub_xfs_inode_has_bigtime (inode))
    return grub_divmod64 (grub_be_to_cpu64 (inode->mtime), NSEC_PER_SEC, NULL) - XFS_BIGTIME_EPOCH_OFFSET;

  lts = (struct grub_xfs_time_legacy *) &inode->mtime;
  return grub_be_to_cpu32 (lts->sec);
}

/* Helper for grub_xfs_dir.  */
static int
grub_xfs_dir_iter (const char *filename, enum grub_fshelp_filetype filetype,
		   grub_fshelp_node_t node, void *data)
{
  struct grub_xfs_dir_ctx *ctx = data;
  struct grub_dirhook_info info;

  grub_memset (&info, 0, sizeof (info));
  if (node->inode_read)
    {
      info.mtimeset = 1;
      info.mtime = grub_xfs_get_inode_time (&node->inode);
    }
  info.dir = ((filetype & GRUB_FSHELP_TYPE_MASK) == GRUB_FSHELP_DIR);
  grub_free (node);
  return ctx->hook (filename, &info, ctx->hook_data);
}

static grub_err_t
grub_xfs_dir (grub_device_t device, const char *path,
	      grub_fs_dir_hook_t hook, void *hook_data)
{
  struct grub_xfs_dir_ctx ctx = { hook, hook_data };
  struct grub_xfs_data *data = 0;
  struct grub_fshelp_node *fdiro = 0;

  grub_dl_ref (my_mod);

  data = grub_xfs_mount (device->disk);
  if (!data)
    goto mount_fail;

  grub_fshelp_find_file (path, &data->diropen, &fdiro, grub_xfs_iterate_dir,
			 grub_xfs_read_symlink, GRUB_FSHELP_DIR);
  if (grub_errno)
    goto fail;

  grub_xfs_iterate_dir (fdiro, grub_xfs_dir_iter, &ctx);

 fail:
  if (fdiro != &data->diropen)
    grub_free (fdiro);
  grub_free (data);

 mount_fail:

  grub_dl_unref (my_mod);

  return grub_errno;
}


/* Open a file named NAME and initialize FILE.  */
static grub_err_t
grub_xfs_open (struct grub_file *file, const char *name)
{
  struct grub_xfs_data *data;
  struct grub_fshelp_node *fdiro = 0;

  grub_dl_ref (my_mod);

  data = grub_xfs_mount (file->device->disk);
  if (!data)
    goto mount_fail;

  grub_fshelp_find_file (name, &data->diropen, &fdiro, grub_xfs_iterate_dir,
			 grub_xfs_read_symlink, GRUB_FSHELP_REG);
  if (grub_errno)
    goto fail;

  if (!fdiro->inode_read)
    {
      grub_xfs_read_inode (data, fdiro->ino, &fdiro->inode);
      if (grub_errno)
	goto fail;
    }

  if (fdiro != &data->diropen)
    {
      grub_memcpy (&data->diropen, fdiro, grub_xfs_fshelp_size(data));
      grub_free (fdiro);
    }

  file->size = grub_be_to_cpu64 (data->diropen.inode.size);
  file->data = data;
  file->offset = 0;

  return 0;

 fail:
  if (fdiro != &data->diropen)
    grub_free (fdiro);
  grub_free (data);

 mount_fail:
  grub_dl_unref (my_mod);

  return grub_errno;
}


static grub_ssize_t
grub_xfs_read (grub_file_t file, char *buf, grub_size_t len)
{
  struct grub_xfs_data *data =
    (struct grub_xfs_data *) file->data;

  return grub_xfs_read_file (&data->diropen,
			     file->read_hook, file->read_hook_data,
			     file->offset, len, buf, 0);
}


static grub_err_t
grub_xfs_close (grub_file_t file)
{
  grub_free (file->data);

  grub_dl_unref (my_mod);

  return GRUB_ERR_NONE;
}


static grub_err_t
grub_xfs_label (grub_device_t device, char **label)
{
  struct grub_xfs_data *data;
  grub_disk_t disk = device->disk;

  grub_dl_ref (my_mod);

  data = grub_xfs_mount (disk);
  if (data)
    *label = grub_strndup ((char *) (data->sblock.label), 12);
  else
    *label = 0;

  grub_dl_unref (my_mod);

  grub_free (data);

  return grub_errno;
}

static grub_err_t
grub_xfs_uuid (grub_device_t device, char **uuid)
{
  struct grub_xfs_data *data;
  grub_disk_t disk = device->disk;

  grub_dl_ref (my_mod);

  data = grub_xfs_mount (disk);
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

static struct grub_fs grub_xfs_fs =
  {
    .name = "xfs",
    .fs_dir = grub_xfs_dir,
    .fs_open = grub_xfs_open,
    .fs_read = grub_xfs_read,
    .fs_close = grub_xfs_close,
    .fs_label = grub_xfs_label,
    .fs_uuid = grub_xfs_uuid,
#ifdef GRUB_UTIL
    .reserved_first_sector = 0,
    .blocklist_install = 1,
#endif
    .next = 0
  };

GRUB_MOD_INIT(xfs)
{
  //grub_xfs_fs.mod = mod;
  grub_fs_register (&grub_xfs_fs);
  my_mod = mod;
}

GRUB_MOD_FINI(xfs)
{
  grub_fs_unregister (&grub_xfs_fs);
}
