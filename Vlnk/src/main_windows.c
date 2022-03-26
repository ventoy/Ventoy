#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <resource.h>
#include <vlnk.h>

static WCHAR g_CurDirW[MAX_PATH];
static CHAR g_CurDirA[MAX_PATH];
static CHAR g_LogFile[MAX_PATH];
static HWND g_create_button;
static HWND g_parse_button;

static BOOL g_ShowHelp = FALSE;
static BOOL g_SaveAs = FALSE;
static WCHAR g_CmdInFile[MAX_PATH];
static WCHAR g_CmdOutFile[MAX_PATH];

typedef enum MSGID
{
    MSGID_ERROR = 0,
	MSGID_INFO,
	MSGID_BTN_CREATE,
	MSGID_BTN_PARSE,
    MSGID_SRC_NONEXIST,
    MSGID_SRC_UNSUPPORTED,
    MSGID_FS_UNSUPPORTED,
    MSGID_SUFFIX_UNSUPPORTED,
    MSGID_DISK_INFO_ERR,
    MSGID_VLNK_SUCCESS,
    MSGID_RUNNING_TIP,
    MSGID_CREATE_FILE_ERR,
    MSGID_ALREADY_VLNK,
    MSGID_INVALID_VLNK,
    MSGID_VLNK_POINT_TO,
    MSGID_VLNK_NO_DST,
    MSGID_FILE_NAME_TOO_LONG,
    MSGID_INVALID_SUFFIX,

    MSGID_BUTT
}MSGID;


const WCHAR *g_msg_cn[MSGID_BUTT] =
{
    L"错误",
	L"提醒",
	L"创建",
	L"解析",	
    L"指定的文件不存在", 
    L"不支持为此文件创建vlnk",
    L"不支持的文件系统",
    L"不支持的文件后缀名",
    L"获取磁盘信息时发生错误",
    L"Vlnk 文件创建成功。",
    L"请先关闭正在运行的 VentoyVlnk 程序！",
    L"创建文件失败",
    L"此文件已经是一个vlnk文件了！",
    L"非法的vlnk文件!",
    L"此 vlnk 文件指向 ",
    L"此 vlnk 指向的文件不存在！",
    L"文件路径太长！",
    L"非法的vlnk文件后缀名!",
};
const WCHAR *g_msg_en[MSGID_BUTT] =
{
    L"Error",
	L"Info",   
	L"Create",
	L"Parse",
    L"The specified file is not exist!",
    L"This file is not supported for vlnk",
    L"Unsupported file system!", 
    L"Unsupported file suffix!",
    L"Error when getting disk info",
    L"Vlnk file successfully created!",
    L"Please close another running VentoyVlnk instance!",
    L"Failed to create file!",
    L"This file is already a vlnk file!",
    L"Invalid vlnk file!",
    L"The vlnk file point to ",
    L"The file pointed by the vlnk does NOT exist!",
    L"The file full path is too long!",
    L"Invalid vlnk file suffix!",
};

const WCHAR **g_msg_lang = NULL;

HINSTANCE g_hInst;

static int VtoyMessageBox
(
    _In_opt_ HWND hWnd,
    _In_opt_ LPCWSTR lpText,
    _In_opt_ LPCWSTR lpCaption,
    _In_ UINT uType
)
{
    if (g_CmdInFile[0] && g_CmdOutFile[0])
    {
        return 0;
    }

    return MessageBox(hWnd, lpText, lpCaption, uType);
}

static void Log2File(const char *log)
{
    time_t stamp;
    struct tm ttm;
    FILE *fp;

    time(&stamp);
    localtime_s(&ttm, &stamp);

    fopen_s(&fp, g_LogFile, "a+");
    if (fp)
    {
        fprintf_s(fp, "[%04u/%02u/%02u %02u:%02u:%02u] %s",
            ttm.tm_year + 1900, ttm.tm_mon + 1, ttm.tm_mday,
            ttm.tm_hour, ttm.tm_min, ttm.tm_sec, log);
        fclose(fp);
    }
}

