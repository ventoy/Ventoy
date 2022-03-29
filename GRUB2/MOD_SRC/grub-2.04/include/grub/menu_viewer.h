/* menu_viewer.h - Interface to menu viewer implementations. */
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

#ifndef GRUB_MENU_VIEWER_HEADER
#define GRUB_MENU_VIEWER_HEADER 1

#include <grub/err.h>
#include <grub/symbol.h>
#include <grub/types.h>
#include <grub/menu.h>
#include <grub/term.h>

struct grub_menu_viewer
{
  struct grub_menu_viewer *next;
  void *data;
  void (*set_chosen_entry) (int entry, void *data);
  void (*scroll_chosen_entry) (void *data, int diren);
  void (*print_timeout) (int timeout, void *data);
  void (*clear_timeout) (void *data);
  void (*fini) (void *fini);
};

void grub_menu_register_viewer (struct grub_menu_viewer *viewer);

grub_err_t
grub_menu_try_text (struct grub_term_output *term, 
		    int entry, grub_menu_t menu, int nested);

extern grub_err_t (*grub_gfxmenu_try_hook) (int entry, grub_menu_t menu,
					    int nested);

#endif /* GRUB_MENU_VIEWER_HEADER */
