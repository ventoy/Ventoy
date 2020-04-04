/******************************************************************************
 * biso_rockridge.c
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
#include "biso_dump.h"
#include "biso_rockridge.h"

/* Rock Ridge扩展处理函数数组, NULL表示暂时不处理这类表项 */
STATIC BISO_RRIP_PARSE_ENTRY_CB_S g_astBISO_RRIP_ParseFunc[] = 
{
    { "CE", NULL },
    { "PD", NULL },
    { "SP", NULL },
    { "ST", NULL },
    { "ER", NULL },
    { "ES", NULL },
    { "RR", NULL },
    { "PX", BISO_RRIP_GetPXInfo },
    { "PN", BISO_RRIP_GetPNInfo },
    { "SL", BISO_RRIP_GetSLInfo },
    { "NM", BISO_RRIP_GetNMInfo },
    { "CL", NULL },
    { "PL", NULL },
    { "RE", NULL },
    { "TF", BISO_RRIP_GetTFInfo },
    { "SF", NULL },
};

STATIC VOID BISO_RRIP_AddLinkBuf
(
    IN CHAR *pcBuf,
    IN UINT  uiBufLen,
    INOUT BISO_POSIX_INFO_S *pstPosixInfo
)
{
    CHAR *pcNewBuf = NULL;
    
    DBGASSERT(NULL != pstPosixInfo);
    DBGASSERT(NULL != pcBuf);

    /*
     * 这里采用的是最简单的每次重新申请大内存保存老数据和新数据的方式
     * 实际上效率是比较低的，但是由于普通ISO文件中link类型本身就不多
     * 而有连续多条SL表项的就更少了，所以这里只是简单实现功能，没有特别考虑效率
     */

    /* 申请一个新Buf用于保存原有的数据和这次的新数据 */
    pcNewBuf = (CHAR *)BISO_ZALLOC(uiBufLen + pstPosixInfo->uiLinkLen);
    if (NULL == pcNewBuf)
    {
        return;
    }

    if (NULL == pstPosixInfo->pcLinkSrc)
    {
        memcpy(pcNewBuf, pcBuf, uiBufLen);
    }
    else
    {
        /* 分别保存新老数据，同时把老的Buf释放掉 */
        memcpy(pcNewBuf, pstPosixInfo->pcLinkSrc, pstPosixInfo->uiLinkLen);
        memcpy(pcNewBuf + pstPosixInfo->uiLinkLen, pcBuf, uiBufLen);
        BISO_FREE(pstPosixInfo->pcLinkSrc);
    }

    /* 更新数据Buf */
    pstPosixInfo->pcLinkSrc = pcNewBuf;
    pstPosixInfo->uiLinkLen += uiBufLen;
}

STATIC UINT BISO_RRIP_CalcLinkLen
(
    IN CONST CHAR *pcComponet, 
    IN UINT uiComponetLen
)
{
    UINT uiBufLen = 0;
    UINT uiOffset = 0;
    UINT uiTotLen = 0;
    BISO_RRIP_SL_COMPONENT_S *pstComp = NULL;

    DBGASSERT(NULL != pcComponet);
    DBGASSERT(uiComponetLen > 0);
    
    /* 拼接出链接的源路径 */
    while (uiOffset < uiComponetLen)
    {
        uiBufLen = 0;
        pstComp = (BISO_RRIP_SL_COMPONENT_S *)(pcComponet + uiOffset);

        if (BOOL_TRUE == BISO_SLCOMP_IS_ROOT(pstComp->ucFlags))
        {
            /* ROOT不需处理，后面会添加/ */
        }
        else if (BOOL_TRUE == BISO_SLCOMP_IS_CURRENT(pstComp->ucFlags))
        {
            uiBufLen = 1; /* . */
        }
        else if (BOOL_TRUE == BISO_SLCOMP_IS_PARENT(pstComp->ucFlags))
        {
            uiBufLen = 2; /* .. */
        }
        else
        {
            uiBufLen = pstComp->ucLength;
        }

        /* ucLength不包括头两个字节 */
        uiOffset += pstComp->ucLength + 2;

        /*
         * 如果是link路径的一部分完结,则需要在后面加上'/',否则不加.
         * 不加的情况有两种: 1是整体已经完了  2是路径的一部分还没有完整
         * 比如说链接的位置是 /root/xxxx/a.txt 而xxxx可能非常长(200+个字符)
         * 那么此时光xxxx的部分可能就需要两个Componet部分才能表达完,而这两个
         * 之间是不能加反斜杠的.
         */
        if ((uiOffset < uiComponetLen) && (BOOL_TRUE != BISO_SLCOMP_IS_CONTINUE(pstComp->ucFlags)))
        {
            uiBufLen++;
        }

        uiTotLen += uiBufLen;
    }

    return uiTotLen;
}

