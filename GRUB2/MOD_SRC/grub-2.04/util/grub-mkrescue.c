/*
 *  Make GRUB rescue image
 *
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 1999,2000,2001,2002,2003,2004,2005,2006,2007,2008,2009,2010  Free Software Foundation, Inc.
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

#include <grub/util/install.h>
#include <grub/util/misc.h>
#include <grub/emu/exec.h>
#include <grub/emu/config.h>
#include <grub/emu/hostdisk.h>
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#include <argp.h>
#pragma GCC diagnostic error "-Wmissing-prototypes"
#pragma GCC diagnostic error "-Wmissing-declarations"

#include <sys/types.h>
#include <sys/wait.h>

#include <string.h>
#include <time.h>

static char *source_dirs[GRUB_INSTALL_PLATFORM_MAX];
static char *rom_directory;
static char *label_font;
static char *label_color;
static char *label_bgcolor;
static char *product_name;
static char *product_version;
static char *output_image;
static char *xorriso;
static char *boot_grub;
static int xorriso_argc;
static int xorriso_arg_alloc;
static char **xorriso_argv;
static char *iso_uuid;
static char *iso9660_dir;

static void
xorriso_push (const char *val)
{
  if (xorriso_arg_alloc <= xorriso_argc + 1)
    {
      xorriso_arg_alloc = 2 * (4 + xorriso_argc);
      xorriso_argv = xrealloc (xorriso_argv,
			       sizeof (xorriso_argv[0])
			       * xorriso_arg_alloc);
    }
  xorriso_argv[xorriso_argc++] = xstrdup (val);
}

static void
xorriso_link (const char *from, const char *to)
{
  char *tof = grub_util_path_concat (2, iso9660_dir, to);
  char *val = xasprintf ("%s=%s", from, tof);
  xorriso_push (val);
  free (val);
  free (tof);
}

enum
  {
    OPTION_OUTPUT = 'o',
    OPTION_ROM_DIRECTORY = 0x301,
    OPTION_XORRISO,
    OPTION_GLUE_EFI,
    OPTION_RENDER_LABEL,
    OPTION_LABEL_FONT,
    OPTION_LABEL_COLOR,
    OPTION_LABEL_BGCOLOR,
    OPTION_PRODUCT_NAME,
    OPTION_PRODUCT_VERSION,
    OPTION_SPARC_BOOT,
    OPTION_ARCS_BOOT
  };

static struct argp_option options[] = {
  GRUB_INSTALL_OPTIONS,
  {"output", 'o', N_("FILE"),
   0, N_("save output in FILE [required]"), 2},
  {"rom-directory", OPTION_ROM_DIRECTORY, N_("DIR"),
   0, N_("save ROM images in DIR [optional]"), 2},
  {"xorriso", OPTION_XORRISO, N_("FILE"),
   /* TRANSLATORS: xorriso is a program for creating ISOs and burning CDs.  */
   0, N_("use FILE as xorriso [optional]"), 2},
  {"grub-glue-efi", OPTION_GLUE_EFI, N_("FILE"), OPTION_HIDDEN, 0, 2},
  {"grub-render-label", OPTION_RENDER_LABEL, N_("FILE"), OPTION_HIDDEN, 0, 2},
  {"label-font", OPTION_LABEL_FONT, N_("FILE"), 0, N_("use FILE as font for label"), 2},
  {"label-color", OPTION_LABEL_COLOR, N_("COLOR"), 0, N_("use COLOR for label"), 2},
  {"label-bgcolor", OPTION_LABEL_BGCOLOR, N_("COLOR"), 0, N_("use COLOR for label background"), 2},
  {"product-name", OPTION_PRODUCT_NAME, N_("STRING"), 0, N_("use STRING as product name"), 2},
  {"product-version", OPTION_PRODUCT_VERSION, N_("STRING"), 0, N_("use STRING as product version"), 2},
  {"sparc-boot", OPTION_SPARC_BOOT, 0, 0, N_("enable sparc boot. Disables HFS+, APM, ARCS and boot as disk image for i386-pc"), 2},
  {"arcs-boot", OPTION_ARCS_BOOT, 0, 0, N_("enable ARCS (big-endian mips machines, mostly SGI) boot. Disables HFS+, APM, sparc64 and boot as disk image for i386-pc"), 2},
  {0, 0, 0, 0, 0, 0}
};

#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static char *
help_filter (int key, const char *text, void *input __attribute__ ((unused)))
{
  switch (key)
    {
    case ARGP_KEY_HELP_PRE_DOC:
      /* TRANSLATORS: it generates one single image which is bootable through any method. */
      return strdup (_("Make GRUB CD-ROM, disk, pendrive and floppy bootable image."));
    case ARGP_KEY_HELP_POST_DOC:
      {
	char *p1, *out;

	p1 = xasprintf (_("Generates a bootable CD/USB/floppy image.  Arguments other than options to this program"
      " are passed to xorriso, and indicate source files, source directories, or any of the "
      "mkisofs options listed by the output of `%s'."), "xorriso -as mkisofs -help");
	out = xasprintf ("%s\n\n%s\n\n%s", p1,
	  _("Option -- switches to native xorriso command mode."),
	  _("Mail xorriso support requests to <bug-xorriso@gnu.org>."));
	free (p1);
	return out;
      }
    default:
      return grub_install_help_filter (key, text, input);
    }
}

