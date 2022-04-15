/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2003,2004,2006,2007,2008,2009,2010  Free Software Foundation, Inc.
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

#include <grub/disk.h>
#include <grub/err.h>
#include <grub/mm.h>
#include <grub/types.h>
#include <grub/partition.h>
#include <grub/misc.h>
#include <grub/time.h>
#include <grub/file.h>
#include <grub/i18n.h>
#include <grub/ventoy.h>

#define	GRUB_CACHE_TIMEOUT	10

/* The last time the disk was used.  */
static grub_uint64_t grub_last_time = 0;

struct grub_disk_cache grub_disk_cache_table[GRUB_DISK_CACHE_NUM];

void (*grub_disk_firmware_fini) (void);
int grub_disk_firmware_is_tainted;

#if DISK_CACHE_STATS
static unsigned long grub_disk_cache_hits;
static unsigned long grub_disk_cache_misses;

void
grub_disk_cache_get_performance (unsigned long *hits, unsigned long *misses)
{
  *hits = grub_disk_cache_hits;
  *misses = grub_disk_cache_misses;
}
#endif

grub_err_t (*grub_disk_write_weak) (grub_disk_t disk,
				    grub_disk_addr_t sector,
				    grub_off_t offset,
				    grub_size_t size,
				    const void *buf);
#include "disk_common.c"

void
grub_disk_cache_invalidate_all (void)
{
  unsigned i;

  for (i = 0; i < GRUB_DISK_CACHE_NUM; i++)
    {
      struct grub_disk_cache *cache = grub_disk_cache_table + i;

      if (cache->data && ! cache->lock)
	{
	  grub_free (cache->data);
	  cache->data = 0;
	}
    }
}

static char *
grub_disk_cache_fetch (unsigned long dev_id, unsigned long disk_id,
		       grub_disk_addr_t sector)
{
  struct grub_disk_cache *cache;
  unsigned cache_index;

  cache_index = grub_disk_cache_get_index (dev_id, disk_id, sector);
  cache = grub_disk_cache_table + cache_index;

  if (cache->dev_id == dev_id && cache->disk_id == disk_id
      && cache->sector == sector)
    {
      cache->lock = 1;
#if DISK_CACHE_STATS
      grub_disk_cache_hits++;
#endif
      return cache->data;
    }

#if DISK_CACHE_STATS
  grub_disk_cache_misses++;
#endif

  return 0;
}

static void
grub_disk_cache_unlock (unsigned long dev_id, unsigned long disk_id,
			grub_disk_addr_t sector)
{
  struct grub_disk_cache *cache;
  unsigned cache_index;

  cache_index = grub_disk_cache_get_index (dev_id, disk_id, sector);
  cache = grub_disk_cache_table + cache_index;

  if (cache->dev_id == dev_id && cache->disk_id == disk_id
      && cache->sector == sector)
    cache->lock = 0;
}

static grub_err_t
grub_disk_cache_store (unsigned long dev_id, unsigned long disk_id,
		       grub_disk_addr_t sector, const char *data)
{
  unsigned cache_index;
  struct grub_disk_cache *cache;

  cache_index = grub_disk_cache_get_index (dev_id, disk_id, sector);
  cache = grub_disk_cache_table + cache_index;

  cache->lock = 1;
  grub_free (cache->data);
  cache->data = 0;
  cache->lock = 0;

  cache->data = grub_malloc (GRUB_DISK_SECTOR_SIZE << GRUB_DISK_CACHE_BITS);
  if (! cache->data)
    return grub_errno;

  grub_memcpy (cache->data, data,
	       GRUB_DISK_SECTOR_SIZE << GRUB_DISK_CACHE_BITS);
  cache->dev_id = dev_id;
  cache->disk_id = disk_id;
  cache->sector = sector;

  return GRUB_ERR_NONE;
}



grub_disk_dev_t grub_disk_dev_list;

void
grub_disk_dev_register (grub_disk_dev_t dev)
{
  dev->next = grub_disk_dev_list;
  grub_disk_dev_list = dev;
}

