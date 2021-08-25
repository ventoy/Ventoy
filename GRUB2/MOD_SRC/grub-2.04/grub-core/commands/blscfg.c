/*-*- Mode: C; c-basic-offset: 2; indent-tabs-mode: t -*-*/

/* bls.c - implementation of the boot loader spec */

/*
 *  GRUB  --  GRand Unified Bootloader
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

#include <grub/list.h>
#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/fs.h>
#include <grub/env.h>
#include <grub/file.h>
#include <grub/normal.h>
#include <grub/lib/envblk.h>

#include <stdbool.h>

GRUB_MOD_LICENSE ("GPLv3+");

#include "loadenv.h"

#define GRUB_BLS_CONFIG_PATH "/loader/entries/"
#ifdef GRUB_MACHINE_EMU
#define GRUB_BOOT_DEVICE "/boot"
#else
#define GRUB_BOOT_DEVICE "($root)"
#endif

struct keyval
{
  const char *key;
  char *val;
};

static struct bls_entry *entries = NULL;

#define FOR_BLS_ENTRIES(var) FOR_LIST_ELEMENTS (var, entries)

static int bls_add_keyval(struct bls_entry *entry, char *key, char *val)
{
  char *k, *v;
  struct keyval **kvs, *kv;
  int new_n = entry->nkeyvals + 1;

  kvs = grub_realloc (entry->keyvals, new_n * sizeof (struct keyval *));
  if (!kvs)
    return grub_error (GRUB_ERR_OUT_OF_MEMORY,
		       "couldn't find space for BLS entry");
  entry->keyvals = kvs;

  kv = grub_malloc (sizeof (struct keyval));
  if (!kv)
    return grub_error (GRUB_ERR_OUT_OF_MEMORY,
		       "couldn't find space for BLS entry");

  k = grub_strdup (key);
  if (!k)
    {
      grub_free (kv);
      return grub_error (GRUB_ERR_OUT_OF_MEMORY,
			 "couldn't find space for BLS entry");
    }

  v = grub_strdup (val);
  if (!v)
    {
      grub_free (k);
      grub_free (kv);
      return grub_error (GRUB_ERR_OUT_OF_MEMORY,
			 "couldn't find space for BLS entry");
    }

  kv->key = k;
  kv->val = v;

  entry->keyvals[entry->nkeyvals] = kv;
  grub_dprintf("blscfg", "new keyval at %p:%s:%s\n", entry->keyvals[entry->nkeyvals], k, v);
  entry->nkeyvals = new_n;

  return 0;
}

/* Find they value of the key named by keyname.  If there are allowed to be
 * more than one, pass a pointer to an int set to -1 the first time, and pass
 * the same pointer through each time after, and it'll return them in sorted
 * order as defined in the BLS fragment file */
static char *bls_get_val(struct bls_entry *entry, const char *keyname, int *last)
{
  int idx, start = 0;
  struct keyval *kv = NULL;

  if (last)
    start = *last + 1;

  for (idx = start; idx < entry->nkeyvals; idx++) {
    kv = entry->keyvals[idx];

    if (!grub_strcmp (keyname, kv->key))
      break;
  }

  if (idx == entry->nkeyvals) {
    if (last)
      *last = -1;
    return NULL;
  }

  if (last)
    *last = idx;

  return kv->val;
}

#define goto_return(x) ({ ret = (x); goto finish; })

