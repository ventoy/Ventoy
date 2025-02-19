/******************************************************************************
 * Ventoy2Disk.h
 *
 * Copyright (c) 2020, longpanda <admin@ventoy.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __VENTOY2DISK_H__
#define __VENTOY2DISK_H__

#include <stdio.h>

typedef enum VTOY_FS
{
    VTOY_FS_EXFAT = 0,
    VTOY_FS_NTFS,
    VTOY_FS_FAT32,
    VTOY_FS_UDF,
    VTOY_FS_BUTT
}VTOY_FS;

#define FAT32_MAX_LIMIT             (32 * 1073741824ULL)

#define VENTOY_EFI_PART_ATTR        0x8000000000000000ULL


#define SIZE_1KB					(1024)
#define SIZE_1GB					(1024 * 1024 * 1024)
#define SIZE_1TB					(1024ULL * 1024ULL * 1024ULL * 1024ULL)
#define SIZE_1MB                    (1024 * 1024)
#define SIZE_2MB                    (2048 * 1024)
#define VENTOY_EFI_PART_SIZE	    (32 * SIZE_1MB)
#define VENTOY_PART1_START_SECTOR    2048

#define VENTOY_FILE_BOOT_IMG    "boot\\boot.img"
#define VENTOY_FILE_STG1_IMG    "boot\\core.img.xz"
#define VENTOY_FILE_DISK_IMG    "ventoy\\ventoy.disk.img.xz"
#define VENTOY_FILE_LOG         "log.txt"
#define VENTOY_FILE_VERSION     "ventoy\\version"

#define VENTOY_CLI_LOG          "cli_log.txt"
#define VENTOY_CLI_PERCENT      "cli_percent.txt"
#define VENTOY_CLI_DONE         "cli_done.txt"


#define DRIVE_ACCESS_TIMEOUT        15000		// How long we should retry drive access (in ms)
#define DRIVE_ACCESS_RETRIES        150			// How many times we should retry

#define IsFileExist(Fmt, ...) IsPathExist(FALSE, Fmt, __VA_ARGS__)
#define IsDirExist(Fmt, ...)  IsPathExist(TRUE, Fmt, __VA_ARGS__)

#define safe_sprintf(dst, fmt, ...) sprintf_s(dst, sizeof(dst), fmt, __VA_ARGS__)
#define safe_strcpy(dst, src)  strcpy_s(dst, sizeof(dst), src)

#define CHECK_FREE(p) \
{\
    if (p)\
    {\
        free(p); \
        (p) = NULL; \
    }\
}

#define CHECK_CLOSE_HANDLE(Handle) \
{\
    if (Handle != INVALID_HANDLE_VALUE) \
    {\
        CloseHandle(Handle); \
        Handle = INVALID_HANDLE_VALUE; \
    }\
}

#define LASTERR     GetLastError()
#define RET_LASTERR (ret ? 0 : LASTERR)

#pragma pack(1)
typedef struct PART_TABLE
{
    UINT8  Active; // 0x00  0x80

    UINT8  StartHead;
    UINT16 StartSector : 6;
    UINT16 StartCylinder : 10;

    UINT8  FsFlag;

    UINT8  EndHead;
    UINT16 EndSector : 6;
    UINT16 EndCylinder : 10;

    UINT32 StartSectorId;
    UINT32 SectorCount;
}PART_TABLE;

typedef struct MBR_HEAD
{
    UINT8 BootCode[446];
    PART_TABLE PartTbl[4];
    UINT8 Byte55;
    UINT8 ByteAA;
}MBR_HEAD;

typedef struct VTOY_GPT_HDR
{
    CHAR   Signature[8]; /* EFI PART */
    UINT8  Version[4];
    UINT32 Length;
    UINT32 Crc;
    UINT8  Reserved1[4];
    UINT64 EfiStartLBA;
    UINT64 EfiBackupLBA;
    UINT64 PartAreaStartLBA;
    UINT64 PartAreaEndLBA;
    GUID   DiskGuid;
    UINT64 PartTblStartLBA;
    UINT32 PartTblTotNum;
    UINT32 PartTblEntryLen;
    UINT32 PartTblCrc;
    UINT8  Reserved2[420];
}VTOY_GPT_HDR;

