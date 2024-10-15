/* squash4.c - SquashFS */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010  Free Software Foundation, Inc.
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
#include <grub/types.h>
#include <grub/fshelp.h>
#include <grub/deflate.h>
#include <minilzo.h>
#include <zstd.h>

#include "xz.h"
#include "xz_stream.h"

GRUB_MOD_LICENSE ("GPLv3+");

/*
  object         format      Pointed by
  superblock     RAW         Fixed offset (0)
  data           RAW ?       Fixed offset (60)
  inode table    Chunk       superblock
  dir table      Chunk       superblock
  fragment table Chunk       unk1
  unk1           RAW, Chunk  superblock
  unk2           RAW         superblock
  UID/GID        Chunk       exttblptr
  exttblptr      RAW         superblock

  UID/GID table is the array ot uint32_t
  unk1 contains pointer to fragment table followed by some chunk.
  unk2 containts one uint64_t
*/

struct grub_squash_super
{
  grub_uint32_t magic;
#define SQUASH_MAGIC 0x73717368
  grub_uint32_t dummy1;
  grub_uint32_t creation_time;
  grub_uint32_t block_size;
  grub_uint32_t dummy2;
  grub_uint16_t compression;
  grub_uint16_t dummy3;
  grub_uint64_t dummy4;
  grub_uint16_t root_ino_offset;
  grub_uint32_t root_ino_chunk;
  grub_uint16_t dummy5;
  grub_uint64_t total_size;
  grub_uint64_t exttbloffset;
  grub_uint64_t dummy6;
  grub_uint64_t inodeoffset;
  grub_uint64_t diroffset;
  grub_uint64_t unk1offset;
  grub_uint64_t unk2offset;
} GRUB_PACKED;

/* Chunk-based */
struct grub_squash_inode
{
  /* Same values as direlem types. */
  grub_uint16_t type;
  grub_uint16_t dummy[3];
  grub_uint32_t mtime;
  grub_uint32_t dummy2;
  union
  {
    struct {
      grub_uint32_t chunk;
      grub_uint32_t fragment;
      grub_uint32_t offset;
      grub_uint32_t size;
      grub_uint32_t block_size[0];
    }  GRUB_PACKED file;
    struct {
      grub_uint64_t chunk;
      grub_uint64_t size;
      grub_uint32_t dummy1[3];
      grub_uint32_t fragment;
      grub_uint32_t offset;
      grub_uint32_t dummy3;
      grub_uint32_t block_size[0];
    }  GRUB_PACKED long_file;
    struct {
      grub_uint32_t chunk;
      grub_uint32_t dummy;
      grub_uint16_t size;
      grub_uint16_t offset;
    } GRUB_PACKED dir;
    struct {
      grub_uint32_t dummy1;
      grub_uint32_t size;
      grub_uint32_t chunk;
      grub_uint32_t dummy2;
      grub_uint16_t dummy3;
      grub_uint16_t offset;
    } GRUB_PACKED long_dir;
    struct {
      grub_uint32_t dummy;
      grub_uint32_t namelen;
      char name[0];
    } GRUB_PACKED symlink;
  }  GRUB_PACKED;
} GRUB_PACKED;

struct grub_squash_cache_inode
{
  struct grub_squash_inode ino;
  grub_disk_addr_t ino_chunk;
  grub_uint16_t	ino_offset;
  grub_uint32_t *block_sizes;
  grub_disk_addr_t *cumulated_block_sizes;
};

/* Chunk-based.  */
struct grub_squash_dirent_header
{
  /* Actually the value is the number of elements - 1.  */
  grub_uint32_t nelems;
  grub_uint32_t ino_chunk;
  grub_uint32_t dummy;
} GRUB_PACKED;

struct grub_squash_dirent
{
  grub_uint16_t ino_offset;
  grub_uint16_t dummy;
  grub_uint16_t type;
  /* Actually the value is the length of name - 1.  */
  grub_uint16_t namelen;
  char name[0];
} GRUB_PACKED;

enum
  {
    SQUASH_TYPE_DIR = 1,
    SQUASH_TYPE_REGULAR = 2,
    SQUASH_TYPE_SYMLINK = 3,
    SQUASH_TYPE_LONG_DIR = 8,
    SQUASH_TYPE_LONG_REGULAR = 9,
  };


struct grub_squash_frag_desc
{
  grub_uint64_t offset;
  grub_uint32_t size;
  grub_uint32_t dummy;
} GRUB_PACKED;

