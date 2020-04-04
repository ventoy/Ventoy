/******************************************************************************
 * biso_util.h
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
 
#ifndef __BISO_UTIL_H__
#define __BISO_UTIL_H__

#ifndef scnprintf
#define scnprintf(buf, bufsize, fmt, arg...) \
{\
    snprintf((buf), (bufsize) - 1, fmt, ##arg);\
    (buf)[(bufsize) - 1] = 0;\
}
#endif

#define MEMORY_DEBUG       0
#define MEMORY_DEBUG_DUMP  0

#if (MEMORY_DEBUG == 1)
    #define  BISO_MALLOC   BISO_UTIL_Malloc
    #define  BISO_ZALLOC   BISO_UTIL_Zalloc
    #define  BISO_FREE     BISO_UTIL_Free
#else
    #define  BISO_MALLOC   malloc
    #define  BISO_ZALLOC   zalloc
    #define  BISO_FREE     free
#endif

#define BISO_UCHAR_MAX    0xFF
#define BISO_USHORT_MAX   0xFFFF
#define BISO_UINT_MAX     0xFFFFFFFF

/* 无条件字节序转换 */
#define BISO_SWAP_UINT(data)         \
    ((((data) & 0x000000FF) << 24) | \
     (((data) & 0x0000FF00) << 8)  | \
     (((data) & 0x00FF0000) >> 8)  | \
     (((data) & 0xFF000000) >> 24))

#define BISO_SWAP_USHORT(data)  \
    ((USHORT)(((data) & 0x00FF) << 8)  | \
    (USHORT)(((data) & 0xFF00) >> 8))

/* 更新计数(加或减) */
#define BISO_STAT_UPDATE(bAdd, a, b) \
{ \
    if (BOOL_TRUE == bAdd) \
    { \
        (a) += (b); \
    } \
    else \
    { \
        (a) -= (b); \
    } \
}

#if (__BYTE_ORDER == __LITTLE_ENDIAN)

/* 从小字节序转为主机序 */
#define BISO_LTOH_UINT(data)   data
#define BISO_LTOH_USHORT(data) data

/* 从主机序转为小字节序 */
#define BISO_HTOL_UINT(data)   data
#define BISO_HTOL_USHORT(data) data

/* 从主机序转为大字节序 */
#define BISO_HTOM_UINT(data)   BISO_SWAP_UINT(data)
#define BISO_HTOM_USHORT(data) BISO_SWAP_USHORT(data)

#elif (__BYTE_ORDER == __BIG_ENDIAN)

/* 从小字节序转为主机序 */
#define BISO_LTOH_UINT(data)   BISO_SWAP_UINT(data)
#define BISO_LTOH_USHORT(data) BISO_SWAP_USHORT(data)

/* 从主机序转为小字节序 */
#define BISO_HTOL_UINT(data)   BISO_SWAP_UINT(data)
#define BISO_HTOL_USHORT(data) BISO_SWAP_USHORT(data)

/* 从主机序转为大字节序 */
#define BISO_HTOM_UINT(data)   data
#define BISO_HTOM_USHORT(data) data

#else
#error ("you must first define __BYTE_ORDER !!!")
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) ((UINT)(sizeof (a) / sizeof ((a)[0])))
#endif

#ifndef DBGASSERT
#define DBGASSERT(expr)
#endif

/* 读写文件时的缓冲区大小 1M */
#define BISO_FILE_BUF_LEN   (1024 * 1024)

/* MBUF中最大内存块个数 */
#define BISO_MBUF_MAX_BLK   256

/* MBUF 结构 */
typedef struct tagBISO_MBUF
{
    UCHAR *apucDataBuf[BISO_MBUF_MAX_BLK];
    UINT   auiBufSize[BISO_MBUF_MAX_BLK];
    UINT   uiCurBufNum;
    UINT   uiTotDataSize;
}BISO_MBUF_S;

/* 检查读操作句柄有没有已经和ISO文件关联 */
#define BISO_IS_READ_HANDLE_VALID(pstRead) \
    (NULL == (((BISO_PARSER_S *)pstRead)->pstPVD) ? BOOL_FALSE : BOOL_TRUE)
#define BISO_CHECK_READ_HANDLE(pstRead) \
{\
    if (NULL == ((BISO_PARSER_S *)(pstRead))->pstPVD) \
    { \
        return BISO_ERR_HANDLE_UNINITIALIZED; \
    } \
}

