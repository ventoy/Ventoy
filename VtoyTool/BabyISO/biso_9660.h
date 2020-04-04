/******************************************************************************
 * bios_9660.h
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

#ifndef __BISO_9660_H__
#define __BISO_9660_H__

#define  BISO_SECTOR_SIZE          2048

#define  BISO_BLOCK_SIZE          2048

#define  BISO_SYSTEM_AREA_SIZE    (16 * BISO_SECTOR_SIZE)

/* Volume Descriptor */
#define  BISO_VD_TYPE_BOOT   0
#define  BISO_VD_TYPE_PVD    1  
#define  BISO_VD_TYPE_SVD    2
#define  BISO_VD_TYPE_PART   3
#define  BISO_VD_TYPE_END    255

#define  BISO_VD_ID          "CD001"

#define BISO_USED_SECTOR_NUM(uiFileSize) \
    (uiFileSize / BISO_SECTOR_SIZE) + ((uiFileSize % BISO_SECTOR_SIZE > 0) ? 1 : 0)

#define BISO_9660_IS_CURRENT(pstDirRecord) \
    (((1 == pstDirRecord->ucNameLen) && (0x0 == pstDirRecord->szName[0])) ? BOOL_TRUE : BOOL_FALSE)

#define BISO_9660_IS_PARENT(pstDirRecord) \
    (((1 == pstDirRecord->ucNameLen) && (0x1 == pstDirRecord->szName[0])) ? BOOL_TRUE : BOOL_FALSE)

#define BISO_9600_FREE_STAT(pstDir) \
{ \
    if (NULL != pstDir->pstDirStat) \
    { \
        BISO_FREE(pstDir->pstDirStat); \
    }\
}

#define BISO_9600_FREE_POSIX(pstDir) \
{ \
    if (NULL != pstDir->pstPosixInfo) \
    { \
        if (NULL != pstDir->pstPosixInfo->pcLinkSrc) \
        { \
            BISO_FREE(pstDir->pstPosixInfo->pcLinkSrc); \
        } \
        BISO_FREE(pstDir->pstPosixInfo); \
    } \
}

#define BISO_9600_FREE_DIRTREE(pstDir) \
{ \
    BISO_9600_FREE_STAT(pstDir); \
    BISO_9600_FREE_POSIX(pstDir); \
    BISO_FREE(pstDir); \
}

#if (__BYTE_ORDER == __LITTLE_ENDIAN)

#define  BISO_DEF_723(usData) \
    UINT16 usData;\
    UINT16 usData##Swap;

#define  BISO_DEF_733(uiData) \
    UINT32 uiData;\
    UINT32 uiData##Swap;

#define  BISO_PATHTBL_LOCATION(pstPVD)  (pstPVD->uiTypeLPathTblLoc)

#elif (__BYTE_ORDER == __BIG_ENDIAN)

#define  BISO_DEF_723(usData) \
    UINT16 usData##Swap;\
    UINT16 usData;

#define  BISO_DEF_733(uiData) \
    UINT32 uiData##Swap;\
    UINT32 uiData;

#define  BISO_PATHTBL_LOCATION(pstPVD)  (pstPVD->uiTypeMPathTblLoc)

#else
#error ("you must first define __BYTE_ORDER !!!")
#endif

#pragma pack(1)

typedef struct tagBISO_DATE_915
{
    UCHAR ucYear;      /* 1900+ */
    UCHAR ucMonth;    
    UCHAR ucDay;      
    UCHAR ucHour;     
    UCHAR ucMin;      
    UCHAR ucSec;      
    CHAR  cTimeZone;   
}BISO_DATE_915_S;

typedef struct tagBISO_PATH_TABLE
{
    UCHAR ucDirNameLen;

    UCHAR ucExtAttrRecordLen;

    UINT32  uiExtent;

    UINT16 usParentDirNum;

    CHAR  szDirName[1];
       
    /* UCHAR ucPadding; */
}BISO_PATH_TABLE_S;

#define BISO_9660_PATH_LEN(pstPath) (pstPath->ucDirNameLen + 8 + (pstPath->ucDirNameLen & 0x01))

typedef struct tagBISO_DIR_RECORD
{
    UCHAR ucLength;

    UCHAR ucExtAttrLen;

    BISO_DEF_733(uiExtent)

    BISO_DEF_733(uiSize)

    BISO_DATE_915_S stDate;    

    UCHAR ucFlags;

    UCHAR ucFileUnitSize;

    UCHAR ucInterLeave;

    BISO_DEF_723(uiVolumeSeqNum)

    UCHAR ucNameLen;

    CHAR  szName[1];

    /* UCHAR aucSystemUse[xxxx] */
}BISO_DIR_RECORD_S;