enum
  {
    SQUASH_CHUNK_FLAGS = 0x8000,
    SQUASH_CHUNK_UNCOMPRESSED = 0x8000
  };

enum
  {
    SQUASH_BLOCK_FLAGS = 0x1000000,
    SQUASH_BLOCK_UNCOMPRESSED = 0x1000000
  };

enum
  {
    COMPRESSION_ZLIB = 1,
    COMPRESSION_LZO = 3,
    COMPRESSION_XZ = 4,
    COMPRESSION_LZ4 = 5,
    COMPRESSION_ZSTD = 6,
  };


#define SQUASH_CHUNK_SIZE 0x2000
#define XZBUFSIZ 0x2000

struct grub_squash_data
{
  grub_disk_t disk;
  struct grub_squash_super sb;
  struct grub_squash_cache_inode ino;
  grub_uint64_t fragments;
  int log2_blksz;
  grub_size_t blksz;
  grub_ssize_t (*decompress) (char *inbuf, grub_size_t insize, grub_off_t off,
			      char *outbuf, grub_size_t outsize,
			      struct grub_squash_data *data);
  struct xz_dec *xzdec;
  char *xzbuf;
};

struct grub_fshelp_node
{
  struct grub_squash_data *data;
  struct grub_squash_inode ino;
  grub_size_t stsize;
  struct 
  {
    grub_disk_addr_t ino_chunk;
    grub_uint16_t ino_offset;
  } stack[1];
};

static grub_err_t
read_chunk (struct grub_squash_data *data, void *buf, grub_size_t len,
	    grub_uint64_t chunk_start, grub_off_t offset)
{
  while (len > 0)
    {
      grub_uint64_t csize;
      grub_uint16_t d;
      grub_err_t err;
      while (1)
	{
	  err = grub_disk_read (data->disk,
				chunk_start >> GRUB_DISK_SECTOR_BITS,
				chunk_start & (GRUB_DISK_SECTOR_SIZE - 1),
				sizeof (d), &d);
	  if (err)
	    return err;
	  if (offset < SQUASH_CHUNK_SIZE)
	    break;
	  offset -= SQUASH_CHUNK_SIZE;
	  chunk_start += 2 + (grub_le_to_cpu16 (d) & ~SQUASH_CHUNK_FLAGS);
	}

      csize = SQUASH_CHUNK_SIZE - offset;
      if (csize > len)
	csize = len;
  
      if (grub_le_to_cpu16 (d) & SQUASH_CHUNK_UNCOMPRESSED)
	{
	  grub_disk_addr_t a = chunk_start + 2 + offset;
	  err = grub_disk_read (data->disk, (a >> GRUB_DISK_SECTOR_BITS),
				a & (GRUB_DISK_SECTOR_SIZE - 1),
				csize, buf);
	  if (err)
	    return err;
	}
      else
	{
	  char *tmp;
	  grub_size_t bsize = grub_le_to_cpu16 (d) & ~SQUASH_CHUNK_FLAGS; 
	  grub_disk_addr_t a = chunk_start + 2;
	  tmp = grub_malloc (bsize);
	  if (!tmp)
	    return grub_errno;
	  /* FIXME: buffer uncompressed data.  */
	  err = grub_disk_read (data->disk, (a >> GRUB_DISK_SECTOR_BITS),
				a & (GRUB_DISK_SECTOR_SIZE - 1),
				bsize, tmp);
	  if (err)
	    {
	      grub_free (tmp);
	      return err;
	    }

	  if (data->decompress (tmp, bsize, offset,
				buf, csize, data) < 0)
	    {
	      grub_free (tmp);
	      return grub_errno;
	    }
	  grub_free (tmp);
	}
      len -= csize;
      offset += csize;
      buf = (char *) buf + csize;
    }
  return GRUB_ERR_NONE;
}

static grub_ssize_t
zlib_decompress (char *inbuf, grub_size_t insize, grub_off_t off,
		 char *outbuf, grub_size_t outsize,
		 struct grub_squash_data *data __attribute__ ((unused)))
{
  return grub_zlib_decompress (inbuf, insize, off, outbuf, outsize);
}