void LogW(const WCHAR *Fmt, ...)
{
    WCHAR log[512];
    CHAR  alog[2048];
    va_list arg;

    if (g_LogFile[0] == 0)
    {
        return;
    }

    va_start(arg, Fmt);
    vswprintf_s(log, 512, Fmt, arg);
    va_end(arg);

    WideCharToMultiByte(CP_UTF8, 0, log, -1, alog, 2048, 0, 0);

    Log2File(alog);
}


void LogA(const CHAR *Fmt, ...)
{
    CHAR log[512];
    va_list arg;

    if (g_LogFile[0] == 0)
    {
        return;
    }

    va_start(arg, Fmt);
    vsprintf_s(log, 512, Fmt, arg);
    va_end(arg);

    Log2File(log);
}

static int Utf8ToUtf16(const char* src, WCHAR * dst)
{
    int size = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, 0);
    return MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, size + 1);
}

static BOOL OnDestroyDialog()
{    
    return TRUE;
}


static BOOL InitDialog(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    HICON hIcon;

    g_create_button = GetDlgItem(hWnd, IDC_BUTTON1);
    g_parse_button = GetDlgItem(hWnd, IDC_BUTTON2);
	
	hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON1));
    SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

    SetWindowTextW(g_create_button, g_msg_lang[MSGID_BTN_CREATE]);
    SetWindowTextW(g_parse_button, g_msg_lang[MSGID_BTN_PARSE]);

    return TRUE;
}

static int GetPhyDiskInfo(const char LogicalDrive, UINT32 *DiskSig, DISK_EXTENT *DiskExtent)
{
    BOOL Ret;
    DWORD dwSize;
    HANDLE Handle;
    VOLUME_DISK_EXTENTS DiskExtents;
    CHAR PhyPath[128];
    UINT8 SectorBuf[512];

    LogA("GetPhyDiskInfo %C\n", LogicalDrive);

    sprintf_s(PhyPath, sizeof(PhyPath), "\\\\.\\%C:", LogicalDrive);
    Handle = CreateFileA(PhyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        LogA("Could not open the disk %C: error:%u\n", LogicalDrive, GetLastError());
        return 1;
    }

    Ret = DeviceIoControl(Handle,
        IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
        NULL,
        0,
        &DiskExtents,
        (DWORD)(sizeof(DiskExtents)),
        (LPDWORD)&dwSize,
        NULL);
    if (!Ret || DiskExtents.NumberOfDiskExtents == 0)
    {
        LogA("DeviceIoControl IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS failed, error:%u\n", GetLastError());
        CloseHandle(Handle);
        return 1;
    }
    CloseHandle(Handle);

    memcpy(DiskExtent, DiskExtents.Extents, sizeof(DISK_EXTENT));
    LogA("%C: is in PhysicalDrive%d Offset:%llu\n", LogicalDrive, DiskExtents.Extents[0].DiskNumber,
        (ULONGLONG)(DiskExtents.Extents[0].StartingOffset.QuadPart));

    sprintf_s(PhyPath, sizeof(PhyPath), "\\\\.\\PhysicalDrive%d", DiskExtents.Extents[0].DiskNumber);
    Handle = CreateFileA(PhyPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        LogA("Could not open the disk<PhysicalDrive%d>, error:%u\n", DiskExtents.Extents[0].DiskNumber, GetLastError());
        return 1;
    }

    if (!ReadFile(Handle, SectorBuf, sizeof(SectorBuf), &dwSize, NULL))
    {
        LogA("ReadFile failed, dwSize:%u  error:%u\n", dwSize, GetLastError());
        CloseHandle(Handle);
        return 1;
    }

    memcpy(DiskSig, SectorBuf + 0x1B8, 4);

    CloseHandle(Handle);
    return 0;
}


