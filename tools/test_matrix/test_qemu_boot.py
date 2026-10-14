#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import zlib
import struct
import socket
import shutil

WORKSPACE = os.environ.get("VTOY_WORKSPACE", "/ventoy" if os.path.exists("/ventoy") else os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
USB_IMG = os.path.join(WORKSPACE, "ventoy_usb.img")
OUTPUT_DIR = os.path.join(WORKSPACE, "qemu_test_results")
os.makedirs(OUTPUT_DIR, exist_ok=True)

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

def run_boot_test(mode="uefi", wait_sec=8):
    monitor_sock = f"/tmp/qemu_monitor_{mode}_{os.getpid()}_{int(time.time())}.sock"
    if os.path.exists(monitor_sock):
        try:
            os.remove(monitor_sock)
        except Exception:
            pass

    ppm_out = os.path.join(OUTPUT_DIR, f"boot_{mode}.ppm")
    png_out = os.path.join(OUTPUT_DIR, f"boot_{mode}.png")

    has_kvm = os.path.exists("/dev/kvm") and os.access("/dev/kvm", os.R_OK | os.W_OK)

    qemu_cmd = [
        "qemu-system-x86_64",
        "-m", "1024",
        "-smp", "2",
        "-drive", f"file={USB_IMG},format=raw,index=0,media=disk",
        "-monitor", f"unix:{monitor_sock},server,nowait",
        "-display", "none",
        "-vga", "std",
        "-no-reboot",
    ]

    if has_kvm:
        qemu_cmd.extend(["-enable-kvm", "-cpu", "host"])
    else:
        qemu_cmd.extend(["-cpu", "max"])

    if mode == "uefi":
        ovmf_code = "/usr/share/ovmf/OVMF.fd"
        if os.path.exists(ovmf_code):
            qemu_cmd.extend(["-bios", ovmf_code])
        else:
            vars_tmp = f"/tmp/OVMF_VARS_{os.getpid()}.fd"
            if os.path.exists("/usr/share/OVMF/OVMF_VARS.fd"):
                shutil.copy("/usr/share/OVMF/OVMF_VARS.fd", vars_tmp)
            qemu_cmd.extend([
                "-drive", "if=pflash,format=raw,unit=0,file=/usr/share/OVMF/OVMF_CODE.fd,readonly=on",
                "-drive", f"if=pflash,format=raw,unit=1,file={vars_tmp}"
            ])

    proc = subprocess.Popen(qemu_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(5.0)
    connected = False
    start_time = time.time()
    connect_timeout = 20 if not has_kvm else 10

    while time.time() - start_time < connect_timeout:
        if os.path.exists(monitor_sock):
            try:
                s.connect(monitor_sock)
                connected = True
                break
            except (socket.error, ConnectionRefusedError):
                pass
        if proc.poll() is not None:
            break
        time.sleep(0.5)

    if not connected:
        print(f"[QEMU Monitor] Failed to connect to monitor socket for {mode.upper()} mode within {connect_timeout}s.")
        try:
            proc.terminate()
            proc.communicate(timeout=3)
        except Exception:
            proc.kill()
        return False, None

    try:
        time.sleep(wait_sec if has_kvm else int(wait_sec * 1.5))
        s.recv(1024)

        cmd = f"screendump {ppm_out}\n"
        s.sendall(cmd.encode())
        time.sleep(1.0)
        s.recv(1024)

        s.sendall(b"quit\n")
        s.close()
    except Exception as e:
        print(f"[QEMU Monitor] Error communicating with monitor: {e}")

    try:
        proc.terminate()
        stdout, stderr = proc.communicate(timeout=5)
    except Exception:
        proc.kill()
        stdout, stderr = proc.communicate()

    if os.path.exists(monitor_sock):
        try:
            os.remove(monitor_sock)
        except Exception:
            pass

    if os.path.exists(ppm_out) and os.path.getsize(ppm_out) > 0:
        if ppm_to_png(ppm_out, png_out):
            print(f"[SUCCESS] {mode.upper()} Boot Test Passed: {png_out}")
            return True, png_out

    print(f"[WARNING] No screendump output produced for {mode.upper()} mode.")
    return False, None

def main():
    if not shutil.which("qemu-system-x86_64"):
        print("Error: qemu-system-x86_64 is not installed or not in PATH.")
        sys.exit(1)

    if not os.path.exists(USB_IMG):
        print(f"Error: USB image not found at {USB_IMG}")
        sys.exit(1)

    uefi_ok, uefi_png = run_boot_test("uefi", wait_sec=6)
    bios_ok, bios_png = run_boot_test("bios", wait_sec=5)

    print("=" * 70)
    print("Boot Test Summary:")
    print(f"  UEFI Boot: {'PASSED' if uefi_ok else 'FAILED'} (Screenshot: {uefi_png})")
    print(f"  BIOS Boot: {'PASSED' if bios_ok else 'FAILED'} (Screenshot: {bios_png})")
    print("=" * 70)

    if not (uefi_ok and bios_ok):
        print("\n[FAIL] One or more boot tests failed!")
        sys.exit(1)

if __name__ == "__main__":
    main()
