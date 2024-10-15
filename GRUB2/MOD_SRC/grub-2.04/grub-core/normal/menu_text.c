/* menu_text.c - Basic text menu implementation.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2004,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
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

#include <grub/normal.h>
#include <grub/term.h>
#include <grub/misc.h>
#include <grub/loader.h>
#include <grub/mm.h>
#include <grub/time.h>
#include <grub/env.h>
#include <grub/menu_viewer.h>
#include <grub/i18n.h>
#include <grub/charset.h>

static grub_uint8_t grub_color_menu_normal;
static grub_uint8_t grub_color_menu_highlight;

extern char g_ventoy_hotkey_tip[256];

struct menu_viewer_data
{
  int first, offset;
  struct grub_term_screen_geometry geo;
  enum { 
    TIMEOUT_UNKNOWN, 
    TIMEOUT_NORMAL,
    TIMEOUT_TERSE,
    TIMEOUT_TERSE_NO_MARGIN
  } timeout_msg;
  grub_menu_t menu;
  int *menu_title_offset;
  struct grub_term_output *term;
};

static inline int
grub_term_cursor_x (const struct grub_term_screen_geometry *geo)
{
  return (geo->first_entry_x + geo->entry_width);
}

grub_size_t
grub_getstringwidth (grub_uint32_t * str, const grub_uint32_t * last_position,
		     struct grub_term_output *term)
{
  grub_ssize_t width = 0;

  while (str < last_position)
    {
      struct grub_unicode_glyph glyph;
      glyph.ncomb = 0;
      str += grub_unicode_aglomerate_comb (str, last_position - str, &glyph);
      width += grub_term_getcharwidth (term, &glyph);
      grub_unicode_destroy_glyph (&glyph);
    }
  return width;
}

static int
grub_print_message_indented_real (const char *msg, int margin_left,
				  int margin_right,
				  struct grub_term_output *term, int dry_run)
{
  grub_uint32_t *unicode_msg;
  grub_uint32_t *last_position;
  grub_size_t msg_len = grub_strlen (msg) + 2;
  int ret = 0;

  unicode_msg = grub_malloc (msg_len * sizeof (grub_uint32_t));
 
  if (!unicode_msg)
    return 0;

  msg_len = grub_utf8_to_ucs4 (unicode_msg, msg_len,
			       (grub_uint8_t *) msg, -1, 0);
  
  last_position = unicode_msg + msg_len;
  *last_position = 0;

  if (dry_run)
    ret = grub_ucs4_count_lines (unicode_msg, last_position, margin_left,
				 margin_right, term);
  else
    grub_print_ucs4_menu (unicode_msg, last_position, margin_left,
			  margin_right, term, 0, -1, 0, 0);

  grub_free (unicode_msg);

  return ret;
}

void
grub_print_message_indented (const char *msg, int margin_left, int margin_right,
			     struct grub_term_output *term)
{
  grub_print_message_indented_real (msg, margin_left, margin_right, term, 0);
}

static void
draw_border (struct grub_term_output *term, const struct grub_term_screen_geometry *geo)
{
  int i;

  grub_term_setcolorstate (term, GRUB_TERM_COLOR_NORMAL);

  grub_term_gotoxy (term, (struct grub_term_coordinate) { geo->first_entry_x - 1,
	geo->first_entry_y - 1 });
  grub_putcode (GRUB_UNICODE_CORNER_UL, term);
  for (i = 0; i < geo->entry_width + 1; i++)
    grub_putcode (GRUB_UNICODE_HLINE, term);
  grub_putcode (GRUB_UNICODE_CORNER_UR, term);

  for (i = 0; i < geo->num_entries; i++)
    {
      grub_term_gotoxy (term, (struct grub_term_coordinate) { geo->first_entry_x - 1,
	    geo->first_entry_y + i });
      grub_putcode (GRUB_UNICODE_VLINE, term);
      grub_term_gotoxy (term,
			(struct grub_term_coordinate) { geo->first_entry_x + geo->entry_width + 1,
			    geo->first_entry_y + i });
      grub_putcode (GRUB_UNICODE_VLINE, term);
    }

  grub_term_gotoxy (term,
		    (struct grub_term_coordinate) { geo->first_entry_x - 1,
			geo->first_entry_y - 1 + geo->num_entries + 1 });
  grub_putcode (GRUB_UNICODE_CORNER_LL, term);
  for (i = 0; i < geo->entry_width + 1; i++)
    grub_putcode (GRUB_UNICODE_HLINE, term);
  grub_putcode (GRUB_UNICODE_CORNER_LR, term);

  grub_term_setcolorstate (term, GRUB_TERM_COLOR_NORMAL);

  grub_term_gotoxy (term,
		    (struct grub_term_coordinate) { geo->first_entry_x - 1,
			(geo->first_entry_y - 1 + geo->num_entries
			 + GRUB_TERM_MARGIN + 1) });
}

static int
print_message (int nested, int edit, struct grub_term_output *term, int dry_run)
{
  int ret = 0;
  grub_term_setcolorstate (term, GRUB_TERM_COLOR_NORMAL);

  if (edit)
    {
      ret += grub_print_message_indented_real (_("Minimum Emacs-like screen editing is \
supported. TAB lists completions. Press Ctrl-x or F10 to boot, Ctrl-c or F2 for a \
command-line or ESC to discard edits and return to the GRUB menu."),
					       STANDARD_MARGIN, STANDARD_MARGIN,
					       term, dry_run);
    }
  else
    {
      char *msg_translated;

      msg_translated = grub_xasprintf (_("Use the %C and %C keys to select which "
					 "entry is highlighted."),
				       GRUB_UNICODE_UPARROW,
				       GRUB_UNICODE_DOWNARROW);
      if (!msg_translated)
	return 0;
      ret += grub_print_message_indented_real (msg_translated, STANDARD_MARGIN,
					       STANDARD_MARGIN, term, dry_run);

      grub_free (msg_translated);

      if (nested)
	{
      #if 0
	  ret += grub_print_message_indented_real
	    (_("Press enter to boot the selected OS, "
	       "`e' to edit the commands before booting "
	       "or `c' for a command-line. ESC to return previous menu."),
	     STANDARD_MARGIN, STANDARD_MARGIN, term, dry_run);
      #endif
	}
      else
	{
	  char szLine[128];
	  const char *checkret = grub_env_get("VTOY_CHKDEV_RESULT_STRING");
      if (checkret == NULL || checkret[0] != '0') {
        grub_snprintf(szLine, sizeof(szLine), "%s  [Unofficial Ventoy]", grub_env_get("VTOY_TEXT_MENU_VER"));
      } else {
        grub_snprintf(szLine, sizeof(szLine), "%s", grub_env_get("VTOY_TEXT_MENU_VER"));
      }
      
	  ret += grub_print_message_indented_real("\n", STANDARD_MARGIN, STANDARD_MARGIN, term, dry_run);

	  ret += grub_print_message_indented_real(szLine, STANDARD_MARGIN, STANDARD_MARGIN, term, dry_run);
      
	  ret += grub_print_message_indented_real("\n", STANDARD_MARGIN, STANDARD_MARGIN, term, dry_run);
	  ret += grub_print_message_indented_real(g_ventoy_hotkey_tip,
	     3, 6, term, dry_run);
	}	
    }
  return ret;
}

static void
print_entry (int y, int highlight, grub_menu_entry_t entry,
	     const struct menu_viewer_data *data)
{
  const char *title;
  grub_size_t title_len;
  grub_ssize_t len;
  grub_uint32_t *unicode_title;
  grub_ssize_t i;
  grub_uint8_t old_color_normal, old_color_highlight;

  title = entry ? entry->title : "";
  title_len = grub_strlen (title);
  unicode_title = grub_malloc (title_len * sizeof (*unicode_title));
  if (! unicode_title)
    /* XXX How to show this error?  */
    return;

  len = grub_utf8_to_ucs4 (unicode_title, title_len,
                           (grub_uint8_t *) title, -1, 0);
  if (len < 0)
    {
      /* It is an invalid sequence.  */
      grub_free (unicode_title);
      return;
    }

  old_color_normal = grub_term_normal_color;
  old_color_highlight = grub_term_highlight_color;
  grub_term_normal_color = grub_color_menu_normal;
  grub_term_highlight_color = grub_color_menu_highlight;
  grub_term_setcolorstate (data->term, highlight
			   ? GRUB_TERM_COLOR_HIGHLIGHT
			   : GRUB_TERM_COLOR_NORMAL);

  grub_term_gotoxy (data->term, (struct grub_term_coordinate) { 
      data->geo.first_entry_x, y });

  for (i = 0; i < len; i++)
    if (unicode_title[i] == '\n' || unicode_title[i] == '\b'
	|| unicode_title[i] == '\r' || unicode_title[i] == '\e')
      unicode_title[i] = ' ';

  if (data->geo.num_entries > 1)
    grub_putcode (highlight ? '*' : ' ', data->term);

  grub_print_ucs4_menu (unicode_title,
			unicode_title + len,
			0,
			data->geo.right_margin,
			data->term, 0, 1,
			GRUB_UNICODE_RIGHTARROW, 0);

  grub_term_setcolorstate (data->term, GRUB_TERM_COLOR_NORMAL);
  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { 
		      grub_term_cursor_x (&data->geo), y });

  grub_term_normal_color = old_color_normal;
  grub_term_highlight_color = old_color_highlight;

  grub_term_setcolorstate (data->term, GRUB_TERM_COLOR_NORMAL);
  grub_free (unicode_title);
}

