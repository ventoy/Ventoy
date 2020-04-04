/******************************************************************************
 * bios_eltorito.c
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

ULONG BISO_ELTORITO_ReadBootInfo
(
    IN  BISO_FILE_S   *pstFile, 
    OUT BISO_PARSER_S *pstParser
)
{
    USHORT i;
    UINT uiReadLen = 0;
    UINT64 ui64Seek = 0;
    UCHAR aucBuf[BISO_SECTOR_SIZE];
    BISO_MBUF_S stMBuf;
    BISO_TORITO_SECHDR_ENTRY_S *pstSecHdr = NULL;
    BISO_TORITO_SECTION_ENTRY_S *pstSection = NULL;
    
    DBGASSERT(NULL != pstFile);
    DBGASSERT(NULL != pstParser);

    if (NULL == pstParser->pstBVD)
    {
        return BISO_SUCCESS;
    }

    memset(&stMBuf, 0, sizeof(stMBuf));
    ui64Seek = (UINT64)pstParser->pstBVD->uiBootCatlogStart * BISO_SECTOR_SIZE;
    BISO_PLAT_SeekFile(pstFile, ui64Seek, SEEK_SET);

    /* 先读取1个逻辑扇区的内容 */
    uiReadLen = (UINT)BISO_PLAT_ReadFile(pstFile, 1, BISO_SECTOR_SIZE, aucBuf);
    if (uiReadLen != BISO_SECTOR_SIZE)
    {
        BISO_DIAG("Read len %u buf len %u.", uiReadLen, BISO_SECTOR_SIZE);
        return BISO_ERR_READ_FILE;
    }

    /* 前两条Entry固定是Validation和Initial */
    pstSecHdr = (BISO_TORITO_SECHDR_ENTRY_S *)(aucBuf + 2 * BISO_ELTORITO_ENTRY_LEN);
    pstSection = (BISO_TORITO_SECTION_ENTRY_S *)pstSecHdr;

    while ((0x90 == pstSecHdr->ucFlag) || (0x91 == pstSecHdr->ucFlag))
    {
        pstSecHdr = (BISO_TORITO_SECHDR_ENTRY_S *)pstSection;
        BISO_ELTORITO_ENTRY_STEP(pstSection, pstFile, aucBuf, stMBuf);

        for (i = 0; i < pstSecHdr->usSecEntryNum; )
        {
            /* 
             * Section Entry和Extension Entry都是由ucFlag的Bit5决定是否结束 
             * 因此这里全部都用Section Entry的结构做判断
             */
            if (0 == (pstSection->ucFlag & 0x10))
            {
                i++;
            }
           
            BISO_ELTORITO_ENTRY_STEP(pstSection, pstFile, aucBuf, stMBuf);            
        }
    
        if (0x91 == pstSecHdr->ucFlag) /* 91代表最后一个 */
        {
            break;
        }
    }

    if ((UCHAR *)pstSection > aucBuf)
    {
        (VOID)BISO_MBUF_Append(&stMBuf, (UCHAR *)pstSection - aucBuf, aucBuf);
    }

    /* 保存到全局结构中 */
    pstParser->uiElToritoLen = stMBuf.uiTotDataSize;    
    pstParser->pucElToritoEntry = (UCHAR *)BISO_MALLOC(pstParser->uiElToritoLen);
    if (NULL == pstParser->pucElToritoEntry)
    {
        BISO_MBUF_Free(&stMBuf);
        return BISO_ERR_ALLOC_MEM;
    }

    BISO_MBUF_CopyToBuf(&stMBuf, pstParser->pucElToritoEntry);
    BISO_MBUF_Free(&stMBuf);
    return  BISO_SUCCESS;
}

VOID BISO_ELTORITO_Dump(IN CONST BISO_PARSER_S *pstParser)
{   
    BISO_DUMP("uiElToritoLen=%u\n", pstParser->uiElToritoLen);
}


UINT BISO_ELTORITO_GetBootEntryNum(IN CONST BISO_PARSER_S *pstParser)
{
    UINT uiRet = 0;
    UINT uiEntryNum = 0;
    UCHAR *pucData = NULL;
    BISO_TORITO_VALIDATION_ENTRY_S *pstValidation = NULL;
    BISO_TORITO_INITIAL_ENTRY_S *pstInitial = NULL;
    
    if (NULL == pstParser->pucElToritoEntry)
    {
        return 0;
    }

    uiEntryNum = pstParser->uiElToritoLen / BISO_ELTORITO_ENTRY_LEN;
    pstValidation = (BISO_TORITO_VALIDATION_ENTRY_S *)pstParser->pucElToritoEntry;
    pstInitial = (BISO_TORITO_INITIAL_ENTRY_S *)(pstValidation + 1);

    if (pstInitial->ucBootId == 0x88)
    {
        uiRet++;
    }

    pucData = pstParser->pucElToritoEntry + 2 * BISO_ELTORITO_ENTRY_LEN;
    uiEntryNum-= 2;

    while (uiEntryNum > 0)
    {
        if ((0x90 == pucData[0]) || (0x91 == pucData[0]))
        {
        }
        else if (0x44 == pucData[0])
        {
        }
        else
        {
            if (((BISO_TORITO_SECTION_ENTRY_S *)pucData)->ucBootId == 0x88)
            {
                uiRet++;
            }
        }
        pucData += BISO_ELTORITO_ENTRY_LEN;
        uiEntryNum--;
    }

    return uiRet;
}

