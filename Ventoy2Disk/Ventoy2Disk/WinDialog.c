/******************************************************************************
 * WinDialog.c
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
#include <commctrl.h>
#include "resource.h"
#include "Language.h"
#include "Ventoy2Disk.h"

HINSTANCE g_hInst;

HWND g_DialogHwnd;
HWND g_ComboxHwnd;
HWND g_StaticLocalVerHwnd;
HWND g_StaticDiskVerHwnd;
HWND g_BtnInstallHwnd;
HWND g_BtnUpdateHwnd;
HWND g_ProgressBarHwnd;
HWND g_StaticStatusHwnd;
CHAR g_CurVersion[64];
HANDLE g_ThreadHandle = NULL;

void GetExeVersionInfo(const char *FilePath)
{
    UINT length;
    DWORD verBufferSize;
    CHAR  verBuffer[2048];
    VS_FIXEDFILEINFO *verInfo = NULL;

    verBufferSize = GetFileVersionInfoSizeA(FilePath, NULL);

    if (verBufferSize > 0 && verBufferSize <= sizeof(verBuffer))
    {
        if (GetFileVersionInfoA(FilePath, 0, verBufferSize, (LPVOID)verBuffer))
        {
            VerQueryValueA(verBuffer, "\\", &verInfo, &length);

            safe_sprintf(g_CurVersion, "%u.%u.%u.%u",
                HIWORD(verInfo->dwProductVersionMS),
                LOWORD(verInfo->dwProductVersionMS),
                HIWORD(verInfo->dwProductVersionLS),
                LOWORD(verInfo->dwProductVersionLS));
        }
    }
}

void SetProgressBarPos(int Pos)
{
    CHAR Ratio[64];

    if (Pos >= PT_FINISH)
    {
        Pos = PT_FINISH;
    }

    SendMessage(g_ProgressBarHwnd, PBM_SETPOS, Pos, 0);

    safe_sprintf(Ratio, "Status - %.0lf%%", Pos * 100.0 / PT_FINISH);
    SetWindowTextA(g_StaticStatusHwnd, Ratio);
}

static void OnComboxSelChange(HWND hCombox)
{
    int nCurSelected;
    PHY_DRIVE_INFO *CurDrive = NULL;

    SetWindowTextA(g_StaticLocalVerHwnd, GetLocalVentoyVersion());
    EnableWindow(g_BtnInstallHwnd, FALSE);
    EnableWindow(g_BtnUpdateHwnd, FALSE);

    nCurSelected = SendMessage(hCombox, CB_GETCURSEL, 0, 0);
    if (CB_ERR == nCurSelected)
    {
        return;
    }

    CurDrive = GetPhyDriveInfoById(nCurSelected);
    if (!CurDrive)
    {
        return;
    }
    
    SetWindowTextA(g_StaticDiskVerHwnd, CurDrive->VentoyVersion);

    if (g_ForceOperation == 0)
    {
        if (CurDrive->VentoyVersion[0])
        {
            //only can update
            EnableWindow(g_BtnInstallHwnd, FALSE);
            EnableWindow(g_BtnUpdateHwnd, TRUE);
        }
        else
        {
            //only can install
            EnableWindow(g_BtnInstallHwnd, TRUE);
            EnableWindow(g_BtnUpdateHwnd, FALSE);
        }
    }
    else
    {
        EnableWindow(g_BtnInstallHwnd, TRUE);
        EnableWindow(g_BtnUpdateHwnd, TRUE);
    }

    InvalidateRect(g_DialogHwnd, NULL, TRUE);
    UpdateWindow(g_DialogHwnd);
}

static void LanguageInit(void)
{
    SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_DEV), _G(STR_DEVICE));
    SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_LOCAL), _G(STR_LOCAL_VER));
    SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_DISK), _G(STR_DISK_VER));
    SetWindowText(g_StaticStatusHwnd, _G(STR_STATUS));

    SetWindowText(g_BtnInstallHwnd, _G(STR_INSTALL));
    SetWindowText(g_BtnUpdateHwnd, _G(STR_UPDATE));
}

static BOOL InitDialog(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    DWORD i;
    HANDLE hCombox;
    HFONT hStaticFont;
    CHAR Letter[32];
    CHAR DeviceName[256];
    HICON hIcon;

    g_DialogHwnd = hWnd;
    g_ComboxHwnd = GetDlgItem(hWnd, IDC_COMBO1);
    g_StaticLocalVerHwnd = GetDlgItem(hWnd, IDC_STATIC_LOCAL_VER);
    g_StaticDiskVerHwnd = GetDlgItem(hWnd, IDC_STATIC_DISK_VER);
    g_BtnInstallHwnd = GetDlgItem(hWnd, IDC_BUTTON4);
    g_BtnUpdateHwnd = GetDlgItem(hWnd, IDC_BUTTON3);
    g_ProgressBarHwnd = GetDlgItem(hWnd, IDC_PROGRESS1);
    g_StaticStatusHwnd = GetDlgItem(hWnd, IDC_STATIC_STATUS);

    hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON1));
    SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

    SendMessage(g_ProgressBarHwnd, PBM_SETRANGE, (WPARAM)0, (LPARAM)(MAKELPARAM(0, PT_FINISH)));
    PROGRESS_BAR_SET_POS(PT_START);

    LanguageInit();

    // Fill device combox
    hCombox = GetDlgItem(hWnd, IDC_COMBO1);
    for (i = 0; i < g_PhyDriveCount; i++)
    {
        if (g_PhyDriveList[i].Id < 0)
        {
            continue;
        }

        if (g_PhyDriveList[i].FirstDriveLetter >= 0)
        {
            safe_sprintf(Letter, "%C: ", g_PhyDriveList[i].FirstDriveLetter);
        }
        else
        {
            Letter[0] = 0;
        }

        safe_sprintf(DeviceName, "%s[%dGB] %s %s", 
            Letter,
            GetHumanReadableGBSize(g_PhyDriveList[i].SizeInBytes),
            g_PhyDriveList[i].VendorId,
            g_PhyDriveList[i].ProductId
            );
        SendMessageA(hCombox, CB_ADDSTRING, 0, (LPARAM)DeviceName);
    }
    
    SendMessage(hCombox, CB_SETCURSEL, 0, 0);

    // Set static text & font 
    hStaticFont = CreateFont(26, 0, 0, 0, FW_BOLD, FALSE, FALSE, 0,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH&FF_SWISS, TEXT("Courier New"));

    SendMessage(g_StaticLocalVerHwnd, WM_SETFONT, (WPARAM)hStaticFont, TRUE);
    SendMessage(g_StaticDiskVerHwnd, WM_SETFONT, (WPARAM)hStaticFont, TRUE);

    OnComboxSelChange(g_ComboxHwnd);

    SetFocus(g_ProgressBarHwnd);

    return TRUE;
}

static DWORD WINAPI InstallVentoyThread(void* Param)
{
    int rc;
    PHY_DRIVE_INFO *pPhyDrive = (PHY_DRIVE_INFO *)Param;

    rc = InstallVentoy2PhyDrive(pPhyDrive);
    if (rc == 0)
    {
		PROGRESS_BAR_SET_POS(PT_FINISH);
        MessageBox(g_DialogHwnd, _G(STR_INSTALL_SUCCESS), _G(STR_INFO), MB_OK | MB_ICONINFORMATION);
        safe_strcpy(pPhyDrive->VentoyVersion, GetLocalVentoyVersion());
    }
    else
    {
		PROGRESS_BAR_SET_POS(PT_FINISH);
        MessageBox(g_DialogHwnd, _G(STR_INSTALL_FAILED), _G(STR_ERROR), MB_OK | MB_ICONERROR);
    }
    
	PROGRESS_BAR_SET_POS(PT_START);
    g_ThreadHandle = NULL;
    SetWindowText(g_StaticStatusHwnd, _G(STR_STATUS));
    OnComboxSelChange(g_ComboxHwnd);

    return 0;
}

static DWORD WINAPI UpdateVentoyThread(void* Param)
{
    int rc;
    PHY_DRIVE_INFO *pPhyDrive = (PHY_DRIVE_INFO *)Param;

    rc = UpdateVentoy2PhyDrive(pPhyDrive);
    if (rc == 0)
    {
		PROGRESS_BAR_SET_POS(PT_FINISH);
        MessageBox(g_DialogHwnd, _G(STR_UPDATE_SUCCESS), _G(STR_INFO), MB_OK | MB_ICONINFORMATION);
        safe_strcpy(pPhyDrive->VentoyVersion, GetLocalVentoyVersion());
    }
    else
    {
		PROGRESS_BAR_SET_POS(PT_FINISH);
        MessageBox(g_DialogHwnd, _G(STR_UPDATE_FAILED), _G(STR_ERROR), MB_OK | MB_ICONERROR);
    }
    
	PROGRESS_BAR_SET_POS(PT_START);
    g_ThreadHandle = NULL;
    SetWindowText(g_StaticStatusHwnd, _G(STR_STATUS));
    OnComboxSelChange(g_ComboxHwnd);

    return 0;
}



static void OnInstallBtnClick(void)
{
    int nCurSel;
    PHY_DRIVE_INFO *pPhyDrive = NULL;

    if (MessageBox(g_DialogHwnd, _G(STR_INSTALL_TIP), _G(STR_WARNING), MB_YESNO | MB_ICONWARNING) != IDYES)
    {
        return;
    }

    if (MessageBox(g_DialogHwnd, _G(STR_INSTALL_TIP2), _G(STR_WARNING), MB_YESNO | MB_ICONWARNING) != IDYES)
    {
        return;
    }

    if (g_ThreadHandle)
    {
        Log("Another thread is runing");
        return;
    }

    nCurSel = SendMessage(g_ComboxHwnd, CB_GETCURSEL, 0, 0);
    if (CB_ERR == nCurSel)
    {
        Log("Failed to get combox sel");
        return;;
    }

    pPhyDrive = GetPhyDriveInfoById(nCurSel);
    if (!pPhyDrive)
    {
        return;
    }

    EnableWindow(g_BtnInstallHwnd, FALSE);
    EnableWindow(g_BtnUpdateHwnd, FALSE);

    g_ThreadHandle = CreateThread(NULL, 0, InstallVentoyThread, (LPVOID)pPhyDrive, 0, NULL);
}



static void OnUpdateBtnClick(void)
{
    int nCurSel;
    PHY_DRIVE_INFO *pPhyDrive = NULL;

    if (MessageBox(g_DialogHwnd, _G(STR_UPDATE_TIP), _G(STR_INFO), MB_YESNO | MB_ICONQUESTION) != IDYES)
    {
        return;
    }

    if (g_ThreadHandle)
    {
        Log("Another thread is runing");
        return;
    }

    nCurSel = SendMessage(g_ComboxHwnd, CB_GETCURSEL, 0, 0);
    if (CB_ERR == nCurSel)
    {
        Log("Failed to get combox sel");
        return;;
    }

    pPhyDrive = GetPhyDriveInfoById(nCurSel);
    if (!pPhyDrive)
    {
        return;
    }

    EnableWindow(g_BtnInstallHwnd, FALSE);
    EnableWindow(g_BtnUpdateHwnd, FALSE);

    g_ThreadHandle = CreateThread(NULL, 0, UpdateVentoyThread, (LPVOID)pPhyDrive, 0, NULL);
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

            if (CtrlID == IDC_COMBO1 && NotifyCode == CBN_SELCHANGE)
            {
                OnComboxSelChange((HWND)lParam);
            }

            if (CtrlID == IDC_BUTTON4 && NotifyCode == BN_CLICKED)
            {
                OnInstallBtnClick();
            }
            else if (CtrlID == IDC_BUTTON3 && NotifyCode == BN_CLICKED)
            {
                OnUpdateBtnClick();
            }


            break;
        }
        case WM_INITDIALOG:
        {
            InitDialog(hWnd, wParam, lParam);
            break;
        }
        case WM_CTLCOLORSTATIC:
        {
            if (GetDlgItem(hWnd, IDC_STATIC_LOCAL_VER) == (HANDLE)lParam || 
                GetDlgItem(hWnd, IDC_STATIC_DISK_VER) == (HANDLE)lParam)
            {
                SetBkMode((HDC)wParam, TRANSPARENT);
                SetTextColor((HDC)wParam, RGB(255, 0, 0));
                return (LRESULT)(HBRUSH)(GetStockObject(HOLLOW_BRUSH));
            }
            else
            {
                break;
            }
        }
        case WM_CLOSE:
        {
            if (g_ThreadHandle)
            {
                MessageBox(g_DialogHwnd, _G(STR_WAIT_PROCESS), _G(STR_INFO), MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                EndDialog(hWnd, 0);
            }
            break;
        }
    }

    return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

    if (!IsFileExist(VENTOY_FILE_VERSION))
    {
        MessageBox(NULL, _G(STR_INCORRECT_DIR), _G(STR_ERROR), MB_OK | MB_ICONERROR);
        return ERROR_NOT_FOUND;
    }

    GetExeVersionInfo(__argv[0]);

    Log("\n################################ Ventoy2Disk %s ################################", g_CurVersion);

    ParseCmdLineOption(lpCmdLine);

    DumpWindowsVersion();

    Ventoy2DiskInit();

    g_hInst = hInstance;
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DialogProc);

    Ventoy2DiskDestroy();

    return 0;
}