STATIC UINT BISO_RRIP_GetPartLink
(
    IN  CONST BISO_RRIP_SL_COMPONENT_S *pstComponent, 
    IN  UINT  uiBufSize,
    OUT CHAR *pcBuf
)
{
    UINT uiBufLen = 0;
    
    DBGASSERT(NULL != pstComponent);
    DBGASSERT(NULL != pcBuf);

    if (BOOL_TRUE == BISO_SLCOMP_IS_ROOT(pstComponent->ucFlags))
    {
        /* ROOT不需处理，后面会添加/ */
    }
    else if (BOOL_TRUE == BISO_SLCOMP_IS_CURRENT(pstComponent->ucFlags))
    {
        scnprintf(pcBuf, uiBufSize, ".");
        uiBufLen = 1;
    }
    else if (BOOL_TRUE == BISO_SLCOMP_IS_PARENT(pstComponent->ucFlags))
    {
        scnprintf(pcBuf, uiBufSize, "..");
        uiBufLen = 2;
    }
    else
    {
        memcpy(pcBuf, pstComponent->aucData, pstComponent->ucLength);
        uiBufLen = pstComponent->ucLength;
    }

    return uiBufLen;
}

STATIC BOOL_T BISO_RRIP_IsThisType(IN BISO_SUSP_ENTRY_S *pstEntry, IN CONST CHAR *pcType)
{
    if (NULL == pstEntry || NULL == pcType)
    {
        return BOOL_FALSE;
    }

    if ((pstEntry->cSignature1 == pcType[0]) && (pstEntry->cSignature2 == pcType[1]))
    {
        return BOOL_TRUE;
    }

    return BOOL_FALSE;
}

