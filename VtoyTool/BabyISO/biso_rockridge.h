/******************************************************************************
 * biso_rockridge.h
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
 


/*
 * 本文件中定义了SUSP(ystem Use Sharing Protocol)协议(IEEE P1281)
 * 以及Rock Ridge 扩展中定义的各种表项结构(IEEE P1282)
 * 其中SUSP是规定了ISO-9660中Directory Record中System Use字段的使用方法
 * Rock Ridge是基于SUSP，在System Use字段中扩展记录了文件的属性、名称等POSIX文件信息
 * 详细的说明请参考标准文档(IEEE P1281、IEEE P1282)
 * SUSP定义的每一条Entry都是不定长的，其长度在结构里面描述
 */
 
#ifndef __BISO_ROCKRIDGE_H__
#define __BISO_ROCKRIDGE_H__

#ifndef S_IFLNK
#define  S_IFLNK 0120000 
#define  S_IFREG 0100000
#define  S_IFDIR 0040000 
#endif

typedef VOID (* BISO_DUMP_ENTRY_PF)(IN VOID *pEntry);

typedef struct tagBISO_DUMP_ENTRY_CB
{
    CHAR szSignature[3];
    BISO_DUMP_ENTRY_PF pfDump;
}BISO_DUMP_ENTRY_CB_S;

typedef VOID (* BISO_RRIP_PARSE_ENTRY_PF)
(
    IN VOID *pEntry,
    OUT BISO_DIR_TREE_S *pstDirTree
);

typedef struct tagBISO_RRIP_PARSE_ENTRY_CB
{
    CHAR szSignature[3];
    BISO_RRIP_PARSE_ENTRY_PF pfFunc;
}BISO_RRIP_PARSE_ENTRY_CB_S;


#pragma pack(1)

/* SUSP: System Use Sharing Protocol */

/* SUSP中定义的标准Entry结构 */
typedef struct tagBISO_SUSP_ENTRY
{
    /* 两个控制字 */
    CHAR cSignature1;
    CHAR cSignature2;

    /* 长度，包括控制字 */
    UCHAR ucEntryLen;

    /* 版本号 */
    UCHAR ucVersion;

    /* 数据，具体长度有ucEntryLen确定 */
    UCHAR aucData[1];        
}BISO_SUSP_ENTRY_S;

/* 
 * Continuation Area(可选)
 * !!!!!!!!!!!!!!!!!
 * !!!!!!!!!!!!!!!!!
 * !!!!!!!!!!!!!!!!!
 * 之所以有CE这个控制字是因为ISO-9660标准定义的Directory Record
 * 的长度是用1个字节表示的.最大就255 Byte，所以System Use字段的长度也就
 * 不可能大于255，而有些扩展信息可能需要很多个Entry才能描述，255的长度不够
 * 就需要扩展，所以才有了这个用于扩展的Entry格式
 * 每个Directory Record的System Use字段或者扩展区域中只能最多有1个CE表项，
 * 而且一般也应该是最后1条表项，不过整个System Use区域的CE表项个数的没有限制的。
 */
typedef struct tagBISO_SUSP_ENTRY_CE
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'C' */
    CHAR  cSignature2;  /* 必须为 'E' */

    /* 长度，包括控制字, 必须为28 */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;

    BISO_DEF_733(uiBlockLoc)
    BISO_DEF_733(uiByteOffset)
    BISO_DEF_733(uiContinuationLen)
}BISO_SUSP_ENTRY_CE_S;

/* Padding Filed(可选) */
typedef struct tagBISO_SUSP_ENTRY_PD
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'P' */
    CHAR  cSignature2;  /* 必须为 'D' */

    /* 长度，包括控制字 */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;

    /* 数据，具体长度有ucEntryLen确定 */
    UCHAR aucData[1];     
}BISO_SUSP_ENTRY_PD_S;

