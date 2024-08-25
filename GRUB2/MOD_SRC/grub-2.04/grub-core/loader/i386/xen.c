/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2013  Free Software Foundation, Inc.
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

#include <grub/loader.h>
#include <grub/memory.h>
#include <grub/normal.h>
#include <grub/file.h>
#include <grub/disk.h>
#include <grub/err.h>
#include <grub/misc.h>
#include <grub/types.h>
#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/term.h>
#include <grub/cpu/linux.h>
#include <grub/video.h>
#include <grub/video_fb.h>
#include <grub/command.h>
#include <grub/xen/relocator.h>
#include <grub/i18n.h>
#include <grub/elf.h>
#include <grub/elfload.h>
#include <grub/lib/cmdline.h>
#include <grub/xen.h>
#include <grub/xen_file.h>
#include <grub/linux.h>
#include <grub/i386/memory.h>
#include <grub/verify.h>

GRUB_MOD_LICENSE ("GPLv3+");

#ifdef __x86_64__
#define NUMBER_OF_LEVELS	4
#define INTERMEDIATE_OR		(GRUB_PAGE_PRESENT | GRUB_PAGE_RW | GRUB_PAGE_USER)
#define VIRT_MASK		0x0000ffffffffffffULL
#else
#define NUMBER_OF_LEVELS	3
#define INTERMEDIATE_OR		(GRUB_PAGE_PRESENT | GRUB_PAGE_RW)
#define VIRT_MASK		0x00000000ffffffffULL
#define HYPERVISOR_PUD_ADDRESS	0xc0000000ULL
#endif

struct grub_xen_mapping_lvl {
  grub_uint64_t virt_start;
  grub_uint64_t virt_end;
  grub_uint64_t pfn_start;
  grub_uint64_t n_pt_pages;
};

struct grub_xen_mapping {
  grub_uint64_t *where;
  struct grub_xen_mapping_lvl area;
  struct grub_xen_mapping_lvl lvls[NUMBER_OF_LEVELS];
};

struct xen_loader_state {
  struct grub_relocator *relocator;
  struct grub_relocator_xen_state state;
  struct start_info next_start;
  struct grub_xen_file_info xen_inf;
  grub_xen_mfn_t *virt_mfn_list;
  struct start_info *virt_start_info;
  grub_xen_mfn_t console_pfn;
  grub_uint64_t max_addr;
  grub_uint64_t pgtbl_end;
  struct xen_multiboot_mod_list *module_info_page;
  grub_uint64_t modules_target_start;
  grub_size_t n_modules;
  struct grub_xen_mapping *map_reloc;
  struct grub_xen_mapping mappings[XEN_MAX_MAPPINGS];
  int n_mappings;
  int loaded;
};

static struct xen_loader_state xen_state;

static grub_dl_t my_mod;

#define PAGE_SIZE (1UL << PAGE_SHIFT)
#define MAX_MODULES (PAGE_SIZE / sizeof (struct xen_multiboot_mod_list))
#define STACK_SIZE 1048576
#define ADDITIONAL_SIZE (1 << 19)
#define ALIGN_SIZE (1 << 22)
#define LOG_POINTERS_PER_PAGE 9
#define POINTERS_PER_PAGE (1 << LOG_POINTERS_PER_PAGE)

static grub_uint64_t
page2offset (grub_uint64_t page)
{
  return page << PAGE_SHIFT;
}

