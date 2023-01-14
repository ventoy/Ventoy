/* theme_loader.c - Theme file loader for gfxmenu.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008  Free Software Foundation, Inc.
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

#include <grub/types.h>
#include <grub/file.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/video.h>
#include <grub/gui_string_util.h>
#include <grub/bitmap.h>
#include <grub/bitmap_scale.h>
#include <grub/gfxwidgets.h>
#include <grub/gfxmenu_view.h>
#include <grub/gui.h>
#include <grub/color.h>
#include <grub/env.h>

static grub_err_t
parse_proportional_spec (const char *value, signed *abs, grub_fixed_signed_t *prop);

/* Construct a new box widget using ABSPATTERN to find the pixmap files for
   it, storing the new box instance at *BOXPTR.
   PATTERN should be of the form: "(hd0,0)/somewhere/style*.png".
   The '*' then gets substituted with the various pixmap names that the
   box uses.  */
static grub_err_t
recreate_box_absolute (grub_gfxmenu_box_t *boxptr, const char *abspattern)
{
  char *prefix;
  char *suffix;
  char *star;
  grub_gfxmenu_box_t box;

  star = grub_strchr (abspattern, '*');
  if (! star)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
                       "missing `*' in box pixmap pattern `%s'", abspattern);

  /* Prefix:  Get the part before the '*'.  */
  prefix = grub_malloc (star - abspattern + 1);
  if (! prefix)
    return grub_errno;

  grub_memcpy (prefix, abspattern, star - abspattern);
  prefix[star - abspattern] = '\0';

  /* Suffix:  Everything after the '*' is the suffix.  */
  suffix = star + 1;

  box = grub_gfxmenu_create_box (prefix, suffix);
  grub_free (prefix);
  if (! box)
    return grub_errno;

  if (*boxptr)
    (*boxptr)->destroy (*boxptr);
  *boxptr = box;
  return grub_errno;
}


/* Construct a new box widget using PATTERN to find the pixmap files for it,
   storing the new widget at *BOXPTR.  PATTERN should be of the form:
   "somewhere/style*.png".  The '*' then gets substituted with the various
   pixmap names that the widget uses.

   Important!  The value of *BOXPTR must be initialized!  It must either
   (1) Be 0 (a NULL pointer), or
   (2) Be a pointer to a valid 'grub_gfxmenu_box_t' instance.
   In this case, the previous instance is destroyed.  */
grub_err_t
grub_gui_recreate_box (grub_gfxmenu_box_t *boxptr,
                       const char *pattern, const char *theme_dir)
{
  char *abspattern;

  /* Check arguments.  */
  if (! pattern)
    {
      /* If no pixmap pattern is given, then just create an empty box.  */
      if (*boxptr)
        (*boxptr)->destroy (*boxptr);
      *boxptr = grub_gfxmenu_create_box (0, 0);
      return grub_errno;
    }

  if (! theme_dir)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
                       "styled box missing theme directory");

  /* Resolve to an absolute path.  */
  abspattern = grub_resolve_relative_path (theme_dir, pattern);
  if (! abspattern)
    return grub_errno;

  /* Create the box.  */
  recreate_box_absolute (boxptr, abspattern);
  grub_free (abspattern);
  return grub_errno;
}

static grub_err_t
theme_get_unsigned_int_from_proportional (const char *value,
                                          unsigned absolute_value,
                                          unsigned int *parsed_value)
{
  grub_err_t err;
  grub_fixed_signed_t frac;
  signed pixels;
  err = parse_proportional_spec (value, &pixels, &frac);
  if (err != GRUB_ERR_NONE)
    return err;
  int result = grub_fixed_sfs_multiply (absolute_value, frac) + pixels;
  if (result < 0)
    result = 0;
  *parsed_value = result;
  return GRUB_ERR_NONE;
}

/* Set the specified property NAME on the view to the given string VALUE.
   The caller is responsible for the lifetimes of NAME and VALUE.  */
