/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009, 2010  Free Software Foundation, Inc.
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

#include <grub/relocator.h>
#include <grub/relocator_private.h>
#include <grub/mm_private.h>
#include <grub/misc.h>
#include <grub/cache.h>
#include <grub/memory.h>
#include <grub/dl.h>
#include <grub/i18n.h>

GRUB_MOD_LICENSE ("GPLv3+");

struct grub_relocator
{
  struct grub_relocator_chunk *chunks;
  grub_phys_addr_t postchunks;
  grub_phys_addr_t highestaddr;
  grub_phys_addr_t highestnonpostaddr;
  grub_size_t relocators_size;
};

struct grub_relocator_subchunk
{
  enum {CHUNK_TYPE_IN_REGION, CHUNK_TYPE_REGION_START,
#if GRUB_RELOCATOR_HAVE_FIRMWARE_REQUESTS
	CHUNK_TYPE_FIRMWARE, CHUNK_TYPE_LEFTOVER
#endif
  } type;
  grub_mm_region_t reg;
  grub_phys_addr_t start;
  grub_size_t size;
  grub_size_t pre_size;
  struct grub_relocator_extra_block *extra;
#if GRUB_RELOCATOR_HAVE_LEFTOVERS
  struct grub_relocator_fw_leftover *pre, *post;
#endif
};

struct grub_relocator_chunk
{
  struct grub_relocator_chunk *next;
  grub_phys_addr_t src;
  void *srcv;
  grub_phys_addr_t target;
  grub_size_t size;
  struct grub_relocator_subchunk *subchunks;
  unsigned nsubchunks;
};

struct grub_relocator_extra_block
{
  struct grub_relocator_extra_block *next;
  struct grub_relocator_extra_block **prev;
  grub_phys_addr_t start;
  grub_phys_addr_t end;
};

#if GRUB_RELOCATOR_HAVE_LEFTOVERS
struct grub_relocator_fw_leftover
{
  struct grub_relocator_fw_leftover *next;
  struct grub_relocator_fw_leftover **prev;
  grub_phys_addr_t quantstart;
  grub_uint8_t freebytes[GRUB_RELOCATOR_FIRMWARE_REQUESTS_QUANT / 8];
};

static struct grub_relocator_fw_leftover *leftovers;
#endif

static struct grub_relocator_extra_block *extra_blocks;

void *
get_virtual_current_address (grub_relocator_chunk_t in)
{
  return in->srcv;
}

grub_phys_addr_t
get_physical_target_address (grub_relocator_chunk_t in)
{
  return in->target;
}

struct grub_relocator *
grub_relocator_new (void)
{
  struct grub_relocator *ret;

  grub_cpu_relocator_init ();

  ret = grub_zalloc (sizeof (struct grub_relocator));
  if (!ret)
    return NULL;
    
  ret->postchunks = ~(grub_phys_addr_t) 0;
  ret->relocators_size = grub_relocator_jumper_size;
  grub_dprintf ("relocator", "relocators_size=%lu\n",
		(unsigned long) ret->relocators_size);
  return ret;
}

#define DIGITSORT_BITS 8
#define DIGITSORT_MASK ((1 << DIGITSORT_BITS) - 1)
#define BITS_IN_BYTE 8

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))

static inline int
is_start (int type)
{
  return !(type & 1) && (type != COLLISION_START);
}

static void
allocate_regstart (grub_phys_addr_t addr, grub_size_t size, grub_mm_region_t rb,
		   grub_mm_region_t *regancestor, grub_mm_header_t hancestor)
{
  grub_addr_t newreg_start, newreg_raw_start
    = (grub_addr_t) rb + (addr - grub_vtop (rb)) + size;
  grub_addr_t newreg_size, newreg_presize;
  grub_mm_header_t new_header;
  grub_mm_header_t hb = (grub_mm_header_t) (rb + 1);

#ifdef DEBUG_RELOCATOR_NOMEM_DPRINTF  
  grub_dprintf ("relocator", "ra = %p, rb = %p\n", regancestor, rb);
#endif
  newreg_start = ALIGN_UP (newreg_raw_start, GRUB_MM_ALIGN);
  newreg_presize = newreg_start - newreg_raw_start;
  newreg_size = rb->size - (newreg_start - (grub_addr_t) rb);
  if ((hb->size << GRUB_MM_ALIGN_LOG2) >= newreg_start
      - (grub_addr_t) rb)
    {
      grub_mm_header_t newhnext = hb->next;
      grub_size_t newhsize = ((hb->size << GRUB_MM_ALIGN_LOG2)
			      - (newreg_start
				 - (grub_addr_t) rb)) >> GRUB_MM_ALIGN_LOG2;
      new_header = (void *) (newreg_start + sizeof (*rb));
      if (newhnext == hb)
	newhnext = new_header;
      new_header->next = newhnext;
      new_header->size = newhsize;
      new_header->magic = GRUB_MM_FREE_MAGIC;
    }
  else
    {
      new_header = hb->next;
      if (new_header == hb)
	new_header = (void *) (newreg_start + sizeof (*rb));	    
    }
  {
    struct grub_mm_header *newregfirst = rb->first;
    struct grub_mm_region *newregnext = rb->next;
    struct grub_mm_region *newreg = (void *) newreg_start;
    hancestor->next = new_header;
    if (newregfirst == hb)
      newregfirst = new_header;
    newreg->first = newregfirst;
    newreg->next = newregnext;
    newreg->pre_size = newreg_presize;
    newreg->size = newreg_size;
    *regancestor = newreg;
    {
      grub_mm_header_t h = newreg->first, hp = NULL;
      do
	{
	  if ((void *) h < (void *) (newreg + 1))
	    grub_fatal ("Failed to adjust memory region: %p, %p, %p, %p, %p",
			newreg, newreg->first, h, hp, hb);
#ifdef DEBUG_RELOCATOR_NOMEM_DPRINTF
	  if ((void *) h == (void *) (newreg + 1))
	    grub_dprintf ("relocator",
			  "Free start memory region: %p, %p, %p, %p, %p",
			  newreg, newreg->first, h, hp, hb);
#endif
	  hp = h;
	  h = h->next;
	}
      while (h != newreg->first);
    }
  }
}

