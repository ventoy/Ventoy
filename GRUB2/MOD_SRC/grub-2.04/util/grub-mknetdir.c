/*
  * Copyright (C) 1999,2000,2001,2002,2003,2004,2005,2006,2007,2008,2009,2010,2013  Free Software Foundation, Inc.
  *
  * GRUB is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  *
  * GRUB is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <config.h>

#include <grub/util/install.h>
#include <grub/emu/config.h>
#include <grub/util/misc.h>

#include <string.h>
#include <errno.h>

#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#include <argp.h>
#pragma GCC diagnostic error "-Wmissing-prototypes"
#pragma GCC diagnostic error "-Wmissing-declarations"

static char *rootdir = NULL, *subdir = NULL;
static char *debug_image = NULL;

enum
  {
    OPTION_NET_DIRECTORY = 0x301,
    OPTION_SUBDIR,
    OPTION_DEBUG,
    OPTION_DEBUG_IMAGE
  };

static struct argp_option options[] = {
  GRUB_INSTALL_OPTIONS,
  {"net-directory", OPTION_NET_DIRECTORY, N_("DIR"),
   0, N_("root directory of TFTP server"), 2},
  {"subdir", OPTION_SUBDIR, N_("DIR"),
   0, N_("relative subdirectory on network server"), 2},
  {"debug", OPTION_DEBUG, 0, OPTION_HIDDEN, 0, 2},
  {"debug-image", OPTION_DEBUG_IMAGE, N_("STRING"), OPTION_HIDDEN, 0, 2},
  {0, 0, 0, 0, 0, 0}
};

static error_t 
argp_parser (int key, char *arg, struct argp_state *state)
{
  if (grub_install_parse (key, arg))
    return 0;
  switch (key)
    {
    case OPTION_NET_DIRECTORY:
      free (rootdir);
      rootdir = xstrdup (arg);
      return 0;
    case OPTION_SUBDIR:
      free (subdir);
      subdir = xstrdup (arg);
      return 0;
      /* This is an undocumented feature...  */
    case OPTION_DEBUG:
      verbosity++;
      return 0;
    case OPTION_DEBUG_IMAGE:
      free (debug_image);
      debug_image = xstrdup (arg);
      return 0;

    case ARGP_KEY_ARG:
    default:
      return ARGP_ERR_UNKNOWN;
    }
}


struct argp argp = {
  options, argp_parser, NULL,
  "\v"N_("Prepares GRUB network boot images at net_directory/subdir "
	 "assuming net_directory being TFTP root."), 
  NULL, grub_install_help_filter, NULL
};

static char *base;

static const struct
{
  const char *mkimage_target;
  const char *netmodule;
  const char *ext;
} targets[GRUB_INSTALL_PLATFORM_MAX] =
  {
    [GRUB_INSTALL_PLATFORM_I386_PC] = { "i386-pc-pxe", "pxe", ".0" },
    [GRUB_INSTALL_PLATFORM_SPARC64_IEEE1275] = { "sparc64-ieee1275-aout", "ofnet", ".img" },
    [GRUB_INSTALL_PLATFORM_I386_IEEE1275] = { "i386-ieee1275", "ofnet", ".elf" },
    [GRUB_INSTALL_PLATFORM_POWERPC_IEEE1275] = { "powerpc-ieee1275", "ofnet", ".elf" },
    [GRUB_INSTALL_PLATFORM_I386_EFI] = { "i386-efi", "efinet", ".efi" },
    [GRUB_INSTALL_PLATFORM_X86_64_EFI] = { "x86_64-efi", "efinet", ".efi" },
    [GRUB_INSTALL_PLATFORM_IA64_EFI] = { "ia64-efi", "efinet", ".efi" },
    [GRUB_INSTALL_PLATFORM_ARM_EFI] = { "arm-efi", "efinet", ".efi" },
    [GRUB_INSTALL_PLATFORM_ARM64_EFI] = { "arm64-efi", "efinet", ".efi" },
	[GRUB_INSTALL_PLATFORM_MIPS64EL_EFI] = { "mips64el-efi", "efinet", ".efi" },
    [GRUB_INSTALL_PLATFORM_RISCV32_EFI] = { "riscv32-efi", "efinet", ".efi" },
    [GRUB_INSTALL_PLATFORM_RISCV64_EFI] = { "riscv64-efi", "efinet", ".efi" },
  };