STATIC UCHAR * BISO_RRIP_GetSysUseArea
(
    IN  BISO_FILE_S *pstFile, 
    IN  UCHAR       *pucSysUseField,
    IN  UINT         uiSysUseFieldLen,
    OUT UINT        *puiAreaSize
)
{
    UINT uiCELen = 0;
    UINT uiCurPos = 0;
    UINT uiReadLen = 0;
    UINT64 ui64Seek = 0;
    UCHAR *pucSysUseArea = NULL;
    BISO_SUSP_ENTRY_S *pstEntry = NULL;
    BISO_SUSP_ENTRY_CE_S *pstCEEntry = NULL;

    DBGASSERT(NULL != pstFile);
    DBGASSERT(NULL != pucSysUseField);
    DBGASSERT(NULL != puiAreaSize);

    /* 
     * 虽然Rock Ridge扩展标准中允许整个System Use Area中有多个CE表项来扩展，
     * 但是由于一条CE表项可以扩展的长度就足够了(32bit) 所以这里我感觉正常情况下
     * 没有必要使用多个CE表项扩展空间。因此这里只支持1条CE表项的情况。
     */

    /* 遍历当前System Use Field, 标准规定剩余空间小于4字节,则后面的忽略 */
    for (uiCurPos = 0; uiCurPos + 4 < uiSysUseFieldLen; uiCurPos += pstEntry->ucEntryLen)
    {
        pstEntry = (BISO_SUSP_ENTRY_S *)(pucSysUseField + uiCurPos);

        /* 找到1个CE表项就停止，这里默认CE表项是最后一条表项 */
        if (BOOL_TRUE == BISO_RRIP_IsThisType(pstEntry, "CE"))
        {
            pstCEEntry = (BISO_SUSP_ENTRY_CE_S *)pstEntry;
            uiCELen = pstCEEntry->uiContinuationLen;
            /* BISO_DUMP_ShowSUSPEntry(pstCEEntry); */
            break;
        }
    }

    /* 申请一块内存把这两部分合并起来 */
    pucSysUseArea = (UCHAR *)BISO_MALLOC(uiCurPos + uiCELen);
    if (NULL == pucSysUseArea)
    {
        return NULL;
    }

    /* 先拷贝System Use Field字段 */
    memcpy(pucSysUseArea, pucSysUseField, uiCurPos);

    /* 如果有CE表项则再同文件中读出CE部分的数据 */
    if (NULL != pstCEEntry)
    {
        ui64Seek = (UINT64)pstCEEntry->uiBlockLoc * BISO_BLOCK_SIZE + pstCEEntry->uiByteOffset;
        BISO_PLAT_SeekFile(pstFile, ui64Seek, SEEK_SET);
        uiReadLen = (UINT)BISO_PLAT_ReadFile(pstFile, 1, uiCELen, pucSysUseArea + uiCurPos);
        if (uiReadLen != uiCELen)
        {
            BISO_DIAG("Read len %u buf len %u.", uiReadLen, uiCELen);
            BISO_FREE(pucSysUseArea);
            return NULL;
        }
    }

    *puiAreaSize = uiCurPos + uiCELen;
    return pucSysUseArea;
}

VOID BISO_RRIP_GetPXInfo(IN VOID *pEntry, OUT BISO_DIR_TREE_S *pstDirTree)
{
    BISO_POSIX_INFO_S *pstPosixInfo = NULL;
    BISO_ROCK_RIDGE_ENTRY_PX_S *pstPXEntry = NULL;

    DBGASSERT(NULL != pEntry);
    DBGASSERT(NULL != pstDirTree);
    DBGASSERT(NULL != pstDirTree->pstPosixInfo);

    pstPXEntry = (BISO_ROCK_RIDGE_ENTRY_PX_S *)pEntry;
    pstPosixInfo = pstDirTree->pstPosixInfo;

    pstPosixInfo->uiPosixFileMode    = pstPXEntry->uiPosixFileMode;
    pstPosixInfo->uiPosixFileLink    = pstPXEntry->uiPosixFileLink;
    pstPosixInfo->uiPosixFileUserId  = pstPXEntry->uiPosixFileUserId;
    pstPosixInfo->uiPosixFileGroupId = pstPXEntry->uiPosixFileGroupId;
    pstPosixInfo->uiPosixFileSNO     = pstPXEntry->uiPosixFileSNO;
}

VOID BISO_RRIP_GetNMInfo(IN VOID *pEntry, OUT BISO_DIR_TREE_S *pstDirTree)
{
    BISO_ROCK_RIDGE_ENTRY_NM_S *pstNMEntry = NULL;

    DBGASSERT(NULL != pEntry);
    DBGASSERT(NULL != pstDirTree);
    DBGASSERT(NULL != pstDirTree->pstPosixInfo);

    pstNMEntry = (BISO_ROCK_RIDGE_ENTRY_NM_S *)pEntry;

    /* 如有NM表项就替换ISO9660文件名 */
    if (BOOL_TRUE != pstDirTree->pstPosixInfo->bHasNMEntry)
    {
        pstDirTree->pstPosixInfo->bHasNMEntry = BOOL_TRUE;
        memset(pstDirTree->szName, 0, sizeof(pstDirTree->szName));
        pstDirTree->usNameLen = 0;
    }
    
    /*
     * 拼接文件名, 有可能本函数会多次调用,多次拼接(文件名超长的情况) 
     * TODO: 是否需要关注字符编码???
     */
    strncat(pstDirTree->szName, pstNMEntry->szFileName, pstNMEntry->ucEntryLen - 5);
    pstDirTree->usNameLen += pstNMEntry->ucEntryLen - 5;
}

