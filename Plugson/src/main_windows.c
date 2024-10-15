#include <Windows.h>
#include <Shlobj.h>
#include <tlhelp32.h>
#include <Psapi.h>
#include <commctrl.h>
#include <resource.h>
#include <ventoy_define.h>
#include <ventoy_util.h>
#include <ventoy_json.h>
#include <ventoy_disk.h>
#include <ventoy_http.h>

char g_ventoy_dir[MAX_PATH];

static BOOL g_ChromeFirst = TRUE;
static BOOL g_running = FALSE;
static HWND g_refresh_button;
static HWND g_start_button;
static HWND g_openlink_button;
static HWND g_exit_button;
static HWND g_ComboxHwnd;

typedef enum MSGID
{
    MSGID_ERROR = 0,
	MSGID_INFO,
    MSGID_INVALID_DIR,
	MSGID_NEW_DIR_FAIL,
	MSGID_RENAME_VENTOY,
	MSGID_INTERNAL_ERR,
	MSGID_BTN_REFRESH,
	MSGID_BTN_START,
	MSGID_BTN_STOP,
	MSGID_BTN_LINK,
	MSGID_BTN_EXIT,

	MSGID_BTN_STOP_TIP, 
	MSGID_BTN_EXIT_TIP,
	MSGID_RUNNING_TIP,
	MSGID_NO_TARXZ_TIP,

    MSGID_BUTT
}MSGID;


const WCHAR *g_msg_cn[MSGID_BUTT] =
{
    L"错误",
	L"提醒",
    L"请在 Ventoy 盘根目录下运行本程序！（存放ISO文件的位置）",
	L"创建 ventoy 目录失败，无法继续！",
	L"ventoy 目录存在，但是大小写不匹配，请先将其重命名！",
	L"内部错误，程序即将退出！",
	L"刷新",
	L"启动",
	L"停止",
	L"链接",
	L"退出",

	L"停止运行后浏览器页面将会关闭，是否继续？",
	L"当前服务正在运行，是否退出？",
	L"请先关闭正在运行的 VentoyPlugson 程序！",
	L"ventoy\\plugson.tar.xz 文件不存在，请在正确的目录下运行！",
};
const WCHAR *g_msg_en[MSGID_BUTT] =
{
    L"Error",
	L"Info",
    L"Please run me at the root of Ventoy partition.",
	L"Failed to create ventoy directory!",
	L"ventoy directory case mismatch, please rename it first!",
	L"Internal error, the program will exit!",
	L"Refresh",
	L"Start",
	L"Stop",
	L"Link",
	L"Exit",

	L"The browser page will close after stop, continue?",
	L"Service is running, continue?",
	L"Please close another running VentoyPlugson instance!",
	L"ventoy\\plugson.tar.xz does not exist, please run under the correct directory!",
};

#define UTF8_Log(fmt, wstr) \
{\
    memset(TmpPathA, 0, sizeof(TmpPathA));\
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, TmpPathA, sizeof(TmpPathA), NULL, NULL);\
    vlog(fmt, TmpPathA);\
}


const WCHAR **g_msg_lang = NULL;

HINSTANCE g_hInst;

char g_log_file[MAX_PATH];
char g_cur_dir[MAX_PATH];

int ventoy_log_init(void);
void ventoy_log_exit(void);

static BOOL OnDestroyDialog()
{    
    ventoy_http_exit();
    ventoy_disk_exit();
#ifndef VENTOY_SIM        
    ventoy_www_exit();
#endif
    ventoy_log_exit();
    return TRUE;
}


static void OpenURL(void)
{
    int i;
	char url[128];
    const static char * Browsers[] = 
    {
        "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
        "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
        "C:\\Program Files (x86)\\Mozilla Firefox\\firefox.exe",
        "C:\\Program Files\\Mozilla Firefox\\firefox.exe",
        NULL
    };

	sprintf_s(url, sizeof(url), "http://%s:%s/index.html", g_sysinfo.ip, g_sysinfo.port);

	if (g_ChromeFirst)
	{
		for (i = 0; Browsers[i] != NULL; i++)
		{
			if (ventoy_is_file_exist("%s", Browsers[i]))
			{
				ShellExecuteA(NULL, "open", Browsers[i], url, NULL, SW_SHOW);
				return;
			}
		}
	}

    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOW);
}