static grub_err_t
theme_set_string (grub_gfxmenu_view_t view,
                  const char *name,
                  const char *value,
                  const char *theme_dir,
                  const char *filename,
                  int line_num,
                  int col_num)
{
  if (! grub_strcmp ("title-font", name))
    view->title_font = grub_font_get (value);
  else if (! grub_strcmp ("message-font", name))
    view->message_font = grub_font_get (value);
  else if (! grub_strcmp ("terminal-font", name))
    {
      grub_free (view->terminal_font_name);
      view->terminal_font_name = grub_strdup (value);
      if (! view->terminal_font_name)
        return grub_errno;
    }
  else if (! grub_strcmp ("title-color", name))
    grub_video_parse_color (value, &view->title_color);
  else if (! grub_strcmp ("message-color", name))
    grub_video_parse_color (value, &view->message_color);
  else if (! grub_strcmp ("message-bg-color", name))
    grub_video_parse_color (value, &view->message_bg_color);
  else if (! grub_strcmp("menu-tip-left", name))
    grub_env_set("VTOY_TIP_LEFT", value);
  else if (! grub_strcmp("menu-tip-top", name))
    grub_env_set("VTOY_TIP_TOP", value);
  else if (! grub_strcmp("menu-tip-color", name))
    grub_env_set("VTOY_TIP_COLOR", value);
  else if (! grub_strcmp ("desktop-image", name))
    {
      struct grub_video_bitmap *raw_bitmap;
      char *path;
      path = grub_resolve_relative_path (theme_dir, value);
      if (! path)
        return grub_errno;
      if (grub_video_bitmap_load (&raw_bitmap, path) != GRUB_ERR_NONE)
        {
          grub_free (path);
          return grub_errno;
        }
      grub_free(path);
      grub_video_bitmap_destroy (view->raw_desktop_image);
      view->raw_desktop_image = raw_bitmap;
    }
  else if (! grub_strcmp ("desktop-image-scale-method", name))
    {
      if (! value || ! grub_strcmp ("stretch", value))
        view->desktop_image_scale_method =
            GRUB_VIDEO_BITMAP_SELECTION_METHOD_STRETCH;
      else if (! grub_strcmp ("crop", value))
        view->desktop_image_scale_method =
            GRUB_VIDEO_BITMAP_SELECTION_METHOD_CROP;
      else if (! grub_strcmp ("padding", value))
        view->desktop_image_scale_method =
            GRUB_VIDEO_BITMAP_SELECTION_METHOD_PADDING;
      else if (! grub_strcmp ("fitwidth", value))
        view->desktop_image_scale_method =
            GRUB_VIDEO_BITMAP_SELECTION_METHOD_FITWIDTH;
      else if (! grub_strcmp ("fitheight", value))
        view->desktop_image_scale_method =
            GRUB_VIDEO_BITMAP_SELECTION_METHOD_FITHEIGHT;
      else
        return grub_error (GRUB_ERR_BAD_ARGUMENT,
                           "Unsupported scale method: %s",
                           value);
    }
  else if (! grub_strcmp ("desktop-image-h-align", name))
    {
      if (! grub_strcmp ("left", value))
        view->desktop_image_h_align = GRUB_VIDEO_BITMAP_H_ALIGN_LEFT;
      else if (! grub_strcmp ("center", value))
        view->desktop_image_h_align = GRUB_VIDEO_BITMAP_H_ALIGN_CENTER;
      else if (! grub_strcmp ("right", value))
        view->desktop_image_h_align = GRUB_VIDEO_BITMAP_H_ALIGN_RIGHT;
      else
        return grub_error (GRUB_ERR_BAD_ARGUMENT,
                           "Unsupported horizontal align method: %s",
                           value);
    }
  else if (! grub_strcmp ("desktop-image-v-align", name))
    {
      if (! grub_strcmp ("top", value))
        view->desktop_image_v_align = GRUB_VIDEO_BITMAP_V_ALIGN_TOP;
      else if (! grub_strcmp ("center", value))
        view->desktop_image_v_align = GRUB_VIDEO_BITMAP_V_ALIGN_CENTER;
      else if (! grub_strcmp ("bottom", value))
        view->desktop_image_v_align = GRUB_VIDEO_BITMAP_V_ALIGN_BOTTOM;
      else
        return grub_error (GRUB_ERR_BAD_ARGUMENT,
                           "Unsupported vertical align method: %s",
                           value);
    }
  else if (! grub_strcmp ("desktop-color", name))
     grub_video_parse_color (value, &view->desktop_color);
  else if (! grub_strcmp ("terminal-box", name))
    {
        grub_err_t err;
        err = grub_gui_recreate_box (&view->terminal_box, value, theme_dir);
        if (err != GRUB_ERR_NONE)
          return err;
    }
  else if (! grub_strcmp ("terminal-border", name))
    {
      view->terminal_border = grub_strtoul (value, 0, 10);
      if (grub_errno)
        return grub_errno;
    }
  else if (! grub_strcmp ("terminal-left", name))
    {
      unsigned int tmp;
      int err = theme_get_unsigned_int_from_proportional (value,
                                                          view->screen.width,
                                                          &tmp);
      if (err != GRUB_ERR_NONE)
        return err;
      view->terminal_rect.x = tmp;
    }
  else if (! grub_strcmp ("terminal-top", name))
    {
      unsigned int tmp;
      int err = theme_get_unsigned_int_from_proportional (value,
                                                          view->screen.height,
                                                          &tmp);
      if (err != GRUB_ERR_NONE)
        return err;
      view->terminal_rect.y = tmp;
    }
  else if (! grub_strcmp ("terminal-width", name))
    {
      unsigned int tmp;
      int err = theme_get_unsigned_int_from_proportional (value,
                                                          view->screen.width,
                                                          &tmp);
      if (err != GRUB_ERR_NONE)
        return err;
      view->terminal_rect.width = tmp;
    }
  else if (! grub_strcmp ("terminal-height", name))
    {
      unsigned int tmp;
      int err = theme_get_unsigned_int_from_proportional (value,
                                                          view->screen.height,
                                                          &tmp);
      if (err != GRUB_ERR_NONE)
        return err;
      view->terminal_rect.height = tmp;
    }
  else if (! grub_strcmp ("title-text", name))
    {
      grub_free (view->title_text);
      view->title_text = grub_strdup (value);
      if (! view->title_text)
        return grub_errno;
    }
  else if (! grub_strcmp ("ventoy_left_top_color", name))
      return grub_errno;
  else
    {
      return grub_error (GRUB_ERR_BAD_ARGUMENT,
                         "%s:%d:%d unknown property `%s'",
                         filename, line_num, col_num, name);
    }
  return grub_errno;
}