static void
allocate_inreg (grub_phys_addr_t paddr, grub_size_t size,
		grub_mm_header_t hb, grub_mm_header_t hbp,
		grub_mm_region_t rb)
{
  struct grub_mm_header *foll = NULL;
  grub_addr_t vaddr = (grub_addr_t) hb + (paddr - grub_vtop (hb));

#ifdef DEBUG_RELOCATOR_NOMEM_DPRINTF
  grub_dprintf ("relocator", "inreg paddr = 0x%lx, size = %lu,"
		" hb = %p, hbp = %p, rb = %p, vaddr = 0x%lx\n",
		(unsigned long) paddr, (unsigned long) size, hb, hbp,
		rb, (unsigned long) vaddr);
#endif
    
  if (ALIGN_UP (vaddr + size, GRUB_MM_ALIGN) + GRUB_MM_ALIGN
      <= (grub_addr_t) (hb + hb->size))
    {
      foll = (void *) ALIGN_UP (vaddr + size, GRUB_MM_ALIGN);
      foll->magic = GRUB_MM_FREE_MAGIC;
      foll->size = hb + hb->size - foll;
#ifdef DEBUG_RELOCATOR_NOMEM_DPRINTF
      grub_dprintf ("relocator", "foll = %p, foll->size = %lu\n", foll,
		    (unsigned long) foll->size);
#endif
    }

  if (vaddr - (grub_addr_t) hb >= sizeof (*hb))
    {
      hb->size = ((vaddr - (grub_addr_t) hb) >> GRUB_MM_ALIGN_LOG2);
      if (foll)
	{
	  foll->next = hb;
	  hbp->next = foll;
	  if (rb->first == hb)
	    {
	      rb->first = foll;
	    }
	}
    }
  else
    {
      if (foll)
	{
	  foll->next = hb->next;
	}
      else
	foll = hb->next;
	
      hbp->next = foll;
      if (rb->first == hb)
	{
	  rb->first = foll;
	}
      if (rb->first == hb)
	{
	  rb->first = (void *) (rb + 1);
	}
    }
}

#if GRUB_RELOCATOR_HAVE_LEFTOVERS
static void
check_leftover (struct grub_relocator_fw_leftover *lo)
{
  unsigned i;
  for (i = 0; i < sizeof (lo->freebytes); i++)
    if (lo->freebytes[i] != 0xff)
      return;
  grub_relocator_firmware_free_region (lo->quantstart,
				       GRUB_RELOCATOR_FIRMWARE_REQUESTS_QUANT);
  *lo->prev = lo->next;
  if (lo->next)
    lo->next->prev = lo->prev;
}
#endif

static void
free_subchunk (const struct grub_relocator_subchunk *subchu)
{
  switch (subchu->type)
    {
    case CHUNK_TYPE_REGION_START:
      {
	grub_mm_region_t r1, r2, *rp;
	grub_mm_header_t h;
	grub_size_t pre_size;
	r1 = subchu->reg;
	r2 = (grub_mm_region_t) ALIGN_UP ((grub_addr_t) subchu->reg
					  + (grub_vtop (subchu->reg)
					     - subchu->start) + subchu->size,
					  GRUB_MM_ALIGN);
	for (rp = &grub_mm_base; *rp && *rp != r2; rp = &((*rp)->next));
	pre_size = subchu->pre_size;

	if (*rp)
	  {
	    grub_mm_header_t h2, *hp;
	    r1->first = r2->first;
	    r1->next = r2->next;
	    r1->pre_size = pre_size;
	    r1->size = r2->size + (r2 - r1) * sizeof (*r2);
	    *rp = r1;
	    h = (grub_mm_header_t) (r1 + 1);
	    h->next = r2->first;
	    h->magic = GRUB_MM_FREE_MAGIC;
	    h->size = (r2 - r1 - 1);
	    for (hp = &r2->first, h2 = *hp; h2->next != r2->first;
		 hp = &(h2->next), h2 = *hp)
	      if (h2 == (grub_mm_header_t) (r2 + 1))
		break;
	    if (h2 == (grub_mm_header_t) (r2 + 1))
	      {
		h->size = h2->size + (h2 - h);
		h->next = h2->next;
		*hp = h;
		if (hp == &r2->first)
		  {
		    for (h2 = r2->first; h2->next != r2->first; h2 = h2->next);
		    h2->next = h;
		  }
	      }
	    else
	      {
		h2->next = h;
	      }
	  }
	else
	  {
	    r1->pre_size = pre_size;
	    r1->size = (r2 - r1) * sizeof (*r2);
	    /* Find where to insert this region.
	       Put a smaller one before bigger ones,
	       to prevent fragmentation.  */
	    for (rp = &grub_mm_base; *rp; rp = &((*rp)->next))
	      if ((*rp)->size > r1->size)
		break;
	    r1->next = *rp;
	    *rp = r1->next;
	    h = (grub_mm_header_t) (r1 + 1);
	    r1->first = h;
	    h->next = h;
	    h->magic = GRUB_MM_FREE_MAGIC;
	    h->size = (r2 - r1 - 1);
	  }
	for (r2 = grub_mm_base; r2; r2 = r2->next)
	  if ((grub_addr_t) r2 + r2->size == (grub_addr_t) r1)
	    break;
	if (r2)
	  {
	    grub_mm_header_t hl2, hl, g;
	    g = (grub_mm_header_t) ((grub_addr_t) r2 + r2->size);
	    g->size = (grub_mm_header_t) r1 - g;
	    r2->size += r1->size;
	    for (hl = r2->first; hl->next != r2->first; hl = hl->next);
	    for (hl2 = r1->first; hl2->next != r1->first; hl2 = hl2->next);
	    hl2->next = r2->first;
	    r2->first = r1->first;
	    hl->next = r2->first;
	    *rp = (*rp)->next;
	    grub_free (g + 1);
	  }
	break;
      }
    case CHUNK_TYPE_IN_REGION:
      {
	grub_mm_header_t h = (grub_mm_header_t) ALIGN_DOWN ((grub_addr_t) subchu->start,
							    GRUB_MM_ALIGN);
	h->size
	  = ((subchu->start + subchu->size + GRUB_MM_ALIGN - 1) / GRUB_MM_ALIGN)
	  - (subchu->start / GRUB_MM_ALIGN) - 1;
	h->next = h;
	h->magic = GRUB_MM_ALLOC_MAGIC;
	grub_free (h + 1);
	break;
      }
#if GRUB_RELOCATOR_HAVE_FIRMWARE_REQUESTS
    case CHUNK_TYPE_FIRMWARE:
    case CHUNK_TYPE_LEFTOVER:
      {
	grub_addr_t fstart, fend;
	fstart = ALIGN_UP (subchu->start,
			   GRUB_RELOCATOR_FIRMWARE_REQUESTS_QUANT);
	fend = ALIGN_DOWN (subchu->start + subchu->size,
			   GRUB_RELOCATOR_FIRMWARE_REQUESTS_QUANT);
	if (fstart < fend)
	  grub_relocator_firmware_free_region (fstart, fend - fstart);
#if GRUB_RELOCATOR_HAVE_LEFTOVERS
	if (subchu->pre)
	  {
	    int off = subchu->start - fstart
	      - GRUB_RELOCATOR_FIRMWARE_REQUESTS_QUANT;
	    grub_memset (subchu->pre->freebytes + off / 8 + 1,
			 0xff, sizeof (subchu->pre->freebytes) - off / 8 - 1);
	    subchu->pre->freebytes[off / 8] |= ~((1 << (off % 8)) - 1);
	    check_leftover (subchu->pre);
	  }
	if (subchu->post)
	  {
	    int off = subchu->start + subchu->size - fend;
	    grub_memset (subchu->pre->freebytes,
			 0xff, sizeof (subchu->pre->freebytes) - off / 8);
	    subchu->pre->freebytes[off / 8] |= ((1 << (8 - (off % 8))) - 1);
	    check_leftover (subchu->post);
	  }
#endif
	*subchu->extra->prev = subchu->extra->next;
	grub_free (subchu->extra);
      }
      break;
#endif
    }  
}

