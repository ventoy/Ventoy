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


BOOL g_SecureBoot = FALSE;
HWND g_DialogHwnd;
HWND g_ComboxHwnd;
HWND g_StaticLocalVerHwnd;
HWND g_StaticDiskVerHwnd;
HWND g_BtnInstallHwnd;
HWND g_StaticDevHwnd;
HWND g_StaticLocalHwnd;
HWND g_StaticDiskHwnd;
HWND g_BtnUpdateHwnd;
HWND g_ProgressBarHwnd;
HWND g_StaticStatusHwnd;
CHAR g_CurVersion[64];
HANDLE g_ThreadHandle = NULL;

int g_language_count = 0;
int g_cur_lang_id = 0;
VENTOY_LANGUAGE *g_language_data = NULL;
VENTOY_LANGUAGE *g_cur_lang_data = NULL;

static int LoadCfgIni(void)
{
	int value;

	value = GetPrivateProfileInt(TEXT("Ventoy"), TEXT("SecureBoot"), 0, VENTOY_CFG_INI);

	if (value == 1)
	{
		g_SecureBoot = TRUE;
	}
	
	return 0;
}

static int WriteCfgIni(void)
{
	WCHAR TmpBuf[128];

	swprintf_s(TmpBuf, 128, TEXT("%d"), g_cur_lang_id);
	WritePrivateProfileString(TEXT("Ventoy"), TEXT("Language"), TmpBuf, VENTOY_CFG_INI);

	swprintf_s(TmpBuf, 128, TEXT("%d"), g_SecureBoot);
	WritePrivateProfileString(TEXT("Ventoy"), TEXT("SecureBoot"), TmpBuf, VENTOY_CFG_INI);

	return 0;
}


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

static void UpdateItemString(int defaultLangId)
{
	int i;
	HMENU SubMenu;
	HFONT hLangFont;
	HMENU hMenu = GetMenu(g_DialogHwnd);

	g_cur_lang_id = defaultLangId;
	g_cur_lang_data = g_language_data + defaultLangId;



	hLangFont = CreateFont(g_language_data[defaultLangId].FontSize, 0, 0, 0, 400, FALSE, FALSE, 0,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH, g_language_data[defaultLangId].FontFamily);

	SendMessage(g_BtnInstallHwnd, WM_SETFONT, (WPARAM)hLangFont, TRUE);
	SendMessage(g_BtnUpdateHwnd, WM_SETFONT, (WPARAM)hLangFont, TRUE);
	SendMessage(g_StaticStatusHwnd, WM_SETFONT, (WPARAM)hLangFont, TRUE);
	SendMessage(g_StaticLocalHwnd, WM_SETFONT, (WPARAM)hLangFont, TRUE);
	SendMessage(g_StaticDiskHwnd, WM_SETFONT, (WPARAM)hLangFont, TRUE);
	SendMessage(g_StaticDevHwnd, WM_SETFONT, (WPARAM)hLangFont, TRUE);
	SendMessage(g_DialogHwnd, WM_SETFONT, (WPARAM)hLangFont, TRUE);

	ModifyMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, 0, _G(STR_MENU_OPTION));

	SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_DEV), _G(STR_DEVICE));
	SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_LOCAL), _G(STR_LOCAL_VER));
	SetWindowText(GetDlgItem(g_DialogHwnd, IDC_STATIC_DISK), _G(STR_DISK_VER));
	SetWindowText(g_StaticStatusHwnd, _G(STR_STATUS));

	SetWindowText(g_BtnInstallHwnd, _G(STR_INSTALL));
	SetWindowText(g_BtnUpdateHwnd, _G(STR_UPDATE));

	SubMenu = GetSubMenu(hMenu, 0);
	if (g_SecureBoot)
	{
		ModifyMenu(SubMenu, 0, MF_BYPOSITION | MF_STRING | MF_CHECKED, 0, _G(STR_MENU_SECURE_BOOT));
	}
	else
	{
		ModifyMenu(SubMenu, 0, MF_BYPOSITION | MF_STRING | MF_UNCHECKED, 0, _G(STR_MENU_SECURE_BOOT));
	}

	ShowWindow(g_DialogHwnd, SW_HIDE);
	ShowWindow(g_DialogHwnd, SW_NORMAL);

	//Update check
	for (i = 0; i < g_language_count; i++)
	{
		CheckMenuItem(hMenu, VTOY_MENU_LANGUAGE_BEGIN | i, MF_BYCOMMAND | MF_STRING | MF_UNCHECKED);
	}
	CheckMenuItem(hMenu, VTOY_MENU_LANGUAGE_BEGIN | defaultLangId, MF_BYCOMMAND | MF_STRING | MF_CHECKED);
}

