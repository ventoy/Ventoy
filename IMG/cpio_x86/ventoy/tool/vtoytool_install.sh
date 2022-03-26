#!/ventoy/busybox/sh
#************************************************************************************
# Copyright (c) 2020, longpanda <admin@ventoy.net>
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 3 of the
# License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
# 
#************************************************************************************

echo "#### install vtoytool #####" >> $VTLOG

for app in ar inotifyd; do
    if [ -e $BUSYBOX_PATH/$app ]; then
        $BUSYBOX_PATH/rm -f $VTOY_PATH/tool/$app
        $BUSYBOX_PATH/ln -s $BUSYBOX_PATH/$app $VTOY_PATH/tool/$app
    else
        $BUSYBOX_PATH/ln -s $VTOY_PATH/tool/$app $BUSYBOX_PATH/$app
    fi
done


if $GREP -q aarch64 $VTOY_PATH/ventoy_arch; then
    for vtdir in $(ls $VTOY_PATH/tool/vtoytool/); do
        echo "try $VTOY_PATH/tool/vtoytool/$vtdir/ ..." >> $VTLOG
        if $VTOY_PATH/tool/vtoytool/$vtdir/vtoytool_aa64 --install 2>>$VTLOG; then
            echo "vtoytool_aa64 OK" >> $VTLOG
            break
        fi        
    done
    
    $BUSYBOX_PATH/cp -a $VTOY_PATH/tool/vtoy_fuse_iso_aa64  $VTOY_PATH/tool/vtoy_fuse_iso
    $BUSYBOX_PATH/cp -a $VTOY_PATH/tool/unsquashfs_aa64  $VTOY_PATH/tool/vtoy_unsquashfs
elif $GREP -q mips64el $VTOY_PATH/ventoy_arch; then
    for vtdir in $(ls $VTOY_PATH/tool/vtoytool/); do
        echo "try $VTOY_PATH/tool/vtoytool/$vtdir/ ..." >> $VTLOG
        if $VTOY_PATH/tool/vtoytool/$vtdir/vtoytool_m64e --install 2>>$VTLOG; then
            echo "vtoytool_m64e OK" >> $VTLOG
            break
        fi        
    done
    
    # TBD
    #$BUSYBOX_PATH/cp -a $VTOY_PATH/tool/vtoy_fuse_iso_aa64  $VTOY_PATH/tool/vtoy_fuse_iso
    #$BUSYBOX_PATH/cp -a $VTOY_PATH/tool/unsquashfs_aa64  $VTOY_PATH/tool/vtoy_unsquashfs    
else
    for vtdir in $(ls $VTOY_PATH/tool/vtoytool/); do
        echo "try $VTOY_PATH/tool/vtoytool/$vtdir/ ..." >> $VTLOG
        if $VTOY_PATH/tool/vtoytool/$vtdir/vtoytool_64 --install 2>>$VTLOG; then
            echo "vtoytool_64 OK" >> $VTLOG
            break
        fi
        
        if $VTOY_PATH/tool/vtoytool/$vtdir/vtoytool_32 --install 2>>$VTLOG; then
            echo "vtoytool_32 OK" >> $VTLOG
            break
        fi
    done
    
    if $VTOY_PATH/tool/vtoy_fuse_iso_64 -t 2>>$VTLOG; then
        echo "use vtoy_fuse_iso_64" >>$VTLOG
        $BUSYBOX_PATH/cp -a $VTOY_PATH/tool/vtoy_fuse_iso_64  $VTOY_PATH/tool/vtoy_fuse_iso
    else
        echo "use vtoy_fuse_iso_32" >>$VTLOG    
        $BUSYBOX_PATH/cp -a $VTOY_PATH/tool/vtoy_fuse_iso_32 $VTOY_PATH/tool/vtoy_fuse_iso
    fi

    if $VTOY_PATH/tool/unsquashfs_64 -t 2>>$VTLOG; then
        echo "use unsquashfs_64" >>$VTLOG
        $BUSYBOX_PATH/cp -a $VTOY_PATH/tool/unsquashfs_64  $VTOY_PATH/tool/vtoy_unsquashfs
    else
        echo "use unsquashfs_32" >>$VTLOG    
        $BUSYBOX_PATH/cp -a $VTOY_PATH/tool/unsquashfs_32 $VTOY_PATH/tool/vtoy_unsquashfs
    fi

    if $GREP -q 64 $VTOY_PATH/ventoy_arch; then
        echo "use veritysetup64" >>$VTLOG    
        if [ -f $VTOY_PATH/tool/veritysetup64 ]; then
            $BUSYBOX_PATH/cp -a $VTOY_PATH/tool/veritysetup64  $VTOY_PATH/tool/veritysetup
        fi
    else
        echo "use veritysetup32" >>$VTLOG    
        
        if [ -f $VTOY_PATH/tool/veritysetup32 ]; then
            $BUSYBOX_PATH/cp -a $VTOY_PATH/tool/veritysetup32  $VTOY_PATH/tool/veritysetup
        fi
    fi
fi

