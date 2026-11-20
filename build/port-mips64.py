#!/usr/bin/env python3
"""Carry Ventoy's mips64el-efi port onto the GRUB 2.14 work tree.

Upstream GRUB never had this platform. The sources under grub-core/*/mips64
and include/grub/mips64 come from Ventoy's GRUB 2.04 overlay. The build
description (configure, gentpl, image tools) is applied onto 2.14 the same
way Ventoy applied it onto 2.04, including LoongArch which 2.14 already has.
"""

import os
import re
import shutil

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OVERLAY = os.path.join(ROOT, "GRUB2", "MOD_SRC", "grub-2.04")
WORK = os.path.join(ROOT, "build", "work", "grub")

COPY_DIRS = [
    "grub-core/kern/mips64",
    "grub-core/lib/mips64",
    "grub-core/loader/mips64",
    "include/grub/mips64",
]

MIPS_RELOCATE = r'''	     case EM_MIPS:
	       {
		 sym_addr += addend;
		 switch (ELF_R_TYPE (info))
		   {
		   case R_MIPS_NONE:
		     break;
		   case R_MIPS_64:
		     *target = grub_host_to_target64 (grub_target_to_host64 (*target) + sym_addr);
		     break;
		   case R_MIPS_32:
		     {
		       grub_uint32_t *t32 = (grub_uint32_t *) target;
		       *t32 = grub_host_to_target32 (grub_target_to_host32 (*t32) + sym_addr);
		     }
		     break;
		   case R_MIPS_26:
		     {
		       grub_uint32_t *t32 = (grub_uint32_t *) target;
		       grub_addr_t addr = sym_addr;
		       *t32 = ((*t32) & 0xfc000000U) | ((addr >> 2) & 0x3ffffffUL);
		     }
		     break;
		   case R_MIPS_LO16:
		     {
		       grub_int16_t *t16 = (grub_int16_t *) target;
		       *t16 = (grub_int16_t) sym_addr;
		     }
		     break;
		   case R_MIPS_HI16:
		     {
		       grub_int16_t *t16 = (grub_int16_t *) target;
		       *t16 = (grub_int16_t) ((sym_addr + 0x8000UL) >> 16);
		     }
		     break;
		   case R_MIPS_HIGHER:
		     {
		       grub_int16_t *t16 = (grub_int16_t *) target;
		       *t16 = (grub_int16_t) ((sym_addr + 0x80008000UL) >> 32);
		     }
		     break;
		   case R_MIPS_HIGHEST:
		     {
		       grub_uint16_t *t16 = (grub_uint16_t *) target;
		       *t16 = (grub_uint16_t) ((sym_addr + 0x800080008000UL) >> 48);
		     }
		     break;
		   default:
		     grub_util_error (_("relocation 0x%x is not implemented yet"),
				      (unsigned int) ELF_R_TYPE (info));
		     break;
		   }
	       break;
	       }
'''