static int SaveBuffer2File(const WCHAR *Fullpath, void *Buffer, DWORD Length)
{
    int rc = 1;
    DWORD dwSize;
    HANDLE Handle;

    LogW(L"SaveBuffer2File <%ls> len:%u\n", Fullpath, Length);

    Handle = CreateFileW(Fullpath, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, 0, CREATE_ALWAYS, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        LogA("Could not create new file, error:%u\n", GetLastError());
        goto End;
    }

    WriteFile(Handle, Buffer, Length, &dwSize, NULL);

    rc = 0;

End:

    if (Handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(Handle);
    }


    return rc;
}

static int DefaultVlnkDstFullPath(WCHAR *Src, WCHAR *Dir, WCHAR *Dst)
{
    int i, j;
    int len;
    int wrlen;
    WCHAR C;

    len = (int)lstrlen(Src);
    for (i = len - 1; i >= 0; i--)
    {
        if (Src[i] == '.')
        {
            C = Src[i];
            Src[i] = 0;
            wrlen = swprintf_s(Dst, MAX_PATH, L"%ls\\%ls.vlnk.%ls", Dir, Src, Src + i + 1);
            Src[i] = C;

            for (j = wrlen - (len - i); j < wrlen; j++)
            {
                if (Dst[j] >= 'A' && Dst[j] <= 'Z')
                {
                    Dst[j] = 'a' + (Dst[j] - 'A');
                }
            }

            break;
        }
    }

    return 0;
}

static BOOL IsVlnkFile(WCHAR *path, ventoy_vlnk *outvlnk)
{
    BOOL bRet;
    BOOL bVlnk = FALSE;
    DWORD dwSize;
    LARGE_INTEGER FileSize;
    HANDLE Handle;
    ventoy_vlnk vlnk;

    Handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        LogA("Could not open this file, error:%u\n", GetLastError());
        return FALSE;
    }

    if (!GetFileSizeEx(Handle, &FileSize))
    {
        LogA("Failed to get vlnk file size\n");
        goto End;
    }

    if (FileSize.QuadPart != VLNK_FILE_LEN)
    {
        LogA("Invalid vlnk file length %llu\n", (unsigned long long)FileSize.QuadPart);
        goto End;
    }

    memset(&vlnk, 0, sizeof(vlnk));
    bRet = ReadFile(Handle, &vlnk, sizeof(vlnk), &dwSize, NULL);
    if (bRet && CheckVlnkData(&vlnk))
    {
        if (outvlnk)
        {
            memcpy(outvlnk, &vlnk, sizeof(vlnk));
        }

        bVlnk = TRUE;
    }

End:

    if (Handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(Handle);
    }

    return bVlnk;
}


static BOOL VentoyGetSaveFileName(HWND hWnd, WCHAR *szFile)
{
    OPENFILENAME ofn = { 0 };

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"Vlnk File\0*.vlnk.iso;*.vlnk.img;*.vlnk.wim;*.vlnk.efi;*.vlnk.vhd;*.vlnk.vhdx;*.vlnk.vtoy;*.vlnk.dat\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;

    return GetSaveFileName(&ofn);
}

