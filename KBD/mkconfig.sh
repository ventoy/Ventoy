#!/bin/sh

# 
# Configfiles are from grubfilemanager project
#

cfgfile=../INSTALL/grub/keyboard.cfg
rm -f ${cfgfile}.gz

echo "submenu \"Keyboard Layouts\" --class=debug_krdlayout {" >>$cfgfile

cat >>$cfgfile << EOF
menuentry QWERTY_USA --class=debug_kbd {
    setkey -r
    setkey -d
}
EOF

ls -1 cfg | while read line; do
    kbd=${line%.cfg}
    name=${kbd#KBD_}
    
    echo "menuentry $name --class=debug_kbd {" >> $cfgfile
    grep '^setkey' cfg/$line >>$cfgfile    
    echo "}" >> $cfgfile   
done

echo "}" >>$cfgfile

gzip $cfgfile
