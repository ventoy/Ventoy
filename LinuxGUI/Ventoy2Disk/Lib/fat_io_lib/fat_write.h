#ifndef __FAT_WRITE_H__
#define __FAT_WRITE_H__

#include "fat_defs.h"
#include "fat_opts.h"

//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
int fatfs_add_file_entry(struct fatfs *fs, uint32 dirCluster, char *filename, char *shortfilename, uint32 startCluster, uint32 size, int dir);
int fatfs_add_free_space(struct fatfs *fs, uint32 *startCluster, uint32 clusters);
int fatfs_allocate_free_space(struct fatfs *fs, int newFile, uint32 *startCluster, uint32 size);

#endif
