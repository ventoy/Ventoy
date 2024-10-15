/* gui_list.c - GUI component to display a selectable list of items.  */
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

#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/gui.h>
#include <grub/gui_string_util.h>
#include <grub/gfxmenu_view.h>
#include <grub/gfxwidgets.h>
#include <grub/color.h>
#include <grub/charset.h>

enum scrollbar_slice_mode {
  SCROLLBAR_SLICE_WEST,
  SCROLLBAR_SLICE_CENTER,
  SCROLLBAR_SLICE_EAST
};

struct grub_gui_list_impl
{
  struct grub_gui_list list;

  grub_gui_container_t parent;
  grub_video_rect_t bounds;
  char *id;
  int visible;

  int icon_width;
  int icon_height;
  int item_height;
  int item_padding;
  int item_icon_space;
  int item_spacing;
  grub_font_t item_font;
  int selected_item_font_inherit;
  grub_font_t selected_item_font;
  grub_video_rgba_color_t item_color;
  int selected_item_color_inherit;
  grub_video_rgba_color_t selected_item_color;

  int draw_scrollbar;
  int need_to_recreate_scrollbar;
  char *scrollbar_frame_pattern;
  char *scrollbar_thumb_pattern;
  grub_gfxmenu_box_t scrollbar_frame;
  grub_gfxmenu_box_t scrollbar_thumb;
  int scrollbar_thumb_overlay;
  int scrollbar_width;
  enum scrollbar_slice_mode scrollbar_slice;
  int scrollbar_left_pad;
  int scrollbar_right_pad;
  int scrollbar_top_pad;
  int scrollbar_bottom_pad;

  int first_shown_index;

  int need_to_recreate_boxes;
  char *theme_dir;
  char *menu_box_pattern;
  char *item_box_pattern;
  int selected_item_box_pattern_inherit;
  char *selected_item_box_pattern;
  grub_gfxmenu_box_t menu_box;
  grub_gfxmenu_box_t selected_item_box;
  grub_gfxmenu_box_t item_box;

  grub_gfxmenu_icon_manager_t icon_manager;

  grub_gfxmenu_view_t view;
};

typedef struct grub_gui_list_impl *list_impl_t;

static void
list_destroy (void *vself)
{
  list_impl_t self = vself;

  grub_free (self->theme_dir);
  grub_free (self->menu_box_pattern);
  grub_free (self->item_box_pattern);
  grub_free (self->selected_item_box_pattern);
  if (self->menu_box)
    self->menu_box->destroy (self->menu_box);
  if (self->item_box)
    self->item_box->destroy (self->item_box);
  if (self->selected_item_box)
    self->selected_item_box->destroy (self->selected_item_box);
  if (self->icon_manager)
    grub_gfxmenu_icon_manager_destroy (self->icon_manager);
  if (self->scrollbar_thumb)
    self->scrollbar_thumb->destroy (self->scrollbar_thumb);
  if (self->scrollbar_frame)
    self->scrollbar_frame->destroy (self->scrollbar_frame);
  grub_free (self->scrollbar_thumb_pattern);
  grub_free (self->scrollbar_frame_pattern);
  grub_free (self);
}

static int
get_num_shown_items (list_impl_t self)
{
  int boxpad = self->item_padding;
  int item_vspace = self->item_spacing;
  int item_height = self->item_height;
  
  grub_gfxmenu_box_t box = self->menu_box;
  int box_top_pad = box->get_top_pad (box);
  int box_bottom_pad = box->get_bottom_pad (box);
  grub_gfxmenu_box_t itembox = self->item_box;
  grub_gfxmenu_box_t selbox = self->selected_item_box;
  int item_top_pad = itembox->get_top_pad (itembox);
  int item_bottom_pad = itembox->get_bottom_pad (itembox);
  int sel_top_pad = selbox->get_top_pad (selbox);
  int sel_bottom_pad = selbox->get_bottom_pad (selbox);
  int max_top_pad = grub_max (item_top_pad, sel_top_pad);
  int max_bottom_pad = grub_max (item_bottom_pad, sel_bottom_pad);

  if (item_height + item_vspace <= 0)
    return 1;

  return (self->bounds.height + item_vspace - 2 * boxpad
          - max_top_pad - max_bottom_pad
          - box_top_pad - box_bottom_pad) / (item_height + item_vspace);
}

