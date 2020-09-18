/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000, 2001, 2010  Free Software Foundation, Inc.
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

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/command.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/file.h>
#include <grub/normal.h>
#include <grub/script_sh.h>
#include <grub/i18n.h>
#include <grub/term.h>
#include <grub/legacy_parse.h>
#include <grub/crypto.h>
#include <grub/auth.h>
#include <grub/disk.h>
#include <grub/partition.h>

GRUB_MOD_LICENSE ("GPLv3+");

/* Helper for legacy_file.  */
static grub_err_t
legacy_file_getline (char **line, int cont __attribute__ ((unused)),
		     void *data __attribute__ ((unused)))
{
  *line = 0;
  return GRUB_ERR_NONE;
}

static grub_err_t
legacy_file (const char *filename)
{
  grub_file_t file;
  char *entryname = NULL, *entrysrc = NULL;
  grub_menu_t menu;
  char *suffix = grub_strdup ("");

  if (!suffix)
    return grub_errno;

  file = grub_file_open (filename, GRUB_FILE_TYPE_CONFIG);
  if (! file)
    {
      grub_free (suffix);
      return grub_errno;
    }

  menu = grub_env_get_menu ();
  if (! menu)
    {
      menu = grub_zalloc (sizeof (*menu));
      if (! menu)
	{
	  grub_free (suffix);
	  return grub_errno;
	}

      grub_env_set_menu (menu);
    }

  while (1)
    {
      char *buf = grub_file_getline (file);
      char *parsed = NULL;

      if (!buf && grub_errno)
	{
	  grub_file_close (file);
	  grub_free (suffix);
	  return grub_errno;
	}

      if (!buf)
	break;

      {
	char *oldname = NULL;
	char *newsuffix;
	char *ptr;

	for (ptr = buf; *ptr && grub_isspace (*ptr); ptr++);

	oldname = entryname;
	parsed = grub_legacy_parse (ptr, &entryname, &newsuffix);
	grub_free (buf);
	buf = NULL;
	if (newsuffix)
	  {
	    char *t;
	    
	    t = suffix;
	    suffix = grub_realloc (suffix, grub_strlen (suffix)
				   + grub_strlen (newsuffix) + 1);
	    if (!suffix)
	      {
		grub_free (t);
		grub_free (entrysrc);
		grub_free (parsed);
		grub_free (newsuffix);
		grub_free (suffix);
		return grub_errno;
	      }
	    grub_memcpy (suffix + grub_strlen (suffix), newsuffix,
			 grub_strlen (newsuffix) + 1);
	    grub_free (newsuffix);
	    newsuffix = NULL;
	  }
	if (oldname != entryname && oldname)
	  {
	    const char **args = grub_malloc (sizeof (args[0]));
	    if (!args)
	      {
		grub_file_close (file);
		return grub_errno;
	      }
	    args[0] = oldname;
	    grub_normal_add_menu_entry (1, args, NULL, NULL, "legacy",
					NULL, NULL,
					entrysrc, 0, NULL, NULL);
	    grub_free (args);
	    entrysrc[0] = 0;
	    grub_free (oldname);
	  }
      }

      if (parsed && !entryname)
	{
	  grub_normal_parse_line (parsed, legacy_file_getline, NULL);
	  grub_print_error ();
	  grub_free (parsed);
	  parsed = NULL;
	}
      else if (parsed)
	{
	  if (!entrysrc)
	    entrysrc = parsed;
	  else
	    {
	      char *t;

	      t = entrysrc;
	      entrysrc = grub_realloc (entrysrc, grub_strlen (entrysrc)
				       + grub_strlen (parsed) + 1);
	      if (!entrysrc)
		{
		  grub_free (t);
		  grub_free (parsed);
		  grub_free (suffix);
		  return grub_errno;
		}
	      grub_memcpy (entrysrc + grub_strlen (entrysrc), parsed,
			   grub_strlen (parsed) + 1);
	      grub_free (parsed);
	      parsed = NULL;
	    }
	}
    }
  grub_file_close (file);

  if (entryname)
    {
      const char **args = grub_malloc (sizeof (args[0]));
      if (!args)
	{
	  grub_file_close (file);
	  grub_free (suffix);
	  grub_free (entrysrc);
	  return grub_errno;
	}
      args[0] = entryname;
      grub_normal_add_menu_entry (1, args, NULL, NULL, NULL,
				  NULL, NULL, entrysrc, 0, NULL,
				  NULL);
      grub_free (args);
    }

  grub_normal_parse_line (suffix, legacy_file_getline, NULL);
  grub_print_error ();
  grub_free (suffix);
  grub_free (entrysrc);

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_legacy_source (struct grub_command *cmd,
			int argc, char **args)
{
  int new_env, extractor;
  grub_err_t ret;

  if (argc != 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  extractor = (cmd->name[0] == 'e');
  new_env = (cmd->name[extractor ? (sizeof ("extract_legacy_entries_") - 1)
		       : (sizeof ("legacy_") - 1)] == 'c');

  if (new_env)
    grub_cls ();

  if (new_env && !extractor)
    grub_env_context_open ();
  if (extractor)
    grub_env_extractor_open (!new_env);

  ret = legacy_file (args[0]);

  if (new_env)
    {
      grub_menu_t menu;
      menu = grub_env_get_menu ();
      if (menu && menu->size)
	grub_show_menu (menu, 1, 0);
      if (!extractor)
	grub_env_context_close ();
    }
  if (extractor)
    grub_env_extractor_close (!new_env);

  return ret;
}

static enum
  { 
    GUESS_IT, LINUX, MULTIBOOT, KFREEBSD, KNETBSD, KOPENBSD 
  } kernel_type;

static grub_err_t
grub_cmd_legacy_kernel (struct grub_command *mycmd __attribute__ ((unused)),
			int argc, char **args)
{
  int i;
#ifdef TODO
  int no_mem_option = 0;
#endif
  struct grub_command *cmd;
  char **cutargs;
  int cutargc;
  grub_err_t err = GRUB_ERR_NONE;
  
  for (i = 0; i < 2; i++)
    {
      /* FIXME: really support this.  */
      if (argc >= 1 && grub_strcmp (args[0], "--no-mem-option") == 0)
	{
#ifdef TODO
	  no_mem_option = 1;
#endif
	  argc--;
	  args++;
	  continue;
	}

      /* linux16 handles both zImages and bzImages.   */
      if (argc >= 1 && (grub_strcmp (args[0], "--type=linux") == 0
			|| grub_strcmp (args[0], "--type=biglinux") == 0))
	{
	  kernel_type = LINUX;
	  argc--;
	  args++;
	  continue;
	}

      if (argc >= 1 && grub_strcmp (args[0], "--type=multiboot") == 0)
	{
	  kernel_type = MULTIBOOT;
	  argc--;
	  args++;
	  continue;
	}

      if (argc >= 1 && grub_strcmp (args[0], "--type=freebsd") == 0)
	{
	  kernel_type = KFREEBSD;
	  argc--;
	  args++;
	  continue;
	}

      if (argc >= 1 && grub_strcmp (args[0], "--type=openbsd") == 0)
	{
	  kernel_type = KOPENBSD;
	  argc--;
	  args++;
	  continue;
	}

      if (argc >= 1 && grub_strcmp (args[0], "--type=netbsd") == 0)
	{
	  kernel_type = KNETBSD;
	  argc--;
	  args++;
	  continue;
	}
    }

  if (argc < 2)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  cutargs = grub_malloc (sizeof (cutargs[0]) * (argc - 1));
  if (!cutargs)
    return grub_errno;
  cutargc = argc - 1;
  grub_memcpy (cutargs + 1, args + 2, sizeof (cutargs[0]) * (argc - 2));
  cutargs[0] = args[0];

  do
    {
      /* First try Linux.  */
      if (kernel_type == GUESS_IT || kernel_type == LINUX)
	{
#ifdef GRUB_MACHINE_PCBIOS
	  cmd = grub_command_find ("linux16");
#else
	  cmd = grub_command_find ("linux");
#endif
	  if (cmd)
	    {
	      if (!(cmd->func) (cmd, cutargc, cutargs))
		{
		  kernel_type = LINUX;
		  goto out;
		}
	    }
	  grub_errno = GRUB_ERR_NONE;
	}

      /* Then multiboot.  */
      if (kernel_type == GUESS_IT || kernel_type == MULTIBOOT)
	{
	  cmd = grub_command_find ("multiboot");
	  if (cmd)
	    {
	      if (!(cmd->func) (cmd, argc, args))
		{
		  kernel_type = MULTIBOOT;
		  goto out;
		}
	    }
	  grub_errno = GRUB_ERR_NONE;
	}

      {
	int bsd_device = -1;
	int bsd_slice = -1;
	int bsd_part = -1;
	{
	  grub_device_t dev;
	  const char *hdbiasstr;
	  int hdbias = 0;
	  hdbiasstr = grub_env_get ("legacy_hdbias");
	  if (hdbiasstr)
	    {
	      hdbias = grub_strtoul (hdbiasstr, 0, 0);
	      grub_errno = GRUB_ERR_NONE;
	    }
	  dev = grub_device_open (0);
	  if (dev && dev->disk
	      && dev->disk->dev->id == GRUB_DISK_DEVICE_BIOSDISK_ID
	      && dev->disk->id >= 0x80 && dev->disk->id <= 0x90)
	    {
	      struct grub_partition *part = dev->disk->partition;
	      bsd_device = dev->disk->id - 0x80 - hdbias;
	      if (part && (grub_strcmp (part->partmap->name, "netbsd") == 0
			   || grub_strcmp (part->partmap->name, "openbsd") == 0
			   || grub_strcmp (part->partmap->name, "bsd") == 0))
		{
		  bsd_part = part->number;
		  part = part->parent;
		}
	      if (part && grub_strcmp (part->partmap->name, "msdos") == 0)
		bsd_slice = part->number;
	    }
	  if (dev)
	    grub_device_close (dev);
	}
	
	/* k*BSD didn't really work well with grub-legacy.  */
	if (kernel_type == GUESS_IT || kernel_type == KFREEBSD)
	  {
	    char buf[sizeof("adXXXXXXXXXXXXsXXXXXXXXXXXXYYY")];
	    if (bsd_device != -1)
	      {
		if (bsd_slice != -1 && bsd_part != -1)
		  grub_snprintf(buf, sizeof(buf), "ad%ds%d%c", bsd_device,
				bsd_slice, 'a' + bsd_part);
		else if (bsd_slice != -1)
		  grub_snprintf(buf, sizeof(buf), "ad%ds%d", bsd_device,
				bsd_slice);
		else
		  grub_snprintf(buf, sizeof(buf), "ad%d", bsd_device);
		grub_env_set ("kFreeBSD.vfs.root.mountfrom", buf);
	      }
	    else
	      grub_env_unset ("kFreeBSD.vfs.root.mountfrom");
	    cmd = grub_command_find ("kfreebsd");
	    if (cmd)
	      {
		if (!(cmd->func) (cmd, cutargc, cutargs))
		  {
		    kernel_type = KFREEBSD;
		    goto out;
		  }
	      }
	    grub_errno = GRUB_ERR_NONE;
	  }
	{
	  char **bsdargs;
	  int bsdargc;
	  char bsddevname[sizeof ("wdXXXXXXXXXXXXY")];
	  int found = 0;

	  if (bsd_device == -1)
	    {
	      bsdargs = cutargs;
	      bsdargc = cutargc;
	    }
	  else
	    {
	      char rbuf[3] = "-r";
	      bsdargc = cutargc + 2;
	      bsdargs = grub_malloc (sizeof (bsdargs[0]) * bsdargc);
	      if (!bsdargs)
		{
		  err = grub_errno;
		  goto out;
		}
	      grub_memcpy (bsdargs, args, argc * sizeof (bsdargs[0]));
	      bsdargs[argc] = rbuf;
	      bsdargs[argc + 1] = bsddevname;
	      grub_snprintf (bsddevname, sizeof (bsddevname),
			     "wd%d%c", bsd_device,
			     bsd_part != -1 ? bsd_part + 'a' : 'c');
	    }
	  if (kernel_type == GUESS_IT || kernel_type == KNETBSD)
	    {
	      cmd = grub_command_find ("knetbsd");
	      if (cmd)
		{
		  if (!(cmd->func) (cmd, bsdargc, bsdargs))
		    {
		      kernel_type = KNETBSD;
		      found = 1;
		      goto free_bsdargs;
		    }
		}
	      grub_errno = GRUB_ERR_NONE;
	    }
	  if (kernel_type == GUESS_IT || kernel_type == KOPENBSD)
	    {
	      cmd = grub_command_find ("kopenbsd");
	      if (cmd)
		{
		  if (!(cmd->func) (cmd, bsdargc, bsdargs))
		    {
		      kernel_type = KOPENBSD;
		      found = 1;
		      goto free_bsdargs;
		    }
		}
	      grub_errno = GRUB_ERR_NONE;
	    }

free_bsdargs:
	  if (bsdargs != cutargs)
	    grub_free (bsdargs);
	  if (found)
	    goto out;
	}
      }
    }
  while (0);

  err = grub_error (GRUB_ERR_BAD_OS, "couldn't load file %s",
		    args[0]);
out:
  grub_free (cutargs);
  return err;
}

static grub_err_t
grub_cmd_legacy_initrd (struct grub_command *mycmd __attribute__ ((unused)),
			int argc, char **args)
{
  struct grub_command *cmd;

  if (kernel_type == LINUX)
    {
#ifdef GRUB_MACHINE_PCBIOS
      cmd = grub_command_find ("initrd16");
#else
      cmd = grub_command_find ("initrd");
#endif
      if (!cmd)
	return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("can't find command `%s'"),
#ifdef GRUB_MACHINE_PCBIOS
			   "initrd16"
#else
			   "initrd"
#endif
			   );

      return cmd->func (cmd, argc ? 1 : 0, args);
    }
  if (kernel_type == MULTIBOOT)
    {
      cmd = grub_command_find ("module");
      if (!cmd)
	return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("can't find command `%s'"),
			   "module");

      return cmd->func (cmd, argc, args);
    }

  return grub_error (GRUB_ERR_BAD_ARGUMENT,
		     N_("you need to load the kernel first"));
}