void
grub_disk_dev_unregister (grub_disk_dev_t dev)
{
  grub_disk_dev_t *p, q;

  for (p = &grub_disk_dev_list, q = *p; q; p = &(q->next), q = q->next)
    if (q == dev)
      {
        *p = q->next;
	break;
      }
}

/* Return the location of the first ',', if any, which is not
   escaped by a '\'.  */
static const char *
find_part_sep (const char *name)
{
  const char *p = name;
  char c;

  while ((c = *p++) != '\0')
    {
      if (c == '\\' && *p == ',')
	p++;
      else if (c == ',')
	return p - 1;
    }
  return NULL;
}

grub_disk_t
grub_disk_open (const char *name)
{
  const char *p;
  grub_disk_t disk;
  grub_disk_dev_t dev;
  char *raw = (char *) name;
  grub_uint64_t current_time;

  grub_dprintf ("disk", "Opening `%s'...\n", name);

  disk = (grub_disk_t) grub_zalloc (sizeof (*disk));
  if (! disk)
    return 0;
  disk->log_sector_size = GRUB_DISK_SECTOR_BITS;
  /* Default 1MiB of maximum agglomerate.  */
  disk->max_agglomerate = 1048576 >> (GRUB_DISK_SECTOR_BITS
				      + GRUB_DISK_CACHE_BITS);

  p = find_part_sep (name);
  if (p)
    {
      grub_size_t len = p - name;

      raw = grub_malloc (len + 1);
      if (! raw)
	goto fail;

      grub_memcpy (raw, name, len);
      raw[len] = '\0';
      disk->name = grub_strdup (raw);
    }
  else
    disk->name = grub_strdup (name);
  if (! disk->name)
    goto fail;

  for (dev = grub_disk_dev_list; dev; dev = dev->next)
    {
      if ((dev->disk_open) (raw, disk) == GRUB_ERR_NONE)
	break;
      else if (grub_errno == GRUB_ERR_UNKNOWN_DEVICE)
	grub_errno = GRUB_ERR_NONE;
      else
	goto fail;
    }

  if (! dev)
    {
      grub_error (GRUB_ERR_UNKNOWN_DEVICE, N_("disk `%s' not found"),
		  name);
      goto fail;
    }
  if (disk->log_sector_size > GRUB_DISK_CACHE_BITS + GRUB_DISK_SECTOR_BITS
      || disk->log_sector_size < GRUB_DISK_SECTOR_BITS)
    {
      grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET,
		  "sector sizes of %d bytes aren't supported yet",
		  (1 << disk->log_sector_size));
      goto fail;
    }

  disk->dev = dev;

  if (p)
    {
      disk->partition = grub_partition_probe (disk, p + 1);
      if (! disk->partition)
	{
	  /* TRANSLATORS: It means that the specified partition e.g.
	     hd0,msdos1=/dev/sda1 doesn't exist.  */
	  grub_error (GRUB_ERR_UNKNOWN_DEVICE, N_("no such partition"));
	  goto fail;
	}
    }

  /* The cache will be invalidated about 2 seconds after a device was
     closed.  */
  current_time = grub_get_time_ms ();

  if (current_time > (grub_last_time
		      + GRUB_CACHE_TIMEOUT * 1000))
    grub_disk_cache_invalidate_all ();

  grub_last_time = current_time;

 fail:

  if (raw && raw != name)
    grub_free (raw);

  if (grub_errno != GRUB_ERR_NONE)
    {
      grub_error_push ();
      grub_dprintf ("disk", "Opening `%s' failed.\n", name);
      grub_error_pop ();

      grub_disk_close (disk);
      return 0;
    }

  return disk;
}

