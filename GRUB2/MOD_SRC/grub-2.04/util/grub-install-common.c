/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 1999,2000,2001,2002,2003,2004,2005,2006,2007,2008,2009,2010,2011,2012,2013 Free Software Foundation, Inc.
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

#include <config.h>
#include <grub/types.h>
#include <grub/emu/misc.h>
#include <grub/util/misc.h>
#include <grub/misc.h>
#include <grub/device.h>
#include <grub/disk.h>
#include <grub/file.h>
#include <grub/fs.h>
#include <grub/env.h>
#include <grub/term.h>
#include <grub/mm.h>
#include <grub/lib/hexdump.h>
#include <grub/crypto.h>
#include <grub/command.h>
#include <grub/i18n.h>
#include <grub/zfs/zfs.h>
#include <grub/util/install.h>
#include <grub/util/resolve.h>
#include <grub/emu/hostfile.h>
#include <grub/emu/config.h>
#include <grub/emu/hostfile.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#pragma GCC diagnostic ignored "-Wformat-nonliteral"

char *
grub_install_help_filter (int key, const char *text,
				 void *input __attribute__ ((unused)))
{
  switch (key)
    {
    case GRUB_INSTALL_OPTIONS_INSTALL_THEMES:
      return xasprintf(text, "starfield");
    case GRUB_INSTALL_OPTIONS_INSTALL_FONTS:
      return xasprintf(text, "unicode");
    case GRUB_INSTALL_OPTIONS_DIRECTORY:
    case GRUB_INSTALL_OPTIONS_DIRECTORY2:
      return xasprintf(text, grub_util_get_pkglibdir ());      
    case GRUB_INSTALL_OPTIONS_LOCALE_DIRECTORY:
      return xasprintf(text, grub_util_get_localedir ());
    case GRUB_INSTALL_OPTIONS_THEMES_DIRECTORY:
      return grub_util_path_concat (2, grub_util_get_pkgdatadir (), "themes");
    default:
      return (char *) text;
    }
}

#pragma GCC diagnostic error "-Wformat-nonliteral"

static int (*compress_func) (const char *src, const char *dest) = NULL;
char *grub_install_copy_buffer;
static char *dtb;

int
grub_install_copy_file (const char *src,
			const char *dst,
			int is_needed)
{
  grub_util_fd_t in, out;  
  ssize_t r;

  grub_util_info ("copying `%s' -> `%s'", src, dst);

  in = grub_util_fd_open (src, GRUB_UTIL_FD_O_RDONLY);
  if (!GRUB_UTIL_FD_IS_VALID (in))
    {
      if (is_needed)
	grub_util_error (_("cannot open `%s': %s"), src, grub_util_fd_strerror ());
      else
	grub_util_info (_("cannot open `%s': %s"), src, grub_util_fd_strerror ());
      return 0;
    }
  out = grub_util_fd_open (dst, GRUB_UTIL_FD_O_WRONLY
			   | GRUB_UTIL_FD_O_CREATTRUNC);
  if (!GRUB_UTIL_FD_IS_VALID (out))
    {
      grub_util_error (_("cannot open `%s': %s"), dst,
		       grub_util_fd_strerror ());
      grub_util_fd_close (in);
      return 0;
    }

  if (!grub_install_copy_buffer)
    grub_install_copy_buffer = xmalloc (GRUB_INSTALL_COPY_BUFFER_SIZE);
 
  while (1)
    {
      r = grub_util_fd_read (in, grub_install_copy_buffer, GRUB_INSTALL_COPY_BUFFER_SIZE);
      if (r <= 0)
	break;
      r = grub_util_fd_write (out, grub_install_copy_buffer, r);
      if (r <= 0)
	break;
    }
  if (grub_util_fd_sync (out) < 0)
    r = -1;
  if (grub_util_fd_close (in) < 0)
    r = -1;
  if (grub_util_fd_close (out) < 0)
    r = -1;

  if (r < 0)
    grub_util_error (_("cannot copy `%s' to `%s': %s"),
		     src, dst, grub_util_fd_strerror ());

  return 1;
}

