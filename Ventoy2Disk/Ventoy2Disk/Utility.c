/******************************************************************************
 * Utility.c
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
#include <Windows.h>
#include "Ventoy2Disk.h"

void Log(const char *Fmt, ...)
{
    va_list Arg;
    int Len = 0;
    FILE *File = NULL;
    SYSTEMTIME Sys;
    char szBuf[1024];

    GetLocalTime(&Sys);
    Len += safe_sprintf(szBuf,
        "[%4d/%02d/%02d %02d:%02d:%02d.%03d] ",
        Sys.wYear, Sys.wMonth, Sys.wDay,
        Sys.wHour, Sys.wMinute, Sys.wSecond,
        Sys.wMilliseconds);

    va_start(Arg, Fmt);
    Len += vsnprintf_s(szBuf + Len, sizeof(szBuf)-Len, sizeof(szBuf)-Len, Fmt, Arg);
    va_end(Arg);

    //printf("%s\n", szBuf);

#if 1
    fopen_s(&File, VENTOY_FILE_LOG, "a+");
    if (File)
    {
        fwrite(szBuf, 1, Len, File);
        fwrite("\n", 1, 1, File);
        fclose(File);
    }
#endif

}

BOOL IsPathExist(BOOL Dir, const char *Fmt, ...)
{
    va_list Arg;
    HANDLE hFile;
    DWORD Attr;
    CHAR FilePath[MAX_PATH];

    va_start(Arg, Fmt);
    vsnprintf_s(FilePath, sizeof(FilePath), sizeof(FilePath), Fmt, Arg);
    va_end(Arg);

    hFile = CreateFileA(FilePath, FILE_READ_EA, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == hFile)
    {
        return FALSE;
    }

    CloseHandle(hFile);

    Attr = GetFileAttributesA(FilePath);

    if (Dir)
    {
        if ((Attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            return FALSE;
        }
    }
    else
    {
        if (Attr & FILE_ATTRIBUTE_DIRECTORY)
        {
            return FALSE;
        }
    }

    return TRUE;
}


int ReadWholeFileToBuf(const CHAR *FileName, int ExtLen, void **Bufer, int *BufLen)
{
    int FileSize;
    FILE *File = NULL;
    void *Data = NULL;

    fopen_s(&File, FileName, "rb");
    if (File == NULL)
    {
        Log("Failed to open file %s", FileName);
        return 1;
    }

    fseek(File, 0, SEEK_END);
    FileSize = (int)ftell(File);

    Data = malloc(FileSize + ExtLen);
    if (!Data)
    {
        fclose(File);
        return 1;
    }

    fseek(File, 0, SEEK_SET);
    fread(Data, 1, FileSize, File);

    fclose(File);

    *Bufer = Data;
    *BufLen = FileSize;

    return 0;
}

const CHAR* GetLocalVentoyVersion(void)
{
    int rc;
    int FileSize;
    CHAR *Pos = NULL;
    CHAR *Buf = NULL;
    static CHAR LocalVersion[64] = { 0 };

    if (LocalVersion[0] == 0)
    {
        rc = ReadWholeFileToBuf(VENTOY_FILE_VERSION, 1, (void **)&Buf, &FileSize);
        if (rc)
        {
            return "";
        }
        Buf[FileSize] = 0;

        for (Pos = Buf; *Pos; Pos++)
        {
            if (*Pos == '\r' || *Pos == '\n')
            {
                *Pos = 0;
                break;
            }
        }

        safe_sprintf(LocalVersion, "%s", Buf);
        free(Buf);
    }
    
    return LocalVersion;
}

const CHAR* ParseVentoyVersionFromString(CHAR *Buf)
{
    CHAR *Pos = NULL;
    CHAR *End = NULL;
    static CHAR LocalVersion[64] = { 0 };

    Pos = strstr(Buf, "VENTOY_VERSION=");
    if (Pos)
    {
        Pos += strlen("VENTOY_VERSION=");
        if (*Pos == '"')
        {
            Pos++;
        }

        End = Pos;
        while (*End != 0 && *End != '"' && *End != '\r' && *End != '\n')
        {
            End++;
        }

        *End = 0;

        safe_sprintf(LocalVersion, "%s", Pos);
        return LocalVersion;
    }

    return "";
}

BOOL IsWow64(void)
{
    typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);
    LPFN_ISWOW64PROCESS fnIsWow64Process;
    BOOL bIsWow64 = FALSE;

    fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandleA("kernel32"), "IsWow64Process");
    if (NULL != fnIsWow64Process)
    {
        fnIsWow64Process(GetCurrentProcess(), &bIsWow64);
    }

    return bIsWow64;
}

void DumpWindowsVersion(void)
{
    int Bit; 
    BOOL WsVer;    
    DWORD Major, Minor;
    ULONGLONG MajorEqual, MinorEqual;
    OSVERSIONINFOEXA Ver1, Ver2;
    const CHAR *Ver = NULL; 
    CHAR WinVer[256] = { 0 };

    memset(&Ver1, 0, sizeof(Ver1));
    memset(&Ver2, 0, sizeof(Ver2));

    Ver1.dwOSVersionInfoSize = sizeof(Ver1);
    
    // suppress the C4996 warning for GetVersionExA
    #pragma warning(push)
    #pragma warning(disable:4996)
    if (!GetVersionExA((OSVERSIONINFOA *)&Ver1))
    {
        memset(&Ver1, 0, sizeof(Ver1));
        Ver1.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
        if (!GetVersionExA((OSVERSIONINFOA *)&Ver1))
        {
            return;
        }
    }
    #pragma warning(pop)

    if (Ver1.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
        if (Ver1.dwMajorVersion > 6 || (Ver1.dwMajorVersion == 6 && Ver1.dwMinorVersion >= 2))
        {
            // GetVersionEx() has problem on some Windows version 

            MajorEqual = VerSetConditionMask(0, VER_MAJORVERSION, VER_EQUAL);
            for (Major = Ver1.dwMajorVersion; Major <= 9; Major++) 
            {
                memset(&Ver2, 0, sizeof(Ver2));
                Ver2.dwOSVersionInfoSize = sizeof(Ver2);
                Ver2.dwMajorVersion = Major;

                if (!VerifyVersionInfoA(&Ver2, VER_MAJORVERSION, MajorEqual))
                {
                    continue;
                }
                    
                if (Ver1.dwMajorVersion < Major) 
                {
                    Ver1.dwMajorVersion = Major;
                    Ver1.dwMinorVersion = 0;
                }

                MinorEqual = VerSetConditionMask(0, VER_MINORVERSION, VER_EQUAL);
                for (Minor = Ver1.dwMinorVersion; Minor <= 9; Minor++) 
                {
                    memset(&Ver2, 0, sizeof(Ver2)); 
                    
                    Ver2.dwOSVersionInfoSize = sizeof(Ver2);
                    Ver2.dwMinorVersion = Minor;

                    if (!VerifyVersionInfoA(&Ver2, VER_MINORVERSION, MinorEqual))
                    {
                        continue;
                    }
                        
                    Ver1.dwMinorVersion = Minor;
                    break;
                }

                break;
            }
        }

        if (Ver1.dwMajorVersion <= 0xF && Ver1.dwMinorVersion <= 0xF)
        {
            WsVer = (Ver1.wProductType <= VER_NT_WORKSTATION);
            switch ((Ver1.dwMajorVersion << 4) | Ver2.dwMinorVersion)
            {
                case 0x51:
                {
                    Ver = "XP";
                    break;
                }
                case 0x52:
                {
                    Ver = GetSystemMetrics(89) ? "Server 2003 R2" : "Server 2003";
                    break;
                }
                case 0x60:
                {
                    Ver = WsVer ? "Vista" : "Server 2008";
                    break;
                }
                case 0x61:
                {
                    Ver = WsVer ? "7" : "Server 2008 R2";
                    break;
                }
                case 0x62:
                {
                    Ver = WsVer ? "8" : "Server 2012";
                    break;
                }
                case 0x63:
                {
                    Ver = WsVer ? "8.1" : "Server 2012 R2";
                    break;
                }
                case 0x64:
                {
                    Ver = WsVer ? "10 (Preview 1)" : "Server 10 (Preview 1)";
                    break;
                }
                case 0xA0:
                {
                    Ver = WsVer ? "10" : ((Ver1.dwBuildNumber > 15000) ? "Server 2019" : "Server 2016");
                    break;
                }
                default:
                {
                    Ver = "10 or later";
                    break;
                }
            }
        }
    }

    Bit = IsWow64() ? 64 : 32;

    if (Ver1.wServicePackMinor)
    {
        safe_sprintf(WinVer, "Windows %s SP%u.%u %d-bit", Ver, Ver1.wServicePackMajor, Ver1.wServicePackMinor, Bit);
    }
    else if (Ver1.wServicePackMajor)
    {
        safe_sprintf(WinVer, "Windows %s SP%u %d-bit", Ver, Ver1.wServicePackMajor, Bit);
    }
    else
    {
        safe_sprintf(WinVer, "Windows %s %d-bit", Ver, Bit);
    }

    if (((Ver1.dwMajorVersion << 4) | Ver2.dwMinorVersion) >= 0x62)
    {
        Log("Windows Version : %s (Build %u)", WinVer, Ver1.dwBuildNumber);
    }
    else
    {
        Log("Windows Version : %s", WinVer);
    }

    return;
}

BOOL IsVentoyLogicalDrive(CHAR DriveLetter)
{
    int i;
    CONST CHAR *Files[] =
    {
        "EFI\\BOOT\\BOOTX64.EFI",
        "grub\\themes\\ventoy\\theme.txt",
        "ventoy\\ventoy.cpio",
    };

    for (i = 0; i < sizeof(Files) / sizeof(Files[0]); i++)
    {
        if (!IsFileExist("%C:\\%s", DriveLetter, Files[i]))
        {
            return FALSE;
        }
    }

    return TRUE;
}


static int VentoyFillLocation(UINT64 DiskSizeInBytes, UINT32 StartSectorId, UINT32 SectorCount, PART_TABLE *Table)
{
    BYTE Head;
    BYTE Sector;
    BYTE nSector = 63;
    BYTE nHead = 8;    
    UINT32 Cylinder;
    UINT32 EndSectorId;

    while (nHead != 0 && (DiskSizeInBytes / 512 / nSector / nHead) > 1024)
    {
        nHead = (BYTE)nHead * 2;
    }

    if (nHead == 0)
    {
        nHead = 255;
    }

    Cylinder = StartSectorId / nSector / nHead;
    Head = StartSectorId / nSector % nHead;
    Sector = StartSectorId % nSector + 1;

    Table->StartHead = Head;
    Table->StartSector = Sector;
    Table->StartCylinder = Cylinder;

    EndSectorId = StartSectorId + SectorCount - 1;
    Cylinder = EndSectorId / nSector / nHead;
    Head = EndSectorId / nSector % nHead;
    Sector = EndSectorId % nSector + 1;

    Table->EndHead = Head;
    Table->EndSector = Sector;
    Table->EndCylinder = Cylinder;

    Table->StartSectorId = StartSectorId;
    Table->SectorCount = SectorCount;

    return 0;
}

int VentoyFillMBR(UINT64 DiskSizeBytes, MBR_HEAD *pMBR)
{
    GUID Guid;
	int ReservedValue;
    UINT32 DiskSignature;
    UINT32 DiskSectorCount;
    UINT32 PartSectorCount;
    UINT32 PartStartSector;
	UINT32 ReservedSector;

    VentoyGetLocalBootImg(pMBR);

    CoCreateGuid(&Guid);

    memcpy(&DiskSignature, &Guid, sizeof(UINT32));

    Log("Disk signature: 0x%08x", DiskSignature);

    *((UINT32 *)(pMBR->BootCode + 0x1B8)) = DiskSignature;

    DiskSectorCount = (UINT32)(DiskSizeBytes / 512);

	ReservedValue = GetReservedSpaceInMB();
	if (ReservedValue <= 0)
	{
		ReservedSector = 0;
	}
	else
	{
		ReservedSector = (UINT32)(ReservedValue * 2048);
	}

	Log("ReservedSector: %u", ReservedSector);

    //Part1
    PartStartSector = VENTOY_PART1_START_SECTOR;
	PartSectorCount = DiskSectorCount - ReservedSector - VENTOY_EFI_PART_SIZE / 512 - PartStartSector;
    VentoyFillLocation(DiskSizeBytes, PartStartSector, PartSectorCount, pMBR->PartTbl);

    pMBR->PartTbl[0].Active = 0x80; // bootable
    pMBR->PartTbl[0].FsFlag = 0x07; // exFAT/NTFS/HPFS

    //Part2
    PartStartSector += PartSectorCount;
    PartSectorCount = VENTOY_EFI_PART_SIZE / 512;
    VentoyFillLocation(DiskSizeBytes, PartStartSector, PartSectorCount, pMBR->PartTbl + 1);

    pMBR->PartTbl[1].Active = 0x00; 
    pMBR->PartTbl[1].FsFlag = 0xEF; // EFI System Partition

    pMBR->Byte55 = 0x55;
    pMBR->ByteAA = 0xAA;

    return 0;
}

CHAR GetFirstUnusedDriveLetter(void)
{
    CHAR Letter = 'D';
    DWORD Drives = GetLogicalDrives();

    Drives >>= 3;
    while (Drives & 0x1)
    {
        Letter++;
        Drives >>= 1;
    }

    return Letter;
}

const CHAR * GetBusTypeString(STORAGE_BUS_TYPE Type)
{
    switch (Type)
    {
        case BusTypeUnknown: return "unknown";
        case BusTypeScsi: return "SCSI";
        case BusTypeAtapi: return "Atapi";
        case BusTypeAta: return "ATA";
        case BusType1394: return "1394";
        case BusTypeSsa: return "SSA";
        case BusTypeFibre: return "Fibre";
        case BusTypeUsb: return "USB";
        case BusTypeRAID: return "RAID";
        case BusTypeiScsi: return "iSCSI";
        case BusTypeSas: return "SAS";
        case BusTypeSata: return "SATA";
        case BusTypeSd: return "SD";
        case BusTypeMmc: return "MMC";
        case BusTypeVirtual: return "Virtual";
        case BusTypeFileBackedVirtual: return "FileBackedVirtual";
        case BusTypeSpaces: return "Spaces";
        case BusTypeNvme: return "Nvme";
    }
    return "unknown";
}

int VentoyGetLocalBootImg(MBR_HEAD *pMBR)
{
    int Len = 0;
    BYTE *ImgBuf = NULL;
    static int Loaded = 0;
    static MBR_HEAD MBR;

    if (Loaded)
    {
        memcpy(pMBR, &MBR, 512);
        return 0;
    }

    if (0 == ReadWholeFileToBuf(VENTOY_FILE_BOOT_IMG, 0, (void **)&ImgBuf, &Len))
    {
        Log("Copy boot img success");
        memcpy(pMBR, ImgBuf, 512);
        free(ImgBuf);
        
        CoCreateGuid((GUID *)(pMBR->BootCode + 0x180));

        memcpy(&MBR, pMBR, 512);
        Loaded = 1;

        return 0;
    }
    else
    {
        Log("Copy boot img failed");
        return 1;
    }
}

int GetHumanReadableGBSize(UINT64 SizeBytes)
{
    int i;
    int Pow2 = 1;
    double Delta;
    double GB = SizeBytes * 1.0 / 1000 / 1000 / 1000;

    for (i = 0; i < 12; i++)
    {
        if (Pow2 > GB)
        {
            Delta = (Pow2 - GB) / Pow2;
        }
        else
        {
            Delta = (GB - Pow2) / Pow2;
        }

        if (Delta < 0.05)
        {
            return Pow2;
        }

        Pow2 <<= 1;
    }

    return (int)GB;
}

void TrimString(CHAR *String)
{
    CHAR *Pos1 = String;
    CHAR *Pos2 = String;
    size_t Len = strlen(String);

    while (Len > 0)
    {
        if (String[Len - 1] != ' ' && String[Len - 1] != '\t')
        {
            break;
        }
        String[Len - 1] = 0;
        Len--;
    }

    while (*Pos1 == ' ' || *Pos1 == '\t')
    {
        Pos1++;
    }

    while (*Pos1)
    {
        *Pos2++ = *Pos1++;
    }
    *Pos2++ = 0;

    return;
}

int GetRegDwordValue(HKEY Key, LPCSTR SubKey, LPCSTR ValueName, DWORD *pValue)
{
    HKEY hKey;
    DWORD Type;
    DWORD Size;
    LSTATUS lRet;
    DWORD Value;

    lRet = RegOpenKeyExA(Key, SubKey, 0, KEY_QUERY_VALUE, &hKey);
    Log("RegOpenKeyExA <%s> Ret:%ld", SubKey, lRet);

    if (ERROR_SUCCESS == lRet)
    {
        Size = sizeof(Value);
        lRet = RegQueryValueExA(hKey, ValueName, NULL, &Type, (LPBYTE)&Value, &Size);
        Log("RegQueryValueExA <%s> ret:%u  Size:%u Value:%u", ValueName, lRet, Size, Value);

        *pValue = Value;
        RegCloseKey(hKey);

        return 0;
    }
    else
    {
        return 1;
    }
}

int GetPhysicalDriveCount(void)
{
    DWORD Value;
    int Count = 0;

    if (GetRegDwordValue(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\disk\\Enum", "Count", &Value) == 0)
    {
        Count = (int)Value;
    }

    Log("GetPhysicalDriveCount: %d", Count);
    return Count;
}



