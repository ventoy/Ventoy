/* mdraid_linux.c - module to handle Linux Software RAID.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008,2009,2010  Free Software Foundation, Inc.
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

#include <grub/dl.h>
#include <grub/disk.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/misc.h>
#include <grub/diskfilter.h>

GRUB_MOD_LICENSE ("GPLv3+");

/* Linux RAID on disk structures and constants,
   copied from include/linux/raid/md_p.h.  */

#define SB_MAGIC			0xa92b4efc

/*
 * The version-1 superblock :
 * All numeric fields are little-endian.
 *
 * Total size: 256 bytes plus 2 per device.
 * 1K allows 384 devices.
 */

struct grub_raid_super_1x
{
  /* Constant array information - 128 bytes.  */
  grub_uint32_t magic;		/* MD_SB_MAGIC: 0xa92b4efc - little endian.  */
  grub_uint32_t major_version;	/* 1.  */
  grub_uint32_t feature_map;	/* Bit 0 set if 'bitmap_offset' is meaningful.   */
  grub_uint32_t pad0;		/* Always set to 0 when writing.  */

  grub_uint8_t set_uuid[16];	/* User-space generated.  */
  char set_name[32];		/* Set and interpreted by user-space.  */

  grub_uint64_t ctime;		/* Lo 40 bits are seconds, top 24 are microseconds or 0.  */
  grub_uint32_t level;		/* -4 (multipath), -1 (linear), 0,1,4,5.  */
  grub_uint32_t layout;		/* only for raid5 and raid10 currently.  */
  grub_uint64_t size;		/* Used size of component devices, in 512byte sectors.  */

  grub_uint32_t chunksize;	/* In 512byte sectors.  */
  grub_uint32_t raid_disks;
  grub_uint32_t bitmap_offset;	/* Sectors after start of superblock that bitmap starts
				 * NOTE: signed, so bitmap can be before superblock
				 * only meaningful of feature_map[0] is set.
				 */

  /* These are only valid with feature bit '4'.  */
  grub_uint32_t new_level;	/* New level we are reshaping to.  */
  grub_uint64_t reshape_position;	/* Next address in array-space for reshape.  */
  grub_uint32_t delta_disks;	/* Change in number of raid_disks.  */
  grub_uint32_t new_layout;	/* New layout.  */
  grub_uint32_t new_chunk;	/* New chunk size (512byte sectors).  */
  grub_uint8_t pad1[128 - 124];	/* Set to 0 when written.  */

  /* Constant this-device information - 64 bytes.  */
  grub_uint64_t data_offset;	/* Sector start of data, often 0.  */
  grub_uint64_t data_size;	/* Sectors in this device that can be used for data.  */
  grub_uint64_t super_offset;	/* Sector start of this superblock.  */
  grub_uint64_t recovery_offset;	/* Sectors before this offset (from data_offset) have been recovered.  */
  grub_uint32_t dev_number;	/* Permanent identifier of this  device - not role in raid.  */
  grub_uint32_t cnt_corrected_read;	/* Number of read errors that were corrected by re-writing.  */
  grub_uint8_t device_uuid[16];	/* User-space setable, ignored by kernel.  */
  grub_uint8_t devflags;	/* Per-device flags.  Only one defined...  */
  grub_uint8_t pad2[64 - 57];	/* Set to 0 when writing.  */

  /* Array state information - 64 bytes.  */
  grub_uint64_t utime;		/* 40 bits second, 24 btes microseconds.  */
  grub_uint64_t events;		/* Incremented when superblock updated.  */
  grub_uint64_t resync_offset;	/* Data before this offset (from data_offset) known to be in sync.  */
  grub_uint32_t sb_csum;	/* Checksum upto devs[max_dev].  */
  grub_uint32_t max_dev;	/* Size of devs[] array to consider.  */
  grub_uint8_t pad3[64 - 32];	/* Set to 0 when writing.  */

  /* Device state information. Indexed by dev_number.
   * 2 bytes per device.
   * Note there are no per-device state flags. State information is rolled
   * into the 'roles' value.  If a device is spare or faulty, then it doesn't
   * have a meaningful role.
   */
  grub_uint16_t dev_roles[0];	/* Role in array, or 0xffff for a spare, or 0xfffe for faulty.  */
};
/* Could be GRUB_PACKED, but since all members in this struct
   are already appropriately aligned, we can omit this and avoid suboptimal
   assembly in some cases.  */

