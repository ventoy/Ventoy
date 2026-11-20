#!/usr/bin/env python3
"""Rebase Ventoy's GRUB 2.04 overlay onto a stock GRUB release.

The historical tree copies whole files on top of grub-2.04. This script
3-way-merges those edits onto the pinned GRUB (see versions.env) and
keeps Ventoy's mips64el-efi port, which upstream never carried. LoongArch uses
the loongarch64-efi platform that GRUB 2.14 already builds.

Files that still conflict are kept as both sides of the hunk. The result
is a throwaway source tree under build/work, not a second copy of GRUB.
"""

import os
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OVERLAY = os.path.join(ROOT, "GRUB2", "MOD_SRC", "grub-2.04")
BASELINE = os.path.join(ROOT, ".cache", "upstream", "grub-2.04")
UPSTREAM = os.path.join(ROOT, ".cache", "upstream", "grub-2.14")
WORK = os.path.join(ROOT, "build", "work", "grub")

# MIPS64-EFI support and host image tools that only exist to carry it.
# GRUB 2.14 already installs loongarch64-efi without these.
SKIP = {
    "configure.ac",
    "gentpl.py",
    "install.sh",
    "grub-core/Makefile.core.def",
    "grub-core/Makefile.am",
    "grub-core/lib/setjmp.S",
    "grub-core/loader/arm64/linux.c",
    "grub-core/loader/i386/linux.c",
    "grub-core/loader/efi/linux.c",
    "include/grub/util/install.h",
    "util/grub-install.c",
    "util/grub-mkrescue.c",
    "util/grub-install-common.c",
    "util/mkimage.c",
    "util/grub-mkimagexx.c",
    "util/grub-module-verifier.c",
    "util/grub-mknetdir.c",
    # Ventoy split this only so its backported blscfg could share helpers.
    "grub-core/commands/loadenv.c",
}

# Additive sources. MIPS64, blscfg, setkey and mouse stay on the legacy tree
# until they are ported; they are not required to link ventoy.mod.
COPY_PREFIXES = (
    "grub-core/ventoy/",
    "include/grub/ventoy.h",
)


def keep_both(text):
    lines = text.splitlines(keepends=True)
    out = []
    i = 0
    while i < len(lines):
        if lines[i].startswith("<<<<<<<"):
            i += 1
            ours = []
            while i < len(lines) and not lines[i].startswith("======="):
                ours.append(lines[i])
                i += 1
            if i >= len(lines) or not lines[i].startswith("======="):
                raise SystemExit("broken conflict marker")
            i += 1
            theirs = []
            while i < len(lines) and not lines[i].startswith(">>>>>>>"):
                theirs.append(lines[i])
                i += 1
            if i >= len(lines):
                raise SystemExit("broken conflict marker")
            i += 1
            # A conflict with no Ventoy identifier is an upstream edit that
            # both sides touched (MIPS tweaks, includes, alignment). Keep the
            # GRUB 2.14 side. A Ventoy edit is kept, and the upstream side is
            # appended when it is not already contained in the Ventoy side.
            blob = "".join(ours + theirs).lower()
            if "ventoy" not in blob and "vtoy" not in blob:
                out.extend(theirs)
            else:
                out.extend(ours)
                theirs_text = "".join(theirs)
                if theirs_text.strip() and theirs_text not in "".join(ours):
                    out.extend(theirs)
        else:
            out.append(lines[i])
            i += 1
    return "".join(out)


