/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2003,2005,2007,2008,2009  Free Software Foundation, Inc.
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

#include <grub/term.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/file.h>
#include <grub/dl.h>
#include <grub/env.h>
#include <grub/normal.h>
#include <grub/charset.h>
#include <grub/i18n.h>
#include <grub/crypto.h>

struct term_state
{
  struct term_state *next;
  struct grub_unicode_glyph *backlog_glyphs;
  const grub_uint32_t *backlog_ucs4;
  int backlog_fixed_tab;
  grub_size_t backlog_len;

  int bidi_stack_depth;
  grub_uint8_t bidi_stack[GRUB_BIDI_MAX_EXPLICIT_LEVEL];
  int invalid_pushes;

  void *free;
  int num_lines;
  char *term_name;
};

static int
print_ucs4_real (const grub_uint32_t * str,
		 const grub_uint32_t * last_position,
		 int margin_left, int margin_right,
		 struct grub_term_output *term, int backlog,
		 int dry_run, int fixed_tab, unsigned skip_lines,
		 unsigned max_lines,
		 grub_uint32_t contchar, int fill_right,
		 struct grub_term_pos *pos);

static struct term_state *term_states = NULL;

/* If the more pager is active.  */
static int grub_more;

static void
putcode_real (grub_uint32_t code, struct grub_term_output *term, int fixed_tab);

void
grub_normal_reset_more (void)
{
  static struct term_state *state;
  for (state = term_states; state; state = state->next)
    state->num_lines = 0;
}

static void
print_more (void)
{
  char key;
  struct grub_term_coordinate *pos;
  grub_term_output_t term;
  grub_uint32_t *unicode_str, *unicode_last_position;

  /* TRANSLATORS: This has to fit on one line.  It's ok to include few
     words but don't write poems.  */
  grub_utf8_to_ucs4_alloc (_("--MORE--"), &unicode_str,
			   &unicode_last_position);

  if (!unicode_str)
    {
      grub_errno = GRUB_ERR_NONE;
      return;
    }

  pos = grub_term_save_pos ();

  grub_setcolorstate (GRUB_TERM_COLOR_HIGHLIGHT);

  FOR_ACTIVE_TERM_OUTPUTS(term)
  {
    grub_print_ucs4 (unicode_str, unicode_last_position, 0, 0, term);
  }
  grub_setcolorstate (GRUB_TERM_COLOR_NORMAL);

  grub_free (unicode_str);
  
  key = grub_getkey ();

  /* Remove the message.  */
  grub_term_restore_pos (pos);
  FOR_ACTIVE_TERM_OUTPUTS(term)
    grub_print_spaces (term, 8);
  grub_term_restore_pos (pos);
  grub_free (pos);

  /* Scroll one line or an entire page, depending on the key.  */

  if (key == '\r' || key =='\n')
    {
      static struct term_state *state;
      for (state = term_states; state; state = state->next)
	state->num_lines--;
    }
  else
    grub_normal_reset_more ();
}

void
grub_set_more (int onoff)
{
  grub_more = !!onoff;
  grub_normal_reset_more ();
}

enum
  {
    GRUB_CP437_UPDOWNARROW     = 0x12,
    GRUB_CP437_UPARROW         = 0x18,
    GRUB_CP437_DOWNARROW       = 0x19,
    GRUB_CP437_RIGHTARROW      = 0x1a,
    GRUB_CP437_LEFTARROW       = 0x1b,
    GRUB_CP437_VLINE           = 0xb3,
    GRUB_CP437_CORNER_UR       = 0xbf,
    GRUB_CP437_CORNER_LL       = 0xc0,
    GRUB_CP437_HLINE           = 0xc4,
    GRUB_CP437_CORNER_LR       = 0xd9,
    GRUB_CP437_CORNER_UL       = 0xda,
  };

