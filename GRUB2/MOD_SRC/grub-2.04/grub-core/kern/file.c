/* file.c - file I/O functions */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2006,2007,2009  Free Software Foundation, Inc.
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

#include <grub/misc.h>
#include <grub/err.h>
#include <grub/file.h>
#include <grub/net.h>
#include <grub/mm.h>
#include <grub/fs.h>
#include <grub/device.h>
#include <grub/i18n.h>

void (*EXPORT_VAR (grub_grubnet_fini)) (void);

grub_file_filter_t grub_file_filters[GRUB_FILE_FILTER_MAX];

/* Get the device part of the filename NAME. It is enclosed by parentheses.  */
char *
grub_file_get_device_name (const char *name)
{
  if (name[0] == '(')
    {
      char *p = grub_strchr (name, ')');
      char *ret;

      if (! p)
	{
	  grub_error (GRUB_ERR_BAD_FILENAME, N_("missing `%c' symbol"), ')');
	  return 0;
	}

      ret = (char *) grub_malloc (p - name);
      if (! ret)
	return 0;

      grub_memcpy (ret, name + 1, p - name - 1);
      ret[p - name - 1] = '\0';
      return ret;
    }

  return 0;
}

/* Support mem:xxx:size:xxx format in chainloader */
grub_file_t grub_memfile_open(const char *name);
#define GRUB_MEMFILE_MEM     "mem:"
#define GRUB_MEMFILE_SIZE    "size:"

grub_file_t grub_memfile_open(const char *name)
{
    char *size = NULL;
    grub_file_t file = 0;
    
    file = (grub_file_t)grub_zalloc(sizeof(*file));
    if (NULL == file)
    {
        return 0;
    }

    file->name = grub_strdup(name);
    file->data = (void *)grub_strtoul(name + grub_strlen(GRUB_MEMFILE_MEM), NULL, 0);

    size = grub_strstr(name, GRUB_MEMFILE_SIZE);
    file->size = (grub_off_t)grub_strtoul(size + grub_strlen(GRUB_MEMFILE_SIZE), NULL, 0);

    grub_errno = GRUB_ERR_NONE;
    return file;
}

int ventoy_check_file_exist(const char * fmt, ...)
{
    va_list ap;
    grub_file_t file;
    char fullpath[256] = {0};

    va_start (ap, fmt);
    grub_vsnprintf(fullpath, 255, fmt, ap);
    va_end (ap);

    file = grub_file_open(fullpath, GRUB_FILE_TYPE_NONE);
    if (!file)
    {
        grub_errno = 0;
        return 0;
    }
    else
    {
        grub_file_close(file);
        return 1;
    }
}

typedef struct grub_vlnk
{
    int srclen;
    char src[512];
    char dst[512];
    struct grub_vlnk *next;
}grub_vlnk;

static grub_vlnk g_vtoy_vlnk;
static grub_vlnk *g_vlnk_list;

int grub_file_is_vlnk_suffix(const char *name, int len)
{
    grub_uint32_t suffix;

    if (len > 9)
    {
        suffix = *(grub_uint32_t *)(name + len - 4);
        if (grub_strncmp(name + len - 9, ".vlnk.", 6) == 0)
        {
            /* .iso .wim .img .vhd .efi .dat */
            if (suffix == 0x6F73692E || suffix == 0x6D69772E || 
                suffix == 0x676D692E || suffix == 0x6468762E ||
                suffix == 0x6966652E || suffix == 0x7461642E)
            {
                return 1;
            }
        }
        else if (len > 10 && grub_strncmp(name + len - 10, ".vlnk.", 6) == 0)
        {
            /* vhdx vtoy */
            if (suffix == 0x78646876 || suffix == 0x796F7476)
            {
                return 1;
            }
        }
    }

    return 0;
}

int grub_file_vtoy_vlnk(const char *src, const char *dst)
{
    if (src)
    {
        g_vtoy_vlnk.srclen = (int)grub_strlen(src);
        grub_strncpy(g_vtoy_vlnk.src, src, sizeof(g_vtoy_vlnk.src) - 1);
        grub_strncpy(g_vtoy_vlnk.dst, dst, sizeof(g_vtoy_vlnk.dst) - 1);        
    }
    else
    {
        g_vtoy_vlnk.srclen = 0;
        g_vtoy_vlnk.src[0] = 0;
        g_vtoy_vlnk.dst[0] = 0;
    }
    return 0;
}

int grub_file_add_vlnk(const char *src, const char *dst)
{
    grub_vlnk *node = NULL;
    
    if (src && dst)
    {
        node = grub_zalloc(sizeof(grub_vlnk));    
        if (node)
        {
            node->srclen = (int)grub_strlen(src);
            grub_strncpy(node->src, src, sizeof(node->src) - 1);
            grub_strncpy(node->dst, dst, sizeof(node->dst) - 1);

            node->next = g_vlnk_list;
            g_vlnk_list = node;
            return 0;            
        }
    }

    return 1;
}