static grub_ssize_t
lzo_decompress (char *inbuf, grub_size_t insize, grub_off_t off,
		char *outbuf, grub_size_t len, struct grub_squash_data *data)
{
  lzo_uint usize = data->blksz;
  grub_uint8_t *udata;

  if (usize < 8192)
    usize = 8192;

  udata = grub_malloc (usize);
  if (!udata)
    return -1;

  if (lzo1x_decompress_safe ((grub_uint8_t *) inbuf,
			     insize, udata, &usize, NULL) != LZO_E_OK)
    {
      grub_error (GRUB_ERR_BAD_FS, "incorrect compressed chunk");
      grub_free (udata);
      return -1;
    }
  grub_memcpy (outbuf, udata + off, len);
  grub_free (udata);
  return len;
}

static grub_ssize_t
xz_decompress (char *inbuf, grub_size_t insize, grub_off_t off,
	       char *outbuf, grub_size_t len, struct grub_squash_data *data)
{
  grub_size_t ret = 0;
  grub_off_t pos = 0;
  struct xz_buf buf;

  xz_dec_reset (data->xzdec);
  buf.in = (grub_uint8_t *) inbuf;
  buf.in_pos = 0;
  buf.in_size = insize;
  buf.out = (grub_uint8_t *) data->xzbuf;
  buf.out_pos = 0;
  buf.out_size = XZBUFSIZ;

  while (len)
    {
      enum xz_ret xzret;
      
      buf.out_pos = 0;

      xzret = xz_dec_run (data->xzdec, &buf);

      if (xzret != XZ_OK && xzret != XZ_STREAM_END)
	{
	  grub_error (GRUB_ERR_BAD_COMPRESSED_DATA, "invalid xz chunk");
	  return -1;
	}
      if (pos + buf.out_pos >= off)
	{
	  grub_ssize_t outoff = pos - off;
	  grub_size_t l;
	  if (outoff >= 0)
	    {
	      l = buf.out_pos;
	      if (l > len)
		l = len;
	      grub_memcpy (outbuf + outoff, buf.out, l);
	    }
	  else
	    {
	      outoff = -outoff;
	      l = buf.out_pos - outoff;
	      if (l > len)
		l = len;
	      grub_memcpy (outbuf, buf.out + outoff, l);
	    }
	  ret += l;
	  len -= l;
	}
      pos += buf.out_pos;
      if (xzret == XZ_STREAM_END)
	break;
    }
  return ret;
}

int LZ4_uncompress_unknownOutputSize(const char *source, char *dest, int isize, int maxOutputSize);
static grub_ssize_t lz4_decompress_wrap(char *inbuf, grub_size_t insize, grub_off_t off, 
    char *outbuf, grub_size_t len, struct grub_squash_data *data)
{
  char *udata = NULL;
  int usize = data->blksz;

  if (usize < 8192)
    usize = 8192;

  udata = grub_malloc (usize);
  if (!udata)
    return -1;

  LZ4_uncompress_unknownOutputSize(inbuf, udata, insize, usize);
  grub_memcpy (outbuf, udata + off, len);
  grub_free (udata);
  return len;
}

static grub_ssize_t zstd_decompress_wrap(char *inbuf, grub_size_t insize, grub_off_t off, 
    char *outbuf, grub_size_t len, struct grub_squash_data *data)
{
  char *udata = NULL;
  int usize = data->blksz;
  if (usize < 8192)
    usize = 8192;

  udata = grub_malloc (usize);
  if (!udata)
    return -1;
  
  ZSTD_decompress(udata, usize, inbuf, insize);      
  grub_memcpy(outbuf, udata + off, len);
  grub_free(udata);
  
  return len;
}

