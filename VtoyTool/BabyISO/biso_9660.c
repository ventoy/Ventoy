/******************************************************************************
 * biso_9660.c
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
#include "biso_plat.h"
#include "biso_9660.h"
#include "biso_eltorito.h"
#include "biso_rockridge.h"
#include "biso_dump.h"

STATIC ULONG BISO_9660_ReadPathTable(IN BISO_FILE_S *pstFile, OUT BISO_PARSER_S *pstParser)
{
    UINT64 ui64Seek = 0;
    UINT uiReadLen = 0;
    UCHAR *pucBuf = NULL;
    BISO_PVD_S *pstPVD = NULL;

    DBGASSERT(NULL != pstFile);
    DBGASSERT(NULL != pstParser);
        
    pstPVD = pstParser->pstPVD;
    ui64Seek = BISO_PATHTBL_LOCATION(pstPVD);
    ui64Seek = ui64Seek * BISO_BLOCK_SIZE;

    /*
     * 申请内存用于保存Path Table
     * 由于Path Table是连续保存的，所以这里一次性读出保存，使用时再做解析
     * Path Table保存时是连续的，一条Path Table可以跨block, 最后一个扇区的剩余空间填0
     */
    pucBuf = (UCHAR *)BISO_MALLOC(pstPVD->uiPathTblSize);
    if (NULL == pucBuf)
    {
        return BISO_ERR_ALLOC_MEM;
    }

    BISO_PLAT_SeekFile(pstFile, ui64Seek, SEEK_SET);

    /* 读出Path Table */
    uiReadLen = (UINT)BISO_PLAT_ReadFile(pstFile, 1, pstPVD->uiPathTblSize, pucBuf);
    if (uiReadLen != pstPVD->uiPathTblSize)
    {
        BISO_FREE(pucBuf);
        BISO_DIAG("Read Len %u, data len %u.", uiReadLen, pstPVD->uiPathTblSize);
        return BISO_ERR_READ_FILE;
    }

    pstParser->pucPathTable = pucBuf;
    return BISO_SUCCESS;
}

/* 深度优先文件树目录节点入栈 注意这里只是针对目录节点入栈, 文件节点没有处理 */
VOID BISO_9660_FillDfsStack
(
    IN BISO_DIR_TREE_S *pstTop, 
    INOUT BISO_QUEUE_S *pstQueue
)
{
    BISO_DIR_TREE_S *pstCurDir = NULL;
    
    DBGASSERT(NULL != pstTop);
    DBGASSERT(NULL != pstQueue);

    /* TOP入栈 */
    BISO_QUEUE_Push(pstQueue, pstTop);

    pstCurDir = pstTop->pstChild;
    if (NULL == pstCurDir)
    {
        return;
    }

    for ( ; ; )
    {
        /* 
         * 按照以下顺序依次入栈:
         * 1. 自己入栈
         * 2. 子节点入栈
         * 3. Next节点入栈
         * 4. Parent的Next节点入栈
         */
    
        BISO_QUEUE_Push(pstQueue, pstCurDir);

        if (NULL != pstCurDir->pstChild)
        {
            pstCurDir = pstCurDir->pstChild;
        }
        else if (NULL != pstCurDir->pstNext)
        {
            pstCurDir = pstCurDir->pstNext;
        }
        else
        {
            /* 往上回溯, 一直找到需要入栈的节点 */
            while ((pstTop != pstCurDir->pstParent) && (NULL == pstCurDir->pstParent->pstNext))
            {
                pstCurDir = pstCurDir->pstParent;
            }

            if (pstTop == pstCurDir->pstParent)
            {
                break;
            }
            pstCurDir = pstCurDir->pstParent->pstNext;
        }
    }
}

/* 根据Extent的值查找子目录节点  */
STATIC BISO_DIR_TREE_S *BISO_9660_FindChild
(
    IN BISO_DIR_TREE_S *pstParent, 
    IN UINT uiChildExtent
)
{
    BISO_DIR_TREE_S *pstCurNode = NULL;
    
    DBGASSERT(NULL != pstParent);
    
    pstCurNode = pstParent->pstChild;
    while (NULL != pstCurNode)
    {
        if (pstCurNode->uiExtent == uiChildExtent)
        {
            return pstCurNode;
        }
        pstCurNode = pstCurNode->pstNext;
    }
    return NULL;
}

STATIC ULONG BISO_9660_ReadVD(IN BISO_FILE_S *pstFile, OUT BISO_PARSER_S *pstParser)
{
    UINT uiReadLen = 0;
    BISO_VD_S stVolDesc;
    BISO_VD_NODE_S *pstVdNode = NULL;

    DBGASSERT(NULL != pstFile);
    DBGASSERT(NULL != pstParser);

    /* 标准规定前16个逻辑扇区用来保存系统数据，VD信息从第17个扇区开始 */
    BISO_PLAT_SeekFile(pstFile, BISO_SYSTEM_AREA_SIZE, SEEK_SET);

    do
    {
        /* 每次读取1个VD结构 */
        uiReadLen = (UINT)BISO_PLAT_ReadFile(pstFile, 1, sizeof(stVolDesc), &stVolDesc);
        if (uiReadLen != sizeof(stVolDesc))
        {
            BISO_DIAG("Read Len %u, struct len %u.", uiReadLen, (UINT)sizeof(stVolDesc));
            return BISO_ERR_READ_FILE;
        }

        /* 根据ID检验是否是合法的ISO-9660格式 */
        if (0 != strncmp(stVolDesc.szId, BISO_VD_ID, strlen(BISO_VD_ID)))
        {
            BISO_DIAG("Invalid cdid: %02x %02x %02x %02x %02x\n", 
                (UCHAR)stVolDesc.szId[0], (UCHAR)stVolDesc.szId[1], 
                (UCHAR)stVolDesc.szId[2], (UCHAR)stVolDesc.szId[3], 
                (UCHAR)stVolDesc.szId[4]);
            return BISO_ERR_INVALID_ISO9660;
        }

        /* 申请内存保存VD信息 */
        pstVdNode = (BISO_VD_NODE_S *)BISO_ZALLOC(sizeof(BISO_VD_NODE_S));
        if (NULL == pstVdNode)
        {
            return BISO_ERR_ALLOC_MEM;
        }

        /* 链表节点挂接 */
        memcpy(&(pstVdNode->stVD), &stVolDesc, sizeof(BISO_VD_S));
        BISO_DLL_AddTail(&(pstParser->stVDList), (BISO_DLL_NODE_S *)pstVdNode);

        switch (stVolDesc.ucType)
        {
            case BISO_VD_TYPE_BOOT:
            {
                pstParser->pstBVD = (BISO_BVD_S *)&(pstVdNode->stVD);
                pstParser->pstBVD->uiBootCatlogStart = BISO_LTOH_UINT(pstParser->pstBVD->uiBootCatlogStart);
                break;
            }
            case BISO_VD_TYPE_PVD:
            {
                pstParser->pstPVD = (BISO_PVD_S *)&(pstVdNode->stVD);
                break;
            }
            case BISO_VD_TYPE_SVD:
            {
                pstParser->pstSVD = (BISO_SVD_S *)&(pstVdNode->stVD);
                break;
            }
            case BISO_VD_TYPE_PART:
            case BISO_VD_TYPE_END:
            {
                break;
            }
            default :
            {
                BISO_DIAG("Invalid VD type: %u\n", stVolDesc.ucType);
                return BISO_ERR_INVALID_ISO9660;
            }
        }
    } while (BISO_VD_TYPE_END != stVolDesc.ucType);

    /* 标准规定必须有1个主卷描述符 */
    if (NULL == pstParser->pstPVD)
    {
        BISO_DIAG("No PVD found.");
        return BISO_ERR_INVALID_ISO9660;
    }

    /* 目前只支持逻辑块大小为2048 */
    if (BISO_BLOCK_SIZE != pstParser->pstPVD->usBlockSize)
    {
        BISO_DIAG("Unsupported block size %u.", pstParser->pstPVD->usBlockSize);
        return BISO_ERR_UNSUPPORTED_BLKSIZE;
    }
    
    return BISO_SUCCESS;
}