VOID BISO_RRIP_GetTFInfo(IN VOID *pEntry, OUT BISO_DIR_TREE_S *pstDirTree)
{
    UINT i;
    UCHAR *pucCur = NULL;
    BISO_DATE_915_S *pst915Date = NULL;
    BISO_ROCK_RIDGE_ENTRY_TF_S *pstTFEntry = NULL;
    BISO_DATE_S *apstDate[] = 
    {
        &(pstDirTree->pstPosixInfo->stCreateTime),
        &(pstDirTree->pstPosixInfo->stModifyTime),
        &(pstDirTree->pstPosixInfo->stLastAccessTime),
        &(pstDirTree->pstPosixInfo->stLastAttrChangeTime),
        &(pstDirTree->pstPosixInfo->stLastBackupTime),
        &(pstDirTree->pstPosixInfo->stExpirationTime),
        &(pstDirTree->pstPosixInfo->stEffectiveTime)
    };

    DBGASSERT(NULL != pEntry);
    DBGASSERT(NULL != pstDirTree);
    DBGASSERT(NULL != pstDirTree->pstPosixInfo);

    pstTFEntry = (BISO_ROCK_RIDGE_ENTRY_TF_S *)pEntry;
    pucCur = pstTFEntry->aucTimeStamp;

    for (i = 0; i < ARRAY_SIZE(apstDate); i++)
    {
        /* 比特位0说明该时间戳没有记录 */
        if (0 == ((pstTFEntry->ucFlags >> i) & 0x1))
        {
            continue;
        }

        /* Bit7决定是按照哪种格式记录的 */
        if ((pstTFEntry->ucFlags >> 7) & 0x1)
        {
            (VOID)BISO_9660_ParseDate84261((CHAR *)pucCur, apstDate[i]);
            pucCur += 17;
        }
        else
        {
            pst915Date = (BISO_DATE_915_S *)pucCur;
            pucCur += 7;

            apstDate[i]->usYear    = pst915Date->ucYear + 1900;
            apstDate[i]->ucMonth   = pst915Date->ucMonth;
            apstDate[i]->ucDay     = pst915Date->ucDay;
            apstDate[i]->ucHour    = pst915Date->ucHour;
            apstDate[i]->ucMin     = pst915Date->ucMin;
            apstDate[i]->ucSecond  = pst915Date->ucSec;
            apstDate[i]->usMillSec = 0;
            apstDate[i]->cZone     = pst915Date->cTimeZone / 4;
        }
    }
}

VOID BISO_RRIP_GetPNInfo(IN VOID *pEntry, OUT BISO_DIR_TREE_S *pstDirTree)
{
    BISO_ROCK_RIDGE_ENTRY_PN_S *pstPNEntry = NULL;

    DBGASSERT(NULL != pEntry);
    DBGASSERT(NULL != pstDirTree);
    DBGASSERT(NULL != pstDirTree->pstPosixInfo);

    pstPNEntry = (BISO_ROCK_RIDGE_ENTRY_PN_S *)pEntry;
    
    pstDirTree->pstPosixInfo->ui64DevNum = ((UINT64)(pstPNEntry->uiDevNumHigh) << 32) | pstPNEntry->uiDevNumLow;
}

