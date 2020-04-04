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
    STR_WARNING,
    STR_INFO,
    STR_INCORRECT_DIR,

    STR_DEVICE,
    STR_LOCAL_VER,
    STR_DISK_VER,
    STR_STATUS,
    STR_INSTALL,
    STR_UPDATE,

    STR_UPDATE_TIP,
    STR_INSTALL_TIP,
    STR_INSTALL_TIP2,

    STR_INSTALL_SUCCESS,
    STR_INSTALL_FAILED,
    STR_UPDATE_SUCCESS,
    STR_UPDATE_FAILED,

    STR_WAIT_PROCESS,


    STR_ID_MAX
}STR_ID;

const TCHAR * GetString(enum STR_ID ID);

#define _G(a) GetString(a)

#endif
