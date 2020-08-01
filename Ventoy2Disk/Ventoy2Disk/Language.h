/******************************************************************************
 * Language.h
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
 
#ifndef __LANGUAGE_H__
#define __LANGUAGE_H__


typedef enum STR_ID
{
	STR_ERROR = 0,
	STR_WARNING, // 1
	STR_INFO, // 2
	STR_INCORRECT_DIR, //3
	STR_INCORRECT_TREE_DIR, //4

	STR_DEVICE, //5
	STR_LOCAL_VER, //6
	STR_DISK_VER, //7
	STR_STATUS, //8
	STR_INSTALL, //9
	STR_UPDATE, //10

	STR_UPDATE_TIP, //11
	STR_INSTALL_TIP, //12
	STR_INSTALL_TIP2,//13

	STR_INSTALL_SUCCESS,//14
	STR_INSTALL_FAILED,//15
	STR_UPDATE_SUCCESS,//16
	STR_UPDATE_FAILED,//17

	STR_WAIT_PROCESS,//18

	STR_MENU_OPTION,//19
	STR_MENU_SECURE_BOOT,//20

    STR_MENU_PART_CFG,//21
    STR_BTN_OK,//22
    STR_BTN_CANCEL,//23
    STR_PRESERVE_SPACE,//24
	STR_SPACE_VAL_INVALID,//25

    STR_MENU_CLEAR, //26
    STR_CLEAR_SUCCESS, //27
    STR_CLEAR_FAILED, //28
    STR_MENU_PART_STYLE, //29
	STR_DISK_2TB_MBR_ERROR,//30


	STR_ID_MAX
}STR_ID;

extern BOOL g_SecureBoot;

#define VTOY_MENU_SECURE_BOOT	  0xA000
#define VTOY_MENU_PART_CFG        0xA001
#define VTOY_MENU_CLEAN           0xA002
#define VTOY_MENU_PART_STYLE      0xA003
#define VTOY_MENU_PART_MBR        0xA004
#define VTOY_MENU_PART_GPT        0xA005


typedef enum OPT_SUBMENU
{
    OPT_SUBMENU_SECURE_BOOT = 0,
    OPT_SUBMENU_PART_STYLE,
    OPT_SUBMENU_PART_CFG,
    OPT_SUBMENU_CLEAR,

    OPT_SUBMENU_MAX
}OPT_SUBMENU;


#define VTOY_MENU_LANGUAGE_BEGIN  0xB000


#define VENTOY_LANGUAGE_INI  TEXT(".\\ventoy\\languages.ini")
#define VENTOY_CFG_INI     TEXT(".\\Ventoy2Disk.ini")
#define VENTOY_CFG_INI_A   ".\\Ventoy2Disk.ini"
#define VENTOY_MAX_LANGUAGE	 200

#define GET_INI_STRING(Section, Key, Buf) GetPrivateProfileString(Section, Key, TEXT("#"), Buf, sizeof(Buf), VENTOY_LANGUAGE_INI)

typedef struct VENTOY_LANGUAGE
{
	WCHAR Name[128];
	WCHAR FontFamily[64];
	int FontSize;

	WCHAR StrId[STR_ID_MAX][64];
	WCHAR MsgString[STR_ID_MAX][1024];

}VENTOY_LANGUAGE;

extern VENTOY_LANGUAGE *g_cur_lang_data;

const TCHAR * GetString(enum STR_ID ID);

#define _G(a) GetString(a)

#endif