void
grub_disk_close (grub_disk_t disk)
{
  grub_partition_t part;
  grub_dprintf ("disk", "Closing `%s'.\n", disk->name);

  if (disk->dev && disk->dev->disk_close)
    (disk->dev->disk_close) (disk);

  /* Reset the timer.  */
  grub_last_time = grub_get_time_ms ();

  while (disk->partition)
    {
      part = disk->partition->parent;
      grub_free (disk->partition);
      disk->partition = part;
    }
  grub_free ((void *) disk->name);
  grub_free (disk);
}

/* Small read (less than cache size and not pass across cache unit boundaries).
   sector is already adjusted and is divisible by cache unit size.
 */
static grub_err_t
grub_disk_read_small_real (grub_disk_t disk, grub_disk_addr_t sector,
			   grub_off_t offset, grub_size_t size, void *buf)
{
  char *data;
  char *tmp_buf;

  /* Fetch the cache.  */
  data = grub_disk_cache_fetch (disk->dev->id, disk->id, sector);
  if (data)
    {
      /* Just copy it!  */
      grub_memcpy (buf, data + offset, size);
      grub_disk_cache_unlock (disk->dev->id, disk->id, sector);
      return GRUB_ERR_NONE;
    }

  /* Allocate a temporary buffer.  */
  tmp_buf = grub_malloc (GRUB_DISK_SECTOR_SIZE << GRUB_DISK_CACHE_BITS);
  if (! tmp_buf)
    return grub_errno;

  /* Otherwise read data from the disk actually.  */
  if (disk->total_sectors == GRUB_DISK_SIZE_UNKNOWN
      || sector + GRUB_DISK_CACHE_SIZE
      < (disk->total_sectors << (disk->log_sector_size - GRUB_DISK_SECTOR_BITS)))
    {
      grub_err_t err;
      err = (disk->dev->disk_read) (disk, transform_sector (disk, sector),
				    1U << (GRUB_DISK_CACHE_BITS
					   + GRUB_DISK_SECTOR_BITS
					   - disk->log_sector_size), tmp_buf);
      if (!err)
	{
	  /* Copy it and store it in the disk cache.  */
	  grub_memcpy (buf, tmp_buf + offset, size);
	  grub_disk_cache_store (disk->dev->id, disk->id,
				 sector, tmp_buf);
	  grub_free (tmp_buf);
	  return GRUB_ERR_NONE;
	}
    }

  grub_free (tmp_buf);
  grub_errno = GRUB_ERR_NONE;

  {
    /* Uggh... Failed. Instead, just read necessary data.  */
    unsigned num;
    grub_disk_addr_t aligned_sector;

    sector += (offset >> GRUB_DISK_SECTOR_BITS);
    offset &= ((1 << GRUB_DISK_SECTOR_BITS) - 1);
    aligned_sector = (sector & ~((1ULL << (disk->log_sector_size
					   - GRUB_DISK_SECTOR_BITS))
				 - 1));
    offset += ((sector - aligned_sector) << GRUB_DISK_SECTOR_BITS);
    num = ((size + offset + (1ULL << (disk->log_sector_size))
	    - 1) >> (disk->log_sector_size));

    tmp_buf = grub_malloc (num << disk->log_sector_size);
    if (!tmp_buf)
      return grub_errno;
    
    if ((disk->dev->disk_read) (disk, transform_sector (disk, aligned_sector),
				num, tmp_buf))
      {
	grub_error_push ();
	grub_dprintf ("disk", "%s read failed\n", disk->name);
	grub_error_pop ();
	grub_free (tmp_buf);
	return grub_errno;
      }
    grub_memcpy (buf, tmp_buf + offset, size);
    grub_free (tmp_buf);
    return GRUB_ERR_NONE;
  }
}

static grub_err_t
grub_disk_read_small (grub_disk_t disk, grub_disk_addr_t sector,
		      grub_off_t offset, grub_size_t size, void *buf)
{
  grub_err_t err;

  err = grub_disk_read_small_real (disk, sector, offset, size, buf);
  if (err)
    return err;
  if (disk->read_hook)
    (disk->read_hook) (sector + (offset >> GRUB_DISK_SECTOR_BITS),
		       offset & (GRUB_DISK_SECTOR_SIZE - 1),
		       size, disk->read_hook_data);
  return GRUB_ERR_NONE;
}