/* 设置类型 */
#define BISO_SET_FLAG(pstFileNode, pstDirTree) \
{ \
    (pstFileNode)->ucFlag = BISO_NODE_REGFILE; \
    if (NULL != (pstDirTree)->pstDirStat) \
    { \
        (pstFileNode)->ucFlag = BISO_NODE_DIRECTORY; \
    } \
    else if (BOOL_TRUE == BISO_DIR_TREE_IS_SYMLINK(pstDirTree)) \
    { \
        (pstFileNode)->ucFlag = BISO_NODE_SYMLINK; \
    } \
}

#define BISO_QUEUE_PTR_NUM   1024

/* 队列节点，为避免频繁分配内存，这里每次扩展1024个指针的长度空间 */
typedef struct tagBISO_QUEUE_NODE
{
    BISO_DLL_NODE_S stNode;
    PVOID apList[BISO_QUEUE_PTR_NUM];
    USHORT usFirst;
    USHORT usLast;
}BISO_QUEUE_NODE_S;

/* 队列/堆栈 简单实现 */
typedef BISO_DLL_S BISO_QUEUE_S;

INT BISO_UTIL_GetTimeZone(VOID);

CHAR * BISO_UTIL_CopyStr
(
    IN CONST CHAR *szSrc,
    IN UINT uiSrcSize,
    OUT CHAR *szDest
);
CHAR * BISO_UTIL_CopyUCS2Str
(
    IN  CONST CHAR *szSrc, 
    IN  UINT        uiSrcSize, 
    OUT CHAR       *szDest
);
VOID BISO_UTIL_PathProc(INOUT CHAR *pcPath, INOUT UINT *puiLen);
ULONG BISO_UTIL_PathSplit
(
    IN CONST CHAR *pcFullPath, 
    OUT UINT *puiDirNum, 
    OUT UINT *puiDirPos
);

ULONG BISO_UTIL_ReadFile
(
    IN  CONST CHAR *pcFileName,
    IN  UINT64 ui64Seek,
    IN  UINT   uiDataLen,
    OUT VOID *pDataBuf
);

ULONG BISO_MBUF_Append
(
    IN BISO_MBUF_S *pstMBuf,
    IN UINT uiDataSize,
    IN VOID *pData
);
VOID BISO_MBUF_Free(IN BISO_MBUF_S *pstMBuf);
VOID BISO_MBUF_CopyToBuf(IN CONST BISO_MBUF_S *pstMBuf, OUT VOID *pDataBuf);
VOID BISO_MBUF_PULLUP(INOUT BISO_MBUF_S *pstMBuf);

BISO_QUEUE_S * BISO_QUEUE_Create(VOID);
VOID BISO_QUEUE_Destroy(IN BISO_QUEUE_S *pstQueue);
VOID BISO_QUEUE_Push(IN BISO_QUEUE_S *pstQueue, IN VOID *pData);
VOID * BISO_QUEUE_PopHead(IN BISO_QUEUE_S *pstQueue);
VOID * BISO_QUEUE_PopTail(IN BISO_QUEUE_S *pstQueue);
VOID * BISO_UTIL_Malloc(IN size_t ulSize);
VOID * BISO_UTIL_Zalloc(IN size_t ulSize);
VOID BISO_UTIL_Free(IN VOID *pData);
VOID BISO_UTIL_DumpMemOp(VOID);

ULONG BISO_UTIL_ExtractFile
(
    IN BISO_FILE_S *pstFile,
    IN UINT64  ui64Seek,
    IN UINT64  ui64Len,
    IN CONST CHAR *pcNewFileName
);
BOOL_T BISO_PLAT_IsPathExist(IN CONST CHAR *pcPath);
ULONG BISO_PLAT_MkDir(IN CONST CHAR *pcFullPath);
UINT BISO_PLAT_GetMaxPath(VOID);

BISO_FILE_S * BISO_PLAT_OpenExistFile(IN CONST CHAR *pcFileName);
BISO_FILE_S * BISO_PLAT_CreateNewFile(IN CONST CHAR *pcFileName);
VOID BISO_PLAT_CloseFile(IN BISO_FILE_S *pstFile);
INT64 BISO_PLAT_SeekFile(BISO_FILE_S *pstFile, INT64 i64Offset, INT iFromWhere);
UINT64 BISO_PLAT_ReadFile
(
    IN  BISO_FILE_S *pstFile,
    IN  UINT         uiBlkSize,
    IN  UINT         uiBlkNum,
    OUT VOID        *pBuf
);
UINT64 BISO_PLAT_WriteFile
(
    IN  BISO_FILE_S *pstFile,
    IN  UINT         uiBlkSize,
    IN  UINT         uiBlkNum,
    IN  VOID        *pBuf
);

VOID *zalloc(size_t size);

#endif /* __BISO_UTIL_H__ */

