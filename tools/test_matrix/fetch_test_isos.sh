#!/usr/bin/env bash
set -euo pipefail

DEST_DIR="${1:-${VTOY_ISO_DIR:-/tmp/iso_test_set}}"
FILTER="${2:-${MATRIX_FILTER_OS:-all}}"
mkdir -p "${DEST_DIR}"

log() {
    echo "[$(date -u +"%Y-%m-%dT%H:%M:%SZ")] [FETCH-ISO] $*"
}

download_iso() {
    local filename="$1"
    local url="$2"
    local target="${DEST_DIR}/${filename}"

    if [ -f "${target}" ] && [ -s "${target}" ]; then
        log "ISO already present: ${filename} ($(du -h "${target}" | cut -f1))"
        return 0
    fi

    for cached_dir in "${HOME}/Downloads/iso_test_set" "/downloads/iso_test_set" "/tmp/iso_test_set" "${HOME}/Downloads" "/var/tmp/iso_test_set"; do
        if [ -f "${cached_dir}/${filename}" ] && [ -s "${cached_dir}/${filename}" ] && [ "${cached_dir}" != "${DEST_DIR}" ]; then
            log "Using cached ISO from ${cached_dir}/${filename}..."
            cp "${cached_dir}/${filename}" "${target}"
            return 0
        fi
    done

    log "Downloading ${filename} from ${url}..."
    curl -fLC - --retry 3 --retry-delay 2 -sSL -o "${target}" "${url}" || {
        log "Warning: Failed to download ${filename} from ${url}"
        rm -f "${target}"
        return 1
    }
    log "Successfully downloaded ${filename} ($(du -h "${target}" | cut -f1))"
}

log "Target ISO Directory: ${DEST_DIR} (Filter: ${FILTER})"

# Alpine Linux: musl and busybox early userspace archetype.
if [ "${FILTER}" = "all" ] || [ "${FILTER}" = "alpine" ]; then
    download_iso "alpine-standard-3.20.3-x86_64.iso" \
        "https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-standard-3.20.3-x86_64.iso" || true
fi

# Arch Linux: archiso and systemd initramfs archetype.
if [ "${FILTER}" = "all" ] || [ "${FILTER}" = "arch" ]; then
    download_iso "archlinux-x86_64.iso" \
        "https://archive.archlinux.org/iso/2024.08.01/archlinux-x86_64.iso" || \
    download_iso "archlinux-x86_64.iso" \
        "https://geo.mirror.pkgbuild.com/iso/latest/archlinux-x86_64.iso" || true
fi

# Debian GNU/Linux: live-boot and debian-installer archetype.
if [ "${FILTER}" = "all" ] || [ "${FILTER}" = "debian" ]; then
    download_iso "debian-amd64-netinst.iso" \
        "https://cdimage.debian.org/cdimage/archive/12.7.0/amd64/iso-cd/debian-12.7.0-amd64-netinst.iso" || \
    download_iso "debian-amd64-netinst.iso" \
        "https://cdimage.debian.org/debian-cd/current/amd64/iso-cd/debian-12.9.0-amd64-netinst.iso" || true
fi

# Fedora: dracut dmsquash and Anaconda installer archetype.
if [ "${FILTER}" = "all" ] || [ "${FILTER}" = "fedora" ]; then
    download_iso "Fedora-Server-netinst-x86_64-39.iso" \
        "https://archives.fedoraproject.org/pub/archive/fedora/linux/releases/39/Server/x86_64/iso/Fedora-Server-netinst-x86_64-39-1.5.iso" || true
fi

# FreeBSD: BSD kernel loader and geom_ventoy.ko driver archetype.
if [ "${FILTER}" = "all" ] || [ "${FILTER}" = "freebsd" ]; then
    download_iso "FreeBSD-13.2-RELEASE-amd64-bootonly.iso" \
        "https://archive.freebsd.org/old-releases/amd64/amd64/ISO-IMAGES/13.2/FreeBSD-13.2-RELEASE-amd64-bootonly.iso" || true
fi

# Ubuntu Server: casper initramfs and Subiquity installer archetype.
if [ "${FILTER}" = "all" ] || [ "${FILTER}" = "ubuntu" ]; then
    download_iso "ubuntu-24.04-live-server-amd64.iso" \
        "https://releases.ubuntu.com/24.04/ubuntu-24.04.1-live-server-amd64.iso" || true
fi

log "Discovered ISO files in ${DEST_DIR}:"
ls -lh "${DEST_DIR}"/*.iso 2>/dev/null || log "No ISOs downloaded."