MIPS_FIXUP = r'''#if defined(MKIMAGE_ELF64)
    case EM_MIPS:
      switch (ELF_R_TYPE (info))
	{
	case R_MIPS_64:
	  ctx->current_address
	    = add_fixup_entry (&ctx->lst, GRUB_PE32_REL_BASED_DIR64,
			       addr, 0, ctx->current_address, image_target);
	  break;
	case R_MIPS_32:
	  break;
	case R_MIPS_26:
	  ctx->current_address
	    = add_fixup_entry (&ctx->lst, GRUB_PE32_REL_BASED_MIPS_JMPADDR,
			       addr, 0, ctx->current_address, image_target);
	  break;
	case R_MIPS_LO16:
	  {
	    Elf_Addr target_addr = sym_addr;
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, GRUB_PE32_REL_BASED_MIPS_LOW,
				 addr, 0, ctx->current_address, image_target);
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_int16_t) ((target_addr & 0x8000UL) >> 16),
				 0, ctx->current_address, image_target);
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_int16_t) ((target_addr & 0x80008000UL) >> 32),
				 0, ctx->current_address, image_target);
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_uint16_t) ((target_addr & 0x800080008000UL) >> 48),
				 0, ctx->current_address, image_target);
	  }
	  break;
	case R_MIPS_HI16:
	  {
	    Elf_Addr target_addr = sym_addr;
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, GRUB_PE32_REL_BASED_MIPS_HIGH,
				 addr, 0, ctx->current_address, image_target);
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0, (grub_int16_t) target_addr,
				 0, ctx->current_address, image_target);
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_int16_t) ((target_addr & 0x80008000UL) >> 32),
				 0, ctx->current_address, image_target);
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_uint16_t) ((target_addr & 0x800080008000UL) >> 48),
				 0, ctx->current_address, image_target);
	  }
	  break;
	case R_MIPS_HIGHER:
	  {
	    Elf_Addr target_addr = sym_addr;
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, GRUB_PE32_REL_BASED_MIPS_HIGHER,
				 addr, 0, ctx->current_address, image_target);
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0, (grub_int16_t) target_addr,
				 0, ctx->current_address, image_target);
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_int16_t) ((target_addr & 0x8000UL) >> 16),
				 0, ctx->current_address, image_target);
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_uint16_t) ((target_addr & 0x800080008000UL) >> 48),
				 0, ctx->current_address, image_target);
	  }
	  break;
	case R_MIPS_HIGHEST:
	  {
	    Elf_Addr target_addr = sym_addr;
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, GRUB_PE32_REL_BASED_MIPS_HIGHEST,
				 addr, 0, ctx->current_address, image_target);
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0, (grub_int16_t) target_addr,
				 0, ctx->current_address, image_target);
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_int16_t) ((target_addr & 0x8000UL) >> 16),
				 0, ctx->current_address, image_target);
	    ctx->current_address
	      = add_fixup_entry (&ctx->lst, 0,
				 (grub_int16_t) ((target_addr & 0x80008000UL) >> 32),
				 0, ctx->current_address, image_target);
	  }
	  break;
	default:
	  grub_util_error (_("relocation 0x%x is not implemented yet"),
			   (unsigned int) ELF_R_TYPE (info));
	  break;
	}
      break;
#endif
'''


def read(path):
    with open(path, "r", errors="replace") as fh:
        return fh.read()


def write(path, text):
    with open(path, "w") as fh:
        fh.write(text)


def must_replace(text, old, new, label, once=True):
    if new in text:
        return text
    if old not in text:
        raise SystemExit("port-mips64: anchor missing in %s" % label)
    count = 1 if once else text.count(old)
    return text.replace(old, new, count)


def copy_sources():
    for rel in COPY_DIRS:
        src = os.path.join(OVERLAY, rel)
        dst = os.path.join(WORK, rel)
        if os.path.isdir(dst):
            shutil.rmtree(dst)
        shutil.copytree(src, dst)


def patch_copied_linux():
    """Adapt the copied 2.04 sources to GRUB 2.14 call conventions."""
    rel = "grub-core/loader/mips64/linux.c"
    path = os.path.join(WORK, rel)
    text = read(path)
    old = "grub_initrd_load (&initrd_ctx, argv, initrd_dest)"
    new = "grub_initrd_load (&initrd_ctx, initrd_dest)"
    if old in text:
        write(path, text.replace(old, new, 1))

    path = os.path.join(WORK, "grub-core/kern/mips64/dl.c")
    text = read(path)
    old = """N_("relocation 0x%x is not implemented yet"),
			       ELF_R_TYPE (r_info));"""
    new = """N_("relocation 0x%x is not implemented yet"),
			       (unsigned int) ELF_R_TYPE (r_info));"""
    if old in text:
        write(path, text.replace(old, new, 1))

    path = os.path.join(WORK, "grub-core/lib/mips64/relocator.c")
    text = read(path)
    old = "grub_size_t grub_relocator_jumper_size = JUMP_SIZEOF + REGW_SIZEOF;\n"
    new = old + """grub_size_t grub_relocator_preamble_size = 0;

void
grub_cpu_relocator_preamble (void *rels __attribute__ ((unused)))
{
}

"""
    if "grub_relocator_preamble_size" not in text and old in text:
        write(path, text.replace(old, new, 1))

    # 2.14 calls EFI boot services directly. efi_call_N was a 2.04 thunk.
    for root, _dirs, files in os.walk(WORK):
        if "/mips64" not in root.replace("\\", "/"):
            continue
        for name in files:
            if not name.endswith(".c"):
                continue
            path = os.path.join(root, name)
            text = read(path)
            updated = re.sub(r"efi_call_\d+\s*\(\s*([^,\s]+)\s*,", r"\1 (", text)
            if updated != text:
                write(path, updated)