static int
grub_install_compress_file (const char *in_name,
			    const char *out_name,
			    int is_needed)
{
  int ret;

  if (!compress_func)
    ret = grub_install_copy_file (in_name, out_name, is_needed);
  else
    {
      grub_util_info ("compressing `%s' -> `%s'", in_name, out_name);
      ret = !compress_func (in_name, out_name);
      if (!ret && is_needed)
	grub_util_warn (_("can't compress `%s' to `%s'"), in_name, out_name);
    }

  if (!ret && is_needed)
    grub_util_error (_("cannot copy `%s' to `%s': %s"),
		     in_name, out_name, grub_util_fd_strerror ());

  return ret;
}

static int
is_path_separator (char c)
{
#if defined (__MINGW32__) || defined (__CYGWIN__)
  if (c == '\\')
    return 1;
#endif
  if (c == '/')
    return 1;
  return 0;
}

void
grub_install_mkdir_p (const char *dst)
{
  char *t = xstrdup (dst);
  char *p;
  for (p = t; *p; p++)
    {
      if (is_path_separator (*p))
	{
	  char s = *p;
	  *p = '\0';
	  grub_util_mkdir (t);
	  *p = s;
	}
    }
  grub_util_mkdir (t);
  free (t);
}

static void
clean_grub_dir (const char *di)
{
  grub_util_fd_dir_t d;
  grub_util_fd_dirent_t de;

  d = grub_util_fd_opendir (di);
  if (!d)
    grub_util_error (_("cannot open directory `%s': %s"),
		     di, grub_util_fd_strerror ());

  while ((de = grub_util_fd_readdir (d)))
    {
      const char *ext = strrchr (de->d_name, '.');
      if ((ext && (strcmp (ext, ".mod") == 0
		   || strcmp (ext, ".lst") == 0
		   || strcmp (ext, ".img") == 0
		   || strcmp (ext, ".mo") == 0)
	   && strcmp (de->d_name, "menu.lst") != 0)
	  || strcmp (de->d_name, "efiemu32.o") == 0
	  || strcmp (de->d_name, "efiemu64.o") == 0)
	{
	  char *x = grub_util_path_concat (2, di, de->d_name);
	  if (grub_util_unlink (x) < 0)
	    grub_util_error (_("cannot delete `%s': %s"), x,
			     grub_util_fd_strerror ());
	  free (x);
	}
    }
  grub_util_fd_closedir (d);
}

struct install_list
{
  int is_default;
  char **entries;
  size_t n_entries;
  size_t n_alloc;
};

struct install_list install_modules = { 1, 0, 0, 0 };
struct install_list modules = { 1, 0, 0, 0 };
struct install_list install_locales = { 1, 0, 0, 0 };
struct install_list install_fonts = { 1, 0, 0, 0 };
struct install_list install_themes = { 1, 0, 0, 0 };
char *grub_install_source_directory = NULL;
char *grub_install_locale_directory = NULL;
char *grub_install_themes_directory = NULL;

void
grub_install_push_module (const char *val)
{
  modules.is_default = 0;
  if (modules.n_entries + 1 >= modules.n_alloc)
    {
      modules.n_alloc <<= 1;
      if (modules.n_alloc < 16)
	modules.n_alloc = 16;
      modules.entries = xrealloc (modules.entries,
				  modules.n_alloc * sizeof (*modules.entries));
    }
  modules.entries[modules.n_entries++] = xstrdup (val);
  modules.entries[modules.n_entries] = NULL;
}

void
grub_install_pop_module (void)
{
  modules.n_entries--;
  free (modules.entries[modules.n_entries]);
  modules.entries[modules.n_entries] = NULL;
}


