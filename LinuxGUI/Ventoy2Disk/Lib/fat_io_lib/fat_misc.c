//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//                            FAT16/32 File IO Library
//                                    V2.6
//                              Ultra-Embedded.com
//                            Copyright 2003 - 2012
//
//                         Email: admin@ultra-embedded.com
//
//                                License: GPL
//   If you would like a version with a more permissive license for use in
//   closed source commercial applications please contact me for details.
//-----------------------------------------------------------------------------
//
// This file is part of FAT File IO Library.
//
// FAT File IO Library is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// FAT File IO Library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with FAT File IO Library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#include <stdlib.h>
#include <string.h>
#include "fat_misc.h"

//-----------------------------------------------------------------------------
// fatfs_lfn_cache_init: Clear long file name cache
//-----------------------------------------------------------------------------
void fatfs_lfn_cache_init(struct lfn_cache *lfn, int wipeTable)
{
    int i = 0;

    lfn->no_of_strings = 0;

#if FATFS_INC_LFN_SUPPORT

    // Zero out buffer also
    if (wipeTable)
        for (i=0;i<MAX_LONGFILENAME_ENTRIES;i++)
            memset(lfn->String[i], 0x00, MAX_LFN_ENTRY_LENGTH);
#endif
}
//-----------------------------------------------------------------------------
// fatfs_lfn_cache_entry - Function extracts long file name text from sector
// at a specific offset
//-----------------------------------------------------------------------------
#if FATFS_INC_LFN_SUPPORT
void fatfs_lfn_cache_entry(struct lfn_cache *lfn, uint8 *entryBuffer)
{
    uint8 LFNIndex, i;
    LFNIndex = entryBuffer[0] & 0x1F;

    // Limit file name to cache size!
    if (LFNIndex > MAX_LONGFILENAME_ENTRIES)
        return ;

    // This is an error condition
    if (LFNIndex == 0)
        return ;

    if (lfn->no_of_strings == 0)
        lfn->no_of_strings = LFNIndex;

    lfn->String[LFNIndex-1][0] = entryBuffer[1];
    lfn->String[LFNIndex-1][1] = entryBuffer[3];
    lfn->String[LFNIndex-1][2] = entryBuffer[5];
    lfn->String[LFNIndex-1][3] = entryBuffer[7];
    lfn->String[LFNIndex-1][4] = entryBuffer[9];
    lfn->String[LFNIndex-1][5] = entryBuffer[0x0E];
    lfn->String[LFNIndex-1][6] = entryBuffer[0x10];
    lfn->String[LFNIndex-1][7] = entryBuffer[0x12];
    lfn->String[LFNIndex-1][8] = entryBuffer[0x14];
    lfn->String[LFNIndex-1][9] = entryBuffer[0x16];
    lfn->String[LFNIndex-1][10] = entryBuffer[0x18];
    lfn->String[LFNIndex-1][11] = entryBuffer[0x1C];
    lfn->String[LFNIndex-1][12] = entryBuffer[0x1E];

    for (i=0; i<MAX_LFN_ENTRY_LENGTH; i++)
        if (lfn->String[LFNIndex-1][i]==0xFF)
            lfn->String[LFNIndex-1][i] = 0x20; // Replace with spaces
}
#endif
//-----------------------------------------------------------------------------
// fatfs_lfn_cache_get: Get a reference to the long filename
//-----------------------------------------------------------------------------
#if FATFS_INC_LFN_SUPPORT
char* fatfs_lfn_cache_get(struct lfn_cache *lfn)
{
    // Null terminate long filename
    if (lfn->no_of_strings == MAX_LONGFILENAME_ENTRIES)
        lfn->Null = '\0';
    else if (lfn->no_of_strings)
        lfn->String[lfn->no_of_strings][0] = '\0';
    else
        lfn->String[0][0] = '\0';

    return (char*)&lfn->String[0][0];
}
#endif
//-----------------------------------------------------------------------------
// fatfs_entry_lfn_text: If LFN text entry found
//-----------------------------------------------------------------------------
#if FATFS_INC_LFN_SUPPORT
int fatfs_entry_lfn_text(struct fat_dir_entry *entry)
{
    if ((entry->Attr & FILE_ATTR_LFN_TEXT) == FILE_ATTR_LFN_TEXT)
        return 1;
    else
        return 0;
}
#endif
//-----------------------------------------------------------------------------
// fatfs_entry_lfn_invalid: If SFN found not relating to LFN
//-----------------------------------------------------------------------------
#if FATFS_INC_LFN_SUPPORT
int fatfs_entry_lfn_invalid(struct fat_dir_entry *entry)
{
    if ( (entry->Name[0]==FILE_HEADER_BLANK)  ||
         (entry->Name[0]==FILE_HEADER_DELETED)||
         (entry->Attr==FILE_ATTR_VOLUME_ID) ||
         (entry->Attr & FILE_ATTR_SYSHID) )
        return 1;
    else
        return 0;
}
#endif
//-----------------------------------------------------------------------------
// fatfs_entry_lfn_exists: If LFN exists and correlation SFN found
//-----------------------------------------------------------------------------
#if FATFS_INC_LFN_SUPPORT
int fatfs_entry_lfn_exists(struct lfn_cache *lfn, struct fat_dir_entry *entry)
{
    if ( (entry->Attr!=FILE_ATTR_LFN_TEXT) &&
         (entry->Name[0]!=FILE_HEADER_BLANK) &&
         (entry->Name[0]!=FILE_HEADER_DELETED) &&
         (entry->Attr!=FILE_ATTR_VOLUME_ID) &&
         (!(entry->Attr&FILE_ATTR_SYSHID)) &&
         (lfn->no_of_strings) )
        return 1;
    else
        return 0;
}
#endif
//-----------------------------------------------------------------------------
// fatfs_entry_sfn_only: If SFN only exists
//-----------------------------------------------------------------------------
int fatfs_entry_sfn_only(struct fat_dir_entry *entry)
{
    if ( (entry->Attr!=FILE_ATTR_LFN_TEXT) &&
         (entry->Name[0]!=FILE_HEADER_BLANK) &&
         (entry->Name[0]!=FILE_HEADER_DELETED) &&
         (entry->Attr!=FILE_ATTR_VOLUME_ID) &&
         (!(entry->Attr&FILE_ATTR_SYSHID)) )
        return 1;
    else
        return 0;
}
// TODO: FILE_ATTR_SYSHID ?!?!??!
//-----------------------------------------------------------------------------
// fatfs_entry_is_dir: Returns 1 if a directory
//-----------------------------------------------------------------------------
int fatfs_entry_is_dir(struct fat_dir_entry *entry)
{
    if (entry->Attr & FILE_TYPE_DIR)
        return 1;
    else
        return 0;
}
//-----------------------------------------------------------------------------
// fatfs_entry_is_file: Returns 1 is a file entry
//-----------------------------------------------------------------------------
int fatfs_entry_is_file(struct fat_dir_entry *entry)
{
    if (entry->Attr & FILE_TYPE_FILE)
        return 1;
    else
        return 0;
}
//-----------------------------------------------------------------------------
// fatfs_lfn_entries_required: Calculate number of 13 characters entries
//-----------------------------------------------------------------------------
#if FATFS_INC_LFN_SUPPORT
int fatfs_lfn_entries_required(char *filename)
{
    int length = (int)strlen(filename);

    if (length)
        return (length + MAX_LFN_ENTRY_LENGTH - 1) / MAX_LFN_ENTRY_LENGTH;
    else
        return 0;
}
#endif
//-----------------------------------------------------------------------------
// fatfs_filename_to_lfn:
//-----------------------------------------------------------------------------
#if FATFS_INC_LFN_SUPPORT
void fatfs_filename_to_lfn(char *filename, uint8 *buffer, int entry, uint8 sfnChk)
{
    int i;
    int nameIndexes[MAX_LFN_ENTRY_LENGTH] = {1,3,5,7,9,0x0E,0x10,0x12,0x14,0x16,0x18,0x1C,0x1E};

    // 13 characters entries
    int length = (int)strlen(filename);
    int entriesRequired = fatfs_lfn_entries_required(filename);

    // Filename offset
    int start = entry * MAX_LFN_ENTRY_LENGTH;

    // Initialise to zeros
    memset(buffer, 0x00, FAT_DIR_ENTRY_SIZE);

    // LFN entry number
    buffer[0] = (uint8)(((entriesRequired-1)==entry)?(0x40|(entry+1)):(entry+1));

    // LFN flag
    buffer[11] = 0x0F;

    // Checksum of short filename
    buffer[13] = sfnChk;

    // Copy to buffer
    for (i=0;i<MAX_LFN_ENTRY_LENGTH;i++)
    {
        if ( (start+i) < length )
            buffer[nameIndexes[i]] = filename[start+i];
        else if ( (start+i) == length )
            buffer[nameIndexes[i]] = 0x00;
        else
        {
            buffer[nameIndexes[i]] = 0xFF;
            buffer[nameIndexes[i]+1] = 0xFF;
        }
    }
}
#endif
//-----------------------------------------------------------------------------
// fatfs_sfn_create_entry: Create the short filename directory entry
//-----------------------------------------------------------------------------
#if FATFS_INC_WRITE_SUPPORT
void fatfs_sfn_create_entry(char *shortfilename, uint32 size, uint32 startCluster, struct fat_dir_entry *entry, int dir)
{
    int i;

    // Copy short filename
    for (i=0;i<FAT_SFN_SIZE_FULL;i++)
        entry->Name[i] = shortfilename[i];

    // Unless we have a RTC we might as well set these to 1980
    entry->CrtTimeTenth = 0x00;
    entry->CrtTime[1] = entry->CrtTime[0] = 0x00;
    entry->CrtDate[1] = 0x00;
    entry->CrtDate[0] = 0x20;
    entry->LstAccDate[1] = 0x00;
    entry->LstAccDate[0] = 0x20;
    entry->WrtTime[1] = entry->WrtTime[0] = 0x00;
    entry->WrtDate[1] = 0x00;
    entry->WrtDate[0] = 0x20;

    if (!dir)
        entry->Attr = FILE_TYPE_FILE;
    else
        entry->Attr = FILE_TYPE_DIR;

    entry->NTRes = 0x00;

    entry->FstClusHI = FAT_HTONS((uint16)((startCluster>>16) & 0xFFFF));
    entry->FstClusLO = FAT_HTONS((uint16)((startCluster>>0) & 0xFFFF));
    entry->FileSize = FAT_HTONL(size);
}
#endif
//-----------------------------------------------------------------------------
// fatfs_lfn_create_sfn: Create a padded SFN
//-----------------------------------------------------------------------------
#if FATFS_INC_WRITE_SUPPORT
int fatfs_lfn_create_sfn(char *sfn_output, char *filename)
{
    int i;
    int dotPos = -1;
    char ext[3];
    int pos;
    int len = (int)strlen(filename);

    // Invalid to start with .
    if (filename[0]=='.')
        return 0;

    memset(sfn_output, ' ', FAT_SFN_SIZE_FULL);
    memset(ext, ' ', 3);

    // Find dot seperator
    for (i = 0; i< len; i++)
    {
        if (filename[i]=='.')
            dotPos = i;
    }

    // Extract extensions
    if (dotPos!=-1)
    {
        // Copy first three chars of extension
        for (i = (dotPos+1); i < (dotPos+1+3); i++)
            if (i<len)
                ext[i-(dotPos+1)] = filename[i];

        // Shorten the length to the dot position
        len = dotPos;
    }

    // Add filename part
    pos = 0;
    for (i=0;i<len;i++)
    {
        if ( (filename[i]!=' ') && (filename[i]!='.') )
        {
            if (filename[i] >= 'a' && filename[i] <= 'z')
                sfn_output[pos++] = filename[i] - 'a' + 'A';
            else
                sfn_output[pos++] = filename[i];
        }

        // Fill upto 8 characters
        if (pos==FAT_SFN_SIZE_PARTIAL)
            break;
    }

    // Add extension part
    for (i=FAT_SFN_SIZE_PARTIAL;i<FAT_SFN_SIZE_FULL;i++)
    {
        if (ext[i-FAT_SFN_SIZE_PARTIAL] >= 'a' && ext[i-FAT_SFN_SIZE_PARTIAL] <= 'z')
            sfn_output[i] = ext[i-FAT_SFN_SIZE_PARTIAL] - 'a' + 'A';
        else
            sfn_output[i] = ext[i-FAT_SFN_SIZE_PARTIAL];
    }

    return 1;
}
//-----------------------------------------------------------------------------
// fatfs_itoa:
//-----------------------------------------------------------------------------
static void fatfs_itoa(uint32 num, char *s)
{
    char* cp;
    char outbuf[12];
    const char digits[] = "0123456789ABCDEF";

    // Build string backwards
    cp = outbuf;
    do
    {
        *cp++ = digits[(int)(num % 10)];
    }
    while ((num /= 10) > 0);

    *cp-- = 0;

    // Copy in forwards
    while (cp >= outbuf)
        *s++ = *cp--;

    *s = 0;
}
#endif
//-----------------------------------------------------------------------------
// fatfs_lfn_generate_tail:
// sfn_input = Input short filename, spaced format & in upper case
// sfn_output = Output short filename with tail
//-----------------------------------------------------------------------------
#if FATFS_INC_LFN_SUPPORT
#if FATFS_INC_WRITE_SUPPORT
int fatfs_lfn_generate_tail(char *sfn_output, char *sfn_input, uint32 tailNum)
{
    int tail_chars;
    char tail_str[12];

    if (tailNum > 99999)
        return 0;

    // Convert to number
    memset(tail_str, 0x00, sizeof(tail_str));
    tail_str[0] = '~';
    fatfs_itoa(tailNum, tail_str+1);

    // Copy in base filename
    memcpy(sfn_output, sfn_input, FAT_SFN_SIZE_FULL);

    // Overwrite with tail
    tail_chars = (int)strlen(tail_str);
    memcpy(sfn_output+(FAT_SFN_SIZE_PARTIAL-tail_chars), tail_str, tail_chars);

    return 1;
}
#endif
#endif
//-----------------------------------------------------------------------------
// fatfs_convert_from_fat_time: Convert FAT time to h/m/s
//-----------------------------------------------------------------------------
#if FATFS_INC_TIME_DATE_SUPPORT
void fatfs_convert_from_fat_time(uint16 fat_time, int *hours, int *minutes, int *seconds)
{
    *hours = (fat_time >> FAT_TIME_HOURS_SHIFT) & FAT_TIME_HOURS_MASK;
    *minutes = (fat_time >> FAT_TIME_MINUTES_SHIFT) & FAT_TIME_MINUTES_MASK;
    *seconds = (fat_time >> FAT_TIME_SECONDS_SHIFT) & FAT_TIME_SECONDS_MASK;
    *seconds = *seconds * FAT_TIME_SECONDS_SCALE;
}
//-----------------------------------------------------------------------------
// fatfs_convert_from_fat_date: Convert FAT date to d/m/y
//-----------------------------------------------------------------------------
void fatfs_convert_from_fat_date(uint16 fat_date, int *day, int *month, int *year)
{
    *day = (fat_date >> FAT_DATE_DAY_SHIFT) & FAT_DATE_DAY_MASK;
    *month = (fat_date >> FAT_DATE_MONTH_SHIFT) & FAT_DATE_MONTH_MASK;
    *year = (fat_date >> FAT_DATE_YEAR_SHIFT) & FAT_DATE_YEAR_MASK;
    *year = *year + FAT_DATE_YEAR_OFFSET;
}
//-----------------------------------------------------------------------------
// fatfs_convert_to_fat_time: Convert h/m/s to FAT time
//-----------------------------------------------------------------------------
uint16 fatfs_convert_to_fat_time(int hours, int minutes, int seconds)
{
    uint16 fat_time = 0;

    // Most FAT times are to a resolution of 2 seconds
    seconds /= FAT_TIME_SECONDS_SCALE;

    fat_time = (hours & FAT_TIME_HOURS_MASK) << FAT_TIME_HOURS_SHIFT;
    fat_time|= (minutes & FAT_TIME_MINUTES_MASK) << FAT_TIME_MINUTES_SHIFT;
    fat_time|= (seconds & FAT_TIME_SECONDS_MASK) << FAT_TIME_SECONDS_SHIFT;

    return fat_time;
}
//-----------------------------------------------------------------------------
// fatfs_convert_to_fat_date: Convert d/m/y to FAT date
//-----------------------------------------------------------------------------
uint16 fatfs_convert_to_fat_date(int day, int month, int year)
{
    uint16 fat_date = 0;

    // FAT dates are relative to 1980
    if (year >= FAT_DATE_YEAR_OFFSET)
        year -= FAT_DATE_YEAR_OFFSET;

    fat_date = (day & FAT_DATE_DAY_MASK) << FAT_DATE_DAY_SHIFT;
    fat_date|= (month & FAT_DATE_MONTH_MASK) << FAT_DATE_MONTH_SHIFT;
    fat_date|= (year & FAT_DATE_YEAR_MASK) << FAT_DATE_YEAR_SHIFT;

    return fat_date;
}
#endif
//-----------------------------------------------------------------------------
// fatfs_print_sector:
//-----------------------------------------------------------------------------
#ifdef FATFS_DEBUG
void fatfs_print_sector(uint32 sector, uint8 *data)
{
    int i;
    int j;

    FAT_PRINTF(("Sector %d:\n", sector));

    for (i=0;i<FAT_SECTOR_SIZE;i++)
    {
        if (!((i) % 16))
        {
            FAT_PRINTF(("  %04d: ", i));
        }

        FAT_PRINTF(("%02x", data[i]));
        if (!((i+1) % 4))
        {
            FAT_PRINTF((" "));
        }

        if (!((i+1) % 16))
        {
            FAT_PRINTF(("   "));
            for (j=0;j<16;j++)
            {
                char ch = data[i-15+j];

                // Is printable?
                if (ch > 31 && ch < 127)
                {
                    FAT_PRINTF(("%c", ch));
                }
                else
                {
                    FAT_PRINTF(("."));
                }
            }

            FAT_PRINTF(("\n"));
        }
    }
}
#endif