static grub_err_t
get_pgtable_size (grub_uint64_t from, grub_uint64_t to, grub_uint64_t pfn)
{
  struct grub_xen_mapping *map, *map_cmp;
  grub_uint64_t mask, bits;
  int i, m;

  if (xen_state.n_mappings == XEN_MAX_MAPPINGS)
    return grub_error (GRUB_ERR_BUG, "too many mapped areas");

  grub_dprintf ("xen", "get_pgtable_size %d from=%llx, to=%llx, pfn=%llx\n",
		xen_state.n_mappings, (unsigned long long) from,
		(unsigned long long) to, (unsigned long long) pfn);

  map = xen_state.mappings + xen_state.n_mappings;
  grub_memset (map, 0, sizeof (*map));

  map->area.virt_start = from & VIRT_MASK;
  map->area.virt_end = (to - 1) & VIRT_MASK;
  map->area.n_pt_pages = 0;

  for (i = NUMBER_OF_LEVELS - 1; i >= 0; i--)
    {
      map->lvls[i].pfn_start = pfn + map->area.n_pt_pages;
      if (i == NUMBER_OF_LEVELS - 1)
	{
	  if (xen_state.n_mappings == 0)
	    {
	      map->lvls[i].virt_start = 0;
	      map->lvls[i].virt_end = VIRT_MASK;
	      map->lvls[i].n_pt_pages = 1;
	      map->area.n_pt_pages++;
	    }
	  continue;
	}

      bits = PAGE_SHIFT + (i + 1) * LOG_POINTERS_PER_PAGE;
      mask = (1ULL << bits) - 1;
      map->lvls[i].virt_start = map->area.virt_start & ~mask;
      map->lvls[i].virt_end = map->area.virt_end | mask;
#ifdef __i386__
      /* PAE wants last root directory present. */
      if (i == 1 && to <= HYPERVISOR_PUD_ADDRESS && xen_state.n_mappings == 0)
	map->lvls[i].virt_end = VIRT_MASK;
#endif
      for (m = 0; m < xen_state.n_mappings; m++)
	{
	  map_cmp = xen_state.mappings + m;
	  if (map_cmp->lvls[i].virt_start == map_cmp->lvls[i].virt_end)
	    continue;
	  if (map->lvls[i].virt_start >= map_cmp->lvls[i].virt_start &&
	      map->lvls[i].virt_end <= map_cmp->lvls[i].virt_end)
	   {
	     map->lvls[i].virt_start = 0;
	     map->lvls[i].virt_end = 0;
	     break;
	   }
	   if (map->lvls[i].virt_start >= map_cmp->lvls[i].virt_start &&
	       map->lvls[i].virt_start <= map_cmp->lvls[i].virt_end)
	     map->lvls[i].virt_start = map_cmp->lvls[i].virt_end + 1;
	   if (map->lvls[i].virt_end >= map_cmp->lvls[i].virt_start &&
	       map->lvls[i].virt_end <= map_cmp->lvls[i].virt_end)
	     map->lvls[i].virt_end = map_cmp->lvls[i].virt_start - 1;
	}
      if (map->lvls[i].virt_start < map->lvls[i].virt_end)
	map->lvls[i].n_pt_pages =
	  ((map->lvls[i].virt_end - map->lvls[i].virt_start) >> bits) + 1;
      map->area.n_pt_pages += map->lvls[i].n_pt_pages;
      grub_dprintf ("xen", "get_pgtable_size level %d: virt %llx-%llx %d pts\n",
		    i, (unsigned long long)  map->lvls[i].virt_start,
		    (unsigned long long)  map->lvls[i].virt_end,
		    (int) map->lvls[i].n_pt_pages);
    }

  grub_dprintf ("xen", "get_pgtable_size return: %d page tables\n",
		(int) map->area.n_pt_pages);

  xen_state.state.paging_start[xen_state.n_mappings] = pfn;
  xen_state.state.paging_size[xen_state.n_mappings] = map->area.n_pt_pages;

  return GRUB_ERR_NONE;
}

static grub_uint64_t *
get_pg_table_virt (int mapping, int level)
{
  grub_uint64_t pfn;
  struct grub_xen_mapping *map;

  map = xen_state.mappings + mapping;
  pfn = map->lvls[level].pfn_start - map->lvls[NUMBER_OF_LEVELS - 1].pfn_start;
  return map->where + pfn * POINTERS_PER_PAGE;
}

static grub_uint64_t
get_pg_table_prot (int level, grub_uint64_t pfn)
{
  int m;
  grub_uint64_t pfn_s, pfn_e;

  if (level > 0)
    return INTERMEDIATE_OR;
  for (m = 0; m < xen_state.n_mappings; m++)
    {
      pfn_s = xen_state.mappings[m].lvls[NUMBER_OF_LEVELS - 1].pfn_start;
      pfn_e = xen_state.mappings[m].area.n_pt_pages + pfn_s;
      if (pfn >= pfn_s && pfn < pfn_e)
	return GRUB_PAGE_PRESENT | GRUB_PAGE_USER;
    }
  return GRUB_PAGE_PRESENT | GRUB_PAGE_RW | GRUB_PAGE_USER;
}

