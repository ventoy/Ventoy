/******************************************************************************
 * biso_util.c
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
 

#include "biso.h"
#include "biso_list.h"
#include "biso_util.h"
#include "biso_9660.h"

VOID *zalloc(size_t size)
{
    void *p = malloc(size);
    if (NULL != p)
    {
        memset(p, 0, size);
    }
    return p;
}


#if (1 == MEMORY_DEBUG)
STATIC UINT g_uiBISOTotMalloc = 0;
STATIC UINT g_uiBISOPeekMalloc = 0;
STATIC UINT g_uiBISOMallocTime = 0;
STATIC UINT g_uiBISOFreeTime = 0;

VOID *g_apstBISOMalloc[7000];
VOID *g_apstBISOFree[7000];

VOID * BISO_UTIL_Malloc(IN size_t ulSize)
{
    VOID *pData = malloc(ulSize + 4);
    
    #if (1 == MEMORY_DEBUG_DUMP)
    printf("ID %u Malloc %p %lu\n", g_uiBISOMallocTime, (UCHAR *)pData + 4, ulSize);
    g_apstBISOMalloc[g_uiBISOMallocTime] = (UCHAR *)pData + 4;
    #endif

    *(UINT32 *)pData = (UINT32)ulSize;
    g_uiBISOMallocTime++;
    g_uiBISOTotMalloc += (UINT32)ulSize;
    if (g_uiBISOTotMalloc > g_uiBISOPeekMalloc)
    {
        g_uiBISOPeekMalloc = g_uiBISOTotMalloc;
    }
   
    return (UCHAR *)pData + 4;
}

VOID *BISO_UTIL_Zalloc(IN size_t ulSize)
{
    VOID *pData = zalloc(ulSize + 4);

    #if (1 == MEMORY_DEBUG_DUMP)
    printf("ID %u Zalloc %p %lu\n", g_uiBISOMallocTime, (UCHAR *)pData + 4, ulSize);
    g_apstBISOMalloc[g_uiBISOMallocTime] = (UCHAR *)pData + 4;
    #endif

    *(UINT32 *)pData = (UINT32)ulSize;
    g_uiBISOMallocTime++;
    g_uiBISOTotMalloc += (UINT32)ulSize;
    if (g_uiBISOTotMalloc > g_uiBISOPeekMalloc)
    {
        g_uiBISOPeekMalloc = g_uiBISOTotMalloc;
    }
   
    return (UCHAR *)pData + 4;
}

VOID BISO_UTIL_Free(IN VOID *pData)
{    
    #if (1 == MEMORY_DEBUG_DUMP)
    printf("ID %u Free %p %u\n", g_uiBISOFreeTime, pData, *(UINT32 *)((UCHAR *)pData - 4));
    g_apstBISOFree[g_uiBISOFreeTime] = pData;
    #endif

    g_uiBISOFreeTime++;
    g_uiBISOTotMalloc -= *(UINT32 *)((UCHAR *)pData - 4);
    if (g_uiBISOTotMalloc > g_uiBISOPeekMalloc)
    {
        g_uiBISOPeekMalloc = g_uiBISOTotMalloc;
    }
    
    free((UCHAR *)pData - 4);
}

VOID BISO_UTIL_DumpMemOp(VOID)
{
    BISO_DUMP("\n Memory Operation: Malloc(%u) Free(%u) \nTotal current use %u, Peek memory use %u.\n", 
              g_uiBISOMallocTime, g_uiBISOFreeTime, g_uiBISOTotMalloc, g_uiBISOPeekMalloc);

#if (1 == MEMORY_DEBUG_DUMP)
{
    UINT i, j;
    for (i = 0; i < g_uiBISOMallocTime; i++)
    {
        for (j = 0; j < g_uiBISOFreeTime; j++)
        {
            if (g_apstBISOMalloc[i] == g_apstBISOFree[j])
            {
                break;
            }
        }

        if (j >= g_uiBISOFreeTime)
        {
            printf("ID %u ptr %p is not freed.\n", i, g_apstBISOMalloc[i]);
        }
    }
}
#endif

}
#endif

INT BISO_UTIL_GetTimeZone(VOID)
{
    INT iTimeZone;
    INT iLocalHour;
    INT iGMTHour;
    time_t ulTime;
    struct tm *pstLocalTM = NULL;
    struct tm *pstGMTM = NULL;
    
    time(&ulTime);
    pstGMTM = gmtime(&ulTime); 
    iGMTHour = pstGMTM->tm_hour;

    pstLocalTM = localtime(&ulTime);
    iLocalHour = pstLocalTM->tm_hour;

    iTimeZone = iLocalHour - iGMTHour;     
    if (iTimeZone < -12) 
    {  
        iTimeZone += 24;
    } 
    else if (iTimeZone > 12) 
    {
        iTimeZone -= 24;
    }

    return iTimeZone;
}

ULONG BISO_UTIL_ReadFile
(
    IN  CONST CHAR *pcFileName, 
    IN  UINT64 ui64Seek, 
    IN  UINT   uiDataLen,
    OUT VOID  *pDataBuf
)
{
    UINT uiReadLen = 0;
    BISO_FILE_S *pstFile = NULL;
    
    if ((NULL == pcFileName) || (NULL == pDataBuf))
    {
        return BISO_ERR_NULL_PTR;
    }

    pstFile = BISO_PLAT_OpenExistFile(pcFileName);
    if (NULL == pstFile)
    {
        return BISO_ERR_OPEN_FILE;
    }

    BISO_PLAT_SeekFile(pstFile, ui64Seek, SEEK_SET);
    uiReadLen = (UINT)BISO_PLAT_ReadFile(pstFile, 1, uiDataLen, pDataBuf);
    if (uiReadLen != uiDataLen)
    {
        BISO_DIAG("Read Len %u, data len %u.", uiReadLen, uiDataLen);
        BISO_PLAT_CloseFile(pstFile);
        return BISO_ERR_READ_FILE;
    }

    BISO_PLAT_CloseFile(pstFile);
    return BISO_SUCCESS;
}


CHAR * BISO_UTIL_CopyStr
(
    IN  CONST CHAR *szSrc, 
    IN  UINT        uiSrcSize, 
    OUT CHAR       *szDest
)
{
    UINT i;
    UINT uiAllSpace = 1;

    for (i = uiSrcSize; i > 0; i--)
    {
        if ((0 != szSrc[i - 1]) && (' ' != szSrc[i - 1]))
        {
            uiAllSpace = 0;
            break;
        }

        if (' ' != szSrc[i - 1])
        {
            uiAllSpace = 0;
        }
    }

    if (i > 0)
    {
        memcpy(szDest, szSrc, i);
    }
    szDest[i] = 0;

    if (uiAllSpace == 1)
    {
        scnprintf(szDest, uiSrcSize, "*All Space*"); /* no safe */
    }

    if (szDest[0] == 0)
    {
        scnprintf(szDest, uiSrcSize, "*Empty*"); /* no safe */
    }

    return szDest;    
}

