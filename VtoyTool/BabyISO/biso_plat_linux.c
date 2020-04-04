/******************************************************************************
 * biso_plat_linux.c
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
 

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <unistd.h>

#include "biso.h"
#include "biso_list.h"
#include "biso_util.h"
#include "biso_plat.h"

UINT64 vtoydm_get_file_size(const char *pcFileName);
BISO_FILE_S * vtoydm_open_file(const char *pcFileName);
void vtoydm_close_file(BISO_FILE_S *pstFile);
INT64 vtoydm_seek_file(BISO_FILE_S *pstFile, INT64 i64Offset, INT iFromWhere);
UINT64 vtoydm_read_file
(
    BISO_FILE_S *pstFile, 
    UINT         uiBlkSize, 
    UINT         uiBlkNum, 
    VOID        *pBuf
);


UINT64 BISO_PLAT_GetFileSize(IN CONST CHAR *pcFileName)
{
    return vtoydm_get_file_size(pcFileName);  
}

BISO_FILE_S * BISO_PLAT_OpenExistFile(IN CONST CHAR *pcFileName)
{
    return vtoydm_open_file(pcFileName);  
}

VOID BISO_PLAT_CloseFile(IN BISO_FILE_S *pstFile)
{
    return vtoydm_close_file(pstFile);
}

INT64 BISO_PLAT_SeekFile(BISO_FILE_S *pstFile, INT64 i64Offset, INT iFromWhere)
{
    return vtoydm_seek_file(pstFile, i64Offset, iFromWhere);
}

UINT64 BISO_PLAT_ReadFile
(
    IN  BISO_FILE_S *pstFile, 
    IN  UINT         uiBlkSize, 
    IN  UINT         uiBlkNum, 
    OUT VOID        *pBuf
)
{
    return vtoydm_read_file(pstFile, uiBlkSize, uiBlkNum, pBuf);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

