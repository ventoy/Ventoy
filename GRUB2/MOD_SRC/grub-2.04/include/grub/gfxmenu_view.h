/* gfxmenu_view.h - gfxmenu view interface. */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008,2009  Free Software Foundation, Inc.
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

#ifndef GRUB_GFXMENU_VIEW_HEADER
#define GRUB_GFXMENU_VIEW_HEADER 1

#include <grub/types.h>
#include <grub/err.h>
#include <grub/menu.h>
#include <grub/font.h>
#include <grub/gfxwidgets.h>

struct grub_gfxmenu_view;   /* Forward declaration of opaque type.  */
typedef struct grub_gfxmenu_view *grub_gfxmenu_view_t;


grub_gfxmenu_view_t grub_gfxmenu_view_new (const char *theme_path,
					   int width, int height);

void grub_gfxmenu_view_destroy (grub_gfxmenu_view_t view);

/* Set properties on the view based on settings from the specified
   theme file.  */
grub_err_t grub_gfxmenu_view_load_theme (grub_gfxmenu_view_t view,
                                         const char *theme_path);

grub_err_t grub_gui_recreate_box (grub_gfxmenu_box_t *boxptr,
                                  const char *pattern, const char *theme_dir);

void grub_gfxmenu_view_draw (grub_gfxmenu_view_t view);

void
grub_gfxmenu_redraw_menu (grub_gfxmenu_view_t view);

void
grub_gfxmenu_redraw_timeout (grub_gfxmenu_view_t view);

void
grub_gfxmenu_view_redraw (grub_gfxmenu_view_t view,
			  const grub_video_rect_t *region);

void 
grub_gfxmenu_clear_timeout (void *data);
void 
grub_gfxmenu_print_timeout (int timeout, void *data);
void
grub_gfxmenu_set_chosen_entry (int entry, void *data);
void
grub_gfxmenu_scroll_chosen_entry (void *data, int diren);

grub_err_t grub_font_draw_string (const char *str,
				  grub_font_t font,
				  grub_video_color_t color,
				  int left_x, int baseline_y);
int grub_font_get_string_width (grub_font_t font,
				const char *str);


/* Implementation details -- this should not be used outside of the
   view itself.  */

#include <grub/video.h>
#include <grub/bitmap.h>
#include <grub/bitmap_scale.h>
#include <grub/gui.h>
#include <grub/gfxwidgets.h>
#include <grub/icon_manager.h>

/* Definition of the private representation of the view.  */
struct grub_gfxmenu_view
{
  grub_video_rect_t screen;

  int need_to_check_sanity;
  grub_video_rect_t terminal_rect;
  int terminal_border;

  grub_font_t title_font;
  grub_font_t message_font;
  char *terminal_font_name;
  grub_video_rgba_color_t title_color;
  grub_video_rgba_color_t message_color;
  grub_video_rgba_color_t message_bg_color;
  struct grub_video_bitmap *raw_desktop_image;
  struct grub_video_bitmap *scaled_desktop_image;
  grub_video_bitmap_selection_method_t desktop_image_scale_method;
  grub_video_bitmap_h_align_t desktop_image_h_align;
  grub_video_bitmap_v_align_t desktop_image_v_align;
  grub_video_rgba_color_t desktop_color;
  grub_gfxmenu_box_t terminal_box;
  char *title_text;
  char *progress_message_text;
  char *theme_path;

  grub_gui_container_t canvas;

  int double_repaint;

  int selected;

  grub_video_rect_t progress_message_frame;

  grub_menu_t menu;

  int nested;

  int first_timeout;

  int *menu_title_offset;
};

#endif /* ! GRUB_GFXMENU_VIEW_HEADER */
