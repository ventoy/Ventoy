/* grub-setup.c - make GRUB usable */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 1999,2000,2001,2002,2003,2004,2005,2006,2007,2008,2009,2010,2011  Free Software Foundation, Inc.
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

#include <config.h>
#include <grub/types.h>
#include <grub/emu/misc.h>
#include <grub/util/misc.h>
#include <grub/device.h>
#include <grub/disk.h>
#include <grub/file.h>
#include <grub/fs.h>
#include <grub/partition.h>
#include <grub/env.h>
#include <grub/emu/hostdisk.h>
#include <grub/term.h>
#include <grub/i18n.h>

#ifdef GRUB_SETUP_SPARC64
#include <grub/util/ofpath.h>
#include <grub/sparc64/ieee1275/boot.h>
#include <grub/sparc64/ieee1275/kernel.h>
#else
#include <grub/i386/pc/boot.h>
#include <grub/i386/pc/kernel.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <assert.h>
#include <grub/emu/getroot.h>
#include "progname.h"
#include <grub/reed_solomon.h>
#include <grub/msdos_partition.h>
#include <grub/crypto.h>
#include <grub/util/install.h>
#include <grub/emu/hostfile.h>

#include <errno.h>

/* On SPARC this program fills in various fields inside of the 'boot' and 'core'
 * image files.
 *
 * The 'boot' image needs to know the OBP path name of the root
 * device.  It also needs to know the initial block number of
 * 'core' (which is 'diskboot' concatenated with 'kernel' and
 * all the modules, this is created by grub-mkimage).  This resulting
 * 'boot' image is 512 bytes in size and is placed in the second block
 * of a partition.
 *
 * The initial 'diskboot' block acts as a loader for the actual GRUB
 * kernel.  It contains the loading code and then a block list.
 *
 * The block list of 'core' starts at the end of the 'diskboot' image
 * and works it's way backwards towards the end of the code of 'diskboot'.
 *
 * We patch up the images with the necessary values and write out the
 * result.
 */

#ifdef GRUB_SETUP_SPARC64
#define grub_target_to_host16(x)	grub_be_to_cpu16(x)
#define grub_target_to_host32(x)	grub_be_to_cpu32(x)
#define grub_target_to_host64(x)	grub_be_to_cpu64(x)
#define grub_host_to_target16(x)	grub_cpu_to_be16(x)
#define grub_host_to_target32(x)	grub_cpu_to_be32(x)
#define grub_host_to_target64(x)	grub_cpu_to_be64(x)
#elif defined (GRUB_SETUP_BIOS)
#define grub_target_to_host16(x)	grub_le_to_cpu16(x)
#define grub_target_to_host32(x)	grub_le_to_cpu32(x)
#define grub_target_to_host64(x)	grub_le_to_cpu64(x)
#define grub_host_to_target16(x)	grub_cpu_to_le16(x)
#define grub_host_to_target32(x)	grub_cpu_to_le32(x)
#define grub_host_to_target64(x)	grub_cpu_to_le64(x)
#else
#error Complete this
#endif

static void
write_rootdev (grub_device_t root_dev,
	       char *boot_img, grub_uint64_t first_sector)
{
#ifdef GRUB_SETUP_BIOS
  {
    grub_uint8_t *boot_drive;
    void *kernel_sector;
    boot_drive = (grub_uint8_t *) (boot_img + GRUB_BOOT_MACHINE_BOOT_DRIVE);
    kernel_sector = (boot_img + GRUB_BOOT_MACHINE_KERNEL_SECTOR);

    /* FIXME: can this be skipped?  */
    *boot_drive = 0xFF;

    grub_set_unaligned64 (kernel_sector, grub_cpu_to_le64 (first_sector));
  }
#endif
#ifdef GRUB_SETUP_SPARC64
  {
    void *kernel_byte;
    kernel_byte = (boot_img + GRUB_BOOT_AOUT_HEADER_SIZE
		   + GRUB_BOOT_MACHINE_KERNEL_BYTE);
    grub_set_unaligned64 (kernel_byte,
			  grub_cpu_to_be64 (first_sector << GRUB_DISK_SECTOR_BITS));
  }
#endif
}

