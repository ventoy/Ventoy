/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2002,2007  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GRUB_FILE_HEADER
#define GRUB_FILE_HEADER	1

#include <grub/types.h>
#include <grub/err.h>
#include <grub/device.h>
#include <grub/fs.h>
#include <grub/disk.h>

enum grub_file_type
  {
    GRUB_FILE_TYPE_NONE = 0,
    /* GRUB module to be loaded.  */
    GRUB_FILE_TYPE_GRUB_MODULE,
    /* Loopback file to be represented as disk.  */
    GRUB_FILE_TYPE_LOOPBACK,
    /* Linux kernel to be loaded.  */
    GRUB_FILE_TYPE_LINUX_KERNEL,
    /* Linux initrd.  */
    GRUB_FILE_TYPE_LINUX_INITRD,

    /* Multiboot kernel.  */
    GRUB_FILE_TYPE_MULTIBOOT_KERNEL,
    /* Multiboot module.  */
    GRUB_FILE_TYPE_MULTIBOOT_MODULE,

    /* Xen hypervisor - used on ARM only. */
    GRUB_FILE_TYPE_XEN_HYPERVISOR,
    /* Xen module - used on ARM only. */
    GRUB_FILE_TYPE_XEN_MODULE,

    GRUB_FILE_TYPE_BSD_KERNEL,
    GRUB_FILE_TYPE_FREEBSD_ENV,
    GRUB_FILE_TYPE_FREEBSD_MODULE,
    GRUB_FILE_TYPE_FREEBSD_MODULE_ELF,
    GRUB_FILE_TYPE_NETBSD_MODULE,
    GRUB_FILE_TYPE_OPENBSD_RAMDISK,

    GRUB_FILE_TYPE_XNU_INFO_PLIST,
    GRUB_FILE_TYPE_XNU_MKEXT,
    GRUB_FILE_TYPE_XNU_KEXT,
    GRUB_FILE_TYPE_XNU_KERNEL,
    GRUB_FILE_TYPE_XNU_RAMDISK,
    GRUB_FILE_TYPE_XNU_HIBERNATE_IMAGE,
    GRUB_FILE_XNU_DEVPROP,

    GRUB_FILE_TYPE_PLAN9_KERNEL,

    GRUB_FILE_TYPE_NTLDR,
    GRUB_FILE_TYPE_TRUECRYPT,
    GRUB_FILE_TYPE_FREEDOS,
    GRUB_FILE_TYPE_PXECHAINLOADER,
    GRUB_FILE_TYPE_PCCHAINLOADER,

    GRUB_FILE_TYPE_COREBOOT_CHAINLOADER,

    GRUB_FILE_TYPE_EFI_CHAINLOADED_IMAGE,

    /* File holding signature.  */
    GRUB_FILE_TYPE_SIGNATURE,
    /* File holding public key to verify signature once.  */
    GRUB_FILE_TYPE_PUBLIC_KEY,
    /* File holding public key to add to trused keys.  */
    GRUB_FILE_TYPE_PUBLIC_KEY_TRUST,
    /* File of which we intend to print a blocklist to the user.  */
    GRUB_FILE_TYPE_PRINT_BLOCKLIST,
    /* File we intend to use for test loading or testing speed.  */
    GRUB_FILE_TYPE_TESTLOAD,
    /* File we open only to get its size. E.g. in ls output.  */
    GRUB_FILE_TYPE_GET_SIZE,
    /* Font file.  */
    GRUB_FILE_TYPE_FONT,
    /* File holding encryption key for encrypted ZFS.  */
    GRUB_FILE_TYPE_ZFS_ENCRYPTION_KEY,
    /* File we open n grub-fstest.  */
    GRUB_FILE_TYPE_FSTEST,
    /* File we open n grub-mount.  */
    GRUB_FILE_TYPE_MOUNT,
    /* File which we attempt to identify the type of.  */
    GRUB_FILE_TYPE_FILE_ID,
    /* File holding ACPI table.  */
    GRUB_FILE_TYPE_ACPI_TABLE,
    /* File holding Device Tree.  */
    GRUB_FILE_TYPE_DEVICE_TREE_IMAGE,
    /* File we intend show to user.  */
    GRUB_FILE_TYPE_CAT,
    GRUB_FILE_TYPE_HEXCAT,
    /* One of pair of files we intend to compare.  */
    GRUB_FILE_TYPE_CMP,
    /* List of hashes for hashsum.  */
    GRUB_FILE_TYPE_HASHLIST,
    /* File hashed by hashsum.  */
    GRUB_FILE_TYPE_TO_HASH,
    /* Keyboard layout.  */
    GRUB_FILE_TYPE_KEYBOARD_LAYOUT,
    /* Picture file.  */
    GRUB_FILE_TYPE_PIXMAP,
    /* *.lst shipped by GRUB.  */
    GRUB_FILE_TYPE_GRUB_MODULE_LIST,
    /* config file.  */
    GRUB_FILE_TYPE_CONFIG,
    GRUB_FILE_TYPE_THEME,
    GRUB_FILE_TYPE_GETTEXT_CATALOG,
    GRUB_FILE_TYPE_FS_SEARCH,
    GRUB_FILE_TYPE_AUDIO,
    GRUB_FILE_TYPE_VBE_DUMP,

    GRUB_FILE_TYPE_LOADENV,
    GRUB_FILE_TYPE_SAVEENV,

    GRUB_FILE_TYPE_VERIFY_SIGNATURE,

    GRUB_FILE_TYPE_MASK = 0xffff,

    /* --skip-sig is specified.  */
    GRUB_FILE_TYPE_SKIP_SIGNATURE = 0x10000,
    GRUB_FILE_TYPE_NO_DECOMPRESS = 0x20000,
    GRUB_FILE_TYPE_NO_VLNK = 0x40000,
  };