static void FillCombox(HWND hWnd)
{
	int i = 0;
	int num = 0;
	const ventoy_disk *list = NULL;
	CHAR DeviceName[256];

	// delete all items
	SendMessage(g_ComboxHwnd, CB_RESETCONTENT, 0, 0);

	list = ventoy_get_disk_list(&num);
	if (NULL == list || num <= 0)
	{
		return;
	}

	for (i = 0; i < num; i++)
	{
		sprintf_s(DeviceName, sizeof(DeviceName),
			"%C: [%s] %s",
			list[i].devname[0],
			list[i].cur_capacity,
			list[i].cur_model);
		SendMessageA(g_ComboxHwnd, CB_ADDSTRING, 0, (LPARAM)DeviceName);
	}
	SendMessage(g_ComboxHwnd, CB_SETCURSEL, 0, 0);
}


static BOOL InitDialog(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    HICON hIcon;

	g_ComboxHwnd = GetDlgItem(hWnd, IDC_COMBO1);
	g_refresh_button = GetDlgItem(hWnd, IDC_BUTTON1);
	g_start_button = GetDlgItem(hWnd, IDC_BUTTON2);
	g_openlink_button = GetDlgItem(hWnd, IDC_BUTTON3);
	g_exit_button = GetDlgItem(hWnd, IDC_BUTTON4);
	
	hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON1));
    SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

	SetWindowTextW(g_refresh_button, g_msg_lang[MSGID_BTN_REFRESH]);
	SetWindowTextW(g_start_button, g_msg_lang[MSGID_BTN_START]);
	SetWindowTextW(g_openlink_button, g_msg_lang[MSGID_BTN_LINK]);
	SetWindowTextW(g_exit_button, g_msg_lang[MSGID_BTN_EXIT]);

	EnableWindow(g_openlink_button, FALSE);

	FillCombox(hWnd);

    return TRUE;
}

static void VentoyStopService()
{
    ventoy_http_stop();
}

