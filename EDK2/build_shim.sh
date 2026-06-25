#!/bin/sh

if [ -z "$1" ]; then
    EDKARCH=X64
    postfix=x64
elif [ "$1" = "ia32" ]; then
    EDKARCH=IA32
    postfix=ia32
    shift
elif [ "$1" = "aa64" ]; then
    EDKARCH=AARCH64
    postfix=aa64
    shift
fi

cd edk2-edk2-stable201911

rm -rf ./Conf/.cache
rm -f ./Conf/.AutoGenIdFile.txt

VTEFI_PATH=Build/MdeModule/RELEASE_GCC48/$EDKARCH/MdeModulePkg/Application/VtoyShim/VtoyShim/OUTPUT/VtoyShim.efi
DST_PATH=../../INSTALL/EFI/BOOT/fb${postfix}.efi


rm -f $VTEFI_PATH
rm -f $DST_PATH

unset WORKSPACE
source ./edksetup.sh

if [ "$EDKARCH" = "AARCH64" ]; then    
    PATH=$PATH:/opt/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin \
    GCC48_AARCH64_PREFIX=aarch64-linux-gnu- \
    build -p MdeModulePkg/MdeModulePkg.dsc -a $EDKARCH -b RELEASE -t GCC48  -m MdeModulePkg/Application/VtoyShim/VtoyShim.inf
else
    build -p MdeModulePkg/MdeModulePkg.dsc -a $EDKARCH -b RELEASE -t GCC48  -m MdeModulePkg/Application/VtoyShim/VtoyShim.inf
fi

if [ -e $VTEFI_PATH ]; then

    objdump -h "$VTEFI_PATH"
    echo ""

    objcopy \
        --add-section .sbat="MdeModulePkg/Application/VtoyShim/sbat.csv" \
        --set-section-flags .sbat=alloc,load,readonly,data \
        "$VTEFI_PATH" "$DST_PATH"

    #find the right sbat section VMA
    tmpfile=$(mktemp)
    
    cnt1=$(objdump -h "$DST_PATH" | grep -P '^\s*[0-9][0-9]* \.' | wc -l)
    cnt2=$(objdump -h "$DST_PATH" | grep -P 'ALLOC' | wc -l)
    
    if [ $cnt1 -ne $cnt2 ]; then
        echo "Section count mismatch $cnt1 $cnt2"
        objdump -h "$DST_PATH"
        exit 1
    fi

    objdump -h "$DST_PATH" | grep -P '^\s*[0-9][0-9]* \.' > $tmpfile    
    sbat_size=$(stat -c '%s' MdeModulePkg/Application/VtoyShim/sbat.csv)    
    sbat_size_hex=$(printf '0x%08x' $sbat_size)
    
    lmax='0000000000000000'
    lsize='0000000000000000'
    while read line; do
        echo $line        
        lbase=$(echo $line | awk '{print $4}')        
        if expr "$lmax" \< "$lbase" > /dev/null; then
            lmax=$lbase
            lsize=$(echo $line | awk '{print $3}')
        fi
        echo "max=$lmax size=$lsize"
    done < $tmpfile
    rm -f $tmpfile


    lvma=$((0x${lsize}+0x${lmax}))
    lvma_align=`printf '0x%08x' $(( (lvma + 4095) / 4096 * 4096 ))`
    echo "sbat section lvma_align=$lvma_align"    
    
    objcopy --adjust-section-vma .sbat=$lvma_align "$DST_PATH"

    img_base=$(objdump -p "$DST_PATH" | grep ImageBase | awk '{print $2}')
    size_img=0x$(objdump -p "$DST_PATH" | grep SizeOfImage | awk '{print $2}')
    sbat_end=`printf '0x%08x' $(($lvma_align+$sbat_size_hex))`

    if [ "$img_base" != "0000000000000000" ]; then
        echo "#### ImageBase is not 0 $img_base"
        exit 1
    fi
    
    echo "size_img=$size_img  sbat_size=$sbat_size_hex sbat_range $lvma_align - $sbat_end"    

    if expr "$size_img" \< "$sbat_end" > /dev/null; then
        echo "SizeOfImage $size_img less than sbat section addr $sbat_end"
        exit 1
    else
        echo "SizeOfImage $size_img >= $sbat_end is OK"
    fi

    objdump -h "$DST_PATH"

    echo -e '\n\n====================== SUCCESS ========================\n\n'    

    cd ..
else
    echo -e '\n\n====================== FAILED ========================\n\n'
    cd ..
    exit 1
fi

