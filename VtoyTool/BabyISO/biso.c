/******************************************************************************
 * biso.c
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
#include "biso_eltorito.h"
#include "biso_rockridge.h"
#include "biso_joliet.h"
#include "biso_dump.h"

CONST STATIC CHAR *g_aszErrMsg[] = 
{
    "Success",                   /* BISO_SUCCESS */
    "General failed",            /* BISO_ERR_FAILED */
    "Null pointer",              /* BISO_ERR_NULL_PTR */
    "Failed to alloc memory",    /* BISO_ERR_ALLOC_MEM */
    "Failed to open file",       /* BISO_ERR_OPEN_FILE */
    "Failed to read file",       /* BISO_ERR_READ_FILE */
    "Failed to write file",      /* BISO_ERR_WRITE_FILE */
    "Invalid iso-9660 format",   /* BISO_ERR_INVALID_ISO9660 */
    "Unsupported block size",    /* BISO_ERR_UNSUPPORTED_BLKSIZE */
    "Invalid parameter",         /* BISO_ERR_INVALID_PARAM */
    "Not found",                 /* BISO_ERR_NOT_FOUND */
    "Not record in iso file",    /* BISO_ERR_NOT_RECORD */
    "Handle is not initialized", /* BISO_ERR_HANDLE_UNINITIALIZED */
};

int g_biso_debug = 0;

VOID BISO_SetDebug(int debug)
{
    g_biso_debug = debug;
}

CONST CHAR * BISO_GetErrMsg(IN ULONG ulErrCode)
{
    if (ulErrCode > BISO_ERR_BASE)
    {
        ulErrCode -= BISO_ERR_BASE;
    }

    if (ulErrCode > ARRAY_SIZE(g_aszErrMsg))
    {
        return NULL;
    }

    return g_aszErrMsg[ulErrCode];
}

VOID BISO_GetNow(OUT BISO_DATE_S *pstTM)
{
    INT iTimeZone;
    INT iLocalHour;
    INT iGMTHour;
    time_t ulTime;
    struct tm *pstLocalTM = NULL;
    struct tm *pstGMTM = NULL;

    if (NULL == pstTM)
    {
        return;
    }
    
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

    pstTM->usYear    = pstLocalTM->tm_year + 1900;
    pstTM->ucMonth   = pstLocalTM->tm_mon  + 1;
    pstTM->ucDay     = pstLocalTM->tm_mday;
    pstTM->ucHour    = pstLocalTM->tm_hour;
    pstTM->ucMin     = pstLocalTM->tm_min;
    pstTM->ucSecond  = pstLocalTM->tm_sec;
    pstTM->usMillSec = 0;
    pstTM->cZone     = (CHAR)iTimeZone;

    return;
}

VOID BISO_TimeConv(IN ULONG ulTime, OUT BISO_DATE_S *pstTM)
{
    time_t ulTm = ulTime;
    struct tm *pstLocalTM = NULL;

    pstLocalTM = localtime(&ulTm);
    pstTM->usYear    = pstLocalTM->tm_year + 1900;
    pstTM->ucMonth   = pstLocalTM->tm_mon  + 1;
    pstTM->ucDay     = pstLocalTM->tm_mday;
    pstTM->ucHour    = pstLocalTM->tm_hour;
    pstTM->ucMin     = pstLocalTM->tm_min;
    pstTM->ucSecond  = pstLocalTM->tm_sec;
    pstTM->usMillSec = 0;
    pstTM->cZone     = (CHAR)BISO_UTIL_GetTimeZone();
    
    return;
}

BISO_READ_S * BISO_AllocReadHandle(VOID)
{
    return (BISO_READ_S *)BISO_9660_CreateParser();
}

VOID BISO_FreeReadHandle(INOUT BISO_READ_S *pstRead)
{
    BISO_9660_DestroyParser((BISO_PARSER_S *)pstRead);
}

