#!/usr/bin/env python3
import os
import sys
import struct
import subprocess
import shutil

WORKSPACE = os.environ.get("VTOY_WORKSPACE", "/ventoy" if os.path.exists("/ventoy") else os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
IMAGE_PATH = os.path.join(WORKSPACE, "ventoy_usb.img")
iso_search_dirs = [
    os.environ.get("VTOY_ISO_DIR"),
    os.path.expanduser("~/Downloads/iso_test_set"),
    "/downloads/iso_test_set",
    "/tmp/iso_test_set",
    "/var/tmp/iso_test_set",
]
ISO_DIR = "/tmp/iso_test_set"
for d in iso_search_dirs:
    if d and os.path.isdir(d) and any(f.endswith(".iso") for f in os.listdir(d)):
        ISO_DIR = d
        break

PART1_IMG = "/tmp/part1_data.img"
PART2_IMG = "/tmp/part2_vtoyefi.img"

SECTOR_SIZE = 512
PART1_START = 2048
PART1_SIZE = 33554432      # 16 GB data partition
PART2_START = PART1_START + PART1_SIZE
PART2_SIZE = 65536         # 32 MB VTOYEFI partition
TOTAL_SECTORS = PART2_START + PART2_SIZE

BOOT_IMG_PATH = os.path.join(WORKSPACE, "INSTALL", "grub", "i386-pc", "boot.img")
CORE_IMG_PATH = os.path.join(WORKSPACE, "INSTALL", "grub", "i386-pc", "core.img")

def make_mbr():
    mbr = bytearray(512)

    if os.path.exists(BOOT_IMG_PATH):
        with open(BOOT_IMG_PATH, "rb") as f:
            boot_code = f.read(446)
            mbr[0:len(boot_code)] = boot_code

    struct.pack_into("<I", mbr, 0x1B8, 0x56544F59)
    struct.pack_into("<B3sB3sII", mbr, 0x1BE, 0x00, b'\x00\x02\x00', 0x07, b'\xff\xff\xff', PART1_START, PART1_SIZE)
    struct.pack_into("<B3sB3sII", mbr, 0x1CE, 0x80, b'\x00\x02\x00', 0xEF, b'\xff\xff\xff', PART2_START, PART2_SIZE)

    mbr[510] = 0x55
    mbr[511] = 0xAA

    return mbr

def build_vtoyefi_partition():
    if os.path.exists(PART2_IMG):
        os.remove(PART2_IMG)

    with open(PART2_IMG, "wb") as f:
        f.seek(PART2_SIZE * SECTOR_SIZE - 1)
        f.write(b'\x00')

    subprocess.run(["mkfs.vfat", "-F", "16", "-n", "VTOYEFI", PART2_IMG], check=True)
    subprocess.run(["mmd", "-i", PART2_IMG, "::/EFI", "::/EFI/BOOT", "::/ventoy", "::/grub", "::/grub/fonts", "::/grub/themes", "::/tool"], check=True)

    efi_boot = os.path.join(WORKSPACE, "INSTALL", "EFI", "BOOT")
    for fname in os.listdir(efi_boot):
        src = os.path.join(efi_boot, fname)
        if os.path.isfile(src):
            subprocess.run(["mcopy", "-o", "-i", PART2_IMG, src, "::/EFI/BOOT/"], check=True)

    grubx64_real = os.path.join(efi_boot, "grubx64_real.efi")
    if os.path.exists(grubx64_real):
        subprocess.run(["mcopy", "-o", "-i", PART2_IMG, grubx64_real, "::/EFI/BOOT/grubx64.efi"], check=True)

    vtoy_dir = os.path.join(WORKSPACE, "INSTALL", "ventoy")
    for fname in os.listdir(vtoy_dir):
        src = os.path.join(vtoy_dir, fname)
        if os.path.isfile(src):
            subprocess.run(["mcopy", "-o", "-i", PART2_IMG, src, "::/ventoy/"], check=True)

    grub_dir = os.path.join(WORKSPACE, "INSTALL", "grub")
    for fname in os.listdir(grub_dir):
        src = os.path.join(grub_dir, fname)
        if os.path.isfile(src) and fname.endswith(".cfg"):
            subprocess.run(["mcopy", "-o", "-i", PART2_IMG, src, "::/grub/"], check=True)

    fonts_dir = os.path.join(grub_dir, "fonts")
    if os.path.isdir(fonts_dir):
        for fname in os.listdir(fonts_dir):
            src = os.path.join(fonts_dir, fname)
            subprocess.run(["mcopy", "-o", "-i", PART2_IMG, src, "::/grub/fonts/"], check=True)

    themes_dir = os.path.join(grub_dir, "themes")
    if os.path.isdir(themes_dir):
        subprocess.run(["mmd", "-i", PART2_IMG, "::/grub/themes/ventoy"], check=True)
        vtoy_theme = os.path.join(themes_dir, "ventoy")
        if os.path.isdir(vtoy_theme):
            for fname in os.listdir(vtoy_theme):
                src = os.path.join(vtoy_theme, fname)
                subprocess.run(["mcopy", "-o", "-i", PART2_IMG, src, "::/grub/themes/ventoy/"], check=True)

    tar_tmp = "/tmp/vtoy_tar"
    shutil.rmtree(tar_tmp, ignore_errors=True)
    os.makedirs(tar_tmp, exist_ok=True)
    if os.path.exists(os.path.join(grub_dir, "menu.tar.gz")):
        subprocess.run(["mcopy", "-o", "-i", PART2_IMG, os.path.join(grub_dir, "menu.tar.gz"), "::/grub/"], check=True)
    elif os.path.isdir(os.path.join(grub_dir, "menu")):
        menu_tar = os.path.join(tar_tmp, "menu.tar.gz")
        subprocess.run(f"tar --mtime=@1700000000 --owner=0 --group=0 --numeric-owner --sort=name -cf - -C '{grub_dir}' menu | gzip -n > '{menu_tar}'", shell=True, check=True)
        subprocess.run(["mcopy", "-o", "-i", PART2_IMG, menu_tar, "::/grub/"], check=True)

    if os.path.exists(os.path.join(grub_dir, "help.tar.gz")):
        subprocess.run(["mcopy", "-o", "-i", PART2_IMG, os.path.join(grub_dir, "help.tar.gz"), "::/grub/"], check=True)
    elif os.path.isdir(os.path.join(grub_dir, "help")):
        help_tar = os.path.join(tar_tmp, "help.tar.gz")
        subprocess.run(f"tar --mtime=@1700000000 --owner=0 --group=0 --numeric-owner --sort=name -cf - -C '{grub_dir}' help | gzip -n > '{help_tar}'", shell=True, check=True)
        subprocess.run(["mcopy", "-o", "-i", PART2_IMG, help_tar, "::/grub/"], check=True)
    shutil.rmtree(tar_tmp, ignore_errors=True)

    tool_dir = os.path.join(WORKSPACE, "INSTALL", "tool")
    if os.path.isdir(tool_dir):
        for arch in ["i386", "x86_64", "aarch64"]:
            vcli = os.path.join(tool_dir, arch, "vtoycli")
            stub = f"/tmp/mount.exfat-fuse_{arch}"
            if os.path.exists(vcli):
                with open(vcli, "rb") as sf, open(stub, "wb") as df:
                    df.write(sf.read(16384))
            else:
                with open(stub, "wb") as df:
                    df.write(b'\x00' * 16384)
            subprocess.run(["mcopy", "-o", "-i", PART2_IMG, stub, "::/tool/"], check=True)
            os.remove(stub)

        cert = os.path.join(tool_dir, "ENROLL_THIS_KEY_IN_MOKMANAGER.cer")
        if os.path.exists(cert):
            subprocess.run(["mcopy", "-o", "-i", PART2_IMG, cert, "::/tool/"], check=True)

        script = os.path.join(tool_dir, "create_ventoy_iso_part_dm.sh")
        if os.path.exists(script):
            subprocess.run(["mcopy", "-o", "-i", PART2_IMG, script, "::/tool/"], check=True)

def build_data_partition():
    if os.path.exists(PART1_IMG):
        os.remove(PART1_IMG)

    with open(PART1_IMG, "wb") as f:
        f.seek(PART1_SIZE * SECTOR_SIZE - 1)
        f.write(b'\x00')

    subprocess.run(["mkfs.ntfs", "-F", "-f", "-L", "Ventoy", "-H", "255", "-S", "63", "-p", str(PART1_START), PART1_IMG], check=True)

    if os.path.isdir(ISO_DIR):
        for iso_file in sorted(os.listdir(ISO_DIR)):
            if iso_file.endswith(".iso"):
                full_path = os.path.join(ISO_DIR, iso_file)
                subprocess.run(["ntfscp", PART1_IMG, full_path, f"/{iso_file}"], check=True)

def assemble_final_disk():
    with open(IMAGE_PATH, "wb") as f:
        mbr = make_mbr()
        f.write(mbr)

        if os.path.exists(CORE_IMG_PATH):
            with open(CORE_IMG_PATH, "rb") as cf:
                f.write(cf.read())

        f.seek(TOTAL_SECTORS * SECTOR_SIZE - 1)
        f.write(b'\x00')

    subprocess.run(["dd", f"if={PART1_IMG}", f"of={IMAGE_PATH}", "bs=1M", "seek=1", "conv=notrunc", "status=none"], check=True)
    subprocess.run(["dd", f"if={PART2_IMG}", f"of={IMAGE_PATH}", f"bs={SECTOR_SIZE}", f"seek={PART2_START}", "conv=notrunc", "status=none"], check=True)

    # grub-bios-setup embeds core.img and writes the disk geometry blocklist into sector 0 for BIOS booting.
    grub_setup = os.path.join(WORKSPACE, "GRUB2", "INSTALL", "sbin", "grub-bios-setup")
    i386_dir = os.path.join(WORKSPACE, "INSTALL", "grub", "i386-pc")
    if os.path.exists(grub_setup) and os.path.isdir(i386_dir):
        try:
            subprocess.run([grub_setup, "--skip-fs-probe", f"--directory={i386_dir}", IMAGE_PATH], check=True)
        except Exception:
            try:
                # Fallback to hermetic container if host lacks required libdevmapper dependencies.
                subprocess.run(["docker", "run", "--privileged", "--rm", "-v", f"{WORKSPACE}:/ventoy", "ventoy-repro",
                                "/ventoy/GRUB2/INSTALL/sbin/grub-bios-setup", "--skip-fs-probe",
                                "--directory=/ventoy/INSTALL/grub/i386-pc", "/ventoy/ventoy_usb.img"], check=True)
            except Exception as e2:
                print(f"[GRUB-BIOS] Note: grub-bios-setup skipped or encountered: {e2}")

def main():
    build_vtoyefi_partition()
    build_data_partition()
    assemble_final_disk()

if __name__ == "__main__":
    main()