static grub_err_t
grub_cmd_legacy_initrdnounzip (struct grub_command *mycmd __attribute__ ((unused)),
			       int argc, char **args)
{
  struct grub_command *cmd;

  if (kernel_type == LINUX)
    {
      cmd = grub_command_find ("initrd16");
      if (!cmd)
	return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("can't find command `%s'"),
			   "initrd16");

      return cmd->func (cmd, argc, args);
    }
  if (kernel_type == MULTIBOOT)
    {
      char **newargs;
      grub_err_t err;
      char nounzipbuf[10] = "--nounzip";

      cmd = grub_command_find ("module");
      if (!cmd)
	return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("can't find command `%s'"),
			   "module");

      newargs = grub_malloc ((argc + 1) * sizeof (newargs[0]));
      if (!newargs)
	return grub_errno;
      grub_memcpy (newargs + 1, args, argc * sizeof (newargs[0]));
      newargs[0] = nounzipbuf;

      err = cmd->func (cmd, argc + 1, newargs);
      grub_free (newargs);
      return err;
    }

  return grub_error (GRUB_ERR_BAD_ARGUMENT,
		     N_("you need to load the kernel first"));
}

static grub_err_t
check_password_deny (const char *user __attribute__ ((unused)),
		     const char *entered  __attribute__ ((unused)),
		     void *password __attribute__ ((unused)))
{
  return GRUB_ACCESS_DENIED;
}

