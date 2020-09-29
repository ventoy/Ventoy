#!/bin/sh

info() {
    echo -e "\033[32m$*\033[0m"
}

warn() {
    echo -e "\033[33m$*\033[0m"
}

err() {
    echo -e "\033[31m$*\033[0m"
}

get_disk_size() {
    sec=$(cat /sys/block/$1/size)
    /ventoy/disksize $sec
}

enum_disk() {
    id=1
    rm -f /device.list
    ls /sys/block/ | egrep 'd[a-z]|nvme|mmc' | while read dev; do
        if ! [ -b /dev/$dev ]; then
            continue
        fi

        size=$(get_disk_size $dev)
        model=$(parted -s /dev/$dev p 2>/dev/null | grep Model | sed 's/Model: \(.*\)/\1/')
        printf " <%d> %-4s  %3s GB  %s\r\n" $id "$dev" "$size" "$model" >> /device.list
        id=$(expr $id + 1)
    done
}

select_disk() {
    
    echo "" > /dev/console
    echo "" > /dev/console
    
    if [ -f /device.list ]; then
        lines=$(cat /device.list | wc -l)
        cat /device.list > /dev/console
    else
        echo -e "\033[31m !!! NO device detected !!!\033[0m" > /dev/console
        lines=0
    fi
    
    echo "" > /dev/console
    echo " <a> Refresh device list  <b> Reboot  <c> Enter shell" > /dev/console
    echo "" > /dev/console
    
    while true; do        
        
        if [ $lines -gt 0 ]; then
            read -p "Please select the disk to operator [1-$lines] "  Answer
        else
            read -p "Please choose your operation [a-c] "  Answer
        fi
        
        if [ "$Answer" = "shell" ]; then
            echo 8888; return
        elif [ "$Answer" = "c" ] || [ "$Answer" = "C" ]; then
            echo 8888; return
        fi
        
        if [ "$Answer" = "a" ] || [ "$Answer" = "A" ]; then
            echo 0; return
        elif [ "$Answer" = "b" ] || [ "$Answer" = "B" ]; then
            read -p "Do you really want to reboot? (y/n) "  Ask
            if [ "$Ask" = "y" ] || [ "$Ask" = "Y" ]; then
                reboot
            else
                continue
            fi
        fi

        if [ -n "$Answer" ]; then
            if echo $Answer | grep -q "^[1-9][0-9]*$"; then
                if [ $Answer -gt 0 ] && [ $Answer -le $lines ]; then
                    echo $Answer
                    return
                fi
            fi
        fi
    done
}

get_dev_ventoy_ver() {
    if ! [ -b /dev/${1}2 ]; then
        echo "NO"; return
    fi
    
    mount -t vfat -o ro /dev/${1}2 /ventoy/mnt >/dev/null 2>/dev/null
    if [ -e /ventoy/mnt/ventoy ] && [ -f /ventoy/mnt/grub/grub.cfg ]; then
        if grep -q 'set.*VENTOY_VERSION=' /ventoy/mnt/grub/grub.cfg; then
            grep 'set.*VENTOY_VERSION=' /ventoy/mnt/grub/grub.cfg | awk -F'"' '{print $2}'
        else
            echo 'NO'
        fi
        
        umount /ventoy/mnt
        return
    fi
    
    echo "NO"
}

ventoy_configuration() {
    while true; do
    
        if [ -f /preserve.txt ]; then
            SPACE=$(cat /preserve.txt)
        else
            SPACE=0
        fi
        
        if [ -f /secureboot.txt ]; then
            SECURE=$(cat /secureboot.txt)
        else
            SECURE=Disable
        fi
    
        if [ -f /partstyle.txt ]; then
            STYLE=$(cat /partstyle.txt)
        else
            STYLE=MBR
        fi
    
        echo ""
        echo -e " <1> Preserve space (only for install) \033[32m[${SPACE}MB]\033[0m"
        echo -e " <2> Secure boot support \033[32m[$SECURE]\033[0m"
        echo -e " <3> Partition style (only for install) \033[32m[$STYLE]\033[0m"
        echo " <0> Back to previous menu"
        echo ""
       
        while true; do
            read -p "Please choose your operation: "  Answer
            if echo $Answer | grep -q "^[0-3]$"; then
                break
            fi
        done
        
        if [ "$Answer" = "0" ]; then
            break
        elif [ "$Answer" = "1" ]; then            
            while true; do
                read -p "Please input the preserve space in MB: "  Answer
                if echo $Answer | grep -q "^[0-9][0-9]*$"; then
                    echo $Answer > /preserve.txt
                    break
                fi
            done
        elif [ "$Answer" = "2" ]; then
            if [ "$SECURE" = "Disable" ]; then
                echo "Enable" > /secureboot.txt
            else
                echo "Disable" > /secureboot.txt
            fi
        else
            if [ "$STYLE" = "GPT" ]; then
                echo "MBR" > /partstyle.txt
            else
                echo "GPT" > /partstyle.txt
            fi
        fi
    done
}