static int VentoyStartService(int sel)
{
	int rc;
	BOOL bRet;
	char Path[128];
	char CurDir[MAX_PATH];
    const ventoy_disk *disk = NULL;

    vlog("VentoyStartService ...\n");
    
    disk = ventoy_get_disk_node(sel);
    if (disk == NULL)
    {
        return 1;
    }

    vlog("Start service at %C: %s %s ...\n", disk->devname[0], disk->cur_model, disk->cur_ventoy_ver);

    g_cur_dir[0] = disk->devname[0];
    g_cur_dir[1] = ':';
	g_cur_dir[2] = '\\';
    g_cur_dir[3] = 0;

    g_sysinfo.pathcase = disk->pathcase;
    g_sysinfo.cur_secureboot = disk->cur_secureboot;
    g_sysinfo.cur_part_style = disk->cur_part_style;
    strlcpy(g_sysinfo.cur_fsname, disk->cur_fsname);
    strlcpy(g_sysinfo.cur_capacity, disk->cur_capacity);
    strlcpy(g_sysinfo.cur_model, disk->cur_model);
    strlcpy(g_sysinfo.cur_ventoy_ver, disk->cur_ventoy_ver);

    bRet = SetCurrentDirectoryA(g_cur_dir);
	vlog("SetCurrentDirectoryA %u <%s>\n", bRet, g_cur_dir);
	
	CurDir[0] = 0;
	GetCurrentDirectoryA(sizeof(CurDir), CurDir);
	vlog("CurDir=<%s>\n", CurDir);

	if (strcmp(g_cur_dir, CurDir))
	{
		vlog("Failed to change current directory.");
	}

	g_cur_dir[2] = 0;

	if (ventoy_is_directory_exist("ventoy"))
    {
		if (g_sysinfo.pathcase)
		{
			vlog("ventoy directory already exist, check case sensitive.\n");
            strlcpy(Path, "ventoy");

			rc = ventoy_path_case(Path, 0);
			vlog("ventoy_path_case actual path is <%s> <count:%d>\n", Path, rc);

			if (rc)
			{
				vlog("ventoy directory case mismatch, rename<%s>--><%s>\n", Path, "ventoy");
				if (MoveFileA(Path, "ventoy"))
				{
					vlog("Rename <%s>--><%s> success\n", Path, "ventoy");
				}
				else
				{
					vlog("Rename <%s>--><%s> failed %u\n", Path, "ventoy", LASTERR);
					MessageBoxW(NULL, g_msg_lang[MSGID_RENAME_VENTOY], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
					return 1;
				}
			}
		}
		else
		{
			vlog("ventoy directory already exist, no need to check case sensitive.\n");
		}
    }
	else
	{
		if (CreateDirectoryA("ventoy", NULL))
		{
			vlog("Create ventoy directory success.\n");
		}
		else
		{
			vlog("Create ventoy directory failed %u.\n", LASTERR);
			MessageBoxW(NULL, g_msg_lang[MSGID_NEW_DIR_FAIL], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
			return 1;
		}
	}

    return ventoy_http_start(g_sysinfo.ip, g_sysinfo.port);
}

INT_PTR CALLBACK DialogProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	int rc;
	int nCurSel;
	WORD NotifyCode;
	WORD CtrlID;

    switch (Message)
    {
        case WM_NOTIFY:
        {
            UINT code = 0;
            UINT_PTR idFrom = 0;

            if (lParam)
            {
                code = ((LPNMHDR)lParam)->code;
                idFrom = ((LPNMHDR)lParam)->idFrom;
            }

            if (idFrom == IDC_SYSLINK1 && (NM_CLICK == code || NM_RETURN == code))
            {
                OpenURL();
            }
            break;
        }
		case WM_COMMAND:
		{
			NotifyCode = HIWORD(wParam);
			CtrlID = LOWORD(wParam);

			if (NotifyCode == BN_CLICKED)
			{
				if (CtrlID == IDC_BUTTON1)
				{
					if (!g_running)
					{
						//refresh
						ventoy_disk_exit();
						ventoy_disk_init();
						FillCombox(hWnd);
					}
				}
				else if (CtrlID == IDC_BUTTON2)
				{
					if (g_running)
					{
						if (IDYES == MessageBoxW(NULL, g_msg_lang[MSGID_BTN_STOP_TIP], g_msg_lang[MSGID_INFO], MB_YESNO | MB_ICONINFORMATION))
						{
						    VentoyStopService();

							g_running = FALSE;
							SetWindowTextW(g_start_button, g_msg_lang[MSGID_BTN_START]);
							EnableWindow(g_ComboxHwnd, TRUE);
							EnableWindow(g_refresh_button, TRUE);
							EnableWindow(g_openlink_button, FALSE);
						}
					}
					else
					{
                        nCurSel = (int)SendMessage(g_ComboxHwnd, CB_GETCURSEL, 0, 0);
                    	if (CB_ERR != nCurSel)
                    	{
							rc = VentoyStartService(nCurSel);
    						if (rc)
    						{
    							vlog("Ventoy failed to start http server, check %s for detail\n", g_log_file);
                                MessageBoxW(NULL, g_msg_lang[MSGID_INTERNAL_ERR], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
    						}
    						else
    						{
    							g_running = TRUE;
    							SetWindowTextW(g_start_button, g_msg_lang[MSGID_BTN_STOP]);

								EnableWindow(g_ComboxHwnd, FALSE);
    							EnableWindow(g_refresh_button, FALSE);
    							EnableWindow(g_openlink_button, TRUE);

                                OpenURL();
    						}
                    	}
					}
				}
				else if (CtrlID == IDC_BUTTON3)
				{
					if (g_running)
					{
						OpenURL();
					}
				}
				else if (CtrlID == IDC_BUTTON4)
				{
					if (g_running)
					{
						if (IDYES != MessageBoxW(NULL, g_msg_lang[MSGID_BTN_EXIT_TIP], g_msg_lang[MSGID_INFO], MB_YESNO | MB_ICONINFORMATION))
						{
							return 0;
						}

						ventoy_http_stop();
					}

					OnDestroyDialog();
					EndDialog(hWnd, 0);
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
			if (g_running)
			{
				if (IDYES != MessageBoxW(NULL, g_msg_lang[MSGID_BTN_EXIT_TIP], g_msg_lang[MSGID_INFO], MB_YESNO | MB_ICONINFORMATION))
				{
					return 0;
				}

				VentoyStopService();
			}

            OnDestroyDialog();
            EndDialog(hWnd, 0);
			break;
        }
    }

    return 0;
}

static int ParseCmdLine(LPSTR lpCmdLine, char *ip, char *port)
{
	int portnum;
	char *ipstart = ip; 
	char *pos;
	

	if (!lpCmdLine)
	{
		return 0;
	}

	pos = strstr(lpCmdLine, "-H");
	if (!pos)
	{
		pos = strstr(lpCmdLine, "-h");
	}

	if (pos)
	{
		pos += 2;
		while (*pos == ' ' || *pos == '\t')
		{
			pos++;
		}

		while (isdigit(*pos) || *pos == '.')
		{
			*ipstart++ = *pos++;
		}
	}


	pos = strstr(lpCmdLine, "-P");
	if (!pos)
	{
		pos = strstr(lpCmdLine, "-p");
	}

	if (pos)
	{
		portnum = (int)strtol(pos + 3, NULL, 10);
		sprintf_s(port, 16, "%d", portnum);
	}

	return 0;
}



//
//copy from Rufus
//Copyright © 2011-2021 Pete Batard <pete@akeo.ie>
//
#include <delayimp.h>
// For delay-loaded DLLs, use LOAD_LIBRARY_SEARCH_SYSTEM32 to avoid DLL search order hijacking.
FARPROC WINAPI dllDelayLoadHook(unsigned dliNotify, PDelayLoadInfo pdli)
{
	if (dliNotify == dliNotePreLoadLibrary) {
		// Windows 7 without KB2533623 does not support the LOAD_LIBRARY_SEARCH_SYSTEM32 flag.
		// That is is OK, because the delay load handler will interrupt the NULL return value
		// to mean that it should perform a normal LoadLibrary.
		return (FARPROC)LoadLibraryExA(pdli->szDll, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
	}
	return NULL;
}

#if defined(_MSC_VER)
// By default the Windows SDK headers have a `const` while MinGW does not.
const
#endif
PfnDliHook __pfnDliNotifyHook2 = dllDelayLoadHook;

typedef BOOL(WINAPI* SetDefaultDllDirectories_t)(DWORD);
static void DllProtect(void)
{
	SetDefaultDllDirectories_t pfSetDefaultDllDirectories = NULL;

	// Disable loading system DLLs from the current directory (sideloading mitigation)
	// PS: You know that official MSDN documentation for SetDllDirectory() that explicitly
	// indicates that "If the parameter is an empty string (""), the call removes the current
	// directory from the default DLL search order"? Yeah, that doesn't work. At all.
	// Still, we invoke it, for platforms where the following call might actually work...
	SetDllDirectoryA("");

	// For libraries on the KnownDLLs list, the system will always load them from System32.
	// For other DLLs we link directly to, we can delay load the DLL and use a delay load
	// hook to load them from System32. Note that, for this to work, something like:
	// 'somelib.dll;%(DelayLoadDLLs)' must be added to the 'Delay Loaded Dlls' option of
	// the linker properties in Visual Studio (which means this won't work with MinGW).
	// For all other DLLs, use SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32).
	// Finally, we need to perform the whole gymkhana below, where we can't call on
	// SetDefaultDllDirectories() directly, because Windows 7 doesn't have the API exposed.
	// Also, no, Coverity, we never need to care about freeing kernel32 as a library.
	// coverity[leaked_storage]

	pfSetDefaultDllDirectories = (SetDefaultDllDirectories_t)
		GetProcAddress(LoadLibraryW(L"kernel32.dll"), "SetDefaultDllDirectories");
	if (pfSetDefaultDllDirectories != NULL)
		pfSetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
	int i;
    int rc;
	int status = 0;
	HANDLE hMutex;
	WCHAR* Pos = NULL;
	WCHAR CurDir[MAX_PATH];
	WCHAR ExePath[MAX_PATH];
	WCHAR CurDirBk[MAX_PATH];
	WCHAR ExePathBk[MAX_PATH];
	CHAR TmpPathA[MAX_PATH];

    UNREFERENCED_PARAMETER(hPrevInstance);

	for (i = 0; i < __argc; i++)
	{
		if (__argv[i] && _stricmp(__argv[i], "/F") == 0)
		{
			g_ChromeFirst = FALSE;
			break;
		}
	}

	DllProtect();

    if (GetUserDefaultUILanguage() == 0x0804)
    {
        g_sysinfo.language = LANGUAGE_CN;
        g_msg_lang = g_msg_cn;
    }
    else
    {
        g_sysinfo.language = LANGUAGE_EN;
        g_msg_lang = g_msg_en;
    }

	hMutex = CreateMutexA(NULL, TRUE, "PlugsonMUTEX");
	if ((hMutex != NULL) && (GetLastError() == ERROR_ALREADY_EXISTS))
	{
		MessageBoxW(NULL, g_msg_lang[MSGID_RUNNING_TIP], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
		return 1;
	}

	GetCurrentDirectoryW(MAX_PATH, CurDir);
	GetCurrentDirectoryW(MAX_PATH, CurDirBk);
	GetModuleFileNameW(NULL, ExePath, MAX_PATH);
	GetModuleFileNameW(NULL, ExePathBk, MAX_PATH);

	for (Pos = NULL, i = 0; i < MAX_PATH && ExePath[i]; i++)
	{
		if (ExePath[i] == '\\' || ExePath[i] == '/')
		{
			Pos = ExePath + i;
		}
	}

	if (Pos)
	{
		*Pos = 0;
		if (wcscmp(CurDir, ExePath))
		{
			status |= 1;
			SetCurrentDirectoryW(ExePath);
			GetCurrentDirectoryW(MAX_PATH, CurDir);
		}
		else
		{
			status |= 2;
		}
	}

	Pos = wcsstr(CurDir, L"\\altexe");
	if (Pos)
	{
		*Pos = 0;
		status |= 4;
		SetCurrentDirectoryW(CurDir);
	}


	WideCharToMultiByte(CP_UTF8, 0, CurDir, -1, g_cur_dir, MAX_PATH, NULL, 0);

	sprintf_s(g_ventoy_dir, sizeof(g_ventoy_dir), "%s", g_cur_dir);
	sprintf_s(g_log_file, sizeof(g_log_file), "%s", LOG_FILE);
	ventoy_log_init();

	vlog("====================== Ventoy Plugson =========================\n");

	UTF8_Log("Current Directory <%s>\n", CurDirBk);
	UTF8_Log("Exe file path <%s>\n", ExePathBk);
	
	if (status & 1)
	{
		UTF8_Log("Change current dir to exe <%s>\n", ExePath);
	}
	if (status & 2)
	{
		vlog("Current directory check OK.\n");
	}
	if (status & 4)
	{
		UTF8_Log("altexe detected, change current dir to <%s>\n", CurDir);
	}


    if (!ventoy_is_file_exist("%s\\ventoy\\%s", g_ventoy_dir, PLUGSON_TXZ))
    {        
		MessageBoxW(NULL, g_msg_lang[MSGID_NO_TARXZ_TIP], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);
        return 1;
    }

	ParseCmdLine(lpCmdLine, g_sysinfo.ip, g_sysinfo.port);
	if (g_sysinfo.ip[0] == 0)
	{
		strlcpy(g_sysinfo.ip, "127.0.0.1");
	}
	if (g_sysinfo.port[0] == 0)
	{
		strlcpy(g_sysinfo.port, "24681");
	}

	vlog("===============================================\n");
	vlog("========= Ventoy Plugson %s:%s =========\n", g_sysinfo.ip, g_sysinfo.port);
	vlog("===============================================\n");


	ventoy_disk_init();
#ifndef VENTOY_SIM        
    rc = ventoy_www_init();
	if (rc)
	{
		vlog("Failed to init www\n");
		MessageBoxW(NULL, g_msg_lang[MSGID_INTERNAL_ERR], g_msg_lang[MSGID_ERROR], MB_OK | MB_ICONERROR);

		ventoy_disk_exit();
		ventoy_log_exit();
		return 1;
	}
#endif
    ventoy_http_init();

    g_hInst = hInstance;
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DialogProc);

    return 0;
}

