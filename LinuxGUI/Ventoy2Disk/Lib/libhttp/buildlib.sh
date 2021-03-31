#!/bin/sh

# https://github.com/lammertb/libhttp/archive/v1.8.tar.gz


# rm -rf include
# rm -rf lib
# mkdir include
# mkdir lib

# rm -rf libhttp-1.8
# tar xf libhttp-1.8.tar.gz
# cd libhttp-1.8
# cp -a include/civetweb.h  ../include/


# cd ..
# rm -rf libhttp-1.8
# tar xf libhttp-1.8.tar.gz
# cd libhttp-1.8
# make lib COPT="-DNDEBUG -DNO_CGI -DNO_CACHING -DNO_SSL -DSQLITE_DISABLE_LFS -DSSL_ALREADY_INITIALIZED"
# cp -a libcivetweb.a ../lib/libcivetweb_64.a



# cd ..
# rm -rf libhttp-1.8
# tar xf libhttp-1.8.tar.gz
# cd libhttp-1.8
# make lib COPT="-m32 -DNDEBUG -DNO_CGI -DNO_CACHING -DNO_SSL -DSQLITE_DISABLE_LFS -DSSL_ALREADY_INITIALIZED"
# cp -a libcivetweb.a ../lib/libcivetweb_32.a



# cd ..
# rm -rf libhttp-1.8
# tar xf libhttp-1.8.tar.gz
# cd libhttp-1.8
# make lib CC=aarch64-linux-gnu-gcc  COPT="-DNDEBUG -DNO_CGI -DNO_CACHING -DNO_SSL -DSQLITE_DISABLE_LFS -DSSL_ALREADY_INITIALIZED"
# cp -a libcivetweb.a ../lib/libcivetweb_aa64.a


# cd ..
# rm -rf libhttp-1.8