def patch_configure_text(text, label):
    text = must_replace(
        text,
        """  mipsel|mips64el)
                target_cpu=mipsel
		machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_CPU_MIPSEL=1"
		;;
""",
        """  mips64el)
                target_cpu=mips64el
		machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_CPU_MIPS64EL=1"
		;;
  mipsel)
                target_cpu=mipsel
		machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_CPU_MIPSEL=1"
		;;
""",
        label + " cpu",
    )
    text = must_replace(
        text,
        "    mipsel-*) platform=loongson ;;\n",
        "    mips64el-*) platform=efi;;\n    mipsel-*) platform=loongson ;;\n",
        label + " guess",
    )
    text = must_replace(
        text,
        "  mipsel-loongson) ;;\n",
        "  mipsel-loongson) ;;\n  mips64el-efi) ;;\n",
        label + " platform",
    )
    text = must_replace(
        text,
        'if test x"$platform" != xemu ; then\n',
        'if test x"$platform" != xemu && test x"$target_cpu" != xmips64el ; then\n',
        label + " soft-float",
    )
    text = must_replace(
        text,
        "if ( test x$target_cpu = xmips || test x$target_cpu = xmipsel || test x$target_cpu = xarm64 ) && test \"x$grub_cv_cc_target_clang\" = xyes ; then\n",
        "if ( test x$target_cpu = xmips || test x$target_cpu = xmipsel || test x$target_cpu = xarm64 || test x$target_cpu = xmips64el ) && test \"x$grub_cv_cc_target_clang\" = xyes ; then\n",
        label + " pic",
    )
    abi = """if test x$target_cpu = xmips64el ; then
   TARGET_CFLAGS="$TARGET_CFLAGS -mabi=64 -mno-abicalls"
   TARGET_CCASFLAGS="$TARGET_CCASFLAGS -mabi=64 -mno-abicalls"
fi
"""
    if "-mno-abicalls" not in text:
        needle = """elif [ x"$pic_possible" = xyes ]; then
   TARGET_CFLAGS="$TARGET_CFLAGS -fno-PIC"
fi
"""
        needle_m4 = needle.replace("fi\n", "fi]\n", 1)
        if needle_m4 in text:
            text = text.replace(needle_m4, needle_m4 + abi, 1)
        else:
            text = must_replace(text, needle, needle + abi, label + " abi")
    text = must_replace(
        text,
        "  if test x${cpudir} = xmipsel; then\n    cpudir=mips;\n  fi\n",
        "  if test x${cpudir} = xmips64el; then\n    cpudir=mips64;\n  elif test x${cpudir} = xmipsel; then\n    cpudir=mips;\n  fi\n",
        label + " cpudir",
    )
    return text


def patch_configure_ac(text):
    text = patch_configure_text(text, "configure.ac")
    cond = """AM_CONDITIONAL([COND_mips64el], [test x$target_cpu = xmips64el])
AM_CONDITIONAL([COND_mips64_efi], [test x$target_cpu = xmips64el -a x$platform = xefi])
"""
    if "COND_mips64_efi" not in text:
        text = must_replace(
            text,
            "AM_CONDITIONAL([COND_loongarch64_efi], [test x$target_cpu = xloongarch64 -a x$platform = xefi])\n",
            "AM_CONDITIONAL([COND_loongarch64_efi], [test x$target_cpu = xloongarch64 -a x$platform = xefi])\n" + cond,
            "configure.ac conditional",
        )
    return text