static int
malloc_in_range (struct grub_relocator *rel,
		 grub_addr_t start, grub_addr_t end, grub_addr_t align,
		 grub_size_t size, struct grub_relocator_chunk *res,
		 int from_low_priv, int collisioncheck)
{
  grub_mm_region_t r, *ra, base_saved;
  struct grub_relocator_mmap_event *events = NULL, *eventt = NULL, *t;
  /* 128 is just in case of additional malloc (shouldn't happen).  */
  unsigned maxevents = 2 + 128;
  grub_mm_header_t p, pa;
  unsigned *counter;
  int nallocs = 0;
  unsigned j, N = 0;
  grub_addr_t target = 0;

  grub_dprintf ("relocator",
		"trying to allocate in 0x%lx-0x%lx aligned 0x%lx size 0x%lx\n",
		(unsigned long) start, (unsigned long) end,
		(unsigned long) align, (unsigned long) size);

  start = ALIGN_UP (start, align);
  end = ALIGN_DOWN (end - size, align) + size;

  if (end < start + size)
    return 0;

  /* We have to avoid any allocations when filling scanline events. 
     Hence 2-stages.
   */
  for (r = grub_mm_base; r; r = r->next)
    {
      p = r->first;
      do
	{
	  if ((grub_addr_t) p < (grub_addr_t) (r + 1)
	      || (grub_addr_t) p >= (grub_addr_t) (r + 1) + r->size)
	    grub_fatal ("%d: out of range pointer: %p\n", __LINE__, p);
	  maxevents += 2;
	  p = p->next;
	}
      while (p != r->first);
      maxevents += 4;
    }

  if (collisioncheck && rel)
    {
      struct grub_relocator_chunk *chunk;
      for (chunk = rel->chunks; chunk; chunk = chunk->next)
	maxevents += 2;
    }

#if GRUB_RELOCATOR_HAVE_FIRMWARE_REQUESTS
  {
    struct grub_relocator_extra_block *cur;
    for (cur = extra_blocks; cur; cur = cur->next)
      maxevents += 2;
  }
  for (r = grub_mm_base; r; r = r->next)
    maxevents += 2;

  maxevents += grub_relocator_firmware_get_max_events ();
#endif

#if GRUB_RELOCATOR_HAVE_LEFTOVERS
  {
    struct grub_relocator_fw_leftover *cur;
    for (cur = leftovers; cur; cur = cur->next)
      {
	int l = 0;
	unsigned i;
	for (i = 0; i < GRUB_RELOCATOR_FIRMWARE_REQUESTS_QUANT; i++)
	  {
	    if (l != ((cur->freebytes[i / 8] >> (i % 8)) & 1))
	      maxevents++;
	    l = ((cur->freebytes[i / 8] >> (i % 8)) & 1);
	  }
	if (l)
	  maxevents++;
      }
  }
#endif

  eventt = grub_malloc (maxevents * sizeof (events[0]));
  counter = grub_malloc ((DIGITSORT_MASK + 2) * sizeof (counter[0]));
  events = grub_malloc (maxevents * sizeof (events[0]));
  if (!events || !eventt || !counter)
    {
      grub_dprintf ("relocator", "events or counter allocation failed %d\n",
		    maxevents);
      grub_free (events);
      grub_free (eventt);
      grub_free (counter);
      return 0;
    }

  if (collisioncheck && rel)
    {
      struct grub_relocator_chunk *chunk;
      for (chunk = rel->chunks; chunk; chunk = chunk->next)
	{
	  events[N].type = COLLISION_START;
	  events[N].pos = chunk->target;
	  N++;
	  events[N].type = COLLISION_END;
	  events[N].pos = chunk->target + chunk->size;
	  N++;
	}
    }

#if GRUB_RELOCATOR_HAVE_FIRMWARE_REQUESTS
  for (r = grub_mm_base; r; r = r->next)
    {
#ifdef DEBUG_RELOCATOR_NOMEM_DPRINTF
      grub_dprintf ("relocator", "Blocking at 0x%lx-0x%lx\n",
		    (unsigned long) r - r->pre_size, 
		    (unsigned long) (r + 1) + r->size);
#endif
      events[N].type = FIRMWARE_BLOCK_START;
      events[N].pos = (grub_addr_t) r - r->pre_size;
      N++;
      events[N].type = FIRMWARE_BLOCK_END;
      events[N].pos = (grub_addr_t) (r + 1) + r->size;
      N++;
    }
  {
    struct grub_relocator_extra_block *cur;
    for (cur = extra_blocks; cur; cur = cur->next)
      {
#ifdef DEBUG_RELOCATOR_NOMEM_DPRINTF
	grub_dprintf ("relocator", "Blocking at 0x%lx-0x%lx\n",
		      (unsigned long) cur->start, (unsigned long) cur->end);
#endif
	events[N].type = FIRMWARE_BLOCK_START;
	events[N].pos = cur->start;
	N++;
	events[N].type = FIRMWARE_BLOCK_END;
	events[N].pos = cur->end;
	N++;
      }
  }

  N += grub_relocator_firmware_fill_events (events + N);

#if GRUB_RELOCATOR_HAVE_LEFTOVERS
  {
    struct grub_relocator_fw_leftover *cur;
    for (cur = leftovers; cur; cur = cur->next)
      {
	unsigned i;
	int l = 0;
	for (i = 0; i < GRUB_RELOCATOR_FIRMWARE_REQUESTS_QUANT; i++)
	  {
	    if (l != ((cur->freebytes[i / 8] >> (i % 8)) & 1))
	      {
		events[N].type = l ? REG_LEFTOVER_END : REG_LEFTOVER_START;
		events[N].pos = cur->quantstart + i;
		events[N].leftover = cur;
		N++;
	      }
	    l = ((cur->freebytes[i / 8] >> (i % 8)) & 1);
	  }
	if (l)
	  {
	    events[N].type = REG_LEFTOVER_END;
	    events[N].pos = cur->quantstart + i;
	    events[N].leftover = cur;
	    N++;
	  }
      }
  }
#endif
#endif

  /* No malloc from this point.  */
  base_saved = grub_mm_base;
  grub_mm_base = NULL;

  for (ra = &base_saved, r = *ra; r; ra = &(r->next), r = *ra)
    {
      pa = r->first;
      p = pa->next;
      if (p->magic == GRUB_MM_ALLOC_MAGIC)
	continue;
      do 
	{
	  if (p->magic != GRUB_MM_FREE_MAGIC)
	    grub_fatal ("%s:%d free magic broken at %p (0x%x)\n",
			__FILE__,
			__LINE__, p, p->magic);
	  if (p == (grub_mm_header_t) (r + 1))
	    {
	      events[N].type = REG_BEG_START;
	      events[N].pos = grub_vtop (r) - r->pre_size;
	      events[N].reg = r;
	      events[N].regancestor = ra;
	      events[N].head = p;
	      events[N].hancestor = pa;
	      N++;
	      events[N].type = REG_BEG_END;
	      events[N].pos = grub_vtop (p + p->size) - sizeof (*r)
		- sizeof (struct grub_mm_header);
	      N++;
	    }
	  else
	    {
	      events[N].type = IN_REG_START;
	      events[N].pos = grub_vtop (p);
	      events[N].head = p;
	      events[N].hancestor = pa;
	      events[N].reg = r;
	      N++;
	      events[N].type = IN_REG_END;
	      events[N].pos = grub_vtop (p + p->size);
	      N++;
	    }
	  pa = p;
	  p = pa->next;
	}
      while (pa != r->first);
    }

  /* Put ending events after starting events.  */
  {
    int st = 0, e = N / 2;
    for (j = 0; j < N; j++)
      if (is_start (events[j].type) || events[j].type == COLLISION_START)
	eventt[st++] = events[j];
      else
	eventt[e++] = events[j];
    t = eventt;
    eventt = events;
    events = t;
  }

  {
    unsigned i;
    for (i = 0; i < (BITS_IN_BYTE * sizeof (grub_addr_t) / DIGITSORT_BITS);
	 i++)
      {
	grub_memset (counter, 0, (1 + (1 << DIGITSORT_BITS)) * sizeof (counter[0]));
	for (j = 0; j < N; j++)
	  counter[((events[j].pos >> (DIGITSORT_BITS * i)) 
		   & DIGITSORT_MASK) + 1]++;
	for (j = 0; j <= DIGITSORT_MASK; j++)
	  counter[j+1] += counter[j];
	for (j = 0; j < N; j++)
	  eventt[counter[((events[j].pos >> (DIGITSORT_BITS * i)) 
			  & DIGITSORT_MASK)]++] = events[j];
	t = eventt;
	eventt = events;
	events = t;
      }
  }

#if GRUB_RELOCATOR_HAVE_FIRMWARE_REQUESTS
 retry:
#endif

  /* Now events are nicely sorted.  */
  {
    int nstarted = 0, ncollisions = 0, nstartedfw = 0, nblockfw = 0;
#if GRUB_RELOCATOR_HAVE_LEFTOVERS
    int nlefto = 0;
#else
    const int nlefto = 0;
#endif
    grub_addr_t starta = 0;
    for (j = from_low_priv ? 0 : N - 1; from_low_priv ? j < N : (j + 1); 
	 from_low_priv ? j++ : j--)
      {
	int isinsidebefore, isinsideafter;
	isinsidebefore = (!ncollisions && (nstarted || (((nlefto || nstartedfw)
							 && !nblockfw))));
	switch (events[j].type)
	  {
#if GRUB_RELOCATOR_HAVE_FIRMWARE_REQUESTS
	  case REG_FIRMWARE_START:
	    nstartedfw++;
	    break;

	  case REG_FIRMWARE_END:
	    nstartedfw--;
	    break;

	  case FIRMWARE_BLOCK_START:
	    nblockfw++;
	    break;

	  case FIRMWARE_BLOCK_END:
	    nblockfw--;
	    break;
#endif

#if GRUB_RELOCATOR_HAVE_LEFTOVERS
	  case REG_LEFTOVER_START:
	    nlefto++;
	    break;

	  case REG_LEFTOVER_END:
	    nlefto--;
	    break;
#endif

	  case COLLISION_START:
	    ncollisions++;
	    break;

	  case COLLISION_END:
	    ncollisions--;
	    break;

	  case IN_REG_START:
	  case REG_BEG_START:
	    nstarted++;
	    break;

	  case IN_REG_END:
	  case REG_BEG_END:
	    nstarted--;
	    break;
	  }
	isinsideafter = (!ncollisions && (nstarted || ((nlefto || nstartedfw) 
						       && !nblockfw)));
	if (from_low_priv) {
	  if (!isinsidebefore && isinsideafter)
	    starta = ALIGN_UP (events[j].pos, align);

	  if (isinsidebefore && !isinsideafter)
	    {
	      target = starta;
	      if (target < start)
		target = start;
	      if (target + size <= end && target + size <= events[j].pos)
		/* Found an usable address.  */
		goto found;
	    }
	} else {
	  if (!isinsidebefore && isinsideafter)
	    {
	      if (events[j].pos >= size)
		starta = ALIGN_DOWN (events[j].pos - size, align) + size;
	      else
		starta = 0;
	    }
	  if (isinsidebefore && !isinsideafter && starta >= size)
	    {
	      target = starta - size;
	      if (target > end - size)
		target = end - size;
	      if (target >= start && target >= events[j].pos)
		goto found;
	    }
	}
      }
  }

  grub_mm_base = base_saved;
  grub_free (events);
  grub_free (eventt);
  grub_free (counter);
  return 0;

 found:
  {
    int inreg = 0, regbeg = 0, ncol = 0;
#if GRUB_RELOCATOR_HAVE_FIRMWARE_REQUESTS
    int fwin = 0, fwb = 0, fwlefto = 0;
#endif
#if GRUB_RELOCATOR_HAVE_LEFTOVERS
    int last_lo = 0;
#endif
    int last_start = 0;
    for (j = 0; j < N; j++)
      {
	int typepre;
	if (ncol)
	  typepre = -1;
	else if (regbeg)
	  typepre = CHUNK_TYPE_REGION_START;
	else if (inreg)
	  typepre = CHUNK_TYPE_IN_REGION;
#if GRUB_RELOCATOR_HAVE_FIRMWARE_REQUESTS
	else if (fwin && !fwb)
	  typepre = CHUNK_TYPE_FIRMWARE;
	else if (fwlefto && !fwb)
	  typepre = CHUNK_TYPE_LEFTOVER;
#endif
	else
	  typepre = -1;

	if (j != 0 && events[j - 1].pos != events[j].pos)
	  {
	    grub_addr_t alloc_start, alloc_end;
	    alloc_start = max (events[j - 1].pos, target);
	    alloc_end = min (events[j].pos, target + size);
	    if (alloc_end > alloc_start)
	      {
		switch (typepre)
		  {
		  case CHUNK_TYPE_REGION_START:
		    allocate_regstart (alloc_start, alloc_end - alloc_start,
				       events[last_start].reg,
				       events[last_start].regancestor,
				       events[last_start].hancestor);
		    /* TODO: maintain a reverse lookup tree for hancestor.  */
		    {
		      unsigned k;
		      for (k = 0; k < N; k++)
			if (events[k].hancestor == events[last_start].head)
			  events[k].hancestor = events[last_start].hancestor;
		    }
		    break;
		  case CHUNK_TYPE_IN_REGION:
		    allocate_inreg (alloc_start, alloc_end - alloc_start,
				    events[last_start].head,
				    events[last_start].hancestor,
				    events[last_start].reg);
		    {
		      unsigned k;
		      for (k = 0; k < N; k++)
			if (events[k].hancestor == events[last_start].head)
			  events[k].hancestor = events[last_start].hancestor;
		    }
		    break;
#if GRUB_RELOCATOR_HAVE_FIRMWARE_REQUESTS
		  case CHUNK_TYPE_FIRMWARE:
		    {
		      grub_addr_t fstart, fend;
		      fstart
			= ALIGN_DOWN (alloc_start,
				      GRUB_RELOCATOR_FIRMWARE_REQUESTS_QUANT);
		      fend
			= ALIGN_UP (alloc_end,
				    GRUB_RELOCATOR_FIRMWARE_REQUESTS_QUANT);
#ifdef DEBUG_RELOCATOR_NOMEM_DPRINTF  
		      grub_dprintf ("relocator", "requesting %lx-%lx\n",
				    (unsigned long) fstart,
				    (unsigned long) fend);
#endif
		      /* The failure here can be very expensive.  */
		      if (!grub_relocator_firmware_alloc_region (fstart, 
								 fend - fstart))
			{
			  if (from_low_priv)
			    start = fend;
			  else
			    end = fstart;
			  goto retry;
			}
		      break;
		    }
#endif

#if GRUB_RELOCATOR_HAVE_LEFTOVERS
		  case CHUNK_TYPE_LEFTOVER:
		    {
		      unsigned offstart = alloc_start
			% GRUB_RELOCATOR_FIRMWARE_REQUESTS_QUANT;
		      unsigned offend = alloc_end
			% GRUB_RELOCATOR_FIRMWARE_REQUESTS_QUANT;
		      struct grub_relocator_fw_leftover *lo
			= events[last_lo].leftover;
		      if (offend == 0 && alloc_end != alloc_start)
			offend = GRUB_RELOCATOR_FIRMWARE_REQUESTS_QUANT;
		      lo->freebytes[offstart / 8]
			&= ((1 << (8 - (start % 8))) - 1);
		      grub_memset (lo->freebytes + (offstart + 7) / 8, 0,
				   offend / 8 - (offstart + 7) / 8);
		      lo->freebytes[offend / 8] &= ~((1 << (offend % 8)) - 1);
		    }
		    break;
#endif
		  }
		nallocs++;
	      }
	  }
	  
	switch (events[j].type)
	  {
	  case REG_BEG_START:
	  case IN_REG_START:
	    if (events[j].type == REG_BEG_START &&
		(grub_addr_t) (events[j].reg + 1) > target)
	      regbeg++;
	    else
	      inreg++;
	    last_start = j;
	    break;

	  case REG_BEG_END:
	  case IN_REG_END:
	    if (regbeg)
	      regbeg--;
	    else
	      inreg--;
	    break;

#if GRUB_RELOCATOR_HAVE_FIRMWARE_REQUESTS
	  case REG_FIRMWARE_START:
	    fwin++;
	    break;

	  case REG_FIRMWARE_END:
	    fwin--;
	    break;

	  case FIRMWARE_BLOCK_START:
	    fwb++;
	    break;

	  case FIRMWARE_BLOCK_END:
	    fwb--;
	    break;
#endif

#if GRUB_RELOCATOR_HAVE_LEFTOVERS
	  case REG_LEFTOVER_START:
	    fwlefto++;
	    last_lo = j;
	    break;

	  case REG_LEFTOVER_END:
	    fwlefto--;
	    break;
#endif
	  case COLLISION_START:
	    ncol++;
	    break;
	  case COLLISION_END:
	    ncol--;
	    break;
	  }

      }
  }

  /* Malloc is available again.  */
  grub_mm_base = base_saved;

  grub_free (eventt);
  grub_free (counter);

  {
    int last_start = 0;
    int inreg = 0, regbeg = 0, ncol = 0;
#if GRUB_RELOCATOR_HAVE_FIRMWARE_REQUESTS
    int fwin = 0, fwlefto = 0, fwb = 0;
#endif
    unsigned cural = 0;
    int oom = 0;
    res->subchunks = grub_malloc (sizeof (res->subchunks[0]) * nallocs);
    if (!res->subchunks)
      oom = 1;
    res->nsubchunks = nallocs;

    for (j = 0; j < N; j++)
      {
	int typepre;
	if (ncol)
	  typepre = -1;
	else if (regbeg)
	  typepre = CHUNK_TYPE_REGION_START;
	else if (inreg)
	  typepre = CHUNK_TYPE_IN_REGION;
#if GRUB_RELOCATOR_HAVE_FIRMWARE_REQUESTS
	else if (fwin && !fwb)
	  typepre = CHUNK_TYPE_FIRMWARE;
	else if (fwlefto && !fwb)
	  typepre = CHUNK_TYPE_LEFTOVER;
#endif
	else
	  typepre = -1;

	if (j != 0 && events[j - 1].pos != events[j].pos)
	  {
	    grub_addr_t alloc_start, alloc_end;
	    struct grub_relocator_subchunk tofree;
	    struct grub_relocator_subchunk *curschu = &tofree;
	    if (!oom)
	      curschu = &res->subchunks[cural];
	    alloc_start = max (events[j - 1].pos, target);
	    alloc_end = min (events[j].pos, target + size);
	    if (alloc_end > alloc_start)
	      {
#ifdef DEBUG_RELOCATOR_NOMEM_DPRINTF
		grub_dprintf ("relocator", "subchunk 0x%lx-0x%lx, %d\n",
			      (unsigned long) alloc_start,
			      (unsigned long) alloc_end, typepre);
#endif
		curschu->type = typepre;
		curschu->start = alloc_start;
		curschu->size = alloc_end - alloc_start;
		if (typepre == CHUNK_TYPE_REGION_START
		    || typepre == CHUNK_TYPE_IN_REGION)
		  {
		    curschu->reg = events[last_start].reg;
		    curschu->pre_size = alloc_start - events[j - 1].pos;
		  }
		if (!oom && (typepre == CHUNK_TYPE_REGION_START
#if GRUB_RELOCATOR_HAVE_FIRMWARE_REQUESTS
			     || typepre == CHUNK_TYPE_FIRMWARE
#endif
			     ))
		  {
		    struct grub_relocator_extra_block *ne;
		    ne = grub_malloc (sizeof (*ne));
		    if (!ne)
		      {
			oom = 1;
			grub_memcpy (&tofree, curschu, sizeof (tofree));
		      }
		    else
		      {
			ne->start = alloc_start;
			ne->end = alloc_end;
			ne->next = extra_blocks;
			ne->prev = &extra_blocks;
			if (extra_blocks)
			  extra_blocks->prev = &(ne->next);
			extra_blocks = ne;
			curschu->extra = ne;
		      }
		  }

#if GRUB_RELOCATOR_HAVE_LEFTOVERS
		if (!oom && typepre == CHUNK_TYPE_FIRMWARE)
		  {
		    grub_addr_t fstart, fend;

		    fstart
		      = ALIGN_DOWN (alloc_start,
				    GRUB_RELOCATOR_FIRMWARE_REQUESTS_QUANT);
		    fend
		      = ALIGN_UP (alloc_end,
				  GRUB_RELOCATOR_FIRMWARE_REQUESTS_QUANT);

		    {
		      struct grub_relocator_fw_leftover *lo1 = NULL;
		      struct grub_relocator_fw_leftover *lo2 = NULL;
		      if (fstart != alloc_start)
			lo1 = grub_malloc (sizeof (*lo1));
		      if (fend != alloc_end)
			lo2 = grub_malloc (sizeof (*lo2));
		      if ((!lo1 && fstart != alloc_start)
			  || (!lo2 && fend != alloc_end))
			{
			  struct grub_relocator_extra_block *ne;
			  grub_free (lo1);
			  grub_free (lo2);
			  lo1 = NULL;
			  lo2 = NULL;
			  oom = 1;
			  grub_memcpy (&tofree, curschu, sizeof (tofree));
			  ne = extra_blocks;
			  extra_blocks = extra_blocks->next;
			  grub_free (ne);
			}
		      if (lo1)
			{
			  lo1->quantstart = fstart;
			  grub_memset (lo1->freebytes, 0xff,
				       (alloc_start - fstart) / 8);
			  lo1->freebytes[(alloc_start - fstart) / 8]
			    = (1 << ((alloc_start - fstart) % 8)) - 1;
			  grub_memset (lo1->freebytes
				       + ((alloc_start - fstart) / 8) + 1, 0,
				       sizeof (lo1->freebytes)
				       - (alloc_start - fstart) / 8 - 1);
			  lo1->next = leftovers;
			  lo1->prev = &leftovers;
			  if (leftovers)
			    leftovers->prev = &lo1->next;
			  leftovers = lo1;
			}
		      if (lo2)
			{
			  lo2->quantstart
			    = fend - GRUB_RELOCATOR_FIRMWARE_REQUESTS_QUANT;
			  grub_memset (lo2->freebytes, 0,
				       (alloc_end - lo2->quantstart) / 8);
			  lo2->freebytes[(alloc_end - lo2->quantstart) / 8]
			    = ~((1 << ((alloc_end - lo2->quantstart) % 8)) - 1);
			  grub_memset (lo2->freebytes
				       + ((alloc_end - lo2->quantstart) / 8)
				       + 1, 0, sizeof (lo2->freebytes)
				       - (alloc_end - lo2->quantstart) / 8 - 1);
			  lo2->prev = &leftovers;
			  if (leftovers)
			    leftovers->prev = &lo2->next;
			  lo2->next = leftovers;
			  leftovers = lo2;
			}
		      curschu->pre = lo1;
		      curschu->post = lo2;
		    }
		  }

		if (typepre == CHUNK_TYPE_LEFTOVER)
		  {
		    curschu->pre = events[last_start].leftover;
		    curschu->post = events[last_start].leftover;
		  }
#endif

		if (!oom)
		  cural++;
		else
		  free_subchunk (&tofree);
	      }
	  }

	switch (events[j].type)
	  {
	  case REG_BEG_START:
	  case IN_REG_START:
	    if (events[j].type == REG_BEG_START &&
		(grub_addr_t) (events[j].reg + 1) > target)
	      regbeg++;
	    else
	      inreg++;
	    last_start = j;
	    break;

	  case REG_BEG_END:
	  case IN_REG_END:
	    inreg = regbeg = 0;
	    break;

#if GRUB_RELOCATOR_HAVE_FIRMWARE_REQUESTS
	  case REG_FIRMWARE_START:
	    fwin++;
	    break;

	  case REG_FIRMWARE_END:
	    fwin--;
	    break;

	  case FIRMWARE_BLOCK_START:
	    fwb++;
	    break;

	  case FIRMWARE_BLOCK_END:
	    fwb--;
	    break;
#endif

#if GRUB_RELOCATOR_HAVE_LEFTOVERS
	  case REG_LEFTOVER_START:
	    fwlefto++;
	    break;

	  case REG_LEFTOVER_END:
	    fwlefto--;
	    break;
#endif
	  case COLLISION_START:
	    ncol++;
	    break;
	  case COLLISION_END:
	    ncol--;
	    break;
	  }
      }
    if (oom)
      {
	unsigned i;
	for (i = 0; i < cural; i++)
	  free_subchunk (&res->subchunks[i]);
	grub_free (res->subchunks);
	grub_dprintf ("relocator", "allocation failed with out-of-memory\n");
	grub_free (events);

	return 0;
      }
  }

  res->src = target;
  res->size = size;

  grub_free (events);

  grub_dprintf ("relocator", "allocated: 0x%lx+0x%lx\n", (unsigned long) target,
		(unsigned long) size);

  return 1;
}