static grub_uint32_t
map_code (grub_uint32_t in, struct grub_term_output *term)
{
  if (in <= 0x7f)
    return in;

  switch (term->flags & GRUB_TERM_CODE_TYPE_MASK)
    {
    case GRUB_TERM_CODE_TYPE_CP437:
      switch (in)
	{
	case GRUB_UNICODE_LEFTARROW:
	  return GRUB_CP437_LEFTARROW;
	case GRUB_UNICODE_UPARROW:
	  return GRUB_CP437_UPARROW;
	case GRUB_UNICODE_UPDOWNARROW:
	  return GRUB_CP437_UPDOWNARROW;
	case GRUB_UNICODE_RIGHTARROW:
	  return GRUB_CP437_RIGHTARROW;
	case GRUB_UNICODE_DOWNARROW:
	  return GRUB_CP437_DOWNARROW;
	case GRUB_UNICODE_HLINE:
	  return GRUB_CP437_HLINE;
	case GRUB_UNICODE_VLINE:
	  return GRUB_CP437_VLINE;
	case GRUB_UNICODE_CORNER_UL:
	  return GRUB_CP437_CORNER_UL;
	case GRUB_UNICODE_CORNER_UR:
	  return GRUB_CP437_CORNER_UR;
	case GRUB_UNICODE_CORNER_LL:
	  return GRUB_CP437_CORNER_LL;
	case GRUB_UNICODE_CORNER_LR:
	  return GRUB_CP437_CORNER_LR;
	}
      return '?';
    case GRUB_TERM_CODE_TYPE_ASCII:
      /* Better than nothing.  */
      switch (in)
	{
	case GRUB_UNICODE_LEFTARROW:
	  return '<';
		
	case GRUB_UNICODE_UPARROW:
	  return '^';
	  
	case GRUB_UNICODE_RIGHTARROW:
	  return '>';
		
	case GRUB_UNICODE_DOWNARROW:
	  return 'v';
		  
	case GRUB_UNICODE_HLINE:
	  return '-';
		  
	case GRUB_UNICODE_VLINE:
	  return '|';
		  
	case GRUB_UNICODE_CORNER_UL:
	case GRUB_UNICODE_CORNER_UR:
	case GRUB_UNICODE_CORNER_LL:
	case GRUB_UNICODE_CORNER_LR:
	  return '+';
		
	}
      return '?';
    }
  return in;
}

void
grub_puts_terminal (const char *str, struct grub_term_output *term)
{
  grub_uint32_t *unicode_str, *unicode_last_position;
  grub_error_push ();
  grub_utf8_to_ucs4_alloc (str, &unicode_str,
			   &unicode_last_position);
  grub_error_pop ();
  if (!unicode_str)
    {
      for (; *str; str++)
	{
	  struct grub_unicode_glyph c =
	    {
	      .variant = 0,
	      .attributes = 0,
	      .ncomb = 0,
	      .estimated_width = 1,
	      .base = *str
	    };

	  FOR_ACTIVE_TERM_OUTPUTS(term)
	  {
	    (term->putchar) (term, &c);
	  }
	  if (*str == '\n')
	    {
	      c.base = '\r';
	      FOR_ACTIVE_TERM_OUTPUTS(term)
	      {
		(term->putchar) (term, &c);
	      }
	    }
	}
      return;
    }

  print_ucs4_real (unicode_str, unicode_last_position, 0, 0, term,
		   0, 0, 0, 0, -1, 0, 0, 0);
  grub_free (unicode_str);
}

struct grub_term_coordinate *
grub_term_save_pos (void)
{
  struct grub_term_output *cur;
  unsigned cnt = 0;
  struct grub_term_coordinate *ret, *ptr;
  
  FOR_ACTIVE_TERM_OUTPUTS(cur)
    cnt++;

  ret = grub_malloc (cnt * sizeof (ret[0]));
  if (!ret)
    return NULL;

  ptr = ret;
  FOR_ACTIVE_TERM_OUTPUTS(cur)
    *ptr++ = grub_term_getxy (cur);

  return ret;
}

void
grub_term_restore_pos (struct grub_term_coordinate *pos)
{
  struct grub_term_output *cur;
  struct grub_term_coordinate *ptr = pos;

  if (!pos)
    return;

  FOR_ACTIVE_TERM_OUTPUTS(cur)
  {
    grub_term_gotoxy (cur, *ptr);
    ptr++;
  }
}