/* compare alpha and numeric segments of two versions */
/* return 1: a is newer than b */
/*        0: a and b are the same version */
/*       -1: b is newer than a */
static int vercmp(const char * a, const char * b)
{
    char oldch1, oldch2;
    char *abuf, *bbuf;
    char *str1, *str2;
    char * one, * two;
    int rc;
    int isnum;
    int ret = 0;

    grub_dprintf("blscfg", "%s comparing %s and %s\n", __func__, a, b);
    if (!grub_strcmp(a, b))
	    return 0;

    abuf = grub_malloc(grub_strlen(a) + 1);
    bbuf = grub_malloc(grub_strlen(b) + 1);
    str1 = abuf;
    str2 = bbuf;
    grub_strcpy(str1, a);
    grub_strcpy(str2, b);

    one = str1;
    two = str2;

    /* loop through each version segment of str1 and str2 and compare them */
    while (*one || *two) {
	while (*one && !grub_isalnum(*one) && *one != '~') one++;
	while (*two && !grub_isalnum(*two) && *two != '~') two++;

	/* handle the tilde separator, it sorts before everything else */
	if (*one == '~' || *two == '~') {
	    if (*one != '~') goto_return (1);
	    if (*two != '~') goto_return (-1);
	    one++;
	    two++;
	    continue;
	}

	/* If we ran to the end of either, we are finished with the loop */
	if (!(*one && *two)) break;

	str1 = one;
	str2 = two;

	/* grab first completely alpha or completely numeric segment */
	/* leave one and two pointing to the start of the alpha or numeric */
	/* segment and walk str1 and str2 to end of segment */
	if (grub_isdigit(*str1)) {
	    while (*str1 && grub_isdigit(*str1)) str1++;
	    while (*str2 && grub_isdigit(*str2)) str2++;
	    isnum = 1;
	} else {
	    while (*str1 && grub_isalpha(*str1)) str1++;
	    while (*str2 && grub_isalpha(*str2)) str2++;
	    isnum = 0;
	}

	/* save character at the end of the alpha or numeric segment */
	/* so that they can be restored after the comparison */
	oldch1 = *str1;
	*str1 = '\0';
	oldch2 = *str2;
	*str2 = '\0';

	/* this cannot happen, as we previously tested to make sure that */
	/* the first string has a non-null segment */
	if (one == str1) goto_return(-1);	/* arbitrary */

	/* take care of the case where the two version segments are */
	/* different types: one numeric, the other alpha (i.e. empty) */
	/* numeric segments are always newer than alpha segments */
	/* XXX See patch #60884 (and details) from bugzilla #50977. */
	if (two == str2) goto_return (isnum ? 1 : -1);

	if (isnum) {
	    grub_size_t onelen, twolen;
	    /* this used to be done by converting the digit segments */
	    /* to ints using atoi() - it's changed because long  */
	    /* digit segments can overflow an int - this should fix that. */

	    /* throw away any leading zeros - it's a number, right? */
	    while (*one == '0') one++;
	    while (*two == '0') two++;

	    /* whichever number has more digits wins */
	    onelen = grub_strlen(one);
	    twolen = grub_strlen(two);
	    if (onelen > twolen) goto_return (1);
	    if (twolen > onelen) goto_return (-1);
	}

	/* grub_strcmp will return which one is greater - even if the two */
	/* segments are alpha or if they are numeric.  don't return  */
	/* if they are equal because there might be more segments to */
	/* compare */
	rc = grub_strcmp(one, two);
	if (rc) goto_return (rc < 1 ? -1 : 1);

	/* restore character that was replaced by null above */
	*str1 = oldch1;
	one = str1;
	*str2 = oldch2;
	two = str2;
    }

    /* this catches the case where all numeric and alpha segments have */
    /* compared identically but the segment sepparating characters were */
    /* different */
    if ((!*one) && (!*two)) goto_return (0);

    /* whichever version still has characters left over wins */
    if (!*one) goto_return (-1); else goto_return (1);

finish:
    grub_free (abuf);
    grub_free (bbuf);
    return ret;
}

/* returns name/version/release */
/* NULL string pointer returned if nothing found */
static void
split_package_string (char *package_string, char **name,
                     char **version, char **release)
{
  char *package_version, *package_release;

  /* Release */
  package_release = grub_strrchr (package_string, '-');

  if (package_release != NULL)
      *package_release++ = '\0';

  *release = package_release;

  if (name == NULL)
    {
      *version = package_string;
    }
  else
    {
      /* Version */
      package_version = grub_strrchr(package_string, '-');

      if (package_version != NULL)
	*package_version++ = '\0';

      *version = package_version;
      /* Name */
      *name = package_string;
    }

  /* Bubble up non-null values from release to name */
  if (name != NULL && *name == NULL)
    {
      *name = (*version == NULL ? *release : *version);
      *version = *release;
      *release = NULL;
    }
  if (*version == NULL)
    {
      *version = *release;
      *release = NULL;
    }
}

