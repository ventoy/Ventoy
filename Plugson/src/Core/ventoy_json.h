/******************************************************************************
 * ventoy_json.h
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

#define JSON_NEW_ITEM(pstJson, ret) \
{ \
    (pstJson) = (VTOY_JSON *)zalloc(sizeof(VTOY_JSON)); \
    if (NULL == (pstJson)) \
    { \
        vdebug("Failed to alloc memory for json."); \
        return (ret); \
    } \
}

#if defined(_MSC_VER) || defined(WIN32)
#define ssprintf(curpos, buf, len, fmt, ...) \
    curpos += scnprintf(buf + curpos, len - curpos, fmt, ##__VA_ARGS__)

#define VTOY_JSON_IS_SKIPABLE(c) (((c) <= 32) ? 1 : 0)

#define VTOY_JSON_PRINT_PREFIX(uiDepth, ...) \
{ \
    uint32_t _uiLoop = 0; \
    for (_uiLoop = 0; _uiLoop < (uiDepth); _uiLoop++) \
    { \
        ssprintf(uiCurPos, pcBuf, uiBufLen, "    "); \
    } \
    ssprintf(uiCurPos, pcBuf, uiBufLen, ##__VA_ARGS__); \
}
#else

#define ssprintf(curpos, buf, len, fmt, args...) \
    curpos += scnprintf(buf + curpos, len - curpos, fmt, ##args)

#define VTOY_JSON_IS_SKIPABLE(c) (((c) <= 32) ? 1 : 0)

#define VTOY_JSON_PRINT_PREFIX(uiDepth, args...) \
{ \
    uint32_t _uiLoop = 0; \
    for (_uiLoop = 0; _uiLoop < (uiDepth); _uiLoop++) \
    { \
        ssprintf(uiCurPos, pcBuf, uiBufLen, "    "); \
    } \
    ssprintf(uiCurPos, pcBuf, uiBufLen, ##args); \
}
#endif


#define VTOY_JSON_SUCCESS_RET      "{ \"result\" : \"success\" }"
#define VTOY_JSON_FAILED_RET       "{ \"result\" : \"failed\" }"
#define VTOY_JSON_INVALID_RET      "{ \"result\" : \"invalidfmt\" }"
#define VTOY_JSON_TOKEN_ERR_RET    "{ \"result\" : \"tokenerror\" }"
#define VTOY_JSON_EXIST_RET        "{ \"result\" : \"exist\" }"
#define VTOY_JSON_TIMEOUT_RET      "{ \"result\" : \"timeout\" }"
#define VTOY_JSON_BUSY_RET         "{ \"result\" : \"busy\" }"
#define VTOY_JSON_INUSE_RET        "{ \"result\" : \"inuse\" }"
#define VTOY_JSON_NOTFOUND_RET     "{ \"result\" : \"notfound\" }"
#define VTOY_JSON_NOTRUNNING_RET   "{ \"result\" : \"notrunning\" }"
#define VTOY_JSON_NOT_READY_RET    "{ \"result\" : \"notready\" }"
#define VTOY_JSON_NOT_SUPPORT_RET  "{ \"result\" : \"notsupport\" }"
#define VTOY_JSON_MBR_2TB_RET      "{ \"result\" : \"mbr2tb\" }"
#define VTOY_JSON_INVALID_RSV_RET      "{ \"result\" : \"reserve_invalid\" }"
#define VTOY_JSON_FILE_NOT_FOUND_RET     "{ \"result\" : \"file_not_found\" }"

typedef enum tagJSON_TYPE
{
    JSON_TYPE_NUMBER = 0,
    JSON_TYPE_STRING,
    JSON_TYPE_BOOL,
    JSON_TYPE_ARRAY,
    JSON_TYPE_OBJECT,
    JSON_TYPE_NULL,
    JSON_TYPE_BUTT
}JSON_TYPE;

typedef struct tagVTOY_JSON
{
    struct tagVTOY_JSON *pstPrev;
    struct tagVTOY_JSON *pstNext;
    struct tagVTOY_JSON *pstChild;

    JSON_TYPE enDataType;
    union 
    {
        char  *pcStrVal;
        int    iNumVal;
        uint64_t lValue;
    }unData;

    char *pcName;
}VTOY_JSON;

#define VTOY_JSON_FMT_BEGIN(uiCurPos, pcBuf, uiBufLen) \
{\
    uint32_t __uiCurPos = (uiCurPos);\
    uint32_t __uiBufLen = (uiBufLen);\
    char *__pcBuf = (pcBuf);
    
#define VTOY_JSON_FMT_END(uiCurPos) \
    (uiCurPos) = __uiCurPos;\
}
    
#define VTOY_JSON_FMT_OBJ_BEGIN()  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "{")
#define VTOY_JSON_FMT_OBJ_BEGIN_L(P)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s{", P)
#define VTOY_JSON_FMT_OBJ_BEGIN_N()  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "{\n")
#define VTOY_JSON_FMT_OBJ_BEGIN_LN(P)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s{\n", P)
    
#define VTOY_JSON_FMT_OBJ_END()  \
{\
    if (',' == *(__pcBuf + (__uiCurPos - 1)))\
    {\
        __uiCurPos -= 1;\
    }\
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "}");\
}

#define VTOY_JSON_FMT_OBJ_ENDEX()  \
{\
    if (',' == *(__pcBuf + (__uiCurPos - 1)))\
    {\
        __uiCurPos -= 1;\
    }\
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "},");\
}


    
#define VTOY_JSON_FMT_KEY(Key)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\":", (Key))
#define VTOY_JSON_FMT_KEY_L(P, Key)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\":", P, (Key))


#define VTOY_JSON_FMT_ITEM(Item)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\",", (Item))
#define VTOY_JSON_FMT_ITEM_L(P, Item)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\",", P, (Item))
#define VTOY_JSON_FMT_ITEM_LN(P, Item)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\",\n", P, (Item))
#define VTOY_JSON_FMT_ITEM_PATH_LN(P, Item)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\",\n", P, ventoy_real_path(Item))

#define VTOY_JSON_FMT_COMA()  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, ",")
#define VTOY_JSON_FMT_COMA_N(cnt)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, ",\n")
#define VTOY_JSON_FMT_COMA_N_CNT(cnt) if ((cnt) > 0)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, ",\n")


#define VTOY_JSON_FMT_APPEND_BEGIN() \
{ \
    if ('}' == *(__pcBuf + (__uiCurPos - 1)))\
    {\
        __uiCurPos -= 1;\
    }\
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, ",");\
}

#define VTOY_JSON_FMT_APPEND_END() \
{ \
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "}");\
}

#define VTOY_JSON_FMT_ARY_BEGIN()  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "[")
#define VTOY_JSON_FMT_ARY_BEGIN_N()  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "[\n")
    
#define VTOY_JSON_FMT_ARY_END()    \
{\
    if (',' == *(__pcBuf + (__uiCurPos - 1)))\
    {\
        __uiCurPos -= 1;\
    }\
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "]");\
}
#define VTOY_JSON_FMT_ARY_ENDEX()    \
{\
    if (',' == *(__pcBuf + (__uiCurPos - 1)))\
    {\
        __uiCurPos -= 1;\
    }\
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "],");\
}



#define VTOY_JSON_FMT_OBJ_END_L(P)  \
{\
    if ('\n' == *(__pcBuf + (__uiCurPos - 1)) && ',' == *(__pcBuf + (__uiCurPos - 2)))\
    {\
        *(__pcBuf + (__uiCurPos - 2)) = '\n';\
        __uiCurPos -= 1;\
    }\
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s}%s", P);\
}
#define VTOY_JSON_FMT_OBJ_ENDEX_L(P)  \
{\
    if ('\n' == *(__pcBuf + (__uiCurPos - 1)) && ',' == *(__pcBuf + (__uiCurPos - 2)))\
    {\
        *(__pcBuf + (__uiCurPos - 2)) = '\n';\
        __uiCurPos -= 1;\
    }\
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s},%s", P);\
}

#define VTOY_JSON_FMT_OBJ_END_LN(P)  \
{\
    if ('\n' == *(__pcBuf + (__uiCurPos - 1)) && ',' == *(__pcBuf + (__uiCurPos - 2)))\
    {\
        *(__pcBuf + (__uiCurPos - 2)) = '\n';\
        __uiCurPos -= 1;\
    }\
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s}\n", P);\
}
#define VTOY_JSON_FMT_OBJ_ENDEX_LN(P)  \
{\
    if ('\n' == *(__pcBuf + (__uiCurPos - 1)) && ',' == *(__pcBuf + (__uiCurPos - 2)))\
    {\
        *(__pcBuf + (__uiCurPos - 2)) = '\n';\
        __uiCurPos -= 1;\
    }\
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s},\n", P);\
}




#define VTOY_JSON_FMT_ARY_END_L(P)    \
{\
    if ('\n' == *(__pcBuf + (__uiCurPos - 1)) && ',' == *(__pcBuf + (__uiCurPos - 2)))\
    {\
        *(__pcBuf + (__uiCurPos - 2)) = '\n';\
        __uiCurPos -= 1;\
    }\
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s]", P);\
}

#define VTOY_JSON_FMT_ARY_END_LN(P)    \
{\
    if ('\n' == *(__pcBuf + (__uiCurPos - 1)) && ',' == *(__pcBuf + (__uiCurPos - 2)))\
    {\
        *(__pcBuf + (__uiCurPos - 2)) = '\n';\
        __uiCurPos -= 1;\
    }\
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s]\n", P);\
}

#define VTOY_JSON_FMT_ARY_ENDEX_L(P)    \
{\
    if ('\n' == *(__pcBuf + (__uiCurPos - 1)) && ',' == *(__pcBuf + (__uiCurPos - 2)))\
    {\
        *(__pcBuf + (__uiCurPos - 2)) = '\n';\
        __uiCurPos -= 1;\
    }\
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s],", P);\
}
    

#define VTOY_JSON_FMT_ARY_ENDEX_LN(P)    \
{\
    if ('\n' == *(__pcBuf + (__uiCurPos - 1)) && ',' == *(__pcBuf + (__uiCurPos - 2)))\
    {\
        *(__pcBuf + (__uiCurPos - 2)) = '\n';\
        __uiCurPos -= 1;\
    }\
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s],\n", P);\
}



    
#define VTOY_JSON_FMT_UINT64(Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": %llu,", Key, (_ull)Val)

#define VTOY_JSON_FMT_ULONG(Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": %lu,", Key, Val)
#define VTOY_JSON_FMT_LONG(Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": %ld,", Key, Val)

#define VTOY_JSON_FMT_UINT(Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": %u,", Key, Val)
#define VTOY_JSON_FMT_UINT_N(Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": %u,\n", Key, Val)
#define VTOY_JSON_FMT_UINT_L(P, Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\": %u,", P, Key, Val)
#define VTOY_JSON_FMT_UINT_LN(P, Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\": %u,\n", P, Key, Val)


#define VTOY_JSON_FMT_STRUINT(Key, Val) ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": \"%u\",", Key, Val)
#define VTOY_JSON_FMT_STRUINT_N(Key, Val) ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": \"%u\",\n", Key, Val)
#define VTOY_JSON_FMT_STRUINT_L(P, Key, Val) ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\": \"%u\",", P, Key, Val)
#define VTOY_JSON_FMT_STRUINT_LN(P, Key, Val) ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\": \"%u\",\n", P, Key, Val)
    
#define VTOY_JSON_FMT_STRSINT(Key, Val) ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": \"%d\",", Key, Val)
#define VTOY_JSON_FMT_STRSINT_N(Key, Val) ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": \"%d\",\n", Key, Val)
#define VTOY_JSON_FMT_STRSINT_L(P, Key, Val) ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\": \"%d\",", P, Key, Val)
#define VTOY_JSON_FMT_STRSINT_LN(P, Key, Val) ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\": \"%d\",\n", P, Key, Val)

#define VTOY_JSON_FMT_CTRL_INT(Prefix, Key, Field)  \
    if (def->Field != data->Field) \
        ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s{ \"%s\": \"%d\" },\n", Prefix, Key, data->Field)

#define VTOY_JSON_FMT_CTRL_STRN(P, Key, Field) \
    if (strcmp(def->Field, data->Field)) \
        ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s{ \"%s\": \"%s\" },\n", P, Key, data->Field) 
        
#define VTOY_JSON_FMT_CTRL_STRN_STR(P, Key, ptr) \
        ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s{ \"%s\": \"%s\" },\n", P, Key, ptr)
        
#define VTOY_JSON_FMT_CTRL_PUB_STRN(P, Key, Field) \
    if (strcmp(def->Field, g_pub_path)) \
        ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s{ \"%s\": \"%s\" },\n", P, Key, g_pub_path) 


#define VTOY_JSON_FMT_DIFF_STRN(P, Key, Field) \
    if (strcmp(def->Field, data->Field)) \
        ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\": \"%s\",\n", P, Key, data->Field) 


#define VTOY_JSON_FMT_STRINT64(Key, Val)  \
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": \"%llu\",", Key, Val)

#define VTOY_JSON_FMT_SINT(Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": %d,", Key, Val)
#define VTOY_JSON_FMT_SINT_N(Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": %d,\n", Key, Val)
#define VTOY_JSON_FMT_SINT_L(P, Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\": %d,", P, Key, Val)
#define VTOY_JSON_FMT_SINT_LN(P, Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\": %d,\n", P, Key, Val)
    
#define VTOY_JSON_FMT_DUBL(Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": %.1lf,", Key, Val)
#define VTOY_JSON_FMT_DUBL2(Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": %10.02lf,", Key, Val)

#define VTOY_JSON_FMT_STRN(Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": \"%s\",", Key, Val) 
#define VTOY_JSON_FMT_STRN_N(Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": \"%s\",\n", Key, Val) 
#define VTOY_JSON_FMT_STRN_L(P, Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\": \"%s\",", P, Key, Val) 
#define VTOY_JSON_FMT_STRN_LN(P, Key, Val)  ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\": \"%s\",\n", P, Key, Val) 
#define VTOY_JSON_FMT_STRN_PATH_LN(P, Key, Val)  \
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\": \"%s\",\n", P, Key, ventoy_real_path(Val))

int vtoy_json_escape_string(char *buf, int buflen, const char *str, int newline);

#define VTOY_JSON_FMT_STRN_EX_LN(P, Key, Val)  \
{\
    ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "%s\"%s\": ", P, Key);\
    __uiCurPos += vtoy_json_escape_string(__pcBuf + __uiCurPos, __uiBufLen - __uiCurPos, Val, 1);\
}

    
#define VTOY_JSON_FMT_NULL(Key)       ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": null,", Key)

#define VTOY_JSON_FMT_TRUE(Key) ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": true,", (Key))
#define VTOY_JSON_FMT_FALSE(Key) ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": false,", (Key))

#define VTOY_JSON_FMT_BOOL(Key, Val)  \
{\
    if (0 == (Val))\
    {\
        ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": false,", (Key));\
    }\
    else \
    {\
        ssprintf(__uiCurPos, __pcBuf, __uiBufLen, "\"%s\": true,", (Key));\
    }\
}

typedef struct tagVTOY_JSON_PARSE
{
    char *pcKey;
    void *pDataBuf;
    uint32_t  uiBufSize;
}VTOY_JSON_PARSE_S;

#define JSON_SUCCESS    0
#define JSON_FAILED     1
#define JSON_NOT_FOUND  2

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
int vtoy_json_parse_ex(VTOY_JSON *pstJson, const char *szJsonData, int szLen);
int vtoy_json_destroy(VTOY_JSON *pstJson);
VTOY_JSON *vtoy_json_find_item
(
    VTOY_JSON *pstJson,
    JSON_TYPE  enDataType,
    const char *szKey
);
int vtoy_json_scan_parse
(
    const VTOY_JSON    *pstJson,
    uint32_t            uiParseNum,
    VTOY_JSON_PARSE_S  *pstJsonParse
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
    uint32_t *puiValue
);
int vtoy_json_get_uint64
(
    VTOY_JSON *pstJson, 
    const char *szKey, 
    uint64_t *pui64Value
);
int vtoy_json_get_bool
(
    VTOY_JSON *pstJson,
    const char *szKey, 
    uint8_t *pbValue
);
int vtoy_json_get_string
(
     VTOY_JSON *pstJson, 
     const char *szKey, 
     uint32_t  uiBufLen,
     char *pcBuf
);
const char * vtoy_json_get_string_ex(VTOY_JSON *pstJson,  const char *szKey);

#endif /* __VENTOY_JSON_H__ */