static void 
grub_terminal_autoload_free (void)
{
  struct grub_term_autoload *cur, *next;
  unsigned i;
  for (i = 0; i < 2; i++)
    for (cur = i ? grub_term_input_autoload : grub_term_output_autoload;
	 cur; cur = next)
      {
	next = cur->next;
	grub_free (cur->name);
	grub_free (cur->modname);
	grub_free (cur);
      }
  grub_term_input_autoload = NULL;
  grub_term_output_autoload = NULL;
}

/* Read the file terminal.lst for auto-loading.  */
void
read_terminal_list (const char *prefix)
{
  char *filename;
  grub_file_t file;
  char *buf = NULL;

  if (!prefix)
    {
      grub_errno = GRUB_ERR_NONE;
      return;
    }
  
  filename = grub_xasprintf ("%s/" GRUB_TARGET_CPU "-" GRUB_PLATFORM
			     "/terminal.lst", prefix);
  if (!filename)
    {
      grub_errno = GRUB_ERR_NONE;
      return;
    }

  file = grub_file_open (filename, GRUB_FILE_TYPE_GRUB_MODULE_LIST);
  grub_free (filename);
  if (!file)
    {
      grub_errno = GRUB_ERR_NONE;
      return;
    }

  /* Override previous terminal.lst.  */
  grub_terminal_autoload_free ();

  for (;; grub_free (buf))
    {
      char *p, *name;
      struct grub_term_autoload *cur;
      struct grub_term_autoload **target = NULL;
      
      buf = grub_file_getline (file);
	
      if (! buf)
	break;

      p = buf;
      while (grub_isspace (p[0]))
	p++;

      switch (p[0])
	{
	case 'i':
	  target = &grub_term_input_autoload;
	  break;

	case 'o':
	  target = &grub_term_output_autoload;
	  break;
	}
      if (!target)
	continue;
      
      name = p + 1;
            
      p = grub_strchr (name, ':');
      if (! p)
	continue;
      *p = 0;

      p++;
      while (*p == ' ' || *p == '\t')
	p++;

      cur = grub_malloc (sizeof (*cur));
      if (!cur)
	{
	  grub_errno = GRUB_ERR_NONE;
	  continue;
	}
      
      cur->name = grub_strdup (name);
      if (! cur->name)
	{
	  grub_errno = GRUB_ERR_NONE;
	  grub_free (cur);
	  continue;
	}
	
      cur->modname = grub_strdup (p);
      if (! cur->modname)
	{
	  grub_errno = GRUB_ERR_NONE;
	  grub_free (cur->name);
	  grub_free (cur);
	  continue;
	}
      cur->next = *target;
      *target = cur;
    }
  
  grub_file_close (file);

  grub_errno = GRUB_ERR_NONE;
}

