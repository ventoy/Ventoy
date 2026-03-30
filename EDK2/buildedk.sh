#!/bin/sh

VTOY_ARM64_TOOLCHAIN_DIR=${VTOY_ARM64_TOOLCHAIN_DIR:-/opt/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu}
VTOY_ARM64_TOOLCHAIN_TAR=${VTOY_ARM64_TOOLCHAIN_TAR:-/opt/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz}
VTOY_ARM64_TOOLCHAIN_URL=${VTOY_ARM64_TOOLCHAIN_URL:-https://developer.arm.com/-/media/files/downloads/gnu/11.3.rel1/binrel/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz?rev=8d05006a68d24d929d602804ec9abfb4&revision=8d05006a-68d2-4d92-9d60-2804ec9abfb4&hash=6FA08C9E09B688DDA26E8F7A60407532}
VTOY_ARM64_PREFIX=${VTOY_ARM64_PREFIX:-aarch64-none-linux-gnu-}

prepare_arm64_toolchain() {
    if [ -x "$VTOY_ARM64_TOOLCHAIN_DIR/bin/${VTOY_ARM64_PREFIX}gcc" ]; then
        return
    fi

    if [ ! -f "$VTOY_ARM64_TOOLCHAIN_TAR" ]; then
        wget -O "$VTOY_ARM64_TOOLCHAIN_TAR" "$VTOY_ARM64_TOOLCHAIN_URL" || exit 1
    fi

    tar xf "$VTOY_ARM64_TOOLCHAIN_TAR" -C /opt || exit 1
}

rm -rf edk2-stable201911
rm -rf edk2-edk2-stable201911

unzip edk2-stable201911.zip > /dev/null

if [ -d edk2-edk2-stable201911 ] && [ ! -d edk2-stable201911 ]; then
    mv edk2-edk2-stable201911 edk2-stable201911
fi

/bin/cp -a ./edk2_mod/edk2-stable201911  ./

cd edk2-stable201911

# Python 3.9 normalizes codec names to `ucs_2`, so accept both spellings.
python3 - <<'PY'
from pathlib import Path

path = Path("BaseTools/Source/Python/AutoGen/UniClassObject.py")
text = path.read_text(encoding="utf-8")
text = text.replace("if name == 'ucs-2':", "if name in ('ucs-2', 'ucs_2'):")
path.write_text(text, encoding="utf-8")

path = Path("MdeModulePkg/Library/BrotliCustomDecompressLib/dec/decode.c")
text = path.read_text(encoding="utf-8")
text = text.replace(
    "BrotliDecoderResult BrotliDecoderDecompress(\n"
    "    size_t encoded_size, const uint8_t* encoded_buffer, size_t* decoded_size,\n"
    "    uint8_t* decoded_buffer) {",
    "BrotliDecoderResult BrotliDecoderDecompress(\n"
    "    size_t encoded_size,\n"
    "    const uint8_t encoded_buffer[BROTLI_ARRAY_PARAM(encoded_size)],\n"
    "    size_t* decoded_size,\n"
    "    uint8_t decoded_buffer[BROTLI_ARRAY_PARAM(*decoded_size)]) {"
)
path.write_text(text, encoding="utf-8")

path = Path("MdeModulePkg/Bus/Usb/UsbBusDxe/UsbBus.c")
text = path.read_text(encoding="utf-8")
text = text.replace(
    "  UINT8                   BufNum;\n"
    "  UINT8                   Toggle;\n"
    "  EFI_TPL                 OldTpl;\n",
    "  UINT8                   BufNum;\n"
    "  UINT8                   Toggle;\n"
    "  VOID                    *DataMap[EFI_USB_MAX_BULK_BUFFER_NUM] = { 0 };\n"
    "  EFI_TPL                 OldTpl;\n"
)
text = text.replace(
    "  BufNum  = 1;\n"
    "  Toggle  = EpDesc->Toggle;\n"
    "  Status  = UsbHcBulkTransfer (\n",
    "  BufNum  = 1;\n"
    "  Toggle  = EpDesc->Toggle;\n"
    "  DataMap[0] = Data;\n"
    "  Status  = UsbHcBulkTransfer (\n"
)
text = text.replace(
    "              &Data,\n",
    "              DataMap,\n"
)
path.write_text(text, encoding="utf-8")

for relpath in (
    "BaseTools/Source/Python/Common/Misc.py",
    "BaseTools/Source/Python/Eot/EotMain.py",
    "BaseTools/Source/Python/GenFds/GenFdsGlobalVariable.py",
):
    path = Path(relpath)
    text = path.read_text(encoding="utf-8")
    text = text.replace(".tostring()", ".tobytes()")
    text = text.replace(".fromstring(", ".frombytes(")
    path.write_text(text, encoding="utf-8")
PY

# Newer GCC treats this Brotli prototype warning as fatal by default.
sed -i 's/-Werror /-Werror -Wno-error=vla-parameter /g' BaseTools/Source/C/Makefiles/header.makefile

make -j 4 -C BaseTools/ || exit 1
cd ..

echo '======== build EDK2 for i386-efi ==============='
sh ./build.sh ia32 || exit 1

echo '======== build EDK2 for arm64-efi ==============='
prepare_arm64_toolchain
sh ./build.sh aa64 || exit 1

echo '======== build EDK2 for x86_64-efi ==============='
sh ./build.sh      || exit 1