static void
generate_page_table (grub_xen_mfn_t *mfn_list)
{
  int l, m1, m2;
  long p, p_s, p_e;
  grub_uint64_t start, end, pfn;
  grub_uint64_t *pg;
  struct grub_xen_mapping_lvl *lvl;

  for (m1 = 0; m1 < xen_state.n_mappings; m1++)
    grub_memset (xen_state.mappings[m1].where, 0,
		 xen_state.mappings[m1].area.n_pt_pages * PAGE_SIZE);

  for (l = NUMBER_OF_LEVELS - 1; l >= 0; l--)
    {
      for (m1 = 0; m1 < xen_state.n_mappings; m1++)
	{
	  start = xen_state.mappings[m1].lvls[l].virt_start;
	  end = xen_state.mappings[m1].lvls[l].virt_end;
	  pg = get_pg_table_virt(m1, l);
	  for (m2 = 0; m2 < xen_state.n_mappings; m2++)
	    {
	      lvl = (l > 0) ? xen_state.mappings[m2].lvls + l - 1
			    : &xen_state.mappings[m2].area;
	      if (l > 0 && lvl->n_pt_pages == 0)
		continue;
	      if (lvl->virt_start >= end || lvl->virt_end <= start)
		continue;
	      p_s = (grub_max (start, lvl->virt_start) - start) >>
		    (PAGE_SHIFT + l * LOG_POINTERS_PER_PAGE);
	      p_e = (grub_min (end, lvl->virt_end) - start) >>
		    (PAGE_SHIFT + l * LOG_POINTERS_PER_PAGE);
	      pfn = ((grub_max (start, lvl->virt_start) - lvl->virt_start) >>
		     (PAGE_SHIFT + l * LOG_POINTERS_PER_PAGE)) + lvl->pfn_start;
	      grub_dprintf ("xen", "write page table entries level %d pg %p "
			    "mapping %d/%d index %lx-%lx pfn %llx\n",
			    l, pg, m1, m2, p_s, p_e, (unsigned long long) pfn);
	      for (p = p_s; p <= p_e; p++)
		{
		  pg[p] = page2offset (mfn_list[pfn]) |
			  get_pg_table_prot (l, pfn);
		  pfn++;
		}
	    }
	}
    }
}