static int
split_cmp(char *nvr0, char *nvr1, int has_name)
{
  int ret = 0;
  char *name0, *version0, *release0;
  char *name1, *version1, *release1;

  split_package_string(nvr0, has_name ? &name0 : NULL, &version0, &release0);
  split_package_string(nvr1, has_name ? &name1 : NULL, &version1, &release1);

  if (has_name)
    {
      ret = vercmp(name0 == NULL ? "" : name0,
		   name1 == NULL ? "" : name1);
      if (ret != 0)
	return ret;
    }

  ret = vercmp(version0 == NULL ? "" : version0,
	       version1 == NULL ? "" : version1);
  if (ret != 0)
    return ret;

  ret = vercmp(release0 == NULL ? "" : release0,
	       release1 == NULL ? "" : release1);
  return ret;
}

/* return 1: e0 is newer than e1 */
/*        0: e0 and e1 are the same version */
/*       -1: e1 is newer than e0 */
static int bls_cmp(const struct bls_entry *e0, const struct bls_entry *e1)
{
  char *id0, *id1;
  int r;

  id0 = grub_strdup(e0->filename);
  id1 = grub_strdup(e1->filename);

  r = split_cmp(id0, id1, 1);

  grub_free(id0);
  grub_free(id1);

  return r;
}

static void list_add_tail(struct bls_entry *head, struct bls_entry *item)
{
  item->next = head;
  if (head->prev)
    head->prev->next = item;
  item->prev = head->prev;
  head->prev = item;
}

static int bls_add_entry(struct bls_entry *entry)
{
  struct bls_entry *e, *last = NULL;
  int rc;

  if (!entries) {
    grub_dprintf ("blscfg", "Add entry with id \"%s\"\n", entry->filename);
    entries = entry;
    return 0;
  }

  FOR_BLS_ENTRIES(e) {
    rc = bls_cmp(entry, e);

    if (!rc)
      return GRUB_ERR_BAD_ARGUMENT;

    if (rc == 1) {
      grub_dprintf ("blscfg", "Add entry with id \"%s\"\n", entry->filename);
      list_add_tail (e, entry);
      if (e == entries) {
	entries = entry;
	entry->prev = NULL;
      }
      return 0;
    }
    last = e;
  }

  if (last) {
    grub_dprintf ("blscfg", "Add entry with id \"%s\"\n", entry->filename);
    last->next = entry;
    entry->prev = last;
  }

  return 0;
}

struct read_entry_info {
  const char *devid;
  const char *dirname;
  grub_file_t file;
};

static int read_entry (
    const char *filename,
    const struct grub_dirhook_info *dirhook_info UNUSED,
    void *data)
{
  grub_size_t m = 0, n, clip = 0;
  int rc = 0;
  char *p = NULL;
  grub_file_t f = NULL;
  struct bls_entry *entry;
  struct read_entry_info *info = (struct read_entry_info *)data;

  grub_dprintf ("blscfg", "filename: \"%s\"\n", filename);

  n = grub_strlen (filename);

  if (info->file)
    {
      f = info->file;
    }
  else
    {
      if (filename[0] == '.')
	return 0;

      if (n <= 5)
	return 0;

      if (grub_strcmp (filename + n - 5, ".conf") != 0)
	return 0;

      p = grub_xasprintf ("(%s)%s/%s", info->devid, info->dirname, filename);

      f = grub_file_open (p, GRUB_FILE_TYPE_CONFIG);
      if (!f)
	goto finish;
    }

  entry = grub_zalloc (sizeof (*entry));
  if (!entry)
    goto finish;

  if (info->file)
    {
      char *slash;

      if (n > 5 && !grub_strcmp (filename + n - 5, ".conf") == 0)
	clip = 5;

      slash = grub_strrchr (filename, '/');
      if (!slash)
	slash = grub_strrchr (filename, '\\');

      while (*slash == '/' || *slash == '\\')
	slash++;

      m = slash ? slash - filename : 0;
    }
  else
    {
      m = 0;
      clip = 5;
    }
  n -= m;

  entry->filename = grub_strndup(filename + m, n - clip);
  if (!entry->filename)
    goto finish;

  entry->filename[n - 5] = '\0';

  for (;;)
    {
      char *buf;
      char *separator;

      buf = grub_file_getline (f);
      if (!buf)
	break;

      while (buf && buf[0] && (buf[0] == ' ' || buf[0] == '\t'))
	buf++;
      if (buf[0] == '#')
	continue;

      separator = grub_strchr (buf, ' ');

      if (!separator)
	separator = grub_strchr (buf, '\t');

      if (!separator || separator[1] == '\0')
	{
	  grub_free (buf);
	  break;
	}

      separator[0] = '\0';

      do {
	separator++;
      } while (*separator == ' ' || *separator == '\t');

      rc = bls_add_keyval (entry, buf, separator);
      grub_free (buf);
      if (rc < 0)
	break;
    }

    if (!rc)
      bls_add_entry(entry);

finish:
  if (p)
    grub_free (p);

  if (f)
    grub_file_close (f);

  return 0;
}