static struct grub_squash_data *
squash_mount (grub_disk_t disk)
{
  struct grub_squash_super sb;
  grub_err_t err;
  struct grub_squash_data *data;
  grub_uint64_t frag;

  err = grub_disk_read (disk, 0, 0, sizeof (sb), &sb);
  if (grub_errno == GRUB_ERR_OUT_OF_RANGE)
    grub_error (GRUB_ERR_BAD_FS, "not a squash4");
  if (err)
    return NULL;
  if (sb.magic != grub_cpu_to_le32_compile_time (SQUASH_MAGIC)
      || sb.block_size == 0
      || ((sb.block_size - 1) & sb.block_size))
    {
      grub_error (GRUB_ERR_BAD_FS, "not squash4");
      return NULL;
    }

  err = grub_disk_read (disk, 
			grub_le_to_cpu64 (sb.unk1offset)
			>> GRUB_DISK_SECTOR_BITS, 
			grub_le_to_cpu64 (sb.unk1offset)
			& (GRUB_DISK_SECTOR_SIZE - 1), sizeof (frag), &frag);
  if (grub_errno == GRUB_ERR_OUT_OF_RANGE)
    grub_error (GRUB_ERR_BAD_FS, "not a squash4");
  if (err)
    return NULL;

  data = grub_zalloc (sizeof (*data));
  if (!data)
    return NULL;
  data->sb = sb;
  data->disk = disk;
  data->fragments = grub_le_to_cpu64 (frag);

  switch (sb.compression)
    {
    case grub_cpu_to_le16_compile_time (COMPRESSION_ZLIB):
      data->decompress = zlib_decompress;
      break;
    case grub_cpu_to_le16_compile_time (COMPRESSION_LZO):
      data->decompress = lzo_decompress;
      break;
    case grub_cpu_to_le16_compile_time (COMPRESSION_LZ4):
      data->decompress = lz4_decompress_wrap;
      break;
    case grub_cpu_to_le16_compile_time (COMPRESSION_ZSTD):
      data->decompress = zstd_decompress_wrap;
      break;
    case grub_cpu_to_le16_compile_time (COMPRESSION_XZ):
      data->decompress = xz_decompress;
      data->xzbuf = grub_malloc (XZBUFSIZ);
      if (!data->xzbuf)
	{
	  grub_free (data);
	  return NULL;
	}
      data->xzdec = xz_dec_init (1 << 16);
      if (!data->xzdec)
	{
	  grub_free (data->xzbuf);
	  grub_free (data);
	  return NULL;
	}
      break;
    default:
      grub_free (data);
      grub_error (GRUB_ERR_BAD_FS, "unsupported compression %d",
		  grub_le_to_cpu16 (sb.compression));
      return NULL;
    }

  data->blksz = grub_le_to_cpu32 (data->sb.block_size);
  for (data->log2_blksz = 0; 
       (1U << data->log2_blksz) < data->blksz;
       data->log2_blksz++);

  return data;
}

static char *
grub_squash_read_symlink (grub_fshelp_node_t node)
{
  char *ret;
  grub_err_t err;
  ret = grub_malloc (grub_le_to_cpu32 (node->ino.symlink.namelen) + 1);

  err = read_chunk (node->data, ret,
		    grub_le_to_cpu32 (node->ino.symlink.namelen),
		    grub_le_to_cpu64 (node->data->sb.inodeoffset)
		    + node->stack[node->stsize - 1].ino_chunk,
		    node->stack[node->stsize - 1].ino_offset
		    + (node->ino.symlink.name - (char *) &node->ino));
  if (err)
    {
      grub_free (ret);
      return NULL;
    }
  ret[grub_le_to_cpu32 (node->ino.symlink.namelen)] = 0;
  return ret;
}

