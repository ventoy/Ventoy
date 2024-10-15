/* gui_label.c - GUI component to display a line of text.  */
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
#include <grub/font.h>
#include <grub/gui_string_util.h>
#include <grub/i18n.h>
#include <grub/color.h>
#include <grub/env.h>

extern int g_ventoy_memdisk_mode;
extern int g_ventoy_iso_raw;
extern int g_ventoy_grub2_mode;
extern int g_ventoy_wimboot_mode;
extern int g_ventoy_iso_uefi_drv;
extern char g_ventoy_hotkey_tip[256];

static const char *align_options[] =
{
  "left",
  "center",
  "right",
  0
};

enum align_mode {
  align_left,
  align_center,
  align_right
};

struct grub_gui_label
{
  struct grub_gui_component comp;

  grub_gui_container_t parent;
  grub_video_rect_t bounds;
  char *id;
  int visible;
  char *text;
  char *template;
  grub_font_t font;
  grub_video_rgba_color_t color;
  int value;
  int vtoytip;
  enum align_mode align;
};

typedef struct grub_gui_label *grub_gui_label_t;

extern const char * g_ventoy_tip_msg1;
extern const char * g_ventoy_tip_msg2;

static void
label_destroy (void *vself)
{
  grub_gui_label_t self = vself;
  grub_gfxmenu_timeout_unregister ((grub_gui_component_t) self);
  grub_free (self->text);
  grub_free (self->template);
  grub_free (self);
}

static const char *
label_get_id (void *vself)
{
  grub_gui_label_t self = vself;
  return self->id;
}

static int
label_is_instance (void *vself __attribute__((unused)), const char *type)
{
  return grub_strcmp (type, "component") == 0;
}

static void
label_paint (void *vself, const grub_video_rect_t *region)
{
  const char *text;
  grub_gui_label_t self = vself;

  if (! self->visible)
    return;

  if (!grub_video_have_common_points (region, &self->bounds))
    return;

  if (self->vtoytip == 1) {
     text = g_ventoy_tip_msg1 ? g_ventoy_tip_msg1 : "";
  } else if (self->vtoytip == 2) {
     text = g_ventoy_tip_msg2 ? g_ventoy_tip_msg2 : "";
  } else {
     text = self->text;
  }

  /* Calculate the starting x coordinate.  */
  int left_x;
  if (self->align == align_left)
    left_x = 0;
  else if (self->align == align_center)
    left_x = (self->bounds.width
	      - grub_font_get_string_width (self->font, text)) / 2;
  else if (self->align == align_right)
    left_x = (self->bounds.width
              - grub_font_get_string_width (self->font, text));
  else
    return;   /* Invalid alignment.  */

  if (left_x < 0 || left_x > (int) self->bounds.width)
    left_x = 0;

  grub_video_rect_t vpsave;
  grub_gui_set_viewport (&self->bounds, &vpsave);
  grub_font_draw_string (text,
                         self->font,
                         grub_video_map_rgba_color (self->color),
                         left_x,
                         grub_font_get_ascent (self->font));
  grub_gui_restore_viewport (&vpsave);
}

static void
label_set_parent (void *vself, grub_gui_container_t parent)
{
  grub_gui_label_t self = vself;
  self->parent = parent;
}

static grub_gui_container_t
label_get_parent (void *vself)
{
  grub_gui_label_t self = vself;
  return self->parent;
}

static void
label_set_bounds (void *vself, const grub_video_rect_t *bounds)
{
  grub_gui_label_t self = vself;
  self->bounds = *bounds;
}

static void
label_get_bounds (void *vself, grub_video_rect_t *bounds)
{
  grub_gui_label_t self = vself;
  *bounds = self->bounds;
}

static void
label_get_minimal_size (void *vself, unsigned *width, unsigned *height)
{
  grub_gui_label_t self = vself;
   *width = grub_font_get_string_width (self->font, self->text);
   *height = (grub_font_get_ascent (self->font)
             + grub_font_get_descent (self->font));
}

#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static void
label_set_state (void *vself, int visible, int start __attribute__ ((unused)),
		 int current, int end __attribute__ ((unused)))
{
  grub_gui_label_t self = vself;  
  self->value = -current;
  self->visible = visible;
  grub_free (self->text);
  self->text = grub_xasprintf (self->template ? : "%d", self->value);
}

