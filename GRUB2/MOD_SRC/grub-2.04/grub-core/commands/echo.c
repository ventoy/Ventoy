/* echo.c - Command to display a line of text  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007,2010  Free Software Foundation, Inc.
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
#include <grub/misc.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/term.h>

GRUB_MOD_LICENSE ("GPLv3+");

extern const char *ventoy_get_vmenu_title(const char *vMenu);

static const struct grub_arg_option options[] =
  {
    {0, 'n', 0, N_("Do not output the trailing newline."), 0, 0},
    {0, 'e', 0, N_("Enable interpretation of backslash escapes."), 0, 0},
    {0, 'v', 0, N_("ventoy menu language."), 0, 0},
    {0, 'V', 0, N_("ventoy menu language with pre-newline."), 0, 0},
    {0, 0, 0, 0, 0, 0}
  };

static grub_err_t
grub_cmd_echo (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  char ch;
  int vtmenu = 0;
  int newline = 1;
  int i;

  /* Check if `-n' was used.  */
  if (state[0].set)
    newline = 0;

  if (state[2].set || state[3].set)
    vtmenu = 1;

  for (i = 0; i < argc; i++)
    {
      char *arg = *args;
      /* Unescaping results in a string no longer than the original.  */
      char *unescaped = grub_malloc (grub_strlen (arg) + 1);
      char *p = unescaped;
      args++;

      if (!unescaped)
	return grub_errno;

      while (*arg)
	{
	  /* In case `-e' is used, parse backslashes.  */
	  if (*arg == '\\' && state[1].set)
	    {
	      arg++;
	      if (*arg == '\0')
		break;

	      switch (*arg)
		{
		case '\\':
		  *p++ = '\\';
		  break;

		case 'a':
		  *p++ = '\a';
		  break;

		case 'c':
		  newline = 0;
		  break;

		case 'f':
		  *p++ = '\f';
		  break;

		case 'n':
		  *p++ = '\n';
		  break;

		case 'r':
		  *p++ = '\r';
		  break;

		case 't':
		  *p++ = '\t';
		  break;

		case 'v':
		  *p++ = '\v';
		  break;
		}
	      arg++;
	      continue;
	    }

	  /* This was not an escaped character, or escaping is not
	     enabled.  */
	  *p++ = *arg;
	  arg++;
	}

      *p = '\0';

    if (vtmenu && grub_strncmp(unescaped, "VTMENU_", 7) == 0) 
    {
        p = unescaped;
        while ((*p >= 'A' && *p <= 'Z') || *p == '_')
        {
            p++;
        }

        ch = *p;
        *p = 0;
        if (state[3].set)
        {
            grub_xputs("\n");            
        }
        grub_xputs(ventoy_get_vmenu_title(unescaped));

        *p = ch;
        grub_xputs(p);
    }
    else    
    {
        grub_xputs (unescaped);
    }
    
    grub_free (unescaped);

    /* If another argument follows, insert a space.  */
    if ((0 == vtmenu) && (i != argc - 1))
	    grub_printf (" " );
    }

  if (newline)
    grub_printf ("\n");

  grub_refresh ();  

  return 0;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(echo)
{
  cmd = grub_register_extcmd ("echo", grub_cmd_echo,
			      GRUB_COMMAND_ACCEPT_DASH
			      | GRUB_COMMAND_OPTIONS_AT_START,
			      N_("[-e|-n] STRING"), N_("Display a line of text."),
			      options);
}

GRUB_MOD_FINI(echo)
{
  grub_unregister_extcmd (cmd);
}
