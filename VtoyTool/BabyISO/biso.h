/******************************************************************************
 * biso.h
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

#ifndef __BISO_H__
#define __BISO_H__

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"

extern int g_biso_debug;
void BISO_SetDebug(int debug);

#define BISO_DIAG(fmt, ...) if(g_biso_debug) printf(fmt, ##__VA_ARGS__)
#define BISO_DUMP   printf

#ifndef STATIC
#define STATIC    static
#endif

#ifndef CONST
#define CONST    const
#endif

#ifndef INLINE
#define INLINE    inline
#endif

#ifndef VOID
#define VOID    void
#endif

#ifndef PVOID
typedef VOID *  PVOID;
#endif

#ifndef CHAR
#define CHAR    char
#endif

#ifndef UCHAR
#define UCHAR   unsigned char
#endif

#ifndef SHORT
#define SHORT   short
#endif

#ifndef USHORT
#define USHORT    unsigned short
#endif

#ifndef LONG
#define LONG      long
#endif

#ifndef ULONG
#define ULONG     unsigned long
#endif

#ifndef ULONGLONG
#define ULONGLONG     unsigned long long
#endif


#ifndef INT
#define INT       int
#endif

#ifndef UINT
#define UINT      unsigned int
#endif

#ifndef INT16
#define INT16     short
#endif

#ifndef UINT16
#define UINT16    unsigned short
#endif

#ifndef INT32
#define INT32     int
#endif

#ifndef UINT32
#define UINT32    unsigned int
#endif

#ifndef BOOL_T
typedef USHORT  BOOL_T;
#define BOOL_TRUE   ((BOOL_T)1)
#define BOOL_FALSE  ((BOOL_T)0)
#endif

typedef long long  INT64;
typedef unsigned long long UINT64;

#define BISO_PATH_STRCMP  strcmp

#ifndef NULL
#define NULL      (void *)0
#endif

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

#define BISO_TRUE  1
#define BISO_FALSE 0

typedef VOID BISO_READ_S;
typedef VOID BISO_WRITE_S;
typedef CONST VOID * BISO_HANDLE;

/* error code */
#define  BISO_SUCCESS                        0
#define  BISO_ERR_BASE                       0x1000
#define  BISO_ERR_FAILED                    (BISO_ERR_BASE + 1)
#define  BISO_ERR_NULL_PTR                  (BISO_ERR_BASE + 2)
#define  BISO_ERR_ALLOC_MEM                 (BISO_ERR_BASE + 3)
#define  BISO_ERR_OPEN_FILE                 (BISO_ERR_BASE + 4)
#define  BISO_ERR_READ_FILE                 (BISO_ERR_BASE + 5)
#define  BISO_ERR_WRITE_FILE                (BISO_ERR_BASE + 6)
#define  BISO_ERR_INVALID_ISO9660           (BISO_ERR_BASE + 7)
#define  BISO_ERR_UNSUPPORTED_BLKSIZE       (BISO_ERR_BASE + 8)
#define  BISO_ERR_INVALID_PARAM             (BISO_ERR_BASE + 9)
#define  BISO_ERR_NOT_FOUND                 (BISO_ERR_BASE + 10)
#define  BISO_ERR_NOT_RECORD                (BISO_ERR_BASE + 11)
#define  BISO_ERR_HANDLE_UNINITIALIZED      (BISO_ERR_BASE + 12)
#define  BISO_ERR_INVALID_RRIP_SP           (BISO_ERR_BASE + 13)
#define  BISO_ERR_ABORT                     (BISO_ERR_BASE + 14)

typedef struct tagBISO_VOLUME_SUMMARY
{
    CHAR szVolumeId[33];       
    CHAR szSystemId[33];       
    CHAR szPublisherId[129];   
    CHAR szPreparerId[129];    
    CHAR szApplicationId[129]; 
    CHAR szCopyrightFileId[38];
    CHAR szAbstractFileId[38]; 

    UINT uiRockRidgeVer;
    UINT uiJolietLevel;
    
    UINT uiTotDirNum;
    UINT uiTotFileNum;
    UINT uiTotLinkNum;
}BISO_VOLUME_SUMMARY_S;