static void
print_entries (grub_menu_t menu, const struct menu_viewer_data *data)
{
  grub_menu_entry_t e;
  int i;

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { 
		      data->geo.first_entry_x + data->geo.entry_width
			+ data->geo.border + 1,
			data->geo.first_entry_y });

  if (data->geo.num_entries != 1)
    {
      if (data->first)
	grub_putcode (GRUB_UNICODE_UPARROW, data->term);
      else
	grub_putcode (' ', data->term);
    }
  e = grub_menu_get_entry (menu, data->first);

  for (i = 0; i < data->geo.num_entries; i++)
    {
      print_entry (data->geo.first_entry_y + i, data->offset == i,
		   e, data);
      if (e)
	e = e->next;
    }

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { data->geo.first_entry_x + data->geo.entry_width
			+ data->geo.border + 1,
			data->geo.first_entry_y + data->geo.num_entries - 1 });
  if (data->geo.num_entries == 1)
    {
      if (data->first && e)
	grub_putcode (GRUB_UNICODE_UPDOWNARROW, data->term);
      else if (data->first)
	grub_putcode (GRUB_UNICODE_UPARROW, data->term);
      else if (e)
	grub_putcode (GRUB_UNICODE_DOWNARROW, data->term);
      else
	grub_putcode (' ', data->term);
    }
  else
    {
      if (e)
	grub_putcode (GRUB_UNICODE_DOWNARROW, data->term);
      else
	grub_putcode (' ', data->term);
    }

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { grub_term_cursor_x (&data->geo),
			data->geo.first_entry_y + data->offset });
}