#define BISO_DIR_RECORD_IS_PATH(pstDirRecord) \
    ((1 == ((pstDirRecord->ucFlags >> 1) & 0x01)) ? BOOL_TRUE : BOOL_FALSE)

typedef struct tagBISO_VOLUME_DESC 
{
    UCHAR ucType;

    CHAR  szId[5];

    UCHAR ucVersion;

    UCHAR ucData[2041];
}BISO_VD_S;

typedef struct tagBISO_PRIMARY_VOLUME_DESC
{
    UCHAR ucType;

    CHAR  szId[5];

    /* ucVersion:　version [ISODCL (  7,   7)] */
    UCHAR ucVersion;

    /* ucResv1: unused1 [ISODCL (  8,   8)] */
    UCHAR ucResv1;

    /* 
     * szSystemId[32]: system_id [ISODCL (  9,  40)] 
     */
    CHAR  szSystemId[32];

    /* szVolumeId[32]: volume_id [ISODCL ( 41,  72)] */
    CHAR  szVolumeId[32];

    /* aucResv2[8]: unused2 [ISODCL ( 73,  80)] */
    UCHAR aucResv2[8];

    BISO_DEF_733(uiVolumeSpace)

    /* aucResv3[32]: unused3 [ISODCL ( 89,  120)] */
    UCHAR aucResv3[32];

    BISO_DEF_723(usVolumeSet)

    /* 
     * usVolumeSeq: volume_sequence_number [ISODCL (125, 128)] 
     */
    BISO_DEF_723(usVolumeSeq)
    
    /* 
     * usBlockSize: logical_block_size [ISODCL (129, 132)]
     */
    BISO_DEF_723(usBlockSize)

    /*
     * uiPathTblSize: path_table_size [ISODCL (133, 140)] 
     */
    BISO_DEF_733(uiPathTblSize)

    UINT32 uiTypeLPathTblLoc;

    UINT32 uiOptTypeLPathTblLoc;

    UINT32 uiTypeMPathTblLoc;
    
    UINT32 uiOptTypeMPathTblLoc;

    BISO_DIR_RECORD_S stRootDirRecord;

    /* szVolumeSetId: volume_set_id [ISODCL (191, 318)]  */
    CHAR szVolumeSetId[128];
    
    /* szPublisherId: publisher_id [ISODCL (319, 446)]  */
    CHAR szPublisherId[128];
    
    /* szPreparerId: preparer_id [ISODCL (447, 574)]  */
    CHAR szPreparerId[128];
    
    /* szApplicationId: application_id [ISODCL (575, 702)]  */
    CHAR szApplicationId[128];
    
    /* szCopyrightFileId: copyright_file_id [ISODCL (703, 739)] [1] */
    CHAR szCopyrightFileId[37];
    
    /* szAbstractFileId: abstract_file_id [ISODCL (740, 776)] */
    CHAR szAbstractFileId[37];
    
    /* szBibliographicFileId: bibliographic_file_id [ISODCL (777, 813)]  */
    CHAR szBibliographicFileId[37];

    /* szCreationDate: creation_date [ISODCL (814, 830)]  */
    CHAR szCreationDate[17];
    
    /* szModifyDate: modification_date [ISODCL (831, 847)] */
    CHAR szModifyDate[17];
    
    /* szExpirationDate: expiration_date [ISODCL (848, 864)]  */
    CHAR szExpirationDate[17];
    
    /* szEffectiveDate: effective_date [ISODCL (865, 881)]  */
    CHAR szEffectiveDate[17];
    
    /* ucFileStructVer: file_structure_version [ISODCL (882, 882)]  */
    UCHAR ucFileStructVer;
    
    /* ucResv4: unused4 [ISODCL (883, 883)] */
    UCHAR ucResv4;
    
    /* aucApplicationData: application_data [ISODCL (884, 1395)] */
    UCHAR aucApplicationData[512];

    /* ucResv5: unused5 [ISODCL (1396, 2048)] */
    UCHAR aucResv5[653];
}BISO_PVD_S;