static void
process_input_dir (const char *input_dir, enum grub_install_plat platform)
{
  char *platsub = grub_install_get_platform_name (platform);
  char *grubdir = grub_util_path_concat (3, rootdir, subdir, platsub);
  char *load_cfg = grub_util_path_concat (2, grubdir, "load.cfg");
  char *prefix;
  char *output;
  char *grub_cfg;
  FILE *cfg;

  grub_install_copy_files (input_dir, base, platform);
  grub_util_unlink (load_cfg);

  if (debug_image)
    {
      FILE *f = grub_util_fopen (load_cfg, "wb");
      if (!f)
	grub_util_error (_("cannot open `%s': %s"), load_cfg,
			 strerror (errno));
      fprintf (f, "set debug='%s'\n", debug_image);
      fclose (f);
    }
  else
    {
      free (load_cfg);
      load_cfg = 0;
    }

  prefix = xasprintf ("/%s", subdir);
  if (!targets[platform].mkimage_target)
    grub_util_error (_("unsupported platform %s"), platsub);

  grub_cfg = grub_util_path_concat (2, grubdir, "grub.cfg");
  cfg = grub_util_fopen (grub_cfg, "wb");
  if (!cfg)
    grub_util_error (_("cannot open `%s': %s"), grub_cfg,
		     strerror (errno));
  fprintf (cfg, "source %s/grub.cfg", subdir);
  fclose (cfg);

  grub_install_push_module (targets[platform].netmodule);

  output = grub_util_path_concat_ext (2, grubdir, "core", targets[platform].ext);
  grub_install_make_image_wrap (input_dir, prefix, output,
				0, load_cfg,
				targets[platform].mkimage_target, 0);
  grub_install_pop_module ();

  /* TRANSLATORS: First %s is replaced by platform name. Second one by filename.  */
  printf (_("Netboot directory for %s created. Configure your DHCP server to point to %s\n"),
	  platsub, output);

  free (platsub);
  free (output);
  free (prefix);
  free (grub_cfg);
  free (grubdir);
}


int
main (int argc, char *argv[])
{
  const char *pkglibdir;

  grub_util_host_init (&argc, &argv);
  grub_util_disable_fd_syncs ();
  rootdir = xstrdup ("/srv/tftp");
  pkglibdir = grub_util_get_pkglibdir ();

  subdir = grub_util_path_concat (2, GRUB_BOOT_DIR_NAME, GRUB_DIR_NAME);

  argp_parse (&argp, argc, argv, 0, 0, 0);

  base = grub_util_path_concat (2, rootdir, subdir);
  /* Create the GRUB directory if it is not present.  */

  grub_install_mkdir_p (base);

  grub_install_push_module ("tftp");

  if (!grub_install_source_directory)
    {
      enum grub_install_plat plat;

      for (plat = 0; plat < GRUB_INSTALL_PLATFORM_MAX; plat++)
	if (targets[plat].mkimage_target)
	  {
	    char *platdir = grub_util_path_concat (2, pkglibdir,
						   grub_install_get_platform_name (plat));

	    grub_util_info ("Looking for `%s'", platdir);

	    if (!grub_util_is_directory (platdir))
	      {
		free (platdir);
		continue;
	      }
	    process_input_dir (platdir, plat);
	  }
    }
  else
    {
      enum grub_install_plat plat;
      plat = grub_install_get_target (grub_install_source_directory);
      process_input_dir (grub_install_source_directory, plat);
    }
  return 0;
}