CHAR * BISO_UTIL_CopyUCS2Str
(
    IN  CONST CHAR *szSrc, 
    IN  UINT        uiSrcSize, 
    OUT CHAR       *szDest
)
{
    UINT i;
    
    memcpy(szDest, szSrc, uiSrcSize);

    for (i = 0; (i * 2 + 1) < uiSrcSize; i++)
    {
        szDest[i] = szDest[i * 2 + 1];
    }
    szDest[i] = 0;

    return szDest;    
}

VOID BISO_UTIL_PathProc(INOUT CHAR *pcPath, INOUT UINT *puiLen)
{
    UINT i;
    
    if ((NULL == pcPath) || (NULL == puiLen) || (0 == *puiLen))
    {
        return;
    }

    /* 把所有的\替换为/ */
    for (i = 0; i < *puiLen; i++)
    {
        if ('\\' == pcPath[i])
        {
            pcPath[i] = '/';
        }
    }

    /* 确保最后有1个/ */
    if ('/' != pcPath[*puiLen - 1])
    {
        pcPath[(*puiLen)++] = '/';
        pcPath[*puiLen] = 0;
    }
}

ULONG BISO_UTIL_PathSplit
(
    IN CONST CHAR *pcFullPath, 
    OUT UINT *puiDirNum, 
    OUT UINT *puiDirPos
)
{
    USHORT usPos = 0;
    USHORT usLen = 0;
    UINT uiDirNum = 0;
    CONST CHAR *pcLastPos = pcFullPath;
    CONST CHAR *pcCurPos = pcFullPath;
    
    DBGASSERT(NULL != pcFullPath);
    DBGASSERT(NULL != puiDirNum);
    DBGASSERT(NULL != puiDirPos);

    while (*pcCurPos)
    {
        if (('/' == *pcCurPos) || ('\\' == *pcCurPos))
        {
            usPos = pcLastPos - pcFullPath;
            usLen = pcCurPos - pcLastPos;
            if (usLen <= 0)
            {
                return BISO_ERR_FAILED;
            }
            
            puiDirPos[uiDirNum] = (UINT)((UINT)usPos << 16) | usLen;

            uiDirNum++;
            pcLastPos = pcCurPos + 1;
        }
        
        pcCurPos++;
    }

    usPos = pcLastPos - pcFullPath;
    usLen = pcCurPos - pcLastPos;
    if (usLen <= 0)
    {
        return BISO_ERR_FAILED;
    }    
    puiDirPos[uiDirNum++] = (UINT)((UINT)usPos << 16) | usLen;
    
    *puiDirNum = uiDirNum;
    return BISO_SUCCESS;
}