static grub_envblk_t saved_env = NULL;

static int UNUSED
save_var (const char *name, const char *value, void *whitelist UNUSED)
{
  const char *val = grub_env_get (name);
  grub_dprintf("blscfg", "saving \"%s\"\n", name);

  if (val)
    grub_envblk_set (saved_env, name, value);

  return 0;
}

static int UNUSED
unset_var (const char *name, const char *value UNUSED, void *whitelist)
{
  grub_dprintf("blscfg", "restoring \"%s\"\n", name);
  if (! whitelist)
    {
      grub_env_unset (name);
      return 0;
    }

  if (test_whitelist_membership (name,
				 (const grub_env_whitelist_t *) whitelist))
    grub_env_unset (name);

  return 0;
}

static char **bls_make_list (struct bls_entry *entry, const char *key, int *num)
{
  int last = -1;
  char *val;

  int nlist = 0;
  char **list = NULL;

  list = grub_malloc (sizeof (char *));
  if (!list)
    return NULL;
  list[0] = NULL;

  while (1)
    {
      char **new;

      val = bls_get_val (entry, key, &last);
      if (!val)
	break;

      new = grub_realloc (list, (nlist + 2) * sizeof (char *));
      if (!new)
	break;

      list = new;
      list[nlist++] = val;
      list[nlist] = NULL;
  }

  if (num)
    *num = nlist;

  return list;
}

static char *field_append(bool is_var, char *buffer, char *start, char *end)
{
  char *temp = grub_strndup(start, end - start + 1);
  const char *field = temp;

  if (is_var) {
    field = grub_env_get (temp);
    if (!field)
      return buffer;
  }

  if (!buffer) {
    buffer = grub_strdup(field);
    if (!buffer)
      return NULL;
  } else {
    buffer = grub_realloc (buffer, grub_strlen(buffer) + grub_strlen(field));
    if (!buffer)
      return NULL;

    grub_stpcpy (buffer + grub_strlen(buffer), field);
  }

  return buffer;
}

static char *expand_val(char *value)
{
  char *buffer = NULL;
  char *start = value;
  char *end = value;
  bool is_var = false;

  if (!value)
    return NULL;

  while (*value) {
    if (*value == '$') {
      if (start != end) {
	buffer = field_append(is_var, buffer, start, end);
	if (!buffer)
	  return NULL;
      }

      is_var = true;
      start = value + 1;
    } else if (is_var) {
      if (!grub_isalnum(*value) && *value != '_') {
	buffer = field_append(is_var, buffer, start, end);
	is_var = false;
	start = value;
      }
    }

    end = value;
    value++;
  }

  if (start != end) {
    buffer = field_append(is_var, buffer, start, end);
    if (!buffer)
      return NULL;
  }

  return buffer;
}

static char **early_initrd_list (const char *initrd)
{
  int nlist = 0;
  char **list = NULL;
  char *separator;

  while ((separator = grub_strchr (initrd, ' ')))
    {
      list = grub_realloc (list, (nlist + 2) * sizeof (char *));
      if (!list)
        return NULL;

      list[nlist++] = grub_strndup(initrd, separator - initrd);
      list[nlist] = NULL;
      initrd = separator + 1;
  }

  list = grub_realloc (list, (nlist + 2) * sizeof (char *));
  if (!list)
    return NULL;

  list[nlist++] = grub_strndup(initrd, grub_strlen(initrd));
  list[nlist] = NULL;

  return list;
}