static void
adjust_limits (struct grub_relocator *rel, 
	       grub_phys_addr_t *min_addr, grub_phys_addr_t *max_addr,
	       grub_phys_addr_t in_min, grub_phys_addr_t in_max)
{
  struct grub_relocator_chunk *chunk;

  *min_addr = 0;
  *max_addr = rel->postchunks;

  /* Keep chunks in memory in the same order as they'll be after relocation.  */
  for (chunk = rel->chunks; chunk; chunk = chunk->next)
    {
      if (chunk->target > in_max && chunk->src < *max_addr
	  && chunk->src < rel->postchunks)
	*max_addr = chunk->src;
      if (chunk->target + chunk->size <= in_min
	  && chunk->src + chunk->size > *min_addr
	  && chunk->src < rel->postchunks)
	*min_addr = chunk->src + chunk->size;
    }
}

grub_err_t
grub_relocator_alloc_chunk_addr (struct grub_relocator *rel,
				 grub_relocator_chunk_t *out,
				 grub_phys_addr_t target, grub_size_t size)
{
  struct grub_relocator_chunk *chunk;
  grub_phys_addr_t min_addr = 0, max_addr;

  if (target > ~size)
    return grub_error (GRUB_ERR_BUG, "address is out of range");

  adjust_limits (rel, &min_addr, &max_addr, target, target);

  for (chunk = rel->chunks; chunk; chunk = chunk->next)
    if ((chunk->target <= target && target < chunk->target + chunk->size)
	|| (target <= chunk->target && chunk->target < target + size))
      return grub_error (GRUB_ERR_BUG, "overlap detected");

  chunk = grub_malloc (sizeof (struct grub_relocator_chunk));
  if (!chunk)
    return grub_errno;

  grub_dprintf ("relocator",
		"min_addr = 0x%llx, max_addr = 0x%llx, target = 0x%llx\n",
		(unsigned long long) min_addr, (unsigned long long) max_addr,
		(unsigned long long) target);

  do
    {
      /* A trick to improve Linux allocation.  */
#if defined (__i386__) || defined (__x86_64__)
      if (target < 0x100000)
	if (malloc_in_range (rel, rel->highestnonpostaddr, ~(grub_addr_t)0, 1,
			     size, chunk, 0, 1))
	  {
	    if (rel->postchunks > chunk->src)
	      rel->postchunks = chunk->src;
	    break;
	  }
#elif defined(__mips__) && (_MIPS_SIM == _ABI64)
      if (malloc_in_range (rel, target, max_addr, 8, size, chunk, 1, 0))
	break;
#endif
      if (malloc_in_range (rel, target, max_addr, 1, size, chunk, 1, 0))
	break;

      if (malloc_in_range (rel, min_addr, target, 1, size, chunk, 0, 0))
	break;

      if (malloc_in_range (rel, rel->highestnonpostaddr, ~(grub_addr_t)0, 1,
			   size, chunk, 0, 1))
	{
	  if (rel->postchunks > chunk->src)
	    rel->postchunks = chunk->src;
	  break;
	}

      grub_dprintf ("relocator", "not allocated\n");
      grub_free (chunk);
      return grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
    }
  while (0);

  grub_dprintf ("relocator", "allocated 0x%llx/0x%llx\n",
		(unsigned long long) chunk->src, (unsigned long long) target);

  if (rel->highestaddr < target + size)
    rel->highestaddr = target + size;

  if (rel->highestaddr < chunk->src + size)
    rel->highestaddr = chunk->src + size;

  if (chunk->src < rel->postchunks)
    {
      if (rel->highestnonpostaddr < target + size)
	rel->highestnonpostaddr = target + size;
      
      if (rel->highestnonpostaddr < chunk->src + size)
	rel->highestnonpostaddr = chunk->src + size;  
    }

  grub_dprintf ("relocator", "relocators_size=%ld\n",
		(unsigned long) rel->relocators_size);

  if (chunk->src < target)
    rel->relocators_size += grub_relocator_backward_size;
  if (chunk->src > target)
    rel->relocators_size += grub_relocator_forward_size;

  grub_dprintf ("relocator", "relocators_size=%ld\n",
		(unsigned long) rel->relocators_size);

  chunk->target = target;
  chunk->size = size;
  chunk->next = rel->chunks;
  rel->chunks = chunk;
  grub_dprintf ("relocator", "cur = %p, next = %p\n", rel->chunks,
		rel->chunks->next);

  chunk->srcv = grub_map_memory (chunk->src, chunk->size);
  *out = chunk;
#ifdef DEBUG_RELOCATOR
  grub_memset (chunk->srcv, 0xfa, chunk->size);
  grub_mm_check ();
#endif
  return GRUB_ERR_NONE;
}

