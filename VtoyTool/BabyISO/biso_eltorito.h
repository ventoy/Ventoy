/******************************************************************************
 * bios_eltorito.h
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
 * EL TORITO扩展规范相关定义
 * 注意点:
 *   [1] EL TORITO扩展规范里定义的数据都是小字节序的 
 */

#ifndef __BISO_ELTORITO_H__
#define __BISO_ELTORITO_H__

/* 
 * EL TORITO规范里对于Boot Record里面BootCatlog指向的扩展区域做了
 * 结构定义，分成一条条的表项，每一条表项固定32个字节长,1个扇区
 * 可以保存64条表项。BootCatlog可以占用多个扇区。
 * 表项必须按照如下顺序保存:
 *   ID   Entry
 *    0   Validation Entry
 *    1   Initial Entry
 *    2   Section Header
 *    3   Section Entry
 *    4   Section Extension Entry 1
 *    5   Section Extension Entry 2
 *    6   Section Extension Entry 3
 *        ....
 *    N   Section Header
 *    N+1 Section Entry
 *    N+2 Section Extension Entry 1
 *    N+3 Section Extension Entry 2
 *    N+4 Section Extension Entry 3
 *        ....
 */

#define  BISO_ELTORITO_ENTRY_LEN    32

#pragma pack(1)

/* 检验表项, 必须是第一条 */
typedef struct tagBISO_TORITO_VALIDATION_ENTRY
{
    UCHAR ucHeaderID;  /* Must be 01 */

    /* 
     * PlatID: CPU平台架构
     * 0x00: x86
     * 0x01: PowerPC
     * 0x02: Mac
     * 0xEF: EFI System Partition
     */
    UCHAR ucPlatID;

    UINT16 usResv;

    /* ID，一般用来保存CD-ROM的制造商信息 */
    CHAR szID[24];

    /* 
     * 校验补充字段, 注意[1]
     * 这个字段用来保证整个Validation Entry的数据
     * 按照WORD(双字节)累加起来为0(截取之后) 
     */
    UINT16 usCheckSum;

    /* 魔数字，检验使用，值必须为0x55和0xAA */
    UCHAR ucData55;
    UCHAR ucDataAA;
}BISO_TORITO_VALIDATION_ENTRY_S;

/* 默认初始化表项(BIOS里面的 INT 13) */
typedef struct tagBISO_TORITO_INITIAL_ENTRY
{
    /* BOOTID: 0x88:Bootable, 00:Not Bootable */
    UCHAR ucBootId;

    /* 
     * ucBootMedia:
     * Bit0 - Bit3的值:
     *  0: No Emulation
     *  1: 1.2 meg diskette
     *  2: 1.44 meg diskette
     *  3: 2.88 meg diskette
     *  4: Hard Disk(drive 80)
     *  5-F:保留
     * Bit4 - Bit7:保留
     */
    UCHAR ucBootMedia;

    /* 启动段，只对x86构架有效，为0默认使用7C0, 注意[1] */
    UINT16 usLoadSegment;

    /* 是Boot Image里面Partition Table的第5个字节(System Type) */
    UCHAR ucSystemType;

    UCHAR ucResv;

    /* 启动时每次往内存读多长, 注意[1] */
    UINT16 usSectorCount;

    /* 启动文件所在的起始逻辑块编号，例如isolinux.bin文件的位置, 注意[1] */
    UINT32 uiLoadRBA;

    UCHAR aucResv[20];
}BISO_TORITO_INITIAL_ENTRY_S;

/*
 * Section Header, 补充一系列可启动的Entry(比如UEFI启动)
 * 如果默认的Initial/Default Entry不满足BIOS需求
 * BIOS可以继续往下找，根据Section Header里面的ID
 * 以及Section Entry里面的Criteria信息决定是否从
 * 该条Entry启动
 *
 */
