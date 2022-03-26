#!/bin/bash

#[ -d /opt/diet64 ] || sh ./installdietlibc.sh

[ -d /opt/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu ] || tar xf /opt/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu.tar.xz  -C /opt

[ -d /opt/aarch64--uclibc--stable-2020.08-1 ] || tar xf /opt/aarch64--uclibc--stable-2020.08-1.tar.bz2  -C /opt

[ -d /opt/mips-loongson-gcc7.3-linux-gnu ] || tar xf /opt/mips-loongson-gcc7.3-2019.06-29-linux-gnu.tar.gz  -C /opt
