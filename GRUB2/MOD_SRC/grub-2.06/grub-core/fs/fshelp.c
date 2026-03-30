/* fshelp.c -- Filesystem helper functions */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2004,2005,2006,2007,2008  Free Software Foundation, Inc.
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
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/disk.h>
#include <grub/fshelp.h>
#include <grub/dl.h>
#include <grub/i18n.h>

GRUB_MOD_LICENSE ("GPLv3+");

typedef int (*iterate_dir_func) (grub_fshelp_node_t dir,
				 grub_fshelp_iterate_dir_hook_t hook,
				 void *data);
typedef grub_err_t (*lookup_file_func) (grub_fshelp_node_t dir,
					const char *name,
					grub_fshelp_node_t *foundnode,
					enum grub_fshelp_filetype *foundtype);
typedef char *(*read_symlink_func) (grub_fshelp_node_t node);

struct stack_element {
  struct stack_element *parent;
  grub_fshelp_node_t node;
  enum grub_fshelp_filetype type;
};

/* Context for grub_fshelp_find_file.  */
struct grub_fshelp_find_file_ctx
{
  /* Inputs.  */
  const char *path;
  grub_fshelp_node_t rootnode;

  /* Global options. */
  int symlinknest;

  /* Current file being traversed and its parents.  */
  struct stack_element *currnode;
};

/* Helper for find_file_iter.  */
static void
free_node (grub_fshelp_node_t node, struct grub_fshelp_find_file_ctx *ctx)
{
  if (node != ctx->rootnode)
    grub_free (node);
}

static void
pop_element (struct grub_fshelp_find_file_ctx *ctx)
{
  struct stack_element *el;
  el = ctx->currnode;
  ctx->currnode = el->parent;
  free_node (el->node, ctx);
  grub_free (el);
}

static void
free_stack (struct grub_fshelp_find_file_ctx *ctx)
{
  while (ctx->currnode)
    pop_element (ctx);
}

static void
go_up_a_level (struct grub_fshelp_find_file_ctx *ctx)
{
  if (!ctx->currnode->parent)
    return;
  pop_element (ctx);
}

static grub_err_t
push_node (struct grub_fshelp_find_file_ctx *ctx, grub_fshelp_node_t node, enum grub_fshelp_filetype filetype)
{
  struct stack_element *nst;
  nst = grub_malloc (sizeof (*nst));
  if (!nst)
    return grub_errno;
  nst->node = node;
  nst->type = filetype & ~GRUB_FSHELP_CASE_INSENSITIVE;
  nst->parent = ctx->currnode;
  ctx->currnode = nst;
  return GRUB_ERR_NONE;
}

static grub_err_t
go_to_root (struct grub_fshelp_find_file_ctx *ctx)
{
  free_stack (ctx);
  return push_node (ctx, ctx->rootnode, GRUB_FSHELP_DIR);
}

struct grub_fshelp_find_file_iter_ctx
{
  const char *name;
  grub_fshelp_node_t *foundnode;
  enum grub_fshelp_filetype *foundtype;
};

int g_ventoy_case_insensitive = 0;

/* Helper for grub_fshelp_find_file.  */
static int
find_file_iter (const char *filename, enum grub_fshelp_filetype filetype,
		grub_fshelp_node_t node, void *data)
{
  struct grub_fshelp_find_file_iter_ctx *ctx = data;

  if (g_ventoy_case_insensitive)
  {
      filetype |= GRUB_FSHELP_CASE_INSENSITIVE;
  }

  if (filetype == GRUB_FSHELP_UNKNOWN ||
      ((filetype & GRUB_FSHELP_CASE_INSENSITIVE)
       ? grub_strcasecmp (ctx->name, filename)
       : grub_strcmp (ctx->name, filename)))
    {
      grub_free (node);
      return 0;
    }

  /* The node is found, stop iterating over the nodes.  */
  *ctx->foundnode = node;
  *ctx->foundtype = filetype;
  return 1;
}

static grub_err_t
directory_find_file (grub_fshelp_node_t node, const char *name, grub_fshelp_node_t *foundnode,
		     enum grub_fshelp_filetype *foundtype, iterate_dir_func iterate_dir)
{
  int found;
  struct grub_fshelp_find_file_iter_ctx ctx = {
    .foundnode = foundnode,
    .foundtype = foundtype,
    .name = name
  };
  found = iterate_dir (node, find_file_iter, &ctx);
  if (! found)
    {
      if (grub_errno)
	return grub_errno;
    }
  return GRUB_ERR_NONE;
}