static int
grub_squash_iterate_dir (grub_fshelp_node_t dir,
			 grub_fshelp_iterate_dir_hook_t hook, void *hook_data)
{
  grub_uint32_t off;
  grub_uint32_t endoff;
  grub_uint64_t chunk;
  unsigned i;

  /* FIXME: why - 3 ? */
  switch (dir->ino.type)
    {
    case grub_cpu_to_le16_compile_time (SQUASH_TYPE_DIR):
      off = grub_le_to_cpu16 (dir->ino.dir.offset);
      endoff = grub_le_to_cpu16 (dir->ino.dir.size) + off - 3;
      chunk = grub_le_to_cpu32 (dir->ino.dir.chunk);
      break;
    case grub_cpu_to_le16_compile_time (SQUASH_TYPE_LONG_DIR):
      off = grub_le_to_cpu16 (dir->ino.long_dir.offset);
      endoff = grub_le_to_cpu32 (dir->ino.long_dir.size) + off - 3;
      chunk = grub_le_to_cpu32 (dir->ino.long_dir.chunk);
      break;
    default:
      grub_error (GRUB_ERR_BAD_FS, "unexpected ino type 0x%x",
		  grub_le_to_cpu16 (dir->ino.type));
      return 0;
    }

  {
    grub_fshelp_node_t node;
    node = grub_malloc (sizeof (*node) + dir->stsize * sizeof (dir->stack[0]));
    if (!node)
      return 0;
    grub_memcpy (node, dir,
		 sizeof (*node) + dir->stsize * sizeof (dir->stack[0]));
    if (hook (".", GRUB_FSHELP_DIR, node, hook_data))
      return 1;

    if (dir->stsize != 1)
      {
	grub_err_t err;

	node = grub_malloc (sizeof (*node) + dir->stsize * sizeof (dir->stack[0]));
	if (!node)
	  return 0;

	grub_memcpy (node, dir,
		     sizeof (*node) + dir->stsize * sizeof (dir->stack[0]));

	node->stsize--;
	err = read_chunk (dir->data, &node->ino, sizeof (node->ino),
			  grub_le_to_cpu64 (dir->data->sb.inodeoffset)
			  + node->stack[node->stsize - 1].ino_chunk,
			  node->stack[node->stsize - 1].ino_offset);
	if (err)
	  return 0;

	if (hook ("..", GRUB_FSHELP_DIR, node, hook_data))
	  return 1;
      }
  }

  while (off < endoff)
    {
      struct grub_squash_dirent_header dh;
      grub_err_t err;

      err = read_chunk (dir->data, &dh, sizeof (dh),
			grub_le_to_cpu64 (dir->data->sb.diroffset)
			+ chunk, off);
      if (err)
	return 0;
      off += sizeof (dh);
      for (i = 0; i < (unsigned) grub_le_to_cpu32 (dh.nelems) + 1; i++)
	{
	  char *buf;
	  int r;
	  struct grub_fshelp_node *node;
	  enum grub_fshelp_filetype filetype = GRUB_FSHELP_REG;
	  struct grub_squash_dirent di;
	  struct grub_squash_inode ino;

	  err = read_chunk (dir->data, &di, sizeof (di),
			    grub_le_to_cpu64 (dir->data->sb.diroffset)
			    + chunk, off);
	  if (err)
	    return 0;
	  off += sizeof (di);

	  err = read_chunk (dir->data, &ino, sizeof (ino),
			    grub_le_to_cpu64 (dir->data->sb.inodeoffset)
			    + grub_le_to_cpu32 (dh.ino_chunk),
			    grub_cpu_to_le16 (di.ino_offset));
	  if (err)
	    return 0;

	  buf = grub_malloc (grub_le_to_cpu16 (di.namelen) + 2);
	  if (!buf)
	    return 0;
	  err = read_chunk (dir->data, buf,
			    grub_le_to_cpu16 (di.namelen) + 1,
			    grub_le_to_cpu64 (dir->data->sb.diroffset)
			    + chunk, off);
	  if (err)
	    return 0;

	  off += grub_le_to_cpu16 (di.namelen) + 1;
	  buf[grub_le_to_cpu16 (di.namelen) + 1] = 0;
	  if (grub_le_to_cpu16 (di.type) == SQUASH_TYPE_DIR)
	    filetype = GRUB_FSHELP_DIR;
	  if (grub_le_to_cpu16 (di.type) == SQUASH_TYPE_SYMLINK)
	    filetype = GRUB_FSHELP_SYMLINK;

	  node = grub_malloc (sizeof (*node)
			      + (dir->stsize + 1) * sizeof (dir->stack[0]));
	  if (! node)
	    return 0;

	  grub_memcpy (node, dir,
		       sizeof (*node) + dir->stsize * sizeof (dir->stack[0]));

	  node->ino = ino;
	  node->stack[node->stsize].ino_chunk = grub_le_to_cpu32 (dh.ino_chunk);
	  node->stack[node->stsize].ino_offset = grub_le_to_cpu16 (di.ino_offset);
	  node->stsize++;
	  r = hook (buf, filetype, node, hook_data);

	  grub_free (buf);
	  if (r)
	    return r;
	}
    }
  return 0;
}

static grub_err_t
make_root_node (struct grub_squash_data *data, struct grub_fshelp_node *root)
{
  grub_memset (root, 0, sizeof (*root));
  root->data = data;
  root->stsize = 1;
  root->stack[0].ino_chunk = grub_le_to_cpu32 (data->sb.root_ino_chunk);
  root->stack[0].ino_offset = grub_cpu_to_le16 (data->sb.root_ino_offset);
 return read_chunk (data, &root->ino, sizeof (root->ino),
		    grub_le_to_cpu64 (data->sb.inodeoffset) 
		    + root->stack[0].ino_chunk,
		    root->stack[0].ino_offset);
}

static void
squash_unmount (struct grub_squash_data *data)
{
  if (data->xzdec)
    xz_dec_end (data->xzdec);
  grub_free (data->xzbuf);
  grub_free (data->ino.cumulated_block_sizes);
  grub_free (data->ino.block_sizes);
  grub_free (data);
}


