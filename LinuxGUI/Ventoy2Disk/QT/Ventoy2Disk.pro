QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    Core/ventoy_crc32.c \
    Core/ventoy_disk.c \
    Core/ventoy_json.c \
    Core/ventoy_log.c \
    Core/ventoy_md5.c \
    Core/ventoy_util.c \
    Lib/exfat/src/libexfat/cluster.c \
    Lib/exfat/src/libexfat/io.c \
    Lib/exfat/src/libexfat/lookup.c \
    Lib/exfat/src/libexfat/mount.c \
    Lib/exfat/src/libexfat/node.c \
    Lib/exfat/src/libexfat/repair.c \
    Lib/exfat/src/libexfat/time.c \
    Lib/exfat/src/libexfat/utf.c \
    Lib/exfat/src/libexfat/utils.c \
    Lib/exfat/src/mkfs/cbm.c \
    Lib/exfat/src/mkfs/fat.c \
    Lib/exfat/src/mkfs/mkexfat.c \
    Lib/exfat/src/mkfs/mkexfat_main.c \
    Lib/exfat/src/mkfs/rootdir.c \
    Lib/exfat/src/mkfs/uct.c \
    Lib/exfat/src/mkfs/uctc.c \
    Lib/exfat/src/mkfs/vbr.c \
    Lib/fat_io_lib/fat_access.c \
    Lib/fat_io_lib/fat_cache.c \
    Lib/fat_io_lib/fat_filelib.c \
    Lib/fat_io_lib/fat_format.c \
    Lib/fat_io_lib/fat_misc.c \
    Lib/fat_io_lib/fat_string.c \
    Lib/fat_io_lib/fat_table.c \
    Lib/fat_io_lib/fat_write.c \
    Lib/xz-embedded/linux/lib/decompress_unxz.c \
    QT/refresh_icon_data.c \
    QT/secure_icon_data.c \
    QT/ventoy_qt_stub.c \
    Web/ventoy_http.c \
    main.cpp \
    partcfgdialog.cpp \
    ventoy2diskwindow.cpp

HEADERS += \
    Core/ventoy_define.h \
    Core/ventoy_disk.h \
    Core/ventoy_json.h \
    Core/ventoy_util.h \
    Include/Ventoy2Disk.h \
    Lib/exfat/src/libexfat/byteorder.h \
    Lib/exfat/src/libexfat/compiler.h \
    Lib/exfat/src/libexfat/config.h \
    Lib/exfat/src/libexfat/exfat.h \
    Lib/exfat/src/libexfat/exfatfs.h \
    Lib/exfat/src/libexfat/platform.h \
    Lib/exfat/src/mkfs/cbm.h \
    Lib/exfat/src/mkfs/fat.h \
    Lib/exfat/src/mkfs/mkexfat.h \
    Lib/exfat/src/mkfs/rootdir.h \
    Lib/exfat/src/mkfs/uct.h \
    Lib/exfat/src/mkfs/uctc.h \
    Lib/exfat/src/mkfs/vbr.h \
    Lib/fat_io_lib/fat_access.h \
    Lib/fat_io_lib/fat_cache.h \
    Lib/fat_io_lib/fat_defs.h \
    Lib/fat_io_lib/fat_filelib.h \
    Lib/fat_io_lib/fat_format.h \
    Lib/fat_io_lib/fat_list.h \
    Lib/fat_io_lib/fat_misc.h \
    Lib/fat_io_lib/fat_opts.h \
    Lib/fat_io_lib/fat_string.h \
    Lib/fat_io_lib/fat_table.h \
    Lib/fat_io_lib/fat_types.h \
    Lib/fat_io_lib/fat_write.h \
    Lib/libhttp/include/civetweb.h \
    Lib/libhttp/include/handle_form.inl \
    Lib/libhttp/include/md5.inl \
    Lib/libhttp/include/mod_duktape.inl \
    Lib/libhttp/include/mod_lua.inl \
    Lib/libhttp/include/timer.inl \
    QT/ventoy_qt.h \
    Web/ventoy_http.h \
    partcfgdialog.h \
    ventoy2diskwindow.h

FORMS += \
    partcfgdialog.ui \
    ventoy2diskwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    Lib/fat_io_lib/API.txt \
    Lib/fat_io_lib/COPYRIGHT.txt \
    Lib/fat_io_lib/Configuration.txt \
    Lib/fat_io_lib/History.txt \
    Lib/fat_io_lib/License.txt \
    Lib/fat_io_lib/Media Access API.txt \
    Lib/fat_io_lib/version.txt


INCLUDEPATH +=/home/panda/Ventoy2Disk/Core
INCLUDEPATH +=/home/panda/Ventoy2Disk/Web
INCLUDEPATH +=/home/panda/Ventoy2Disk/QT
INCLUDEPATH +=/home/panda/Ventoy2Disk/Include
INCLUDEPATH +=/home/panda/Ventoy2Disk/Lib/libhttp/include
INCLUDEPATH +=/home/panda/Ventoy2Disk/Lib/fat_io_lib/include
INCLUDEPATH +=/home/panda/Ventoy2Disk/Lib/xz-embedded/linux/include
INCLUDEPATH +=/home/panda/Ventoy2Disk/Lib/xz-embedded/linux/include/linux
INCLUDEPATH +=/home/panda/Ventoy2Disk/Lib/xz-embedded/userspace
INCLUDEPATH +=/home/panda/Ventoy2Disk/Lib/exfat/src/libexfat
INCLUDEPATH +=/home/panda/Ventoy2Disk/Lib/fat_io_lib
#INCLUDEPATH +=/usr/src/linux-headers-5.10.18-amd64-desktop/include
#INCLUDEPATH +=/usr/src/linux-headers-5.10.18-amd64-desktop/arch/x86/include
#INCLUDEPATH +=/usr/src/linux-headers-5.10.18-amd64-desktop/arch/x86/include/generated


DEFINES += STATIC=static
DEFINES += INIT=


