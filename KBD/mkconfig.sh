#!/bin/sh

# 
# Configfiles are from grubfilemanager project
#

cfgfile=keyboard_layout.c
rm -f ${cfgfile}

cat >>$cfgfile << EOF

#define ventoy_keyboard_set_layout(name) if (grub_strcmp(layout, #name) == 0) return ventoy_keyboard_layout_##name()

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

dos2unix $cfgfile
sed 's/menuentry \([^ ]*\) .*/static void ventoy_keyboard_layout_\1(void) {/g' -i $cfgfile
sed 's/setkey *-r/grub_keymap_reset();/g' -i $cfgfile
sed 's/setkey *-d/grub_keymap_disable();/g' -i $cfgfile
sed 's/setkey *-e/grub_keymap_enable();/g' -i $cfgfile
sed 's/^setkey  *\([^ ]*\)  *\([^ ]*\)/grub_keymap_add_by_string("\1", "\2");/g' -i $cfgfile

rm -f .tmpfunc
echo "void ventoy_set_keyboard_layout(const char *layout);" >> .tmpfunc
echo "void ventoy_set_keyboard_layout(const char *layout) {" >> .tmpfunc
grep 'void *ventoy_keyboard_layout_' $cfgfile | while read line; do
    name=$(echo $line | sed 's/.*ventoy_keyboard_layout_\(.*\)(.*/\1/g')
    echo "ventoy_keyboard_set_layout($name);" >> .tmpfunc
done

echo "}" >> .tmpfunc

cat .tmpfunc >> $cfgfile
rm -f .tmpfunc

rm -f ../GRUB2/SRC/grub-2.04/grub-core/term/$cfgfile
cp -a $cfgfile ../GRUB2/SRC/grub-2.04/grub-core/term/$cfgfile





############
#
# cfg
#############

cfgfile=../INSTALL/grub/keyboard.cfg
rm -f ${cfgfile}

echo "submenu \"Keyboard Layouts\" --class=debug_krdlayout {" >>$cfgfile

cat >>$cfgfile << EOF
    menuentry QWERTY_USA --class=debug_kbd {
        set_keyboard_layout QWERTY_USA
    }
EOF

ls -1 cfg | while read line; do
    kbd=${line%.cfg}
    name=${kbd#KBD_}
    
    echo "    menuentry $name --class=debug_kbd {" >> $cfgfile
    echo "        set_keyboard_layout $name" >> $cfgfile
    echo "    }" >> $cfgfile   
done

echo "}" >>$cfgfile



