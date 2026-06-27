#!/bin/sh

if [ "$VENTOY_CERT_PASS" = "YES" ]; then
    read -s -p "Enter cert key passphrase: " KEY_PASS   
    echo

    if openssl pkey -in "$VENTOY_CERT_KEY" -passin pass:"$KEY_PASS" -out /dev/null  > /dev/null 2>&1; then
        echo "Password check OK"
    else
        echo "Incorrect password"
        exit 1
    fi
fi

sign_efi() {
    efi=$1
    
    if [ ! -f "$efi" ]; then
        printf "### %-64s  non-exist\n" "$efi"
        return
    fi

    #Don't sign if VENTOY_CERT_KEY is not defined.
    if [ -z "$VENTOY_CERT_KEY" -o -z "$VENTOY_CERT_PEM" ]; then
        printf "### %-64s  NO-CA\n" "$efi"
        return
    fi    

    if echo $efi | grep -q '\.xz$'; then
        xzcat $efi > ${efi}.unxz
        mv ${efi}.unxz  ${efi}
    fi

    rm -f "${efi}.signed"
    if [ "$VENTOY_CERT_PASS" = "YES" ]; then
        expect -f ./sign_with_pass.exp "$KEY_PASS" "$VENTOY_CERT_KEY" "$VENTOY_CERT_PEM" "${efi}" "${efi}.signed" >/dev/null 2>&1
    else
        sbsign --key "$VENTOY_CERT_KEY" --cert "$VENTOY_CERT_PEM" --output "${efi}.signed" "${efi}" >/dev/null 2>&1
    fi

    if [ -f "${efi}.signed" ]; then
        if echo $efi | grep -q '\.xz$'; then
            xz --check=crc32 "${efi}.signed"
            mv "${efi}.signed.xz" "$efi"
            rm -f "${efi}.signed"
        else
            mv "${efi}.signed" "$efi"
        fi
    else
        printf "### %-64s  failed\n" "$efi"
        exit 1
    fi 

    printf "### %-64s  success\n" "$efi"
}

if [ "$1" = "CI" ]; then
    OPT='-dR'
else
    OPT='-a'
fi

dos2unix -q ./tool/ventoy_lib.sh
dos2unix -q ./tool/VentoyWorker.sh
dos2unix -q ./tool/VentoyGTK.glade
dos2unix -q ./tool/distro_gui_type.json

. ./tool/ventoy_lib.sh

GRUB_DIR=../GRUB2/INSTALL
LANG_DIR=../LANGUAGES

if ! [ -d $GRUB_DIR ]; then
    echo "$GRUB_DIR not exist"
    exit 1
fi


cd ../IMG
sh mkcpio.sh
sh mkloopex.sh
cd -

cd ../Unix
sh pack_unix.sh
cd -

cd ../LinuxGUI
sh language.sh || exit 1
sh build.sh
cd -

cd ../Plugson
sh build.sh
sh pack.sh
cd -

cd ../Vlnk
sh build.sh
sh pack.sh
cd -


LOOP=$(losetup -f)

rm -f img.bin
dd if=/dev/zero of=img.bin bs=1M count=256 status=none

losetup -P $LOOP img.bin 