static int
check_boxes (list_impl_t self)
{
  if (self->need_to_recreate_boxes)
    {
      grub_gui_recreate_box (&self->menu_box,
                             self->menu_box_pattern,
                             self->theme_dir);

      grub_gui_recreate_box (&self->item_box,
                             self->item_box_pattern,
                             self->theme_dir);

      grub_gui_recreate_box (&self->selected_item_box,
                             self->selected_item_box_pattern,
                             self->theme_dir);

      self->need_to_recreate_boxes = 0;
    }

  return (self->menu_box != 0 && self->selected_item_box != 0
          && self->item_box != 0);
}

static int
check_scrollbar (list_impl_t self)
{
  if (self->need_to_recreate_scrollbar)
    {
      grub_gui_recreate_box (&self->scrollbar_frame,
                             self->scrollbar_frame_pattern,
                             self->theme_dir);

      grub_gui_recreate_box (&self->scrollbar_thumb,
                             self->scrollbar_thumb_pattern,
                             self->theme_dir);

      self->need_to_recreate_scrollbar = 0;
    }

  if (self->scrollbar_frame == 0 || self->scrollbar_thumb == 0)
    return 0;

  /* Sanity checks. */
  grub_gfxmenu_box_t frame = self->scrollbar_frame;
  grub_gfxmenu_box_t thumb = self->scrollbar_thumb;
  grub_gfxmenu_box_t menu = self->menu_box;
  int min_width = frame->get_left_pad (frame)
                  + frame->get_right_pad (frame);
  int min_height = frame->get_top_pad (frame)
                   + frame->get_bottom_pad (frame)
                   + self->scrollbar_top_pad + self->scrollbar_bottom_pad
                   + menu->get_top_pad (menu)
                   + menu->get_bottom_pad (menu);
  if (!self->scrollbar_thumb_overlay)
    {
      min_width += thumb->get_left_pad (thumb)
                   + thumb->get_right_pad (thumb);
      min_height += thumb->get_top_pad (thumb)
                    + thumb->get_bottom_pad (thumb);
    }
  if (min_width <= self->scrollbar_width
      && min_height <= (int) self->bounds.height)
    return 1;

  /* Unprintable dimenstions. */
  self->draw_scrollbar = 0;
  return 0;
}

static const char *
list_get_id (void *vself)
{
  list_impl_t self = vself;
  return self->id;
}

static int
list_is_instance (void *vself __attribute__((unused)), const char *type)
{
  return (grub_strcmp (type, "component") == 0
          || grub_strcmp (type, "list") == 0);
}

static struct grub_video_bitmap *
get_item_icon (list_impl_t self, int item_index)
{
  grub_menu_entry_t entry;
  entry = grub_menu_get_entry (self->view->menu, item_index);
  if (! entry)
    return 0;

  return grub_gfxmenu_icon_manager_get_icon (self->icon_manager, entry);
}

static void
make_selected_item_visible (list_impl_t self)
{
  int selected_index = self->view->selected;
  if (selected_index < 0)
    return;   /* No item is selected.  */
  int num_shown_items = get_num_shown_items (self);
  int last_shown_index = self->first_shown_index + (num_shown_items - 1);
  if (selected_index < self->first_shown_index)
    self->first_shown_index = selected_index;
  else if (selected_index > last_shown_index)
    self->first_shown_index = selected_index - (num_shown_items - 1);
}

