#!/bin/sh

dos2unix -q ./tool/ventoy_lib.sh
dos2unix -q ./tool/VentoyWorker.sh

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
cp -a ./grub/grub.cfg     $tmpmnt/grub/

ls -1 ./grub/ | grep -v 'grub\.cfg' | while read line; do
    cp -a ./grub/$line $tmpmnt/grub/
done

cp -a ./ventoy   $tmpmnt/
cp -a ./EFI   $tmpmnt/
cp -a ./tool/ENROLL_THIS_KEY_IN_MOKMANAGER.cer $tmpmnt/


mkdir -p $tmpmnt/tool
cp -a ./tool/mount*     $tmpmnt/tool/

rm -f $tmpmnt/grub/i386-pc/*.img


umount $tmpmnt && rm -rf $tmpmnt


rm -rf $tmpdir
mkdir -p $tmpdir/boot
mkdir -p $tmpdir/ventoy
echo $curver > $tmpdir/ventoy/version
dd if=$LOOP of=$tmpdir/boot/boot.img bs=1 count=512  status=none
dd if=$LOOP of=$tmpdir/boot/core.img bs=512 count=2047 skip=1 status=none
xz --check=crc32 $tmpdir/boot/core.img

cp -a ./tool $tmpdir/
rm -f $tmpdir/ENROLL_THIS_KEY_IN_MOKMANAGER.cer
cp -a Ventoy2Disk.sh $tmpdir/
cp -a README $tmpdir/
cp -a plugin $tmpdir/
cp -a CreatePersistentImg.sh $tmpdir/
dos2unix -q $tmpdir/Ventoy2Disk.sh
dos2unix -q $tmpdir/CreatePersistentImg.sh

#32MB disk img
dd status=none if=$LOOP of=$tmpdir/ventoy/ventoy.disk.img bs=512 count=$VENTOY_SECTOR_NUM skip=$part2_start_sector
xz --check=crc32 $tmpdir/ventoy/ventoy.disk.img

losetup -d $LOOP && rm -f img.bin

rm -f ventoy-${curver}-linux.tar.gz


CurDir=$PWD
cd $tmpdir/tool

for file in $(ls); do
    if [ "$file" != "xzcat" ] && [ "$file" != "ventoy_lib.sh" ]; then
        xz --check=crc32 $file
    fi
done

#chmod 
cd $CurDir
find $tmpdir/ -type d -exec chmod 755 "{}" +
find $tmpdir/ -type f -exec chmod 644 "{}" +
chmod +x $tmpdir/Ventoy2Disk.sh
chmod +x $tmpdir/CreatePersistentImg.sh

tar -czvf ventoy-${curver}-linux.tar.gz $tmpdir



rm -f ventoy-${curver}-windows.zip
cp -a Ventoy2Disk*.exe $tmpdir/
cp -a $LANG_DIR/languages.ini $tmpdir/ventoy/
rm -rf $tmpdir/tool
rm -f $tmpdir/*.sh
rm -f $tmpdir/README


zip -r ventoy-${curver}-windows.zip $tmpdir/

rm -rf $tmpdir

cd ../LiveCD
sh livecd.sh
cd $CurDir

mv ../LiveCD/ventoy*.iso ./

if [ -e ventoy-${curver}-windows.zip ] && [ -e ventoy-${curver}-linux.tar.gz ]; then
    echo -e "\n ============= SUCCESS =================\n"
else
    echo -e "\n ============= FAILED =================\n"
    exit 1
fi

rm -f log.txt