static void
putglyph (const struct grub_unicode_glyph *c, struct grub_term_output *term,
	  int fixed_tab)
{
  struct grub_unicode_glyph c2 =
    {
      .variant = 0,
      .attributes = 0,
      .ncomb = 0,
      .estimated_width = 1
    };

  if (c->base == '\t' && fixed_tab)
    {
      int n;

      n = GRUB_TERM_TAB_WIDTH;
      c2.base = ' ';
      while (n--)
	(term->putchar) (term, &c2);

      return;
    }

  if (c->base == '\t' && term->getxy)
    {
      int n;

      n = GRUB_TERM_TAB_WIDTH - ((term->getxy (term).x)
				 % GRUB_TERM_TAB_WIDTH);
      c2.base = ' ';
      while (n--)
	(term->putchar) (term, &c2);

      return;
    }

  if ((term->flags & GRUB_TERM_CODE_TYPE_MASK)
      == GRUB_TERM_CODE_TYPE_UTF8_LOGICAL 
      || (term->flags & GRUB_TERM_CODE_TYPE_MASK)
      == GRUB_TERM_CODE_TYPE_UTF8_VISUAL)
    {
      int i;
      c2.estimated_width = grub_term_getcharwidth (term, c);
      for (i = -1; i < (int) c->ncomb; i++)
	{
	  grub_uint8_t u8[20], *ptr;
	  grub_uint32_t code;
	      
	  if (i == -1)
	    {
	      code = c->base;
	      if ((term->flags & GRUB_TERM_CODE_TYPE_MASK)
		  == GRUB_TERM_CODE_TYPE_UTF8_VISUAL)
		{
		  if ((c->attributes & GRUB_UNICODE_GLYPH_ATTRIBUTE_MIRROR))
		    code = grub_unicode_mirror_code (code);
		  code = grub_unicode_shape_code (code, c->attributes);
		}
	    }
	  else
	    code = grub_unicode_get_comb (c) [i].code;

	  grub_ucs4_to_utf8 (&code, 1, u8, sizeof (u8));

	  for (ptr = u8; *ptr; ptr++)
	    {
	      c2.base = *ptr;
	      (term->putchar) (term, &c2);
	      c2.estimated_width = 0;
	    }
	}
      c2.estimated_width = 1;
    }
  else
    (term->putchar) (term, c);

  if (c->base == '\n')
    {
      c2.base = '\r';
      (term->putchar) (term, &c2);
    }
}

static void
putcode_real (grub_uint32_t code, struct grub_term_output *term, int fixed_tab)
{
  struct grub_unicode_glyph c =
    {
      .variant = 0,
      .attributes = 0,
      .ncomb = 0,
      .estimated_width = 1
    };

  c.base = map_code (code, term);
  putglyph (&c, term, fixed_tab);
}

/* Put a Unicode character.  */
void
grub_putcode (grub_uint32_t code, struct grub_term_output *term)
{
  /* Combining character by itself?  */
  if (grub_unicode_get_comb_type (code) != GRUB_UNICODE_COMB_NONE)
    return;

  putcode_real (code, term, 0);
}

static grub_ssize_t
get_maxwidth (struct grub_term_output *term,
	      int margin_left, int margin_right)
{
  struct grub_unicode_glyph space_glyph = {
    .base = ' ',
    .variant = 0,
    .attributes = 0,
    .ncomb = 0,
  };
  return (grub_term_width (term)
	  - grub_term_getcharwidth (term, &space_glyph) 
	  * (margin_left + margin_right) - 1);
}

static grub_ssize_t
get_startwidth (struct grub_term_output *term,
		int margin_left)
{
  return (term->getxy (term).x) - margin_left;
}

static void
fill_margin (struct grub_term_output *term, int r)
{
  int sp = (term->getwh (term).x)
    - (term->getxy (term).x) - r;
  if (sp > 0)
    grub_print_spaces (term, sp);
}