BOOL_T BISO_IsISOFile(IN CONST CHAR *pcFileName)
{
    UINT uiReadLen;
    UINT64 ui64FileSize = 0;
    BISO_FILE_S *pstFile = NULL;
    BISO_VD_S stVolDesc;

    /* 先看文件大小，过小的文件不可能是ISO文件 */
    ui64FileSize = BISO_PLAT_GetFileSize(pcFileName);
    if (ui64FileSize < BISO_SYSTEM_AREA_SIZE + sizeof(BISO_PVD_S))
    {
        return BOOL_FALSE;
    }

    /* 打开ISO文件 */
    pstFile = BISO_PLAT_OpenExistFile(pcFileName);
    if (NULL == pstFile)
    {
        return BOOL_FALSE;
    }

    /* 标准规定前16个逻辑扇区用来保存系统数据，VD信息从第17个扇区开始 */
    BISO_PLAT_SeekFile(pstFile, BISO_SYSTEM_AREA_SIZE, SEEK_SET);

    /* 读出VD信息 */
    uiReadLen = (UINT)BISO_PLAT_ReadFile(pstFile, 1, sizeof(stVolDesc), &stVolDesc);
    if (uiReadLen != sizeof(stVolDesc))
    {
        BISO_PLAT_CloseFile(pstFile);
        return BOOL_FALSE;
    }

    /* 根据ID检验是否是合法的ISO-9660格式 */
    if (0 != strncmp(stVolDesc.szId, BISO_VD_ID, strlen(BISO_VD_ID)))
    {
        BISO_PLAT_CloseFile(pstFile);
        return BOOL_FALSE;
    }

    BISO_PLAT_CloseFile(pstFile);
    return BOOL_TRUE;
}

BOOL_T BISO_HasSVD(IN CONST BISO_READ_S *pstRead)
{
    if (((BISO_PARSER_S *)pstRead)->pstSVD)
    {
        return BOOL_TRUE;
    }

    return BOOL_FALSE;
}

BOOL_T BISO_IsUDFFile(IN CONST CHAR *pcFileName)
{
    UINT uiReadLen;
    UINT64 ui64FileSize = 0;
    BISO_FILE_S *pstFile = NULL;
    BISO_VD_S stVolDesc;

    /* 先看文件大小，过小的文件不可能是ISO文件 */
    ui64FileSize = BISO_PLAT_GetFileSize(pcFileName);
    if (ui64FileSize < BISO_SYSTEM_AREA_SIZE + sizeof(BISO_PVD_S))
    {
        return BOOL_FALSE;
    }

    /* 打开ISO文件 */
    pstFile = BISO_PLAT_OpenExistFile(pcFileName);
    if (NULL == pstFile)
    {
        return BOOL_FALSE;
    }

    /* 标准规定前16个逻辑扇区用来保存系统数据，VD信息从第17个扇区开始 */
    BISO_PLAT_SeekFile(pstFile, BISO_SYSTEM_AREA_SIZE, SEEK_SET);

    do
    {
        /* 每次读取1个VD结构 */
        uiReadLen = (UINT)BISO_PLAT_ReadFile(pstFile, 1, sizeof(stVolDesc), &stVolDesc);
        if (uiReadLen != sizeof(stVolDesc))
        {
            BISO_PLAT_CloseFile(pstFile);
            return BOOL_FALSE;
        }
    } while (BISO_VD_TYPE_END != stVolDesc.ucType);

    /* 根据ID检验是否是合法的UDF格式 */
    (VOID)BISO_PLAT_ReadFile(pstFile, 1, sizeof(stVolDesc), &stVolDesc);
    if (0 != strncmp(stVolDesc.szId, "BEA01", strlen("BEA01")))
    {
        BISO_PLAT_CloseFile(pstFile);
        return BOOL_FALSE;
    }

    /* 根据ID检验是否是合法的UDF格式 */
    (VOID)BISO_PLAT_ReadFile(pstFile, 1, sizeof(stVolDesc), &stVolDesc);
    if (0 != strncmp(stVolDesc.szId, "NSR02", strlen("NSR02")) &&
        0 != strncmp(stVolDesc.szId, "NSR03", strlen("NSR03")))
    {
        BISO_PLAT_CloseFile(pstFile);
        return BOOL_FALSE;
    }

    BISO_PLAT_CloseFile(pstFile);
    return BOOL_TRUE;
}

