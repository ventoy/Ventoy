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

/* The initial context.  */
static struct grub_env_context initial_context;

/* The current context.  */
struct grub_env_context *grub_current_context = &initial_context;

static grub_env_read_hook_t vtoy_menu_lang_read_hook;

/* Return the hash representation of the string S.  */
static unsigned int
grub_env_hashval (const char *s)
{
  unsigned int i = 0;

  /* XXX: This can be done much more efficiently.  */
  while (*s)
    i += 5 * *(s++);

  return i % HASHSZ;
}

static struct grub_env_var *
grub_env_find (const char *name)
{
  struct grub_env_var *var;
  int idx = grub_env_hashval (name);

  /* Look for the variable in the current context.  */
  for (var = grub_current_context->vars[idx]; var; var = var->next)
    if (grub_strcmp (var->name, name) == 0)
      return var;

  return 0;
}

static void
grub_env_insert (struct grub_env_context *context,
		 struct grub_env_var *var)
{
  int idx = grub_env_hashval (var->name);

  /* Insert the variable into the hashtable.  */
  var->prevp = &context->vars[idx];
  var->next = context->vars[idx];
  if (var->next)
    var->next->prevp = &(var->next);
  context->vars[idx] = var;
}

static void
grub_env_remove (struct grub_env_var *var)
{
  /* Remove the entry from the variable table.  */
  *var->prevp = var->next;
  if (var->next)
    var->next->prevp = var->prevp;
}

grub_err_t
grub_env_set (const char *name, const char *val)
{
  struct grub_env_var *var;

  /* If the variable does already exist, just update the variable.  */
  var = grub_env_find (name);
  if (var)
    {
      char *old = var->value;

      if (var->write_hook)
	var->value = var->write_hook (var, val);
      else
	var->value = grub_strdup (val);

      if (! var->value)
	{
	  var->value = old;
	  return grub_errno;
	}

      grub_free (old);
      return GRUB_ERR_NONE;
    }

  /* The variable does not exist, so create a new one.  */
  var = grub_zalloc (sizeof (*var));
  if (! var)
    return grub_errno;

  var->name = grub_strdup (name);
  if (! var->name)
    goto fail;

  var->value = grub_strdup (val);
  if (! var->value)
    goto fail;

  grub_env_insert (grub_current_context, var);

  return GRUB_ERR_NONE;

 fail:
  grub_free (var->name);
  grub_free (var->value);
  grub_free (var);

  return grub_errno;
}

const char *
grub_env_get (const char *name)
{
  struct grub_env_var *var;

  if (name && vtoy_menu_lang_read_hook && grub_strncmp(name, "VTLANG_", 7) == 0)
    return vtoy_menu_lang_read_hook(NULL, name);

  var = grub_env_find (name);
  if (! var)
    return 0;

  if (var->read_hook)
    return var->read_hook (var, var->value);

  return var->value;
}

void
grub_env_unset (const char *name)
{
  struct grub_env_var *var;

  var = grub_env_find (name);
  if (! var)
    return;

  if (var->read_hook || var->write_hook)
    {
      grub_env_set (name, "");
      return;
    }

  grub_env_remove (var);

  grub_free (var->name);
  grub_free (var->value);
  grub_free (var);
}

struct grub_env_var *
grub_env_update_get_sorted (void)
{
  struct grub_env_var *sorted_list = 0;
  int i;

  /* Add variables associated with this context into a sorted list.  */
  for (i = 0; i < HASHSZ; i++)
    {
      struct grub_env_var *var;

      for (var = grub_current_context->vars[i]; var; var = var->next)
	{
	  struct grub_env_var *p, **q;

	  for (q = &sorted_list, p = *q; p; q = &((*q)->sorted_next), p = *q)
	    {
	      if (grub_strcmp (p->name, var->name) > 0)
		break;
	    }

	  var->sorted_next = *q;
	  *q = var;
	}
    }

  return sorted_list;
}

grub_err_t
grub_register_variable_hook (const char *name,
			     grub_env_read_hook_t read_hook,
			     grub_env_write_hook_t write_hook)
{
  struct grub_env_var *var = grub_env_find (name);

  if (! var)
    {
      if (grub_env_set (name, "") != GRUB_ERR_NONE)
	return grub_errno;

      var = grub_env_find (name);
      /* XXX Insert an assertion?  */
    }

  var->read_hook = read_hook;
  var->write_hook = write_hook;

  return GRUB_ERR_NONE;
}

grub_err_t
grub_register_vtoy_menu_lang_hook(grub_env_read_hook_t read_hook)
{
  vtoy_menu_lang_read_hook = read_hook;
  return GRUB_ERR_NONE;
}

grub_err_t
grub_env_export (const char *name)
{
  struct grub_env_var *var;

  var = grub_env_find (name);
  if (! var)
    {
      grub_err_t err;
      
      err = grub_env_set (name, "");
      if (err)
	return err;
      var = grub_env_find (name);
    }    
  var->global = 1;

  return GRUB_ERR_NONE;
}