/* Context for grub_relocator_alloc_chunk_align.  */
struct grub_relocator_alloc_chunk_align_ctx
{
  grub_phys_addr_t min_addr, max_addr;
  grub_size_t size, align;
  int preference;
  struct grub_relocator_chunk *chunk;
  int found;
};

/* Helper for grub_relocator_alloc_chunk_align.  */
static int
grub_relocator_alloc_chunk_align_iter (grub_uint64_t addr, grub_uint64_t sz,
				       grub_memory_type_t type, void *data)
{
  struct grub_relocator_alloc_chunk_align_ctx *ctx = data;
  grub_uint64_t candidate;

  if (type != GRUB_MEMORY_AVAILABLE)
    return 0;
  candidate = ALIGN_UP (addr, ctx->align);
  if (candidate < ctx->min_addr)
    candidate = ALIGN_UP (ctx->min_addr, ctx->align);
  if (candidate + ctx->size > addr + sz
      || candidate > ALIGN_DOWN (ctx->max_addr, ctx->align))
    return 0;
  if (ctx->preference == GRUB_RELOCATOR_PREFERENCE_HIGH)
    candidate = ALIGN_DOWN (min (addr + sz - ctx->size, ctx->max_addr),
			    ctx->align);
  if (!ctx->found || (ctx->preference == GRUB_RELOCATOR_PREFERENCE_HIGH
		      && candidate > ctx->chunk->target))
    ctx->chunk->target = candidate;
  if (!ctx->found || (ctx->preference == GRUB_RELOCATOR_PREFERENCE_LOW
		      && candidate < ctx->chunk->target))
    ctx->chunk->target = candidate;
  ctx->found = 1;
  return 0;
}

