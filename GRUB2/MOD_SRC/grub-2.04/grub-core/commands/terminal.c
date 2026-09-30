/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009,2010  Free Software Foundation, Inc.
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

#include <grub/mm.h>
#include <grub/dl.h>
#include <grub/command.h>
#include <grub/term.h>
#include <grub/i18n.h>
#include <grub/misc.h>

GRUB_MOD_LICENSE ("GPLv3+");

static grub_uint8_t g_terminal_lock = 0;

struct grub_term_autoload *grub_term_input_autoload = NULL;
struct grub_term_autoload *grub_term_output_autoload = NULL;

struct abstract_terminal
{
  struct abstract_terminal *next;
  struct abstract_terminal *prev;
  const char *name;
  grub_err_t (*init) (struct abstract_terminal *term);
  grub_err_t (*fini) (struct abstract_terminal *term);
};

static grub_err_t
handle_command (int argc, char **args, struct abstract_terminal **enabled,
               struct abstract_terminal **disabled,
               struct grub_term_autoload *autoloads,
               const char *active_str,
               const char *available_str)
{
  int i;
  struct abstract_terminal *term;
  struct grub_term_autoload *aut;

  if (argc == 0)
    {
      grub_puts_ (active_str);
      for (term = *enabled; term; term = term->next)
       grub_printf ("%s ", term->name);
      grub_printf ("\n");
      grub_puts_ (available_str);
      for (term = *disabled; term; term = term->next)
       grub_printf ("%s ", term->name);
      /* This is quadratic but we don't expect mode than 30 terminal
        modules ever.  */
      for (aut = autoloads; aut; aut = aut->next)
       {
         for (term = *disabled; term; term = term->next)
           if (grub_strcmp (term->name, aut->name) == 0
	       || (aut->name[0] && aut->name[grub_strlen (aut->name) - 1] == '*'
		   && grub_memcmp (term->name, aut->name,
				   grub_strlen (aut->name) - 1) == 0))
             break;
         if (!term)
           for (term = *enabled; term; term = term->next)
             if (grub_strcmp (term->name, aut->name) == 0
		 || (aut->name[0] && aut->name[grub_strlen (aut->name) - 1] == '*'
		     && grub_memcmp (term->name, aut->name,
				     grub_strlen (aut->name) - 1) == 0))
               break;
         if (!term)
           grub_printf ("%s ", aut->name);
       }
      grub_printf ("\n");
      return GRUB_ERR_NONE;
    }
  i = 0;

  if (grub_strcmp (args[0], "--append") == 0
      || grub_strcmp (args[0], "--remove") == 0)
    i++;

  if (i == argc)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_ ("no terminal specified"));

  for (; i < argc; i++)
    {
      int again = 0;
      while (1)
       {
         for (term = *disabled; term; term = term->next)
           if (grub_strcmp (args[i], term->name) == 0
	       || (grub_strcmp (args[i], "ofconsole") == 0
		   && grub_strcmp ("console", term->name) == 0))
             break;
         if (term == 0)
           for (term = *enabled; term; term = term->next)
             if (grub_strcmp (args[i], term->name) == 0
		 || (grub_strcmp (args[i], "ofconsole") == 0
		     && grub_strcmp ("console", term->name) == 0))
               break;
         if (term)
           break;
         if (again)
	   return grub_error (GRUB_ERR_BAD_ARGUMENT,
			      N_("terminal `%s' isn't found"),
			      args[i]);
         for (aut = autoloads; aut; aut = aut->next)
           if (grub_strcmp (args[i], aut->name) == 0
	       || (grub_strcmp (args[i], "ofconsole") == 0
		   && grub_strcmp ("console", aut->name) == 0)
	       || (aut->name[0] && aut->name[grub_strlen (aut->name) - 1] == '*'
		   && grub_memcmp (args[i], aut->name,
				   grub_strlen (aut->name) - 1) == 0))
             {
               grub_dl_t mod;
               mod = grub_dl_load (aut->modname);
               if (mod)
                 grub_dl_ref (mod);
               grub_errno = GRUB_ERR_NONE;
               break;
             }
	 if (grub_memcmp (args[i], "serial_usb",
				  sizeof ("serial_usb") - 1) == 0
	     && grub_term_poll_usb)
	   {
	     grub_term_poll_usb (1);
	     again = 1;
	     continue;
	   }
         if (!aut)
           return grub_error (GRUB_ERR_BAD_ARGUMENT,
			      N_("terminal `%s' isn't found"),
                              args[i]);
         again = 1;
       }
    }

  if (grub_strcmp (args[0], "--append") == 0)
    {
      for (i = 1; i < argc; i++)
       {
         for (term = *disabled; term; term = term->next)
           if (grub_strcmp (args[i], term->name) == 0
	       || (grub_strcmp (args[i], "ofconsole") == 0
		   && grub_strcmp ("console", term->name) == 0))
             break;
         if (term)
           {
              if (term->init && term->init (term) != GRUB_ERR_NONE)
                return grub_errno;

             grub_list_remove (GRUB_AS_LIST (term));
             grub_list_push (GRUB_AS_LIST_P (enabled), GRUB_AS_LIST (term));
           }
       }
      return GRUB_ERR_NONE;
    }

  if (grub_strcmp (args[0], "--remove") == 0)
    {
      for (i = 1; i < argc; i++)
       {
         for (term = *enabled; term; term = term->next)
           if (grub_strcmp (args[i], term->name) == 0
	       || (grub_strcmp (args[i], "ofconsole") == 0
		   && grub_strcmp ("console", term->name) == 0))
             break;
         if (term)
           {
             if (!term->next && term == *enabled)
               return grub_error (GRUB_ERR_BAD_ARGUMENT,
                                  "can't remove the last terminal");
             grub_list_remove (GRUB_AS_LIST (term));
             if (term->fini)
               term->fini (term);
             grub_list_push (GRUB_AS_LIST_P (disabled), GRUB_AS_LIST (term));
           }
       }
      return GRUB_ERR_NONE;
    }
  for (i = 0; i < argc; i++)
    {
      for (term = *disabled; term; term = term->next)
       if (grub_strcmp (args[i], term->name) == 0
	   || (grub_strcmp (args[i], "ofconsole") == 0
	       && grub_strcmp ("console", term->name) == 0))
         break;
      if (term)
       {
         if (term->init && term->init (term) != GRUB_ERR_NONE)
           return grub_errno;

         grub_list_remove (GRUB_AS_LIST (term));
         grub_list_push (GRUB_AS_LIST_P (enabled), GRUB_AS_LIST (term));
       }
    }

  {
    struct abstract_terminal *next;
    for (term = *enabled; term; term = next)
      {
       next = term->next;
       for (i = 0; i < argc; i++)
         if (grub_strcmp (args[i], term->name) == 0
	     || (grub_strcmp (args[i], "ofconsole") == 0
		 && grub_strcmp ("console", term->name) == 0))
           break;
       if (i == argc)
         {
           if (!term->next && term == *enabled)
             return grub_error (GRUB_ERR_BAD_ARGUMENT,
                                "can't remove the last terminal");
           grub_list_remove (GRUB_AS_LIST (term));
           if (term->fini)
             term->fini (term);
           grub_list_push (GRUB_AS_LIST_P (disabled), GRUB_AS_LIST (term));
         }
      }
  }

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_terminal_lock (grub_command_t cmd __attribute__ ((unused)),
			 int argc, char **args)
{
    (void)argc;
    (void)args;

    if (args[0] && args[0][0] == '1')
        g_terminal_lock = 1;
    else
        g_terminal_lock = 0;

    return 0;
}

