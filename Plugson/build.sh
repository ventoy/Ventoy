#!/bin/bash

if [ "$1" = "sim" ]; then
    exopt="-DVENTOY_SIM"
fi

build_func() {    
    libsuffix=$2
    toolDir=$3
    
    XXFLAG='-std=gnu99 -D_FILE_OFFSET_BITS=64'
    XXLIB=""
    
    echo "CC=$1 libsuffix=$libsuffix toolDir=$toolDir"    
    
    echo "CC civetweb.o"
    $1 $XXFLAG -c -Wall -Wextra -Wshadow -Wformat-security -Winit-self \
        -Wmissing-prototypes -O2 -DLINUX \
        -I./src/Lib/libhttp/include \
        -DNDEBUG -DNO_CGI -DNO_CACHING -DNO_SSL -DSQLITE_DISABLE_LFS -DSSL_ALREADY_INITIALIZED \
        -DUSE_STACK_SIZE=102400 -DNDEBUG -fPIC \
        ./src/Lib/libhttp/include/civetweb.c \
        -o ./civetweb.o

    echo "CC plugson.o"
    $1 $XXFLAG -O2 $exopt -Wall -Wno-unused-function -DSTATIC=static -DINIT= \
        -I./src \
        -I./src/Core \
        -I./src/Web \
        -I./src/Include \
        -I./src/Lib/libhttp/include \
        -I./src/Lib/fat_io_lib/include \
        -I./src/Lib/xz-embedded/linux/include \
        -I./src/Lib/xz-embedded/linux/include/linux \
        -I./src/Lib/xz-embedded/userspace \
        -I ./src/Lib/exfat/src/libexfat \
        -I ./src/Lib/exfat/src/mkfs \
        -I ./src/Lib/fat_io_lib \
        \
        -L ./src/Lib/fat_io_lib/lib \
        src/main_linux.c \
        src/Core/ventoy_crc32.c \
        src/Core/ventoy_disk.c \
        src/Core/ventoy_disk_linux.c \
        src/Core/ventoy_json.c \
        src/Core/ventoy_log.c \
        src/Core/ventoy_md5.c \
        src/Core/ventoy_util.c \
        src/Core/ventoy_util_linux.c \
        src/Web/*.c \
        src/Lib/xz-embedded/linux/lib/decompress_unxz.c \
        src/Lib/fat_io_lib/*.c \
        $XXLIB \
        -l pthread \
        ./civetweb.o \
        -o Plugson$libsuffix

    rm -f *.o
    
    if [ "$libsuffix" = "aa64" ]; then
        aarch64-linux-gnu-strip Plugson$libsuffix
    elif [ "$libsuffix" = "m64e" ]; then
        mips-linux-gnu-strip Plugson$libsuffix
    else
        strip Plugson$libsuffix
    fi
    
    rm -f ../INSTALL/tool/$toolDir/Plugson
    cp -a Plugson$libsuffix ../INSTALL/tool/$toolDir/Plugson
    
}

build_func "gcc" '64' 'x86_64'

build_func "gcc -m32" '32' 'i386'
build_func "aarch64-linux-gnu-gcc" 'aa64' 'aarch64'
build_func "mips-linux-gnu-gcc -mips64r2 -mabi=64" 'm64e' 'mips64el'