/* Draw a scrollbar on the menu.  */
static void
draw_scrollbar (list_impl_t self,
                int value, int extent, int min, int max,
                int scrollbar_width, int scrollbar_height)
{
  unsigned thumby, thumbheight;

  grub_gfxmenu_box_t frame = self->scrollbar_frame;
  grub_gfxmenu_box_t thumb = self->scrollbar_thumb;
  int frame_vertical_pad = (frame->get_top_pad (frame)
                            + frame->get_bottom_pad (frame));
  int frame_horizontal_pad = (frame->get_left_pad (frame)
                              + frame->get_right_pad (frame));
  unsigned thumb_vertical_pad = (thumb->get_top_pad (thumb)
				 + thumb->get_bottom_pad (thumb));
  int thumb_horizontal_pad = (thumb->get_left_pad (thumb)
                              + thumb->get_right_pad (thumb));
  int tracktop = frame->get_top_pad (frame);
  unsigned tracklen;
  if (scrollbar_height <= frame_vertical_pad)
    tracklen = 0;
  else
    tracklen = scrollbar_height - frame_vertical_pad;
  frame->set_content_size (frame,
                           scrollbar_width - frame_horizontal_pad,
                           tracklen);
  if (self->scrollbar_thumb_overlay)
    {
      tracklen += thumb_vertical_pad;
      tracktop -= thumb->get_top_pad (thumb);
    }
  if (value <= min || max <= min)
    thumby = 0;
  else
    thumby = ((unsigned) tracklen * (value - min))
      / ((unsigned) (max - min));
  if (max <= min)
    thumbheight = 1;
  else
    thumbheight = ((unsigned) (tracklen * extent)
		   / ((unsigned) (max - min))) + 1;
  /* Rare occasion: too many entries or too low height. */
  if (thumbheight < thumb_vertical_pad)
    {
      thumbheight = thumb_vertical_pad;
      if (value <= min || max <= extent
	  || tracklen <= thumb_vertical_pad)
	thumby = 0;
      else
	thumby = ((unsigned) ((tracklen - thumb_vertical_pad) * (value - min))
		  / ((unsigned)(max - extent)));
    }
  thumby += tracktop;
  int thumbx = frame->get_left_pad (frame);
  int thumbwidth = scrollbar_width - frame_horizontal_pad;
  if (!self->scrollbar_thumb_overlay)
    thumbwidth -= thumb_horizontal_pad;
  else
    thumbx -= thumb->get_left_pad (thumb);
  thumb->set_content_size (thumb, thumbwidth,
                           thumbheight - thumb_vertical_pad);
  frame->draw (frame, 0, 0);
  thumb->draw (thumb, thumbx, thumby);
}

