#!/bin/bash

if [ ! -f "edk2-edk2-stable201911.zip" ] || ! echo "c6f691aa91afbaab811a369fe729f61d8e5b58bb5ad79a45446c9ee849c1a60b  edk2-edk2-stable201911.zip" | sha256sum -c - >/dev/null 2>&1; then
    echo "Downloading edk2-edk2-stable201911.zip..."
    (curl -fSL --retry 3 https://github.com/tianocore/edk2/archive/refs/tags/edk2-stable201911.zip -o edk2-edk2-stable201911.zip || \
     wget -t 3 https://github.com/tianocore/edk2/archive/refs/tags/edk2-stable201911.zip -O edk2-edk2-stable201911.zip)
    echo "c6f691aa91afbaab811a369fe729f61d8e5b58bb5ad79a45446c9ee849c1a60b  edk2-edk2-stable201911.zip" | sha256sum -c -
fi

if [ -f "edk2-edk2-stable201911.zip" ]; then
    rm -rf edk2-edk2-stable201911
    unzip -q edk2-edk2-stable201911.zip > /dev/null
    /bin/cp -a ./edk2_mod/edk2-edk2-stable201911 ./
    cd edk2-edk2-stable201911
    sed -i "s/'ucs-2'/'utf-16'/g" BaseTools/Source/Python/AutoGen/UniClassObject.py 2>/dev/null || true
    sed -i 's/"ucs-2"/"utf-16"/g' BaseTools/Source/Python/AutoGen/UniClassObject.py 2>/dev/null || true
    sed -i "s/\.tostring()/\.tobytes()/g" BaseTools/Source/Python/Common/Misc.py 2>/dev/null || true
    sed -i "s/\.tostring()/\.tobytes()/g" BaseTools/Source/Python/GenPatchPcdTable/GenPatchPcdTable.py 2>/dev/null || true
    find BaseTools/Source/Python -name "*.py" -exec sed -i "s/\.tostring()/\.tobytes()/g" {} + 2>/dev/null || true
    find BaseTools/Source/C -type f \( -name "Makefile*" -o -name "*.makefile" \) -exec sed -i 's/-Werror//g' {} + 2>/dev/null || true
    BUILD_CFLAGS="-Wno-error" make -j 4 -C BaseTools/ || true
    cd ..

    echo '======== build EDK2 for i386-efi ==============='
    bash ./build.sh ia32

    echo '======== build EDK2 for arm64-efi ==============='
    bash ./build.sh aa64

    echo '======== build EDK2 for x86_64-efi ==============='
    bash ./build.sh

    echo '======== build EDK2 for x86_64-efi shim ==============='
    bash ./build_shim.sh
else
    echo "Warning: edk2-edk2-stable201911.zip not available, using pre-staged EFI binaries."
fi

