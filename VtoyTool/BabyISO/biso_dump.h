/******************************************************************************
 * biso_dump.h
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

#ifndef __BISO_DUMP_H__
#define __BISO_DUMP_H__

#define BISO_DUMP_INT(Name, Value)  BISO_DUMP("%-24s : %u\n", Name, Value)
#define BISO_DUMP_STR(Name, Value, szBuf) \
    BISO_DUMP("%-24s : %s\n", Name, BISO_UTIL_CopyStr(Value, sizeof(Value), szBuf))

#define BISO_DUMP_CHAR(str, len) \
{\
    UINT uiLoop;\
    for (uiLoop = 0; uiLoop < (UINT)len; uiLoop++)\
    {\
        BISO_DUMP("%c", str[uiLoop]);\
    }\
    BISO_DUMP("\n");\
}


#define BISO_DUMP_BYTE(Buf, Len) \
{ \
    UINT i; \
    for (i = 0; i < Len; i++) \
    { \
        BISO_DUMP("%02x ", Buf[i]); \
    } \
    BISO_DUMP("\n"); \
}

/* 显示日期 */
#define BISO_DUMP_DAY(Name, Value) \
{\
    ULONG _ulRet;\
    BISO_DATE_S _stDate;\
    _ulRet = BISO_9660_ParseDate84261((Value), &_stDate);\
    if (BISO_SUCCESS == _ulRet)\
    {\
        BISO_DUMP("%-24s : %04u-%02u-%02u %02u:%02u:%02u.%03u ",\
                  (Name), _stDate.usYear, _stDate.ucMonth, _stDate.ucDay,\
                  _stDate.ucHour, _stDate.ucMin, _stDate.ucSecond,\
                  _stDate.usMillSec);\
        if (_stDate.cZone > 0)\
        {\
            BISO_DUMP("GMT+%d\n", _stDate.cZone);\
        }\
        else\
        {\
            BISO_DUMP("GMT%d\n", _stDate.cZone);\
        }\
    }\
    else\
    {\
        BISO_DUMP("%-24s : ---\n", (Name));\
    }\
}

VOID BISO_DUMP_ShowFileTree
(
    IN UINT uiDepth,
    IN CONST BISO_DIR_TREE_S *pstDirTree
);

#endif /* __BISO_DUMP_H__ */

