/* menu.h - Menu model function prototypes and data structures. */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009  Free Software Foundation, Inc.
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

#ifndef GRUB_MENU_HEADER
#define GRUB_MENU_HEADER 1

struct bls_entry
{
  struct bls_entry *next;
  struct bls_entry *prev;
  struct keyval **keyvals;
  int nkeyvals;
  char *filename;
  int visible;
};

struct grub_menu_entry_class
{
  char *name;
  struct grub_menu_entry_class *next;
};

/* The menu entry.  */
struct grub_menu_entry
{
  /* The title name.  */
  const char *title;

  /* The identifier.  */
  const char *id;

  /* If set means not everybody is allowed to boot this entry.  */
  int restricted;

  /* Allowed users.  */
  const char *users;

  /* The classes associated with the menu entry:
     used to choose an icon or other style attributes.
     This is a dummy head node for the linked list, so for an entry E,
     E.classes->next is the first class if it is not NULL.  */
  struct grub_menu_entry_class *classes;

  /* The sourcecode of the menu entry, used by the editor.  */
  const char *sourcecode;

  /* Parameters to be passed to menu definition.  */
  int argc;
  char **args;

  int hotkey;

  int submenu;

  /* The next element.  */
  struct grub_menu_entry *next;

  /* BLS used to populate the entry */
  struct bls_entry *bls;
};
typedef struct grub_menu_entry *grub_menu_entry_t;

/* The menu.  */
struct grub_menu
{
  /* The size of a menu.  */
  int size;

  /* The list of menu entries.  */
  grub_menu_entry_t entry_list;
};
typedef struct grub_menu *grub_menu_t;

/* Callback structure menu viewers can use to provide user feedback when
   default entries are executed, possibly including fallback entries.  */
typedef struct grub_menu_execute_callback
{
  /* Called immediately before ENTRY is booted.  */
  void (*notify_booting) (grub_menu_entry_t entry, void *userdata);

  /* Called when executing one entry has failed, and another entry, ENTRY, will
     be executed as a fallback.  The implementation of this function should
     delay for a period of at least 2 seconds before returning in order to
     allow the user time to read the information before it can be lost by
     executing ENTRY.  */
  void (*notify_fallback) (grub_menu_entry_t entry, void *userdata);

  /* Called when an entry has failed to execute and there is no remaining
     fallback entry to attempt.  */
  void (*notify_failure) (void *userdata);
}
*grub_menu_execute_callback_t;

grub_menu_entry_t grub_menu_get_entry (grub_menu_t menu, int no);
int grub_menu_get_timeout (void);
void grub_menu_set_timeout (int timeout);
void grub_menu_entry_run (grub_menu_entry_t entry);
int grub_menu_get_default_entry_index (grub_menu_t menu);

void grub_menu_init (void);
void grub_menu_fini (void);

#endif /* GRUB_MENU_HEADER */