static int
print_ucs4_terminal (const grub_uint32_t * str,
		     const grub_uint32_t * last_position,
		     int margin_left, int margin_right,
		     struct grub_term_output *term,
		     struct term_state *state,
		     int dry_run, int fixed_tab, unsigned skip_lines,
		     unsigned max_lines,
		     grub_uint32_t contchar,
		     int primitive_wrap, int fill_right, struct grub_term_pos *pos)
{
  const grub_uint32_t *ptr;
  grub_ssize_t startwidth = dry_run ? 0 : get_startwidth (term, margin_left);
  grub_ssize_t line_width = startwidth;
  grub_ssize_t lastspacewidth = 0;
  grub_ssize_t max_width = get_maxwidth (term, margin_left, margin_right);
  const grub_uint32_t *line_start = str, *last_space = str - 1;
  int lines = 0;
  int i;
  struct term_state local_state;

  if (!state)
    {
      grub_memset (&local_state, 0, sizeof (local_state));
      state = &local_state;
    }

  for (i = 0; i < state->bidi_stack_depth; i++)
    putcode_real (state->bidi_stack[i] | (GRUB_UNICODE_LRE & ~0xff),
		  term, fixed_tab);

  for (ptr = str; ptr < last_position; ptr++)
    {
      grub_ssize_t last_width = 0;

      switch (*ptr)
	{
	case GRUB_UNICODE_LRE:
	case GRUB_UNICODE_RLE:
	case GRUB_UNICODE_LRO:
	case GRUB_UNICODE_RLO:
	  if (state->bidi_stack_depth >= (int) ARRAY_SIZE (state->bidi_stack))
	    state->invalid_pushes++;
	  else
	    state->bidi_stack[state->bidi_stack_depth++] = *ptr;
	  break;
	case GRUB_UNICODE_PDF:
	  if (state->invalid_pushes)
	    state->invalid_pushes--;
	  else if (state->bidi_stack_depth)
	    state->bidi_stack_depth--;
	  break;
	}
      if (grub_unicode_get_comb_type (*ptr) == GRUB_UNICODE_COMB_NONE)
	{
	  struct grub_unicode_glyph c = {
	    .variant = 0,
	    .attributes = 0,
	    .ncomb = 0,
	  };
	  c.base = *ptr;
	  if (pos)
	    {
	      pos[ptr - str].x = line_width;
	      pos[ptr - str].y = lines;
	      pos[ptr - str].valid = 1;
	    }
	  line_width += last_width = grub_term_getcharwidth (term, &c);
	}

      if (*ptr == ' ' && !primitive_wrap)
	{
	  lastspacewidth = line_width;
	  last_space = ptr;
	}

      if (line_width > max_width || *ptr == '\n')
	{
	  const grub_uint32_t *ptr2;
	  int wasn = (*ptr == '\n');

	  if (wasn)
	    {
	      state->bidi_stack_depth = 0;
	      state->invalid_pushes = 0;
	    }

	  if (line_width > max_width && last_space > line_start)
	    ptr = last_space;
	  else if (line_width > max_width 
		   && line_start == str && line_width - lastspacewidth < max_width - 5)
	    {
	      ptr = str;
	      lastspacewidth = startwidth;
	    }
	  else
	    lastspacewidth = line_width - last_width;

	  lines++;

	  if (!skip_lines && !dry_run)
	    {
	      for (ptr2 = line_start; ptr2 < ptr; ptr2++)
		{
		  /* Skip combining characters on non-UTF8 terminals.  */
		  if ((term->flags & GRUB_TERM_CODE_TYPE_MASK) 
		      != GRUB_TERM_CODE_TYPE_UTF8_LOGICAL
		      && grub_unicode_get_comb_type (*ptr2)
		      != GRUB_UNICODE_COMB_NONE)
		    continue;
		  putcode_real (*ptr2, term, fixed_tab);
		}

	      if (!wasn && contchar)
		putcode_real (contchar, term, fixed_tab);
	      if (fill_right)
		fill_margin (term, margin_right);

	      if (!contchar || max_lines != 1)
		grub_putcode ('\n', term);
	      if (state != &local_state && ++state->num_lines
		  >= (grub_ssize_t) grub_term_height (term) - 2)
		{
		  state->backlog_ucs4 = (ptr == last_space || *ptr == '\n') 
		    ? ptr + 1 : ptr;
		  state->backlog_len = last_position - state->backlog_ucs4;
		  state->backlog_fixed_tab = fixed_tab;
		  return 1;
		}
	    }

	  line_width -= lastspacewidth;
	  if (ptr == last_space || *ptr == '\n')
	    ptr++;
	  else if (pos)
	      {
		pos[ptr - str].x = line_width - last_width;
		pos[ptr - str].y = lines;
		pos[ptr - str].valid = 1;
	      }

	  line_start = ptr;

	  if (skip_lines)
	    skip_lines--;
	  else if (max_lines != (unsigned) -1)
	    {
	      max_lines--;
	      if (!max_lines)
		break;
	    }
	  if (!skip_lines && !dry_run)
	    {
	      if (!contchar)
		grub_print_spaces (term, margin_left);
	      else
		grub_term_gotoxy (term, (struct grub_term_coordinate)
				  { margin_left, grub_term_getxy (term).y });
	      for (i = 0; i < state->bidi_stack_depth; i++)
		putcode_real (state->bidi_stack[i] | (GRUB_UNICODE_LRE & ~0xff),
			      term, fixed_tab);
	    }
	}
    }

  if (pos)
    {
      pos[ptr - str].x = line_width;
      pos[ptr - str].y = lines;
      pos[ptr - str].valid = 1;
    }

  if (line_start < last_position)
      lines++;
  if (!dry_run && !skip_lines && max_lines)
    {
      const grub_uint32_t *ptr2;

      for (ptr2 = line_start; ptr2 < last_position; ptr2++)
	{
	  /* Skip combining characters on non-UTF8 terminals.  */
	  if ((term->flags & GRUB_TERM_CODE_TYPE_MASK) 
	      != GRUB_TERM_CODE_TYPE_UTF8_LOGICAL
	      && grub_unicode_get_comb_type (*ptr2)
	      != GRUB_UNICODE_COMB_NONE)
	    continue;
	  putcode_real (*ptr2, term, fixed_tab);
	}

      if (fill_right)
	fill_margin (term, margin_right);
    }
  return dry_run ? lines : 0;
}

