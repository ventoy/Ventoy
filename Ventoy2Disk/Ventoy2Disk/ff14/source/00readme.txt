FatFs Module Source Files R0.14


FILES

  00readme.txt   This file.
  00history.txt  Revision history.
  ff.c           FatFs module.
  ffconf.h       Configuration file of FatFs module.
  ff.h           Common include file for FatFs and application module.
  diskio.h       Common include file for FatFs and disk I/O module.
  diskio.c       An example of glue function to attach existing disk I/O module to FatFs.
  ffunicode.c    Optional Unicode utility functions.
  ffsystem.c     An example of optional O/S related functions.


  Low level disk I/O module is not included in this archive because the FatFs
  module is only a generic file system layer and it does not depend on any specific
  storage device. You need to provide a low level disk I/O module written to
  control the storage device that attached to the target system.