struct parsebuf
{
  char *buf;
  int pos;
  int len;
  int line_num;
  int col_num;
  const char *filename;
  char *theme_dir;
  grub_gfxmenu_view_t view;
};

static int
has_more (struct parsebuf *p)
{
  return p->pos < p->len;
}

static int
read_char (struct parsebuf *p)
{
  if (has_more (p))
    {
      char c;
      c = p->buf[p->pos++];
      if (c == '\n')
        {
          p->line_num++;
          p->col_num = 1;
        }
      else
        {
          p->col_num++;
        }
      return c;
    }
  else
    return -1;
}

static int
peek_char (struct parsebuf *p)
{
  if (has_more (p))
    return p->buf[p->pos];
  else
    return -1;
}

static int
is_whitespace (char c)
{
  return (c == ' '
          || c == '\t'
          || c == '\r'
          || c == '\n'
          || c == '\f');
}

static void
skip_whitespace (struct parsebuf *p)
{
  while (has_more (p) && is_whitespace(peek_char (p)))
    read_char (p);
}

static void
advance_to_next_line (struct parsebuf *p)
{
  int c;

  /* Eat characters up to the newline.  */
  do
    {
      c = read_char (p);
    }
  while (c != -1 && c != '\n');
}

static int
is_identifier_char (int c)
{
  return (c != -1
          && (grub_isalpha(c)
              || grub_isdigit(c)
              || c == '_'
              || c == '-'));
}

static char *
read_identifier (struct parsebuf *p)
{
  /* Index of the first character of the identifier in p->buf.  */
  int start;
  /* Next index after the last character of the identifer in p->buf.  */
  int end;

  skip_whitespace (p);

  /* Capture the start of the identifier.  */
  start = p->pos;

  /* Scan for the end.  */
  while (is_identifier_char (peek_char (p)))
    read_char (p);
  end = p->pos;

  if (end - start < 1)
    return 0;

  return grub_new_substring (p->buf, start, end);
}