/* Draw the list of items.  */
static void
draw_menu (list_impl_t self, int num_shown_items)
{
  if (! self->menu_box || ! self->selected_item_box || ! self->item_box)
    return;

  int boxpad = self->item_padding;
  int icon_text_space = self->item_icon_space;
  int item_vspace = self->item_spacing;

  int ascent = grub_font_get_ascent (self->item_font);
  int descent = grub_font_get_descent (self->item_font);
  int selected_ascent = grub_font_get_ascent (self->selected_item_font);
  int selected_descent = grub_font_get_descent (self->selected_item_font);
  int text_box_height = self->item_height;

  make_selected_item_visible (self);

  grub_gfxmenu_box_t itembox = self->item_box;
  grub_gfxmenu_box_t selbox = self->selected_item_box;
  int item_leftpad = itembox->get_left_pad (itembox);
  int item_rightpad = itembox->get_right_pad (itembox);
  int item_border_width = item_leftpad + item_rightpad;
  int item_toppad = itembox->get_top_pad (itembox);
  int sel_leftpad = selbox->get_left_pad (selbox);
  int sel_rightpad = selbox->get_right_pad (selbox);
  int sel_border_width = sel_leftpad + sel_rightpad;
  int sel_toppad = selbox->get_top_pad (selbox);

  int max_leftpad = grub_max (item_leftpad, sel_leftpad);
  int max_toppad = grub_max (item_toppad, sel_toppad);
  int item_top = 0;
  int menu_index;
  int visible_index;
  struct grub_video_rect oviewport;

  grub_video_get_viewport (&oviewport.x, &oviewport.y,
			   &oviewport.width, &oviewport.height);
  grub_video_set_viewport (oviewport.x + boxpad, 
			   oviewport.y + boxpad,
			   oviewport.width - 2 * boxpad,
			   oviewport.height - 2 * boxpad);

  int cwidth = oviewport.width - 2 * boxpad;

  itembox->set_content_size (itembox, cwidth - item_border_width,
                             text_box_height);
  selbox->set_content_size (selbox, cwidth - sel_border_width,
                            text_box_height);

  int text_left_offset = self->icon_width + icon_text_space;
  int item_text_top_offset = (text_box_height - (ascent + descent)) / 2 + ascent;
  int sel_text_top_offset = (text_box_height - (selected_ascent
                                                + selected_descent)) / 2
                                 + selected_ascent;

  grub_video_rect_t svpsave, sviewport;
  sviewport.x = max_leftpad + text_left_offset;
  int text_viewport_width = cwidth - sviewport.x;
  sviewport.height = text_box_height;

  grub_video_color_t item_color;
  grub_video_color_t sel_color;
  item_color = grub_video_map_rgba_color (self->item_color);
  sel_color = grub_video_map_rgba_color (self->selected_item_color);

  int item_box_top_offset = max_toppad - item_toppad;
  int sel_box_top_offset = max_toppad - sel_toppad;
  int item_viewport_width = text_viewport_width - item_rightpad;
  int sel_viewport_width = text_viewport_width - sel_rightpad;
  int tmp_icon_top_offset = (text_box_height - self->icon_height) / 2;
  int item_icon_top_offset = item_toppad + tmp_icon_top_offset;
  int sel_icon_top_offset = sel_toppad + tmp_icon_top_offset;

  for (visible_index = 0, menu_index = self->first_shown_index;
       visible_index < num_shown_items && menu_index < self->view->menu->size;
       visible_index++, menu_index++)
    {
      int is_selected = (menu_index == self->view->selected);
      struct grub_video_bitmap *icon;
      grub_font_t font;
      grub_video_color_t color;
      int text_top_offset;
      int top_pad;
      int icon_top_offset;
      int viewport_width;

      if (is_selected)
        {
          selbox->draw (selbox, 0, item_top + sel_box_top_offset);
          font = self->selected_item_font;
          color = sel_color;
          text_top_offset = sel_text_top_offset;
          top_pad = sel_toppad;
          icon_top_offset = sel_icon_top_offset;
          viewport_width = sel_viewport_width;
        }
      else
        {
          itembox->draw (itembox, 0, item_top + item_box_top_offset);
          font = self->item_font;
          color = item_color;
          text_top_offset = item_text_top_offset;
          top_pad = item_toppad;
          icon_top_offset = item_icon_top_offset;
          viewport_width = item_viewport_width;
        }

      icon = get_item_icon (self, menu_index);
      if (icon != 0)
        grub_video_blit_bitmap (icon, GRUB_VIDEO_BLIT_BLEND,
                                max_leftpad,
                                item_top + icon_top_offset,
                                0, 0, self->icon_width, self->icon_height);

      const char *item_title =
        grub_menu_get_entry (self->view->menu, menu_index)->title;


      int off = self->view->menu_title_offset[menu_index];
      const char *scrolled_title =
        grub_utf8_offset_code (item_title, grub_strlen (item_title), off);
      if (scrolled_title)
        item_title = scrolled_title;

      sviewport.y = item_top + top_pad;
      sviewport.width = viewport_width;
      grub_gui_set_viewport (&sviewport, &svpsave);
      grub_font_draw_string (item_title,
                             font,
                             color,
                             0,
                             text_top_offset);
      grub_gui_restore_viewport (&svpsave);

      item_top += text_box_height + item_vspace;
    }
  grub_video_set_viewport (oviewport.x,
			   oviewport.y,
			   oviewport.width,
			   oviewport.height);
}