typedef struct VTOY_GPT_PART_TBL
{
    GUID   PartType;
    GUID   PartGuid;
    UINT64 StartLBA;
    UINT64 LastLBA;
    UINT64 Attr;
    UINT16 Name[36];
}VTOY_GPT_PART_TBL;

typedef struct VTOY_GPT_INFO
{
    MBR_HEAD MBR;
    VTOY_GPT_HDR Head;
    VTOY_GPT_PART_TBL PartTbl[128];
}VTOY_GPT_INFO;


typedef struct ventoy_secure_data
{
    UINT8 magic1[16];     /* VENTOY_GUID */
    UINT8 diskuuid[16];
    UINT8 Checksum[16];
    UINT8 adminSHA256[32];
    UINT8 reserved[4000];
    UINT8 magic2[16];     /* VENTOY_GUID */
}ventoy_secure_data;


#pragma pack()

#define VENTOY_MAX_PHY_DRIVE  128

typedef struct PHY_DRIVE_INFO
{
    int Id;
    int PhyDrive;
    int PartStyle;//0:MBR 1:GPT
    UINT64 SizeInBytes;
    BYTE DeviceType;
    BOOL RemovableMedia;
    CHAR VendorId[128];
    CHAR ProductId[128];
    CHAR ProductRev[128];
    CHAR SerialNumber[128];
    STORAGE_BUS_TYPE BusType;

    DWORD BytesPerLogicalSector;
    DWORD BytesPerPhysicalSector;

    CHAR DriveLetters[64];
    
    int  VentoyFsClusterSize;
    CHAR VentoyFsType[16];
    CHAR VentoyVersion[32];

    BOOL SecureBootSupport;
    MBR_HEAD MBR;
    UINT64 Part2GPTAttr;

	BOOL ResizeNoShrink;
	UINT64 ResizeOldPart1Size;
	CHAR Part1DriveLetter;
    CHAR ResizeVolumeGuid[64];
	CHAR FsName[64];
	UINT64 ResizePart2StartSector;
	VTOY_GPT_INFO Gpt;

}PHY_DRIVE_INFO;

typedef enum PROGRESS_POINT
{
    PT_START = 0,
    PT_LOCK_FOR_CLEAN = 8,
    PT_DEL_ALL_PART,
    PT_LOCK_FOR_WRITE,
    PT_FORMAT_PART1,
    PT_LOCK_VOLUME = PT_FORMAT_PART1,
    PT_FORMAT_PART2,

    PT_WRITE_VENTOY_START,
    PT_WRITE_VENTOY_FINISH = PT_WRITE_VENTOY_START + 32,

    PT_WRITE_STG1_IMG,
    PT_WRITE_PART_TABLE,
    PT_MOUNT_VOLUME,

    PT_REFORMAT_START,
    PT_REFORMAT_FINISH = PT_REFORMAT_START + 16,

    PT_FINISH
}PROGRESS_POINT;

#define PROGRESS_BAR_SET_POS(pos)  SetProgressBarPos(pos)

extern PHY_DRIVE_INFO *g_PhyDriveList;
extern DWORD g_PhyDriveCount;
extern int g_ForceOperation;
extern int g_NoNeedInputYes;
extern HWND g_ProgressBarHwnd;
extern HFONT g_language_normal_font;
extern HFONT g_language_bold_font;
extern int g_FilterUSB;