def merge_tree():
    if not os.path.isdir(UPSTREAM):
        raise SystemExit("missing %s (download grub tarballs first)" % UPSTREAM)
    if os.path.isdir(WORK):
        shutil.rmtree(WORK)
    shutil.copytree(UPSTREAM, WORK, symlinks=True)

    merged = conflicted = copied = 0
    for dirpath, _, files in os.walk(OVERLAY):
        for name in files:
            rel = os.path.relpath(os.path.join(dirpath, name), OVERLAY)
            if rel in SKIP or rel.startswith("grub-core/kern/mips64/") or rel.startswith("grub-core/lib/mips64/") or rel.startswith("include/grub/mips64/") or rel.startswith("grub-core/loader/mips64/"):
                continue
            src_new = os.path.join(OVERLAY, rel)
            src_old = os.path.join(BASELINE, rel)
            dst = os.path.join(WORK, rel)
            if rel.startswith(COPY_PREFIXES) or not os.path.exists(src_old):
                if rel.startswith("grub-core/ventoy/") or rel == "include/grub/ventoy.h":
                    os.makedirs(os.path.dirname(dst), exist_ok=True)
                    shutil.copy(src_new, dst)
                    copied += 1
                continue
            if not os.path.exists(dst):
                continue
            tmp = dst + ".ventoy-merge"
            shutil.copy(src_new, tmp)
            subprocess.run(["git", "merge-file", tmp, src_old, dst], check=False)
            text = open(tmp, "r", errors="replace").read()
            if "<<<<<<<" in text:
                text = keep_both(text)
                conflicted += 1
            else:
                merged += 1
            open(dst, "w").write(text)
            os.remove(tmp)
    print("merged=%d keep_both=%d ventoy_files=%d" % (merged, conflicted, copied))


def patch_makefile():
    path = os.path.join(WORK, "grub-core", "Makefile.core.def")
    text = open(path).read()
    block = """
module = {
  name = ventoy;
  common = ventoy/ventoy.c;
  common = ventoy/ventoy_cmd.c;
  common = ventoy/ventoy_linux.c;
  common = ventoy/ventoy_unix.c;
  common = ventoy/ventoy_windows.c;
  common = ventoy/ventoy_vhd.c;
  common = ventoy/ventoy_plugin.c;
  common = ventoy/ventoy_json.c;
  common = ventoy/ventoy_browser.c;
  common = ventoy/lzx.c;
  common = ventoy/xpress.c;
  common = ventoy/huffman.c;
  common = ventoy/miniz.c;
};

"""
    needle = "module = {\n  name = hello;\n"
    if needle not in text:
        raise SystemExit("Makefile.core.def has no hello module anchor")
    if "name = ventoy;" not in text:
        text = text.replace(needle, block + needle, 1)
    old_cpp = "cppflags = '-I$(srcdir)/lib/posix_wrap -I$(srcdir)/lib/xzembed -I$(srcdir)/lib/minilzo -DMINILZO_HAVE_CONFIG_H';"
    new_cpp = "cppflags = '-I$(srcdir)/lib/posix_wrap -I$(srcdir)/lib/xzembed -I$(srcdir)/lib/minilzo -I$(srcdir)/lib/zstd -DMINILZO_HAVE_CONFIG_H';"
    if old_cpp not in text:
        raise SystemExit("Makefile.core.def: squash4 cppflags not found")
    text = text.replace(old_cpp, new_cpp, 1)
    open(path, "w").write(text)