/*
 * System Use Sharing Protocol Indicator(必须存在)
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * SP必须是在Root Directory Record的System Use里面从1个字节开始的第1条表项
 * 另外，SP表项还有一个作用就是它里面的Skip Len字段，它的意思是
 * 在除了ROOT之外的所有Directory Record中，System Use字段不一定是从第1个字节开始就是
 * SUSP的各种表项，可以统一跳过一定的偏移(为了兼容性考虑),就是这里的Skip Len
 * 注意这个Skip Len对ROOT根目录无效，对于ROOT永远是从SYSTEM Use的第1个字节开始是SP Entry
 */
typedef struct tagBISO_SUSP_ENTRY_SP
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'S' */
    CHAR  cSignature2;  /* 必须为 'P' */

    /* 长度，包括控制字, 必须为7 */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;

    /* 检查字段 */
    UCHAR ucChkBE;     
    UCHAR ucChkEF;     
    UCHAR ucSkipLen;
}BISO_SUSP_ENTRY_SP_S;

/* System Use Sharing Protocol Termonitor: System Use或者CE区域最后一条表项 */
typedef struct tagBISO_SUSP_ENTRY_ST
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'S' */
    CHAR  cSignature2;  /* 必须为 'T' */

    /* 长度，包括控制字, 必须为4 */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;
}BISO_SUSP_ENTRY_ST_S;

/* Extensions Reference */
typedef struct tagBISO_SUSP_ENTRY_ER
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'E' */
    CHAR  cSignature2;  /* 必须为 'R' */

    /* 长度，包括控制字 */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;

    UCHAR ucIdLen;

    UCHAR ucDescLen;

    UCHAR ucSrcLen;

    UCHAR ucExtVer;

    /* 后面跟着数据 */
    /* UCHAR aucId[ucIdLen]; */
    /* UCHAR aucDesc[ucDescLen]; */
    /* UCHAR aucSrc[ucSrcLen]; */
}BISO_SUSP_ENTRY_ER_S;

/* Extension Selector */
typedef struct tagBISO_SUSP_ENTRY_ES
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'E' */
    CHAR  cSignature2;  /* 必须为 'S' */

    /* 长度，包括控制字, 必须为5 */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;

    /* 扩展编号 */
    UCHAR ucExtSeq;
}BISO_SUSP_ENTRY_ES_S;

/* Rock Ridge In Use RRIP 1991A版本里定义的结构, 新版本已经废弃 */
typedef struct tagBISO_ROCK_RIDGE_ENTRY_RR
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'R' */
    CHAR  cSignature2;  /* 必须为 'R' */

    /* 长度，包括控制字, 必须为5 */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;

    /*
     * ucFlags, 各Bit含义如下:
     * Bit0: "PX" System Use Field recorded
     * Bit1: "PN" System Use Field recorded
     * Bit2: "SL" System Use Field recorded
     * Bit3: "NM" System Use Field recorded
     * Bit4: "CL" System Use Field recorded
     * Bit5: "PL" System Use Field recorded
     * Bit6: "RE" System Use Field recorded
     * Bit7: "TF" System Use Field recorded
     */
    UCHAR ucFlags;
}BISO_ROCK_RIDGE_ENTRY_RR_S;


/* POSIX file attributes */
typedef struct tagBISO_ROCK_RIDGE_ENTRY_PX
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'P' */
    CHAR  cSignature2;  /* 必须为 'X' */

    /* 长度，包括控制字, 必须为44
     * !!!!!!!!!!!!!!!!!!!!!!!!!! 
     * !!!!!!!!!!!!!!!!!!!!!!!!!! 
     * !!!!!!!!!!!!!!!!!!!!!!!!!! 
     * !!!!!!!!!!!!!!!!!!!!!!!!!! 
     * IEEE P1282 V1.10之前是36不是44
     */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;

    /* File Mode的比特位图, 就是linux里面stat结构里面的st_mode字段 */
    BISO_DEF_733(uiPosixFileMode)

    /* st_nlink字段 */
    BISO_DEF_733(uiPosixFileLink)
    
    /* st_uid字段 */
    BISO_DEF_733(uiPosixFileUserId)
    
    /* st_gid字段 */
    BISO_DEF_733(uiPosixFileGroupId)
    
    /* st_ino字段, 注意这个字段在IEEE P1282 V1.10之前是没有的 */
    BISO_DEF_733(uiPosixFileSNO)
}BISO_ROCK_RIDGE_ENTRY_PX_S;