void TraceOut(const char *Fmt, ...);
void Log(const char *Fmt, ...);
void LogCache(BOOL cache);
void LogFlush(void);
BOOL IsPathExist(BOOL Dir, const char *Fmt, ...);
void DumpWindowsVersion(void);
const CHAR* GetLocalVentoyVersion(void);
const CHAR* ParseVentoyVersionFromString(CHAR *Buf);
CHAR GetFirstUnusedDriveLetter(void);
const CHAR * GetBusTypeString(STORAGE_BUS_TYPE Type);
int VentoyGetLocalBootImg(MBR_HEAD *pMBR);
int GetHumanReadableGBSize(UINT64 SizeBytes);
void TrimString(CHAR *String);
int VentoyFillMBR(UINT64 DiskSizeBytes, MBR_HEAD *pMBR, int PartStyle, UINT8 FsFlag);
int VentoyFillGpt(UINT64 DiskSizeBytes, VTOY_GPT_INFO *pInfo);
BOOL IsVentoyLogicalDrive(CHAR DriveLetter);
int GetRegDwordValue(HKEY Key, LPCSTR SubKey, LPCSTR ValueName, DWORD *pValue);
int GetPhysicalDriveCount(void);
BOOL VentoyPhydriveMatch(PHY_DRIVE_INFO* pPhyDrive);
int GetAllPhysicalDriveInfo(PHY_DRIVE_INFO *pDriveList, DWORD *pDriveCount);
int GetPhyDriveByLogicalDrive(int DriveLetter, UINT64*Offset);
int GetVentoyVerInPhyDrive(const PHY_DRIVE_INFO *pDriveInfo, UINT64 Part2StartSector, CHAR *VerBuf, size_t BufLen, BOOL *pSecureBoot);
int Ventoy2DiskInit(void);
int Ventoy2DiskDestroy(void);
PHY_DRIVE_INFO * GetPhyDriveInfoById(int Id);
PHY_DRIVE_INFO * GetPhyDriveInfoByPhyDrive(int PhyDrive);
int ParseCmdLineOption(LPSTR lpCmdLine);
int InstallVentoy2PhyDrive(PHY_DRIVE_INFO *pPhyDrive, int PartStyle, int TryId);
int PartitionResizeForVentoy(PHY_DRIVE_INFO *pPhyDrive);
int UpdateVentoy2PhyDrive(PHY_DRIVE_INFO *pPhyDrive, int TryId);
int VentoyFillBackupGptHead(VTOY_GPT_INFO *pInfo, VTOY_GPT_HDR *pHead);
int VentoyFillWholeGpt(UINT64 DiskSizeBytes, VTOY_GPT_INFO *pInfo);
void SetProgressBarPos(int Pos);
int SaveBufToFile(const CHAR *FileName, const void *Buffer, int BufLen);
int ReadWholeFileToBuf(const CHAR *FileName, int ExtLen, void **Bufer, int *BufLen);
int INIT unxz(unsigned char *in, int in_size,
    int(*fill)(void *dest, unsigned int size),
    int(*flush)(void *src, unsigned int size),
    unsigned char *out, int *in_used,
    void(*error)(char *x));
void disk_io_set_param(HANDLE Handle, UINT64 SectorCount);
int GetVolumeClusterSize(char Drive);

extern BOOL g_InputYes;
INT_PTR CALLBACK YesDialogProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK PartDialogProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
int GetReservedSpaceInMB(void);
int IsPartNeed4KBAlign(void);
int GetVentoyFsType(void);
void SetVentoyFsType(int fs);
int GetClusterSize(void);
void SetClusterSize(int ClusterSize);
WCHAR* GetClusterSizeTip(void);
void FormatClusterSizeTip(int Size, WCHAR* pBuf, size_t len);
const char* GetVentoyFsName(void);
const char* GetVentoyFsNameByType(int fs);
CHAR* GetVentoyFsFmtNameByTypeA(int fs);
WCHAR* GetVentoyFsFmtNameByTypeW(int fs);
int FindProcessOccupyDisk(HANDLE hDrive, PHY_DRIVE_INFO *pPhyDrive);
int VentoyFillMBRLocation(UINT64 DiskSizeInBytes, UINT32 StartSectorId, UINT32 SectorCount, PART_TABLE *Table);
int ClearVentoyFromPhyDrive(HWND hWnd, PHY_DRIVE_INFO *pPhyDrive, char *pDrvLetter);
UINT32 VentoyCrc32(void *Buffer, UINT32 Length);
BOOL PartResizePreCheck(PHY_DRIVE_INFO** ppPhyDrive);

