----------------------------------------------------------------------------
  Revision history of FatFs module
----------------------------------------------------------------------------

R0.00 (February 26, 2006)

  Prototype.



R0.01 (April 29, 2006)

  The first release.



R0.02 (June 01, 2006)

  Added FAT12 support.
  Removed unbuffered mode.
  Fixed a problem on small (<32M) partition.



R0.02a (June 10, 2006)

  Added a configuration option (_FS_MINIMUM).



R0.03 (September 22, 2006)

  Added f_rename().
  Changed option _FS_MINIMUM to _FS_MINIMIZE.



R0.03a (December 11, 2006)

  Improved cluster scan algorithm to write files fast.
  Fixed f_mkdir() creates incorrect directory on FAT32.



R0.04 (February 04, 2007)

  Added f_mkfs().
  Supported multiple drive system.
  Changed some interfaces for multiple drive system.
  Changed f_mountdrv() to f_mount().



R0.04a (April 01, 2007)

  Supported multiple partitions on a physical drive.
  Added a capability of extending file size to f_lseek().
  Added minimization level 3.
  Fixed an endian sensitive code in f_mkfs().



R0.04b (May 05, 2007)

  Added a configuration option _USE_NTFLAG.
  Added FSINFO support.
  Fixed DBCS name can result FR_INVALID_NAME.
  Fixed short seek (<= csize) collapses the file object.



R0.05 (August 25, 2007)

  Changed arguments of f_read(), f_write() and f_mkfs().
  Fixed f_mkfs() on FAT32 creates incorrect FSINFO.
  Fixed f_mkdir() on FAT32 creates incorrect directory.



R0.05a (February 03, 2008)

  Added f_truncate() and f_utime().
  Fixed off by one error at FAT sub-type determination.
  Fixed btr in f_read() can be mistruncated.
  Fixed cached sector is not flushed when create and close without write.



R0.06 (April 01, 2008)

  Added fputc(), fputs(), fprintf() and fgets().
  Improved performance of f_lseek() on moving to the same or following cluster.



R0.07 (April 01, 2009)

  Merged Tiny-FatFs as a configuration option. (_FS_TINY)
  Added long file name feature. (_USE_LFN)
  Added multiple code page feature. (_CODE_PAGE)
  Added re-entrancy for multitask operation. (_FS_REENTRANT)
  Added auto cluster size selection to f_mkfs().
  Added rewind option to f_readdir().
  Changed result code of critical errors.
  Renamed string functions to avoid name collision.



R0.07a (April 14, 2009)

  Septemberarated out OS dependent code on reentrant cfg.
  Added multiple sector size feature.



R0.07c (June 21, 2009)

  Fixed f_unlink() can return FR_OK on error.
  Fixed wrong cache control in f_lseek().
  Added relative path feature.
  Added f_chdir() and f_chdrive().
  Added proper case conversion to extended character.



R0.07e (November 03, 2009)

  Septemberarated out configuration options from ff.h to ffconf.h.
  Fixed f_unlink() fails to remove a sub-directory on _FS_RPATH.
  Fixed name matching error on the 13 character boundary.
  Added a configuration option, _LFN_UNICODE.
  Changed f_readdir() to return the SFN with always upper case on non-LFN cfg.



R0.08 (May 15, 2010)

  Added a memory configuration option. (_USE_LFN = 3)
  Added file lock feature. (_FS_SHARE)
  Added fast seek feature. (_USE_FASTSEEK)
  Changed some types on the API, XCHAR->TCHAR.
  Changed .fname in the FILINFO structure on Unicode cfg.
  String functions support UTF-8 encoding files on Unicode cfg.



R0.08a (August 16, 2010)

  Added f_getcwd(). (_FS_RPATH = 2)
  Added sector erase feature. (_USE_ERASE)
  Moved file lock semaphore table from fs object to the bss.
  Fixed f_mkfs() creates wrong FAT32 volume.



R0.08b (January 15, 2011)

  Fast seek feature is also applied to f_read() and f_write().
  f_lseek() reports required table size on creating CLMP.
  Extended format syntax of f_printf().
  Ignores duplicated directory separators in given path name.



