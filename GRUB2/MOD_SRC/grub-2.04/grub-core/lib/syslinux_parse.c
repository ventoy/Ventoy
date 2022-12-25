/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2013 Free Software Foundation, Inc.
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
#include <grub/file.h>
#include <grub/normal.h>
#include <grub/syslinux_parse.h>

struct syslinux_say
{
  struct syslinux_say *next;
  struct syslinux_say *prev;
  char msg[0];
};

struct initrd_list
{
  struct initrd_list *next;
  char *file;
};

struct syslinux_menuentry
{
  struct syslinux_menuentry *next;
  struct syslinux_menuentry *prev;
  char *label;
  char *extlabel;
  char *kernel_file;
  struct initrd_list *initrds;
  struct initrd_list *initrds_last;
  char *append;
  char *argument;
  char *help;
  char *comments;
  grub_size_t commentslen;
  char hotkey;
  int make_default;
  struct syslinux_say *say;
  
  enum { KERNEL_NO_KERNEL, KERNEL_LINUX, KERNEL_CHAINLOADER, 
	 KERNEL_BIN, KERNEL_PXE, KERNEL_CHAINLOADER_BPB,
	 KERNEL_COM32, KERNEL_COM, KERNEL_IMG, KERNEL_CONFIG, LOCALBOOT }
    entry_type;
};

struct syslinux_menu
{
  struct syslinux_menu *parent;
  struct syslinux_menuentry *entries;
  char *def;
  char *comments;
  char *background;
  const char *root_read_directory;
  const char *root_target_directory;
  const char *current_read_directory;
  const char *current_target_directory;
  const char *filename;
  grub_size_t commentslen;
  unsigned long timeout;
  struct syslinux_say *say;
  grub_syslinux_flavour_t flavour;
};

struct output_buffer
{
  grub_size_t alloc;
  grub_size_t ptr;
  char *buf;
};

static grub_err_t
syslinux_parse_real (struct syslinux_menu *menu);
static grub_err_t
config_file (struct output_buffer *outbuf,
	     const char *root, const char *target_root,
	     const char *cwd, const char *target_cwd,
	     const char *fname, struct syslinux_menu *parent,
	     grub_syslinux_flavour_t flav);
static grub_err_t
print_entry (struct output_buffer *outbuf,
	     struct syslinux_menu *menu,
	     const char *str);

static grub_err_t
ensure_space (struct output_buffer *outbuf, grub_size_t len)
{
  grub_size_t newlen;
  char *newbuf;
  if (len < outbuf->alloc - outbuf->ptr)
    return GRUB_ERR_NONE;
  newlen = (outbuf->ptr + len + 10) * 2;
  newbuf = grub_realloc (outbuf->buf, newlen);
  if (!newbuf)
    return grub_errno;
  outbuf->alloc = newlen;
  outbuf->buf = newbuf;
  return GRUB_ERR_NONE;
}

static grub_err_t
print (struct output_buffer *outbuf, const char *str, grub_size_t len)
{
  grub_err_t err;
  err = ensure_space (outbuf, len);
  if (err)
    return err;
  grub_memcpy (&outbuf->buf[outbuf->ptr], str, len);
  outbuf->ptr += len;
  return GRUB_ERR_NONE;
}

static grub_err_t
add_comment (struct syslinux_menu *menu, const char *comment, int nl)
{
  if (menu->entries)
    {
      if (menu->entries->commentslen == 0 && *comment == 0)
	return GRUB_ERR_NONE;
      menu->entries->comments = grub_realloc (menu->entries->comments,
					      menu->entries->commentslen
					      + 2 + grub_strlen (comment));
      if (!menu->entries->comments)
	return grub_errno;
      menu->entries->commentslen
	+= grub_stpcpy (menu->entries->comments + menu->entries->commentslen,
			comment)
	- (menu->entries->comments + menu->entries->commentslen);
      if (nl)
	menu->entries->comments[menu->entries->commentslen++] = '\n';
      menu->entries->comments[menu->entries->commentslen] = '\0';
    }
  else
    {
      if (menu->commentslen == 0 && *comment == 0)
	return GRUB_ERR_NONE;
      menu->comments = grub_realloc (menu->comments, menu->commentslen
				     + 2 + grub_strlen (comment));
      if (!menu->comments)
	return grub_errno;
      menu->commentslen += grub_stpcpy (menu->comments + menu->commentslen,
					comment)
	- (menu->comments + menu->commentslen);
      if (nl)
	menu->comments[menu->commentslen++] = '\n';
      menu->comments[menu->commentslen] = '\0';
    }
  return GRUB_ERR_NONE;
}


#define print_string(x) do { err = print (outbuf, x, sizeof (x) - 1); if (err) return err; } while (0)

static grub_err_t
print_num (struct output_buffer *outbuf, int n)
{
  char buf[20];
  grub_snprintf (buf, sizeof (buf), "%d", n);
  return print (outbuf, buf, grub_strlen (buf)); 
}

static grub_err_t
label (const char *line, struct syslinux_menu *menu)
{
  struct syslinux_menuentry *entry;

  entry = grub_malloc (sizeof (*entry));
  if (!entry)
    return grub_errno;
  grub_memset (entry, 0, sizeof (*entry));
  entry->label = grub_strdup (line);
  if (!entry->label)
    {
      grub_free (entry);
      return grub_errno;
    }
  entry->next = menu->entries;
  entry->prev = NULL;
  if (menu->entries)
    menu->entries->prev = entry;
  menu->entries = entry;
  return GRUB_ERR_NONE;
}