VOID BISO_RRIP_GetSLInfo(IN VOID *pEntry, OUT BISO_DIR_TREE_S *pstDirTree)
{
    UINT  uiBufLen = 0;
    UINT  uiOffset = 0;
    UINT  uiCurPos = 0;
    UCHAR ucCompentLen = 0;
    CHAR *pcFullLinkPath = NULL;
    BISO_POSIX_INFO_S *pstPosixInfo = NULL;
    BISO_ROCK_RIDGE_ENTRY_SL_S *pstSLEntry = NULL;
    BISO_RRIP_SL_COMPONENT_S *pstComp = NULL;
    CHAR szBuf[300]; /* 当前Length是用UCHAR存储的，一定不会超过300 */

    DBGASSERT(NULL != pEntry);
    DBGASSERT(NULL != pstDirTree);
    DBGASSERT(NULL != pstDirTree->pstPosixInfo);

    pstSLEntry = (BISO_ROCK_RIDGE_ENTRY_SL_S *)pEntry;
    pstPosixInfo = pstDirTree->pstPosixInfo;
    ucCompentLen = pstSLEntry->ucEntryLen - 5;

    /*
     * 把所有SL表项的Componet部分拼接起来，如果有连续几条SL表项
     * 那么当前函数会依次被调用，每次都拼接一部分,直到整个Componet整合完成.
     */
    BISO_RRIP_AddLinkBuf((CHAR *)(pstSLEntry->aucComponet), ucCompentLen, pstPosixInfo);

    /* FLAG的Bit0为0表示是最后1个SL表项,此时Componet已经整合在一起了,这里直接处理 */
    if (0 == (pstSLEntry->ucFlags & 0x1))
    {
        /* 申请一段内存用来保存符号链接的源路径 */
        uiBufLen = BISO_RRIP_CalcLinkLen(pstPosixInfo->pcLinkSrc, pstPosixInfo->uiLinkLen);
        pcFullLinkPath = (CHAR *)BISO_MALLOC(uiBufLen + 10);
        if (NULL == pcFullLinkPath)
        {
            BISO_FREE(pstPosixInfo->pcLinkSrc);
            pstPosixInfo->uiLinkLen = 0;
            return;
        }

        /* 拼接出链接的源路径 */
        while (uiOffset < pstPosixInfo->uiLinkLen)
        {
            pstComp = (BISO_RRIP_SL_COMPONENT_S *)(pstPosixInfo->pcLinkSrc + uiOffset);
            uiBufLen = BISO_RRIP_GetPartLink(pstComp, sizeof(szBuf), szBuf);

            /* ucLength不包括头两个字节 */
            uiOffset += pstComp->ucLength + 2;

            /*
             * 如果是link路径的一部分完结,则需要在后面加上'/',否则不加.
             * 不加的情况有两种: 1是整体已经完了  2是路径的一部分还没有完整
             * 比如说链接的位置是 /root/xxxx/a.txt 而xxxx可能非常长(200+个字符)
             * 那么此时光xxxx的部分可能就需要两个Componet部分才能表达完,而这两个
             * 之间是不能加反斜杠的.
             */
            if ((uiOffset < pstPosixInfo->uiLinkLen) && (BOOL_TRUE != BISO_SLCOMP_IS_CONTINUE(pstComp->ucFlags)))
            {
                szBuf[uiBufLen++] = '/';
            }

            memcpy(pcFullLinkPath + uiCurPos, szBuf, uiBufLen);
            uiCurPos += uiBufLen;
        }

        pcFullLinkPath[uiCurPos++] = 0;

        /* 原来的内存释放掉 */
        BISO_FREE(pstPosixInfo->pcLinkSrc);
        pstPosixInfo->pcLinkSrc = pcFullLinkPath;
        pstPosixInfo->uiLinkLen = uiCurPos;
    }
}

