sync

while [ -n "Y" ]; do
    clear > /dev/console
    echo '################################################' > /dev/console
    echo '################################################' > /dev/console
    echo '### Please reboot and load from Ventoy again ###' > /dev/console
    echo '### This only needed for the first boot time ###' > /dev/console
    echo '################################################' > /dev/console
    echo '################################################' > /dev/console

    clear > /dev/tty0
    echo '################################################' > /dev/tty0
    echo '################################################' > /dev/tty0
    echo '### Please reboot and load from Ventoy again ###' > /dev/tty0
    echo '### This only needed for the first boot time ###' > /dev/tty0
    echo '################################################' > /dev/tty0
    echo '################################################' > /dev/tty0
    sleep 1
done