static void
handle_install_list (struct install_list *il, const char *val,
		     int default_all)
{
  const char *ptr;
  char **ce;
  il->is_default = 0;
  free (il->entries);
  il->entries = NULL;
  il->n_entries = 0;
  if (strcmp (val, "all") == 0 && default_all)
    {
      il->is_default = 1;
      return;
    }
  ptr = val;
  while (1)
    {
      while (*ptr && grub_isspace (*ptr))
	ptr++;
      if (!*ptr)
	break;
      while (*ptr && !grub_isspace (*ptr))
	ptr++;
      il->n_entries++;
    }
  il->n_alloc = il->n_entries + 1;
  il->entries = xmalloc (il->n_alloc * sizeof (il->entries[0]));
  ptr = val;
  for (ce = il->entries; ; ce++)
    {
      const char *bptr;
      while (*ptr && grub_isspace (*ptr))
	ptr++;
      if (!*ptr)
	break;
      bptr = ptr;
      while (*ptr && !grub_isspace (*ptr))
	ptr++;
      *ce = xmalloc (ptr - bptr + 1);
      memcpy (*ce, bptr, ptr - bptr);
      (*ce)[ptr - bptr] = '\0';
    }
  *ce = NULL;
}

static char **pubkeys;
static size_t npubkeys;
static grub_compression_t compression;

int
grub_install_parse (int key, char *arg)
{
  switch (key)
    {
    case 'C':
      if (grub_strcmp (arg, "xz") == 0)
	{
#ifdef HAVE_LIBLZMA
	  compression = GRUB_COMPRESSION_XZ;
#else
	  grub_util_error ("%s",
			   _("grub-mkimage is compiled without XZ support"));
#endif
	}
      else if (grub_strcmp (arg, "none") == 0)
	compression = GRUB_COMPRESSION_NONE;
      else if (grub_strcmp (arg, "auto") == 0)
	compression = GRUB_COMPRESSION_AUTO;
      else
	grub_util_error (_("Unknown compression format %s"), arg);
      return 1;
    case 'k':
      pubkeys = xrealloc (pubkeys,
			  sizeof (pubkeys[0])
			  * (npubkeys + 1));
      pubkeys[npubkeys++] = xstrdup (arg);
      return 1;

    case GRUB_INSTALL_OPTIONS_VERBOSITY:
      verbosity++;
      return 1;

    case GRUB_INSTALL_OPTIONS_DIRECTORY:
    case GRUB_INSTALL_OPTIONS_DIRECTORY2:
      free (grub_install_source_directory);
      grub_install_source_directory = xstrdup (arg);
      return 1;
    case GRUB_INSTALL_OPTIONS_LOCALE_DIRECTORY:
      free (grub_install_locale_directory);
      grub_install_locale_directory = xstrdup (arg);
      return 1;
    case GRUB_INSTALL_OPTIONS_THEMES_DIRECTORY:
      free (grub_install_themes_directory);
      grub_install_themes_directory = xstrdup (arg);
      return 1;
    case GRUB_INSTALL_OPTIONS_INSTALL_MODULES:
      handle_install_list (&install_modules, arg, 0);
      return 1;
    case GRUB_INSTALL_OPTIONS_MODULES:
      handle_install_list (&modules, arg, 0);
      return 1;
    case GRUB_INSTALL_OPTIONS_INSTALL_LOCALES:
      handle_install_list (&install_locales, arg, 0);
      return 1;
    case GRUB_INSTALL_OPTIONS_INSTALL_THEMES:
      handle_install_list (&install_themes, arg, 0);
      return 1;
    case GRUB_INSTALL_OPTIONS_INSTALL_FONTS:
      handle_install_list (&install_fonts, arg, 0);
      return 1;
    case GRUB_INSTALL_OPTIONS_DTB:
      if (dtb)
	free (dtb);
      dtb = xstrdup (arg);
      return 1;
    case GRUB_INSTALL_OPTIONS_INSTALL_COMPRESS:
      if (strcmp (arg, "no") == 0
	  || strcmp (arg, "none") == 0)
	{
	  compress_func = NULL;
	  return 1;
	}
      if (strcmp (arg, "gz") == 0)
	{
	  compress_func = grub_install_compress_gzip;
	  return 1;
	}
      if (strcmp (arg, "xz") == 0)
	{
	  compress_func = grub_install_compress_xz;
	  return 1;
	}
      if (strcmp (arg, "lzo") == 0)
	{
	  compress_func = grub_install_compress_lzop;
	  return 1;
	}
      grub_util_error (_("Unrecognized compression `%s'"), arg);
    case GRUB_INSTALL_OPTIONS_GRUB_MKIMAGE:
      return 1;
    default:
      return 0;
    }
}

