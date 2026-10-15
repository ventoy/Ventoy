#!/usr/bin/env python3
"""Check that an unsigned rebuilt PE/EFI image matches a committed (possibly signed) one.

Usage: compare_signed_pe.py REBUILT COMMITTED

The committed file may carry an appended Authenticode signature. It is cut off at
the Certificate Table offset, then the two images are compared byte for byte. The
only differences allowed are the two header fields that signing changes:
the optional-header CheckSum and the Certificate Table data-directory entry.
Exit status: 0 = match, 1 = mismatch, 2 = bad input.
"""
import struct
import sys


def layout(data):
    pe, = struct.unpack_from("<I", data, 0x3C)
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("not a PE image")
    opt = pe + 24
    magic, = struct.unpack_from("<H", data, opt)
    ddir = opt + (112 if magic == 0x20B else 96)
    cert = ddir + 4 * 8
    cert_off, cert_size = struct.unpack_from("<II", data, cert)
    nsec, = struct.unpack_from("<H", data, pe + 6)
    opt_size, = struct.unpack_from("<H", data, pe + 20)
    sections = []
    base = opt + opt_size
    for i in range(nsec):
        name, _vs, _va, rsize, rptr = struct.unpack_from("<8sIIII", data, base + 40 * i)
        sections.append((name.rstrip(b"\0").decode("latin1"), rptr, rsize))
    allowed = {**{o: "CheckSum" for o in range(opt + 64, opt + 68)},
               **{o: "CertificateTable dir" for o in range(cert, cert + 8)}}
    return sections, allowed, cert_off, cert_size


def compare(rebuilt_path, committed_path):
    rebuilt = open(rebuilt_path, "rb").read()
    committed = open(committed_path, "rb").read()
    _, _, r_off, r_size = layout(rebuilt)
    if r_off or r_size:
        raise ValueError("rebuilt image is already signed")
    sections, allowed, c_off, c_size = layout(committed)
    unsigned = committed[:c_off] if c_off else committed
    report = {"signature_bytes": c_size, "sections": {}, "header_diffs": [], "ok": True}
    if len(unsigned) != len(rebuilt):
        report["ok"] = False
        report["size"] = (len(rebuilt), len(unsigned))
        return report
    in_section = set()
    for name, ptr, size in sections:
        a, b = rebuilt[ptr:ptr + size], unsigned[ptr:ptr + size]
        n = sum(x != y for x, y in zip(a, b))
        report["sections"][name] = n
        in_section.update(range(ptr, ptr + size))
        if n:
            report["ok"] = False
    for off, (x, y) in enumerate(zip(rebuilt, unsigned)):
        if x != y and off not in in_section:
            field = allowed.get(off)
            report["header_diffs"].append((off, field))
            if field is None:
                report["ok"] = False
    return report


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    try:
        r = compare(sys.argv[1], sys.argv[2])
    except (OSError, ValueError, struct.error) as e:
        print(f"ERROR {e}")
        return 2
    secs = " ".join(f"{k}={v}" for k, v in r["sections"].items())
    fields = sorted({f or f"UNEXPECTED@{o}" for o, f in r["header_diffs"]})
    print(f"{'MATCH' if r['ok'] else 'MISMATCH'} {sys.argv[2]}")
    print(f"  signature={r['signature_bytes']}B  differing section bytes: {secs or '-'}")
    print(f"  header bytes differing: {len(r['header_diffs'])} ({', '.join(fields) or 'none'})")
    if "size" in r:
        print(f"  size mismatch: rebuilt={r['size'][0]} committed-unsigned={r['size'][1]}")
    return 0 if r["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