static grub_err_t
find_file (char *currpath,
	   iterate_dir_func iterate_dir, lookup_file_func lookup_file,
	   read_symlink_func read_symlink,
	   struct grub_fshelp_find_file_ctx *ctx)
{
  char *name, *next;
  grub_err_t err;
  for (name = currpath; ; name = next)
    {
      char c;
      grub_fshelp_node_t foundnode = NULL;
      enum grub_fshelp_filetype foundtype = 0;

      /* Remove all leading slashes.  */
      while (*name == '/')
	name++;

      /* Found the node!  */
      if (! *name)
	return 0;

      /* Extract the actual part from the pathname.  */
      for (next = name; *next && *next != '/'; next++);

      /* At this point it is expected that the current node is a
	 directory, check if this is true.  */
      if (ctx->currnode->type != GRUB_FSHELP_DIR)
	return grub_error (GRUB_ERR_BAD_FILE_TYPE, N_("not a directory"));

      /* Don't rely on fs providing actual . in the listing.  */
      if (next - name == 1 && name[0] == '.')
	continue;

      /* Don't rely on fs providing actual .. in the listing.  */
      if (next - name == 2 && name[0] == '.' && name[1] == '.')
	{
	  go_up_a_level (ctx);
	  continue;
	}

      /* Iterate over the directory.  */
      c = *next;
      *next = '\0';
      if (lookup_file)
	err = lookup_file (ctx->currnode->node, name, &foundnode, &foundtype);
      else
	err = directory_find_file (ctx->currnode->node, name, &foundnode, &foundtype, iterate_dir);
      *next = c;

      if (err)
	return err;

      if (!foundnode)
	break;

      push_node (ctx, foundnode, foundtype);
 
      /* Read in the symlink and follow it.  */
      if (ctx->currnode->type == GRUB_FSHELP_SYMLINK)
	{
	  char *symlink;

	  /* Test if the symlink does not loop.  */
	  if (++ctx->symlinknest == 8)
	    return grub_error (GRUB_ERR_SYMLINK_LOOP,
			       N_("too deep nesting of symlinks"));

	  symlink = read_symlink (ctx->currnode->node);

	  if (!symlink)
	    return grub_errno;

	  /* The symlink is an absolute path, go back to the root inode.  */
	  if (symlink[0] == '/')
	    {
	      err = go_to_root (ctx);
	      if (err)
		return err;
	    }
	  else
	    {
	      /* Get from symlink to containing directory. */
	      go_up_a_level (ctx);
	    }


	  /* Lookup the node the symlink points to.  */
	  find_file (symlink, iterate_dir, lookup_file, read_symlink, ctx);
	  grub_free (symlink);

	  if (grub_errno)
	    return grub_errno;
	}
    }

  return grub_error (GRUB_ERR_FILE_NOT_FOUND, N_("file `%s' not found"),
		     ctx->path);
}

static grub_err_t
grub_fshelp_find_file_real (const char *path, grub_fshelp_node_t rootnode,
			    grub_fshelp_node_t *foundnode,
			    iterate_dir_func iterate_dir,
			    lookup_file_func lookup_file,
			    read_symlink_func read_symlink,
			    enum grub_fshelp_filetype expecttype)
{
  struct grub_fshelp_find_file_ctx ctx = {
    .path = path,
    .rootnode = rootnode,
    .symlinknest = 0,
    .currnode = 0
  };
  grub_err_t err;
  enum grub_fshelp_filetype foundtype;
  char *duppath;

  if (!path || path[0] != '/')
    {
      return grub_error (GRUB_ERR_BAD_FILENAME, N_("invalid file name `%s'"), path);
    }

  err = go_to_root (&ctx);
  if (err)
    return err;

  duppath = grub_strdup (path);
  if (!duppath)
    return grub_errno;
  err = find_file (duppath, iterate_dir, lookup_file, read_symlink, &ctx);
  grub_free (duppath);
  if (err)
    {
      free_stack (&ctx);
      return err;
    }

  *foundnode = ctx.currnode->node;
  foundtype = ctx.currnode->type;
  /* Avoid the node being freed.  */
  ctx.currnode->node = 0;
  free_stack (&ctx);

  /* Check if the node that was found was of the expected type.  */
  if (expecttype == GRUB_FSHELP_REG && foundtype != expecttype)
    return grub_error (GRUB_ERR_BAD_FILE_TYPE, N_("not a regular file"));
  else if (expecttype == GRUB_FSHELP_DIR && foundtype != expecttype)
    return grub_error (GRUB_ERR_BAD_FILE_TYPE, N_("not a directory"));