#define MD5_HASHLEN 16

struct legacy_md5_password
{
  grub_uint8_t *salt;
  int saltlen;
  grub_uint8_t hash[MD5_HASHLEN];
};

static int
check_password_md5_real (const char *entered,
			 struct legacy_md5_password *pw)
{
  grub_size_t enteredlen = grub_strlen (entered);
  unsigned char alt_result[MD5_HASHLEN];
  unsigned char *digest;
  grub_uint8_t *ctx;
  grub_size_t i;
  int ret;

  ctx = grub_zalloc (GRUB_MD_MD5->contextsize);
  if (!ctx)
    return 0;

  GRUB_MD_MD5->init (ctx);
  GRUB_MD_MD5->write (ctx, entered, enteredlen);
  GRUB_MD_MD5->write (ctx, pw->salt + 3, pw->saltlen - 3);
  GRUB_MD_MD5->write (ctx, entered, enteredlen);
  digest = GRUB_MD_MD5->read (ctx);
  GRUB_MD_MD5->final (ctx);
  grub_memcpy (alt_result, digest, MD5_HASHLEN);
  
  GRUB_MD_MD5->init (ctx);
  GRUB_MD_MD5->write (ctx, entered, enteredlen);
  GRUB_MD_MD5->write (ctx, pw->salt, pw->saltlen); /* include the $1$ header */
  for (i = enteredlen; i > 16; i -= 16)
    GRUB_MD_MD5->write (ctx, alt_result, 16);
  GRUB_MD_MD5->write (ctx, alt_result, i);

