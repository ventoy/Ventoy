/* loadenv.c - command to load/save environment variable.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008,2009,2010  Free Software Foundation, Inc.
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

#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/file.h>
#include <grub/disk.h>
#include <grub/misc.h>
#include <grub/env.h>
#include <grub/partition.h>
#include <grub/lib/envblk.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>

#include "loadenv.h"

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options[] =
  {
    /* TRANSLATORS: This option is used to override default filename
       for loading and storing environment.  */
    {"file", 'f', 0, N_("Specify filename."), 0, ARG_TYPE_PATHNAME},
    {"skip-sig", 's', 0,
     N_("Skip signature-checking of the environment file."), 0, ARG_TYPE_NONE},
    {0, 0, 0, 0, 0, 0}
  };

/* Opens 'filename' with compression filters disabled. Optionally disables the
   PUBKEY filter (that insists upon properly signed files) as well.  PUBKEY
   filter is restored before the function returns. */
static grub_file_t
open_envblk_file (char *filename,
		  enum grub_file_type type)
{
  grub_file_t file;
  char *buf = 0;

  if (! filename)
    {
      const char *prefix;
      int len;

      prefix = grub_env_get ("prefix");
      if (! prefix)
        {
          grub_error (GRUB_ERR_FILE_NOT_FOUND, N_("variable `%s' isn't set"), "prefix");
          return 0;
        }

      len = grub_strlen (prefix);
      buf = grub_malloc (len + 1 + sizeof (GRUB_ENVBLK_DEFCFG));
      if (! buf)
        return 0;
      filename = buf;

      grub_strcpy (filename, prefix);
      filename[len] = '/';
      grub_strcpy (filename + len + 1, GRUB_ENVBLK_DEFCFG);
    }

  file = grub_file_open (filename, type);

  grub_free (buf);
  return file;
}

static grub_err_t
grub_cmd_load_env (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  grub_file_t file;
  grub_envblk_t envblk;
  grub_env_whitelist_t whitelist;

  whitelist.len = argc;
  whitelist.list = args;

  /* state[0] is the -f flag; state[1] is the --skip-sig flag */
  file = open_envblk_file ((state[0].set) ? state[0].arg : 0,
			   GRUB_FILE_TYPE_LOADENV
			   | (state[1].set
			      ? GRUB_FILE_TYPE_SKIP_SIGNATURE : GRUB_FILE_TYPE_NONE));
  if (! file)
    return grub_errno;

  envblk = read_envblk_file (file);
  if (! envblk)
    goto fail;

  /* argc > 0 indicates caller provided a whitelist of variables to read. */
  grub_envblk_iterate (envblk, argc > 0 ? &whitelist : 0, set_var);
  grub_envblk_close (envblk);

 fail:
  grub_file_close (file);
  return grub_errno;
}

/* Print all variables in current context.  */
static int
print_var (const char *name, const char *value,
           void *hook_data __attribute__ ((unused)))
{
  grub_printf ("%s=%s\n", name, value);
  return 0;
}

static grub_err_t
grub_cmd_list_env (grub_extcmd_context_t ctxt,
		   int argc __attribute__ ((unused)),
		   char **args __attribute__ ((unused)))
{
  struct grub_arg_list *state = ctxt->state;
  grub_file_t file;
  grub_envblk_t envblk;

  file = open_envblk_file ((state[0].set) ? state[0].arg : 0,
			   GRUB_FILE_TYPE_LOADENV
			   | (state[1].set
			      ? GRUB_FILE_TYPE_SKIP_SIGNATURE : GRUB_FILE_TYPE_NONE));
  if (! file)
    return grub_errno;

  envblk = read_envblk_file (file);
  if (! envblk)
    goto fail;

  grub_envblk_iterate (envblk, NULL, print_var);
  grub_envblk_close (envblk);

 fail:
  grub_file_close (file);
  return grub_errno;
}

/* Used to maintain a variable length of blocklists internally.  */
struct blocklist
{
  grub_disk_addr_t sector;
  unsigned offset;
  unsigned length;
  struct blocklist *next;
};

static void
free_blocklists (struct blocklist *p)
{
  struct blocklist *q;

  for (; p; p = q)
    {
      q = p->next;
      grub_free (p);
    }
}

