#!/bin/bash

build_func() {    
    libsuffix=$2
    toolDir=$3
    
    XXFLAG='-std=gnu99 -D_FILE_OFFSET_BITS=64'
    XXLIB=""
    
    echo "CC=$1 libsuffix=$libsuffix toolDir=$toolDir"    
    
    $1 $XXFLAG -c -Wall -Wextra -Wshadow -Wformat-security -Winit-self \
        -Wmissing-prototypes -O2 -DLINUX \
        -I./Ventoy2Disk/Lib/libhttp/include \
        -DNDEBUG -DNO_CGI -DNO_CACHING -DNO_SSL -DSQLITE_DISABLE_LFS -DSSL_ALREADY_INITIALIZED \
        -DUSE_STACK_SIZE=102400 -DNDEBUG -fPIC \
        ./Ventoy2Disk/Lib/libhttp/include/civetweb.c \
        -o ./civetweb.o
    
    $1 $XXFLAG -O2 -Wall -Wno-unused-function -DSTATIC=static -DINIT= \
        -I./Ventoy2Disk \
        -I./Ventoy2Disk/Core \
        -I./Ventoy2Disk/Web \
        -I./Ventoy2Disk/Include \
        -I./Ventoy2Disk/Lib/libhttp/include \
        -I./Ventoy2Disk/Lib/fat_io_lib/include \
        -I./Ventoy2Disk/Lib/xz-embedded/linux/include \
        -I./Ventoy2Disk/Lib/xz-embedded/linux/include/linux \
        -I./Ventoy2Disk/Lib/xz-embedded/userspace \
        -I ./Ventoy2Disk/Lib/exfat/src/libexfat \
        -I ./Ventoy2Disk/Lib/exfat/src/mkfs \
        -I ./Ventoy2Disk/Lib/fat_io_lib \
        \
        -L ./Ventoy2Disk/Lib/fat_io_lib/lib \
        Ventoy2Disk/main_webui.c \
        Ventoy2Disk/Core/*.c \
        Ventoy2Disk/Web/*.c \
        Ventoy2Disk/Lib/xz-embedded/linux/lib/decompress_unxz.c \
        Ventoy2Disk/Lib/exfat/src/libexfat/*.c \
        Ventoy2Disk/Lib/exfat/src/mkfs/*.c \
        Ventoy2Disk/Lib/fat_io_lib/*.c \
        $XXLIB \
        -l pthread \
        ./civetweb.o \
        -o V2D$libsuffix

    rm -f *.o
    
    if [ "$libsuffix" = "aa64" ]; then
        aarch64-linux-gnu-strip V2D$libsuffix
    elif [ "$libsuffix" = "m64e" ]; then
        mips-linux-gnu-strip V2D$libsuffix
    else
        strip V2D$libsuffix
    fi
    
    rm -f ../INSTALL/tool/$toolDir/V2DServer
    cp -a V2D$libsuffix ../INSTALL/tool/$toolDir/V2DServer
}

build_func "gcc" '64' 'x86_64'
build_func "gcc -m32" '32' 'i386'
build_func "aarch64-linux-gnu-gcc" 'aa64' 'aarch64'
build_func "mips-linux-gnu-gcc -mips64r2 -mabi=64" 'm64e' 'mips64el'