STATIC UCHAR * BISO_9660_ReadDirRecord
(
    IN  BISO_FILE_S *pstFile, 
    IN  UINT  uiExtent, 
    OUT UINT *puiSize
)
{
    UINT64 ui64Seek = 0;
    UINT uiReadLen = 0;
    UCHAR *pucBuf = NULL;
    BISO_DIR_RECORD_S stCurrent;

    DBGASSERT(NULL != pstFile);
    DBGASSERT(NULL != puiSize);

    /* 第一条Dir Record是Current, 先读出自己,得到总的缓冲区长度 */
    ui64Seek = BISO_BLOCK_SIZE * (UINT64)uiExtent;
    BISO_PLAT_SeekFile(pstFile, ui64Seek, SEEK_SET);
    uiReadLen = (UINT)BISO_PLAT_ReadFile(pstFile, 1, sizeof(stCurrent), &stCurrent);
    if (uiReadLen != sizeof(stCurrent))
    {
        BISO_DIAG("Read len %u, buf len %u.", uiReadLen, (UINT)sizeof(stCurrent));
        return NULL;
    }

    /* 申请内存, 一次性把当前目录的Directory信息全部读出 */
    pucBuf = (UCHAR *)BISO_MALLOC(stCurrent.uiSize);
    if (NULL == pucBuf)
    {
        return NULL;
    }

    BISO_PLAT_SeekFile(pstFile, ui64Seek, SEEK_SET);
    uiReadLen = (UINT)BISO_PLAT_ReadFile(pstFile, 1, stCurrent.uiSize, pucBuf);
    if (uiReadLen != stCurrent.uiSize)
    {
        BISO_DIAG("Read len %u, buf len %u.", uiReadLen, stCurrent.uiSize);
        BISO_FREE(pucBuf);
        return NULL;
    }

    *puiSize = stCurrent.uiSize;
    return pucBuf;
}

STATIC BISO_DIR_TREE_S * BISO_9660_CreateDirNode
(
    IN BISO_FILE_S *pstFile,
    IN BISO_PARSER_S *pstParser,
    IN BISO_DIR_RECORD_S  *pstRecord,
    INOUT BISO_DIR_TREE_S *pstPre,
    INOUT BISO_DIR_TREE_S *pstParent
)
{
    BISO_DIR_TREE_S *pstNew = NULL;
    
    DBGASSERT(NULL != pstRecord);
    DBGASSERT(NULL != pstPre);
    DBGASSERT(NULL != pstParent);

    /* 申请内存用于保存目录节点 */
    pstNew = (BISO_DIR_TREE_S *)BISO_ZALLOC(sizeof(BISO_DIR_TREE_S));
    if (NULL == pstNew)
    {
        return NULL;
    }
    
    /* 目录节点属性赋值 */
    BISO_UTIL_CopyStr(pstRecord->szName, pstRecord->ucNameLen, pstNew->szName);
    pstNew->uiExtent = pstRecord->uiExtent;
    pstNew->usNameLen = (USHORT)strlen(pstNew->szName);

    /* 申请统计信息的节点 */
    pstNew->pstDirStat = (BISO_DIR_STAT_S *)BISO_ZALLOC(sizeof(BISO_DIR_STAT_S));
    if (NULL == pstNew->pstDirStat)
    {
        BISO_FREE(pstNew);
        return NULL;
    }

    /* 挂接到父目录下 */
    if (NULL == pstPre)
    {
        pstParent->pstChild = pstNew;
    }
    else
    {
        pstPre->pstNext = pstNew;
    }
    pstNew->pstParent = pstParent;

    /* 更新父目录统计 */
    pstParent->pstDirStat->uiCurDirNum++;

    /* 更新目录的Rock Ridge扩展信息 */
    (VOID)BISO_RRIP_ReadExtInfo(pstFile, pstParser, pstRecord, pstNew);
    return pstNew;
}

STATIC BISO_SVD_DIR_TREE_S * BISO_9660_CreateSVDDirNode
(
    IN BISO_FILE_S *pstFile,
    IN BISO_PARSER_S *pstParser,
    IN BISO_DIR_RECORD_S  *pstRecord,
    INOUT BISO_SVD_DIR_TREE_S *pstPre,
    INOUT BISO_SVD_DIR_TREE_S *pstParent
)
{
    BISO_SVD_DIR_TREE_S *pstNew = NULL;
    
    DBGASSERT(NULL != pstRecord);
    DBGASSERT(NULL != pstPre);
    DBGASSERT(NULL != pstParent);

    /* 申请内存用于保存目录节点 */
    pstNew = (BISO_SVD_DIR_TREE_S *)BISO_ZALLOC(sizeof(BISO_SVD_DIR_TREE_S));
    if (NULL == pstNew)
    {
        return NULL;
    }
    
    /* 目录节点属性赋值 */
    pstNew->uiExtent = pstRecord->uiExtent;

    /* 挂接到父目录下 */
    if (NULL == pstPre)
    {
        pstParent->pstChild = pstNew;
    }
    else
    {
        pstPre->pstNext = pstNew;
    }
    pstNew->pstParent = pstParent;

    return pstNew;
}