/* File description.  */
struct grub_file
{
  /* File name.  */
  char *name;

  int vlnk;

  /* The underlying device.  */
  grub_device_t device;

  /* The underlying filesystem.  */
  grub_fs_t fs;

  /* The current offset.  */
  grub_off_t offset;
  grub_off_t progress_offset;

  /* Progress info. */
  grub_uint64_t last_progress_time;
  grub_off_t last_progress_offset;
  grub_uint64_t estimated_speed;

  /* The file size.  */
  grub_off_t size;

  /* If file is not easily seekable. Should be set by underlying layer.  */
  int not_easily_seekable;

  /* Filesystem-specific data.  */
  void *data;

  /* This is called when a sector is read. Used only for a disk device.  */
  grub_disk_read_hook_t read_hook;

  /* Caller-specific data passed to the read hook.  */
  void *read_hook_data;
};
typedef struct grub_file *grub_file_t;

extern grub_disk_read_hook_t EXPORT_VAR(grub_file_progress_hook);

/* Filters with lower ID are executed first.  */
typedef enum grub_file_filter_id
  {
    GRUB_FILE_FILTER_VERIFY,
    GRUB_FILE_FILTER_GZIO,
    GRUB_FILE_FILTER_XZIO,
    GRUB_FILE_FILTER_LZOPIO,
    GRUB_FILE_FILTER_MAX,
    GRUB_FILE_FILTER_COMPRESSION_FIRST = GRUB_FILE_FILTER_GZIO,
    GRUB_FILE_FILTER_COMPRESSION_LAST = GRUB_FILE_FILTER_LZOPIO,
  } grub_file_filter_id_t;

typedef grub_file_t (*grub_file_filter_t) (grub_file_t in, enum grub_file_type type);

extern grub_file_filter_t EXPORT_VAR(grub_file_filters)[GRUB_FILE_FILTER_MAX];

static inline void
grub_file_filter_register (grub_file_filter_id_t id, grub_file_filter_t filter)
{
  grub_file_filters[id] = filter;
}

static inline void
grub_file_filter_unregister (grub_file_filter_id_t id)
{
  grub_file_filters[id] = 0;
}

/* Get a device name from NAME.  */
char *EXPORT_FUNC(grub_file_get_device_name) (const char *name);

int EXPORT_FUNC(ventoy_check_file_exist) (const char * fmt, ...);
grub_file_t EXPORT_FUNC(grub_file_open) (const char *name, enum grub_file_type type);
grub_ssize_t EXPORT_FUNC(grub_file_read) (grub_file_t file, void *buf,
					  grub_size_t len);
grub_off_t EXPORT_FUNC(grub_file_seek) (grub_file_t file, grub_off_t offset);
grub_err_t EXPORT_FUNC(grub_file_close) (grub_file_t file);

int EXPORT_FUNC(grub_file_is_vlnk_suffix)(const char *name, int len);
int EXPORT_FUNC(grub_file_add_vlnk)(const char *src, const char *dst);
int EXPORT_FUNC(grub_file_vtoy_vlnk)(const char *src, const char *dst);
const char * EXPORT_FUNC(grub_file_get_vlnk)(const char *name, int *vlnk);

/* Return value of grub_file_size() in case file size is unknown. */
#define GRUB_FILE_SIZE_UNKNOWN	 0xffffffffffffffffULL

static inline grub_off_t
grub_file_size (const grub_file_t file)
{
  return file->size;
}

static inline grub_off_t
grub_file_tell (const grub_file_t file)
{
  return file->offset;
}

static inline int
grub_file_seekable (const grub_file_t file)
{
  return !file->not_easily_seekable;
}

grub_file_t
grub_file_offset_open (grub_file_t parent, enum grub_file_type type,
		       grub_off_t start, grub_off_t size);
void
grub_file_offset_close (grub_file_t file);

#endif /* ! GRUB_FILE_HEADER */