grub_err_t
grub_relocator_alloc_chunk_align (struct grub_relocator *rel,
				  grub_relocator_chunk_t *out,
				  grub_phys_addr_t min_addr,
				  grub_phys_addr_t max_addr,
				  grub_size_t size, grub_size_t align,
				  int preference,
				  int avoid_efi_boot_services)
{
  struct grub_relocator_alloc_chunk_align_ctx ctx = {
    .min_addr = min_addr,
    .max_addr = max_addr,
    .size = size,
    .align = align,
    .preference = preference,
    .found = 0
  };
  grub_addr_t min_addr2 = 0, max_addr2;

  if (max_addr > ~size)
    max_addr = ~size;

#ifdef GRUB_MACHINE_PCBIOS
  if (min_addr < 0x1000)
    min_addr = 0x1000;
#endif

  grub_dprintf ("relocator", "chunks = %p\n", rel->chunks);

  ctx.chunk = grub_malloc (sizeof (struct grub_relocator_chunk));
  if (!ctx.chunk)
    return grub_errno;

  if (malloc_in_range (rel, min_addr, max_addr, align,
		       size, ctx.chunk,
		       preference != GRUB_RELOCATOR_PREFERENCE_HIGH, 1))
    {
      grub_dprintf ("relocator", "allocated 0x%llx/0x%llx\n",
		    (unsigned long long) ctx.chunk->src,
		    (unsigned long long) ctx.chunk->src);
      grub_dprintf ("relocator", "chunks = %p\n", rel->chunks);
      ctx.chunk->target = ctx.chunk->src;
      ctx.chunk->size = size;
      ctx.chunk->next = rel->chunks;
      rel->chunks = ctx.chunk;
      ctx.chunk->srcv = grub_map_memory (ctx.chunk->src, ctx.chunk->size);
      *out = ctx.chunk;
      return GRUB_ERR_NONE;
    }

  adjust_limits (rel, &min_addr2, &max_addr2, min_addr, max_addr);
  grub_dprintf ("relocator", "Adjusted limits from %lx-%lx to %lx-%lx\n",
		(unsigned long) min_addr, (unsigned long) max_addr,
		(unsigned long) min_addr2, (unsigned long) max_addr2);

  do
    {
      if (malloc_in_range (rel, min_addr2, max_addr2, align,
			   size, ctx.chunk, 1, 1))
	break;

      if (malloc_in_range (rel, rel->highestnonpostaddr, ~(grub_addr_t)0, 1,
			   size, ctx.chunk, 0, 1))
	{
	  if (rel->postchunks > ctx.chunk->src)
	    rel->postchunks = ctx.chunk->src;
	  break;
	}

      return grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
    }
  while (0);

  {
#ifdef GRUB_MACHINE_EFI
    grub_efi_mmap_iterate (grub_relocator_alloc_chunk_align_iter, &ctx,
			   avoid_efi_boot_services);
#elif defined (__powerpc__) || defined (GRUB_MACHINE_XEN)
    (void) avoid_efi_boot_services;
    grub_machine_mmap_iterate (grub_relocator_alloc_chunk_align_iter, &ctx);
#else
    (void) avoid_efi_boot_services;
    grub_mmap_iterate (grub_relocator_alloc_chunk_align_iter, &ctx);
#endif
    if (!ctx.found)
      return grub_error (GRUB_ERR_BAD_OS, "couldn't find suitable memory target");
  }
  while (1)
    {
      struct grub_relocator_chunk *chunk2;
      for (chunk2 = rel->chunks; chunk2; chunk2 = chunk2->next)
	if ((chunk2->target <= ctx.chunk->target
	     && ctx.chunk->target < chunk2->target + chunk2->size)
	    || (ctx.chunk->target <= chunk2->target && chunk2->target
		< ctx.chunk->target + size))
	  {
	    if (preference == GRUB_RELOCATOR_PREFERENCE_HIGH)
	      ctx.chunk->target = ALIGN_DOWN (chunk2->target, align);
	    else
	      ctx.chunk->target = ALIGN_UP (chunk2->target + chunk2->size,
					    align);
	    break;
	  }
      if (!chunk2)
	break;
    }

  grub_dprintf ("relocator", "relocators_size=%ld\n",
		(unsigned long) rel->relocators_size);

  if (ctx.chunk->src < ctx.chunk->target)
    rel->relocators_size += grub_relocator_backward_size;
  if (ctx.chunk->src > ctx.chunk->target)
    rel->relocators_size += grub_relocator_forward_size;

  grub_dprintf ("relocator", "relocators_size=%ld\n",
		(unsigned long) rel->relocators_size);

  ctx.chunk->size = size;
  ctx.chunk->next = rel->chunks;
  rel->chunks = ctx.chunk;
  grub_dprintf ("relocator", "cur = %p, next = %p\n", rel->chunks,
		rel->chunks->next);
  ctx.chunk->srcv = grub_map_memory (ctx.chunk->src, ctx.chunk->size);
  *out = ctx.chunk;
#ifdef DEBUG_RELOCATOR
  grub_memset (ctx.chunk->srcv, 0xfa, ctx.chunk->size);
  grub_mm_check ();
#endif
  return GRUB_ERR_NONE;
}

