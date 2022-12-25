#!/bin/sh

OLDDIR=$(pwd)

if ! [ -f ./tool/ventoy_lib.sh ]; then
    if [ -f ${0%Ventoy2Disk.sh}/tool/ventoy_lib.sh ]; then
        cd ${0%Ventoy2Disk.sh}    
    fi
fi

if [ -f ./ventoy/version ]; then
    curver=$(cat ./ventoy/version) 
fi

if uname -m | grep -E -q 'aarch64|arm64'; then
    export TOOLDIR=aarch64
elif uname -m | grep -E -q 'x86_64|amd64'; then
    export TOOLDIR=x86_64
elif uname -m | grep -E -q 'mips64'; then
    export TOOLDIR=mips64el
else
    export TOOLDIR=i386
fi
export PATH="./tool/$TOOLDIR:$PATH"


echo ''
echo '**********************************************'
echo "      Ventoy: $curver  $TOOLDIR"
echo "      longpanda admin@ventoy.net"
echo "      https://www.ventoy.net"
echo '**********************************************'
echo ''


if ! [ -f ./boot/boot.img ]; then
    if [ -d ./grub ]; then
        echo "Don't run Ventoy2Disk.sh here, please download the released install package, and run the script in it."
    else
        echo "Please run under the correct directory!" 
    fi
    exit 1
fi

echo "############# Ventoy2Disk $* [$TOOLDIR] ################" >> ./log.txt
date >> ./log.txt

#decompress tool
echo "decompress tools" >> ./log.txt
cd ./tool/$TOOLDIR

ls *.xz > /dev/null 2>&1
if [ $? -eq 0 ]; then
    [ -f ./xzcat ] && chmod +x ./xzcat

    for file in $(ls *.xz); do
        echo "decompress $file" >> ./log.txt
        xzcat $file > ${file%.xz}
        [ -f ./${file%.xz} ] && chmod +x ./${file%.xz}
        [ -f ./$file ] && rm -f ./$file
    done
fi

cd ../../
chmod +x -R ./tool/$TOOLDIR


if [ -f /bin/bash ]; then
    /bin/bash ./tool/VentoyWorker.sh $*
else
    ash ./tool/VentoyWorker.sh $*
fi

if [ -n "$OLDDIR" ]; then 
    CURDIR=$(pwd)
    if [ "$CURDIR" != "$OLDDIR" ]; then
        cd "$OLDDIR"
    fi
fi
