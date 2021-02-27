#ifndef __FILESTRING_H__
#define __FILESTRING_H__

//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
int fatfs_total_path_levels(char *path);
int fatfs_get_substring(char *Path, int levelreq, char *output, int max_len);
int fatfs_split_path(char *FullPath, char *Path, int max_path, char *FileName, int max_filename);
int fatfs_compare_names(char* strA, char* strB);
int fatfs_string_ends_with_slash(char *path);
int fatfs_get_sfn_display_name(char* out, char* in);
int fatfs_get_extension(char* filename, char* out, int maxlen);
int fatfs_create_path_string(char* path, char *filename, char* out, int maxlen);

#ifndef NULL
    #define NULL 0
#endif

#endif
