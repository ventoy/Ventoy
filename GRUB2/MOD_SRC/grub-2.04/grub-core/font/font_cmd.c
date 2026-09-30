/* font_cmd.c - Font command definition. */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
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

#include <grub/font.h>
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/command.h>
#include <grub/i18n.h>

static grub_uint8_t g_font_lock = 0;

static grub_err_t
loadfont_command (grub_command_t cmd __attribute__ ((unused)),
		  int argc,
		  char **args)
{
  if (argc == 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  if (g_font_lock) return 0;

  while (argc--)
    if (grub_font_load (*args++) == 0)
      {
	if (!grub_errno)
	  return grub_error (GRUB_ERR_BAD_FONT, "invalid font");
	return grub_errno;
      }

  return GRUB_ERR_NONE;
}

static grub_err_t
lsfonts_command (grub_command_t cmd __attribute__ ((unused)),
                 int argc __attribute__ ((unused)),
                 char **args __attribute__ ((unused)))
{
  struct grub_font_node *node;

  grub_puts_ (N_("Loaded fonts:"));
  for (node = grub_font_list; node; node = node->next)
    {
      grub_font_t font = node->value;
      grub_printf ("%s\n", grub_font_get_name (font));
    }

  return GRUB_ERR_NONE;
}

static grub_err_t
lockfont_command (grub_command_t cmd __attribute__ ((unused)),
                 int argc __attribute__ ((unused)),
                 char **args __attribute__ ((unused)))
{
  if (args[0] && args[0][0] == '1')
  {
    g_font_lock = 1;
  }
  else
  {
    g_font_lock = 0;
  }

  return GRUB_ERR_NONE;
}

static grub_command_t cmd_loadfont, cmd_lsfonts, cmd_lockfont;

#if defined (GRUB_MACHINE_MIPS_LOONGSON) || defined (GRUB_MACHINE_COREBOOT)
void grub_font_init (void)
#else
GRUB_MOD_INIT(font)
#endif
{
  grub_font_loader_init ();

  cmd_loadfont =
    grub_register_command ("loadfont", loadfont_command,
			   N_("FILE..."),
			   N_("Specify one or more font files to load."));
  cmd_lsfonts =
    grub_register_command ("lsfonts", lsfonts_command,
			   0, N_("List the loaded fonts."));
  cmd_lockfont =
    grub_register_command ("lockfont", lockfont_command,
			   0, N_("Lock fonts."));
}

#if defined (GRUB_MACHINE_MIPS_LOONGSON) || defined (GRUB_MACHINE_COREBOOT)
void grub_font_fini (void)
#else
GRUB_MOD_FINI(font)
#endif
{
  /* TODO: Determine way to free allocated resources.
     Warning: possible pointer references could be in use.  */

  grub_unregister_command (cmd_loadfont);
  grub_unregister_command (cmd_lsfonts);
  grub_unregister_command (cmd_lockfont);
}