/* Context for grub_squash_dir.  */
struct grub_squash_dir_ctx
{
  grub_fs_dir_hook_t hook;
  void *hook_data;
};

/* Helper for grub_squash_dir.  */
static int
grub_squash_dir_iter (const char *filename, enum grub_fshelp_filetype filetype,
		      grub_fshelp_node_t node, void *data)
{
  struct grub_squash_dir_ctx *ctx = data;
  struct grub_dirhook_info info;

  grub_memset (&info, 0, sizeof (info));
  info.dir = ((filetype & GRUB_FSHELP_TYPE_MASK) == GRUB_FSHELP_DIR);
  info.mtimeset = 1;
  info.mtime = grub_le_to_cpu32 (node->ino.mtime);
  grub_free (node);
  return ctx->hook (filename, &info, ctx->hook_data);
}

static grub_err_t
grub_squash_dir (grub_device_t device, const char *path,
		 grub_fs_dir_hook_t hook, void *hook_data)
{
  struct grub_squash_dir_ctx ctx = { hook, hook_data };
  struct grub_squash_data *data = 0;
  struct grub_fshelp_node *fdiro = 0;
  struct grub_fshelp_node root;
  grub_err_t err;

  data = squash_mount (device->disk);
  if (! data)
    return grub_errno;

  err = make_root_node (data, &root);
  if (err)
    return err;

  grub_fshelp_find_file (path, &root, &fdiro, grub_squash_iterate_dir,
			 grub_squash_read_symlink, GRUB_FSHELP_DIR);
  if (!grub_errno)
    grub_squash_iterate_dir (fdiro, grub_squash_dir_iter, &ctx);

  squash_unmount (data);

  return grub_errno;
}

static grub_err_t
grub_squash_open (struct grub_file *file, const char *name)
{
  struct grub_squash_data *data = 0;
  struct grub_fshelp_node *fdiro = 0;
  struct grub_fshelp_node root;
  grub_err_t err;

  data = squash_mount (file->device->disk);
  if (! data)
    return grub_errno;

  err = make_root_node (data, &root);
  if (err)
    return err;

  grub_fshelp_find_file (name, &root, &fdiro, grub_squash_iterate_dir,
			 grub_squash_read_symlink, GRUB_FSHELP_REG);
  if (grub_errno)
    {
      squash_unmount (data);
      return grub_errno;
    }

  file->data = data;
  data->ino.ino = fdiro->ino;
  data->ino.block_sizes = NULL;
  data->ino.cumulated_block_sizes = NULL;
  data->ino.ino_chunk = fdiro->stack[fdiro->stsize - 1].ino_chunk;
  data->ino.ino_offset = fdiro->stack[fdiro->stsize - 1].ino_offset;

  switch (fdiro->ino.type)
    {
    case grub_cpu_to_le16_compile_time (SQUASH_TYPE_LONG_REGULAR):
      file->size = grub_le_to_cpu64 (fdiro->ino.long_file.size);
      break;
    case grub_cpu_to_le16_compile_time (SQUASH_TYPE_REGULAR):
      file->size = grub_le_to_cpu32 (fdiro->ino.file.size);
      break;
    default:
      {
	grub_uint16_t type = grub_le_to_cpu16 (fdiro->ino.type);
	grub_free (fdiro);
	squash_unmount (data);
	return grub_error (GRUB_ERR_BAD_FS, "unexpected ino type 0x%x", type);
      }
    }

  grub_free (fdiro);

  return GRUB_ERR_NONE;
}