while ! grep -q 524288 /sys/block/${LOOP#/dev/}/size 2>/dev/null; do
    echo "wait $LOOP ..."
    sleep 1
done

format_ventoy_disk_mbr 0 $LOOP fdisk

$GRUB_DIR/sbin/grub-bios-setup  --skip-fs-probe  --directory="./grub/i386-pc"  $LOOP

curver=$(get_ventoy_version_from_cfg ./grub/grub.cfg)

tmpmnt=./ventoy-${curver}-mnt
tmpdir=./ventoy-${curver}

rm -rf $tmpmnt
mkdir -p $tmpmnt

mount ${LOOP}p2  $tmpmnt 

mkdir -p $tmpmnt/grub

# First copy grub.cfg file, to make it locate at front of the part2
cp $OPT ./grub/grub.cfg     $tmpmnt/grub/

ls -1 ./grub/ | grep -v 'grub\.cfg' | while read line; do
    cp $OPT ./grub/$line $tmpmnt/grub/
done

#tar help txt
cd $tmpmnt/grub/
tar czf help.tar.gz ./help/
rm -rf ./help
cd ../../

#tar menu txt & update menulang.cfg
cd $tmpmnt/grub/

vtlangtitle=$(grep VTLANG_LANGUAGE_NAME menu/zh_CN.json | awk -F\" '{print $4}')
echo "menuentry \"zh_CN  -  $vtlangtitle\" --class=menu_lang_item --class=debug_menu_lang --class=F5tool {" >> menulang.cfg
echo "    vt_load_menu_lang zh_CN"  >> menulang.cfg
echo "}"  >> menulang.cfg

ls -1 menu/ | grep -v 'zh_CN' | sort | while read vtlang; do
    vtlangname=${vtlang%.*}
    vtlangtitle=$(grep VTLANG_LANGUAGE_NAME menu/$vtlang | awk -F\" '{print $4}')
    echo "menuentry \"$vtlangname  -  $vtlangtitle\" --class=menu_lang_item --class=debug_menu_lang --class=F5tool {" >> menulang.cfg
    echo "    vt_load_menu_lang $vtlangname"  >> menulang.cfg
    echo "}"  >> menulang.cfg
done
echo "menuentry \"\$VTLANG_RETURN_PREVIOUS\" --class=vtoyret VTOY_RET {" >> menulang.cfg
echo "        echo \"Return ...\"" >> menulang.cfg
echo "}" >> menulang.cfg

tar czf menu.tar.gz ./menu/
rm -rf ./menu
cd ../../



cp $OPT ./ventoy   $tmpmnt/
cp $OPT ./EFI   $tmpmnt/
cp $OPT ./tool/ENROLL_THIS_KEY_IN_MOKMANAGER.cer $tmpmnt/


mkdir -p $tmpmnt/tool
# cp $OPT ./tool/i386/mount.exfat-fuse     $tmpmnt/tool/mount.exfat-fuse_i386
# cp $OPT ./tool/x86_64/mount.exfat-fuse   $tmpmnt/tool/mount.exfat-fuse_x86_64
# cp $OPT ./tool/aarch64/mount.exfat-fuse  $tmpmnt/tool/mount.exfat-fuse_aarch64
# to save space
dd status=none bs=1024 count=16  if=./tool/i386/vtoycli    of=$tmpmnt/tool/mount.exfat-fuse_i386
dd status=none bs=1024 count=16  if=./tool/x86_64/vtoycli  of=$tmpmnt/tool/mount.exfat-fuse_x86_64
dd status=none bs=1024 count=16  if=./tool/aarch64/vtoycli of=$tmpmnt/tool/mount.exfat-fuse_aarch64
cp -a ./tool/create_ventoy_iso_part_dm.sh  $tmpmnt/tool/


rm -f $tmpmnt/grub/i386-pc/*.img


sign_efi $tmpmnt/EFI/BOOT/fbia32.efi
sign_efi $tmpmnt/EFI/BOOT/fbaa64.efi
sign_efi $tmpmnt/EFI/BOOT/grubx64_real.efi
sign_efi $tmpmnt/EFI/BOOT/grubia32_real.efi
sign_efi $tmpmnt/ventoy/iso9660_x64.efi
sign_efi $tmpmnt/ventoy/iso9660_ia32.efi
sign_efi $tmpmnt/ventoy/iso9660_aa64.efi
sign_efi $tmpmnt/ventoy/udf_x64.efi
sign_efi $tmpmnt/ventoy/udf_ia32.efi
sign_efi $tmpmnt/ventoy/udf_aa64.efi
sign_efi $tmpmnt/ventoy/ventoy_x64.efi
sign_efi $tmpmnt/ventoy/ventoy_ia32.efi
sign_efi $tmpmnt/ventoy/ventoy_aa64.efi
sign_efi $tmpmnt/ventoy/vtoyutil_x64.efi
sign_efi $tmpmnt/ventoy/vtoyutil_ia32.efi
sign_efi $tmpmnt/ventoy/vtoyutil_aa64.efi
sign_efi $tmpmnt/ventoy/wimboot.i386.efi.xz
sign_efi $tmpmnt/ventoy/wimboot.x86_64.xz

#inject Ventoy Grub sign sha256 value into VtoyShim
grub_signsha256=$(pesign -i $tmpmnt/EFI/BOOT/grubx64_real.efi -h -d sha256 | awk '{print $2}')
magic_cnt=$(hexdump -C $tmpmnt/EFI/BOOT/fbx64.efi | grep '26 26 26 26 26 26 26 26' | wc -l)
if [ $magic_cnt -ne 1 ]; then
    echo "hash magic duplicate"
    exit 1
fi
magic_off_hex=$(hexdump -C $tmpmnt/EFI/BOOT/fbx64.efi | grep '26 26 26 26 26 26 26 26' | awk '{print $1}')
magic_off=$(printf '%u' "0x${magic_off_hex}")

echo_cmd=$(echo $grub_signsha256 | sed 's/\(..\)/\\x\1/g')

echo Ventoy Grub sign hash $grub_signsha256
echo -en "$echo_cmd" | dd bs=1 count=32 of=$tmpmnt/EFI/BOOT/fbx64.efi seek=$magic_off conv=notrunc status=none

sign_efi $tmpmnt/EFI/BOOT/fbx64.efi


umount $tmpmnt && rm -rf $tmpmnt


rm -rf $tmpdir
mkdir -p $tmpdir/boot
mkdir -p $tmpdir/ventoy
echo $curver > $tmpdir/ventoy/version
dd if=$LOOP of=$tmpdir/boot/boot.img bs=1 count=512  status=none
dd if=$LOOP of=$tmpdir/boot/core.img bs=512 count=2047 skip=1 status=none
xz --check=crc32 $tmpdir/boot/core.img

cp $OPT ./tool $tmpdir/
rm -f $tmpdir/ENROLL_THIS_KEY_IN_MOKMANAGER.cer
cp $OPT Ventoy2Disk.sh $tmpdir/
cp $OPT VentoyWeb.sh $tmpdir/
cp $OPT VentoyPlugson.sh $tmpdir/
cp $OPT VentoyVlnk.sh $tmpdir/
cp $OPT VentoyGUI* $tmpdir/


cp $OPT README $tmpdir/
cp $OPT plugin $tmpdir/
cp $OPT CreatePersistentImg.sh $tmpdir/
cp $OPT ExtendPersistentImg.sh $tmpdir/
dos2unix -q $tmpdir/Ventoy2Disk.sh
dos2unix -q $tmpdir/VentoyWeb.sh
dos2unix -q $tmpdir/VentoyPlugson.sh
dos2unix -q $tmpdir/VentoyVlnk.sh


dos2unix -q $tmpdir/CreatePersistentImg.sh
dos2unix -q $tmpdir/ExtendPersistentImg.sh

cp $OPT ../LinuxGUI/WebUI $tmpdir/
sed 's/.*SCRIPT_DEL_THIS \(.*\)/\1/g' -i $tmpdir/WebUI/index.html

#32MB disk img
dd status=none if=$LOOP of=$tmpdir/ventoy/ventoy.disk.img bs=512 count=$VENTOY_SECTOR_NUM skip=$part2_start_sector


#4k image
# echo "make 4K img ..."
# dd status=none if=/dev/zero of=$tmpdir/ventoy/ventoy_4k.disk.img bs=1M count=32
# mkfs.vfat -F 16 -n VTOYEFI -s 1 -S 4096 $tmpdir/ventoy/ventoy_4k.disk.img
# vDIR1=$(mktemp -d)
# vDIR2=$(mktemp -d)
# mount $tmpdir/ventoy/ventoy.disk.img $vDIR1
# mount $tmpdir/ventoy/ventoy_4k.disk.img $vDIR2
# cp -a $vDIR1/* $vDIR2/
# umount $vDIR1
# umount $vDIR2
# rm -rf $vDIR1 $vDIR2

# xz --check=crc32 $tmpdir/ventoy/ventoy_4k.disk.img

xz --check=crc32 $tmpdir/ventoy/ventoy.disk.img



losetup -d $LOOP && rm -f img.bin

rm -f ventoy-${curver}-linux.tar.gz


CurDir=$PWD

for d in i386 x86_64 aarch64 mips64el; do
    cd $tmpdir/tool/$d
    for file in $(ls); do
        if [ "$file" != "xzcat" ]; then
            if echo "$file" | grep -q '^Ventoy2Disk'; then
                chmod +x $file
            else
                xz --check=crc32 $file
            fi
        fi
    done
    cd $CurDir
done

#chmod 
find $tmpdir/ -type d -exec chmod 755 "{}" +
find $tmpdir/ -type f -exec chmod 644 "{}" +
chmod +x $tmpdir/Ventoy2Disk.sh
chmod +x $tmpdir/VentoyWeb.sh
chmod +x $tmpdir/VentoyPlugson.sh
chmod +x $tmpdir/VentoyVlnk.sh
chmod +x $tmpdir/VentoyGUI*
chmod +x $tmpdir/tool/*.sh

for d in i386 x86_64 aarch64 mips64el; do
    chmod +x $tmpdir/tool/$d/xzcat
    chmod +x $tmpdir/tool/$d/Ventoy2Disk.*
done


cp $OPT $LANG_DIR/languages.json $tmpdir/tool/


chmod +x $tmpdir/CreatePersistentImg.sh
chmod +x $tmpdir/ExtendPersistentImg.sh

tar -czvf ventoy-${curver}-linux.tar.gz $tmpdir



rm -f ventoy-${curver}-windows.zip

cp $OPT Ventoy2Disk.exe $tmpdir/
cp $OPT VentoyPlugson.exe $tmpdir/
cp $OPT VentoyVlnk.exe $tmpdir/
cp $OPT FOR_X64_ARM.txt $tmpdir/
mkdir -p $tmpdir/altexe
cp $OPT Ventoy2Disk_*.exe $tmpdir/altexe/
cp $OPT VentoyPlugson_*.exe $tmpdir/altexe/



cp $OPT $tmpdir/tool/plugson.tar.xz $tmpdir/ventoy/
cp $OPT $LANG_DIR/languages.json $tmpdir/ventoy/
rm -rf $tmpdir/tool
rm -f $tmpdir/*.sh
rm -f $tmpdir/VentoyGUI.*
rm -rf $tmpdir/WebUI
rm -f $tmpdir/README


zip -r ventoy-${curver}-windows.zip $tmpdir/

rm -rf $tmpdir

echo "=============== run livecd.sh ==============="
cd ../LiveCDGUI
sh livecd.sh $1
cd $CurDir

mv ../LiveCDGUI/ventoy*.iso ./

if [ -e ventoy-${curver}-windows.zip ] && [ -e ventoy-${curver}-linux.tar.gz ]; then

    if [ -z "$VENTOY_CERT_KEY" -o -z "$VENTOY_CERT_PEM" ]; then
        echo "[warning]: EFI files are not signed and can only boot when Secure Boot is disabled."
        echo "[warning]: EFI files are not signed and can only boot when Secure Boot is disabled."
        echo "[warning]: EFI files are not signed and can only boot when Secure Boot is disabled."
    fi

    echo -e "\n ============= SUCCESS =================\n"
else
    echo -e "\n ============= FAILED =================\n"
    exit 1
fi

rm -f log.txt
rm -f sha256.txt
sha256sum ventoy-${curver}-* > sha256.txt