static void create_entry (struct bls_entry *entry)
{
  int argc = 0;
  const char **argv = NULL;

  char *title = NULL;
  char *clinux = NULL;
  char *options = NULL;
  char **initrds = NULL;
  char *initrd = NULL;
  const char *early_initrd = NULL;
  char **early_initrds = NULL;
  char *initrd_prefix = NULL;
  char *id = entry->filename;
  char *dotconf = id;
  char *hotkey = NULL;

  char *users = NULL;
  char **classes = NULL;

  char **args = NULL;

  char *src = NULL;
  int bootlen;
  const char *bootdev;
  int i, index;

  grub_dprintf("blscfg", "%s got here\n", __func__);
  clinux = bls_get_val (entry, "linux", NULL);
  if (!clinux)
    {
      grub_dprintf ("blscfg", "Skipping file %s with no 'linux' key.\n", entry->filename);
      goto finish;
    }

  bootdev = grub_env_get("ventoy_bls_bootdev");
  if (!bootdev)
  {
      bootdev = GRUB_BOOT_DEVICE;
  }
  bootlen = grub_strlen(bootdev) + 2;//space and \0

  /*
   * strip the ".conf" off the end before we make it our "id" field.
   */
  do
    {
      dotconf = grub_strstr(dotconf, ".conf");
    } while (dotconf != NULL && dotconf[5] != '\0');
  if (dotconf)
    dotconf[0] = '\0';

  title = bls_get_val (entry, "title", NULL);
  options = expand_val (bls_get_val (entry, "options", NULL));

  if (!options)
    options = expand_val ((char *)grub_env_get("default_kernelopts"));

  initrds = bls_make_list (entry, "initrd", NULL);

  hotkey = bls_get_val (entry, "grub_hotkey", NULL);
  users = expand_val (bls_get_val (entry, "grub_users", NULL));
  classes = bls_make_list (entry, "grub_class", NULL);
  args = bls_make_list (entry, "grub_arg", &argc);

  argc += 1;
  argv = grub_malloc ((argc + 1) * sizeof (char *));
  argv[0] = title ? title : clinux;
  for (i = 1; i < argc; i++)
    argv[i] = args[i-1];
  argv[argc] = NULL;

  early_initrd = grub_env_get("early_initrd");

  grub_dprintf ("blscfg", "adding menu entry for \"%s\" with id \"%s\"\n",
		title, id);
  if (early_initrd)
    {
      early_initrds = early_initrd_list(early_initrd);
      if (!early_initrds)
      {
	grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
	goto finish;
      }

      if (initrds != NULL && initrds[0] != NULL)
	{
	  initrd_prefix = grub_strrchr (initrds[0], '/');
	  initrd_prefix = grub_strndup(initrds[0], initrd_prefix - initrds[0] + 1);
	}
      else
	{
	  initrd_prefix = grub_strrchr (clinux, '/');
	  initrd_prefix = grub_strndup(clinux, initrd_prefix - clinux + 1);
	}

      if (!initrd_prefix)
	{
	  grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
	  goto finish;
	}
    }

  if (early_initrds || initrds)
    {
      int initrd_size = sizeof ("initrd");
      char *tmp;

      for (i = 0; early_initrds != NULL && early_initrds[i] != NULL; i++)
	initrd_size += bootlen \
		       + grub_strlen(initrd_prefix)  \
		       + grub_strlen (early_initrds[i]) + 1;

      for (i = 0; initrds != NULL && initrds[i] != NULL; i++)
	initrd_size += bootlen \
		       + grub_strlen (initrds[i]) + 1;
      initrd_size += 1;

      initrd = grub_malloc (initrd_size);
      if (!initrd)
	{
	  grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
	  goto finish;
	}


      tmp = grub_stpcpy(initrd, "initrd");
      for (i = 0; early_initrds != NULL && early_initrds[i] != NULL; i++)
	{
	  grub_dprintf ("blscfg", "adding early initrd %s\n", early_initrds[i]);
	  tmp = grub_stpcpy (tmp, " ");
	  tmp = grub_stpcpy (tmp, bootdev);
	  tmp = grub_stpcpy (tmp, initrd_prefix);
	  tmp = grub_stpcpy (tmp, early_initrds[i]);
	  grub_free(early_initrds[i]);
	}

      for (i = 0; initrds != NULL && initrds[i] != NULL; i++)
	{
	  grub_dprintf ("blscfg", "adding initrd %s\n", initrds[i]);
	  tmp = grub_stpcpy (tmp, " ");
	  tmp = grub_stpcpy (tmp, bootdev);
	  tmp = grub_stpcpy (tmp, initrds[i]);
	}
      tmp = grub_stpcpy (tmp, "\n");
    }

  src = grub_xasprintf ("load_video\n"
			"set gfxpayload=keep\n"
			"insmod gzio\n"
			"linux %s%s%s%s\n"
			"%s",
			bootdev, clinux, options ? " " : "", options ? options : "",
			initrd ? initrd : "");

  grub_normal_add_menu_entry (argc, argv, classes, id, users, hotkey, NULL, src, 0, &index, entry);
  grub_dprintf ("blscfg", "Added entry %d id:\"%s\"\n", index, id);

finish:
  grub_free (initrd);
  grub_free (initrd_prefix);
  grub_free (early_initrds);
  grub_free (initrds);
  grub_free (options);
  grub_free (classes);
  grub_free (args);
  grub_free (argv);
  grub_free (src);
}

