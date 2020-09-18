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

static grub_envblk_t UNUSED
read_envblk_file (grub_file_t file)
{
  grub_off_t offset = 0;
  char *buf;
  grub_size_t size = grub_file_size (file);
  grub_envblk_t envblk;

  buf = grub_malloc (size);
  if (! buf)
    return 0;

  while (size > 0)
    {
      grub_ssize_t ret;

      ret = grub_file_read (file, buf + offset, size);
      if (ret <= 0)
        {
          grub_free (buf);
          return 0;
        }

      size -= ret;
      offset += ret;
    }

  envblk = grub_envblk_open (buf, offset);
  if (! envblk)
    {
      grub_free (buf);
      grub_error (GRUB_ERR_BAD_FILE_TYPE, "invalid environment block");
      return 0;
    }

  return envblk;
}

struct grub_env_whitelist
{
  grub_size_t len;
  char **list;
};
typedef struct grub_env_whitelist grub_env_whitelist_t;

static int UNUSED
test_whitelist_membership (const char* name,
                           const grub_env_whitelist_t* whitelist)
{
  grub_size_t i;

  for (i = 0; i < whitelist->len; i++)
    if (grub_strcmp (name, whitelist->list[i]) == 0)
      return 1;  /* found it */

  return 0;  /* not found */
}

/* Helper for grub_cmd_load_env.  */
static int UNUSED
set_var (const char *name, const char *value, void *whitelist)
{
  if (! whitelist)
    {
      grub_env_set (name, value);
      return 0;
    }

  if (test_whitelist_membership (name,
				 (const grub_env_whitelist_t *) whitelist))
    grub_env_set (name, value);

  return 0;
}