ULONG BISO_RRIP_ReadExtInfo
(
    IN  BISO_FILE_S       *pstFile,
    IN  BISO_PARSER_S     *pstParser,
    IN  BISO_DIR_RECORD_S *pstRecord, 
    OUT BISO_DIR_TREE_S   *pstDirTree 
)
{
    UINT   i = 0;
    UINT   uiOffset = 0;
    UINT   uiAreaSize = 0;
    UCHAR *pucSysUseArea = NULL;
    BISO_SUSP_ENTRY_S *pstEntry = NULL;

    DBGASSERT(NULL != pstFile);
    DBGASSERT(NULL != pstParser);
    DBGASSERT(NULL != pstRecord);
    DBGASSERT(NULL != pstDirTree);

    /* 没有使用Rock Ridge扩展则直接返回 */
    if (0 == pstParser->ucRRIPVersion)
    {
        return BISO_SUCCESS;
    }

    /* 先申请POSIX INFO结构体内存 */
    if (NULL == pstDirTree->pstPosixInfo)
    {
        pstDirTree->pstPosixInfo = (BISO_POSIX_INFO_S *)BISO_ZALLOC(sizeof(BISO_POSIX_INFO_S));
        if (NULL == pstDirTree->pstPosixInfo)
        {
            return BISO_ERR_ALLOC_MEM;
        }
    }

    /* 偏移到System Use字段所在的位置，注意Padding字段, 保证偏移为偶数 */
    uiOffset = 33 + pstRecord->ucNameLen;
    uiOffset += uiOffset & 0x1;

    /* 再加上SP Entry中定义的SkipLen长度 */
    uiOffset += pstParser->ucRRIPSkipLen;

    /* 获取整个Syetem Use区域的数据(包括CE扩展区) */
    pucSysUseArea = BISO_RRIP_GetSysUseArea(pstFile, (UCHAR *)pstRecord + uiOffset, 
                                            pstRecord->ucLength - (UCHAR)uiOffset, &uiAreaSize);
    if (NULL == pucSysUseArea)
    {
        return BISO_ERR_ALLOC_MEM;
    }

    /* 遍历所有的RRIP表项 */
    for(uiOffset = 0; uiOffset + 4 < uiAreaSize; uiOffset += pstEntry->ucEntryLen)
    {
        pstEntry = (BISO_SUSP_ENTRY_S *)(pucSysUseArea + uiOffset);
        /* BISO_DUMP_ShowSUSPEntry(pstEntry); */

        /* 找到对应的处理函数处理 */
        for (i = 0; i < ARRAY_SIZE(g_astBISO_RRIP_ParseFunc); i++)
        {
            if (BOOL_TRUE == BISO_RRIP_IsThisType(pstEntry, g_astBISO_RRIP_ParseFunc[i].szSignature))
            {
                if (NULL != g_astBISO_RRIP_ParseFunc[i].pfFunc)
                {
                    g_astBISO_RRIP_ParseFunc[i].pfFunc(pstEntry, pstDirTree);
                }
                break;
            }
        }
    }

    BISO_FREE(pucSysUseArea);
    return BISO_SUCCESS;
}

ULONG BISO_RRIP_ReadIndicator(INOUT BISO_PARSER_S *pstParser)
{
    ULONG ulRet;
    UINT64 ui64Seek = 0;
    BISO_DIR_RECORD_S *pstRootDir = NULL;
    BISO_SUSP_ENTRY_SP_S *pstSPEntry = NULL;
    UCHAR aucBuf[sizeof(BISO_DIR_RECORD_S) + sizeof(BISO_SUSP_ENTRY_SP_S)];
    
    DBGASSERT(NULL != pstParser);

    /* 读出Root Directory Record */
    pstRootDir = &(pstParser->pstPVD->stRootDirRecord);
    ui64Seek = BISO_BLOCK_SIZE * (UINT64)pstRootDir->uiExtent;
    ulRet = BISO_UTIL_ReadFile(pstParser->szFileName, ui64Seek, sizeof(aucBuf), aucBuf);
    if (BISO_SUCCESS != ulRet)
    {
        return ulRet;
    }

    /* 看看Root Directory Record的System Use字段里有没有SP Entry */
    pstRootDir = (BISO_DIR_RECORD_S *)aucBuf;
    pstSPEntry = (BISO_SUSP_ENTRY_SP_S *)(pstRootDir + 1);
    if (('S' != pstSPEntry->cSignature1) || ('P' != pstSPEntry->cSignature2) ||
        (0xBE != pstSPEntry->ucChkBE) || (0xEF != pstSPEntry->ucChkEF))
    {
        pstParser->ucRRIPVersion = 0;
        pstParser->ucRRIPSkipLen = 0;
    }
    else
    {
        pstParser->ucRRIPVersion = pstSPEntry->ucVersion;
        pstParser->ucRRIPSkipLen = pstSPEntry->ucSkipLen;       
    }
    
    return BISO_SUCCESS;
}



