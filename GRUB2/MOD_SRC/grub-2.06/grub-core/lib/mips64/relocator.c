/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2017  Free Software Foundation, Inc.
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
#include <grub/misc.h>

#include <grub/types.h>
#include <grub/types.h>
#include <grub/err.h>
#include <grub/cache.h>

#include <grub/mips64/relocator.h>
#include <grub/relocator_private.h>

extern grub_uint8_t grub_relocator_forward_start;
extern grub_uint8_t grub_relocator_forward_end;
extern grub_uint8_t grub_relocator_backward_start;
extern grub_uint8_t grub_relocator_backward_end;

#define REGW_SIZEOF (6 * sizeof (grub_uint32_t))
#define JUMP_SIZEOF (2 * sizeof (grub_uint32_t))

#define RELOCATOR_SRC_SIZEOF(x) (&grub_relocator_##x##_end \
				 - &grub_relocator_##x##_start)
#define RELOCATOR_SIZEOF(x)	(RELOCATOR_SRC_SIZEOF(x) \
				 + REGW_SIZEOF * 3)
grub_size_t grub_relocator_align = sizeof (grub_uint64_t);
grub_size_t grub_relocator_forward_size;
grub_size_t grub_relocator_backward_size;
grub_size_t grub_relocator_jumper_size = JUMP_SIZEOF + REGW_SIZEOF;

void
grub_cpu_relocator_init (void)
{
  grub_relocator_forward_size = RELOCATOR_SIZEOF(forward);
  grub_relocator_backward_size = RELOCATOR_SIZEOF(backward);
}

static void
write_reg (int regn, grub_uint64_t val, void **target)
{
  grub_uint32_t lui;
  grub_uint32_t ori;
  grub_uint32_t dsll;

  /* lui $r, 0 */
  lui = (0x3c00 | regn) << 16;
  /* ori $r, $r, 0 */
  ori = (0x3400 | (regn << 5) | regn) << 16;
  /* dsll $r, $r, 16 */
  dsll = (regn << 16) | (regn << 11) | (16 << 6) | 56;

  /* lui $r, val[63:48].  */
  *(grub_uint32_t *) *target = lui | (grub_uint16_t) (val >> 48);
  *target = ((grub_uint32_t *) *target) + 1;
  /* ori $r, val[47:32].  */
  *(grub_uint32_t *) *target = ori | (grub_uint16_t) (val >> 32);
  *target = ((grub_uint32_t *) *target) + 1;
  /* dsll $r, $r, 16 */
  *(grub_uint32_t *) *target = dsll;
  *target = ((grub_uint32_t *) *target) + 1;
  /* ori $r, val[31:16].  */
  *(grub_uint32_t *) *target = ori | (grub_uint16_t) (val >> 16);
  *target = ((grub_uint32_t *) *target) + 1;
  /* dsll $r, $r, 16 */
  *(grub_uint32_t *) *target = dsll;
  *target = ((grub_uint32_t *) *target) + 1;
  /* ori $r, val[15:0].  */
  *(grub_uint32_t *) *target = ori | (grub_uint16_t) val;
  *target = ((grub_uint32_t *) *target) + 1;
}

static void
write_jump (int regn, void **target)
{
  /* j $r.  */
  *(grub_uint32_t *) *target = (regn << 21) | 0x8;
  *target = ((grub_uint32_t *) *target) + 1;
  /* nop.  */
  *(grub_uint32_t *) *target = 0;
  *target = ((grub_uint32_t *) *target) + 1;
}

void
grub_cpu_relocator_jumper (void *rels, grub_addr_t addr)
{
  write_reg (1, addr, &rels);
  write_jump (1, &rels);
}

void
grub_cpu_relocator_backward (void *ptr0, void *src, void *dest,
			     grub_size_t size)
{
  void *ptr = ptr0;
  write_reg (8, (grub_uint64_t) src, &ptr);
  write_reg (9, (grub_uint64_t) dest, &ptr);
  write_reg (10, (grub_uint64_t) size, &ptr);
  grub_memcpy (ptr, &grub_relocator_backward_start,
	       RELOCATOR_SRC_SIZEOF (backward));
}

void
grub_cpu_relocator_forward (void *ptr0, void *src, void *dest,
			     grub_size_t size)
{
  void *ptr = ptr0;
  write_reg (8, (grub_uint64_t) src, &ptr);
  write_reg (9, (grub_uint64_t) dest, &ptr);
  write_reg (10, (grub_uint64_t) size, &ptr);
  grub_memcpy (ptr, &grub_relocator_forward_start,
	       RELOCATOR_SRC_SIZEOF (forward));
}

grub_err_t
grub_relocator64_boot (struct grub_relocator *rel,
		       struct grub_relocator64_state state)
{
  grub_relocator_chunk_t ch;
  void *ptr;
  grub_err_t err;
  void *relst;
  grub_size_t relsize;
  grub_size_t stateset_size = 31 * REGW_SIZEOF + JUMP_SIZEOF;
  unsigned i;
  grub_addr_t vtarget;

  err = grub_relocator_alloc_chunk_align (rel, &ch, 0,
					  (0xffffffff - stateset_size)
					  + 1, stateset_size,
					  grub_relocator_align,
					  GRUB_RELOCATOR_PREFERENCE_NONE, 0);
  if (err)
    return err;

  ptr = get_virtual_current_address (ch);
  for (i = 1; i < 32; i++)
    write_reg (i, state.gpr[i], &ptr);
  write_jump (state.jumpreg, &ptr);

  vtarget = (grub_addr_t) grub_map_memory (get_physical_target_address (ch),
					   stateset_size);

  err = grub_relocator_prepare_relocs (rel, vtarget, &relst, &relsize);
  if (err)
    return err;

  grub_arch_sync_caches ((void *) relst, relsize);

  ((void (*) (void)) relst) ();

  /* Not reached.  */
  return GRUB_ERR_NONE;
}
