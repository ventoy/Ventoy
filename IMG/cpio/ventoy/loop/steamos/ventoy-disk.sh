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

. /ventoy/hook/ventoy-hook-lib.sh

if is_ventoy_hook_finished; then
    exit 0
fi

vtlog "####### $0 $* ########"

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

wait_for_usb_disk_ready

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    exit 0
fi

/usr/bin/insmod  $VTOY_PATH/modules/dm-mod.ko*

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"
ventoy_create_dev_ventoy_part


mkdir -p /ventoy/tmpmnt
mount /dev/ventoy3 /ventoy/tmpmnt
mount --bind /ventoy/tmpmnt/lib/modules   /lib/modules

for mod in loop iso9660 ntfs ntfs3 exfat ext2 ext3 ext4 xfs btrfs; do
    vtlog "modprobe $mod"
    /usr/bin/modprobe $mod >> $VTLOG 2>&1
done

umount /lib/modules
umount /ventoy/tmpmnt

ventoy_remove_all_dm


vtimgname=$(get_ventoy_iso_name)
if [ "$vtimgname" = "unknown" -o -z "$vtimgname" ]; then
    vterr "image file path not found"
    exit 0
fi

if echo "$vtdiskname" | $EGREP -q "nvme|mmc|nbd"; then
    vtdatapart=${vtdiskname}p1
else
    vtdatapart=${vtdiskname}1
fi

# rw: the holo hook mounts the image rootfs rw
mkdir -p /run/ventoy-media

if mount "$vtdatapart" /run/ventoy-media >>$VTLOG 2>&1; then    
    vtlog "mounted $vtdatapart at /run/ventoy-media OK"
    mount | grep ventoy >>$VTLOG 2>&1    
else
    vtlog "mounted $vtdatapart at /run/ventoy-media failed"
    exit 0
fi


vtimgfile="/run/ventoy-media${vtimgname}"
if ! [ -f "$vtimgfile" ]; then
    vterr "$vtimgfile not found on data partition"
    umount /run/ventoy-media >>$VTLOG 2>&1
    exit 0
fi


vtloopdev=$(losetup -f)
if [ -z "$vtloopdev" ]; then
    vterr "no free loop device"
    exit 0
fi

if ! losetup -P "$vtloopdev" "$vtimgfile" >>$VTLOG 2>&1; then
    vterr "losetup $vtloopdev $vtimgfile failed"
    exit 0
fi
vtlog "attached $vtimgfile to $vtloopdev"

vtloop=0
while ! [ -e "${vtloopdev}p2" ]; do
    let vtloop=vtloop+1
    if [ $vtloop -gt 10 ]; then
        break
    fi
    $SLEEP 0.3
done

# exact /dev path avoids PARTLABEL/LABEL/UUID collisions with an installed SteamOS
if [ -e "${vtloopdev}p2" ]; then
    ln -s "${vtloopdev}p2" /dev/steamos-media-efi
    vtlog "/dev/steamos-media-efi -> ${vtloopdev}p2"
else
    vterr "${vtloopdev}p2 not created, partition scan failed"
fi

