
find_cow_device() {
    tgt_pers_label="${1}"
    vt_pers_label=""
    vt_pers_dev=""
    
    if [ -f /ventoy/ventoy_persistent_label -a -f /ventoy/ventoy_persistent_dev ]; then
        vt_pers_label=$(cat /ventoy/ventoy_persistent_label)
        vt_pers_dev=$(cat /ventoy/ventoy_persistent_dev)        
    fi

    if [ "$tgt_pers_label" = "$vt_pers_label" ]; then
        echo "$vt_pers_dev"
    else
        find_cow_device_back "$@"
    fi    
}

find_cow_device_back() {