BISO_DIR_TREE_S * BISO_UTIL_FindLinkTgt(IN BISO_DIR_TREE_S *pstCurNode)
{
    UINT i = 0;
    UINT uiDirNum = 0;
    UINT auiDirPos[32];
    CHAR szDirName[1024];        
    USHORT usPos = 0;
    USHORT usLen = 0;
    CHAR *pcLink = NULL;
    BISO_DIR_TREE_S *pstFileList = NULL;
    BISO_DIR_TREE_S *pstRootDir = NULL;
    
    DBGASSERT(NULL != pstCurNode);

    /* 如果不是符号链接则返回自己 */
    if (BOOL_TRUE != BISO_DIR_TREE_IS_SYMLINK(pstCurNode))
    {
        return pstCurNode;
    }

    pcLink = pstCurNode->pstPosixInfo->pcLinkSrc;
    
    if ('/' == pcLink[0])
    {
        return NULL;
    }

    /* 把链接分割开 */
    if (BISO_SUCCESS != BISO_UTIL_PathSplit(pcLink, &uiDirNum, auiDirPos))
    {
        return NULL;
    }

    pstRootDir = pstCurNode->pstParent;

    /* 依次查找每一部分目录 */
    for (i = 0; (i < uiDirNum) && (NULL != pstCurNode)&& (NULL != pstRootDir); i++)
    {
        usPos = auiDirPos[i] >> 16;
        usLen = auiDirPos[i] & 0xFF;

        memcpy(szDirName, pcLink + usPos, usLen);
        szDirName[usLen] = 0;

        if (0 == BISO_PATH_STRCMP(szDirName, "."))
        {
            pstCurNode = pstCurNode->pstParent;
        }
        else if (0 == BISO_PATH_STRCMP(szDirName, ".."))
        {
            if (NULL == pstCurNode->pstParent)
            {
                return NULL;
            }
            pstCurNode = pstCurNode->pstParent->pstParent;
            pstRootDir = pstCurNode;
        }
        else
        {
            pstCurNode = pstRootDir->pstChild;
            pstFileList = pstRootDir->pstFileList;

            /* 先找当前所在目录下的文件夹 */
            while (pstCurNode)
            {
                if (0 == BISO_PATH_STRCMP(pstCurNode->szName, szDirName))
                {
                    pstRootDir = pstCurNode;
                    break;
                }
                pstCurNode = pstCurNode->pstNext;
            }

            /* 文件夹找不到就找文件 */
            if (NULL == pstCurNode)
            {
                pstCurNode = pstFileList;
                while (pstCurNode)
                {
                    if (0 == BISO_PATH_STRCMP(pstCurNode->szName, szDirName))
                    {
                        pstRootDir = NULL;
                        break;
                    }
                    pstCurNode = pstCurNode->pstNext;
                }
            }
        }
    }

    return pstCurNode;
}