ULONG BISO_OpenImage(IN CONST CHAR *pcFileName, OUT BISO_READ_S *pstRead)
{
    return BISO_9660_OpenImage(BOOL_FALSE, pcFileName, (BISO_PARSER_S *)pstRead);
}

ULONG BISO_OpenImageWithSVD(IN CONST CHAR *pcFileName, OUT BISO_READ_S *pstRead)
{
    return BISO_9660_OpenImage(BOOL_TRUE, pcFileName, (BISO_PARSER_S *)pstRead);
}

ULONG BISO_GetVolumeSummary
(
    IN CONST BISO_READ_S *pstRead, 
    OUT BISO_VOLUME_SUMMARY_S *pstSummary
)
{
    BISO_PVD_S *pstPVD = NULL;
    BISO_PARSER_S *pstParser = NULL;

    if (NULL == pstRead || NULL == pstSummary)
    {
        return BISO_ERR_NULL_PTR;
    }

    if (BOOL_TRUE != BISO_IS_READ_HANDLE_VALID(pstRead))
    {
        return BISO_ERR_INVALID_PARAM;
    }

    pstParser = (BISO_PARSER_S *)pstRead;
    pstPVD = pstParser->pstPVD;

    /* 拷贝字符串 */
    BISO_UTIL_CopyStr(pstPVD->szVolumeId, sizeof(pstPVD->szVolumeId), pstSummary->szVolumeId);
    BISO_UTIL_CopyStr(pstPVD->szSystemId, sizeof(pstPVD->szSystemId), pstSummary->szSystemId);
    BISO_UTIL_CopyStr(pstPVD->szPublisherId, sizeof(pstPVD->szPublisherId), pstSummary->szPublisherId);
    BISO_UTIL_CopyStr(pstPVD->szPreparerId, sizeof(pstPVD->szPreparerId), pstSummary->szPreparerId);
    BISO_UTIL_CopyStr(pstPVD->szApplicationId, sizeof(pstPVD->szApplicationId), pstSummary->szApplicationId);
    BISO_UTIL_CopyStr(pstPVD->szCopyrightFileId, sizeof(pstPVD->szCopyrightFileId), pstSummary->szCopyrightFileId);
    BISO_UTIL_CopyStr(pstPVD->szAbstractFileId, sizeof(pstPVD->szAbstractFileId), pstSummary->szAbstractFileId);

    /* 其他字段赋值 */
    pstSummary->uiRockRidgeVer = pstParser->ucRRIPVersion;
    pstSummary->uiJolietLevel  = BISO_GetJolietLevel(pstRead);
    pstSummary->uiTotDirNum    = pstParser->stDirTree.pstDirStat->uiTotDirNum;
    pstSummary->uiTotFileNum   = pstParser->stDirTree.pstDirStat->uiTotFileNum;
    pstSummary->uiTotLinkNum   = pstParser->stDirTree.pstDirStat->uiTotLinkNum;

    return BISO_SUCCESS;
}

