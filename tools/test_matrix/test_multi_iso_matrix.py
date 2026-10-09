#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import socket
import shutil
import zlib
import struct
from dataclasses import dataclass
from typing import Optional, List

WORKSPACE = os.environ.get("VTOY_WORKSPACE", "/ventoy" if os.path.exists("/ventoy") else os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
USB_IMG = os.path.join(WORKSPACE, "ventoy_usb.img")
OUTPUT_BASE = os.path.join(WORKSPACE, "qemu_test_results", "multi_os_matrix")
BOOTED_SCREENSHOTS_DIR = os.path.join(WORKSPACE, "qemu_test_results", "booted_screenshots")
SUMMARY_FILE = os.path.join(WORKSPACE, "qemu_test_results", "matrix_summary.md")

os.makedirs(OUTPUT_BASE, exist_ok=True)
os.makedirs(BOOTED_SCREENSHOTS_DIR, exist_ok=True)

@dataclass
class BootTestCase:
    test_id: str
    name: str
    iso_pattern: str
    is_uefi: bool
    wait_after_boot: int
    extra_enter_count: int = 1

def ppm_to_png(ppm_path, png_path):
    # Encodes raw P6 PPM bytes into PNG format without external PIL/cv2 dependencies.
    if not os.path.exists(ppm_path) or os.path.getsize(ppm_path) == 0:
        return False
    with open(ppm_path, "rb") as f:
        header = b""
        while True:
            line = f.readline()
            if not line:
                break
            if line.startswith(b"#"):
                continue
            header += line
            parts = header.split()
            if len(parts) >= 4 and parts[0] == b"P6":
                break
        width = int(parts[1])
        height = int(parts[2])
        raw_data = f.read()

    row_bytes = width * 3
    scanlines = bytearray()
    for y in range(height):
        scanlines.append(0)
        start = y * row_bytes
        end = start + row_bytes
        scanlines.extend(raw_data[start:end])

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        crc = zlib.crc32(tag + data) & 0xffffffff
        return c + struct.pack(">I", crc)

    png = bytearray(b"\x89PNG\r\n\x1a\n")
    ihdr_data = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    png.extend(chunk(b"IHDR", ihdr_data))
    compressed = zlib.compress(bytes(scanlines), 9)
    png.extend(chunk(b"IDAT", compressed))
    png.extend(chunk(b"IEND", b""))

    with open(png_path, "wb") as f:
        f.write(png)

    try:
        os.remove(ppm_path)
    except Exception:
        pass
    return True

def find_ovmf():
    candidates = [
        "/usr/share/ovmf/OVMF.fd",
        "/usr/share/OVMF/OVMF.fd",
        "/usr/share/OVMF/OVMF_CODE.fd",
        "/usr/share/qemu/OVMF.fd",
        "/usr/share/edk2-ovmf/x64/OVMF.fd",
        "/usr/share/edk2/x64/OVMF.fd",
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    return "/usr/share/ovmf/OVMF.fd"

def run_os_test(test: BootTestCase, nav_down_count: int):
    mode_str = "UEFI" if test.is_uefi else "BIOS"
    print("\n" + "=" * 75)
    print(f"RUNNING TEST [{test.test_id}]: {test.name} ({mode_str} Mode)")
    print("=" * 75)

    test_out_dir = os.path.join(OUTPUT_BASE, test.test_id)
    os.makedirs(test_out_dir, exist_ok=True)

    sock_path = f"/tmp/qemu_{test.test_id}_{os.getpid()}_{int(time.time())}.sock"
    if os.path.exists(sock_path):
        try:
            os.remove(sock_path)
        except Exception:
            pass

    has_kvm = os.path.exists("/dev/kvm") and os.access("/dev/kvm", os.R_OK | os.W_OK)

    cmd = [
        "qemu-system-x86_64",
        "-m", "4096",
        "-drive", f"file={USB_IMG},format=raw,index=0,media=disk,file.locking=off",
        "-monitor", f"unix:{sock_path},server,nowait",
        "-display", "none",
        "-vga", "virtio",
        "-no-reboot"
    ]

    if has_kvm:
        cmd.extend(["-machine", "q35,accel=kvm", "-smp", "4", "-cpu", "host"])
    else:
        cmd.extend(["-machine", "q35", "-smp", "2", "-cpu", "max"])

    if test.is_uefi:
        ovmf_path = find_ovmf()
        cmd.extend(["-bios", ovmf_path])

    print(f"Launching QEMU: {' '.join(cmd)}")
    proc = subprocess.Popen(cmd)

    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(5.0)
    connected = False
    start_time = time.time()
    connect_timeout = 25 if not has_kvm else 15

    while time.time() - start_time < connect_timeout:
        if os.path.exists(sock_path):
            try:
                s.connect(sock_path)
                connected = True
                break
            except (socket.error, ConnectionRefusedError):
                pass
        if proc.poll() is not None:
            print(f"  [ERROR] QEMU terminated prematurely with exit code {proc.returncode}")
            break
        time.sleep(0.5)

    if not connected:
        print(f"  [ERROR] Failed to connect to QEMU monitor socket at {sock_path} within {connect_timeout}s.")
        try:
            proc.terminate()
            proc.wait(timeout=5)
        except Exception:
            proc.kill()
        return {
            "id": test.test_id,
            "name": test.name,
            "mode": mode_str,
            "status": "FAILED",
            "screenshot": "N/A"
        }

    startup_wait = 10 if has_kvm else 18
    time.sleep(startup_wait)

    passed = False
    booted_img_path = None

    try:
        time.sleep(0.5)
        s.recv(1024)

        def capture(step_name):
            ppm_target = os.path.join(test_out_dir, f"{step_name}.ppm")
            png_target = os.path.join(test_out_dir, f"{step_name}.png")
            s.sendall(f"screendump {ppm_target}\n".encode())
            time.sleep(1.0)
            s.recv(1024)
            ppm_to_png(ppm_target, png_target)
            return png_target

        capture("1_ventoy_main_menu")

        for _ in range(nav_down_count):
            s.sendall(b"sendkey down\n")
            time.sleep(0.3)
            s.recv(1024)

        time.sleep(1.0)
        capture("2_iso_selected")

        s.sendall(b"sendkey ret\n")
        time.sleep(1.5)
        s.recv(1024)
        capture("3_ventoy_action_menu")

        s.sendall(b"sendkey ret\n")
        time.sleep(4.0)
        s.recv(1024)
        capture("4_os_boot_initial")

        for _ in range(test.extra_enter_count):
            s.sendall(b"sendkey ret\n")
            time.sleep(1.0)
            s.recv(1024)

        multiplier = float(os.environ.get("MATRIX_WAIT_MULTIPLIER", "1.0" if has_kvm else "2.0"))
        adjusted_wait = int(test.wait_after_boot * multiplier)
        print(f"  Waiting {adjusted_wait}s for {test.name} kernel & userspace initialization...")
        time.sleep(adjusted_wait)
        s.recv(1024)

        final_shot = capture("5_os_running_screen")

        booted_img_path = os.path.join(BOOTED_SCREENSHOTS_DIR, f"{test.test_id}_booted.png")
        if os.path.exists(final_shot):
            shutil.copy2(final_shot, booted_img_path)
            print(f"  [PUBLISHED BOOTED SCREENSHOT] -> {booted_img_path}")

        s.sendall(b"quit\n")
        s.close()
        passed = True
        print(f"  [SUCCESS] {test.name} ({mode_str}) test completed successfully!")
    except Exception as e:
        import traceback
        traceback.print_exc()
        print(f"  [ERROR] {test.name} test encountered error: {e}")
    finally:
        try:
            proc.terminate()
            proc.wait(timeout=5)
        except Exception:
            proc.kill()
            proc.wait()
        if os.path.exists(sock_path):
            try:
                os.remove(sock_path)
            except Exception:
                pass

    return {
        "id": test.test_id,
        "name": test.name,
        "mode": mode_str,
        "status": "PASSED" if passed else "FAILED",
        "screenshot": f"{test.test_id}_booted.png" if booted_img_path and os.path.exists(booted_img_path) else "N/A"
    }

def main():
    if not shutil.which("qemu-system-x86_64"):
        print("Error: qemu-system-x86_64 is not installed or not in PATH.")
        sys.exit(1)

    if not os.path.exists(USB_IMG):
        print(f"Error: Ventoy USB image not found at {USB_IMG}")
        sys.exit(1)

    filter_os = os.environ.get("MATRIX_FILTER_OS", "all").lower()
    filter_mode = os.environ.get("MATRIX_FILTER_MODE", "all").lower()

    iso_search_dirs = [
        os.environ.get("VTOY_ISO_DIR"),
        os.path.expanduser("~/Downloads/iso_test_set"),
        "/downloads/iso_test_set",
        "/tmp/iso_test_set",
        "/var/tmp/iso_test_set",
    ]
    iso_dir = "/tmp/iso_test_set"
    for d in iso_search_dirs:
        if d and os.path.isdir(d) and any(f.endswith(".iso") for f in os.listdir(d)):
            iso_dir = d
            break

    # Ventoy's GRUB menu sorts entries case-insensitively; test navigation must mirror this exact index order.
    available_isos = sorted([f for f in os.listdir(iso_dir) if f.endswith(".iso")], key=str.casefold) if os.path.isdir(iso_dir) else []
    print(f"Discovered ISOs in {iso_dir} (case-folded sort matching Ventoy menu): {available_isos}")

    def get_iso_index(pattern: str) -> Optional[int]:
        for idx, iso_name in enumerate(available_isos):
            if pattern.lower() in iso_name.lower():
                return idx
        return None

    all_tests = [
        BootTestCase("alpine_uefi", "Alpine Linux 3.20", "alpine", True, 15, 1),
        BootTestCase("alpine_bios", "Alpine Linux 3.20", "alpine", False, 15, 1),
        BootTestCase("arch_uefi", "Arch Linux", "arch", True, 20, 1),
        BootTestCase("arch_bios", "Arch Linux", "arch", False, 20, 1),
        BootTestCase("debian_uefi", "Debian GNU/Linux 12", "debian", True, 15, 1),
        BootTestCase("debian_bios", "Debian GNU/Linux 12", "debian", False, 15, 1),
        BootTestCase("fedora_uefi", "Fedora 39 Server", "fedora", True, 40, 1),
        BootTestCase("fedora_bios", "Fedora 39 Server", "fedora", False, 40, 1),
        BootTestCase("freebsd_uefi", "FreeBSD 13.2", "freebsd", True, 15, 1),
        BootTestCase("freebsd_bios", "FreeBSD 13.2", "freebsd", False, 15, 1),
        BootTestCase("ubuntu_uefi", "Ubuntu 24.04 Server", "ubuntu", True, 25, 1),
    ]

    selected_tests: List[BootTestCase] = []
    for test in all_tests:
        mode_str = "uefi" if test.is_uefi else "bios"

        if filter_mode != "all" and filter_mode != mode_str:
            continue
        if filter_os != "all" and filter_os not in test.test_id:
            continue
        selected_tests.append(test)

    print("=" * 75)
    print("STARTING VENTOY MULTI-OS TEST MATRIX SUITE")
    print(f"Total Test Cases Selected: {len(selected_tests)}")
    print("=" * 75)

    results = []
    for test in selected_tests:
        iso_idx = get_iso_index(test.iso_pattern)
        if iso_idx is None:
            mode_str = "UEFI" if test.is_uefi else "BIOS"
            print(f"\n[SKIPPED] {test.name} ({mode_str}): ISO matching '{test.iso_pattern}' not found in {iso_dir}.")
            results.append({
                "id": test.test_id,
                "name": test.name,
                "mode": mode_str,
                "status": "SKIPPED",
                "screenshot": "N/A"
            })
            continue

        res = run_os_test(test, iso_idx)
        results.append(res)

    print("\n" + "=" * 75)
    print("ALL TEST MATRIX RUNS COMPLETED!")
    print("=" * 75)

    summary_lines = [
        "## Ventoy Multi-OS QEMU Boot Matrix Test Summary",
        "",
        "| Test ID | Operating System | Boot Mode | Status | Booted Screenshot |",
        "| :--- | :--- | :---: | :---: | :--- |"
    ]

    for r in results:
        if r["status"] == "PASSED":
            status_badge = "**PASSED**"
        elif r["status"] == "SKIPPED":
            status_badge = "_SKIPPED_"
        else:
            status_badge = "**FAILED**"
        summary_lines.append(f"| `{r['id']}` | {r['name']} | {r['mode']} | {status_badge} | `{r['screenshot']}` |")

    summary_text = "\n".join(summary_lines) + "\n"
    with open(SUMMARY_FILE, "w") as sf:
        sf.write(summary_text)

    print("\n" + summary_text)

    github_summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if github_summary and os.path.exists(github_summary):
        with open(github_summary, "a") as gsf:
            gsf.write(summary_text)

    failed_count = sum(1 for r in results if r["status"] == "FAILED")
    if failed_count > 0:
        print(f"\n[FAIL] {failed_count} test matrix case(s) failed.")
        sys.exit(1)
    else:
        print(f"\n[SUCCESS] Completed {len(results)} test case(s) without failures.")

if __name__ == "__main__":
    main()
