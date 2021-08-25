/* env.c - Environment variables */
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

#include <grub/env.h>
#include <grub/env_private.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/command.h>
#include <grub/normal.h>
#include <grub/i18n.h>

struct menu_pointer
{
  grub_menu_t menu;
  struct menu_pointer *prev;
};

static struct menu_pointer initial_menu;
static struct menu_pointer *current_menu = &initial_menu;

void
grub_env_unset_menu (void)
{
  current_menu->menu = NULL;
}

grub_menu_t
grub_env_get_menu (void)
{
  return current_menu->menu;
}

void
grub_env_set_menu (grub_menu_t nmenu)
{
  current_menu->menu = nmenu;
}

static grub_err_t
grub_env_new_context (int export_all)
{
  struct grub_env_context *context;
  int i;
  struct menu_pointer *menu;

  context = grub_zalloc (sizeof (*context));
  if (! context)
    return grub_errno;
  menu = grub_zalloc (sizeof (*menu));
  if (! menu)
    {
      grub_free (context);
      return grub_errno;
    }

  context->prev = grub_current_context;
  grub_current_context = context;

  menu->prev = current_menu;
  current_menu = menu;

  /* Copy exported variables.  */
  for (i = 0; i < HASHSZ; i++)
    {
      struct grub_env_var *var;

      for (var = context->prev->vars[i]; var; var = var->next)
	if (var->global || export_all)
	  {
	    if (grub_env_set (var->name, var->value) != GRUB_ERR_NONE)
	      {
		grub_env_context_close ();
		return grub_errno;
	      }
	    grub_env_export (var->name);
	    grub_register_variable_hook (var->name, var->read_hook, var->write_hook);
	  }
    }

  return GRUB_ERR_NONE;
}

grub_err_t
grub_env_context_open (void)
{
  return grub_env_new_context (grub_env_get("ventoy_new_context") ? 0 : 1);
}

int grub_extractor_level = 0;

grub_err_t
grub_env_extractor_open (int source)
{
  grub_extractor_level++;
  return grub_env_new_context (source);
}

grub_err_t
grub_env_context_close (void)
{
  struct grub_env_context *context;
  int i;
  struct menu_pointer *menu;

  if (! grub_current_context->prev)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
		       "cannot close the initial context");

  /* Free the variables associated with this context.  */
  for (i = 0; i < HASHSZ; i++)
    {
      struct grub_env_var *p, *q;

      for (p = grub_current_context->vars[i]; p; p = q)
	{
	  q = p->next;
          grub_free (p->name);
	  grub_free (p->value);
	  grub_free (p);
	}
    }

  /* Restore the previous context.  */
  context = grub_current_context->prev;
  grub_free (grub_current_context);
  grub_current_context = context;

  menu = current_menu->prev;
  if (current_menu->menu)
    grub_normal_free_menu (current_menu->menu);
  grub_free (current_menu);
  current_menu = menu;

  return GRUB_ERR_NONE;
}

grub_err_t
grub_env_extractor_close (int source)
{
  grub_menu_t menu = NULL;
  grub_menu_entry_t *last;
  grub_err_t err;

  if (source)
    {
      menu = grub_env_get_menu ();
      grub_env_unset_menu ();
    }
  err = grub_env_context_close ();

  if (source && menu)
    {
      grub_menu_t menu2;
      menu2 = grub_env_get_menu ();
      
      last = &menu2->entry_list;
      while (*last)
	last = &(*last)->next;
      
      *last = menu->entry_list;
      menu2->size += menu->size;
    }

  grub_extractor_level--;
  return err;
}

static grub_command_t export_cmd;

static grub_err_t
grub_cmd_export (struct grub_command *cmd __attribute__ ((unused)),
		 int argc, char **args)
{
  int i;

  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
		       N_("one argument expected"));

  for (i = 0; i < argc; i++)
    grub_env_export (args[i]);

  return 0;
}

void
grub_context_init (void)
{
  export_cmd = grub_register_command ("export", grub_cmd_export,
				      N_("ENVVAR [ENVVAR] ..."),
				      N_("Export variables."));
}

void
grub_context_fini (void)
{
  grub_unregister_command (export_cmd);
}