static void
list_paint (void *vself, const grub_video_rect_t *region)
{
  list_impl_t self = vself;
  grub_video_rect_t vpsave;

  if (! self->visible)
    return;
  if (!grub_video_have_common_points (region, &self->bounds))
    return;

  check_boxes (self);

  if (! self->menu_box || ! self->selected_item_box || ! self->item_box)
    return;

  grub_gui_set_viewport (&self->bounds, &vpsave);
  {
    grub_gfxmenu_box_t box = self->menu_box;
    int box_left_pad = box->get_left_pad (box);
    int box_top_pad = box->get_top_pad (box);
    int box_right_pad = box->get_right_pad (box);
    int box_bottom_pad = box->get_bottom_pad (box);
    grub_video_rect_t vpsave2, content_rect;
    int num_shown_items = get_num_shown_items (self);
    int drawing_scrollbar = (self->draw_scrollbar
			     && (num_shown_items < self->view->menu->size)
			     && check_scrollbar (self));
    int scrollbar_width = self->scrollbar_width;

    content_rect.x = box_left_pad;
    content_rect.y = box_top_pad;
    content_rect.width = self->bounds.width - box_left_pad - box_right_pad;
    content_rect.height = self->bounds.height - box_top_pad - box_bottom_pad;

    box->set_content_size (box, content_rect.width, content_rect.height);

    box->draw (box, 0, 0);

    switch (self->scrollbar_slice)
      {
        case SCROLLBAR_SLICE_WEST:
          content_rect.x += self->scrollbar_right_pad;
          content_rect.width -= self->scrollbar_right_pad;
          break;
        case SCROLLBAR_SLICE_CENTER:
          if (drawing_scrollbar)
            content_rect.width -= scrollbar_width + self->scrollbar_left_pad
                                  + self->scrollbar_right_pad;
          break;
        case SCROLLBAR_SLICE_EAST:
          content_rect.width -= self->scrollbar_left_pad;
          break;
      }

    grub_gui_set_viewport (&content_rect, &vpsave2);
    draw_menu (self, num_shown_items);
    grub_gui_restore_viewport (&vpsave2);

    if (drawing_scrollbar)
      {
        content_rect.y += self->scrollbar_top_pad;
        content_rect.height -= self->scrollbar_top_pad
                               + self->scrollbar_bottom_pad;
        content_rect.width = scrollbar_width;
        switch (self->scrollbar_slice)
          {
            case SCROLLBAR_SLICE_WEST:
              if (box_left_pad > scrollbar_width)
                {
                  content_rect.x = box_left_pad - scrollbar_width;
                  content_rect.width = scrollbar_width;
                }
              else
                {
                  content_rect.x = 0;
                  content_rect.width = box_left_pad;
                }
              break;
            case SCROLLBAR_SLICE_CENTER:
              content_rect.x = self->bounds.width - box_right_pad
                               - scrollbar_width - self->scrollbar_right_pad;
              content_rect.width = scrollbar_width;
              break;
            case SCROLLBAR_SLICE_EAST:
              content_rect.x = self->bounds.width - box_right_pad;
              content_rect.width = box_right_pad;
              break;
          }

        grub_gui_set_viewport (&content_rect, &vpsave2);
        draw_scrollbar (self,
                        self->first_shown_index, num_shown_items,
                        0, self->view->menu->size,
                        scrollbar_width,
                        content_rect.height);
        grub_gui_restore_viewport (&vpsave2);
      }
  }

  grub_gui_restore_viewport (&vpsave);
}

static void
list_set_parent (void *vself, grub_gui_container_t parent)
{
  list_impl_t self = vself;
  self->parent = parent;
}

static grub_gui_container_t
list_get_parent (void *vself)
{
  list_impl_t self = vself;
  return self->parent;
}

static void
list_set_bounds (void *vself, const grub_video_rect_t *bounds)
{
  list_impl_t self = vself;
  self->bounds = *bounds;
}

static void
list_get_bounds (void *vself, grub_video_rect_t *bounds)
{
  list_impl_t self = vself;
  *bounds = self->bounds;
}