#ifdef GRUB_SETUP_SPARC64
#define BOOT_SECTOR 1
#else
#define BOOT_SECTOR 0
#endif

/* Helper for setup.  */

struct blocklists
{
  struct grub_boot_blocklist *first_block, *block;
#ifdef GRUB_SETUP_BIOS
  grub_uint16_t current_segment;
#endif
#ifdef GRUB_SETUP_SPARC64
  grub_uint64_t gpt_offset;
#endif
  grub_uint16_t last_length;
  grub_disk_addr_t first_sector;
};

/* Helper for setup.  */
static void
save_blocklists (grub_disk_addr_t sector, unsigned offset, unsigned length,
		 void *data)
{
  struct blocklists *bl = data;
  struct grub_boot_blocklist *prev = bl->block + 1;
  grub_uint64_t seclen;

#ifdef GRUB_SETUP_SPARC64
  sector -= bl->gpt_offset;
#endif

  grub_util_info ("saving <%"  GRUB_HOST_PRIuLONG_LONG ",%u,%u>",
		  (unsigned long long) sector, offset, length);

  if (bl->first_sector == (grub_disk_addr_t) -1)
    {
      if (offset != 0 || length < GRUB_DISK_SECTOR_SIZE)
	grub_util_error ("%s", _("the first sector of the core file is not sector-aligned"));

      bl->first_sector = sector;
      sector++;
      length -= GRUB_DISK_SECTOR_SIZE;
      if (!length)
	return;
    }

  if (offset != 0 || bl->last_length != 0)
    grub_util_error ("%s", _("non-sector-aligned data is found in the core file"));

  seclen = (length + GRUB_DISK_SECTOR_SIZE - 1) >> GRUB_DISK_SECTOR_BITS;

  if (bl->block != bl->first_block
      && (grub_target_to_host64 (prev->start)
	  + grub_target_to_host16 (prev->len)) == sector)
    {
      grub_uint16_t t = grub_target_to_host16 (prev->len);
      t += seclen;
      prev->len = grub_host_to_target16 (t);
    }
  else
    {
      bl->block->start = grub_host_to_target64 (sector);
      bl->block->len = grub_host_to_target16 (seclen);
#ifdef GRUB_SETUP_BIOS
      bl->block->segment = grub_host_to_target16 (bl->current_segment);
#endif

      bl->block--;
      if (bl->block->len)
	grub_util_error ("%s", _("the sectors of the core file are too fragmented"));
    }

  bl->last_length = length & (GRUB_DISK_SECTOR_SIZE - 1);
#ifdef GRUB_SETUP_BIOS
  bl->current_segment += seclen << (GRUB_DISK_SECTOR_BITS - 4);
#endif
}

/* Context for setup/identify_partmap.  */
struct identify_partmap_ctx
{
  grub_partition_map_t dest_partmap;
  grub_partition_t container;
  int multiple_partmaps;
};

/* Helper for setup.
   Unlike root_dev, with dest_dev we're interested in the partition map even
   if dest_dev itself is a whole disk.  */
static int
identify_partmap (grub_disk_t disk __attribute__ ((unused)),
		  const grub_partition_t p, void *data)
{
  struct identify_partmap_ctx *ctx = data;

  if (p->parent != ctx->container)
    return 0;
  /* NetBSD and OpenBSD subpartitions have metadata inside a partition,
     so they are safe to ignore.
   */
  if (grub_strcmp (p->partmap->name, "netbsd") == 0
      || grub_strcmp (p->partmap->name, "openbsd") == 0)
    return 0;
  if (ctx->dest_partmap == NULL)
    {
      ctx->dest_partmap = p->partmap;
      return 0;
    }
  if (ctx->dest_partmap == p->partmap)
    return 0;
  ctx->multiple_partmaps = 1;
  return 1;
}

#ifdef GRUB_SETUP_BIOS
#define SETUP grub_util_bios_setup
#elif GRUB_SETUP_SPARC64
#define SETUP grub_util_sparc_setup
#else
#error "Shouldn't happen"
#endif