static struct term_state *
find_term_state (struct grub_term_output *term)
{
  struct term_state *state;
  for (state = term_states; state; state = state->next)
    if (grub_strcmp (state->term_name, term->name) == 0)
      return state;

  state = grub_zalloc (sizeof (*state));
  if (!state)
    {
      grub_errno = GRUB_ERR_NONE;
      return NULL;
    }

  state->term_name = grub_strdup (term->name);
  state->next = term_states;
  term_states = state;

  return state;
}

static int
put_glyphs_terminal (struct grub_unicode_glyph *visual,
		     grub_ssize_t visual_len,
		     int margin_left, int margin_right,
		     struct grub_term_output *term,
		     struct term_state *state, int fixed_tab,
		     grub_uint32_t contchar,
		     int fill_right)
{
  struct grub_unicode_glyph *visual_ptr;
  int since_last_nl = 1;
  for (visual_ptr = visual; visual_ptr < visual + visual_len; visual_ptr++)
    {
      if (visual_ptr->base == '\n' && fill_right)
	fill_margin (term, margin_right);

      putglyph (visual_ptr, term, fixed_tab);
      since_last_nl++;
      if (visual_ptr->base == '\n')
	{
	  since_last_nl = 0;
	  if (state && ++state->num_lines
	      >= (grub_ssize_t) grub_term_height (term) - 2)
	    {
	      state->backlog_glyphs = visual_ptr + 1;
	      state->backlog_len = visual_len - (visual_ptr - visual) - 1;
	      state->backlog_fixed_tab = fixed_tab;
	      return 1;
	    }

	  if (!contchar)
	    grub_print_spaces (term, margin_left);
	  else
	    grub_term_gotoxy (term,
			      (struct grub_term_coordinate)
			      { margin_left, grub_term_getxy (term).y });
	}
      grub_unicode_destroy_glyph (visual_ptr);
    }
  if (fill_right && since_last_nl)
	fill_margin (term, margin_right);

  return 0;
}

static int
print_backlog (struct grub_term_output *term,
	       int margin_left, int margin_right)
{
  struct term_state *state = find_term_state (term);

  if (!state)
    return 0;

  if (state->backlog_ucs4)
    {
      int ret;
      ret = print_ucs4_terminal (state->backlog_ucs4,
				 state->backlog_ucs4 + state->backlog_len,
				 margin_left, margin_right, term, state, 0,
				 state->backlog_fixed_tab, 0, -1, 0, 0,
				 0, 0);
      if (!ret)
	{
	  grub_free (state->free);
	  state->free = NULL;
	  state->backlog_len = 0;
	  state->backlog_ucs4 = 0;
	}
      return ret;
    }

  if (state->backlog_glyphs)
    {
      int ret;
      ret = put_glyphs_terminal (state->backlog_glyphs,
				 state->backlog_len,
				 margin_left, margin_right, term, state,
				 state->backlog_fixed_tab, 0, 0);
      if (!ret)
	{
	  grub_free (state->free);
	  state->free = NULL;
	  state->backlog_len = 0;
	  state->backlog_glyphs = 0;
	}
      return ret;
    }

  return 0;
}