typedef struct tagBISO_SUPPLEMENTARY_VOLUME_DESC
{
    UCHAR ucType;

    /* 
     * szId[5]: id [ISODCL (  2,   6)] 
     */
    CHAR  szId[5];

    /* ucVersion:　version [ISODCL (  7,   7)] */
    UCHAR ucVersion;

    /* 
     * ucFlags: flags [ISODCL (  8,   8)] 
     */
    UCHAR ucFlags;

    /* 
     * szSystemId[32]: system_id [ISODCL (  9,  40)] 
     */
    CHAR  szSystemId[32];

    /* szVolumeId[32]: volume_id [ISODCL ( 41,  72)] 名称, 注意[1] */
    CHAR  szVolumeId[32];

    /* aucResv2[8]: unused2 [ISODCL ( 73,  80)] */
    UCHAR aucResv2[8];

    /* 
     * uiVolumeSpace: volume_space_size [ISODCL ( 81,  88)] 
     */
    BISO_DEF_733(uiVolumeSpace)

    /* aucEscape[32]: escape [ISODCL ( 89,  120)] */
    UCHAR aucEscape[32];

    /*
     * usVolumeSet: volume_set_size [ISODCL (121, 124)] 
     */
    BISO_DEF_723(usVolumeSet)

    /* 
     * usVolumeSeq: volume_sequence_number [ISODCL (125, 128)] 
     */
    BISO_DEF_723(usVolumeSeq)
    
    /* 
     * usBlockSize: logical_block_size [ISODCL (129, 132)]
     */
    BISO_DEF_723(usBlockSize)

    BISO_DEF_733(uiPathTblSize)

    UINT32 uiTypeLPathTblLoc;

    UINT32 uiOptTypeLPathTblLoc;

    UINT32 uiTypeMPathTblLoc;
    
    UINT32 uiOptTypeMPathTblLoc;

    BISO_DIR_RECORD_S stRootDirRecord;

    /* szVolumeSetId: volume_set_id [ISODCL (191, 318)]  */
    CHAR szVolumeSetId[128];
    
    /* szPublisherId: publisher_id [ISODCL (319, 446)]  */
    CHAR szPublisherId[128];
    
    /* szPreparerId: preparer_id [ISODCL (447, 574)] */
    CHAR szPreparerId[128];
    
    /* szApplicationId: application_id [ISODCL (575, 702)] */
    CHAR szApplicationId[128];
    
    /* szCopyrightFileId: copyright_file_id [ISODCL (703, 739)] */
    CHAR szCopyrightFileId[37];
    
    /* szAbstractFileId: abstract_file_id [ISODCL (740, 776)]*/
    CHAR szAbstractFileId[37];
    
    /* szBibliographicFileId: bibliographic_file_id [ISODCL (777, 813)] */
    CHAR szBibliographicFileId[37];

    /* szCreationDate: creation_date [ISODCL (814, 830)]*/
    CHAR szCreationDate[17];
    
    /* szModifyDate: modification_date [ISODCL (831, 847)] */
    CHAR szModifyDate[17];
    
    /* szExpirationDate: expiration_date [ISODCL (848, 864)]  */
    CHAR szExpirationDate[17];
    
    /* szEffectiveDate: effective_date [ISODCL (865, 881)]  */
    CHAR szEffectiveDate[17];
    
    /* ucFileStructVer: file_structure_version [ISODCL (882, 882)]  */
    UCHAR ucFileStructVer;
    
    /* ucResv4: unused4 [ISODCL (883, 883)] */
    UCHAR ucResv4;
    
    /* aucApplicationData: application_data [ISODCL (884, 1395)] */
    UCHAR aucApplicationData[512];

    /* ucResv5: unused5 [ISODCL (1396, 2048)] */
    UCHAR aucResv5[653];
}BISO_SVD_S;

typedef struct tagBISO_BOOT_DESC
{
    UCHAR ucType;
    /* 
     * szId[5]: id [ISODCL (  2,   6)] 
     */
    CHAR szId[5];

    /* ucVersion:　version [ISODCL (  7,   7)] */
    UCHAR ucVersion;

    CHAR szBootSystemId[32];

    UCHAR aucResv[32];

    UINT32 uiBootCatlogStart;

    UCHAR aucBootData[1972];
}BISO_BVD_S;

typedef struct tagBISO_EXTATTR_RECORD
{
    BISO_DEF_723(usOwnerId)
    BISO_DEF_723(usGroupId)

    USHORT usPermission;

    CHAR szFileCreateDate[17];
    CHAR szFileModifyDate[17];
    CHAR szFileExpirationDate[17];
    CHAR szFileEffectiveDate[17];

    UCHAR ucRecordFormat;

    UCHAR ucRecordAttr;

    BISO_DEF_723(usRecordLen)

    CHAR szSystemId[32];

    UCHAR aucSystemData[64];

    UCHAR ucRecordVersion;

    UCHAR ucEscapeLen;

    UCHAR aucResv[64];

    BISO_DEF_723(usAppDataLen)
    
    /* UCHAR aucAppData[xxx]; */
    /* UCHAR aucEscape[xxx]; */
}BISO_EXTATTR_RECORD_S;

#pragma pack()

typedef struct tagBISO_VD_NODE
{
    BISO_DLL_NODE_S stNode;
    BISO_VD_S stVD;
}BISO_VD_NODE_S;

