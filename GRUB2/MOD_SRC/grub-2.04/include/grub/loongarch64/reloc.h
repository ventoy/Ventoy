/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2023 Free Software Foundation, Inc.
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

#ifndef GRUB_LOONGARCH64_RELOC_H
#define GRUB_LOONGARCH64_RELOC_H 1
#include <grub/types.h>

#define LOONGARCH64_STACK_MAX 16

struct grub_loongarch64_stack
{
  grub_uint64_t data[LOONGARCH64_STACK_MAX];
  int count;
  int top;
};

typedef struct grub_loongarch64_stack* grub_loongarch64_stack_t;

void grub_loongarch64_stack_init	     (grub_loongarch64_stack_t stack);
void grub_loongarch64_sop_push		     (grub_loongarch64_stack_t stack,
					      grub_int64_t offset);
void grub_loongarch64_sop_sub		     (grub_loongarch64_stack_t stack);
void grub_loongarch64_sop_sl		     (grub_loongarch64_stack_t stack);
void grub_loongarch64_sop_sr		     (grub_loongarch64_stack_t stack);
void grub_loongarch64_sop_add		     (grub_loongarch64_stack_t stack);
void grub_loongarch64_sop_and		     (grub_loongarch64_stack_t stack);
void grub_loongarch64_sop_if_else	     (grub_loongarch64_stack_t stack);
void grub_loongarch64_sop_32_s_10_5	     (grub_loongarch64_stack_t stack,
					      grub_uint64_t *place);
void grub_loongarch64_sop_32_u_10_12	     (grub_loongarch64_stack_t stack,
					      grub_uint64_t *place);
void grub_loongarch64_sop_32_s_10_12	     (grub_loongarch64_stack_t stack,
					      grub_uint64_t *place);
void grub_loongarch64_sop_32_s_10_16	     (grub_loongarch64_stack_t stack,
					      grub_uint64_t *place);
void grub_loongarch64_sop_32_s_10_16_s2	     (grub_loongarch64_stack_t stack,
					      grub_uint64_t *place);
void grub_loongarch64_sop_32_s_5_20	     (grub_loongarch64_stack_t stack,
					      grub_uint64_t *place);
void grub_loongarch64_sop_32_s_0_5_10_16_s2  (grub_loongarch64_stack_t stack,
					      grub_uint64_t *place);
void grub_loongarch64_sop_32_s_0_10_10_16_s2 (grub_loongarch64_stack_t stack,
					      grub_uint64_t *place);

#define GRUB_LOONGARCH64_RELOCATION(STACK, PLACE, OFFSET)	\
  case R_LARCH_SOP_PUSH_ABSOLUTE:				\
    grub_loongarch64_sop_push (STACK, OFFSET);			\
    break;							\
  case R_LARCH_SOP_SUB:						\
    grub_loongarch64_sop_sub (STACK);				\
    break;							\
  case R_LARCH_SOP_SL:						\
    grub_loongarch64_sop_sl (STACK);				\
    break;							\
  case R_LARCH_SOP_SR:						\
    grub_loongarch64_sop_sr (STACK);				\
    break;							\
  case R_LARCH_SOP_ADD:						\
    grub_loongarch64_sop_add (STACK);				\
    break;							\
  case R_LARCH_SOP_AND:						\
    grub_loongarch64_sop_and (STACK);				\
    break;							\
  case R_LARCH_SOP_IF_ELSE:					\
    grub_loongarch64_sop_if_else (STACK);			\
    break;							\
  case R_LARCH_SOP_POP_32_S_10_5:				\
    grub_loongarch64_sop_32_s_10_5 (STACK, PLACE);		\
    break;							\
  case R_LARCH_SOP_POP_32_U_10_12:				\
    grub_loongarch64_sop_32_u_10_12 (STACK, PLACE);		\
    break;							\
  case R_LARCH_SOP_POP_32_S_10_12:				\
    grub_loongarch64_sop_32_s_10_12 (STACK, PLACE);		\
    break;							\
  case R_LARCH_SOP_POP_32_S_10_16:				\
    grub_loongarch64_sop_32_s_10_16 (STACK, PLACE);		\
    break;							\
  case R_LARCH_SOP_POP_32_S_10_16_S2:				\
    grub_loongarch64_sop_32_s_10_16_s2 (STACK, PLACE);		\
    break;							\
  case R_LARCH_SOP_POP_32_S_5_20:				\
    grub_loongarch64_sop_32_s_5_20 (STACK, PLACE);		\
    break;							\
  case R_LARCH_SOP_POP_32_S_0_5_10_16_S2:			\
    grub_loongarch64_sop_32_s_0_5_10_16_s2 (STACK, PLACE);	\
    break;							\
  case R_LARCH_SOP_POP_32_S_0_10_10_16_S2:			\
    grub_loongarch64_sop_32_s_0_10_10_16_s2 (STACK, PLACE);	\
    break;

#endif /* GRUB_LOONGARCH64_RELOC_H */