static int
decompressors (void)
{
  if (compress_func == grub_install_compress_gzip)
    {
      grub_install_push_module ("gzio");
      return 1;
    }
  if (compress_func == grub_install_compress_xz)
    {
      grub_install_push_module ("xzio");
      grub_install_push_module ("gcry_crc");
      return 2;
    }
  if (compress_func == grub_install_compress_lzop)
    {
      grub_install_push_module ("lzopio");
      grub_install_push_module ("adler32");
      grub_install_push_module ("gcry_crc");
      return 3;
    }
  return 0;
}

void
grub_install_make_image_wrap_file (const char *dir, const char *prefix,
				   FILE *fp, const char *outname,
				   char *memdisk_path,
				   char *config_path,
				   const char *mkimage_target, int note)
{
  const struct grub_install_image_target_desc *tgt;
  const char *const compnames[] = 
    {
      [GRUB_COMPRESSION_AUTO] = "auto",
      [GRUB_COMPRESSION_NONE] = "none",
      [GRUB_COMPRESSION_XZ] = "xz",
      [GRUB_COMPRESSION_LZMA] = "lzma",
    };
  grub_size_t slen = 1;
  char *s, *p;
  char **pk, **md;
  int dc = decompressors ();

  if (memdisk_path)
    slen += 20 + grub_strlen (memdisk_path);
  if (config_path)
    slen += 20 + grub_strlen (config_path);

  for (pk = pubkeys; pk < pubkeys + npubkeys; pk++)
    slen += 20 + grub_strlen (*pk);

  for (md = modules.entries; *md; md++)
    {
      slen += 10 + grub_strlen (*md);
    }

  p = s = xmalloc (slen);
  if (memdisk_path)
    {
      p = grub_stpcpy (p, "--memdisk '");
      p = grub_stpcpy (p, memdisk_path);
      *p++ = '\'';
      *p++ = ' ';
    }
  if (config_path)
    {
      p = grub_stpcpy (p, "--config '");
      p = grub_stpcpy (p, config_path);
      *p++ = '\'';
      *p++ = ' ';
    }
  for (pk = pubkeys; pk < pubkeys + npubkeys; pk++)
    {
      p = grub_stpcpy (p, "--pubkey '");
      p = grub_stpcpy (p, *pk);
      *p++ = '\'';
      *p++ = ' ';
    }

  for (md = modules.entries; *md; md++)
    {
      *p++ = '\'';
      p = grub_stpcpy (p, *md);
      *p++ = '\'';
      *p++ = ' ';
    }

  *p = '\0';

  grub_util_info ("grub-mkimage --directory '%s' --prefix '%s'"
		  " --output '%s' "
		  " --dtb '%s' "
		  "--format '%s' --compression '%s' %s %s\n",
		  dir, prefix,
		  outname, dtb ? : "", mkimage_target,
		  compnames[compression], note ? "--note" : "", s);
  free (s);

  tgt = grub_install_get_image_target (mkimage_target);
  if (!tgt)
    grub_util_error (_("unknown target format %s"), mkimage_target);

  grub_install_generate_image (dir, prefix, fp, outname,
			       modules.entries, memdisk_path,
			       pubkeys, npubkeys, config_path, tgt,
			       note, compression, dtb);
  while (dc--)
    grub_install_pop_module ();
}

void
grub_install_make_image_wrap (const char *dir, const char *prefix,
			      const char *outname, char *memdisk_path,
			      char *config_path,
			      const char *mkimage_target, int note)
{
  FILE *fp;

  fp = grub_util_fopen (outname, "wb");
  if (! fp)
    grub_util_error (_("cannot open `%s': %s"), outname,
		     strerror (errno));
  grub_install_make_image_wrap_file (dir, prefix, fp, outname,
				     memdisk_path, config_path,
				     mkimage_target, note);
  if (grub_util_file_sync (fp) < 0)
    grub_util_error (_("cannot sync `%s': %s"), outname, strerror (errno));
  fclose (fp);
}