/* ISO原始文件格式处理 */
STATIC ULONG BISO_9660_ProcRawFileNameFmt(INOUT CHAR *szFileName, INOUT USHORT *pusLen)
{
    UINT i;
    UINT uiSepNum = 0;
    UINT uiSepIndex = 0;
    UINT uiDotNum = 0;
    UINT uiLen = *pusLen;

    for (i = 0; i < uiLen; i++)
    {
        if (szFileName[i] == ';')
        {
            if (uiSepNum > 0)
            {
                return BISO_SUCCESS;
            }
            uiSepNum++;
            uiSepIndex = i;
        }
        else if (szFileName[i] == '.')
        {
            if (uiDotNum > 0)
            {
                return BISO_SUCCESS;
            }
            uiDotNum++;
        }
        else if (szFileName[i] >= 'a' && szFileName[i] <= 'z')
        {
            return BISO_SUCCESS;
        }
    }

    /* 必须只包含1个分号和1个点号 */
    if (uiSepNum != 1 || uiSepIndex == 0 || uiDotNum != 1)
    {
        return BISO_SUCCESS;
    }

    /* 分号后面是文件版本号, 纯数字 */
    for (i = uiSepIndex + 1; i < uiLen; i++)
    {
        if (szFileName[i] < '0' || szFileName[i] > '9')
        {
            return BISO_SUCCESS;
        }
    }

    /* 把分号后面剔除 */
    szFileName[uiSepIndex] = 0;
    uiLen = uiSepIndex;

    /* 如果只有文件名没有扩展名则把最后的点号去掉 */
    if (uiLen > 1 && szFileName[uiLen - 1] == '.')
    {
        szFileName[uiLen - 1] = 0;
        uiLen--;
    }

    *pusLen = (USHORT)uiLen;

    /* 转换为小写 */
    for (i = 0; i < uiLen; i++)
    {
        if (szFileName[i] >= 'A' && szFileName[i] <= 'Z')
        {
            szFileName[i] = 'a' + (szFileName[i] - 'A');            
        }
    }
    return BISO_SUCCESS;
}

STATIC BISO_DIR_TREE_S * BISO_9660_CreateFileNode
(
    IN BISO_FILE_S *pstFile,
    IN BISO_PARSER_S *pstParser,
    IN BISO_DIR_RECORD_S  *pstRecord,
    INOUT BISO_DIR_TREE_S *pstPre,
    INOUT BISO_DIR_TREE_S *pstParent
)
{   
    UINT uiSecNum = 0;
    BISO_DIR_TREE_S *pstNew = NULL;
    
    DBGASSERT(NULL != pstRecord);
    DBGASSERT(NULL != pstPre);
    DBGASSERT(NULL != pstParent);

    /* 申请内存用于保存文件节点 */
    pstNew = (BISO_DIR_TREE_S *)BISO_ZALLOC(sizeof(BISO_DIR_TREE_S));
    if (NULL == pstNew)
    {
        return NULL;
    }
    
    /* 目录节点属性赋值 */
    BISO_UTIL_CopyStr(pstRecord->szName, pstRecord->ucNameLen, pstNew->szName);
    pstNew->uiExtent = pstRecord->uiExtent;
    pstNew->usNameLen = (USHORT)strlen(pstNew->szName);
    pstNew->uiSize = pstRecord->uiSize;
    
    /* 读取文件的Rock Ridge扩展信息 */
    (VOID)BISO_RRIP_ReadExtInfo(pstFile, pstParser, pstRecord, pstNew);

    /* 更新文件所在目录的记录 */
    if (BOOL_TRUE == BISO_DIR_TREE_IS_SYMLINK(pstNew))
    {
        pstParent->pstDirStat->uiCurLinkNum++;
    }
    else
    {
        uiSecNum = BISO_USED_SECTOR_NUM(pstNew->uiSize);
        pstParent->pstDirStat->uiCurFileNum++;
        pstParent->pstDirStat->uiCurUsedSec += uiSecNum;
        pstParent->pstDirStat->ui64CurSpace += pstNew->uiSize;
    }          

    /* 节点挂接到当前目录的FileList上 */
    if (NULL == pstPre)
    {
        pstParent->pstFileList = pstNew;
    }
    else
    {
        pstPre->pstNext = pstNew;
    }
    pstNew->pstParent = pstParent;

    if (NULL == pstNew->pstPosixInfo && pstNew->usNameLen > 2)
    {
        BISO_9660_ProcRawFileNameFmt(pstNew->szName, &pstNew->usNameLen);
    }
    
    return pstNew;
}

STATIC BISO_SVD_DIR_TREE_S * BISO_9660_CreateSVDFileNode
(
    IN BISO_FILE_S *pstFile,
    IN BISO_PARSER_S *pstParser,
    IN BISO_DIR_RECORD_S  *pstRecord,
    INOUT BISO_SVD_DIR_TREE_S *pstPre,
    INOUT BISO_SVD_DIR_TREE_S *pstParent
)
{   
    BISO_SVD_DIR_TREE_S *pstNew = NULL;
    
    DBGASSERT(NULL != pstRecord);
    DBGASSERT(NULL != pstPre);
    DBGASSERT(NULL != pstParent);

    /* 申请内存用于保存文件节点 */
    pstNew = (BISO_SVD_DIR_TREE_S *)BISO_ZALLOC(sizeof(BISO_SVD_DIR_TREE_S));
    if (NULL == pstNew)
    {
        return NULL;
    }
    
    /* 目录节点属性赋值 */
    pstNew->uiExtent = pstRecord->uiExtent;
    pstNew->uiSize = pstRecord->uiSize;
    
    /* 节点挂接到当前目录的FileList上 */
    if (NULL == pstPre)
    {
        pstParent->pstFileList = pstNew;
    }
    else
    {
        pstPre->pstNext = pstNew;
    }
    pstNew->pstParent = pstParent;

    return pstNew;
}

STATIC ULONG BISO_9660_BuildFileList
(
    IN  BISO_FILE_S     *pstFile,
    IN  BISO_PARSER_S   *pstParser,
    OUT BISO_DIR_TREE_S *pstDirTree
)
{
    UINT uiTail = 0;
    UINT uiBufSize = 0;
    UINT uiTotSize = 0;
    UCHAR *pucBuf = NULL;
    BISO_DIR_TREE_S *pstPre = NULL;
    BISO_DIR_TREE_S *pstNew = NULL;
    BISO_DIR_TREE_S *pstChild = NULL;
    BISO_DIR_RECORD_S *pstCurrent = NULL;
    
    DBGASSERT(NULL != pstFile);
    DBGASSERT(NULL != pstDirTree);

    /* 读取Directory Record记录 */
    pucBuf = BISO_9660_ReadDirRecord(pstFile, pstDirTree->uiExtent, &uiBufSize);
    if (NULL == pucBuf)
    {
        return BISO_ERR_ALLOC_MEM;
    }

    pstCurrent = (BISO_DIR_RECORD_S *)pucBuf;
    pstChild = pstDirTree->pstChild;
    
    while (uiTotSize < uiBufSize)
    {
        if (BOOL_TRUE != BISO_DIR_RECORD_IS_PATH(pstCurrent))  /* 只处理文件 */
        {
            /* 创建文件节点 */
            pstNew = BISO_9660_CreateFileNode(pstFile, pstParser, pstCurrent, pstPre, pstDirTree);
            if (NULL == pstNew)
            {
                BISO_FREE(pucBuf);
                return BISO_ERR_ALLOC_MEM;
            }
            pstNew->ui64FileRecordOffset = (UINT64)((UINT64)pstDirTree->uiExtent * BISO_SECTOR_SIZE)
                + ((ULONG)pstCurrent - (ULONG)pucBuf);
            pstPre = pstNew;
        }
        else
        {
            /* 对于子目录在这里更新目录的Rock Ridge扩展信息 */
            if ((BOOL_TRUE != BISO_9660_IS_CURRENT(pstCurrent)) &&
                (BOOL_TRUE != BISO_9660_IS_PARENT(pstCurrent))) 
            {
                /*
                 * 这里首先按照Path Table里记录的子目录顺序来判断, 如果是就不用搜索了
                 * 如果不是则再从子目录列表中查询.
                 * 这里实际上取决于: 
                 * Path Table里的子目录记录和Directory Record里面的子目录记录顺序是否一致!!!!
                 * 绝大多数情况下都是按照字母顺序,两者是一致的, 所以BISO_9660_FindChild一般情况下
                 * 是不会调用的
                 */
                if (pstChild->uiExtent == pstCurrent->uiExtent)
                {
                    pstNew = pstChild;
                    pstChild = pstChild->pstNext;
                }
                else
                {
                    pstNew = BISO_9660_FindChild(pstDirTree, pstCurrent->uiExtent);
                }
            
                if (NULL != pstNew)
                {
                    (VOID)BISO_RRIP_ReadExtInfo(pstFile, pstParser, pstCurrent, pstNew);
                }
            }
        }

        uiTotSize += pstCurrent->ucLength;
        pstCurrent = (BISO_DIR_RECORD_S *)(pucBuf + uiTotSize);
        
        /*
         * !!!!!!!!!!!!!!!!!!!!!!!!
         * ISO-9660规定Directory Record记录不能跨逻辑块，所以如果一个逻辑块的最后
         * 一段区域不够保存一个Directory Record的话这段区域就会废弃(填0)
         */
        if (0 == pstCurrent->ucLength)
        {
            uiTail = BISO_BLOCK_SIZE - (uiTotSize % BISO_BLOCK_SIZE);
            uiTotSize += uiTail;
            pstCurrent = (BISO_DIR_RECORD_S *)((UCHAR *)pstCurrent + uiTail);
        }
    }

    BISO_FREE(pucBuf);
    return BISO_SUCCESS;
}