/* Initialize the screen.  If NESTED is non-zero, assume that this menu
   is run from another menu or a command-line. If EDIT is non-zero, show
   a message for the menu entry editor.  */
void
grub_menu_init_page (int nested, int edit,
		     struct grub_term_screen_geometry *geo,
		     struct grub_term_output *term)
{
  grub_uint8_t old_color_normal, old_color_highlight;
  int msg_num_lines;
  int bottom_message = 1;
  int empty_lines = 1;
  int version_msg = 1;

  geo->border = 1;
  geo->first_entry_x = 1 /* margin */ + 1 /* border */;
  geo->entry_width = grub_term_width (term) - 5;

  geo->first_entry_y = 2 /* two empty lines*/
    + 1 /* GNU GRUB version text  */ + 1 /* top border */;

  geo->timeout_lines = 2;

  /* 3 lines for timeout message and bottom margin.  2 lines for the border.  */
  geo->num_entries = grub_term_height (term) - geo->first_entry_y
    - 1 /* bottom border */
    - 1 /* empty line before info message*/
    - geo->timeout_lines /* timeout */
    - 1 /* empty final line  */;
  msg_num_lines = print_message (nested, edit, term, 1);
  if (geo->num_entries - msg_num_lines < 3
      || geo->entry_width < 10)
    {
      geo->num_entries += 4;
      geo->first_entry_y -= 2;
      empty_lines = 0;
      geo->first_entry_x -= 1;
      geo->entry_width += 1;
    }
  if (geo->num_entries - msg_num_lines < 3
      || geo->entry_width < 10)
    {
      geo->num_entries += 2;
      geo->first_entry_y -= 1;
      geo->first_entry_x -= 1;
      geo->entry_width += 2;
      geo->border = 0;
    }