static void
copy_by_ext (const char *srcd,
	     const char *dstd,
	     const char *extf,
	     int req)
{
  grub_util_fd_dir_t d;
  grub_util_fd_dirent_t de;

  d = grub_util_fd_opendir (srcd);
  if (!d && !req)
    return;
  if (!d)
    grub_util_error (_("cannot open directory `%s': %s"),
		     srcd, grub_util_fd_strerror ());

  while ((de = grub_util_fd_readdir (d)))
    {
      const char *ext = strrchr (de->d_name, '.');
      if (ext && strcmp (ext, extf) == 0)
	{
	  char *srcf = grub_util_path_concat (2, srcd, de->d_name);
	  char *dstf = grub_util_path_concat (2, dstd, de->d_name);
	  grub_install_compress_file (srcf, dstf, 1);
	  free (srcf);
	  free (dstf);
	}
    }
  grub_util_fd_closedir (d);
}

static void
copy_all (const char *srcd,
	     const char *dstd)
{
  grub_util_fd_dir_t d;
  grub_util_fd_dirent_t de;

  d = grub_util_fd_opendir (srcd);
  if (!d)
    grub_util_error (_("cannot open directory `%s': %s"),
		     srcd, grub_util_fd_strerror ());

  while ((de = grub_util_fd_readdir (d)))
    {
      char *srcf;
      char *dstf;
      if (strcmp (de->d_name, ".") == 0
	  || strcmp (de->d_name, "..") == 0)
	continue;
      srcf = grub_util_path_concat (2, srcd, de->d_name);
      if (grub_util_is_special_file (srcf)
	  || grub_util_is_directory (srcf))
	continue;
      dstf = grub_util_path_concat (2, dstd, de->d_name);
      grub_install_compress_file (srcf, dstf, 1);
      free (srcf);
      free (dstf);
    }
  grub_util_fd_closedir (d);
}

#if !(defined (GRUB_UTIL) && defined(ENABLE_NLS) && ENABLE_NLS)
static const char *
get_localedir (void)
{
  if (grub_install_locale_directory)
    return grub_install_locale_directory;
  else
    return grub_util_get_localedir ();
}

static void
copy_locales (const char *dstd)
{
  grub_util_fd_dir_t d;
  grub_util_fd_dirent_t de;
  const char *locale_dir = get_localedir ();

  d = grub_util_fd_opendir (locale_dir);
  if (!d)
    {
      grub_util_warn (_("cannot open directory `%s': %s"),
		      locale_dir, grub_util_fd_strerror ());
      return;
    }

  while ((de = grub_util_fd_readdir (d)))
    {
      char *srcf;
      char *dstf;
      char *ext;
      if (strcmp (de->d_name, ".") == 0)
	continue;
      if (strcmp (de->d_name, "..") == 0)
	continue;
      ext = grub_strrchr (de->d_name, '.');
      if (ext && (grub_strcmp (ext, ".mo") == 0
		  || grub_strcmp (ext, ".gmo") == 0))
	{
	  srcf = grub_util_path_concat (2, locale_dir, de->d_name);
	  dstf = grub_util_path_concat (2, dstd, de->d_name);
	  ext = grub_strrchr (dstf, '.');
	  grub_strcpy (ext, ".mo");
	}
      else
	{
	  srcf = grub_util_path_concat_ext (4, locale_dir, de->d_name,
					    "LC_MESSAGES", PACKAGE, ".mo");
	  dstf = grub_util_path_concat_ext (2, dstd, de->d_name, ".mo");
	}
      grub_install_compress_file (srcf, dstf, 0);
      free (srcf);
      free (dstf);
    }
  grub_util_fd_closedir (d);
}
#endif