/* 通过PathTable构建目录树(只包含目录) 
这里利用Path Table，因此超过65535个文件夹的ISO文件只能读取前                65535个目录里的内容 */
STATIC ULONG BISO_9660_BuildPathTree
(
    IN    BISO_FILE_S    *pstFile, 
    INOUT BISO_PARSER_S  *pstParser,
    OUT   UINT           *puiTotDirNum
)
{
    UINT uiTotDirNum = 0;
    UINT uiPathTblId = 1;
    BISO_QUEUE_S *pstQueue = NULL;
    BISO_DIR_TREE_S *pstNew = NULL;
    BISO_DIR_TREE_S *pstPre = NULL;
    BISO_DIR_TREE_S *pstDirTree = NULL;
    BISO_PATH_TABLE_S *pstPathTable = NULL;

    DBGASSERT(NULL != pstFile);
    DBGASSERT(NULL != pstParser);
    DBGASSERT(NULL != puiTotDirNum);

    /*
     * ISO-9660规定的Path Table的顺序实际上是文件树的一个广度优先遍历的形式
     * 而处理广度优先遍历时一般需要一个先进先出的队列结构，这里创建一个使用
     */
    pstQueue = BISO_QUEUE_Create();
    if (NULL == pstQueue)
    {
        return BISO_ERR_ALLOC_MEM;
    }

    /* ROOT根目录 */
    pstPathTable = (BISO_PATH_TABLE_S *)(pstParser->pucPathTable);
    pstDirTree = &(pstParser->stDirTree);
    pstDirTree->uiPathTblId = 1;  
    pstDirTree->uiExtent = pstPathTable->uiExtent;  

    /* 申请统计信息的节点 */
    pstDirTree->pstDirStat = (BISO_DIR_STAT_S *)BISO_ZALLOC(sizeof(BISO_DIR_STAT_S));
    if (NULL == pstDirTree->pstDirStat)
    {
        return BISO_ERR_ALLOC_MEM;
    }

    /* 先把ROOT入队列 */
    BISO_QUEUE_Push(pstQueue, pstDirTree);
    pstPathTable = (BISO_PATH_TABLE_S *)((UCHAR *)pstPathTable + BISO_9660_PATH_LEN(pstPathTable));

    /* 依次处理队列中的每一项直到队列读空 */
    while (NULL != (pstDirTree = (BISO_DIR_TREE_S *)BISO_QUEUE_PopHead(pstQueue)))
    {
        /* 把该目录下的所有一级子目录读出来入队列 */
        while ((USHORT)pstDirTree->uiPathTblId == pstPathTable->usParentDirNum)
        {
            /* 申请内存用于保存目录节点 */
            pstNew = (BISO_DIR_TREE_S *)BISO_ZALLOC(sizeof(BISO_DIR_TREE_S));
            if (NULL == pstNew)
            {
                BISO_QUEUE_Destroy(pstQueue);
                return BISO_ERR_ALLOC_MEM;
            }
            
            /* 目录节点属性赋值 */
            BISO_UTIL_CopyStr(pstPathTable->szDirName, pstPathTable->ucDirNameLen, pstNew->szName);
            pstNew->uiExtent = pstPathTable->uiExtent;
            pstNew->usNameLen = (USHORT)strlen(pstNew->szName);
            pstNew->uiPathTblId = (++uiPathTblId);

            /* 申请统计信息的节点 */
            pstNew->pstDirStat = (BISO_DIR_STAT_S *)BISO_ZALLOC(sizeof(BISO_DIR_STAT_S));
            if (NULL == pstNew->pstDirStat)
            {
                BISO_QUEUE_Destroy(pstQueue);
                return BISO_ERR_ALLOC_MEM;
            }

            /* 挂接到父目录下 */
            if (NULL == pstDirTree->pstChild)
            {
                pstDirTree->pstChild = pstNew;
            }
            else
            {
                pstPre->pstNext = pstNew;
            }
            pstNew->pstParent = pstDirTree;

            pstPre = pstNew;
            pstDirTree->pstDirStat->uiCurDirNum++;
            uiTotDirNum++;
            BISO_QUEUE_Push(pstQueue, pstNew);
            pstPathTable = (BISO_PATH_TABLE_S *)((UCHAR *)pstPathTable + BISO_9660_PATH_LEN(pstPathTable));
        }
    }

    *puiTotDirNum = uiTotDirNum;
    BISO_QUEUE_Destroy(pstQueue);
    return BISO_SUCCESS;
}