static char *
read_expression (struct parsebuf *p)
{
  int start;
  int end;

  skip_whitespace (p);
  if (peek_char (p) == '"')
    {
      /* Read as a quoted string.  
         The quotation marks are not included in the expression value.  */
      /* Skip opening quotation mark.  */
      read_char (p);
      start = p->pos;
      while (has_more (p) && peek_char (p) != '"')
        read_char (p);
      end = p->pos;
      /* Skip the terminating quotation mark.  */
      read_char (p);
    }
  else if (peek_char (p) == '(')
    {
      /* Read as a parenthesized string -- for tuples/coordinates.  */
      /* The parentheses are included in the expression value.  */
      int c;

      start = p->pos;
      do
        {
          c = read_char (p);
        }
      while (c != -1 && c != ')');
      end = p->pos;
    }
  else if (has_more (p))
    {
      /* Read as a single word -- for numeric values or words without
         whitespace.  */
      start = p->pos;
      while (has_more (p) && ! is_whitespace (peek_char (p))
             && peek_char (p) != '}')
        read_char (p);
      end = p->pos;
    }
  else
    {
      /* The end of the theme file has been reached.  */
      grub_error (GRUB_ERR_IO, "%s:%d:%d expression expected in theme file",
                  p->filename, p->line_num, p->col_num);
      return 0;
    }

  return grub_new_substring (p->buf, start, end);
}

static grub_err_t
parse_proportional_spec (const char *value, signed *abs, grub_fixed_signed_t *prop)
{
  signed num;
  const char *ptr;
  int sig = 0;
  *abs = 0;
  *prop = 0;
  ptr = value;
  while (*ptr)
    {
      sig = 0;

      while (*ptr == '-' || *ptr == '+')
	{
	  if (*ptr == '-')
	    sig = !sig;
	  ptr++;
	}

      num = grub_strtoul (ptr, (char **) &ptr, 0);
      if (grub_errno)
	return grub_errno;
      if (sig)
	num = -num;
      if (*ptr == '%')
	{
	  *prop += grub_fixed_fsf_divide (grub_signed_to_fixed (num), 100);
	  ptr++;
	}
      else
	*abs += num;
    }
  return GRUB_ERR_NONE;
}


/* Read a GUI object specification from the theme file.
   Any components created will be added to the GUI container PARENT.  */