void
grub_relocator_unload (struct grub_relocator *rel)
{
  struct grub_relocator_chunk *chunk, *next;
  if (!rel)
    return;
  for (chunk = rel->chunks; chunk; chunk = next)
    {
      unsigned i;
      for (i = 0; i < chunk->nsubchunks; i++) 
	free_subchunk (&chunk->subchunks[i]);
      grub_unmap_memory (chunk->srcv, chunk->size);
      next = chunk->next;
      grub_free (chunk->subchunks);
      grub_free (chunk);
    }
  grub_free (rel);
}

grub_err_t
grub_relocator_prepare_relocs (struct grub_relocator *rel, grub_addr_t addr,
			       void **relstart, grub_size_t *relsize)
{
  grub_uint8_t *rels;
  grub_uint8_t *rels0;
  struct grub_relocator_chunk *sorted;
  grub_size_t nchunks = 0;
  unsigned j;
  struct grub_relocator_chunk movers_chunk;

  grub_dprintf ("relocator", "Preparing relocs (size=%ld)\n",
		(unsigned long) rel->relocators_size);

  if (!malloc_in_range (rel, 0, ~(grub_addr_t)0 - rel->relocators_size + 1,
			grub_relocator_align,
			rel->relocators_size, &movers_chunk, 1, 1))
    return grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
  movers_chunk.srcv = rels = rels0
    = grub_map_memory (movers_chunk.src, movers_chunk.size);

  if (relsize)
    *relsize = rel->relocators_size;

  grub_dprintf ("relocator", "Relocs allocated at %p\n", movers_chunk.srcv);
  
  {
    unsigned i;
    grub_size_t count[257];
    struct grub_relocator_chunk *from, *to, *tmp;

    grub_memset (count, 0, sizeof (count));

    {
        struct grub_relocator_chunk *chunk;
	for (chunk = rel->chunks; chunk; chunk = chunk->next)
	  {
	    grub_dprintf ("relocator", "chunk %p->%p, 0x%lx\n", 
			  (void *) chunk->src, (void *) chunk->target,
			  (unsigned long) chunk->size);
	    nchunks++;
	    count[(chunk->src & 0xff) + 1]++;
	  }
    }
    from = grub_malloc (nchunks * sizeof (sorted[0]));
    to = grub_malloc (nchunks * sizeof (sorted[0]));
    if (!from || !to)
      {
	grub_free (from);
	grub_free (to);
	return grub_errno;
      }

    for (j = 0; j < 256; j++)
      count[j+1] += count[j];

    {
      struct grub_relocator_chunk *chunk;
      for (chunk = rel->chunks; chunk; chunk = chunk->next)
	from[count[chunk->src & 0xff]++] = *chunk;
    }

    for (i = 1; i < GRUB_CPU_SIZEOF_VOID_P; i++)
      {
	grub_memset (count, 0, sizeof (count));
	for (j = 0; j < nchunks; j++)
	  count[((from[j].src >> (8 * i)) & 0xff) + 1]++;
	for (j = 0; j < 256; j++)
	  count[j+1] += count[j];
	for (j = 0; j < nchunks; j++)
	  to[count[(from[j].src >> (8 * i)) & 0xff]++] = from[j];
	tmp = to;
	to = from;
	from = tmp;
      }
    sorted = from;
    grub_free (to);
  }

  for (j = 0; j < nchunks; j++)
    {
      grub_dprintf ("relocator", "sorted chunk %p->%p, 0x%lx\n", 
		    (void *) sorted[j].src, (void *) sorted[j].target,
		    (unsigned long) sorted[j].size);
      if (sorted[j].src < sorted[j].target)
	{
	  grub_cpu_relocator_backward ((void *) rels,
				       sorted[j].srcv,
				       grub_map_memory (sorted[j].target,
							sorted[j].size),
				       sorted[j].size);
	  rels += grub_relocator_backward_size;
	}
      if (sorted[j].src > sorted[j].target)
	{
	  grub_cpu_relocator_forward ((void *) rels,
				      sorted[j].srcv,
				      grub_map_memory (sorted[j].target,
						       sorted[j].size),
				      sorted[j].size);
	  rels += grub_relocator_forward_size;
	}
      if (sorted[j].src == sorted[j].target)
	grub_arch_sync_caches (sorted[j].srcv, sorted[j].size);
    }
  grub_cpu_relocator_jumper ((void *) rels, (grub_addr_t) addr);
  *relstart = rels0;
  grub_free (sorted);
  return GRUB_ERR_NONE;
}

void
grub_mm_check_real (const char *file, int line)
{
  grub_mm_region_t r;
  grub_mm_header_t p, pa;

  for (r = grub_mm_base; r; r = r->next)
    {
      pa = r->first;
      p = pa->next;
      if (p->magic == GRUB_MM_ALLOC_MAGIC)
	continue;
      do 
	{
	  if ((grub_addr_t) p < (grub_addr_t) (r + 1)
	      || (grub_addr_t) p >= (grub_addr_t) (r + 1) + r->size)
	    grub_fatal ("%s:%d: out of range pointer: %p\n", file, line, p);
	  if (p->magic != GRUB_MM_FREE_MAGIC)
	    grub_fatal ("%s:%d free magic broken at %p (0x%x)\n", file,
			line, p, p->magic);
	  pa = p;
	  p = pa->next;
	}
      while (pa != r->first);
    }
}