ULONG BISO_GetDate
(
    IN CONST BISO_READ_S *pstRead, 
    IN  BISO_DATE_TYPE_E enType, 
    OUT BISO_DATE_S *pstDate
)
{
    CONST CHAR *pcDate = NULL;
    BISO_PVD_S *pstPVD = NULL;
    
    if ((NULL == pstRead) || (enType >= BISO_DATE_TYPE_BUTT) || (NULL == pstDate))
    {
        BISO_DIAG("Invalid param %p %d %p.", pstRead, enType, pstDate);
        return BISO_ERR_INVALID_PARAM;
    }

    BISO_CHECK_READ_HANDLE(pstRead);
    pstPVD = ((BISO_PARSER_S *)pstRead)->pstPVD;

    switch (enType)
    {
        case BISO_DATE_TYPE_CREATE:
        {
            pcDate = pstPVD->szCreationDate;
            break;
        }
        case BISO_DATE_TYPE_MODIFY:
        {
            pcDate = pstPVD->szModifyDate;
            break;
        }
        case BISO_DATE_TYPE_EXPIRATION:
        {
            pcDate = pstPVD->szExpirationDate;
            break;
        }
        case BISO_DATE_TYPE_EFFECTIVE:
        {
            pcDate = pstPVD->szEffectiveDate;
            break;
        }
        default :
        {
            return BISO_ERR_INVALID_PARAM;
        }
    }

    return BISO_9660_ParseDate84261(pcDate, pstDate);
}

/* 获取Rock Ridge扩展的Version 0: 没有使用Rock Ridge扩展  具体版本号: 一般都是1 */
UINT BISO_GetRockRidgeVer(IN CONST BISO_READ_S *pstRead)
{  
    if ((NULL == pstRead) || (BOOL_TRUE != BISO_IS_READ_HANDLE_VALID(pstRead)))
    {
        return 0;
    }

    return ((BISO_PARSER_S *)pstRead)->ucRRIPVersion;    
}

/* 获取Joliet扩展的Level */
UINT BISO_GetJolietLevel(IN CONST BISO_READ_S *pstRead)
{    
    BISO_PARSER_S *pstParser = NULL;

    if ((NULL == pstRead) || (BOOL_TRUE != BISO_IS_READ_HANDLE_VALID(pstRead)))
    {
        return 0;
    }
    
    pstParser = (BISO_PARSER_S *)pstRead;
    if (NULL == pstParser->pstSVD)
    {
        return 0;
    }
    return BISO_JOLIET_GetLevel(pstParser->pstSVD->aucEscape);
}

BISO_HANDLE BISO_GetRoot(IN CONST BISO_READ_S *pstRead)
{
    BISO_PARSER_S *pstParser = (BISO_PARSER_S *)pstRead;
    
    if (NULL == pstParser)
    {
        return NULL;
    }
    return (BISO_HANDLE)(&pstParser->stDirTree);
}

ULONG BISO_GetFileNodeByHdl
(
    IN  BISO_HANDLE       hFileHdl, 
    OUT BISO_FILE_NODE_S *pstFileNode
)
{
    BISO_POSIX_INFO_S *pstPosix = NULL;
    BISO_DIR_TREE_S *pstDirTree = (BISO_DIR_TREE_S *)hFileHdl;

    if ((NULL == pstDirTree) || (NULL == pstFileNode))
    {
        return BISO_ERR_NULL_PTR;
    }

    pstPosix = pstDirTree->pstPosixInfo;

    /* 设置类型 */
    BISO_SET_FLAG(pstFileNode, pstDirTree);

    /* 设置名称 */
    scnprintf(pstFileNode->szName, sizeof(pstFileNode->szName), "%s", pstDirTree->szName);

    /* 设置连接路径 */
    if (BOOL_TRUE == BISO_DIR_TREE_IS_SYMLINK(pstDirTree))
    {
        scnprintf(pstFileNode->szLinkTgt, sizeof(pstFileNode->szLinkTgt), "%s", pstPosix->pcLinkSrc);
    }

    pstFileNode->ui64FileSize = pstDirTree->uiSize;
    pstFileNode->ui64Seek = (UINT64)((UINT64)pstDirTree->uiExtent * BISO_BLOCK_SIZE);
    pstFileNode->hParent = (BISO_HANDLE)(pstDirTree->pstParent);
    pstFileNode->hCurrent = hFileHdl;

    return BISO_SUCCESS;
}