#define BISO_TREE_FLAG_CUR   1
#define BISO_TREE_FLAG_DFS   2
#define BISO_TREE_FLAG_BFS   3

/* time */
typedef struct tagBISO_DATE
{
    USHORT usYear;     
    UCHAR  ucMonth;    
    UCHAR  ucDay;      
    UCHAR  ucHour;     
    UCHAR  ucMin;      
    UCHAR  ucSecond;   
    USHORT usMillSec;  
    CHAR   cZone;      
}BISO_DATE_S;

typedef enum tagBISO_DATE_TYPE
{
    BISO_DATE_TYPE_CREATE = 0,
    BISO_DATE_TYPE_MODIFY,
    BISO_DATE_TYPE_EXPIRATION,
    BISO_DATE_TYPE_EFFECTIVE,
    BISO_DATE_TYPE_BUTT
}BISO_DATE_TYPE_E;

/* dir stat */
typedef struct tagBISO_DIR_STAT
{
    UINT   uiCurDirNum;  
    UINT   uiCurFileNum; 
    UINT   uiCurLinkNum; 
    UINT   uiCurUsedSec; 
    UINT64 ui64CurSpace; 
    UINT   uiTotDirNum;  
    UINT   uiTotFileNum; 
    UINT   uiTotLinkNum; 
    UINT64 ui64TotSpace; 
    UINT   uiTotUsedSec; 
}BISO_DIR_STAT_S;

#define BISO_NODE_REGFILE      1
#define BISO_NODE_SYMLINK      2
#define BISO_NODE_DIRECTORY    4

/* file tree */
typedef struct tagBISO_FILE_NODE
{
    /*
     * ucFlag
     * BISO_NODE_REGFILE
     * BISO_NODE_SYMLINK
     * BISO_NODE_DIRECTORY
     */
    UCHAR ucFlag;
    CHAR  szName[256];    
    CHAR  szLinkTgt[256]; 
    UINT64 ui64FileSize;  
    UINT64 ui64Seek;      
    UINT64 ui64DirRecOffet;
    BISO_HANDLE hParent; 
    BISO_HANDLE hCurrent;
}BISO_FILE_NODE_S;

typedef struct tagBISO_SVD_FILE_NODE
{
    UINT64 ui64FileSize;
    UINT64 ui64Seek;
    UINT64 ui64DirRecOffet;
}BISO_SVD_FILE_NODE_S;

/* timestamp type */
#define BISO_EXTRACT_TIME_FOLLOW    1
#define BISO_EXTRACT_TIME_SPECIFY   2

typedef struct tagBISO_EXTRACT_CTRL
{
    UCHAR ucATimeFlag;
    UCHAR ucMTimeFlag;
    BISO_DATE_S stATime;
    BISO_DATE_S stMTime;
}BISO_EXTRACT_CTRL_S;

#define BISO_EXTRACT_MSG_MAKE_DIR        1
#define BISO_EXTRACT_MSG_CREATE_FILE     2
#define BISO_EXTRACT_MSG_SYMLINK         3

typedef struct tagBISO_EXTRACT_NOTIFY
{
    UINT  uiMsg;
    ULONG ulResult;
    CONST CHAR *pcFileName;
}BISO_EXTRACT_NOTIFY_S;

typedef ULONG (* BISO_EXTRACE_CB_PF)(IN CONST BISO_EXTRACT_NOTIFY_S *pstNotify);

CONST CHAR * BISO_GetErrMsg(IN ULONG ulErrCode);

VOID BISO_GetNow(OUT BISO_DATE_S *pstTM);

VOID BISO_TimeConv(IN ULONG ulTime, OUT BISO_DATE_S *pstTM);

BISO_READ_S * BISO_AllocReadHandle(VOID);

VOID BISO_FreeReadHandle(INOUT BISO_READ_S *pstRead);

BOOL_T BISO_IsISOFile(IN CONST CHAR *pcFileName);

BOOL_T BISO_IsUDFFile(IN CONST CHAR *pcFileName);