static void
list_get_minimal_size (void *vself, unsigned *width, unsigned *height)
{
  list_impl_t self = vself;

  if (check_boxes (self))
    {
      int boxpad = self->item_padding;
      int item_vspace = self->item_spacing;
      int item_height = self->item_height;
      int num_items = 3;

      grub_gfxmenu_box_t box = self->menu_box;
      int box_left_pad = box->get_left_pad (box);
      int box_top_pad = box->get_top_pad (box);
      int box_right_pad = box->get_right_pad (box);
      int box_bottom_pad = box->get_bottom_pad (box);
      unsigned width_s;

      grub_gfxmenu_box_t selbox = self->selected_item_box;
      int sel_top_pad = selbox->get_top_pad (selbox);
      int sel_bottom_pad = selbox->get_bottom_pad (selbox);
      int sel_left_pad = selbox->get_left_pad (selbox);
      int sel_right_pad = selbox->get_right_pad (selbox);

      grub_gfxmenu_box_t itembox = self->item_box;
      int item_top_pad = itembox->get_top_pad (itembox);
      int item_bottom_pad = itembox->get_bottom_pad (itembox);
      int item_left_pad = itembox->get_left_pad (itembox);
      int item_right_pad = itembox->get_right_pad (itembox);

      int max_left_pad = grub_max (item_left_pad, sel_left_pad);
      int max_right_pad = grub_max (item_right_pad, sel_right_pad);
      int max_top_pad = grub_max (item_top_pad, sel_top_pad);
      int max_bottom_pad = grub_max (item_bottom_pad, sel_bottom_pad);

      *width = grub_font_get_string_width (self->item_font, "Typical OS");
      width_s = grub_font_get_string_width (self->selected_item_font,
					    "Typical OS");
      if (*width < width_s)
	*width = width_s;

      *width += 2 * boxpad + box_left_pad + box_right_pad
                + max_left_pad + max_right_pad
                + self->item_icon_space + self->icon_width;

      switch (self->scrollbar_slice)
        {
          case SCROLLBAR_SLICE_WEST:
            *width += self->scrollbar_right_pad;
            break;
          case SCROLLBAR_SLICE_CENTER:
            *width += self->scrollbar_width + self->scrollbar_left_pad
                      + self->scrollbar_right_pad;
            break;
          case SCROLLBAR_SLICE_EAST:
            *width += self->scrollbar_left_pad;
            break;
        }

      /* Set the menu box height to fit the items.  */
      *height = (item_height * num_items
                 + item_vspace * (num_items - 1)
                 + 2 * boxpad
                 + box_top_pad + box_bottom_pad
                 + max_top_pad + max_bottom_pad);
    }
  else
    {
      *width = 0;
      *height = 0;
    }
}