def patch_efi_linux():
    path = os.path.join(WORK, "grub-core", "loader", "efi", "linux.c")
    text = open(path).read()
    if "ventoy_cmd_initrd" in text:
        return
    inc = open(os.path.join(ROOT, "build", "grub", "ventoy_efi_linux.inc")).read()

    if "#include <grub/env.h>" not in text:
        text = text.replace("#include <grub/err.h>\n",
                            "#include <grub/env.h>\n#include <grub/err.h>\n#include <grub/term.h>\n", 1)

    statics = """
#define LINUX_MAX_ARGC 1024
static int ventoy_debug = 0;
static int ventoy_initrd_called = 0;
static int ventoy_linux_argc = 0;
static char **ventoy_linux_args = NULL;
static int ventoy_extra_initrd_num = 0;
static char *ventoy_extra_initrd_list[256];
static int ventoy_preboot (void);
"""
    anchor = "static int loaded;\n"
    if anchor not in text:
        raise SystemExit("efi/linux.c: missing loaded anchor")
    text = text.replace(anchor, anchor + statics, 1)

    boot = "static grub_err_t\ngrub_linux_boot (void)\n{\n"
    if boot not in text:
        raise SystemExit("efi/linux.c: missing grub_linux_boot")
    text = text.replace(boot, boot + "  ventoy_preboot ();\n", 1)

    cmd = "static grub_err_t\ngrub_cmd_linux ("
    if cmd not in text:
        raise SystemExit("efi/linux.c: missing grub_cmd_linux")
    text = text.replace(cmd, inc + "\n" + cmd, 1)

    old_cmd = """  cmdline_size = grub_loader_cmdline_size (argc, argv) + sizeof (LINUX_IMAGE);
  linux_args = grub_malloc (cmdline_size);
  if (!linux_args)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
      goto fail;
    }
  grub_memcpy (linux_args, LINUX_IMAGE, sizeof (LINUX_IMAGE));
  err = grub_create_loader_cmdline (argc, argv,
				    linux_args + sizeof (LINUX_IMAGE) - 1,
				    cmdline_size,
				    GRUB_VERIFY_KERNEL_CMDLINE);"""
    new_cmd = """  if (ventoy_linux_argc)
    ventoy_bootopt_hook (argc, argv);
  if (ventoy_linux_argc)
    cmdline_size = grub_loader_cmdline_size (ventoy_linux_argc, ventoy_linux_args) + sizeof (LINUX_IMAGE);
  else
    cmdline_size = grub_loader_cmdline_size (argc, argv) + sizeof (LINUX_IMAGE);
  linux_args = grub_malloc (cmdline_size);
  if (!linux_args)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
      goto fail;
    }
  grub_memcpy (linux_args, LINUX_IMAGE, sizeof (LINUX_IMAGE));
  if (ventoy_linux_argc)
    err = grub_create_loader_cmdline (ventoy_linux_argc, ventoy_linux_args,
				    linux_args + sizeof (LINUX_IMAGE) - 1,
				    cmdline_size,
				    GRUB_VERIFY_KERNEL_CMDLINE);
  else
    err = grub_create_loader_cmdline (argc, argv,
				    linux_args + sizeof (LINUX_IMAGE) - 1,
				    cmdline_size,
				    GRUB_VERIFY_KERNEL_CMDLINE);"""
    if old_cmd not in text:
        raise SystemExit("efi/linux.c: cmdline block not found")
    text = text.replace(old_cmd, new_cmd, 1)

    text = text.replace(
        "static grub_command_t cmd_linux, cmd_initrd;\n",
        "static grub_command_t cmd_linux, cmd_initrd, cmd_linuxefi, cmd_initrdefi;\n"
        "static grub_command_t cmd_set_bootopt, cmd_unset_bootopt;\n"
        "static grub_command_t cmd_extra_initrd_append, cmd_extra_initrd_reset;\n",
        1)
    text = text.replace(
        'cmd_initrd = grub_register_command ("initrd", grub_cmd_initrd, 0,\n'
        '\t\t\t\t      N_("Load initrd."));\n'
        '  my_mod = mod;\n',
        'cmd_initrd = grub_register_command ("initrd", ventoy_cmd_initrd, 0,\n'
        '\t\t\t\t      N_("Load initrd."));\n'
        '  cmd_linuxefi = grub_register_command ("linuxefi", grub_cmd_linux, 0, N_("Load Linux."));\n'
        '  cmd_initrdefi = grub_register_command ("initrdefi", ventoy_cmd_initrd, 0, N_("Load initrd."));\n'
        '  cmd_set_bootopt = grub_register_command ("vt_set_boot_opt", grub_cmd_set_boot_opt, 0, N_("set ext boot opt"));\n'
        '  cmd_unset_bootopt = grub_register_command ("vt_unset_boot_opt", grub_cmd_unset_boot_opt, 0, N_("unset ext boot opt"));\n'
        '  cmd_extra_initrd_append = grub_register_command ("vt_img_extra_initrd_append", grub_cmd_extra_initrd_append, 0, N_(""));\n'
        '  cmd_extra_initrd_reset = grub_register_command ("vt_img_extra_initrd_reset", grub_cmd_extra_initrd_reset, 0, N_(""));\n'
        '  ventoy_linux_args = grub_zalloc (sizeof (char *) * LINUX_MAX_ARGC);\n'
        '  my_mod = mod;\n',
        1)
    if "ventoy_linux_args = grub_zalloc" not in text:
        raise SystemExit("efi/linux.c: failed to patch module init")
    open(path, "w").write(text)


