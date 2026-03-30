#!/bin/bash

#[ -d /opt/diet64 ] || sh ./installdietlibc.sh
VTOY_ARM64_TOOLCHAIN_DIR=${VTOY_ARM64_TOOLCHAIN_DIR:-/opt/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu}
VTOY_ARM64_TOOLCHAIN_TAR=${VTOY_ARM64_TOOLCHAIN_TAR:-/opt/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz}

[ -d "$VTOY_ARM64_TOOLCHAIN_DIR" ] || tar xf "$VTOY_ARM64_TOOLCHAIN_TAR" -C /opt

[ -d /opt/aarch64--uclibc--stable-2020.08-1 ] || tar xf /opt/aarch64--uclibc--stable-2020.08-1.tar.bz2  -C /opt

[ -d /opt/mips-loongson-gcc7.3-linux-gnu ] || tar xf /opt/mips-loongson-gcc7.3-2019.06-29-linux-gnu.tar.gz  -C /opt