/* 更新整个目录树的目录结构中文件总数、目录总数、总空间大小等信息 */
ULONG BISO_9660_UpdateTreeStat(INOUT BISO_DIR_TREE_S *pstRoot)
{
    VOID *pData = NULL;
    BISO_QUEUE_S *pstQueue = NULL;
    BISO_DIR_TREE_S *pstCurDir = NULL;
    BISO_DIR_STAT_S *pstDirStat = NULL;
    BISO_DIR_STAT_S *pstPreDirStat = NULL;
    
    DBGASSERT(NULL != pstRoot);
    
    pstQueue = BISO_QUEUE_Create();
    if (NULL == pstQueue)
    {
        return BISO_ERR_ALLOC_MEM;
    }

    /* 构建DFS栈 */
    BISO_9660_FillDfsStack(pstRoot, pstQueue);

    /* 依次弹栈处理 */
    while (NULL != (pData = BISO_QUEUE_PopTail(pstQueue)))
    {
        pstCurDir = (BISO_DIR_TREE_S *)pData;
        pstDirStat = pstCurDir->pstDirStat;

        /* 更新自己和父节点 */
        pstDirStat->uiTotDirNum  += pstDirStat->uiCurDirNum;
        pstDirStat->uiTotFileNum += pstDirStat->uiCurFileNum;
        pstDirStat->uiTotLinkNum += pstDirStat->uiCurLinkNum;
        pstDirStat->ui64TotSpace += pstDirStat->ui64CurSpace;
        pstDirStat->uiTotUsedSec += pstDirStat->uiCurUsedSec;

        if (NULL != pstCurDir->pstParent)  /* ROOT节点没有父节点 */
        {
            pstPreDirStat = pstCurDir->pstParent->pstDirStat;
            pstPreDirStat->uiTotDirNum  += pstDirStat->uiTotDirNum;
            pstPreDirStat->uiTotFileNum += pstDirStat->uiTotFileNum;
            pstPreDirStat->uiTotLinkNum += pstDirStat->uiCurLinkNum;
            pstPreDirStat->ui64TotSpace += pstDirStat->ui64TotSpace;
            pstPreDirStat->uiTotUsedSec += pstDirStat->uiTotUsedSec;
        }
    }

    /* 销毁堆栈 */
    BISO_QUEUE_Destroy(pstQueue);
    return BISO_SUCCESS;
}


ULONG BISO_9660_UpdateNodeStat
(
    IN BOOL_T bAdd,
    IN CONST BISO_DIR_TREE_S *pstCurNode,
    INOUT BISO_DIR_TREE_S *pstParent
)
{
    UINT uiSecNum = 0;
    BISO_DIR_TREE_S *pstCurDir = NULL;
    BISO_DIR_STAT_S *pstCurStat = NULL;
    BISO_DIR_STAT_S *pstPreStat = NULL;
    BISO_DIR_STAT_S  stExDirStat;
    
    DBGASSERT(NULL != pstCurNode);

    memset(&stExDirStat, 0, sizeof(stExDirStat));
    pstPreStat = pstParent->pstDirStat;

    if (NULL == pstCurNode->pstDirStat)  /* 非目录 */
    {
        if (BOOL_TRUE == BISO_DIR_TREE_IS_SYMLINK(pstCurNode))  /* 符号链接 */
        {
            /* 更新当前目录链接数统计, 大小为0不用更新 */
            BISO_STAT_UPDATE(bAdd, pstPreStat->uiCurLinkNum, 1);  
            stExDirStat.uiTotLinkNum = 1;
        }
        else
        {
            /* 更新当前目录的文件数,大小等 */
            uiSecNum = BISO_USED_SECTOR_NUM(pstCurNode->uiSize);
            BISO_STAT_UPDATE(bAdd, pstPreStat->uiCurFileNum, 1);
            BISO_STAT_UPDATE(bAdd, pstPreStat->uiCurUsedSec, uiSecNum);
            BISO_STAT_UPDATE(bAdd, pstPreStat->ui64CurSpace, pstCurNode->uiSize);
            stExDirStat.uiTotFileNum = 1;
            stExDirStat.ui64TotSpace = pstCurNode->uiSize;
            stExDirStat.uiTotUsedSec = uiSecNum;
        }
    }
    else
    {
        pstCurStat = pstCurNode->pstDirStat;
        BISO_STAT_UPDATE(bAdd, pstPreStat->uiCurDirNum,  1);  /* Current只需更新目录数 */
        BISO_STAT_UPDATE(bAdd, pstPreStat->uiTotDirNum,  pstCurStat->uiTotDirNum);
        BISO_STAT_UPDATE(bAdd, pstPreStat->uiTotFileNum, pstCurStat->uiTotFileNum);
        BISO_STAT_UPDATE(bAdd, pstPreStat->uiTotLinkNum, pstCurStat->uiTotLinkNum);
        BISO_STAT_UPDATE(bAdd, pstPreStat->ui64TotSpace, pstCurStat->ui64TotSpace);
        BISO_STAT_UPDATE(bAdd, pstPreStat->uiTotUsedSec, pstCurStat->uiTotUsedSec);
        memcpy(&stExDirStat, pstCurStat, sizeof(stExDirStat)); /* 只会用到Total部分,不用Current部分 */
    }

    /* 依次更新上层目录的Total部分 */
    pstCurDir = pstParent->pstParent;
    while (NULL != pstCurDir)
    {
        pstCurStat = pstCurDir->pstDirStat;
        BISO_STAT_UPDATE(bAdd, pstCurStat->uiTotDirNum,  stExDirStat.uiTotDirNum);
        BISO_STAT_UPDATE(bAdd, pstCurStat->uiTotFileNum, stExDirStat.uiTotFileNum);
        BISO_STAT_UPDATE(bAdd, pstCurStat->uiTotLinkNum, stExDirStat.uiTotLinkNum);
        BISO_STAT_UPDATE(bAdd, pstCurStat->ui64TotSpace, stExDirStat.ui64TotSpace);
        BISO_STAT_UPDATE(bAdd, pstCurStat->uiTotUsedSec, stExDirStat.uiTotUsedSec);
        pstCurDir = pstCurDir->pstParent;
    }

    return BISO_SUCCESS;
}

ULONG BISO_9660_BuildFileTreeByTable
(
    IN  BISO_FILE_S          *pstFile, 
    OUT BISO_PARSER_S *pstParser
)
{   
    ULONG ulRet;
    UINT  uiTotDirNum = 0;
    BISO_QUEUE_S *pstQueue = NULL;
    BISO_DIR_TREE_S *pstDirTree = NULL;

    DBGASSERT(NULL != pstFile);
    DBGASSERT(NULL != pstParser);

    /* 先通过Path Table构建目录树(不包含文件) */
    ulRet = BISO_9660_BuildPathTree(pstFile, pstParser, &uiTotDirNum);
    if (BISO_SUCCESS != ulRet)
    {
        return ulRet;
    }

    /* 构建每一个目录下的文件列表 */

    /* 创建队列用于递归遍历 */
    pstQueue = BISO_QUEUE_Create();
    if (NULL == pstQueue)
    {
        return BISO_ERR_ALLOC_MEM;
    }

    /* ROOT目录入队列 */
    BISO_QUEUE_Push(pstQueue, &(pstParser->stDirTree));

    /* 循环依次构建 */
    while (NULL != (pstDirTree = (BISO_DIR_TREE_S *)BISO_QUEUE_PopHead(pstQueue)))
    {
        /* 构建文件列表 */
        ulRet = BISO_9660_BuildFileList(pstFile, pstParser, pstDirTree);
        if (BISO_SUCCESS != ulRet)
        {
            BISO_DIAG("Failed to build file list for dir %s.", pstDirTree->szName);
            BISO_QUEUE_Destroy(pstQueue);
            return ulRet;
        }

        /* 子目录入队列 */
        if (NULL != pstDirTree->pstChild)
        {
            BISO_QUEUE_Push(pstQueue, pstDirTree->pstChild);
        }

        /* 下一个相邻目录入队列 */
        if (NULL != pstDirTree->pstNext)
        {
            BISO_QUEUE_Push(pstQueue, pstDirTree->pstNext);
        }
    }
    
    BISO_QUEUE_Destroy(pstQueue);
    return BISO_SUCCESS;
}