grub_err_t grub_disk_blocklist_read(void *chunklist, grub_uint64_t sector, 
    grub_uint64_t size, grub_uint32_t log_sector_size)
{
    grub_uint64_t sizeshift;
    ventoy_img_chunk *last_chunk = NULL;
    ventoy_img_chunk *new_chunk = NULL;
    ventoy_img_chunk_list *chunk_list = (ventoy_img_chunk_list *)chunklist;

    sizeshift = (size >> log_sector_size);
    if (sizeshift == 0)
    {
        sizeshift = 1;
    }

    if (chunk_list->cur_chunk == 0)
    {
        chunk_list->chunk[0].img_start_sector = 0;
        chunk_list->chunk[0].img_end_sector = (size >> 11) - 1;
        chunk_list->chunk[0].disk_start_sector = sector;
        chunk_list->chunk[0].disk_end_sector = sector + sizeshift - 1;
        chunk_list->cur_chunk = 1;
        return 0;
    }

    last_chunk = chunk_list->chunk + chunk_list->cur_chunk - 1;
    if (last_chunk->disk_end_sector + 1 == sector)
    {
        last_chunk->img_end_sector  += (size >> 11);
        last_chunk->disk_end_sector += sizeshift;
        return 0;
    }

    if (chunk_list->cur_chunk == chunk_list->max_chunk)
    {
        new_chunk = grub_realloc(chunk_list->chunk, chunk_list->max_chunk * 2 * sizeof(ventoy_img_chunk));
        if (NULL == new_chunk)
        {
            return -1;
        }
        chunk_list->chunk = new_chunk;
        chunk_list->max_chunk *= 2;

        /* issue: update last_chunk */
        last_chunk = chunk_list->chunk + chunk_list->cur_chunk - 1;
    }

    new_chunk = chunk_list->chunk + chunk_list->cur_chunk;
    new_chunk->img_start_sector = last_chunk->img_end_sector + 1;
    new_chunk->img_end_sector = new_chunk->img_start_sector + (size >> 11) - 1;
    new_chunk->disk_start_sector = sector;
    new_chunk->disk_end_sector = sector + sizeshift - 1;

    chunk_list->cur_chunk++;

    return 0;
}