R0.09 (September 06, 2011)

  f_mkfs() supports multiple partition to complete the multiple partition feature.
  Added f_fdisk().



R0.09a (August 27, 2012)

  Changed f_open() and f_opendir() reject null object pointer to avoid crash.
  Changed option name _FS_SHARE to _FS_LOCK.
  Fixed assertion failure due to OS/2 EA on FAT12/16 volume.



R0.09b (January 24, 2013)

  Added f_setlabel() and f_getlabel().



R0.10 (October 02, 2013)

  Added selection of character encoding on the file. (_STRF_ENCODE)
  Added f_closedir().
  Added forced full FAT scan for f_getfree(). (_FS_NOFSINFO)
  Added forced mount feature with changes of f_mount().
  Improved behavior of volume auto detection.
  Improved write throughput of f_puts() and f_printf().
  Changed argument of f_chdrive(), f_mkfs(), disk_read() and disk_write().
  Fixed f_write() can be truncated when the file size is close to 4GB.
  Fixed f_open(), f_mkdir() and f_setlabel() can return incorrect value on error.



R0.10a (January 15, 2014)

  Added arbitrary strings as drive number in the path name. (_STR_VOLUME_ID)
  Added a configuration option of minimum sector size. (_MIN_SS)
  2nd argument of f_rename() can have a drive number and it will be ignored.
  Fixed f_mount() with forced mount fails when drive number is >= 1. (appeared at R0.10)
  Fixed f_close() invalidates the file object without volume lock.
  Fixed f_closedir() returns but the volume lock is left acquired. (appeared at R0.10)
  Fixed creation of an entry with LFN fails on too many SFN collisions. (appeared at R0.07)



R0.10b (May 19, 2014)

  Fixed a hard error in the disk I/O layer can collapse the directory entry.
  Fixed LFN entry is not deleted when delete/rename an object with lossy converted SFN. (appeared at R0.07)



R0.10c (November 09, 2014)

  Added a configuration option for the platforms without RTC. (_FS_NORTC)
  Changed option name _USE_ERASE to _USE_TRIM.
  Fixed volume label created by Mac OS X cannot be retrieved with f_getlabel(). (appeared at R0.09b)
  Fixed a potential problem of FAT access that can appear on disk error.
  Fixed null pointer dereference on attempting to delete the root direcotry. (appeared at R0.08)



R0.11 (February 09, 2015)

  Added f_findfirst(), f_findnext() and f_findclose(). (_USE_FIND)
  Fixed f_unlink() does not remove cluster chain of the file. (appeared at R0.10c)
  Fixed _FS_NORTC option does not work properly. (appeared at R0.10c)



R0.11a (September 05, 2015)

  Fixed wrong media change can lead a deadlock at thread-safe configuration.
  Added code page 771, 860, 861, 863, 864, 865 and 869. (_CODE_PAGE)
  Removed some code pages actually not exist on the standard systems. (_CODE_PAGE)
  Fixed errors in the case conversion teble of code page 437 and 850 (ff.c).
  Fixed errors in the case conversion teble of Unicode (cc*.c).



R0.12 (April 12, 2016)

  Added support for exFAT file system. (_FS_EXFAT)
  Added f_expand(). (_USE_EXPAND)
  Changed some members in FINFO structure and behavior of f_readdir().
  Added an option _USE_CHMOD.
  Removed an option _WORD_ACCESS.
  Fixed errors in the case conversion table of Unicode (cc*.c).



R0.12a (July 10, 2016)

  Added support for creating exFAT volume with some changes of f_mkfs().
  Added a file open method FA_OPEN_APPEND. An f_lseek() following f_open() is no longer needed.
  f_forward() is available regardless of _FS_TINY.
  Fixed f_mkfs() creates wrong volume. (appeared at R0.12)
  Fixed wrong memory read in create_name(). (appeared at R0.12)
  Fixed compilation fails at some configurations, _USE_FASTSEEK and _USE_FORWARD.