ULONG BISO_9660_BuildFileTreeRecursively
(
    IN  BISO_FILE_S          *pstFile, 
    OUT BISO_PARSER_S *pstParser
)
{
    UINT uiTail = 0;
    UINT uiTotSize = 0;
    UINT uiBufSize = 0;
    UCHAR *pucBuf = NULL;
    BISO_QUEUE_S *pstQueue = NULL;
    BISO_DIR_TREE_S *pstNew = NULL;
    BISO_DIR_TREE_S *pstPreDir = NULL;
    BISO_DIR_TREE_S *pstPreFile = NULL;
    BISO_DIR_TREE_S *pstDirTree = NULL;
    BISO_DIR_RECORD_S *pstCurrent = NULL;

    DBGASSERT(NULL != pstFile);
    DBGASSERT(NULL != pstParser);

    /* 先对ROOT进行处理 */
    pstDirTree = &(pstParser->stDirTree);
    pstDirTree->uiPathTblId = 1;  
    pstDirTree->uiExtent = pstParser->pstPVD->stRootDirRecord.uiExtent;  

    /* 申请统计信息的内存 */
    pstDirTree->pstDirStat = (BISO_DIR_STAT_S *)BISO_ZALLOC(sizeof(BISO_DIR_STAT_S));
    if (NULL == pstDirTree->pstDirStat)
    {
        return BISO_ERR_ALLOC_MEM;
    }

    /* 创建堆栈,同时ROOT入栈 */
    pstQueue = BISO_QUEUE_Create();
    BISO_QUEUE_Push(pstQueue, pstDirTree);

    while (NULL != (pstDirTree = (BISO_DIR_TREE_S *)BISO_QUEUE_PopHead(pstQueue)))
    {
        uiTotSize  = 0;
        pstPreDir  = NULL;
        pstPreFile = NULL;
        pstDirTree->uiPathTblId = BISO_UINT_MAX;
        
        /* 读取Directory Record记录 */
        pucBuf = BISO_9660_ReadDirRecord(pstFile, pstDirTree->uiExtent, &uiBufSize);
        if (NULL == pucBuf)
        {
            BISO_QUEUE_Destroy(pstQueue);
            return BISO_ERR_ALLOC_MEM;
        }

        pstCurrent = (BISO_DIR_RECORD_S *)pucBuf;
        
        while (uiTotSize < uiBufSize)
        {
            if (BOOL_TRUE == BISO_DIR_RECORD_IS_PATH(pstCurrent))
            {
                if ((BOOL_TRUE != BISO_9660_IS_CURRENT(pstCurrent)) &&
                    (BOOL_TRUE != BISO_9660_IS_PARENT(pstCurrent))) 
                {
                    /* 创建新目录节点 */
                    pstNew = BISO_9660_CreateDirNode(pstFile, pstParser, pstCurrent, pstPreDir, pstDirTree);
                    if (NULL == pstNew)
                    {
                        BISO_FREE(pucBuf);
                        BISO_QUEUE_Destroy(pstQueue);
                        return BISO_ERR_ALLOC_MEM;
                    }
                    pstPreDir = pstNew;

                    /* 新目录入栈 */
                    BISO_QUEUE_Push(pstQueue, pstNew);
                }
            }
            else
            {
                pstNew = BISO_9660_CreateFileNode(pstFile, pstParser, pstCurrent, pstPreFile, pstDirTree);
                if (NULL == pstNew)
                {
                    BISO_FREE(pucBuf);
                    BISO_QUEUE_Destroy(pstQueue);
                    return BISO_ERR_ALLOC_MEM;
                }
                pstNew->ui64FileRecordOffset = (UINT64)((UINT64)pstDirTree->uiExtent * BISO_SECTOR_SIZE)
                    + ((ULONG)pstCurrent - (ULONG)pucBuf);
                pstPreFile = pstNew;
            }

            uiTotSize += pstCurrent->ucLength;
            pstCurrent = (BISO_DIR_RECORD_S *)(pucBuf + uiTotSize);
            
            /*
             * !!!!!!!!!!!!!!!!!!!!!!!!
             * ISO-9660规定Directory Record记录不能跨逻辑块，所以如果一个逻辑块的最后
             * 一段区域不够保存一个Directory Record的话这段区域就会废弃(填0)
             */
            if (0 == pstCurrent->ucLength)
            {
                uiTail = BISO_BLOCK_SIZE - (uiTotSize % BISO_BLOCK_SIZE);
                uiTotSize += uiTail;
                pstCurrent = (BISO_DIR_RECORD_S *)((UCHAR *)pstCurrent + uiTail);
            }
        }

        BISO_FREE(pucBuf);
    }

    BISO_QUEUE_Destroy(pstQueue);
    return BISO_SUCCESS;
}

