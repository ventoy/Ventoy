/******************************************************************************
* AlertSuppress.c
*
* Copyright (c) 2022, longpanda <admin@ventoy.net>
* Copyright (c) 2011-2022 Pete Batard <pete@akeo.ie>
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
#include <winternl.h>
#include <commctrl.h>
#include <initguid.h>
#include <vds.h>
#include "Ventoy2Disk.h"


#define sfree(p) do {if (p != NULL) {free((void*)(p)); p = NULL;}} while(0)
#define wconvert(p)     wchar_t* w ## p = utf8_to_wchar(p)
#define walloc(p, size) wchar_t* w ## p = (p == NULL)?NULL:(wchar_t*)calloc(size, sizeof(wchar_t))
#define wfree(p) sfree(w ## p)

#define static_strcpy(dst, src) strcpy_s(dst, sizeof(dst), src)
#define static_strcat(dst, src) strcat_s(dst, sizeof(dst), src)


#define wchar_to_utf8_no_alloc(wsrc, dest, dest_size) \
	WideCharToMultiByte(CP_UTF8, 0, wsrc, -1, dest, dest_size, NULL, NULL)
#define utf8_to_wchar_no_alloc(src, wdest, wdest_size) \
	MultiByteToWideChar(CP_UTF8, 0, src, -1, wdest, wdest_size)


/*
 * Converts an UTF8 string to UTF-16 (allocate returned string)
 * Returns NULL on error
 */
static __inline wchar_t* utf8_to_wchar(const char* str)
{
	int size = 0;
	wchar_t* wstr = NULL;

	if (str == NULL)
		return NULL;

	// Convert the empty string too
	if (str[0] == 0)
		return (wchar_t*)calloc(1, sizeof(wchar_t));

	// Find out the size we need to allocate for our converted string
	size = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
	if (size <= 1)	// An empty string would be size 1
		return NULL;

	if ((wstr = (wchar_t*)calloc(size, sizeof(wchar_t))) == NULL)
		return NULL;

	if (utf8_to_wchar_no_alloc(str, wstr, size) != size) {
		sfree(wstr);
		return NULL;
	}
	return wstr;
}

static char g_FormatDiskTitle[256];
static char g_FormatDiskButton[256];

static char g_LocNotAvaliableTitle[256]; //Location is not available
static char g_InsertDiskTitle[256]; // Insert disk

static char system_dir[MAX_PATH], sysnative_dir[MAX_PATH];

static HWINEVENTHOOK ap_weh = NULL;


static __inline UINT GetSystemDirectoryU(char* lpBuffer, UINT uSize)
{
	UINT ret = 0, err = ERROR_INVALID_DATA;
	// coverity[returned_null]
	walloc(lpBuffer, uSize);
	ret = GetSystemDirectoryW(wlpBuffer, uSize);
	err = GetLastError();
	if ((ret != 0) && ((ret = wchar_to_utf8_no_alloc(wlpBuffer, lpBuffer, uSize)) == 0)) {
		err = GetLastError();
	}
	wfree(lpBuffer);
	SetLastError(err);
	return ret;
}


static char* ToLocaleName(DWORD lang_id)
{
	static char mui_str[LOCALE_NAME_MAX_LENGTH];
	wchar_t wmui_str[LOCALE_NAME_MAX_LENGTH];

	if (LCIDToLocaleName(lang_id, wmui_str, LOCALE_NAME_MAX_LENGTH, 0) > 0) {
		wchar_to_utf8_no_alloc(wmui_str, mui_str, LOCALE_NAME_MAX_LENGTH);
	}
	else {
		static_strcpy(mui_str, "en-US");
	}
	return mui_str;
}

static __inline int LoadStringU(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int nBufferMax)
{
	int ret;
	DWORD err = ERROR_INVALID_DATA;
	if (nBufferMax == 0) {
		// read-only pointer to resource mode is not supported
		SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}
	// coverity[returned_null]
	walloc(lpBuffer, nBufferMax);
	ret = LoadStringW(hInstance, uID, wlpBuffer, nBufferMax);
	err = GetLastError();
	if ((ret > 0) && ((ret = wchar_to_utf8_no_alloc(wlpBuffer, lpBuffer, nBufferMax)) == 0)) {
		err = GetLastError();
	}
	wfree(lpBuffer);
	SetLastError(err);
	return ret;
}

/*
 * typedefs for the function prototypes. Use the something like:
 *   PF_DECL(FormatEx);
 * which translates to:
 *   FormatEx_t pfFormatEx = NULL;
 * in your code, to declare the entrypoint and then use:
 *   PF_INIT(FormatEx, Fmifs);
 * which translates to:
 *   pfFormatEx = (FormatEx_t) GetProcAddress(GetDLLHandle("fmifs"), "FormatEx");
 * to make it accessible.
 */