struct find_entry_info {
	const char *dirname;
	const char *devid;
	grub_device_t dev;
	grub_fs_t fs;
};

/*
 * info: the filesystem object the file is on.
 */
static int find_entry (struct find_entry_info *info)
{
  struct read_entry_info read_entry_info;
  grub_fs_t blsdir_fs = NULL;
  grub_device_t blsdir_dev = NULL;
  const char *blsdir = info->dirname;
  int fallback = 0;
  int r = 0;

  if (!blsdir) {
    blsdir = grub_env_get ("blsdir");
    if (!blsdir)
      blsdir = GRUB_BLS_CONFIG_PATH;
  }

  read_entry_info.file = NULL;
  read_entry_info.dirname = blsdir;

  grub_dprintf ("blscfg", "scanning blsdir: %s\n", blsdir);

  blsdir_dev = info->dev;
  blsdir_fs = info->fs;
  read_entry_info.devid = info->devid;

read_fallback:
  r = blsdir_fs->fs_dir (blsdir_dev, read_entry_info.dirname, read_entry,
			 &read_entry_info);
  if (r != 0) {
      grub_dprintf ("blscfg", "read_entry returned error\n");
      grub_err_t e;
      do
	{
	  e = grub_error_pop();
	} while (e);
  }

  if (r && !info->dirname && !fallback) {
    read_entry_info.dirname = "/boot" GRUB_BLS_CONFIG_PATH;
    grub_dprintf ("blscfg", "Entries weren't found in %s, fallback to %s\n",
		  blsdir, read_entry_info.dirname);
    fallback = 1;
    goto read_fallback;
  }

  return 0;
}