ULONG BISO_9660_BuildSVDFileTreeRecursively
(
    IN  BISO_FILE_S          *pstFile, 
    OUT BISO_PARSER_S *pstParser
)
{
    UINT uiTail = 0;
    UINT uiTotSize = 0;
    UINT uiBufSize = 0;
    UCHAR *pucBuf = NULL;
    BISO_QUEUE_S *pstQueue = NULL;
    BISO_SVD_DIR_TREE_S *pstNew = NULL;
    BISO_SVD_DIR_TREE_S *pstPreDir = NULL;
    BISO_SVD_DIR_TREE_S *pstPreFile = NULL;
    BISO_SVD_DIR_TREE_S *pstDirTree = NULL;
    BISO_DIR_RECORD_S *pstCurrent = NULL;

    DBGASSERT(NULL != pstFile);
    DBGASSERT(NULL != pstParser);

    /* 先对ROOT进行处理 */
    pstDirTree = &(pstParser->stSVDDirTree);
    pstDirTree->uiExtent = pstParser->pstSVD->stRootDirRecord.uiExtent;  

    /* 创建堆栈,同时ROOT入栈 */
    pstQueue = BISO_QUEUE_Create();
    BISO_QUEUE_Push(pstQueue, pstDirTree);

    while (NULL != (pstDirTree = (BISO_SVD_DIR_TREE_S *)BISO_QUEUE_PopHead(pstQueue)))
    {
        uiTotSize  = 0;
        pstPreDir  = NULL;
        pstPreFile = NULL;
        
        /* 读取Directory Record记录 */
        pucBuf = BISO_9660_ReadDirRecord(pstFile, pstDirTree->uiExtent, &uiBufSize);
        if (NULL == pucBuf)
        {
            BISO_QUEUE_Destroy(pstQueue);
            return BISO_ERR_ALLOC_MEM;
        }

        pstCurrent = (BISO_DIR_RECORD_S *)pucBuf;
        
        while (uiTotSize < uiBufSize)
        {
            if (BOOL_TRUE == BISO_DIR_RECORD_IS_PATH(pstCurrent))
            {
                if ((BOOL_TRUE != BISO_9660_IS_CURRENT(pstCurrent)) &&
                    (BOOL_TRUE != BISO_9660_IS_PARENT(pstCurrent))) 
                {
                    /* 创建新目录节点 */
                    pstNew = BISO_9660_CreateSVDDirNode(pstFile, pstParser, pstCurrent, pstPreDir, pstDirTree);
                    if (NULL == pstNew)
                    {
                        BISO_FREE(pucBuf);
                        BISO_QUEUE_Destroy(pstQueue);
                        return BISO_ERR_ALLOC_MEM;
                    }
                    pstPreDir = pstNew;

                    /* 新目录入栈 */
                    BISO_QUEUE_Push(pstQueue, pstNew);
                }
            }
            else
            {
                pstNew = BISO_9660_CreateSVDFileNode(pstFile, pstParser, pstCurrent, pstPreFile, pstDirTree);
                if (NULL == pstNew)
                {
                    BISO_FREE(pucBuf);
                    BISO_QUEUE_Destroy(pstQueue);
                    return BISO_ERR_ALLOC_MEM;
                }
                pstNew->ui64FileRecordOffset = (UINT64)((UINT64)pstDirTree->uiExtent * BISO_SECTOR_SIZE)
                    + ((ULONG)pstCurrent - (ULONG)pucBuf);
                pstPreFile = pstNew;
            }

            uiTotSize += pstCurrent->ucLength;
            pstCurrent = (BISO_DIR_RECORD_S *)(pucBuf + uiTotSize);
            
            /*
             * !!!!!!!!!!!!!!!!!!!!!!!!
             * ISO-9660规定Directory Record记录不能跨逻辑块，所以如果一个逻辑块的最后
             * 一段区域不够保存一个Directory Record的话这段区域就会废弃(填0)
             */
            if (0 == pstCurrent->ucLength)
            {
                uiTail = BISO_BLOCK_SIZE - (uiTotSize % BISO_BLOCK_SIZE);
                uiTotSize += uiTail;
                pstCurrent = (BISO_DIR_RECORD_S *)((UCHAR *)pstCurrent + uiTail);
            }
        }

        BISO_FREE(pucBuf);
    }

    BISO_QUEUE_Destroy(pstQueue);
    return BISO_SUCCESS;
}

VOID BISO_9660_FreeDirTree(IN BISO_PARSER_S *pstParser)
{
    BISO_QUEUE_S *pstQueue = NULL;
    BISO_DIR_TREE_S *pstRoot = NULL;
    BISO_DIR_TREE_S *pstCurDir = NULL;
    BISO_DIR_TREE_S *pstPre = NULL;
    BISO_DIR_TREE_S *pstNext = NULL;

    if (NULL == pstParser)
    {
        return;
    }

    /* 创建队列 */
    pstQueue = BISO_QUEUE_Create();
    if (NULL == pstQueue)
    {
        return;
    }

    pstRoot = &pstParser->stDirTree;

    /* 构建文件树入栈 */
    BISO_9660_FillDfsStack(pstRoot, pstQueue);

    /* 不需要释放ROOT,把它从队列头弹出来 */
    BISO_QUEUE_PopHead(pstQueue);

    /* 依次释放各个节点 */
    while (NULL != (pstCurDir = (BISO_DIR_TREE_S *)BISO_QUEUE_PopTail(pstQueue)))
    {
        /* 释放当前目录的文件列表 */
        for (pstPre = pstCurDir->pstFileList; NULL != pstPre; pstPre = pstNext)
        {
            pstNext = pstPre->pstNext;
            BISO_9600_FREE_DIRTREE(pstPre);
        }

        /* 释放自己 */
        BISO_9600_FREE_DIRTREE(pstCurDir);
    }

    /* 释放ROOT目录的文件列表 */
    for (pstPre = pstRoot->pstFileList; NULL != pstPre; pstPre = pstNext)
    {
        pstNext = pstPre->pstNext;
        BISO_9600_FREE_DIRTREE(pstPre);
    }

    /* 释放ROOT自己的扩展信息,但不释放自己本身 */
    BISO_9600_FREE_STAT(pstRoot);
    BISO_9600_FREE_POSIX(pstRoot);

    /* 销毁队列 */
    BISO_QUEUE_Destroy(pstQueue);
}

VOID BISO_9660_FreeSVDDirTree(IN BISO_PARSER_S *pstParser)
{
    BISO_QUEUE_S *pstQueue = NULL;
    BISO_SVD_DIR_TREE_S *pstRoot = NULL;
    BISO_SVD_DIR_TREE_S *pstCurDir = NULL;
    BISO_SVD_DIR_TREE_S *pstPre = NULL;
    BISO_SVD_DIR_TREE_S *pstNext = NULL;

    if (NULL == pstParser || 
        0 == pstParser->stSVDDirTree.uiExtent ||
        0 == pstParser->stSVDDirTree.uiSize)
    {
        return;
    }

    /* 创建队列 */
    pstQueue = BISO_QUEUE_Create();
    if (NULL == pstQueue)
    {
        return;
    }

    pstRoot = &pstParser->stSVDDirTree;

    /* 构建文件树入栈 */
    BISO_9660_FillDfsStack((BISO_DIR_TREE_S *)pstRoot, pstQueue);

    /* 不需要释放ROOT,把它从队列头弹出来 */
    BISO_QUEUE_PopHead(pstQueue);

    /* 依次释放各个节点 */
    while (NULL != (pstCurDir = (BISO_SVD_DIR_TREE_S *)BISO_QUEUE_PopTail(pstQueue)))
    {
        /* 释放当前目录的文件列表 */
        for (pstPre = pstCurDir->pstFileList; NULL != pstPre; pstPre = pstNext)
        {
            pstNext = pstPre->pstNext;
            BISO_FREE(pstPre);
        }

        /* 释放自己 */
        BISO_FREE(pstCurDir);
    }

    /* 释放ROOT目录的文件列表 */
    for (pstPre = pstRoot->pstFileList; NULL != pstPre; pstPre = pstNext)
    {
        pstNext = pstPre->pstNext;
        BISO_FREE(pstPre);
    }

    /* 销毁队列 */
    BISO_QUEUE_Destroy(pstQueue);
}

BISO_PARSER_S * BISO_9660_CreateParser(VOID)
{
    BISO_PARSER_S *pstParser = NULL;
    
    pstParser = (BISO_PARSER_S *)BISO_ZALLOC(sizeof(BISO_PARSER_S));
    if (NULL == pstParser)
    {
        return NULL;
    }

    /* 初始化链表 */
    BISO_DLL_Init(&(pstParser->stVDList));

    return pstParser;
}

