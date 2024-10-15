/******************************************************************************
 * VtoyUtil.h
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
 
#ifndef __VTOYUTIL_H__
#define __VTOYUTIL_H__

#pragma pack(1)

typedef EFI_STATUS (*VTOY_UTIL_PROC_PF)(IN EFI_HANDLE ImageHandle, IN CONST CHAR16 *CmdLine);
typedef int (*grub_env_set_pf)(const char *name, const char *val);
typedef const char * (*grub_env_get_pf)(const char *name);
typedef int (*grub_env_printf_pf)(const char *fmt, ...);

#define VTOY_MAX_CONF_REPLACE    2

typedef struct ventoy_grub_param_file_replace
{
    UINT32 magic;
    char   old_file_name[4][256];
    UINT32 old_file_cnt;
    UINT32 new_file_virtual_id;
}ventoy_grub_param_file_replace;

typedef struct ventoy_grub_param
{
    grub_env_get_pf grub_env_get;
    grub_env_set_pf grub_env_set;
    ventoy_grub_param_file_replace file_replace;
    ventoy_grub_param_file_replace img_replace[VTOY_MAX_CONF_REPLACE];
    grub_env_printf_pf grub_env_printf;    
}ventoy_grub_param;
#pragma pack()


typedef struct VtoyUtilFeature
{
    CONST CHAR16 *Cmd;    
    VTOY_UTIL_PROC_PF MainProc;
}VtoyUtilFeature;

extern BOOLEAN gVtoyDebugPrint;
VOID EFIAPI VtoyUtilDebug(IN CONST CHAR8  *Format, ...);
#define debug(expr, ...) if (gVtoyDebugPrint) VtoyUtilDebug("[VTOY] "expr"\n", ##__VA_ARGS__)
#define Printf VtoyUtilDebug

EFI_STATUS VtoyGetComponentName(IN UINTN Ver, IN VOID *Protocol, OUT CHAR16 **DriverName);
EFI_STATUS FixWindowsMemhole(IN EFI_HANDLE    ImageHandle, IN CONST CHAR16 *CmdLine);
EFI_STATUS ShowEfiDrivers(IN EFI_HANDLE    ImageHandle, IN CONST CHAR16 *CmdLine);

#endif

