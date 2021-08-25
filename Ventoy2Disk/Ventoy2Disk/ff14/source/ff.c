/*----------------------------------------------------------------------------/
/  FatFs - Generic FAT Filesystem Module  R0.14                               /
/-----------------------------------------------------------------------------/
/
/ Copyright (C) 2019, ChaN, all right reserved.
/
/ FatFs module is an open source software. Redistribution and use of FatFs in
/ source and binary forms, with or without modification, are permitted provided
/ that the following condition is met:
/
/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition and the following disclaimer.
/
/ This software is provided by the copyright holder and contributors "AS IS"
/ and any warranties related to this software are DISCLAIMED.
/ The copyright owner or contributors be NOT LIABLE for any damages caused
/ by use of this software.
/
/----------------------------------------------------------------------------*/


#include "ff.h"			/* Declarations of FatFs API */
#include "diskio.h"		/* Declarations of device I/O functions */


/*--------------------------------------------------------------------------

   Module Private Definitions

---------------------------------------------------------------------------*/

#if FF_DEFINED != 86606	/* Revision ID */
#error Wrong include file (ff.h).
#endif


/* Limits and boundaries */
#define MAX_DIR		0x200000		/* Max size of FAT directory */
#define MAX_DIR_EX	0x10000000		/* Max size of exFAT directory */
#define MAX_FAT12	0xFF5			/* Max FAT12 clusters (differs from specs, but right for real DOS/Windows behavior) */
#define MAX_FAT16	0xFFF5			/* Max FAT16 clusters (differs from specs, but right for real DOS/Windows behavior) */
#define MAX_FAT32	0x0FFFFFF5		/* Max FAT32 clusters (not specified, practical limit) */
#define MAX_EXFAT	0x7FFFFFFD		/* Max exFAT clusters (differs from specs, implementation limit) */


/* Character code support macros */
#define IsUpper(c)		((c) >= 'A' && (c) <= 'Z')
#define IsLower(c)		((c) >= 'a' && (c) <= 'z')
#define IsDigit(c)		((c) >= '0' && (c) <= '9')
#define IsSurrogate(c)	((c) >= 0xD800 && (c) <= 0xDFFF)
#define IsSurrogateH(c)	((c) >= 0xD800 && (c) <= 0xDBFF)
#define IsSurrogateL(c)	((c) >= 0xDC00 && (c) <= 0xDFFF)


/* Additional file access control and file status flags for internal use */
#define FA_SEEKEND	0x20	/* Seek to end of the file on file open */
#define FA_MODIFIED	0x40	/* File has been modified */
#define FA_DIRTY	0x80	/* FIL.buf[] needs to be written-back */


/* Additional file attribute bits for internal use */
#define AM_VOL		0x08	/* Volume label */
#define AM_LFN		0x0F	/* LFN entry */
#define AM_MASK		0x3F	/* Mask of defined bits */


/* Name status flags in fn[11] */
#define NSFLAG		11		/* Index of the name status byte */
#define NS_LOSS		0x01	/* Out of 8.3 format */
#define NS_LFN		0x02	/* Force to create LFN entry */
#define NS_LAST		0x04	/* Last segment */
#define NS_BODY		0x08	/* Lower case flag (body) */
#define NS_EXT		0x10	/* Lower case flag (ext) */
#define NS_DOT		0x20	/* Dot entry */
#define NS_NOLFN	0x40	/* Do not find LFN */
#define NS_NONAME	0x80	/* Not followed */


/* exFAT directory entry types */
#define	ET_BITMAP	0x81	/* Allocation bitmap */
#define	ET_UPCASE	0x82	/* Up-case table */
#define	ET_VLABEL	0x83	/* Volume label */
#define	ET_FILEDIR	0x85	/* File and directory */
#define	ET_STREAM	0xC0	/* Stream extension */
#define	ET_FILENAME	0xC1	/* Name extension */


/* FatFs refers the FAT structure as simple byte array instead of structure member
/ because the C structure is not binary compatible between different platforms */

#define BS_JmpBoot			0		/* x86 jump instruction (3-byte) */
#define BS_OEMName			3		/* OEM name (8-byte) */
#define BPB_BytsPerSec		11		/* Sector size [byte] (WORD) */
#define BPB_SecPerClus		13		/* Cluster size [sector] (BYTE) */
#define BPB_RsvdSecCnt		14		/* Size of reserved area [sector] (WORD) */
#define BPB_NumFATs			16		/* Number of FATs (BYTE) */
#define BPB_RootEntCnt		17		/* Size of root directory area for FAT [entry] (WORD) */
#define BPB_TotSec16		19		/* Volume size (16-bit) [sector] (WORD) */
#define BPB_Media			21		/* Media descriptor byte (BYTE) */
#define BPB_FATSz16			22		/* FAT size (16-bit) [sector] (WORD) */
#define BPB_SecPerTrk		24		/* Number of sectors per track for int13h [sector] (WORD) */
#define BPB_NumHeads		26		/* Number of heads for int13h (WORD) */
#define BPB_HiddSec			28		/* Volume offset from top of the drive (DWORD) */
#define BPB_TotSec32		32		/* Volume size (32-bit) [sector] (DWORD) */
#define BS_DrvNum			36		/* Physical drive number for int13h (BYTE) */
#define BS_NTres			37		/* WindowsNT error flag (BYTE) */
#define BS_BootSig			38		/* Extended boot signature (BYTE) */
#define BS_VolID			39		/* Volume serial number (DWORD) */
#define BS_VolLab			43		/* Volume label string (8-byte) */
#define BS_FilSysType		54		/* Filesystem type string (8-byte) */
#define BS_BootCode			62		/* Boot code (448-byte) */
#define BS_55AA				510		/* Signature word (WORD) */

#define BPB_FATSz32			36		/* FAT32: FAT size [sector] (DWORD) */
#define BPB_ExtFlags32		40		/* FAT32: Extended flags (WORD) */
#define BPB_FSVer32			42		/* FAT32: Filesystem version (WORD) */
#define BPB_RootClus32		44		/* FAT32: Root directory cluster (DWORD) */
#define BPB_FSInfo32		48		/* FAT32: Offset of FSINFO sector (WORD) */
#define BPB_BkBootSec32		50		/* FAT32: Offset of backup boot sector (WORD) */
#define BS_DrvNum32			64		/* FAT32: Physical drive number for int13h (BYTE) */
#define BS_NTres32			65		/* FAT32: Error flag (BYTE) */
#define BS_BootSig32		66		/* FAT32: Extended boot signature (BYTE) */
#define BS_VolID32			67		/* FAT32: Volume serial number (DWORD) */
#define BS_VolLab32			71		/* FAT32: Volume label string (8-byte) */
#define BS_FilSysType32		82		/* FAT32: Filesystem type string (8-byte) */
#define BS_BootCode32		90		/* FAT32: Boot code (420-byte) */

#define BPB_ZeroedEx		11		/* exFAT: MBZ field (53-byte) */
#define BPB_VolOfsEx		64		/* exFAT: Volume offset from top of the drive [sector] (QWORD) */
#define BPB_TotSecEx		72		/* exFAT: Volume size [sector] (QWORD) */
#define BPB_FatOfsEx		80		/* exFAT: FAT offset from top of the volume [sector] (DWORD) */
#define BPB_FatSzEx			84		/* exFAT: FAT size [sector] (DWORD) */
#define BPB_DataOfsEx		88		/* exFAT: Data offset from top of the volume [sector] (DWORD) */
#define BPB_NumClusEx		92		/* exFAT: Number of clusters (DWORD) */
#define BPB_RootClusEx		96		/* exFAT: Root directory start cluster (DWORD) */
#define BPB_VolIDEx			100		/* exFAT: Volume serial number (DWORD) */
#define BPB_FSVerEx			104		/* exFAT: Filesystem version (WORD) */
#define BPB_VolFlagEx		106		/* exFAT: Volume flags (WORD) */
#define BPB_BytsPerSecEx	108		/* exFAT: Log2 of sector size in unit of byte (BYTE) */
#define BPB_SecPerClusEx	109		/* exFAT: Log2 of cluster size in unit of sector (BYTE) */
#define BPB_NumFATsEx		110		/* exFAT: Number of FATs (BYTE) */
#define BPB_DrvNumEx		111		/* exFAT: Physical drive number for int13h (BYTE) */
#define BPB_PercInUseEx		112		/* exFAT: Percent in use (BYTE) */
#define BPB_RsvdEx			113		/* exFAT: Reserved (7-byte) */
#define BS_BootCodeEx		120		/* exFAT: Boot code (390-byte) */

#define DIR_Name			0		/* Short file name (11-byte) */
#define DIR_Attr			11		/* Attribute (BYTE) */
#define DIR_NTres			12		/* Lower case flag (BYTE) */
#define DIR_CrtTime10		13		/* Created time sub-second (BYTE) */
#define DIR_CrtTime			14		/* Created time (DWORD) */
#define DIR_LstAccDate		18		/* Last accessed date (WORD) */
#define DIR_FstClusHI		20		/* Higher 16-bit of first cluster (WORD) */
#define DIR_ModTime			22		/* Modified time (DWORD) */
#define DIR_FstClusLO		26		/* Lower 16-bit of first cluster (WORD) */
#define DIR_FileSize		28		/* File size (DWORD) */
#define LDIR_Ord			0		/* LFN: LFN order and LLE flag (BYTE) */
#define LDIR_Attr			11		/* LFN: LFN attribute (BYTE) */
#define LDIR_Type			12		/* LFN: Entry type (BYTE) */
#define LDIR_Chksum			13		/* LFN: Checksum of the SFN (BYTE) */
#define LDIR_FstClusLO		26		/* LFN: MBZ field (WORD) */
#define XDIR_Type			0		/* exFAT: Type of exFAT directory entry (BYTE) */
#define XDIR_NumLabel		1		/* exFAT: Number of volume label characters (BYTE) */
#define XDIR_Label			2		/* exFAT: Volume label (11-WORD) */
#define XDIR_CaseSum		4		/* exFAT: Sum of case conversion table (DWORD) */
#define XDIR_NumSec			1		/* exFAT: Number of secondary entries (BYTE) */
#define XDIR_SetSum			2		/* exFAT: Sum of the set of directory entries (WORD) */
#define XDIR_Attr			4		/* exFAT: File attribute (WORD) */
#define XDIR_CrtTime		8		/* exFAT: Created time (DWORD) */
#define XDIR_ModTime		12		/* exFAT: Modified time (DWORD) */
#define XDIR_AccTime		16		/* exFAT: Last accessed time (DWORD) */
#define XDIR_CrtTime10		20		/* exFAT: Created time subsecond (BYTE) */
#define XDIR_ModTime10		21		/* exFAT: Modified time subsecond (BYTE) */
#define XDIR_CrtTZ			22		/* exFAT: Created timezone (BYTE) */
#define XDIR_ModTZ			23		/* exFAT: Modified timezone (BYTE) */
#define XDIR_AccTZ			24		/* exFAT: Last accessed timezone (BYTE) */
#define XDIR_GenFlags		33		/* exFAT: General secondary flags (BYTE) */
#define XDIR_NumName		35		/* exFAT: Number of file name characters (BYTE) */
#define XDIR_NameHash		36		/* exFAT: Hash of file name (WORD) */
#define XDIR_ValidFileSize	40		/* exFAT: Valid file size (QWORD) */
#define XDIR_FstClus		52		/* exFAT: First cluster of the file data (DWORD) */
#define XDIR_FileSize		56		/* exFAT: File/Directory size (QWORD) */

#define SZDIRE				32		/* Size of a directory entry */
#define DDEM				0xE5	/* Deleted directory entry mark set to DIR_Name[0] */
#define RDDEM				0x05	/* Replacement of the character collides with DDEM */
#define LLEF				0x40	/* Last long entry flag in LDIR_Ord */

#define FSI_LeadSig			0		/* FAT32 FSI: Leading signature (DWORD) */
#define FSI_StrucSig		484		/* FAT32 FSI: Structure signature (DWORD) */
#define FSI_Free_Count		488		/* FAT32 FSI: Number of free clusters (DWORD) */
#define FSI_Nxt_Free		492		/* FAT32 FSI: Last allocated cluster (DWORD) */

#define MBR_Table			446		/* MBR: Offset of partition table in the MBR */
#define SZ_PTE				16		/* MBR: Size of a partition table entry */
#define PTE_Boot			0		/* MBR PTE: Boot indicator */
#define PTE_StHead			1		/* MBR PTE: Start head */
#define PTE_StSec			2		/* MBR PTE: Start sector */
#define PTE_StCyl			3		/* MBR PTE: Start cylinder */
#define PTE_System			4		/* MBR PTE: System ID */
#define PTE_EdHead			5		/* MBR PTE: End head */
#define PTE_EdSec			6		/* MBR PTE: End sector */
#define PTE_EdCyl			7		/* MBR PTE: End cylinder */
#define PTE_StLba			8		/* MBR PTE: Start in LBA */
#define PTE_SizLba			12		/* MBR PTE: Size in LBA */

#define GPTH_Sign			0		/* GPT: Header signature (8-byte) */
#define GPTH_Rev			8		/* GPT: Revision (DWORD) */
#define GPTH_Size			12		/* GPT: Header size (DWORD) */
#define GPTH_Bcc			16		/* GPT: Header BCC (DWORD) */
#define GPTH_CurLba			24		/* GPT: Main header LBA (QWORD) */
#define GPTH_BakLba			32		/* GPT: Backup header LBA (QWORD) */
#define GPTH_FstLba			40		/* GPT: First LBA for partitions (QWORD) */
#define GPTH_LstLba			48		/* GPT: Last LBA for partitions (QWORD) */
#define GPTH_DskGuid		56		/* GPT: Disk GUID (16-byte) */
#define GPTH_PtOfs			72		/* GPT: Partation table LBA (QWORD) */
#define GPTH_PtNum			80		/* GPT: Number of table entries (DWORD) */
#define GPTH_PteSize		84		/* GPT: Size of table entry (DWORD) */
#define GPTH_PtBcc			88		/* GPT: Partation table BCC (DWORD) */
#define SZ_GPTE				128		/* GPT: Size of partition table entry */
#define GPTE_PtGuid			0		/* GPT PTE: Partition type GUID (16-byte) */
#define GPTE_UpGuid			16		/* GPT PTE: Partition unique GUID (16-byte) */
#define GPTE_FstLba			32		/* GPT PTE: First LBA (QWORD) */
#define GPTE_LstLba			40		/* GPT PTE: Last LBA inclusive (QWORD) */
#define GPTE_Flags			48		/* GPT PTE: Flags (QWORD) */
#define GPTE_Name			56		/* GPT PTE: Name */


/* Post process on fatal error in the file operations */
#define ABORT(fs, res)		{ fp->err = (BYTE)(res); LEAVE_FF(fs, res); }


/* Re-entrancy related */
#if FF_FS_REENTRANT
#if FF_USE_LFN == 1
#error Static LFN work area cannot be used at thread-safe configuration
#endif
#define LEAVE_FF(fs, res)	{ unlock_fs(fs, res); return res; }
#else
#define LEAVE_FF(fs, res)	return res
#endif


/* Definitions of logical drive - physical location conversion */
#if FF_MULTI_PARTITION
#define LD2PD(vol) VolToPart[vol].pd	/* Get physical drive number */
#define LD2PT(vol) VolToPart[vol].pt	/* Get partition index */
#else
#define LD2PD(vol) (BYTE)(vol)	/* Each logical drive is associated with the same physical drive number */
#define LD2PT(vol) 0			/* Find first valid partition or in SFD */
#endif


/* Definitions of sector size */
#if (FF_MAX_SS < FF_MIN_SS) || (FF_MAX_SS != 512 && FF_MAX_SS != 1024 && FF_MAX_SS != 2048 && FF_MAX_SS != 4096) || (FF_MIN_SS != 512 && FF_MIN_SS != 1024 && FF_MIN_SS != 2048 && FF_MIN_SS != 4096)
#error Wrong sector size configuration
#endif
#if FF_MAX_SS == FF_MIN_SS
#define SS(fs)	((UINT)FF_MAX_SS)	/* Fixed sector size */
#else
#define SS(fs)	((fs)->ssize)	/* Variable sector size */
#endif


/* Timestamp */
#if FF_FS_NORTC == 1
#if FF_NORTC_YEAR < 1980 || FF_NORTC_YEAR > 2107 || FF_NORTC_MON < 1 || FF_NORTC_MON > 12 || FF_NORTC_MDAY < 1 || FF_NORTC_MDAY > 31
#error Invalid FF_FS_NORTC settings
#endif
#define GET_FATTIME()	((DWORD)(FF_NORTC_YEAR - 1980) << 25 | (DWORD)FF_NORTC_MON << 21 | (DWORD)FF_NORTC_MDAY << 16)
#else
#define GET_FATTIME()	get_fattime()
#endif


/* File lock controls */
#if FF_FS_LOCK != 0
#if FF_FS_READONLY
#error FF_FS_LOCK must be 0 at read-only configuration
#endif
typedef struct {
	FATFS *fs;		/* Object ID 1, volume (NULL:blank entry) */
	DWORD clu;		/* Object ID 2, containing directory (0:root) */
	DWORD ofs;		/* Object ID 3, offset in the directory */
	WORD ctr;		/* Object open counter, 0:none, 0x01..0xFF:read mode open count, 0x100:write mode */
} FILESEM;
#endif


