/******************************************************************************
 * VentoyJson.c
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

#ifdef FOR_VTOY_JSON_CHECK
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#else
#include <Windows.h>
#include "Ventoy2Disk.h"
#endif

#include "VentoyJson.h"

static void vtoy_json_free(VTOY_JSON *pstJsonHead)
{
    VTOY_JSON *pstNext = NULL;

    while (NULL != pstJsonHead)
    {
        pstNext = pstJsonHead->pstNext;
        if ((pstJsonHead->enDataType < JSON_TYPE_BUTT) && (NULL != pstJsonHead->pstChild))
        {
            vtoy_json_free(pstJsonHead->pstChild);
        }

        free(pstJsonHead);
        pstJsonHead = pstNext;
    }

    return;
}

static char *vtoy_json_skip(const char *pcData)
{
    while ((NULL != pcData) && ('\0' != *pcData) && (*pcData <= 32))
    {
        pcData++;
    }

    return (char *)pcData;
}

VTOY_JSON *vtoy_json_find_item
(
    VTOY_JSON *pstJson,
    JSON_TYPE  enDataType,
    const char *szKey
)
{
    while (NULL != pstJson)
    {
        if ((enDataType == pstJson->enDataType) && 
            (0 == strcmp(szKey, pstJson->pcName)))
        {
            return pstJson;
        }
        pstJson = pstJson->pstNext;
    }
    
    return NULL;
}

static int vtoy_json_parse_number
(
    VTOY_JSON *pstJson, 
    const char *pcData,
    const char **ppcEnd
)
{
    unsigned long Value;

    Value = strtoul(pcData, (char **)ppcEnd, 10);
    if (*ppcEnd == pcData)
    {
        Log("Failed to parse json number %s.", pcData);
        return JSON_FAILED;
    }

    pstJson->enDataType = JSON_TYPE_NUMBER;
    pstJson->unData.lValue = Value;
    
    return JSON_SUCCESS;
}

static int vtoy_json_parse_string
(
    char *pcNewStart,
    char *pcRawStart,
    VTOY_JSON *pstJson, 
    const char *pcData,
    const char **ppcEnd
)
{
    UINT32 uiLen = 0;
    const char *pcPos = NULL;
    const char *pcTmp = pcData + 1;
    
    *ppcEnd = pcData;

    if ('\"' != *pcData)
    {
        return JSON_FAILED;
    }

    pcPos = strchr(pcTmp, '\"');
    if ((NULL == pcPos) || (pcPos < pcTmp))
    {
        Log("Invalid string %s.", pcData);
        return JSON_FAILED;
    }

    *ppcEnd = pcPos + 1;
    uiLen = (UINT32)(unsigned long)(pcPos - pcTmp);    
    
    pstJson->enDataType = JSON_TYPE_STRING;
    pstJson->unData.pcStrVal = pcNewStart + (pcTmp - pcRawStart);
    pstJson->unData.pcStrVal[uiLen] = '\0';
    
    return JSON_SUCCESS;
}

static int vtoy_json_parse_array
(
    char *pcNewStart,
    char *pcRawStart,
    VTOY_JSON *pstJson, 
    const char *pcData,
    const char **ppcEnd
)
{
    int Ret = JSON_SUCCESS;
    VTOY_JSON *pstJsonChild = NULL;
    VTOY_JSON *pstJsonItem = NULL;
    const char *pcTmp = pcData + 1;

    *ppcEnd = pcData;
    pstJson->enDataType = JSON_TYPE_ARRAY;

    if ('[' != *pcData)
    {
        return JSON_FAILED;
    }

    pcTmp = vtoy_json_skip(pcTmp);

    if (']' == *pcTmp)
    {
        *ppcEnd = pcTmp + 1;
        return JSON_SUCCESS;
    }

    JSON_NEW_ITEM(pstJson->pstChild, JSON_FAILED);

    Ret = vtoy_json_parse_value(pcNewStart, pcRawStart, pstJson->pstChild, pcTmp, ppcEnd);
    if (JSON_SUCCESS != Ret)
    {
        Log("Failed to parse array child.");
        return JSON_FAILED;
    }

    pstJsonChild = pstJson->pstChild;
    pcTmp = vtoy_json_skip(*ppcEnd);
    while ((NULL != pcTmp) && (',' == *pcTmp))
    {
        JSON_NEW_ITEM(pstJsonItem, JSON_FAILED);
        pstJsonChild->pstNext = pstJsonItem;
        pstJsonItem->pstPrev = pstJsonChild;
        pstJsonChild = pstJsonItem;

        Ret = vtoy_json_parse_value(pcNewStart, pcRawStart, pstJsonChild, vtoy_json_skip(pcTmp + 1), ppcEnd);
        if (JSON_SUCCESS != Ret)
        {
            Log("Failed to parse array child.");
            return JSON_FAILED;
        }
        pcTmp = vtoy_json_skip(*ppcEnd);
    }

    if ((NULL != pcTmp) && (']' == *pcTmp))
    {
        *ppcEnd = pcTmp + 1;
        return JSON_SUCCESS;
    }
    else
    {
        *ppcEnd = pcTmp;
        return JSON_FAILED;
    }
}

static int vtoy_json_parse_object
(
    char *pcNewStart,
    char *pcRawStart,
    VTOY_JSON *pstJson, 
    const char *pcData,
    const char **ppcEnd
)
{
    int Ret = JSON_SUCCESS;
    VTOY_JSON *pstJsonChild = NULL;
    VTOY_JSON *pstJsonItem = NULL;
    const char *pcTmp = pcData + 1;

    *ppcEnd = pcData;
    pstJson->enDataType = JSON_TYPE_OBJECT;

    if ('{' != *pcData)
    {
        return JSON_FAILED;
    }

    pcTmp = vtoy_json_skip(pcTmp);
    if ('}' == *pcTmp)
    {
        *ppcEnd = pcTmp + 1;
        return JSON_SUCCESS;
    }

    JSON_NEW_ITEM(pstJson->pstChild, JSON_FAILED);

    Ret = vtoy_json_parse_string(pcNewStart, pcRawStart, pstJson->pstChild, pcTmp, ppcEnd);
    if (JSON_SUCCESS != Ret)
    {
        Log("Failed to parse array child.");
        return JSON_FAILED;
    }

    pstJsonChild = pstJson->pstChild;
    pstJsonChild->pcName = pstJsonChild->unData.pcStrVal;
    pstJsonChild->unData.pcStrVal = NULL;

    pcTmp = vtoy_json_skip(*ppcEnd);
    if ((NULL == pcTmp) || (':' != *pcTmp))
    {
        *ppcEnd = pcTmp;
        return JSON_FAILED;
    }

    Ret = vtoy_json_parse_value(pcNewStart, pcRawStart, pstJsonChild, vtoy_json_skip(pcTmp + 1), ppcEnd);
    if (JSON_SUCCESS != Ret)
    {
        Log("Failed to parse array child.");
        return JSON_FAILED;
    }

    pcTmp = vtoy_json_skip(*ppcEnd);
    while ((NULL != pcTmp) && (',' == *pcTmp))
    {
        JSON_NEW_ITEM(pstJsonItem, JSON_FAILED);
        pstJsonChild->pstNext = pstJsonItem;
        pstJsonItem->pstPrev = pstJsonChild;
        pstJsonChild = pstJsonItem;

        Ret = vtoy_json_parse_string(pcNewStart, pcRawStart, pstJsonChild, vtoy_json_skip(pcTmp + 1), ppcEnd);
        if (JSON_SUCCESS != Ret)
        {
            Log("Failed to parse array child.");
            return JSON_FAILED;
        }

        pcTmp = vtoy_json_skip(*ppcEnd);
        pstJsonChild->pcName = pstJsonChild->unData.pcStrVal;
        pstJsonChild->unData.pcStrVal = NULL;
        if ((NULL == pcTmp) || (':' != *pcTmp))
        {
            *ppcEnd = pcTmp;
            return JSON_FAILED;
        }

        Ret = vtoy_json_parse_value(pcNewStart, pcRawStart, pstJsonChild, vtoy_json_skip(pcTmp + 1), ppcEnd);
        if (JSON_SUCCESS != Ret)
        {
            Log("Failed to parse array child.");
            return JSON_FAILED;
        }

        pcTmp = vtoy_json_skip(*ppcEnd);
    }

    if ((NULL != pcTmp) && ('}' == *pcTmp))
    {
        *ppcEnd = pcTmp + 1;
        return JSON_SUCCESS;
    }
    else
    {
        *ppcEnd = pcTmp;
        return JSON_FAILED;
    }
}

int vtoy_json_parse_value
(
    char *pcNewStart,
    char *pcRawStart,
    VTOY_JSON *pstJson, 
    const char *pcData,
    const char **ppcEnd
)
{
    pcData = vtoy_json_skip(pcData);
    
    switch (*pcData)
    {
        case 'n':
        {
            if (0 == strncmp(pcData, "null", 4))
            {
                pstJson->enDataType = JSON_TYPE_NULL;
                *ppcEnd = pcData + 4;
                return JSON_SUCCESS;
            }
            break;
        }
        case 'f':
        {
            if (0 == strncmp(pcData, "false", 5))
            {
                pstJson->enDataType = JSON_TYPE_BOOL;
                pstJson->unData.lValue = 0;
                *ppcEnd = pcData + 5;
                return JSON_SUCCESS;
            }
            break;
        }
        case 't':
        {
            if (0 == strncmp(pcData, "true", 4))
            {
                pstJson->enDataType = JSON_TYPE_BOOL;
                pstJson->unData.lValue = 1;
                *ppcEnd = pcData + 4;
                return JSON_SUCCESS;
            }
            break;
        }
        case '\"':
        {
            return vtoy_json_parse_string(pcNewStart, pcRawStart, pstJson, pcData, ppcEnd);
        }
        case '[':
        {
            return vtoy_json_parse_array(pcNewStart, pcRawStart, pstJson, pcData, ppcEnd);
        }
        case '{':
        {
            return vtoy_json_parse_object(pcNewStart, pcRawStart, pstJson, pcData, ppcEnd);
        }
        case '-':
        {
            return vtoy_json_parse_number(pstJson, pcData, ppcEnd);
        }
        default :
        {
            if (*pcData >= '0' && *pcData <= '9')
            {
                return vtoy_json_parse_number(pstJson, pcData, ppcEnd);
            }
        }
    }

    *ppcEnd = pcData;
    Log("Invalid json data %u.", (UINT8)(*pcData));
    return JSON_FAILED;
}

VTOY_JSON * vtoy_json_create(void)
{
    VTOY_JSON *pstJson = NULL;

    pstJson = (VTOY_JSON *)malloc(sizeof(VTOY_JSON));
    if (NULL == pstJson)
    {
        return NULL;
    }
    memset(pstJson, 0, sizeof(VTOY_JSON));
    return pstJson;
}

int vtoy_json_parse(VTOY_JSON *pstJson, const char *szJsonData)
{
    UINT32 uiMemSize = 0;
    int Ret = JSON_SUCCESS;
    char *pcNewBuf = NULL;
    const char *pcEnd = NULL;

	uiMemSize = (UINT32)strlen(szJsonData) + 1;
    pcNewBuf = (char *)malloc(uiMemSize);
    if (NULL == pcNewBuf)
    {
        Log("Failed to alloc new buf.");
        return JSON_FAILED;
    }
    memcpy(pcNewBuf, szJsonData, uiMemSize);
    pcNewBuf[uiMemSize - 1] = 0;

    Ret = vtoy_json_parse_value(pcNewBuf, (char *)szJsonData, pstJson, szJsonData, &pcEnd);
    if (JSON_SUCCESS != Ret)
    {
        Log("Failed to parse json data start=%p, end=%p", szJsonData, pcEnd);
        return JSON_FAILED;
    }

    return JSON_SUCCESS;
}

int vtoy_json_scan_parse
(
    const VTOY_JSON    *pstJson,
    UINT32       uiParseNum,
    JSON_PARSE         *pstJsonParse
)
{   
    UINT32 i = 0;
    const VTOY_JSON *pstJsonCur = NULL;
    JSON_PARSE *pstCurParse = NULL;

    for (pstJsonCur = pstJson; NULL != pstJsonCur; pstJsonCur = pstJsonCur->pstNext)
    {
        if ((JSON_TYPE_OBJECT == pstJsonCur->enDataType) ||
            (JSON_TYPE_ARRAY == pstJsonCur->enDataType))
        {
            continue;
        }

        for (i = 0, pstCurParse = NULL; i < uiParseNum; i++)
        {
            if (0 == strcmp(pstJsonParse[i].pcKey, pstJsonCur->pcName))
            {   
                pstCurParse = pstJsonParse + i;
                break;
            }
        }

        if (NULL == pstCurParse)
        {
            continue;
        }
    
        switch (pstJsonCur->enDataType)
        {
            case JSON_TYPE_NUMBER:
            {
                if (sizeof(UINT32) == pstCurParse->uiBufSize)
                {
                    *(UINT32 *)(pstCurParse->pDataBuf) = (UINT32)pstJsonCur->unData.lValue;
                }
                else if (sizeof(UINT16) == pstCurParse->uiBufSize)
                {
                    *(UINT16 *)(pstCurParse->pDataBuf) = (UINT16)pstJsonCur->unData.lValue;
                }
                else if (sizeof(UINT8) == pstCurParse->uiBufSize)
                {
                    *(UINT8 *)(pstCurParse->pDataBuf) = (UINT8)pstJsonCur->unData.lValue;
                }
                else if ((pstCurParse->uiBufSize > sizeof(UINT64)))
                {
                    sprintf_s((char *)pstCurParse->pDataBuf, pstCurParse->uiBufSize, "%llu",
                        (unsigned long long)(pstJsonCur->unData.lValue));
                }
                else
                {
                    Log("Invalid number data buf size %u.", pstCurParse->uiBufSize);
                }
                break;
            }
            case JSON_TYPE_STRING:
            {
                strcpy_s((char *)pstCurParse->pDataBuf, pstCurParse->uiBufSize, pstJsonCur->unData.pcStrVal);
                break;
            }
            case JSON_TYPE_BOOL:
            {
                *(UINT8 *)(pstCurParse->pDataBuf) = (pstJsonCur->unData.lValue) > 0 ? 1 : 0;
                break;
            }
            default :
            {
                break;
            }
        }
    }

    return JSON_SUCCESS;
}

int vtoy_json_scan_array
(
     VTOY_JSON *pstJson, 
     const char *szKey, 
     VTOY_JSON **ppstArrayItem
)
{
    VTOY_JSON *pstJsonItem = NULL;
    
    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_ARRAY, szKey);
    if (NULL == pstJsonItem)
    {
        Log("Key %s is not found in json data.", szKey);
        return JSON_NOT_FOUND;
    }

    *ppstArrayItem = pstJsonItem;

    return JSON_SUCCESS;
}

int vtoy_json_scan_array_ex
(
     VTOY_JSON *pstJson, 
     const char *szKey, 
     VTOY_JSON **ppstArrayItem
)
{
    VTOY_JSON *pstJsonItem = NULL;
    
    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_ARRAY, szKey);
    if (NULL == pstJsonItem)
    {
        Log("Key %s is not found in json data.", szKey);
        return JSON_NOT_FOUND;
    }
    
    *ppstArrayItem = pstJsonItem->pstChild;

    return JSON_SUCCESS;
}

int vtoy_json_scan_object
(
     VTOY_JSON *pstJson, 
     const char *szKey, 
     VTOY_JSON **ppstObjectItem
)
{
    VTOY_JSON *pstJsonItem = NULL;

    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_OBJECT, szKey);
    if (NULL == pstJsonItem)
    {
        Log("Key %s is not found in json data.", szKey);
        return JSON_NOT_FOUND;
    }

    *ppstObjectItem = pstJsonItem;

    return JSON_SUCCESS;
}

int vtoy_json_get_int
(
    VTOY_JSON *pstJson, 
    const char *szKey, 
    int *piValue
)
{
    VTOY_JSON *pstJsonItem = NULL;
    
    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_NUMBER, szKey);
    if (NULL == pstJsonItem)
    {
        Log("Key %s is not found in json data.", szKey);
        return JSON_NOT_FOUND;
    }

    *piValue = (int)pstJsonItem->unData.lValue;

    return JSON_SUCCESS;
}

int vtoy_json_get_uint
(
    VTOY_JSON *pstJson, 
    const char *szKey, 
    UINT32 *puiValue
)
{
    VTOY_JSON *pstJsonItem = NULL;
    
    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_NUMBER, szKey);
    if (NULL == pstJsonItem)
    {
        Log("Key %s is not found in json data.", szKey);
        return JSON_NOT_FOUND;
    }

    *puiValue = (UINT32)pstJsonItem->unData.lValue;

    return JSON_SUCCESS;
}

int vtoy_json_get_uint64
(
    VTOY_JSON *pstJson, 
    const char *szKey, 
    UINT64 *pui64Value
)
{
    VTOY_JSON *pstJsonItem = NULL;
    
    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_NUMBER, szKey);
    if (NULL == pstJsonItem)
    {
        Log("Key %s is not found in json data.", szKey);
        return JSON_NOT_FOUND;
    }

    *pui64Value = (UINT64)pstJsonItem->unData.lValue;

    return JSON_SUCCESS;
}

int vtoy_json_get_bool
(
    VTOY_JSON *pstJson,
    const char *szKey, 
    UINT8 *pbValue
)
{
    VTOY_JSON *pstJsonItem = NULL;
    
    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_BOOL, szKey);
    if (NULL == pstJsonItem)
    {
        Log("Key %s is not found in json data.", szKey);
        return JSON_NOT_FOUND;
    }

    *pbValue = pstJsonItem->unData.lValue > 0 ? 1 : 0;

    return JSON_SUCCESS;
}

int vtoy_json_get_string
(
     VTOY_JSON *pstJson, 
     const char *szKey, 
     UINT32  uiBufLen,
     char *pcBuf
)
{
    VTOY_JSON *pstJsonItem = NULL;
    
    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_STRING, szKey);
    if (NULL == pstJsonItem)
    {
        Log("Key %s is not found in json data.", szKey);
        return JSON_NOT_FOUND;
    }

    strcpy_s(pcBuf, uiBufLen, pstJsonItem->unData.pcStrVal);

    return JSON_SUCCESS;
}

const char * vtoy_json_get_string_ex(VTOY_JSON *pstJson,  const char *szKey)
{
    VTOY_JSON *pstJsonItem = NULL;

    if ((NULL == pstJson) || (NULL == szKey))
    {
        return NULL;
    }

    pstJsonItem = vtoy_json_find_item(pstJson, JSON_TYPE_STRING, szKey);
    if (NULL == pstJsonItem)
    {
        Log("Key %s is not found in json data.", szKey);
        return NULL;
    }

    return pstJsonItem->unData.pcStrVal;
}

int vtoy_json_destroy(VTOY_JSON *pstJson)
{
    if (NULL == pstJson)
    {   
        return JSON_SUCCESS;
    }

    if (NULL != pstJson->pstChild)
    {
        vtoy_json_free(pstJson->pstChild);
    }

    if (NULL != pstJson->pstNext)
    {
        vtoy_json_free(pstJson->pstNext);
    }

    free(pstJson);
    
    return JSON_SUCCESS;
}


#ifdef FOR_VTOY_JSON_CHECK

int main(int argc, char**argv)
{
    int ret = 1;
    int FileSize;
    FILE *fp;
    void *Data = NULL;
    VTOY_JSON *json = NULL;

    fp = fopen(argv[1], "rb");
    if (!fp)
    {
        Log("Failed to open %s\n", argv[1]);
        goto out;
    }

    fseek(fp, 0, SEEK_END);
    FileSize = (int)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    Data = malloc(FileSize + 4);
    if (!Data)
    {
        Log("Failed to malloc %d\n", FileSize + 4);
        goto out;
    }
    *((char *)Data + FileSize) = 0;

    fread(Data, 1, FileSize, fp);

    json = vtoy_json_create();
    if (!json)
    {
        Log("Failed vtoy_json_create\n");
        goto out;
    }

    if (vtoy_json_parse(json, (char *)Data) != JSON_SUCCESS)
    {
        goto out;
    }

    ret = 0;

out:
    if (fp) fclose(fp);
    if (Data) free(Data);
    if (json) vtoy_json_destroy(json);

    printf("\n");
    return ret;
}

#endif