  for (i = enteredlen; i > 0; i >>= 1)
    GRUB_MD_MD5->write (ctx, entered + ((i & 1) ? enteredlen : 0), 1);
  digest = GRUB_MD_MD5->read (ctx);
  GRUB_MD_MD5->final (ctx);

  for (i = 0; i < 1000; i++)
    {
      grub_memcpy (alt_result, digest, 16);

      GRUB_MD_MD5->init (ctx);
      if ((i & 1) != 0)
	GRUB_MD_MD5->write (ctx, entered, enteredlen);
      else
	GRUB_MD_MD5->write (ctx, alt_result, 16);
      
      if (i % 3 != 0)
	GRUB_MD_MD5->write (ctx, pw->salt + 3, pw->saltlen - 3);

      if (i % 7 != 0)
	GRUB_MD_MD5->write (ctx, entered, enteredlen);

      if ((i & 1) != 0)
	GRUB_MD_MD5->write (ctx, alt_result, 16);
      else
	GRUB_MD_MD5->write (ctx, entered, enteredlen);
      digest = GRUB_MD_MD5->read (ctx);
      GRUB_MD_MD5->final (ctx);
    }

  ret = (grub_crypto_memcmp (digest, pw->hash, MD5_HASHLEN) == 0);
  grub_free (ctx);
  return ret;
}