  if (geo->entry_width <= 0)
    geo->entry_width = 1;

  if (geo->num_entries - msg_num_lines < 3
      && geo->timeout_lines == 2)
    {
      geo->timeout_lines = 1;
      geo->num_entries++;
    }

  if (geo->num_entries - msg_num_lines < 3)
    {
      geo->num_entries += 1;
      geo->first_entry_y -= 1;
      version_msg = 0;
    }

  if (geo->num_entries - msg_num_lines >= 2)
    geo->num_entries -= msg_num_lines;
  else
    bottom_message = 0;

  /* By default, use the same colors for the menu.  */
  old_color_normal = grub_term_normal_color;
  old_color_highlight = grub_term_highlight_color;
  grub_color_menu_normal = grub_term_normal_color;
  grub_color_menu_highlight = grub_term_highlight_color;

  /* Then give user a chance to replace them.  */
  grub_parse_color_name_pair (&grub_color_menu_normal,
			      grub_env_get ("menu_color_normal"));
  grub_parse_color_name_pair (&grub_color_menu_highlight,
			      grub_env_get ("menu_color_highlight"));

  if (version_msg)
    grub_normal_init_page (term, empty_lines);
  else
    grub_term_cls (term);

  grub_term_normal_color = grub_color_menu_normal;
  grub_term_highlight_color = grub_color_menu_highlight;
  if (geo->border)
    draw_border (term, geo);
  grub_term_normal_color = old_color_normal;
  grub_term_highlight_color = old_color_highlight;
  geo->timeout_y = geo->first_entry_y + geo->num_entries
    + geo->border + empty_lines;
  if (bottom_message)
    {
      grub_term_gotoxy (term,
			(struct grub_term_coordinate) { GRUB_TERM_MARGIN,
			    geo->timeout_y });

      print_message (nested, edit, term, 0);
      geo->timeout_y += msg_num_lines;
    }
  geo->right_margin = grub_term_width (term)
    - geo->first_entry_x
    - geo->entry_width - 1;
}

static void
menu_text_print_timeout (int timeout, void *dataptr)
{
  struct menu_viewer_data *data = dataptr;
  char *msg_translated = 0;

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { 0, data->geo.timeout_y });

  if (data->timeout_msg == TIMEOUT_TERSE
      || data->timeout_msg == TIMEOUT_TERSE_NO_MARGIN)
    msg_translated = grub_xasprintf (_("%ds"), timeout);
  else
    msg_translated = grub_xasprintf (_("The highlighted entry will be executed automatically in %ds."), timeout);
  if (!msg_translated)
    {
      grub_print_error ();
      grub_errno = GRUB_ERR_NONE;
      return;
    }

  if (data->timeout_msg == TIMEOUT_UNKNOWN)
    {
      data->timeout_msg = grub_print_message_indented_real (msg_translated,
							    3, 1, data->term, 1)
	<= data->geo.timeout_lines ? TIMEOUT_NORMAL : TIMEOUT_TERSE;
      if (data->timeout_msg == TIMEOUT_TERSE)
	{
	  grub_free (msg_translated);
	  msg_translated = grub_xasprintf (_("%ds"), timeout);
	  if (grub_term_width (data->term) < 10)
	    data->timeout_msg = TIMEOUT_TERSE_NO_MARGIN;
	}
    }

  grub_print_message_indented (msg_translated,
			       data->timeout_msg == TIMEOUT_TERSE_NO_MARGIN ? 0 : 3,
			       data->timeout_msg == TIMEOUT_TERSE_NO_MARGIN ? 0 : 1,
			       data->term);
  grub_free (msg_translated);

  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) { 
		      grub_term_cursor_x (&data->geo),
			data->geo.first_entry_y + data->offset });
  grub_term_refresh (data->term);
}

static void
menu_text_set_chosen_entry (int entry, void *dataptr)
{
  struct menu_viewer_data *data = dataptr;
  int oldoffset = data->offset;
  int complete_redraw = 0;

  data->offset = entry - data->first;
  if (data->offset > data->geo.num_entries - 1)
    {
      data->first = entry - (data->geo.num_entries - 1);
      data->offset = data->geo.num_entries - 1;
      complete_redraw = 1;
    }
  if (data->offset < 0)
    {
      data->offset = 0;
      data->first = entry;
      complete_redraw = 1;
    }
  if (complete_redraw)
    print_entries (data->menu, data);
  else
    {
      print_entry (data->geo.first_entry_y + oldoffset, 0,
		   grub_menu_get_entry (data->menu, data->first + oldoffset),
		   data);
      print_entry (data->geo.first_entry_y + data->offset, 1,
		   grub_menu_get_entry (data->menu, data->first + data->offset),
		   data);
    }
  grub_term_refresh (data->term);
}