static grub_err_t
bls_load_entries (const char *path)
{
  grub_size_t len;
  grub_fs_t fs;
  grub_device_t dev;
  static grub_err_t r;
  const char *devid = NULL;
  char *blsdir = NULL;
  struct find_entry_info info = {
      .dev = NULL,
      .fs = NULL,
      .dirname = NULL,
  };
  struct read_entry_info rei = {
      .devid = NULL,
      .dirname = NULL,
  };

  if (path) {
    len = grub_strlen (path);
    if (grub_strcmp (path + len - 5, ".conf") == 0) {
      rei.file = grub_file_open (path, GRUB_FILE_TYPE_CONFIG);
      if (!rei.file)
	return grub_errno;
      /*
       * read_entry() closes the file
       */
      return read_entry(path, NULL, &rei);
    } else if (path[0] == '(') {
      devid = path + 1;

      blsdir = grub_strchr (path, ')');
      if (!blsdir)
	return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("Filepath isn't correct"));

      *blsdir = '\0';
      blsdir = blsdir + 1;
    }
  }

  if (!devid) {
#ifdef GRUB_MACHINE_EMU
    devid = "host";
#elif defined(GRUB_MACHINE_EFI)
    devid = grub_env_get ("root");
#else
    devid = grub_env_get ("boot");
    if (!devid)
    {
        devid = grub_env_get ("root");
    }
#endif
    if (!devid)
      return grub_error (GRUB_ERR_FILE_NOT_FOUND,
			 N_("variable `%s' isn't set"), "boot");
  }

  grub_dprintf ("blscfg", "opening %s\n", devid);
  dev = grub_device_open (devid);
  if (!dev)
    return grub_errno;

  grub_dprintf ("blscfg", "probing fs\n");
  fs = grub_fs_probe (dev);
  if (!fs)
    {
      r = grub_errno;
      goto finish;
    }

  info.dirname = blsdir;
  info.devid = devid;
  info.dev = dev;
  info.fs = fs;
  find_entry(&info);

finish:
  if (dev)
    grub_device_close (dev);

  return r;
}

static bool
is_default_entry(const char *def_entry, struct bls_entry *entry, int idx)
{
  const char *title;
  int def_idx;

  if (!def_entry)
    return false;

  if (grub_strcmp(def_entry, entry->filename) == 0)
    return true;

  title = bls_get_val(entry, "title", NULL);

  if (title && grub_strcmp(def_entry, title) == 0)
    return true;

  def_idx = (int)grub_strtol(def_entry, NULL, 0);
  if (grub_errno == GRUB_ERR_BAD_NUMBER) {
    grub_errno = GRUB_ERR_NONE;
    return false;
  }

  if (def_idx == idx)
    return true;

  return false;
}

static grub_err_t
bls_create_entries (bool show_default, bool show_non_default, char *entry_id)
{
  const char *def_entry = NULL;
  struct bls_entry *entry = NULL;
  int idx = 0;

  def_entry = grub_env_get("default");

  grub_dprintf ("blscfg", "%s Creating entries from bls\n", __func__);
  FOR_BLS_ENTRIES(entry) {
    if (entry->visible) {
      idx++;
      continue;
    }

    if ((show_default && is_default_entry(def_entry, entry, idx)) ||
	(show_non_default && !is_default_entry(def_entry, entry, idx)) ||
	(entry_id && grub_strcmp(entry_id, entry->filename) == 0)) {
      create_entry(entry);
      entry->visible = 1;
    }
    idx++;
  }

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_blscfg (grub_extcmd_context_t ctxt UNUSED,
		 int argc, char **args)
{
  grub_err_t r;
  char *path = NULL;
  char *entry_id = NULL;
  bool show_default = true;
  bool show_non_default = true;

  if (argc == 1) {
    if (grub_strcmp (args[0], "default") == 0) {
      show_non_default = false;
    } else if (grub_strcmp (args[0], "non-default") == 0) {
      show_default = false;
    } else if (args[0][0] == '(') {
      path = args[0];
    } else {
      entry_id = args[0];
      show_default = false;
      show_non_default = false;
    }
  }

  r = bls_load_entries(path);
  if (r)
    return r;

  return bls_create_entries(show_default, show_non_default, entry_id);
}

static grub_extcmd_t cmd;
static grub_extcmd_t oldcmd;

GRUB_MOD_INIT(blscfg)
{
  grub_dprintf("blscfg", "%s got here\n", __func__);
  cmd = grub_register_extcmd ("blscfg",
			      grub_cmd_blscfg,
			      0,
			      NULL,
			      N_("Import Boot Loader Specification snippets."),
			      NULL);
  oldcmd = grub_register_extcmd ("bls_import",
				 grub_cmd_blscfg,
				 0,
				 NULL,
				 N_("Import Boot Loader Specification snippets."),
				 NULL);
}

GRUB_MOD_FINI(blscfg)
{
  grub_unregister_extcmd (cmd);
  grub_unregister_extcmd (oldcmd);
}