def patch_host_gcc15():
    """GCC 15 treats strchr/strrchr of a const string as const char *."""
    path = os.path.join(WORK, "util", "probe.c")
    text = open(path).read()
    old = "  char *ptr;\n  canon = grub_canonicalize_file_name"
    new = "  const char *ptr;\n  canon = grub_canonicalize_file_name"
    if old in text:
        text = text.replace(old, new)
    open(path, "w").write(text)

    path = os.path.join(WORK, "util", "resolve.c")
    text = open(path).read()
    text = text.replace(
        "static char *\nget_module_name (const char *str)\n{\n  char *base;\n  char *ext;\n",
        "static char *\nget_module_name (const char *str)\n{\n  const char *base;\n  const char *ext;\n",
        1)
    text = text.replace(
        "  char *dir;\n  char *base;\n  char *ext;\n",
        "  const char *dir;\n  char *base;\n  const char *ext;\n",
        1)
    open(path, "w").write(text)

    path = os.path.join(WORK, "grub-core", "osdep", "linux", "ofpath.c")
    text = open(path).read()
    old = """  char *ed = strstr (sysfs_path, "end_device");
  char *p, *q, *path;
"""
    new = """  char *p, *q, *path, *ed;
"""
    if old in text:
        text = text.replace(old, new, 1)
        text = text.replace(
            """  if (!ed)
    return;

  /* SAS devices are identified using disk@$PHY_ID */
""",
            """  if (!strstr (sysfs_path, "end_device"))
    return;

  /* SAS devices are identified using disk@$PHY_ID */
""",
            1)
        open(path, "w").write(text)


def patch_menuentry():
    """The menu index out-parameter belonged to Ventoy's blscfg backport."""
    path = os.path.join(WORK, "grub-core", "commands", "menuentry.c")
    text = open(path).read()
    old = "  menu->size++;\n  if (index)\n    *index = ind;\n  return GRUB_ERR_NONE;\n"
    new = "  menu->size++;\n  return GRUB_ERR_NONE;\n"
    if old in text:
        open(path, "w").write(text.replace(old, new, 1))


def patch_test():
    path = os.path.join(WORK, "grub-core", "commands", "test.c")
    text = open(path).read()
    needle = 'GRUB_MOD_LICENSE ("GPLv3+");\n'
    insert = needle + "\nstatic int g_test_case_insensitive = 0;\n"
    if "g_test_case_insensitive" in text and "static int g_test_case_insensitive" not in text:
        if needle not in text:
            raise SystemExit("test.c: license anchor missing")
        text = text.replace(needle, insert, 1)
        open(path, "w").write(text)


def patch_xfs():
    """GRUB 2.14 provides grub_add/grub_mul in safemath.h."""
    path = os.path.join(WORK, "grub-core", "fs", "xfs.c")
    text = open(path).read()
    block = """// GRUB 2.04 doesn't have safemath.h
// #include <grub/safemath.h>

// gcc < 5.1 doesn't support __builtin_add_overflow and __builtin_mul_overflow
// #define grub_add(a, b, res) __builtin_add_overflow(a, b, res)
// #define grub_mul(a, b, res) __builtin_mul_overflow(a, b, res)
// Warning: This is unsafe!
#define grub_add(a, b, res) ({ *(res) = (a) + (b); 0; })

#define grub_mul(a, b, res) ({ *(res) = (a) * (b); 0; })

"""
    if block not in text:
        raise SystemExit("xfs.c: expected 2.04 safemath shim")
    open(path, "w").write(text.replace(block, "", 1))


def patch_loopback():
    """Ventoy's loopback -s/--skip must sit beside GRUB 2.14's decompress option."""
    path = os.path.join(WORK, "grub-core", "disk", "loopback.c")
    text = open(path).read()
    old = """  char *devname;
  grub_file_t file;
  struct grub_loopback *next;
  unsigned long id;
  grub_uint64_t refcnt;
};"""
    new = """  char *devname;
  grub_file_t file;
  struct grub_loopback *next;
  unsigned long id;
  grub_uint64_t refcnt;
  grub_off_t skip;
};"""
    if old not in text:
        raise SystemExit("loopback.c: struct layout changed")
    text = text.replace(old, new, 1)
    old = """    {"decompress", 'D', 0, N_("Transparently decompress backing file."), 0, 0},
    {0, 0, 0, 0, 0, 0}"""
    new = """    {"decompress", 'D', 0, N_("Transparently decompress backing file."), 0, 0},
    {"skip", 's', 0, "skip sectors of the file.", "SECTORS", ARG_TYPE_INT},
    {0, 0, 0, 0, 0, 0}"""
    if old not in text:
        raise SystemExit("loopback.c: option list changed")
    text = text.replace(old, new, 1)
    text = text.replace(
        "  if (state[1].set)\n      skip = (grub_off_t)grub_strtoull(state[1].arg, NULL, 10);",
        "  if (state[2].set)\n      skip = (grub_off_t)grub_strtoull(state[2].arg, NULL, 10);",
        1)
    open(path, "w").write(text)