#pragma GCC diagnostic error "-Wformat-nonliteral"

enum {
  SYS_AREA_AUTO,
  SYS_AREA_COMMON,
  SYS_AREA_SPARC,
  SYS_AREA_ARCS
} system_area = SYS_AREA_AUTO;

static error_t 
argp_parser (int key, char *arg, struct argp_state *state)
{
  if (grub_install_parse (key, arg))
    return 0;
  switch (key)
    {
    case OPTION_OUTPUT:
      free (output_image);
      output_image = xstrdup (arg);
      return 0;
    case OPTION_ROM_DIRECTORY:
      free (rom_directory);
      rom_directory = xstrdup (arg);
      return 0;

      /*
       FIXME:
    # Intentionally undocumented
    --grub-mkimage-extra)
	mkimage_extra_arg="$mkimage_extra_arg `argument $option "$@"`"; shift ;;
    --grub-mkimage-extra=*)
	mkimage_extra_arg="$mkimage_extra_arg `echo "$option" | sed 's/--grub-mkimage-extra=//'`" ;;
      */
    case OPTION_SPARC_BOOT:
      system_area = SYS_AREA_SPARC;
      return 0;
    case OPTION_ARCS_BOOT:
      system_area = SYS_AREA_ARCS;
      return 0;
    case OPTION_PRODUCT_NAME:
      free (product_name);
      product_name = xstrdup (arg);
      return 0;
    case OPTION_PRODUCT_VERSION:
      free (product_version);
      product_version = xstrdup (arg);
      return 0;
      /* Accept and ignore for compatibility.  */
    case OPTION_GLUE_EFI:
    case OPTION_RENDER_LABEL:
      return 0;
    case OPTION_LABEL_FONT:
      free (label_font);
      label_font = xstrdup (arg);
      return 0;

    case OPTION_LABEL_COLOR:
      free (label_color);
      label_color = xstrdup (arg);
      return 0;

    case OPTION_LABEL_BGCOLOR:
      free (label_bgcolor);
      label_bgcolor = xstrdup (arg);
      return 0;

    case OPTION_XORRISO:
      free (xorriso);
      xorriso = xstrdup (arg);
      return 0;

    default:
      return ARGP_ERR_UNKNOWN;
    }
}

struct argp argp = {
  options, argp_parser, N_("[OPTION] SOURCE..."),
  NULL, NULL, help_filter, NULL
};

static void
write_part (FILE *f, const char *srcdir)
{
  FILE *in;
  char *inname = grub_util_path_concat (2, srcdir, "partmap.lst");
  char buf[260];
  in = grub_util_fopen (inname, "rb");
  if (!in)
    return;
  while (fgets (buf, 256, in))
    {
      char *ptr;
      for (ptr = buf + strlen (buf) - 1;
	   ptr >= buf && (*ptr == '\n' || *ptr == '\r');
	   ptr--);
      ptr[1] = '\0';
      fprintf (f, "insmod %s\n", buf);
    }
  fclose (in);
}

static void
make_image_abs (enum grub_install_plat plat,
		const char *mkimage_target,
		const char *output)
{
  char *load_cfg;
  FILE *load_cfg_f;

  if (!source_dirs[plat])
    return;

  grub_util_info (N_("enabling %s support ..."),
		  mkimage_target);

  load_cfg = grub_util_make_temporary_file ();

  load_cfg_f = grub_util_fopen (load_cfg, "wb");
  fprintf (load_cfg_f, "search --fs-uuid --set=root %s\n", iso_uuid);
  fprintf (load_cfg_f, "set prefix=(${root})/boot/grub\n");

  write_part (load_cfg_f, source_dirs[plat]);
  fclose (load_cfg_f);

  grub_install_push_module ("search");
  grub_install_push_module ("iso9660");
  grub_install_make_image_wrap (source_dirs[plat], "/boot/grub", output,
				0, load_cfg,
				mkimage_target, 0);
  grub_install_pop_module ();
  grub_install_pop_module ();
  grub_util_unlink (load_cfg);
}

static void
make_image (enum grub_install_plat plat,
	    const char *mkimage_target,
	    const char *output_sub)
{
  char *out = grub_util_path_concat (2, boot_grub, output_sub);
  make_image_abs (plat, mkimage_target, out);
  free (out);
}