static void
menu_text_scroll_chosen_entry (void *dataptr, int diren)
{
  struct menu_viewer_data *data = dataptr;
  const char *orig_title, *scrolled_title;
  int off;
  int selected;
  grub_menu_entry_t entry;

  if (!data->menu->size)
    return;

  selected = data->first + data->offset;
  entry = grub_menu_get_entry (data->menu, selected);
  orig_title = entry->title;
  off = data->menu_title_offset[selected] + diren;
  if (off < 0
      || off > grub_utf8_get_num_code (orig_title, grub_strlen(orig_title)))
    return;

  scrolled_title =
    grub_utf8_offset_code (orig_title, grub_strlen (orig_title), off);
  if (scrolled_title)
    entry->title = scrolled_title;
  print_entry (data->geo.first_entry_y + data->offset, 1, entry, data);

  entry->title = orig_title;
  data->menu_title_offset[selected] = off;
  grub_term_refresh (data->term);
}

static void
menu_text_fini (void *dataptr)
{
  struct menu_viewer_data *data = dataptr;

  grub_term_setcursor (data->term, 1);
  grub_term_cls (data->term);
  if (data->menu_title_offset)
    grub_free (data->menu_title_offset);
  grub_free (data);
}

static void
menu_text_clear_timeout (void *dataptr)
{
  struct menu_viewer_data *data = dataptr;
  int i;

  for (i = 0; i < data->geo.timeout_lines;i++)
    {
      grub_term_gotoxy (data->term, (struct grub_term_coordinate) {
	  0, data->geo.timeout_y + i });
      grub_print_spaces (data->term, grub_term_width (data->term) - 1);
    }
  if (data->geo.num_entries <= 5 && !data->geo.border)
    {
      grub_term_gotoxy (data->term,
			(struct grub_term_coordinate) { 
			  data->geo.first_entry_x + data->geo.entry_width
			    + data->geo.border + 1,
			    data->geo.first_entry_y + data->geo.num_entries - 1
			    });
      grub_putcode (' ', data->term);

      data->geo.timeout_lines = 0;
      data->geo.num_entries++;
      print_entries (data->menu, data);
    }
  grub_term_gotoxy (data->term,
		    (struct grub_term_coordinate) {
		      grub_term_cursor_x (&data->geo),
			data->geo.first_entry_y + data->offset });
  grub_term_refresh (data->term);
}

grub_err_t 
grub_menu_try_text (struct grub_term_output *term, 
		    int entry, grub_menu_t menu, int nested)
{
  struct menu_viewer_data *data;
  struct grub_menu_viewer *instance;

  instance = grub_zalloc (sizeof (*instance));
  if (!instance)
    return grub_errno;

  data = grub_zalloc (sizeof (*data));
  if (!data)
    {
      grub_free (instance);
      return grub_errno;
    }

  if (menu->size)
    data->menu_title_offset = grub_zalloc (sizeof (*data->menu_title_offset) * menu->size);

  data->term = term;
  instance->data = data;
  instance->set_chosen_entry = menu_text_set_chosen_entry;
  if (data->menu_title_offset)
    instance->scroll_chosen_entry = menu_text_scroll_chosen_entry;
  instance->print_timeout = menu_text_print_timeout;
  instance->clear_timeout = menu_text_clear_timeout;
  instance->fini = menu_text_fini;

  data->menu = menu;

  data->offset = entry;
  data->first = 0;

  grub_term_setcursor (data->term, 0);
  grub_menu_init_page (nested, 0, &data->geo, data->term);

  if (data->offset > data->geo.num_entries - 1)
    {
      data->first = data->offset - (data->geo.num_entries - 1);
      data->offset = data->geo.num_entries - 1;
    }

  print_entries (menu, data);
  grub_term_refresh (data->term);
  grub_menu_register_viewer (instance);

  return GRUB_ERR_NONE;
}