ULONG BISO_GetFileNodeByName
(
    IN CONST BISO_READ_S *pstRead,
    IN CONST CHAR *pcFullPath, 
    IN UCHAR ucFollowLink,
    OUT BISO_FILE_NODE_S *pstFileNode
)
{
    UINT i = 0;
    UINT uiDirNum = 0;
    UINT auiDirPos[32];
    USHORT usPos = 0;
    USHORT usLen = 0;
    CHAR szDirName[1024];
    BISO_DIR_TREE_S *pstCurDir = NULL;
    BISO_DIR_TREE_S *pstRootDir = NULL;
    BISO_DIR_TREE_S *pstFileList = NULL;
    BISO_PARSER_S *pstParser = (BISO_PARSER_S *)pstRead;

    if ((NULL == pstRead) || (NULL == pcFullPath) || (NULL == pstFileNode))
    {
        return BISO_ERR_NULL_PTR;
    }

    if ('/' == pcFullPath[0])
    {
        return BISO_ERR_FAILED;
    }

    pstRootDir = &(pstParser->stDirTree);
    pstCurDir = pstRootDir->pstChild;

    if ((0 == pcFullPath[0]) || ((1 == strlen(pcFullPath)) && ('/' == pcFullPath[0])))
    {
        /* 出参赋值 */
        memset(pstFileNode, 0, sizeof(BISO_FILE_NODE_S));        
        BISO_SET_FLAG(pstFileNode, pstCurDir);
        scnprintf(pstFileNode->szName, sizeof(pstFileNode->szName), "%s", pstCurDir->szName);
        pstFileNode->hParent = 0;
        pstFileNode->hCurrent = (BISO_HANDLE)(pstRootDir);
        return BISO_SUCCESS;
    }

    if ((1 == uiDirNum) && (NULL != pstRootDir))
    {
        pstFileList = pstRootDir->pstFileList;
        pstCurDir = pstFileList;
        while (pstCurDir)
        {
            if (0 == BISO_PATH_STRCMP(pstCurDir->szName, pcFullPath))
            {
                goto FOUND;
            }
            pstCurDir = pstCurDir->pstNext;
        }
    }

    /* 先将目录分解开 */
    if (BISO_SUCCESS != BISO_UTIL_PathSplit(pcFullPath, &uiDirNum, auiDirPos))
    {
        BISO_DIAG("Failed to split path %s", pcFullPath);
        return BISO_ERR_FAILED;
    }

    /* 依次查找每一级目录 */
    if (pstRootDir)
    {
        pstCurDir = pstRootDir->pstChild;
    } 
    for (i = 0; (i < uiDirNum) && (NULL != pstRootDir) && (NULL != pstCurDir); i++)
    {
        usPos = auiDirPos[i] >> 16;
        usLen = auiDirPos[i] & 0xFF;

        memcpy(szDirName, pcFullPath + usPos, usLen);
        szDirName[usLen] = 0;

        pstCurDir = pstRootDir->pstChild;
        pstFileList = pstRootDir->pstFileList;

        /* 先查找目录 */
        while (pstCurDir)
        {
            if (0 == BISO_PATH_STRCMP(pstCurDir->szName, szDirName))
            {
                pstRootDir = pstCurDir;
                break;
            }
            pstCurDir = pstCurDir->pstNext;
        }

        /* 再查找文件 */
        if (NULL == pstCurDir)
        {
            pstCurDir = pstFileList;
            while (pstCurDir)
            {
                if (0 == BISO_PATH_STRCMP(pstCurDir->szName, szDirName))
                {
                    break;
                }
                pstCurDir = pstCurDir->pstNext;
            }
        }

        if (NULL == pstCurDir)
        {
            return BISO_ERR_FAILED;
        }

        /* 如果是符号链接则尝试找对应的实际文件 */
        if ((ucFollowLink > 0) && (BOOL_TRUE == BISO_DIR_TREE_IS_SYMLINK(pstCurDir)))
        {
            pstCurDir = BISO_UTIL_FindLinkTgt(pstCurDir);
        }

        /* 如果是文件(或者是非法链接)的话一定是最后一级 */
        if ((NULL == pstCurDir->pstDirStat) && (i + 1 != uiDirNum))
        {
            return BISO_ERR_FAILED;
        }
    }

FOUND:

    if (NULL == pstCurDir)
    {
        return BISO_ERR_FAILED;
    }
    else
    {
        /* 出参赋值 */
        memset(pstFileNode, 0, sizeof(BISO_FILE_NODE_S));        
        BISO_SET_FLAG(pstFileNode, pstCurDir);
        scnprintf(pstFileNode->szName, sizeof(pstFileNode->szName), "%s", pstCurDir->szName);
        if (BOOL_TRUE == BISO_DIR_TREE_IS_SYMLINK(pstCurDir))
        {
            scnprintf(pstFileNode->szLinkTgt, sizeof(pstFileNode->szLinkTgt), "%s", 
                      pstCurDir->pstPosixInfo->pcLinkSrc);
        }
        pstFileNode->ui64FileSize = pstCurDir->uiSize;
        pstFileNode->ui64DirRecOffet = pstCurDir->ui64FileRecordOffset;
        pstFileNode->ui64Seek = (UINT64)((UINT64)pstCurDir->uiExtent * BISO_BLOCK_SIZE);
        pstFileNode->hParent = (BISO_HANDLE)(pstCurDir->pstParent);
        pstFileNode->hCurrent = (BISO_HANDLE)(pstCurDir);
        return BISO_SUCCESS;
    }
}

