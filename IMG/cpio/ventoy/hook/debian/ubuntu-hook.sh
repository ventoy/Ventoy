#!/ventoy/busybox/sh

echo "Here before mountroot ..." >> $VTLOG
    
$SED  "/^mountroot$/i\\$BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/ubuntu-disk.sh"  -i /init
$SED  "/^mountroot$/i\\maybe_break bottom"  -i /init

#workaround for issue #3718
if [ -f $VTOY_PATH/ventoy_persistent_map -a -f /scripts/casper-helpers ]; then
    echo 'fix casper cow udev issue' >> $VTLOG
    if $GREP -q '^find_cow_device\(\).*{' /scripts/casper-helpers; then
        VTINS_LINE=$($GREP -n '^find_cow_device\(\).*{' /scripts/casper-helpers | $AWK -F':' '{print $1}')
        $AWK "NR < $VTINS_LINE"  /scripts/casper-helpers  >  /ventoy/casper-helpers-tmp
        $CAT $VTOY_PATH/hook/debian/casper-perst-wa.sh    >> /ventoy/casper-helpers-tmp
        $AWK "NR > $VTINS_LINE" /scripts/casper-helpers   >> /ventoy/casper-helpers-tmp
        $CAT /ventoy/casper-helpers-tmp > /scripts/casper-helpers
    fi
fi


if [ -f $VTOY_PATH/autoinstall ]; then
    echo "Do auto install ..." >> $VTLOG
    
    if $GREP -q '^autoinstall:' $VTOY_PATH/autoinstall; then
        echo "cloud-init auto install ..." >> $VTLOG
        if $GREP -q "maybe_break init" /init; then
            $SED "/maybe_break init/i $BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/ventoy-cloud-init.sh \$rootmnt"  -i /init
        fi
    else
        if $GREP -q "^mount /proc$" /init; then
            $SED "/^mount \/proc/a export file=$VTOY_PATH/autoinstall; export auto='true'; export priority='critical'"  -i /init
        fi
    fi
fi

