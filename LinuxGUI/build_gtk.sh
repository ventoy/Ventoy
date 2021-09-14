#!/bin/bash

build_func() {    
    libsuffix=$2
    toolDir=$3
    gtkver=$4   

    if [ "$libsuffix" = "aa64" ]; then
        EXD=./EXLIB/aarch64
        GTKFLAG="-pthread -I$EXD/usr/include/gtk-3.0 -I$EXD/usr/include/atk-1.0 -I$EXD/usr/include/at-spi2-atk/2.0 -I$EXD/usr/include/pango-1.0 -I$EXD/usr/include/gio-unix-2.0/ -I$EXD/usr/include/cairo -I$EXD/usr/include/gdk-pixbuf-2.0 -I$EXD/usr/include/glib-2.0 -I$EXD/usr/lib64/glib-2.0/include -I$EXD/usr/include/at-spi-2.0 -I$EXD/usr/include/dbus-1.0 -I$EXD/usr/lib64/dbus-1.0/include -I$EXD/usr/include/harfbuzz -I$EXD/usr/include/freetype2 -I$EXD/usr/include/pixman-1 -I$EXD/usr/include/libpng15 -I$EXD/usr/include/libdrm"
        XXLIB="-Wl,-rpath-link $EXD/usr/lib64 -Wl,-rpath-link $EXD/lib64 -Wno-deprecated-declarations -L$EXD/usr/lib64 -lgtk-3 -lgdk-3 -latk-1.0 -lgio-2.0 -lpangocairo-1.0 -lgdk_pixbuf-2.0 -lcairo-gobject -lpango-1.0 -lcairo -lgobject-2.0 -lglib-2.0 "
    elif [ "$libsuffix" = "m64e" ]; then
        EXDI=./EXLIB/mips64el
        EXDL=./EXLIB/mips64el/usr/lib/mips64el-linux-gnuabi64
        EXDL2=./EXLIB/mips64el/lib/mips64el-linux-gnuabi64
        GTKFLAG="-pthread -I$EXDI/usr/include/gtk-3.0 -I$EXDI/usr/include/at-spi2-atk/2.0 -I$EXDI/usr/include/at-spi-2.0 -I$EXDI/usr/include/dbus-1.0 -I./EXLIB/mips64el/usr/lib/x86_64-linux-gnu/dbus-1.0/include -I$EXDI/usr/include/gtk-3.0 -I$EXDI/usr/include/gio-unix-2.0 -I$EXDI/usr/include/cairo -I$EXDI/usr/include/pango-1.0 -I$EXDI/usr/include/harfbuzz -I$EXDI/usr/include/pango-1.0 -I$EXDI/usr/include/fribidi -I$EXDI/usr/include/harfbuzz -I$EXDI/usr/include/atk-1.0 -I$EXDI/usr/include/cairo -I$EXDI/usr/include/pixman-1 -I$EXDI/usr/include/uuid -I$EXDI/usr/include/freetype2 -I$EXDI/usr/include/libpng16 -I$EXDI/usr/include/gdk-pixbuf-2.0 -I$EXDI/usr/include/libmount -I$EXDI/usr/include/blkid -I$EXDI/usr/include/glib-2.0 -I$EXDL/glib-2.0/include"
        XXLIB="-Wl,-rpath-link $EXDL -Wl,-rpath-link $EXDL2 -Wno-deprecated-declarations -L$EXDL -L$EXDL2 -lm -lgtk-3 -lgdk-3 -lpangocairo-1.0 -lpango-1.0 -lharfbuzz -latk-1.0 -lcairo-gobject -lcairo -lgdk_pixbuf-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0 -lpcre "
    else
        if [ "$gtkver" = "gtk3" ]; then
            GTKFLAG=$(pkg-config --cflags --libs gtk+-3.0)
            GLADE=""
        else
            GTKFLAG=$(pkg-config --cflags --libs gtk+-2.0)
            GLADE=$(pkg-config --cflags --libs libglade-2.0)
        fi
        XXLIB=""
    fi
    
    XXFLAG="-std=gnu99 -D_FILE_OFFSET_BITS=64 $GTKFLAG $GLADE -Wall"
    
    
    echo "CC=$1 libsuffix=$libsuffix toolDir=$toolDir $gtkver"    
    
    $1 $XXFLAG -c -Wall -Wextra -Wshadow -Wformat-security -Winit-self \
        -Wmissing-prototypes -O2 -DLINUX \
        -I./Ventoy2Disk/Lib/libhttp/include \
        -DNDEBUG -DNO_CGI -DNO_CACHING -DNO_SSL -DSQLITE_DISABLE_LFS -DSSL_ALREADY_INITIALIZED \
        -DUSE_STACK_SIZE=102400 -DNDEBUG -fPIC \
        ./Ventoy2Disk/Lib/libhttp/include/civetweb.c \
        -o ./civetweb.o
    
    $1 -O2 -Wall -Wno-unused-function -DSTATIC=static -DINIT= \
        -I./Ventoy2Disk \
        -I./Ventoy2Disk/Core \
        -I./Ventoy2Disk/Web \
        -I./Ventoy2Disk/GTK \
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
        Ventoy2Disk/main_gtk.c \
        Ventoy2Disk/Core/*.c \
        Ventoy2Disk/Web/*.c \
        Ventoy2Disk/GTK/*.c \
        Ventoy2Disk/Lib/xz-embedded/linux/lib/decompress_unxz.c \
        Ventoy2Disk/Lib/exfat/src/libexfat/*.c \
        Ventoy2Disk/Lib/exfat/src/mkfs/*.c \
        Ventoy2Disk/Lib/fat_io_lib/*.c \
        $XXLIB \
        -l pthread \
        ./civetweb.o \
        -o Ventoy2Disk.${gtkver}_$libsuffix $XXFLAG 

    rm -f *.o
    
    if [ "$libsuffix" = "aa64" ]; then
        aarch64-linux-gnu-strip Ventoy2Disk.${gtkver}_$libsuffix
    elif [ "$libsuffix" = "m64e" ]; then
        mips-linux-gnu-strip Ventoy2Disk.${gtkver}_$libsuffix
    else
        strip Ventoy2Disk.${gtkver}_$libsuffix
    fi
    
    rm -f ../INSTALL/tool/$toolDir/Ventoy2Disk.${gtkver}_$libsuffix
    cp -a Ventoy2Disk.${gtkver}_$libsuffix ../INSTALL/tool/$toolDir/Ventoy2Disk.${gtkver}
    
    $1 -O2 -D_FILE_OFFSET_BITS=64 Ventoy2Disk/ventoy_gui.c Ventoy2Disk/Core/ventoy_json.c -I Ventoy2Disk/Core  -DVTOY_GUI_ARCH="\"$toolDir\"" -o VentoyGUI.$toolDir
    cp -a VentoyGUI.$toolDir ../INSTALL/
}


build_func "gcc" '64' 'x86_64' 'gtk2'
build_func "gcc" '64' 'x86_64' 'gtk3'

build_func "gcc -m32" '32' 'i386' 'gtk2'
build_func "gcc -m32" '32' 'i386' 'gtk3'

build_func "aarch64-linux-gnu-gcc" 'aa64' 'aarch64' 'gtk3'

export PATH=/opt/mips-loongson-gcc8-linux-gnu-2021-02-08/bin/:$PATH
build_func "mips-linux-gnu-gcc -mips64r2 -mabi=64" 'm64e' 'mips64el' 'gtk3'