def patch_efi_compat():
    """GRUB 2.14 renamed grub_efi_guid_t and changed grub_efi_get_variable."""
    header = os.path.join(WORK, "include", "grub", "efi", "efi.h")
    text = open(header).read()
    if "typedef grub_guid_t grub_efi_guid_t;" not in text:
        anchor = "#include <grub/efi/pe32.h>\n"
        if anchor not in text:
            raise SystemExit("efi.h: pe32 include missing")
        text = text.replace(anchor, anchor + "\ntypedef grub_guid_t grub_efi_guid_t;\n", 1)
    proto = (
        "void *EXPORT_FUNC (grub_efi_allocate_iso_buf) (grub_uint64_t size);\n"
        "void *EXPORT_FUNC (grub_efi_allocate_chain_buf) (grub_uint64_t size);\n"
        "void EXPORT_FUNC (grub_efi_get_reserved_page_num) (grub_uint64_t *total,\n"
        "\t\t\t\t\t\t   grub_uint64_t *org_required,\n"
        "\t\t\t\t\t\t   grub_uint64_t *new_required);\n"
    )
    if "grub_efi_allocate_iso_buf" not in text:
        anchor = "void **data_out);\n"
        if anchor not in text:
            raise SystemExit("efi.h: get_variable anchor missing")
        text = text.replace(anchor, anchor + proto, 1)
    open(header, "w").write(text)

    efi_c = os.path.join(WORK, "grub-core", "kern", "efi", "efi.c")
    body = open(efi_c).read()
    inc = "#include <grub/cpu/efi/memory.h>\n"
    if inc not in body:
        anchor = "#include <grub/efi/efi.h>\n"
        if anchor not in body:
            raise SystemExit("efi.c: efi.h include missing")
        body = body.replace(anchor, anchor + inc, 1)
        open(efi_c, "w").write(body)
        body = open(efi_c).read()
    if "grub_efi_allocate_iso_buf" not in body:
        body += """
static void *
ventoy_efi_alloc (grub_uint64_t size, grub_efi_memory_type_t memtype)
{
  grub_efi_uintn_t pages;

  if (size == 0)
    return NULL;
  pages = (grub_efi_uintn_t) ((size + GRUB_EFI_PAGE_SIZE - 1) / GRUB_EFI_PAGE_SIZE);
  return grub_efi_allocate_pages_real (GRUB_EFI_MAX_USABLE_ADDRESS, pages,
				       GRUB_EFI_ALLOCATE_MAX_ADDRESS, memtype);
}

void *
grub_efi_allocate_iso_buf (grub_uint64_t size)
{
  return ventoy_efi_alloc (size, GRUB_EFI_RUNTIME_SERVICES_DATA);
}

void *
grub_efi_allocate_chain_buf (grub_uint64_t size)
{
  return ventoy_efi_alloc (size, GRUB_EFI_LOADER_DATA);
}
"""
        open(efi_c, "w").write(body)

    mm_c = os.path.join(WORK, "grub-core", "kern", "efi", "mm.c")
    mm = open(mm_c).read()
    if "get_total_pages" in mm:
        start = mm.find("  /* By default, request a quarter of the available memory.  */")
        end = mm.find("  /* Sort the filtered descriptors", start)
        if start < 0 or end < 0:
            raise SystemExit("mm.c: heap block anchors missing")
        mm = mm[:start] + (
            "  g_org_required_pages = BYTES_TO_PAGES (required_bytes);\n"
            "  g_total_pages = g_org_required_pages;\n"
            "  g_new_required_pages = g_org_required_pages;\n\n"
        ) + mm[end:]
        open(mm_c, "w").write(mm)
    if "grub_efi_get_reserved_page_num" not in mm:
        mm += """
void
grub_efi_get_reserved_page_num (grub_uint64_t *total, grub_uint64_t *org_required,
				grub_uint64_t *new_required)
{
  *total = g_total_pages;
  *org_required = g_org_required_pages;
  *new_required = g_new_required_pages;
}
"""
        open(mm_c, "w").write(mm)

    ventoy = os.path.join(WORK, "grub-core", "ventoy", "ventoy.c")
    src = open(ventoy).read()
    src2 = src.replace(
        'var = grub_efi_get_variable("SecureBoot", &global, &size);\n'
        "    if (var && size == 1 && *var == 1)",
        'if (grub_efi_get_variable("SecureBoot", &global, &size, (void **) &var)\n'
        "        == GRUB_EFI_SUCCESS && var && size == 1 && *var == 1)",
    ).replace(
        'var = grub_efi_get_variable("SetupMode", &global, &size);\n'
        "    if (var && size == 1 && *var == 1)",
        'if (grub_efi_get_variable("SetupMode", &global, &size, (void **) &var)\n'
        "        == GRUB_EFI_SUCCESS && var && size == 1 && *var == 1)",
    )
    if src2 == src and 'grub_efi_get_variable("SecureBoot"' in src:
        raise SystemExit("ventoy.c: get_variable call site changed")
    open(ventoy, "w").write(src2)