static void
grub_install_copy_nls(const char *src __attribute__ ((unused)),
                      const char *dst __attribute__ ((unused)))
{
#if !(defined (GRUB_UTIL) && defined(ENABLE_NLS) && ENABLE_NLS)
  char *dst_locale;

  dst_locale = grub_util_path_concat (2, dst, "locale");
  grub_install_mkdir_p (dst_locale);
  clean_grub_dir (dst_locale);

  if (install_locales.is_default)
    {
      char *srcd = grub_util_path_concat (2, src, "po");
      copy_by_ext (srcd, dst_locale, ".mo", 0);
      copy_locales (dst_locale);
      free (srcd);
    }
  else
    {
      size_t i;
      const char *locale_dir = get_localedir ();

      for (i = 0; i < install_locales.n_entries; i++)
      {
        char *srcf = grub_util_path_concat_ext (3, src, "po",
                                                install_locales.entries[i],
                                                ".mo");
        char *dstf = grub_util_path_concat_ext (2, dst_locale,
                                                install_locales.entries[i],
                                                ".mo");
        if (grub_install_compress_file (srcf, dstf, 0))
          {
            free (srcf);
            free (dstf);
            continue;
          }
        free (srcf);
        srcf = grub_util_path_concat_ext (4, locale_dir,
                                          install_locales.entries[i],
                                          "LC_MESSAGES", PACKAGE, ".mo");
        if (grub_install_compress_file (srcf, dstf, 0) == 0)
          grub_util_error (_("cannot find locale `%s'"),
                           install_locales.entries[i]);
        free (srcf);
        free (dstf);
      }
    }
  free (dst_locale);
#endif
}

static struct
{
  const char *cpu;
  const char *platform;
} platforms[GRUB_INSTALL_PLATFORM_MAX] =
  {
    [GRUB_INSTALL_PLATFORM_I386_PC] =          { "i386",    "pc"        },
    [GRUB_INSTALL_PLATFORM_I386_EFI] =         { "i386",    "efi"       },
    [GRUB_INSTALL_PLATFORM_I386_QEMU] =        { "i386",    "qemu"      },
    [GRUB_INSTALL_PLATFORM_I386_COREBOOT] =    { "i386",    "coreboot"  },
    [GRUB_INSTALL_PLATFORM_I386_MULTIBOOT] =   { "i386",    "multiboot" },
    [GRUB_INSTALL_PLATFORM_I386_IEEE1275] =    { "i386",    "ieee1275"  },
    [GRUB_INSTALL_PLATFORM_X86_64_EFI] =       { "x86_64",  "efi"       },
    [GRUB_INSTALL_PLATFORM_I386_XEN] =         { "i386",    "xen"       },
    [GRUB_INSTALL_PLATFORM_X86_64_XEN] =       { "x86_64",  "xen"       },
    [GRUB_INSTALL_PLATFORM_I386_XEN_PVH] =     { "i386",    "xen_pvh"   },
    [GRUB_INSTALL_PLATFORM_MIPSEL_LOONGSON] =  { "mipsel",  "loongson"  },
    [GRUB_INSTALL_PLATFORM_MIPSEL_QEMU_MIPS] = { "mipsel",  "qemu_mips" },
    [GRUB_INSTALL_PLATFORM_MIPS_QEMU_MIPS] =   { "mips",    "qemu_mips" },
    [GRUB_INSTALL_PLATFORM_MIPSEL_ARC] =       { "mipsel",  "arc"       },
    [GRUB_INSTALL_PLATFORM_MIPS_ARC] =         { "mips",    "arc"       },
    [GRUB_INSTALL_PLATFORM_SPARC64_IEEE1275] = { "sparc64", "ieee1275"  },
    [GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275] = { "powerpc", "ieee1275"  },
    [GRUB_INSTALL_PLATFORM_IA64_EFI] =         { "ia64",    "efi"       },
    [GRUB_INSTALL_PLATFORM_ARM_EFI] =          { "arm",     "efi"       },
    [GRUB_INSTALL_PLATFORM_ARM64_EFI] =        { "arm64",   "efi"       },
    [GRUB_INSTALL_PLATFORM_MIPS64EL_EFI] =     { "mips64el","efi"       },
    [GRUB_INSTALL_PLATFORM_ARM_UBOOT] =        { "arm",     "uboot"     },
    [GRUB_INSTALL_PLATFORM_ARM_COREBOOT] =     { "arm",     "coreboot"  },
    [GRUB_INSTALL_PLATFORM_RISCV32_EFI] =      { "riscv32", "efi"       },
    [GRUB_INSTALL_PLATFORM_RISCV64_EFI] =      { "riscv64", "efi"       },
  }; 

