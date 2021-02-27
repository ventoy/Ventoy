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
#include <string.h>
#include <assert.h>
#include "fat_string.h"

//-----------------------------------------------------------------------------
// fatfs_total_path_levels: Take a filename and path and count the sub levels
// of folders. E.g. C:\folder\file.zip = 1 level
// Acceptable input formats are:
//        c:\folder\file.zip
//        /dev/etc/samba.conf
// Returns: -1 = Error, 0 or more = Ok
//-----------------------------------------------------------------------------
int fatfs_total_path_levels(char *path)
{
    int levels = 0;
    char expectedchar;

    if (!path)
        return -1;

    // Acceptable formats:
    //  c:\folder\file.zip
    //  /dev/etc/samba.conf
    if (*path == '/')
    {
        expectedchar = '/';
        path++;
    }
    else if (path[1] == ':' || path[2] == '\\')
    {
        expectedchar = '\\';
        path += 3;
    }
    else
        return -1;

    // Count levels in path string
    while (*path)
    {
        // Fast forward through actual subdir text to next slash
        for (; *path; )
        {
            // If slash detected escape from for loop
            if (*path == expectedchar) { path++; break; }
            path++;
        }

        // Increase number of subdirs founds
        levels++;
    }

    // Subtract the file itself
    return levels-1;
}
//-----------------------------------------------------------------------------
// fatfs_get_substring: Get a substring from 'path' which contains the folder
// (or file) at the specified level.
// E.g. C:\folder\file.zip : Level 0 = C:\folder, Level 1 = file.zip
// Returns: -1 = Error, 0 = Ok
//-----------------------------------------------------------------------------
int fatfs_get_substring(char *path, int levelreq, char *output, int max_len)
{
    int i;
    int pathlen=0;
    int levels=0;
    int copypnt=0;
    char expectedchar;

    if (!path || max_len <= 0)
        return -1;

    // Acceptable formats:
    //  c:\folder\file.zip
    //  /dev/etc/samba.conf
    if (*path == '/')
    {
        expectedchar = '/';
        path++;
    }
    else if (path[1] == ':' || path[2] == '\\')
    {
        expectedchar = '\\';
        path += 3;
    }
    else
        return -1;

    // Get string length of path
    pathlen = (int)strlen (path);

    // Loop through the number of times as characters in 'path'
    for (i = 0; i<pathlen; i++)
    {
        // If a '\' is found then increase level
        if (*path == expectedchar) levels++;

        // If correct level and the character is not a '\' or '/' then copy text to 'output'
        if ( (levels == levelreq) && (*path != expectedchar) && (copypnt < (max_len-1)))
            output[copypnt++] = *path;

        // Increment through path string
        path++;
    }

    // Null Terminate
    output[copypnt] = '\0';

    // If a string was copied return 0 else return 1
    if (output[0] != '\0')
        return 0;    // OK
    else
        return -1;    // Error
}
//-----------------------------------------------------------------------------
// fatfs_split_path: Full path contains the passed in string.
// Returned is the path string and file Name string
// E.g. C:\folder\file.zip -> path = C:\folder  filename = file.zip
// E.g. C:\file.zip -> path = [blank]  filename = file.zip
//-----------------------------------------------------------------------------
int fatfs_split_path(char *full_path, char *path, int max_path, char *filename, int max_filename)
{
    int strindex;

    // Count the levels to the filepath
    int levels = fatfs_total_path_levels(full_path);
    if (levels == -1)
        return -1;

    // Get filename part of string
    if (fatfs_get_substring(full_path, levels, filename, max_filename) != 0)
        return -1;

    // If root file
    if (levels == 0)
        path[0] = '\0';
    else
    {
        strindex = (int)strlen(full_path) - (int)strlen(filename);
        if (strindex > max_path)
            strindex = max_path;

        memcpy(path, full_path, strindex);
        path[strindex-1] = '\0';
    }

    return 0;
}
//-----------------------------------------------------------------------------
// FileString_StrCmpNoCase: Compare two strings case with case sensitivity
//-----------------------------------------------------------------------------
static int FileString_StrCmpNoCase(char *s1, char *s2, int n)
{
    int diff;
    char a,b;

    while (n--)
    {
        a = *s1;
        b = *s2;

        // Make lower case if uppercase
        if ((a>='A') && (a<='Z'))
            a+= 32;
        if ((b>='A') && (b<='Z'))
            b+= 32;

        diff = a - b;

        // If different
        if (diff)
            return diff;

        // If run out of strings
        if ( (*s1 == 0) || (*s2 == 0) )
            break;

        s1++;
        s2++;
    }
    return 0;
}
//-----------------------------------------------------------------------------
// FileString_GetExtension: Get index to extension within filename
// Returns -1 if not found or index otherwise
//-----------------------------------------------------------------------------
static int FileString_GetExtension(char *str)
{
    int dotPos = -1;
    char *strSrc = str;

    // Find last '.' in string (if at all)
    while (*strSrc)
    {
        if (*strSrc=='.')
            dotPos = (int)(strSrc-str);

        strSrc++;
    }

    return dotPos;
}
//-----------------------------------------------------------------------------
// FileString_TrimLength: Get length of string excluding trailing spaces
// Returns -1 if not found or index otherwise
//-----------------------------------------------------------------------------
static int FileString_TrimLength(char *str, int strLen)
{
    int length = strLen;
    char *strSrc = str+strLen-1;

    // Find last non white space
    while (strLen != 0)
    {
        if (*strSrc == ' ')
            length = (int)(strSrc - str);
        else
            break;

        strSrc--;
        strLen--;
    }

    return length;
}
//-----------------------------------------------------------------------------
// fatfs_compare_names: Compare two filenames (without copying or changing origonals)
// Returns 1 if match, 0 if not
//-----------------------------------------------------------------------------
int fatfs_compare_names(char* strA, char* strB)
{
    char *ext1 = NULL;
    char *ext2 = NULL;
    int ext1Pos, ext2Pos;
    int file1Len, file2Len;

    // Get both files extension
    ext1Pos = FileString_GetExtension(strA);
    ext2Pos = FileString_GetExtension(strB);

    // NOTE: Extension position can be different for matching
    // filename if trailing space are present before it!
    // Check that if one has an extension, so does the other
    if ((ext1Pos==-1) && (ext2Pos!=-1))
        return 0;
    if ((ext2Pos==-1) && (ext1Pos!=-1))
        return 0;

    // If they both have extensions, compare them
    if (ext1Pos!=-1)
    {
        // Set pointer to start of extension
        ext1 = strA+ext1Pos+1;
        ext2 = strB+ext2Pos+1;

        // Verify that the file extension lengths match!
        if (strlen(ext1) != strlen(ext2))
            return 0;

        // If they dont match
        if (FileString_StrCmpNoCase(ext1, ext2, (int)strlen(ext1))!=0)
            return 0;

        // Filelength is upto extensions
        file1Len = ext1Pos;
        file2Len = ext2Pos;
    }
    // No extensions
    else
    {
        // Filelength is actual filelength
        file1Len = (int)strlen(strA);
        file2Len = (int)strlen(strB);
    }

    // Find length without trailing spaces (before ext)
    file1Len = FileString_TrimLength(strA, file1Len);
    file2Len = FileString_TrimLength(strB, file2Len);

    // Check the file lengths match
    if (file1Len!=file2Len)
        return 0;

    // Compare main part of filenames
    if (FileString_StrCmpNoCase(strA, strB, file1Len)!=0)
        return 0;
    else
        return 1;
}
//-----------------------------------------------------------------------------
// fatfs_string_ends_with_slash: Does the string end with a slash (\ or /)
//-----------------------------------------------------------------------------
int fatfs_string_ends_with_slash(char *path)
{
    if (path)
    {
        while (*path)
        {
            // Last character?
            if (!(*(path+1)))
            {
                if (*path == '\\' || *path == '/')
                    return 1;
            }

            path++;
        }
    }

    return 0;
}
//-----------------------------------------------------------------------------
// fatfs_get_sfn_display_name: Get display name for SFN entry
//-----------------------------------------------------------------------------
int fatfs_get_sfn_display_name(char* out, char* in)
{
    int len = 0;
    while (*in && len <= 11)
    {
        char a = *in++;

        if (a == ' ')
            continue;
        // Make lower case if uppercase
        else if ((a>='A') && (a<='Z'))
            a+= 32;

        *out++ = a;
        len++;
    }

    *out = '\0';
    return 1;
}
//-----------------------------------------------------------------------------
// fatfs_get_extension: Get extension of filename passed in 'filename'.
// Returned extension is always lower case.
// Returns: 1 if ok, 0 if not.
//-----------------------------------------------------------------------------
int fatfs_get_extension(char* filename, char* out, int maxlen)
{
    int len = 0;

    // Get files extension offset
    int ext_pos = FileString_GetExtension(filename);

    if (ext_pos > 0 && out && maxlen)
    {
        filename += ext_pos + 1;

        while (*filename && len < (maxlen-1))
        {
            char a = *filename++;

            // Make lowercase if uppercase
            if ((a>='A') && (a<='Z'))
                a+= 32;

            *out++ = a;
            len++;
        }

        *out = '\0';
        return 1;
    }

    return 0;
}
//-----------------------------------------------------------------------------
// fatfs_create_path_string: Append path & filename to create file path string.
// Returns: 1 if ok, 0 if not.
//-----------------------------------------------------------------------------
int fatfs_create_path_string(char* path, char *filename, char* out, int maxlen)
{
    int len = 0;
    char last = 0;
    char seperator = '/';

    if (path && filename && out && maxlen > 0)
    {
        while (*path && len < (maxlen-2))
        {
            last = *path++;
            if (last == '\\')
                seperator = '\\';
            *out++ = last;
            len++;
        }

        // Add a seperator if trailing one not found
        if (last != '\\' && last != '/')
            *out++ = seperator;

        while (*filename && len < (maxlen-1))
        {
            *out++ = *filename++;
            len++;
        }

        *out = '\0';

        return 1;
    }

    return 0;
}
//-----------------------------------------------------------------------------
// Test Bench
//-----------------------------------------------------------------------------
#ifdef FAT_STRING_TESTBENCH
void main(void)
{
    char output[255];
    char output2[255];

    assert(fatfs_total_path_levels("C:\\folder\\file.zip") == 1);
    assert(fatfs_total_path_levels("C:\\file.zip") == 0);
    assert(fatfs_total_path_levels("C:\\folder\\folder2\\file.zip") == 2);
    assert(fatfs_total_path_levels("C:\\") == -1);
    assert(fatfs_total_path_levels("") == -1);
    assert(fatfs_total_path_levels("/dev/etc/file.zip") == 2);
    assert(fatfs_total_path_levels("/dev/file.zip") == 1);

    assert(fatfs_get_substring("C:\\folder\\file.zip", 0, output, sizeof(output)) == 0);
    assert(strcmp(output, "folder") == 0);

    assert(fatfs_get_substring("C:\\folder\\file.zip", 1, output, sizeof(output)) == 0);
    assert(strcmp(output, "file.zip") == 0);

    assert(fatfs_get_substring("/dev/etc/file.zip", 0, output, sizeof(output)) == 0);
    assert(strcmp(output, "dev") == 0);

    assert(fatfs_get_substring("/dev/etc/file.zip", 1, output, sizeof(output)) == 0);
    assert(strcmp(output, "etc") == 0);

    assert(fatfs_get_substring("/dev/etc/file.zip", 2, output, sizeof(output)) == 0);
    assert(strcmp(output, "file.zip") == 0);

    assert(fatfs_split_path("C:\\folder\\file.zip", output, sizeof(output), output2, sizeof(output2)) == 0);
    assert(strcmp(output, "C:\\folder") == 0);
    assert(strcmp(output2, "file.zip") == 0);

    assert(fatfs_split_path("C:\\file.zip", output, sizeof(output), output2, sizeof(output2)) == 0);
    assert(output[0] == 0);
    assert(strcmp(output2, "file.zip") == 0);

    assert(fatfs_split_path("/dev/etc/file.zip", output, sizeof(output), output2, sizeof(output2)) == 0);
    assert(strcmp(output, "/dev/etc") == 0);
    assert(strcmp(output2, "file.zip") == 0);

    assert(FileString_GetExtension("C:\\file.zip") == strlen("C:\\file"));
    assert(FileString_GetExtension("C:\\file.zip.ext") == strlen("C:\\file.zip"));
    assert(FileString_GetExtension("C:\\file.zip.") == strlen("C:\\file.zip"));

    assert(FileString_TrimLength("C:\\file.zip", strlen("C:\\file.zip")) == strlen("C:\\file.zip"));
    assert(FileString_TrimLength("C:\\file.zip   ", strlen("C:\\file.zip   ")) == strlen("C:\\file.zip"));
    assert(FileString_TrimLength("   ", strlen("   ")) == 0);

    assert(fatfs_compare_names("C:\\file.ext", "C:\\file.ext") == 1);
    assert(fatfs_compare_names("C:\\file2.ext", "C:\\file.ext") == 0);
    assert(fatfs_compare_names("C:\\file  .ext", "C:\\file.ext") == 1);
    assert(fatfs_compare_names("C:\\file  .ext", "C:\\file2.ext") == 0);

    assert(fatfs_string_ends_with_slash("C:\\folder") == 0);
    assert(fatfs_string_ends_with_slash("C:\\folder\\") == 1);
    assert(fatfs_string_ends_with_slash("/path") == 0);
    assert(fatfs_string_ends_with_slash("/path/a") == 0);
    assert(fatfs_string_ends_with_slash("/path/") == 1);

    assert(fatfs_get_extension("/mypath/file.wav", output, 4) == 1);
    assert(strcmp(output, "wav") == 0);
    assert(fatfs_get_extension("/mypath/file.WAV", output, 4) == 1);
    assert(strcmp(output, "wav") == 0);
    assert(fatfs_get_extension("/mypath/file.zip", output, 4) == 1);
    assert(strcmp(output, "ext") != 0);

    assert(fatfs_create_path_string("/mydir1", "myfile.txt", output, sizeof(output)) == 1);
    assert(strcmp(output, "/mydir1/myfile.txt") == 0);
    assert(fatfs_create_path_string("/mydir2/", "myfile2.txt", output, sizeof(output)) == 1);
    assert(strcmp(output, "/mydir2/myfile2.txt") == 0);
    assert(fatfs_create_path_string("C:\\mydir3", "myfile3.txt", output, sizeof(output)) == 1);
    assert(strcmp(output, "C:\\mydir3\\myfile3.txt") == 0);
}
#endif