def patch_configure_script(text):
    text = patch_configure_text(text, "configure")
    block = """
 if test x$target_cpu = xmips64el; then
  COND_mips64el_TRUE=
  COND_mips64el_FALSE='#'
else
  COND_mips64el_TRUE='#'
  COND_mips64el_FALSE=
fi

 if test x$target_cpu = xmips64el -a x$platform = xefi; then
  COND_mips64_efi_TRUE=
  COND_mips64_efi_FALSE='#'
else
  COND_mips64_efi_TRUE='#'
  COND_mips64_efi_FALSE=
fi
"""
    if "COND_mips64_efi_TRUE" not in text:
        text = must_replace(
            text,
            """ if test x$target_cpu = xloongarch64 -a x$platform = xefi; then
  COND_loongarch64_efi_TRUE=
  COND_loongarch64_efi_FALSE='#'
else
  COND_loongarch64_efi_TRUE='#'
  COND_loongarch64_efi_FALSE=
fi
""",
            """ if test x$target_cpu = xloongarch64 -a x$platform = xefi; then
  COND_loongarch64_efi_TRUE=
  COND_loongarch64_efi_FALSE='#'
else
  COND_loongarch64_efi_TRUE='#'
  COND_loongarch64_efi_FALSE=
fi
""" + block,
            "configure conditional",
        )
    return text


def patch_gentpl(text):
    text = must_replace(
        text,
        '"arm_coreboot", "loongarch64_efi", "riscv32_efi", "riscv64_efi" ]\n',
        '"arm_coreboot", "loongarch64_efi", "mips64_efi", "riscv32_efi", "riscv64_efi" ]\n',
        "gentpl platforms",
    )
    text = must_replace(
        text,
        'GROUPS["loongarch64"] = [ "loongarch64_efi" ]\n',
        'GROUPS["mips64"]      = [ "mips64_efi" ]\nGROUPS["loongarch64"] = [ "loongarch64_efi" ]\n',
        "gentpl group",
    )
    text = must_replace(
        text,
        '"loongarch64_efi", "riscv32_efi", "riscv64_efi" ]\n',
        '"loongarch64_efi", "mips64_efi", "riscv32_efi", "riscv64_efi" ]\n',
        "gentpl efi",
    )
    return text


def patch_makefile_def(text):
    text = must_replace(
        text,
        """  loongarch64_efi_cflags       = '-fshort-wchar';
  loongarch64_efi_ldflags      = '-Wl,-r';
  loongarch64_efi_stripflags   = '--strip-unneeded -K start -R .note -R .comment -R .note.gnu.gold-version -R .eh_frame';
""",
        """  loongarch64_efi_cflags       = '-fshort-wchar';
  loongarch64_efi_ldflags      = '-Wl,-r';
  loongarch64_efi_stripflags   = '--strip-unneeded -K start -R .note -R .comment -R .note.gnu.gold-version -R .eh_frame';

  mips64_efi_ldflags          = '-Wl,-r,-d';
  mips64_efi_stripflags       = '--strip-unneeded -K start -R .note -R .comment -R .note.gnu.gold-version -R .eh_frame -R .MIPS.abiflags';
""",
        "def ldflags",
    )
    text = must_replace(
        text,
        "  loongarch64_efi_startup = kern/loongarch64/efi/startup.S;\n",
        "  loongarch64_efi_startup = kern/loongarch64/efi/startup.S;\n  mips64_efi_startup = kern/mips64/efi/startup.S;\n",
        "def startup",
    )
    text = must_replace(
        text,
        """  loongarch64_efi = kern/loongarch64/efi/init.c;
  loongarch64_efi = kern/efi/fdt.c;
""",
        """  loongarch64_efi = kern/loongarch64/efi/init.c;
  loongarch64_efi = kern/efi/fdt.c;

  mips64 = kern/mips64/init.c;
  mips64 = kern/mips64/dl.c;
  mips64 = kern/mips64/cache.S;
  mips64 = kern/generic/rtc_get_time_ms.c;
  mips64_efi = kern/mips64/efi/init.c;
  mips64_efi = kern/mips64/efi/loongson.c;
  mips64_efi = lib/mips64/efi/loongson.c;
  mips64_efi = lib/mips64/efi/loongson_asm.S;
""",
        "def kernel",
    )
    text = must_replace(
        text,
        "  enable = mips_arc;\n  enable = ia64_efi;\n",
        "  enable = mips_arc;\n  enable = mips64_efi;\n  enable = ia64_efi;\n",
        "def boot",
    )
    text = must_replace(
        text,
        "  mips = lib/mips/relocator.c;\n",
        "  mips = lib/mips/relocator.c;\n  mips64 = lib/mips64/relocator_asm.S;\n  mips64 = lib/mips64/relocator.c;\n",
        "def relocator",
    )
    text = must_replace(
        text,
        "  enable = mips;\n  enable = powerpc;\n",
        "  enable = mips;\n  enable = mips64;\n  enable = powerpc;\n",
        "def relocator enable",
    )
    text = must_replace(
        text,
        "  extra_dist = lib/loongarch64/setjmp.S;\n",
        "  extra_dist = lib/loongarch64/setjmp.S;\n  extra_dist = lib/mips64/setjmp.S;\n",
        "def setjmp",
    )
    text = must_replace(
        text,
        "  mips = loader/mips/linux.c;\n",
        "  mips = loader/mips/linux.c;\n  mips64 = loader/mips64/linux.c;\n",
        "def linux",
    )
    text = must_replace(
        text,
        "  enable = mips;\n};\n\nmodule = {\n  name = normal;\n",
        "  enable = mips;\n  enable = mips64_efi;\n};\n\nmodule = {\n  name = normal;\n",
        "def mmap",
    )
    return text