char *
grub_install_get_platforms_string (void)
{
  char **arr = xmalloc (sizeof (char *) * ARRAY_SIZE (platforms));
  int platform_strins_len = 0;
  char *platforms_string;
  char *ptr;
  unsigned i;
  for (i = 0; i < ARRAY_SIZE (platforms); i++)
    {
      arr[i] = xasprintf ("%s-%s", platforms[i].cpu,
			  platforms[i].platform);
      platform_strins_len += strlen (arr[i]) + 2;
    }
  ptr = platforms_string = xmalloc (platform_strins_len);
  qsort (arr, ARRAY_SIZE (platforms), sizeof (char *), grub_qsort_strcmp);
  for (i = 0; i < ARRAY_SIZE (platforms); i++)
    {
      strcpy (ptr, arr[i]);
      ptr += strlen (arr[i]);
      *ptr++ = ',';
      *ptr++ = ' ';
      free (arr[i]);
    }
  ptr[-2] = 0;
  free (arr);
 
  return platforms_string;
}

char *
grub_install_get_platform_name (enum grub_install_plat platid)
{
  return xasprintf ("%s-%s", platforms[platid].cpu,
		    platforms[platid].platform);
}

const char *
grub_install_get_platform_cpu (enum grub_install_plat platid)
{
  return platforms[platid].cpu;
}

const char *
grub_install_get_platform_platform (enum grub_install_plat platid)
{
  return platforms[platid].platform;
}


void
grub_install_copy_files (const char *src,
			 const char *dst,
			 enum grub_install_plat platid)
{
  char *dst_platform, *dst_fonts;
  const char *pkgdatadir = grub_util_get_pkgdatadir ();
  char *themes_dir;

  {
    char *platform;
    platform = xasprintf ("%s-%s", platforms[platid].cpu,
			  platforms[platid].platform);
    dst_platform = grub_util_path_concat (2, dst, platform);
    free (platform);
  }
  dst_fonts = grub_util_path_concat (2, dst, "fonts");
  grub_install_mkdir_p (dst_platform);
  clean_grub_dir (dst);
  clean_grub_dir (dst_platform);

  grub_install_copy_nls(src, dst);

  if (install_modules.is_default)
    copy_by_ext (src, dst_platform, ".mod", 1);
  else
    {
      struct grub_util_path_list *path_list, *p;

      path_list = grub_util_resolve_dependencies (src, "moddep.lst",
						  install_modules.entries);
      for (p = path_list; p; p = p->next)
	{
	  const char *srcf = p->name;
	  const char *dir;
	  char *dstf;

	  dir = grub_strrchr (srcf, '/');
	  if (dir)
	    dir++;
	  else
	    dir = srcf;
	  dstf = grub_util_path_concat (2, dst_platform, dir);
	  grub_install_compress_file (srcf, dstf, 1);
	  free (dstf);
	}

      grub_util_free_path_list (path_list);
    }

  const char *pkglib_DATA[] = {"efiemu32.o", "efiemu64.o",
			       "moddep.lst", "command.lst",
			       "fs.lst", "partmap.lst",
			       "parttool.lst",
			       "video.lst", "crypto.lst",
			       "terminal.lst", "modinfo.sh" };
  size_t i;

  for (i = 0; i < ARRAY_SIZE (pkglib_DATA); i++)
    {
      char *srcf = grub_util_path_concat (2, src, pkglib_DATA[i]);
      char *dstf = grub_util_path_concat (2, dst_platform, pkglib_DATA[i]);
      if (i == 0 || i == 1)
	grub_install_compress_file (srcf, dstf, 0);
      else
	grub_install_compress_file (srcf, dstf, 1);
      free (srcf);
      free (dstf);
    }

  if (install_themes.is_default)
    {
      install_themes.is_default = 0;
      install_themes.n_entries = 1;
      install_themes.entries = xmalloc (2 * sizeof (install_themes.entries[0]));
      install_themes.entries[0] = xstrdup ("starfield");
      install_themes.entries[1] = NULL;
    }

  if (grub_install_themes_directory)
    themes_dir = xstrdup (grub_install_themes_directory);
  else
    themes_dir = grub_util_path_concat (2, grub_util_get_pkgdatadir (),
					"themes");