static grub_err_t
check_password_md5 (const char *user,
		    const char *entered,
		    void *password)
{
  if (!check_password_md5_real (entered, password))
    return GRUB_ACCESS_DENIED;

  grub_auth_authenticate (user);

  return GRUB_ERR_NONE;
}

static inline int
ib64t (char c)
{
  if (c == '.')
    return 0;
  if (c == '/')
    return 1;
  if (c >= '0' && c <= '9')
    return c - '0' + 2;
  if (c >= 'A' && c <= 'Z')
    return c - 'A' + 12;
  if (c >= 'a' && c <= 'z')
    return c - 'a' + 38;
  return -1;
}

static struct legacy_md5_password *
parse_legacy_md5 (int argc, char **args)
{
  const char *salt, *saltend;
  struct legacy_md5_password *pw = NULL;
  int i;
  const char *p;

  if (grub_memcmp (args[0], "--md5", sizeof ("--md5")) != 0)
    goto fail;
  if (argc == 1)
    goto fail;
  if (grub_strlen(args[1]) <= 3)
    goto fail;
  salt = args[1];
  saltend = grub_strchr (salt + 3, '$');
  if (!saltend)
    goto fail;
  pw = grub_malloc (sizeof (*pw));
  if (!pw)
    goto fail;

  p = saltend + 1;
  for (i = 0; i < 5; i++)
    {
      int n;
      grub_uint32_t w = 0;

      for (n = 0; n < 4; n++)
	{
	  int ww = ib64t(*p++);
	  if (ww == -1)
	    goto fail;
	  w |= ww << (n * 6);
	}
      pw->hash[i == 4 ? 5 : 12+i] = w & 0xff;
      pw->hash[6+i] = (w >> 8) & 0xff;
      pw->hash[i] = (w >> 16) & 0xff;
    }
  {
    int n;
    grub_uint32_t w = 0;
    for (n = 0; n < 2; n++)
      {
	int ww = ib64t(*p++);
	if (ww == -1)
	  goto fail;
	w |= ww << (6 * n);
      }
    if (w >= 0x100)
      goto fail;
    pw->hash[11] = w;
  }

  pw->saltlen = saltend - salt;
  pw->salt = (grub_uint8_t *) grub_strndup (salt, pw->saltlen);
  if (!pw->salt)
    goto fail;

  return pw;

 fail:
  grub_free (pw);
  return NULL;
}

static grub_err_t
grub_cmd_legacy_password (struct grub_command *mycmd __attribute__ ((unused)),
			  int argc, char **args)
{
  struct legacy_md5_password *pw = NULL;

  if (argc == 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("one argument expected"));
  if (args[0][0] != '-' || args[0][1] != '-')
    return grub_normal_set_password ("legacy", args[0]);

  pw = parse_legacy_md5 (argc, args);

  if (pw)
    return grub_auth_register_authentication ("legacy", check_password_md5, pw);
  else
    /* This is to imitate minor difference between grub-legacy in GRUB2.
       If 2 password commands are executed in a row and second one fails
       on GRUB2 the password of first one is used, whereas in grub-legacy
       authenthication is denied. In case of no password command was executed
       early both versions deny any access.  */
    return grub_auth_register_authentication ("legacy", check_password_deny,
					      NULL);
}