const char *grub_file_get_vlnk(const char *name, int *vlnk)
{
    int len;
    grub_vlnk *node = g_vlnk_list;

    len = grub_strlen(name);

    if (!grub_file_is_vlnk_suffix(name, len))
    {
        return name;
    }

    if (len == g_vtoy_vlnk.srclen && grub_strcmp(name, g_vtoy_vlnk.src) == 0)
    {
        *vlnk = 1;
        return g_vtoy_vlnk.dst; 
    }
    
    while (node)
    {
        if (node->srclen == len && grub_strcmp(name, node->src) == 0)
        {
            *vlnk = 1;
            return node->dst;
        }
        node = node->next;
    }

    return name;
}

grub_file_t
grub_file_open (const char *name, enum grub_file_type type)
{
  int vlnk = 0;
  grub_device_t device = 0;
  grub_file_t file = 0, last_file = 0;
  char *device_name;
  const char *file_name;
  grub_file_filter_id_t filter;

  /* <DESC> : mem:xxx:size:xxx format in chainloader grub_strlen(GRUB_MEMFILE_MEM) */
  if (grub_strncmp(name, GRUB_MEMFILE_MEM, 4) == 0) {
      return grub_memfile_open(name);
  }

  if ((g_vlnk_list || g_vtoy_vlnk.srclen) && (type & GRUB_FILE_TYPE_NO_VLNK) == 0)
    name = grub_file_get_vlnk(name, &vlnk);

  device_name = grub_file_get_device_name (name);
  if (grub_errno)
    goto fail;

  /* Get the file part of NAME.  */
  file_name = (name[0] == '(') ? grub_strchr (name, ')') : NULL;
  if (file_name)
    file_name++;
  else
    file_name = name;

  device = grub_device_open (device_name);
  grub_free (device_name);
  if (! device)
    goto fail;

  file = (grub_file_t) grub_zalloc (sizeof (*file));
  if (! file)
    goto fail;

  file->device = device;
  file->vlnk = vlnk;

  /* In case of relative pathnames and non-Unix systems (like Windows)
   * name of host files may not start with `/'. Blocklists for host files
   * are meaningless as well (for a start, host disk does not allow any direct
   * access - it is just a marker). So skip host disk in this case.
   */
  if (device->disk && file_name[0] != '/'
#if defined(GRUB_UTIL) || defined(GRUB_MACHINE_EMU)
      && grub_strcmp (device->disk->name, "host")
#endif
     )
    /* This is a block list.  */
    file->fs = &grub_fs_blocklist;
  else
    {
      file->fs = grub_fs_probe (device);
      if (! file->fs)
	goto fail;
    }

  if ((file->fs->fs_open) (file, file_name) != GRUB_ERR_NONE)
    goto fail;

  file->name = grub_strdup (name);
  grub_errno = GRUB_ERR_NONE;

  for (filter = 0; file && filter < ARRAY_SIZE (grub_file_filters);
       filter++)
    if (grub_file_filters[filter])
      {
	last_file = file;
	file = grub_file_filters[filter] (file, type);
	if (file && file != last_file)
	  {
	    file->name = grub_strdup (name);
	    grub_errno = GRUB_ERR_NONE;
	  }
      }
  if (!file)
    grub_file_close (last_file);

  return file;

 fail:
  if (device)
    grub_device_close (device);

  /* if (net) grub_net_close (net);  */

  grub_free (file);

  return 0;
}

grub_disk_read_hook_t grub_file_progress_hook;

grub_ssize_t
grub_file_read (grub_file_t file, void *buf, grub_size_t len)
{
  grub_ssize_t res;
  grub_disk_read_hook_t read_hook;
  void *read_hook_data;

  if (file->offset > file->size)
    {
      grub_error (GRUB_ERR_OUT_OF_RANGE,
		  N_("attempt to read past the end of file"));
      return -1;
    }

  if (len == 0)
    return 0;

  if (len > file->size - file->offset)
    len = file->size - file->offset;

  /* Prevent an overflow.  */
  if ((grub_ssize_t) len < 0)
    len >>= 1;

  if (len == 0)
    return 0;

  if (grub_strncmp(file->name, GRUB_MEMFILE_MEM, grub_strlen(GRUB_MEMFILE_MEM)) == 0) {
      grub_memcpy(buf, (grub_uint8_t *)(file->data) + file->offset, len);
      file->offset += len;
      return len;
  }
  
  read_hook = file->read_hook;
  read_hook_data = file->read_hook_data;
  if (!file->read_hook)
    {
      file->read_hook = grub_file_progress_hook;
      file->read_hook_data = file;
      file->progress_offset = file->offset;
    }
  res = (file->fs->fs_read) (file, buf, len);
  file->read_hook = read_hook;
  file->read_hook_data = read_hook_data;
  if (res > 0)
    file->offset += res;

  return res;
}

grub_err_t
grub_file_close (grub_file_t file)
{
  if (file->fs && file->fs->fs_close)
    (file->fs->fs_close) (file);

  if (file->device)
    grub_device_close (file->device);
  grub_free (file->name);
  grub_free (file);
  return grub_errno;
}

grub_off_t
grub_file_seek (grub_file_t file, grub_off_t offset)
{
  grub_off_t old;

  if (offset > file->size)
    {
      grub_error (GRUB_ERR_OUT_OF_RANGE,
		  N_("attempt to seek outside of the file"));
      return -1;
    }
  
  old = file->offset;
  file->offset = offset;
    
  return old;
}
