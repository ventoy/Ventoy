#!/bin/sh

if ! [ -f ./tool/ventoy_lib.sh ]; then
    if [ -f ${0%Ventoy2Disk.sh}/tool/ventoy_lib.sh ]; then
        cd ${0%Ventoy2Disk.sh}    
    fi
fi

if [ -f ./ventoy/version ]; then
    curver=$(cat ./ventoy/version) 
fi

echo ''
echo '**********************************************'
echo "      Ventoy: $curver"
echo "      longpanda admin@ventoy.net"
echo "      https://www.ventoy.net"
echo '**********************************************'
echo ''

OLDDIR=$(pwd)
PATH=./tool:$PATH

if ! [ -f ./boot/boot.img ]; then
    if [ -d ./grub ]; then
        echo "Don't run Ventoy2Disk.sh here, please download the released install package, and run the script in it."
    else
        echo "Please run under the correct directory!" 
    fi
    exit 1
fi

echo "############# Ventoy2Disk $* ################" >> ./log.txt
date >> ./log.txt

#decompress tool
if [ -f ./tool/VentoyWorker.sh ]; then
    echo "no need to decompress tools" >> ./log.txt
else
    cd tool
    
    if [ -f ./xzcat ]; then
        chmod +x ./xzcat
    fi
    
    for file in $(ls *.xz); do
        xzcat $file > ${file%.xz}
        chmod +x ${file%.xz}
    done
    cd ../
fi

if [ -f /bin/bash ]; then
    bash ./tool/VentoyWorker.sh $*
else
    ./tool/ash ./tool/VentoyWorker.sh $*
fi

if [ -n "$OLDDIR" ]; then 
    cd $OLDDIR
fi