static grub_size_t
getcharwidth (const struct grub_unicode_glyph *c, void *term)
{
  return grub_term_getcharwidth (term, c);
}

static int
print_ucs4_real (const grub_uint32_t * str,
		 const grub_uint32_t * last_position,
		 int margin_left, int margin_right,
		 struct grub_term_output *term, int backlog,
		 int dry_run, int fixed_tab, unsigned skip_lines,
		 unsigned max_lines,
		 grub_uint32_t contchar, int fill_right,
		 struct grub_term_pos *pos)
{
  struct term_state *state = NULL;

  if (!dry_run)
    {
      struct grub_term_coordinate xy;
      if (backlog)
	state = find_term_state (term);

      xy = term->getxy (term);
      
      if (xy.x < margin_left)
	{
	  if (!contchar)
	    grub_print_spaces (term, margin_left - xy.x);
	  else
	    grub_term_gotoxy (term, (struct grub_term_coordinate) {margin_left,
		  xy.y});
	}
    }

  if ((term->flags & GRUB_TERM_CODE_TYPE_MASK) 
      == GRUB_TERM_CODE_TYPE_VISUAL_GLYPHS
      || (term->flags & GRUB_TERM_CODE_TYPE_MASK) 
      == GRUB_TERM_CODE_TYPE_UTF8_VISUAL)
    {
      grub_ssize_t visual_len;
      struct grub_unicode_glyph *visual;
      grub_ssize_t visual_len_show;
      struct grub_unicode_glyph *visual_show;
      int ret;
      struct grub_unicode_glyph *vptr;

      visual_len = grub_bidi_logical_to_visual (str, last_position - str,
						&visual, getcharwidth, term,
						get_maxwidth (term, 
							      margin_left,
							      margin_right),
						dry_run ? 0 : get_startwidth (term, 
									      margin_left),
						contchar, pos, !!contchar);
      if (visual_len < 0)
	{
	  grub_print_error ();
	  return 0;
	}
      visual_show = visual;
      for (; skip_lines && visual_show < visual + visual_len; visual_show++)
	if (visual_show->base == '\n')
	  skip_lines--;
      if (max_lines != (unsigned) -1)
	{
	  for (vptr = visual_show;
	       max_lines && vptr < visual + visual_len; vptr++)
	    if (vptr->base == '\n')
	      max_lines--;

	  visual_len_show = vptr - visual_show;	  
	}
      else
	visual_len_show = visual + visual_len - visual_show;

      if (dry_run)
	{
	  ret = 0;
	  for (vptr = visual_show; vptr < visual_show + visual_len_show; vptr++)
	    if (vptr->base == '\n')
	      ret++;
	  if (visual_len_show && visual[visual_len_show - 1].base != '\n')
	    ret++;
	  for (vptr = visual; vptr < visual + visual_len; vptr++)
	    grub_unicode_destroy_glyph (vptr);
	  grub_free (visual);
	}
      else
	{
	  ret = put_glyphs_terminal (visual_show, visual_len_show, margin_left,
				     margin_right,
				     term, state, fixed_tab, contchar, fill_right);

	  if (!ret)
	    grub_free (visual);
	  else
	    state->free = visual;
	}
      return ret;
    }
  return print_ucs4_terminal (str, last_position, margin_left, margin_right,
			      term, state, dry_run, fixed_tab, skip_lines,
			      max_lines, contchar, !!contchar, fill_right, pos);
}

void
grub_print_ucs4_menu (const grub_uint32_t * str,
		      const grub_uint32_t * last_position,
		      int margin_left, int margin_right,
		      struct grub_term_output *term,
		      int skip_lines, int max_lines,
		      grub_uint32_t contchar,
		      struct grub_term_pos *pos)
{
  print_ucs4_real (str, last_position, margin_left, margin_right,
		   term, 0, 0, 1, skip_lines, max_lines,
		   contchar, 1, pos);
}