def patch_makefile_am(text):
    block = """
if COND_mips64_efi
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/efi.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/efi/disk.h
KERNEL_HEADER_FILES += $(top_srcdir)/include/grub/acpi.h
KERNEL_HEADER_FILES += $(top_builddir)/include/grub/machine/loongson.h
endif
"""
    if "COND_mips64_efi" not in text:
        text = must_replace(
            text,
            "if COND_loongarch64_efi\n",
            block + "if COND_loongarch64_efi\n",
            "Makefile.am",
        )
    return text


def patch_setjmp(text):
    return must_replace(
        text,
        """#elif defined(__mips__)
#include "./mips/setjmp.S"
""",
        """#elif defined(__mips__)
#if _MIPS_SIM == _ABI64
#include "./mips64/setjmp.S"
#else
#include "./mips/setjmp.S"
#endif
""",
        "setjmp.S",
    )


def patch_install_h(text):
    return must_replace(
        text,
        "    GRUB_INSTALL_PLATFORM_RISCV64_EFI,\n    GRUB_INSTALL_PLATFORM_MAX\n",
        "    GRUB_INSTALL_PLATFORM_RISCV64_EFI,\n    GRUB_INSTALL_PLATFORM_MIPS64EL_EFI,\n    GRUB_INSTALL_PLATFORM_MAX\n",
        "install.h",
    )


def patch_pe32(text):
    text = must_replace(
        text,
        "#define GRUB_PE32_MACHINE_LOONGARCH64\t\t0x6264\n",
        "#define GRUB_PE32_MACHINE_LOONGARCH64\t\t0x6264\n#define GRUB_PE32_MACHINE_MIPS\t\t\t0x166\n",
        "pe32 machine",
    )
    extra = """#define GRUB_PE32_REL_BASED_MIPS_LOW		6
#define GRUB_PE32_REL_BASED_MIPS_HIGH		4
#define GRUB_PE32_REL_BASED_MIPS_HIGHER		7
#define GRUB_PE32_REL_BASED_MIPS_HIGHEST	8
"""
    if "GRUB_PE32_REL_BASED_MIPS_LOW" not in text:
        text = must_replace(
            text,
            "#define GRUB_PE32_REL_BASED_MIPS_JMPADDR\t5\n",
            "#define GRUB_PE32_REL_BASED_MIPS_JMPADDR\t5\n" + extra,
            "pe32 reloc",
        )
    return text


def add_fallthrough_cases(text):
    lines = text.splitlines(keepends=True)
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        out.append(line)
        if "GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI:" in line and i + 1 < len(lines):
            nxt = lines[i + 1]
            if "GRUB_INSTALL_PLATFORM_MIPS64EL_EFI" not in nxt and nxt.lstrip().startswith("case "):
                prefix = line.split("GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI:")[0]
                out.append(prefix + "GRUB_INSTALL_PLATFORM_MIPS64EL_EFI:\n")
        i += 1
    return "".join(out)