static grub_err_t
set_mfns (grub_xen_mfn_t pfn)
{
  grub_xen_mfn_t i, t;
  grub_xen_mfn_t cn_pfn = -1, st_pfn = -1;
  struct mmu_update m2p_updates[4];


  for (i = 0; i < grub_xen_start_page_addr->nr_pages; i++)
    {
      if (xen_state.virt_mfn_list[i] ==
	  grub_xen_start_page_addr->console.domU.mfn)
	cn_pfn = i;
      if (xen_state.virt_mfn_list[i] == grub_xen_start_page_addr->store_mfn)
	st_pfn = i;
    }
  if (cn_pfn == (grub_xen_mfn_t)-1)
    return grub_error (GRUB_ERR_BUG, "no console");
  if (st_pfn == (grub_xen_mfn_t)-1)
    return grub_error (GRUB_ERR_BUG, "no store");
  t = xen_state.virt_mfn_list[pfn];
  xen_state.virt_mfn_list[pfn] = xen_state.virt_mfn_list[cn_pfn];
  xen_state.virt_mfn_list[cn_pfn] = t;
  t = xen_state.virt_mfn_list[pfn + 1];
  xen_state.virt_mfn_list[pfn + 1] = xen_state.virt_mfn_list[st_pfn];
  xen_state.virt_mfn_list[st_pfn] = t;

  m2p_updates[0].ptr =
    page2offset (xen_state.virt_mfn_list[pfn]) | MMU_MACHPHYS_UPDATE;
  m2p_updates[0].val = pfn;
  m2p_updates[1].ptr =
    page2offset (xen_state.virt_mfn_list[pfn + 1]) | MMU_MACHPHYS_UPDATE;
  m2p_updates[1].val = pfn + 1;
  m2p_updates[2].ptr =
    page2offset (xen_state.virt_mfn_list[cn_pfn]) | MMU_MACHPHYS_UPDATE;
  m2p_updates[2].val = cn_pfn;
  m2p_updates[3].ptr =
    page2offset (xen_state.virt_mfn_list[st_pfn]) | MMU_MACHPHYS_UPDATE;
  m2p_updates[3].val = st_pfn;

  grub_xen_mmu_update (m2p_updates, 4, NULL, DOMID_SELF);

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_xen_p2m_alloc (void)
{
  grub_relocator_chunk_t ch;
  grub_size_t p2msize, p2malloc;
  grub_err_t err;
  struct grub_xen_mapping *map;

  if (xen_state.virt_mfn_list)
    return GRUB_ERR_NONE;

  map = xen_state.mappings + xen_state.n_mappings;
  p2msize = ALIGN_UP (sizeof (grub_xen_mfn_t) *
		      grub_xen_start_page_addr->nr_pages, PAGE_SIZE);
  if (xen_state.xen_inf.has_p2m_base)
    {
      err = get_pgtable_size (xen_state.xen_inf.p2m_base,
			      xen_state.xen_inf.p2m_base + p2msize,
			      (xen_state.max_addr + p2msize) >> PAGE_SHIFT);
      if (err)
	return err;

      map->area.pfn_start = xen_state.max_addr >> PAGE_SHIFT;
      p2malloc = p2msize + page2offset (map->area.n_pt_pages);
      xen_state.n_mappings++;
      xen_state.next_start.mfn_list = xen_state.xen_inf.p2m_base;
      xen_state.next_start.first_p2m_pfn = map->area.pfn_start;
      xen_state.next_start.nr_p2m_frames = p2malloc >> PAGE_SHIFT;
    }
  else
    {
      xen_state.next_start.mfn_list =
	xen_state.max_addr + xen_state.xen_inf.virt_base;
      p2malloc = p2msize;
    }

  xen_state.state.mfn_list = xen_state.max_addr;
  err = grub_relocator_alloc_chunk_addr (xen_state.relocator, &ch,
					 xen_state.max_addr, p2malloc);
  if (err)
    return err;
  xen_state.virt_mfn_list = get_virtual_current_address (ch);
  if (xen_state.xen_inf.has_p2m_base)
    map->where = (grub_uint64_t *) xen_state.virt_mfn_list +
		 p2msize / sizeof (grub_uint64_t);
  grub_memcpy (xen_state.virt_mfn_list,
	       (void *) grub_xen_start_page_addr->mfn_list, p2msize);
  xen_state.max_addr += p2malloc;

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_xen_special_alloc (void)
{
  grub_relocator_chunk_t ch;
  grub_err_t err;

  if (xen_state.virt_start_info)
    return GRUB_ERR_NONE;

  err = grub_relocator_alloc_chunk_addr (xen_state.relocator, &ch,
					 xen_state.max_addr,
					 sizeof (xen_state.next_start));
  if (err)
    return err;
  xen_state.state.start_info = xen_state.max_addr + xen_state.xen_inf.virt_base;
  xen_state.virt_start_info = get_virtual_current_address (ch);
  xen_state.max_addr =
    ALIGN_UP (xen_state.max_addr + sizeof (xen_state.next_start), PAGE_SIZE);
  xen_state.console_pfn = xen_state.max_addr >> PAGE_SHIFT;
  xen_state.max_addr += 2 * PAGE_SIZE;

  xen_state.next_start.nr_pages = grub_xen_start_page_addr->nr_pages;
  grub_memcpy (xen_state.next_start.magic, grub_xen_start_page_addr->magic,
	       sizeof (xen_state.next_start.magic));
  xen_state.next_start.store_mfn = grub_xen_start_page_addr->store_mfn;
  xen_state.next_start.store_evtchn = grub_xen_start_page_addr->store_evtchn;
  xen_state.next_start.console.domU = grub_xen_start_page_addr->console.domU;
  xen_state.next_start.shared_info = grub_xen_start_page_addr->shared_info;

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_xen_pt_alloc (void)
{
  grub_relocator_chunk_t ch;
  grub_err_t err;
  grub_uint64_t nr_info_pages;
  grub_uint64_t nr_need_pages;
  grub_uint64_t try_virt_end;
  struct grub_xen_mapping *map;

  if (xen_state.pgtbl_end)
    return GRUB_ERR_NONE;

  map = xen_state.mappings + xen_state.n_mappings;
  xen_state.map_reloc = map + 1;

  xen_state.next_start.pt_base =
    xen_state.max_addr + xen_state.xen_inf.virt_base;
  nr_info_pages = xen_state.max_addr >> PAGE_SHIFT;
  nr_need_pages = nr_info_pages;

  while (1)
    {
      try_virt_end = ALIGN_UP (xen_state.xen_inf.virt_base +
			       page2offset (nr_need_pages) +
			       ADDITIONAL_SIZE + STACK_SIZE, ALIGN_SIZE);

      err = get_pgtable_size (xen_state.xen_inf.virt_base, try_virt_end,
			      nr_info_pages);
      if (err)
	return err;
      xen_state.n_mappings++;

      /* Map the relocator page either at virtual 0 or after end of area. */
      nr_need_pages = nr_info_pages + map->area.n_pt_pages;
      if (xen_state.xen_inf.virt_base)
	err = get_pgtable_size (0, PAGE_SIZE, nr_need_pages);
      else
	err = get_pgtable_size (try_virt_end, try_virt_end + PAGE_SIZE,
				nr_need_pages);
      if (err)
	return err;
      nr_need_pages += xen_state.map_reloc->area.n_pt_pages;

      if (xen_state.xen_inf.virt_base + page2offset (nr_need_pages) <=
	  try_virt_end)
	break;

      xen_state.n_mappings--;
    }

  xen_state.n_mappings++;
  nr_need_pages = map->area.n_pt_pages + xen_state.map_reloc->area.n_pt_pages;
  err = grub_relocator_alloc_chunk_addr (xen_state.relocator, &ch,
					 xen_state.max_addr,
					 page2offset (nr_need_pages));
  if (err)
    return err;

  map->where = get_virtual_current_address (ch);
  map->area.pfn_start = 0;
  xen_state.max_addr += page2offset (nr_need_pages);
  xen_state.state.stack =
    xen_state.max_addr + STACK_SIZE + xen_state.xen_inf.virt_base;
  xen_state.next_start.nr_pt_frames = nr_need_pages;
  xen_state.max_addr = try_virt_end - xen_state.xen_inf.virt_base;
  xen_state.pgtbl_end = xen_state.max_addr >> PAGE_SHIFT;
  xen_state.map_reloc->where = (grub_uint64_t *) ((char *) map->where +
					page2offset (map->area.n_pt_pages));

  return GRUB_ERR_NONE;
}

/* Allocate all not yet allocated areas mapped by initial page tables. */
static grub_err_t
grub_xen_alloc_boot_data (void)
{
  grub_err_t err;

  if (!xen_state.xen_inf.has_p2m_base)
    {
      err = grub_xen_p2m_alloc ();
      if (err)
	return err;
    }
  err = grub_xen_special_alloc ();
  if (err)
    return err;
  err = grub_xen_pt_alloc ();
  if (err)
    return err;

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_xen_boot (void)
{
  grub_err_t err;
  grub_uint64_t nr_pages;
  struct gnttab_set_version gnttab_setver;
  grub_size_t i;

  if (grub_xen_n_allocated_shared_pages)
    return grub_error (GRUB_ERR_BUG, "active grants");

  err = grub_xen_alloc_boot_data ();
  if (err)
    return err;
  if (xen_state.xen_inf.has_p2m_base)
    {
      err = grub_xen_p2m_alloc ();
      if (err)
	return err;
    }

  err = set_mfns (xen_state.console_pfn);
  if (err)
    return err;

  nr_pages = xen_state.max_addr >> PAGE_SHIFT;

  grub_dprintf ("xen", "bootstrap domain %llx+%llx\n",
		(unsigned long long) xen_state.xen_inf.virt_base,
		(unsigned long long) page2offset (nr_pages));

  xen_state.map_reloc->area.pfn_start = nr_pages;
  generate_page_table (xen_state.virt_mfn_list);

  xen_state.state.entry_point = xen_state.xen_inf.entry_point;

  *xen_state.virt_start_info = xen_state.next_start;

  grub_memset (&gnttab_setver, 0, sizeof (gnttab_setver));

  gnttab_setver.version = 1;
  grub_xen_grant_table_op (GNTTABOP_set_version, &gnttab_setver, 1);

  for (i = 0; i < ARRAY_SIZE (grub_xen_shared_info->evtchn_pending); i++)
    grub_xen_shared_info->evtchn_pending[i] = 0;

  return grub_relocator_xen_boot (xen_state.relocator, xen_state.state, nr_pages,
				  xen_state.xen_inf.virt_base <
				  PAGE_SIZE ? page2offset (nr_pages) : 0,
				  xen_state.pgtbl_end - 1,
				  page2offset (xen_state.pgtbl_end - 1) +
				  xen_state.xen_inf.virt_base);
}

static void
grub_xen_reset (void)
{
  grub_relocator_unload (xen_state.relocator);

  grub_memset (&xen_state, 0, sizeof (xen_state));
}

static grub_err_t
grub_xen_unload (void)
{
  grub_xen_reset ();
  grub_dl_unref (my_mod);
  return GRUB_ERR_NONE;
}

#define HYPERCALL_INTERFACE_SIZE 32

#ifdef __x86_64__
static grub_uint8_t template[] =
  {
    0x51, /* push %rcx */
    0x41, 0x53, /* push %r11 */
    0x48, 0xc7, 0xc0, 0xbb, 0xaa, 0x00, 0x00, 	/* mov    $0xaabb,%rax */
    0x0f, 0x05,  /* syscall  */
    0x41, 0x5b, /* pop %r11 */
    0x59, /* pop %rcx  */
    0xc3 /* ret */
  };

static grub_uint8_t template_iret[] =
  {
    0x51, /* push   %rcx */
    0x41, 0x53, /* push   %r11 */
    0x50, /* push   %rax */
    0x48, 0xc7, 0xc0, 0x17, 0x00, 0x00, 0x00, /* mov    $0x17,%rax */
    0x0f, 0x05 /* syscall */
  };
#define CALLNO_OFFSET 6
#else

static grub_uint8_t template[] =
  {
    0xb8, 0xbb, 0xaa, 0x00, 0x00, /* mov imm32, %eax */
    0xcd, 0x82,  /* int $0x82  */
    0xc3 /* ret */
  };

static grub_uint8_t template_iret[] =
  {
    0x50, /* push   %eax */
    0xb8, 0x17, 0x00, 0x00, 0x00, /* mov    $0x17,%eax */
    0xcd, 0x82,  /* int $0x82  */
  };
#define CALLNO_OFFSET 1

#endif


static void
set_hypercall_interface (grub_uint8_t *tgt, unsigned callno)
{
  if (callno == 0x17)
    {
      grub_memcpy (tgt, template_iret, ARRAY_SIZE (template_iret));
      grub_memset (tgt + ARRAY_SIZE (template_iret), 0xcc,
		   HYPERCALL_INTERFACE_SIZE - ARRAY_SIZE (template_iret));
      return;
    }
  grub_memcpy (tgt, template, ARRAY_SIZE (template));
  grub_memset (tgt + ARRAY_SIZE (template), 0xcc,
	       HYPERCALL_INTERFACE_SIZE - ARRAY_SIZE (template));
  tgt[CALLNO_OFFSET] = callno & 0xff;
  tgt[CALLNO_OFFSET + 1] = callno >> 8;
}

#ifdef __x86_64__
#define grub_elfXX_load grub_elf64_load
#else
#define grub_elfXX_load grub_elf32_load
#endif

static grub_err_t
grub_cmd_xen (grub_command_t cmd __attribute__ ((unused)),
	      int argc, char *argv[])
{
  grub_file_t file;
  grub_elf_t elf;
  grub_err_t err;
  void *kern_chunk_src;
  grub_relocator_chunk_t ch;
  grub_addr_t kern_start;
  grub_addr_t kern_end;

  if (argc == 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  /* Call grub_loader_unset early to avoid it being called by grub_loader_set */
  grub_loader_unset ();

  grub_xen_reset ();

  err = grub_create_loader_cmdline (argc - 1, argv + 1,
				    (char *) xen_state.next_start.cmd_line,
				    sizeof (xen_state.next_start.cmd_line) - 1,
				    GRUB_VERIFY_KERNEL_CMDLINE);
  if (err)
    return err;

  file = grub_file_open (argv[0], GRUB_FILE_TYPE_LINUX_KERNEL);
  if (!file)
    return grub_errno;

  elf = grub_xen_file (file);
  if (!elf)
    goto fail;

  err = grub_xen_get_info (elf, &xen_state.xen_inf);
  if (err)
    goto fail;
#ifdef __x86_64__
  if (xen_state.xen_inf.arch != GRUB_XEN_FILE_X86_64)
#else
  if (xen_state.xen_inf.arch != GRUB_XEN_FILE_I386_PAE
      && xen_state.xen_inf.arch != GRUB_XEN_FILE_I386_PAE_BIMODE)
#endif
    {
      grub_error (GRUB_ERR_BAD_OS, "incompatible architecture: %d",
		  xen_state.xen_inf.arch);
      goto fail;
    }

  if (xen_state.xen_inf.virt_base & (PAGE_SIZE - 1))
    {
      grub_error (GRUB_ERR_BAD_OS, "unaligned virt_base");
      goto fail;
    }
  grub_dprintf ("xen", "virt_base = %llx, entry = %llx\n",
		(unsigned long long) xen_state.xen_inf.virt_base,
		(unsigned long long) xen_state.xen_inf.entry_point);

  xen_state.relocator = grub_relocator_new ();
  if (!xen_state.relocator)
    goto fail;

  kern_start = xen_state.xen_inf.kern_start - xen_state.xen_inf.paddr_offset;
  kern_end = xen_state.xen_inf.kern_end - xen_state.xen_inf.paddr_offset;

  if (xen_state.xen_inf.has_hypercall_page)
    {
      grub_dprintf ("xen", "hypercall page at 0x%llx\n",
		    (unsigned long long) xen_state.xen_inf.hypercall_page);
      kern_start = grub_min (kern_start, xen_state.xen_inf.hypercall_page -
					 xen_state.xen_inf.virt_base);
      kern_end = grub_max (kern_end, xen_state.xen_inf.hypercall_page -
				     xen_state.xen_inf.virt_base + PAGE_SIZE);
    }

  xen_state.max_addr = ALIGN_UP (kern_end, PAGE_SIZE);

  err = grub_relocator_alloc_chunk_addr (xen_state.relocator, &ch, kern_start,
					 kern_end - kern_start);
  if (err)
    goto fail;
  kern_chunk_src = get_virtual_current_address (ch);

  grub_dprintf ("xen", "paddr_offset = 0x%llx\n",
		(unsigned long long) xen_state.xen_inf.paddr_offset);
  grub_dprintf ("xen", "kern_start = 0x%llx, kern_end = 0x%llx\n",
		(unsigned long long) xen_state.xen_inf.kern_start,
		(unsigned long long) xen_state.xen_inf.kern_end);

  err = grub_elfXX_load (elf, argv[0],
			 (grub_uint8_t *) kern_chunk_src - kern_start
			 - xen_state.xen_inf.paddr_offset, 0, 0, 0);

  if (xen_state.xen_inf.has_hypercall_page)
    {
      unsigned i;
      for (i = 0; i < PAGE_SIZE / HYPERCALL_INTERFACE_SIZE; i++)
	set_hypercall_interface ((grub_uint8_t *) kern_chunk_src +
				 i * HYPERCALL_INTERFACE_SIZE +
				 xen_state.xen_inf.hypercall_page -
				 xen_state.xen_inf.virt_base - kern_start, i);
    }

  if (err)
    goto fail;

  grub_dl_ref (my_mod);
  xen_state.loaded = 1;

  grub_loader_set (grub_xen_boot, grub_xen_unload, 0);

  goto fail;

fail:
  /* grub_errno might be clobbered by further calls, save the error reason. */
  err = grub_errno;

  if (elf)
    grub_elf_close (elf);
  else if (file)
    grub_file_close (file);

  if (err != GRUB_ERR_NONE)
    grub_xen_reset ();

  return err;
}

static grub_err_t
grub_cmd_initrd (grub_command_t cmd __attribute__ ((unused)),
		 int argc, char *argv[])
{
  grub_size_t size = 0;
  grub_err_t err;
  struct grub_linux_initrd_context initrd_ctx = { 0, 0, 0 };
  grub_relocator_chunk_t ch;

  if (argc == 0)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
      goto fail;
    }

  if (!xen_state.loaded)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT,
		  N_("you need to load the kernel first"));
      goto fail;
    }

  if (xen_state.next_start.mod_start || xen_state.next_start.mod_len)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("initrd already loaded"));
      goto fail;
    }

  if (xen_state.xen_inf.unmapped_initrd)
    {
      err = grub_xen_alloc_boot_data ();
      if (err)
	goto fail;
    }

  if (grub_initrd_init (argc, argv, &initrd_ctx))
    goto fail;

  size = grub_get_initrd_size (&initrd_ctx);

  if (size)
    {
      err = grub_relocator_alloc_chunk_addr (xen_state.relocator, &ch,
					     xen_state.max_addr, size);
      if (err)
	goto fail;

      if (grub_initrd_load (&initrd_ctx, argv,
			    get_virtual_current_address (ch)))
	goto fail;
    }

  xen_state.next_start.mod_len = size;

  if (xen_state.xen_inf.unmapped_initrd)
    {
      xen_state.next_start.flags |= SIF_MOD_START_PFN;
      xen_state.next_start.mod_start = xen_state.max_addr >> PAGE_SHIFT;
    }
  else
    xen_state.next_start.mod_start =
      xen_state.max_addr + xen_state.xen_inf.virt_base;

  grub_dprintf ("xen", "Initrd, addr=0x%x, size=0x%x\n",
		(unsigned) (xen_state.max_addr + xen_state.xen_inf.virt_base),
		(unsigned) size);

  xen_state.max_addr = ALIGN_UP (xen_state.max_addr + size, PAGE_SIZE);

fail:
  grub_initrd_close (&initrd_ctx);

  return grub_errno;
}

