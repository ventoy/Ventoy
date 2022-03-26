/* libexfat/config.h.  Generated from config.h.in by configure.  */
/* libexfat/config.h.in.  Generated from configure.ac by autoheader.  */

/* Name of package */
#define PACKAGE "exfat"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "relan@users.noreply.github.com"

/* Define to the full name of this package. */
#define PACKAGE_NAME "Free exFAT implementation"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "Free exFAT implementation 1.3.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "exfat"

/* Define to the home page for this package. */
#define PACKAGE_URL "https://github.com/relan/exfat"

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.3.0"

/* Define if block devices are not supported. */
/* #undef USE_UBLIO */

/* Version number of package */
#define VERSION "1.3.0"

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */
