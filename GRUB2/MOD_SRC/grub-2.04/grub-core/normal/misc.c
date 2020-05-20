/* misc.c - miscellaneous functions */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2005,2007,2008,2009  Free Software Foundation, Inc.
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
#include <grub/disk.h>
#include <grub/fs.h>
#include <grub/err.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/datetime.h>
#include <grub/term.h>
#include <grub/i18n.h>
#include <grub/partition.h>

static const char *grub_human_sizes[3][6] =
  {
    /* This algorithm in reality would work only up to (2^64) / 100 B = 81 PiB.
       Put here all possible suffixes it can produce so no array bounds check
       is needed.
     */
    /* TRANSLATORS: that's the list of binary unit prefixes.  */
    { N_("B"),   N_("KiB"),   N_("MiB"),   N_("GiB"),   N_("TiB"),   N_("PiB")},
    /* TRANSLATORS: that's the list of binary unit prefixes.  */
    {    "",     N_("KB"),     N_("MB"),     N_("GB"),     N_("TB"),     N_("PB") },
    /* TRANSLATORS: that's the list of binary unit prefixes.  */
    { N_("B/s"), N_("KiB/s"), N_("MiB/s"), N_("GiB/s"), N_("TiB/s"), N_("PiB/s"),  },    
  };

const char *
grub_get_human_size (grub_uint64_t size, enum grub_human_size_type type)
{
  grub_uint64_t fsize;
  unsigned units = 0;
  static char buf[30];
  const char *umsg;

  if (type != GRUB_HUMAN_SIZE_SPEED)
    fsize = size * 100ULL;
  else
    fsize = size;

  /* Since 2^64 / 1024^5  < 102400, this can give at most 5 iterations.
     So units <=5, so impossible to go past the end of array.
   */
  while (fsize >= 102400)
    {
      fsize = (fsize + 512) / 1024;
      units++;
    }

  umsg = _(grub_human_sizes[type][units]);

  if (units || type == GRUB_HUMAN_SIZE_SPEED)
    {
      grub_uint64_t whole, fraction;

      whole = grub_divmod64 (fsize, 100, &fraction);
      grub_snprintf (buf, sizeof (buf),
		     "%" PRIuGRUB_UINT64_T
		     ".%02" PRIuGRUB_UINT64_T "%s", whole, fraction,
		     umsg);
    }
  else
    grub_snprintf (buf, sizeof (buf), "%llu%s", (unsigned long long) size,
		   umsg);
  return buf;
}

/* Print the information on the device NAME.  */
grub_err_t
grub_normal_print_device_info (const char *name)
{
  grub_device_t dev;
  char *p;

  p = grub_strchr (name, ',');
  if (p)
    {
      grub_xputs ("\t");
      grub_printf_ (N_("Partition %s:"), name);
      grub_xputs (" ");
    }
  else
    {
      grub_printf_ (N_("Device %s:"), name);
      grub_xputs (" ");
    }

  dev = grub_device_open (name);
  if (! dev)
    grub_printf ("%s", _("Filesystem cannot be accessed"));
  else if (dev->disk)
    {
      grub_fs_t fs;

      fs = grub_fs_probe (dev);
      /* Ignore all errors.  */
      grub_errno = 0;

      if (fs)
	{
	  const char *fsname = fs->name;
	  if (grub_strcmp (fsname, "ext2") == 0)
	    fsname = "ext*";
	  grub_printf_ (N_("Filesystem type %s"), fsname);
	  if (fs->fs_label)
	    {
	      char *label;
	      (fs->fs_label) (dev, &label);
	      if (grub_errno == GRUB_ERR_NONE)
		{
		  if (label && grub_strlen (label))
		    {
		      grub_xputs (" ");
		      grub_printf_ (N_("- Label `%s'"), label);
		    }
		  grub_free (label);
		}
	      grub_errno = GRUB_ERR_NONE;
	    }
	  if (fs->fs_mtime)
	    {
	      grub_int32_t tm;
	      struct grub_datetime datetime;
	      (fs->fs_mtime) (dev, &tm);
	      if (grub_errno == GRUB_ERR_NONE)
		{
		  grub_unixtime2datetime (tm, &datetime);
		  grub_xputs (" ");
		  /* TRANSLATORS: Arguments are year, month, day, hour, minute,
		     second, day of the week (translated).  */
		  grub_printf_ (N_("- Last modification time %d-%02d-%02d "
			       "%02d:%02d:%02d %s"),
			       datetime.year, datetime.month, datetime.day,
			       datetime.hour, datetime.minute, datetime.second,
			       grub_get_weekday_name (&datetime));

		}
	      grub_errno = GRUB_ERR_NONE;
	    }
	  if (fs->fs_uuid)
	    {
	      char *uuid;
	      (fs->fs_uuid) (dev, &uuid);
	      if (grub_errno == GRUB_ERR_NONE)
		{
		  if (uuid && grub_strlen (uuid))
		    grub_printf (", UUID %s", uuid);
		  grub_free (uuid);
		}
	      grub_errno = GRUB_ERR_NONE;
	    }
	}
      else
	grub_printf ("%s", _("No known filesystem detected"));

      if (dev->disk->partition)
	grub_printf (_(" - Partition start at %llu%sKiB"),
		     (unsigned long long) (grub_partition_get_start (dev->disk->partition) >> 1),
		     (grub_partition_get_start (dev->disk->partition) & 1) ? ".5" : "" );
      else
	grub_printf_ (N_(" - Sector size %uB"), 1 << dev->disk->log_sector_size);
      if (grub_disk_get_size (dev->disk) == GRUB_DISK_SIZE_UNKNOWN)
	grub_puts_ (N_(" - Total size unknown"));
      else
	grub_printf (_(" - Total size %llu%sKiB"),
		     (unsigned long long) (grub_disk_get_size (dev->disk) >> 1),
		     /* TRANSLATORS: Replace dot with appropriate decimal separator for
			your language.  */
		     (grub_disk_get_size (dev->disk) & 1) ? _(".5") : "");
    }

  if (dev)
    grub_device_close (dev);

  grub_xputs ("\n");
  return grub_errno;
}
