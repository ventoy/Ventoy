# Ventoy
Ventoy is an open source tool to create bootable USB drive for ISO files.   
With ventoy, you don't need to format the disk over and over, you just need to copy the iso file to the USB drive and boot it.   
You can copy many iso files at a time and ventoy will give you a boot menu to select them.  
Both Legacy BIOS and UEFI are supported in the same way. 200+ ISO files are tested.  
A "Ventoy Compatible" concept is introduced by ventoy, which can help to support any ISO file.  

See https://www.ventoy.net for detail.

# Features
* 100% open source
* Simple to use
* Fast (limited only by the speed of copying iso file)
* Directly boot from iso file, no extraction needed
* Legacy + UEFI supported in the same way
* UEFI Secure Boot supported (since 1.0.07+)
* ISO files larger than 4GB supported
* Native boot menu style for Legacy & UEFI
* Most type of OS supported, 200+ iso files tested
* Not only boot but also complete installation process
* "Ventoy Compatible" concept
* Plugin Framework
* Auto installation supported (1.0.09+)
* Readonly to USB drive during boot
* USB normal use unafftected
* Data nondestructive during version upgrade
* No need to update Ventoy when a new distro is released

![avatar](https://www.ventoy.net/static/img/screen/screen_uefi.png)

# Installation Instructions

## For Windows
* Download the installation package, like ventoy-x.x.xx-windows.zip and decompress it.
* Run **Ventoy2Disk.exe** , select the device and click Install or Update button. 

![ventoy_windows](https://www.ventoy.net/static/img/ventoy2disk_en.png)

## For Linux
* Download the installation package, like ventoy-x.x.xx-linux.tar.gz and decompress it.
* Run the shell script as root sh Ventoy2Disk.sh { -i | -I | -u } /dev/XXX   XXX is the USB device, for example /dev/sdb. 


    Ventoy2Disk.sh  OPTION  /dev/XXX
    OPTION:
    -i   install ventoy to sdX (fail if disk already installed with ventoy)
    -I   force install ventoy to sdX (no matter installed or not)
    -u   update ventoy in sdX

**Attention the USB drive will be formatted and all the data will be lost after install.**
You just need to install Ventoy once, after that all the things needed is to copy the iso files to the USB.
You can also use it as a plain USB drive to store files and this will not affect Ventoy's function. 

## Copy ISO files

* After the installation is complete, the USB drive will be divided into 2 partitions.
* The 1st partition was formated with exFAT filesystem. You just need to copy iso files to this partition. You can place the iso files anywhere. Ventoy will search all the directories and subdirectories recursively to find all the iso files and list them in the boot menu alphabetically.
* The full path of the iso file (directories,subdirectories and file name) could NOT contain space or non-ascii characters

## Update Ventoy

If a new version of Ventoy is released, you can update it to the USB drive.
It should be noted that the upgrade operation is safe, all the files in the first partition will be unchanged. Upgrade operation is in the same way with installation. Ventoy2Disk.exe and Ventoy2Disk.sh will prompt you for update if the USB drive already installed with Ventoy. 