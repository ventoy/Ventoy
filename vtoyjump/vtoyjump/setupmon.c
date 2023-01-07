/******************************************************************************
* setupmon.c
*
* Copyright (c) 2022, longpanda <admin@ventoy.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <virtdisk.h>
#include <winioctl.h>
#include <VersionHelpers.h>
#include "vtoyjump.h"


#define PROMPT_MAX  1024
typedef struct VTOY_MUI_DATA
{
    WCHAR Prompt[PROMPT_MAX];
    struct VTOY_MUI_DATA* pNext;
}VTOY_MUI_DATA;

static VTOY_MUI_DATA* g_MuiDataHead = NULL;
static FILETIME g_SetupMonStartTime;

#pragma pack(1)
typedef struct {
    WORD      dlgVer;
    WORD      signature;
    DWORD     helpID;
    DWORD     exStyle;
    DWORD     style;
    WORD      cDlgItems;
    short     x;
    short     y;
    short     cx;
    short     cy;
    //sz_Or_Ord menu;
    //sz_Or_Ord windowClass;
    //WCHAR     title[titleLen];
    //WORD      pointsize;
    //WORD      weight;
    //BYTE      italic;
    //BYTE      charset;
    //WCHAR     typeface[stringLen];
} DLGTEMPLATEEX;

typedef struct {
    DWORD     helpID;
    DWORD     exStyle;
    DWORD     style;
    short     x;
    short     y;
    short     cx;
    short     cy;
    DWORD     id;
    //  sz_Or_Ord windowClass;
    //  sz_Or_Ord title;
    //  WORD      extraCount;
} DLGITEMTEMPLATEEX;


#pragma pack()

#define wchar_to_utf8_no_alloc(wsrc, dest, dest_size) \
	WideCharToMultiByte(CP_UTF8, 0, wsrc, -1, dest, dest_size, NULL, NULL)
#define utf8_to_wchar_no_alloc(src, wdest, wdest_size) \
	MultiByteToWideChar(CP_UTF8, 0, src, -1, wdest, wdest_size)

#define VTOY_DLG_SKIP_SZ_ORD(pWord) \
{\
    if (*pWord == 0x0000)\
    {\
        pWordData += 1;\
    }\
    else if (*pWordData == 0xFFFF)\
    {\
        pWordData += 2;\
    }\
    else\
    {\
        while (*pWordData++)\
        {\
            ;\
        }\
    }\
}

#define VTOY_DLG_SKIP_SZ(pWord) \
{\
    while (*pWord)\
    {\
        pWord++;\
    }\
    pWord++;\
}


static BOOL LoadSetupRebootDialogPrompt(HMODULE hMui, DWORD ID, WCHAR* Prompt, DWORD Len)
{
    DWORD i = 0;
    UINT64 Addr = 0;
    WORD* pWordData = NULL;
    HRSRC hDlg = NULL;
    HGLOBAL hTemplate = NULL;
    DLGTEMPLATEEX* pDlgTempEx = NULL;
    DLGITEMTEMPLATEEX* pDlgItemTempEx = NULL;

    hDlg = FindResource(hMui, MAKEINTRESOURCE(ID), RT_DIALOG);
    if (!hDlg)
    {
        return FALSE;
    }

    hTemplate = LoadResource(hMui, hDlg);
    if (!hTemplate)
    {
        return FALSE;
    }

    pDlgTempEx = (DLGTEMPLATEEX*)LockResource(hTemplate);
    if (!pDlgTempEx)
    {
        return FALSE;
    }

    if (pDlgTempEx->signature != 0xFFFF)
    {
        return FALSE;
    }

    pWordData = (WORD*)(pDlgTempEx + 1);

    //skip menu
    VTOY_DLG_SKIP_SZ_ORD(pWordData);

    //skip windowClass
    VTOY_DLG_SKIP_SZ_ORD(pWordData);

    //skip title
    VTOY_DLG_SKIP_SZ(pWordData);

    //skip pointsize/weight/italic + charset
    pWordData += 3;

    //skip typeface
    VTOY_DLG_SKIP_SZ(pWordData);

    //align to DWORD
    Addr = (UINT64)pWordData;
    if ((Addr % 4) > 0)
    {
        Addr += 4 - (Addr % 4);
        pWordData = (WORD*)Addr;
    }

    //First Dlg Item 
    pDlgItemTempEx = (DLGITEMTEMPLATEEX*)pWordData;

    if (pDlgItemTempEx->id != 1023)
    {
        return FALSE;
    }

    pWordData = (WORD*)(pDlgItemTempEx + 1);

    //skip windowClass
    VTOY_DLG_SKIP_SZ_ORD(pWordData);

    for (i = 0; i < Len; i++)
    {
        Prompt[i] = pWordData[i];
        if (Prompt[i] == 0)
        {
            break;
        }
    }

    return TRUE;
}

static int LoadPromptString(const char* szDir)
{
    BOOL bRet;
    HMODULE hMui;
    CHAR MuiFile[MAX_PATH];
    WCHAR Prompt[PROMPT_MAX];
    VTOY_MUI_DATA* Node = NULL;

    sprintf_s(MuiFile, sizeof(MuiFile), "X:\\Sources\\%s\\w32uires.dll.mui", szDir);

    hMui = LoadLibraryA(MuiFile);
    if (!hMui)
    {
        Log("Failed to loadlibrary <%s> %u", MuiFile, LASTERR);
        return 1;
    }

    bRet = LoadSetupRebootDialogPrompt(hMui, 103, Prompt, PROMPT_MAX);
    if (bRet)
    {
        Node = malloc(sizeof(VTOY_MUI_DATA));
        if (Node)
        {
            memset(Node, 0, sizeof(VTOY_MUI_DATA));
            memcpy(Node->Prompt, Prompt, sizeof(Prompt));
            if (g_MuiDataHead == NULL)
            {
                g_MuiDataHead = Node;
            }
            else
            {
                Node->pNext = g_MuiDataHead;
                g_MuiDataHead = Node;
            }

            Log("Successfully add prompt string for <%s>", szDir);
        }
    }
    else
    {
        Log("Failed to load prompt string from %s", szDir);
    }

    FreeLibrary(hMui);
    return 0;
}

static int FindAllPromptStrings(void)
{
    HANDLE hFind;
    WIN32_FIND_DATAA FindData;

    hFind = FindFirstFileA("X:\\Sources\\*", &FindData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        Log("FindFirstFileA failed %u", LASTERR);
        return 0;
    }

    do
    {
        if ((FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (strlen(FindData.cFileName) == 5))
        {
            if (IsFileExist("X:\\Sources\\%s\\w32uires.dll.mui", FindData.cFileName))
            {
                LoadPromptString(FindData.cFileName);
            }
        }        
    } while (FindNextFileA(hFind, &FindData));

    return 0;
}

static BOOL CALLBACK SetupPromptCallback(HWND hWnd, LPARAM lParam)
{
    VTOY_MUI_DATA* Node = NULL;
    WCHAR Prompt[PROMPT_MAX] = { 0 };
    BOOL* found = (BOOL*)lParam;

    if (GetWindowTextW(hWnd, Prompt, PROMPT_MAX - 1) > 0)
    {
        for (Node = g_MuiDataHead; Node; Node = Node->pNext)
        {
            if (wcscmp(Node->Prompt, Prompt) == 0)
            {
                *found = TRUE;
                break;
            }
        }
    }
    
    return TRUE;
}

static BOOL CALLBACK EnumWindowsProcFunc(HWND hWnd, LPARAM lParam)
{
    EnumChildWindows(hWnd, SetupPromptCallback, lParam);
    return TRUE;
}


static CHAR FindWindowsInstallDstDrive(void)
{
    CHAR ret = 0;
    CHAR Drv = 'A';
    DWORD Drives;
    CHAR ChkFilePath[MAX_PATH];
    HANDLE hFile;
    UINT64 Time1, Time2, Time3;
    FILETIME fTime;
    FILETIME sysftime;
    SYSTEMTIME cur_systime;

    GetSystemTime(&cur_systime);
    SystemTimeToFileTime(&cur_systime, &sysftime);

    memcpy(&Time1, &g_SetupMonStartTime, 8);
    memcpy(&Time2, &sysftime, 8);

    Drives = GetLogicalDrives();
    Log("Find Windows: Logical Drives: 0x%x", Drives);

    while ((Drives > 0) && (ret == 0))
    {
        if ((Drives & 1) && Drv != 'X')
        {
            sprintf_s(ChkFilePath, sizeof(ChkFilePath), "%C:\\$WINDOWS.~BT\\Sources\\Panther\\setupact.log", Drv);
            hFile = CreateFileA(ChkFilePath, FILE_READ_EA, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                Log("%s Exist", ChkFilePath);

                if (GetFileTime(hFile, NULL, NULL, &fTime))
                {
                    memcpy(&Time3, &fTime, 8);

                    Log("### %s %llu %llu %llu", ChkFilePath, Time1, Time2, Time3);

                    if (Time3 > Time1 && Time3 < Time2)
                    {
                        Log("Detect Windows11 install drive:<%C>", Drv);
                        ret = Drv;
                    }
                }
                CloseHandle(hFile);
            }
            else
            {
                Log("%s NOT Exist %u", ChkFilePath, LASTERR);
            }
        }

        Drives >>= 1;
        Drv++;
    }

    return ret;
}

static int AddBypassNROReg(const CHAR* OfflineRegPath)
{
    int ret = 1;
    BOOL Unload = FALSE;
    LSTATUS Status;
    HKEY hKey = NULL;
    HKEY hSubKey = NULL;    
    DWORD dwValue = 1;
    DWORD dwSize;
    HANDLE htoken;
    DWORD s = 0;
    TOKEN_PRIVILEGES* p = NULL;

    Log("AddBypassNROReg<%s>", OfflineRegPath);

    if (!IsFileExist(OfflineRegPath))
    {
        return ret;
    }
    
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &htoken))
    {
        Log("Open process token failed! %u", LASTERR);
        return ret;
    }

    s = sizeof(TOKEN_PRIVILEGES) + 2 * sizeof(LUID_AND_ATTRIBUTES);
    p = (PTOKEN_PRIVILEGES)malloc(s);
    if (!p)
    {
        Log("Failed to alloc privileges memory");
        return ret;
    }

    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &(p->Privileges[0].Luid)) ||
        !LookupPrivilegeValue(NULL, SE_BACKUP_NAME, &(p->Privileges[1].Luid)) ||
        !LookupPrivilegeValue(NULL, SE_RESTORE_NAME, &(p->Privileges[2].Luid))) 
    {
        Log("LookupPrivilegeValue failed!");
        goto End;
    }

    p->PrivilegeCount = 3;    
    p->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    p->Privileges[1].Attributes = SE_PRIVILEGE_ENABLED;
    p->Privileges[2].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(htoken, FALSE, p, s, NULL, NULL) || GetLastError() != ERROR_SUCCESS) 
    {
        Log("AdjustTokenPrivileges failed!");
        goto End;
    }

    Log("AdjustTokenPrivileges success");

    Status = RegLoadKeyA(HKEY_LOCAL_MACHINE, "VTOYNEWSW", OfflineRegPath);
    if (Status != ERROR_SUCCESS)
    {
        Log("RegLoadKey Failed 0x%x", Status);
        goto End;
    }
    Unload = TRUE;

    Status = RegCreateKeyExA(HKEY_LOCAL_MACHINE, "VTOYNEWSW\\Microsoft\\Windows\\CurrentVersion", 0, NULL, REG_OPTION_BACKUP_RESTORE, KEY_ALL_ACCESS, NULL, &hKey, &dwSize);
    if (ERROR_SUCCESS != Status)
    {
        Log("Failed to create reg key VTOYNEWSW\\Microsoft\\Windows\\CurrentVersion %u %u", LASTERR, Status);
        goto End;
    }

    Status = RegCreateKeyExA(hKey, "OOBE", 0, NULL, 0, KEY_SET_VALUE | KEY_QUERY_VALUE | KEY_CREATE_SUB_KEY, NULL, &hSubKey, &dwSize);
    if (ERROR_SUCCESS != Status)
    {
        Log("Failed to create OOBE reg  %u %u", LASTERR, Status);
        goto End;
    }

    Status = RegSetValueExA(hSubKey, "BypassNRO", 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
    Log("Create BypassNRO registry %s %u", (Status == ERROR_SUCCESS) ? "SUCCESS" : "FAILED", Status);

    Status = RegFlushKey(hSubKey);
    Status += RegCloseKey(hSubKey);
    Log("Flush and close subkey %s %u", (Status == ERROR_SUCCESS) ? "SUCCESS" : "FAILED", Status);

    Status = RegFlushKey(hKey);
    Status += RegCloseKey(hKey);
    Log("Flush and close key %s %u", (Status == ERROR_SUCCESS) ? "SUCCESS" : "FAILED", Status);

    ret = 0;
    
End:
    if (Unload)
    {
        Status = RegUnLoadKeyA(HKEY_LOCAL_MACHINE, "VTOYNEWSW");
        Log("RegUnLoadKey %s %u", (Status == ERROR_SUCCESS) ? "SUCCESS" : "FAILED", Status);
    }
    
    if (p)
    {
        free(p);
    }

    return ret;
}

static DWORD WINAPI WaitSetupFinishThread(void* Param)
{
    CHAR Drv;
    BOOL Found = FALSE;
    CHAR OfflineRegPath[MAX_PATH];

    while (!Found)
    {
        Sleep(300);
        EnumWindows(EnumWindowsProcFunc, (LPARAM)&Found);
    }

    Log("### Setup finish detected");

    // Add HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\OOBE\BypassNRO
    Drv = FindWindowsInstallDstDrive();
    Log("Find Windows install drive %d", Drv);
    if (Drv)
    {
        sprintf_s(OfflineRegPath, sizeof(OfflineRegPath), "%C:\\Windows\\system32\\config\\SOFTWARE", Drv);
        AddBypassNROReg(OfflineRegPath);
    }

    return 0;
}

int SetupMonNroStart(const char *isopath)
{
    SYSTEMTIME systime;

    Log("SetupMonNroStart ...");

    FindAllPromptStrings();

    if (!g_MuiDataHead)
    {
        Log("Prompt not found, add default");
        g_MuiDataHead = malloc(sizeof(VTOY_MUI_DATA));
        if (g_MuiDataHead)
        {
            wcscpy_s(g_MuiDataHead->Prompt, PROMPT_MAX, L"Windows needs to restart to continue");
            g_MuiDataHead->pNext = NULL;
        }
    }

    Log("Wait for setup finish...");
    GetSystemTime(&systime);
    SystemTimeToFileTime(&systime, &g_SetupMonStartTime);

    CreateThread(NULL, 0, WaitSetupFinishThread, NULL, 0, NULL);

    return 0;
}