int
grub_legacy_check_md5_password (int argc, char **args,
				char *entered)
{
  struct legacy_md5_password *pw = NULL;
  int ret;

  if (args[0][0] != '-' || args[0][1] != '-')
    {
      char correct[GRUB_AUTH_MAX_PASSLEN];

      grub_memset (correct, 0, sizeof (correct));
      grub_strncpy (correct, args[0], sizeof (correct));

      return grub_crypto_memcmp (entered, correct, GRUB_AUTH_MAX_PASSLEN) == 0;
    }

  pw = parse_legacy_md5 (argc, args);

  if (!pw)
    return 0;

  ret = check_password_md5_real (entered, pw);
  grub_free (pw);
  return ret;
}

static grub_err_t
grub_cmd_legacy_check_password (struct grub_command *mycmd __attribute__ ((unused)),
				int argc, char **args)
{
  char entered[GRUB_AUTH_MAX_PASSLEN];

  if (argc == 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("one argument expected"));
  grub_puts_ (N_("Enter password: "));
  if (!grub_password_get (entered, GRUB_AUTH_MAX_PASSLEN))
    return GRUB_ACCESS_DENIED;

  if (!grub_legacy_check_md5_password (argc, args,
				       entered))
    return GRUB_ACCESS_DENIED;

  return GRUB_ERR_NONE;
}

static grub_command_t cmd_source, cmd_configfile;
static grub_command_t cmd_source_extract, cmd_configfile_extract;
static grub_command_t cmd_kernel, cmd_initrd, cmd_initrdnounzip;
static grub_command_t cmd_password, cmd_check_password;

GRUB_MOD_INIT(legacycfg)
{
  cmd_source
    = grub_register_command ("legacy_source",
			     grub_cmd_legacy_source,
			     N_("FILE"),
			     /* TRANSLATORS: "legacy config" means
				"config as used by grub-legacy".  */
			     N_("Parse legacy config in same context"));
  cmd_configfile
    = grub_register_command ("legacy_configfile",
			     grub_cmd_legacy_source,
			     N_("FILE"),
			     N_("Parse legacy config in new context"));
  cmd_source_extract
    = grub_register_command ("extract_legacy_entries_source",
			     grub_cmd_legacy_source,
			     N_("FILE"),
			     N_("Parse legacy config in same context taking only menu entries"));
  cmd_configfile_extract
    = grub_register_command ("extract_legacy_entries_configfile",
			     grub_cmd_legacy_source,
			     N_("FILE"),
			     N_("Parse legacy config in new context taking only menu entries"));

  cmd_kernel = grub_register_command ("legacy_kernel",
				      grub_cmd_legacy_kernel,
				      N_("[--no-mem-option] [--type=TYPE] FILE [ARG ...]"),
				      N_("Simulate grub-legacy `kernel' command"));

  cmd_initrd = grub_register_command ("legacy_initrd",
				      grub_cmd_legacy_initrd,
				      N_("FILE [ARG ...]"),
				      N_("Simulate grub-legacy `initrd' command"));
  cmd_initrdnounzip = grub_register_command ("legacy_initrd_nounzip",
					     grub_cmd_legacy_initrdnounzip,
					     N_("FILE [ARG ...]"),
					     N_("Simulate grub-legacy `modulenounzip' command"));

  cmd_password = grub_register_command ("legacy_password",
					grub_cmd_legacy_password,
					N_("[--md5] PASSWD [FILE]"),
					N_("Simulate grub-legacy `password' command"));

  cmd_check_password = grub_register_command ("legacy_check_password",
					      grub_cmd_legacy_check_password,
					      N_("[--md5] PASSWD [FILE]"),
					      N_("Simulate grub-legacy `password' command in menu entry mode"));

}

GRUB_MOD_FINI(legacycfg)
{
  grub_unregister_command (cmd_source);
  grub_unregister_command (cmd_configfile);
  grub_unregister_command (cmd_source_extract);
  grub_unregister_command (cmd_configfile_extract);

  grub_unregister_command (cmd_kernel);
  grub_unregister_command (cmd_initrd);
  grub_unregister_command (cmd_initrdnounzip);

  grub_unregister_command (cmd_password);
  grub_unregister_command (cmd_check_password);
}