static void
make_image_fwdisk_abs (enum grub_install_plat plat,
		       const char *mkimage_target,
		       const char *output)
{
  char *load_cfg;
  FILE *load_cfg_f;

  if (!source_dirs[plat])
    return;

  grub_util_info (N_("enabling %s support ..."),
		  mkimage_target);

  load_cfg = grub_util_make_temporary_file ();

  load_cfg_f = grub_util_fopen (load_cfg, "wb");
  write_part (load_cfg_f, source_dirs[plat]);
  fclose (load_cfg_f);

  grub_install_push_module ("iso9660");
  grub_install_make_image_wrap (source_dirs[plat], "()/boot/grub", output,
				0, load_cfg, mkimage_target, 0);
  grub_install_pop_module ();
  grub_util_unlink (load_cfg);
}

static int
check_xorriso (const char *val)
{
  const char *argv[5];
  int fd;
  pid_t pid;
  FILE *mdadm;
  char *buf = NULL;
  size_t len = 0;
  int ret = 0;
  int wstatus = 0;

  argv[0] = xorriso;
  argv[1] = "-as";
  argv[2] = "mkisofs";
  argv[3] = "-help";
  argv[4] = NULL;

  pid = grub_util_exec_pipe_stderr (argv, &fd);

  if (!pid)
    return 0;

  /* Parent.  Read mdadm's output.  */
  mdadm = fdopen (fd, "r");
  if (! mdadm)
    return 0;

  while (getline (&buf, &len, mdadm) > 0)
    {
      if (grub_strstr (buf, val))
	ret = 1;
    }

  close (fd);
  waitpid (pid, &wstatus, 0);
  free (buf);
  if (!WIFEXITED (wstatus) || WEXITSTATUS(wstatus) != 0)
    return 0;
  return ret;
}

static void
make_image_fwdisk (enum grub_install_plat plat,
		   const char *mkimage_target,
		   const char *output_sub)
{
  char *out = grub_util_path_concat (2, boot_grub, output_sub);
  make_image_fwdisk_abs (plat, mkimage_target, out);
  free (out);
}

static int
option_is_end (const struct argp_option *opt)
{
  return !opt->key && !opt->name && !opt->doc && !opt->group;
}


static int
args_to_eat (const char *arg)
{
  int j;

  if (arg[0] != '-')
    return 0;

  if (arg[1] == '-')
    {
      for (j = 0; !option_is_end(&options[j]); j++)
	{
	  size_t len = strlen (options[j].name);
	  if (strncmp (arg + 2, options[j].name, len) == 0)
	    {
	      if (arg[2 + len] == '=')
		return 1;
	      if (arg[2 + len] == '\0' && options[j].arg)
		return 2;
	      if (arg[2 + len] == '\0')
		return 1;
	    }
	}
      if (strcmp (arg, "--help") == 0)
	return 1;
      if (strcmp (arg, "--usage") == 0)
	return 1;
      if (strcmp (arg, "--version") == 0)
	return 1;
      return 0;
    }
  if (arg[2] && arg[3])
    return 0;
  for (j = 0; !option_is_end(&options[j]); j++)
    {
      if (options[j].key > 0 && options[j].key < 128 && arg[1] == options[j].key)
	{
	  if (options[j].arg)
	    return 2;
	  return 1;
	}
      if (arg[1] == '?')
	return 1;
    }
  return 0;
}

