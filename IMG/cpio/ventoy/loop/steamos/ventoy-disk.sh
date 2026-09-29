#!/ventoy/busybox/sh
#************************************************************************************
# Copyright (c) 2026, longpanda <admin@ventoy.net>
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

# loop driver is not in the recovery initramfs, grub appended it from the image rootfs
for vtmod in $VTOY_PATH/modules/loop.ko*; do
    if [ -e "$vtmod" ]; then
        /usr/bin/insmod "$vtmod" max_part=31 >>$VTLOG 2>&1
        break
    fi
done

for vtmod in $VTOY_PATH/modules/ntfs3.ko*; do
    if [ -e "$vtmod" ]; then
        /usr/bin/insmod "$vtmod" >>$VTLOG 2>&1
        break
    fi
done

if ! [ -e /dev/loop-control ]; then
    vterr "loop driver could not be loaded"
    PATH=$VTPATH_OLD
    exit 0
fi

# systemd-udevd from the earlyhook misses uevents emitted before it started,
# and the udev coldplug replay only happens in run_hook after us. Replay it
# here if the ventoy disk is not already visible.
vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ] || ! check_usb_disk_ready "$vtdiskname"; then
    if [ -x /usr/bin/udevadm ]; then
        /usr/bin/udevadm trigger --action=add --type=subsystems >>$VTLOG 2>&1
        /usr/bin/udevadm trigger --action=add --type=devices >>$VTLOG 2>&1
        /usr/bin/udevadm settle --timeout=15 >>$VTLOG 2>&1
    fi
fi

vtloop=0
vtdiskready=0
while [ $vtloop -lt 60 ]; do
    vtdiskname=$(get_ventoy_disk_name)
    if [ "$vtdiskname" != "unknown" ] && check_usb_disk_ready "$vtdiskname"; then
        vtdiskready=1
        break
    fi
    let vtloop=vtloop+1
    $SLEEP 0.5
done

if [ $vtdiskready -eq 0 ]; then
    vterr "ventoy disk not found"
    $VTOY_PATH/tool/vtoydump -f $VTOY_PATH/ventoy_os_param -v >>$VTLOG 2>&1
    PATH=$VTPATH_OLD
    exit 0
fi

vtimgname=$(get_ventoy_iso_name)
if [ "$vtimgname" = "unknown" -o -z "$vtimgname" ]; then
    vterr "image file path not found"
    PATH=$VTPATH_OLD
    exit 0
fi

if echo "$vtdiskname" | $EGREP -q "nvme|mmc|nbd"; then
    vtdatapart=${vtdiskname}p1
else
    vtdatapart=${vtdiskname}1
fi

# rw: the holo hook mounts the image rootfs rw
$BUSYBOX_PATH/mkdir -p /run/ventoy-media
vtmounted=
for vtfs in exfat vfat ntfs3 ext4; do
    if $BUSYBOX_PATH/mount -t $vtfs "$vtdatapart" /run/ventoy-media >>$VTLOG 2>&1; then
        vtmounted=$vtfs
        vtlog "mounted $vtdatapart ($vtfs) at /run/ventoy-media"
        break
    fi
done

if [ -z "$vtmounted" ]; then
    vterr "failed to mount $vtdatapart (tried exfat vfat ntfs3 ext4)"
    PATH=$VTPATH_OLD
    exit 0
fi

vtimgfile="/run/ventoy-media${vtimgname}"
if ! [ -f "$vtimgfile" ]; then
    vterr "$vtimgfile not found on data partition"
    $BUSYBOX_PATH/umount /run/ventoy-media >>$VTLOG 2>&1
    PATH=$VTPATH_OLD
    exit 0
fi

vtloopdev=$($BUSYBOX_PATH/losetup -f)
if [ -z "$vtloopdev" ]; then
    vterr "no free loop device"
    PATH=$VTPATH_OLD
    exit 0
fi

if ! $BUSYBOX_PATH/losetup -P "$vtloopdev" "$vtimgfile" >>$VTLOG 2>&1; then
    vterr "losetup $vtloopdev $vtimgfile failed"
    PATH=$VTPATH_OLD
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
    $BUSYBOX_PATH/ln -s "${vtloopdev}p2" /dev/steamos-media-efi
    vtlog "/dev/steamos-media-efi -> ${vtloopdev}p2"
else
    vterr "${vtloopdev}p2 not created, partition scan failed"
fi

PATH=$VTPATH_OLD

set_ventoy_hook_finish
