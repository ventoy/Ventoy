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

    ret = IsFileExist("%s\\system32\\diskpart.exe", DISK_GetWindowsDir());
    if (!ret)
    {
        Log("diskpart.exe not exist");
    }

    return ret;
}


STATIC BOOL IsCmdExist(void)
{
    BOOL ret;

    ret = IsFileExist("%s\\system32\\cmd.exe", DISK_GetWindowsDir());
    if (!ret)
    {
        Log("cmd.exe not exist");
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


STATIC BOOL CMD_CommProc(char* Cmd)
{
    STARTUPINFOA Si;
    PROCESS_INFORMATION Pi;

    GetStartupInfoA(&Si);
    Si.dwFlags |= STARTF_USESHOWWINDOW;
    Si.wShowWindow = SW_HIDE;

    Log("CreateProcess <%s>", Cmd);
    CreateProcessA(NULL, Cmd, NULL, NULL, FALSE, 0, NULL, NULL, &Si, &Pi);

    Log("Wair process ...");
    WaitForSingleObject(Pi.hProcess, INFINITE);
    Log("Process finished...");

    CHECK_CLOSE_HANDLE(Pi.hProcess);
    CHECK_CLOSE_HANDLE(Pi.hThread);

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

BOOL DSPT_FormatVolume(char DriveLetter, int fs, DWORD ClusterSize)
{
    const char* fsname = NULL;
    CHAR CmdBuf[256];
    CHAR FsName[128];

    Log("FormatVolumeByDiskpart <%C:>", DriveLetter);

    if (!IsDiskpartExist())
    {
        return FALSE;
    }

    fsname = GetVentoyFsFmtNameByTypeA(fs);

    if (ClusterSize > 0)
    {
        sprintf_s(CmdBuf, sizeof(CmdBuf), "select volume %C:\r\nformat FS=%s LABEL=Ventoy UNIT=%u QUICK OVERRIDE\r\n", DriveLetter, fsname, ClusterSize);
    }
    else
    {
        sprintf_s(CmdBuf, sizeof(CmdBuf), "select volume %C:\r\nformat FS=%s LABEL=Ventoy QUICK OVERRIDE\r\n", DriveLetter, fsname);
    }

    Log("Diskpart cmd:<%s>", CmdBuf);

    DSPT_CommProc(CmdBuf);

    sprintf_s(CmdBuf, sizeof(CmdBuf), "%C:\\", DriveLetter);
    GetVolumeInformationA(CmdBuf, NULL, 0, NULL, NULL, NULL, FsName, sizeof(FsName));
    VentoyStringToUpper(FsName);

    Log("New fs name after run diskpart:<%s>", FsName);

    if (strcmp(FsName, fsname) == 0)
    {
        Log("FormatVolumeByDiskpart <%C:> SUCCESS", DriveLetter);
        return TRUE;
    }
    else
    {
        Log("FormatVolumeByDiskpart <%C:> FAILED", DriveLetter);
        return FALSE;
    }
}


BOOL CMD_FormatVolume(char DriveLetter, int fs, DWORD ClusterSize)
{
    const char* fsname = NULL;
    CHAR CmdBuf[256];
    CHAR FsName[128];

    Log("FormatVolumeByCmd <%C:>", DriveLetter);

    if (!IsCmdExist())
    {
        return FALSE;
    }

    fsname = GetVentoyFsFmtNameByTypeA(fs);

    if (ClusterSize > 0)
    {
        sprintf_s(CmdBuf, sizeof(CmdBuf), "cmd.exe /c \"echo Y|format %C: /V:Ventoy /fs:%s /q /A:%u /X\"", DriveLetter, fsname, ClusterSize);
    }
    else
    {
        sprintf_s(CmdBuf, sizeof(CmdBuf), "cmd.exe /c \"echo Y|format %C: /V:Ventoy /fs:%s /q /X\"", DriveLetter, fsname);
    }

    Log("Cmd.exe <%s>", CmdBuf);

    CMD_CommProc(CmdBuf);

    sprintf_s(CmdBuf, sizeof(CmdBuf), "%C:\\", DriveLetter);
    GetVolumeInformationA(CmdBuf, NULL, 0, NULL, NULL, NULL, FsName, sizeof(FsName));
    VentoyStringToUpper(FsName);

    Log("New fs name after run cmd.exe:<%s>", FsName);

    if (strcmp(FsName, fsname) == 0)
    {
        Log("FormatVolumeByCmd <%C:> SUCCESS", DriveLetter);
        return TRUE;
    }
    else
    {
        Log("FormatVolumeByCmd <%C:> FAILED", DriveLetter);
        return FALSE;
    }
}
