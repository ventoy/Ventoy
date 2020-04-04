/******************************************************************************
 * biso_list.c
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

VOID BISO_DLL_Init(OUT BISO_DLL_S *pstList)
{
    pstList->stHead.pstNext = &(pstList->stHead);
    pstList->stHead.pstPre  = &(pstList->stHead);
    pstList->pstTail        = &(pstList->stHead);
    pstList->uiCount        = 0;
}

VOID BISO_DLL_AddTail
(
    IN BISO_DLL_S *pstList, 
    IN BISO_DLL_NODE_S *pstNode
)
{
    pstList->pstTail->pstNext = pstNode;
    pstNode->pstNext = NULL;
    pstNode->pstPre  = pstList->pstTail;
    pstList->pstTail = pstNode;
    pstList->uiCount++;
}

VOID BISO_DLL_DelHead(IN BISO_DLL_S *pstList)
{
    BISO_DLL_NODE_S *pstFirst = BISO_DLL_First(pstList);
    
    if (NULL != pstFirst)
    {
        if (1 == BISO_DLL_Count(pstList))  /* 唯一节点 */
        {
            BISO_DLL_Init(pstList);
        }
        else
        {
            pstFirst->pstNext->pstPre = &(pstList->stHead);
            pstList->stHead.pstNext = pstFirst->pstNext;
            pstList->uiCount--;
        }
    }
}
VOID BISO_DLL_DelTail(IN BISO_DLL_S *pstList)
{
    BISO_DLL_NODE_S *pstLast = BISO_DLL_Last(pstList);

    if (NULL != pstLast)
    {
        if (1 == BISO_DLL_Count(pstList))  /* 唯一节点 */
        {
            BISO_DLL_Init(pstList);
        }
        else
        {
            pstLast->pstPre->pstNext = NULL;
            pstList->pstTail = pstLast->pstPre;
            pstList->uiCount--;
        }
    }
}

VOID BISO_DLL_Free(IN BISO_DLL_S *pstList)
{
    BISO_DLL_NODE_S *pstFirst = BISO_DLL_First(pstList);

    while (NULL != pstFirst)
    {
        /* 每次都摘掉头节点 */
        BISO_DLL_DelHead(pstList);

        /* 使用free释放节点 */
        BISO_FREE(pstFirst);
        pstFirst = BISO_DLL_First(pstList);
    }
}