ULONG BISO_GetFileNodeByExtent
(
    IN CONST BISO_READ_S *pstRead,
    IN UINT uiExtent,
    OUT BISO_FILE_NODE_S *pstFileNode
)
{
    BOOL_T bFind = BOOL_FALSE;
    BISO_QUEUE_S *pstQueue = NULL;
    BISO_DIR_TREE_S *pstRootDir = NULL;
    BISO_DIR_TREE_S *pstDirTree = NULL;
    BISO_DIR_TREE_S *pstCurDir = NULL;
    BISO_DIR_TREE_S *pstFileList = NULL;
    BISO_PARSER_S *pstParser = (BISO_PARSER_S *)pstRead;

    if ((NULL == pstRead) || (NULL == pstFileNode))
    {
        return BISO_ERR_NULL_PTR;
    }

    pstRootDir = &(pstParser->stDirTree);

    /* 创建堆栈,同时ROOT入栈 */
    pstQueue = BISO_QUEUE_Create();
    BISO_QUEUE_Push(pstQueue, pstRootDir);

    while (NULL != (pstDirTree = (BISO_DIR_TREE_S *)BISO_QUEUE_PopHead(pstQueue)))
    {
        pstCurDir = pstDirTree->pstChild;
        while (pstCurDir)
        {
            BISO_QUEUE_Push(pstQueue, pstCurDir);
            pstCurDir = pstCurDir->pstNext;
        }
        
        pstFileList = pstDirTree->pstFileList;
        pstCurDir = pstFileList;
        while (pstCurDir)
        {
            if (uiExtent == pstCurDir->uiExtent)
            {
                while (BISO_QUEUE_PopHead(pstQueue))
                {
                    bFind = BOOL_TRUE;
                }
                break;
            }
            pstCurDir = pstCurDir->pstNext;
        }
    }

    BISO_QUEUE_Destroy(pstQueue);
    if (BOOL_TRUE != bFind)
    {
        return BISO_ERR_FAILED;
    }
    else
    {
        /* 出参赋值 */
        memset(pstFileNode, 0, sizeof(BISO_FILE_NODE_S));        
        BISO_SET_FLAG(pstFileNode, pstCurDir);
        scnprintf(pstFileNode->szName, sizeof(pstFileNode->szName), "%s", pstCurDir->szName);
        if (BOOL_TRUE == BISO_DIR_TREE_IS_SYMLINK(pstCurDir))
        {
            scnprintf(pstFileNode->szLinkTgt, sizeof(pstFileNode->szLinkTgt), "%s", 
                      pstCurDir->pstPosixInfo->pcLinkSrc);
        }
        pstFileNode->ui64FileSize = pstCurDir->uiSize;
        pstFileNode->ui64DirRecOffet = pstCurDir->ui64FileRecordOffset;
        pstFileNode->ui64Seek = (UINT64)((UINT64)pstCurDir->uiExtent * BISO_BLOCK_SIZE);
        pstFileNode->hParent = (BISO_HANDLE)(pstCurDir->pstParent);
        pstFileNode->hCurrent = (BISO_HANDLE)(pstCurDir);
        return BISO_SUCCESS;
    }
}