static BOOL IsSupportedVlnkSuffix(WCHAR *FileName)
{
    int len;

    len = lstrlen(FileName);

    if (len > 9)
    {
        if (lstrcmp(FileName + len - 9, L".vlnk.iso") == 0 ||
            lstrcmp(FileName + len - 9, L".vlnk.img") == 0 ||
            lstrcmp(FileName + len - 9, L".vlnk.wim") == 0 ||
            lstrcmp(FileName + len - 9, L".vlnk.vhd") == 0 ||
            lstrcmp(FileName + len - 9, L".vlnk.efi") == 0 ||
            lstrcmp(FileName + len - 9, L".vlnk.dat") == 0)
        {
            return TRUE;
        }
    }


    if (len > 10)
    {
        if (lstrcmp(FileName + len - 10, L".vlnk.vhdx") == 0 ||
            lstrcmp(FileName + len - 10, L".vlnk.vtoy") == 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static int CreateVlnk(HWND hWnd, WCHAR *Dir, WCHAR *InFile, WCHAR *OutFile)
{
    int i;
    int end;
    int len;
    BOOL SetOutFile = FALSE;
    UINT32 DiskSig;
    DISK_EXTENT DiskExtend;
    OPENFILENAME ofn = { 0 };
    CHAR UTF8Path[MAX_PATH];
    WCHAR DstFullPath[MAX_PATH];
    WCHAR szFile[MAX_PATH] = { 0 };
    CHAR suffix[8] = { 0 };
    CHAR Drive[8] = { 0 };
    CHAR FsName[64] = { 0 };
    CHAR *Buf = NULL;
    WCHAR *Pos = NULL;
    ventoy_vlnk *vlnk = NULL;

    if (InFile)
    {
        wcscpy_s(szFile, MAX_PATH, InFile);
    }
    else
    {
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hWnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = L"Vlnk Source File\0*.iso;*.img;*.wim;*.vhd;*.vhdx;*.vtoy;*.efi;*.dat\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (GetOpenFileName(&ofn) != TRUE)
        {
            return 1;
        }
    }

    LogW(L"Create vlnk for <%ls>\n", szFile);
    
    len = lstrlen(szFile);

    if (len < 5 || szFile[0] == '.' || szFile[1] != ':')
    {
        VtoyMessageBox(hWnd, g_msg_lang[MSGID_SRC_UNSUPPORTED], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
        return 1;
    }
    
    Drive[0] = (CHAR)szFile[0];
    Drive[1] = ':';
    Drive[2] = '\\';
    if (0 == GetVolumeInformationA(Drive, NULL, 0, NULL, NULL, NULL, FsName, sizeof(FsName) - 1))
    {
        LogA("GetVolumeInformationA failed %u\n", GetLastError());
        return 1;
    }

    LogA("Partition filesystem of <%s> is <%s>\n", Drive, FsName);
    if (_stricmp(FsName, "NTFS") == 0 ||
        _stricmp(FsName, "exFAT") == 0 ||
        _stricmp(FsName, "FAT") == 0 ||
        _stricmp(FsName, "FAT32") == 0 ||
        _stricmp(FsName, "FAT16") == 0 ||
        _stricmp(FsName, "FAT12") == 0 ||
        _stricmp(FsName, "UDF") == 0)
    {
        LogA("FS Check OK\n");
    }
    else
    {
        VtoyMessageBox(hWnd, g_msg_lang[MSGID_FS_UNSUPPORTED], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
        return 1;
    }


    end = (szFile[len - 5] == '.') ? 5 : 4;
    for (i = 0; i < end; i++)
    {
        suffix[i] = (CHAR)szFile[len - (end - i)];
    }

    if (!IsSupportedImgSuffix(suffix))
    {
        VtoyMessageBox(hWnd, g_msg_lang[MSGID_SUFFIX_UNSUPPORTED], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
        return 1;
    }

    if (IsVlnkFile(szFile, NULL))
    {
        VtoyMessageBox(hWnd, g_msg_lang[MSGID_ALREADY_VLNK], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
        return 1;
    }

    for (i = 0; i < MAX_PATH && szFile[i]; i++)
    {
        if (szFile[i] == '\\' || szFile[i] == '/')
        {
            Pos = szFile + i;
        }
    }

    if (!Pos)
    {
        LogA("name part not found\n");
        return 1;
    }
    LogW(L"File Name is <%ls>\n", Pos + 1);

    memset(UTF8Path, 0, sizeof(UTF8Path));
    WideCharToMultiByte(CP_UTF8, 0, szFile + 2, -1, UTF8Path, MAX_PATH, NULL, 0);

    for (i = 0; i < MAX_PATH && UTF8Path[i]; i++)
    {
        if (UTF8Path[i] == '\\')
        {
            UTF8Path[i] = '/';
        }
    }

    len = (int)strlen(UTF8Path);
    if (len >= VLNK_NAME_MAX)
    {
        LogA("File name length %d overflow\n", len);
        VtoyMessageBox(hWnd, g_msg_lang[MSGID_FILE_NAME_TOO_LONG], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
        return 1;
    }

    DiskExtend.StartingOffset.QuadPart = 0;
    if (GetPhyDiskInfo((char)szFile[0], &DiskSig, &DiskExtend))
    {
        VtoyMessageBox(hWnd, g_msg_lang[MSGID_DISK_INFO_ERR], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
        return 1;
    }


    Buf = malloc(VLNK_FILE_LEN);
    if (Buf)
    {
        memset(Buf, 0, VLNK_FILE_LEN);
        vlnk = (ventoy_vlnk *)Buf;
        ventoy_create_vlnk(DiskSig, (uint64_t)DiskExtend.StartingOffset.QuadPart, UTF8Path, vlnk);

        if (OutFile)
        {
            wcscpy_s(DstFullPath, MAX_PATH, OutFile);
            SetOutFile = TRUE;
        }
        else
        {
            DefaultVlnkDstFullPath(Pos + 1, Dir, DstFullPath);

            if (g_SaveAs)
            {
                wcscpy_s(szFile, MAX_PATH, DstFullPath);
                if (VentoyGetSaveFileName(hWnd, szFile))
                {
                    if (IsSupportedVlnkSuffix(szFile))
                    {
                        wcscpy_s(DstFullPath, MAX_PATH, szFile);
                        SetOutFile = TRUE;
                    }
                    else
                    {
                        VtoyMessageBox(hWnd, g_msg_lang[MSGID_INVALID_SUFFIX], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
                        LogA("Invalid vlnk suffix\n");
                        goto end;
                    }
                }
                else
                {
                    LogA("User cancel the save as diaglog, use default name\n");
                }
            }
        }

        LogW(L"vlnk output file path is <%ls>\n", DstFullPath);

        if (SaveBuffer2File(DstFullPath, Buf, VLNK_FILE_LEN) == 0)
        {
            WCHAR Msg[1024];

            LogW(L"Vlnk file create success <%ls>\n", DstFullPath);

            if (SetOutFile)
            {
                swprintf_s(Msg, 1024, L"%ls\r\n\r\n%ls", g_msg_lang[MSGID_VLNK_SUCCESS], DstFullPath);
                VtoyMessageBox(hWnd, Msg, g_msg_lang[MSGID_INFO], MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                swprintf_s(Msg, 1024, L"%ls\r\n\r\n%ls", g_msg_lang[MSGID_VLNK_SUCCESS], DstFullPath + lstrlen(Dir) + 1);
                VtoyMessageBox(hWnd, Msg, g_msg_lang[MSGID_INFO], MB_OK | MB_ICONINFORMATION);
            }
        }
        else
        {
            LogA("Vlnk file save failed\n");
            VtoyMessageBox(hWnd, g_msg_lang[MSGID_CREATE_FILE_ERR], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
        }

        end:
        free(Buf);
    }

    return 0;
}

static CHAR GetDriveLetter(UINT32 disksig, UINT64 PartOffset)
{
    CHAR Letter;
    DWORD Drives;
    UINT32 Sig;
    DISK_EXTENT DiskExtent;

    Letter = 'A';
    Drives = GetLogicalDrives();
    LogA("Logic Drives: 0x%x", Drives);

    while (Drives)
    {
        if (Drives & 0x01)
        {
            Sig = 0;
            DiskExtent.StartingOffset.QuadPart = 0;
            if (GetPhyDiskInfo(Letter, &Sig, &DiskExtent) == 0)
            {
                if (Sig == disksig && DiskExtent.StartingOffset.QuadPart == PartOffset)
                {
                    return Letter;
                }
            }
        }

        Drives >>= 1;
        Letter++;
    }

    return 0;
}

static int ParseVlnk(HWND hWnd)
{
    int i;
    CHAR Letter;
    ventoy_vlnk vlnk;
    OPENFILENAME ofn = { 0 };
    WCHAR szFile[MAX_PATH] = { 0 };
    WCHAR szDst[MAX_PATH + 2] = { 0 };
    WCHAR Msg[1024];
    CHAR *suffix = NULL;
    HANDLE hFile;

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"Vlnk File\0*.vlnk.iso;*.vlnk.img;*.vlnk.wim;*.vlnk.efi;*.vlnk.vhd;*.vlnk.vhdx;*.vlnk.vtoy;*.vlnk.dat\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) != TRUE)
    {
        return 1;
    }

    LogW(L"Parse vlnk for <%ls>\n", szFile);

    if (!IsVlnkFile(szFile, &vlnk))
    {
        VtoyMessageBox(hWnd, g_msg_lang[MSGID_INVALID_VLNK], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
        return 1;
    }

    for (i = 0; i < sizeof(vlnk.filepath) && vlnk.filepath[i]; i++)
    {
        if (vlnk.filepath[i] == '.')
        {
            suffix = vlnk.filepath + i;
        }
    }

    if (!IsSupportedImgSuffix(suffix))
    {
        VtoyMessageBox(hWnd, g_msg_lang[MSGID_SUFFIX_UNSUPPORTED], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
        return 1;
    }

    Utf8ToUtf16(vlnk.filepath, szDst + 2);
    for (i = 2; i < MAX_PATH && szDst[i]; i++)
    {
        if (szDst[i] == '/')
        {
            szDst[i] = '\\';
        }
    }
    

    Letter = GetDriveLetter(vlnk.disk_signature, vlnk.part_offset);
    if (Letter == 0)
    {
        VtoyMessageBox(hWnd, g_msg_lang[MSGID_VLNK_NO_DST], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
        return 1;
    }

    szDst[0] = toupper(Letter);
    szDst[1] = ':';
    LogW(L"vlnk dst is %ls\n", szDst);

    hFile = CreateFileW(szDst, FILE_READ_EA, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (INVALID_HANDLE_VALUE == hFile)
    {
        VtoyMessageBox(hWnd, g_msg_lang[MSGID_VLNK_NO_DST], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
        return 1;
    }
    CloseHandle(hFile);

    swprintf_s(Msg, 1024, L"%ls %ls", g_msg_lang[MSGID_VLNK_POINT_TO], szDst);
    VtoyMessageBox(hWnd, Msg, g_msg_lang[MSGID_INFO], MB_OK | MB_ICONINFORMATION);

    return 0;
}

INT_PTR CALLBACK DialogProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	WORD NotifyCode;
	WORD CtrlID;

    switch (Message)
    {
		case WM_COMMAND:
		{
			NotifyCode = HIWORD(wParam);
			CtrlID = LOWORD(wParam);

			if (NotifyCode == BN_CLICKED)
			{
				if (CtrlID == IDC_BUTTON1)
				{
                    EnableWindow(g_create_button, FALSE);
                    CreateVlnk(hWnd, g_CurDirW, NULL, NULL);
                    EnableWindow(g_create_button, TRUE);
				}
				else if (CtrlID == IDC_BUTTON2)
				{
                    EnableWindow(g_parse_button, FALSE);
                    ParseVlnk(hWnd);
                    EnableWindow(g_parse_button, TRUE);
				}
			}
			break;
		}
        case WM_INITDIALOG:
        {
            InitDialog(hWnd, wParam, lParam);
            break;
        }        
        case WM_CLOSE:
        {
            OnDestroyDialog();
            EndDialog(hWnd, 0);
        }
    }

    return 0;
}

static int ParseCmdLine(LPSTR lpCmdLine)
{
    int i;
    int argc = 0;
    LPWSTR *lpszArgv = NULL;
    
    lpszArgv = CommandLineToArgvW(GetCommandLineW(), &argc);

    for (i = 0; i < argc; i++)
    {
        if (lstrcmp(lpszArgv[i], L"-q") == 0 || lstrcmp(lpszArgv[i], L"-Q") == 0)
        {
            g_LogFile[0] = 0;
        }
        else if (lstrcmp(lpszArgv[i], L"-h") == 0 || lstrcmp(lpszArgv[i], L"-H") == 0)
        {
            g_ShowHelp = TRUE;
        }
        else if (lstrcmp(lpszArgv[i], L"-s") == 0 || lstrcmp(lpszArgv[i], L"-S") == 0)
        {
            g_SaveAs = TRUE;
        }
        else if (lstrcmp(lpszArgv[i], L"-i") == 0 || lstrcmp(lpszArgv[i], L"-I") == 0)
        {
            if (i + 1 < argc)
            {
                wcscpy_s(g_CmdInFile, MAX_PATH, lpszArgv[i + 1]);
            }
        }
        else if (lstrcmp(lpszArgv[i], L"-o") == 0 || lstrcmp(lpszArgv[i], L"-O") == 0)
        {
            if (i + 1 < argc)
            {
                wcscpy_s(g_CmdOutFile, MAX_PATH, lpszArgv[i + 1]);
            }
        }
    }

    return argc;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
    DWORD dwAttrib;
	HANDLE hMutex;

    UNREFERENCED_PARAMETER(hPrevInstance);

    if (GetUserDefaultUILanguage() == 0x0804)
    {
        g_msg_lang = g_msg_cn;
    }
    else
    {
        g_msg_lang = g_msg_en;
    }

    hMutex = CreateMutexA(NULL, TRUE, "VtoyVlnkMUTEX");
    if ((hMutex != NULL) && (GetLastError() == ERROR_ALREADY_EXISTS))
    {
        MessageBoxW(NULL, g_msg_lang[MSGID_RUNNING_TIP], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
        return 1;
    }

    GetCurrentDirectoryA(MAX_PATH, g_CurDirA);
    GetCurrentDirectoryW(MAX_PATH, g_CurDirW);
    sprintf_s(g_LogFile, sizeof(g_LogFile), "%s\\VentoyVlnk.log", g_CurDirA);

    ParseCmdLine(lpCmdLine);
    
    g_hInst = hInstance;

    if (g_ShowHelp)
    {
        VtoyMessageBox(NULL, L"VentoyVlnk.exe  CMD\r\n  -i  Input file path\r\n  -o  Output vlnk file path\r\n  -q  Quite mode (no log)", L"Tip", MB_OK);
        return 0;
    }
    else if (g_CmdInFile[0] && g_CmdOutFile[0])
    {
        LogA("========= VentoyVlnk Cmdline Mode =========\n");

        dwAttrib = GetFileAttributesW(g_CmdInFile);
        if (dwAttrib == INVALID_FILE_ATTRIBUTES || (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
        {
            LogW(L"File <<%ls>> does not exist!\n", g_CmdInFile);
            VtoyMessageBox(NULL, g_msg_lang[MSGID_SRC_NONEXIST], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
            return 1;
        }

        if (!IsSupportedVlnkSuffix(g_CmdOutFile))
        {
            LogW(L"File <<%ls>> contains invalid vlnk suffix!\n", g_CmdOutFile);
            VtoyMessageBox(NULL, g_msg_lang[MSGID_INVALID_SUFFIX], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
            return 1;
        }

        return CreateVlnk(NULL, g_CurDirW, g_CmdInFile, g_CmdOutFile);
    }
    else
    {
        LogA("========= VentoyVlnk GUI Mode =========\n");

        DialogBoxA(hInstance, MAKEINTRESOURCEA(IDD_DIALOG1), NULL, DialogProc);
        return 0;
    }
}