/* POSIX device number */
typedef struct tagBISO_ROCK_RIDGE_ENTRY_PN
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'P' */
    CHAR  cSignature2;  /* 必须为 'N' */

    /* 长度，包括控制字, 必须为20 */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;

    /* Device Number的高低32位 */
    BISO_DEF_733(uiDevNumHigh)
    BISO_DEF_733(uiDevNumLow)
}BISO_ROCK_RIDGE_ENTRY_PN_S;

/* Symbol Link */
typedef struct tagBISO_ROCK_RIDGE_ENTRY_SL
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'S' */
    CHAR  cSignature2;  /* 必须为 'L' */

    /* 长度，包括控制字, 必须为5+Componet长度 */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;

    /* 
     * 0:最后一个软链接 
     * 1:后面还有
     */
    UCHAR ucFlags;

    /* Componet内容，有具体的格式定义ISO_RRIP_SL_COMPONENT_S */
    UCHAR aucComponet[1];    
}BISO_ROCK_RIDGE_ENTRY_SL_S;

typedef struct tagBISO_RRIP_SL_COMPONENT
{
    /* ucFlags 
     * Bit0: Continue 
     * Bit1: Current当前目录 '.'
     * Bit2: Parent父目录 '..'
     * Bit3: Root 目录 '/'
     * 其他位保留
     */
    UCHAR ucFlags;

    /* 长度，不包括ucFlags和自己，纯粹是后面aucData的长度 */
    UCHAR ucLength;

    UCHAR aucData[1];
}BISO_RRIP_SL_COMPONENT_S;

#define BISO_SLCOMP_IS_CONTINUE(ucFlag) (((ucFlag >> 0) & 0x1) > 0 ? BOOL_TRUE : BOOL_FALSE)
#define BISO_SLCOMP_IS_CURRENT(ucFlag)  (((ucFlag >> 1) & 0x1) > 0 ? BOOL_TRUE : BOOL_FALSE)
#define BISO_SLCOMP_IS_PARENT(ucFlag)   (((ucFlag >> 2) & 0x1) > 0 ? BOOL_TRUE : BOOL_FALSE)
#define BISO_SLCOMP_IS_ROOT(ucFlag)     (((ucFlag >> 3) & 0x1) > 0 ? BOOL_TRUE : BOOL_FALSE)

/*
 * Alternate Name 用来记录POSIX文件名
 * 注意NM表项可以有很多个，一直到ucFlags的比特0为0
 * 多个NM表项里面的szFileName部分要拼接在一起表示整个完整的文件名
 * 这个应该主要是为了解决长文件名的问题
 */
typedef struct tagBISO_ROCK_RIDGE_ENTRY_NM
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'N' */
    CHAR  cSignature2;  /* 必须为 'M' */

    /* 长度，包括控制字, 必须为5+Name长度 */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;

    /* 
     * ucFlags 
     * Bit0: Continue 
     * Bit1: Current当前目录 '.'
     * Bit2: Parent父目录 '..'
     * 其他位保留
     */
    UCHAR ucFlags;

    /* 后面跟着具体的名称，如果Flag的Bit1 2 5置为则后面就没有了 */
    CHAR szFileName[1];
}BISO_ROCK_RIDGE_ENTRY_NM_S;


/* 后面的CL PL RE用来扩展超过8级目录的情况 */

/* Child Link */
typedef struct tagBISO_ROCK_RIDGE_ENTRY_CL
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'C' */
    CHAR  cSignature2;  /* 必须为 'L' */

    /* 长度，包括控制字, 必须为12 */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;

    BISO_DEF_733(uiChildDirLoc)
}BISO_ROCK_RIDGE_ENTRY_CL_S;

