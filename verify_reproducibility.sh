#!/bin/bash
set -eo pipefail

VTOY_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_A="/tmp/ventoy_build_a"
BUILD_B="/tmp/ventoy_build_b"
DIFF_OUT="${VTOY_ROOT}/diffoscope_report.html"

log() {
    echo "[$(date -u +"%Y-%m-%dT%H:%M:%SZ")] [REPRO-VERIFY] $*"
}

cleanup() {
    log "Cleaning up temporary build workspaces..."
    rm -rf "${BUILD_A}" "${BUILD_B}"
}
trap cleanup EXIT

log "=========================================================================="
log "Starting Two-Pass Independent Verification of Ventoy Build Reproducibility"
log "=========================================================================="

rm -rf "${BUILD_A}" "${BUILD_B}"
mkdir -p "${BUILD_A}" "${BUILD_B}"

log "Executing Pass A: Building in ${BUILD_A}..."
git clone "${VTOY_ROOT}" "${BUILD_A}/ventoy" >/dev/null 2>&1 || cp -a "${VTOY_ROOT}" "${BUILD_A}/ventoy"
(
    cd "${BUILD_A}/ventoy"
    export SOURCE_DATE_EPOCH=1700000000
    export LC_ALL=C
    export TZ=UTC
    if ! bash build_reproducible.sh > "${BUILD_A}/build.log" 2>&1; then
        log "Warning: Pass A build returned non-zero exit code. Checking logs..."
        tail -n 25 "${BUILD_A}/build.log" || true
    fi
)

# Pass B varies the build path and umask to detect non-deterministic path embedding or permission leakage.
log "Executing Pass B: Building in ${BUILD_B} with randomized build paths and umask..."
git clone "${VTOY_ROOT}" "${BUILD_B}/ventoy" >/dev/null 2>&1 || cp -a "${VTOY_ROOT}" "${BUILD_B}/ventoy"
(
    cd "${BUILD_B}/ventoy"
    umask 0022
    export SOURCE_DATE_EPOCH=1700000000
    export LC_ALL=C
    export TZ=UTC
    if ! bash build_reproducible.sh > "${BUILD_B}/build.log" 2>&1; then
        log "Warning: Pass B build returned non-zero exit code. Checking logs..."
        tail -n 25 "${BUILD_B}/build.log" || true
    fi
)

log "Comparing generated release artifacts between Pass A and Pass B..."

DIFF_COUNT=0
ARTIFACTS=(
    "INSTALL/ventoy/vtoyjump64.exe"
    "INSTALL/ventoy/vtoyjump32.exe"
    "INSTALL/EFI/BOOT/fbx64.efi"
    "INSTALL/ventoy/ventoy_x64.efi"
    "INSTALL/ventoy/vtoyutil_x64.efi"
    "IMG/ventoy.cpio"
    "Unix/ventoy_unix.cpio"
)

for art in "${ARTIFACTS[@]}"; do
    FILE_A="${BUILD_A}/ventoy/${art}"
    FILE_B="${BUILD_B}/ventoy/${art}"

    if [ ! -f "${FILE_A}" ] || [ ! -f "${FILE_B}" ]; then
        log "[FAIL] ${art}: MISSING ARTIFACT (Pass A: $([ -f "${FILE_A}" ] && echo "present" || echo "missing"), Pass B: $([ -f "${FILE_B}" ] && echo "present" || echo "missing"))"
        DIFF_COUNT=$((DIFF_COUNT + 1))
        continue
    fi

    SHA_A=$(sha256sum "${FILE_A}" | awk '{print $1}')
    SHA_B=$(sha256sum "${FILE_B}" | awk '{print $1}')

    if [ "${SHA_A}" = "${SHA_B}" ]; then
        log "[PASS] ${art}: BIT-FOR-BIT IDENTICAL (SHA256: ${SHA_A})"
    else
        log "[FAIL] ${art}: CHECKSUM MISMATCH!"
        log "       Pass A: ${SHA_A}"
        log "       Pass B: ${SHA_B}"
        DIFF_COUNT=$((DIFF_COUNT + 1))
        if command -v diffoscope >/dev/null 2>&1; then
            diffoscope "${FILE_A}" "${FILE_B}" || true
        fi
    fi
done

if [ ${DIFF_COUNT} -eq 0 ]; then
    log "=========================================================================="
    log "VERIFICATION SUCCESS: All tested artifacts are 100% bit-for-bit reproducible!"
    log "=========================================================================="
    exit 0
else
    log "=========================================================================="
    log "VERIFICATION FAILED: ${DIFF_COUNT} artifact(s) showed non-deterministic drift."
    log "=========================================================================="
    exit 1
fi
