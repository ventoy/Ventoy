#!/bin/bash

force_copy() {
    [ -e "$2" ] && rm -f "$2"
    cp -a "$1" "$2"
}

compile_file() {
    name=$(basename $2)
    obj=${name%.*}
    
    echo "$1 ${obj}.o ..."
    $1 -O2 -Wall -std=gnu99 -Wno-unused-function -Wno-format-truncation -Wno-address-of-packed-member -DSTATIC=static -DINIT= -D_FILE_OFFSET_BITS=64 \
        -I./Ventoy2Disk \
        -I./Ventoy2Disk/Core \
        -I./Ventoy2Disk/Web \
        -I./Ventoy2Disk/QT \
        -I./Ventoy2Disk/Include \
        -I./Ventoy2Disk/Lib/libhttp/include \
        -I./Ventoy2Disk/Lib/fat_io_lib/include \
        -I./Ventoy2Disk/Lib/xz-embedded/linux/include \
        -I./Ventoy2Disk/Lib/xz-embedded/linux/include/linux \
        -I./Ventoy2Disk/Lib/xz-embedded/userspace \
        -I ./Ventoy2Disk/Lib/exfat/src/libexfat \
        -I ./Ventoy2Disk/Lib/exfat/src/mkfs \
        -I ./Ventoy2Disk/Lib/fat_io_lib \
        -c $2 -o ${obj}.o
}

