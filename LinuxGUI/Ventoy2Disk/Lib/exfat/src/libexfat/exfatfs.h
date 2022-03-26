/*
	exfatfs.h (29.08.09)
	Definitions of structures and constants used in exFAT file system.

	Free exFAT implementation.
	Copyright (C) 2010-2018  Andrew Nayenko

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef EXFATFS_H_INCLUDED
#define EXFATFS_H_INCLUDED

#include "byteorder.h"
#include "compiler.h"

typedef uint32_t cluster_t;		/* cluster number */

#define EXFAT_FIRST_DATA_CLUSTER 2
#define EXFAT_LAST_DATA_CLUSTER 0xfffffff6

#define EXFAT_CLUSTER_FREE         0 /* free cluster */
#define EXFAT_CLUSTER_BAD 0xfffffff7 /* cluster contains bad sector */
#define EXFAT_CLUSTER_END 0xffffffff /* final cluster of file or directory */

#define EXFAT_STATE_MOUNTED 2

struct exfat_super_block
{
	uint8_t jump[3];				/* 0x00 jmp and nop instructions */
	uint8_t oem_name[8];			/* 0x03 "EXFAT   " */
	uint8_t	__unused1[53];			/* 0x0B always 0 */
	le64_t sector_start;			/* 0x40 partition first sector */
	le64_t sector_count;			/* 0x48 partition sectors count */
	le32_t fat_sector_start;		/* 0x50 FAT first sector */
	le32_t fat_sector_count;		/* 0x54 FAT sectors count */
	le32_t cluster_sector_start;	/* 0x58 first cluster sector */
	le32_t cluster_count;			/* 0x5C total clusters count */
	le32_t rootdir_cluster;			/* 0x60 first cluster of the root dir */
	le32_t volume_serial;			/* 0x64 volume serial number */
	struct							/* 0x68 FS version */
	{
		uint8_t minor;
		uint8_t major;
	}
	version;
	le16_t volume_state;			/* 0x6A volume state flags */
	uint8_t sector_bits;			/* 0x6C sector size as (1 << n) */
	uint8_t spc_bits;				/* 0x6D sectors per cluster as (1 << n) */
	uint8_t fat_count;				/* 0x6E always 1 */
	uint8_t drive_no;				/* 0x6F always 0x80 */
	uint8_t allocated_percent;		/* 0x70 percentage of allocated space */
	uint8_t __unused2[397];			/* 0x71 always 0 */
	le16_t boot_signature;			/* the value of 0xAA55 */
}
PACKED;
STATIC_ASSERT(sizeof(struct exfat_super_block) == 512);

#define EXFAT_ENTRY_VALID     0x80
#define EXFAT_ENTRY_CONTINUED 0x40
#define EXFAT_ENTRY_OPTIONAL  0x20

#define EXFAT_ENTRY_BITMAP    (0x01 | EXFAT_ENTRY_VALID)
#define EXFAT_ENTRY_UPCASE    (0x02 | EXFAT_ENTRY_VALID)
#define EXFAT_ENTRY_LABEL     (0x03 | EXFAT_ENTRY_VALID)
#define EXFAT_ENTRY_FILE      (0x05 | EXFAT_ENTRY_VALID)
#define EXFAT_ENTRY_FILE_INFO (0x00 | EXFAT_ENTRY_VALID | EXFAT_ENTRY_CONTINUED)
#define EXFAT_ENTRY_FILE_NAME (0x01 | EXFAT_ENTRY_VALID | EXFAT_ENTRY_CONTINUED)
#define EXFAT_ENTRY_FILE_TAIL (0x00 | EXFAT_ENTRY_VALID \
                                    | EXFAT_ENTRY_CONTINUED \
                                    | EXFAT_ENTRY_OPTIONAL)

struct exfat_entry					/* common container for all entries */
{
	uint8_t type;					/* any of EXFAT_ENTRY_xxx */
	uint8_t data[31];
}
PACKED;
STATIC_ASSERT(sizeof(struct exfat_entry) == 32);

#define EXFAT_ENAME_MAX 15

struct exfat_entry_bitmap			/* allocated clusters bitmap */
{
	uint8_t type;					/* EXFAT_ENTRY_BITMAP */
	uint8_t __unknown1[19];
	le32_t start_cluster;
	le64_t size;					/* in bytes */
}
PACKED;
STATIC_ASSERT(sizeof(struct exfat_entry_bitmap) == 32);

#define EXFAT_UPCASE_CHARS 0x10000

struct exfat_entry_upcase			/* upper case translation table */
{
	uint8_t type;					/* EXFAT_ENTRY_UPCASE */
	uint8_t __unknown1[3];
	le32_t checksum;
	uint8_t __unknown2[12];
	le32_t start_cluster;
	le64_t size;					/* in bytes */
}
PACKED;
STATIC_ASSERT(sizeof(struct exfat_entry_upcase) == 32);

struct exfat_entry_label			/* volume label */
{
	uint8_t type;					/* EXFAT_ENTRY_LABEL */
	uint8_t length;					/* number of characters */
	le16_t name[EXFAT_ENAME_MAX];	/* in UTF-16LE */
}
PACKED;
STATIC_ASSERT(sizeof(struct exfat_entry_label) == 32);

#define EXFAT_ATTRIB_RO     0x01
#define EXFAT_ATTRIB_HIDDEN 0x02
#define EXFAT_ATTRIB_SYSTEM 0x04
#define EXFAT_ATTRIB_VOLUME 0x08
#define EXFAT_ATTRIB_DIR    0x10
#define EXFAT_ATTRIB_ARCH   0x20

struct exfat_entry_meta1			/* file or directory info (part 1) */
{
	uint8_t type;					/* EXFAT_ENTRY_FILE */
	uint8_t continuations;
	le16_t checksum;
	le16_t attrib;					/* combination of EXFAT_ATTRIB_xxx */
	le16_t __unknown1;
	le16_t crtime, crdate;			/* creation date and time */
	le16_t mtime, mdate;			/* latest modification date and time */
	le16_t atime, adate;			/* latest access date and time */
	uint8_t crtime_cs;				/* creation time in cs (centiseconds) */
	uint8_t mtime_cs;				/* latest modification time in cs */
	uint8_t __unknown2[10];
}
PACKED;
STATIC_ASSERT(sizeof(struct exfat_entry_meta1) == 32);

#define EXFAT_FLAG_ALWAYS1		(1u << 0)
#define EXFAT_FLAG_CONTIGUOUS	(1u << 1)

struct exfat_entry_meta2			/* file or directory info (part 2) */
{
	uint8_t type;					/* EXFAT_ENTRY_FILE_INFO */
	uint8_t flags;					/* combination of EXFAT_FLAG_xxx */
	uint8_t __unknown1;
	uint8_t name_length;
	le16_t name_hash;
	le16_t __unknown2;
	le64_t valid_size;				/* in bytes, less or equal to size */
	uint8_t __unknown3[4];
	le32_t start_cluster;
	le64_t size;					/* in bytes */
}
PACKED;
STATIC_ASSERT(sizeof(struct exfat_entry_meta2) == 32);

struct exfat_entry_name				/* file or directory name */
{
	uint8_t type;					/* EXFAT_ENTRY_FILE_NAME */
	uint8_t __unknown;
	le16_t name[EXFAT_ENAME_MAX];	/* in UTF-16LE */
}
PACKED;
STATIC_ASSERT(sizeof(struct exfat_entry_name) == 32);

#endif /* ifndef EXFATFS_H_INCLUDED */