/* Read data from the disk.  */
grub_err_t
grub_disk_read (grub_disk_t disk, grub_disk_addr_t sector,
		grub_off_t offset, grub_size_t size, void *buf)
{
    if (disk->read_hook == (grub_disk_read_hook_t)(void *)grub_disk_blocklist_read)
    {
        return grub_disk_blocklist_read((ventoy_img_chunk_list *)disk->read_hook_data, sector, size, disk->log_sector_size);
    }

  /* First of all, check if the region is within the disk.  */
  if (grub_disk_adjust_range (disk, &sector, &offset, size) != GRUB_ERR_NONE)
    {
      grub_error_push ();
      grub_dprintf ("disk", "Read out of range: sector 0x%llx (%s).\n",
		    (unsigned long long) sector, grub_errmsg);
      grub_error_pop ();
      return grub_errno;
    }

  /* First read until first cache boundary.   */
  if (offset || (sector & (GRUB_DISK_CACHE_SIZE - 1)))
    {
      grub_disk_addr_t start_sector;
      grub_size_t pos;
      grub_err_t err;
      grub_size_t len;

      start_sector = sector & ~((grub_disk_addr_t) GRUB_DISK_CACHE_SIZE - 1);
      pos = (sector - start_sector) << GRUB_DISK_SECTOR_BITS;
      len = ((GRUB_DISK_SECTOR_SIZE << GRUB_DISK_CACHE_BITS)
	     - pos - offset);
      if (len > size)
	len = size;
      err = grub_disk_read_small (disk, start_sector,
				  offset + pos, len, buf);
      if (err)
	return err;
      buf = (char *) buf + len;
      size -= len;
      offset += len;
      sector += (offset >> GRUB_DISK_SECTOR_BITS);
      offset &= ((1 << GRUB_DISK_SECTOR_BITS) - 1);
    }

  /* Until SIZE is zero...  */
  while (size >= (GRUB_DISK_CACHE_SIZE << GRUB_DISK_SECTOR_BITS))
    {
      char *data = NULL;
      grub_disk_addr_t agglomerate;
      grub_err_t err;

      /* agglomerate read until we find a first cached entry.  */
      for (agglomerate = 0; agglomerate
	     < (size >> (GRUB_DISK_SECTOR_BITS + GRUB_DISK_CACHE_BITS))
	     && agglomerate < disk->max_agglomerate;
	   agglomerate++)
	{
	  data = grub_disk_cache_fetch (disk->dev->id, disk->id,
					sector + (agglomerate
						  << GRUB_DISK_CACHE_BITS));
	  if (data)
	    break;
	}

      if (data)
	{
	  grub_memcpy ((char *) buf
		       + (agglomerate << (GRUB_DISK_CACHE_BITS
					  + GRUB_DISK_SECTOR_BITS)),
		       data, GRUB_DISK_CACHE_SIZE << GRUB_DISK_SECTOR_BITS);
	  grub_disk_cache_unlock (disk->dev->id, disk->id,
				  sector + (agglomerate
					    << GRUB_DISK_CACHE_BITS));
	}

      if (agglomerate)
	{
	  grub_disk_addr_t i;

	  err = (disk->dev->disk_read) (disk, transform_sector (disk, sector),
					agglomerate << (GRUB_DISK_CACHE_BITS
							+ GRUB_DISK_SECTOR_BITS
							- disk->log_sector_size),
					buf);
	  if (err)
	    return err;
	  
	  for (i = 0; i < agglomerate; i ++)
	    grub_disk_cache_store (disk->dev->id, disk->id,
				   sector + (i << GRUB_DISK_CACHE_BITS),
				   (char *) buf
				   + (i << (GRUB_DISK_CACHE_BITS
					    + GRUB_DISK_SECTOR_BITS)));


	  if (disk->read_hook)
	    (disk->read_hook) (sector, 0, agglomerate << (GRUB_DISK_CACHE_BITS + GRUB_DISK_SECTOR_BITS),
			       disk->read_hook_data);

	  sector += agglomerate << GRUB_DISK_CACHE_BITS;
	  size -= agglomerate << (GRUB_DISK_CACHE_BITS + GRUB_DISK_SECTOR_BITS);
	  buf = (char *) buf 
	    + (agglomerate << (GRUB_DISK_CACHE_BITS + GRUB_DISK_SECTOR_BITS));
	}

      if (data)
	{
	  if (disk->read_hook)
	    (disk->read_hook) (sector, 0, (GRUB_DISK_CACHE_SIZE << GRUB_DISK_SECTOR_BITS),
			       disk->read_hook_data);
	  sector += GRUB_DISK_CACHE_SIZE;
	  buf = (char *) buf + (GRUB_DISK_CACHE_SIZE << GRUB_DISK_SECTOR_BITS);
	  size -= (GRUB_DISK_CACHE_SIZE << GRUB_DISK_SECTOR_BITS);
	}
    }

  /* And now read the last part.  */
  if (size)
    {
      grub_err_t err;
      err = grub_disk_read_small (disk, sector, 0, size, buf);
      if (err)
	return err;
    }

  return grub_errno;
}

grub_uint64_t
grub_disk_get_size (grub_disk_t disk)
{
  if (disk->partition)
    return grub_partition_get_len (disk->partition);
  else if (disk->total_sectors != GRUB_DISK_SIZE_UNKNOWN)
    return disk->total_sectors << (disk->log_sector_size - GRUB_DISK_SECTOR_BITS);
  else
    return GRUB_DISK_SIZE_UNKNOWN;
}