R0.12b (September 04, 2016)

  Made f_rename() be able to rename objects with the same name but case.
  Fixed an error in the case conversion teble of code page 866. (ff.c)
  Fixed writing data is truncated at the file offset 4GiB on the exFAT volume. (appeared at R0.12)
  Fixed creating a file in the root directory of exFAT volume can fail. (appeared at R0.12)
  Fixed f_mkfs() creating exFAT volume with too small cluster size can collapse unallocated memory. (appeared at R0.12)
  Fixed wrong object name can be returned when read directory at Unicode cfg. (appeared at R0.12)
  Fixed large file allocation/removing on the exFAT volume collapses allocation bitmap. (appeared at R0.12)
  Fixed some internal errors in f_expand() and f_lseek(). (appeared at R0.12)



R0.12c (March 04, 2017)

  Improved write throughput at the fragmented file on the exFAT volume.
  Made memory usage for exFAT be able to be reduced as decreasing _MAX_LFN.
  Fixed successive f_getfree() can return wrong count on the FAT12/16 volume. (appeared at R0.12)
  Fixed configuration option _VOLUMES cannot be set 10. (appeared at R0.10c)



R0.13 (May 21, 2017)

  Changed heading character of configuration keywords "_" to "FF_".
  Removed ASCII-only configuration, FF_CODE_PAGE = 1. Use FF_CODE_PAGE = 437 instead.
  Added f_setcp(), run-time code page configuration. (FF_CODE_PAGE = 0)
  Improved cluster allocation time on stretch a deep buried cluster chain.
  Improved processing time of f_mkdir() with large cluster size by using FF_USE_LFN = 3.
  Improved NoFatChain flag of the fragmented file to be set after it is truncated and got contiguous.
  Fixed archive attribute is left not set when a file on the exFAT volume is renamed. (appeared at R0.12)
  Fixed exFAT FAT entry can be collapsed when write or lseek operation to the existing file is done. (appeared at R0.12c)
  Fixed creating a file can fail when a new cluster allocation to the exFAT directory occures. (appeared at R0.12c)



R0.13a (October 14, 2017)

  Added support for UTF-8 encoding on the API. (FF_LFN_UNICODE = 2)
  Added options for file name output buffer. (FF_LFN_BUF, FF_SFN_BUF).
  Added dynamic memory allocation option for working buffer of f_mkfs() and f_fdisk().
  Fixed f_fdisk() and f_mkfs() create the partition table with wrong CHS parameters. (appeared at R0.09)
  Fixed f_unlink() can cause lost clusters at fragmented file on the exFAT volume. (appeared at R0.12c)
  Fixed f_setlabel() rejects some valid characters for exFAT volume. (appeared at R0.12)



R0.13b (April 07, 2018)

  Added support for UTF-32 encoding on the API. (FF_LFN_UNICODE = 3)
  Added support for Unix style volume ID. (FF_STR_VOLUME_ID = 2)
  Fixed accesing any object on the exFAT root directory beyond the cluster boundary can fail. (appeared at R0.12c)
  Fixed f_setlabel() does not reject some invalid characters. (appeared at R0.09b)



R0.13c (October 14, 2018)
  Supported stdint.h for C99 and later. (integer.h was included in ff.h)
  Fixed reading a directory gets infinite loop when the last directory entry is not empty. (appeared at R0.12)
  Fixed creating a sub-directory in the fragmented sub-directory on the exFAT volume collapses FAT chain of the parent directory. (appeared at R0.12)
  Fixed f_getcwd() cause output buffer overrun when the buffer has a valid drive number. (appeared at R0.13b)



R0.14 (October 14, 2019)
  Added support for 64-bit LBA and GUID partition table (FF_LBA64 = 1)
  Changed some API functions, f_mkfs() and f_fdisk().
  Fixed f_open() function cannot find the file with file name in length of FF_MAX_LFN characters.
  Fixed f_readdir() function cannot retrieve long file names in length of FF_MAX_LFN - 1 characters.
  Fixed f_readdir() function returns file names with wrong case conversion. (appeared at R0.12)
  Fixed f_mkfs() function can fail to create exFAT volume in the second partition. (appeared at R0.12)