static grub_err_t
list_set_property (void *vself, const char *name, const char *value)
{
  list_impl_t self = vself;
  if (grub_strcmp (name, "item_font") == 0)
    {
      self->item_font = grub_font_get (value);
      if (self->selected_item_font_inherit)
        self->selected_item_font = self->item_font;
    }
  else if (grub_strcmp (name, "selected_item_font") == 0)
    {
      if (! value || grub_strcmp (value, "inherit") == 0)
        {
          self->selected_item_font = self->item_font;
          self->selected_item_font_inherit = 1;
        }
      else
        {
          self->selected_item_font = grub_font_get (value);
          self->selected_item_font_inherit = 0;
        }
    }
  else if (grub_strcmp (name, "item_color") == 0)
    {
      grub_video_rgba_color_t color;
      if (grub_video_parse_color (value, &color) == GRUB_ERR_NONE)
        {
          self->item_color = color;
          if (self->selected_item_color_inherit)
            self->selected_item_color = self->item_color;
        }
    }
  else if (grub_strcmp (name, "selected_item_color") == 0)
    {
      if (! value || grub_strcmp (value, "inherit") == 0)
        {
          self->selected_item_color = self->item_color;
          self->selected_item_color_inherit = 1;
        }
      else
        {
          grub_video_rgba_color_t color;
          if (grub_video_parse_color (value, &color)
              == GRUB_ERR_NONE)
            {
              self->selected_item_color = color;
              self->selected_item_color_inherit = 0;
            }
        }
    }
  else if (grub_strcmp (name, "icon_width") == 0)
    {
      self->icon_width = grub_strtol (value, 0, 10);
      grub_gfxmenu_icon_manager_set_icon_size (self->icon_manager,
                                               self->icon_width,
                                               self->icon_height);
    }
  else if (grub_strcmp (name, "icon_height") == 0)
    {
      self->icon_height = grub_strtol (value, 0, 10);
      grub_gfxmenu_icon_manager_set_icon_size (self->icon_manager,
                                               self->icon_width,
                                               self->icon_height);
    }
  else if (grub_strcmp (name, "item_height") == 0)
    {
      self->item_height = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "item_padding") == 0)
    {
      self->item_padding = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "item_icon_space") == 0)
    {
      self->item_icon_space = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "item_spacing") == 0)
    {
      self->item_spacing = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "visible") == 0)
    {
      self->visible = grub_strcmp (value, "false") != 0;
    }
  else if (grub_strcmp (name, "menu_pixmap_style") == 0)
    {
      self->need_to_recreate_boxes = 1;
      grub_free (self->menu_box_pattern);
      self->menu_box_pattern = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "item_pixmap_style") == 0)
    {
      self->need_to_recreate_boxes = 1;
      grub_free (self->item_box_pattern);
      self->item_box_pattern = value ? grub_strdup (value) : 0;
      if (self->selected_item_box_pattern_inherit)
        {
          grub_free (self->selected_item_box_pattern);
          self->selected_item_box_pattern = value ? grub_strdup (value) : 0;
        }
    }
  else if (grub_strcmp (name, "selected_item_pixmap_style") == 0)
    {
      if (!value || grub_strcmp (value, "inherit") == 0)
        {
          grub_free (self->selected_item_box_pattern);
          char *tmp = self->item_box_pattern;
          self->selected_item_box_pattern = tmp ? grub_strdup (tmp) : 0;
          self->selected_item_box_pattern_inherit = 1;
        }
      else
        {
          self->need_to_recreate_boxes = 1;
          grub_free (self->selected_item_box_pattern);
          self->selected_item_box_pattern = value ? grub_strdup (value) : 0;
          self->selected_item_box_pattern_inherit = 0;
        }
    }
  else if (grub_strcmp (name, "scrollbar_frame") == 0)
    {
      self->need_to_recreate_scrollbar = 1;
      grub_free (self->scrollbar_frame_pattern);
      self->scrollbar_frame_pattern = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "scrollbar_thumb") == 0)
    {
      self->need_to_recreate_scrollbar = 1;
      grub_free (self->scrollbar_thumb_pattern);
      self->scrollbar_thumb_pattern = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "scrollbar_thumb_overlay") == 0)
    {
      self->scrollbar_thumb_overlay = grub_strcmp (value, "true") == 0;
    }
  else if (grub_strcmp (name, "scrollbar_width") == 0)
    {
      self->scrollbar_width = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "scrollbar_slice") == 0)
    {
      if (grub_strcmp (value, "west") == 0)
        self->scrollbar_slice = SCROLLBAR_SLICE_WEST;
      else if (grub_strcmp (value, "center") == 0)
        self->scrollbar_slice = SCROLLBAR_SLICE_CENTER;
      else if (grub_strcmp (value, "east") == 0)
        self->scrollbar_slice = SCROLLBAR_SLICE_EAST;
    }
  else if (grub_strcmp (name, "scrollbar_left_pad") == 0)
    {
      self->scrollbar_left_pad = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "scrollbar_right_pad") == 0)
    {
      self->scrollbar_right_pad = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "scrollbar_top_pad") == 0)
    {
      self->scrollbar_top_pad = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "scrollbar_bottom_pad") == 0)
    {
      self->scrollbar_bottom_pad = grub_strtol (value, 0, 10);
    }
  else if (grub_strcmp (name, "scrollbar") == 0)
    {
      self->draw_scrollbar = grub_strcmp (value, "false") != 0;
    }
  else if (grub_strcmp (name, "theme_dir") == 0)
    {
      self->need_to_recreate_boxes = 1;
      grub_free (self->theme_dir);
      self->theme_dir = value ? grub_strdup (value) : 0;
    }
  else if (grub_strcmp (name, "id") == 0)
    {
      grub_free (self->id);
      if (value)
        self->id = grub_strdup (value);
      else
        self->id = 0;
    }
  return grub_errno;
}