compile_dir() {
    dir=$1  
    for i in $(ls $dir/*.c); do
        compile_file "$2" "$i"
    done
}

compile_lib() {
    compile_dir ./Ventoy2Disk/Core "$1"
    compile_dir ./Ventoy2Disk/Web  "$1"
    compile_dir ./Ventoy2Disk/QT   "$1"
    compile_dir ./Ventoy2Disk/Lib/exfat/src/libexfat  "$1"
    compile_dir ./Ventoy2Disk/Lib/exfat/src/mkfs  "$1"
    compile_dir ./Ventoy2Disk/Lib/fat_io_lib  "$1"
    compile_file "$1" Ventoy2Disk/Lib/xz-embedded/linux/lib/decompress_unxz.c

    rm -f libVentoyQT_$3.a
    $2 -rcs libVentoyQT_$3.a *.o
    rm -f *.o
}

build_qt() {
    echo "$1 main.o ..."
    $1 -c $2 $3 -o main.o ./Ventoy2Disk/QT/main.cpp
    
    echo "$1 partcfgdialog.o ..."
    $1 -c $2 $3 -o partcfgdialog.o ./Ventoy2Disk/QT/partcfgdialog.cpp
    
    echo "$1 ventoy2diskwindow.o ..."
    $1 -c $2 $3 -o ventoy2diskwindow.o ./Ventoy2Disk/QT/ventoy2diskwindow.cpp
    
    echo "$1 moc_partcfgdialog.o ..."
    $1 -c $2 $3 -o moc_partcfgdialog.o ./Ventoy2Disk/QT/build/moc_partcfgdialog.cpp
    
    echo "$1 moc_ventoy2diskwindow.o ..."
    $1 -c $2 $3 -o moc_ventoy2diskwindow.o ./Ventoy2Disk/QT/build/moc_ventoy2diskwindow.cpp
    
    echo "$1 Ventoy2Disk.qt5_${6} ..."
    $1 $4 -o Ventoy2Disk.qt5_${6} *.o $5
    rm -f *.o    
}

build_qt_app() {
    
    DEFINES="-DQT_CHECK_EUID -DQT_DEPRECATED_WARNINGS -DSTATIC=static -DINIT= -DQT_NO_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB"
    CXXFLAGS="-pipe -O2 -std=gnu++11 -Wall -W -D_REENTRANT -fPIC $DEFINES -Wno-deprecated-declarations -Wno-deprecated-copy"
    INCPATH=" -I./Ventoy2Disk -I. -I./Ventoy2Disk/Core -I./Ventoy2Disk/Web -I./Ventoy2Disk/QT -I./Ventoy2Disk/QT/build -I./Ventoy2Disk/Include -I./Ventoy2Disk/Lib/libhttp/include -I./Ventoy2Disk/Lib/fat_io_lib/include -I./Ventoy2Disk/Lib/xz-embedded/linux/include -I./Ventoy2Disk/Lib/xz-embedded/linux/include/linux -I./Ventoy2Disk/Lib/xz-embedded/userspace -I./Ventoy2Disk/Lib/exfat/src/libexfat -I./Ventoy2Disk/Lib/fat_io_lib -I$QT_INC_PATH -I$QT_INC_PATH/QtWidgets -I$QT_INC_PATH/QtGui -I$QT_INC_PATH/QtCore -I. -I."

    SUBLIBS="./libVentoyQT_${2}.a"
    LIBS="$SUBLIBS $QT_LIB_PATH/libQt5Widgets.so $QT_LIB_PATH/libQt5Gui.so $QT_LIB_PATH/libQt5Core.so -lpthread"
    
    build_qt "$1" "$CXXFLAGS" "$INCPATH" "$LFLAGS" "$LIBS" "$3"
}

# build QT5 for i386
build_qt_i386() {
    QT_INC_PATH="./EXLIB/i386/usr/include/i386-linux-gnu/qt5"
    QT_LIB_PATH="./EXLIB/i386/usr/lib/i386-linux-gnu"
    DEFINES="-DQT_CHECK_EUID -DQT_DEPRECATED_WARNINGS -DSTATIC=static -DINIT= -DQT_NO_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB"
    CXXFLAGS="-pipe -O2 -std=gnu++11 -Wall -W -D_REENTRANT -fPIC $DEFINES -Wno-deprecated-declarations"
    INCPATH=" -I./Ventoy2Disk -I. -I./Ventoy2Disk/Core -I./Ventoy2Disk/Web -I./Ventoy2Disk/QT -I./Ventoy2Disk/QT/build -I./Ventoy2Disk/Include -I./Ventoy2Disk/Lib/libhttp/include -I./Ventoy2Disk/Lib/fat_io_lib/include -I./Ventoy2Disk/Lib/xz-embedded/linux/include -I./Ventoy2Disk/Lib/xz-embedded/linux/include/linux -I./Ventoy2Disk/Lib/xz-embedded/userspace -I./Ventoy2Disk/Lib/exfat/src/libexfat -I./Ventoy2Disk/Lib/fat_io_lib -I$QT_INC_PATH -I$QT_INC_PATH/QtWidgets -I$QT_INC_PATH/QtGui -I$QT_INC_PATH/QtCore -I."
    LFLAGS="-Wl,-O1 -Wl,-rpath-link,$QT_LIB_PATH  -L$QT_LIB_PATH -Wl,-rpath-link,./EXLIB/i386/lib/i386-linux-gnu -L./EXLIB/i386/lib/i386-linux-gnu"
    SUBLIBS="./libVentoyQT_i386.a"
    LIBS="$SUBLIBS $QT_LIB_PATH/libQt5Widgets.so $QT_LIB_PATH/libQt5Gui.so $QT_LIB_PATH/libQt5Core.so -lpthread"

    compile_lib "gcc -m32" "ar" "i386"
    build_qt "g++ -m32" "$CXXFLAGS" "$INCPATH" "$LFLAGS" "$LIBS" "32"
}


#build QT5 for x86_64
build_qt_x86_64() {
    compile_lib "gcc" "ar" "x86_64"
    QT_INC_PATH="/opt/Qt5.9.0/5.9/gcc_64/include"
    QT_LIB_PATH="/opt/Qt5.9.0/5.9/gcc_64/lib"
    LFLAGS="-Wl,-O1 -Wl,-rpath-link,$QT_LIB_PATH -L$QT_LIB_PATH"
    build_qt_app "g++"   "x86_64" "64"
    
    force_copy Ventoy2Disk.qt5_64 ../INSTALL/tool/x86_64/Ventoy2Disk.qt5
    rm -f ./libVentoyQT_x86_64.a
}


# build QT5 for arm64
build_qt_aarch64() {
    compile_lib "aarch64-linux-gnu-gcc" "aarch64-linux-gnu-ar" "aarch64"
    QT_INC_PATH="./EXLIB/aarch64/usr/include/qt5"
    QT_LIB_PATH="./EXLIB/aarch64/usr/lib64"
    LFLAGS="-Wl,-O1 -Wl,-rpath-link,$QT_LIB_PATH -L$QT_LIB_PATH -Wl,-rpath-link,./EXLIB/aarch64/lib64 -L./EXLIB/aarch64/lib64"
    build_qt_app "aarch64-linux-gnu-g++" "aarch64" "aa64"
    
    force_copy Ventoy2Disk.qt5_aa64 ../INSTALL/tool/aarch64/Ventoy2Disk.qt5
    rm -f ./libVentoyQT_aarch64.a
}

# build QT5 for mips64
build_qt_mips64el() {
    QT_INC_PATH="./EXLIB/mips64el/usr/include/mips64el-linux-gnuabi64/qt5"
    QT_LIB_PATH="./EXLIB/mips64el/usr/lib/mips64el-linux-gnuabi64"
    DEFINES="-DQT_CHECK_EUID -DQT_DEPRECATED_WARNINGS -DSTATIC=static -DINIT= -DQT_NO_DEBUG -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB"
    CXXFLAGS="-pipe -O2 -std=gnu++11 -Wall -W -D_REENTRANT -fPIC $DEFINES -Wno-deprecated-declarations"
    INCPATH=" -I./Ventoy2Disk -I. -I./Ventoy2Disk/Core -I./Ventoy2Disk/Web -I./Ventoy2Disk/QT -I./Ventoy2Disk/QT/build -I./Ventoy2Disk/Include -I./Ventoy2Disk/Lib/libhttp/include -I./Ventoy2Disk/Lib/fat_io_lib/include -I./Ventoy2Disk/Lib/xz-embedded/linux/include -I./Ventoy2Disk/Lib/xz-embedded/linux/include/linux -I./Ventoy2Disk/Lib/xz-embedded/userspace -I./Ventoy2Disk/Lib/exfat/src/libexfat -I./Ventoy2Disk/Lib/fat_io_lib -I$QT_INC_PATH -I$QT_INC_PATH/QtWidgets -I$QT_INC_PATH/QtGui -I$QT_INC_PATH/QtCore -I."
    LFLAGS="-Wl,-O1 -Wl,-rpath-link,$QT_LIB_PATH  -L$QT_LIB_PATH -Wl,-rpath-link,./EXLIB/mips64el/lib/mips64el-linux-gnuabi64 -L./EXLIB/mips64el/lib/mips64el-linux-gnuabi64"
    SUBLIBS="./libVentoyQT_mips64el.a"
    LIBS="$SUBLIBS $QT_LIB_PATH/libQt5Widgets.so $QT_LIB_PATH/libQt5Gui.so $QT_LIB_PATH/libQt5Core.so -lpthread"

    export PATH=/opt/mips-loongson-gcc8-linux-gnu-2021-02-08/bin/:$PATH
    compile_lib "mips-linux-gnu-gcc -mips64r2 -mabi=64" "mips-linux-gnu-ar" "mips64el"
    build_qt "mips-linux-gnu-g++ -mips64r2 -mabi=64" "$CXXFLAGS" "$INCPATH" "$LFLAGS" "$LIBS" "m64e"

    force_copy Ventoy2Disk.qt5_m64e ../INSTALL/tool/mips64el/Ventoy2Disk.qt5
    rm -f ./libVentoyQT_mips64el.a
}


####################################################################
####################################################################
####################################################################
####################################################################

sed "s#../Ventoy2Disk#..#g" -i ./Ventoy2Disk/QT/build/moc_partcfgdialog.cpp
sed "s#../Ventoy2Disk#..#g" -i ./Ventoy2Disk/QT/build/moc_ventoy2diskwindow.cpp

#build qt5 i386 in centos 8 environment
if [ "$1" = "VENTOY_I386_QT_BUILD" ]; then
    echo "build i386 qt ..."
    build_qt_i386
    exit 0
fi

if [ ! -f /opt/CentOS8/LinuxGUI/build.sh ]; then
    mount --bind /home/share/Ventoy/LinuxGUI /opt/CentOS8/LinuxGUI
fi

chroot /opt/CentOS8 sh /buildqt.sh
force_copy ./Ventoy2Disk.qt5_32 ../INSTALL/tool/i386/Ventoy2Disk.qt5
rm -f ./libVentoyQT_i386.a

build_qt_x86_64
build_qt_aarch64
build_qt_mips64el
