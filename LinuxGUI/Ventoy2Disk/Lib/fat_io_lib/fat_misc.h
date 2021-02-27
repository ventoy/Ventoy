#ifndef __FAT_MISC_H__
#define __FAT_MISC_H__

#include "fat_defs.h"
#include "fat_opts.h"

//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------
#define MAX_LONGFILENAME_ENTRIES    20
#define MAX_LFN_ENTRY_LENGTH        13

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------
#define GET_32BIT_WORD(buffer, location)    ( ((uint32)buffer[location+3]<<24) + ((uint32)buffer[location+2]<<16) + ((uint32)buffer[location+1]<<8) + (uint32)buffer[location+0] )
#define GET_16BIT_WORD(buffer, location)    ( ((uint16)buffer[location+1]<<8) + (uint16)buffer[location+0] )

#define SET_32BIT_WORD(buffer, location, value)    { buffer[location+0] = (uint8)((value)&0xFF); \
                                                  buffer[location+1] = (uint8)((value>>8)&0xFF); \
                                                  buffer[location+2] = (uint8)((value>>16)&0xFF); \
                                                  buffer[location+3] = (uint8)((value>>24)&0xFF); }

#define SET_16BIT_WORD(buffer, location, value)    { buffer[location+0] = (uint8)((value)&0xFF); \
                                                  buffer[location+1] = (uint8)((value>>8)&0xFF); }

//-----------------------------------------------------------------------------
// Structures
//-----------------------------------------------------------------------------
struct lfn_cache
{
#if FATFS_INC_LFN_SUPPORT
    // Long File Name Structure (max 260 LFN length)
    uint8 String[MAX_LONGFILENAME_ENTRIES][MAX_LFN_ENTRY_LENGTH];
    uint8 Null;
#endif
    uint8 no_of_strings;
};

//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
void    fatfs_lfn_cache_init(struct lfn_cache *lfn, int wipeTable);
void    fatfs_lfn_cache_entry(struct lfn_cache *lfn, uint8 *entryBuffer);
char*   fatfs_lfn_cache_get(struct lfn_cache *lfn);
int     fatfs_entry_lfn_text(struct fat_dir_entry *entry);
int     fatfs_entry_lfn_invalid(struct fat_dir_entry *entry);
int     fatfs_entry_lfn_exists(struct lfn_cache *lfn, struct fat_dir_entry *entry);
int     fatfs_entry_sfn_only(struct fat_dir_entry *entry);
int     fatfs_entry_is_dir(struct fat_dir_entry *entry);
int     fatfs_entry_is_file(struct fat_dir_entry *entry);
int     fatfs_lfn_entries_required(char *filename);
void    fatfs_filename_to_lfn(char *filename, uint8 *buffer, int entry, uint8 sfnChk);
void    fatfs_sfn_create_entry(char *shortfilename, uint32 size, uint32 startCluster, struct fat_dir_entry *entry, int dir);
int     fatfs_lfn_create_sfn(char *sfn_output, char *filename);
int     fatfs_lfn_generate_tail(char *sfn_output, char *sfn_input, uint32 tailNum);
void    fatfs_convert_from_fat_time(uint16 fat_time, int *hours, int *minutes, int *seconds);
void    fatfs_convert_from_fat_date(uint16 fat_date, int *day, int *month, int *year);
uint16  fatfs_convert_to_fat_time(int hours, int minutes, int seconds);
uint16  fatfs_convert_to_fat_date(int day, int month, int year);
void    fatfs_print_sector(uint32 sector, uint8 *data);

#endif