typedef struct tagBISO_TORITO_SECHDR_ENTRY
{
    /* ucFlag: 0x90:表示后面还有Header, 0x91:表示最后一个Header */
    UCHAR ucFlag;

    /* 
     * PlatID: CPU平台架构
     * 0x00: x86
     * 0x01: PowerPC
     * 0x02: Mac
     * 0xEF: EFI System Partition
     */
    UCHAR ucPlatID;

    /* 跟在这个头的后面有多少个Section Entry */
    UINT16 usSecEntryNum;

    /* ID信息 */
    CHAR szId[28];
}BISO_TORITO_SECHDR_ENTRY_S;

/* Section Entry */
typedef struct tagBISO_TORITO_SECTION_ENTRY
{
    /* BOOTID: 88:Bootable, 00:Not Bootable */
    UCHAR ucBootId;
    
    /* 
     * ucBootMedia:
     * Bit0 - Bit3的值:
     *  0: No Emulation
     *  1: 1.2 meg diskette
     *  2: 1.44 meg diskette
     *  3: 2.88 meg diskette
     *  4: Hard Disk(drive 80)
     *  5-F:保留
     * Bit4:保留
     * Bit5:Continuation Entry Follows
     * Bit6:Image contains an ATAPI driver
     * Bit7:Image contains SCSI driver
     */
    UCHAR ucFlag;

    /* 启动段，只对x86构架有效，为0默认使用7C0, 注意[1] */
    UINT16 usLoadSegment;

    /* 是Boot Image里面Partition Table的第5个字节(System Type) */
    UCHAR ucSystemType;

    UCHAR ucResv;

    /* 启动时每次往内存读多长, 注意[1] */
    UINT16 usSectorCount;

    /* 启动文件所在的起始逻辑块编号，例如isolinux.bin文件的位置, 注意[1] */
    UINT32 uiLoadRBA;

    /* 
     * 它的值描述了后面的aucCriteria的格式
     * 0: 没有
     * 1: Language and Version Information (IBM)
     * 2-FF - Reserved
     */
    UCHAR ucCriteriaType;

    /* Criteria信息，如果这19个字节不够用，可以使用Section Extension Entry里的 */
    UCHAR aucCriteria[19];
}BISO_TORITO_SECTION_ENTRY_S;

/* 扩展Section Entry */
typedef struct tagBISO_TORITO_SECEXT_ENTRY
{
    /* ucExtId: 必须为44 */
    UCHAR ucExtId;

    /* 
     * ucFlag: 只有Bit5有用
     * Bit5: 1表示后面还有Extension Record  0表示最后一个
     */
    UCHAR ucFlag;

    /* Criteria信息 */
    UCHAR aucCriteria[39];
}BISO_TORITO_SECEXT_ENTRY_S;

#pragma pack()

/*
 * 当前缓冲区指针位置向前1条Entry 
 * 如果已经超过了2048字节，则继续从文件中读
 */
#define BISO_ELTORITO_ENTRY_STEP(pstSection, pstFile, aucBuf, stMBuf) \
{\
    UINT _uiReadLen;\
    pstSection++;\
    if ((UCHAR *)pstSection >= aucBuf + BISO_SECTOR_SIZE)\
    {\
        (VOID)BISO_MBUF_Append(&stMBuf, BISO_SECTOR_SIZE, aucBuf);\
        _uiReadLen = (UINT)BISO_PLAT_ReadFile(pstFile, 1, BISO_SECTOR_SIZE, aucBuf);\
        if (_uiReadLen != BISO_SECTOR_SIZE)\
        {\
            BISO_DIAG("Read Len %u, sector len %u.", _uiReadLen, BISO_SECTOR_SIZE);\
            BISO_MBUF_Free(&stMBuf);\
            return BISO_ERR_READ_FILE;\
        }\
        pstSection = (BISO_TORITO_SECTION_ENTRY_S *)aucBuf;\
    }\
}

ULONG BISO_ELTORITO_ReadBootInfo(IN BISO_FILE_S *pstFile, OUT BISO_PARSER_S *pstParser);
VOID BISO_ELTORITO_Dump(IN CONST BISO_PARSER_S *pstParser);
UINT BISO_ELTORITO_GetBootEntryNum(IN CONST BISO_PARSER_S *pstParser);

#endif /* __BISO_ELTORITO_H__ */