ULONG BISO_OpenImage(IN CONST CHAR *pcFileName, OUT BISO_READ_S *pstRead);
ULONG BISO_OpenImageWithSVD(IN CONST CHAR *pcFileName, OUT BISO_READ_S *pstRead);
BOOL_T BISO_HasSVD(IN CONST BISO_READ_S *pstRead);

ULONG BISO_GetVolumeSummary
(
    IN CONST BISO_READ_S *pstRead, 
    OUT BISO_VOLUME_SUMMARY_S *pstSummary
);

ULONG BISO_GetDate
(
    IN CONST BISO_READ_S *pstRead, 
    IN  BISO_DATE_TYPE_E enType, 
    OUT BISO_DATE_S *pstDate
);

UINT BISO_GetRockRidgeVer(IN CONST BISO_READ_S *pstRead);

UINT BISO_GetJolietLevel(IN CONST BISO_READ_S *pstRead);

BISO_HANDLE BISO_GetRoot(IN CONST BISO_READ_S *pstRead);

ULONG BISO_GetFileNodeByHdl
(
    IN  BISO_HANDLE       hFileHdl, 
    OUT BISO_FILE_NODE_S *pstFileNode
);

ULONG BISO_GetFileNodeByName
(
    IN CONST BISO_READ_S *pstRead,
    IN CONST CHAR *pcFullPath, 
    IN UCHAR ucFollowLink,
    OUT BISO_FILE_NODE_S *pstFileNode
);

ULONG BISO_GetFileNodeByExtent
(
    IN CONST BISO_READ_S *pstRead,
    IN UINT uiExtent,
    OUT BISO_FILE_NODE_S *pstFileNode
);

ULONG BISO_GetSVDFileNodeByExtent
(
    IN CONST BISO_READ_S *pstRead,
    IN UINT uiExtent,
    OUT BISO_SVD_FILE_NODE_S *pstFileNode
);

ULONG BISO_GetFileTree
(
    IN  BISO_HANDLE  hTopDir, 
    IN  UINT         uiFlag,
    OUT BISO_HANDLE *phFileTree,
    OUT UINT        *puiNodeNum
);

ULONG BISO_GetDirStat
(
    IN  BISO_HANDLE      hTopDir, 
    OUT BISO_DIR_STAT_S *pstDirStat
);

ULONG BISO_ExtractFile
(
    IN CONST BISO_READ_S *pstRead,
    IN CONST BISO_HANDLE  hTopDir,
    IN CONST CHAR        *pcDstPath,
    IN CONST BISO_EXTRACT_CTRL_S *pstCtrl,
    IN BISO_EXTRACE_CB_PF pfCallBack
);

VOID BISO_Fill733(IN UINT uiData, OUT VOID *pBuf);
UINT BISO_Get733(IN CONST VOID *pBuf);
UINT BISO_GetFileOccupySize(IN UINT uiRawSize);
UINT BISO_GetBootEntryNum(IN CONST BISO_READ_S *pstRead);

VOID BISO_DumpVD(IN CONST BISO_READ_S *pstRead);

VOID BISO_DumpPathTable(IN CONST BISO_READ_S *pstRead);

VOID BISO_DumpFileTree(IN CONST BISO_READ_S *pstRead);

typedef struct tagBISO_FILE
{
    UINT64 CurPos;
    UINT64 FileSize;
}BISO_FILE_S;

UINT64 BISO_PLAT_GetFileSize(IN CONST CHAR *pcFileName);

VOID BISO_PLAT_UTime
(
    IN CONST CHAR        *pcFileName,
    IN CONST BISO_DATE_S *pstAccessTime,
    IN CONST BISO_DATE_S *pstModifyTime
);

BOOL_T BISO_PLAT_IsPathExist(IN CONST CHAR *pcPath);

BOOL_T BISO_PLAT_IsFileExist(IN CONST CHAR *pcFilePath);

ULONG BISO_PLAT_MkDir(IN CONST CHAR *pcFullPath);

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

CHAR * BISO_PLAT_GetCurDir(VOID);
UINT64 BISO_UTIL_WholeFile2Buf(IN CONST CHAR *szFileName, OUT UCHAR *pucBuf);

#endif /* __BISO_H__ */