static grub_err_t
label_set_property (void *vself, const char *name, const char *value)
{
  grub_gui_label_t self = vself;
  if (grub_strcmp (name, "text") == 0)
    {
      grub_free (self->text);
      grub_free (self->template);
      if (! value)
	{
	  self->template = NULL;
	  self->text = grub_strdup ("");
	}
      else
	{
	   if (grub_strcmp (value, "@KEYMAP_LONG@") == 0)
	    value = _("Press enter to boot the selected OS, "
	       "`e' to edit the commands before booting "
	       "or `c' for a command-line. ESC to return previous menu.");
           else if (grub_strcmp (value, "@KEYMAP_MIDDLE@") == 0)
	    value = _("Press enter to boot the selected OS, "
	       "`e' to edit the commands before booting "
	       "or `c' for a command-line.");
	   else if (grub_strcmp (value, "@KEYMAP_SHORT@") == 0)
	    value = _("enter: boot, `e': options, `c': cmd-line");
	   /* FIXME: Add more templates here if needed.  */
       
       else if (grub_strcmp (value, "@VTOY_MEM_DISK@") == 0) {
            value = g_ventoy_memdisk_mode ? grub_env_get("VTOY_MEM_DISK_STR") : " ";
       }
       else if (grub_strcmp (value, "@VTOY_ISO_RAW@") == 0) {
            value = g_ventoy_iso_raw ? grub_env_get("VTOY_ISO_RAW_STR") : " ";
       }
       else if (grub_strcmp (value, "@VTOY_GRUB2_MODE@") == 0) {
            value = g_ventoy_grub2_mode ? grub_env_get("VTOY_GRUB2_MODE_STR") : " ";
       }
       else if (grub_strcmp (value, "@VTOY_WIMBOOT_MODE@") == 0) {
            value = g_ventoy_wimboot_mode ? grub_env_get("VTOY_WIMBOOT_MODE_STR") : " ";
       }
       else if (grub_strcmp (value, "@VTOY_ISO_UEFI_DRV@") == 0) {
            value = g_ventoy_iso_uefi_drv ? grub_env_get("VTOY_ISO_UEFI_DRV_STR") : " ";
       }
       else if (grub_strcmp (value, "@VTOY_HOTKEY_TIP@") == 0) {
            value = g_ventoy_hotkey_tip;
       } else if (value[0] == '@' && value[1] == '@' && value[2]) {
            value = grub_env_get(value + 2);
            if (!value)
                value = " ";
       }
       
	  self->template = grub_strdup (value);
	  self->text = grub_xasprintf (value, self->value);
	}
    }
  else if (grub_strcmp (name, "font") == 0)
    {
      self->font = grub_font_get (value);
    }
  else if (grub_strcmp (name, "color") == 0)
    {
      grub_video_parse_color (value, &self->color);
    }
  else if (grub_strcmp (name, "align") == 0)
    {
      int i;
      for (i = 0; align_options[i]; i++)
        {
          if (grub_strcmp (align_options[i], value) == 0)
            {
              self->align = i;   /* Set the alignment mode.  */
              break;
            }
        }
    }
  else if (grub_strcmp (name, "visible") == 0)
    {
      self->visible = grub_strcmp (value, "false") != 0;
    }
  else if (grub_strcmp (name, "id") == 0)
    {
      grub_gfxmenu_timeout_unregister ((grub_gui_component_t) self);
      grub_free (self->id);
      if (value) {
        self->id = grub_strdup (value);
        if (grub_strcmp(value, "VTOY_MENU_TIP_1") == 0) {
            self->vtoytip = 1;
        } else if (grub_strcmp(value, "VTOY_MENU_TIP_2") == 0) {
            self->vtoytip = 2;
        }
      }
      else
        self->id = 0;
      if (self->id && grub_strcmp (self->id, GRUB_GFXMENU_TIMEOUT_COMPONENT_ID)
	  == 0)
	grub_gfxmenu_timeout_register ((grub_gui_component_t) self,
				       label_set_state);
    }
  return GRUB_ERR_NONE;
}

#pragma GCC diagnostic error "-Wformat-nonliteral"

static struct grub_gui_component_ops label_ops =
{
  .destroy = label_destroy,
  .get_id = label_get_id,
  .is_instance = label_is_instance,
  .paint = label_paint,
  .set_parent = label_set_parent,
  .get_parent = label_get_parent,
  .set_bounds = label_set_bounds,
  .get_bounds = label_get_bounds,
  .get_minimal_size = label_get_minimal_size,
  .set_property = label_set_property
};

grub_gui_component_t
grub_gui_label_new (void)
{
  grub_gui_label_t label;
  label = grub_zalloc (sizeof (*label));
  if (! label)
    return 0;
  label->comp.ops = &label_ops;
  label->visible = 1;
  label->text = grub_strdup ("");
  label->font = grub_font_get ("Unknown Regular 16");
  label->color.red = 0;
  label->color.green = 0;
  label->color.blue = 0;
  label->color.alpha = 255;
  label->align = align_left;
  return (grub_gui_component_t) label;
}