static grub_err_t
grub_cmd_module (grub_command_t cmd __attribute__ ((unused)),
		 int argc, char *argv[])
{
  grub_size_t size = 0;
  grub_err_t err;
  grub_relocator_chunk_t ch;
  grub_size_t cmdline_len;
  int nounzip = 0;
  grub_file_t file;

  if (argc == 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  if (grub_strcmp (argv[0], "--nounzip") == 0)
    {
      argv++;
      argc--;
      nounzip = 1;
    }

  if (argc == 0)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  if (!xen_state.loaded)
    {
      return grub_error (GRUB_ERR_BAD_ARGUMENT,
			 N_("you need to load the kernel first"));
    }

  if ((xen_state.next_start.mod_start || xen_state.next_start.mod_len) &&
      !xen_state.module_info_page)
    {
      return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("initrd already loaded"));
    }

  /* Leave one space for terminator.  */
  if (xen_state.n_modules >= MAX_MODULES - 1)
    {
      return grub_error (GRUB_ERR_BAD_ARGUMENT, "too many modules");
    }

  if (!xen_state.module_info_page)
    {
      xen_state.xen_inf.unmapped_initrd = 0;
      xen_state.n_modules = 0;
      xen_state.max_addr = ALIGN_UP (xen_state.max_addr, PAGE_SIZE);
      xen_state.modules_target_start = xen_state.max_addr;
      xen_state.next_start.mod_start =
	xen_state.max_addr + xen_state.xen_inf.virt_base;
      xen_state.next_start.flags |= SIF_MULTIBOOT_MOD;

      err = grub_relocator_alloc_chunk_addr (xen_state.relocator, &ch,
					     xen_state.max_addr, MAX_MODULES
					     *
					     sizeof (xen_state.module_info_page
						     [0]));
      if (err)
	return err;
      xen_state.module_info_page = get_virtual_current_address (ch);
      grub_memset (xen_state.module_info_page, 0, MAX_MODULES
		   * sizeof (xen_state.module_info_page[0]));
      xen_state.max_addr +=
	MAX_MODULES * sizeof (xen_state.module_info_page[0]);
    }

  xen_state.max_addr = ALIGN_UP (xen_state.max_addr, PAGE_SIZE);

  file = grub_file_open (argv[0], GRUB_FILE_TYPE_LINUX_INITRD |
			 (nounzip ? GRUB_FILE_TYPE_NO_DECOMPRESS : GRUB_FILE_TYPE_NONE));
  if (!file)
    return grub_errno;
  size = grub_file_size (file);

  cmdline_len = grub_loader_cmdline_size (argc - 1, argv + 1);

  err = grub_relocator_alloc_chunk_addr (xen_state.relocator, &ch,
					 xen_state.max_addr, cmdline_len);
  if (err)
    goto fail;

  err = grub_create_loader_cmdline (argc - 1, argv + 1,
				    get_virtual_current_address (ch), cmdline_len,
				    GRUB_VERIFY_MODULE_CMDLINE);
  if (err)
    goto fail;

  xen_state.module_info_page[xen_state.n_modules].cmdline =
    xen_state.max_addr - xen_state.modules_target_start;
  xen_state.max_addr = ALIGN_UP (xen_state.max_addr + cmdline_len, PAGE_SIZE);

  if (size)
    {
      err = grub_relocator_alloc_chunk_addr (xen_state.relocator, &ch,
					     xen_state.max_addr, size);
      if (err)
	goto fail;
      if (grub_file_read (file, get_virtual_current_address (ch), size)
	  != (grub_ssize_t) size)
	{
	  if (!grub_errno)
	    grub_error (GRUB_ERR_FILE_READ_ERROR,
			N_("premature end of file %s"), argv[0]);
	  goto fail;
	}
    }
  xen_state.next_start.mod_len =
    xen_state.max_addr + size - xen_state.modules_target_start;
  xen_state.module_info_page[xen_state.n_modules].mod_start =
    xen_state.max_addr - xen_state.modules_target_start;
  xen_state.module_info_page[xen_state.n_modules].mod_end =
    xen_state.max_addr + size - xen_state.modules_target_start;

  xen_state.n_modules++;
  grub_dprintf ("xen", "module, addr=0x%x, size=0x%x\n",
		(unsigned) xen_state.max_addr, (unsigned) size);
  xen_state.max_addr = ALIGN_UP (xen_state.max_addr + size, PAGE_SIZE);


fail:
  grub_file_close (file);

  return grub_errno;
}

static grub_command_t cmd_xen, cmd_initrd, cmd_module, cmd_multiboot;

GRUB_MOD_INIT (xen)
{
  cmd_xen = grub_register_command ("linux", grub_cmd_xen,
				   0, N_("Load Linux."));
  cmd_multiboot = grub_register_command ("multiboot", grub_cmd_xen,
					 0, N_("Load Linux."));
  cmd_initrd = grub_register_command ("initrd", grub_cmd_initrd,
				      0, N_("Load initrd."));
  cmd_module = grub_register_command ("module", grub_cmd_module,
				      0, N_("Load module."));
  my_mod = mod;
}

GRUB_MOD_FINI (xen)
{
  grub_unregister_command (cmd_xen);
  grub_unregister_command (cmd_initrd);
  grub_unregister_command (cmd_multiboot);
  grub_unregister_command (cmd_module);
}