ULONG BISO_MBUF_Append
(
    IN BISO_MBUF_S *pstMBuf, 
    IN UINT uiDataSize, 
    IN VOID *pData
)
{
    if ((NULL == pstMBuf) || (pstMBuf->uiCurBufNum >= BISO_MBUF_MAX_BLK))
    {
        return BISO_ERR_INVALID_PARAM;
    }

    pstMBuf->apucDataBuf[pstMBuf->uiCurBufNum] = (UCHAR *)BISO_MALLOC(uiDataSize);
    if (NULL == pstMBuf->apucDataBuf[pstMBuf->uiCurBufNum])
    {
        return BISO_ERR_ALLOC_MEM;
    }

    if (NULL == pData)
    {
        memset(pstMBuf->apucDataBuf[pstMBuf->uiCurBufNum], 0, uiDataSize);        
    }
    else
    {
        memcpy(pstMBuf->apucDataBuf[pstMBuf->uiCurBufNum], pData, uiDataSize);        
    }
    
    pstMBuf->auiBufSize[pstMBuf->uiCurBufNum] = uiDataSize;
    pstMBuf->uiTotDataSize += uiDataSize;
    pstMBuf->uiCurBufNum++;
    
    return BISO_SUCCESS;
}

VOID BISO_MBUF_Free(IN BISO_MBUF_S *pstMBuf)
{
    UINT i;
    if (NULL != pstMBuf)
    {
        for (i = 0; i < pstMBuf->uiCurBufNum; i++)
        {
            BISO_FREE(pstMBuf->apucDataBuf[i]);
        }
        memset(pstMBuf, 0, sizeof(BISO_MBUF_S));
    }
}

VOID BISO_MBUF_CopyToBuf(IN CONST BISO_MBUF_S *pstMBuf, OUT VOID *pDataBuf)
{
    UINT i;
    UCHAR *pucDataBuf = (UCHAR *)pDataBuf;
    
    if ((NULL != pstMBuf) && (NULL != pucDataBuf))
    {
        for (i = 0; i < pstMBuf->uiCurBufNum; i++)
        {
            if (NULL != pstMBuf->apucDataBuf[i])
            {
                memcpy(pucDataBuf, pstMBuf->apucDataBuf[i], pstMBuf->auiBufSize[i]);
                pucDataBuf += pstMBuf->auiBufSize[i];
            }
        }
    }
}

VOID BISO_MBUF_PULLUP(INOUT BISO_MBUF_S *pstMBuf)
{
    UINT uiSize = 0;
    VOID *pData = NULL;
    
    DBGASSERT(NULL != pstMBuf);

    if (pstMBuf->uiCurBufNum <= 1)
    {
        return;
    }

    uiSize = pstMBuf->uiTotDataSize;
    pData = BISO_MALLOC(uiSize);
    if (NULL == pData)
    {
        return;
    }

    BISO_MBUF_CopyToBuf(pstMBuf, pData);
    BISO_MBUF_Free(pstMBuf);

    memset(pstMBuf, 0, sizeof(BISO_MBUF_S));
    pstMBuf->apucDataBuf[0] = (UCHAR *)pData;
    pstMBuf->auiBufSize[0]  = uiSize;
    pstMBuf->uiTotDataSize  = uiSize;
    pstMBuf->uiCurBufNum    = 1;

    return;
}

