/* blocklist.c - print the block list of a file */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007  Free Software Foundation, Inc.
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
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/mm.h>
#include <grub/disk.h>
#include <grub/partition.h>
#include <grub/command.h>
#include <grub/i18n.h>

GRUB_MOD_LICENSE ("GPLv3+");

/* Context for grub_cmd_blocklist.  */
struct blocklist_ctx
{
  unsigned long start_sector;
  unsigned num_sectors;
  int num_entries;
  grub_disk_addr_t part_start;
};

/* Helper for grub_cmd_blocklist.  */
static void
print_blocklist (grub_disk_addr_t sector, unsigned num,
		 unsigned offset, unsigned length, struct blocklist_ctx *ctx)
{
  if (ctx->num_entries++)
    grub_printf (",");

  grub_printf ("%llu", (unsigned long long) (sector - ctx->part_start));
  if (num > 0)
    grub_printf ("+%u", num);
  if (offset != 0 || length != 0)
    grub_printf ("[%u-%u]", offset, offset + length);
}

/* Helper for grub_cmd_blocklist.  */
static void
read_blocklist (grub_disk_addr_t sector, unsigned offset, unsigned length,
		void *data)
{
  struct blocklist_ctx *ctx = data;

  if (ctx->num_sectors > 0)
    {
      if (ctx->start_sector + ctx->num_sectors == sector
	  && offset == 0 && length >= GRUB_DISK_SECTOR_SIZE)
	{
	  ctx->num_sectors += length >> GRUB_DISK_SECTOR_BITS;
	  sector += length >> GRUB_DISK_SECTOR_BITS;
	  length &= (GRUB_DISK_SECTOR_SIZE - 1);
	}

      if (!length)
	return;
      print_blocklist (ctx->start_sector, ctx->num_sectors, 0, 0, ctx);
      ctx->num_sectors = 0;
    }

  if (offset)
    {
      unsigned l = length + offset;
      l &= (GRUB_DISK_SECTOR_SIZE - 1);
      l -= offset;
      print_blocklist (sector, 0, offset, l, ctx);
      length -= l;
      sector++;
      offset = 0;
    }

  if (!length)
    return;

  if (length & (GRUB_DISK_SECTOR_SIZE - 1))
    {
      if (length >> GRUB_DISK_SECTOR_BITS)
	{
	  print_blocklist (sector, length >> GRUB_DISK_SECTOR_BITS, 0, 0, ctx);
	  sector += length >> GRUB_DISK_SECTOR_BITS;
	}
      print_blocklist (sector, 0, 0, length & (GRUB_DISK_SECTOR_SIZE - 1), ctx);
    }
  else
    {
      ctx->start_sector = sector;
      ctx->num_sectors = length >> GRUB_DISK_SECTOR_BITS;
    }
}

static grub_err_t
grub_cmd_blocklist (grub_command_t cmd __attribute__ ((unused)),
		    int argc, char **args)
{
  grub_file_t file;
  char buf[GRUB_DISK_SECTOR_SIZE];
  struct blocklist_ctx ctx = {
    .start_sector = 0,
    .num_sectors = 0,
    .num_entries = 0,
    .part_start = 0
  };

  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  file = grub_file_open (args[0], GRUB_FILE_TYPE_PRINT_BLOCKLIST
			 | GRUB_FILE_TYPE_NO_DECOMPRESS);
  if (! file)
    return grub_errno;

  if (! file->device->disk)
    return grub_error (GRUB_ERR_BAD_DEVICE,
		       "this command is available only for disk devices");

  ctx.part_start = grub_partition_get_start (file->device->disk->partition);

  file->read_hook = read_blocklist;
  file->read_hook_data = &ctx;

  while (grub_file_read (file, buf, sizeof (buf)) > 0)
    ;

  if (ctx.num_sectors > 0)
    print_blocklist (ctx.start_sector, ctx.num_sectors, 0, 0, &ctx);

  grub_printf("\nentry number:%d \n", ctx.num_entries);

  grub_file_close (file);

  return grub_errno;
}

static grub_command_t cmd;

GRUB_MOD_INIT(blocklist)
{
  cmd = grub_register_command ("blocklist", grub_cmd_blocklist,
			       N_("FILE"), N_("Print a block list."));
}

GRUB_MOD_FINI(blocklist)
{
  grub_unregister_command (cmd);
}
