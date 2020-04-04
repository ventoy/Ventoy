/******************************************************************************
 * biso_dump.c
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


VOID BISO_DUMP_ShowFileTree
(
    IN UINT uiDepth,
    IN CONST BISO_DIR_TREE_S *pstDirTree
)
{
    UINT i;
    
    if (NULL == pstDirTree)
    {
        return;
    }

    for (i = 0; i + 1 < uiDepth; i++)
    {
        BISO_DUMP("    ");
    }

    if (BOOL_TRUE == BISO_DIR_TREE_IS_SYMLINK(pstDirTree))
    {
        BISO_DUMP("|-- %s --> %s", pstDirTree->szName, pstDirTree->pstPosixInfo->pcLinkSrc); 
    }
    else
    {
        BISO_DUMP("|-- %s", pstDirTree->szName);
    }

    BISO_DUMP(" %u %u\n", pstDirTree->uiExtent, pstDirTree->uiSize);
    
    
    #if 0
    if (NULL != pstDirTree->pstDirStat)
    {
        BISO_DUMP(" ([%u %u %u]  [%u %u %u]\n", 
            pstDirTree->pstDirStat->uiCurDirNum, 
            pstDirTree->pstDirStat->uiCurFileNum, 
            pstDirTree->pstDirStat->uiCurLinkNum, 
            pstDirTree->pstDirStat->uiTotDirNum, 
            pstDirTree->pstDirStat->uiTotFileNum,
            pstDirTree->pstDirStat->uiTotLinkNum); 
    }
    else
    {
        BISO_DUMP("\n");
    }
    #endif /* #if 0 */

    /* 递归显示子目录 */
    BISO_DUMP_ShowFileTree(uiDepth + 1, pstDirTree->pstChild);

    /* 显示本目录内的文件列表 */
    BISO_DUMP_ShowFileTree(uiDepth + 1, pstDirTree->pstFileList);

    /* 显示下一个同级目录 */
    BISO_DUMP_ShowFileTree(uiDepth, pstDirTree->pstNext);
}