BISO_QUEUE_S * BISO_QUEUE_Create(VOID)
{
    BISO_DLL_S *pstSLL = (BISO_DLL_S *)BISO_ZALLOC(sizeof(BISO_DLL_S));
    if (NULL != pstSLL)
    {
        BISO_DLL_Init(pstSLL);
    }
    return (BISO_QUEUE_S *)pstSLL;
}

VOID BISO_QUEUE_Destroy(IN BISO_QUEUE_S *pstQueue)
{
    BISO_DLL_Free(pstQueue);
    BISO_FREE(pstQueue);    
}

VOID BISO_QUEUE_Push(IN BISO_QUEUE_S *pstQueue, IN VOID *pData)
{
    BISO_QUEUE_NODE_S *pstNode = NULL;
    
    pstNode = (BISO_QUEUE_NODE_S *)BISO_DLL_Last(pstQueue);

    /* 当前节点已满需要扩展新内存节点 */
    if ((NULL == pstNode) || (BISO_QUEUE_PTR_NUM == pstNode->usLast))
    {
        pstNode = (BISO_QUEUE_NODE_S *)BISO_ZALLOC(sizeof(BISO_QUEUE_NODE_S));
        if (NULL == pstNode)
        {
            return;
        }
        BISO_DLL_AddTail(pstQueue, (BISO_DLL_NODE_S *)pstNode);
    }

    /* Last往前走一步 */
    pstNode = (BISO_QUEUE_NODE_S *)BISO_DLL_Last(pstQueue);
    pstNode->apList[pstNode->usLast++] = pData;
}

VOID * BISO_QUEUE_PopHead(IN BISO_QUEUE_S *pstQueue)
{
    VOID *pData = NULL;
    BISO_QUEUE_NODE_S *pstNode = NULL;
    
    pstNode = (BISO_QUEUE_NODE_S *)BISO_DLL_First(pstQueue);
    if (NULL == pstNode)
    {
        return NULL;
    }

    /* First往前走一步 */
    pData = pstNode->apList[pstNode->usFirst++];

    /* 该节点已空，则摘除节点，释放内存 */
    if (pstNode->usFirst == pstNode->usLast)
    {
        BISO_DLL_DelHead(pstQueue);
        BISO_FREE(pstNode);
    }

    return pData;
}

VOID * BISO_QUEUE_PopTail(IN BISO_QUEUE_S *pstQueue)
{
    VOID *pData = NULL;
    BISO_QUEUE_NODE_S *pstNode = NULL;
    
    pstNode = (BISO_QUEUE_NODE_S *)BISO_DLL_Last(pstQueue);
    if ((NULL == pstNode) || (0 == pstNode->usLast))
    {
        return NULL;
    }

    /* Last往后退一步 */
    pstNode->usLast--;
    pData = pstNode->apList[pstNode->usLast];

    /* 该节点已空，则摘除节点，释放内存 */
    if (pstNode->usFirst == pstNode->usLast)
    {
        BISO_DLL_DelTail(pstQueue);
        BISO_FREE(pstNode);
    }

    return pData;
}

UINT64 BISO_UTIL_WholeFile2Buf(IN CONST CHAR *szFileName, OUT UCHAR *pucBuf)
{
    UINT uiFileSize;
    UINT uiReadSize;
    BISO_FILE_S *pstFile = BISO_PLAT_OpenExistFile(szFileName);

    uiFileSize = BISO_PLAT_GetFileSize(szFileName);
    uiReadSize = BISO_PLAT_ReadFile(pstFile, 1, uiFileSize, pucBuf);

    BISO_PLAT_CloseFile(pstFile);

    return uiReadSize;
}