def patch_grub_install(text):
    text = must_replace(
        text,
        """#elif defined (__MIPSEL__)
   return "mipsel-loongson";
""",
        """#elif defined (__MIPSEL__)
#if _MIPS_SIM == _ABI64
   return "mips64el-efi";
#else
   return "mipsel-loongson";
#endif
""",
        "grub-install default",
    )
    text = add_fallthrough_cases(text)
    if "BOOTMIPS.EFI" not in text:
        text = must_replace(
        text,
        """	    case GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI:
	      efi_file = "BOOTLOONGARCH64.EFI";
	      break;
""",
        """	    case GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI:
	      efi_file = "BOOTLOONGARCH64.EFI";
	      break;
	    case GRUB_INSTALL_PLATFORM_MIPS64EL_EFI:
	      efi_file = "BOOTMIPS.EFI";
	      break;
""",
        "grub-install removable",
    )
    if "grubmips64el.efi" not in text:
        text = must_replace(
        text,
        """	    case GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI:
	      efi_file = "grubloongarch64.efi";
	      break;
""",
        """	    case GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI:
	      efi_file = "grubloongarch64.efi";
	      break;
	    case GRUB_INSTALL_PLATFORM_MIPS64EL_EFI:
	      efi_file = "grubmips64el.efi";
	      break;
""",
        "grub-install name",
    )
    return text


def patch_install_common(text):
    return must_replace(
        text,
        '[GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI] =  { "loongarch64", "efi"       },\n',
        '[GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI] =  { "loongarch64", "efi"       },\n'
        '    [GRUB_INSTALL_PLATFORM_MIPS64EL_EFI] =     { "mips64el",    "efi"       },\n',
        "install-common",
    )


def patch_mknetdir(text):
    return must_replace(
        text,
        '[GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI] = { "loongarch64-efi", "efinet", ".efi" },\n',
        '[GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI] = { "loongarch64-efi", "efinet", ".efi" },\n'
        '    [GRUB_INSTALL_PLATFORM_MIPS64EL_EFI] = { "mips64el-efi", "efinet", ".efi" },\n',
        "mknetdir",
    )


def patch_mkrescue(text):
    lines = text.splitlines(keepends=True)
    out = []
    for i, line in enumerate(lines):
        out.append(line)
        if "source_dirs[GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI]" in line:
            nxt = lines[i + 1] if i + 1 < len(lines) else ""
            if "MIPS64EL_EFI" not in nxt:
                indent = line[:len(line) - len(line.lstrip(" \t"))]
                out.append(indent + "|| source_dirs[GRUB_INSTALL_PLATFORM_MIPS64EL_EFI]\n")
    text = "".join(out)
    if "bootmips64el.efi" not in text:
        text = must_replace(
            text,
            """      imgname = grub_util_path_concat (2, efidir_efi_boot, "bootloongarch64.efi");
      make_image_fwdisk_abs (GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI,
			     "loongarch64-efi",
			     imgname);
      free (imgname);
""",
            """      imgname = grub_util_path_concat (2, efidir_efi_boot, "bootloongarch64.efi");
      make_image_fwdisk_abs (GRUB_INSTALL_PLATFORM_LOONGARCH64_EFI,
			     "loongarch64-efi",
			     imgname);
      free (imgname);

      imgname = grub_util_path_concat (2, efidir_efi_boot, "bootmips64el.efi");
      make_image_fwdisk_abs (GRUB_INSTALL_PLATFORM_MIPS64EL_EFI, "mips64el-efi",
			     imgname);
      free (imgname);
""",
            "mkrescue image",
        )
    return text