  return 0;
}

/* Lookup the node PATH.  The node ROOTNODE describes the root of the
   directory tree.  The node found is returned in FOUNDNODE, which is
   either a ROOTNODE or a new malloc'ed node.  ITERATE_DIR is used to
   iterate over all directory entries in the current node.
   READ_SYMLINK is used to read the symlink if a node is a symlink.
   EXPECTTYPE is the type node that is expected by the called, an
   error is generated if the node is not of the expected type.  */
grub_err_t
grub_fshelp_find_file (const char *path, grub_fshelp_node_t rootnode,
		       grub_fshelp_node_t *foundnode,
		       iterate_dir_func iterate_dir,
		       read_symlink_func read_symlink,
		       enum grub_fshelp_filetype expecttype)
{
  return grub_fshelp_find_file_real (path, rootnode, foundnode,
				     iterate_dir, NULL, 
				     read_symlink, expecttype);

}

grub_err_t
grub_fshelp_find_file_lookup (const char *path, grub_fshelp_node_t rootnode,
			      grub_fshelp_node_t *foundnode,
			      lookup_file_func lookup_file,
			      read_symlink_func read_symlink,
			      enum grub_fshelp_filetype expecttype)
{
  return grub_fshelp_find_file_real (path, rootnode, foundnode,
				     NULL, lookup_file, 
				     read_symlink, expecttype);

}

/* Read LEN bytes from the file NODE on disk DISK into the buffer BUF,
   beginning with the block POS.  READ_HOOK should be set before
   reading a block from the file.  READ_HOOK_DATA is passed through as
   the DATA argument to READ_HOOK.  GET_BLOCK is used to translate
   file blocks to disk blocks.  The file is FILESIZE bytes big and the
   blocks have a size of LOG2BLOCKSIZE (in log2).  */
grub_ssize_t
grub_fshelp_read_file (grub_disk_t disk, grub_fshelp_node_t node,
		       grub_disk_read_hook_t read_hook, void *read_hook_data,
		       grub_off_t pos, grub_size_t len, char *buf,
		       grub_disk_addr_t (*get_block) (grub_fshelp_node_t node,
                                                      grub_disk_addr_t block),
		       grub_off_t filesize, int log2blocksize,
		       grub_disk_addr_t blocks_start)
{
  grub_disk_addr_t i, blockcnt;
  int blocksize = 1 << (log2blocksize + GRUB_DISK_SECTOR_BITS);

  if (pos > filesize)
    {
      grub_error (GRUB_ERR_OUT_OF_RANGE,
		  N_("attempt to read past the end of file"));
      return -1;
    }

  /* Adjust LEN so it we can't read past the end of the file.  */
  if (pos + len > filesize)
    len = filesize - pos;

  blockcnt = ((len + pos) + blocksize - 1) >> (log2blocksize + GRUB_DISK_SECTOR_BITS);

  for (i = pos >> (log2blocksize + GRUB_DISK_SECTOR_BITS); i < blockcnt; i++)
    {
      grub_disk_addr_t blknr;
      int blockoff = pos & (blocksize - 1);
      int blockend = blocksize;

      int skipfirst = 0;

      blknr = get_block (node, i);
      if (grub_errno)
	return -1;

      blknr = blknr << log2blocksize;

      /* Last block.  */
      if (i == blockcnt - 1)
	{
	  blockend = (len + pos) & (blocksize - 1);

	  /* The last portion is exactly blocksize.  */
	  if (! blockend)
	    blockend = blocksize;
	}

      /* First block.  */
      if (i == (pos >> (log2blocksize + GRUB_DISK_SECTOR_BITS)))
	{
	  skipfirst = blockoff;
	  blockend -= skipfirst;
	}

      /* If the block number is 0 this block is not stored on disk but
	 is zero filled instead.  */
      if (blknr)
	{
	  disk->read_hook = read_hook;
	  disk->read_hook_data = read_hook_data;

	  grub_disk_read (disk, blknr + blocks_start, skipfirst,
			  blockend, buf);
	  disk->read_hook = 0;
	  if (grub_errno)
	    return -1;
	}
      else if (read_hook != (grub_disk_read_hook_t)(void *)grub_disk_blocklist_read)
	grub_memset (buf, 0, blockend);

      buf += blocksize - skipfirst;
    }

  return len;
}