static grub_ssize_t
direct_read (struct grub_squash_data *data, 
	     struct grub_squash_cache_inode *ino,
	     grub_off_t off, char *buf, grub_size_t len)
{
  grub_err_t err;
  grub_off_t cumulated_uncompressed_size = 0;
  grub_uint64_t a = 0;
  grub_size_t i;
  grub_size_t origlen = len;

  switch (ino->ino.type)
    {
    case grub_cpu_to_le16_compile_time (SQUASH_TYPE_LONG_REGULAR):
      a = grub_le_to_cpu64 (ino->ino.long_file.chunk);
      break;
    case grub_cpu_to_le16_compile_time (SQUASH_TYPE_REGULAR):
      a = grub_le_to_cpu32 (ino->ino.file.chunk);
      break;
    }

  if (!ino->block_sizes)
    {
      grub_off_t total_size = 0;
      grub_size_t total_blocks;
      grub_size_t block_offset = 0;
      switch (ino->ino.type)
	{
	case grub_cpu_to_le16_compile_time (SQUASH_TYPE_LONG_REGULAR):
	  total_size = grub_le_to_cpu64 (ino->ino.long_file.size);
	  block_offset = ((char *) &ino->ino.long_file.block_size
			  - (char *) &ino->ino);
	  break;
	case grub_cpu_to_le16_compile_time (SQUASH_TYPE_REGULAR):
	  total_size = grub_le_to_cpu32 (ino->ino.file.size);
	  block_offset = ((char *) &ino->ino.file.block_size
			  - (char *) &ino->ino);
	  break;
	}
      total_blocks = ((total_size + data->blksz - 1) >> data->log2_blksz);
      ino->block_sizes = grub_malloc (total_blocks
				      * sizeof (ino->block_sizes[0]));
      ino->cumulated_block_sizes = grub_malloc (total_blocks
						* sizeof (ino->cumulated_block_sizes[0]));
      if (!ino->block_sizes || !ino->cumulated_block_sizes)
	{
	  grub_free (ino->block_sizes);
	  grub_free (ino->cumulated_block_sizes);
	  ino->block_sizes = 0;
	  ino->cumulated_block_sizes = 0;
	  return -1;
	}
      err = read_chunk (data, ino->block_sizes,
			total_blocks * sizeof (ino->block_sizes[0]),
			grub_le_to_cpu64 (data->sb.inodeoffset)
			+ ino->ino_chunk,
			ino->ino_offset + block_offset);
      if (err)
	{
	  grub_free (ino->block_sizes);
	  grub_free (ino->cumulated_block_sizes);
	  ino->block_sizes = 0;
	  ino->cumulated_block_sizes = 0;
	  return -1;
	}
      ino->cumulated_block_sizes[0] = 0;
      for (i = 1; i < total_blocks; i++)
	ino->cumulated_block_sizes[i] = ino->cumulated_block_sizes[i - 1]
	  + (grub_le_to_cpu32 (ino->block_sizes[i - 1]) & ~SQUASH_BLOCK_FLAGS);
    }

  if (a == 0)
    a = sizeof (struct grub_squash_super);
  i = off >> data->log2_blksz;
  cumulated_uncompressed_size = data->blksz * (grub_disk_addr_t) i;
  while (cumulated_uncompressed_size < off + len)
    {
      grub_size_t boff, curread;
      boff = off - cumulated_uncompressed_size;
      curread = data->blksz - boff;
      if (curread > len)
	curread = len;
      if (!ino->block_sizes[i])
	{
	  /* Sparse block */
	  grub_memset (buf, '\0', curread);
	}
      else if (!(ino->block_sizes[i]
	    & grub_cpu_to_le32_compile_time (SQUASH_BLOCK_UNCOMPRESSED)))
	{
	  char *block;
	  grub_size_t csize;
	  csize = grub_le_to_cpu32 (ino->block_sizes[i]) & ~SQUASH_BLOCK_FLAGS;
	  block = grub_malloc (csize);
	  if (!block)
	    return -1;
	  err = grub_disk_read (data->disk,
				(ino->cumulated_block_sizes[i] + a)
				>> GRUB_DISK_SECTOR_BITS,
				(ino->cumulated_block_sizes[i] + a)
				& (GRUB_DISK_SECTOR_SIZE - 1),
				csize, block);
	  if (err)
	    {
	      grub_free (block);
	      return -1;
	    }
	  if (data->decompress (block, csize, boff, buf, curread, data)
	      != (grub_ssize_t) curread)
	    {
	      grub_free (block);
	      if (!grub_errno)
		grub_error (GRUB_ERR_BAD_FS, "incorrect compressed chunk");
	      return -1;
	    }
	  grub_free (block);
	}
      else
	err = grub_disk_read (data->disk, 
			      (ino->cumulated_block_sizes[i] + a + boff)
			      >> GRUB_DISK_SECTOR_BITS,
			      (ino->cumulated_block_sizes[i] + a + boff)
			      & (GRUB_DISK_SECTOR_SIZE - 1),
			      curread, buf);
      if (err)
	return -1;
      off += curread;
      len -= curread;
      buf += curread;
      cumulated_uncompressed_size += grub_le_to_cpu32 (data->sb.block_size);
      i++;
    }
  return origlen;
}


