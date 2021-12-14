/******************************************************************************
 * DiskService_diskpart.c
 *
 * Copyright (c) 2021, longpanda <admin@ventoy.net>
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
#include "DiskService.h"

STATIC BOOL IsDiskpartExist(void)
{
    BOOL ret;

    ret = IsFileExist("C:\\Windows\\system32\\diskpart.exe");
    if (!ret)
    {
        Log("diskpart.exe not exist");
    }

    return ret;
}

STATIC BOOL DSPT_CommProc(const char *Cmd)
{
    CHAR CmdBuf[MAX_PATH];
    CHAR CmdFile[MAX_PATH];
    STARTUPINFOA Si;
    PROCESS_INFORMATION Pi;

    GetCurrentDirectoryA(sizeof(CmdBuf), CmdBuf);
    sprintf_s(CmdFile, sizeof(CmdFile), "%s\\ventoy\\diskpart_%u.txt", CmdBuf, GetCurrentProcessId());
    
    SaveBufToFile(CmdFile, Cmd, (int)strlen(Cmd));

    GetStartupInfoA(&Si);
    Si.dwFlags |= STARTF_USESHOWWINDOW;
    Si.wShowWindow = SW_HIDE;

    sprintf_s(CmdBuf, sizeof(CmdBuf), "C:\\Windows\\system32\\diskpart.exe /s \"%s\"", CmdFile);

    Log("CreateProcess <%s>", CmdBuf);
    CreateProcessA(NULL, CmdBuf, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);

    Log("Wair process ...");
    WaitForSingleObject(Pi.hProcess, INFINITE);
    Log("Process finished...");

    CHECK_CLOSE_HANDLE(Pi.hProcess);
    CHECK_CLOSE_HANDLE(Pi.hThread);

    DeleteFileA(CmdFile);
    return TRUE;
}

BOOL DSPT_CleanDisk(int DriveIndex)
{
    CHAR CmdBuf[128];

    Log("CleanDiskByDiskpart <%d>", DriveIndex);

    if (!IsDiskpartExist())
    {
        return FALSE;
    }

    sprintf_s(CmdBuf, sizeof(CmdBuf), "select disk %d\r\nclean\r\n", DriveIndex);
    return DSPT_CommProc(CmdBuf);
}