def patch_loongarch_cache():
    path = os.path.join(WORK, "include", "grub", "cache.h")
    text = open(path).read()
    old = "#elif defined(__mips__) && (_MIPS_SIM != _ABI64)\n"
    new = "#elif defined (__loongarch__) || (defined(__mips__) && (_MIPS_SIM != _ABI64))\n"
    if old in text:
        open(path, "w").write(text.replace(old, new, 1))


def patch_ventoy_warnings():
    miniz = os.path.join(WORK, "grub-core", "ventoy", "miniz.c")
    text = open(miniz).read()
    old = '#pragma GCC diagnostic ignored "-Wundef"\n'
    new = old + '#pragma GCC diagnostic ignored "-Wmisleading-indentation"\n'
    if "-Wmisleading-indentation" not in text.split("tinfl_decompress", 1)[0]:
        if old not in text:
            raise SystemExit("miniz.c: undef pragma missing")
        text = text.replace(old, new, 1)
        open(miniz, "w").write(text)

    cmd = os.path.join(WORK, "grub-core", "ventoy", "ventoy_cmd.c")
    text = open(cmd).read()
    old = '"No such many images %ld %ld", img_id, g_ventoy_img_count'
    new = '"No such many images %ld %d", img_id, g_ventoy_img_count'
    if old in text:
        text = text.replace(old, new, 1)
        open(cmd, "w").write(text)

    js = os.path.join(WORK, "grub-core", "ventoy", "ventoy_json.c")
    text = open(js).read()
    old = "grub_strtoul(pcData, (char **)ppcEnd, 10)"
    new = "grub_strtoul(pcData, ppcEnd, 10)"
    if old in text:
        text = text.replace(old, new, 1)
        open(js, "w").write(text)

    linux = os.path.join(WORK, "grub-core", "ventoy", "ventoy_linux.c")
    text = open(linux).read()
    old = '"Can\'t alloc memory %llu\\n", file->size'
    new = '"Can\'t alloc memory %llu\\n", (unsigned long long) file->size'
    if old in text:
        text = text.replace(old, new, 1)
        open(linux, "w").write(text)