int
main (int argc, char *argv[])
{
  char *romdir;
  char *sysarea_img = NULL;
  const char *pkgdatadir;
  int argp_argc;
  char **argp_argv;
  int xorriso_tail_argc;
  char **xorriso_tail_argv;
  int rv;

  grub_util_host_init (&argc, &argv);
  grub_util_disable_fd_syncs ();

  pkgdatadir = grub_util_get_pkgdatadir ();

  product_name = xstrdup (PACKAGE_NAME);
  product_version = xstrdup (PACKAGE_VERSION);
  xorriso = xstrdup ("xorriso");
  label_font = grub_util_path_concat (2, pkgdatadir, "unicode.pf2");

  argp_argv = xmalloc (sizeof (argp_argv[0]) * argc);
  xorriso_tail_argv = xmalloc (sizeof (argp_argv[0]) * argc);

  xorriso_tail_argc = 0;
  /* Program name */
  argp_argv[0] = argv[0];
  argp_argc = 1;

  /* argp doesn't allow us to catch unknwon arguments,
     so catch them before passing to argp
   */
  {
    int i;
    for (i = 1; i < argc; i++)
      {
	if (strcmp (argv[i], "-output") == 0) {
	  argp_argv[argp_argc++] = (char *) "--output";
	  i++;
	  argp_argv[argp_argc++] = argv[i];
	  continue;
	}
	switch (args_to_eat (argv[i]))
	  {
	  case 2:
	    argp_argv[argp_argc++] = argv[i++];
	    /* Fallthrough  */
	  case 1:
	    argp_argv[argp_argc++] = argv[i];
	    break;
	  case 0:
	    xorriso_tail_argv[xorriso_tail_argc++] = argv[i];
	    break;
	  }
      }
  }

  argp_parse (&argp, argp_argc, argp_argv, 0, 0, 0);

  if (!output_image)
    grub_util_error ("%s", _("output file must be specified"));

  if (!check_xorriso ("graft-points")) {
    grub_util_error ("%s", _("xorriso not found"));
  }

  grub_init_all ();
  grub_hostfs_init ();
  grub_host_init ();

  xorriso_push (xorriso);
  xorriso_push ("-as");
  xorriso_push ("mkisofs");
  xorriso_push ("-graft-points");
  
  iso9660_dir = grub_util_make_temporary_dir ();
  grub_util_info ("temporary iso9660 dir is `%s'", iso9660_dir);
  boot_grub = grub_util_path_concat (3, iso9660_dir, "boot", "grub");
  grub_install_mkdir_p (boot_grub);
  romdir = grub_util_path_concat (2, boot_grub, "roms");
  grub_util_mkdir (romdir);

  if (!grub_install_source_directory)
    {
      const char *pkglibdir = grub_util_get_pkglibdir ();
      enum grub_install_plat plat;

      for (plat = 0; plat < GRUB_INSTALL_PLATFORM_MAX; plat++)
	{
	  char *platdir = grub_util_path_concat (2, pkglibdir,
						 grub_install_get_platform_name (plat));

	  if (!grub_util_is_directory (platdir))
	    {
	      free (platdir);
	      continue;
	    }
	  source_dirs[plat] = platdir;
	  grub_install_copy_files (platdir,
				   boot_grub, plat);
	}
    }
  else
    {
      enum grub_install_plat plat;
      plat = grub_install_get_target (grub_install_source_directory);
      grub_install_copy_files (grub_install_source_directory,
			       boot_grub, plat);
      source_dirs[plat] = xstrdup (grub_install_source_directory);
    }
  if (system_area == SYS_AREA_AUTO || grub_install_source_directory)
    {
      if (source_dirs[GRUB_INSTALL_PLATFORM_I386_PC]
	  || source_dirs[GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275]
	  || source_dirs[GRUB_INSTALL_PLATFORM_I386_EFI]
	  || source_dirs[GRUB_INSTALL_PLATFORM_IA64_EFI]
	  || source_dirs[GRUB_INSTALL_PLATFORM_ARM_EFI]
	  || source_dirs[GRUB_INSTALL_PLATFORM_ARM64_EFI]
	  || source_dirs[GRUB_INSTALL_PLATFORM_MIPS64EL_EFI]
	  || source_dirs[GRUB_INSTALL_PLATFORM_RISCV32_EFI]
	  || source_dirs[GRUB_INSTALL_PLATFORM_RISCV64_EFI]
	  || source_dirs[GRUB_INSTALL_PLATFORM_X86_64_EFI])
	system_area = SYS_AREA_COMMON;
      else if (source_dirs[GRUB_INSTALL_PLATFORM_SPARC64_IEEE1275])
	system_area = SYS_AREA_SPARC;
      else if (source_dirs[GRUB_INSTALL_PLATFORM_MIPS_ARC])
	system_area = SYS_AREA_ARCS;
    }

  /* obtain date-based UUID.  */
  {
    time_t tim;
    struct tm *tmm;
    tim = time (NULL);
    tmm = gmtime (&tim);
    iso_uuid = xmalloc (55);
    grub_snprintf (iso_uuid, 50,
		   "%04d-%02d-%02d-%02d-%02d-%02d-00",
		   tmm->tm_year + 1900,
		   tmm->tm_mon + 1,
		   tmm->tm_mday,
		   tmm->tm_hour,
		   tmm->tm_min,
		   tmm->tm_sec);
  }
  {
    char *uuid_out = xmalloc (strlen (iso_uuid) + 1 + 40);
    char *optr;
    const char *iptr;
    optr = grub_stpcpy (uuid_out, "--modification-date=");
    for (iptr = iso_uuid; *iptr; iptr++)
      if (*iptr != '-')
	*optr++ = *iptr;
    *optr = '\0';
    xorriso_push (uuid_out);
    free (uuid_out);
  }

  /* build BIOS core.img.  */
  if (source_dirs[GRUB_INSTALL_PLATFORM_I386_PC])
    {
      char *load_cfg;
      FILE *load_cfg_f;
      char *output = grub_util_path_concat (3, boot_grub, "i386-pc", "eltorito.img");
      load_cfg = grub_util_make_temporary_file ();

      grub_util_info (N_("enabling %s support ..."), "BIOS");
      load_cfg_f = grub_util_fopen (load_cfg, "wb");
      write_part (load_cfg_f, source_dirs[GRUB_INSTALL_PLATFORM_I386_PC]);
      fclose (load_cfg_f);

      grub_install_push_module ("biosdisk");
      grub_install_push_module ("iso9660");
      grub_install_make_image_wrap (source_dirs[GRUB_INSTALL_PLATFORM_I386_PC],
				    "/boot/grub", output,
				    0, load_cfg,
				    "i386-pc-eltorito", 0);

      xorriso_push ("-b");
      xorriso_push ("boot/grub/i386-pc/eltorito.img");
      xorriso_push ("-no-emul-boot");
      xorriso_push ("-boot-load-size");
      xorriso_push ("4");
      xorriso_push ("-boot-info-table");
      if (system_area == SYS_AREA_COMMON)
	{
	  if (check_xorriso ("grub2-boot-info"))
	    {
	      char *boot_hybrid = grub_util_path_concat (2, source_dirs[GRUB_INSTALL_PLATFORM_I386_PC],
							 "boot_hybrid.img");
	      xorriso_push ("--grub2-boot-info");
	      xorriso_push ("--grub2-mbr");
	      xorriso_push (boot_hybrid);
	    }
	  else
	    {
	      FILE *sa, *bi;
	      size_t sz;
	      char buf[512];
	      char *bin = grub_util_path_concat (2, source_dirs[GRUB_INSTALL_PLATFORM_I386_PC],
						 "boot.img");
	      grub_util_warn ("%s", _("Your xorriso doesn't support `--grub2-boot-info'. Some features are disabled. Please use xorriso 1.2.9 or later."));
	      sysarea_img = grub_util_make_temporary_file ();
	      sa = grub_util_fopen (sysarea_img, "wb");
	      if (!sa)
		grub_util_error (_("cannot open `%s': %s"), sysarea_img,
				 strerror (errno));
	      bi = grub_util_fopen (bin, "rb");
	      if (!bi)
		grub_util_error (_("cannot open `%s': %s"), bin,
				 strerror (errno));
	      if (fread (buf, 1, 512, bi) != 512)
		grub_util_error (_("cannot read `%s': %s"), bin,
				 strerror (errno));
	      fclose (bi);
	      fwrite (buf, 1, 512, sa);
	      
	      grub_install_make_image_wrap_file (source_dirs[GRUB_INSTALL_PLATFORM_I386_PC],
						 "/boot/grub", sa, sysarea_img,
						 0, load_cfg,
						 "i386-pc", 0);
	      sz = ftello (sa);
	      fflush (sa);
	      grub_util_fd_sync (fileno (sa));
	      fclose (sa);
	      
	      if (sz > 32768)
		{
		  grub_util_warn ("%s", _("Your xorriso doesn't support `--grub2-boot-info'. Your core image is too big. Boot as disk is disabled. Please use xorriso 1.2.9 or later."));
		}
	      else
		{
		  xorriso_push ("-G");
		  xorriso_push (sysarea_img);
		}
	    }
	}
      grub_install_pop_module ();
      grub_install_pop_module ();
      grub_util_unlink (load_cfg);
    }

  /** build multiboot core.img */
  grub_install_push_module ("pata");
  grub_install_push_module ("ahci");
  grub_install_push_module ("at_keyboard");
  make_image (GRUB_INSTALL_PLATFORM_I386_MULTIBOOT, "i386-multiboot", "i386-multiboot/core.elf");
  grub_install_pop_module ();
  grub_install_pop_module ();
  grub_install_pop_module ();

  make_image_fwdisk (GRUB_INSTALL_PLATFORM_I386_IEEE1275, "i386-ieee1275", "ofwx86.elf");

  char *core_services = NULL;

  if (source_dirs[GRUB_INSTALL_PLATFORM_I386_EFI]
      || source_dirs[GRUB_INSTALL_PLATFORM_X86_64_EFI]
      || source_dirs[GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275])
    {
      char *mach_ker, *sv, *label, *label_text;
      FILE *f;
      core_services = grub_util_path_concat (4, iso9660_dir, "System", "Library", "CoreServices");
      grub_install_mkdir_p (core_services);

      mach_ker = grub_util_path_concat (2, iso9660_dir, "mach_kernel");
      f = grub_util_fopen (mach_ker, "wb");
      fclose (f);
      free (mach_ker);

      sv = grub_util_path_concat (2, core_services, "SystemVersion.plist");
      f = grub_util_fopen (sv, "wb");
      fprintf (f, "<plist version=\"1.0\">\n"
	       "<dict>\n"
	       "        <key>ProductBuildVersion</key>\n"
	       "        <string></string>\n"
	       "        <key>ProductName</key>\n"
	       "        <string>%s</string>\n"
	       "        <key>ProductVersion</key>\n"
	       "        <string>%s</string>\n"
	       "</dict>\n"
	       "</plist>\n", product_name, product_version);
      fclose (f);
      free (sv);
      label = grub_util_path_concat (2, core_services, ".disk_label");
      char *label_string = xasprintf ("%s %s", product_name, product_version);
      grub_util_render_label (label_font, label_bgcolor ? : "white",
			      label_color ? : "black", label_string, label);
      free (label);
      label_text = grub_util_path_concat (2, core_services, ".disk_label.contentDetails");
      f = grub_util_fopen (label_text, "wb");
      fprintf (f, "%s\n", label_string);
      fclose (f);
      free (label_string);
      free (label_text);
      if (system_area == SYS_AREA_COMMON)
	{
	  xorriso_push ("-hfsplus");
	  xorriso_push ("-apm-block-size");
	  xorriso_push ("2048");
	  xorriso_push ("-hfsplus-file-creator-type");
	  xorriso_push ("chrp");
	  xorriso_push ("tbxj");
	  xorriso_push ("/System/Library/CoreServices/.disk_label");

	  if (source_dirs[GRUB_INSTALL_PLATFORM_I386_EFI]
	      || source_dirs[GRUB_INSTALL_PLATFORM_X86_64_EFI])
	    {
	      xorriso_push ("-hfs-bless-by");
	      xorriso_push ("i");
	      xorriso_push ("/System/Library/CoreServices/boot.efi");
	    }
	}
    }

  if (source_dirs[GRUB_INSTALL_PLATFORM_I386_EFI]
      || source_dirs[GRUB_INSTALL_PLATFORM_X86_64_EFI]
      || source_dirs[GRUB_INSTALL_PLATFORM_IA64_EFI]
      || source_dirs[GRUB_INSTALL_PLATFORM_ARM_EFI]
      || source_dirs[GRUB_INSTALL_PLATFORM_ARM64_EFI]
	  || source_dirs[GRUB_INSTALL_PLATFORM_MIPS64EL_EFI]
      || source_dirs[GRUB_INSTALL_PLATFORM_RISCV32_EFI]
      || source_dirs[GRUB_INSTALL_PLATFORM_RISCV64_EFI])
    {
      char *efidir = grub_util_make_temporary_dir ();
      char *efidir_efi = grub_util_path_concat (2, efidir, "efi");
      char *efidir_efi_boot = grub_util_path_concat (3, efidir, "efi", "boot");
      char *imgname, *img32, *img64, *img_mac = NULL;
      char *efiimgfat;
      grub_install_mkdir_p (efidir_efi_boot);

      grub_install_push_module ("part_gpt");
      grub_install_push_module ("part_msdos");

      imgname = grub_util_path_concat (2, efidir_efi_boot, "bootia64.efi");
      make_image_fwdisk_abs (GRUB_INSTALL_PLATFORM_IA64_EFI, "ia64-efi", imgname);
      free (imgname);

      grub_install_push_module ("part_apple");
      img64 = grub_util_path_concat (2, efidir_efi_boot, "bootx64.efi");
      make_image_fwdisk_abs (GRUB_INSTALL_PLATFORM_X86_64_EFI, "x86_64-efi", img64);
      grub_install_pop_module ();

      grub_install_push_module ("part_apple");
      img32 = grub_util_path_concat (2, efidir_efi_boot, "bootia32.efi");
      make_image_fwdisk_abs (GRUB_INSTALL_PLATFORM_I386_EFI, "i386-efi", img32);
      grub_install_pop_module ();

      imgname = grub_util_path_concat (2, efidir_efi_boot, "bootarm.efi");
      make_image_fwdisk_abs (GRUB_INSTALL_PLATFORM_ARM_EFI, "arm-efi", imgname);
      free (imgname);

      imgname = grub_util_path_concat (2, efidir_efi_boot, "bootaa64.efi");
      make_image_fwdisk_abs (GRUB_INSTALL_PLATFORM_ARM64_EFI, "arm64-efi",
			     imgname);
      free (imgname);

      imgname = grub_util_path_concat (2, efidir_efi_boot, "bootmips64el.efi");
      make_image_fwdisk_abs (GRUB_INSTALL_PLATFORM_MIPS64EL_EFI, "mips64el-efi",
			     imgname);
      free (imgname);
	  
      imgname = grub_util_path_concat (2, efidir_efi_boot, "bootriscv32.efi");
      make_image_fwdisk_abs (GRUB_INSTALL_PLATFORM_RISCV32_EFI, "riscv32-efi",
			     imgname);
      free (imgname);

      imgname = grub_util_path_concat (2, efidir_efi_boot, "bootriscv64.efi");
      make_image_fwdisk_abs (GRUB_INSTALL_PLATFORM_RISCV64_EFI, "riscv64-efi",
			     imgname);
      free (imgname);

      if (source_dirs[GRUB_INSTALL_PLATFORM_I386_EFI])
	{
	  imgname = grub_util_path_concat (2, efidir_efi_boot, "boot.efi");
	  /* For old macs. Suggested by Peter Jones.  */
	  grub_install_copy_file (img32, imgname, 1);
	}

      if (source_dirs[GRUB_INSTALL_PLATFORM_I386_EFI]
	  || source_dirs[GRUB_INSTALL_PLATFORM_X86_64_EFI])
	img_mac = grub_util_path_concat (2, core_services, "boot.efi");

      if (source_dirs[GRUB_INSTALL_PLATFORM_I386_EFI]
	  && source_dirs[GRUB_INSTALL_PLATFORM_X86_64_EFI])
	grub_util_glue_efi (img32, img64, img_mac);
      else if (source_dirs[GRUB_INSTALL_PLATFORM_X86_64_EFI])
	grub_install_copy_file (img64, img_mac, 1);
      else if (source_dirs[GRUB_INSTALL_PLATFORM_I386_EFI])
	grub_install_copy_file (img32, img_mac, 1);

      free (img_mac);
      free (img32);
      free (img64);
      free (efidir_efi_boot);

      efiimgfat = grub_util_path_concat (2, iso9660_dir, "efi.img");
      rv = grub_util_exec ((const char * []) { "mformat", "-C", "-f", "2880", "-L", "16", "-i",
	    efiimgfat, "::", NULL });
      if (rv != 0)
	grub_util_error ("`%s` invocation failed\n", "mformat");
      rv = grub_util_exec ((const char * []) { "mcopy", "-s", "-i", efiimgfat, efidir_efi, "::/", NULL });
      if (rv != 0)
	grub_util_error ("`%s` invocation failed\n", "mcopy");
      xorriso_push ("--efi-boot");
      xorriso_push ("efi.img");
      xorriso_push ("-efi-boot-part");
      xorriso_push ("--efi-boot-image");

      grub_util_unlink_recursive (efidir);
      free (efiimgfat);
      free (efidir_efi);
      free (efidir);
      grub_install_pop_module ();
      grub_install_pop_module ();
    }

  grub_install_push_module ("part_apple");
  make_image_fwdisk (GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275, "powerpc-ieee1275", "powerpc-ieee1275/core.elf");
  grub_install_pop_module ();

  if (source_dirs[GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275])
    {
      char *grub_chrp = grub_util_path_concat (2, source_dirs[GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275],
					       "grub.chrp");
      char *bisrc = grub_util_path_concat (2, source_dirs[GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275],
					   "bootinfo.txt");
      char *bootx = grub_util_path_concat (2, core_services, "BootX");
      char *ppc_chrp = grub_util_path_concat (3, iso9660_dir, "ppc", "chrp");
      char *bitgt = grub_util_path_concat (3, iso9660_dir, "ppc", "bootinfo.txt");
      grub_install_copy_file (grub_chrp, bootx, 1);
      grub_install_mkdir_p (ppc_chrp);
      grub_install_copy_file (bisrc, bitgt, 1);
      xorriso_link ("/System/Library/CoreServices/grub.elf", "/boot/grub/powerpc-ieee1275/core.elf");
      xorriso_link ("/boot/grub/powerpc.elf", "/boot/grub/powerpc-ieee1275/core.elf");
      /* FIXME: add PreP */
      if (system_area == SYS_AREA_COMMON)
	{
	  xorriso_push ("-hfsplus-file-creator-type");
	  xorriso_push ("chrp");
	  xorriso_push ("tbxi");
	  xorriso_push ("/System/Library/CoreServices/BootX");
	  xorriso_push ("-hfs-bless-by");
	  xorriso_push ("p");
	  xorriso_push ("/System/Library/CoreServices");
	}
      xorriso_push ("-sysid");
      xorriso_push ("PPC");
    }

  make_image_fwdisk (GRUB_INSTALL_PLATFORM_SPARC64_IEEE1275,
		     "sparc64-ieee1275-cdcore", "sparc64-ieee1275/core.img");

  if (source_dirs[GRUB_INSTALL_PLATFORM_SPARC64_IEEE1275]
      && system_area == SYS_AREA_SPARC)
    {
      char *cdboot;
      FILE *in, *out;
      char buf[512];
      sysarea_img = grub_util_make_temporary_file ();
      cdboot = grub_util_path_concat (2, source_dirs[GRUB_INSTALL_PLATFORM_SPARC64_IEEE1275],
				      "cdboot.img");
      in = grub_util_fopen (cdboot, "rb");
      if (!in)
	grub_util_error (_("cannot open `%s': %s"), cdboot,
			 strerror (errno));
      out = grub_util_fopen (sysarea_img, "wb");
      if (!out)
	grub_util_error (_("cannot open `%s': %s"), sysarea_img,
			 strerror (errno));
      memset (buf, 0, 512);
      fwrite (buf, 1, 512, out);
      if (fread (buf, 1, 512, in) != 512)
	grub_util_error (_("cannot read `%s': %s"), cdboot,
			 strerror (errno));
      fwrite (buf, 1, 512, out);
      fclose (in);
      fclose (out);
      xorriso_push ("-G");
      xorriso_push (sysarea_img);
      xorriso_push ("-B");
      xorriso_push (",");
      xorriso_push ("--grub2-sparc-core");
      xorriso_push ("/boot/grub/sparc64-ieee1275/core.img");
    }

  make_image_fwdisk (GRUB_INSTALL_PLATFORM_MIPS_ARC, "mips-arc", "mips-arc/core.img");

  if (source_dirs[GRUB_INSTALL_PLATFORM_MIPS_ARC])
    {
      xorriso_link ("/boot/grub/mips-arc/grub", "/boot/grub/mips-arc/core.img");
      xorriso_link ("/boot/grub/mips-arc/sashARCS", "/boot/grub/mips-arc/core.img");
      xorriso_link ("/boot/grub/mips-arc/sash", "/boot/grub/mips-arc/core.img");
    }
  if (source_dirs[GRUB_INSTALL_PLATFORM_MIPS_ARC] && system_area == SYS_AREA_ARCS)
    {
      xorriso_push ("-mips-boot");
      xorriso_push ("/boot/grub/mips-arc/sashARCS");
      xorriso_push ("-mips-boot");
      xorriso_push ("/boot/grub/mips-arc/sash");
      xorriso_push ("-mips-boot");
      xorriso_push ("/boot/grub/mips-arc/grub");
    }

  make_image_fwdisk (GRUB_INSTALL_PLATFORM_MIPSEL_ARC, "mipsel-arc", "arc.exe");

  grub_install_push_module ("pata");
  make_image (GRUB_INSTALL_PLATFORM_MIPSEL_QEMU_MIPS, "mipsel-qemu_mips-elf", "roms/mipsel-qemu_mips.elf");

  make_image (GRUB_INSTALL_PLATFORM_MIPSEL_LOONGSON, "mipsel-loongson-elf", "loongson.elf");

  make_image (GRUB_INSTALL_PLATFORM_MIPSEL_LOONGSON, "mipsel-yeeloong-flash", "mipsel-yeeloong.bin");
  make_image (GRUB_INSTALL_PLATFORM_MIPSEL_LOONGSON, "mipsel-fuloong2f-flash", "mipsel-fuloong2f.bin");

  make_image (GRUB_INSTALL_PLATFORM_MIPS_QEMU_MIPS, "mips-qemu_mips-elf", "roms/mips-qemu_mips.elf");

  grub_install_push_module ("at_keyboard");

  make_image (GRUB_INSTALL_PLATFORM_I386_QEMU, "i386-qemu", "roms/qemu.img");

  grub_install_push_module ("ahci");

  make_image (GRUB_INSTALL_PLATFORM_I386_COREBOOT, "i386-coreboot", "roms/coreboot.elf");
  grub_install_pop_module ();
  grub_install_pop_module ();
  grub_install_pop_module ();

  if (rom_directory)
    {
      const struct
      {
	enum grub_install_plat plat;
	const char *from, *to;
      } roms[] =
	  {
	    {GRUB_INSTALL_PLATFORM_MIPSEL_QEMU_MIPS, "roms/mipsel-qemu_mips.elf", "mipsel-qemu_mips.elf"},
	    {GRUB_INSTALL_PLATFORM_MIPSEL_LOONGSON, "loongson.elf", "mipsel-loongson.elf"},
	    {GRUB_INSTALL_PLATFORM_MIPSEL_LOONGSON, "roms/mipsel-yeeloong.bin", "mipsel-yeeloong.bin"},
	    {GRUB_INSTALL_PLATFORM_MIPSEL_LOONGSON, "roms/mipsel-fulong.bin", "mipsel-fulong.bin"},
	    {GRUB_INSTALL_PLATFORM_MIPS_QEMU_MIPS, "roms/mips-qemu_mips.elf", "mips-qemu_mips.elf"},
	    {GRUB_INSTALL_PLATFORM_I386_QEMU, "roms/qemu.img", "qemu.img"},
	    {GRUB_INSTALL_PLATFORM_I386_COREBOOT, "roms/coreboot.elf", "coreboot.elf"},
	  };
      grub_size_t i;
      for (i = 0; i < ARRAY_SIZE (roms); i++)
	{
	  char *from = grub_util_path_concat (2, boot_grub, roms[i].from);
	  char *to = grub_util_path_concat (2, rom_directory, roms[i].to);
	  grub_install_copy_file (from, to, 0);
	}
    }

  xorriso_push ("--protective-msdos-label");
  xorriso_push ("-o");
  xorriso_push (output_image);
  xorriso_push ("-r");
  xorriso_push (iso9660_dir);
  xorriso_push ("--sort-weight");
  xorriso_push ("0");
  xorriso_push ("/");
  xorriso_push ("--sort-weight");
  xorriso_push ("1");
  xorriso_push ("/boot");
  int i;
  for (i = 0; i < xorriso_tail_argc; i++)
    xorriso_push (xorriso_tail_argv[i]);

  xorriso_argv[xorriso_argc] = NULL;

  rv = grub_util_exec ((const char *const *)xorriso_argv);
  if (rv != 0)
    grub_util_error ("`%s` invocation failed\n", "xorriso");

  grub_util_unlink_recursive (iso9660_dir);

  if (sysarea_img)
    grub_util_unlink (sysarea_img);

  free (core_services);
  free (romdir);
  return 0;
}
