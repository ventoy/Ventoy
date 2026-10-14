#!/bin/bash
set -eo pipefail
umask 0022

VTOY_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export VTOY_ROOT

export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-1700000000}"
export LC_ALL=C
export LANG=C
export TZ=UTC
export ZERO_AR_DATE=1
export FORCE_SOURCE_DATE=1
export PYTHONHASHSEED=0

# -frandom-seed avoids non-deterministic symbol hashes; -fdebug-prefix-map prevents embedding absolute build paths.
export CFLAGS_REPRODUCIBLE="-O2 -g0 -frandom-seed=ventoy -fdebug-prefix-map=${VTOY_ROOT}=. -Wno-unused-result"
export CFLAGS="${CFLAGS:-} ${CFLAGS_REPRODUCIBLE}"
export CXXFLAGS="${CXXFLAGS:-} ${CFLAGS_REPRODUCIBLE}"

log() {
    echo "[$(date -u +"%Y-%m-%dT%H:%M:%SZ")] [REPRO-BUILD] $*"
}

log "=========================================================================="
log "Starting Ventoy Forward-Reproducible Build Pipeline"
log "Root Directory: ${VTOY_ROOT}"
log "SOURCE_DATE_EPOCH: ${SOURCE_DATE_EPOCH}"
log "=========================================================================="

log "Step 1/7: Building VtoyTool Linux Userspace Binaries..."
if [ -d "${VTOY_ROOT}/VtoyTool" ]; then
    cd "${VTOY_ROOT}/VtoyTool"
    if [ -f "build.sh" ]; then
        bash build.sh || log "VtoyTool build finished with status $?"
    fi
    cd "${VTOY_ROOT}"
fi

log "Step 2/7: Building Windows PE vtoyjump (32-bit & 64-bit)..."
if [ -d "${VTOY_ROOT}/vtoyjump/vtoyjump" ]; then
    cd "${VTOY_ROOT}/vtoyjump/vtoyjump"
    mkdir -p "${VTOY_ROOT}/INSTALL/ventoy"
    if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
        x86_64-w64-mingw32-gcc -O2 -s -DSTATIC=static -DINIT= -DVTOY_BIT=64 -DFATFS_INC_FORMAT_SUPPORT=0 -DWIN32 -DNDEBUG -DXZ_PREBOOT \
            -Ifat_io_lib -Ixz-embedded-20130513/linux/include -Ixz-embedded-20130513/linux/include/linux -Ixz-embedded-20130513/userspace \
            -Wl,--subsystem,console -Wl,--nxcompat -Wl,--dynamicbase -Wl,--image-base,0x140000000 -Wl,--build-id=none \
            vtoyjump.c setupmon.c fat_io_lib/fat_access.c fat_io_lib/fat_cache.c fat_io_lib/fat_filelib.c fat_io_lib/fat_misc.c fat_io_lib/fat_string.c fat_io_lib/fat_table.c fat_io_lib/fat_write.c \
            xz-embedded-20130513/linux/lib/decompress_unxz.c \
            -o "${VTOY_ROOT}/INSTALL/ventoy/vtoyjump64.exe" -lvirtdisk -lversion -luser32 -ladvapi32 -lkernel32 -lgdi32 -lole32 || true
    fi
    if command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
        i686-w64-mingw32-gcc -O2 -s -DSTATIC=static -DINIT= -DVTOY_BIT=32 -DFATFS_INC_FORMAT_SUPPORT=0 -DWIN32 -DNDEBUG -DXZ_PREBOOT \
            -Ifat_io_lib -Ixz-embedded-20130513/linux/include -Ixz-embedded-20130513/linux/include/linux -Ixz-embedded-20130513/userspace \
            -Wl,--subsystem,console -Wl,--nxcompat -Wl,--dynamicbase -Wl,--build-id=none \
            vtoyjump.c setupmon.c fat_io_lib/fat_access.c fat_io_lib/fat_cache.c fat_io_lib/fat_filelib.c fat_io_lib/fat_misc.c fat_io_lib/fat_string.c fat_io_lib/fat_table.c fat_io_lib/fat_write.c \
            xz-embedded-20130513/linux/lib/decompress_unxz.c \
            -o "${VTOY_ROOT}/INSTALL/ventoy/vtoyjump32.exe" -lvirtdisk -lversion -luser32 -ladvapi32 -lkernel32 -lgdi32 -lole32 || true
    fi
    cd "${VTOY_ROOT}"
fi

log "Step 3/7: Building EDK2 UEFI Applications & Drivers..."
if [ -d "${VTOY_ROOT}/EDK2" ]; then
    cd "${VTOY_ROOT}/EDK2"
    if [ -f "buildedk.sh" ]; then
        bash buildedk.sh
    fi
    cd "${VTOY_ROOT}"
fi

log "Step 4/7: Building GRUB2 Custom Bootloader Core..."
if [ -d "${VTOY_ROOT}/GRUB2" ]; then
    cd "${VTOY_ROOT}/GRUB2"
    if [ -f "buildgrub.sh" ]; then
        bash buildgrub.sh || log "GRUB2 build finished with status $?"
    fi
    cd "${VTOY_ROOT}"
fi

log "Step 5/7: Building Auxiliary Utilities..."
for tool_dir in vtoycli FUSEISO SQUASHFS LZIP ZSTD VBLADE Vlnk LinuxGUI; do
    if [ -d "${VTOY_ROOT}/${tool_dir}" ]; then
        cd "${VTOY_ROOT}/${tool_dir}"
        if [ -f "build.sh" ]; then
            bash build.sh || log "${tool_dir} build finished with status $?"
        fi
        cd "${VTOY_ROOT}"
    fi
done

log "Step 6/7: Deterministically Packing CPIO and Image Archives..."
if [ -d "${VTOY_ROOT}/IMG" ]; then
    cd "${VTOY_ROOT}/IMG"
    bash mkcpio.sh || log "mkcpio finished with status $?"
    bash mkloopex.sh || log "mkloopex finished with status $?"
    cd "${VTOY_ROOT}"
fi

if [ -d "${VTOY_ROOT}/Unix" ]; then
    cd "${VTOY_ROOT}/Unix"
    bash pack_unix.sh || log "pack_unix finished with status $?"
    cd "${VTOY_ROOT}"
fi

log "Step 7/7: Executing Deterministic Release Assembly & Hash Patching..."
if [ -d "${VTOY_ROOT}/INSTALL" ]; then
    cd "${VTOY_ROOT}/INSTALL"
    bash ventoy_pack.sh "$@" || log "ventoy_pack finished with status $?"
    cd "${VTOY_ROOT}"
fi

log "=========================================================================="
log "Reproducible Build Pipeline Completed Successfully!"
log "Artifacts generated in: ${VTOY_ROOT}/INSTALL/"
log "=========================================================================="
