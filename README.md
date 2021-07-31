<h1 align="center">
  <a href=https://www.ventoy.net/>Ventoy</a>
</h1>

<p align="center">
  <img src="https://img.shields.io/github/release/ventoy/Ventoy.svg?style=for-the-badge">
  <img src="https://img.shields.io/github/license/ventoy/Ventoy?style=for-the-badge">
  <img src="https://img.shields.io/github/stars/ventoy/Ventoy?style=for-the-badge">
  <img src="https://img.shields.io/github/downloads/ventoy/Ventoy/total.svg?style=for-the-badge">
</p>

<h4 align="left">
Ventoy is an open source tool to create bootable USB drive for ISO/WIM/IMG/VHD(x)/EFI files. <br/>
With ventoy, you don't need to format the disk over and over, you just need to copy the image files to the USB drive and boot it.   
You can copy many image files at a time and ventoy will give you a boot menu to select them. <br/> 
x86 Legacy BIOS, IA32 UEFI, x86_64 UEFI, ARM64 UEFI and MIPS64EL UEFI are supported in the same way.<br/>
Both MBR and GPT partition style are supported in the same way.<br/>
Most type of OS supported(Windows/WinPE/Linux/Unix/Vmware/Xen...) <br/>
  720+ ISO files are tested (<a href="https://www.ventoy.net/en/isolist.html">List</a>). 90%+ distros in <a href="https://distrowatch.com/">distrowatch.com</a> supported. <br/>
<br/>Official Website: <a href=https://www.ventoy.net>https://www.ventoy.net</a>
</h4>

# Features
* 100% open source
* Simple to use
* Fast (limited only by the speed of copying iso file)
* Can be installed in USB/Local Disk/SSD/NVMe/SD Card
* Directly boot from ISO/WIM/IMG/VHD(x)/EFI files, no extraction needed
* No need to be continuous in disk for ISO/IMG files
* MBR and GPT partition style supported (1.0.15+)
* x86 Legacy BIOS, IA32 UEFI, x86_64 UEFI, ARM64 UEFI, MIPS64EL UEFI supported
* IA32/x86_64 UEFI Secure Boot supported (1.0.07+)
* Persistence supported (1.0.11+)
* Windows auto installation supported (1.0.09+)
* RHEL7/8/CentOS/7/8/SUSE/Ubuntu Server/Debian ... auto installation supported (1.0.09+)
* FAT32/exFAT/NTFS/UDF/XFS/Ext2(3)(4) supported for main partition
* ISO files larger than 4GB supported
* Native boot menu style for Legacy & UEFI
* Most type of OS supported, 720+ iso files tested
* Linux vDisk boot supported
* Not only boot but also complete installation process
* Menu dynamically switchable between List/TreeView mode
* "Ventoy Compatible" concept
* Plugin Framework
* Injection files to runtime environment
* Boot configuration file dynamically replacement
* Highly customizable theme and menu
* USB drive write-protected support
* USB normal use unaffected
* Data nondestructive during version upgrade
* No need to update Ventoy when a new distro is released

![avatar](https://www.ventoy.net/static/img/screen/screen_uefi.png)


# Installation Instructions
See [https://www.ventoy.net/en/doc_start.html](https://www.ventoy.net/en/doc_start.html) for detail

# Compile Instructions
Please refer to [BuildVentoyFromSource.txt](DOC/BuildVentoyFromSource.txt)

# Document
Title | Link
-|-
**Install & Update** | [https://www.ventoy.net/en/doc_start.html](https://www.ventoy.net/en/doc_start.html)
**Secure Boot** | [https://www.ventoy.net/en/doc_secure.html](https://www.ventoy.net/en/doc_secure.html)
**Customize Theme** | [https://www.ventoy.net/en/plugin_theme.html](https://www.ventoy.net/en/plugin_theme.html)  
**Global Control** | [https://www.ventoy.net/en/plugin_control.html](https://www.ventoy.net/en/plugin_control.html)  
**Image List** | [https://www.ventoy.net/en/plugin_imagelist.html](https://www.ventoy.net/en/plugin_imagelist.html)  
**Auto Installation** | [https://www.ventoy.net/en/plugin_autoinstall.html](https://www.ventoy.net/en/plugin_autoinstall.html)  
**Injection Plugin** | [https://www.ventoy.net/en/plugin_injection.html](https://www.ventoy.net/en/plugin_injection.html)  
**Persistence Support** | [https://www.ventoy.net/en/plugin_persistence.html](https://www.ventoy.net/en/plugin_persistence.html)  
**Boot WIM file** | [https://www.ventoy.net/en/plugin_wimboot.html](https://www.ventoy.net/en/plugin_wimboot.html)  
**Windows VHD Boot** | [https://www.ventoy.net/en/plugin_vhdboot.html](https://www.ventoy.net/en/plugin_vhdboot.html)  
**Linux vDisk Boot** | [https://www.ventoy.net/en/plugin_vtoyboot.html](https://www.ventoy.net/en/plugin_vtoyboot.html)  
**DUD Plugin** | [https://www.ventoy.net/en/plugin_dud.html](https://www.ventoy.net/en/plugin_dud.html)  
**Password Plugin** | [https://www.ventoy.net/en/plugin_password.html](https://www.ventoy.net/en/plugin_password.html)  
**Conf Replace Plugin** | [https://www.ventoy.net/en/plugin_bootconf_replace.html](https://www.ventoy.net/en/plugin_bootconf_replace.html)  
**Menu Class** | [https://www.ventoy.net/en/plugin_menuclass.html](https://www.ventoy.net/en/plugin_menuclass.html)  
**Menu Alias** | [https://www.ventoy.net/en/plugin_menualias.html](https://www.ventoy.net/en/plugin_menualias.html)  
**Menu Extension** | [https://www.ventoy.net/en/plugin_grubmenu.html](https://www.ventoy.net/en/plugin_grubmenu.html)  
**Memdisk Mode** | [https://www.ventoy.net/en/doc_memdisk.html](https://www.ventoy.net/en/doc_memdisk.html)  
**TreeView Mode** | [https://www.ventoy.net/en/doc_treeview.html](https://www.ventoy.net/en/doc_treeview.html)  
**Disk Layout MBR** | [https://www.ventoy.net/en/doc_disk_layout.html](https://www.ventoy.net/en/doc_disk_layout.html)  
**Disk Layout GPT** | [https://www.ventoy.net/en/doc_disk_layout_gpt.html](https://www.ventoy.net/en/doc_disk_layout_gpt.html)  
**Search Configuration** | [https://www.ventoy.net/en/doc_search_path.html](https://www.ventoy.net/en/doc_search_path.html)


# FAQ
See [https://www.ventoy.net/en/faq.html](https://www.ventoy.net/en/faq.html) for detail


# Forum
[https://forums.ventoy.net](https://forums.ventoy.net)


