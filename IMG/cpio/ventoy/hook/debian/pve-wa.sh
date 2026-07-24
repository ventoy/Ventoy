#!/bin/sh

script=$1
tmpscript=${script}.tmp

LINE=$(grep -n 'proc.*proc.*proc' $script | awk -F':' '{print $1}')

sed -n "1,${LINE}p" $script >> $tmpscript

echo "if grep -q 'rdinit=/vtoy/vtoy' /proc/cmdline; then"    >> $tmpscript
echo "    sed 's#rdinit=/vtoy/vtoy##g' /proc/cmdline > /etc/cmdline"    >> $tmpscript
echo "    mount --bind /etc/cmdline  /proc/cmdline"    >> $tmpscript
echo "fi"    >> $tmpscript


let LINE++
sed -n "${LINE},\$p" $script >> $tmpscript

cat $tmpscript > $script