def patch_mkimage(text):
    block = """    {
      .dirname = "mips64el-efi",
      .names = { "mips64el-efi", NULL },
      .voidp_sizeof = 8,
      .bigendian = 0,
      .id = IMAGE_EFI,
      .flags = PLATFORM_FLAGS_NONE,
      .total_module_size = TARGET_NO_FIELD,
      .decompressor_compressed_size = TARGET_NO_FIELD,
      .decompressor_uncompressed_size = TARGET_NO_FIELD,
      .decompressor_uncompressed_addr = TARGET_NO_FIELD,
      .section_align = GRUB_PE32_SECTION_ALIGNMENT,
      .vaddr_offset = EFI64_HEADER_SIZE,
      .pe_target = GRUB_PE32_MACHINE_MIPS,
      .elf_target = EM_MIPS,
    },
"""
    if '.dirname = "mips64el-efi"' not in text:
        text = must_replace(
            text,
            """      .pe_target = GRUB_PE32_MACHINE_LOONGARCH64,
      .elf_target = EM_LOONGARCH,
    },
""",
            """      .pe_target = GRUB_PE32_MACHINE_LOONGARCH64,
      .elf_target = EM_LOONGARCH,
    },
""" + block,
            "mkimage",
        )
    return text


def patch_verifier(text):
    block = """  { "mips64el", 8, 0, EM_MIPS, GRUB_MODULE_VERIFY_SUPPORTS_REL | GRUB_MODULE_VERIFY_SUPPORTS_RELA, (int[]){
      R_MIPS_64,
      R_MIPS_32,
      R_MIPS_26,
      R_MIPS_LO16,
      R_MIPS_HI16,
      R_MIPS_HIGHER,
      R_MIPS_HIGHEST,
      -1
    }, (int[]){
      -1
    }
  },
"""
    if '"mips64el"' not in text:
        text = must_replace(
            text,
            '  { "loongarch64", 8, 0, EM_LOONGARCH,',
            block + '  { "loongarch64", 8, 0, EM_LOONGARCH,',
            "verifier",
        )
    return text


