# Module lists from GRUB2/MOD_SRC/grub-2.04/install.sh.
# The image embeds GRUB_EMBED_*. The ESP receives every .mod/.lst that is
# not named in GRUB_SKIP_* (that skip list also contains the net modules,
# which install.sh excludes even though they are not inside the image).
# GRUB 2.14 does not build setkey, gfxterm_menu, mouse or fwload, so those
# names are left out. grub-mkimage would fail if they were requested.
# shellcheck shell=sh

GRUB_EMBED_UEFI="file blocklist ventoy test true regexp newc search at_keyboard usb_keyboard gcry_md5 hashsum gzio xzio lzopio ext2 xfs read halt sleep serial terminfo png password_pbkdf2 gcry_sha512 pbkdf2 part_gpt part_msdos ls tar squash4 loopback part_apple minicmd diskfilter linux relocator jpeg iso9660 udf hfsplus halt acpi mmap gfxmenu video_colors trig bitmap_scale gfxterm bitmap font fat exfat ntfs fshelp efifwsetup reboot echo configfile normal terminal gettext chain priority_queue bufio datetime cat extcmd crypto boot all_video efi_gop efi_uga video_bochs video_cirrus video video_fb gfxterm_background smbios zfs"
GRUB_SKIP_UEFI="efinet net tftp http $GRUB_EMBED_UEFI"

GRUB_EMBED_ARM="file blocklist ventoy test true regexp newc search gcry_md5 hashsum gzio xzio lzopio ext2 xfs read halt sleep serial terminfo png password_pbkdf2 gcry_sha512 pbkdf2 part_gpt part_msdos ls tar squash4 loopback part_apple minicmd diskfilter linux jpeg iso9660 udf hfsplus halt acpi mmap gfxmenu video_colors trig bitmap_scale gfxterm bitmap font fat exfat ntfs fshelp efifwsetup reboot echo configfile normal terminal gettext chain priority_queue bufio datetime cat extcmd crypto boot all_video efi_gop video video_fb gfxterm_background zfs"
GRUB_SKIP_ARM="efinet net tftp http $GRUB_EMBED_ARM"

GRUB_EMBED_PC="file date drivemap blocklist newc vga_text ntldr search at_keyboard usb_keyboard gcry_md5 hashsum gzio xzio lzopio lspci pci ext2 xfs ventoy chain read halt iso9660 linux16 test true sleep reboot echo videotest videoinfo videotest_checksum video_colors video_cirrus video_bochs vga vbe font video gettext extcmd terminal linux minicmd help configfile tr trig boot biosdisk disk ls tar squash4 password_pbkdf2 all_video png jpeg part_gpt part_msdos fat exfat ntfs loopback normal video_fb udf gfxmenu gfxterm gfxterm_background smbios"
GRUB_SKIP_PC="net tftp http $GRUB_EMBED_PC"

grub_embed_mods() {
    case $1 in
        x86_64-efi|i386-efi) printf '%s\n' "$GRUB_EMBED_UEFI" ;;
        arm64-efi|loongarch64-efi|mips64el-efi) printf '%s\n' "$GRUB_EMBED_ARM" ;;
        i386-pc) printf '%s\n' "$GRUB_EMBED_PC" ;;
        *) echo "grub_embed_mods: unknown $1" >&2; return 1 ;;
    esac
}

grub_skip_mods() {
    case $1 in
        x86_64-efi|i386-efi) printf '%s\n' "$GRUB_SKIP_UEFI" ;;
        arm64-efi|loongarch64-efi|mips64el-efi) printf '%s\n' "$GRUB_SKIP_ARM" ;;
        i386-pc) printf '%s\n' "$GRUB_SKIP_PC" ;;
        *) echo "grub_skip_mods: unknown $1" >&2; return 1 ;;
    esac
}

# Copy the modules install.sh leaves on the ESP.
# arm64, mips64el and loongarch64 files keep the .mod name but are xz,
# except normal.mod, which install.sh copies from the PXE build uncompressed.
grub_install_esp_mods() {
    plat=$1
    src=$2
    dest=$3
    skip=$(grub_skip_mods "$plat")
    compress=0
    case $plat in
        arm64-efi|loongarch64-efi|mips64el-efi) compress=1 ;;
    esac
    rm -rf "$dest"
    mkdir -p "$dest"
    cp -a "$src/normal.mod" "$dest/normal.mod"
    for srcfile in "$src"/*.mod "$src"/*.lst; do
        if [ ! -f "$srcfile" ]; then
            continue
        fi
        base=$(basename "$srcfile")
        if [ "$base" = "normal.mod" ]; then
            continue
        fi
        name=${base%.mod}
        case " $skip " in
            *" $name "*) continue ;;
        esac
        if [ "$compress" -eq 1 ]; then
            xz -c "$srcfile" > "$dest/$base"
        else
            cp -a "$srcfile" "$dest/$base"
        fi
    done
    test -f "$dest/normal.mod"
}