static grub_err_t
read_object (struct parsebuf *p, grub_gui_container_t parent)
{
  grub_video_rect_t bounds;

  char *name;
  name = read_identifier (p);
  if (! name)
    goto cleanup;

  grub_gui_component_t component = 0;
  if (grub_strcmp (name, "label") == 0)
    {
      component = grub_gui_label_new ();
    }
  else if (grub_strcmp (name, "image") == 0)
    {
      component = grub_gui_image_new ();
    }
  else if (grub_strcmp (name, "vbox") == 0)
    {
      component = (grub_gui_component_t) grub_gui_vbox_new ();
    }
  else if (grub_strcmp (name, "hbox") == 0)
    {
      component = (grub_gui_component_t) grub_gui_hbox_new ();
    }
  else if (grub_strcmp (name, "canvas") == 0)
    {
      component = (grub_gui_component_t) grub_gui_canvas_new ();
    }
  else if (grub_strcmp (name, "progress_bar") == 0)
    {
      component = grub_gui_progress_bar_new ();
    }
  else if (grub_strcmp (name, "circular_progress") == 0)
    {
      component = grub_gui_circular_progress_new ();
    }
  else if (grub_strcmp (name, "boot_menu") == 0)
    {
      component = grub_gui_list_new ();
    }
  else
    {
      /* Unknown type.  */
      grub_error (GRUB_ERR_IO, "%s:%d:%d unknown object type `%s'",
                  p->filename, p->line_num, p->col_num, name);
      goto cleanup;
    }

  if (! component)
    goto cleanup;

  /* Inform the component about the theme so it can find its resources.  */
  component->ops->set_property (component, "theme_dir", p->theme_dir);
  component->ops->set_property (component, "theme_path", p->filename);

  /* Add the component as a child of PARENT.  */
  bounds.x = 0;
  bounds.y = 0;
  bounds.width = -1;
  bounds.height = -1;
  component->ops->set_bounds (component, &bounds);
  parent->ops->add (parent, component);

  skip_whitespace (p);
  if (read_char (p) != '{')
    {
      grub_error (GRUB_ERR_IO,
                  "%s:%d:%d expected `{' after object type name `%s'",
                  p->filename, p->line_num, p->col_num, name);
      goto cleanup;
    }

  while (has_more (p))
    {
      skip_whitespace (p);

      /* Check whether the end has been encountered.  */
      if (peek_char (p) == '}')
        {
          /* Skip the closing brace.  */
          read_char (p);
          break;
        }

      if (peek_char (p) == '#')
        {
          /* Skip comments.  */
          advance_to_next_line (p);
          continue;
        }

      if (peek_char (p) == '+')
        {
          /* Skip the '+'.  */
          read_char (p);

          /* Check whether this component is a container.  */
          if (component->ops->is_instance (component, "container"))
            {
              /* Read the sub-object recursively and add it as a child.  */
              if (read_object (p, (grub_gui_container_t) component) != 0)
                goto cleanup;
              /* After reading the sub-object, resume parsing, expecting
                 another property assignment or sub-object definition.  */
              continue;
            }
          else
            {
              grub_error (GRUB_ERR_IO,
                          "%s:%d:%d attempted to add object to non-container",
                          p->filename, p->line_num, p->col_num);
              goto cleanup;
            }
        }

      char *property;
      property = read_identifier (p);
      if (! property)
        {
          grub_error (GRUB_ERR_IO, "%s:%d:%d identifier expected in theme file",
                      p->filename, p->line_num, p->col_num);
          goto cleanup;
        }

      skip_whitespace (p);
      if (read_char (p) != '=')
        {
          grub_error (GRUB_ERR_IO,
                      "%s:%d:%d expected `=' after property name `%s'",
                      p->filename, p->line_num, p->col_num, property);
          grub_free (property);
          goto cleanup;
        }
      skip_whitespace (p);

      char *value;
      value = read_expression (p);
      if (! value)
        {
          grub_free (property);
          goto cleanup;
        }

      /* Handle the property value.  */
      if (grub_strcmp (property, "left") == 0)
	parse_proportional_spec (value, &component->x, &component->xfrac);
      else if (grub_strcmp (property, "top") == 0)
	parse_proportional_spec (value, &component->y, &component->yfrac);
      else if (grub_strcmp (property, "width") == 0)
	parse_proportional_spec (value, &component->w, &component->wfrac);
      else if (grub_strcmp (property, "height") == 0)
	parse_proportional_spec (value, &component->h, &component->hfrac);
      else
	/* General property handling.  */
	component->ops->set_property (component, property, value);

      grub_free (value);
      grub_free (property);
      if (grub_errno != GRUB_ERR_NONE)
        goto cleanup;
    }

cleanup:
  grub_free (name);
  return grub_errno;
}

static grub_err_t
read_property (struct parsebuf *p)
{
  char *name;

  /* Read the property name.  */
  name = read_identifier (p);
  if (! name)
    {
      advance_to_next_line (p);
      return grub_errno;
    }

  /* Skip whitespace before separator.  */
  skip_whitespace (p);

  /* Read separator.  */
  if (read_char (p) != ':')
    {
      grub_error (GRUB_ERR_IO,
                  "%s:%d:%d missing separator after property name `%s'",
                  p->filename, p->line_num, p->col_num, name);
      goto done;
    }

  /* Skip whitespace after separator.  */
  skip_whitespace (p);

  /* Get the value based on its type.  */
  if (peek_char (p) == '"')
    {
      /* String value (e.g., '"My string"').  */
      char *value = read_expression (p);
      if (! value)
        {
          grub_error (GRUB_ERR_IO, "%s:%d:%d missing property value",
                      p->filename, p->line_num, p->col_num);
          goto done;
        }
      /* If theme_set_string results in an error, grub_errno will be returned
         below.  */
      theme_set_string (p->view, name, value, p->theme_dir,
                        p->filename, p->line_num, p->col_num);
      grub_free (value);
    }
  else
    {
      grub_error (GRUB_ERR_IO,
                  "%s:%d:%d property value invalid; "
                  "enclose literal values in quotes (\")",
                  p->filename, p->line_num, p->col_num);
      
      grub_printf("File: %s\nLine:%d Column:%d\n"
                  "property value invalid; enclose literal values in quotes (\")\n\n",
                  p->filename, p->line_num, p->col_num);

      goto done;
    }

done:
  grub_free (name);
  return grub_errno;
}