void
grub_print_ucs4 (const grub_uint32_t * str,
		 const grub_uint32_t * last_position,
		 int margin_left, int margin_right,
		 struct grub_term_output *term)
{
  print_ucs4_real (str, last_position, margin_left, margin_right,
		   term, 0, 0, 1, 0, -1, 0, 0, 0);
}

int
grub_ucs4_count_lines (const grub_uint32_t * str,
		       const grub_uint32_t * last_position,
		       int margin_left, int margin_right,
		       struct grub_term_output *term)
{
  return print_ucs4_real (str, last_position, margin_left, margin_right,
			  term, 0, 1, 1, 0, -1, 0, 0, 0);
}

void
grub_xnputs (const char *str, grub_size_t msg_len)
{
  grub_uint32_t *unicode_str = NULL, *unicode_last_position;
  int backlog = 0;
  grub_term_output_t term;

  grub_error_push ();

  unicode_str = grub_malloc (msg_len * sizeof (grub_uint32_t));
 
  grub_error_pop ();

  if (!unicode_str)
    {
      for (; msg_len--; str++, msg_len++)
	{
	  struct grub_unicode_glyph c =
	    {
	      .variant = 0,
	      .attributes = 0,
	      .ncomb = 0,
	      .estimated_width = 1,
	      .base = *str
	    };

	  FOR_ACTIVE_TERM_OUTPUTS(term)
	  {
	    (term->putchar) (term, &c);
	  }
	  if (*str == '\n')
	    {
	      c.base = '\r';
	      FOR_ACTIVE_TERM_OUTPUTS(term)
	      {
		(term->putchar) (term, &c);
	      }
	    }
	}

      return;
    }

  msg_len = grub_utf8_to_ucs4 (unicode_str, msg_len,
			       (grub_uint8_t *) str, -1, 0);
  unicode_last_position = unicode_str + msg_len;

  FOR_ACTIVE_TERM_OUTPUTS(term)
  {
    int cur;
    cur = print_ucs4_real (unicode_str, unicode_last_position, 0, 0,
			   term, grub_more, 0, 0, 0, -1, 0, 0, 0);
    if (cur)
      backlog = 1;
  }
  while (backlog)
    {
      print_more ();
      backlog = 0;
      FOR_ACTIVE_TERM_OUTPUTS(term)
      {
	int cur;
	cur = print_backlog (term, 0, 0);
	if (cur)
	  backlog = 1;
      }
    }
  grub_free (unicode_str);
}

void
grub_xputs_normal (const char *str)
{
  grub_xnputs (str, grub_strlen (str));
}

void
grub_cls (void)
{
  struct grub_term_output *term;

  FOR_ACTIVE_TERM_OUTPUTS(term)  
  {
    if ((term->flags & GRUB_TERM_DUMB) || (grub_env_get ("debug")))
      {
	grub_putcode ('\n', term);
	grub_term_refresh (term);
      }
    else
      (term->cls) (term);
  }
}

int
grub_password_get (char buf[], unsigned buf_size)
{
  unsigned i, cur_len = 0;
  int key;
  struct grub_term_coordinate *pos = grub_term_save_pos ();

  while (1)
    {
      key = grub_getkey (); 
      if (key == '\n' || key == '\r')
	break;

      if (key == GRUB_TERM_ESC)
	{
	  cur_len = 0;
	  break;
	}

      if (key == '\b')
	{
	  if (cur_len)
	  {
	    grub_term_restore_pos (pos);
	    for (i = 0; i < cur_len; i++)
	      grub_xputs (" ");
	    grub_term_restore_pos (pos);
	    cur_len--;
	    for (i = 0; i < cur_len; i++)
	      grub_xputs ("*");
	    grub_refresh ();
	  }
	  continue;
	}

      if (!grub_isprint (key))
	continue;

      if (cur_len + 2 < buf_size)
	buf[cur_len++] = key;
      grub_xputs ("*");
      grub_refresh ();
    }

  grub_memset (buf + cur_len, 0, buf_size - cur_len);

  grub_xputs ("\n");
  grub_refresh ();
  grub_free (pos);

  return (key != GRUB_TERM_ESC);
}