VOID BISO_9660_CleanParser(INOUT BISO_PARSER_S *pstParser)
{
    if (NULL == pstParser)
    {
        return;
    }

    /* 释放Volume Descriptor链表 */
    BISO_DLL_Free(&(pstParser->stVDList));

    /* 释放Path Table */
    if (NULL != pstParser->pucPathTable)
    {
        BISO_FREE(pstParser->pucPathTable);
    }

    /* 释放 El Torito数据 */
    if (NULL != pstParser->pucElToritoEntry)
    {
        BISO_FREE(pstParser->pucElToritoEntry);
    }

    /* 释放文件树 */
    BISO_9660_FreeDirTree(pstParser);
    BISO_9660_FreeSVDDirTree(pstParser);
}

VOID BISO_9660_DestroyParser(INOUT BISO_PARSER_S *pstParser)
{
    if (NULL == pstParser)
    {
        return;
    }

    /* 清理解析器 */
    BISO_9660_CleanParser(pstParser);
    
    /* 释放解析器自己 */
    BISO_FREE(pstParser);
}

ULONG BISO_9660_OpenImage
(
    IN BOOL_T bParseSVDDirTree,
    IN CONST CHAR *pcFileName, 
    OUT BISO_PARSER_S *pstParser
)
{
    UINT64 ui64FileSize = 0;
    ULONG ulRet = BISO_SUCCESS;
    BISO_FILE_S *pstFile = NULL;
    
    if ((NULL == pcFileName) || (NULL == pstParser))
    {
        return BISO_ERR_NULL_PTR;
    }

    /* 先看文件大小，过小的文件不可能是ISO文件 */
    ui64FileSize = BISO_PLAT_GetFileSize(pcFileName);
    if (ui64FileSize < BISO_SYSTEM_AREA_SIZE + sizeof(BISO_PVD_S))
    {
        BISO_DIAG("File len %llu is too small.", (ULONGLONG)ui64FileSize);
        return BISO_ERR_INVALID_ISO9660;
    }

    /* 如果已有数据则先清空 */
    if (NULL != pstParser->pstPVD)
    {
        BISO_9660_CleanParser(pstParser);
    }

    /* 打开ISO文件 */
    pstFile = BISO_PLAT_OpenExistFile(pcFileName);
    if (NULL == pstFile)
    {
        BISO_DIAG("Failed to open file %s.", pcFileName);
        return BISO_ERR_OPEN_FILE;
    }

    scnprintf(pstParser->szFileName, sizeof(pstParser->szFileName), "%s", pcFileName);

    /* 读取Volume Description信息 */
    ulRet = BISO_9660_ReadVD(pstFile, pstParser);
    BISO_9660_CHECK_RET(ulRet, pstFile);

    /* 读取Path Table */
    ulRet = BISO_9660_ReadPathTable(pstFile, pstParser);
    BISO_9660_CHECK_RET(ulRet, pstFile);

    /* 读取BOOT启动信息 */
    ulRet = BISO_ELTORITO_ReadBootInfo(pstFile, pstParser);
    BISO_9660_CHECK_RET(ulRet, pstFile);

    /* 读取Rock Ridge扩展标识 */
    ulRet = BISO_RRIP_ReadIndicator(pstParser);
    BISO_9660_CHECK_RET(ulRet, pstFile);

    /* 这里构建文件树有两种方法可供选择, 暂时选择从ROOT递归创建的方法 */

    #if 0
    /* 从ISO文件中读取整个文件树, 这里选择根据Path Table构建 */
    ulRet += BISO_9660_BuildFileTreeByTable(pstFile, pstParser);
    BISO_9660_CHECK_RET(ulRet, pstFile);
    #else
    /* 从ISO文件中读取整个文件树, 这里选择从ROOT开始递归构建 */
    ulRet = BISO_9660_BuildFileTreeRecursively(pstFile, pstParser);
    BISO_9660_CHECK_RET(ulRet, pstFile);
    #endif /* #if 0 */

    if (bParseSVDDirTree && pstParser->pstSVD)
    {
        ulRet = BISO_9660_BuildSVDFileTreeRecursively(pstFile, pstParser);
        BISO_9660_CHECK_RET(ulRet, pstFile);
    }

    /* 更新目录结构里的统计信息 */
    ulRet = BISO_9660_UpdateTreeStat(&(pstParser->stDirTree));
    BISO_9660_CHECK_RET(ulRet, pstFile);

    BISO_PLAT_CloseFile(pstFile);
    return BISO_SUCCESS;
}


ULONG BISO_9660_ParseDate84261
(
    IN CONST CHAR *pcDate,
    OUT BISO_DATE_S *pstDate
)
{
    INT aiBuf[7] = { 0 };

    if ((NULL == pcDate) || (NULL == pstDate))
    {
        return BISO_ERR_NULL_PTR;
    }

    /* 
     * ECMA-119 8.4.26.1节定义的日期格式，共17个字节 
     * 前16个字节是字符，第17个字节是有符号整数
     * 如果前16个字节是字符'0', 最后一个是'\0'则表示时间无效
     * 形如 "2014122013000500*"
     */

    if ((0 == pcDate[0]) || (' ' == pcDate[0]) || ('0' == pcDate[0]))
    {
        return BISO_ERR_NOT_RECORD;
    }
    
    sscanf(pcDate, "%4d%2d%2d%2d%2d%2d%2d", 
           aiBuf + 0, aiBuf + 1, aiBuf + 2, 
           aiBuf + 3, aiBuf + 4, aiBuf + 5, aiBuf + 6);
    pstDate->usYear    = (USHORT)aiBuf[0];
    pstDate->ucMonth   = (UCHAR)aiBuf[1];
    pstDate->ucDay     = (UCHAR)aiBuf[2];
    pstDate->ucHour    = (UCHAR)aiBuf[3];
    pstDate->ucMin     = (UCHAR)aiBuf[4];
    pstDate->ucSecond  = (UCHAR)aiBuf[5];
    pstDate->usMillSec = (UCHAR)(aiBuf[6] * 10); /* 表示百分之一秒 */

    /* 第17字节表示时区信息, 15分钟为1个单位，4个单位就是1个时区 */
    pstDate->cZone     = pcDate[16] / 4; 
    
    return BISO_SUCCESS;
}

VOID BISO_9660_FmtDate84261(IN time_t ulTime, IN UINT uiBufSize, OUT CHAR *pcDate)
{
    INT iTimeZone = BISO_UTIL_GetTimeZone();
    struct tm *pstTm = NULL;
    
    if (NULL != pcDate)
    {
        pstTm = localtime(&ulTime);
        scnprintf(pcDate, uiBufSize, "%04d%02d%02d%02d%02d%02d%02d",
                  pstTm->tm_year + 1900,
                  pstTm->tm_mon + 1,
                  pstTm->tm_mday,
                  pstTm->tm_hour,
                  pstTm->tm_min,
                  pstTm->tm_sec,
                  0);

        /* 第17个字节记录当前时区 */
        pcDate[16] = (CHAR)(4 * iTimeZone);
    }
}

