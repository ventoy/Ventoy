/******************************************************************************
 * VentoyJson.h
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

#ifndef __VENTOY_JSON_H__
#define __VENTOY_JSON_H__

#ifdef FOR_VTOY_JSON_CHECK
typedef unsigned char      UINT8;
typedef unsigned short     UINT16;
typedef unsigned int       UINT32;
typedef unsigned long long UINT64;

#define Log printf
#define strcpy_s(a, b, c) strncpy(a, c, b)
#define sprintf_s snprintf
#endif

#define JSON_SUCCESS    0
#define JSON_FAILED     1
#define JSON_NOT_FOUND  2

typedef enum _JSON_TYPE
{
    JSON_TYPE_NUMBER = 0,
    JSON_TYPE_STRING,
    JSON_TYPE_BOOL,
    JSON_TYPE_ARRAY,
    JSON_TYPE_OBJECT,
    JSON_TYPE_NULL,
    JSON_TYPE_BUTT
}JSON_TYPE;


typedef struct _VTOY_JSON
{
    struct _VTOY_JSON *pstPrev;
    struct _VTOY_JSON *pstNext;
    struct _VTOY_JSON *pstChild;

    JSON_TYPE enDataType;
    union 
    {
        char  *pcStrVal;
        int   iNumVal;
        UINT64 lValue;
    }unData;

    char *pcName;
}VTOY_JSON;

typedef struct _JSON_PARSE
{
    char *pcKey;
    void *pDataBuf;
    UINT32  uiBufSize;
}JSON_PARSE;

#define JSON_NEW_ITEM(pstJson, ret) \
{ \
    (pstJson) = (VTOY_JSON *)malloc(sizeof(VTOY_JSON)); \
    if (NULL == (pstJson)) \
    { \
        Log("Failed to alloc memory for json.\n"); \
        return (ret); \
    } \
    memset((pstJson), 0, sizeof(VTOY_JSON));\
}

VTOY_JSON *vtoy_json_find_item
(
    VTOY_JSON *pstJson,
    JSON_TYPE  enDataType,
    const char *szKey
);
int vtoy_json_parse_value
(
    char *pcNewStart,
    char *pcRawStart,
    VTOY_JSON *pstJson, 
    const char *pcData,
    const char **ppcEnd
);
VTOY_JSON * vtoy_json_create(void);
int vtoy_json_parse(VTOY_JSON *pstJson, const char *szJsonData);

int vtoy_json_scan_parse
(
    const VTOY_JSON    *pstJson,
    UINT32              uiParseNum,
    JSON_PARSE         *pstJsonParse
);

int vtoy_json_scan_array
(
     VTOY_JSON *pstJson, 
     const char *szKey, 
     VTOY_JSON **ppstArrayItem
);

int vtoy_json_scan_array_ex
(
     VTOY_JSON *pstJson, 
     const char *szKey, 
     VTOY_JSON **ppstArrayItem
);
int vtoy_json_scan_object
(
     VTOY_JSON *pstJson, 
     const char *szKey, 
    VTOY_JSON **ppstObjectItem
);
int vtoy_json_get_int
(
    VTOY_JSON *pstJson, 
    const char *szKey, 
    int *piValue
);
int vtoy_json_get_uint
(
    VTOY_JSON *pstJson, 
    const char *szKey, 
    UINT32     *puiValue
);
int vtoy_json_get_uint64
(
    VTOY_JSON *pstJson, 
    const char *szKey, 
    UINT64 *pui64Value
);
int vtoy_json_get_bool
(
    VTOY_JSON *pstJson,
    const char *szKey, 
    UINT8 *pbValue
);
int vtoy_json_get_string
(
     VTOY_JSON *pstJson, 
     const char *szKey, 
     UINT32  uiBufLen,
     char *pcBuf
);
const char * vtoy_json_get_string_ex(VTOY_JSON *pstJson,  const char *szKey);
int vtoy_json_destroy(VTOY_JSON *pstJson);

#endif /* __VENTOY_JSON_H__ */