static grub_err_t
check_blocklists (grub_envblk_t envblk, struct blocklist *blocklists,
                  grub_file_t file)
{
  grub_size_t total_length;
  grub_size_t index;
  grub_disk_t disk;
  grub_disk_addr_t part_start;
  struct blocklist *p;
  char *buf;

  /* Sanity checks.  */
  total_length = 0;
  for (p = blocklists; p; p = p->next)
    {
      struct blocklist *q;
      /* Check if any pair of blocks overlap.  */
      for (q = p->next; q; q = q->next)
        {
	  grub_disk_addr_t s1, s2;
	  grub_disk_addr_t e1, e2;

	  s1 = p->sector;
	  e1 = s1 + ((p->length + GRUB_DISK_SECTOR_SIZE - 1) >> GRUB_DISK_SECTOR_BITS);

	  s2 = q->sector;
	  e2 = s2 + ((q->length + GRUB_DISK_SECTOR_SIZE - 1) >> GRUB_DISK_SECTOR_BITS);

	  if (s1 < e2 && s2 < e1)
            {
              /* This might be actually valid, but it is unbelievable that
                 any filesystem makes such a silly allocation.  */
              return grub_error (GRUB_ERR_BAD_FS, "malformed file");
            }
        }

      total_length += p->length;
    }

  if (total_length != grub_file_size (file))
    {
      /* Maybe sparse, unallocated sectors. No way in GRUB.  */
      return grub_error (GRUB_ERR_BAD_FILE_TYPE, "sparse file not allowed");
    }

  /* One more sanity check. Re-read all sectors by blocklists, and compare
     those with the data read via a file.  */
  disk = file->device->disk;

  part_start = grub_partition_get_start (disk->partition);

  buf = grub_envblk_buffer (envblk);
  char *blockbuf = NULL;
  grub_size_t blockbuf_len = 0;
  for (p = blocklists, index = 0; p; index += p->length, p = p->next)
    {
      if (p->length > blockbuf_len)
	{
	  grub_free (blockbuf);
	  blockbuf_len = 2 * p->length;
	  blockbuf = grub_malloc (blockbuf_len);
	  if (!blockbuf)
	    return grub_errno;
	}

      if (grub_disk_read (disk, p->sector - part_start,
                          p->offset, p->length, blockbuf))
        return grub_errno;

      if (grub_memcmp (buf + index, blockbuf, p->length) != 0)
	return grub_error (GRUB_ERR_FILE_READ_ERROR, "invalid blocklist");
    }

  return GRUB_ERR_NONE;
}

static int
write_blocklists (grub_envblk_t envblk, struct blocklist *blocklists,
                  grub_file_t file)
{
  char *buf;
  grub_disk_t disk;
  grub_disk_addr_t part_start;
  struct blocklist *p;
  grub_size_t index;

  buf = grub_envblk_buffer (envblk);
  disk = file->device->disk;
  part_start = grub_partition_get_start (disk->partition);

  index = 0;
  for (p = blocklists; p; index += p->length, p = p->next)
    {
      if (grub_disk_write (disk, p->sector - part_start,
                           p->offset, p->length, buf + index))
        return 0;
    }

  return 1;
}

/* Context for grub_cmd_save_env.  */
struct grub_cmd_save_env_ctx
{
  struct blocklist *head, *tail;
};

/* Store blocklists in a linked list.  */
static void
save_env_read_hook (grub_disk_addr_t sector, unsigned offset, unsigned length,
		    void *data)
{
  struct grub_cmd_save_env_ctx *ctx = data;
  struct blocklist *block;

  block = grub_malloc (sizeof (*block));
  if (! block)
    return;

  block->sector = sector;
  block->offset = offset;
  block->length = length;

  /* Slightly complicated, because the list should be FIFO.  */
  block->next = 0;
  if (ctx->tail)
    ctx->tail->next = block;
  ctx->tail = block;
  if (! ctx->head)
    ctx->head = block;
}

static grub_err_t
grub_cmd_save_env (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  grub_file_t file;
  grub_envblk_t envblk;
  struct grub_cmd_save_env_ctx ctx = {
    .head = 0,
    .tail = 0
  };

  if (! argc)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "no variable is specified");

  file = open_envblk_file ((state[0].set) ? state[0].arg : 0,
			   GRUB_FILE_TYPE_SAVEENV
			   | GRUB_FILE_TYPE_SKIP_SIGNATURE);
  if (! file)
    return grub_errno;

  if (! file->device->disk)
    {
      grub_file_close (file);
      return grub_error (GRUB_ERR_BAD_DEVICE, "disk device required");
    }

  file->read_hook = save_env_read_hook;
  file->read_hook_data = &ctx;
  envblk = read_envblk_file (file);
  file->read_hook = 0;
  if (! envblk)
    goto fail;

  if (check_blocklists (envblk, ctx.head, file))
    goto fail;

  while (argc)
    {
      const char *value;

      value = grub_env_get (args[0]);
      if (value)
        {
          if (! grub_envblk_set (envblk, args[0], value))
            {
              grub_error (GRUB_ERR_BAD_ARGUMENT, "environment block too small");
              goto fail;
            }
        }
      else
	grub_envblk_delete (envblk, args[0]);

      argc--;
      args++;
    }

  write_blocklists (envblk, ctx.head, file);

 fail:
  if (envblk)
    grub_envblk_close (envblk);
  free_blocklists (ctx.head);
  grub_file_close (file);
  return grub_errno;
}

static grub_extcmd_t cmd_load, cmd_list, cmd_save;

GRUB_MOD_INIT(loadenv)
{
  cmd_load =
    grub_register_extcmd ("load_env", grub_cmd_load_env, 0,
			  N_("[-f FILE] [-s|--skip-sig] [variable_name_to_whitelist] [...]"),
			  N_("Load variables from environment block file."),
			  options);
  cmd_list =
    grub_register_extcmd ("list_env", grub_cmd_list_env, 0, N_("[-f FILE]"),
			  N_("List variables from environment block file."),
			  options);
  cmd_save =
    grub_register_extcmd ("save_env", grub_cmd_save_env, 0,
			  N_("[-f FILE] variable_name [...]"),
			  N_("Save variables to environment block file."),
			  options);
}

GRUB_MOD_FINI(loadenv)
{
  grub_unregister_extcmd (cmd_load);
  grub_unregister_extcmd (cmd_list);
  grub_unregister_extcmd (cmd_save);
}