#define         MAX_LIBRARY_HANDLES 64
extern HMODULE  OpenedLibrariesHandle[MAX_LIBRARY_HANDLES];
extern UINT16 OpenedLibrariesHandleSize;
#define         OPENED_LIBRARIES_VARS HMODULE OpenedLibrariesHandle[MAX_LIBRARY_HANDLES]; uint16_t OpenedLibrariesHandleSize = 0
#define         CLOSE_OPENED_LIBRARIES while(OpenedLibrariesHandleSize > 0) FreeLibrary(OpenedLibrariesHandle[--OpenedLibrariesHandleSize])

static __inline HMODULE GetLibraryHandle(char* szLibraryName) {
	HMODULE h = NULL;
	wchar_t* wszLibraryName = NULL;
	int size;
	if (szLibraryName == NULL || szLibraryName[0] == 0)
		goto out;
	size = MultiByteToWideChar(CP_UTF8, 0, szLibraryName, -1, NULL, 0);
	if ((size <= 1) || ((wszLibraryName = (wchar_t*)calloc(size, sizeof(wchar_t))) == NULL) ||
		(MultiByteToWideChar(CP_UTF8, 0, szLibraryName, -1, wszLibraryName, size) != size))
		goto out;
	// If the library is already opened, just return a handle (that doesn't need to be freed)
	if ((h = GetModuleHandleW(wszLibraryName)) != NULL)
		goto out;
	// Sanity check
	if (OpenedLibrariesHandleSize >= MAX_LIBRARY_HANDLES) {
		Log("Error: MAX_LIBRARY_HANDLES is too small");
		goto out;
	}
	h = LoadLibraryExW(wszLibraryName, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
	// Some Windows 7 platforms (most likely the ones missing KB2533623 per the
	// official LoadLibraryEx doc) can return ERROR_INVALID_PARAMETER when using
	// the Ex() version. If that's the case, fallback to using LoadLibraryW().
	if ((h == NULL) && (SCODE_CODE(GetLastError()) == ERROR_INVALID_PARAMETER))
		h = LoadLibraryW(wszLibraryName);
	if (h != NULL)
		OpenedLibrariesHandle[OpenedLibrariesHandleSize++] = h;
	else
		Log("Unable to load '%S.dll': %u", wszLibraryName, LASTERR);
out:
	free(wszLibraryName);
	return h;
}

#define PF_TYPE(api, ret, proc, args)		typedef ret (api *proc##_t)args
#define PF_DECL(proc)						static proc##_t pf##proc = NULL
#define PF_TYPE_DECL(api, ret, proc, args)	PF_TYPE(api, ret, proc, args); PF_DECL(proc)
#define PF_INIT(proc, name)					if (pf##proc == NULL) pf##proc = \
	(proc##_t) GetProcAddress(GetLibraryHandle(#name), #proc)
#define PF_INIT_OR_OUT(proc, name)			do {PF_INIT(proc, name);         \
	if (pf##proc == NULL) {Log("Unable to locate %s() in '%s.dll': %u",  \
	#proc, #name, LASTERR); goto out;} } while(0)
#define PF_INIT_OR_SET_STATUS(proc, name)	do {PF_INIT(proc, name);         \
	if ((pf##proc == NULL) && (NT_SUCCESS(status))) status = STATUS_NOT_IMPLEMENTED; } while(0)

static BOOL is_x64(void)
{
	BOOL ret = FALSE;
	PF_TYPE_DECL(WINAPI, BOOL, IsWow64Process, (HANDLE, PBOOL));
	// Detect if we're running a 32 or 64 bit system
	if (sizeof(uintptr_t) < 8) {
		PF_INIT(IsWow64Process, Kernel32);
		if (pfIsWow64Process != NULL) {
			(*pfIsWow64Process)(GetCurrentProcess(), &ret);
		}
	}
	else {
		ret = TRUE;
	}
	return ret;
}


static __inline UINT GetSystemWindowsDirectoryU(char* lpBuffer, UINT uSize)
{
	UINT ret = 0, err = ERROR_INVALID_DATA;
	// coverity[returned_null]
	walloc(lpBuffer, uSize);
	ret = GetSystemWindowsDirectoryW(wlpBuffer, uSize);
	err = GetLastError();
	if ((ret != 0) && ((ret = wchar_to_utf8_no_alloc(wlpBuffer, lpBuffer, uSize)) == 0)) {
		err = GetLastError();
	}
	wfree(lpBuffer);
	SetLastError(err);
	return ret;
}

static __inline HMODULE LoadLibraryU(LPCSTR lpFileName)
{
	HMODULE ret;
	DWORD err = ERROR_INVALID_DATA;
	wconvert(lpFileName);
	ret = LoadLibraryW(wlpFileName);
	err = GetLastError();
	wfree(lpFileName);
	SetLastError(err);
	return ret;
}

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
#pragma pack()

static BOOL LoadDialogCaption(HMODULE hMui, DWORD ID, CHAR* title, DWORD len)
{
	BOOL bRet = FALSE;
	int WordNum = 0;
	HRSRC hDlg = NULL;
	DLGTEMPLATEEX* pDlgTempEx = NULL;
	HGLOBAL hTemplate = NULL;
	WORD* pWordData = NULL;

	hDlg = FindResource(hMui, MAKEINTRESOURCE(1024), RT_DIALOG);
	if (hDlg)
	{
		hTemplate = LoadResource(hMui, hDlg);
		if (hTemplate)
		{
			pDlgTempEx = (DLGTEMPLATEEX*)LockResource(hTemplate);
			if (pDlgTempEx)
			{
				if (pDlgTempEx->signature != 0xFFFF)
				{
					return FALSE;
				}

				pWordData = (WORD *)(pDlgTempEx + 1);
				
				//skip menu
				if (*pWordData == 0x0000)
				{
					pWordData += 1;
				}
				else if (*pWordData == 0xFFFF)
				{
					pWordData += 2;
				}
				else
				{
					while (*pWordData++)
					{
						;
					}
				}

				//skip windowClass
				if (*pWordData == 0x0000)
				{
					pWordData += 1;
				}
				else if (*pWordData == 0xFFFF)
				{
					pWordData += 2;
				}
				else
				{
					while (*pWordData++)
					{
						;
					}
				}

				wchar_to_utf8_no_alloc(pWordData, title, len);
				bRet = TRUE;
			}
		}
	}

	return bRet;
}

BOOL SetAlertPromptMessages(void)
{
	HMODULE hMui;
	char mui_path[MAX_PATH];

	if (GetSystemDirectoryU(system_dir, sizeof(system_dir)) == 0) {
		Log("Could not get system directory: %u", LASTERR);
		static_strcpy(system_dir, "C:\\Windows\\System32");
	}

	// Construct Sysnative ourselves as there is no GetSysnativeDirectory() call
	// By default (64bit app running on 64 bit OS or 32 bit app running on 32 bit OS)
	// Sysnative and System32 are the same
	static_strcpy(sysnative_dir, system_dir);
	// But if the app is 32 bit and the OS is 64 bit, Sysnative must differ from System32
#if (defined(VTARCH_X86) || defined(VTARCH_ARM))
	if (is_x64()) {
		if (GetSystemWindowsDirectoryU(sysnative_dir, sizeof(sysnative_dir)) == 0) {
			Log("Could not get Windows directory: %u", LASTERR);
			static_strcpy(sysnative_dir, "C:\\Windows");
		}
		static_strcat(sysnative_dir, "\\Sysnative");
	}
#endif

	Log("system_dir=<%s>", system_dir);
	Log("sysnative_dir=<%s>", sysnative_dir);

	sprintf_s(mui_path, MAX_PATH, "%s\\%s\\shell32.dll.mui", sysnative_dir, ToLocaleName(GetUserDefaultUILanguage()));

	hMui = LoadLibraryU(mui_path);
	if (hMui)
	{
		Log("LoadLibrary shell32.dll.mui SUCCESS");
	}
	else
	{
		Log("LoadLibrary shell32.dll.mui FAILED");
		return FALSE;
	}


	// String Table:
	// 4097 = "You need to format the disk in drive %c: before you can use it." (dialog text)
	// 4125 = "Microsoft Windows" (dialog title)
	// 4126 = "Format disk" (button)
	if (LoadStringU(hMui, 4125, g_FormatDiskTitle, sizeof(g_FormatDiskTitle)) <= 0) {
		static_strcpy(g_FormatDiskTitle, "Microsoft Windows");
		Log("Warning: Could not locate localized format prompt title string in '%s': %u", mui_path, LASTERR);
	}

	if (LoadStringU(hMui, 4126, g_FormatDiskButton, sizeof(g_FormatDiskButton)) <= 0) {
		static_strcpy(g_FormatDiskButton, "Format disk");
		Log("Warning: Could not locate localized format prompt button string in '%s': %u", mui_path, LASTERR);
	}

	// 32964 = "Location is not available"
	if (LoadStringU(hMui, 32964, g_LocNotAvaliableTitle, sizeof(g_LocNotAvaliableTitle)) <= 0) {
		static_strcpy(g_LocNotAvaliableTitle, "Location is not available");
		Log("Warning: Could not locate localized format prompt title string in '%s': %u", mui_path, LASTERR);
	}

	if (!LoadDialogCaption(hMui, 1024, g_InsertDiskTitle, sizeof(g_InsertDiskTitle)))
	{
		static_strcpy(g_InsertDiskTitle, "Insert disk");
		Log("Warning: Could not locate insert disk title string in '%s': %u", mui_path, LASTERR);
	}

	FreeLibrary(hMui);
	return TRUE;
}

static __inline int GetWindowTextU(HWND hWnd, char* lpString, int nMaxCount)
{
	int ret = 0;
	DWORD err = ERROR_INVALID_DATA;
	if (nMaxCount < 0)
		return 0;
	// Handle the empty string as GetWindowTextW() returns 0 then
	if ((lpString != NULL) && (nMaxCount > 0))
		lpString[0] = 0;
	// coverity[returned_null]
	walloc(lpString, nMaxCount);
	ret = GetWindowTextW(hWnd, wlpString, nMaxCount);
	err = GetLastError();
	// coverity[var_deref_model]
	if ((ret != 0) && ((ret = wchar_to_utf8_no_alloc(wlpString, lpString, nMaxCount)) == 0)) {
		err = GetLastError();
	}
	wfree(lpString);
	lpString[nMaxCount - 1] = 0;
	SetLastError(err);
	return ret;
}


/*
 * The following function calls are used to automatically detect and close the native
 * Windows format prompt "You must format the disk in drive X:". To do that, we use
 * an event hook that gets triggered whenever a window is placed in the foreground.
 * In that hook, we look for a dialog that has style WS_POPUPWINDOW and has the relevant
 * title. However, because the title in itself is too generic (the expectation is that
 * it will be "Microsoft Windows") we also enumerate all the child controls from that
 * prompt, using another callback, until we find one that contains the text we expect
 * for the "Format disk" button.
 * Oh, and since all of these strings are localized, we must first pick them up from
 * the relevant mui's.
 */
static BOOL CALLBACK AlertPromptCallback(HWND hWnd, LPARAM lParam)
{
	char str[128];
	BOOL* found = (BOOL*)lParam;

	if (GetWindowTextU(hWnd, str, sizeof(str)) == 0)
		return TRUE;
	if (strcmp(str, g_FormatDiskButton) == 0)
		*found = TRUE;
	return TRUE;
}

static volatile BOOL g_AlertPromptHookEnable = FALSE;

void SetAlertPromptHookEnable(BOOL enable)
{
	g_AlertPromptHookEnable = enable;
}

static void CALLBACK AlertPromptHook(HWINEVENTHOOK hWinEventHook, DWORD Event, HWND hWnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
	char str[128];
	BOOL found;

	if (Event != EVENT_SYSTEM_FOREGROUND)
	{
		return;
	}

	if (!g_AlertPromptHookEnable)
	{
		return;
	}

	//GetWindowTextU(hWnd, str, sizeof(str));
	//Log("###### EVENT_SYSTEM_FOREGROUND Windows prompt <%s> #######", str);

	if (GetWindowLongPtr(hWnd, GWL_STYLE) & WS_POPUPWINDOW) {
		str[0] = 0;
		GetWindowTextU(hWnd, str, sizeof(str));
		if (strcmp(str, g_FormatDiskTitle) == 0)
		{
			found = FALSE;
			EnumChildWindows(hWnd, AlertPromptCallback, (LPARAM)&found);
			if (found)
			{
				SendMessage(hWnd, WM_COMMAND, (WPARAM)IDCANCEL, (LPARAM)0);
				Log("###### Detect 'Windows format' prompt, now close it. #######");
			}
		}
		else if (strcmp(str, g_LocNotAvaliableTitle) == 0)
		{
			SendMessage(hWnd, WM_COMMAND, (WPARAM)IDCANCEL, (LPARAM)0);
			Log("###### Detect 'Location is not available' prompt, now close it. #######");
		}
		else if (strcmp(str, g_InsertDiskTitle) == 0)
		{
			SendMessage(hWnd, WM_COMMAND, (WPARAM)IDCANCEL, (LPARAM)0);
			Log("###### Detect 'Insert disk' prompt, now close it. #######");
		}
	}
}


BOOL SetAlertPromptHook(void)
{
	if (ap_weh != NULL)
		return TRUE;	// No need to set again if active
	ap_weh = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL,
		AlertPromptHook, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
	return (ap_weh != NULL);
}

BOOL AlertSuppressInit(void)
{
	BOOL bRet;

	SetAlertPromptMessages();

	bRet = SetAlertPromptHook();
	Log("SetAlertPromptHook %s", bRet ? "SUCCESS" : "FAILED");

	return TRUE;
}