static grub_ssize_t
grub_squash_read (grub_file_t file, char *buf, grub_size_t len)
{
  struct grub_squash_data *data = file->data;
  struct grub_squash_cache_inode *ino = &data->ino;
  grub_off_t off = file->offset;
  grub_err_t err;
  grub_uint64_t a, b;
  grub_uint32_t fragment = 0;
  int compressed = 0;
  struct grub_squash_frag_desc frag;
  grub_off_t direct_len;
  grub_uint64_t mask = grub_le_to_cpu32 (data->sb.block_size) - 1;
  grub_size_t orig_len = len;

  switch (ino->ino.type)
    {
    case grub_cpu_to_le16_compile_time (SQUASH_TYPE_LONG_REGULAR):
      fragment = grub_le_to_cpu32 (ino->ino.long_file.fragment);
      break;
    case grub_cpu_to_le16_compile_time (SQUASH_TYPE_REGULAR):
      fragment = grub_le_to_cpu32 (ino->ino.file.fragment);
      break;
    }

  /* Squash may pack file tail as fragment. So read initial part directly and
     get tail from fragments */
  direct_len = fragment == 0xffffffff ? file->size : file->size & ~mask;
  if (off < direct_len)
    {
      grub_size_t read_len = direct_len - off;
      grub_ssize_t res;

      if (read_len > len)
	read_len = len;
      res = direct_read (data, ino, off, buf, read_len);
      if ((grub_size_t) res != read_len)
	return -1; /* FIXME: is short read possible here? */
      len -= read_len;
      if (!len)
	return read_len;
      buf += read_len;
      off = 0;
    }
  else
    off -= direct_len;
 
  err = read_chunk (data, &frag, sizeof (frag),
		    data->fragments, sizeof (frag) * fragment);
  if (err)
    return -1;
  a = grub_le_to_cpu64 (frag.offset);
  compressed = !(frag.size & grub_cpu_to_le32_compile_time (SQUASH_BLOCK_UNCOMPRESSED));
  if (ino->ino.type == grub_cpu_to_le16_compile_time (SQUASH_TYPE_LONG_REGULAR))
    b = grub_le_to_cpu32 (ino->ino.long_file.offset) + off;
  else
    b = grub_le_to_cpu32 (ino->ino.file.offset) + off;
  
  /* FIXME: cache uncompressed chunks.  */
  if (compressed)
    {
      char *block;
      block = grub_malloc (grub_le_to_cpu32 (frag.size));
      if (!block)
	return -1;
      err = grub_disk_read (data->disk,
			    a >> GRUB_DISK_SECTOR_BITS,
			    a & (GRUB_DISK_SECTOR_SIZE - 1),
			    grub_le_to_cpu32 (frag.size), block);
      if (err)
	{
	  grub_free (block);
	  return -1;
	}
      if (data->decompress (block, grub_le_to_cpu32 (frag.size),
			    b, buf, len, data)
	  != (grub_ssize_t) len)
	{
	  grub_free (block);
	  if (!grub_errno)
	    grub_error (GRUB_ERR_BAD_FS, "incorrect compressed chunk");
	  return -1;
	}
      grub_free (block);
    }
  else
    {
      err = grub_disk_read (data->disk, (a + b) >> GRUB_DISK_SECTOR_BITS,
			  (a + b) & (GRUB_DISK_SECTOR_SIZE - 1), len, buf);
      if (err)
	return -1;
    }
  return orig_len;
}

static grub_err_t
grub_squash_close (grub_file_t file)
{
  squash_unmount (file->data);
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_squash_mtime (grub_device_t dev, grub_int32_t *tm)
{
  struct grub_squash_data *data = 0;

  data = squash_mount (dev->disk);
  if (! data)
    return grub_errno;
  *tm = grub_le_to_cpu32 (data->sb.creation_time);
  squash_unmount (data);
  return GRUB_ERR_NONE;
} 

static struct grub_fs grub_squash_fs =
  {
    .name = "squash4",
    .fs_dir = grub_squash_dir,
    .fs_open = grub_squash_open,
    .fs_read = grub_squash_read,
    .fs_close = grub_squash_close,
    .fs_mtime = grub_squash_mtime,
#ifdef GRUB_UTIL
    .reserved_first_sector = 0,
    .blocklist_install = 0,
#endif
    .next = 0
  };

GRUB_MOD_INIT(squash4)
{
  grub_fs_register (&grub_squash_fs);
}

GRUB_MOD_FINI(squash4)
{
  grub_fs_unregister (&grub_squash_fs);
}