static void LanguageInit(void)
{
	int i, j, k;
	int id, DefaultId;
	WCHAR Language[64];
	WCHAR TmpBuf[256];
	LANGID LangId = GetSystemDefaultUILanguage();
	HMENU SubMenu;
	HMENU hMenu = GetMenu(g_DialogHwnd);

	SubMenu = GetSubMenu(hMenu, 1);
	DeleteMenu(SubMenu, 0, MF_BYPOSITION);

	g_language_data = (VENTOY_LANGUAGE *)malloc(sizeof(VENTOY_LANGUAGE)* VENTOY_MAX_LANGUAGE);
	memset(g_language_data, 0, sizeof(VENTOY_LANGUAGE)* VENTOY_MAX_LANGUAGE);

	swprintf_s(Language, 64, L"StringDefine");
	for (i = 0; i < STR_ID_MAX; i++)
	{
		swprintf_s(TmpBuf, 256, L"%d", i);
		GET_INI_STRING(TmpBuf, g_language_data[0].StrId[i]);
	}

	for (i = 0; i < VENTOY_MAX_LANGUAGE; i++)
	{
		swprintf_s(Language, 64, L"Language%d", i);
		GET_INI_STRING(TEXT("name"), g_language_data[i].Name);

		if (g_language_data[i].Name[0] == '#')
		{
			break;
		}

		g_language_count++;
		Log("Find Language%d ...", i);
		
		AppendMenu(SubMenu, MF_STRING | MF_BYCOMMAND, VTOY_MENU_LANGUAGE_BEGIN | i, g_language_data[i].Name);

		GET_INI_STRING(TEXT("FontFamily"), g_language_data[i].FontFamily);
		g_language_data[i].FontSize = GetPrivateProfileInt(Language, TEXT("FontSize"), 10, VENTOY_LANGUAGE_INI);

		for (j = 0; j < STR_ID_MAX; j++)
		{
			GET_INI_STRING(g_language_data[0].StrId[j], g_language_data[i].MsgString[j]);

			for (k = 0; g_language_data[i].MsgString[j][k] && g_language_data[i].MsgString[j][k + 1]; k++)
			{
				if (g_language_data[i].MsgString[j][k] == '#' && g_language_data[i].MsgString[j][k + 1] == '@')
				{
					g_language_data[i].MsgString[j][k] = '\r';
					g_language_data[i].MsgString[j][k + 1] = '\n';
				}
			}
		}
	}

	DefaultId = (MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED) == LangId) ? 0 : 1;
	id = GetPrivateProfileInt(TEXT("Ventoy"), TEXT("Language"), DefaultId, VENTOY_CFG_INI);
	if (id >= i)
	{
		id = DefaultId;
	}

	UpdateItemString(id);
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


	g_StaticDevHwnd = GetDlgItem(hWnd, IDC_STATIC_DEV);
	g_StaticLocalHwnd = GetDlgItem(hWnd, IDC_STATIC_LOCAL);
	g_StaticDiskHwnd = GetDlgItem(hWnd, IDC_STATIC_DISK);
	

    g_BtnUpdateHwnd = GetDlgItem(hWnd, IDC_BUTTON3);
    g_ProgressBarHwnd = GetDlgItem(hWnd, IDC_PROGRESS1);
    g_StaticStatusHwnd = GetDlgItem(hWnd, IDC_STATIC_STATUS);

    hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON1));
    SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

    SendMessage(g_ProgressBarHwnd, PBM_SETRANGE, (WPARAM)0, (LPARAM)(MAKELPARAM(0, PT_FINISH)));
    PROGRESS_BAR_SET_POS(PT_START);

	SetMenu(hWnd, LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_MENU1)));

	LoadCfgIni();
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
	if (rc)
	{
		Log("This time install failed, now wait and retry...");
		Sleep(10000);

		Log("Now retry to install...");

		rc = InstallVentoy2PhyDrive(pPhyDrive);
	}

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
	if (rc)
	{
		Log("This time update failed, now wait and retry...");
		Sleep(10000);

		Log("Now retry to update...");

		rc = UpdateVentoy2PhyDrive(pPhyDrive);
	}

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

static void MenuProc(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	WORD CtrlID;
	HMENU hMenu = GetMenu(hWnd);

	CtrlID = LOWORD(wParam);

	if (CtrlID == 0)
	{
		g_SecureBoot = !g_SecureBoot;

		if (g_SecureBoot)
		{
			CheckMenuItem(hMenu, 0, MF_BYCOMMAND | MF_STRING | MF_CHECKED);
		}
		else
		{
			CheckMenuItem(hMenu, 0, MF_BYCOMMAND | MF_STRING | MF_UNCHECKED);
		}
	}
	else if (CtrlID >= VTOY_MENU_LANGUAGE_BEGIN && CtrlID < VTOY_MENU_LANGUAGE_BEGIN + g_language_count)
	{
		UpdateItemString(CtrlID - VTOY_MENU_LANGUAGE_BEGIN);
	}
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

			if (lParam == 0 && NotifyCode == 0)
			{
				MenuProc(hWnd, wParam, lParam);
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
			WriteCfgIni();
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
		if (IsDirExist("grub"))
		{
			MessageBox(NULL, _G(STR_INCORRECT_DIR), _G(STR_ERROR), MB_OK | MB_ICONERROR);
		}
		else
		{
			MessageBox(NULL, _G(STR_INCORRECT_DIR), _G(STR_ERROR), MB_OK | MB_ICONERROR);
		}
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