/* Parent Link */
typedef struct tagBISO_ROCK_RIDGE_ENTRY_PL
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'P' */
    CHAR  cSignature2;  /* 必须为 'L' */

    /* 长度，包括控制字, 必须为12 */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;

    BISO_DEF_733(uiParentDirLoc)
}BISO_ROCK_RIDGE_ENTRY_PL_S;

/* Relocated Directory */
typedef struct tagBISO_ROCK_RIDGE_ENTRY_RE
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'R' */
    CHAR  cSignature2;  /* 必须为 'E' */

    /* 长度，包括控制字, 必须为4 */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;
}BISO_ROCK_RIDGE_ENTRY_RE_S;

/* Time Stamps For File */
typedef struct tagBISO_ROCK_RIDGE_ENTRY_TF
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'T' */
    CHAR  cSignature2;  /* 必须为 'F' */

    /* 长度，包括控制字  */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;

    /*
     * ucFlags:
     * Bit0: Create Time  是否记录
     * Bit1: Modify Time  是否记录
     * Bit2: Last Access Time  是否记录
     * Bit3: Last Attribute Change Time  是否记录
     * Bit4: Last Backup Time  是否记录
     * Bit5: Expiration Time   是否记录
     * Bit6: Effective Time    是否记录
     * Bit7: Long-Form 时间格式 
     *       0 表示7字节数组格式(ECMA-119 9.1.5)  
     *       1 表示17字符串格式(ECMA-119 8.4.26.1)
     */
    UCHAR ucFlags;

    /* 具体的每一个时间戳，每个时间戳7字节或者17字节，取决于ucFlags的Bit7 */
    UCHAR aucTimeStamp[1];
}BISO_ROCK_RIDGE_ENTRY_TF_S;

/* File Data in sparse format 稀疏文件 */
typedef struct tagBISO_ROCK_RIDGE_ENTRY_SF
{
    /* 两个控制字 */
    CHAR  cSignature1;  /* 必须为 'S' */
    CHAR  cSignature2;  /* 必须为 'F' */

    /* 长度，包括控制字, 必须为21 */
    UCHAR ucEntryLen;

    /* 版本号， 必须为1 */
    UCHAR ucVersion;

    BISO_DEF_733(uiVirFileSizeHigh)
    BISO_DEF_733(uiVirFileSizeLow)

    /*
     * ucDepth: 
     * 1-->64KB
     * 2-->16MB
     * 3-->4GB
     * 4-->1TB
     * 5-->256TB
     * 6-->64K TB
     * 7-->16M TB
     */
    UCHAR ucDepth;
}BISO_ROCK_RIDGE_ENTRY_SF_S;

#pragma pack()

ULONG BISO_RRIP_ReadIndicator(INOUT BISO_PARSER_S *pstParser);
ULONG BISO_RRIP_ReadExtInfo
(
    IN  BISO_FILE_S       *pstFile,
    IN  BISO_PARSER_S     *pstParser,
    IN  BISO_DIR_RECORD_S *pstRecord, 
    OUT BISO_DIR_TREE_S   *pstDirTree 
);
VOID BISO_RRIP_GetPXInfo(IN VOID *pEntry, OUT BISO_DIR_TREE_S *pstDirTree);
VOID BISO_RRIP_GetNMInfo(IN VOID *pEntry, OUT BISO_DIR_TREE_S *pstDirTree);
VOID BISO_RRIP_GetTFInfo(IN VOID *pEntry, OUT BISO_DIR_TREE_S *pstDirTree);
VOID BISO_RRIP_GetSLInfo(IN VOID *pEntry, OUT BISO_DIR_TREE_S *pstDirTree);
VOID BISO_RRIP_GetPNInfo(IN VOID *pEntry, OUT BISO_DIR_TREE_S *pstDirTree);

#endif /* __BISO_ROCKRIDGE_H__ */