def patch_menu_files():
    menu = os.path.join(WORK, "grub-core", "normal", "menu.c")
    text = open(menu).read()
    dup = (
        "\t    case GRUB_TERM_CTRL | 'l':\n"
        "\t      menu_fini ();\n"
        "\t      goto refresh;\n\n"
    )
    if dup in text:
        text = text.replace(dup, "", 1)
    text = text.replace("      char *pos = NULL;\n", "      const char *pos = NULL;\n", 1)
    open(menu, "w").write(text)

    path = os.path.join(WORK, "grub-core", "normal", "menu_text.c")
    text = open(path).read()
    extra = """\t  if (nested)
\t    {
\t      ret += grub_print_message_indented_real
\t\t(_("Press enter to boot the selected OS, "
\t\t   "`e' to edit the commands before booting "
\t\t   "or `c' for a command-line. ESC to return previous menu."),
\t\t STANDARD_MARGIN, STANDARD_MARGIN, term, dry_run);
\t    }
\t  else
\t    {
\t      ret += grub_print_message_indented_real
\t\t(_("Press enter to boot the selected OS, "
\t\t   "`e' to edit the commands before booting "
\t\t   "or `c' for a command-line."),
\t\t STANDARD_MARGIN, STANDARD_MARGIN, term, dry_run);
\t    }
\t}
"""
    if extra in text:
        text = text.replace(extra, "", 1)
    needle = "  int ret = 0;\n  grub_term_setcolorstate (term, GRUB_TERM_COLOR_NORMAL);\n"
    repl = "  int ret = 0;\n  (void) nested;\n  grub_term_setcolorstate (term, GRUB_TERM_COLOR_NORMAL);\n"
    if needle in text:
        text = text.replace(needle, repl, 1)
    open(path, "w").write(text)


def patch_mips64_build():
    """MIPS64 n64 needs 8-byte module alignment, and unaligned suffix loads trap."""
    path = os.path.join(WORK, "include", "grub", "dl.h")
    text = open(path).read()
    needle = "    defined (__loongarch_lp64) || \\\n"
    insert = needle + "    (defined (__mips__) && (_MIPS_SIM == _ABI64)) || \\\n"
    if "_MIPS_SIM == _ABI64" not in text and needle in text:
        text = text.replace(needle, insert, 1)
        open(path, "w").write(text)

    path = os.path.join(WORK, "include", "grub", "serial.h")
    text = open(path).read()
    old = "#if (defined(__mips__) && _MIPS_SIM != _ABI64) || defined (__i386__) || defined (__x86_64__)\n#include <grub/cpu/io.h>\n"
    new = "#if defined(__mips__) || defined (__i386__) || defined (__x86_64__)\n#include <grub/cpu/io.h>\n"
    if old in text:
        text = text.replace(old, new, 1)
        open(path, "w").write(text)

    path = os.path.join(WORK, "grub-core", "term", "serial.c")
    text = open(path).read()
    old = "#if (defined(__mips__) || defined (__i386__) || defined (__x86_64__)) && !defined(GRUB_MACHINE_EMU) && !defined(GRUB_MACHINE_ARC)"
    new = "#if ((defined(__mips__) && _MIPS_SIM != _ABI64) || defined (__i386__) || defined (__x86_64__)) && !defined(GRUB_MACHINE_EMU) && !defined(GRUB_MACHINE_ARC)"
    if old in text:
        open(path, "w").write(text.replace(old, new, 1))

    path = os.path.join(WORK, "grub-core", "lib", "efi", "halt.c")
    text = open(path).read()
    old = "!defined(__loongarch__) && !defined(__riscv)"
    new = "!defined(__loongarch__) && !defined(__riscv) && !defined(__mips__)"
    if old in text and "!defined(__mips__)" not in text:
        open(path, "w").write(text.replace(old, new, 1))

    path = os.path.join(WORK, "grub-core", "kern", "file.c")
    text = open(path).read()
    old = "suffix = *(grub_uint32_t *)(name + len - 4);"
    new = "suffix = grub_get_unaligned32 (name + len - 4);"
    if old in text:
        open(path, "w").write(text.replace(old, new, 1))


def main():
    if os.environ.get("VENTOY_IMPORT_GRUB_OVERLAY") != "1":
        raise SystemExit(
            "prepare-grub.py deletes build/work/grub and reimports the GRUB 2.04 "
            "overlay. The build applies build/patches/grub instead. Set "
            "VENTOY_IMPORT_GRUB_OVERLAY=1 to regenerate the work tree, then "
            "run build/refresh-grub-patches.py."
        )
    merge_tree()
    patch_makefile()
    patch_efi_linux()
    patch_host_gcc15()
    patch_loopback()
    patch_xfs()
    patch_test()
    patch_menuentry()
    patch_efi_compat()
    patch_loongarch_cache()
    patch_ventoy_warnings()
    patch_menu_files()
    patch_mips64_build()
    subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), "port-mips64.py")], check=True)
    print("prepared", WORK)


if __name__ == "__main__":
    sys.exit(main())