ULONG BISO_GetSVDFileNodeByExtent
(
    IN CONST BISO_READ_S *pstRead,
    IN UINT uiExtent,
    OUT BISO_SVD_FILE_NODE_S *pstFileNode
)
{
    BOOL_T bFind = BOOL_FALSE;
    BISO_QUEUE_S *pstQueue = NULL;
    BISO_SVD_DIR_TREE_S *pstRootDir = NULL;
    BISO_SVD_DIR_TREE_S *pstDirTree = NULL;
    BISO_SVD_DIR_TREE_S *pstCurDir = NULL;
    BISO_SVD_DIR_TREE_S *pstFileList = NULL;
    BISO_PARSER_S *pstParser = (BISO_PARSER_S *)pstRead;

    if ((NULL == pstRead) || (NULL == pstFileNode))
    {
        return BISO_ERR_NULL_PTR;
    }

    pstRootDir = &(pstParser->stSVDDirTree);

    /* 创建堆栈,同时ROOT入栈 */
    pstQueue = BISO_QUEUE_Create();
    BISO_QUEUE_Push(pstQueue, pstRootDir);

    while (NULL != (pstDirTree = (BISO_SVD_DIR_TREE_S *)BISO_QUEUE_PopHead(pstQueue)))
    {
        pstCurDir = pstDirTree->pstChild;
        while (pstCurDir)
        {
            BISO_QUEUE_Push(pstQueue, pstCurDir);
            pstCurDir = pstCurDir->pstNext;
        }
        
        pstFileList = pstDirTree->pstFileList;
        pstCurDir = pstFileList;
        while (pstCurDir)
        {
            if (uiExtent == pstCurDir->uiExtent)
            {
                while (BISO_QUEUE_PopHead(pstQueue))
                {
                    bFind = BOOL_TRUE;
                }
                break;
            }
            pstCurDir = pstCurDir->pstNext;
        }
    }

    BISO_QUEUE_Destroy(pstQueue);
    if (BOOL_TRUE != bFind)
    {
        return BISO_ERR_FAILED;
    }
    else
    {
        /* 出参赋值 */
        memset(pstFileNode, 0, sizeof(BISO_SVD_FILE_NODE_S));        
        pstFileNode->ui64FileSize = pstCurDir->uiSize;
        pstFileNode->ui64DirRecOffet = pstCurDir->ui64FileRecordOffset;
        pstFileNode->ui64Seek = (UINT64)((UINT64)pstCurDir->uiExtent * BISO_BLOCK_SIZE);
        return BISO_SUCCESS;
    }
}