/* SBCS up-case tables (\x80-\xFF) */
#define TBL_CT437  {0x80,0x9A,0x45,0x41,0x8E,0x41,0x8F,0x80,0x45,0x45,0x45,0x49,0x49,0x49,0x8E,0x8F, \
					0x90,0x92,0x92,0x4F,0x99,0x4F,0x55,0x55,0x59,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
					0x41,0x49,0x4F,0x55,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
					0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
					0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT720  {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
					0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
					0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
					0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
					0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT737  {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
					0x90,0x92,0x92,0x93,0x94,0x95,0x96,0x97,0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87, \
					0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0xAA,0x92,0x93,0x94,0x95,0x96, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
					0x97,0xEA,0xEB,0xEC,0xE4,0xED,0xEE,0xEF,0xF5,0xF0,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
					0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT771  {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
					0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
					0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDC,0xDE,0xDE, \
					0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
					0xF0,0xF0,0xF2,0xF2,0xF4,0xF4,0xF6,0xF6,0xF8,0xF8,0xFA,0xFA,0xFC,0xFC,0xFE,0xFF}
#define TBL_CT775  {0x80,0x9A,0x91,0xA0,0x8E,0x95,0x8F,0x80,0xAD,0xED,0x8A,0x8A,0xA1,0x8D,0x8E,0x8F, \
					0x90,0x92,0x92,0xE2,0x99,0x95,0x96,0x97,0x97,0x99,0x9A,0x9D,0x9C,0x9D,0x9E,0x9F, \
					0xA0,0xA1,0xE0,0xA3,0xA3,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xB5,0xB6,0xB7,0xB8,0xBD,0xBE,0xC6,0xC7,0xA5,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
					0xE0,0xE1,0xE2,0xE3,0xE5,0xE5,0xE6,0xE3,0xE8,0xE8,0xEA,0xEA,0xEE,0xED,0xEE,0xEF, \
					0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT850  {0x43,0x55,0x45,0x41,0x41,0x41,0x41,0x43,0x45,0x45,0x45,0x49,0x49,0x49,0x41,0x41, \
					0x45,0x92,0x92,0x4F,0x4F,0x4F,0x55,0x55,0x59,0x4F,0x55,0x4F,0x9C,0x4F,0x9E,0x9F, \
					0x41,0x49,0x4F,0x55,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0x41,0x41,0x41,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0x41,0x41,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD1,0xD1,0x45,0x45,0x45,0x49,0x49,0x49,0x49,0xD9,0xDA,0xDB,0xDC,0xDD,0x49,0xDF, \
					0x4F,0xE1,0x4F,0x4F,0x4F,0x4F,0xE6,0xE8,0xE8,0x55,0x55,0x55,0x59,0x59,0xEE,0xEF, \
					0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT852  {0x80,0x9A,0x90,0xB6,0x8E,0xDE,0x8F,0x80,0x9D,0xD3,0x8A,0x8A,0xD7,0x8D,0x8E,0x8F, \
					0x90,0x91,0x91,0xE2,0x99,0x95,0x95,0x97,0x97,0x99,0x9A,0x9B,0x9B,0x9D,0x9E,0xAC, \
					0xB5,0xD6,0xE0,0xE9,0xA4,0xA4,0xA6,0xA6,0xA8,0xA8,0xAA,0x8D,0xAC,0xB8,0xAE,0xAF, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBD,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC6,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD1,0xD1,0xD2,0xD3,0xD2,0xD5,0xD6,0xD7,0xB7,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
					0xE0,0xE1,0xE2,0xE3,0xE3,0xD5,0xE6,0xE6,0xE8,0xE9,0xE8,0xEB,0xED,0xED,0xDD,0xEF, \
					0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xEB,0xFC,0xFC,0xFE,0xFF}
#define TBL_CT855  {0x81,0x81,0x83,0x83,0x85,0x85,0x87,0x87,0x89,0x89,0x8B,0x8B,0x8D,0x8D,0x8F,0x8F, \
					0x91,0x91,0x93,0x93,0x95,0x95,0x97,0x97,0x99,0x99,0x9B,0x9B,0x9D,0x9D,0x9F,0x9F, \
					0xA1,0xA1,0xA3,0xA3,0xA5,0xA5,0xA7,0xA7,0xA9,0xA9,0xAB,0xAB,0xAD,0xAD,0xAE,0xAF, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB6,0xB6,0xB8,0xB8,0xB9,0xBA,0xBB,0xBC,0xBE,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC7,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD1,0xD1,0xD3,0xD3,0xD5,0xD5,0xD7,0xD7,0xDD,0xD9,0xDA,0xDB,0xDC,0xDD,0xE0,0xDF, \
					0xE0,0xE2,0xE2,0xE4,0xE4,0xE6,0xE6,0xE8,0xE8,0xEA,0xEA,0xEC,0xEC,0xEE,0xEE,0xEF, \
					0xF0,0xF2,0xF2,0xF4,0xF4,0xF6,0xF6,0xF8,0xF8,0xFA,0xFA,0xFC,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT857  {0x80,0x9A,0x90,0xB6,0x8E,0xB7,0x8F,0x80,0xD2,0xD3,0xD4,0xD8,0xD7,0x49,0x8E,0x8F, \
					0x90,0x92,0x92,0xE2,0x99,0xE3,0xEA,0xEB,0x98,0x99,0x9A,0x9D,0x9C,0x9D,0x9E,0x9E, \
					0xB5,0xD6,0xE0,0xE9,0xA5,0xA5,0xA6,0xA6,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC7,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD0,0xD1,0xD2,0xD3,0xD4,0x49,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
					0xE0,0xE1,0xE2,0xE3,0xE5,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xDE,0xED,0xEE,0xEF, \
					0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT860  {0x80,0x9A,0x90,0x8F,0x8E,0x91,0x86,0x80,0x89,0x89,0x92,0x8B,0x8C,0x98,0x8E,0x8F, \
					0x90,0x91,0x92,0x8C,0x99,0xA9,0x96,0x9D,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
					0x86,0x8B,0x9F,0x96,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
					0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
					0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT861  {0x80,0x9A,0x90,0x41,0x8E,0x41,0x8F,0x80,0x45,0x45,0x45,0x8B,0x8B,0x8D,0x8E,0x8F, \
					0x90,0x92,0x92,0x4F,0x99,0x8D,0x55,0x97,0x97,0x99,0x9A,0x9D,0x9C,0x9D,0x9E,0x9F, \
					0xA4,0xA5,0xA6,0xA7,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
					0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
					0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT862  {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
					0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
					0x41,0x49,0x4F,0x55,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
					0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
					0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT863  {0x43,0x55,0x45,0x41,0x41,0x41,0x86,0x43,0x45,0x45,0x45,0x49,0x49,0x8D,0x41,0x8F, \
					0x45,0x45,0x45,0x4F,0x45,0x49,0x55,0x55,0x98,0x4F,0x55,0x9B,0x9C,0x55,0x55,0x9F, \
					0xA0,0xA1,0x4F,0x55,0xA4,0xA5,0xA6,0xA7,0x49,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
					0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
					0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT864  {0x80,0x9A,0x45,0x41,0x8E,0x41,0x8F,0x80,0x45,0x45,0x45,0x49,0x49,0x49,0x8E,0x8F, \
					0x90,0x92,0x92,0x4F,0x99,0x4F,0x55,0x55,0x59,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
					0x41,0x49,0x4F,0x55,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
					0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
					0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT865  {0x80,0x9A,0x90,0x41,0x8E,0x41,0x8F,0x80,0x45,0x45,0x45,0x49,0x49,0x49,0x8E,0x8F, \
					0x90,0x92,0x92,0x4F,0x99,0x4F,0x55,0x55,0x59,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
					0x41,0x49,0x4F,0x55,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
					0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
					0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT866  {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
					0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
					0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
					0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
					0xF0,0xF0,0xF2,0xF2,0xF4,0xF4,0xF6,0xF6,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT869  {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
					0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x86,0x9C,0x8D,0x8F,0x90, \
					0x91,0x90,0x92,0x95,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
					0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
					0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
					0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xA4,0xA5,0xA6,0xD9,0xDA,0xDB,0xDC,0xA7,0xA8,0xDF, \
					0xA9,0xAA,0xAC,0xAD,0xB5,0xB6,0xB7,0xB8,0xBD,0xBE,0xC6,0xC7,0xCF,0xCF,0xD0,0xEF, \
					0xF0,0xF1,0xD1,0xD2,0xD3,0xF5,0xD4,0xF7,0xF8,0xF9,0xD5,0x96,0x95,0x98,0xFE,0xFF}


/* DBCS code range |----- 1st byte -----|  |----------- 2nd byte -----------| */
/*                  <------>    <------>    <------>    <------>    <------>  */
#define TBL_DC932 {0x81, 0x9F, 0xE0, 0xFC, 0x40, 0x7E, 0x80, 0xFC, 0x00, 0x00}
#define TBL_DC936 {0x81, 0xFE, 0x00, 0x00, 0x40, 0x7E, 0x80, 0xFE, 0x00, 0x00}
#define TBL_DC949 {0x81, 0xFE, 0x00, 0x00, 0x41, 0x5A, 0x61, 0x7A, 0x81, 0xFE}
#define TBL_DC950 {0x81, 0xFE, 0x00, 0x00, 0x40, 0x7E, 0xA1, 0xFE, 0x00, 0x00}


/* Macros for table definitions */
#define MERGE_2STR(a, b) a ## b
#define MKCVTBL(hd, cp) MERGE_2STR(hd, cp)




/*--------------------------------------------------------------------------

   Module Private Work Area

---------------------------------------------------------------------------*/
/* Remark: Variables defined here without initial value shall be guaranteed
/  zero/null at start-up. If not, the linker option or start-up routine is
/  not compliance with C standard. */

/*--------------------------------*/
/* File/Volume controls           */
/*--------------------------------*/

#if FF_VOLUMES < 1 || FF_VOLUMES > 10
#error Wrong FF_VOLUMES setting
#endif
static FATFS* FatFs[FF_VOLUMES];	/* Pointer to the filesystem objects (logical drives) */
static WORD Fsid;					/* Filesystem mount ID */

#if FF_FS_RPATH != 0
static BYTE CurrVol;				/* Current drive */
#endif

#if FF_FS_LOCK != 0
static FILESEM Files[FF_FS_LOCK];	/* Open object lock semaphores */
#endif

#if FF_STR_VOLUME_ID
#ifdef FF_VOLUME_STRS
static const char* const VolumeStr[FF_VOLUMES] = {FF_VOLUME_STRS};	/* Pre-defined volume ID */
#endif
#endif

#if FF_LBA64
//#if FF_MIN_GPT > 0x100000000
//#error Wrong FF_MIN_GPT setting
//#endif
static const BYTE GUID_MS_Basic[16] = {0xA2,0xA0,0xD0,0xEB,0xE5,0xB9,0x33,0x44,0x87,0xC0,0x68,0xB6,0xB7,0x26,0x99,0xC7};
#endif



/*--------------------------------*/
/* LFN/Directory working buffer   */
/*--------------------------------*/

#if FF_USE_LFN == 0		/* Non-LFN configuration */
#if FF_FS_EXFAT
#error LFN must be enabled when enable exFAT
#endif
#define DEF_NAMBUF
#define INIT_NAMBUF(fs)
#define FREE_NAMBUF()
#define LEAVE_MKFS(res)	return res

#else					/* LFN configurations */
#if FF_MAX_LFN < 12 || FF_MAX_LFN > 255
#error Wrong setting of FF_MAX_LFN
#endif
#if FF_LFN_BUF < FF_SFN_BUF || FF_SFN_BUF < 12
#error Wrong setting of FF_LFN_BUF or FF_SFN_BUF
#endif
#if FF_LFN_UNICODE < 0 || FF_LFN_UNICODE > 3
#error Wrong setting of FF_LFN_UNICODE
#endif
static const BYTE LfnOfs[] = {1,3,5,7,9,14,16,18,20,22,24,28,30};	/* FAT: Offset of LFN characters in the directory entry */
#define MAXDIRB(nc)	((nc + 44U) / 15 * SZDIRE)	/* exFAT: Size of directory entry block scratchpad buffer needed for the name length */

#if FF_USE_LFN == 1		/* LFN enabled with static working buffer */
#if FF_FS_EXFAT
static BYTE	DirBuf[MAXDIRB(FF_MAX_LFN)];	/* Directory entry block scratchpad buffer */
#endif
static WCHAR LfnBuf[FF_MAX_LFN + 1];		/* LFN working buffer */
#define DEF_NAMBUF
#define INIT_NAMBUF(fs)
#define FREE_NAMBUF()
#define LEAVE_MKFS(res)	return res

#elif FF_USE_LFN == 2 	/* LFN enabled with dynamic working buffer on the stack */
#if FF_FS_EXFAT
#define DEF_NAMBUF		WCHAR lbuf[FF_MAX_LFN+1]; BYTE dbuf[MAXDIRB(FF_MAX_LFN)];	/* LFN working buffer and directory entry block scratchpad buffer */
#define INIT_NAMBUF(fs)	{ (fs)->lfnbuf = lbuf; (fs)->dirbuf = dbuf; }
#define FREE_NAMBUF()
#else
#define DEF_NAMBUF		WCHAR lbuf[FF_MAX_LFN+1];	/* LFN working buffer */
#define INIT_NAMBUF(fs)	{ (fs)->lfnbuf = lbuf; }
#define FREE_NAMBUF()
#endif
#define LEAVE_MKFS(res)	return res

#elif FF_USE_LFN == 3 	/* LFN enabled with dynamic working buffer on the heap */
#if FF_FS_EXFAT
#define DEF_NAMBUF		WCHAR *lfn;	/* Pointer to LFN working buffer and directory entry block scratchpad buffer */
#define INIT_NAMBUF(fs)	{ lfn = ff_memalloc((FF_MAX_LFN+1)*2 + MAXDIRB(FF_MAX_LFN)); if (!lfn) LEAVE_FF(fs, FR_NOT_ENOUGH_CORE); (fs)->lfnbuf = lfn; (fs)->dirbuf = (BYTE*)(lfn+FF_MAX_LFN+1); }
#define FREE_NAMBUF()	ff_memfree(lfn)
#else
#define DEF_NAMBUF		WCHAR *lfn;	/* Pointer to LFN working buffer */
#define INIT_NAMBUF(fs)	{ lfn = ff_memalloc((FF_MAX_LFN+1)*2); if (!lfn) LEAVE_FF(fs, FR_NOT_ENOUGH_CORE); (fs)->lfnbuf = lfn; }
#define FREE_NAMBUF()	ff_memfree(lfn)
#endif
#define LEAVE_MKFS(res)	{ if (!work) ff_memfree(buf); return res; }
#define MAX_MALLOC	0x8000	/* Must be >=FF_MAX_SS */

#else
#error Wrong setting of FF_USE_LFN

#endif	/* FF_USE_LFN == 1 */
#endif	/* FF_USE_LFN == 0 */



/*--------------------------------*/
/* Code conversion tables         */
/*--------------------------------*/

#if FF_CODE_PAGE == 0		/* Run-time code page configuration */
#define CODEPAGE CodePage
static WORD CodePage;	/* Current code page */
static const BYTE *ExCvt, *DbcTbl;	/* Pointer to current SBCS up-case table and DBCS code range table below */

static const BYTE Ct437[] = TBL_CT437;
static const BYTE Ct720[] = TBL_CT720;
static const BYTE Ct737[] = TBL_CT737;
static const BYTE Ct771[] = TBL_CT771;
static const BYTE Ct775[] = TBL_CT775;
static const BYTE Ct850[] = TBL_CT850;
static const BYTE Ct852[] = TBL_CT852;
static const BYTE Ct855[] = TBL_CT855;
static const BYTE Ct857[] = TBL_CT857;
static const BYTE Ct860[] = TBL_CT860;
static const BYTE Ct861[] = TBL_CT861;
static const BYTE Ct862[] = TBL_CT862;
static const BYTE Ct863[] = TBL_CT863;
static const BYTE Ct864[] = TBL_CT864;
static const BYTE Ct865[] = TBL_CT865;
static const BYTE Ct866[] = TBL_CT866;
static const BYTE Ct869[] = TBL_CT869;
static const BYTE Dc932[] = TBL_DC932;
static const BYTE Dc936[] = TBL_DC936;
static const BYTE Dc949[] = TBL_DC949;
static const BYTE Dc950[] = TBL_DC950;

#elif FF_CODE_PAGE < 900	/* Static code page configuration (SBCS) */
#define CODEPAGE FF_CODE_PAGE
static const BYTE ExCvt[] = MKCVTBL(TBL_CT, FF_CODE_PAGE);

#else					/* Static code page configuration (DBCS) */
#define CODEPAGE FF_CODE_PAGE
static const BYTE DbcTbl[] = MKCVTBL(TBL_DC, FF_CODE_PAGE);

#endif




/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------*/
/* Load/Store multi-byte word in the FAT structure                       */
/*-----------------------------------------------------------------------*/

static WORD ld_word (const BYTE* ptr)	/*	 Load a 2-byte little-endian word */
{
	WORD rv;

	rv = ptr[1];
	rv = rv << 8 | ptr[0];
	return rv;
}

static DWORD ld_dword (const BYTE* ptr)	/* Load a 4-byte little-endian word */
{
	DWORD rv;

	rv = ptr[3];
	rv = rv << 8 | ptr[2];
	rv = rv << 8 | ptr[1];
	rv = rv << 8 | ptr[0];
	return rv;
}

#if FF_FS_EXFAT
static QWORD ld_qword (const BYTE* ptr)	/* Load an 8-byte little-endian word */
{
	QWORD rv;

	rv = ptr[7];
	rv = rv << 8 | ptr[6];
	rv = rv << 8 | ptr[5];
	rv = rv << 8 | ptr[4];
	rv = rv << 8 | ptr[3];
	rv = rv << 8 | ptr[2];
	rv = rv << 8 | ptr[1];
	rv = rv << 8 | ptr[0];
	return rv;
}
#endif

#if !FF_FS_READONLY
static void st_word (BYTE* ptr, WORD val)	/* Store a 2-byte word in little-endian */
{
	*ptr++ = (BYTE)val; val >>= 8;
	*ptr++ = (BYTE)val;
}

static void st_dword (BYTE* ptr, DWORD val)	/* Store a 4-byte word in little-endian */
{
	*ptr++ = (BYTE)val; val >>= 8;
	*ptr++ = (BYTE)val; val >>= 8;
	*ptr++ = (BYTE)val; val >>= 8;
	*ptr++ = (BYTE)val;
}

#if FF_FS_EXFAT
static void st_qword (BYTE* ptr, QWORD val)	/* Store an 8-byte word in little-endian */
{
	*ptr++ = (BYTE)val; val >>= 8;
	*ptr++ = (BYTE)val; val >>= 8;
	*ptr++ = (BYTE)val; val >>= 8;
	*ptr++ = (BYTE)val; val >>= 8;
	*ptr++ = (BYTE)val; val >>= 8;
	*ptr++ = (BYTE)val; val >>= 8;
	*ptr++ = (BYTE)val; val >>= 8;
	*ptr++ = (BYTE)val;
}
#endif
#endif	/* !FF_FS_READONLY */



/*-----------------------------------------------------------------------*/
/* String functions                                                      */
/*-----------------------------------------------------------------------*/

/* Copy memory to memory */
static void mem_cpy (void* dst, const void* src, UINT cnt)
{
	BYTE *d = (BYTE*)dst;
	const BYTE *s = (const BYTE*)src;

	if (cnt != 0) {
		do {
			*d++ = *s++;
		} while (--cnt);
	}
}


/* Fill memory block */
static void mem_set (void* dst, int val, UINT cnt)
{
	BYTE *d = (BYTE*)dst;

	do {
		*d++ = (BYTE)val;
	} while (--cnt);
}


/* Compare memory block */
static int mem_cmp (const void* dst, const void* src, UINT cnt)	/* ZR:same, NZ:different */
{
	const BYTE *d = (const BYTE *)dst, *s = (const BYTE *)src;
	int r = 0;

	do {
		r = *d++ - *s++;
	} while (--cnt && r == 0);

	return r;
}


/* Check if chr is contained in the string */
static int chk_chr (const char* str, int chr)	/* NZ:contained, ZR:not contained */
{
	while (*str && *str != chr) str++;
	return *str;
}


/* Test if the byte is DBC 1st byte */
static int dbc_1st (BYTE c)
{
#if FF_CODE_PAGE == 0		/* Variable code page */
	if (DbcTbl && c >= DbcTbl[0]) {
		if (c <= DbcTbl[1]) return 1;					/* 1st byte range 1 */
		if (c >= DbcTbl[2] && c <= DbcTbl[3]) return 1;	/* 1st byte range 2 */
	}
#elif FF_CODE_PAGE >= 900	/* DBCS fixed code page */
	if (c >= DbcTbl[0]) {
		if (c <= DbcTbl[1]) return 1;
		if (c >= DbcTbl[2] && c <= DbcTbl[3]) return 1;
	}
#else						/* SBCS fixed code page */
	if (c != 0) return 0;	/* Always false */
#endif
	return 0;
}


/* Test if the byte is DBC 2nd byte */
static int dbc_2nd (BYTE c)
{
#if FF_CODE_PAGE == 0		/* Variable code page */
	if (DbcTbl && c >= DbcTbl[4]) {
		if (c <= DbcTbl[5]) return 1;					/* 2nd byte range 1 */
		if (c >= DbcTbl[6] && c <= DbcTbl[7]) return 1;	/* 2nd byte range 2 */
		if (c >= DbcTbl[8] && c <= DbcTbl[9]) return 1;	/* 2nd byte range 3 */
	}
#elif FF_CODE_PAGE >= 900	/* DBCS fixed code page */
	if (c >= DbcTbl[4]) {
		if (c <= DbcTbl[5]) return 1;
		if (c >= DbcTbl[6] && c <= DbcTbl[7]) return 1;
		if (c >= DbcTbl[8] && c <= DbcTbl[9]) return 1;
	}
#else						/* SBCS fixed code page */
	if (c != 0) return 0;	/* Always false */
#endif
	return 0;
}


#if FF_USE_LFN

/* Get a Unicode code point from the TCHAR string in defined API encodeing */
static DWORD tchar2uni (	/* Returns a character in UTF-16 encoding (>=0x10000 on surrogate pair, 0xFFFFFFFF on decode error) */
	const TCHAR** str		/* Pointer to pointer to TCHAR string in configured encoding */
)
{
	DWORD uc;
	const TCHAR *p = *str;

#if FF_LFN_UNICODE == 1		/* UTF-16 input */
	WCHAR wc;

	uc = *p++;	/* Get a unit */
	if (IsSurrogate(uc)) {	/* Surrogate? */
		wc = *p++;		/* Get low surrogate */
		if (!IsSurrogateH(uc) || !IsSurrogateL(wc)) return 0xFFFFFFFF;	/* Wrong surrogate? */
		uc = uc << 16 | wc;
	}

#elif FF_LFN_UNICODE == 2	/* UTF-8 input */
	BYTE b;
	int nf;

	uc = (BYTE)*p++;	/* Get an encoding unit */
	if (uc & 0x80) {	/* Multiple byte code? */
		if ((uc & 0xE0) == 0xC0) {	/* 2-byte sequence? */
			uc &= 0x1F; nf = 1;
		} else {
			if ((uc & 0xF0) == 0xE0) {	/* 3-byte sequence? */
				uc &= 0x0F; nf = 2;
			} else {
				if ((uc & 0xF8) == 0xF0) {	/* 4-byte sequence? */
					uc &= 0x07; nf = 3;
				} else {					/* Wrong sequence */
					return 0xFFFFFFFF;
				}
			}
		}
		do {	/* Get trailing bytes */
			b = (BYTE)*p++;
			if ((b & 0xC0) != 0x80) return 0xFFFFFFFF;	/* Wrong sequence? */
			uc = uc << 6 | (b & 0x3F);
		} while (--nf != 0);
		if (uc < 0x80 || IsSurrogate(uc) || uc >= 0x110000) return 0xFFFFFFFF;	/* Wrong code? */
		if (uc >= 0x010000) uc = 0xD800DC00 | ((uc - 0x10000) << 6 & 0x3FF0000) | (uc & 0x3FF);	/* Make a surrogate pair if needed */
	}

#elif FF_LFN_UNICODE == 3	/* UTF-32 input */
	uc = (TCHAR)*p++;	/* Get a unit */
	if (uc >= 0x110000 || IsSurrogate(uc)) return 0xFFFFFFFF;	/* Wrong code? */
	if (uc >= 0x010000) uc = 0xD800DC00 | ((uc - 0x10000) << 6 & 0x3FF0000) | (uc & 0x3FF);	/* Make a surrogate pair if needed */

#else		/* ANSI/OEM input */
	BYTE b;
	WCHAR wc;

	wc = (BYTE)*p++;			/* Get a byte */
	if (dbc_1st((BYTE)wc)) {	/* Is it a DBC 1st byte? */
		b = (BYTE)*p++;			/* Get 2nd byte */
		if (!dbc_2nd(b)) return 0xFFFFFFFF;	/* Invalid code? */
		wc = (wc << 8) + b;		/* Make a DBC */
	}
	if (wc != 0) {
		wc = ff_oem2uni(wc, CODEPAGE);	/* ANSI/OEM ==> Unicode */
		if (wc == 0) return 0xFFFFFFFF;	/* Invalid code? */
	}
	uc = wc;

#endif
	*str = p;	/* Next read pointer */
	return uc;
}


/* Output a TCHAR string in defined API encoding */
static BYTE put_utf (	/* Returns number of encoding units written (0:buffer overflow or wrong encoding) */
	DWORD chr,	/* UTF-16 encoded character (Surrogate pair if >=0x10000) */
	TCHAR* buf,	/* Output buffer */
	UINT szb	/* Size of the buffer */
)
{
#if FF_LFN_UNICODE == 1	/* UTF-16 output */
	WCHAR hs, wc;

	hs = (WCHAR)(chr >> 16);
	wc = (WCHAR)chr;
	if (hs == 0) {	/* Single encoding unit? */
		if (szb < 1 || IsSurrogate(wc)) return 0;	/* Buffer overflow or wrong code? */
		*buf = wc;
		return 1;
	}
	if (szb < 2 || !IsSurrogateH(hs) || !IsSurrogateL(wc)) return 0;	/* Buffer overflow or wrong surrogate? */
	*buf++ = hs;
	*buf++ = wc;
	return 2;

#elif FF_LFN_UNICODE == 2	/* UTF-8 output */
	DWORD hc;

	if (chr < 0x80) {	/* Single byte code? */
		if (szb < 1) return 0;	/* Buffer overflow? */
		*buf = (TCHAR)chr;
		return 1;
	}
	if (chr < 0x800) {	/* 2-byte sequence? */
		if (szb < 2) return 0;	/* Buffer overflow? */
		*buf++ = (TCHAR)(0xC0 | (chr >> 6 & 0x1F));
		*buf++ = (TCHAR)(0x80 | (chr >> 0 & 0x3F));
		return 2;
	}
	if (chr < 0x10000) {	/* 3-byte sequence? */
		if (szb < 3 || IsSurrogate(chr)) return 0;	/* Buffer overflow or wrong code? */
		*buf++ = (TCHAR)(0xE0 | (chr >> 12 & 0x0F));
		*buf++ = (TCHAR)(0x80 | (chr >> 6 & 0x3F));
		*buf++ = (TCHAR)(0x80 | (chr >> 0 & 0x3F));
		return 3;
	}
	/* 4-byte sequence */
	if (szb < 4) return 0;	/* Buffer overflow? */
	hc = ((chr & 0xFFFF0000) - 0xD8000000) >> 6;	/* Get high 10 bits */
	chr = (chr & 0xFFFF) - 0xDC00;					/* Get low 10 bits */
	if (hc >= 0x100000 || chr >= 0x400) return 0;	/* Wrong surrogate? */
	chr = (hc | chr) + 0x10000;
	*buf++ = (TCHAR)(0xF0 | (chr >> 18 & 0x07));
	*buf++ = (TCHAR)(0x80 | (chr >> 12 & 0x3F));
	*buf++ = (TCHAR)(0x80 | (chr >> 6 & 0x3F));
	*buf++ = (TCHAR)(0x80 | (chr >> 0 & 0x3F));
	return 4;

#elif FF_LFN_UNICODE == 3	/* UTF-32 output */
	DWORD hc;

	if (szb < 1) return 0;	/* Buffer overflow? */
	if (chr >= 0x10000) {	/* Out of BMP? */
		hc = ((chr & 0xFFFF0000) - 0xD8000000) >> 6;	/* Get high 10 bits */
		chr = (chr & 0xFFFF) - 0xDC00;					/* Get low 10 bits */
		if (hc >= 0x100000 || chr >= 0x400) return 0;	/* Wrong surrogate? */
		chr = (hc | chr) + 0x10000;
	}
	*buf++ = (TCHAR)chr;
	return 1;

#else						/* ANSI/OEM output */
	WCHAR wc;

	wc = ff_uni2oem(chr, CODEPAGE);
	if (wc >= 0x100) {	/* Is this a DBC? */
		if (szb < 2) return 0;
		*buf++ = (char)(wc >> 8);	/* Store DBC 1st byte */
		*buf++ = (TCHAR)wc;			/* Store DBC 2nd byte */
		return 2;
	}
	if (wc == 0 || szb < 1) return 0;	/* Invalid char or buffer overflow? */
	*buf++ = (TCHAR)wc;					/* Store the character */
	return 1;
#endif
}
#endif	/* FF_USE_LFN */


#if FF_FS_REENTRANT
/*-----------------------------------------------------------------------*/
/* Request/Release grant to access the volume                            */
/*-----------------------------------------------------------------------*/
static int lock_fs (		/* 1:Ok, 0:timeout */
	FATFS* fs		/* Filesystem object */
)
{
	return ff_req_grant(fs->sobj);
}


static void unlock_fs (
	FATFS* fs,		/* Filesystem object */
	FRESULT res		/* Result code to be returned */
)
{
	if (fs && res != FR_NOT_ENABLED && res != FR_INVALID_DRIVE && res != FR_TIMEOUT) {
		ff_rel_grant(fs->sobj);
	}
}

#endif



#if FF_FS_LOCK != 0
/*-----------------------------------------------------------------------*/
/* File lock control functions                                           */
/*-----------------------------------------------------------------------*/

static FRESULT chk_lock (	/* Check if the file can be accessed */
	DIR* dp,		/* Directory object pointing the file to be checked */
	int acc			/* Desired access type (0:Read mode open, 1:Write mode open, 2:Delete or rename) */
)
{
	UINT i, be;

	/* Search open object table for the object */
	be = 0;
	for (i = 0; i < FF_FS_LOCK; i++) {
		if (Files[i].fs) {	/* Existing entry */
			if (Files[i].fs == dp->obj.fs &&	 	/* Check if the object matches with an open object */
				Files[i].clu == dp->obj.sclust &&
				Files[i].ofs == dp->dptr) break;
		} else {			/* Blank entry */
			be = 1;
		}
	}
	if (i == FF_FS_LOCK) {	/* The object has not been opened */
		return (!be && acc != 2) ? FR_TOO_MANY_OPEN_FILES : FR_OK;	/* Is there a blank entry for new object? */
	}

	/* The object was opened. Reject any open against writing file and all write mode open */
	return (acc != 0 || Files[i].ctr == 0x100) ? FR_LOCKED : FR_OK;
}


static int enq_lock (void)	/* Check if an entry is available for a new object */
{
	UINT i;

	for (i = 0; i < FF_FS_LOCK && Files[i].fs; i++) ;
	return (i == FF_FS_LOCK) ? 0 : 1;
}


static UINT inc_lock (	/* Increment object open counter and returns its index (0:Internal error) */
	DIR* dp,	/* Directory object pointing the file to register or increment */
	int acc		/* Desired access (0:Read, 1:Write, 2:Delete/Rename) */
)
{
	UINT i;


	for (i = 0; i < FF_FS_LOCK; i++) {	/* Find the object */
		if (Files[i].fs == dp->obj.fs
		 && Files[i].clu == dp->obj.sclust
		 && Files[i].ofs == dp->dptr) break;
	}

	if (i == FF_FS_LOCK) {				/* Not opened. Register it as new. */
		for (i = 0; i < FF_FS_LOCK && Files[i].fs; i++) ;
		if (i == FF_FS_LOCK) return 0;	/* No free entry to register (int err) */
		Files[i].fs = dp->obj.fs;
		Files[i].clu = dp->obj.sclust;
		Files[i].ofs = dp->dptr;
		Files[i].ctr = 0;
	}

	if (acc >= 1 && Files[i].ctr) return 0;	/* Access violation (int err) */

	Files[i].ctr = acc ? 0x100 : Files[i].ctr + 1;	/* Set semaphore value */

	return i + 1;	/* Index number origin from 1 */
}


static FRESULT dec_lock (	/* Decrement object open counter */
	UINT i			/* Semaphore index (1..) */
)
{
	WORD n;
	FRESULT res;


	if (--i < FF_FS_LOCK) {	/* Index number origin from 0 */
		n = Files[i].ctr;
		if (n == 0x100) n = 0;		/* If write mode open, delete the entry */
		if (n > 0) n--;				/* Decrement read mode open count */
		Files[i].ctr = n;
		if (n == 0) Files[i].fs = 0;	/* Delete the entry if open count gets zero */
		res = FR_OK;
	} else {
		res = FR_INT_ERR;			/* Invalid index nunber */
	}
	return res;
}


static void clear_lock (	/* Clear lock entries of the volume */
	FATFS *fs
)
{
	UINT i;

	for (i = 0; i < FF_FS_LOCK; i++) {
		if (Files[i].fs == fs) Files[i].fs = 0;
	}
}

#endif	/* FF_FS_LOCK != 0 */



/*-----------------------------------------------------------------------*/
/* Move/Flush disk access window in the filesystem object                */
/*-----------------------------------------------------------------------*/
#if !FF_FS_READONLY
static FRESULT sync_window (	/* Returns FR_OK or FR_DISK_ERR */
	FATFS* fs			/* Filesystem object */
)
{
	FRESULT res = FR_OK;


	if (fs->wflag) {	/* Is the disk access window dirty? */
		if (disk_write(fs->pdrv, fs->win, fs->winsect, 1) == RES_OK) {	/* Write it back into the volume */
			fs->wflag = 0;	/* Clear window dirty flag */
			if (fs->winsect - fs->fatbase < fs->fsize) {	/* Is it in the 1st FAT? */
				if (fs->n_fats == 2) disk_write(fs->pdrv, fs->win, fs->winsect + fs->fsize, 1);	/* Reflect it to 2nd FAT if needed */
			}
		} else {
			res = FR_DISK_ERR;
		}
	}
	return res;
}
#endif


static FRESULT move_window (	/* Returns FR_OK or FR_DISK_ERR */
	FATFS* fs,		/* Filesystem object */
	LBA_t sect		/* Sector LBA to make appearance in the fs->win[] */
)
{
	FRESULT res = FR_OK;


	if (sect != fs->winsect) {	/* Window offset changed? */
#if !FF_FS_READONLY
		res = sync_window(fs);		/* Flush the window */
#endif
		if (res == FR_OK) {			/* Fill sector window with new data */
			if (disk_read(fs->pdrv, fs->win, sect, 1) != RES_OK) {
				sect = (LBA_t)0 - 1;	/* Invalidate window if read data is not valid */
				res = FR_DISK_ERR;
			}
			fs->winsect = sect;
		}
	}
	return res;
}




#if !FF_FS_READONLY
/*-----------------------------------------------------------------------*/
/* Synchronize filesystem and data on the storage                        */
/*-----------------------------------------------------------------------*/

static FRESULT sync_fs (	/* Returns FR_OK or FR_DISK_ERR */
	FATFS* fs		/* Filesystem object */
)
{
	FRESULT res;


	res = sync_window(fs);
	if (res == FR_OK) {
		if (fs->fs_type == FS_FAT32 && fs->fsi_flag == 1) {	/* FAT32: Update FSInfo sector if needed */
			/* Create FSInfo structure */
			mem_set(fs->win, 0, sizeof fs->win);
			st_word(fs->win + BS_55AA, 0xAA55);
			st_dword(fs->win + FSI_LeadSig, 0x41615252);
			st_dword(fs->win + FSI_StrucSig, 0x61417272);
			st_dword(fs->win + FSI_Free_Count, fs->free_clst);
			st_dword(fs->win + FSI_Nxt_Free, fs->last_clst);
			/* Write it into the FSInfo sector */
			fs->winsect = fs->volbase + 1;
			disk_write(fs->pdrv, fs->win, fs->winsect, 1);
			fs->fsi_flag = 0;
		}
		/* Make sure that no pending write process in the lower layer */
		if (disk_ioctl(fs->pdrv, CTRL_SYNC, 0) != RES_OK) res = FR_DISK_ERR;
	}

	return res;
}

#endif



/*-----------------------------------------------------------------------*/
/* Get physical sector number from cluster number                        */
/*-----------------------------------------------------------------------*/

static LBA_t clst2sect (	/* !=0:Sector number, 0:Failed (invalid cluster#) */
	FATFS* fs,		/* Filesystem object */
	DWORD clst		/* Cluster# to be converted */
)
{
	clst -= 2;		/* Cluster number is origin from 2 */
	if (clst >= fs->n_fatent - 2) return 0;		/* Is it invalid cluster number? */
	return fs->database + (LBA_t)fs->csize * clst;	/* Start sector number of the cluster */
}




/*-----------------------------------------------------------------------*/
/* FAT access - Read value of a FAT entry                                */
/*-----------------------------------------------------------------------*/

static DWORD get_fat (		/* 0xFFFFFFFF:Disk error, 1:Internal error, 2..0x7FFFFFFF:Cluster status */
	FFOBJID* obj,	/* Corresponding object */
	DWORD clst		/* Cluster number to get the value */
)
{
	UINT wc, bc;
	DWORD val;
	FATFS *fs = obj->fs;


	if (clst < 2 || clst >= fs->n_fatent) {	/* Check if in valid range */
		val = 1;	/* Internal error */

	} else {
		val = 0xFFFFFFFF;	/* Default value falls on disk error */

		switch (fs->fs_type) {
		case FS_FAT12 :
			bc = (UINT)clst; bc += bc / 2;
			if (move_window(fs, fs->fatbase + (bc / SS(fs))) != FR_OK) break;
			wc = fs->win[bc++ % SS(fs)];		/* Get 1st byte of the entry */
			if (move_window(fs, fs->fatbase + (bc / SS(fs))) != FR_OK) break;
			wc |= fs->win[bc % SS(fs)] << 8;	/* Merge 2nd byte of the entry */
			val = (clst & 1) ? (wc >> 4) : (wc & 0xFFF);	/* Adjust bit position */
			break;

		case FS_FAT16 :
			if (move_window(fs, fs->fatbase + (clst / (SS(fs) / 2))) != FR_OK) break;
			val = ld_word(fs->win + clst * 2 % SS(fs));		/* Simple WORD array */
			break;

		case FS_FAT32 :
			if (move_window(fs, fs->fatbase + (clst / (SS(fs) / 4))) != FR_OK) break;
			val = ld_dword(fs->win + clst * 4 % SS(fs)) & 0x0FFFFFFF;	/* Simple DWORD array but mask out upper 4 bits */
			break;
#if FF_FS_EXFAT
		case FS_EXFAT :
			if ((obj->objsize != 0 && obj->sclust != 0) || obj->stat == 0) {	/* Object except root dir must have valid data length */
				DWORD cofs = clst - obj->sclust;	/* Offset from start cluster */
				DWORD clen = (DWORD)((LBA_t)((obj->objsize - 1) / SS(fs)) / fs->csize);	/* Number of clusters - 1 */

				if (obj->stat == 2 && cofs <= clen) {	/* Is it a contiguous chain? */
					val = (cofs == clen) ? 0x7FFFFFFF : clst + 1;	/* No data on the FAT, generate the value */
					break;
				}
				if (obj->stat == 3 && cofs < obj->n_cont) {	/* Is it in the 1st fragment? */
					val = clst + 1; 	/* Generate the value */
					break;
				}
				if (obj->stat != 2) {	/* Get value from FAT if FAT chain is valid */
					if (obj->n_frag != 0) {	/* Is it on the growing edge? */
						val = 0x7FFFFFFF;	/* Generate EOC */
					} else {
						if (move_window(fs, fs->fatbase + (clst / (SS(fs) / 4))) != FR_OK) break;
						val = ld_dword(fs->win + clst * 4 % SS(fs)) & 0x7FFFFFFF;
					}
					break;
				}
			}
			/* go to default */
#endif
		default:
			val = 1;	/* Internal error */
		}
	}

	return val;
}




#if !FF_FS_READONLY
/*-----------------------------------------------------------------------*/
/* FAT access - Change value of a FAT entry                              */
/*-----------------------------------------------------------------------*/

static FRESULT put_fat (	/* FR_OK(0):succeeded, !=0:error */
	FATFS* fs,		/* Corresponding filesystem object */
	DWORD clst,		/* FAT index number (cluster number) to be changed */
	DWORD val		/* New value to be set to the entry */
)
{
	UINT bc;
	BYTE *p;
	FRESULT res = FR_INT_ERR;


	if (clst >= 2 && clst < fs->n_fatent) {	/* Check if in valid range */
		switch (fs->fs_type) {
		case FS_FAT12 :
			bc = (UINT)clst; bc += bc / 2;	/* bc: byte offset of the entry */
			res = move_window(fs, fs->fatbase + (bc / SS(fs)));
			if (res != FR_OK) break;
			p = fs->win + bc++ % SS(fs);
			*p = (clst & 1) ? ((*p & 0x0F) | ((BYTE)val << 4)) : (BYTE)val;		/* Update 1st byte */
			fs->wflag = 1;
			res = move_window(fs, fs->fatbase + (bc / SS(fs)));
			if (res != FR_OK) break;
			p = fs->win + bc % SS(fs);
			*p = (clst & 1) ? (BYTE)(val >> 4) : ((*p & 0xF0) | ((BYTE)(val >> 8) & 0x0F));	/* Update 2nd byte */
			fs->wflag = 1;
			break;

		case FS_FAT16 :
			res = move_window(fs, fs->fatbase + (clst / (SS(fs) / 2)));
			if (res != FR_OK) break;
			st_word(fs->win + clst * 2 % SS(fs), (WORD)val);	/* Simple WORD array */
			fs->wflag = 1;
			break;

		case FS_FAT32 :
#if FF_FS_EXFAT
		case FS_EXFAT :
#endif
			res = move_window(fs, fs->fatbase + (clst / (SS(fs) / 4)));
			if (res != FR_OK) break;
			if (!FF_FS_EXFAT || fs->fs_type != FS_EXFAT) {
				val = (val & 0x0FFFFFFF) | (ld_dword(fs->win + clst * 4 % SS(fs)) & 0xF0000000);
			}
			st_dword(fs->win + clst * 4 % SS(fs), val);
			fs->wflag = 1;
			break;
		}
	}
	return res;
}

#endif /* !FF_FS_READONLY */




#if FF_FS_EXFAT && !FF_FS_READONLY
/*-----------------------------------------------------------------------*/
/* exFAT: Accessing FAT and Allocation Bitmap                            */
/*-----------------------------------------------------------------------*/

/*--------------------------------------*/
/* Find a contiguous free cluster block */
/*--------------------------------------*/

static DWORD find_bitmap (	/* 0:Not found, 2..:Cluster block found, 0xFFFFFFFF:Disk error */
	FATFS* fs,	/* Filesystem object */
	DWORD clst,	/* Cluster number to scan from */
	DWORD ncl	/* Number of contiguous clusters to find (1..) */
)
{
	BYTE bm, bv;
	UINT i;
	DWORD val, scl, ctr;


	clst -= 2;	/* The first bit in the bitmap corresponds to cluster #2 */
	if (clst >= fs->n_fatent - 2) clst = 0;
	scl = val = clst; ctr = 0;
	for (;;) {
		if (move_window(fs, fs->bitbase + val / 8 / SS(fs)) != FR_OK) return 0xFFFFFFFF;
		i = val / 8 % SS(fs); bm = 1 << (val % 8);
		do {
			do {
				bv = fs->win[i] & bm; bm <<= 1;		/* Get bit value */
				if (++val >= fs->n_fatent - 2) {	/* Next cluster (with wrap-around) */
					val = 0; bm = 0; i = SS(fs);
				}
				if (bv == 0) {	/* Is it a free cluster? */
					if (++ctr == ncl) return scl + 2;	/* Check if run length is sufficient for required */
				} else {
					scl = val; ctr = 0;		/* Encountered a cluster in-use, restart to scan */
				}
				if (val == clst) return 0;	/* All cluster scanned? */
			} while (bm != 0);
			bm = 1;
		} while (++i < SS(fs));
	}
}


/*----------------------------------------*/
/* Set/Clear a block of allocation bitmap */
/*----------------------------------------*/

static FRESULT change_bitmap (
	FATFS* fs,	/* Filesystem object */
	DWORD clst,	/* Cluster number to change from */
	DWORD ncl,	/* Number of clusters to be changed */
	int bv		/* bit value to be set (0 or 1) */
)
{
	BYTE bm;
	UINT i;
	LBA_t sect;


	clst -= 2;	/* The first bit corresponds to cluster #2 */
	sect = fs->bitbase + clst / 8 / SS(fs);	/* Sector address */
	i = clst / 8 % SS(fs);					/* Byte offset in the sector */
	bm = 1 << (clst % 8);					/* Bit mask in the byte */
	for (;;) {
		if (move_window(fs, sect++) != FR_OK) return FR_DISK_ERR;
		do {
			do {
				if (bv == (int)((fs->win[i] & bm) != 0)) return FR_INT_ERR;	/* Is the bit expected value? */
				fs->win[i] ^= bm;	/* Flip the bit */
				fs->wflag = 1;
				if (--ncl == 0) return FR_OK;	/* All bits processed? */
			} while (bm <<= 1);		/* Next bit */
			bm = 1;
		} while (++i < SS(fs));		/* Next byte */
		i = 0;
	}
}


/*---------------------------------------------*/
/* Fill the first fragment of the FAT chain    */
/*---------------------------------------------*/

static FRESULT fill_first_frag (
	FFOBJID* obj	/* Pointer to the corresponding object */
)
{
	FRESULT res;
	DWORD cl, n;


	if (obj->stat == 3) {	/* Has the object been changed 'fragmented' in this session? */
		for (cl = obj->sclust, n = obj->n_cont; n; cl++, n--) {	/* Create cluster chain on the FAT */
			res = put_fat(obj->fs, cl, cl + 1);
			if (res != FR_OK) return res;
		}
		obj->stat = 0;	/* Change status 'FAT chain is valid' */
	}
	return FR_OK;
}


/*---------------------------------------------*/
/* Fill the last fragment of the FAT chain     */
/*---------------------------------------------*/

static FRESULT fill_last_frag (
	FFOBJID* obj,	/* Pointer to the corresponding object */
	DWORD lcl,		/* Last cluster of the fragment */
	DWORD term		/* Value to set the last FAT entry */
)
{
	FRESULT res;


	while (obj->n_frag > 0) {	/* Create the chain of last fragment */
		res = put_fat(obj->fs, lcl - obj->n_frag + 1, (obj->n_frag > 1) ? lcl - obj->n_frag + 2 : term);
		if (res != FR_OK) return res;
		obj->n_frag--;
	}
	return FR_OK;
}

#endif	/* FF_FS_EXFAT && !FF_FS_READONLY */



#if !FF_FS_READONLY
/*-----------------------------------------------------------------------*/
/* FAT handling - Remove a cluster chain                                 */
/*-----------------------------------------------------------------------*/

static FRESULT remove_chain (	/* FR_OK(0):succeeded, !=0:error */
	FFOBJID* obj,		/* Corresponding object */
	DWORD clst,			/* Cluster to remove a chain from */
	DWORD pclst			/* Previous cluster of clst (0 if entire chain) */
)
{
	FRESULT res = FR_OK;
	DWORD nxt;
	FATFS *fs = obj->fs;
#if FF_FS_EXFAT || FF_USE_TRIM
	DWORD scl = clst, ecl = clst;
#endif
#if FF_USE_TRIM
	LBA_t rt[2];
#endif

	if (clst < 2 || clst >= fs->n_fatent) return FR_INT_ERR;	/* Check if in valid range */

	/* Mark the previous cluster 'EOC' on the FAT if it exists */
	if (pclst != 0 && (!FF_FS_EXFAT || fs->fs_type != FS_EXFAT || obj->stat != 2)) {
		res = put_fat(fs, pclst, 0xFFFFFFFF);
		if (res != FR_OK) return res;
	}

	/* Remove the chain */
	do {
		nxt = get_fat(obj, clst);			/* Get cluster status */
		if (nxt == 0) break;				/* Empty cluster? */
		if (nxt == 1) return FR_INT_ERR;	/* Internal error? */
		if (nxt == 0xFFFFFFFF) return FR_DISK_ERR;	/* Disk error? */
		if (!FF_FS_EXFAT || fs->fs_type != FS_EXFAT) {
			res = put_fat(fs, clst, 0);		/* Mark the cluster 'free' on the FAT */
			if (res != FR_OK) return res;
		}
		if (fs->free_clst < fs->n_fatent - 2) {	/* Update FSINFO */
			fs->free_clst++;
			fs->fsi_flag |= 1;
		}
#if FF_FS_EXFAT || FF_USE_TRIM
		if (ecl + 1 == nxt) {	/* Is next cluster contiguous? */
			ecl = nxt;
		} else {				/* End of contiguous cluster block */
#if FF_FS_EXFAT
			if (fs->fs_type == FS_EXFAT) {
				res = change_bitmap(fs, scl, ecl - scl + 1, 0);	/* Mark the cluster block 'free' on the bitmap */
				if (res != FR_OK) return res;
			}
#endif
#if FF_USE_TRIM
			rt[0] = clst2sect(fs, scl);					/* Start of data area to be freed */
			rt[1] = clst2sect(fs, ecl) + fs->csize - 1;	/* End of data area to be freed */
			disk_ioctl(fs->pdrv, CTRL_TRIM, rt);		/* Inform storage device that the data in the block may be erased */
#endif
			scl = ecl = nxt;
		}
#endif
		clst = nxt;					/* Next cluster */
	} while (clst < fs->n_fatent);	/* Repeat while not the last link */

#if FF_FS_EXFAT
	/* Some post processes for chain status */
	if (fs->fs_type == FS_EXFAT) {
		if (pclst == 0) {	/* Has the entire chain been removed? */
			obj->stat = 0;		/* Change the chain status 'initial' */
		} else {
			if (obj->stat == 0) {	/* Is it a fragmented chain from the beginning of this session? */
				clst = obj->sclust;		/* Follow the chain to check if it gets contiguous */
				while (clst != pclst) {
					nxt = get_fat(obj, clst);
					if (nxt < 2) return FR_INT_ERR;
					if (nxt == 0xFFFFFFFF) return FR_DISK_ERR;
					if (nxt != clst + 1) break;	/* Not contiguous? */
					clst++;
				}
				if (clst == pclst) {	/* Has the chain got contiguous again? */
					obj->stat = 2;		/* Change the chain status 'contiguous' */
				}
			} else {
				if (obj->stat == 3 && pclst >= obj->sclust && pclst <= obj->sclust + obj->n_cont) {	/* Was the chain fragmented in this session and got contiguous again? */
					obj->stat = 2;	/* Change the chain status 'contiguous' */
				}
			}
		}
	}
#endif
	return FR_OK;
}




/*-----------------------------------------------------------------------*/
/* FAT handling - Stretch a chain or Create a new chain                  */
/*-----------------------------------------------------------------------*/

static DWORD create_chain (	/* 0:No free cluster, 1:Internal error, 0xFFFFFFFF:Disk error, >=2:New cluster# */
	FFOBJID* obj,		/* Corresponding object */
	DWORD clst			/* Cluster# to stretch, 0:Create a new chain */
)
{
	DWORD cs, ncl, scl;
	FRESULT res;
	FATFS *fs = obj->fs;


	if (clst == 0) {	/* Create a new chain */
		scl = fs->last_clst;				/* Suggested cluster to start to find */
		if (scl == 0 || scl >= fs->n_fatent) scl = 1;
	}
	else {				/* Stretch a chain */
		cs = get_fat(obj, clst);			/* Check the cluster status */
		if (cs < 2) return 1;				/* Test for insanity */
		if (cs == 0xFFFFFFFF) return cs;	/* Test for disk error */
		if (cs < fs->n_fatent) return cs;	/* It is already followed by next cluster */
		scl = clst;							/* Cluster to start to find */
	}
	if (fs->free_clst == 0) return 0;		/* No free cluster */

#if FF_FS_EXFAT
	if (fs->fs_type == FS_EXFAT) {	/* On the exFAT volume */
		ncl = find_bitmap(fs, scl, 1);				/* Find a free cluster */
		if (ncl == 0 || ncl == 0xFFFFFFFF) return ncl;	/* No free cluster or hard error? */
		res = change_bitmap(fs, ncl, 1, 1);			/* Mark the cluster 'in use' */
		if (res == FR_INT_ERR) return 1;
		if (res == FR_DISK_ERR) return 0xFFFFFFFF;
		if (clst == 0) {							/* Is it a new chain? */
			obj->stat = 2;							/* Set status 'contiguous' */
		} else {									/* It is a stretched chain */
			if (obj->stat == 2 && ncl != scl + 1) {	/* Is the chain got fragmented? */
				obj->n_cont = scl - obj->sclust;	/* Set size of the contiguous part */
				obj->stat = 3;						/* Change status 'just fragmented' */
			}
		}
		if (obj->stat != 2) {	/* Is the file non-contiguous? */
			if (ncl == clst + 1) {	/* Is the cluster next to previous one? */
				obj->n_frag = obj->n_frag ? obj->n_frag + 1 : 2;	/* Increment size of last framgent */
			} else {				/* New fragment */
				if (obj->n_frag == 0) obj->n_frag = 1;
				res = fill_last_frag(obj, clst, ncl);	/* Fill last fragment on the FAT and link it to new one */
				if (res == FR_OK) obj->n_frag = 1;
			}
		}
	} else
#endif
	{	/* On the FAT/FAT32 volume */
		ncl = 0;
		if (scl == clst) {						/* Stretching an existing chain? */
			ncl = scl + 1;						/* Test if next cluster is free */
			if (ncl >= fs->n_fatent) ncl = 2;
			cs = get_fat(obj, ncl);				/* Get next cluster status */
			if (cs == 1 || cs == 0xFFFFFFFF) return cs;	/* Test for error */
			if (cs != 0) {						/* Not free? */
				cs = fs->last_clst;				/* Start at suggested cluster if it is valid */
				if (cs >= 2 && cs < fs->n_fatent) scl = cs;
				ncl = 0;
			}
		}
		if (ncl == 0) {	/* The new cluster cannot be contiguous and find another fragment */
			ncl = scl;	/* Start cluster */
			for (;;) {
				ncl++;							/* Next cluster */
				if (ncl >= fs->n_fatent) {		/* Check wrap-around */
					ncl = 2;
					if (ncl > scl) return 0;	/* No free cluster found? */
				}
				cs = get_fat(obj, ncl);			/* Get the cluster status */
				if (cs == 0) break;				/* Found a free cluster? */
				if (cs == 1 || cs == 0xFFFFFFFF) return cs;	/* Test for error */
				if (ncl == scl) return 0;		/* No free cluster found? */
			}
		}
		res = put_fat(fs, ncl, 0xFFFFFFFF);		/* Mark the new cluster 'EOC' */
		if (res == FR_OK && clst != 0) {
			res = put_fat(fs, clst, ncl);		/* Link it from the previous one if needed */
		}
	}

	if (res == FR_OK) {			/* Update FSINFO if function succeeded. */
		fs->last_clst = ncl;
		if (fs->free_clst <= fs->n_fatent - 2) fs->free_clst--;
		fs->fsi_flag |= 1;
	} else {
		ncl = (res == FR_DISK_ERR) ? 0xFFFFFFFF : 1;	/* Failed. Generate error status */
	}

	return ncl;		/* Return new cluster number or error status */
}

#endif /* !FF_FS_READONLY */




#if FF_USE_FASTSEEK
/*-----------------------------------------------------------------------*/
/* FAT handling - Convert offset into cluster with link map table        */
/*-----------------------------------------------------------------------*/

static DWORD clmt_clust (	/* <2:Error, >=2:Cluster number */
	FIL* fp,		/* Pointer to the file object */
	FSIZE_t ofs		/* File offset to be converted to cluster# */
)
{
	DWORD cl, ncl, *tbl;
	FATFS *fs = fp->obj.fs;


	tbl = fp->cltbl + 1;	/* Top of CLMT */
	cl = (DWORD)(ofs / SS(fs) / fs->csize);	/* Cluster order from top of the file */
	for (;;) {
		ncl = *tbl++;			/* Number of cluters in the fragment */
		if (ncl == 0) return 0;	/* End of table? (error) */
		if (cl < ncl) break;	/* In this fragment? */
		cl -= ncl; tbl++;		/* Next fragment */
	}
	return cl + *tbl;	/* Return the cluster number */
}

#endif	/* FF_USE_FASTSEEK */




/*-----------------------------------------------------------------------*/
/* Directory handling - Fill a cluster with zeros                        */
/*-----------------------------------------------------------------------*/

#if !FF_FS_READONLY
static FRESULT dir_clear (	/* Returns FR_OK or FR_DISK_ERR */
	FATFS *fs,		/* Filesystem object */
	DWORD clst		/* Directory table to clear */
)
{
	LBA_t sect;
	UINT n, szb;
	BYTE *ibuf;


	if (sync_window(fs) != FR_OK) return FR_DISK_ERR;	/* Flush disk access window */
	sect = clst2sect(fs, clst);		/* Top of the cluster */
	fs->winsect = sect;				/* Set window to top of the cluster */
	mem_set(fs->win, 0, sizeof fs->win);	/* Clear window buffer */
#if FF_USE_LFN == 3		/* Quick table clear by using multi-secter write */
	/* Allocate a temporary buffer */
	for (szb = ((DWORD)fs->csize * SS(fs) >= MAX_MALLOC) ? MAX_MALLOC : fs->csize * SS(fs), ibuf = 0; szb > SS(fs) && (ibuf = ff_memalloc(szb)) == 0; szb /= 2) ;
	if (szb > SS(fs)) {		/* Buffer allocated? */
		mem_set(ibuf, 0, szb);
		szb /= SS(fs);		/* Bytes -> Sectors */
		for (n = 0; n < fs->csize && disk_write(fs->pdrv, ibuf, sect + n, szb) == RES_OK; n += szb) ;	/* Fill the cluster with 0 */
		ff_memfree(ibuf);
	} else
#endif
	{
		ibuf = fs->win; szb = 1;	/* Use window buffer (many single-sector writes may take a time) */
		for (n = 0; n < fs->csize && disk_write(fs->pdrv, ibuf, sect + n, szb) == RES_OK; n += szb) ;	/* Fill the cluster with 0 */
	}
	return (n == fs->csize) ? FR_OK : FR_DISK_ERR;
}
#endif	/* !FF_FS_READONLY */




/*-----------------------------------------------------------------------*/
/* Directory handling - Set directory index                              */
/*-----------------------------------------------------------------------*/

static FRESULT dir_sdi (	/* FR_OK(0):succeeded, !=0:error */
	DIR* dp,		/* Pointer to directory object */
	DWORD ofs		/* Offset of directory table */
)
{
	DWORD csz, clst;
	FATFS *fs = dp->obj.fs;


	if (ofs >= (DWORD)((FF_FS_EXFAT && fs->fs_type == FS_EXFAT) ? MAX_DIR_EX : MAX_DIR) || ofs % SZDIRE) {	/* Check range of offset and alignment */
		return FR_INT_ERR;
	}
	dp->dptr = ofs;				/* Set current offset */
	clst = dp->obj.sclust;		/* Table start cluster (0:root) */
	if (clst == 0 && fs->fs_type >= FS_FAT32) {	/* Replace cluster# 0 with root cluster# */
		clst = (DWORD)fs->dirbase;
		if (FF_FS_EXFAT) dp->obj.stat = 0;	/* exFAT: Root dir has an FAT chain */
	}

	if (clst == 0) {	/* Static table (root-directory on the FAT volume) */
		if (ofs / SZDIRE >= fs->n_rootdir) return FR_INT_ERR;	/* Is index out of range? */
		dp->sect = fs->dirbase;

	} else {			/* Dynamic table (sub-directory or root-directory on the FAT32/exFAT volume) */
		csz = (DWORD)fs->csize * SS(fs);	/* Bytes per cluster */
		while (ofs >= csz) {				/* Follow cluster chain */
			clst = get_fat(&dp->obj, clst);				/* Get next cluster */
			if (clst == 0xFFFFFFFF) return FR_DISK_ERR;	/* Disk error */
			if (clst < 2 || clst >= fs->n_fatent) return FR_INT_ERR;	/* Reached to end of table or internal error */
			ofs -= csz;
		}
		dp->sect = clst2sect(fs, clst);
	}
	dp->clust = clst;					/* Current cluster# */
	if (dp->sect == 0) return FR_INT_ERR;
	dp->sect += ofs / SS(fs);			/* Sector# of the directory entry */
	dp->dir = fs->win + (ofs % SS(fs));	/* Pointer to the entry in the win[] */

	return FR_OK;
}




/*-----------------------------------------------------------------------*/
/* Directory handling - Move directory table index next                  */
/*-----------------------------------------------------------------------*/

static FRESULT dir_next (	/* FR_OK(0):succeeded, FR_NO_FILE:End of table, FR_DENIED:Could not stretch */
	DIR* dp,				/* Pointer to the directory object */
	int stretch				/* 0: Do not stretch table, 1: Stretch table if needed */
)
{
	DWORD ofs, clst;
	FATFS *fs = dp->obj.fs;


	ofs = dp->dptr + SZDIRE;	/* Next entry */
	if (ofs >= (DWORD)((FF_FS_EXFAT && fs->fs_type == FS_EXFAT) ? MAX_DIR_EX : MAX_DIR)) dp->sect = 0;	/* Disable it if the offset reached the max value */
	if (dp->sect == 0) return FR_NO_FILE;	/* Report EOT if it has been disabled */

	if (ofs % SS(fs) == 0) {	/* Sector changed? */
		dp->sect++;				/* Next sector */

		if (dp->clust == 0) {	/* Static table */
			if (ofs / SZDIRE >= fs->n_rootdir) {	/* Report EOT if it reached end of static table */
				dp->sect = 0; return FR_NO_FILE;
			}
		}
		else {					/* Dynamic table */
			if ((ofs / SS(fs) & (fs->csize - 1)) == 0) {	/* Cluster changed? */
				clst = get_fat(&dp->obj, dp->clust);		/* Get next cluster */
				if (clst <= 1) return FR_INT_ERR;			/* Internal error */
				if (clst == 0xFFFFFFFF) return FR_DISK_ERR;	/* Disk error */
				if (clst >= fs->n_fatent) {					/* It reached end of dynamic table */
#if !FF_FS_READONLY
					if (!stretch) {								/* If no stretch, report EOT */
						dp->sect = 0; return FR_NO_FILE;
					}
					clst = create_chain(&dp->obj, dp->clust);	/* Allocate a cluster */
					if (clst == 0) return FR_DENIED;			/* No free cluster */
					if (clst == 1) return FR_INT_ERR;			/* Internal error */
					if (clst == 0xFFFFFFFF) return FR_DISK_ERR;	/* Disk error */
					if (dir_clear(fs, clst) != FR_OK) return FR_DISK_ERR;	/* Clean up the stretched table */
					if (FF_FS_EXFAT) dp->obj.stat |= 4;			/* exFAT: The directory has been stretched */
#else
					if (!stretch) dp->sect = 0;					/* (this line is to suppress compiler warning) */
					dp->sect = 0; return FR_NO_FILE;			/* Report EOT */
#endif
				}
				dp->clust = clst;		/* Initialize data for new cluster */
				dp->sect = clst2sect(fs, clst);
			}
		}
	}
	dp->dptr = ofs;						/* Current entry */
	dp->dir = fs->win + ofs % SS(fs);	/* Pointer to the entry in the win[] */

	return FR_OK;
}




#if !FF_FS_READONLY
/*-----------------------------------------------------------------------*/
/* Directory handling - Reserve a block of directory entries             */
/*-----------------------------------------------------------------------*/

static FRESULT dir_alloc (	/* FR_OK(0):succeeded, !=0:error */
	DIR* dp,				/* Pointer to the directory object */
	UINT nent				/* Number of contiguous entries to allocate */
)
{
	FRESULT res;
	UINT n;
	FATFS *fs = dp->obj.fs;


	res = dir_sdi(dp, 0);
	if (res == FR_OK) {
		n = 0;
		do {
			res = move_window(fs, dp->sect);
			if (res != FR_OK) break;
#if FF_FS_EXFAT
			if ((fs->fs_type == FS_EXFAT) ? (int)((dp->dir[XDIR_Type] & 0x80) == 0) : (int)(dp->dir[DIR_Name] == DDEM || dp->dir[DIR_Name] == 0)) {
#else
			if (dp->dir[DIR_Name] == DDEM || dp->dir[DIR_Name] == 0) {
#endif
				if (++n == nent) break;	/* A block of contiguous free entries is found */
			} else {
				n = 0;					/* Not a blank entry. Restart to search */
			}
			res = dir_next(dp, 1);
		} while (res == FR_OK);	/* Next entry with table stretch enabled */
	}

	if (res == FR_NO_FILE) res = FR_DENIED;	/* No directory entry to allocate */
	return res;
}

#endif	/* !FF_FS_READONLY */




/*-----------------------------------------------------------------------*/
/* FAT: Directory handling - Load/Store start cluster number             */
/*-----------------------------------------------------------------------*/

static DWORD ld_clust (	/* Returns the top cluster value of the SFN entry */
	FATFS* fs,			/* Pointer to the fs object */
	const BYTE* dir		/* Pointer to the key entry */
)
{
	DWORD cl;

	cl = ld_word(dir + DIR_FstClusLO);
	if (fs->fs_type == FS_FAT32) {
		cl |= (DWORD)ld_word(dir + DIR_FstClusHI) << 16;
	}

	return cl;
}


#if !FF_FS_READONLY
static void st_clust (
	FATFS* fs,	/* Pointer to the fs object */
	BYTE* dir,	/* Pointer to the key entry */
	DWORD cl	/* Value to be set */
)
{
	st_word(dir + DIR_FstClusLO, (WORD)cl);
	if (fs->fs_type == FS_FAT32) {
		st_word(dir + DIR_FstClusHI, (WORD)(cl >> 16));
	}
}
#endif



#if FF_USE_LFN
/*--------------------------------------------------------*/
/* FAT-LFN: Compare a part of file name with an LFN entry */
/*--------------------------------------------------------*/

static int cmp_lfn (		/* 1:matched, 0:not matched */
	const WCHAR* lfnbuf,	/* Pointer to the LFN working buffer to be compared */
	BYTE* dir				/* Pointer to the directory entry containing the part of LFN */
)
{
	UINT i, s;
	WCHAR wc, uc;


	if (ld_word(dir + LDIR_FstClusLO) != 0) return 0;	/* Check LDIR_FstClusLO */

	i = ((dir[LDIR_Ord] & 0x3F) - 1) * 13;	/* Offset in the LFN buffer */

	for (wc = 1, s = 0; s < 13; s++) {		/* Process all characters in the entry */
		uc = ld_word(dir + LfnOfs[s]);		/* Pick an LFN character */
		if (wc != 0) {
			if (i >= FF_MAX_LFN + 1 || ff_wtoupper(uc) != ff_wtoupper(lfnbuf[i++])) {	/* Compare it */
				return 0;					/* Not matched */
			}
			wc = uc;
		} else {
			if (uc != 0xFFFF) return 0;		/* Check filler */
		}
	}

	if ((dir[LDIR_Ord] & LLEF) && wc && lfnbuf[i]) return 0;	/* Last segment matched but different length */

	return 1;		/* The part of LFN matched */
}


#if FF_FS_MINIMIZE <= 1 || FF_FS_RPATH >= 2 || FF_USE_LABEL || FF_FS_EXFAT
/*-----------------------------------------------------*/
/* FAT-LFN: Pick a part of file name from an LFN entry */
/*-----------------------------------------------------*/

static int pick_lfn (	/* 1:succeeded, 0:buffer overflow or invalid LFN entry */
	WCHAR* lfnbuf,		/* Pointer to the LFN working buffer */
	BYTE* dir			/* Pointer to the LFN entry */
)
{
	UINT i, s;
	WCHAR wc, uc;


	if (ld_word(dir + LDIR_FstClusLO) != 0) return 0;	/* Check LDIR_FstClusLO is 0 */

	i = ((dir[LDIR_Ord] & ~LLEF) - 1) * 13;	/* Offset in the LFN buffer */

	for (wc = 1, s = 0; s < 13; s++) {		/* Process all characters in the entry */
		uc = ld_word(dir + LfnOfs[s]);		/* Pick an LFN character */
		if (wc != 0) {
			if (i >= FF_MAX_LFN + 1) return 0;	/* Buffer overflow? */
			lfnbuf[i++] = wc = uc;			/* Store it */
		} else {
			if (uc != 0xFFFF) return 0;		/* Check filler */
		}
	}

	if (dir[LDIR_Ord] & LLEF && wc != 0) {	/* Put terminator if it is the last LFN part and not terminated */
		if (i >= FF_MAX_LFN + 1) return 0;	/* Buffer overflow? */
		lfnbuf[i] = 0;
	}

	return 1;		/* The part of LFN is valid */
}
#endif


#if !FF_FS_READONLY
/*-----------------------------------------*/
/* FAT-LFN: Create an entry of LFN entries */
/*-----------------------------------------*/

static void put_lfn (
	const WCHAR* lfn,	/* Pointer to the LFN */
	BYTE* dir,			/* Pointer to the LFN entry to be created */
	BYTE ord,			/* LFN order (1-20) */
	BYTE sum			/* Checksum of the corresponding SFN */
)
{
	UINT i, s;
	WCHAR wc;


	dir[LDIR_Chksum] = sum;			/* Set checksum */
	dir[LDIR_Attr] = AM_LFN;		/* Set attribute. LFN entry */
	dir[LDIR_Type] = 0;
	st_word(dir + LDIR_FstClusLO, 0);

	i = (ord - 1) * 13;				/* Get offset in the LFN working buffer */
	s = wc = 0;
	do {
		if (wc != 0xFFFF) wc = lfn[i++];	/* Get an effective character */
		st_word(dir + LfnOfs[s], wc);		/* Put it */
		if (wc == 0) wc = 0xFFFF;		/* Padding characters for following items */
	} while (++s < 13);
	if (wc == 0xFFFF || !lfn[i]) ord |= LLEF;	/* Last LFN part is the start of LFN sequence */
	dir[LDIR_Ord] = ord;			/* Set the LFN order */
}

#endif	/* !FF_FS_READONLY */
#endif	/* FF_USE_LFN */



#if FF_USE_LFN && !FF_FS_READONLY
/*-----------------------------------------------------------------------*/
/* FAT-LFN: Create a Numbered SFN                                        */
/*-----------------------------------------------------------------------*/

static void gen_numname (
	BYTE* dst,			/* Pointer to the buffer to store numbered SFN */
	const BYTE* src,	/* Pointer to SFN */
	const WCHAR* lfn,	/* Pointer to LFN */
	UINT seq			/* Sequence number */
)
{
	BYTE ns[8], c;
	UINT i, j;
	WCHAR wc;
	DWORD sreg;


	mem_cpy(dst, src, 11);

	if (seq > 5) {	/* In case of many collisions, generate a hash number instead of sequential number */
		sreg = seq;
		while (*lfn) {	/* Create a CRC as hash value */
			wc = *lfn++;
			for (i = 0; i < 16; i++) {
				sreg = (sreg << 1) + (wc & 1);
				wc >>= 1;
				if (sreg & 0x10000) sreg ^= 0x11021;
			}
		}
		seq = (UINT)sreg;
	}

	/* itoa (hexdecimal) */
	i = 7;
	do {
		c = (BYTE)((seq % 16) + '0');
		if (c > '9') c += 7;
		ns[i--] = c;
		seq /= 16;
	} while (seq);
	ns[i] = '~';

	/* Append the number to the SFN body */
	for (j = 0; j < i && dst[j] != ' '; j++) {
		if (dbc_1st(dst[j])) {
			if (j == i - 1) break;
			j++;
		}
	}
	do {
		dst[j++] = (i < 8) ? ns[i++] : ' ';
	} while (j < 8);
}
#endif	/* FF_USE_LFN && !FF_FS_READONLY */



#if FF_USE_LFN
/*-----------------------------------------------------------------------*/
/* FAT-LFN: Calculate checksum of an SFN entry                           */
/*-----------------------------------------------------------------------*/

static BYTE sum_sfn (
	const BYTE* dir		/* Pointer to the SFN entry */
)
{
	BYTE sum = 0;
	UINT n = 11;

	do {
		sum = (sum >> 1) + (sum << 7) + *dir++;
	} while (--n);
	return sum;
}

#endif	/* FF_USE_LFN */



#if FF_FS_EXFAT
/*-----------------------------------------------------------------------*/
/* exFAT: Checksum                                                       */
/*-----------------------------------------------------------------------*/

static WORD xdir_sum (	/* Get checksum of the directoly entry block */
	const BYTE* dir		/* Directory entry block to be calculated */
)
{
	UINT i, szblk;
	WORD sum;


	szblk = (dir[XDIR_NumSec] + 1) * SZDIRE;	/* Number of bytes of the entry block */
	for (i = sum = 0; i < szblk; i++) {
		if (i == XDIR_SetSum) {	/* Skip 2-byte sum field */
			i++;
		} else {
			sum = ((sum & 1) ? 0x8000 : 0) + (sum >> 1) + dir[i];
		}
	}
	return sum;
}



static WORD xname_sum (	/* Get check sum (to be used as hash) of the file name */
	const WCHAR* name	/* File name to be calculated */
)
{
	WCHAR chr;
	WORD sum = 0;


	while ((chr = *name++) != 0) {
		chr = (WCHAR)ff_wtoupper(chr);		/* File name needs to be up-case converted */
		sum = ((sum & 1) ? 0x8000 : 0) + (sum >> 1) + (chr & 0xFF);
		sum = ((sum & 1) ? 0x8000 : 0) + (sum >> 1) + (chr >> 8);
	}
	return sum;
}


#if !FF_FS_READONLY && FF_USE_MKFS
static DWORD xsum32 (	/* Returns 32-bit checksum */
	BYTE  dat,			/* Byte to be calculated (byte-by-byte processing) */
	DWORD sum			/* Previous sum value */
)
{
	sum = ((sum & 1) ? 0x80000000 : 0) + (sum >> 1) + dat;
	return sum;
}
#endif


#if FF_FS_MINIMIZE <= 1 || FF_FS_RPATH >= 2
/*------------------------------------------------------*/
/* exFAT: Get object information from a directory block */
/*------------------------------------------------------*/

static void get_xfileinfo (
	BYTE* dirb,			/* Pointer to the direcotry entry block 85+C0+C1s */
	FILINFO* fno		/* Buffer to store the extracted file information */
)
{
	WCHAR wc, hs;
	UINT di, si, nc;

	/* Get file name from the entry block */
	si = SZDIRE * 2;	/* 1st C1 entry */
	nc = 0; hs = 0; di = 0;
	while (nc < dirb[XDIR_NumName]) {
		if (si >= MAXDIRB(FF_MAX_LFN)) { di = 0; break; }	/* Truncated directory block? */
		if ((si % SZDIRE) == 0) si += 2;		/* Skip entry type field */
		wc = ld_word(dirb + si); si += 2; nc++;	/* Get a character */
		if (hs == 0 && IsSurrogate(wc)) {	/* Is it a surrogate? */
			hs = wc; continue;	/* Get low surrogate */
		}
		wc = put_utf((DWORD)hs << 16 | wc, &fno->fname[di], FF_LFN_BUF - di);	/* Store it in API encoding */
		if (wc == 0) { di = 0; break; }	/* Buffer overflow or wrong encoding? */
		di += wc;
		hs = 0;
	}
	if (hs != 0) di = 0;					/* Broken surrogate pair? */
	if (di == 0) fno->fname[di++] = '?';	/* Inaccessible object name? */
	fno->fname[di] = 0;						/* Terminate the name */
	fno->altname[0] = 0;					/* exFAT does not support SFN */

	fno->fattrib = dirb[XDIR_Attr];			/* Attribute */
	fno->fsize = (fno->fattrib & AM_DIR) ? 0 : ld_qword(dirb + XDIR_FileSize);	/* Size */
	fno->ftime = ld_word(dirb + XDIR_ModTime + 0);	/* Time */
	fno->fdate = ld_word(dirb + XDIR_ModTime + 2);	/* Date */
}

#endif	/* FF_FS_MINIMIZE <= 1 || FF_FS_RPATH >= 2 */


/*-----------------------------------*/
/* exFAT: Get a directry entry block */
/*-----------------------------------*/

static FRESULT load_xdir (	/* FR_INT_ERR: invalid entry block */
	DIR* dp					/* Reading direcotry object pointing top of the entry block to load */
)
{
	FRESULT res;
	UINT i, sz_ent;
	BYTE* dirb = dp->obj.fs->dirbuf;	/* Pointer to the on-memory direcotry entry block 85+C0+C1s */


	/* Load file-directory entry */
	res = move_window(dp->obj.fs, dp->sect);
	if (res != FR_OK) return res;
	if (dp->dir[XDIR_Type] != ET_FILEDIR) return FR_INT_ERR;	/* Invalid order */
	mem_cpy(dirb + 0 * SZDIRE, dp->dir, SZDIRE);
	sz_ent = (dirb[XDIR_NumSec] + 1) * SZDIRE;
	if (sz_ent < 3 * SZDIRE || sz_ent > 19 * SZDIRE) return FR_INT_ERR;

	/* Load stream-extension entry */
	res = dir_next(dp, 0);
	if (res == FR_NO_FILE) res = FR_INT_ERR;	/* It cannot be */
	if (res != FR_OK) return res;
	res = move_window(dp->obj.fs, dp->sect);
	if (res != FR_OK) return res;
	if (dp->dir[XDIR_Type] != ET_STREAM) return FR_INT_ERR;	/* Invalid order */
	mem_cpy(dirb + 1 * SZDIRE, dp->dir, SZDIRE);
	if (MAXDIRB(dirb[XDIR_NumName]) > sz_ent) return FR_INT_ERR;

	/* Load file-name entries */
	i = 2 * SZDIRE;	/* Name offset to load */
	do {
		res = dir_next(dp, 0);
		if (res == FR_NO_FILE) res = FR_INT_ERR;	/* It cannot be */
		if (res != FR_OK) return res;
		res = move_window(dp->obj.fs, dp->sect);
		if (res != FR_OK) return res;
		if (dp->dir[XDIR_Type] != ET_FILENAME) return FR_INT_ERR;	/* Invalid order */
		if (i < MAXDIRB(FF_MAX_LFN)) mem_cpy(dirb + i, dp->dir, SZDIRE);
	} while ((i += SZDIRE) < sz_ent);

	/* Sanity check (do it for only accessible object) */
	if (i <= MAXDIRB(FF_MAX_LFN)) {
		if (xdir_sum(dirb) != ld_word(dirb + XDIR_SetSum)) return FR_INT_ERR;
	}
	return FR_OK;
}


/*------------------------------------------------------------------*/
/* exFAT: Initialize object allocation info with loaded entry block */
/*------------------------------------------------------------------*/

static void init_alloc_info (
	FATFS* fs,		/* Filesystem object */
	FFOBJID* obj	/* Object allocation information to be initialized */
)
{
	obj->sclust = ld_dword(fs->dirbuf + XDIR_FstClus);		/* Start cluster */
	obj->objsize = ld_qword(fs->dirbuf + XDIR_FileSize);	/* Size */
	obj->stat = fs->dirbuf[XDIR_GenFlags] & 2;				/* Allocation status */
	obj->n_frag = 0;										/* No last fragment info */
}



#if !FF_FS_READONLY || FF_FS_RPATH != 0
/*------------------------------------------------*/
/* exFAT: Load the object's directory entry block */
/*------------------------------------------------*/

static FRESULT load_obj_xdir (
	DIR* dp,			/* Blank directory object to be used to access containing direcotry */
	const FFOBJID* obj	/* Object with its containing directory information */
)
{
	FRESULT res;

	/* Open object containing directory */
	dp->obj.fs = obj->fs;
	dp->obj.sclust = obj->c_scl;
	dp->obj.stat = (BYTE)obj->c_size;
	dp->obj.objsize = obj->c_size & 0xFFFFFF00;
	dp->obj.n_frag = 0;
	dp->blk_ofs = obj->c_ofs;

	res = dir_sdi(dp, dp->blk_ofs);	/* Goto object's entry block */
	if (res == FR_OK) {
		res = load_xdir(dp);		/* Load the object's entry block */
	}
	return res;
}
#endif


#if !FF_FS_READONLY
/*----------------------------------------*/
/* exFAT: Store the directory entry block */
/*----------------------------------------*/

static FRESULT store_xdir (
	DIR* dp				/* Pointer to the direcotry object */
)
{
	FRESULT res;
	UINT nent;
	BYTE* dirb = dp->obj.fs->dirbuf;	/* Pointer to the direcotry entry block 85+C0+C1s */

	/* Create set sum */
	st_word(dirb + XDIR_SetSum, xdir_sum(dirb));
	nent = dirb[XDIR_NumSec] + 1;

	/* Store the direcotry entry block to the directory */
	res = dir_sdi(dp, dp->blk_ofs);
	while (res == FR_OK) {
		res = move_window(dp->obj.fs, dp->sect);
		if (res != FR_OK) break;
		mem_cpy(dp->dir, dirb, SZDIRE);
		dp->obj.fs->wflag = 1;
		if (--nent == 0) break;
		dirb += SZDIRE;
		res = dir_next(dp, 0);
	}
	return (res == FR_OK || res == FR_DISK_ERR) ? res : FR_INT_ERR;
}



/*-------------------------------------------*/
/* exFAT: Create a new directory enrty block */
/*-------------------------------------------*/

static void create_xdir (
	BYTE* dirb,			/* Pointer to the direcotry entry block buffer */
	const WCHAR* lfn	/* Pointer to the object name */
)
{
	UINT i;
	BYTE nc1, nlen;
	WCHAR wc;


	/* Create file-directory and stream-extension entry */
	mem_set(dirb, 0, 2 * SZDIRE);
	dirb[0 * SZDIRE + XDIR_Type] = ET_FILEDIR;
	dirb[1 * SZDIRE + XDIR_Type] = ET_STREAM;

	/* Create file-name entries */
	i = SZDIRE * 2;	/* Top of file_name entries */
	nlen = nc1 = 0; wc = 1;
	do {
		dirb[i++] = ET_FILENAME; dirb[i++] = 0;
		do {	/* Fill name field */
			if (wc != 0 && (wc = lfn[nlen]) != 0) nlen++;	/* Get a character if exist */
			st_word(dirb + i, wc); 		/* Store it */
			i += 2;
		} while (i % SZDIRE != 0);
		nc1++;
	} while (lfn[nlen]);	/* Fill next entry if any char follows */

	dirb[XDIR_NumName] = nlen;		/* Set name length */
	dirb[XDIR_NumSec] = 1 + nc1;	/* Set secondary count (C0 + C1s) */
	st_word(dirb + XDIR_NameHash, xname_sum(lfn));	/* Set name hash */
}

#endif	/* !FF_FS_READONLY */
#endif	/* FF_FS_EXFAT */



#if FF_FS_MINIMIZE <= 1 || FF_FS_RPATH >= 2 || FF_USE_LABEL || FF_FS_EXFAT
/*-----------------------------------------------------------------------*/
/* Read an object from the directory                                     */
/*-----------------------------------------------------------------------*/

#define DIR_READ_FILE(dp) dir_read(dp, 0)
#define DIR_READ_LABEL(dp) dir_read(dp, 1)

static FRESULT dir_read (
	DIR* dp,		/* Pointer to the directory object */
	int vol			/* Filtered by 0:file/directory or 1:volume label */
)
{
	FRESULT res = FR_NO_FILE;
	FATFS *fs = dp->obj.fs;
	BYTE attr, b;
#if FF_USE_LFN
	BYTE ord = 0xFF, sum = 0xFF;
#endif

	while (dp->sect) {
		res = move_window(fs, dp->sect);
		if (res != FR_OK) break;
		b = dp->dir[DIR_Name];	/* Test for the entry type */
		if (b == 0) {
			res = FR_NO_FILE; break; /* Reached to end of the directory */
		}
#if FF_FS_EXFAT
		if (fs->fs_type == FS_EXFAT) {	/* On the exFAT volume */
			if (FF_USE_LABEL && vol) {
				if (b == ET_VLABEL) break;	/* Volume label entry? */
			} else {
				if (b == ET_FILEDIR) {		/* Start of the file entry block? */
					dp->blk_ofs = dp->dptr;	/* Get location of the block */
					res = load_xdir(dp);	/* Load the entry block */
					if (res == FR_OK) {
						dp->obj.attr = fs->dirbuf[XDIR_Attr] & AM_MASK;	/* Get attribute */
					}
					break;
				}
			}
		} else
#endif
		{	/* On the FAT/FAT32 volume */
			dp->obj.attr = attr = dp->dir[DIR_Attr] & AM_MASK;	/* Get attribute */
#if FF_USE_LFN		/* LFN configuration */
			if (b == DDEM || b == '.' || (int)((attr & ~AM_ARC) == AM_VOL) != vol) {	/* An entry without valid data */
				ord = 0xFF;
			} else {
				if (attr == AM_LFN) {			/* An LFN entry is found */
					if (b & LLEF) {			/* Is it start of an LFN sequence? */
						sum = dp->dir[LDIR_Chksum];
						b &= (BYTE)~LLEF; ord = b;
						dp->blk_ofs = dp->dptr;
					}
					/* Check LFN validity and capture it */
					ord = (b == ord && sum == dp->dir[LDIR_Chksum] && pick_lfn(fs->lfnbuf, dp->dir)) ? ord - 1 : 0xFF;
				} else {					/* An SFN entry is found */
					if (ord != 0 || sum != sum_sfn(dp->dir)) {	/* Is there a valid LFN? */
						dp->blk_ofs = 0xFFFFFFFF;			/* It has no LFN. */
					}
					break;
				}
			}
#else		/* Non LFN configuration */
			if (b != DDEM && b != '.' && attr != AM_LFN && (int)((attr & ~AM_ARC) == AM_VOL) == vol) {	/* Is it a valid entry? */
				break;
			}
#endif
		}
		res = dir_next(dp, 0);		/* Next entry */
		if (res != FR_OK) break;
	}

	if (res != FR_OK) dp->sect = 0;		/* Terminate the read operation on error or EOT */
	return res;
}

#endif	/* FF_FS_MINIMIZE <= 1 || FF_USE_LABEL || FF_FS_RPATH >= 2 */



/*-----------------------------------------------------------------------*/
/* Directory handling - Find an object in the directory                  */
/*-----------------------------------------------------------------------*/

static FRESULT dir_find (	/* FR_OK(0):succeeded, !=0:error */
	DIR* dp					/* Pointer to the directory object with the file name */
)
{
	FRESULT res;
	FATFS *fs = dp->obj.fs;
	BYTE c;
#if FF_USE_LFN
	BYTE a, ord, sum;
#endif

	res = dir_sdi(dp, 0);			/* Rewind directory object */
	if (res != FR_OK) return res;
#if FF_FS_EXFAT
	if (fs->fs_type == FS_EXFAT) {	/* On the exFAT volume */
		BYTE nc;
		UINT di, ni;
		WORD hash = xname_sum(fs->lfnbuf);		/* Hash value of the name to find */

		while ((res = DIR_READ_FILE(dp)) == FR_OK) {	/* Read an item */
#if FF_MAX_LFN < 255
			if (fs->dirbuf[XDIR_NumName] > FF_MAX_LFN) continue;			/* Skip comparison if inaccessible object name */
#endif
			if (ld_word(fs->dirbuf + XDIR_NameHash) != hash) continue;	/* Skip comparison if hash mismatched */
			for (nc = fs->dirbuf[XDIR_NumName], di = SZDIRE * 2, ni = 0; nc; nc--, di += 2, ni++) {	/* Compare the name */
				if ((di % SZDIRE) == 0) di += 2;
				if (ff_wtoupper(ld_word(fs->dirbuf + di)) != ff_wtoupper(fs->lfnbuf[ni])) break;
			}
			if (nc == 0 && !fs->lfnbuf[ni]) break;	/* Name matched? */
		}
		return res;
	}
#endif
	/* On the FAT/FAT32 volume */
#if FF_USE_LFN
	ord = sum = 0xFF; dp->blk_ofs = 0xFFFFFFFF;	/* Reset LFN sequence */
#endif
	do {
		res = move_window(fs, dp->sect);
		if (res != FR_OK) break;
		c = dp->dir[DIR_Name];
		if (c == 0) { res = FR_NO_FILE; break; }	/* Reached to end of table */
#if FF_USE_LFN		/* LFN configuration */
		dp->obj.attr = a = dp->dir[DIR_Attr] & AM_MASK;
		if (c == DDEM || ((a & AM_VOL) && a != AM_LFN)) {	/* An entry without valid data */
			ord = 0xFF; dp->blk_ofs = 0xFFFFFFFF;	/* Reset LFN sequence */
		} else {
			if (a == AM_LFN) {			/* An LFN entry is found */
				if (!(dp->fn[NSFLAG] & NS_NOLFN)) {
					if (c & LLEF) {		/* Is it start of LFN sequence? */
						sum = dp->dir[LDIR_Chksum];
						c &= (BYTE)~LLEF; ord = c;	/* LFN start order */
						dp->blk_ofs = dp->dptr;	/* Start offset of LFN */
					}
					/* Check validity of the LFN entry and compare it with given name */
					ord = (c == ord && sum == dp->dir[LDIR_Chksum] && cmp_lfn(fs->lfnbuf, dp->dir)) ? ord - 1 : 0xFF;
				}
			} else {					/* An SFN entry is found */
				if (ord == 0 && sum == sum_sfn(dp->dir)) break;	/* LFN matched? */
				if (!(dp->fn[NSFLAG] & NS_LOSS) && !mem_cmp(dp->dir, dp->fn, 11)) break;	/* SFN matched? */
				ord = 0xFF; dp->blk_ofs = 0xFFFFFFFF;	/* Reset LFN sequence */
			}
		}
#else		/* Non LFN configuration */
		dp->obj.attr = dp->dir[DIR_Attr] & AM_MASK;
		if (!(dp->dir[DIR_Attr] & AM_VOL) && !mem_cmp(dp->dir, dp->fn, 11)) break;	/* Is it a valid entry? */
#endif
		res = dir_next(dp, 0);	/* Next entry */
	} while (res == FR_OK);

	return res;
}




#if !FF_FS_READONLY
/*-----------------------------------------------------------------------*/
/* Register an object to the directory                                   */
/*-----------------------------------------------------------------------*/

static FRESULT dir_register (	/* FR_OK:succeeded, FR_DENIED:no free entry or too many SFN collision, FR_DISK_ERR:disk error */
	DIR* dp						/* Target directory with object name to be created */
)
{
	FRESULT res;
	FATFS *fs = dp->obj.fs;
#if FF_USE_LFN		/* LFN configuration */
	UINT n, nlen, nent;
	BYTE sn[12], sum;


	if (dp->fn[NSFLAG] & (NS_DOT | NS_NONAME)) return FR_INVALID_NAME;	/* Check name validity */
	for (nlen = 0; fs->lfnbuf[nlen]; nlen++) ;	/* Get lfn length */

#if FF_FS_EXFAT
	if (fs->fs_type == FS_EXFAT) {	/* On the exFAT volume */
		nent = (nlen + 14) / 15 + 2;	/* Number of entries to allocate (85+C0+C1s) */
		res = dir_alloc(dp, nent);		/* Allocate directory entries */
		if (res != FR_OK) return res;
		dp->blk_ofs = dp->dptr - SZDIRE * (nent - 1);	/* Set the allocated entry block offset */

		if (dp->obj.stat & 4) {			/* Has the directory been stretched by new allocation? */
			dp->obj.stat &= ~4;
			res = fill_first_frag(&dp->obj);	/* Fill the first fragment on the FAT if needed */
			if (res != FR_OK) return res;
			res = fill_last_frag(&dp->obj, dp->clust, 0xFFFFFFFF);	/* Fill the last fragment on the FAT if needed */
			if (res != FR_OK) return res;
			if (dp->obj.sclust != 0) {		/* Is it a sub-directory? */
				DIR dj;

				res = load_obj_xdir(&dj, &dp->obj);	/* Load the object status */
				if (res != FR_OK) return res;
				dp->obj.objsize += (DWORD)fs->csize * SS(fs);			/* Increase the directory size by cluster size */
				st_qword(fs->dirbuf + XDIR_FileSize, dp->obj.objsize);
				st_qword(fs->dirbuf + XDIR_ValidFileSize, dp->obj.objsize);
				fs->dirbuf[XDIR_GenFlags] = dp->obj.stat | 1;			/* Update the allocation status */
				res = store_xdir(&dj);				/* Store the object status */
				if (res != FR_OK) return res;
			}
		}

		create_xdir(fs->dirbuf, fs->lfnbuf);	/* Create on-memory directory block to be written later */
		return FR_OK;
	}
#endif
	/* On the FAT/FAT32 volume */
	mem_cpy(sn, dp->fn, 12);
	if (sn[NSFLAG] & NS_LOSS) {			/* When LFN is out of 8.3 format, generate a numbered name */
		dp->fn[NSFLAG] = NS_NOLFN;		/* Find only SFN */
		for (n = 1; n < 100; n++) {
			gen_numname(dp->fn, sn, fs->lfnbuf, n);	/* Generate a numbered name */
			res = dir_find(dp);				/* Check if the name collides with existing SFN */
			if (res != FR_OK) break;
		}
		if (n == 100) return FR_DENIED;		/* Abort if too many collisions */
		if (res != FR_NO_FILE) return res;	/* Abort if the result is other than 'not collided' */
		dp->fn[NSFLAG] = sn[NSFLAG];
	}

	/* Create an SFN with/without LFNs. */
	nent = (sn[NSFLAG] & NS_LFN) ? (nlen + 12) / 13 + 1 : 1;	/* Number of entries to allocate */
	res = dir_alloc(dp, nent);		/* Allocate entries */
	if (res == FR_OK && --nent) {	/* Set LFN entry if needed */
		res = dir_sdi(dp, dp->dptr - nent * SZDIRE);
		if (res == FR_OK) {
			sum = sum_sfn(dp->fn);	/* Checksum value of the SFN tied to the LFN */
			do {					/* Store LFN entries in bottom first */
				res = move_window(fs, dp->sect);
				if (res != FR_OK) break;
				put_lfn(fs->lfnbuf, dp->dir, (BYTE)nent, sum);
				fs->wflag = 1;
				res = dir_next(dp, 0);	/* Next entry */
			} while (res == FR_OK && --nent);
		}
	}

#else	/* Non LFN configuration */
	res = dir_alloc(dp, 1);		/* Allocate an entry for SFN */

#endif

	/* Set SFN entry */
	if (res == FR_OK) {
		res = move_window(fs, dp->sect);
		if (res == FR_OK) {
			mem_set(dp->dir, 0, SZDIRE);	/* Clean the entry */
			mem_cpy(dp->dir + DIR_Name, dp->fn, 11);	/* Put SFN */
#if FF_USE_LFN
			dp->dir[DIR_NTres] = dp->fn[NSFLAG] & (NS_BODY | NS_EXT);	/* Put NT flag */
#endif
			fs->wflag = 1;
		}
	}

	return res;
}

#endif /* !FF_FS_READONLY */



#if !FF_FS_READONLY && FF_FS_MINIMIZE == 0
/*-----------------------------------------------------------------------*/
/* Remove an object from the directory                                   */
/*-----------------------------------------------------------------------*/

static FRESULT dir_remove (	/* FR_OK:Succeeded, FR_DISK_ERR:A disk error */
	DIR* dp					/* Directory object pointing the entry to be removed */
)
{
	FRESULT res;
	FATFS *fs = dp->obj.fs;
#if FF_USE_LFN		/* LFN configuration */
	DWORD last = dp->dptr;

	res = (dp->blk_ofs == 0xFFFFFFFF) ? FR_OK : dir_sdi(dp, dp->blk_ofs);	/* Goto top of the entry block if LFN is exist */
	if (res == FR_OK) {
		do {
			res = move_window(fs, dp->sect);
			if (res != FR_OK) break;
			if (FF_FS_EXFAT && fs->fs_type == FS_EXFAT) {	/* On the exFAT volume */
				dp->dir[XDIR_Type] &= 0x7F;	/* Clear the entry InUse flag. */
			} else {									/* On the FAT/FAT32 volume */
				dp->dir[DIR_Name] = DDEM;	/* Mark the entry 'deleted'. */
			}
			fs->wflag = 1;
			if (dp->dptr >= last) break;	/* If reached last entry then all entries of the object has been deleted. */
			res = dir_next(dp, 0);	/* Next entry */
		} while (res == FR_OK);
		if (res == FR_NO_FILE) res = FR_INT_ERR;
	}
#else			/* Non LFN configuration */

	res = move_window(fs, dp->sect);
	if (res == FR_OK) {
		dp->dir[DIR_Name] = DDEM;	/* Mark the entry 'deleted'.*/
		fs->wflag = 1;
	}
#endif

	return res;
}

#endif /* !FF_FS_READONLY && FF_FS_MINIMIZE == 0 */



#if FF_FS_MINIMIZE <= 1 || FF_FS_RPATH >= 2
/*-----------------------------------------------------------------------*/
/* Get file information from directory entry                             */
/*-----------------------------------------------------------------------*/

static void get_fileinfo (
	DIR* dp,			/* Pointer to the directory object */
	FILINFO* fno		/* Pointer to the file information to be filled */
)
{
	UINT si, di;
#if FF_USE_LFN
	BYTE lcf;
	WCHAR wc, hs;
	FATFS *fs = dp->obj.fs;
#else
	TCHAR c;
#endif


	fno->fname[0] = 0;			/* Invaidate file info */
	if (dp->sect == 0) return;	/* Exit if read pointer has reached end of directory */

#if FF_USE_LFN		/* LFN configuration */
#if FF_FS_EXFAT
	if (fs->fs_type == FS_EXFAT) {	/* On the exFAT volume */
		get_xfileinfo(fs->dirbuf, fno);
		return;
	} else
#endif
	{	/* On the FAT/FAT32 volume */
		if (dp->blk_ofs != 0xFFFFFFFF) {	/* Get LFN if available */
			si = di = hs = 0;
			while (fs->lfnbuf[si] != 0) {
				wc = fs->lfnbuf[si++];		/* Get an LFN character (UTF-16) */
				if (hs == 0 && IsSurrogate(wc)) {	/* Is it a surrogate? */
					hs = wc; continue;		/* Get low surrogate */
				}
				wc = put_utf((DWORD)hs << 16 | wc, &fno->fname[di], FF_LFN_BUF - di);	/* Store it in UTF-16 or UTF-8 encoding */
				if (wc == 0) { di = 0; break; }	/* Invalid char or buffer overflow? */
				di += wc;
				hs = 0;
			}
			if (hs != 0) di = 0;	/* Broken surrogate pair? */
			fno->fname[di] = 0;		/* Terminate the LFN (null string means LFN is invalid) */
		}
	}

	si = di = 0;
	while (si < 11) {		/* Get SFN from SFN entry */
		wc = dp->dir[si++];			/* Get a char */
		if (wc == ' ') continue;	/* Skip padding spaces */
		if (wc == RDDEM) wc = DDEM;	/* Restore replaced DDEM character */
		if (si == 9 && di < FF_SFN_BUF) fno->altname[di++] = '.';	/* Insert a . if extension is exist */
#if FF_LFN_UNICODE >= 1	/* Unicode output */
		if (dbc_1st((BYTE)wc) && si != 8 && si != 11 && dbc_2nd(dp->dir[si])) {	/* Make a DBC if needed */
			wc = wc << 8 | dp->dir[si++];
		}
		wc = ff_oem2uni(wc, CODEPAGE);		/* ANSI/OEM -> Unicode */
		if (wc == 0) { di = 0; break; }		/* Wrong char in the current code page? */
		wc = put_utf(wc, &fno->altname[di], FF_SFN_BUF - di);	/* Store it in Unicode */
		if (wc == 0) { di = 0; break; }		/* Buffer overflow? */
		di += wc;
#else					/* ANSI/OEM output */
		fno->altname[di++] = (TCHAR)wc;	/* Store it without any conversion */
#endif
	}
	fno->altname[di] = 0;	/* Terminate the SFN  (null string means SFN is invalid) */

	if (fno->fname[0] == 0) {	/* If LFN is invalid, altname[] needs to be copied to fname[] */
		if (di == 0) {	/* If LFN and SFN both are invalid, this object is inaccesible */
			fno->fname[di++] = '?';
		} else {
			for (si = di = 0, lcf = NS_BODY; fno->altname[si]; si++, di++) {	/* Copy altname[] to fname[] with case information */
				wc = (WCHAR)fno->altname[si];
				if (wc == '.') lcf = NS_EXT;
				if (IsUpper(wc) && (dp->dir[DIR_NTres] & lcf)) wc += 0x20;
				fno->fname[di] = (TCHAR)wc;
			}
		}
		fno->fname[di] = 0;	/* Terminate the LFN */
		if (!dp->dir[DIR_NTres]) fno->altname[0] = 0;	/* Altname is not needed if neither LFN nor case info is exist. */
	}

#else	/* Non-LFN configuration */
	si = di = 0;
	while (si < 11) {		/* Copy name body and extension */
		c = (TCHAR)dp->dir[si++];
		if (c == ' ') continue;		/* Skip padding spaces */
		if (c == RDDEM) c = DDEM;	/* Restore replaced DDEM character */
		if (si == 9) fno->fname[di++] = '.';/* Insert a . if extension is exist */
		fno->fname[di++] = c;
	}
	fno->fname[di] = 0;
#endif

	fno->fattrib = dp->dir[DIR_Attr];					/* Attribute */
	fno->fsize = ld_dword(dp->dir + DIR_FileSize);		/* Size */
	fno->ftime = ld_word(dp->dir + DIR_ModTime + 0);	/* Time */
	fno->fdate = ld_word(dp->dir + DIR_ModTime + 2);	/* Date */
}

#endif /* FF_FS_MINIMIZE <= 1 || FF_FS_RPATH >= 2 */



#if FF_USE_FIND && FF_FS_MINIMIZE <= 1
/*-----------------------------------------------------------------------*/
/* Pattern matching                                                      */
/*-----------------------------------------------------------------------*/

static DWORD get_achar (	/* Get a character and advances ptr */
	const TCHAR** ptr		/* Pointer to pointer to the ANSI/OEM or Unicode string */
)
{
	DWORD chr;


#if FF_USE_LFN && FF_LFN_UNICODE >= 1	/* Unicode input */
	chr = tchar2uni(ptr);
	if (chr == 0xFFFFFFFF) chr = 0;		/* Wrong UTF encoding is recognized as end of the string */
	chr = ff_wtoupper(chr);

#else									/* ANSI/OEM input */
	chr = (BYTE)*(*ptr)++;				/* Get a byte */
	if (IsLower(chr)) chr -= 0x20;		/* To upper ASCII char */
#if FF_CODE_PAGE == 0
	if (ExCvt && chr >= 0x80) chr = ExCvt[chr - 0x80];	/* To upper SBCS extended char */
#elif FF_CODE_PAGE < 900
	if (chr >= 0x80) chr = ExCvt[chr - 0x80];	/* To upper SBCS extended char */
#endif
#if FF_CODE_PAGE == 0 || FF_CODE_PAGE >= 900
	if (dbc_1st((BYTE)chr)) {	/* Get DBC 2nd byte if needed */
		chr = dbc_2nd((BYTE)**ptr) ? chr << 8 | (BYTE)*(*ptr)++ : 0;
	}
#endif

#endif
	return chr;
}


static int pattern_matching (	/* 0:not matched, 1:matched */
	const TCHAR* pat,	/* Matching pattern */
	const TCHAR* nam,	/* String to be tested */
	int skip,			/* Number of pre-skip chars (number of ?s) */
	int inf				/* Infinite search (* specified) */
)
{
	const TCHAR *pp, *np;
	DWORD pc, nc;
	int nm, nx;


	while (skip--) {				/* Pre-skip name chars */
		if (!get_achar(&nam)) return 0;	/* Branch mismatched if less name chars */
	}
	if (*pat == 0 && inf) return 1;	/* (short circuit) */

	do {
		pp = pat; np = nam;			/* Top of pattern and name to match */
		for (;;) {
			if (*pp == '?' || *pp == '*') {	/* Wildcard? */
				nm = nx = 0;
				do {				/* Analyze the wildcard block */
					if (*pp++ == '?') nm++; else nx = 1;
				} while (*pp == '?' || *pp == '*');
				if (pattern_matching(pp, np, nm, nx)) return 1;	/* Test new branch (recurs upto number of wildcard blocks in the pattern) */
				nc = *np; break;	/* Branch mismatched */
			}
			pc = get_achar(&pp);	/* Get a pattern char */
			nc = get_achar(&np);	/* Get a name char */
			if (pc != nc) break;	/* Branch mismatched? */
			if (pc == 0) return 1;	/* Branch matched? (matched at end of both strings) */
		}
		get_achar(&nam);			/* nam++ */
	} while (inf && nc);			/* Retry until end of name if infinite search is specified */

	return 0;
}

#endif /* FF_USE_FIND && FF_FS_MINIMIZE <= 1 */



/*-----------------------------------------------------------------------*/
/* Pick a top segment and create the object name in directory form       */
/*-----------------------------------------------------------------------*/

static FRESULT create_name (	/* FR_OK: successful, FR_INVALID_NAME: could not create */
	DIR* dp,					/* Pointer to the directory object */
	const TCHAR** path			/* Pointer to pointer to the segment in the path string */
)
{
#if FF_USE_LFN		/* LFN configuration */
	BYTE b, cf;
	WCHAR wc, *lfn;
	DWORD uc;
	UINT i, ni, si, di;
	const TCHAR *p;


	/* Create LFN into LFN working buffer */
	p = *path; lfn = dp->obj.fs->lfnbuf; di = 0;
	for (;;) {
		uc = tchar2uni(&p);			/* Get a character */
		if (uc == 0xFFFFFFFF) return FR_INVALID_NAME;		/* Invalid code or UTF decode error */
		if (uc >= 0x10000) lfn[di++] = (WCHAR)(uc >> 16);	/* Store high surrogate if needed */
		wc = (WCHAR)uc;
		if (wc < ' ' || wc == '/' || wc == '\\') break;	/* Break if end of the path or a separator is found */
		if (wc < 0x80 && chk_chr("\"*:<>\?|\x7F", wc)) return FR_INVALID_NAME;	/* Reject illegal characters for LFN */
		if (di >= FF_MAX_LFN) return FR_INVALID_NAME;	/* Reject too long name */
		lfn[di++] = wc;					/* Store the Unicode character */
	}
	if (wc < ' ') {				/* End of path? */
		cf = NS_LAST;			/* Set last segment flag */
	} else {
		cf = 0;					/* Next segment follows */
		while (*p == '/' || *p == '\\') p++;	/* Skip duplicated separators if exist */
	}
	*path = p;					/* Return pointer to the next segment */

#if FF_FS_RPATH != 0
	if ((di == 1 && lfn[di - 1] == '.') ||
		(di == 2 && lfn[di - 1] == '.' && lfn[di - 2] == '.')) {	/* Is this segment a dot name? */
		lfn[di] = 0;
		for (i = 0; i < 11; i++) {		/* Create dot name for SFN entry */
			dp->fn[i] = (i < di) ? '.' : ' ';
		}
		dp->fn[i] = cf | NS_DOT;		/* This is a dot entry */
		return FR_OK;
	}
#endif
	while (di) {						/* Snip off trailing spaces and dots if exist */
		wc = lfn[di - 1];
		if (wc != ' ' && wc != '.') break;
		di--;
	}
	lfn[di] = 0;							/* LFN is created into the working buffer */
	if (di == 0) return FR_INVALID_NAME;	/* Reject null name */

	/* Create SFN in directory form */
	for (si = 0; lfn[si] == ' '; si++) ;	/* Remove leading spaces */
	if (si > 0 || lfn[si] == '.') cf |= NS_LOSS | NS_LFN;	/* Is there any leading space or dot? */
	while (di > 0 && lfn[di - 1] != '.') di--;	/* Find last dot (di<=si: no extension) */

	mem_set(dp->fn, ' ', 11);
	i = b = 0; ni = 8;
	for (;;) {
		wc = lfn[si++];					/* Get an LFN character */
		if (wc == 0) break;				/* Break on end of the LFN */
		if (wc == ' ' || (wc == '.' && si != di)) {	/* Remove embedded spaces and dots */
			cf |= NS_LOSS | NS_LFN;
			continue;
		}

		if (i >= ni || si == di) {		/* End of field? */
			if (ni == 11) {				/* Name extension overflow? */
				cf |= NS_LOSS | NS_LFN;
				break;
			}
			if (si != di) cf |= NS_LOSS | NS_LFN;	/* Name body overflow? */
			if (si > di) break;						/* No name extension? */
			si = di; i = 8; ni = 11; b <<= 2;		/* Enter name extension */
			continue;
		}

		if (wc >= 0x80) {	/* Is this a non-ASCII character? */
			cf |= NS_LFN;	/* LFN entry needs to be created */
#if FF_CODE_PAGE == 0
			if (ExCvt) {	/* At SBCS */
				wc = ff_uni2oem(wc, CODEPAGE);			/* Unicode ==> ANSI/OEM code */
				if (wc & 0x80) wc = ExCvt[wc & 0x7F];	/* Convert extended character to upper (SBCS) */
			} else {		/* At DBCS */
				wc = ff_uni2oem(ff_wtoupper(wc), CODEPAGE);	/* Unicode ==> Upper convert ==> ANSI/OEM code */
			}
#elif FF_CODE_PAGE < 900	/* SBCS cfg */
			wc = ff_uni2oem(wc, CODEPAGE);			/* Unicode ==> ANSI/OEM code */
			if (wc & 0x80) wc = ExCvt[wc & 0x7F];	/* Convert extended character to upper (SBCS) */
#else						/* DBCS cfg */
			wc = ff_uni2oem(ff_wtoupper(wc), CODEPAGE);	/* Unicode ==> Upper convert ==> ANSI/OEM code */
#endif
		}

		if (wc >= 0x100) {				/* Is this a DBC? */
			if (i >= ni - 1) {			/* Field overflow? */
				cf |= NS_LOSS | NS_LFN;
				i = ni; continue;		/* Next field */
			}
			dp->fn[i++] = (BYTE)(wc >> 8);	/* Put 1st byte */
		} else {						/* SBC */
			if (wc == 0 || chk_chr("+,;=[]", wc)) {	/* Replace illegal characters for SFN if needed */
				wc = '_'; cf |= NS_LOSS | NS_LFN;/* Lossy conversion */
			} else {
				if (IsUpper(wc)) {		/* ASCII upper case? */
					b |= 2;
				}
				if (IsLower(wc)) {		/* ASCII lower case? */
					b |= 1; wc -= 0x20;
				}
			}
		}
		dp->fn[i++] = (BYTE)wc;
	}

	if (dp->fn[0] == DDEM) dp->fn[0] = RDDEM;	/* If the first character collides with DDEM, replace it with RDDEM */

	if (ni == 8) b <<= 2;				/* Shift capital flags if no extension */
	if ((b & 0x0C) == 0x0C || (b & 0x03) == 0x03) cf |= NS_LFN;	/* LFN entry needs to be created if composite capitals */
	if (!(cf & NS_LFN)) {				/* When LFN is in 8.3 format without extended character, NT flags are created */
		if (b & 0x01) cf |= NS_EXT;		/* NT flag (Extension has small capital letters only) */
		if (b & 0x04) cf |= NS_BODY;	/* NT flag (Body has small capital letters only) */
	}

	dp->fn[NSFLAG] = cf;	/* SFN is created into dp->fn[] */

	return FR_OK;


#else	/* FF_USE_LFN : Non-LFN configuration */
	BYTE c, d, *sfn;
	UINT ni, si, i;
	const char *p;

	/* Create file name in directory form */
	p = *path; sfn = dp->fn;
	mem_set(sfn, ' ', 11);
	si = i = 0; ni = 8;
#if FF_FS_RPATH != 0
	if (p[si] == '.') { /* Is this a dot entry? */
		for (;;) {
			c = (BYTE)p[si++];
			if (c != '.' || si >= 3) break;
			sfn[i++] = c;
		}
		if (c != '/' && c != '\\' && c > ' ') return FR_INVALID_NAME;
		*path = p + si;								/* Return pointer to the next segment */
		sfn[NSFLAG] = (c <= ' ') ? NS_LAST | NS_DOT : NS_DOT;	/* Set last segment flag if end of the path */
		return FR_OK;
	}
#endif
	for (;;) {
		c = (BYTE)p[si++];				/* Get a byte */
		if (c <= ' ') break; 			/* Break if end of the path name */
		if (c == '/' || c == '\\') {	/* Break if a separator is found */
			while (p[si] == '/' || p[si] == '\\') si++;	/* Skip duplicated separator if exist */
			break;
		}
		if (c == '.' || i >= ni) {		/* End of body or field overflow? */
			if (ni == 11 || c != '.') return FR_INVALID_NAME;	/* Field overflow or invalid dot? */
			i = 8; ni = 11;				/* Enter file extension field */
			continue;
		}
#if FF_CODE_PAGE == 0
		if (ExCvt && c >= 0x80) {		/* Is SBC extended character? */
			c = ExCvt[c & 0x7F];		/* To upper SBC extended character */
		}
#elif FF_CODE_PAGE < 900
		if (c >= 0x80) {				/* Is SBC extended character? */
			c = ExCvt[c & 0x7F];		/* To upper SBC extended character */
		}
#endif
		if (dbc_1st(c)) {				/* Check if it is a DBC 1st byte */
			d = (BYTE)p[si++];			/* Get 2nd byte */
			if (!dbc_2nd(d) || i >= ni - 1) return FR_INVALID_NAME;	/* Reject invalid DBC */
			sfn[i++] = c;
			sfn[i++] = d;
		} else {						/* SBC */
			if (chk_chr("\"*+,:;<=>\?[]|\x7F", c)) return FR_INVALID_NAME;	/* Reject illegal chrs for SFN */
			if (IsLower(c)) c -= 0x20;	/* To upper */
			sfn[i++] = c;
		}
	}
	*path = p + si;						/* Return pointer to the next segment */
	if (i == 0) return FR_INVALID_NAME;	/* Reject nul string */

	if (sfn[0] == DDEM) sfn[0] = RDDEM;	/* If the first character collides with DDEM, replace it with RDDEM */
	sfn[NSFLAG] = (c <= ' ') ? NS_LAST : 0;		/* Set last segment flag if end of the path */

	return FR_OK;
#endif /* FF_USE_LFN */
}




/*-----------------------------------------------------------------------*/
/* Follow a file path                                                    */
/*-----------------------------------------------------------------------*/

static FRESULT follow_path (	/* FR_OK(0): successful, !=0: error code */
	DIR* dp,					/* Directory object to return last directory and found object */
	const TCHAR* path			/* Full-path string to find a file or directory */
)
{
	FRESULT res;
	BYTE ns;
	FATFS *fs = dp->obj.fs;


#if FF_FS_RPATH != 0
	if (*path != '/' && *path != '\\') {	/* Without heading separator */
		dp->obj.sclust = fs->cdir;				/* Start from current directory */
	} else
#endif
	{										/* With heading separator */
		while (*path == '/' || *path == '\\') path++;	/* Strip heading separator */
		dp->obj.sclust = 0;					/* Start from root directory */
	}
#if FF_FS_EXFAT
	dp->obj.n_frag = 0;	/* Invalidate last fragment counter of the object */
#if FF_FS_RPATH != 0
	if (fs->fs_type == FS_EXFAT && dp->obj.sclust) {	/* exFAT: Retrieve the sub-directory's status */
		DIR dj;

		dp->obj.c_scl = fs->cdc_scl;
		dp->obj.c_size = fs->cdc_size;
		dp->obj.c_ofs = fs->cdc_ofs;
		res = load_obj_xdir(&dj, &dp->obj);
		if (res != FR_OK) return res;
		dp->obj.objsize = ld_dword(fs->dirbuf + XDIR_FileSize);
		dp->obj.stat = fs->dirbuf[XDIR_GenFlags] & 2;
	}
#endif
#endif

	if ((UINT)*path < ' ') {				/* Null path name is the origin directory itself */
		dp->fn[NSFLAG] = NS_NONAME;
		res = dir_sdi(dp, 0);

	} else {								/* Follow path */
		for (;;) {
			res = create_name(dp, &path);	/* Get a segment name of the path */
			if (res != FR_OK) break;
			res = dir_find(dp);				/* Find an object with the segment name */
			ns = dp->fn[NSFLAG];
			if (res != FR_OK) {				/* Failed to find the object */
				if (res == FR_NO_FILE) {	/* Object is not found */
					if (FF_FS_RPATH && (ns & NS_DOT)) {	/* If dot entry is not exist, stay there */
						if (!(ns & NS_LAST)) continue;	/* Continue to follow if not last segment */
						dp->fn[NSFLAG] = NS_NONAME;
						res = FR_OK;
					} else {							/* Could not find the object */
						if (!(ns & NS_LAST)) res = FR_NO_PATH;	/* Adjust error code if not last segment */
					}
				}
				break;
			}
			if (ns & NS_LAST) break;			/* Last segment matched. Function completed. */
			/* Get into the sub-directory */
			if (!(dp->obj.attr & AM_DIR)) {		/* It is not a sub-directory and cannot follow */
				res = FR_NO_PATH; break;
			}
#if FF_FS_EXFAT
			if (fs->fs_type == FS_EXFAT) {		/* Save containing directory information for next dir */
				dp->obj.c_scl = dp->obj.sclust;
				dp->obj.c_size = ((DWORD)dp->obj.objsize & 0xFFFFFF00) | dp->obj.stat;
				dp->obj.c_ofs = dp->blk_ofs;
				init_alloc_info(fs, &dp->obj);	/* Open next directory */
			} else
#endif
			{
				dp->obj.sclust = ld_clust(fs, fs->win + dp->dptr % SS(fs));	/* Open next directory */
			}
		}
	}

	return res;
}




/*-----------------------------------------------------------------------*/
/* Get logical drive number from path name                               */
/*-----------------------------------------------------------------------*/

static int get_ldnumber (	/* Returns logical drive number (-1:invalid drive number or null pointer) */
	const TCHAR** path		/* Pointer to pointer to the path name */
)
{
	const TCHAR *tp, *tt;
	TCHAR tc;
	int i, vol = -1;
#if FF_STR_VOLUME_ID		/* Find string volume ID */
	const char *sp;
	char c;
#endif

	tt = tp = *path;
	if (!tp) return vol;	/* Invalid path name? */
	do tc = *tt++; while ((UINT)tc >= (FF_USE_LFN ? ' ' : '!') && tc != ':');	/* Find a colon in the path */

	if (tc == ':') {	/* DOS/Windows style volume ID? */
		i = FF_VOLUMES;
		if (IsDigit(*tp) && tp + 2 == tt) {	/* Is there a numeric volume ID + colon? */
			i = (int)*tp - '0';	/* Get the LD number */
		}
#if FF_STR_VOLUME_ID == 1	/* Arbitrary string is enabled */
		else {
			i = 0;
			do {
				sp = VolumeStr[i]; tp = *path;	/* This string volume ID and path name */
				do {	/* Compare the volume ID with path name */
					c = *sp++; tc = *tp++;
					if (IsLower(c)) c -= 0x20;
					if (IsLower(tc)) tc -= 0x20;
				} while (c && (TCHAR)c == tc);
			} while ((c || tp != tt) && ++i < FF_VOLUMES);	/* Repeat for each id until pattern match */
		}
#endif
		if (i < FF_VOLUMES) {	/* If a volume ID is found, get the drive number and strip it */
			vol = i;		/* Drive number */
			*path = tt;		/* Snip the drive prefix off */
		}
		return vol;
	}
#if FF_STR_VOLUME_ID == 2		/* Unix style volume ID is enabled */
	if (*tp == '/') {
		i = 0;
		do {
			sp = VolumeStr[i]; tp = *path;	/* This string volume ID and path name */
			do {	/* Compare the volume ID with path name */
				c = *sp++; tc = *(++tp);
				if (IsLower(c)) c -= 0x20;
				if (IsLower(tc)) tc -= 0x20;
			} while (c && (TCHAR)c == tc);
		} while ((c || (tc != '/' && (UINT)tc >= (FF_USE_LFN ? ' ' : '!'))) && ++i < FF_VOLUMES);	/* Repeat for each ID until pattern match */
		if (i < FF_VOLUMES) {	/* If a volume ID is found, get the drive number and strip it */
			vol = i;		/* Drive number */
			*path = tp;		/* Snip the drive prefix off */
			return vol;
		}
	}
#endif
	/* No drive prefix is found */
#if FF_FS_RPATH != 0
	vol = CurrVol;	/* Default drive is current drive */
#else
	vol = 0;		/* Default drive is 0 */
#endif
	return vol;		/* Return the default drive */
}




/*-----------------------------------------------------------------------*/
/* GPT support functions                                                 */
/*-----------------------------------------------------------------------*/

#if FF_LBA64

/* Calculate CRC32 in byte-by-byte */

static DWORD crc32 (	/* Returns next CRC value */
	DWORD crc,			/* Current CRC value */
	BYTE d				/* A byte to be processed */
)
{
	BYTE b;


	for (b = 1; b; b <<= 1) {
		crc ^= (d & b) ? 1 : 0;
		crc = (crc & 1) ? crc >> 1 ^ 0xEDB88320 : crc >> 1;
	}
	return crc;
}


/* Check validity of GPT header */

static int test_gpt_header (	/* 0:Invalid, 1:Valid */
	const BYTE* gpth			/* Pointer to the GPT header */
)
{
	UINT i;
	DWORD bcc;


	if (mem_cmp(gpth + GPTH_Sign, "EFI PART" "\0\0\1\0" "\x5C\0\0", 16)) return 0;	/* Check sign, version (1.0) and length (92) */
	for (i = 0, bcc = 0xFFFFFFFF; i < 92; i++) {		/* Check header BCC */
		bcc = crc32(bcc, i - GPTH_Bcc < 4 ? 0 : gpth[i]);
	}
	if (~bcc != ld_dword(gpth + GPTH_Bcc)) return 0;
	if (ld_dword(gpth + GPTH_PteSize) != SZ_GPTE) return 0;	/* Table entry size (must be SZ_GPTE bytes) */
	if (ld_dword(gpth + GPTH_PtNum) > 128) return 0;	/* Table size (must be 128 entries or less) */

	return 1;
}

#if !FF_FS_READONLY && FF_USE_MKFS

/* Generate random value */
static DWORD make_rand (
	DWORD seed,		/* Seed value */
	BYTE* buff,		/* Output buffer */
	UINT n			/* Data length */
)
{
	UINT r;


	if (seed == 0) seed = 1;
	do {
		for (r = 0; r < 8; r++) seed = seed & 1 ? seed >> 1 ^ 0xA3000000 : seed >> 1;	/* Shift 8 bits the 32-bit LFSR */
		*buff++ = (BYTE)seed;
	} while (--n);
	return seed;
}

#endif
#endif



/*-----------------------------------------------------------------------*/
/* Load a sector and check if it is an FAT VBR                           */
/*-----------------------------------------------------------------------*/

/* Check what the sector is */

static UINT check_fs (	/* 0:FAT VBR, 1:exFAT VBR, 2:Valid BS but not FAT, 3:Invalid BS, 4:Disk error */
	FATFS* fs,			/* Filesystem object */
	LBA_t sect			/* Sector to load and check if it is an FAT-VBR or not */
)
{
	fs->wflag = 0; fs->winsect = (LBA_t)0 - 1;		/* Invaidate window */
	if (move_window(fs, sect) != FR_OK) return 4;	/* Load the boot sector */

	if (ld_word(fs->win + BS_55AA) != 0xAA55) return 3;	/* Check boot signature (always here regardless of the sector size) */

	if (FF_FS_EXFAT && !mem_cmp(fs->win + BS_JmpBoot, "\xEB\x76\x90" "EXFAT   ", 11)) return 1;	/* Check if exFAT VBR */

	if (fs->win[BS_JmpBoot] == 0xE9 || fs->win[BS_JmpBoot] == 0xEB || fs->win[BS_JmpBoot] == 0xE8) {	/* Valid JumpBoot code? */
		if (!mem_cmp(fs->win + BS_FilSysType, "FAT", 3)) return 0;		/* Is it an FAT VBR? */
		if (!mem_cmp(fs->win + BS_FilSysType32, "FAT32", 5)) return 0;	/* Is it an FAT32 VBR? */
	}
	return 2;	/* Valid BS but not FAT */
}


/* Find an FAT volume */
/* (It supports only generic partitioning rules, MBR, GPT and SFD) */

static UINT find_volume (	/* Returns BS status found in the hosting drive */
	FATFS* fs,		/* Filesystem object */
	UINT part		/* Partition to fined = 0:auto, 1..:forced */
)
{
	UINT fmt, i;
	DWORD mbr_pt[4];


	fmt = check_fs(fs, 0);				/* Load sector 0 and check if it is an FAT VBR as SFD */
	if (fmt != 2 && (fmt >= 3 || part == 0)) return fmt;	/* Returns if it is a FAT VBR as auto scan, not a BS or disk error */

	/* Sector 0 is not an FAT VBR or forced partition number wants a partition */

#if FF_LBA64
	if (fs->win[MBR_Table + PTE_System] == 0xEE) {	/* GPT protective MBR? */
		DWORD n_ent, v_ent, ofs;
		QWORD pt_lba;

		if (move_window(fs, 1) != FR_OK) return 4;	/* Load GPT header sector (next to MBR) */
		if (!test_gpt_header(fs->win)) return 3;	/* Check if GPT header is valid */
		n_ent = ld_dword(fs->win + GPTH_PtNum);		/* Number of entries */
		pt_lba = ld_qword(fs->win + GPTH_PtOfs);	/* Table location */
		for (v_ent = i = 0; i < n_ent; i++) {		/* Find FAT partition */
			if (move_window(fs, pt_lba + i * SZ_GPTE / SS(fs)) != FR_OK) return 4;	/* PT sector */
			ofs = i * SZ_GPTE % SS(fs);												/* Offset in the sector */
			if (!mem_cmp(fs->win + ofs + GPTE_PtGuid, GUID_MS_Basic, 16)) {	/* MS basic data partition? */
				v_ent++;
				fmt = check_fs(fs, ld_qword(fs->win + ofs + GPTE_FstLba));	/* Load VBR and check status */
				if (part == 0 && fmt <= 1) return fmt;			/* Auto search (valid FAT volume found first) */
				if (part != 0 && v_ent == part) return fmt;		/* Forced partition order (regardless of it is valid or not) */
			}
		}
		return 3;	/* Not found */
	}
#endif
	if (FF_MULTI_PARTITION && part > 4) return 3;	/* MBR has 4 partitions max */
	for (i = 0; i < 4; i++) {		/* Load partition offset in the MBR */
		mbr_pt[i] = ld_dword(fs->win + MBR_Table + i * SZ_PTE + PTE_StLba);
	}
	i = part ? part - 1 : 0;		/* Table index to find first */
	do {							/* Find an FAT volume */
		fmt = mbr_pt[i] ? check_fs(fs, mbr_pt[i]) : 3;	/* Check if the partition is FAT */
	} while (part == 0 && fmt >= 2 && ++i < 4);
	return fmt;
}




/*-----------------------------------------------------------------------*/
/* Determine logical drive number and mount the volume if needed         */
/*-----------------------------------------------------------------------*/

static FRESULT mount_volume (	/* FR_OK(0): successful, !=0: an error occurred */
	const TCHAR** path,			/* Pointer to pointer to the path name (drive number) */
	FATFS** rfs,				/* Pointer to pointer to the found filesystem object */
	BYTE mode					/* !=0: Check write protection for write access */
)
{
	int vol;
	DSTATUS stat;
	LBA_t bsect;
	DWORD tsect, sysect, fasize, nclst, szbfat;
	WORD nrsv;
	FATFS *fs;
	UINT fmt;


	/* Get logical drive number */
	*rfs = 0;
	vol = get_ldnumber(path);
	if (vol < 0) return FR_INVALID_DRIVE;

	/* Check if the filesystem object is valid or not */
	fs = FatFs[vol];					/* Get pointer to the filesystem object */
	if (!fs) return FR_NOT_ENABLED;		/* Is the filesystem object available? */
#if FF_FS_REENTRANT
	if (!lock_fs(fs)) return FR_TIMEOUT;	/* Lock the volume */
#endif
	*rfs = fs;							/* Return pointer to the filesystem object */

	mode &= (BYTE)~FA_READ;				/* Desired access mode, write access or not */
	if (fs->fs_type != 0) {				/* If the volume has been mounted */
		stat = disk_status(fs->pdrv);
		if (!(stat & STA_NOINIT)) {		/* and the physical drive is kept initialized */
			if (!FF_FS_READONLY && mode && (stat & STA_PROTECT)) {	/* Check write protection if needed */
				return FR_WRITE_PROTECTED;
			}
			return FR_OK;				/* The filesystem object is already valid */
		}
	}

	/* The filesystem object is not valid. */
	/* Following code attempts to mount the volume. (find a FAT volume, analyze the BPB and initialize the filesystem object) */

	fs->fs_type = 0;					/* Clear the filesystem object */
	fs->pdrv = LD2PD(vol);				/* Volume hosting physical drive */
	stat = disk_initialize(fs->pdrv);	/* Initialize the physical drive */
	if (stat & STA_NOINIT) { 			/* Check if the initialization succeeded */
		return FR_NOT_READY;			/* Failed to initialize due to no medium or hard error */
	}
	if (!FF_FS_READONLY && mode && (stat & STA_PROTECT)) { /* Check disk write protection if needed */
		return FR_WRITE_PROTECTED;
	}
#if FF_MAX_SS != FF_MIN_SS				/* Get sector size (multiple sector size cfg only) */
	if (disk_ioctl(fs->pdrv, GET_SECTOR_SIZE, &SS(fs)) != RES_OK) return FR_DISK_ERR;
	if (SS(fs) > FF_MAX_SS || SS(fs) < FF_MIN_SS || (SS(fs) & (SS(fs) - 1))) return FR_DISK_ERR;
#endif

	/* Find an FAT volume on the drive */
	fmt = find_volume(fs, LD2PT(vol));
	if (fmt == 4) return FR_DISK_ERR;		/* An error occured in the disk I/O layer */
	if (fmt >= 2) return FR_NO_FILESYSTEM;	/* No FAT volume is found */
	bsect = fs->winsect;					/* Volume location */

	/* An FAT volume is found (bsect). Following code initializes the filesystem object */

#if FF_FS_EXFAT
	if (fmt == 1) {
		QWORD maxlba;
		DWORD so, cv, bcl, i;

		for (i = BPB_ZeroedEx; i < BPB_ZeroedEx + 53 && fs->win[i] == 0; i++) ;	/* Check zero filler */
		if (i < BPB_ZeroedEx + 53) return FR_NO_FILESYSTEM;

		if (ld_word(fs->win + BPB_FSVerEx) != 0x100) return FR_NO_FILESYSTEM;	/* Check exFAT version (must be version 1.0) */

		if (1 << fs->win[BPB_BytsPerSecEx] != SS(fs)) {	/* (BPB_BytsPerSecEx must be equal to the physical sector size) */
			return FR_NO_FILESYSTEM;
		}

		maxlba = ld_qword(fs->win + BPB_TotSecEx) + bsect;	/* Last LBA + 1 of the volume */
		if (!FF_LBA64 && maxlba >= 0x100000000) return FR_NO_FILESYSTEM;	/* (It cannot be handled in 32-bit LBA) */

		fs->fsize = ld_dword(fs->win + BPB_FatSzEx);	/* Number of sectors per FAT */

		fs->n_fats = fs->win[BPB_NumFATsEx];			/* Number of FATs */
		if (fs->n_fats != 1) return FR_NO_FILESYSTEM;	/* (Supports only 1 FAT) */

		fs->csize = 1 << fs->win[BPB_SecPerClusEx];		/* Cluster size */
		if (fs->csize == 0)	return FR_NO_FILESYSTEM;	/* (Must be 1..32768) */

		nclst = ld_dword(fs->win + BPB_NumClusEx);		/* Number of clusters */
		if (nclst > MAX_EXFAT) return FR_NO_FILESYSTEM;	/* (Too many clusters) */
		fs->n_fatent = nclst + 2;

		/* Boundaries and Limits */
		fs->volbase = bsect;
		fs->database = bsect + ld_dword(fs->win + BPB_DataOfsEx);
		fs->fatbase = bsect + ld_dword(fs->win + BPB_FatOfsEx);
		if (maxlba < (QWORD)fs->database + nclst * fs->csize) return FR_NO_FILESYSTEM;	/* (Volume size must not be smaller than the size requiered) */
		fs->dirbase = ld_dword(fs->win + BPB_RootClusEx);

		/* Get bitmap location and check if it is contiguous (implementation assumption) */
		so = i = 0;
		for (;;) {	/* Find the bitmap entry in the root directory (in only first cluster) */
			if (i == 0) {
				if (so >= fs->csize) return FR_NO_FILESYSTEM;	/* Not found? */
				if (move_window(fs, clst2sect(fs, (DWORD)fs->dirbase) + so) != FR_OK) return FR_DISK_ERR;
				so++;
			}
			if (fs->win[i] == ET_BITMAP) break;				/* Is it a bitmap entry? */
			i = (i + SZDIRE) % SS(fs);	/* Next entry */
		}
		bcl = ld_dword(fs->win + i + 20);					/* Bitmap cluster */
		if (bcl < 2 || bcl >= fs->n_fatent) return FR_NO_FILESYSTEM;
		fs->bitbase = fs->database + fs->csize * (bcl - 2);	/* Bitmap sector */
		for (;;) {	/* Check if bitmap is contiguous */
			if (move_window(fs, fs->fatbase + bcl / (SS(fs) / 4)) != FR_OK) return FR_DISK_ERR;
			cv = ld_dword(fs->win + bcl % (SS(fs) / 4) * 4);
			if (cv == 0xFFFFFFFF) break;				/* Last link? */
			if (cv != ++bcl) return FR_NO_FILESYSTEM;	/* Fragmented? */
		}

#if !FF_FS_READONLY
		fs->last_clst = fs->free_clst = 0xFFFFFFFF;		/* Initialize cluster allocation information */
#endif
		fmt = FS_EXFAT;			/* FAT sub-type */
	} else
#endif	/* FF_FS_EXFAT */
	{
		if (ld_word(fs->win + BPB_BytsPerSec) != SS(fs)) return FR_NO_FILESYSTEM;	/* (BPB_BytsPerSec must be equal to the physical sector size) */

		fasize = ld_word(fs->win + BPB_FATSz16);		/* Number of sectors per FAT */
		if (fasize == 0) fasize = ld_dword(fs->win + BPB_FATSz32);
		fs->fsize = fasize;

		fs->n_fats = fs->win[BPB_NumFATs];				/* Number of FATs */
		if (fs->n_fats != 1 && fs->n_fats != 2) return FR_NO_FILESYSTEM;	/* (Must be 1 or 2) */
		fasize *= fs->n_fats;							/* Number of sectors for FAT area */

		fs->csize = fs->win[BPB_SecPerClus];			/* Cluster size */
		if (fs->csize == 0 || (fs->csize & (fs->csize - 1))) return FR_NO_FILESYSTEM;	/* (Must be power of 2) */

		fs->n_rootdir = ld_word(fs->win + BPB_RootEntCnt);	/* Number of root directory entries */
		if (fs->n_rootdir % (SS(fs) / SZDIRE)) return FR_NO_FILESYSTEM;	/* (Must be sector aligned) */

		tsect = ld_word(fs->win + BPB_TotSec16);		/* Number of sectors on the volume */
		if (tsect == 0) tsect = ld_dword(fs->win + BPB_TotSec32);

		nrsv = ld_word(fs->win + BPB_RsvdSecCnt);		/* Number of reserved sectors */
		if (nrsv == 0) return FR_NO_FILESYSTEM;			/* (Must not be 0) */

		/* Determine the FAT sub type */
		sysect = nrsv + fasize + fs->n_rootdir / (SS(fs) / SZDIRE);	/* RSV + FAT + DIR */
		if (tsect < sysect) return FR_NO_FILESYSTEM;	/* (Invalid volume size) */
		nclst = (tsect - sysect) / fs->csize;			/* Number of clusters */
		if (nclst == 0) return FR_NO_FILESYSTEM;		/* (Invalid volume size) */
		fmt = 0;
		if (nclst <= MAX_FAT32) fmt = FS_FAT32;
		if (nclst <= MAX_FAT16) fmt = FS_FAT16;
		if (nclst <= MAX_FAT12) fmt = FS_FAT12;
		if (fmt == 0) return FR_NO_FILESYSTEM;

		/* Boundaries and Limits */
		fs->n_fatent = nclst + 2;						/* Number of FAT entries */
		fs->volbase = bsect;							/* Volume start sector */
		fs->fatbase = bsect + nrsv; 					/* FAT start sector */
		fs->database = bsect + sysect;					/* Data start sector */
		if (fmt == FS_FAT32) {
			if (ld_word(fs->win + BPB_FSVer32) != 0) return FR_NO_FILESYSTEM;	/* (Must be FAT32 revision 0.0) */
			if (fs->n_rootdir != 0) return FR_NO_FILESYSTEM;	/* (BPB_RootEntCnt must be 0) */
			fs->dirbase = ld_dword(fs->win + BPB_RootClus32);	/* Root directory start cluster */
			szbfat = fs->n_fatent * 4;					/* (Needed FAT size) */
		} else {
			if (fs->n_rootdir == 0)	return FR_NO_FILESYSTEM;	/* (BPB_RootEntCnt must not be 0) */
			fs->dirbase = fs->fatbase + fasize;			/* Root directory start sector */
			szbfat = (fmt == FS_FAT16) ?				/* (Needed FAT size) */
				fs->n_fatent * 2 : fs->n_fatent * 3 / 2 + (fs->n_fatent & 1);
		}
		if (fs->fsize < (szbfat + (SS(fs) - 1)) / SS(fs)) return FR_NO_FILESYSTEM;	/* (BPB_FATSz must not be less than the size needed) */

#if !FF_FS_READONLY
		/* Get FSInfo if available */
		fs->last_clst = fs->free_clst = 0xFFFFFFFF;		/* Initialize cluster allocation information */
		fs->fsi_flag = 0x80;
#if (FF_FS_NOFSINFO & 3) != 3
		if (fmt == FS_FAT32				/* Allow to update FSInfo only if BPB_FSInfo32 == 1 */
			&& ld_word(fs->win + BPB_FSInfo32) == 1
			&& move_window(fs, bsect + 1) == FR_OK)
		{
			fs->fsi_flag = 0;
			if (ld_word(fs->win + BS_55AA) == 0xAA55	/* Load FSInfo data if available */
				&& ld_dword(fs->win + FSI_LeadSig) == 0x41615252
				&& ld_dword(fs->win + FSI_StrucSig) == 0x61417272)
			{
#if (FF_FS_NOFSINFO & 1) == 0
				fs->free_clst = ld_dword(fs->win + FSI_Free_Count);
#endif
#if (FF_FS_NOFSINFO & 2) == 0
				fs->last_clst = ld_dword(fs->win + FSI_Nxt_Free);
#endif
			}
		}
#endif	/* (FF_FS_NOFSINFO & 3) != 3 */
#endif	/* !FF_FS_READONLY */
	}

	fs->fs_type = (BYTE)fmt;/* FAT sub-type */
	fs->id = ++Fsid;		/* Volume mount ID */
#if FF_USE_LFN == 1
	fs->lfnbuf = LfnBuf;	/* Static LFN working buffer */
#if FF_FS_EXFAT
	fs->dirbuf = DirBuf;	/* Static directory block scratchpad buuffer */
#endif
#endif
#if FF_FS_RPATH != 0
	fs->cdir = 0;			/* Initialize current directory */
#endif
#if FF_FS_LOCK != 0			/* Clear file lock semaphores */
	clear_lock(fs);
#endif
	return FR_OK;
}




/*-----------------------------------------------------------------------*/
/* Check if the file/directory object is valid or not                    */
/*-----------------------------------------------------------------------*/

static FRESULT validate (	/* Returns FR_OK or FR_INVALID_OBJECT */
	FFOBJID* obj,			/* Pointer to the FFOBJID, the 1st member in the FIL/DIR object, to check validity */
	FATFS** rfs				/* Pointer to pointer to the owner filesystem object to return */
)
{
	FRESULT res = FR_INVALID_OBJECT;


	if (obj && obj->fs && obj->fs->fs_type && obj->id == obj->fs->id) {	/* Test if the object is valid */
#if FF_FS_REENTRANT
		if (lock_fs(obj->fs)) {	/* Obtain the filesystem object */
			if (!(disk_status(obj->fs->pdrv) & STA_NOINIT)) { /* Test if the phsical drive is kept initialized */
				res = FR_OK;
			} else {
				unlock_fs(obj->fs, FR_OK);
			}
		} else {
			res = FR_TIMEOUT;
		}
#else
		if (!(disk_status(obj->fs->pdrv) & STA_NOINIT)) { /* Test if the phsical drive is kept initialized */
			res = FR_OK;
		}
#endif
	}
	*rfs = (res == FR_OK) ? obj->fs : 0;	/* Corresponding filesystem object */
	return res;
}




/*---------------------------------------------------------------------------

   Public Functions (FatFs API)

----------------------------------------------------------------------------*/



/*-----------------------------------------------------------------------*/
/* Mount/Unmount a Logical Drive                                         */
/*-----------------------------------------------------------------------*/

FRESULT f_mount (
	FATFS* fs,			/* Pointer to the filesystem object (NULL:unmount)*/
	const TCHAR* path,	/* Logical drive number to be mounted/unmounted */
	BYTE opt			/* Mode option 0:Do not mount (delayed mount), 1:Mount immediately */
)
{
	FATFS *cfs;
	int vol;
	FRESULT res;
	const TCHAR *rp = path;


	/* Get logical drive number */
	vol = get_ldnumber(&rp);
	if (vol < 0) return FR_INVALID_DRIVE;
	cfs = FatFs[vol];					/* Pointer to fs object */

	if (cfs) {
#if FF_FS_LOCK != 0
		clear_lock(cfs);
#endif
#if FF_FS_REENTRANT						/* Discard sync object of the current volume */
		if (!ff_del_syncobj(cfs->sobj)) return FR_INT_ERR;
#endif
		cfs->fs_type = 0;				/* Clear old fs object */
	}

	if (fs) {
		fs->fs_type = 0;				/* Clear new fs object */
#if FF_FS_REENTRANT						/* Create sync object for the new volume */
		if (!ff_cre_syncobj((BYTE)vol, &fs->sobj)) return FR_INT_ERR;
#endif
	}
	FatFs[vol] = fs;					/* Register new fs object */

	if (opt == 0) return FR_OK;			/* Do not mount now, it will be mounted later */

	res = mount_volume(&path, &fs, 0);	/* Force mounted the volume */
	LEAVE_FF(fs, res);
}




/*-----------------------------------------------------------------------*/
/* Open or Create a File                                                 */
/*-----------------------------------------------------------------------*/

FRESULT f_open (
	FIL* fp,			/* Pointer to the blank file object */
	const TCHAR* path,	/* Pointer to the file name */
	BYTE mode			/* Access mode and file open mode flags */
)
{
	FRESULT res;
	DIR dj;
	FATFS *fs;
#if !FF_FS_READONLY
	DWORD cl, bcs, clst;
	LBA_t sc;
	FSIZE_t ofs;
#endif
	DEF_NAMBUF


	if (!fp) return FR_INVALID_OBJECT;

	/* Get logical drive number */
	mode &= FF_FS_READONLY ? FA_READ : FA_READ | FA_WRITE | FA_CREATE_ALWAYS | FA_CREATE_NEW | FA_OPEN_ALWAYS | FA_OPEN_APPEND;
	res = mount_volume(&path, &fs, mode);
	if (res == FR_OK) {
		dj.obj.fs = fs;
		INIT_NAMBUF(fs);
		res = follow_path(&dj, path);	/* Follow the file path */
#if !FF_FS_READONLY	/* Read/Write configuration */
		if (res == FR_OK) {
			if (dj.fn[NSFLAG] & NS_NONAME) {	/* Origin directory itself? */
				res = FR_INVALID_NAME;
			}
#if FF_FS_LOCK != 0
			else {
				res = chk_lock(&dj, (mode & ~FA_READ) ? 1 : 0);		/* Check if the file can be used */
			}
#endif
		}
		/* Create or Open a file */
		if (mode & (FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW)) {
			if (res != FR_OK) {					/* No file, create new */
				if (res == FR_NO_FILE) {		/* There is no file to open, create a new entry */
#if FF_FS_LOCK != 0
					res = enq_lock() ? dir_register(&dj) : FR_TOO_MANY_OPEN_FILES;
#else
					res = dir_register(&dj);
#endif
				}
				mode |= FA_CREATE_ALWAYS;		/* File is created */
			}
			else {								/* Any object with the same name is already existing */
				if (dj.obj.attr & (AM_RDO | AM_DIR)) {	/* Cannot overwrite it (R/O or DIR) */
					res = FR_DENIED;
				} else {
					if (mode & FA_CREATE_NEW) res = FR_EXIST;	/* Cannot create as new file */
				}
			}
			if (res == FR_OK && (mode & FA_CREATE_ALWAYS)) {	/* Truncate the file if overwrite mode */
#if FF_FS_EXFAT
				if (fs->fs_type == FS_EXFAT) {
					/* Get current allocation info */
					fp->obj.fs = fs;
					init_alloc_info(fs, &fp->obj);
					/* Set directory entry block initial state */
					mem_set(fs->dirbuf + 2, 0, 30);		/* Clear 85 entry except for NumSec */
					mem_set(fs->dirbuf + 38, 0, 26);	/* Clear C0 entry except for NumName and NameHash */
					fs->dirbuf[XDIR_Attr] = AM_ARC;
					st_dword(fs->dirbuf + XDIR_CrtTime, GET_FATTIME());
					fs->dirbuf[XDIR_GenFlags] = 1;
					res = store_xdir(&dj);
					if (res == FR_OK && fp->obj.sclust != 0) {	/* Remove the cluster chain if exist */
						res = remove_chain(&fp->obj, fp->obj.sclust, 0);
						fs->last_clst = fp->obj.sclust - 1;		/* Reuse the cluster hole */
					}
				} else
#endif
				{
					/* Set directory entry initial state */
					cl = ld_clust(fs, dj.dir);			/* Get current cluster chain */
					st_dword(dj.dir + DIR_CrtTime, GET_FATTIME());	/* Set created time */
					dj.dir[DIR_Attr] = AM_ARC;			/* Reset attribute */
					st_clust(fs, dj.dir, 0);			/* Reset file allocation info */
					st_dword(dj.dir + DIR_FileSize, 0);
					fs->wflag = 1;
					if (cl != 0) {						/* Remove the cluster chain if exist */
						sc = fs->winsect;
						res = remove_chain(&dj.obj, cl, 0);
						if (res == FR_OK) {
							res = move_window(fs, sc);
							fs->last_clst = cl - 1;		/* Reuse the cluster hole */
						}
					}
				}
			}
		}
		else {	/* Open an existing file */
			if (res == FR_OK) {					/* Is the object exsiting? */
				if (dj.obj.attr & AM_DIR) {		/* File open against a directory */
					res = FR_NO_FILE;
				} else {
					if ((mode & FA_WRITE) && (dj.obj.attr & AM_RDO)) { /* Write mode open against R/O file */
						res = FR_DENIED;
					}
				}
			}
		}
		if (res == FR_OK) {
			if (mode & FA_CREATE_ALWAYS) mode |= FA_MODIFIED;	/* Set file change flag if created or overwritten */
			fp->dir_sect = fs->winsect;			/* Pointer to the directory entry */
			fp->dir_ptr = dj.dir;
#if FF_FS_LOCK != 0
			fp->obj.lockid = inc_lock(&dj, (mode & ~FA_READ) ? 1 : 0);	/* Lock the file for this session */
			if (fp->obj.lockid == 0) res = FR_INT_ERR;
#endif
		}
#else		/* R/O configuration */
		if (res == FR_OK) {
			if (dj.fn[NSFLAG] & NS_NONAME) {	/* Is it origin directory itself? */
				res = FR_INVALID_NAME;
			} else {
				if (dj.obj.attr & AM_DIR) {		/* Is it a directory? */
					res = FR_NO_FILE;
				}
			}
		}
#endif

		if (res == FR_OK) {
#if FF_FS_EXFAT
			if (fs->fs_type == FS_EXFAT) {
				fp->obj.c_scl = dj.obj.sclust;							/* Get containing directory info */
				fp->obj.c_size = ((DWORD)dj.obj.objsize & 0xFFFFFF00) | dj.obj.stat;
				fp->obj.c_ofs = dj.blk_ofs;
				init_alloc_info(fs, &fp->obj);
			} else
#endif
			{
				fp->obj.sclust = ld_clust(fs, dj.dir);					/* Get object allocation info */
				fp->obj.objsize = ld_dword(dj.dir + DIR_FileSize);
			}
#if FF_USE_FASTSEEK
			fp->cltbl = 0;			/* Disable fast seek mode */
#endif
			fp->obj.fs = fs;	 	/* Validate the file object */
			fp->obj.id = fs->id;
			fp->flag = mode;		/* Set file access mode */
			fp->err = 0;			/* Clear error flag */
			fp->sect = 0;			/* Invalidate current data sector */
			fp->fptr = 0;			/* Set file pointer top of the file */
#if !FF_FS_READONLY
#if !FF_FS_TINY
			mem_set(fp->buf, 0, sizeof fp->buf);	/* Clear sector buffer */
#endif
			if ((mode & FA_SEEKEND) && fp->obj.objsize > 0) {	/* Seek to end of file if FA_OPEN_APPEND is specified */
				fp->fptr = fp->obj.objsize;			/* Offset to seek */
				bcs = (DWORD)fs->csize * SS(fs);	/* Cluster size in byte */
				clst = fp->obj.sclust;				/* Follow the cluster chain */
				for (ofs = fp->obj.objsize; res == FR_OK && ofs > bcs; ofs -= bcs) {
					clst = get_fat(&fp->obj, clst);
					if (clst <= 1) res = FR_INT_ERR;
					if (clst == 0xFFFFFFFF) res = FR_DISK_ERR;
				}
				fp->clust = clst;
				if (res == FR_OK && ofs % SS(fs)) {	/* Fill sector buffer if not on the sector boundary */
					sc = clst2sect(fs, clst);
					if (sc == 0) {
						res = FR_INT_ERR;
					} else {
						fp->sect = sc + (DWORD)(ofs / SS(fs));
#if !FF_FS_TINY
						if (disk_read(fs->pdrv, fp->buf, fp->sect, 1) != RES_OK) res = FR_DISK_ERR;
#endif
					}
				}
			}
#endif
		}

		FREE_NAMBUF();
	}

	if (res != FR_OK) fp->obj.fs = 0;	/* Invalidate file object on error */

	LEAVE_FF(fs, res);
}




/*-----------------------------------------------------------------------*/
/* Read File                                                             */
/*-----------------------------------------------------------------------*/

FRESULT f_read (
	FIL* fp, 	/* Pointer to the file object */
	void* buff,	/* Pointer to data buffer */
	UINT btr,	/* Number of bytes to read */
	UINT* br	/* Pointer to number of bytes read */
)
{
	FRESULT res;
	FATFS *fs;
	DWORD clst;
	LBA_t sect;
	FSIZE_t remain;
	UINT rcnt, cc, csect;
	BYTE *rbuff = (BYTE*)buff;


	*br = 0;	/* Clear read byte counter */
	res = validate(&fp->obj, &fs);				/* Check validity of the file object */
	if (res != FR_OK || (res = (FRESULT)fp->err) != FR_OK) LEAVE_FF(fs, res);	/* Check validity */
	if (!(fp->flag & FA_READ)) LEAVE_FF(fs, FR_DENIED); /* Check access mode */
	remain = fp->obj.objsize - fp->fptr;
	if (btr > remain) btr = (UINT)remain;		/* Truncate btr by remaining bytes */

	for ( ;  btr;								/* Repeat until btr bytes read */
		btr -= rcnt, *br += rcnt, rbuff += rcnt, fp->fptr += rcnt) {
		if (fp->fptr % SS(fs) == 0) {			/* On the sector boundary? */
			csect = (UINT)(fp->fptr / SS(fs) & (fs->csize - 1));	/* Sector offset in the cluster */
			if (csect == 0) {					/* On the cluster boundary? */
				if (fp->fptr == 0) {			/* On the top of the file? */
					clst = fp->obj.sclust;		/* Follow cluster chain from the origin */
				} else {						/* Middle or end of the file */
#if FF_USE_FASTSEEK
					if (fp->cltbl) {
						clst = clmt_clust(fp, fp->fptr);	/* Get cluster# from the CLMT */
					} else
#endif
					{
						clst = get_fat(&fp->obj, fp->clust);	/* Follow cluster chain on the FAT */
					}
				}
				if (clst < 2) ABORT(fs, FR_INT_ERR);
				if (clst == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
				fp->clust = clst;				/* Update current cluster */
			}
			sect = clst2sect(fs, fp->clust);	/* Get current sector */
			if (sect == 0) ABORT(fs, FR_INT_ERR);
			sect += csect;
			cc = btr / SS(fs);					/* When remaining bytes >= sector size, */
			if (cc > 0) {						/* Read maximum contiguous sectors directly */
				if (csect + cc > fs->csize) {	/* Clip at cluster boundary */
					cc = fs->csize - csect;
				}
				if (disk_read(fs->pdrv, rbuff, sect, cc) != RES_OK) ABORT(fs, FR_DISK_ERR);
#if !FF_FS_READONLY && FF_FS_MINIMIZE <= 2		/* Replace one of the read sectors with cached data if it contains a dirty sector */
#if FF_FS_TINY
				if (fs->wflag && fs->winsect - sect < cc) {
					mem_cpy(rbuff + ((fs->winsect - sect) * SS(fs)), fs->win, SS(fs));
				}
#else
				if ((fp->flag & FA_DIRTY) && fp->sect - sect < cc) {
					mem_cpy(rbuff + ((fp->sect - sect) * SS(fs)), fp->buf, SS(fs));
				}
#endif
#endif
				rcnt = SS(fs) * cc;				/* Number of bytes transferred */
				continue;
			}
#if !FF_FS_TINY
			if (fp->sect != sect) {			/* Load data sector if not in cache */
#if !FF_FS_READONLY
				if (fp->flag & FA_DIRTY) {		/* Write-back dirty sector cache */
					if (disk_write(fs->pdrv, fp->buf, fp->sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
					fp->flag &= (BYTE)~FA_DIRTY;
				}
#endif
				if (disk_read(fs->pdrv, fp->buf, sect, 1) != RES_OK)	ABORT(fs, FR_DISK_ERR);	/* Fill sector cache */
			}
#endif
			fp->sect = sect;
		}
		rcnt = SS(fs) - (UINT)fp->fptr % SS(fs);	/* Number of bytes remains in the sector */
		if (rcnt > btr) rcnt = btr;					/* Clip it by btr if needed */
#if FF_FS_TINY
		if (move_window(fs, fp->sect) != FR_OK) ABORT(fs, FR_DISK_ERR);	/* Move sector window */
		mem_cpy(rbuff, fs->win + fp->fptr % SS(fs), rcnt);	/* Extract partial sector */
#else
		mem_cpy(rbuff, fp->buf + fp->fptr % SS(fs), rcnt);	/* Extract partial sector */
#endif
	}

	LEAVE_FF(fs, FR_OK);
}




#if !FF_FS_READONLY
/*-----------------------------------------------------------------------*/
/* Write File                                                            */
/*-----------------------------------------------------------------------*/

FRESULT f_write (
	FIL* fp,			/* Pointer to the file object */
	const void* buff,	/* Pointer to the data to be written */
	UINT btw,			/* Number of bytes to write */
	UINT* bw			/* Pointer to number of bytes written */
)
{
	FRESULT res;
	FATFS *fs;
	DWORD clst;
	LBA_t sect;
	UINT wcnt, cc, csect;
	const BYTE *wbuff = (const BYTE*)buff;


	*bw = 0;	/* Clear write byte counter */
	res = validate(&fp->obj, &fs);			/* Check validity of the file object */
	if (res != FR_OK || (res = (FRESULT)fp->err) != FR_OK) LEAVE_FF(fs, res);	/* Check validity */
	if (!(fp->flag & FA_WRITE)) LEAVE_FF(fs, FR_DENIED);	/* Check access mode */

	/* Check fptr wrap-around (file size cannot reach 4 GiB at FAT volume) */
	if ((!FF_FS_EXFAT || fs->fs_type != FS_EXFAT) && (DWORD)(fp->fptr + btw) < (DWORD)fp->fptr) {
		btw = (UINT)(0xFFFFFFFF - (DWORD)fp->fptr);
	}

	for ( ;  btw;							/* Repeat until all data written */
		btw -= wcnt, *bw += wcnt, wbuff += wcnt, fp->fptr += wcnt, fp->obj.objsize = (fp->fptr > fp->obj.objsize) ? fp->fptr : fp->obj.objsize) {
		if (fp->fptr % SS(fs) == 0) {		/* On the sector boundary? */
			csect = (UINT)(fp->fptr / SS(fs)) & (fs->csize - 1);	/* Sector offset in the cluster */
			if (csect == 0) {				/* On the cluster boundary? */
				if (fp->fptr == 0) {		/* On the top of the file? */
					clst = fp->obj.sclust;	/* Follow from the origin */
					if (clst == 0) {		/* If no cluster is allocated, */
						clst = create_chain(&fp->obj, 0);	/* create a new cluster chain */
					}
				} else {					/* On the middle or end of the file */
#if FF_USE_FASTSEEK
					if (fp->cltbl) {
						clst = clmt_clust(fp, fp->fptr);	/* Get cluster# from the CLMT */
					} else
#endif
					{
						clst = create_chain(&fp->obj, fp->clust);	/* Follow or stretch cluster chain on the FAT */
					}
				}
				if (clst == 0) break;		/* Could not allocate a new cluster (disk full) */
				if (clst == 1) ABORT(fs, FR_INT_ERR);
				if (clst == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
				fp->clust = clst;			/* Update current cluster */
				if (fp->obj.sclust == 0) fp->obj.sclust = clst;	/* Set start cluster if the first write */
			}
#if FF_FS_TINY
			if (fs->winsect == fp->sect && sync_window(fs) != FR_OK) ABORT(fs, FR_DISK_ERR);	/* Write-back sector cache */
#else
			if (fp->flag & FA_DIRTY) {		/* Write-back sector cache */
				if (disk_write(fs->pdrv, fp->buf, fp->sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
				fp->flag &= (BYTE)~FA_DIRTY;
			}
#endif
			sect = clst2sect(fs, fp->clust);	/* Get current sector */
			if (sect == 0) ABORT(fs, FR_INT_ERR);
			sect += csect;
			cc = btw / SS(fs);				/* When remaining bytes >= sector size, */
			if (cc > 0) {					/* Write maximum contiguous sectors directly */
				if (csect + cc > fs->csize) {	/* Clip at cluster boundary */
					cc = fs->csize - csect;
				}
				if (disk_write(fs->pdrv, wbuff, sect, cc) != RES_OK) ABORT(fs, FR_DISK_ERR);
#if FF_FS_MINIMIZE <= 2
#if FF_FS_TINY
				if (fs->winsect - sect < cc) {	/* Refill sector cache if it gets invalidated by the direct write */
					mem_cpy(fs->win, wbuff + ((fs->winsect - sect) * SS(fs)), SS(fs));
					fs->wflag = 0;
				}
#else
				if (fp->sect - sect < cc) { /* Refill sector cache if it gets invalidated by the direct write */
					mem_cpy(fp->buf, wbuff + ((fp->sect - sect) * SS(fs)), SS(fs));
					fp->flag &= (BYTE)~FA_DIRTY;
				}
#endif
#endif
				wcnt = SS(fs) * cc;		/* Number of bytes transferred */
				continue;
			}
#if FF_FS_TINY
			if (fp->fptr >= fp->obj.objsize) {	/* Avoid silly cache filling on the growing edge */
				if (sync_window(fs) != FR_OK) ABORT(fs, FR_DISK_ERR);
				fs->winsect = sect;
			}
#else
			if (fp->sect != sect && 		/* Fill sector cache with file data */
				fp->fptr < fp->obj.objsize &&
				disk_read(fs->pdrv, fp->buf, sect, 1) != RES_OK) {
					ABORT(fs, FR_DISK_ERR);
			}
#endif
			fp->sect = sect;
		}
		wcnt = SS(fs) - (UINT)fp->fptr % SS(fs);	/* Number of bytes remains in the sector */
		if (wcnt > btw) wcnt = btw;					/* Clip it by btw if needed */
#if FF_FS_TINY
		if (move_window(fs, fp->sect) != FR_OK) ABORT(fs, FR_DISK_ERR);	/* Move sector window */
		mem_cpy(fs->win + fp->fptr % SS(fs), wbuff, wcnt);	/* Fit data to the sector */
		fs->wflag = 1;
#else
		mem_cpy(fp->buf + fp->fptr % SS(fs), wbuff, wcnt);	/* Fit data to the sector */
		fp->flag |= FA_DIRTY;
#endif
	}

	fp->flag |= FA_MODIFIED;				/* Set file change flag */

	LEAVE_FF(fs, FR_OK);
}




/*-----------------------------------------------------------------------*/
/* Synchronize the File                                                  */
/*-----------------------------------------------------------------------*/

FRESULT f_sync (
	FIL* fp		/* Pointer to the file object */
)
{
	FRESULT res;
	FATFS *fs;
	DWORD tm;
	BYTE *dir;


	res = validate(&fp->obj, &fs);	/* Check validity of the file object */
	if (res == FR_OK) {
		if (fp->flag & FA_MODIFIED) {	/* Is there any change to the file? */
#if !FF_FS_TINY
			if (fp->flag & FA_DIRTY) {	/* Write-back cached data if needed */
				if (disk_write(fs->pdrv, fp->buf, fp->sect, 1) != RES_OK) LEAVE_FF(fs, FR_DISK_ERR);
				fp->flag &= (BYTE)~FA_DIRTY;
			}
#endif
			/* Update the directory entry */
			tm = GET_FATTIME();				/* Modified time */
#if FF_FS_EXFAT
			if (fs->fs_type == FS_EXFAT) {
				res = fill_first_frag(&fp->obj);	/* Fill first fragment on the FAT if needed */
				if (res == FR_OK) {
					res = fill_last_frag(&fp->obj, fp->clust, 0xFFFFFFFF);	/* Fill last fragment on the FAT if needed */
				}
				if (res == FR_OK) {
					DIR dj;
					DEF_NAMBUF

					INIT_NAMBUF(fs);
					res = load_obj_xdir(&dj, &fp->obj);	/* Load directory entry block */
					if (res == FR_OK) {
						fs->dirbuf[XDIR_Attr] |= AM_ARC;				/* Set archive attribute to indicate that the file has been changed */
						fs->dirbuf[XDIR_GenFlags] = fp->obj.stat | 1;	/* Update file allocation information */
						st_dword(fs->dirbuf + XDIR_FstClus, fp->obj.sclust);		/* Update start cluster */
						st_qword(fs->dirbuf + XDIR_FileSize, fp->obj.objsize);		/* Update file size */
						st_qword(fs->dirbuf + XDIR_ValidFileSize, fp->obj.objsize);	/* (FatFs does not support Valid File Size feature) */
						st_dword(fs->dirbuf + XDIR_ModTime, tm);		/* Update modified time */
						fs->dirbuf[XDIR_ModTime10] = 0;
						st_dword(fs->dirbuf + XDIR_AccTime, 0);
						res = store_xdir(&dj);	/* Restore it to the directory */
						if (res == FR_OK) {
							res = sync_fs(fs);
							fp->flag &= (BYTE)~FA_MODIFIED;
						}
					}
					FREE_NAMBUF();
				}
			} else
#endif
			{
				res = move_window(fs, fp->dir_sect);
				if (res == FR_OK) {
					dir = fp->dir_ptr;
					dir[DIR_Attr] |= AM_ARC;						/* Set archive attribute to indicate that the file has been changed */
					st_clust(fp->obj.fs, dir, fp->obj.sclust);		/* Update file allocation information  */
					st_dword(dir + DIR_FileSize, (DWORD)fp->obj.objsize);	/* Update file size */
					st_dword(dir + DIR_ModTime, tm);				/* Update modified time */
					st_word(dir + DIR_LstAccDate, 0);
					fs->wflag = 1;
					res = sync_fs(fs);					/* Restore it to the directory */
					fp->flag &= (BYTE)~FA_MODIFIED;
				}
			}
		}
	}

	LEAVE_FF(fs, res);
}

#endif /* !FF_FS_READONLY */




/*-----------------------------------------------------------------------*/
/* Close File                                                            */
/*-----------------------------------------------------------------------*/

FRESULT f_close (
	FIL* fp		/* Pointer to the file object to be closed */
)
{
	FRESULT res;
	FATFS *fs;

#if !FF_FS_READONLY
	res = f_sync(fp);					/* Flush cached data */
	if (res == FR_OK)
#endif
	{
		res = validate(&fp->obj, &fs);	/* Lock volume */
		if (res == FR_OK) {
#if FF_FS_LOCK != 0
			res = dec_lock(fp->obj.lockid);		/* Decrement file open counter */
			if (res == FR_OK) fp->obj.fs = 0;	/* Invalidate file object */
#else
			fp->obj.fs = 0;	/* Invalidate file object */
#endif
#if FF_FS_REENTRANT
			unlock_fs(fs, FR_OK);		/* Unlock volume */
#endif
		}
	}
	return res;
}




#if FF_FS_RPATH >= 1
/*-----------------------------------------------------------------------*/
/* Change Current Directory or Current Drive, Get Current Directory      */
/*-----------------------------------------------------------------------*/

FRESULT f_chdrive (
	const TCHAR* path		/* Drive number to set */
)
{
	int vol;


	/* Get logical drive number */
	vol = get_ldnumber(&path);
	if (vol < 0) return FR_INVALID_DRIVE;
	CurrVol = (BYTE)vol;	/* Set it as current volume */

	return FR_OK;
}



FRESULT f_chdir (
	const TCHAR* path	/* Pointer to the directory path */
)
{
#if FF_STR_VOLUME_ID == 2
	UINT i;
#endif
	FRESULT res;
	DIR dj;
	FATFS *fs;
	DEF_NAMBUF


	/* Get logical drive */
	res = mount_volume(&path, &fs, 0);
	if (res == FR_OK) {
		dj.obj.fs = fs;
		INIT_NAMBUF(fs);
		res = follow_path(&dj, path);		/* Follow the path */
		if (res == FR_OK) {					/* Follow completed */
			if (dj.fn[NSFLAG] & NS_NONAME) {	/* Is it the start directory itself? */
				fs->cdir = dj.obj.sclust;
#if FF_FS_EXFAT
				if (fs->fs_type == FS_EXFAT) {
					fs->cdc_scl = dj.obj.c_scl;
					fs->cdc_size = dj.obj.c_size;
					fs->cdc_ofs = dj.obj.c_ofs;
				}
#endif
			} else {
				if (dj.obj.attr & AM_DIR) {	/* It is a sub-directory */
#if FF_FS_EXFAT
					if (fs->fs_type == FS_EXFAT) {
						fs->cdir = ld_dword(fs->dirbuf + XDIR_FstClus);		/* Sub-directory cluster */
						fs->cdc_scl = dj.obj.sclust;						/* Save containing directory information */
						fs->cdc_size = ((DWORD)dj.obj.objsize & 0xFFFFFF00) | dj.obj.stat;
						fs->cdc_ofs = dj.blk_ofs;
					} else
#endif
					{
						fs->cdir = ld_clust(fs, dj.dir);					/* Sub-directory cluster */
					}
				} else {
					res = FR_NO_PATH;		/* Reached but a file */
				}
			}
		}
		FREE_NAMBUF();
		if (res == FR_NO_FILE) res = FR_NO_PATH;
#if FF_STR_VOLUME_ID == 2	/* Also current drive is changed at Unix style volume ID */
		if (res == FR_OK) {
			for (i = FF_VOLUMES - 1; i && fs != FatFs[i]; i--) ;	/* Set current drive */
			CurrVol = (BYTE)i;
		}
#endif
	}

	LEAVE_FF(fs, res);
}


#if FF_FS_RPATH >= 2
FRESULT f_getcwd (
	TCHAR* buff,	/* Pointer to the directory path */
	UINT len		/* Size of buff in unit of TCHAR */
)
{
	FRESULT res;
	DIR dj;
	FATFS *fs;
	UINT i, n;
	DWORD ccl;
	TCHAR *tp = buff;
#if FF_VOLUMES >= 2
	UINT vl;
#if FF_STR_VOLUME_ID
	const char *vp;
#endif
#endif
	FILINFO fno;
	DEF_NAMBUF


	/* Get logical drive */
	buff[0] = 0;	/* Set null string to get current volume */
	res = mount_volume((const TCHAR**)&buff, &fs, 0);	/* Get current volume */
	if (res == FR_OK) {
		dj.obj.fs = fs;
		INIT_NAMBUF(fs);

		/* Follow parent directories and create the path */
		i = len;			/* Bottom of buffer (directory stack base) */
		if (!FF_FS_EXFAT || fs->fs_type != FS_EXFAT) {	/* (Cannot do getcwd on exFAT and returns root path) */
			dj.obj.sclust = fs->cdir;				/* Start to follow upper directory from current directory */
			while ((ccl = dj.obj.sclust) != 0) {	/* Repeat while current directory is a sub-directory */
				res = dir_sdi(&dj, 1 * SZDIRE);	/* Get parent directory */
				if (res != FR_OK) break;
				res = move_window(fs, dj.sect);
				if (res != FR_OK) break;
				dj.obj.sclust = ld_clust(fs, dj.dir);	/* Goto parent directory */
				res = dir_sdi(&dj, 0);
				if (res != FR_OK) break;
				do {							/* Find the entry links to the child directory */
					res = DIR_READ_FILE(&dj);
					if (res != FR_OK) break;
					if (ccl == ld_clust(fs, dj.dir)) break;	/* Found the entry */
					res = dir_next(&dj, 0);
				} while (res == FR_OK);
				if (res == FR_NO_FILE) res = FR_INT_ERR;/* It cannot be 'not found'. */
				if (res != FR_OK) break;
				get_fileinfo(&dj, &fno);		/* Get the directory name and push it to the buffer */
				for (n = 0; fno.fname[n]; n++) ;	/* Name length */
				if (i < n + 1) {	/* Insufficient space to store the path name? */
					res = FR_NOT_ENOUGH_CORE; break;
				}
				while (n) buff[--i] = fno.fname[--n];	/* Stack the name */
				buff[--i] = '/';
			}
		}
		if (res == FR_OK) {
			if (i == len) buff[--i] = '/';	/* Is it the root-directory? */
#if FF_VOLUMES >= 2			/* Put drive prefix */
			vl = 0;
#if FF_STR_VOLUME_ID >= 1	/* String volume ID */
			for (n = 0, vp = (const char*)VolumeStr[CurrVol]; vp[n]; n++) ;
			if (i >= n + 2) {
				if (FF_STR_VOLUME_ID == 2) *tp++ = (TCHAR)'/';
				for (vl = 0; vl < n; *tp++ = (TCHAR)vp[vl], vl++) ;
				if (FF_STR_VOLUME_ID == 1) *tp++ = (TCHAR)':';
				vl++;
			}
#else						/* Numeric volume ID */
			if (i >= 3) {
				*tp++ = (TCHAR)'0' + CurrVol;
				*tp++ = (TCHAR)':';
				vl = 2;
			}
#endif
			if (vl == 0) res = FR_NOT_ENOUGH_CORE;
#endif
			/* Add current directory path */
			if (res == FR_OK) {
				do *tp++ = buff[i++]; while (i < len);	/* Copy stacked path string */
			}
		}
		FREE_NAMBUF();
	}

	*tp = 0;
	LEAVE_FF(fs, res);
}

#endif /* FF_FS_RPATH >= 2 */
#endif /* FF_FS_RPATH >= 1 */



#if FF_FS_MINIMIZE <= 2
/*-----------------------------------------------------------------------*/
/* Seek File Read/Write Pointer                                          */
/*-----------------------------------------------------------------------*/

FRESULT f_lseek (
	FIL* fp,		/* Pointer to the file object */
	FSIZE_t ofs		/* File pointer from top of file */
)
{
	FRESULT res;
	FATFS *fs;
	DWORD clst, bcs;
	LBA_t nsect;
	FSIZE_t ifptr;
#if FF_USE_FASTSEEK
	DWORD cl, pcl, ncl, tcl, tlen, ulen, *tbl;
	LBA_t dsc;
#endif

	res = validate(&fp->obj, &fs);		/* Check validity of the file object */
	if (res == FR_OK) res = (FRESULT)fp->err;
#if FF_FS_EXFAT && !FF_FS_READONLY
	if (res == FR_OK && fs->fs_type == FS_EXFAT) {
		res = fill_last_frag(&fp->obj, fp->clust, 0xFFFFFFFF);	/* Fill last fragment on the FAT if needed */
	}
#endif
	if (res != FR_OK) LEAVE_FF(fs, res);

#if FF_USE_FASTSEEK
	if (fp->cltbl) {	/* Fast seek */
		if (ofs == CREATE_LINKMAP) {	/* Create CLMT */
			tbl = fp->cltbl;
			tlen = *tbl++; ulen = 2;	/* Given table size and required table size */
			cl = fp->obj.sclust;		/* Origin of the chain */
			if (cl != 0) {
				do {
					/* Get a fragment */
					tcl = cl; ncl = 0; ulen += 2;	/* Top, length and used items */
					do {
						pcl = cl; ncl++;
						cl = get_fat(&fp->obj, cl);
						if (cl <= 1) ABORT(fs, FR_INT_ERR);
						if (cl == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
					} while (cl == pcl + 1);
					if (ulen <= tlen) {		/* Store the length and top of the fragment */
						*tbl++ = ncl; *tbl++ = tcl;
					}
				} while (cl < fs->n_fatent);	/* Repeat until end of chain */
			}
			*fp->cltbl = ulen;	/* Number of items used */
			if (ulen <= tlen) {
				*tbl = 0;		/* Terminate table */
			} else {
				res = FR_NOT_ENOUGH_CORE;	/* Given table size is smaller than required */
			}
		} else {						/* Fast seek */
			if (ofs > fp->obj.objsize) ofs = fp->obj.objsize;	/* Clip offset at the file size */
			fp->fptr = ofs;				/* Set file pointer */
			if (ofs > 0) {
				fp->clust = clmt_clust(fp, ofs - 1);
				dsc = clst2sect(fs, fp->clust);
				if (dsc == 0) ABORT(fs, FR_INT_ERR);
				dsc += (DWORD)((ofs - 1) / SS(fs)) & (fs->csize - 1);
				if (fp->fptr % SS(fs) && dsc != fp->sect) {	/* Refill sector cache if needed */
#if !FF_FS_TINY
#if !FF_FS_READONLY
					if (fp->flag & FA_DIRTY) {		/* Write-back dirty sector cache */
						if (disk_write(fs->pdrv, fp->buf, fp->sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
						fp->flag &= (BYTE)~FA_DIRTY;
					}
#endif
					if (disk_read(fs->pdrv, fp->buf, dsc, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);	/* Load current sector */
#endif
					fp->sect = dsc;
				}
			}
		}
	} else
#endif

	/* Normal Seek */
	{
#if FF_FS_EXFAT
		if (fs->fs_type != FS_EXFAT && ofs >= 0x100000000) ofs = 0xFFFFFFFF;	/* Clip at 4 GiB - 1 if at FATxx */
#endif
		if (ofs > fp->obj.objsize && (FF_FS_READONLY || !(fp->flag & FA_WRITE))) {	/* In read-only mode, clip offset with the file size */
			ofs = fp->obj.objsize;
		}
		ifptr = fp->fptr;
		fp->fptr = nsect = 0;
		if (ofs > 0) {
			bcs = (DWORD)fs->csize * SS(fs);	/* Cluster size (byte) */
			if (ifptr > 0 &&
				(ofs - 1) / bcs >= (ifptr - 1) / bcs) {	/* When seek to same or following cluster, */
				fp->fptr = (ifptr - 1) & ~(FSIZE_t)(bcs - 1);	/* start from the current cluster */
				ofs -= fp->fptr;
				clst = fp->clust;
			} else {									/* When seek to back cluster, */
				clst = fp->obj.sclust;					/* start from the first cluster */
#if !FF_FS_READONLY
				if (clst == 0) {						/* If no cluster chain, create a new chain */
					clst = create_chain(&fp->obj, 0);
					if (clst == 1) ABORT(fs, FR_INT_ERR);
					if (clst == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
					fp->obj.sclust = clst;
				}
#endif
				fp->clust = clst;
			}
			if (clst != 0) {
				while (ofs > bcs) {						/* Cluster following loop */
					ofs -= bcs; fp->fptr += bcs;
#if !FF_FS_READONLY
					if (fp->flag & FA_WRITE) {			/* Check if in write mode or not */
						if (FF_FS_EXFAT && fp->fptr > fp->obj.objsize) {	/* No FAT chain object needs correct objsize to generate FAT value */
							fp->obj.objsize = fp->fptr;
							fp->flag |= FA_MODIFIED;
						}
						clst = create_chain(&fp->obj, clst);	/* Follow chain with forceed stretch */
						if (clst == 0) {				/* Clip file size in case of disk full */
							ofs = 0; break;
						}
					} else
#endif
					{
						clst = get_fat(&fp->obj, clst);	/* Follow cluster chain if not in write mode */
					}
					if (clst == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
					if (clst <= 1 || clst >= fs->n_fatent) ABORT(fs, FR_INT_ERR);
					fp->clust = clst;
				}
				fp->fptr += ofs;
				if (ofs % SS(fs)) {
					nsect = clst2sect(fs, clst);	/* Current sector */
					if (nsect == 0) ABORT(fs, FR_INT_ERR);
					nsect += (DWORD)(ofs / SS(fs));
				}
			}
		}
		if (!FF_FS_READONLY && fp->fptr > fp->obj.objsize) {	/* Set file change flag if the file size is extended */
			fp->obj.objsize = fp->fptr;
			fp->flag |= FA_MODIFIED;
		}
		if (fp->fptr % SS(fs) && nsect != fp->sect) {	/* Fill sector cache if needed */
#if !FF_FS_TINY
#if !FF_FS_READONLY
			if (fp->flag & FA_DIRTY) {			/* Write-back dirty sector cache */
				if (disk_write(fs->pdrv, fp->buf, fp->sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
				fp->flag &= (BYTE)~FA_DIRTY;
			}
#endif
			if (disk_read(fs->pdrv, fp->buf, nsect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);	/* Fill sector cache */
#endif
			fp->sect = nsect;
		}
	}

	LEAVE_FF(fs, res);
}



#if FF_FS_MINIMIZE <= 1
/*-----------------------------------------------------------------------*/
/* Create a Directory Object                                             */
/*-----------------------------------------------------------------------*/

FRESULT f_opendir (
	DIR* dp,			/* Pointer to directory object to create */
	const TCHAR* path	/* Pointer to the directory path */
)
{
	FRESULT res;
	FATFS *fs;
	DEF_NAMBUF


	if (!dp) return FR_INVALID_OBJECT;

	/* Get logical drive */
	res = mount_volume(&path, &fs, 0);
	if (res == FR_OK) {
		dp->obj.fs = fs;
		INIT_NAMBUF(fs);
		res = follow_path(dp, path);			/* Follow the path to the directory */
		if (res == FR_OK) {						/* Follow completed */
			if (!(dp->fn[NSFLAG] & NS_NONAME)) {	/* It is not the origin directory itself */
				if (dp->obj.attr & AM_DIR) {		/* This object is a sub-directory */
#if FF_FS_EXFAT
					if (fs->fs_type == FS_EXFAT) {
						dp->obj.c_scl = dp->obj.sclust;							/* Get containing directory inforamation */
						dp->obj.c_size = ((DWORD)dp->obj.objsize & 0xFFFFFF00) | dp->obj.stat;
						dp->obj.c_ofs = dp->blk_ofs;
						init_alloc_info(fs, &dp->obj);	/* Get object allocation info */
					} else
#endif
					{
						dp->obj.sclust = ld_clust(fs, dp->dir);	/* Get object allocation info */
					}
				} else {						/* This object is a file */
					res = FR_NO_PATH;
				}
			}
			if (res == FR_OK) {
				dp->obj.id = fs->id;
				res = dir_sdi(dp, 0);			/* Rewind directory */
#if FF_FS_LOCK != 0
				if (res == FR_OK) {
					if (dp->obj.sclust != 0) {
						dp->obj.lockid = inc_lock(dp, 0);	/* Lock the sub directory */
						if (!dp->obj.lockid) res = FR_TOO_MANY_OPEN_FILES;
					} else {
						dp->obj.lockid = 0;	/* Root directory need not to be locked */
					}
				}
#endif
			}
		}
		FREE_NAMBUF();
		if (res == FR_NO_FILE) res = FR_NO_PATH;
	}
	if (res != FR_OK) dp->obj.fs = 0;		/* Invalidate the directory object if function faild */

	LEAVE_FF(fs, res);
}




/*-----------------------------------------------------------------------*/
/* Close Directory                                                       */
/*-----------------------------------------------------------------------*/

FRESULT f_closedir (
	DIR *dp		/* Pointer to the directory object to be closed */
)
{
	FRESULT res;
	FATFS *fs;


	res = validate(&dp->obj, &fs);	/* Check validity of the file object */
	if (res == FR_OK) {
#if FF_FS_LOCK != 0
		if (dp->obj.lockid) res = dec_lock(dp->obj.lockid);	/* Decrement sub-directory open counter */
		if (res == FR_OK) dp->obj.fs = 0;	/* Invalidate directory object */
#else
		dp->obj.fs = 0;	/* Invalidate directory object */
#endif
#if FF_FS_REENTRANT
		unlock_fs(fs, FR_OK);		/* Unlock volume */
#endif
	}
	return res;
}




/*-----------------------------------------------------------------------*/
/* Read Directory Entries in Sequence                                    */
/*-----------------------------------------------------------------------*/

FRESULT f_readdir (
	DIR* dp,			/* Pointer to the open directory object */
	FILINFO* fno		/* Pointer to file information to return */
)
{
	FRESULT res;
	FATFS *fs;
	DEF_NAMBUF


	res = validate(&dp->obj, &fs);	/* Check validity of the directory object */
	if (res == FR_OK) {
		if (!fno) {
			res = dir_sdi(dp, 0);			/* Rewind the directory object */
		} else {
			INIT_NAMBUF(fs);
			res = DIR_READ_FILE(dp);		/* Read an item */
			if (res == FR_NO_FILE) res = FR_OK;	/* Ignore end of directory */
			if (res == FR_OK) {				/* A valid entry is found */
				get_fileinfo(dp, fno);		/* Get the object information */
				res = dir_next(dp, 0);		/* Increment index for next */
				if (res == FR_NO_FILE) res = FR_OK;	/* Ignore end of directory now */
			}
			FREE_NAMBUF();
		}
	}
	LEAVE_FF(fs, res);
}



#if FF_USE_FIND
/*-----------------------------------------------------------------------*/
/* Find Next File                                                        */
/*-----------------------------------------------------------------------*/

FRESULT f_findnext (
	DIR* dp,		/* Pointer to the open directory object */
	FILINFO* fno	/* Pointer to the file information structure */
)
{
	FRESULT res;


	for (;;) {
		res = f_readdir(dp, fno);		/* Get a directory item */
		if (res != FR_OK || !fno || !fno->fname[0]) break;	/* Terminate if any error or end of directory */
		if (pattern_matching(dp->pat, fno->fname, 0, 0)) break;		/* Test for the file name */
#if FF_USE_LFN && FF_USE_FIND == 2
		if (pattern_matching(dp->pat, fno->altname, 0, 0)) break;	/* Test for alternative name if exist */
#endif
	}
	return res;
}



/*-----------------------------------------------------------------------*/
/* Find First File                                                       */
/*-----------------------------------------------------------------------*/

FRESULT f_findfirst (
	DIR* dp,				/* Pointer to the blank directory object */
	FILINFO* fno,			/* Pointer to the file information structure */
	const TCHAR* path,		/* Pointer to the directory to open */
	const TCHAR* pattern	/* Pointer to the matching pattern */
)
{
	FRESULT res;


	dp->pat = pattern;		/* Save pointer to pattern string */
	res = f_opendir(dp, path);		/* Open the target directory */
	if (res == FR_OK) {
		res = f_findnext(dp, fno);	/* Find the first item */
	}
	return res;
}

#endif	/* FF_USE_FIND */



#if FF_FS_MINIMIZE == 0
/*-----------------------------------------------------------------------*/
/* Get File Status                                                       */
/*-----------------------------------------------------------------------*/

FRESULT f_stat (
	const TCHAR* path,	/* Pointer to the file path */
	FILINFO* fno		/* Pointer to file information to return */
)
{
	FRESULT res;
	DIR dj;
	DEF_NAMBUF


	/* Get logical drive */
	res = mount_volume(&path, &dj.obj.fs, 0);
	if (res == FR_OK) {
		INIT_NAMBUF(dj.obj.fs);
		res = follow_path(&dj, path);	/* Follow the file path */
		if (res == FR_OK) {				/* Follow completed */
			if (dj.fn[NSFLAG] & NS_NONAME) {	/* It is origin directory */
				res = FR_INVALID_NAME;
			} else {							/* Found an object */
				if (fno) get_fileinfo(&dj, fno);
			}
		}
		FREE_NAMBUF();
	}

	LEAVE_FF(dj.obj.fs, res);
}



#if !FF_FS_READONLY
/*-----------------------------------------------------------------------*/
/* Get Number of Free Clusters                                           */
/*-----------------------------------------------------------------------*/

FRESULT f_getfree (
	const TCHAR* path,	/* Logical drive number */
	DWORD* nclst,		/* Pointer to a variable to return number of free clusters */
	FATFS** fatfs		/* Pointer to return pointer to corresponding filesystem object */
)
{
	FRESULT res;
	FATFS *fs;
	DWORD nfree, clst, stat;
	LBA_t sect;
	UINT i;
	FFOBJID obj;


	/* Get logical drive */
	res = mount_volume(&path, &fs, 0);
	if (res == FR_OK) {
		*fatfs = fs;				/* Return ptr to the fs object */
		/* If free_clst is valid, return it without full FAT scan */
		if (fs->free_clst <= fs->n_fatent - 2) {
			*nclst = fs->free_clst;
		} else {
			/* Scan FAT to obtain number of free clusters */
			nfree = 0;
			if (fs->fs_type == FS_FAT12) {	/* FAT12: Scan bit field FAT entries */
				clst = 2; obj.fs = fs;
				do {
					stat = get_fat(&obj, clst);
					if (stat == 0xFFFFFFFF) { res = FR_DISK_ERR; break; }
					if (stat == 1) { res = FR_INT_ERR; break; }
					if (stat == 0) nfree++;
				} while (++clst < fs->n_fatent);
			} else {
#if FF_FS_EXFAT
				if (fs->fs_type == FS_EXFAT) {	/* exFAT: Scan allocation bitmap */
					BYTE bm;
					UINT b;

					clst = fs->n_fatent - 2;	/* Number of clusters */
					sect = fs->bitbase;			/* Bitmap sector */
					i = 0;						/* Offset in the sector */
					do {	/* Counts numbuer of bits with zero in the bitmap */
						if (i == 0) {
							res = move_window(fs, sect++);
							if (res != FR_OK) break;
						}
						for (b = 8, bm = fs->win[i]; b && clst; b--, clst--) {
							if (!(bm & 1)) nfree++;
							bm >>= 1;
						}
						i = (i + 1) % SS(fs);
					} while (clst);
				} else
#endif
				{	/* FAT16/32: Scan WORD/DWORD FAT entries */
					clst = fs->n_fatent;	/* Number of entries */
					sect = fs->fatbase;		/* Top of the FAT */
					i = 0;					/* Offset in the sector */
					do {	/* Counts numbuer of entries with zero in the FAT */
						if (i == 0) {
							res = move_window(fs, sect++);
							if (res != FR_OK) break;
						}
						if (fs->fs_type == FS_FAT16) {
							if (ld_word(fs->win + i) == 0) nfree++;
							i += 2;
						} else {
							if ((ld_dword(fs->win + i) & 0x0FFFFFFF) == 0) nfree++;
							i += 4;
						}
						i %= SS(fs);
					} while (--clst);
				}
			}
			*nclst = nfree;			/* Return the free clusters */
			fs->free_clst = nfree;	/* Now free_clst is valid */
			fs->fsi_flag |= 1;		/* FAT32: FSInfo is to be updated */
		}
	}

	LEAVE_FF(fs, res);
}




/*-----------------------------------------------------------------------*/
/* Truncate File                                                         */
/*-----------------------------------------------------------------------*/

FRESULT f_truncate (
	FIL* fp		/* Pointer to the file object */
)
{
	FRESULT res;
	FATFS *fs;
	DWORD ncl;


	res = validate(&fp->obj, &fs);	/* Check validity of the file object */
	if (res != FR_OK || (res = (FRESULT)fp->err) != FR_OK) LEAVE_FF(fs, res);
	if (!(fp->flag & FA_WRITE)) LEAVE_FF(fs, FR_DENIED);	/* Check access mode */

	if (fp->fptr < fp->obj.objsize) {	/* Process when fptr is not on the eof */
		if (fp->fptr == 0) {	/* When set file size to zero, remove entire cluster chain */
			res = remove_chain(&fp->obj, fp->obj.sclust, 0);
			fp->obj.sclust = 0;
		} else {				/* When truncate a part of the file, remove remaining clusters */
			ncl = get_fat(&fp->obj, fp->clust);
			res = FR_OK;
			if (ncl == 0xFFFFFFFF) res = FR_DISK_ERR;
			if (ncl == 1) res = FR_INT_ERR;
			if (res == FR_OK && ncl < fs->n_fatent) {
				res = remove_chain(&fp->obj, ncl, fp->clust);
			}
		}
		fp->obj.objsize = fp->fptr;	/* Set file size to current read/write point */
		fp->flag |= FA_MODIFIED;
#if !FF_FS_TINY
		if (res == FR_OK && (fp->flag & FA_DIRTY)) {
			if (disk_write(fs->pdrv, fp->buf, fp->sect, 1) != RES_OK) {
				res = FR_DISK_ERR;
			} else {
				fp->flag &= (BYTE)~FA_DIRTY;
			}
		}
#endif
		if (res != FR_OK) ABORT(fs, res);
	}

	LEAVE_FF(fs, res);
}




/*-----------------------------------------------------------------------*/
/* Delete a File/Directory                                               */
/*-----------------------------------------------------------------------*/

FRESULT f_unlink (
	const TCHAR* path		/* Pointer to the file or directory path */
)
{
	FRESULT res;
	DIR dj, sdj;
	DWORD dclst = 0;
	FATFS *fs;
#if FF_FS_EXFAT
	FFOBJID obj;
#endif
	DEF_NAMBUF


	/* Get logical drive */
	res = mount_volume(&path, &fs, FA_WRITE);
	if (res == FR_OK) {
		dj.obj.fs = fs;
		INIT_NAMBUF(fs);
		res = follow_path(&dj, path);		/* Follow the file path */
		if (FF_FS_RPATH && res == FR_OK && (dj.fn[NSFLAG] & NS_DOT)) {
			res = FR_INVALID_NAME;			/* Cannot remove dot entry */
		}
#if FF_FS_LOCK != 0
		if (res == FR_OK) res = chk_lock(&dj, 2);	/* Check if it is an open object */
#endif
		if (res == FR_OK) {					/* The object is accessible */
			if (dj.fn[NSFLAG] & NS_NONAME) {
				res = FR_INVALID_NAME;		/* Cannot remove the origin directory */
			} else {
				if (dj.obj.attr & AM_RDO) {
					res = FR_DENIED;		/* Cannot remove R/O object */
				}
			}
			if (res == FR_OK) {
#if FF_FS_EXFAT
				obj.fs = fs;
				if (fs->fs_type == FS_EXFAT) {
					init_alloc_info(fs, &obj);
					dclst = obj.sclust;
				} else
#endif
				{
					dclst = ld_clust(fs, dj.dir);
				}
				if (dj.obj.attr & AM_DIR) {			/* Is it a sub-directory? */
#if FF_FS_RPATH != 0
					if (dclst == fs->cdir) {		 	/* Is it the current directory? */
						res = FR_DENIED;
					} else
#endif
					{
						sdj.obj.fs = fs;				/* Open the sub-directory */
						sdj.obj.sclust = dclst;
#if FF_FS_EXFAT
						if (fs->fs_type == FS_EXFAT) {
							sdj.obj.objsize = obj.objsize;
							sdj.obj.stat = obj.stat;
						}
#endif
						res = dir_sdi(&sdj, 0);
						if (res == FR_OK) {
							res = DIR_READ_FILE(&sdj);			/* Test if the directory is empty */
							if (res == FR_OK) res = FR_DENIED;	/* Not empty? */
							if (res == FR_NO_FILE) res = FR_OK;	/* Empty? */
						}
					}
				}
			}
			if (res == FR_OK) {
				res = dir_remove(&dj);			/* Remove the directory entry */
				if (res == FR_OK && dclst != 0) {	/* Remove the cluster chain if exist */
#if FF_FS_EXFAT
					res = remove_chain(&obj, dclst, 0);
#else
					res = remove_chain(&dj.obj, dclst, 0);
#endif
				}
				if (res == FR_OK) res = sync_fs(fs);
			}
		}
		FREE_NAMBUF();
	}

	LEAVE_FF(fs, res);
}




/*-----------------------------------------------------------------------*/
/* Create a Directory                                                    */
/*-----------------------------------------------------------------------*/

FRESULT f_mkdir (
	const TCHAR* path		/* Pointer to the directory path */
)
{
	FRESULT res;
	DIR dj;
	FFOBJID sobj;
	FATFS *fs;
	DWORD dcl, pcl, tm;
	DEF_NAMBUF


	res = mount_volume(&path, &fs, FA_WRITE);	/* Get logical drive */
	if (res == FR_OK) {
		dj.obj.fs = fs;
		INIT_NAMBUF(fs);
		res = follow_path(&dj, path);			/* Follow the file path */
		if (res == FR_OK) res = FR_EXIST;		/* Name collision? */
		if (FF_FS_RPATH && res == FR_NO_FILE && (dj.fn[NSFLAG] & NS_DOT)) {	/* Invalid name? */
			res = FR_INVALID_NAME;
		}
		if (res == FR_NO_FILE) {				/* It is clear to create a new directory */
			sobj.fs = fs;						/* New object id to create a new chain */
			dcl = create_chain(&sobj, 0);		/* Allocate a cluster for the new directory */
			res = FR_OK;
			if (dcl == 0) res = FR_DENIED;		/* No space to allocate a new cluster? */
			if (dcl == 1) res = FR_INT_ERR;		/* Any insanity? */
			if (dcl == 0xFFFFFFFF) res = FR_DISK_ERR;	/* Disk error? */
			tm = GET_FATTIME();
			if (res == FR_OK) {
				res = dir_clear(fs, dcl);		/* Clean up the new table */
				if (res == FR_OK) {
					if (!FF_FS_EXFAT || fs->fs_type != FS_EXFAT) {	/* Create dot entries (FAT only) */
						mem_set(fs->win + DIR_Name, ' ', 11);	/* Create "." entry */
						fs->win[DIR_Name] = '.';
						fs->win[DIR_Attr] = AM_DIR;
						st_dword(fs->win + DIR_ModTime, tm);
						st_clust(fs, fs->win, dcl);
						mem_cpy(fs->win + SZDIRE, fs->win, SZDIRE); /* Create ".." entry */
						fs->win[SZDIRE + 1] = '.'; pcl = dj.obj.sclust;
						st_clust(fs, fs->win + SZDIRE, pcl);
						fs->wflag = 1;
					}
					res = dir_register(&dj);	/* Register the object to the parent directoy */
				}
			}
			if (res == FR_OK) {
#if FF_FS_EXFAT
				if (fs->fs_type == FS_EXFAT) {	/* Initialize directory entry block */
					st_dword(fs->dirbuf + XDIR_ModTime, tm);	/* Created time */
					st_dword(fs->dirbuf + XDIR_FstClus, dcl);	/* Table start cluster */
					st_dword(fs->dirbuf + XDIR_FileSize, (DWORD)fs->csize * SS(fs));	/* Directory size needs to be valid */
					st_dword(fs->dirbuf + XDIR_ValidFileSize, (DWORD)fs->csize * SS(fs));
					fs->dirbuf[XDIR_GenFlags] = 3;				/* Initialize the object flag */
					fs->dirbuf[XDIR_Attr] = AM_DIR;				/* Attribute */
					res = store_xdir(&dj);
				} else
#endif
				{
					st_dword(dj.dir + DIR_ModTime, tm);	/* Created time */
					st_clust(fs, dj.dir, dcl);			/* Table start cluster */
					dj.dir[DIR_Attr] = AM_DIR;			/* Attribute */
					fs->wflag = 1;
				}
				if (res == FR_OK) {
					res = sync_fs(fs);
				}
			} else {
				remove_chain(&sobj, dcl, 0);		/* Could not register, remove the allocated cluster */
			}
		}
		FREE_NAMBUF();
	}

	LEAVE_FF(fs, res);
}




/*-----------------------------------------------------------------------*/
/* Rename a File/Directory                                               */
/*-----------------------------------------------------------------------*/

FRESULT f_rename (
	const TCHAR* path_old,	/* Pointer to the object name to be renamed */
	const TCHAR* path_new	/* Pointer to the new name */
)
{
	FRESULT res;
	DIR djo, djn;
	FATFS *fs;
	BYTE buf[FF_FS_EXFAT ? SZDIRE * 2 : SZDIRE], *dir;
	LBA_t sect;
	DEF_NAMBUF


	get_ldnumber(&path_new);						/* Snip the drive number of new name off */
	res = mount_volume(&path_old, &fs, FA_WRITE);	/* Get logical drive of the old object */
	if (res == FR_OK) {
		djo.obj.fs = fs;
		INIT_NAMBUF(fs);
		res = follow_path(&djo, path_old);		/* Check old object */
		if (res == FR_OK && (djo.fn[NSFLAG] & (NS_DOT | NS_NONAME))) res = FR_INVALID_NAME;	/* Check validity of name */
#if FF_FS_LOCK != 0
		if (res == FR_OK) {
			res = chk_lock(&djo, 2);
		}
#endif
		if (res == FR_OK) {						/* Object to be renamed is found */
#if FF_FS_EXFAT
			if (fs->fs_type == FS_EXFAT) {	/* At exFAT volume */
				BYTE nf, nn;
				WORD nh;

				mem_cpy(buf, fs->dirbuf, SZDIRE * 2);	/* Save 85+C0 entry of old object */
				mem_cpy(&djn, &djo, sizeof djo);
				res = follow_path(&djn, path_new);		/* Make sure if new object name is not in use */
				if (res == FR_OK) {						/* Is new name already in use by any other object? */
					res = (djn.obj.sclust == djo.obj.sclust && djn.dptr == djo.dptr) ? FR_NO_FILE : FR_EXIST;
				}
				if (res == FR_NO_FILE) { 				/* It is a valid path and no name collision */
					res = dir_register(&djn);			/* Register the new entry */
					if (res == FR_OK) {
						nf = fs->dirbuf[XDIR_NumSec]; nn = fs->dirbuf[XDIR_NumName];
						nh = ld_word(fs->dirbuf + XDIR_NameHash);
						mem_cpy(fs->dirbuf, buf, SZDIRE * 2);	/* Restore 85+C0 entry */
						fs->dirbuf[XDIR_NumSec] = nf; fs->dirbuf[XDIR_NumName] = nn;
						st_word(fs->dirbuf + XDIR_NameHash, nh);
						if (!(fs->dirbuf[XDIR_Attr] & AM_DIR)) fs->dirbuf[XDIR_Attr] |= AM_ARC;	/* Set archive attribute if it is a file */
/* Start of critical section where an interruption can cause a cross-link */
						res = store_xdir(&djn);
					}
				}
			} else
#endif
			{	/* At FAT/FAT32 volume */
				mem_cpy(buf, djo.dir, SZDIRE);			/* Save directory entry of the object */
				mem_cpy(&djn, &djo, sizeof (DIR));		/* Duplicate the directory object */
				res = follow_path(&djn, path_new);		/* Make sure if new object name is not in use */
				if (res == FR_OK) {						/* Is new name already in use by any other object? */
					res = (djn.obj.sclust == djo.obj.sclust && djn.dptr == djo.dptr) ? FR_NO_FILE : FR_EXIST;
				}
				if (res == FR_NO_FILE) { 				/* It is a valid path and no name collision */
					res = dir_register(&djn);			/* Register the new entry */
					if (res == FR_OK) {
						dir = djn.dir;					/* Copy directory entry of the object except name */
						mem_cpy(dir + 13, buf + 13, SZDIRE - 13);
						dir[DIR_Attr] = buf[DIR_Attr];
						if (!(dir[DIR_Attr] & AM_DIR)) dir[DIR_Attr] |= AM_ARC;	/* Set archive attribute if it is a file */
						fs->wflag = 1;
						if ((dir[DIR_Attr] & AM_DIR) && djo.obj.sclust != djn.obj.sclust) {	/* Update .. entry in the sub-directory if needed */
							sect = clst2sect(fs, ld_clust(fs, dir));
							if (sect == 0) {
								res = FR_INT_ERR;
							} else {
/* Start of critical section where an interruption can cause a cross-link */
								res = move_window(fs, sect);
								dir = fs->win + SZDIRE * 1;	/* Ptr to .. entry */
								if (res == FR_OK && dir[1] == '.') {
									st_clust(fs, dir, djn.obj.sclust);
									fs->wflag = 1;
								}
							}
						}
					}
				}
			}
			if (res == FR_OK) {
				res = dir_remove(&djo);		/* Remove old entry */
				if (res == FR_OK) {
					res = sync_fs(fs);
				}
			}
/* End of the critical section */
		}
		FREE_NAMBUF();
	}

	LEAVE_FF(fs, res);
}

#endif /* !FF_FS_READONLY */
#endif /* FF_FS_MINIMIZE == 0 */
#endif /* FF_FS_MINIMIZE <= 1 */
#endif /* FF_FS_MINIMIZE <= 2 */



#if FF_USE_CHMOD && !FF_FS_READONLY
/*-----------------------------------------------------------------------*/
/* Change Attribute                                                      */
/*-----------------------------------------------------------------------*/

FRESULT f_chmod (
	const TCHAR* path,	/* Pointer to the file path */
	BYTE attr,			/* Attribute bits */
	BYTE mask			/* Attribute mask to change */
)
{
	FRESULT res;
	DIR dj;
	FATFS *fs;
	DEF_NAMBUF


	res = mount_volume(&path, &fs, FA_WRITE);	/* Get logical drive */
	if (res == FR_OK) {
		dj.obj.fs = fs;
		INIT_NAMBUF(fs);
		res = follow_path(&dj, path);	/* Follow the file path */
		if (res == FR_OK && (dj.fn[NSFLAG] & (NS_DOT | NS_NONAME))) res = FR_INVALID_NAME;	/* Check object validity */
		if (res == FR_OK) {
			mask &= AM_RDO|AM_HID|AM_SYS|AM_ARC;	/* Valid attribute mask */
#if FF_FS_EXFAT
			if (fs->fs_type == FS_EXFAT) {
				fs->dirbuf[XDIR_Attr] = (attr & mask) | (fs->dirbuf[XDIR_Attr] & (BYTE)~mask);	/* Apply attribute change */
				res = store_xdir(&dj);
			} else
#endif
			{
				dj.dir[DIR_Attr] = (attr & mask) | (dj.dir[DIR_Attr] & (BYTE)~mask);	/* Apply attribute change */
				fs->wflag = 1;
			}
			if (res == FR_OK) {
				res = sync_fs(fs);
			}
		}
		FREE_NAMBUF();
	}

	LEAVE_FF(fs, res);
}




/*-----------------------------------------------------------------------*/
/* Change Timestamp                                                      */
/*-----------------------------------------------------------------------*/

FRESULT f_utime (
	const TCHAR* path,	/* Pointer to the file/directory name */
	const FILINFO* fno	/* Pointer to the timestamp to be set */
)
{
	FRESULT res;
	DIR dj;
	FATFS *fs;
	DEF_NAMBUF


	res = mount_volume(&path, &fs, FA_WRITE);	/* Get logical drive */
	if (res == FR_OK) {
		dj.obj.fs = fs;
		INIT_NAMBUF(fs);
		res = follow_path(&dj, path);	/* Follow the file path */
		if (res == FR_OK && (dj.fn[NSFLAG] & (NS_DOT | NS_NONAME))) res = FR_INVALID_NAME;	/* Check object validity */
		if (res == FR_OK) {
#if FF_FS_EXFAT
			if (fs->fs_type == FS_EXFAT) {
				st_dword(fs->dirbuf + XDIR_ModTime, (DWORD)fno->fdate << 16 | fno->ftime);
				res = store_xdir(&dj);
			} else
#endif
			{
				st_dword(dj.dir + DIR_ModTime, (DWORD)fno->fdate << 16 | fno->ftime);
				fs->wflag = 1;
			}
			if (res == FR_OK) {
				res = sync_fs(fs);
			}
		}
		FREE_NAMBUF();
	}

	LEAVE_FF(fs, res);
}

#endif	/* FF_USE_CHMOD && !FF_FS_READONLY */



#if FF_USE_LABEL
/*-----------------------------------------------------------------------*/
/* Get Volume Label                                                      */
/*-----------------------------------------------------------------------*/

FRESULT f_getlabel (
	const TCHAR* path,	/* Logical drive number */
	TCHAR* label,		/* Buffer to store the volume label */
	DWORD* vsn			/* Variable to store the volume serial number */
)
{
	FRESULT res;
	DIR dj;
	FATFS *fs;
	UINT si, di;
	WCHAR wc;

	/* Get logical drive */
	res = mount_volume(&path, &fs, 0);

	/* Get volume label */
	if (res == FR_OK && label) {
		dj.obj.fs = fs; dj.obj.sclust = 0;	/* Open root directory */
		res = dir_sdi(&dj, 0);
		if (res == FR_OK) {
		 	res = DIR_READ_LABEL(&dj);		/* Find a volume label entry */
		 	if (res == FR_OK) {
#if FF_FS_EXFAT
				if (fs->fs_type == FS_EXFAT) {
					WCHAR hs;

					for (si = di = hs = 0; si < dj.dir[XDIR_NumLabel]; si++) {	/* Extract volume label from 83 entry */
						wc = ld_word(dj.dir + XDIR_Label + si * 2);
						if (hs == 0 && IsSurrogate(wc)) {	/* Is the code a surrogate? */
							hs = wc; continue;
						}
						wc = put_utf((DWORD)hs << 16 | wc, &label[di], 4);
						if (wc == 0) { di = 0; break; }
						di += wc;
						hs = 0;
					}
					if (hs != 0) di = 0;	/* Broken surrogate pair? */
					label[di] = 0;
				} else
#endif
				{
					si = di = 0;		/* Extract volume label from AM_VOL entry */
					while (si < 11) {
						wc = dj.dir[si++];
#if FF_USE_LFN && FF_LFN_UNICODE >= 1 	/* Unicode output */
						if (dbc_1st((BYTE)wc) && si < 11) wc = wc << 8 | dj.dir[si++];	/* Is it a DBC? */
						wc = ff_oem2uni(wc, CODEPAGE);					/* Convert it into Unicode */
						if (wc != 0) wc = put_utf(wc, &label[di], 4);	/* Put it in Unicode */
						if (wc == 0) { di = 0; break; }
						di += wc;
#else									/* ANSI/OEM output */
						label[di++] = (TCHAR)wc;
#endif
					}
					do {				/* Truncate trailing spaces */
						label[di] = 0;
						if (di == 0) break;
					} while (label[--di] == ' ');
				}
			}
		}
		if (res == FR_NO_FILE) {	/* No label entry and return nul string */
			label[0] = 0;
			res = FR_OK;
		}
	}

	/* Get volume serial number */
	if (res == FR_OK && vsn) {
		res = move_window(fs, fs->volbase);
		if (res == FR_OK) {
			switch (fs->fs_type) {
			case FS_EXFAT:
				di = BPB_VolIDEx; break;

			case FS_FAT32:
				di = BS_VolID32; break;

			default:
				di = BS_VolID;
			}
			*vsn = ld_dword(fs->win + di);
		}
	}

	LEAVE_FF(fs, res);
}



#if !FF_FS_READONLY
/*-----------------------------------------------------------------------*/
/* Set Volume Label                                                      */
/*-----------------------------------------------------------------------*/

FRESULT f_setlabel (
	const TCHAR* label	/* Volume label to set with heading logical drive number */
)
{
	FRESULT res;
	DIR dj;
	FATFS *fs;
	BYTE dirvn[22];
	UINT di;
	WCHAR wc;
	static const char badchr[] = "+.,;=[]/\\\"*:<>\?|\x7F";	/* [0..] for FAT, [7..] for exFAT */
#if FF_USE_LFN
	DWORD dc;
#endif

	/* Get logical drive */
	res = mount_volume(&label, &fs, FA_WRITE);
	if (res != FR_OK) LEAVE_FF(fs, res);

#if FF_FS_EXFAT
	if (fs->fs_type == FS_EXFAT) {	/* On the exFAT volume */
		mem_set(dirvn, 0, 22);
		di = 0;
		while ((UINT)*label >= ' ') {	/* Create volume label */
			dc = tchar2uni(&label);	/* Get a Unicode character */
			if (dc >= 0x10000) {
				if (dc == 0xFFFFFFFF || di >= 10) {	/* Wrong surrogate or buffer overflow */
					dc = 0;
				} else {
					st_word(dirvn + di * 2, (WCHAR)(dc >> 16)); di++;
				}
			}
			if (dc == 0 || chk_chr(badchr + 7, (int)dc) || di >= 11) {	/* Check validity of the volume label */
				LEAVE_FF(fs, FR_INVALID_NAME);
			}
			st_word(dirvn + di * 2, (WCHAR)dc); di++;
		}
	} else
#endif
	{	/* On the FAT/FAT32 volume */
		mem_set(dirvn, ' ', 11);
		di = 0;
		while ((UINT)*label >= ' ') {	/* Create volume label */
#if FF_USE_LFN
			dc = tchar2uni(&label);
			wc = (dc < 0x10000) ? ff_uni2oem(ff_wtoupper(dc), CODEPAGE) : 0;
#else									/* ANSI/OEM input */
			wc = (BYTE)*label++;
			if (dbc_1st((BYTE)wc)) wc = dbc_2nd((BYTE)*label) ? wc << 8 | (BYTE)*label++ : 0;
			if (IsLower(wc)) wc -= 0x20;		/* To upper ASCII characters */
#if FF_CODE_PAGE == 0
			if (ExCvt && wc >= 0x80) wc = ExCvt[wc - 0x80];	/* To upper extended characters (SBCS cfg) */
#elif FF_CODE_PAGE < 900
			if (wc >= 0x80) wc = ExCvt[wc - 0x80];	/* To upper extended characters (SBCS cfg) */
#endif
#endif
			if (wc == 0 || chk_chr(badchr + 0, (int)wc) || di >= (UINT)((wc >= 0x100) ? 10 : 11)) {	/* Reject invalid characters for volume label */
				LEAVE_FF(fs, FR_INVALID_NAME);
			}
			if (wc >= 0x100) dirvn[di++] = (BYTE)(wc >> 8);
			dirvn[di++] = (BYTE)wc;
		}
		if (dirvn[0] == DDEM) LEAVE_FF(fs, FR_INVALID_NAME);	/* Reject illegal name (heading DDEM) */
		while (di && dirvn[di - 1] == ' ') di--;				/* Snip trailing spaces */
	}

	/* Set volume label */
	dj.obj.fs = fs; dj.obj.sclust = 0;	/* Open root directory */
	res = dir_sdi(&dj, 0);
	if (res == FR_OK) {
		res = DIR_READ_LABEL(&dj);	/* Get volume label entry */
		if (res == FR_OK) {
			if (FF_FS_EXFAT && fs->fs_type == FS_EXFAT) {
				dj.dir[XDIR_NumLabel] = (BYTE)di;	/* Change the volume label */
				mem_cpy(dj.dir + XDIR_Label, dirvn, 22);
			} else {
				if (di != 0) {
					mem_cpy(dj.dir, dirvn, 11);	/* Change the volume label */
				} else {
					dj.dir[DIR_Name] = DDEM;	/* Remove the volume label */
				}
			}
			fs->wflag = 1;
			res = sync_fs(fs);
		} else {			/* No volume label entry or an error */
			if (res == FR_NO_FILE) {
				res = FR_OK;
				if (di != 0) {	/* Create a volume label entry */
					res = dir_alloc(&dj, 1);	/* Allocate an entry */
					if (res == FR_OK) {
						mem_set(dj.dir, 0, SZDIRE);	/* Clean the entry */
						if (FF_FS_EXFAT && fs->fs_type == FS_EXFAT) {
							dj.dir[XDIR_Type] = ET_VLABEL;	/* Create volume label entry */
							dj.dir[XDIR_NumLabel] = (BYTE)di;
							mem_cpy(dj.dir + XDIR_Label, dirvn, 22);
						} else {
							dj.dir[DIR_Attr] = AM_VOL;		/* Create volume label entry */
							mem_cpy(dj.dir, dirvn, 11);
						}
						fs->wflag = 1;
						res = sync_fs(fs);
					}
				}
			}
		}
	}

	LEAVE_FF(fs, res);
}

#endif /* !FF_FS_READONLY */
#endif /* FF_USE_LABEL */



#if FF_USE_EXPAND && !FF_FS_READONLY
/*-----------------------------------------------------------------------*/
/* Allocate a Contiguous Blocks to the File                              */
/*-----------------------------------------------------------------------*/

FRESULT f_expand (
	FIL* fp,		/* Pointer to the file object */
	FSIZE_t fsz,	/* File size to be expanded to */
	BYTE opt		/* Operation mode 0:Find and prepare or 1:Find and allocate */
)
{
	FRESULT res;
	FATFS *fs;
	DWORD n, clst, stcl, scl, ncl, tcl, lclst;


	res = validate(&fp->obj, &fs);		/* Check validity of the file object */
	if (res != FR_OK || (res = (FRESULT)fp->err) != FR_OK) LEAVE_FF(fs, res);
	if (fsz == 0 || fp->obj.objsize != 0 || !(fp->flag & FA_WRITE)) LEAVE_FF(fs, FR_DENIED);
#if FF_FS_EXFAT
	if (fs->fs_type != FS_EXFAT && fsz >= 0x100000000) LEAVE_FF(fs, FR_DENIED);	/* Check if in size limit */
#endif
	n = (DWORD)fs->csize * SS(fs);	/* Cluster size */
	tcl = (DWORD)(fsz / n) + ((fsz & (n - 1)) ? 1 : 0);	/* Number of clusters required */
	stcl = fs->last_clst; lclst = 0;
	if (stcl < 2 || stcl >= fs->n_fatent) stcl = 2;

#if FF_FS_EXFAT
	if (fs->fs_type == FS_EXFAT) {
		scl = find_bitmap(fs, stcl, tcl);			/* Find a contiguous cluster block */
		if (scl == 0) res = FR_DENIED;				/* No contiguous cluster block was found */
		if (scl == 0xFFFFFFFF) res = FR_DISK_ERR;
		if (res == FR_OK) {	/* A contiguous free area is found */
			if (opt) {		/* Allocate it now */
				res = change_bitmap(fs, scl, tcl, 1);	/* Mark the cluster block 'in use' */
				lclst = scl + tcl - 1;
			} else {		/* Set it as suggested point for next allocation */
				lclst = scl - 1;
			}
		}
	} else
#endif
	{
		scl = clst = stcl; ncl = 0;
		for (;;) {	/* Find a contiguous cluster block */
			n = get_fat(&fp->obj, clst);
			if (++clst >= fs->n_fatent) clst = 2;
			if (n == 1) { res = FR_INT_ERR; break; }
			if (n == 0xFFFFFFFF) { res = FR_DISK_ERR; break; }
			if (n == 0) {	/* Is it a free cluster? */
				if (++ncl == tcl) break;	/* Break if a contiguous cluster block is found */
			} else {
				scl = clst; ncl = 0;		/* Not a free cluster */
			}
			if (clst == stcl) { res = FR_DENIED; break; }	/* No contiguous cluster? */
		}
		if (res == FR_OK) {	/* A contiguous free area is found */
			if (opt) {		/* Allocate it now */
				for (clst = scl, n = tcl; n; clst++, n--) {	/* Create a cluster chain on the FAT */
					res = put_fat(fs, clst, (n == 1) ? 0xFFFFFFFF : clst + 1);
					if (res != FR_OK) break;
					lclst = clst;
				}
			} else {		/* Set it as suggested point for next allocation */
				lclst = scl - 1;
			}
		}
	}

	if (res == FR_OK) {
		fs->last_clst = lclst;		/* Set suggested start cluster to start next */
		if (opt) {	/* Is it allocated now? */
			fp->obj.sclust = scl;		/* Update object allocation information */
			fp->obj.objsize = fsz;
			if (FF_FS_EXFAT) fp->obj.stat = 2;	/* Set status 'contiguous chain' */
			fp->flag |= FA_MODIFIED;
			if (fs->free_clst <= fs->n_fatent - 2) {	/* Update FSINFO */
				fs->free_clst -= tcl;
				fs->fsi_flag |= 1;
			}
		}
	}

	LEAVE_FF(fs, res);
}

#endif /* FF_USE_EXPAND && !FF_FS_READONLY */



#if FF_USE_FORWARD
/*-----------------------------------------------------------------------*/
/* Forward Data to the Stream Directly                                   */
/*-----------------------------------------------------------------------*/

FRESULT f_forward (
	FIL* fp, 						/* Pointer to the file object */
	UINT (*func)(const BYTE*,UINT),	/* Pointer to the streaming function */
	UINT btf,						/* Number of bytes to forward */
	UINT* bf						/* Pointer to number of bytes forwarded */
)
{
	FRESULT res;
	FATFS *fs;
	DWORD clst;
	LBA_t sect;
	FSIZE_t remain;
	UINT rcnt, csect;
	BYTE *dbuf;


	*bf = 0;	/* Clear transfer byte counter */
	res = validate(&fp->obj, &fs);		/* Check validity of the file object */
	if (res != FR_OK || (res = (FRESULT)fp->err) != FR_OK) LEAVE_FF(fs, res);
	if (!(fp->flag & FA_READ)) LEAVE_FF(fs, FR_DENIED);	/* Check access mode */

	remain = fp->obj.objsize - fp->fptr;
	if (btf > remain) btf = (UINT)remain;			/* Truncate btf by remaining bytes */

	for ( ;  btf && (*func)(0, 0);					/* Repeat until all data transferred or stream goes busy */
		fp->fptr += rcnt, *bf += rcnt, btf -= rcnt) {
		csect = (UINT)(fp->fptr / SS(fs) & (fs->csize - 1));	/* Sector offset in the cluster */
		if (fp->fptr % SS(fs) == 0) {				/* On the sector boundary? */
			if (csect == 0) {						/* On the cluster boundary? */
				clst = (fp->fptr == 0) ?			/* On the top of the file? */
					fp->obj.sclust : get_fat(&fp->obj, fp->clust);
				if (clst <= 1) ABORT(fs, FR_INT_ERR);
				if (clst == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
				fp->clust = clst;					/* Update current cluster */
			}
		}
		sect = clst2sect(fs, fp->clust);			/* Get current data sector */
		if (sect == 0) ABORT(fs, FR_INT_ERR);
		sect += csect;
#if FF_FS_TINY
		if (move_window(fs, sect) != FR_OK) ABORT(fs, FR_DISK_ERR);	/* Move sector window to the file data */
		dbuf = fs->win;
#else
		if (fp->sect != sect) {		/* Fill sector cache with file data */
#if !FF_FS_READONLY
			if (fp->flag & FA_DIRTY) {		/* Write-back dirty sector cache */
				if (disk_write(fs->pdrv, fp->buf, fp->sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
				fp->flag &= (BYTE)~FA_DIRTY;
			}
#endif
			if (disk_read(fs->pdrv, fp->buf, sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
		}
		dbuf = fp->buf;
#endif
		fp->sect = sect;
		rcnt = SS(fs) - (UINT)fp->fptr % SS(fs);	/* Number of bytes remains in the sector */
		if (rcnt > btf) rcnt = btf;					/* Clip it by btr if needed */
		rcnt = (*func)(dbuf + ((UINT)fp->fptr % SS(fs)), rcnt);	/* Forward the file data */
		if (rcnt == 0) ABORT(fs, FR_INT_ERR);
	}

	LEAVE_FF(fs, FR_OK);
}
#endif /* FF_USE_FORWARD */



#if !FF_FS_READONLY && FF_USE_MKFS
/*-----------------------------------------------------------------------*/
/* Create an FAT/exFAT volume                                            */
/*-----------------------------------------------------------------------*/

#define N_SEC_TRACK 63			/* Sectors per track for determination of drive CHS */
#define	GPT_ALIGN	0x100000	/* Alignment of partitions in GPT [byte] (>=128KB) */
#define GPT_ITEMS	128			/* Number of GPT table size (>=128, sector aligned) */


/* Create partitions on the physical drive */

static FRESULT create_partition (
	BYTE drv,			/* Physical drive number */
	const LBA_t plst[],	/* Partition list */
	UINT sys,			/* System ID (for only MBR, temp setting) and bit8:GPT */
	BYTE* buf			/* Working buffer for a sector */
)
{
	UINT i, cy;
	LBA_t sz_drv;
	DWORD sz_drv32, s_lba32, n_lba32;
	BYTE *pte, hd, n_hd, sc, n_sc;

	/* Get drive size */
	if (disk_ioctl(drv, GET_SECTOR_COUNT, &sz_drv) != RES_OK) return FR_DISK_ERR;

#if FF_LBA64
	if (sz_drv >= FF_MIN_GPT) {	/* Create partitions in GPT */
		WORD ss;
		UINT sz_pt, pi, si, ofs;
		DWORD bcc, rnd, align;
		QWORD s_lba64, n_lba64, sz_pool, s_bpt;
		static const BYTE gpt_mbr[16] = {0x00, 0x00, 0x02, 0x00, 0xEE, 0xFE, 0xFF, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF};

#if FF_MAX_SS != FF_MIN_SS
		if (disk_ioctl(drv, GET_SECTOR_SIZE, &ss) != RES_OK) return FR_DISK_ERR;	/* Get sector size */
		if (ss > FF_MAX_SS || ss < FF_MIN_SS || (ss & (ss - 1))) return FR_DISK_ERR;
#else
		ss = FF_MAX_SS;
#endif
		rnd = GET_FATTIME();			/* Random seed */
		align = GPT_ALIGN / ss;			/* Partition alignment [sector] */
		sz_pt = GPT_ITEMS * SZ_GPTE / ss;	/* Size of PT [sector] */
		s_bpt = sz_drv - sz_pt - 1;		/* Backup PT start sector */
		s_lba64 = 2 + sz_pt;			/* First allocatable sector */
		sz_pool = s_bpt - s_lba64;		/* Size of allocatable area */
		bcc = 0xFFFFFFFF; n_lba64 = 1;
		pi = si = 0;	/* partition table index, size table index */
		do {
			if (pi * SZ_GPTE % ss == 0) mem_set(buf, 0, ss);	/* Clean the buffer if needed */
			if (n_lba64 != 0) {	/* Is the size table not termintated? */
				s_lba64 = (s_lba64 + align - 1) & ((QWORD)0 - align);	/* Align partition start */
				n_lba64 = plst[si++];	/* Get a partition size */
				if (n_lba64 <= 100) {	/* Is the size in percentage? */
					n_lba64 = sz_pool * n_lba64 / 100;
					n_lba64 = (n_lba64 + align - 1) & ((QWORD)0 - align);	/* Align partition end (only if in percentage) */
				}
				if (s_lba64 + n_lba64 > s_bpt) {	/* Clip at end of the pool */
					n_lba64 = (s_lba64 < s_bpt) ? s_bpt - s_lba64 : 0;
				}
			}
			if (n_lba64 != 0) {		/* Add a partition? */
				ofs = pi * SZ_GPTE % ss;
				mem_cpy(buf + ofs + GPTE_PtGuid, GUID_MS_Basic, 16);	/* Partition GUID (Microsoft Basic Data) */
				rnd = make_rand(rnd, buf + ofs + GPTE_UpGuid, 16);		/* Unique partition GUID */
				st_qword(buf + ofs + GPTE_FstLba, s_lba64);				/* Partition start LBA */
				st_qword(buf + ofs + GPTE_LstLba, s_lba64 + n_lba64 - 1);	/* Partition end LBA */
				s_lba64 += n_lba64;		/* Next partition LBA */
			}
			if ((pi + 1) * SZ_GPTE % ss == 0) {		/* Write the buffer if it is filled up */
				for (i = 0; i < ss; bcc = crc32(bcc, buf[i++])) ;	/* Calculate table check sum */
				if (disk_write(drv, buf, 2 + pi * SZ_GPTE / ss, 1) != RES_OK) return FR_DISK_ERR;		/* Primary table */
				if (disk_write(drv, buf, s_bpt + pi * SZ_GPTE / ss, 1) != RES_OK) return FR_DISK_ERR;	/* Secondary table */
			}
		} while (++pi < GPT_ITEMS);

		/* Create primary GPT header */
		mem_set(buf, 0, ss);
		mem_cpy(buf + GPTH_Sign, "EFI PART" "\0\0\1\0" "\x5C\0\0", 16);	/* Signature, version (1.0) and size (92) */
		st_dword(buf + GPTH_PtBcc, ~bcc);				/* Table check sum */
		st_qword(buf + GPTH_CurLba, 1);					/* LBA of this header */
		st_qword(buf + GPTH_BakLba, sz_drv - 1);		/* LBA of another header */
		st_qword(buf + GPTH_FstLba, 2 + sz_pt);			/* LBA of first allocatable sector */
		st_qword(buf + GPTH_LstLba, s_bpt - 1);			/* LBA of last allocatable sector */
		st_dword(buf + GPTH_PteSize, SZ_GPTE);			/* Size of a table entry */
		st_dword(buf + GPTH_PtNum, GPT_ITEMS);			/* Number of table entries */
		st_dword(buf + GPTH_PtOfs, 2);					/* LBA of this table */
		rnd = make_rand(rnd, buf + GPTH_DskGuid, 16);	/* Disk GUID */
		for (i = 0, bcc= 0xFFFFFFFF; i < 92; bcc = crc32(bcc, buf[i++])) ;	/* Calculate header check sum */
		st_dword(buf + GPTH_Bcc, ~bcc);					/* Header check sum */
		if (disk_write(drv, buf, 1, 1) != RES_OK) return FR_DISK_ERR;

		/* Create secondary GPT header */
		st_qword(buf + GPTH_CurLba, sz_drv - 1);		/* LBA of this header */
		st_qword(buf + GPTH_BakLba, 1);					/* LBA of another header */
		st_qword(buf + GPTH_PtOfs, s_bpt);				/* LBA of this table */
		st_dword(buf + GPTH_Bcc, 0);
		for (i = 0, bcc= 0xFFFFFFFF; i < 92; bcc = crc32(bcc, buf[i++])) ;	/* Calculate header check sum */
		st_dword(buf + GPTH_Bcc, ~bcc);					/* Header check sum */
		if (disk_write(drv, buf, sz_drv - 1, 1) != RES_OK) return FR_DISK_ERR;

		/* Create protective MBR */
		mem_set(buf, 0, ss);
		mem_cpy(buf + MBR_Table, gpt_mbr, 16);			/* Create a GPT partition */
		st_word(buf + BS_55AA, 0xAA55);
		if (disk_write(drv, buf, 0, 1) != RES_OK) return FR_DISK_ERR;

	} else
#endif
	{					/* Create partitions in MBR */
		sz_drv32 = (DWORD)sz_drv;
		n_sc = N_SEC_TRACK;		/* Determine drive CHS without any consideration of the drive geometry */
		for (n_hd = 8; n_hd != 0 && sz_drv32 / n_hd / n_sc > 1024; n_hd *= 2) ;
		if (n_hd == 0) n_hd = 255;	/* Number of heads needs to be <256 */

		mem_set(buf, 0, FF_MAX_SS);	/* Clear MBR */
		pte = buf + MBR_Table;	/* Partition table in the MBR */
		for (i = 0, s_lba32 = 2048; i < 4 && s_lba32 != 0 && s_lba32 < sz_drv32; i++, s_lba32 += n_lba32) {
			n_lba32 = (DWORD)plst[i];	/* Get partition size */
			if (n_lba32 <= 100) n_lba32 = (n_lba32 == 100) ? sz_drv32 : sz_drv32 / 100 * n_lba32;	/* Size in percentage? */
			if (s_lba32 + n_lba32 > sz_drv32 || s_lba32 + n_lba32 < s_lba32) n_lba32 = sz_drv32 - s_lba32;	/* Clip at drive size */
			if (n_lba32 == 0) break;	/* End of table or no sector to allocate? */

			st_dword(pte + PTE_StLba, s_lba32);		/* Start LBA */
			st_dword(pte + PTE_SizLba, n_lba32);	/* Number of sectors */
			pte[PTE_System] = (BYTE)sys;			/* System type */

			cy = (UINT)(s_lba32 / n_sc / n_hd);		/* Start cylinder */
			hd = (BYTE)(s_lba32 / n_sc % n_hd);		/* Start head */
			sc = (BYTE)(s_lba32 % n_sc + 1);		/* Start sector */
			pte[PTE_StHead] = hd;
			pte[PTE_StSec] = (BYTE)((cy >> 2 & 0xC0) | sc);
			pte[PTE_StCyl] = (BYTE)cy;

			cy = (UINT)((s_lba32 + n_lba32 - 1) / n_sc / n_hd);	/* End cylinder */
			hd = (BYTE)((s_lba32 + n_lba32 - 1) / n_sc % n_hd);	/* End head */
			sc = (BYTE)((s_lba32 + n_lba32 - 1) % n_sc + 1);	/* End sector */
			pte[PTE_EdHead] = hd;
			pte[PTE_EdSec] = (BYTE)((cy >> 2 & 0xC0) | sc);
			pte[PTE_EdCyl] = (BYTE)cy;

			pte += SZ_PTE;		/* Next entry */
		}

		st_word(buf + BS_55AA, 0xAA55);		/* MBR signature */
		if (disk_write(drv, buf, 0, 1) != RES_OK) return FR_DISK_ERR;	/* Write it to the MBR */
	}

	return FR_OK;
}



FRESULT f_mkfs (
	const TCHAR* path,		/* Logical drive number */
	const MKFS_PARM* opt,	/* Format options */
	void* work,				/* Pointer to working buffer (null: use heap memory) */
	UINT len				/* Size of working buffer [byte] */
)
{
	static const WORD cst[] = {1, 4, 16, 64, 256, 512, 0};	/* Cluster size boundary for FAT volume (4Ks unit) */
	static const WORD cst32[] = {1, 2, 4, 8, 16, 32, 0};	/* Cluster size boundary for FAT32 volume (128Ks unit) */
	static const MKFS_PARM defopt = {FM_ANY, 0, 0, 0, 0};	/* Default parameter */
	BYTE fsopt, fsty, sys, *buf, *pte, pdrv, ipart;
	WORD ss;	/* Sector size */
	DWORD sz_buf, sz_blk, n_clst, pau, nsect, n;
	LBA_t sz_vol, b_vol, b_fat, b_data;		/* Size of volume, Base LBA of volume, fat, data */
	LBA_t sect, lba[2];
	DWORD sz_rsv, sz_fat, sz_dir, sz_au;	/* Size of reserved, fat, dir, data, cluster */
	UINT n_fat, n_root, i;					/* Index, Number of FATs and Number of roor dir entries */
	int vol;
	DSTATUS ds;
	FRESULT fr;


	/* Check mounted drive and clear work area */
	vol = get_ldnumber(&path);					/* Get target logical drive */
	if (vol < 0) return FR_INVALID_DRIVE;
	if (FatFs[vol]) FatFs[vol]->fs_type = 0;	/* Clear the fs object if mounted */
	pdrv = LD2PD(vol);			/* Physical drive */
	ipart = LD2PT(vol);			/* Partition (0:create as new, 1..:get from partition table) */
	if (!opt) opt = &defopt;	/* Use default parameter if it is not given */

	/* Get physical drive status (sz_drv, sz_blk, ss) */
	ds = disk_initialize(pdrv);
	if (ds & STA_NOINIT) return FR_NOT_READY;
	if (ds & STA_PROTECT) return FR_WRITE_PROTECTED;
	sz_blk = opt->align;
	if (sz_blk == 0 && disk_ioctl(pdrv, GET_BLOCK_SIZE, &sz_blk) != RES_OK) sz_blk = 1;
 	if (sz_blk == 0 || sz_blk > 0x8000 || (sz_blk & (sz_blk - 1))) sz_blk = 1;
#if FF_MAX_SS != FF_MIN_SS
	if (disk_ioctl(pdrv, GET_SECTOR_SIZE, &ss) != RES_OK) return FR_DISK_ERR;
	if (ss > FF_MAX_SS || ss < FF_MIN_SS || (ss & (ss - 1))) return FR_DISK_ERR;
#else
	ss = FF_MAX_SS;
#endif
	/* Options for FAT sub-type and FAT parameters */
	fsopt = opt->fmt & (FM_ANY | FM_SFD);
	n_fat = (opt->n_fat >= 1 && opt->n_fat <= 2) ? opt->n_fat : 1;
	n_root = (opt->n_root >= 1 && opt->n_root <= 32768 && (opt->n_root % (ss / SZDIRE)) == 0) ? opt->n_root : 512;
	sz_au = (opt->au_size <= 0x1000000 && (opt->au_size & (opt->au_size - 1)) == 0) ? opt->au_size : 0;
	sz_au /= ss;	/* Byte --> Sector */

	/* Get working buffer */
	sz_buf = len / ss;		/* Size of working buffer [sector] */
	if (sz_buf == 0) return FR_NOT_ENOUGH_CORE;
	buf = (BYTE*)work;		/* Working buffer */
#if FF_USE_LFN == 3
	if (!buf) buf = ff_memalloc(sz_buf * ss);	/* Use heap memory for working buffer */
#endif
	if (!buf) return FR_NOT_ENOUGH_CORE;

	/* Determine where the volume to be located (b_vol, sz_vol) */
	b_vol = sz_vol = 0;
	if (FF_MULTI_PARTITION && ipart != 0) {	/* Is the volume associated with any specific partition? */
		/* Get partition location from the existing partition table */
		if (disk_read(pdrv, buf, 0, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);	/* Load MBR */
		if (ld_word(buf + BS_55AA) != 0xAA55) LEAVE_MKFS(FR_MKFS_ABORTED);	/* Check if MBR is valid */
#if FF_LBA64
		if (buf[MBR_Table + PTE_System] == 0xEE) {	/* GPT protective MBR? */
			DWORD n_ent, ofs;
			QWORD pt_lba;

			/* Get the partition location from GPT */
			if (disk_read(pdrv, buf, 1, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);	/* Load GPT header sector (next to MBR) */
			if (!test_gpt_header(buf)) LEAVE_MKFS(FR_MKFS_ABORTED);	/* Check if GPT header is valid */
			n_ent = ld_dword(buf + GPTH_PtNum);		/* Number of entries */
			pt_lba = ld_qword(buf + GPTH_PtOfs);	/* Table start sector */
			ofs = i = 0;
			while (n_ent) {		/* Find MS Basic partition with order of ipart */
				if (ofs == 0 && disk_read(pdrv, buf, pt_lba++, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);	/* Get PT sector */
				if (!mem_cmp(buf + ofs + GPTE_PtGuid, GUID_MS_Basic, 16) && ++i == ipart) {	/* MS basic data partition? */
					b_vol = ld_qword(buf + ofs + GPTE_FstLba);
					sz_vol = ld_qword(buf + ofs + GPTE_LstLba) - b_vol + 1;
					break;
				}
				n_ent--; ofs = (ofs + SZ_GPTE) % ss;	/* Next entry */
			}
			if (n_ent == 0) LEAVE_MKFS(FR_MKFS_ABORTED);	/* Partition not found */
			fsopt |= 0x80;	/* Partitioning is in GPT */
		} else
#endif
		{	/* Get the partition location from MBR partition table */
			pte = buf + (MBR_Table + (ipart - 1) * SZ_PTE);
			if (ipart > 4 || pte[PTE_System] == 0) LEAVE_MKFS(FR_MKFS_ABORTED);	/* No partition? */
			b_vol = ld_dword(pte + PTE_StLba);		/* Get volume start sector */
			sz_vol = ld_dword(pte + PTE_SizLba);	/* Get volume size */
		}
	} else {	/* The volume is associated with a physical drive */
		if (disk_ioctl(pdrv, GET_SECTOR_COUNT, &sz_vol) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
		if (!(fsopt & FM_SFD)) {	/* To be partitioned? */
			/* Create a single-partition on the drive in this function */
#if FF_LBA64
			if (sz_vol >= FF_MIN_GPT) {	/* Which partition type to create, MBR or GPT? */
				fsopt |= 0x80;		/* Partitioning is in GPT */
				b_vol = GPT_ALIGN / ss; sz_vol -= b_vol + GPT_ITEMS * SZ_GPTE / ss + 1;	/* Estimated partition offset and size */
			} else
#endif
			{	/* Partitioning is in MBR */
				if (sz_vol > N_SEC_TRACK) {
					b_vol = 2048; sz_vol -= b_vol;	/* Estimated partition offset and size */
				}
			}
		}
	}
	if (sz_vol < 128) LEAVE_MKFS(FR_MKFS_ABORTED);	/* Check if volume size is >=128s */

	/* Now start to create a FAT volume at b_vol and sz_vol */

	do {	/* Pre-determine the FAT type */
		if (FF_FS_EXFAT && (fsopt & FM_EXFAT)) {	/* exFAT possible? */
			if ((fsopt & FM_ANY) == FM_EXFAT || sz_vol >= 0x4000000 || sz_au > 128) {	/* exFAT only, vol >= 64MS or sz_au > 128S ? */
				fsty = FS_EXFAT; break;
			}
		}
#if FF_LBA64
		if (sz_vol >= 0x100000000) LEAVE_MKFS(FR_MKFS_ABORTED);	/* Too large volume for FAT/FAT32 */
#endif
		if (sz_au > 128) sz_au = 128;	/* Invalid AU for FAT/FAT32? */
		if (fsopt & FM_FAT32) {	/* FAT32 possible? */
			if (!(fsopt & FM_FAT)) {	/* no-FAT? */
				fsty = FS_FAT32; break;
			}
		}
		if (!(fsopt & FM_FAT)) LEAVE_MKFS(FR_INVALID_PARAMETER);	/* no-FAT? */
		fsty = FS_FAT16;
	} while (0);

#if FF_FS_EXFAT
	if (fsty == FS_EXFAT) {	/* Create an exFAT volume */
		DWORD szb_bit, szb_case, sum, nb, cl, tbl[3];
		WCHAR ch, si;
		UINT j, st;
		BYTE b;

		if (sz_vol < 0x1000) LEAVE_MKFS(FR_MKFS_ABORTED);	/* Too small volume for exFAT? */
#if FF_USE_TRIM
		lba[0] = b_vol; lba[1] = b_vol + sz_vol - 1;	/* Inform storage device that the volume area may be erased */
		disk_ioctl(pdrv, CTRL_TRIM, lba);
#endif
		/* Determine FAT location, data location and number of clusters */
		if (sz_au == 0) {	/* AU auto-selection */
			sz_au = 8;
			if (sz_vol >= 0x80000) sz_au = 64;		/* >= 512Ks */
			if (sz_vol >= 0x4000000) sz_au = 256;	/* >= 64Ms */
		}
		b_fat = b_vol + 32;										/* FAT start at offset 32 */
		sz_fat = (DWORD)(((sz_vol / sz_au + 2) * 4 + ss - 1) / ss);	/* Number of FAT sectors */
		b_data = (b_fat + sz_fat + sz_blk - 1) & ~((LBA_t)sz_blk - 1);	/* Align data area to the erase block boundary */
		if (b_data - b_vol >= sz_vol / 2) LEAVE_MKFS(FR_MKFS_ABORTED);	/* Too small volume? */
		n_clst = (DWORD)((sz_vol - (b_data - b_vol)) / sz_au);	/* Number of clusters */
		if (n_clst <16) LEAVE_MKFS(FR_MKFS_ABORTED);			/* Too few clusters? */
		if (n_clst > MAX_EXFAT) LEAVE_MKFS(FR_MKFS_ABORTED);	/* Too many clusters? */

		szb_bit = (n_clst + 7) / 8;							/* Size of allocation bitmap */
		tbl[0] = (szb_bit + sz_au * ss - 1) / (sz_au * ss);	/* Number of allocation bitmap clusters */

		/* Create a compressed up-case table */
		sect = b_data + sz_au * tbl[0];	/* Table start sector */
		sum = 0;						/* Table checksum to be stored in the 82 entry */
		st = 0; si = 0; i = 0; j = 0; szb_case = 0;
		do {
			switch (st) {
			case 0:
				ch = (WCHAR)ff_wtoupper(si);	/* Get an up-case char */
				if (ch != si) {
					si++; break;		/* Store the up-case char if exist */
				}
				for (j = 1; (WCHAR)(si + j) && (WCHAR)(si + j) == ff_wtoupper((WCHAR)(si + j)); j++) ;	/* Get run length of no-case block */
				if (j >= 128) {
					ch = 0xFFFF; st = 2; break;	/* Compress the no-case block if run is >= 128 */
				}
				st = 1;			/* Do not compress short run */
				/* go to next case */
			case 1:
				ch = si++;		/* Fill the short run */
				if (--j == 0) st = 0;
				break;

			default:
				ch = (WCHAR)j; si += (WCHAR)j;	/* Number of chars to skip */
				st = 0;
			}
			sum = xsum32(buf[i + 0] = (BYTE)ch, sum);		/* Put it into the write buffer */
			sum = xsum32(buf[i + 1] = (BYTE)(ch >> 8), sum);
			i += 2; szb_case += 2;
			if (si == 0 || i == sz_buf * ss) {		/* Write buffered data when buffer full or end of process */
				n = (i + ss - 1) / ss;
				if (disk_write(pdrv, buf, sect, n) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
				sect += n; i = 0;
			}
		} while (si);
		tbl[1] = (szb_case + sz_au * ss - 1) / (sz_au * ss);	/* Number of up-case table clusters */
		tbl[2] = 1;										/* Number of root dir clusters */

		/* Initialize the allocation bitmap */
		sect = b_data; nsect = (szb_bit + ss - 1) / ss;	/* Start of bitmap and number of sectors */
		nb = tbl[0] + tbl[1] + tbl[2];					/* Number of clusters in-use by system */
		do {
			mem_set(buf, 0, sz_buf * ss);
			for (i = 0; nb >= 8 && i < sz_buf * ss; buf[i++] = 0xFF, nb -= 8) ;
			for (b = 1; nb != 0 && i < sz_buf * ss; buf[i] |= b, b <<= 1, nb--) ;
			n = (nsect > sz_buf) ? sz_buf : nsect;		/* Write the buffered data */
			if (disk_write(pdrv, buf, sect, n) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
			sect += n; nsect -= n;
		} while (nsect);

		/* Initialize the FAT */
		sect = b_fat; nsect = sz_fat;	/* Start of FAT and number of FAT sectors */
		j = nb = cl = 0;
		do {
			mem_set(buf, 0, sz_buf * ss); i = 0;	/* Clear work area and reset write index */
			if (cl == 0) {	/* Set FAT [0] and FAT[1] */
				st_dword(buf + i, 0xFFFFFFF8); i += 4; cl++;
				st_dword(buf + i, 0xFFFFFFFF); i += 4; cl++;
			}
			do {			/* Create chains of bitmap, up-case and root dir */
				while (nb != 0 && i < sz_buf * ss) {	/* Create a chain */
					st_dword(buf + i, (nb > 1) ? cl + 1 : 0xFFFFFFFF);
					i += 4; cl++; nb--;
				}
				if (nb == 0 && j < 3) nb = tbl[j++];	/* Next chain */
			} while (nb != 0 && i < sz_buf * ss);
			n = (nsect > sz_buf) ? sz_buf : nsect;	/* Write the buffered data */
			if (disk_write(pdrv, buf, sect, n) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
			sect += n; nsect -= n;
		} while (nsect);

		/* Initialize the root directory */
		mem_set(buf, 0, sz_buf * ss);
		buf[SZDIRE * 0 + 0] = ET_VLABEL;					/* Volume label entry (no label) */

        //default label: Ventoy (unicode)
        buf[1] = 6; //label length
        memcpy(buf + 2, L"Ventoy", 2 * buf[1]);

		buf[SZDIRE * 1 + 0] = ET_BITMAP;					/* Bitmap entry */
		st_dword(buf + SZDIRE * 1 + 20, 2);					/*  cluster */
		st_dword(buf + SZDIRE * 1 + 24, szb_bit);			/*  size */
		buf[SZDIRE * 2 + 0] = ET_UPCASE;					/* Up-case table entry */
		st_dword(buf + SZDIRE * 2 + 4, sum);				/*  sum */
		st_dword(buf + SZDIRE * 2 + 20, 2 + tbl[0]);		/*  cluster */
		st_dword(buf + SZDIRE * 2 + 24, szb_case);			/*  size */
		sect = b_data + sz_au * (tbl[0] + tbl[1]); nsect = sz_au;	/* Start of the root directory and number of sectors */
		do {	/* Fill root directory sectors */
			n = (nsect > sz_buf) ? sz_buf : nsect;
			if (disk_write(pdrv, buf, sect, n) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
			mem_set(buf, 0, ss);
			sect += n; nsect -= n;
		} while (nsect);

		/* Create two set of the exFAT VBR blocks */
		sect = b_vol;
		for (n = 0; n < 2; n++) {
			/* Main record (+0) */
			mem_set(buf, 0, ss);
			mem_cpy(buf + BS_JmpBoot, "\xEB\x76\x90" "EXFAT   ", 11);	/* Boot jump code (x86), OEM name */
			st_qword(buf + BPB_VolOfsEx, b_vol);					/* Volume offset in the physical drive [sector] */
			st_qword(buf + BPB_TotSecEx, sz_vol);					/* Volume size [sector] */
			st_dword(buf + BPB_FatOfsEx, (DWORD)(b_fat - b_vol));	/* FAT offset [sector] */
			st_dword(buf + BPB_FatSzEx, sz_fat);					/* FAT size [sector] */
			st_dword(buf + BPB_DataOfsEx, (DWORD)(b_data - b_vol));	/* Data offset [sector] */
			st_dword(buf + BPB_NumClusEx, n_clst);					/* Number of clusters */
			st_dword(buf + BPB_RootClusEx, 2 + tbl[0] + tbl[1]);	/* Root dir cluster # */
			st_dword(buf + BPB_VolIDEx, GET_FATTIME());				/* VSN */
			st_word(buf + BPB_FSVerEx, 0x100);						/* Filesystem version (1.00) */
			for (buf[BPB_BytsPerSecEx] = 0, i = ss; i >>= 1; buf[BPB_BytsPerSecEx]++) ;	/* Log2 of sector size [byte] */
			for (buf[BPB_SecPerClusEx] = 0, i = sz_au; i >>= 1; buf[BPB_SecPerClusEx]++) ;	/* Log2 of cluster size [sector] */
			buf[BPB_NumFATsEx] = 1;					/* Number of FATs */
			buf[BPB_DrvNumEx] = 0x80;				/* Drive number (for int13) */
			st_word(buf + BS_BootCodeEx, 0xFEEB);	/* Boot code (x86) */
			st_word(buf + BS_55AA, 0xAA55);			/* Signature (placed here regardless of sector size) */
			for (i = sum = 0; i < ss; i++) {		/* VBR checksum */
				if (i != BPB_VolFlagEx && i != BPB_VolFlagEx + 1 && i != BPB_PercInUseEx) sum = xsum32(buf[i], sum);
			}
			if (disk_write(pdrv, buf, sect++, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
			/* Extended bootstrap record (+1..+8) */
			mem_set(buf, 0, ss);
			st_word(buf + ss - 2, 0xAA55);	/* Signature (placed at end of sector) */
			for (j = 1; j < 9; j++) {
				for (i = 0; i < ss; sum = xsum32(buf[i++], sum)) ;	/* VBR checksum */
				if (disk_write(pdrv, buf, sect++, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
			}
			/* OEM/Reserved record (+9..+10) */
			mem_set(buf, 0, ss);
			for ( ; j < 11; j++) {
				for (i = 0; i < ss; sum = xsum32(buf[i++], sum)) ;	/* VBR checksum */
				if (disk_write(pdrv, buf, sect++, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
			}
			/* Sum record (+11) */
			for (i = 0; i < ss; i += 4) st_dword(buf + i, sum);		/* Fill with checksum value */
			if (disk_write(pdrv, buf, sect++, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
		}

	} else
#endif	/* FF_FS_EXFAT */
	{	/* Create an FAT/FAT32 volume */
		do {
			pau = sz_au;
			/* Pre-determine number of clusters and FAT sub-type */
			if (fsty == FS_FAT32) {	/* FAT32 volume */
				if (pau == 0) {	/* AU auto-selection */
					n = (DWORD)sz_vol / 0x20000;	/* Volume size in unit of 128KS */
					for (i = 0, pau = 1; cst32[i] && cst32[i] <= n; i++, pau <<= 1) ;	/* Get from table */
				}
				n_clst = (DWORD)sz_vol / pau;	/* Number of clusters */
				sz_fat = (n_clst * 4 + 8 + ss - 1) / ss;	/* FAT size [sector] */
				sz_rsv = 32;	/* Number of reserved sectors */
				sz_dir = 0;		/* No static directory */
				if (n_clst <= MAX_FAT16 || n_clst > MAX_FAT32) LEAVE_MKFS(FR_MKFS_ABORTED);
			} else {				/* FAT volume */
				if (pau == 0) {	/* au auto-selection */
					n = (DWORD)sz_vol / 0x1000;	/* Volume size in unit of 4KS */
					for (i = 0, pau = 1; cst[i] && cst[i] <= n; i++, pau <<= 1) ;	/* Get from table */
				}
				n_clst = (DWORD)sz_vol / pau;
				if (n_clst > MAX_FAT12) {
					n = n_clst * 2 + 4;		/* FAT size [byte] */
				} else {
					fsty = FS_FAT12;
					n = (n_clst * 3 + 1) / 2 + 3;	/* FAT size [byte] */
				}
				sz_fat = (n + ss - 1) / ss;		/* FAT size [sector] */
				sz_rsv = 1;						/* Number of reserved sectors */
				sz_dir = (DWORD)n_root * SZDIRE / ss;	/* Root dir size [sector] */
			}
			b_fat = b_vol + sz_rsv;						/* FAT base */
			b_data = b_fat + sz_fat * n_fat + sz_dir;	/* Data base */

			/* Align data area to erase block boundary (for flash memory media) */
			n = (DWORD)(((b_data + sz_blk - 1) & ~(sz_blk - 1)) - b_data);	/* Sectors to next nearest from current data base */
			if (fsty == FS_FAT32) {		/* FAT32: Move FAT */
				sz_rsv += n; b_fat += n;
			} else {					/* FAT: Expand FAT */
				if (n % n_fat) {	/* Adjust fractional error if needed */
					n--; sz_rsv++; b_fat++;
				}
				sz_fat += n / n_fat;
			}

			/* Determine number of clusters and final check of validity of the FAT sub-type */
			if (sz_vol < b_data + pau * 16 - b_vol) LEAVE_MKFS(FR_MKFS_ABORTED);	/* Too small volume? */
			n_clst = ((DWORD)sz_vol - sz_rsv - sz_fat * n_fat - sz_dir) / pau;
			if (fsty == FS_FAT32) {
				if (n_clst <= MAX_FAT16) {	/* Too few clusters for FAT32? */
					if (sz_au == 0 && (sz_au = pau / 2) != 0) continue;	/* Adjust cluster size and retry */
					LEAVE_MKFS(FR_MKFS_ABORTED);
				}
			}
			if (fsty == FS_FAT16) {
				if (n_clst > MAX_FAT16) {	/* Too many clusters for FAT16 */
					if (sz_au == 0 && (pau * 2) <= 64) {
						sz_au = pau * 2; continue;		/* Adjust cluster size and retry */
					}
					if ((fsopt & FM_FAT32)) {
						fsty = FS_FAT32; continue;	/* Switch type to FAT32 and retry */
					}
					if (sz_au == 0 && (sz_au = pau * 2) <= 128) continue;	/* Adjust cluster size and retry */
					LEAVE_MKFS(FR_MKFS_ABORTED);
				}
				if  (n_clst <= MAX_FAT12) {	/* Too few clusters for FAT16 */
					if (sz_au == 0 && (sz_au = pau * 2) <= 128) continue;	/* Adjust cluster size and retry */
					LEAVE_MKFS(FR_MKFS_ABORTED);
				}
			}
			if (fsty == FS_FAT12 && n_clst > MAX_FAT12) LEAVE_MKFS(FR_MKFS_ABORTED);	/* Too many clusters for FAT12 */

			/* Ok, it is the valid cluster configuration */
			break;
		} while (1);

#if FF_USE_TRIM
		lba[0] = b_vol; lba[1] = b_vol + sz_vol - 1;	/* Inform storage device that the volume area may be erased */
		disk_ioctl(pdrv, CTRL_TRIM, lba);
#endif
		/* Create FAT VBR */
		mem_set(buf, 0, ss);
		mem_cpy(buf + BS_JmpBoot, "\xEB\xFE\x90" "MSDOS5.0", 11);/* Boot jump code (x86), OEM name */
		st_word(buf + BPB_BytsPerSec, ss);				/* Sector size [byte] */
		buf[BPB_SecPerClus] = (BYTE)pau;				/* Cluster size [sector] */
		st_word(buf + BPB_RsvdSecCnt, (WORD)sz_rsv);	/* Size of reserved area */
		buf[BPB_NumFATs] = (BYTE)n_fat;					/* Number of FATs */
		st_word(buf + BPB_RootEntCnt, (WORD)((fsty == FS_FAT32) ? 0 : n_root));	/* Number of root directory entries */
		if (sz_vol < 0x10000) {
			st_word(buf + BPB_TotSec16, (WORD)sz_vol);	/* Volume size in 16-bit LBA */
		} else {
			st_dword(buf + BPB_TotSec32, (DWORD)sz_vol);	/* Volume size in 32-bit LBA */
		}
		buf[BPB_Media] = 0xF8;							/* Media descriptor byte */
		st_word(buf + BPB_SecPerTrk, 63);				/* Number of sectors per track (for int13) */
		st_word(buf + BPB_NumHeads, 255);				/* Number of heads (for int13) */
		st_dword(buf + BPB_HiddSec, (DWORD)b_vol);		/* Volume offset in the physical drive [sector] */
		if (fsty == FS_FAT32) {
			st_dword(buf + BS_VolID32, GET_FATTIME());	/* VSN */
			st_dword(buf + BPB_FATSz32, sz_fat);		/* FAT size [sector] */
			st_dword(buf + BPB_RootClus32, 2);			/* Root directory cluster # (2) */
			st_word(buf + BPB_FSInfo32, 1);				/* Offset of FSINFO sector (VBR + 1) */
			st_word(buf + BPB_BkBootSec32, 6);			/* Offset of backup VBR (VBR + 6) */
			buf[BS_DrvNum32] = 0x80;					/* Drive number (for int13) */
			buf[BS_BootSig32] = 0x29;					/* Extended boot signature */
			mem_cpy(buf + BS_VolLab32, "NO NAME    " "FAT32   ", 19);	/* Volume label, FAT signature */
		} else {
			st_dword(buf + BS_VolID, GET_FATTIME());	/* VSN */
			st_word(buf + BPB_FATSz16, (WORD)sz_fat);	/* FAT size [sector] */
			buf[BS_DrvNum] = 0x80;						/* Drive number (for int13) */
			buf[BS_BootSig] = 0x29;						/* Extended boot signature */
			mem_cpy(buf + BS_VolLab, "NO NAME    " "FAT     ", 19);	/* Volume label, FAT signature */
		}
		st_word(buf + BS_55AA, 0xAA55);					/* Signature (offset is fixed here regardless of sector size) */
		if (disk_write(pdrv, buf, b_vol, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);	/* Write it to the VBR sector */

		/* Create FSINFO record if needed */
		if (fsty == FS_FAT32) {
			disk_write(pdrv, buf, b_vol + 6, 1);		/* Write backup VBR (VBR + 6) */
			mem_set(buf, 0, ss);
			st_dword(buf + FSI_LeadSig, 0x41615252);
			st_dword(buf + FSI_StrucSig, 0x61417272);
			st_dword(buf + FSI_Free_Count, n_clst - 1);	/* Number of free clusters */
			st_dword(buf + FSI_Nxt_Free, 2);			/* Last allocated cluster# */
			st_word(buf + BS_55AA, 0xAA55);
			disk_write(pdrv, buf, b_vol + 7, 1);		/* Write backup FSINFO (VBR + 7) */
			disk_write(pdrv, buf, b_vol + 1, 1);		/* Write original FSINFO (VBR + 1) */
		}

		/* Initialize FAT area */
		mem_set(buf, 0, sz_buf * ss);
		sect = b_fat;		/* FAT start sector */
		for (i = 0; i < n_fat; i++) {			/* Initialize FATs each */
			if (fsty == FS_FAT32) {
				st_dword(buf + 0, 0xFFFFFFF8);	/* FAT[0] */
				st_dword(buf + 4, 0xFFFFFFFF);	/* FAT[1] */
				st_dword(buf + 8, 0x0FFFFFFF);	/* FAT[2] (root directory) */
			} else {
				st_dword(buf + 0, (fsty == FS_FAT12) ? 0xFFFFF8 : 0xFFFFFFF8);	/* FAT[0] and FAT[1] */
			}
			nsect = sz_fat;		/* Number of FAT sectors */
			do {	/* Fill FAT sectors */
				n = (nsect > sz_buf) ? sz_buf : nsect;
				if (disk_write(pdrv, buf, sect, (UINT)n) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
				mem_set(buf, 0, ss);	/* Rest of FAT all are cleared */
				sect += n; nsect -= n;
			} while (nsect);
		}

		/* Initialize root directory (fill with zero) */
		nsect = (fsty == FS_FAT32) ? pau : sz_dir;	/* Number of root directory sectors */
		do {
			n = (nsect > sz_buf) ? sz_buf : nsect;
			if (disk_write(pdrv, buf, sect, (UINT)n) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
			sect += n; nsect -= n;
		} while (nsect);
	}

	/* A FAT volume has been created here */

	/* Determine system ID in the MBR partition table */
	if (FF_FS_EXFAT && fsty == FS_EXFAT) {
		sys = 0x07;			/* exFAT */
	} else {
		if (fsty == FS_FAT32) {
			sys = 0x0C;		/* FAT32X */
		} else {
			if (sz_vol >= 0x10000) {
				sys = 0x06;	/* FAT12/16 (large) */
			} else {
				sys = (fsty == FS_FAT16) ? 0x04 : 0x01;	/* FAT16 : FAT12 */
			}
		}
	}

	/* Update partition information */
	if (FF_MULTI_PARTITION && ipart != 0) {	/* Volume is in the existing partition */
		if (!FF_LBA64 || !(fsopt & 0x80)) {
			/* Update system ID in the partition table */
			if (disk_read(pdrv, buf, 0, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);	/* Read the MBR */
			buf[MBR_Table + (ipart - 1) * SZ_PTE + PTE_System] = sys;			/* Set system ID */
			if (disk_write(pdrv, buf, 0, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);	/* Write it back to the MBR */
		}
	} else {								/* Volume as a new single partition */
		if (!(fsopt & FM_SFD)) {	/* Create partition table if not in SFD */
			lba[0] = sz_vol, lba[1] = 0;
			fr = create_partition(pdrv, lba, sys, buf);
			if (fr != FR_OK) LEAVE_MKFS(fr);
		}
	}

	if (disk_ioctl(pdrv, CTRL_SYNC, 0) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);

	LEAVE_MKFS(FR_OK);
}




#if FF_MULTI_PARTITION
/*-----------------------------------------------------------------------*/
/* Create Partition Table on the Physical Drive                          */
/*-----------------------------------------------------------------------*/

FRESULT f_fdisk (
	BYTE pdrv,			/* Physical drive number */
	const LBA_t ptbl[],	/* Pointer to the size table for each partitions */
	void* work			/* Pointer to the working buffer (null: use heap memory) */
)
{
	BYTE *buf = (BYTE*)work;
	DSTATUS stat;


	stat = disk_initialize(pdrv);
	if (stat & STA_NOINIT) return FR_NOT_READY;
	if (stat & STA_PROTECT) return FR_WRITE_PROTECTED;
#if FF_USE_LFN == 3
	if (!buf) buf = ff_memalloc(FF_MAX_SS);	/* Use heap memory for working buffer */
#endif
	if (!buf) return FR_NOT_ENOUGH_CORE;

	LEAVE_MKFS(create_partition(pdrv, ptbl, 0x07, buf));
}

#endif /* FF_MULTI_PARTITION */
#endif /* !FF_FS_READONLY && FF_USE_MKFS */




#if FF_USE_STRFUNC
#if FF_USE_LFN && FF_LFN_UNICODE && (FF_STRF_ENCODE < 0 || FF_STRF_ENCODE > 3)
#error Wrong FF_STRF_ENCODE setting
#endif
/*-----------------------------------------------------------------------*/
/* Get a String from the File                                            */
/*-----------------------------------------------------------------------*/

TCHAR* f_gets (
	TCHAR* buff,	/* Pointer to the buffer to store read string */
	int len,		/* Size of string buffer (items) */
	FIL* fp			/* Pointer to the file object */
)
{
	int nc = 0;
	TCHAR *p = buff;
	BYTE s[4];
	UINT rc;
	DWORD dc;
#if FF_USE_LFN && FF_LFN_UNICODE && FF_STRF_ENCODE <= 2
	WCHAR wc;
#endif
#if FF_USE_LFN && FF_LFN_UNICODE && FF_STRF_ENCODE == 3
	UINT ct;
#endif

#if FF_USE_LFN && FF_LFN_UNICODE			/* With code conversion (Unicode API) */
	/* Make a room for the character and terminator  */
	if (FF_LFN_UNICODE == 1) len -= (FF_STRF_ENCODE == 0) ? 1 : 2;
	if (FF_LFN_UNICODE == 2) len -= (FF_STRF_ENCODE == 0) ? 3 : 4;
	if (FF_LFN_UNICODE == 3) len -= 1;
	while (nc < len) {
#if FF_STRF_ENCODE == 0				/* Read a character in ANSI/OEM */
		f_read(fp, s, 1, &rc);		/* Get a code unit */
		if (rc != 1) break;			/* EOF? */
		wc = s[0];
		if (dbc_1st((BYTE)wc)) {	/* DBC 1st byte? */
			f_read(fp, s, 1, &rc);	/* Get DBC 2nd byte */
			if (rc != 1 || !dbc_2nd(s[0])) continue;	/* Wrong code? */
			wc = wc << 8 | s[0];
		}
		dc = ff_oem2uni(wc, CODEPAGE);	/* OEM --> */
		if (dc == 0) continue;
#elif FF_STRF_ENCODE == 1 || FF_STRF_ENCODE == 2 	/* Read a character in UTF-16LE/BE */
		f_read(fp, s, 2, &rc);		/* Get a code unit */
		if (rc != 2) break;			/* EOF? */
		dc = (FF_STRF_ENCODE == 1) ? ld_word(s) : s[0] << 8 | s[1];
		if (IsSurrogateL(dc)) continue;	/* Broken surrogate pair? */
		if (IsSurrogateH(dc)) {		/* High surrogate? */
			f_read(fp, s, 2, &rc);	/* Get low surrogate */
			if (rc != 2) break;		/* EOF? */
			wc = (FF_STRF_ENCODE == 1) ? ld_word(s) : s[0] << 8 | s[1];
			if (!IsSurrogateL(wc)) continue;	/* Broken surrogate pair? */
			dc = ((dc & 0x3FF) + 0x40) << 10 | (wc & 0x3FF);	/* Merge surrogate pair */
		}
#else	/* Read a character in UTF-8 */
		f_read(fp, s, 1, &rc);		/* Get a code unit */
		if (rc != 1) break;			/* EOF? */
		dc = s[0];
		if (dc >= 0x80) {			/* Multi-byte sequence? */
			ct = 0;
			if ((dc & 0xE0) == 0xC0) { dc &= 0x1F; ct = 1; }	/* 2-byte sequence? */
			if ((dc & 0xF0) == 0xE0) { dc &= 0x0F; ct = 2; }	/* 3-byte sequence? */
			if ((dc & 0xF8) == 0xF0) { dc &= 0x07; ct = 3; }	/* 4-byte sequence? */
			if (ct == 0) continue;
			f_read(fp, s, ct, &rc);		/* Get trailing bytes */
			if (rc != ct) break;
			rc = 0;
			do {	/* Merge the byte sequence */
				if ((s[rc] & 0xC0) != 0x80) break;
				dc = dc << 6 | (s[rc] & 0x3F);
			} while (++rc < ct);
			if (rc != ct || dc < 0x80 || IsSurrogate(dc) || dc >= 0x110000) continue;	/* Wrong encoding? */
		}
#endif
		/* A code point is avaialble in dc to be output */

		if (FF_USE_STRFUNC == 2 && dc == '\r') continue;	/* Strip \r off if needed */
#if FF_LFN_UNICODE == 1	|| FF_LFN_UNICODE == 3	/* Output it in UTF-16/32 encoding */
		if (FF_LFN_UNICODE == 1 && dc >= 0x10000) {	/* Out of BMP at UTF-16? */
			*p++ = (TCHAR)(0xD800 | ((dc >> 10) - 0x40)); nc++;	/* Make and output high surrogate */
			dc = 0xDC00 | (dc & 0x3FF);		/* Make low surrogate */
		}
		*p++ = (TCHAR)dc; nc++;
		if (dc == '\n') break;	/* End of line? */
#elif FF_LFN_UNICODE == 2		/* Output it in UTF-8 encoding */
		if (dc < 0x80) {	/* Single byte? */
			*p++ = (TCHAR)dc;
			nc++;
			if (dc == '\n') break;	/* End of line? */
		} else {
			if (dc < 0x800) {		/* 2-byte sequence? */
				*p++ = (TCHAR)(0xC0 | (dc >> 6 & 0x1F));
				*p++ = (TCHAR)(0x80 | (dc >> 0 & 0x3F));
				nc += 2;
			} else {
				if (dc < 0x10000) {	/* 3-byte sequence? */
					*p++ = (TCHAR)(0xE0 | (dc >> 12 & 0x0F));
					*p++ = (TCHAR)(0x80 | (dc >> 6 & 0x3F));
					*p++ = (TCHAR)(0x80 | (dc >> 0 & 0x3F));
					nc += 3;
				} else {			/* 4-byte sequence? */
					*p++ = (TCHAR)(0xF0 | (dc >> 18 & 0x07));
					*p++ = (TCHAR)(0x80 | (dc >> 12 & 0x3F));
					*p++ = (TCHAR)(0x80 | (dc >> 6 & 0x3F));
					*p++ = (TCHAR)(0x80 | (dc >> 0 & 0x3F));
					nc += 4;
				}
			}
		}
#endif
	}

#else			/* Byte-by-byte read without any conversion (ANSI/OEM API) */
	len -= 1;	/* Make a room for the terminator */
	while (nc < len) {
		f_read(fp, s, 1, &rc);	/* Get a byte */
		if (rc != 1) break;		/* EOF? */
		dc = s[0];
		if (FF_USE_STRFUNC == 2 && dc == '\r') continue;
		*p++ = (TCHAR)dc; nc++;
		if (dc == '\n') break;
	}
#endif

	*p = 0;		/* Terminate the string */
	return nc ? buff : 0;	/* When no data read due to EOF or error, return with error. */
}




#if !FF_FS_READONLY
#include <stdarg.h>
/*-----------------------------------------------------------------------*/
/* Put a Character to the File (sub-functions)                           */
/*-----------------------------------------------------------------------*/

/* Putchar output buffer and work area */

typedef struct {
	FIL *fp;		/* Ptr to the writing file */
	int idx, nchr;	/* Write index of buf[] (-1:error), number of encoding units written */
#if FF_USE_LFN && FF_LFN_UNICODE == 1
	WCHAR hs;
#elif FF_USE_LFN && FF_LFN_UNICODE == 2
	BYTE bs[4];
	UINT wi, ct;
#endif
	BYTE buf[64];	/* Write buffer */
} putbuff;


/* Buffered write with code conversion */

static void putc_bfd (putbuff* pb, TCHAR c)
{
	UINT n;
	int i, nc;
#if FF_USE_LFN && FF_LFN_UNICODE
	WCHAR hs, wc;
#if FF_LFN_UNICODE == 2
	DWORD dc;
	TCHAR *tp;
#endif
#endif

	if (FF_USE_STRFUNC == 2 && c == '\n') {	 /* LF -> CRLF conversion */
		putc_bfd(pb, '\r');
	}

	i = pb->idx;			/* Write index of pb->buf[] */
	if (i < 0) return;
	nc = pb->nchr;			/* Write unit counter */

#if FF_USE_LFN && FF_LFN_UNICODE
#if FF_LFN_UNICODE == 1		/* UTF-16 input */
	if (IsSurrogateH(c)) {	/* High surrogate? */
		pb->hs = c; return;	/* Save it for next */
	}
	hs = pb->hs; pb->hs = 0;
	if (hs != 0) {			/* There is a leading high surrogate */
		if (!IsSurrogateL(c)) hs = 0;	/* Discard high surrogate if not a surrogate pair */
	} else {
		if (IsSurrogateL(c)) return;	/* Discard stray low surrogate */
	}
	wc = c;
#elif FF_LFN_UNICODE == 2	/* UTF-8 input */
	for (;;) {
		if (pb->ct == 0) {	/* Out of multi-byte sequence? */
			pb->bs[pb->wi = 0] = (BYTE)c;	/* Save 1st byte */
			if ((BYTE)c < 0x80) break;					/* Single byte? */
			if (((BYTE)c & 0xE0) == 0xC0) pb->ct = 1;	/* 2-byte sequence? */
			if (((BYTE)c & 0xF0) == 0xE0) pb->ct = 2;	/* 3-byte sequence? */
			if (((BYTE)c & 0xF1) == 0xF0) pb->ct = 3;	/* 4-byte sequence? */
			return;
		} else {				/* In the multi-byte sequence */
			if (((BYTE)c & 0xC0) != 0x80) {	/* Broken sequence? */
				pb->ct = 0; continue;
			}
			pb->bs[++pb->wi] = (BYTE)c;	/* Save the trailing byte */
			if (--pb->ct == 0) break;	/* End of multi-byte sequence? */
			return;
		}
	}
	tp = (TCHAR*)pb->bs;
	dc = tchar2uni(&tp);	/* UTF-8 ==> UTF-16 */
	if (dc == 0xFFFFFFFF) return;	/* Wrong code? */
	wc = (WCHAR)dc;
	hs = (WCHAR)(dc >> 16);
#elif FF_LFN_UNICODE == 3	/* UTF-32 input */
	if (IsSurrogate(c) || c >= 0x110000) return;	/* Discard invalid code */
	if (c >= 0x10000) {		/* Out of BMP? */
		hs = (WCHAR)(0xD800 | ((c >> 10) - 0x40)); 	/* Make high surrogate */
		wc = 0xDC00 | (c & 0x3FF);					/* Make low surrogate */
	} else {
		hs = 0;
		wc = (WCHAR)c;
	}
#endif
	/* A code point in UTF-16 is available in hs and wc */

#if FF_STRF_ENCODE == 1		/* Write a code point in UTF-16LE */
	if (hs != 0) {	/* Surrogate pair? */
		st_word(&pb->buf[i], hs);
		i += 2;
		nc++;
	}
	st_word(&pb->buf[i], wc);
	i += 2;
#elif FF_STRF_ENCODE == 2	/* Write a code point in UTF-16BE */
	if (hs != 0) {	/* Surrogate pair? */
		pb->buf[i++] = (BYTE)(hs >> 8);
		pb->buf[i++] = (BYTE)hs;
		nc++;
	}
	pb->buf[i++] = (BYTE)(wc >> 8);
	pb->buf[i++] = (BYTE)wc;
#elif FF_STRF_ENCODE == 3	/* Write a code point in UTF-8 */
	if (hs != 0) {	/* 4-byte sequence? */
		nc += 3;
		hs = (hs & 0x3FF) + 0x40;
		pb->buf[i++] = (BYTE)(0xF0 | hs >> 8);
		pb->buf[i++] = (BYTE)(0x80 | (hs >> 2 & 0x3F));
		pb->buf[i++] = (BYTE)(0x80 | (hs & 3) << 4 | (wc >> 6 & 0x0F));
		pb->buf[i++] = (BYTE)(0x80 | (wc & 0x3F));
	} else {
		if (wc < 0x80) {	/* Single byte? */
			pb->buf[i++] = (BYTE)wc;
		} else {
			if (wc < 0x800) {	/* 2-byte sequence? */
				nc += 1;
				pb->buf[i++] = (BYTE)(0xC0 | wc >> 6);
			} else {			/* 3-byte sequence */
				nc += 2;
				pb->buf[i++] = (BYTE)(0xE0 | wc >> 12);
				pb->buf[i++] = (BYTE)(0x80 | (wc >> 6 & 0x3F));
			}
			pb->buf[i++] = (BYTE)(0x80 | (wc & 0x3F));
		}
	}
#else						/* Write a code point in ANSI/OEM */
	if (hs != 0) return;
	wc = ff_uni2oem(wc, CODEPAGE);	/* UTF-16 ==> ANSI/OEM */
	if (wc == 0) return;
	if (wc >= 0x100) {
		pb->buf[i++] = (BYTE)(wc >> 8); nc++;
	}
	pb->buf[i++] = (BYTE)wc;
#endif

#else									/* ANSI/OEM input (without re-encoding) */
	pb->buf[i++] = (BYTE)c;
#endif

	if (i >= (int)(sizeof pb->buf) - 4) {	/* Write buffered characters to the file */
		f_write(pb->fp, pb->buf, (UINT)i, &n);
		i = (n == (UINT)i) ? 0 : -1;
	}
	pb->idx = i;
	pb->nchr = nc + 1;
}


/* Flush remaining characters in the buffer */

static int putc_flush (putbuff* pb)
{
	UINT nw;

	if (   pb->idx >= 0	/* Flush buffered characters to the file */
		&& f_write(pb->fp, pb->buf, (UINT)pb->idx, &nw) == FR_OK
		&& (UINT)pb->idx == nw) return pb->nchr;
	return EOF;
}


/* Initialize write buffer */

static void putc_init (putbuff* pb, FIL* fp)
{
	mem_set(pb, 0, sizeof (putbuff));
	pb->fp = fp;
}



int f_putc (
	TCHAR c,	/* A character to be output */
	FIL* fp		/* Pointer to the file object */
)
{
	putbuff pb;


	putc_init(&pb, fp);
	putc_bfd(&pb, c);	/* Put the character */
	return putc_flush(&pb);
}




/*-----------------------------------------------------------------------*/
/* Put a String to the File                                              */
/*-----------------------------------------------------------------------*/

int f_puts (
	const TCHAR* str,	/* Pointer to the string to be output */
	FIL* fp				/* Pointer to the file object */
)
{
	putbuff pb;


	putc_init(&pb, fp);
	while (*str) putc_bfd(&pb, *str++);		/* Put the string */
	return putc_flush(&pb);
}




/*-----------------------------------------------------------------------*/
/* Put a Formatted String to the File                                    */
/*-----------------------------------------------------------------------*/

int f_printf (
	FIL* fp,			/* Pointer to the file object */
	const TCHAR* fmt,	/* Pointer to the format string */
	...					/* Optional arguments... */
)
{
	va_list arp;
	putbuff pb;
	BYTE f, r;
	UINT i, j, w;
	DWORD v;
	TCHAR c, d, str[32], *p;


	putc_init(&pb, fp);

	va_start(arp, fmt);

	for (;;) {
		c = *fmt++;
		if (c == 0) break;			/* End of string */
		if (c != '%') {				/* Non escape character */
			putc_bfd(&pb, c);
			continue;
		}
		w = f = 0;
		c = *fmt++;
		if (c == '0') {				/* Flag: '0' padding */
			f = 1; c = *fmt++;
		} else {
			if (c == '-') {			/* Flag: left justified */
				f = 2; c = *fmt++;
			}
		}
		if (c == '*') {				/* Minimum width by argument */
			w = va_arg(arp, int);
			c = *fmt++;
		} else {
			while (IsDigit(c)) {	/* Minimum width */
				w = w * 10 + c - '0';
				c = *fmt++;
			}
		}
		if (c == 'l' || c == 'L') {	/* Type prefix: Size is long int */
			f |= 4; c = *fmt++;
		}
		if (c == 0) break;
		d = c;
		if (IsLower(d)) d -= 0x20;
		switch (d) {				/* Atgument type is... */
		case 'S' :					/* String */
			p = va_arg(arp, TCHAR*);
			for (j = 0; p[j]; j++) ;
			if (!(f & 2)) {						/* Right padded */
				while (j++ < w) putc_bfd(&pb, ' ') ;
			}
			while (*p) putc_bfd(&pb, *p++) ;		/* String body */
			while (j++ < w) putc_bfd(&pb, ' ') ;	/* Left padded */
			continue;

		case 'C' :					/* Character */
			putc_bfd(&pb, (TCHAR)va_arg(arp, int)); continue;

		case 'B' :					/* Unsigned binary */
			r = 2; break;

		case 'O' :					/* Unsigned octal */
			r = 8; break;

		case 'D' :					/* Signed decimal */
		case 'U' :					/* Unsigned decimal */
			r = 10; break;

		case 'X' :					/* Unsigned hexdecimal */
			r = 16; break;

		default:					/* Unknown type (pass-through) */
			putc_bfd(&pb, c); continue;
		}

		/* Get an argument and put it in numeral */
		v = (f & 4) ? (DWORD)va_arg(arp, long) : ((d == 'D') ? (DWORD)(long)va_arg(arp, int) : (DWORD)va_arg(arp, unsigned int));
		if (d == 'D' && (v & 0x80000000)) {
			v = 0 - v;
			f |= 8;
		}
		i = 0;
		do {
			d = (TCHAR)(v % r); v /= r;
			if (d > 9) d += (c == 'x') ? 0x27 : 0x07;
			str[i++] = d + '0';
		} while (v && i < sizeof str / sizeof *str);
		if (f & 8) str[i++] = '-';
		j = i; d = (f & 1) ? '0' : ' ';
		if (!(f & 2)) {
			while (j++ < w) putc_bfd(&pb, d);	/* Right pad */
		}
		do {
			putc_bfd(&pb, str[--i]);			/* Number body */
		} while (i);
		while (j++ < w) putc_bfd(&pb, d);		/* Left pad */
	}

	va_end(arp);

	return putc_flush(&pb);
}

#endif /* !FF_FS_READONLY */
#endif /* FF_USE_STRFUNC */



#if FF_CODE_PAGE == 0
/*-----------------------------------------------------------------------*/
/* Set Active Codepage for the Path Name                                 */
/*-----------------------------------------------------------------------*/

FRESULT f_setcp (
	WORD cp		/* Value to be set as active code page */
)
{
	static const WORD       validcp[] = {  437,   720,   737,   771,   775,   850,   852,   857,   860,   861,   862,   863,   864,   865,   866,   869,   932,   936,   949,   950, 0};
	static const BYTE* const tables[] = {Ct437, Ct720, Ct737, Ct771, Ct775, Ct850, Ct852, Ct857, Ct860, Ct861, Ct862, Ct863, Ct864, Ct865, Ct866, Ct869, Dc932, Dc936, Dc949, Dc950, 0};
	UINT i;


	for (i = 0; validcp[i] != 0 && validcp[i] != cp; i++) ;	/* Find the code page */
	if (validcp[i] != cp) return FR_INVALID_PARAMETER;	/* Not found? */

	CodePage = cp;
	if (cp >= 900) {	/* DBCS */
		ExCvt = 0;
		DbcTbl = tables[i];
	} else {			/* SBCS */
		ExCvt = tables[i];
		DbcTbl = 0;
	}
	return FR_OK;
}
#endif	/* FF_CODE_PAGE == 0 */

