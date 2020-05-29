#!/bin/sh


echo ''
echo '***********************************************************'
echo '*                Ventoy2Disk Script                       *'
echo '*             longpanda  admin@ventoy.net                 *'
echo '***********************************************************'
echo ''

OLDDIR=$PWD

if ! [ -f ./tool/xzcat ]; then
    if [ -f ${0%Ventoy2Disk.sh}/tool/xzcat ]; then
        cd ${0%Ventoy2Disk.sh}    
    fi
fi

if ! [ -f ./boot/boot.img ]; then
    if [ -d ./grub ]; then
        echo "Don't run Ventoy2Disk.sh here, please download the released install package, and run the script in it."
    else
        echo "Please run under the correct directory!" 
    fi
    exit 1
fi

echo "############# Ventoy2Disk $* ################" >> ./log.txt

#decompress tool
if ! [ -f ./tool/ash ]; then
    cd tool
    chmod +x ./xzcat
    for file in $(ls *.xz); do
        ./xzcat $file > ${file%.xz}
        chmod +x ${file%.xz}
    done
    cd ../

    if ! [ -f ./tool/ash ]; then
        echo 'Failed to decompress tools ...'
        cd $OLDDIR
        exit 1
    fi
fi

./tool/ash ./tool/VentoyWorker.sh $*

cd $OLDDIR

