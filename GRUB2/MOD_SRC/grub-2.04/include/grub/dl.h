/* dl.h - types and prototypes for loadable module support */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2004,2005,2007,2008,2009  Free Software Foundation, Inc.
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

#ifndef GRUB_DL_H
#define GRUB_DL_H	1

#include <grub/symbol.h>
#ifndef ASM_FILE
#include <grub/err.h>
#include <grub/types.h>
#include <grub/elf.h>
#include <grub/list.h>
#include <grub/misc.h>
#endif

/*
 * Macros GRUB_MOD_INIT and GRUB_MOD_FINI are also used by build rules
 * to collect module names, so we define them only when they are not
 * defined already.
 */
#ifndef ASM_FILE

#ifndef GRUB_MOD_INIT

#if !defined (GRUB_UTIL) && !defined (GRUB_MACHINE_EMU) && !defined (GRUB_KERNEL)

#define GRUB_MOD_INIT(name)	\
static void grub_mod_init (grub_dl_t mod __attribute__ ((unused))) __attribute__ ((used)); \
static void \
grub_mod_init (grub_dl_t mod __attribute__ ((unused)))

#define GRUB_MOD_FINI(name)	\
static void grub_mod_fini (void) __attribute__ ((used)); \
static void \
grub_mod_fini (void)

#elif defined (GRUB_KERNEL)

#define GRUB_MOD_INIT(name)	\
static void grub_mod_init (grub_dl_t mod __attribute__ ((unused))) __attribute__ ((used)); \
void \
grub_##name##_init (void) { grub_mod_init (0); } \
static void \
grub_mod_init (grub_dl_t mod __attribute__ ((unused)))

#define GRUB_MOD_FINI(name)	\
static void grub_mod_fini (void) __attribute__ ((used)); \
void \
grub_##name##_fini (void) { grub_mod_fini (); } \
static void \
grub_mod_fini (void)

#else

#define GRUB_MOD_INIT(name)	\
static void grub_mod_init (grub_dl_t mod __attribute__ ((unused))) __attribute__ ((used)); \
void grub_##name##_init (void); \
void \
grub_##name##_init (void) { grub_mod_init (0); } \
static void \
grub_mod_init (grub_dl_t mod __attribute__ ((unused)))

#define GRUB_MOD_FINI(name)	\
static void grub_mod_fini (void) __attribute__ ((used)); \
void grub_##name##_fini (void); \
void \
grub_##name##_fini (void) { grub_mod_fini (); } \
static void \
grub_mod_fini (void)

#endif

#endif

#endif

#ifndef ASM_FILE
#ifdef __APPLE__
#define GRUB_MOD_SECTION(x) "_" #x ", _" #x ""
#else
#define GRUB_MOD_SECTION(x) "." #x
#endif
#else
#ifdef __APPLE__
#define GRUB_MOD_SECTION(x) _ ## x , _ ##x 
#else
#define GRUB_MOD_SECTION(x) . ## x
#endif
#endif

/* Me, Vladimir Serbinenko, hereby I add this module check as per new
   GNU module policy. Note that this license check is informative only.
   Modules have to be licensed under GPLv3 or GPLv3+ (optionally
   multi-licensed under other licences as well) independently of the
   presence of this check and solely by linking (module loading in GRUB
   constitutes linking) and GRUB core being licensed under GPLv3+.
   Be sure to understand your license obligations.
*/
#ifndef ASM_FILE
#if GNUC_PREREQ (3,2)
#define ATTRIBUTE_USED __used__
#else
#define ATTRIBUTE_USED __unused__
#endif
#define GRUB_MOD_LICENSE(license)	\
  static char grub_module_license[] __attribute__ ((section (GRUB_MOD_SECTION (module_license)), ATTRIBUTE_USED)) = "LICENSE=" license;
#define GRUB_MOD_DEP(name)	\
static const char grub_module_depend_##name[] \
 __attribute__((section(GRUB_MOD_SECTION(moddeps)), ATTRIBUTE_USED)) = #name
#define GRUB_MOD_NAME(name)	\
static const char grub_module_name_##name[] \
 __attribute__((section(GRUB_MOD_SECTION(modname)), __used__)) = #name
#else
#ifdef __APPLE__
.macro GRUB_MOD_LICENSE
  .section GRUB_MOD_SECTION(module_license)
  .ascii "LICENSE="
  .ascii $0
  .byte 0
.endm
#else
.macro GRUB_MOD_LICENSE license
  .section GRUB_MOD_SECTION(module_license), "a"
  .ascii "LICENSE="
  .ascii "\license"
  .byte 0
.endm
#endif
#endif

/* Under GPL license obligations you have to distribute your module
   under GPLv3(+). However, you can also distribute the same code under
   another license as long as GPLv3(+) version is provided.
*/
#define GRUB_MOD_DUAL_LICENSE(x)

#ifndef ASM_FILE

struct grub_dl_segment
{
  struct grub_dl_segment *next;
  void *addr;
  grub_size_t size;
  unsigned section;
};
typedef struct grub_dl_segment *grub_dl_segment_t;