  for (i = 0; i < install_themes.n_entries; i++)
    {
      char *srcf = grub_util_path_concat (3, themes_dir,
					install_themes.entries[i],
					"theme.txt");
      if (grub_util_is_regular (srcf))
	{
	  char *srcd = grub_util_path_concat (2, themes_dir,
					    install_themes.entries[i]);
	  char *dstd = grub_util_path_concat (3, dst, "themes",
					    install_themes.entries[i]);
	  grub_install_mkdir_p (dstd);
	  copy_all (srcd, dstd);
	  free (srcd);
	  free (dstd);
	}
      free (srcf);
    }

  free (themes_dir);

  if (install_fonts.is_default)
    {
      install_fonts.is_default = 0;
      install_fonts.n_entries = 1;
      install_fonts.entries = xmalloc (2 * sizeof (install_fonts.entries[0]));
      install_fonts.entries[0] = xstrdup ("unicode");
      install_fonts.entries[1] = NULL;
    }

  grub_install_mkdir_p (dst_fonts);

  for (i = 0; i < install_fonts.n_entries; i++)
    {
      char *srcf = grub_util_path_concat_ext (2, pkgdatadir,
						   install_fonts.entries[i],
						   ".pf2");
      char *dstf = grub_util_path_concat_ext (2, dst_fonts,
						   install_fonts.entries[i],
						   ".pf2");

      grub_install_compress_file (srcf, dstf, 0);
      free (srcf);
      free (dstf);
    }

  free (dst_platform);
  free (dst_fonts);
}

enum grub_install_plat
grub_install_get_target (const char *src)
{
  char *fn;
  grub_util_fd_t f;
  char buf[8192];
  ssize_t r;
  char *c, *pl, *p;
  size_t i;
  fn = grub_util_path_concat (2, src, "modinfo.sh");
  f = grub_util_fd_open (fn, GRUB_UTIL_FD_O_RDONLY);
  if (!GRUB_UTIL_FD_IS_VALID (f))
    grub_util_error (_("%s doesn't exist. Please specify --target or --directory"), 
		     fn);
  r = grub_util_fd_read (f, buf, sizeof (buf) - 1);
  if (r < 0)
    grub_util_error (_("cannot read `%s': %s"), fn, strerror (errno));
  grub_util_fd_close (f);
  buf[r] = '\0';
  c = strstr (buf, "grub_modinfo_target_cpu=");
  if (!c || (c != buf && !grub_isspace (*(c-1))))
    grub_util_error (_("invalid modinfo file `%s'"), fn);
  pl = strstr (buf, "grub_modinfo_platform=");
  if (!pl || (pl != buf && !grub_isspace (*(pl-1))))
    grub_util_error (_("invalid modinfo file `%s'"), fn);
  c += sizeof ("grub_modinfo_target_cpu=") - 1;
  pl += sizeof ("grub_modinfo_platform=") - 1;
  for (p = c; *p && !grub_isspace (*p); p++);
  *p = '\0';
  for (p = pl; *p && !grub_isspace (*p); p++);
  *p = '\0';

  for (i = 0; i < ARRAY_SIZE (platforms); i++)
    if (strcmp (platforms[i].cpu, c) == 0
	&& strcmp (platforms[i].platform, pl) == 0)
      {
	free (fn);
	return i;
      }
  grub_util_error (_("Unknown platform `%s-%s'"), c, pl);
}


void
grub_util_unlink_recursive (const char *name)
{
  grub_util_fd_dir_t d;
  grub_util_fd_dirent_t de;

  d = grub_util_fd_opendir (name);

  while ((de = grub_util_fd_readdir (d)))
    {
      char *fp;
      if (strcmp (de->d_name, ".") == 0)
	continue;
      if (strcmp (de->d_name, "..") == 0)
	continue;
      fp = grub_util_path_concat (2, name, de->d_name);
      if (grub_util_is_special_file (fp))
	{
	  free (fp);
	  continue;
	}
      if (grub_util_is_regular (fp))
	grub_util_unlink (fp);
      else if (grub_util_is_directory (fp))
	grub_util_unlink_recursive (fp);
      free (fp);
    }
  grub_util_rmdir (name);
  grub_util_fd_closedir (d);
}