typedef struct tagBISO_POSIX_INFO
{
    BOOL_T bHasNMEntry;
    UINT uiPosixFileMode;
    UINT uiPosixFileLink;
    UINT uiPosixFileUserId;
    UINT uiPosixFileGroupId;
    UINT uiPosixFileSNO;
    UINT64 ui64DevNum;
    BISO_DATE_S stCreateTime;
    BISO_DATE_S stModifyTime;
    BISO_DATE_S stLastAccessTime;
    BISO_DATE_S stLastAttrChangeTime;
    BISO_DATE_S stLastBackupTime;
    BISO_DATE_S stExpirationTime;
    BISO_DATE_S stEffectiveTime;

    UINT  uiLinkLen;
    CHAR *pcLinkSrc;
}BISO_POSIX_INFO_S;

typedef struct tagBISO_SVD_DIR_TREE
{
    struct tagBISO_SVD_DIR_TREE *pstParent;
    struct tagBISO_SVD_DIR_TREE *pstNext;
    struct tagBISO_SVD_DIR_TREE *pstChild;

    UINT uiExtent;
    UINT uiSize;

    UINT64 ui64FileRecordOffset;
    struct tagBISO_SVD_DIR_TREE *pstFileList;
}BISO_SVD_DIR_TREE_S;

typedef struct tagBISO_DIR_TREE
{
    struct tagBISO_DIR_TREE *pstParent;
    struct tagBISO_DIR_TREE *pstNext;
    struct tagBISO_DIR_TREE *pstChild;

    USHORT usNameLen;
    CHAR szName[300];
    UINT uiPathTblId;
    UINT uiExtent;
    UINT uiSize;

    UINT64 ui64FileRecordOffset;

    BISO_DIR_STAT_S *pstDirStat;

    BISO_POSIX_INFO_S *pstPosixInfo;

    struct tagBISO_DIR_TREE *pstFileList;
}BISO_DIR_TREE_S;

#define BISO_DIR_TREE_IS_SYMLINK(pstDirTree) \
    (((NULL != pstDirTree->pstPosixInfo) && \
      (NULL != pstDirTree->pstPosixInfo->pcLinkSrc)) ? BOOL_TRUE : BOOL_FALSE)

typedef struct tagBISO_PARSER
{
    CHAR szFileName[1024];

    BISO_PVD_S *pstPVD;
    BISO_SVD_S *pstSVD;
    BISO_BVD_S *pstBVD;
    BISO_DLL_S stVDList;

    UCHAR *pucPathTable;

    UINT uiElToritoLen;
    UCHAR *pucElToritoEntry;

    UCHAR ucRRIPVersion;
    UCHAR ucRRIPSkipLen;

    BISO_DIR_TREE_S stDirTree;
    
    BISO_SVD_DIR_TREE_S stSVDDirTree;
}BISO_PARSER_S;

ULONG BISO_9660_OpenImage
(
    IN BOOL_T bParseSVDDirTree,
    IN CONST CHAR *pcFileName, 
    OUT BISO_PARSER_S *pstParser
);
BISO_PARSER_S * BISO_9660_CreateParser(VOID);
VOID BISO_9660_DestroyParser(INOUT BISO_PARSER_S *pstParser);
ULONG BISO_9660_ParseDate84261
(
    IN CONST CHAR *pcDate,
    OUT BISO_DATE_S *pstDate
);
VOID BISO_9660_FmtDate84261(IN time_t ulTime, IN UINT uiBufSize, OUT CHAR *pcDate);
VOID BISO_9660_FillDfsStack
(
    IN BISO_DIR_TREE_S *pstTop, 
    INOUT BISO_QUEUE_S *pstQueue
);
ULONG BISO_9660_ExtractFile
(
    IN BISO_FILE_S *pstFile, 
    IN CONST CHAR  *pcDstFullPath,
    IN CONST BISO_DIR_TREE_S *pstFileNode,
    IN CONST BISO_EXTRACT_CTRL_S *pstCtrl
);

/* 拼接路径 */
#define BISO_EXTRACE_CAT_PATH(szFullPath, uiCurPos, szName, NameLen) \
{\
    memcpy(szFullPath + uiCurPos, szName, NameLen); \
    uiCurPos += NameLen; \
    szFullPath[uiCurPos++] = '/'; \
    szFullPath[uiCurPos] = 0; \
}

/* 检查结果返回 */
#define BISO_9660_CHECK_RET(ulRet, pstFile) \
    if (BISO_SUCCESS != ulRet) \
    { \
        BISO_PLAT_CloseFile(pstFile); \
        return ulRet; \
    }

BISO_DIR_TREE_S * BISO_UTIL_FindLinkTgt(IN BISO_DIR_TREE_S *pstCurNode);

#endif /* __BISO_9660_H__ */