static grub_err_t
grub_cmd_terminal_input (grub_command_t cmd __attribute__ ((unused)),
			 int argc, char **args)
{
  (void) GRUB_FIELD_MATCH (grub_term_inputs, struct abstract_terminal *, next);
  (void) GRUB_FIELD_MATCH (grub_term_inputs, struct abstract_terminal *, prev);
  (void) GRUB_FIELD_MATCH (grub_term_inputs, struct abstract_terminal *, name);
  (void) GRUB_FIELD_MATCH (grub_term_inputs, struct abstract_terminal *, init);
  (void) GRUB_FIELD_MATCH (grub_term_inputs, struct abstract_terminal *, fini);

  if (g_terminal_lock) return 0;

  return handle_command (argc, args,
			 (struct abstract_terminal **) (void *) &grub_term_inputs,
			 (struct abstract_terminal **) (void *) &grub_term_inputs_disabled,
			 grub_term_input_autoload,
			 N_ ("Active input terminals:"),
			 N_ ("Available input terminals:"));
}

static grub_err_t
grub_cmd_terminal_output (grub_command_t cmd __attribute__ ((unused)),
                         int argc, char **args)
{
  (void) GRUB_FIELD_MATCH (grub_term_outputs, struct abstract_terminal *, next);
  (void) GRUB_FIELD_MATCH (grub_term_outputs, struct abstract_terminal *, prev);
  (void) GRUB_FIELD_MATCH (grub_term_outputs, struct abstract_terminal *, name);
  (void) GRUB_FIELD_MATCH (grub_term_outputs, struct abstract_terminal *, init);
  (void) GRUB_FIELD_MATCH (grub_term_outputs, struct abstract_terminal *, fini);

  if (g_terminal_lock) return 0;

  return handle_command (argc, args,
			 (struct abstract_terminal **) (void *) &grub_term_outputs,
			 (struct abstract_terminal **) (void *) &grub_term_outputs_disabled,
			 grub_term_output_autoload,
			 N_ ("Active output terminals:"),
			 N_ ("Available output terminals:"));
}

static grub_command_t cmd_terminal_input, cmd_terminal_output, cmd_lock_terminal;

GRUB_MOD_INIT(terminal)
{
  cmd_lock_terminal =
    grub_register_command ("terminal_lock", grub_cmd_terminal_lock,
			   0, N_("lock terminal."));

  cmd_terminal_input =
    grub_register_command ("terminal_input", grub_cmd_terminal_input,
			   N_("[--append|--remove] "
			      "[TERMINAL1] [TERMINAL2] ..."),
			   N_("List or select an input terminal."));
  cmd_terminal_output =
    grub_register_command ("terminal_output", grub_cmd_terminal_output,
			   N_("[--append|--remove] "
			      "[TERMINAL1] [TERMINAL2] ..."),
			   N_("List or select an output terminal."));
}

GRUB_MOD_FINI(terminal)
{
  grub_unregister_command (cmd_lock_terminal);
  grub_unregister_command (cmd_terminal_input);
  grub_unregister_command (cmd_terminal_output);
}
