#!/usr/bin/env python3
"""Export build/work/grub against the pinned GRUB tarball as a patch series.

Source edits and added files go in 0001. Files produced by autoconf/automake
go in 0002, because this tree cannot rerun autogen without autoconf-archive.
Build directories are not part of the series.
"""

import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
UPSTREAM = os.path.join(ROOT, ".cache", "upstream", "grub-2.14")
WORK = os.path.join(ROOT, "build", "work", "grub")
OUT = os.path.join(ROOT, "build", "patches", "grub")

SKIP_DIRS = {
    "build-x86_64-efi",
    "build-loongarch64-efi",
    "build-mips64el-efi",
    "autom4te.cache",
    ".deps",
    ".deps-core",
    "__pycache__",
}
GENERATED = {
    "configure",
    "Makefile.in",
    "Makefile.core.am",
    "Makefile.util.am",
    "config.h.in",
    "config-util.h.in",
    "aclocal.m4",
}


def rel_files(root):
    found = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS and not d.endswith("~")]
        for name in filenames:
            if name.endswith(("~", ".o", ".pyc", ".pyo")):
                continue
            path = os.path.join(dirpath, name)
            found.append(os.path.relpath(path, root))
    return set(found)


def rewrite(diff, rel, new):
    rel_b = rel.encode()
    lines = diff.splitlines(keepends=True)
    out = []
    for line in lines:
        if line.startswith(b"--- "):
            out.append(b"--- /dev/null\n" if new else b"--- a/" + rel_b + b"\n")
        elif line.startswith(b"+++ "):
            out.append(b"+++ b/" + rel_b + b"\n")
        elif line.startswith(b"diff "):
            continue
        else:
            out.append(line)
    if out and not out[-1].endswith(b"\n"):
        out[-1] += b"\n"
    return b"".join(out)


def one_diff(rel):
    src = os.path.join(UPSTREAM, rel)
    dst = os.path.join(WORK, rel)
    new = not os.path.exists(src)
    if not os.path.exists(dst):
        proc = subprocess.run(
            ["diff", "-uN", src, "/dev/null"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        text = rewrite(proc.stdout, rel, False)
        return text.replace(b"+++ /dev/null\n", b"+++ b/" + rel.encode() + b"\n")
    proc = subprocess.run(
        ["diff", "-uN", "/dev/null" if new else src, dst],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if proc.returncode not in (0, 1):
        raise SystemExit(proc.stderr.decode() or "diff failed for %s" % rel)
    if proc.returncode == 0:
        return b""
    if b"\0" in proc.stdout[:8000]:
        raise SystemExit("binary file in grub diff: %s" % rel)
    return rewrite(proc.stdout, rel, new)


def main():
    if not os.path.isdir(UPSTREAM) or not os.path.isdir(WORK):
        raise SystemExit("need the grub 2.14 tarball tree and build/work/grub")
    os.makedirs(OUT, exist_ok=True)
    source, generated = [], []  # bytes hunks
    empty = []
    rels = sorted(rel_files(UPSTREAM) | rel_files(WORK))
    for rel in rels:
        dst = os.path.join(WORK, rel)
        src = os.path.join(UPSTREAM, rel)
        if os.path.isfile(dst) and not os.path.exists(src) and os.path.getsize(dst) == 0:
            empty.append(rel)
            print(rel, "(empty)")
            continue
        text = one_diff(rel)
        if not text:
            continue
        (generated if os.path.basename(rel) in GENERATED else source).append(text)
        print(rel)
    files = [
        ("0001-ventoy-grub.patch", source, "Ventoy module, hooks, and the mips64el port."),
        ("0002-grub-generated-build-files.patch", generated, "configure and automake output. Regenerated only when autogen can run."),
    ]
    series = []
    for name, hunks, comment in files:
        if not hunks:
            continue
        path = os.path.join(OUT, name)
        with open(path, "wb") as fh:
            fh.write(b"".join(hunks))
        series.append("%s  # %s" % (name, comment))
        print("wrote", name, "hunks", len(hunks))
    with open(os.path.join(OUT, "series"), "w", encoding="utf-8") as fh:
        fh.write("# Patches against GRUB %s\n" % "2.14")
        fh.write("\n".join(series) + "\n")
    with open(os.path.join(OUT, "new-empty"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(empty) + ("\n" if empty else ""))


if __name__ == "__main__":
    sys.exit(main())