ULONG BISO_GetFileTree
(
    IN  BISO_HANDLE  hTopDir, 
    IN  UINT         uiFlag,
    OUT BISO_HANDLE *phFileTree,
    OUT UINT        *puiNodeNum
)
{    
    BISO_DIR_STAT_S *pstDirStat = NULL;
    BISO_DIR_TREE_S *pstCurNode = NULL;
    BISO_DIR_TREE_S *pstDirTree = (BISO_DIR_TREE_S *)hTopDir;
    
    if ((NULL == pstDirTree) || (NULL == phFileTree) || (NULL == puiNodeNum))
    {
        return BISO_ERR_NULL_PTR;
    }

    pstDirStat = pstDirTree->pstDirStat;
    if (NULL == pstDirStat)
    {
        return BISO_ERR_INVALID_PARAM;
    }

    *puiNodeNum = pstDirStat->uiCurDirNum + pstDirStat->uiCurFileNum + pstDirStat->uiCurLinkNum;

    switch (uiFlag)
    {
        case BISO_TREE_FLAG_CUR:
        {
            pstCurNode = pstDirTree->pstChild;
            
            while (NULL != pstCurNode)
            {
                *phFileTree++ = (BISO_HANDLE)pstCurNode;
                pstCurNode = pstCurNode->pstNext;
            }

            pstCurNode = pstDirTree->pstFileList;
            while (NULL != pstCurNode)
            {
                *phFileTree++ = (BISO_HANDLE)pstCurNode;
                pstCurNode = pstCurNode->pstNext;
            }
            
            break;
        }
        case BISO_TREE_FLAG_DFS:
        {
            break;
        }
        case BISO_TREE_FLAG_BFS:
        {
            break;
        }
        default :
        {
            return BISO_ERR_INVALID_PARAM;
        }
    }

    return BISO_SUCCESS;
}

ULONG BISO_GetDirStat
(
    IN  BISO_HANDLE      hTopDir, 
    OUT BISO_DIR_STAT_S *pstDirStat
)
{
    BISO_DIR_TREE_S *pstDirTree = NULL;
    
    if ((NULL == hTopDir) || (NULL == pstDirStat))
    {
        return BISO_ERR_NULL_PTR;
    }

    pstDirTree = (BISO_DIR_TREE_S *)hTopDir;
    if (NULL == pstDirTree->pstDirStat)
    {
        return BISO_ERR_INVALID_PARAM;
    }
    
    memcpy(pstDirStat, pstDirTree->pstDirStat, sizeof(BISO_DIR_STAT_S));
    return BISO_SUCCESS;
}


VOID BISO_Fill733(IN UINT uiData, OUT VOID *pBuf)
{
    UINT uiSwap = 0;
    UINT *puiData = (UINT *)pBuf;

    uiSwap |= (uiData & 0xFF) << 24;
    uiSwap |= ((uiData >> 8) & 0xFF) << 16;
    uiSwap |= ((uiData >> 16) & 0xFF) << 8;
    uiSwap |= (uiData >> 24) & 0xFF;

#if (__BYTE_ORDER == __LITTLE_ENDIAN)
    puiData[0] = uiData;
    puiData[1] = uiSwap;
#else
    puiData[0] = uiSwap;
    puiData[1] = uiData;
#endif    
}

UINT BISO_Get733(IN CONST VOID *pBuf)
{
    UINT *puiData = (UINT *)pBuf;
    
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
    return puiData[0];
#else
    return puiData[1];
#endif  
}

UINT BISO_GetFileOccupySize(IN UINT uiRawSize)
{
    UINT uiAlign = uiRawSize % BISO_SECTOR_SIZE;
    
    if (0 == uiAlign)
    {
        return uiRawSize;
    }
    else
    {
        return uiRawSize + BISO_SECTOR_SIZE - uiAlign;
    }
}

UINT BISO_GetBootEntryNum(IN CONST BISO_READ_S *pstRead)
{
    return BISO_ELTORITO_GetBootEntryNum((CONST BISO_PARSER_S *)pstRead);
}

VOID BISO_DumpFileTree(IN CONST BISO_READ_S *pstRead)
{
    BISO_PARSER_S *pstParser = (BISO_PARSER_S *)pstRead;

    if (NULL != pstParser)
    {
        BISO_DUMP_ShowFileTree(1, pstParser->stDirTree.pstChild);
        BISO_DUMP_ShowFileTree(1, pstParser->stDirTree.pstFileList);
    }
}

