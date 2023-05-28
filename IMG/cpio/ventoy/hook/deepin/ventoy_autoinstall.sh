#!/ventoy/busybox/sh

. /ventoy/hook/ventoy_hook_lib.sh

change_var_value() {
    local vfile=$1
    local vkey=$2
    local vVal=$3
    local quote=$4
    local vline
    
    if [ $quote -eq 0 ]; then
        vline="$vkey = $vVal"
    else
        vline="$vkey = \"$vVal\""
    fi
    
    if grep -q -m1 "^$vkey[[:space:]]*=" $vfile; then
        sed "s#^$vkey[[:space:]]*=.*#$vline#g" -i $vfile
    else
        echo "$vline" >> $vfile
    fi
}

setting_script_process() {
    local sfile=$1
    local vItem
    local vB64Item
    
    vItem=$(grep '^language[[:space:]]*=' /ventoy/autoinstall | awk  '{print $3}')
    if [ -n "$vItem" ]; then
        change_var_value $sfile 'select_language_default_locale' "$vItem" 0
    fi
    
    vItem=$(grep '^timezone[[:space:]]*=' /ventoy/autoinstall | awk  '{print $3}')
    if [ -n "$vItem" ]; then
        change_var_value $sfile 'timezone_default' "$vItem" 0
    fi
    
    vItem=$(grep '^hostname[[:space:]]*=' /ventoy/autoinstall | awk  '{print $3}')
    if [ -n "$vItem" ]; then
        change_var_value $sfile 'system_info_default_hostname' "$vItem" 1
        change_var_value $sfile 'DI_HOSTNAME' "$vItem" 1
    fi
    
    vItem=$(grep '^root_password[[:space:]]*=' /ventoy/autoinstall | awk  '{print $3}')
    if [ -n "$vItem" ]; then
        vB64Item=$(echo -n "$vItem" | base64)
        change_var_value $sfile 'system_info_default_root_password' "$vB64Item" 1
        change_var_value $sfile 'DI_ROOTPASSWORD' "$vB64Item" 1
    fi
    
    vItem=$(grep '^default_username[[:space:]]*=' /ventoy/autoinstall | awk  '{print $3}')
    if [ -n "$vItem" ]; then
        change_var_value $sfile 'system_info_default_username' "$vItem" 1
        change_var_value $sfile 'DI_USERNAME' "$vItem" 1
    fi
    
    vItem=$(grep '^default_password[[:space:]]*=' /ventoy/autoinstall | awk  '{print $3}')
    if [ -n "$vItem" ]; then
        change_var_value $sfile 'system_info_default_password' "$vItem" 1
        change_var_value $sfile 'DI_PASSWORD' "$vItem" 1
    fi
    
    vItem=$(grep '^install_disk[[:space:]]*=' /ventoy/autoinstall | awk  '{print $3}')
    if [ -n "$vItem" ]; then
        echo "DI_FULLDISK_MULTIDISK_DEVICE = $vItem" >> $sfile
        echo "DI_ROOTDISK = $vItem" >> $sfile
        echo "DI_BOOTLOADER = $vItem" >> $sfile
    fi
    
    change_var_value $sfile 'skip_virtual_machine_page' 'true' 0
    change_var_value $sfile 'skip_select_language_page' 'true' 0
    change_var_value $sfile 'skip_select_language_page_on_first_boot' 'true' 0
    change_var_value $sfile 'skip_system_keyboard_page' 'true' 0
    change_var_value $sfile 'skip_system_info_page' 'true' 0
    change_var_value $sfile 'skip_qr_code_system_info_page' 'true' 0
    change_var_value $sfile 'skip_timezone_page' 'true' 0
    change_var_value $sfile 'skip_partition_page' 'true' 0
    change_var_value $sfile 'system_info_password_validate_required' '0' 0
    change_var_value $sfile 'system_info_password_strong_check' 'false' 0
    change_var_value $sfile 'partition_do_auto_part' 'true' 0
    change_var_value $sfile 'system_info_disable_license' 'true' 0
    change_var_value $sfile 'system_info_disable_experience' 'true' 0
    change_var_value $sfile 'system_info_disable_privacy_license' 'true' 0
    
    #filesystem.squashfs search ini
    #first_page_state=0，表示不跳过首页，展示首页让用户自己选择
    #first_page_state=1，表示跳过首页，并且自动点击一键安装
    #first_page_state=2，表示跳过首页，并且自动点击自定义安装
    #first_page_state=3，表示跳过首页，并且直接以全盘安装方式自动安装
    change_var_value $sfile 'first_page_state' '3' 0
}

update_settings() {
    local script=$1
    local newscript
    
    echo "update_settings for $script ..."
    
    newscript=$(basename $script)
    cp -a $script /ventoy/vini_${newscript}
    setting_script_process /ventoy/vini_${newscript}
    
    rm -f $script
    cp -a /ventoy/vini_${newscript} $script
}

sh /ventoy/hook/common/auto_install_varexp.sh  /ventoy/autoinstall

update_settings /root/usr/share/deepin-installer/resources/default_settings.ini

ls -1 /root/usr/share/deepin-installer/resources/override/ | while read line; do
    update_settings /root/usr/share/deepin-installer/resources/override/$line
done

ls -1 /root/usr/share/deepin-installer/resources/oem/ | while read line; do
    update_settings /root/usr/share/deepin-installer/resources/oem/$line
done