def patch_mkimagexx(text):
    old = "\t    info = grub_target_to_host (r->r_info);\n"
    new = (
        "\t    if (image_target->elf_target == EM_MIPS && image_target->voidp_sizeof == 8)\n"
        "\t      info = ((grub_uint64_t) r->r_info << 32) |\n"
        "\t\t      (grub_uint32_t) grub_be_to_cpu64 (r->r_info);\n"
        "\t    else\n"
        "\t      info = grub_target_to_host (r->r_info);\n"
    )
    if "elf_target == EM_MIPS && image_target->voidp_sizeof == 8" not in text:
        if text.count(old) < 1:
            raise SystemExit("port-mips64: r_info sites missing")
        text = text.replace(old, new)
    anchor = """\t\t break;
\t       }
#endif
#if defined(MKIMAGE_ELF32)
\t     case EM_ARM:
\t       {
\t\t sym_addr += addend;
\t\t sym_addr -= image_target->vaddr_offset;
"""
    if "case EM_MIPS:" not in text.split("add_fixup_entry", 1)[0]:
        if anchor not in text:
            raise SystemExit("port-mips64: relocate_addrs anchor missing")
        text = text.replace(anchor, "\t\t break;\n\t       }\n" + MIPS_RELOCATE + """#endif
#if defined(MKIMAGE_ELF32)
\t     case EM_ARM:
\t       {
\t\t sym_addr += addend;
\t\t sym_addr -= image_target->vaddr_offset;
""", 1)
    fix_anchor = """#endif /* defined(MKIMAGE_ELF64) */
      break;
#if defined(MKIMAGE_ELF32)
    case EM_ARM:
"""
    if text.count("case EM_MIPS:") < 2:
        if fix_anchor not in text:
            raise SystemExit("port-mips64: fixup anchor missing")
        text = text.replace(
            fix_anchor,
            """#endif /* defined(MKIMAGE_ELF64) */
      break;
""" + MIPS_FIXUP + """#if defined(MKIMAGE_ELF32)
    case EM_ARM:
""",
            1)
    text = text.replace(
        """static void
translate_relocation_pe (struct translate_context *ctx,
			 Elf_Addr addr,
			 Elf_Addr info,
			 const struct grub_install_image_target_desc *image_target)
""",
        """static void
translate_relocation_pe (struct translate_context *ctx,
			 Elf_Addr addr,
			 Elf_Addr info,
			 Elf_Addr sym_addr,
			 const struct grub_install_image_target_desc *image_target)
""",
    )
    text = text.replace(
        """static void
translate_relocation (struct translate_context *ctx,
		      Elf_Addr addr,
		      Elf_Addr info,
		      const struct grub_install_image_target_desc *image_target)
{
  if (image_target->id == IMAGE_EFI)
    translate_relocation_pe (ctx, addr, info, image_target);
""",
        """static void
translate_relocation (struct translate_context *ctx,
		      Elf_Addr addr,
		      Elf_Addr info,
		      Elf_Addr sym_addr,
		      const struct grub_install_image_target_desc *image_target)
{
  if (image_target->id == IMAGE_EFI)
    translate_relocation_pe (ctx, addr, info, sym_addr, image_target);
""",
    )
    old_call = """	    offset = grub_target_to_host (r->r_offset);
	    if (image_target->elf_target == EM_MIPS && image_target->voidp_sizeof == 8)
	      info = ((grub_uint64_t) r->r_info << 32) |
		      (grub_uint32_t) grub_be_to_cpu64 (r->r_info);
	    else
	      info = grub_target_to_host (r->r_info);

	    addr = section_address + offset;

	    translate_relocation (&ctx, addr, info, image_target);
"""
    new_call = """	    Elf_Addr sym_addr = 0;

	    offset = grub_target_to_host (r->r_offset);
	    if (image_target->elf_target == EM_MIPS && image_target->voidp_sizeof == 8)
	      info = ((grub_uint64_t) r->r_info << 32) |
		      (grub_uint32_t) grub_be_to_cpu64 (r->r_info);
	    else
	      info = grub_target_to_host (r->r_info);
	    if (image_target->elf_target == EM_MIPS && image_target->voidp_sizeof == 8)
	      {
		sym_addr = SUFFIX (get_symbol_address) (e, smd->symtab,
							ELF_R_SYM (info), image_target);
		if (grub_target_to_host32 (s->sh_type) == SHT_RELA)
		  sym_addr += grub_target_to_host (((Elf_Rela *) r)->r_addend);
	      }

	    addr = section_address + offset;

	    translate_relocation (&ctx, addr, info, sym_addr, image_target);
"""
    if old_call in text:
        text = text.replace(old_call, new_call, 1)
    text = text.replace(
        "      if (flush || addr < b->page_rva || b->page_rva + 0x1000 <= addr)\n",
        "      if (flush || (type && (addr < b->page_rva || b->page_rva + 0x1000 <= addr)))\n",
        1,
    )
    text = text.replace(
        "\t  b->page_rva = (addr & ~(0x1000 - 1));\n",
        "\t  if (type)\n\t    b->page_rva = (addr & ~(0x1000 - 1));\n",
        1,
    )
    text = text.replace(
        "      entry = GRUB_PE32_FIXUP_ENTRY (type, addr - b->page_rva);\n",
        "      entry = GRUB_PE32_FIXUP_ENTRY (type, type ? (addr - b->page_rva) : addr);\n",
        1,
    )
    return text


def patch_file(rel, fn):
    path = os.path.join(WORK, rel)
    write(path, fn(read(path)))


def main():
    if not os.path.isdir(WORK):
        raise SystemExit("missing GRUB work tree %s" % WORK)
    copy_sources()
    patch_copied_linux()
    patch_file("configure.ac", patch_configure_ac)
    patch_file("configure", patch_configure_script)
    patch_file("gentpl.py", patch_gentpl)
    patch_file("grub-core/Makefile.core.def", patch_makefile_def)
    patch_file("grub-core/Makefile.am", patch_makefile_am)
    patch_file("grub-core/lib/setjmp.S", patch_setjmp)
    patch_file("include/grub/util/install.h", patch_install_h)
    patch_file("include/grub/efi/pe32.h", patch_pe32)
    patch_file("util/grub-install.c", patch_grub_install)
    patch_file("util/grub-install-common.c", patch_install_common)
    patch_file("util/grub-mknetdir.c", patch_mknetdir)
    patch_file("util/grub-mkrescue.c", patch_mkrescue)
    patch_file("util/mkimage.c", patch_mkimage)
    patch_file("util/grub-module-verifier.c", patch_verifier)
    patch_file("util/grub-mkimagexx.c", patch_mkimagexx)
    print("mips64el-efi port applied")


if __name__ == "__main__":
    main()