extern int g_menu_update_mode;

/* Set properties on the view based on settings from the specified
   theme file.  */
grub_err_t
grub_gfxmenu_view_load_theme (grub_gfxmenu_view_t view, const char *theme_path)
{
  int flag = 0;
  grub_file_t file;
  struct parsebuf p;

  p.view = view;
  p.theme_dir = grub_get_dirname (theme_path);

  file = grub_file_open (theme_path, GRUB_FILE_TYPE_THEME);
  if (! file)
    {
      grub_free (p.theme_dir);
      return grub_errno;
    }

  p.len = grub_file_size (file);
  p.buf = grub_malloc (p.len + 8192);
  p.pos = 0;
  p.line_num = 1;
  p.col_num = 1;
  p.filename = theme_path;
  if (! p.buf)
    {
      grub_file_close (file);
      grub_free (p.theme_dir);
      return grub_errno;
    }
  if (grub_file_read (file, p.buf, p.len) != p.len)
    {
      grub_free (p.buf);
      grub_file_close (file);
      grub_free (p.theme_dir);
      return grub_errno;
    }

{
  const char *checkret = grub_env_get("VTOY_CHKDEV_RESULT_STRING");
  if (checkret == NULL || checkret[0] != '0')
  {
    p.len += grub_snprintf(p.buf + p.len, 4096, "\n+ hbox{\n    left = 1%%\n    top = 90%%\n"
      "    + label {text = \"[Unofficial Ventoy]\" color = \"red\" align = \"left\"}\n"
      "}\n");    
  }
}



  if (view->canvas)
    view->canvas->component.ops->destroy (view->canvas);

  view->canvas = grub_gui_canvas_new ();
  if (!view->canvas)
    goto fail;
  ((grub_gui_component_t) view->canvas)
    ->ops->set_bounds ((grub_gui_component_t) view->canvas,
                       &view->screen);

parse:    
  while (has_more (&p))
    {
      /* Skip comments (lines beginning with #).  */
      if (peek_char (&p) == '#')
        {
          advance_to_next_line (&p);
          continue;
        }

      /* Find the first non-whitespace character.  */
      skip_whitespace (&p);

      /* Handle the content.  */
      if (peek_char (&p) == '+')
        {
          /* Skip the '+'.  */
          read_char (&p);
          read_object (&p, view->canvas);
        }
      else
        {
          read_property (&p);
        }

      if (grub_errno != GRUB_ERR_NONE)
        goto fail;
    }


if (flag == 0)
{
    const char *tip = grub_env_get("VTOY_MENU_TIP_ENABLE");
    if (tip && tip[0] == '1')
    {
        char tmpmsg[512];

        grub_memset(tmpmsg, 'w', 500);
        tmpmsg[500] = 0;
        
        g_menu_update_mode = 1;
        p.len += grub_snprintf(p.buf + p.len, 4096, 
            "\n+ vbox{\n    left = %s\n    top = %s\n"
            "+ label { id=\"VTOY_MENU_TIP_1\" text = \"%s\" color = \"%s\" align = \"%s\"}\n"
            "+ label { id=\"VTOY_MENU_TIP_2\" text = \"%s\" color = \"%s\" align = \"%s\"}\n"
            "}\n",
            grub_env_get("VTOY_TIP_LEFT"),
            grub_env_get("VTOY_TIP_TOP"),
            tmpmsg,
            grub_env_get("VTOY_TIP_COLOR"),
            grub_env_get("VTOY_TIP_ALIGN"),
            tmpmsg,
            grub_env_get("VTOY_TIP_COLOR"),
            grub_env_get("VTOY_TIP_ALIGN")
        );

        flag = 1;
        goto parse;
    }
}



  /* Set the new theme path.  */
  grub_free (view->theme_path);
  view->theme_path = grub_strdup (theme_path);
  goto cleanup;

fail:
  if (view->canvas)
    {
      view->canvas->component.ops->destroy (view->canvas);
      view->canvas = 0;
    }

cleanup:
  grub_free (p.buf);
  grub_file_close (file);
  grub_free (p.theme_dir);
  return grub_errno;
}