#define SET_FILE_POS(pos) \
    liCurrentPosition.QuadPart = pos; \
    SetFilePointerEx(hDrive, liCurrentPosition, &liCurrentPosition, FILE_BEGIN)\

extern int g_WriteImage;

#define VTSI_IMG_MAGIC 0x0000594F544E4556ULL  // "VENTOY\0\0"

#pragma pack(1)

/*
 +---------------------------------
 + sector 0 ~ sector N-1
 +     data area
 +---------------------------------
 + sector N ~ 
 +     segment[0]
 +     segment[1]
 +     segment[2]
 +      ......
 +     segment[M-1]
 +     align data (aligned with 512)
 +---------------------------------
 +     footer
 +---------------------------------
 *
 * All the integers are in little endian
 * The sector size is fixed 512 for ventoy image file.
 *
 */

#define VTSI_IMG_MAX_SEG   128

typedef struct {
    UINT64 disk_start_sector;
    UINT64 sector_num;
    UINT64 data_offset;
}VTSI_SEGMENT;

typedef struct {
    UINT64 magic;
    UINT32 version;
    UINT64 disk_size;
    UINT32 disk_signature;
    UINT32 foot_chksum;

    UINT32 segment_num;
    UINT32 segment_chksum;
    UINT64 segment_offset;

    UINT8  reserved[512 - 44];
}VTSI_FOOTER;
#pragma pack()
extern int __static_assert__[sizeof(VTSI_FOOTER) == 512 ? 1 : -1];

extern HWND g_DialogHwnd;

extern BOOL g_CLI_Mode;

#define SAFE_FREE(ptr) if (ptr) { free(ptr); (ptr) = NULL; }
int InstallVentoy2FileImage(PHY_DRIVE_INFO *pPhyDrive, int PartStyle);
void disk_io_set_imghook(FILE *fp, VTSI_SEGMENT *segment, int maxseg, UINT64 data_offset);
void disk_io_reset_imghook(int *psegnum, UINT64 *pDataOffset);

HANDLE GetPhysicalHandle(int Drive, BOOLEAN bLockDrive, BOOLEAN bWriteAccess, BOOLEAN bWriteShare);
void InitComboxCtrl(HWND hWnd, int PhyDrive);
int disk_io_is_write_error(void);
void disk_io_reset_write_error(void);
const char* GUID2String(void* guid, char* buf, int len);
void VentoyStringToUpper(CHAR* str);
BOOL AlertSuppressInit(void);
void SetAlertPromptHookEnable(BOOL enable);
int VentoyCLIMain(int argc, char** argv);
BOOL IsVentoyPhyDrive(int PhyDrive, UINT64 SizeBytes, MBR_HEAD* pMBR, UINT64* Part2StartSector, UINT64* GptPart2Attr);
int GetVentoyFsNameInPhyDrive(PHY_DRIVE_INFO* CurDrive);
void CLISetReserveSpace(int MB);
void CLI_UpdatePercent(int Pos);
int GetLettersBelongPhyDrive(int PhyDrive, char* DriveLetters, size_t Length);
PHY_DRIVE_INFO* CLI_PhyDrvInfo(void);

#define UTF8_Log(fmt, wstr) \
{\
    memset(TmpPathA, 0, sizeof(TmpPathA));\
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, TmpPathA, sizeof(TmpPathA), NULL, NULL);\
    Log(fmt, TmpPathA);\
}

#define VTSI_SUPPORT 1


#define WM_OFFSET        (WM_USER + 40)
#define WM_WIDTH_CHANGE  (WM_OFFSET + 1)


int ExpandDlg(HWND hParent, UINT uiID, int WidthDelta);
int MoveDlg(HWND hParent, UINT uiID, int WidthDelta);

#endif