/* Set necessary information that the gfxmenu view provides.  */
static void
list_set_view_info (void *vself,
                    grub_gfxmenu_view_t view)
{
  list_impl_t self = vself;
  grub_gfxmenu_icon_manager_set_theme_path (self->icon_manager,
					    view->theme_path);
  self->view = view;
}

/* Refresh list variables */
static void
list_refresh_info (void *vself,
                   grub_gfxmenu_view_t view)
{
  list_impl_t self = vself;
  if (view->nested)
    self->first_shown_index = 0;
}

static struct grub_gui_component_ops list_comp_ops =
  {
    .destroy = list_destroy,
    .get_id = list_get_id,
    .is_instance = list_is_instance,
    .paint = list_paint,
    .set_parent = list_set_parent,
    .get_parent = list_get_parent,
    .set_bounds = list_set_bounds,
    .get_bounds = list_get_bounds,
    .get_minimal_size = list_get_minimal_size,
    .set_property = list_set_property
  };

static struct grub_gui_list_ops list_ops =
{
  .set_view_info = list_set_view_info,
  .refresh_list = list_refresh_info
};

grub_gui_component_t
grub_gui_list_new (void)
{
  list_impl_t self;
  grub_font_t default_font;
  grub_video_rgba_color_t default_fg_color;

  self = grub_zalloc (sizeof (*self));
  if (! self)
    return 0;

  self->list.ops = &list_ops;
  self->list.component.ops = &list_comp_ops;

  self->visible = 1;

  default_font = grub_font_get ("Unknown Regular 16");
  default_fg_color = grub_video_rgba_color_rgb (0, 0, 0);

  self->icon_width = 32;
  self->icon_height = 32;
  self->item_height = 42;
  self->item_padding = 14;
  self->item_icon_space = 4;
  self->item_spacing = 16;
  self->item_font = default_font;
  self->selected_item_font_inherit = 1; /* Default to using the item_font.  */
  self->selected_item_font = default_font;
  self->item_color = default_fg_color;
  self->selected_item_color_inherit = 1;  /* Default to using the item_color.  */
  self->selected_item_color = default_fg_color;

  self->draw_scrollbar = 1;
  self->need_to_recreate_scrollbar = 1;
  self->scrollbar_frame = 0;
  self->scrollbar_thumb = 0;
  self->scrollbar_frame_pattern = 0;
  self->scrollbar_thumb_pattern = 0;
  self->scrollbar_thumb_overlay = 0;
  self->scrollbar_width = 16;
  self->scrollbar_slice = SCROLLBAR_SLICE_EAST;
  self->scrollbar_left_pad = 2;
  self->scrollbar_right_pad = 0;
  self->scrollbar_top_pad = 0;
  self->scrollbar_bottom_pad = 0;

  self->first_shown_index = 0;

  self->need_to_recreate_boxes = 0;
  self->theme_dir = 0;
  self->menu_box_pattern = 0;
  self->item_box_pattern = 0;
  self->selected_item_box_pattern_inherit = 1;/*Default to using the item_box.*/
  self->selected_item_box_pattern = 0;
  self->menu_box = grub_gfxmenu_create_box (0, 0);
  self->item_box = grub_gfxmenu_create_box (0, 0);
  self->selected_item_box = grub_gfxmenu_create_box (0, 0);

  self->icon_manager = grub_gfxmenu_icon_manager_new ();
  if (! self->icon_manager)
    {
      self->list.component.ops->destroy (self);
      return 0;
    }
  grub_gfxmenu_icon_manager_set_icon_size (self->icon_manager,
                                           self->icon_width,
                                           self->icon_height);
  return (grub_gui_component_t) self;
}