#define WriteMostly1    1	/* Mask for writemostly flag in above devflags.  */

static struct grub_diskfilter_vg *
grub_mdraid_detect (grub_disk_t disk,
		    struct grub_diskfilter_pv_id *id,
		    grub_disk_addr_t *start_sector)
{
  grub_uint64_t size;
  grub_uint8_t minor_version;

  size = grub_disk_get_size (disk);

  /* Check for an 1.x superblock.
   * It's always aligned to a 4K boundary
   * and depending on the minor version it can be:
   * 0: At least 8K, but less than 12K, from end of device
   * 1: At start of device
   * 2: 4K from start of device.
   */

  for (minor_version = 0; minor_version < 3; ++minor_version)
    {
      grub_disk_addr_t sector = 0;
      struct grub_raid_super_1x sb;
      grub_uint16_t role;
      grub_uint32_t level;
      struct grub_diskfilter_vg *array;
      char *uuid;
	
      if (size == GRUB_DISK_SIZE_UNKNOWN && minor_version == 0)
	continue;
	
      switch (minor_version)
	{
	case 0:
	  sector = (size - 8 * 2) & ~(4 * 2 - 1);
	  break;
	case 1:
	  sector = 0;
	  break;
	case 2:
	  sector = 4 * 2;
	  break;
	}

      if (grub_disk_read (disk, sector, 0, sizeof (struct grub_raid_super_1x),
			  &sb))
	return NULL;

      if (sb.magic != grub_cpu_to_le32_compile_time (SB_MAGIC)
	  || grub_le_to_cpu64 (sb.super_offset) != sector)
	continue;

      if (sb.major_version != grub_cpu_to_le32_compile_time (1))
	/* Unsupported version.  */
	return NULL;

      level = grub_le_to_cpu32 (sb.level);

      /* Multipath.  */
      if ((int) level == -4)
	level = 1;

      if (level != 0 && level != 1 && level != 4 &&
	  level != 5 && level != 6 && level != 10)
	{
	  grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET,
		      "Unsupported RAID level: %d", sb.level);
	  return NULL;
	}

      if (grub_le_to_cpu32 (sb.dev_number) >=
	  grub_le_to_cpu32 (sb.max_dev))
	/* Spares aren't implemented.  */
	return NULL;

      if (grub_disk_read (disk, sector, 
			  (char *) (sb.dev_roles + grub_le_to_cpu32 (sb.dev_number))
			  - (char *) &sb,
			  sizeof (role), &role))
	return NULL;

      if (grub_le_to_cpu16 (role)
	  >= grub_le_to_cpu32 (sb.raid_disks))
	/* Spares aren't implemented.  */
	return NULL;

      id->uuidlen = 0;
      id->id = grub_le_to_cpu16 (role);

      uuid = grub_malloc (16);
      if (!uuid)
	return NULL;

      grub_memcpy (uuid, sb.set_uuid, 16);

      *start_sector = grub_le_to_cpu64 (sb.data_offset);

      array = grub_diskfilter_make_raid (16, uuid,
					 grub_le_to_cpu32 (sb.raid_disks),
					 sb.set_name,
					 (sb.size)
					 ? grub_le_to_cpu64 (sb.size) 
					 : grub_le_to_cpu64 (sb.data_size),
					 grub_le_to_cpu32 (sb.chunksize),
					 grub_le_to_cpu32 (sb.layout),
					 grub_le_to_cpu32 (sb.level));

      return array;
    }

  /* not 1.x raid.  */
  return NULL;
}

static struct grub_diskfilter grub_mdraid_dev = {
  .name = "mdraid1x",
  .detect = grub_mdraid_detect,
  .next = 0
};

GRUB_MOD_INIT (mdraid1x)
{
  grub_diskfilter_register_front (&grub_mdraid_dev);
}

GRUB_MOD_FINI (mdraid1x)
{
  grub_diskfilter_unregister (&grub_mdraid_dev);
}