struct grub_dl;

struct grub_dl_dep
{
  struct grub_dl_dep *next;
  struct grub_dl *mod;
};
typedef struct grub_dl_dep *grub_dl_dep_t;

#ifndef GRUB_UTIL
struct grub_dl
{
  char *name;
  int ref_count;
  int persistent;
  grub_dl_dep_t dep;
  grub_dl_segment_t segment;
  Elf_Sym *symtab;
  grub_size_t symsize;
  void (*init) (struct grub_dl *mod);
  void (*fini) (void);
#if !defined (__i386__) && !defined (__x86_64__)
  void *got;
  void *gotptr;
  void *tramp;
  void *trampptr;
#endif
#if defined(__mips__) && (_MIPS_SIM != _ABI64)
  grub_uint32_t *reginfo;
#endif
  void *base;
  grub_size_t sz;
  struct grub_dl *next;
};
#endif
typedef struct grub_dl *grub_dl_t;

grub_dl_t grub_dl_load_file (const char *filename);
grub_dl_t EXPORT_FUNC(grub_dl_load) (const char *name);
grub_dl_t grub_dl_load_core (void *addr, grub_size_t size);
grub_dl_t EXPORT_FUNC(grub_dl_load_core_noinit) (void *addr, grub_size_t size);
int EXPORT_FUNC(grub_dl_unload) (grub_dl_t mod);
void grub_dl_unload_unneeded (void);
int EXPORT_FUNC(grub_dl_ref) (grub_dl_t mod);
int EXPORT_FUNC(grub_dl_unref) (grub_dl_t mod);
extern grub_dl_t EXPORT_VAR(grub_dl_head);

#ifndef GRUB_UTIL

#define FOR_DL_MODULES(var) FOR_LIST_ELEMENTS ((var), (grub_dl_head))

#ifdef GRUB_MACHINE_EMU
void *
grub_osdep_dl_memalign (grub_size_t align, grub_size_t size);
void
grub_dl_osdep_dl_free (void *ptr);
#endif

static inline void
grub_dl_init (grub_dl_t mod)
{
  if (mod->init)
    (mod->init) (mod);

  mod->next = grub_dl_head;
  grub_dl_head = mod;
}

static inline grub_dl_t
grub_dl_get (const char *name)
{
  grub_dl_t l;

  FOR_DL_MODULES(l)
    if (grub_strcmp (name, l->name) == 0)
      return l;

  return 0;
}

static inline void
grub_dl_set_persistent (grub_dl_t mod)
{
  mod->persistent = 1;
}

static inline int
grub_dl_is_persistent (grub_dl_t mod)
{
  return mod->persistent;
}

#endif

grub_err_t grub_dl_register_symbol (const char *name, void *addr,
				    int isfunc, grub_dl_t mod);

grub_err_t grub_arch_dl_check_header (void *ehdr);
#ifndef GRUB_UTIL
grub_err_t
grub_arch_dl_relocate_symbols (grub_dl_t mod, void *ehdr,
			       Elf_Shdr *s, grub_dl_segment_t seg);
#endif

#if defined (__mips__) && (_MIPS_SIM != _ABI64)
#define GRUB_LINKER_HAVE_INIT 1
void grub_arch_dl_init_linker (void);
#endif

#define GRUB_IA64_DL_TRAMP_ALIGN 16
#define GRUB_IA64_DL_GOT_ALIGN 16

grub_err_t
grub_ia64_dl_get_tramp_got_size (const void *ehdr, grub_size_t *tramp,
				 grub_size_t *got);
grub_err_t
grub_arm64_dl_get_tramp_got_size (const void *ehdr, grub_size_t *tramp,
				  grub_size_t *got);

#if defined (__ia64__)
#define GRUB_ARCH_DL_TRAMP_ALIGN GRUB_IA64_DL_TRAMP_ALIGN
#define GRUB_ARCH_DL_GOT_ALIGN GRUB_IA64_DL_GOT_ALIGN
#define grub_arch_dl_get_tramp_got_size grub_ia64_dl_get_tramp_got_size
#elif defined (__aarch64__)
#define grub_arch_dl_get_tramp_got_size grub_arm64_dl_get_tramp_got_size
#else
grub_err_t
grub_arch_dl_get_tramp_got_size (const void *ehdr, grub_size_t *tramp,
				 grub_size_t *got);
#endif

#if defined (__powerpc__) || (defined (__mips__) && (_MIPS_SIM != _ABI64)) || defined (__arm__) || \
    (defined(__riscv) && (__riscv_xlen == 32))
#define GRUB_ARCH_DL_TRAMP_ALIGN 4
#define GRUB_ARCH_DL_GOT_ALIGN 4
#endif

#if defined (__aarch64__) || defined (__sparc__) || (defined (__mips__) && (_MIPS_SIM == _ABI64)) || \
    (defined(__riscv) && (__riscv_xlen == 64))
#define GRUB_ARCH_DL_TRAMP_ALIGN 8
#define GRUB_ARCH_DL_GOT_ALIGN 8
#endif

#endif

#endif /* ! GRUB_DL_H */
