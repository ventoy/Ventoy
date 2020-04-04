/******************************************************************************
 * biso_list.h
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
 
#ifndef __BISO_LIST_H__
#define __BISO_LIST_H__

/* 简单链表的实现 */
typedef struct tagBISO_DLL_NODE{
   struct tagBISO_DLL_NODE *pstPre;  /* Points to The Previous Node In The List */
   struct tagBISO_DLL_NODE *pstNext; /* Points to The Next Node In The List */
}BISO_DLL_NODE_S;

typedef struct tagBISO_DLL{
   BISO_DLL_NODE_S  stHead;      /* 链表头 */
   BISO_DLL_NODE_S *pstTail;     /* 链表尾 */
   UINT             uiCount;     /* 链表节点个数 */
}BISO_DLL_S;

#define BISO_DLL_Count(pList)              ((pList)->uiCount)
#define BISO_DLL_First(pList)              ((BISO_DLL_Count((pList)) == 0) ? NULL: (pList)->stHead.pstNext)
#define BISO_DLL_Last(pList)               ((BISO_DLL_Count((pList)) == 0) ? NULL : (pList)->pstTail)

#define BISO_DLL_Next(pList, pNode)  \
            (((pNode) == NULL) ? BISO_DLL_First(pList) : \
            (((pNode)->pstNext == &(pList)->stHead) ? NULL : (pNode)->pstNext)) 

VOID BISO_DLL_Init(OUT BISO_DLL_S *pstList);
VOID BISO_DLL_AddTail
(
    IN BISO_DLL_S *pstList, 
    IN BISO_DLL_NODE_S *pstNode
);
VOID BISO_DLL_DelHead(IN BISO_DLL_S *pstList);
VOID BISO_DLL_DelTail(IN BISO_DLL_S *pstList);
VOID BISO_DLL_Free(IN BISO_DLL_S *pstList);

#endif /* __BISO_LIST_H__ */