void
SETUP (const char *dir,
       const char *boot_file, const char *core_file,
       const char *dest, int force,
       int fs_probe, int allow_floppy,
       int add_rs_codes __attribute__ ((unused))) /* unused on sparc64 */
{
  char *core_path;
  char *boot_img, *core_img, *boot_path;
  char *root = 0;
  size_t boot_size, core_size;
  grub_uint16_t core_sectors;
  grub_device_t root_dev = 0, dest_dev, core_dev;
  grub_util_fd_t fp;
  struct blocklists bl;

  bl.first_sector = (grub_disk_addr_t) -1;

#ifdef GRUB_SETUP_BIOS
  bl.current_segment =
    GRUB_BOOT_I386_PC_KERNEL_SEG + (GRUB_DISK_SECTOR_SIZE >> 4);
#endif
  bl.last_length = 0;

  /* Read the boot image by the OS service.  */
  boot_path = grub_util_get_path (dir, boot_file);
  boot_size = grub_util_get_image_size (boot_path);
  if (boot_size != GRUB_DISK_SECTOR_SIZE)
    grub_util_error (_("the size of `%s' is not %u"),
		     boot_path, GRUB_DISK_SECTOR_SIZE);
  boot_img = grub_util_read_image (boot_path);
  free (boot_path);

  core_path = grub_util_get_path (dir, core_file);
  core_size = grub_util_get_image_size (core_path);
  core_sectors = ((core_size + GRUB_DISK_SECTOR_SIZE - 1)
		  >> GRUB_DISK_SECTOR_BITS);
  if (core_size < GRUB_DISK_SECTOR_SIZE)
    grub_util_error (_("the size of `%s' is too small"), core_path);
#ifdef GRUB_SETUP_BIOS
  if (core_size > 0xFFFF * GRUB_DISK_SECTOR_SIZE)
    grub_util_error (_("the size of `%s' is too large"), core_path);
#endif

  core_img = grub_util_read_image (core_path);

  /* Have FIRST_BLOCK to point to the first blocklist.  */
  bl.first_block = (struct grub_boot_blocklist *) (core_img
						   + GRUB_DISK_SECTOR_SIZE
						   - sizeof (*bl.block));

  grub_util_info ("Opening dest `%s'", dest);
  dest_dev = grub_device_open (dest);
  if (! dest_dev)
    grub_util_error ("%s", grub_errmsg);

  core_dev = dest_dev;

  {
    char **root_devices = grub_guess_root_devices (dir);
    char **cur;
    int found = 0;

    if (!root_devices)
      grub_util_error (_("cannot find a device for %s (is /dev mounted?)"), dir);

    for (cur = root_devices; *cur; cur++)
      {
	char *drive;
	grub_device_t try_dev;

	drive = grub_util_get_grub_dev (*cur);
	if (!drive)
	  continue;
	try_dev = grub_device_open (drive);
	if (! try_dev)
	  {
	    free (drive);
	    continue;
	  }
	if (!found && try_dev->disk->id == dest_dev->disk->id
	    && try_dev->disk->dev->id == dest_dev->disk->dev->id)
	  {
	    if (root_dev)
	      grub_device_close (root_dev);
	    free (root);
	    root_dev = try_dev;
	    root = drive;
	    found = 1;
	    continue;
	  }
	if (!root_dev)
	  {
	    root_dev = try_dev;
	    root = drive;
	    continue;
	  }
	grub_device_close (try_dev);	
	free (drive);
      }
    if (!root_dev)
      {
        root = grub_util_get_grub_dev ("/dev/sda");
        root_dev = grub_device_open (root);
        if (root_dev)
	        grub_util_info ("guessing the root device failed, because of `%s'", grub_errmsg);
	    else
    	    grub_util_error ("guessing the root device failed, because of `%s'", grub_errmsg);
      }
    grub_util_info ("guessed root_dev `%s' from "
		    "dir `%s'", root_dev->disk->name, dir);

    for (cur = root_devices; *cur; cur++)
      free (*cur);
    free (root_devices);
  }

  grub_util_info ("setting the root device to `%s'", root);
  if (grub_env_set ("root", root) != GRUB_ERR_NONE)
    grub_util_error ("%s", grub_errmsg);

  {
#ifdef GRUB_SETUP_BIOS
    char *tmp_img;
    grub_uint8_t *boot_drive_check;

    /* Read the original sector from the disk.  */
    tmp_img = xmalloc (GRUB_DISK_SECTOR_SIZE);
    if (grub_disk_read (dest_dev->disk, 0, 0, GRUB_DISK_SECTOR_SIZE, tmp_img))
      grub_util_error ("%s", grub_errmsg);
    
    boot_drive_check = (grub_uint8_t *) (boot_img
					  + GRUB_BOOT_MACHINE_DRIVE_CHECK);
    /* Copy the possible DOS BPB.  */
    memcpy (boot_img + GRUB_BOOT_MACHINE_BPB_START,
	    tmp_img + GRUB_BOOT_MACHINE_BPB_START,
	    GRUB_BOOT_MACHINE_BPB_END - GRUB_BOOT_MACHINE_BPB_START);

    /* If DEST_DRIVE is a hard disk, enable the workaround, which is
       for buggy BIOSes which don't pass boot drive correctly. Instead,
       they pass 0x00 or 0x01 even when booted from 0x80.  */
    if (!allow_floppy && !grub_util_biosdisk_is_floppy (dest_dev->disk))
      {
	/* Replace the jmp (2 bytes) with double nop's.  */
	boot_drive_check[0] = 0x90;
	boot_drive_check[1] = 0x90;
      }
#endif

    struct identify_partmap_ctx ctx = {
      .dest_partmap = NULL,
      .container = dest_dev->disk->partition,
      .multiple_partmaps = 0
    };
    int is_ldm;
    grub_err_t err;
    grub_disk_addr_t *sectors;
    int i;
    grub_fs_t fs;
    unsigned int nsec, maxsec;

    grub_partition_iterate (dest_dev->disk, identify_partmap, &ctx);

#ifdef GRUB_SETUP_BIOS
    /* Copy the partition table.  */
    if (ctx.dest_partmap ||
        (!allow_floppy && !grub_util_biosdisk_is_floppy (dest_dev->disk)))
      memcpy (boot_img + GRUB_BOOT_MACHINE_WINDOWS_NT_MAGIC,
	      tmp_img + GRUB_BOOT_MACHINE_WINDOWS_NT_MAGIC,
	      GRUB_BOOT_MACHINE_PART_END - GRUB_BOOT_MACHINE_WINDOWS_NT_MAGIC);

    free (tmp_img);
#endif

    if (ctx.container
	&& grub_strcmp (ctx.container->partmap->name, "msdos") == 0
	&& ctx.dest_partmap
	&& (ctx.container->msdostype == GRUB_PC_PARTITION_TYPE_NETBSD
	    || ctx.container->msdostype == GRUB_PC_PARTITION_TYPE_OPENBSD))
      {
	grub_util_warn ("%s", _("Attempting to install GRUB to a disk with multiple partition labels or both partition label and filesystem.  This is not supported yet."));
	goto unable_to_embed;
      }

    fs = grub_fs_probe (dest_dev);
    if (!fs)
      grub_errno = GRUB_ERR_NONE;

    is_ldm = grub_util_is_ldm (dest_dev->disk);

    if (fs_probe)
      {
	if (!fs && !ctx.dest_partmap)
	  grub_util_error (_("unable to identify a filesystem in %s; safety check can't be performed"),
			   dest_dev->disk->name);
	if (fs && !fs->reserved_first_sector)
	  /* TRANSLATORS: Filesystem may reserve the space just GRUB isn't sure about it.  */
	  grub_util_error (_("%s appears to contain a %s filesystem which isn't known to "
			     "reserve space for DOS-style boot.  Installing GRUB there could "
			     "result in FILESYSTEM DESTRUCTION if valuable data is overwritten "
			     "by grub-setup (--skip-fs-probe disables this "
			     "check, use at your own risk)"), dest_dev->disk->name, fs->name);

	if (ctx.dest_partmap && strcmp (ctx.dest_partmap->name, "msdos") != 0
	    && strcmp (ctx.dest_partmap->name, "gpt") != 0
	    && strcmp (ctx.dest_partmap->name, "bsd") != 0
	    && strcmp (ctx.dest_partmap->name, "netbsd") != 0
	    && strcmp (ctx.dest_partmap->name, "openbsd") != 0
	    && strcmp (ctx.dest_partmap->name, "sunpc") != 0)
	  /* TRANSLATORS: Partition map may reserve the space just GRUB isn't sure about it.  */
	  grub_util_error (_("%s appears to contain a %s partition map which isn't known to "
			     "reserve space for DOS-style boot.  Installing GRUB there could "
			     "result in FILESYSTEM DESTRUCTION if valuable data is overwritten "
			     "by grub-setup (--skip-fs-probe disables this "
			     "check, use at your own risk)"), dest_dev->disk->name, ctx.dest_partmap->name);
	if (is_ldm && ctx.dest_partmap && strcmp (ctx.dest_partmap->name, "msdos") != 0
	    && strcmp (ctx.dest_partmap->name, "gpt") != 0)
	  grub_util_error (_("%s appears to contain a %s partition map and "
			     "LDM which isn't known to be a safe combination."
			     "  Installing GRUB there could "
			     "result in FILESYSTEM DESTRUCTION if valuable data"
			     " is overwritten "
			     "by grub-setup (--skip-fs-probe disables this "
			     "check, use at your own risk)"),
			   dest_dev->disk->name, ctx.dest_partmap->name);

      }

    if (! ctx.dest_partmap && ! fs && !is_ldm)
      {
	grub_util_warn ("%s", _("Attempting to install GRUB to a partitionless disk or to a partition.  This is a BAD idea."));
	goto unable_to_embed;
      }
    if (ctx.multiple_partmaps || (ctx.dest_partmap && fs) || (is_ldm && fs))
      {
	grub_util_warn ("%s", _("Attempting to install GRUB to a disk with multiple partition labels.  This is not supported yet."));
	goto unable_to_embed;
      }

    if (ctx.dest_partmap && !ctx.dest_partmap->embed)
      {
	grub_util_warn (_("Partition style `%s' doesn't support embedding"),
			ctx.dest_partmap->name);
	goto unable_to_embed;
      }

    if (fs && !fs->fs_embed)
      {
	grub_util_warn (_("File system `%s' doesn't support embedding"),
			fs->name);
	goto unable_to_embed;
      }

    nsec = core_sectors;

    if (add_rs_codes)
      maxsec = 2 * core_sectors;
    else
      maxsec = core_sectors;

#ifdef GRUB_SETUP_BIOS
    if (maxsec > ((0x78000 - GRUB_KERNEL_I386_PC_LINK_ADDR)
		>> GRUB_DISK_SECTOR_BITS))
      maxsec = ((0x78000 - GRUB_KERNEL_I386_PC_LINK_ADDR)
		>> GRUB_DISK_SECTOR_BITS);
#endif

#ifdef GRUB_SETUP_SPARC64
    /*
     * On SPARC we need two extra. One is because we are combining the
     * core.img with the boot.img. The other is because the boot sector
     * starts at 1.
     */
    nsec += 2;
    maxsec += 2;
#endif

    if (is_ldm)
      err = grub_util_ldm_embed (dest_dev->disk, &nsec, maxsec,
				 GRUB_EMBED_PCBIOS, &sectors);
    else if (ctx.dest_partmap)
      err = ctx.dest_partmap->embed (dest_dev->disk, &nsec, maxsec,
				     GRUB_EMBED_PCBIOS, &sectors);
    else
      err = fs->fs_embed (dest_dev, &nsec, maxsec,
			  GRUB_EMBED_PCBIOS, &sectors);
    if (!err && nsec < core_sectors)
      {
	err = grub_error (GRUB_ERR_OUT_OF_RANGE,
			  N_("Your embedding area is unusually small.  "
			     "core.img won't fit in it."));
      }
    
    if (err)
      {
	grub_util_warn ("%s", grub_errmsg);
	grub_errno = GRUB_ERR_NONE;
	goto unable_to_embed;
      }

    assert (nsec <= maxsec);

    /* Clean out the blocklists.  */
    bl.block = bl.first_block;
    while (bl.block->len)
      {
	grub_memset (bl.block, 0, sizeof (*bl.block));
      
	bl.block--;

	if ((char *) bl.block <= core_img)
	  grub_util_error ("%s", _("no terminator in the core image"));
      }

    bl.block = bl.first_block;
    for (i = 0; i < nsec; i++)
      save_blocklists (sectors[i] + grub_partition_get_start (ctx.container),
		       0, GRUB_DISK_SECTOR_SIZE, &bl);

    /* Make sure that the last blocklist is a terminator.  */
    if (bl.block == bl.first_block)
      bl.block--;
    bl.block->start = 0;
    bl.block->len = 0;
#ifdef GRUB_SETUP_BIOS
    bl.block->segment = 0;
#endif

#ifdef GRUB_SETUP_SPARC64
    {
      /*
       * On SPARC, the block-list entries need to be based off the beginning
       * of the parition, not the beginning of the disk.
       */
      struct grub_boot_blocklist *block;
      block = bl.first_block;

      while (block->len)
        {
          block->start -= bl.first_sector;
          block--;
        }
    }

    /*
     * Reserve space for the boot block since it can not be in the
     * Parition table on SPARC.
     */
    assert (bl.first_block->len > 2);
    bl.first_block->start += 2;
    bl.first_block->len -= 2;
    write_rootdev (root_dev, boot_img, sectors[BOOT_SECTOR + 1] - bl.first_sector);
#endif

#ifdef GRUB_SETUP_BIOS
    write_rootdev (root_dev, boot_img, bl.first_sector);
#endif

    /* Round up to the nearest sector boundary, and zero the extra memory */
    core_img = xrealloc (core_img, nsec * GRUB_DISK_SECTOR_SIZE);
    assert (core_img && (nsec * GRUB_DISK_SECTOR_SIZE >= core_size));
    memset (core_img + core_size, 0, nsec * GRUB_DISK_SECTOR_SIZE - core_size);

    bl.first_block = (struct grub_boot_blocklist *) (core_img
						     + GRUB_DISK_SECTOR_SIZE
						     - sizeof (*bl.block));
#if GRUB_SETUP_BIOS
    grub_size_t no_rs_length;
    no_rs_length = grub_target_to_host16 
      (grub_get_unaligned16 (core_img
			     + GRUB_DISK_SECTOR_SIZE
			     + GRUB_KERNEL_I386_PC_NO_REED_SOLOMON_LENGTH));

    if (no_rs_length == 0xffff)
      grub_util_error ("%s", _("core.img version mismatch"));

    if (add_rs_codes)
      {
	grub_set_unaligned32 ((core_img + GRUB_DISK_SECTOR_SIZE
			       + GRUB_KERNEL_I386_PC_REED_SOLOMON_REDUNDANCY),
			      grub_host_to_target32 (nsec * GRUB_DISK_SECTOR_SIZE - core_size));

	void *tmp = xmalloc (core_size);
	grub_memcpy (tmp, core_img, core_size);
	grub_reed_solomon_add_redundancy (core_img + no_rs_length + GRUB_DISK_SECTOR_SIZE,
					  core_size - no_rs_length - GRUB_DISK_SECTOR_SIZE,
					  nsec * GRUB_DISK_SECTOR_SIZE
					  - core_size);
	assert (grub_memcmp (tmp, core_img, core_size) == 0);
	free (tmp);
      }

    /* Write the core image onto the disk.  */
    for (i = 0; i < nsec; i++)
      grub_disk_write (dest_dev->disk, sectors[i], 0,
		       GRUB_DISK_SECTOR_SIZE,
		       core_img + i * GRUB_DISK_SECTOR_SIZE);
#endif

#ifdef GRUB_SETUP_SPARC64
    {
      int isec = BOOT_SECTOR;

      /* Write the boot image onto the disk. */
      if (grub_disk_write (dest_dev->disk, sectors[isec++], 0,
                           GRUB_DISK_SECTOR_SIZE, boot_img))
        grub_util_error ("%s", grub_errmsg);

      /* Write the core image onto the disk. */
      for (i = 0 ; isec < nsec; i++, isec++)
        {
          if (grub_disk_write (dest_dev->disk, sectors[isec], 0,
                               GRUB_DISK_SECTOR_SIZE,
                               core_img  + i * GRUB_DISK_SECTOR_SIZE))
            grub_util_error ("%s", grub_errmsg);
        }
    }

#endif
    grub_free (sectors);

    goto finish;
  }

unable_to_embed:

  if (dest_dev->disk->dev->id != root_dev->disk->dev->id)
    grub_util_error ("%s", _("embedding is not possible, but this is required for "
			     "RAID and LVM install"));

  {
    grub_fs_t fs;
    fs = grub_fs_probe (root_dev);
    if (!fs)
      grub_util_error (_("can't determine filesystem on %s"), root);

    if (!fs->blocklist_install)
      grub_util_error (_("filesystem `%s' doesn't support blocklists"),
		       fs->name);
  }

#ifdef GRUB_SETUP_BIOS
  if (dest_dev->disk->id != root_dev->disk->id
      || dest_dev->disk->dev->id != root_dev->disk->dev->id)
    /* TRANSLATORS: cross-disk refers to /boot being on one disk
       but MBR on another.  */
    grub_util_error ("%s", _("embedding is not possible, but this is required for "
			     "cross-disk install"));
#else
  core_dev = root_dev;
#endif

  grub_util_warn ("%s", _("Embedding is not possible.  GRUB can only be installed in this "
			  "setup by using blocklists.  However, blocklists are UNRELIABLE and "
			  "their use is discouraged."));
  if (! force)
    /* TRANSLATORS: Here GRUB refuses to continue with blocklist install.  */
    grub_util_error ("%s", _("will not proceed with blocklists"));

  /* The core image must be put on a filesystem unfortunately.  */
  grub_util_info ("will leave the core image on the filesystem");

  grub_util_biosdisk_flush (root_dev->disk);

  /* Clean out the blocklists.  */
  bl.block = bl.first_block;
  while (bl.block->len)
    {
      bl.block->start = 0;
      bl.block->len = 0;
#ifdef GRUB_SETUP_BIOS
      bl.block->segment = 0;
#endif

      bl.block--;

      if ((char *) bl.block <= core_img)
	grub_util_error ("%s", _("no terminator in the core image"));
    }

  bl.block = bl.first_block;

#ifdef GRUB_SETUP_SPARC64
  {
    grub_partition_t container = root_dev->disk->partition;
    bl.gpt_offset = 0;

    if (grub_strstr (container->partmap->name, "gpt"))
      bl.gpt_offset = grub_partition_get_start (container);
  }
#endif

  grub_install_get_blocklist (root_dev, core_path, core_img, core_size,
			      save_blocklists, &bl);

  if (bl.first_sector == (grub_disk_addr_t)-1)
    grub_util_error ("%s", _("can't retrieve blocklists"));

#ifdef GRUB_SETUP_SPARC64
  {
    char *boot_devpath;
    boot_devpath = (char *) (boot_img
			     + GRUB_BOOT_AOUT_HEADER_SIZE
			     + GRUB_BOOT_MACHINE_BOOT_DEVPATH);
    if (dest_dev->disk->id != root_dev->disk->id
	|| dest_dev->disk->dev->id != root_dev->disk->dev->id)
      {
	char *dest_ofpath;
	dest_ofpath
	  = grub_util_devname_to_ofpath (grub_util_biosdisk_get_osdev (root_dev->disk));
	/* FIXME handle NULL result */
	grub_util_info ("dest_ofpath is `%s'", dest_ofpath);
	strncpy (boot_devpath, dest_ofpath,
		 GRUB_BOOT_MACHINE_BOOT_DEVPATH_END
		 - GRUB_BOOT_MACHINE_BOOT_DEVPATH - 1);
	boot_devpath[GRUB_BOOT_MACHINE_BOOT_DEVPATH_END
		   - GRUB_BOOT_MACHINE_BOOT_DEVPATH - 1] = 0;
	free (dest_ofpath);
      }
    else
      {
	grub_util_info ("non cross-disk install");
	memset (boot_devpath, 0, GRUB_BOOT_MACHINE_BOOT_DEVPATH_END
		- GRUB_BOOT_MACHINE_BOOT_DEVPATH);
      }
    grub_util_info ("boot device path %s", boot_devpath);
  }
#endif

  write_rootdev (root_dev, boot_img, bl.first_sector);

  /* Write the first two sectors of the core image onto the disk.  */
  grub_util_info ("opening the core image `%s'", core_path);
  fp = grub_util_fd_open (core_path, GRUB_UTIL_FD_O_WRONLY);
  if (! GRUB_UTIL_FD_IS_VALID (fp))
    grub_util_error (_("cannot open `%s': %s"), core_path,
		     grub_util_fd_strerror ());

  if (grub_util_fd_write (fp, core_img, GRUB_DISK_SECTOR_SIZE * 2)
      != GRUB_DISK_SECTOR_SIZE * 2)
    grub_util_error (_("cannot write to `%s': %s"),
		     core_path, strerror (errno));
  if (grub_util_fd_sync (fp) < 0)
    grub_util_error (_("cannot sync `%s': %s"), core_path, strerror (errno));
  if (grub_util_fd_close (fp) < 0)
    grub_util_error (_("cannot close `%s': %s"), core_path, strerror (errno));
  grub_util_biosdisk_flush (root_dev->disk);

  grub_disk_cache_invalidate_all ();

  {
    char *buf, *ptr = core_img;
    size_t len = core_size;
    grub_uint64_t blk, offset = 0;
    grub_partition_t container = core_dev->disk->partition;
    grub_err_t err;

    core_dev->disk->partition = 0;
#ifdef GRUB_SETUP_SPARC64
    offset = bl.gpt_offset;
#endif

    buf = xmalloc (core_size);
    blk = bl.first_sector;
    err = grub_disk_read (core_dev->disk, blk + offset, 0, GRUB_DISK_SECTOR_SIZE, buf);
    if (err)
      grub_util_error (_("cannot read `%s': %s"), core_dev->disk->name,
		       grub_errmsg);
    if (grub_memcmp (buf, ptr, GRUB_DISK_SECTOR_SIZE) != 0)
      grub_util_error ("%s", _("blocklists are invalid"));

    ptr += GRUB_DISK_SECTOR_SIZE;
    len -= GRUB_DISK_SECTOR_SIZE;

    bl.block = bl.first_block;
    while (bl.block->len)
      {
	size_t cur = grub_target_to_host16 (bl.block->len) << GRUB_DISK_SECTOR_BITS;
	blk = grub_target_to_host64 (bl.block->start);

	if (cur > len)
	  cur = len;

	err = grub_disk_read (core_dev->disk, blk + offset, 0, cur, buf);
	if (err)
	  grub_util_error (_("cannot read `%s': %s"), core_dev->disk->name,
			   grub_errmsg);

	if (grub_memcmp (buf, ptr, cur) != 0)
	  grub_util_error ("%s", _("blocklists are invalid"));

	ptr += cur;
	len -= cur;
	bl.block--;
	
	if ((char *) bl.block <= core_img)
	  grub_util_error ("%s", _("no terminator in the core image"));
      }
    if (len)
      grub_util_error ("%s", _("blocklists are incomplete"));
    core_dev->disk->partition = container;
    free (buf);
  }

#ifdef GRUB_SETUP_BIOS
 finish:
#endif

  /* Write the boot image onto the disk.  */
  if (grub_disk_write (dest_dev->disk, BOOT_SECTOR,
		       0, GRUB_DISK_SECTOR_SIZE, boot_img))
    grub_util_error ("%s", grub_errmsg);

#ifdef GRUB_SETUP_SPARC64
 finish:
#endif

  grub_util_biosdisk_flush (root_dev->disk);
  grub_util_biosdisk_flush (dest_dev->disk);

  free (core_path);
  free (core_img);
  free (boot_img);
  grub_device_close (dest_dev);
  grub_device_close (root_dev);
}