static grub_err_t
kernel (const char *line, struct syslinux_menu *menu)
{
  const char *end = line + grub_strlen (line);

  if (!menu->entries)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "kernel without label");

  menu->entries->kernel_file = grub_strdup (line);
  if (!menu->entries->kernel_file)
    return grub_errno;

  menu->entries->entry_type = KERNEL_LINUX;

  if (end - line >= 2 && grub_strcmp (end - 2, ".0") == 0)
    menu->entries->entry_type = KERNEL_PXE;

  if (end - line >= 4 && grub_strcasecmp (end - 4, ".bin") == 0)
    menu->entries->entry_type = KERNEL_BIN;

  if (end - line >= 3 && grub_strcasecmp (end - 3, ".bs") == 0)
    menu->entries->entry_type = KERNEL_CHAINLOADER;

  if (end - line >= 4 && grub_strcasecmp (end - 4, ".bss") == 0)
    menu->entries->entry_type = KERNEL_CHAINLOADER_BPB;

  if (end - line >= 4 && grub_strcasecmp (end - 4, ".c32") == 0)
    menu->entries->entry_type = KERNEL_COM32;

  if (end - line >= 4 && grub_strcasecmp (end - 4, ".cbt") == 0)
    menu->entries->entry_type = KERNEL_COM;

  if (end - line >= 4 && grub_strcasecmp (end - 4, ".com") == 0)
    menu->entries->entry_type = KERNEL_COM;

  if (end - line >= 4 && grub_strcasecmp (end - 4, ".img") == 0)
    menu->entries->entry_type = KERNEL_IMG;
  
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_linux (const char *line, struct syslinux_menu *menu)
{
  if (!menu->entries)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "kernel without label");

  menu->entries->kernel_file = grub_strdup (line);
  if (!menu->entries->kernel_file)
    return grub_errno;
  menu->entries->entry_type = KERNEL_LINUX;
  
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_boot (const char *line, struct syslinux_menu *menu)
{
  if (!menu->entries)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "kernel without label");

  menu->entries->kernel_file = grub_strdup (line);
  if (!menu->entries->kernel_file)
    return grub_errno;
  menu->entries->entry_type = KERNEL_CHAINLOADER;
  
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_bss (const char *line, struct syslinux_menu *menu)
{
  if (!menu->entries)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "kernel without label");

  menu->entries->kernel_file = grub_strdup (line);
  if (!menu->entries->kernel_file)
    return grub_errno;
  menu->entries->entry_type = KERNEL_CHAINLOADER_BPB;
  
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_pxe (const char *line, struct syslinux_menu *menu)
{
  if (!menu->entries)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "kernel without label");

  menu->entries->kernel_file = grub_strdup (line);
  if (!menu->entries->kernel_file)
    return grub_errno;
  menu->entries->entry_type = KERNEL_PXE;
  
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_fdimage (const char *line, struct syslinux_menu *menu)
{
  if (!menu->entries)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "kernel without label");

  menu->entries->kernel_file = grub_strdup (line);
  if (!menu->entries->kernel_file)
    return grub_errno;
  menu->entries->entry_type = KERNEL_IMG;
  
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_comboot (const char *line, struct syslinux_menu *menu)
{
  if (!menu->entries)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "kernel without label");

  menu->entries->kernel_file = grub_strdup (line);
  if (!menu->entries->kernel_file)
    return grub_errno;
  menu->entries->entry_type = KERNEL_COM;
  
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_com32 (const char *line, struct syslinux_menu *menu)
{
  if (!menu->entries)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "kernel without label");

  menu->entries->kernel_file = grub_strdup (line);
  if (!menu->entries->kernel_file)
    return grub_errno;
  menu->entries->entry_type = KERNEL_COM32;
  
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_config (const char *line, struct syslinux_menu *menu)
{
  const char *space;
  if (!menu->entries)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "kernel without label");

  for (space = line; *space && !grub_isspace (*space); space++);
  menu->entries->kernel_file = grub_strndup (line, space - line);
  if (!menu->entries->kernel_file)
    return grub_errno;
  for (; *space && grub_isspace (*space); space++);
  if (*space)
    {
      menu->entries->argument = grub_strdup (space);
      if (!menu->entries->argument)
	return grub_errno;
    }
  menu->entries->entry_type = KERNEL_CONFIG;
  
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_append (const char *line, struct syslinux_menu *menu)
{
  if (!menu->entries)
    return GRUB_ERR_NONE;
    //return grub_error (GRUB_ERR_BAD_ARGUMENT, "kernel without label");

  menu->entries->append = grub_strdup (line);
  if (!menu->entries->append)
    return grub_errno;
  
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_initrd (const char *line, struct syslinux_menu *menu)
{
  struct initrd_list *ninitrd;
  const char *comma;
  if (!menu->entries)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "kernel without label");

  while (*line)
    {
      for (comma = line; *comma && *comma != ','; comma++);

      ninitrd = grub_malloc (sizeof (*ninitrd));
      if (!ninitrd)
	return grub_errno;
      ninitrd->file = grub_strndup (line, comma - line);
      if (!ninitrd->file)
	{
	  grub_free (ninitrd);
	  return grub_errno;
	}
      ninitrd->next = NULL;
      if (menu->entries->initrds_last)
	menu->entries->initrds_last->next = ninitrd;
      else
	{
	  menu->entries->initrds_last = ninitrd;
	  menu->entries->initrds = ninitrd;
	}

      line = comma;
      while (*line == ',')
	line++;
    }
  
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_default (const char *line, struct syslinux_menu *menu)
{
  menu->def = grub_strdup (line);
  if (!menu->def)
    return grub_errno;
  
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_timeout (const char *line, struct syslinux_menu *menu)
{
  menu->timeout = grub_strtoul (line, NULL, 0);
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_menudefault (const char *line __attribute__ ((unused)),
		 struct syslinux_menu *menu)
{
  if (!menu->entries)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "kernel without label");

  menu->entries->make_default = 1; 
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_menubackground (const char *line,
		    struct syslinux_menu *menu)
{
  menu->background = grub_strdup (line);
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_localboot (const char *line,
	       struct syslinux_menu *menu)
{
  if (!menu->entries)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "kernel without label");

  menu->entries->kernel_file = grub_strdup (line);
  if (!menu->entries->kernel_file)
    return grub_errno;
  menu->entries->entry_type = LOCALBOOT;
  
  return GRUB_ERR_NONE;
}

static grub_err_t
cmd_extlabel (const char *line, struct syslinux_menu *menu)
{
  const char *in;
  char *out;

  if (!menu->entries)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "kernel without label");

  menu->entries->extlabel = grub_malloc (grub_strlen (line) + 1);
  if (!menu->entries->extlabel)
    return grub_errno;
  in = line;
  out = menu->entries->extlabel;
  while (*in)
    {
      if (in[0] == '^' && in[1])
	{
	  menu->entries->hotkey = grub_tolower (in[1]);
	  in++;
	}
      *out++ = *in++;
    }
  *out = 0;
  
  return GRUB_ERR_NONE;
}


static grub_err_t
cmd_say (const char *line, struct syslinux_menu *menu)
{
  struct syslinux_say *nsay;
  nsay = grub_malloc (sizeof (*nsay) + grub_strlen (line) + 1);
  if (!nsay)
    return grub_errno;
  nsay->prev = NULL;
  if (menu->entries)
    {
      nsay->next = menu->entries->say;
      menu->entries->say = nsay;
    }
  else
    {
      nsay->next = menu->say;
      menu->say = nsay;
    }

  if (nsay->next)
    nsay->next->prev = nsay;

  grub_memcpy (nsay->msg, line, grub_strlen (line) + 1);
  return GRUB_ERR_NONE;
}

static char *
get_read_filename (struct syslinux_menu *menu,
		   const char *filename)
{
  return grub_xasprintf ("%s/%s",
			 filename[0] == '/' ? menu->root_read_directory
			 : menu->current_read_directory, filename);
}

static char *
get_target_filename (struct syslinux_menu *menu,
		   const char *filename)
{
  return grub_xasprintf ("%s/%s",
			 filename[0] == '/' ? menu->root_target_directory
			 : menu->current_target_directory, filename);
}

static grub_err_t
syslinux_parse (const char *filename,
		struct syslinux_menu *menu)
{
  const char *old_filename = menu->filename;
  grub_err_t ret;
  char *nf;
  nf = get_read_filename (menu, filename);
  if (!nf)
    return grub_errno;
  menu->filename = nf;
  ret = syslinux_parse_real (menu);
  if (ret == GRUB_ERR_FILE_NOT_FOUND
      || ret == GRUB_ERR_BAD_FILENAME)
    {	
      grub_errno = ret = GRUB_ERR_NONE;
      add_comment (menu, "# File ", 0);
      add_comment (menu, nf, 0);
      add_comment (menu, " not found", 1);
    }
  grub_free (nf);
  menu->filename = old_filename;
  return ret;
}

struct
{
  const char *name1;
  const char *name2;
  grub_err_t (*parse) (const char *line, struct syslinux_menu *menu);
} commands[] = {
  /* FIXME: support tagname.  */
  {"include", NULL, syslinux_parse},
  {"menu", "include", syslinux_parse},
  {"label",   NULL, label},
  {"kernel",  NULL, kernel},
  {"linux",  NULL, cmd_linux},
  {"boot",  NULL, cmd_boot},
  {"bss",  NULL, cmd_bss},
  {"pxe",  NULL, cmd_pxe},
  {"fdimage",  NULL, cmd_fdimage},
  {"comboot",  NULL, cmd_comboot},
  {"com32",  NULL, cmd_com32},
  {"config",  NULL, cmd_config},
  {"append",  NULL, cmd_append},
  /* FIXME: ipappend not supported.  */
  {"localboot",  NULL, cmd_localboot},
  {"initrd",  NULL, cmd_initrd},
  {"default",  NULL, cmd_default},
  {"menu", "label", cmd_extlabel},
  /* FIXME: MENU LABEL not supported.  */
  /* FIXME: MENU HIDDEN not supported.  */
  /* FIXME: MENU SEPARATOR not supported.  */
  /* FIXME: MENU INDENT not supported.  */
  /* FIXME: MENU DISABLE not supported.  */
  /* FIXME: MENU HIDE not supported.  */
  {"menu", "default", cmd_menudefault},
  /* FIXME: MENU PASSWD not supported.  */
  /* FIXME: MENU MASTER PASSWD not supported.  */
  {"menu", "background", cmd_menubackground},
  /* FIXME: MENU BEGIN not supported.  */
  /* FIXME: MENU GOTO not supported.  */
  /* FIXME: MENU EXIT not supported.  */
  /* FIXME: MENU QUIT not supported.  */
  /* FIXME: MENU START not supported.  */
  /* FIXME: MENU AUTOBOOT not supported.  */
  /* FIXME: MENU TABMSG not supported.  */
  /* FIXME: MENU NOTABMSG not supported.  */
  /* FIXME: MENU PASSPROMPT not supported.  */
  /* FIXME: MENU COLOR not supported.  */
  /* FIXME: MENU MSGCOLOR not supported.  */
  /* FIXME: MENU WIDTH not supported.  */
  /* FIXME: MENU MARGIN not supported.  */
  /* FIXME: MENU PASSWORDMARGIN not supported.  */
  /* FIXME: MENU ROWS not supported.  */
  /* FIXME: MENU TABMSGROW not supported.  */
  /* FIXME: MENU CMDLINEROW not supported.  */
  /* FIXME: MENU ENDROW not supported.  */
  /* FIXME: MENU PASSWORDROW not supported.  */
  /* FIXME: MENU TIMEOUTROW not supported.  */
  /* FIXME: MENU HELPMSGROW not supported.  */
  /* FIXME: MENU HELPMSGENDROW not supported.  */
  /* FIXME: MENU HIDDENROW not supported.  */
  /* FIXME: MENU HSHIFT not supported.  */
  /* FIXME: MENU VSHIFT not supported.  */
  {"timeout", NULL, cmd_timeout},
  /* FIXME: TOTALTIMEOUT not supported.  */
  /* FIXME: ONTIMEOUT not supported.  */
  /* FIXME: ONERROR not supported.  */
  /* FIXME: SERIAL not supported.  */
  /* FIXME: CONSOLE not supported.  */
  /* FIXME: FONT not supported.  */
  /* FIXME: KBDMAP not supported.  */
  {"say", NULL, cmd_say},
  /* FIXME: DISPLAY not supported.  */
  /* FIXME: F* not supported.  */

  /* Commands to control interface behaviour which aren't needed with GRUB.
     If they are important in your environment please contact GRUB team.
   */
  {"prompt",       NULL, NULL},
  {"nocomplete",   NULL, NULL},
  {"noescape",     NULL, NULL},
  {"implicit",     NULL, NULL},
  {"allowoptions", NULL, NULL}
};

static grub_err_t
helptext (const char *line, grub_file_t file, struct syslinux_menu *menu)
{
  char *help;
  char *buf = NULL;
  grub_size_t helplen, alloclen = 0;

  help = grub_strdup (line);
  if (!help)
    return grub_errno;
  helplen = grub_strlen (line);
  while ((grub_free (buf), buf = grub_file_getline (file)))
    {
      char *ptr;
      grub_size_t needlen;
      for (ptr = buf; *ptr && grub_isspace (*ptr); ptr++);
      if (grub_strncasecmp (ptr, "endtext", sizeof ("endtext") - 1) == 0)
	{
	  ptr += sizeof ("endtext") - 1;
	  for (; *ptr && (grub_isspace (*ptr) || *ptr == '\n' || *ptr == '\r');
	       ptr++);
	  if (!*ptr)
	    {
	      menu->entries->help = help;
	      grub_free (buf);
	      return GRUB_ERR_NONE;
	    }
	}
      needlen = helplen + 1 + grub_strlen (buf);
      if (alloclen < needlen)
	{
	  alloclen = 2 * needlen;
	  help = grub_realloc (help, alloclen);
	  if (!help)
	    {
	      grub_free (buf);
	      return grub_errno;
	    }
	}
      helplen += grub_stpcpy (help + helplen, buf) - (help + helplen);
    }

  grub_free (buf);
  grub_free (help);
  return grub_errno;
}


static grub_err_t
syslinux_parse_real (struct syslinux_menu *menu)
{
  grub_file_t file;
  char *buf = NULL;
  grub_err_t err = GRUB_ERR_NONE;

  file = grub_file_open (menu->filename, GRUB_FILE_TYPE_CONFIG);
  if (!file)
    return grub_errno;
  while ((grub_free (buf), buf = grub_file_getline (file)))
    {
      const char *ptr1, *ptr2, *ptr3, *ptr4, *ptr5;
      char *end;
      unsigned i;
      end = buf + grub_strlen (buf);
      while (end > buf && (end[-1] == '\n' || end[-1] == '\r'))
	end--;
      *end = 0;
      for (ptr1 = buf; *ptr1 && grub_isspace (*ptr1); ptr1++);
      if (*ptr1 == '#' || *ptr1 == 0)
	{
	  err = add_comment (menu, ptr1, 1);
	  if (err)
	    goto fail;
	  continue;
	}
      for (ptr2 = ptr1; !grub_isspace (*ptr2) && *ptr2; ptr2++);
      for (ptr3 = ptr2;  grub_isspace (*ptr3) && *ptr3; ptr3++);
      for (ptr4 = ptr3; !grub_isspace (*ptr4) && *ptr4; ptr4++);
      for (ptr5 = ptr4;  grub_isspace (*ptr5) && *ptr5; ptr5++);
      for (i = 0; i < ARRAY_SIZE(commands); i++)
	if (grub_strlen (commands[i].name1) == (grub_size_t) (ptr2 - ptr1)
	    && grub_strncasecmp (commands[i].name1, ptr1, ptr2 - ptr1) == 0
	    && (commands[i].name2 == NULL
		|| (grub_strlen (commands[i].name2)
		    == (grub_size_t) (ptr4 - ptr3)
		    && grub_strncasecmp (commands[i].name2, ptr3, ptr4 - ptr3)
		    == 0)))
	  break;
      if (i == ARRAY_SIZE(commands))
	{
	  if (sizeof ("text") - 1 == ptr2 - ptr1
	      && grub_strncasecmp ("text", ptr1, ptr2 - ptr1) == 0
	      && (sizeof ("help") - 1 == ptr4 - ptr3
		  && grub_strncasecmp ("help", ptr3, ptr4 - ptr3) == 0))
	    {
	      if (helptext (ptr5, file, menu))
		return 1;
	      continue;
	    }

	  add_comment (menu, "  # UNSUPPORTED command '", 0);
	  add_comment (menu, ptr1, 0);
	  add_comment (menu, "'", 1);

	  continue;
	}
      if (commands[i].parse)
	{
	  err = commands[i].parse (commands[i].name2
				   ? ptr5 : ptr3, menu);
	  if (err)
	    goto fail;
	}
    }
 fail:
  grub_file_close (file);
  return err;
}

static grub_err_t
print_escaped (struct output_buffer *outbuf, 
	       const char *from, const char *to)
{
  const char *ptr;
  grub_err_t err;
  if (!to)
    to = from + grub_strlen (from);
  err = ensure_space (outbuf, (to - from) * 4 + 2);
  if (err)
    return err;
  outbuf->buf[outbuf->ptr++] = '\'';
  for (ptr = from; *ptr && ptr < to; ptr++)
    {
      if (*ptr == '\'')
	{
	  outbuf->buf[outbuf->ptr++] = '\'';
	  outbuf->buf[outbuf->ptr++] = '\\';
	  outbuf->buf[outbuf->ptr++] = '\'';
	  outbuf->buf[outbuf->ptr++] = '\'';
	}
      else
	outbuf->buf[outbuf->ptr++] = *ptr;
    }
  outbuf->buf[outbuf->ptr++] = '\'';
  return GRUB_ERR_NONE;
}

static grub_err_t
print_file (struct output_buffer *outbuf,
	    struct syslinux_menu *menu, const char *from, const char *to)
{
  grub_err_t err;
  if (!to)
    to = from + grub_strlen (from);
  err = print_escaped (outbuf, from[0] == '/'
		       ? menu->root_target_directory
		       : menu->current_target_directory, NULL);
  if (err)
    return err;

  err = print (outbuf, "/", 1);
  if (err)
    return err;
  return print_escaped (outbuf, from, to);
}

/*
 * Makefile.am mimics this when generating tests/syslinux/ubuntu10.04_grub.cfg,
 * so changes here may need to be reflected there too.
 */
static void
simplify_filename (char *str)
{
  char *iptr, *optr = str;
  for (iptr = str; *iptr; iptr++)
    {
      if (*iptr == '/' && optr != str && optr[-1] == '/')
	continue;
      if (iptr[0] == '/' && iptr[1] == '.' && iptr[2] == '/')
	{
	  iptr += 2;
	  continue;
	}
      if (iptr[0] == '/' && iptr[1] == '.' && iptr[2] == '.'
	  && iptr[3] == '/')
	{
	  iptr += 3;
	  while (optr >= str && *optr != '/')
	    optr--;
	  if (optr < str)
	    {
	      str[0] = '/';
	      optr = str;
	    }
	  optr++;
	  continue;
	}
      *optr++ = *iptr;
    }
  *optr = '\0';
}

static grub_err_t
print_config (struct output_buffer *outbuf,
	      struct syslinux_menu *menu,
              const char *filename, const char *basedir)
{
  struct syslinux_menu *menuptr;
  grub_err_t err = GRUB_ERR_NONE;
  char *new_cwd = NULL;
  char *new_target_cwd = NULL;
  char *newname = NULL;
  int depth = 0;

  new_cwd = get_read_filename (menu, basedir);
  if (!new_cwd)
    {
      err = grub_errno;
      goto out;
    }
  new_target_cwd = get_target_filename (menu, basedir);
  if (!new_target_cwd)
    {
      err = grub_errno;
      goto out;
    }
  newname = get_read_filename (menu, filename);
  if (!newname)
    {
      err = grub_errno;
      goto out;
    }
  simplify_filename (newname);

  print_string ("#");
  print_file (outbuf, menu, filename, NULL);
  print_string (" ");
  err = print (outbuf, newname, grub_strlen (newname));
  if (err)
    return err;
  print_string (":\n");

  for (menuptr = menu; menuptr; menuptr = menuptr->parent, depth++)
    if (grub_strcmp (menuptr->filename, newname) == 0
        || depth > 20)
      break;
  if (menuptr)
    {
      print_string ("  syslinux_configfile -r ");
      print_file (outbuf, menu, "/", NULL);
      print_string (" -c ");
      print_file (outbuf, menu, basedir, NULL);
      print_string (" ");
      print_file (outbuf, menu, filename, NULL);
      print_string ("\n");
    }
  else
    {
      err = config_file (outbuf, menu->root_read_directory,
                         menu->root_target_directory, new_cwd, new_target_cwd,
                         newname, menu, menu->flavour);
      if (err == GRUB_ERR_FILE_NOT_FOUND
          || err == GRUB_ERR_BAD_FILENAME)
        {
          grub_errno = err = GRUB_ERR_NONE;
          print_string ("# File ");
          err = print (outbuf, newname, grub_strlen (newname));
          if (err)
            goto out;
          print_string (" not found\n");
        }
    }

 out:
  grub_free (newname);
  grub_free (new_cwd);
  grub_free (new_target_cwd);
  return err;
}

static grub_err_t
write_entry (struct output_buffer *outbuf,
	     struct syslinux_menu *menu,
	     struct syslinux_menuentry *curentry)
{
  grub_err_t err;
  if (curentry->comments)
    {
      err = print (outbuf, curentry->comments,
		   grub_strlen (curentry->comments));
      if (err)
	return err;
    }
  {
    struct syslinux_say *say;
    for (say = curentry->say; say && say->next; say = say->next);
    for (; say && say->prev; say = say->prev)
      {
	print_string ("echo ");
	if (print_escaped (outbuf, say->msg, NULL)) return grub_errno;
	print_string ("\n");
      }
  }

  /* FIXME: support help text.  */
  switch (curentry->entry_type)
    {
    case KERNEL_LINUX:
      {
	const char *ptr;
	const char *initrd = NULL, *initrde= NULL;
	for (ptr = curentry->append; ptr && *ptr; ptr++)
	  if ((ptr == curentry->append || grub_isspace (ptr[-1]))
	      && grub_strncasecmp (ptr, "initrd=", sizeof ("initrd=") - 1)
	      == 0)
	    break;
	if (ptr && *ptr)
	  {
	    initrd = ptr + sizeof ("initrd=") - 1;
	    for (initrde = initrd; *initrde && !grub_isspace (*initrde); initrde++);
	  }
	print_string (" if test x$ventoy_linux_16 = x1; then "
		      "linux_suffix=16; else linux_suffix= ; fi\n");
	print_string ("  linux$linux_suffix ");
	print_file (outbuf, menu, curentry->kernel_file, NULL);
	print_string (" ");
	if (curentry->append)
	  {
	    err = print (outbuf, curentry->append, grub_strlen (curentry->append));
	    if (err)
	      return err;
	  }
	print_string ("\n");
	if (initrd || curentry->initrds)
	  {
	    struct initrd_list *lst;
	    print_string ("  initrd$linux_suffix ");
	    if (initrd)
	      {
		print_file (outbuf, menu, initrd, initrde);
		print_string (" ");
	      }
	    for (lst = curentry->initrds; lst; lst = lst->next)
	      {
		print_file (outbuf, menu, lst->file, NULL);
		print_string (" ");
	      }

	    print_string ("\n");
	  }
      print_string ("boot\n");
      }
      break;
    case KERNEL_CHAINLOADER:
      print_string ("  chainloader ");
      print_file (outbuf, menu, curentry->kernel_file, NULL);
      print_string ("\n");
      break;
    case KERNEL_CHAINLOADER_BPB:
      print_string ("  chainloader --bpb ");
      print_file (outbuf, menu, curentry->kernel_file, NULL);
      print_string ("\n");
      break;
    case LOCALBOOT:
      /* FIXME: support -1.  */
      /* FIXME: PXELINUX.  */
      {
	int n = grub_strtol (curentry->kernel_file, NULL, 0);
	if (n >= 0 && n <= 0x02)
	  {
	    print_string ("  root=fd");
	    if (print_num (outbuf, n))
	      return grub_errno;
	    print_string (";\n  chainloader +1;\n");

	    break;
	  }
	if (n >= 0x80 && n < 0x8a)
	  {
	    print_string ("  root=hd");
	    if (print_num (outbuf, n - 0x80))
	      return grub_errno;
	    print_string (";\n  chainloader +1;\n");
	    break;
	  }
	print_string ("  # UNSUPPORTED localboot type ");
	print_string ("\ntrue;\n");
	if (print_num (outbuf, n))
	  return grub_errno;
	print_string ("\n");
	break;
      }
    case KERNEL_COM32:
    case KERNEL_COM:
      {
	char *basename = NULL;
	
	{
	  char *ptr;
	  for (ptr = curentry->kernel_file; *ptr; ptr++)
	    if (*ptr == '/' || *ptr == '\\')
	      basename = ptr;
	}
	if (!basename)
	  basename = curentry->kernel_file;
	else
	  basename++;
	if (grub_strcasecmp (basename, "chain.c32") == 0)
	  {
	    char *file = NULL;
	    int is_fd = -1, devn = 0;
	    int part = -1;
	    int swap = 0;
	    char *ptr;
	    for (ptr = curentry->append; *ptr; )
	      {
		while (grub_isspace (*ptr))
		  ptr++;
		/* FIXME: support mbr: and boot.  */
		if (ptr[0] == 'h' && ptr[1] == 'd')
		  {
		    is_fd = 0;
		    devn = grub_strtoul (ptr + 2, &ptr, 0);
		    continue;
		  }
		if (grub_strncasecmp (ptr, "file=", 5) == 0)
		  {
		    file = ptr + 5;
		    for (ptr = file; *ptr && !grub_isspace (*ptr); ptr++);
		    if (*ptr)
		      {
			*ptr = 0;
			ptr++;
		      }
		    continue;
		  }
		if (grub_strncasecmp (ptr, "swap", sizeof ("swap") - 1) == 0)
		  {
		    swap = 1;
		    ptr += sizeof ("swap") - 1;
		    continue;
		  }

		if (ptr[0] == 'f' && ptr[1] == 'd')
		  {
		    is_fd = 1;
		    devn = grub_strtoul (ptr + 2, &ptr, 0);
		    continue;
		  }
		if (grub_isdigit (ptr[0]))
		  {
		    part = grub_strtoul (ptr, &ptr, 0);
		    continue;
		  }
		/* FIXME: isolinux, ntldr, cmldr, *dos, seg, hide
		   FIXME: sethidden.  */
		print_string ("  # UNSUPPORTED option ");
		if (print (outbuf, ptr, grub_strlen (ptr)))
		  return 0;
		print_string ("\n");
		break;
	      }
	    if (is_fd == -1)
	      {
		print_string ("  # no drive specified\n");
		break;
	      }
	    if (!*ptr)
	      {
		print_string (is_fd ? " root=fd": " root=hd");
		if (print_num (outbuf, devn))
		  return grub_errno;
		if (part != -1)
		  {
		    print_string (",");
		    if (print_num (outbuf, part + 1))
		      return grub_errno;
		  }
		print_string (";\n");
		if (file)
		  {
		    print_string ("  chainloader ");
		    print_file (outbuf, menu, file, NULL);
		    print_string (";\n");
		  }
		else
		  print_string (" chainloader +1;\n");
		if (swap)
		  print_string (" drivemap -s hd0 \"root\";\n");
	      }
	    break;
	  }

	if (grub_strcasecmp (basename, "mboot.c32") == 0)
	  {
	    char *ptr;
	    int first = 1;
	    int is_kernel = 1;
	    for (ptr = curentry->append; *ptr; )
	      {
		char *ptrr = ptr;
		while (*ptr && !grub_isspace (*ptr))
		  ptr++;
		if (ptrr + 2 == ptr && ptrr[0] == '-' && ptrr[1] == '-')
		  {
		    print_string ("\n");
		    first = 1;
		    continue;
		  }
		if (first)
		  {
		    if (is_kernel)
		      print_string ("  multiboot ");
		    else
		      print_string ("  module ");
		    first = 0;
		    is_kernel = 0;
		    if (print_file (outbuf, menu, ptrr, ptr))
		      return grub_errno;
		    continue;
		  }
		if (print_escaped (outbuf, ptrr, ptr))
		  return grub_errno;
	      }
	    break;
	  }

	if (grub_strcasecmp (basename, "ifcpu64.c32") == 0)
	  {
	    char *lm, *lme, *pae = 0, *paee = 0, *i386s = 0, *i386e = 0;
	    char *ptr;
	    ptr = curentry->append;
	    while (grub_isspace (*ptr))
	      ptr++;
	    lm = ptr;
	    while (*ptr && !grub_isspace (*ptr))
	      ptr++;
	    lme = ptr;
	    while (grub_isspace (*ptr))
	      ptr++;
	    if (ptr[0] == '-' && ptr[1] == '-')
	      {
		ptr += 2;
		while (grub_isspace (*ptr))
		  ptr++;
		pae = ptr;
		while (*ptr && !grub_isspace (*ptr))
		  ptr++;
		paee = ptr;
	      }
	    while (grub_isspace (*ptr))
	      ptr++;
	    if (ptr[0] == '-' && ptr[1] == '-')
	      {
		ptr += 2;
		while (grub_isspace (*ptr))
		  ptr++;
		i386s = ptr;
		while (*ptr && !grub_isspace (*ptr))
		  ptr++;
		i386e = ptr;
	      }
	    *lme = '\0';
	    if (paee)
	      *paee = '\0';
	    if (i386e)
	      *i386e = '\0';
	    if (!i386s)
	      {
		i386s = pae;
		pae = 0;
	      }
	    print_string ("if cpuid --long-mode; then true;\n");
	    if (print_entry (outbuf, menu, lm))
	      return grub_errno;
	    if (pae)
	      {
		print_string ("elif cpuid --pae; then true;\n");
		if (print_entry (outbuf, menu, pae))
		  return grub_errno;
	      }
	    print_string ("else\n");
	    if (print_entry (outbuf, menu, i386s))
	      return grub_errno;
	    print_string ("fi\n");
	    break;
	  }

	if (grub_strcasecmp (basename, "reboot.c32") == 0)
	  {
	    print_string ("  reboot\n");
	    break;
	  }

	if (grub_strcasecmp (basename, "poweroff.com") == 0)
	  {
	    print_string ("  halt\n");
	    break;
	  }

	if (grub_strcasecmp (basename, "whichsys.c32") == 0)
	  {
	    grub_syslinux_flavour_t flavour = GRUB_SYSLINUX_ISOLINUX;
	    const char *flav[] = 
	      { 
		[GRUB_SYSLINUX_ISOLINUX] = "iso",
		[GRUB_SYSLINUX_PXELINUX] = "pxe",
		[GRUB_SYSLINUX_SYSLINUX] = "sys"
	      };
	    char *ptr;
	    for (ptr = curentry->append; *ptr; )
	      {
		char *bptr, c;
		while (grub_isspace (*ptr))
		  ptr++;
		if (grub_strncasecmp (ptr, "-iso-", 5) == 0)
		  {
		    ptr += sizeof ("-iso-") - 1;
		    flavour = GRUB_SYSLINUX_ISOLINUX;
		    continue;
		  }
		if (grub_strncasecmp (ptr, "-pxe-", 5) == 0)
		  {
		    ptr += sizeof ("-pxe-") - 1;
		    flavour = GRUB_SYSLINUX_PXELINUX;
		    continue;
		  }
		if (grub_strncasecmp (ptr, "-sys-", 5) == 0)
		  {
		    ptr += sizeof ("-sys-") - 1;
		    flavour = GRUB_SYSLINUX_SYSLINUX;
		    continue;
		  }
		bptr = ptr;
		while (*ptr && !grub_isspace (*ptr))
		  ptr++;
		c = *ptr;
		*ptr = '\0';
		if (menu->flavour == GRUB_SYSLINUX_UNKNOWN
		    && flavour == GRUB_SYSLINUX_ISOLINUX)
		  {
		    print_string ("if [ x$syslinux_flavour = xiso -o x$syslinux_flavour = x ]; then true;\n");
		    menu->flavour = GRUB_SYSLINUX_ISOLINUX;
		    print_entry (outbuf, menu, bptr);
		    menu->flavour = GRUB_SYSLINUX_UNKNOWN;
		    print_string ("fi\n");
		  }
		else if (menu->flavour == GRUB_SYSLINUX_UNKNOWN)
		  {
		    print_string ("if [ x$syslinux_flavour = x");
		    err = print (outbuf, flav[flavour], grub_strlen (flav[flavour]));
		    if (err)
		      return err;
		    print_string (" ]; then true;\n");
		    menu->flavour = flavour;
		    print_entry (outbuf, menu, bptr);
		    menu->flavour = GRUB_SYSLINUX_UNKNOWN;
		    print_string ("fi\n");
		  }
		if (menu->flavour != GRUB_SYSLINUX_UNKNOWN
		    && menu->flavour == flavour)
		  print_entry (outbuf, menu, bptr);
		*ptr = c;
	      }
	    break;
	  }

	if (grub_strcasecmp (basename, "menu.c32") == 0 ||
	    grub_strcasecmp (basename, "vesamenu.c32") == 0)
	  {
	    char *ptr;
	    char *end;

	    ptr = curentry->append;
	    if (!ptr)
	      return grub_errno;

	    while (*ptr)
	      {
		end = ptr;
		for (end = ptr; *end && !grub_isspace (*end); end++);
		if (*end)
		  *end++ = '\0';

		/* "~" is supposed to be current file, so let's skip it */
		if (grub_strcmp (ptr, "~") != 0)
		  {
		    err = print_config (outbuf, menu, ptr, "");
		    if (err != GRUB_ERR_NONE)
		      break;
                  }
		for (ptr = end; *ptr && grub_isspace (*ptr); ptr++);
	      }
	    err = GRUB_ERR_NONE;
	    break;
	  }

	/* FIXME: gdb, GFXBoot, Hdt, Ifcpu, Ifplop, Kbdmap,
	   FIXME: Linux, Lua, Meminfo, rosh, Sanbboot  */

	print_string ("  # UNSUPPORTED com(32) ");
	err = print (outbuf, basename, grub_strlen (basename));
	if (err)
	  return err;
	print_string ("\ntrue;\n");
	break;
      }
    case KERNEL_CONFIG:
      {
	const char *ap;
	ap = curentry->append;
	if (!ap)
	  ap = curentry->argument;
	if (!ap)
	  ap = "";
	print_config (outbuf, menu, curentry->kernel_file, ap);
      }
      break;
    case KERNEL_NO_KERNEL:
      /* FIXME: support this.  */
    case KERNEL_BIN:
    case KERNEL_PXE:
    case KERNEL_IMG:
      print_string ("  # UNSUPPORTED entry type ");
      if (print_num (outbuf, curentry->entry_type))
	return grub_errno;
      print_string ("\ntrue;\n");
      break;
    }
  return GRUB_ERR_NONE;
}

static grub_err_t
print_entry (struct output_buffer *outbuf,
	     struct syslinux_menu *menu,
	     const char *str)
{
  struct syslinux_menuentry *curentry;
  for (curentry = menu->entries; curentry; curentry = curentry->next)
    if (grub_strcasecmp (curentry->label, str) == 0)
      {
	grub_err_t err;
	err = write_entry (outbuf, menu, curentry);
	if (err)
	  return err;
      }
  return GRUB_ERR_NONE;
}

static void
free_menu (struct syslinux_menu *menu)
{
  struct syslinux_say *say, *nsay;
  struct syslinux_menuentry *entry, *nentry;

  grub_free (menu->def);
  grub_free (menu->comments);
  grub_free (menu->background);
  for (say = menu->say; say ; say = nsay)
    {
      nsay = say->next;
      grub_free (say);
    }

  for (entry = menu->entries; entry ; entry = nentry)
    {
      nentry = entry->next;
      struct initrd_list *initrd, *ninitrd;

      for (initrd = entry->initrds; initrd ; initrd = ninitrd)
	{
	  ninitrd = initrd->next;
	  grub_free (initrd->file);
	  grub_free (initrd);
	}
 
      grub_free (entry->comments);
      grub_free (entry->kernel_file);
      grub_free (entry->label);
      grub_free (entry->extlabel);
      grub_free (entry->append);
      grub_free (entry->help);
      grub_free (entry);
    }
}

static grub_err_t
config_file (struct output_buffer *outbuf,
	     const char *root, const char *target_root,
	     const char *cwd, const char *target_cwd,
	     const char *fname, struct syslinux_menu *parent,
	     grub_syslinux_flavour_t flav)
{
  const char *data;
  grub_err_t err;
  struct syslinux_menu menu;
  struct syslinux_menuentry *curentry, *lentry;
  struct syslinux_say *say;

  grub_memset (&menu, 0, sizeof (menu));
  menu.flavour = flav;
  menu.root_read_directory = root;
  menu.root_target_directory = target_root;
  menu.current_read_directory = cwd;
  menu.current_target_directory = target_cwd;

  menu.filename = fname;
  menu.parent = parent;

  data = grub_env_get("vtdebug_flag");
  if (data && data[0])
  {
      menu.timeout = 100;
  }
  
  err = syslinux_parse_real (&menu);
  if (err)
    return err;

  for (say = menu.say; say && say->next; say = say->next);
  for (; say && say->prev; say = say->prev)
    {
      print_string ("echo ");
      err = print_escaped (outbuf, say->msg, NULL);
      if (err)
	return err;
      print_string ("\n");
    }

  if (menu.background)
    {
      print_string ("  background_image ");
      err = print_file (outbuf, &menu, menu.background, NULL);
      if (err)
	return err;
      print_string ("\n");
    }

  if (menu.comments)
    {
      err = print (outbuf, menu.comments, grub_strlen (menu.comments));
      if (err)
	return err;
    }

  if (menu.timeout == 0 && menu.entries && menu.def)
    {
      err = print_entry (outbuf, &menu, menu.def);
      if (err)
	return err;
    }
  else if (menu.entries)
    {
      for (curentry = menu.entries; curentry->next; curentry = curentry->next);
      lentry = curentry;

      print_string ("set timeout=");
      err = print_num (outbuf, (menu.timeout + 9) / 10);
      if (err)
	return err;
      print_string ("\n");

      if (menu.def)
	{
	  print_string (" default=");
	  err = print_escaped (outbuf, menu.def, NULL);
	  if (err)
	    return err;
	  print_string ("\n");
	}
      for (curentry = lentry; curentry; curentry = curentry->prev)
	{      
	  print_string ("menuentry ");
	  err = print_escaped (outbuf,
			       curentry->extlabel ? : curentry->label, NULL);
	  if (err)
	    return err;
	  if (curentry->hotkey)
	    {
	      char hk[] = { curentry->hotkey, '\0' };
	      print_string (" --hotkey '");
	      print_string (hk);
	      print_string ("'");
	    }
	  print_string (" --id ");
	  err = print_escaped (outbuf, curentry->label, NULL);
	  if (err)
	    return err;
	  print_string (" {\n");

	  err = write_entry (outbuf, &menu, curentry);
	  if (err)
	    return err;

	  print_string ("}\n");
	}
    }
  free_menu (&menu);
  return GRUB_ERR_NONE;
}

char *
grub_syslinux_config_file (const char *base, const char *target_base,
			   const char *cwd, const char *target_cwd,
			   const char *fname, grub_syslinux_flavour_t flav)
{
  struct output_buffer outbuf = { 0, 0, 0 };
  grub_err_t err;
  err = config_file (&outbuf, base, target_base, cwd, target_cwd,
		     fname, NULL, flav);
  if (err)
    return NULL;
  err = print (&outbuf, "\0", 1);
  if (err)
    return NULL;
  return outbuf.buf;
}