cd /
VTPATH=$(ls -1 | grep ventoy-)
VTVER=${VTPATH#*-}

cd $VTPATH

clear

echo ""
info "**************************************************"
vline=$(printf "*              Ventoy LiveCD %6s              *\r\n" "$VTVER")
info "$vline"
info "**************************************************"
echo ""
info "Scaning devices ......"
sleep 5

enum_disk

while true; do
    sel=$(select_disk)
    
    if [ $sel -eq 8888 ]; then
        break
    elif [ $sel -eq 0 ]; then
        enum_disk
        continue
    fi
    
    DEV=$(sed -n "${sel}p" /device.list | awk '{print $2}')
    DevVtVer=$(get_dev_ventoy_ver $DEV)


    if [ "$DevVtVer" = "NO" ]; then
        
        while true; do
            echo ""
            echo " <1> Install Ventoy to $DEV"
            echo " <2> Set Configuration"
            echo " <0> Back to previous menu"
            echo ""
            
            while true; do
                read -p "Please choose your operation: "  Answer
                if echo $Answer | grep -q "^[0-2]$"; then
                    break;
                fi
            done
        
            if [ "$Answer" = "0" ]; then
                break
            elif [ "$Answer" = "2" ]; then
                ventoy_configuration
            else
            
                opt=""
                if [ -f /preserve.txt ]; then
                    opt="$opt -r $(cat /preserve.txt)"
                fi
                
                if [ -f /secureboot.txt ] && grep -q "Enable" /secureboot.txt; then                    
                    opt="$opt -s"
                fi
                
                if [ -f /partstyle.txt ] && grep -q "GPT" /partstyle.txt; then            
                    opt="$opt -g"
                fi
                
                info "Ventoy2Disk.sh $opt -i /dev/$DEV"            
                sh Ventoy2Disk.sh $opt -i /dev/$DEV
                sync
                break
            fi
        done
    else
        info "Ventoy $DevVtVer detected in the device $DEV"
        
        while true; do
            echo ""
            echo " <1> Update Ventoy in $DEV from $DevVtVer ==> $VTVER"
            echo " <2> Re-install Ventoy to $DEV"
            echo " <3> Set Configuration"
            echo " <0> Back to previous menu"
            echo ""
           
            while true; do
                read -p "Please choose your operation: "  Answer
                if echo $Answer | grep -q "^[0-3]$"; then
                    break;
                fi
            done
            
            if [ "$Answer" = "0" ]; then
                break
            elif [ "$Answer" = "1" ]; then
                opt=""
                if [ -f /secureboot.txt ] && grep -q "Enable" /secureboot.txt; then                    
                    opt="$opt -s"
                fi
                
                info "Ventoy2Disk.sh $opt -u /dev/$DEV" 
                sh Ventoy2Disk.sh $opt -u /dev/$DEV
                sync
                break
            elif [ "$Answer" = "2" ]; then
                opt=""
                if [ -f /preserve.txt ]; then
                    opt="$opt -r $(cat /preserve.txt)"
                fi
                
                if [ -f /secureboot.txt ] && grep -q "Enable" /secureboot.txt; then                    
                    opt="$opt -s"
                fi
                
                if [ -f /partstyle.txt ] && grep -q "GPT" /partstyle.txt; then            
                    opt="$opt -g"
                fi
                
                info "Ventoy2Disk.sh $opt -I /dev/$DEV"            
                sh Ventoy2Disk.sh $opt -I /dev/$DEV
                sync
                break
            else    
                ventoy_configuration
            fi            
        done        
    fi
done


