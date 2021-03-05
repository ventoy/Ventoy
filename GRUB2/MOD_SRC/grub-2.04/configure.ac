# -*- autoconf -*-

# Process this file with autoconf to produce a configure script.

# Copyright (C) 2002,2003,2004,2005,2006,2007,2008,2009,2010  Free Software Foundation, Inc.
#
# This configure.ac is free software; the author
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

dnl This configure script is complicated, because GRUB needs to deal
dnl with three potentially different types:
dnl
dnl   build  -- the environment for building GRUB
dnl   host   -- the environment for running utilities
dnl   target -- the environment for running GRUB
dnl
dnl In addition, GRUB needs to deal with a platform specification
dnl which specifies the system running GRUB, such as firmware.
dnl This is necessary because the target type in autoconf does not
dnl describe such a system very well.
dnl
dnl The current strategy is to use variables with no prefix (such as
dnl CC, CFLAGS, etc.) for the host type, variables with prefix "BUILD_"
dnl (such as BUILD_CC, BUILD_CFLAGS, etc.) for the build type and variables
dnl with the prefix "TARGET_" (such as TARGET_CC, TARGET_CFLAGS, etc.) are
dnl used for the target type. See INSTALL for full list of variables.

AC_INIT([GRUB],[2.04],[bug-grub@gnu.org])

AC_CONFIG_AUX_DIR([build-aux])

# We don't want -g -O2 by default in CFLAGS
: ${CFLAGS=""}

# Checks for build, host and target systems.
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
save_program_prefix="${program_prefix}"
AC_CANONICAL_TARGET
program_prefix="${save_program_prefix}"

AM_INIT_AUTOMAKE([1.11])
AC_PREREQ(2.63)
AC_CONFIG_SRCDIR([include/grub/dl.h])
AC_CONFIG_HEADER([config-util.h])

# Explicitly check for pkg-config early on, since otherwise conditional
# calls are problematic.
PKG_PROG_PKG_CONFIG

# Program name transformations
AC_ARG_PROGRAM
grub_TRANSFORM([grub-bios-setup])
grub_TRANSFORM([grub-editenv])
grub_TRANSFORM([grub-install])
grub_TRANSFORM([grub-mkconfig])
grub_TRANSFORM([grub-mkfont])
grub_TRANSFORM([grub-mkimage])
grub_TRANSFORM([grub-glue-efi])
grub_TRANSFORM([grub-mklayout])
grub_TRANSFORM([grub-mkpasswd-pbkdf2])
grub_TRANSFORM([grub-mkrelpath])
grub_TRANSFORM([grub-mkrescue])
grub_TRANSFORM([grub-probe])
grub_TRANSFORM([grub-reboot])
grub_TRANSFORM([grub-script-check])
grub_TRANSFORM([grub-set-default])
grub_TRANSFORM([grub-sparc64-setup])
grub_TRANSFORM([grub-render-label])
grub_TRANSFORM([grub-file])

# Optimization flag.  Allow user to override.
if test "x$TARGET_CFLAGS" = x; then
  TARGET_CFLAGS="$TARGET_CFLAGS -Os"
fi

# Default HOST_CPPFLAGS
HOST_CPPFLAGS="$HOST_CPPFLAGS -Wall -W"
HOST_CPPFLAGS="$HOST_CPPFLAGS -DGRUB_UTIL=1"

TARGET_CPPFLAGS="$TARGET_CPPFLAGS -Wall -W"

case "$target_cpu" in
  i[[3456]]86)	target_cpu=i386 ;;
  amd64)	target_cpu=x86_64 ;;
  sparc)	target_cpu=sparc64 ;;
  mips64el)
                target_cpu=mips64el
		machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_CPU_MIPS64EL=1"
		;;
  mipsel)
                target_cpu=mipsel
		machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_CPU_MIPSEL=1"
		;;
  mips|mips64)
                target_cpu=mips
		machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_CPU_MIPS=1"
		;;
  arm*)
		target_cpu=arm
		;;
  aarch64*)
		target_cpu=arm64
		;;
  riscv32*)
		target_cpu=riscv32
		;;
  riscv64*)
		target_cpu=riscv64
		;;
esac

# Specify the platform (such as firmware).
AC_ARG_WITH([platform],
            AS_HELP_STRING([--with-platform=PLATFORM],
                           [select the host platform [[guessed]]]))

# Guess the platform if not specified.
if test "x$with_platform" = x; then
  case "$target_cpu"-"$target_vendor" in
    i386-apple) platform=efi ;;
    i386-*) platform=pc ;;
    x86_64-apple) platform=efi ;;
    x86_64-*) platform=pc ;;
    powerpc-*) platform=ieee1275 ;;
    powerpc64-*) platform=ieee1275 ;;
    powerpc64le-*) platform=ieee1275 ;;
    sparc64-*) platform=ieee1275 ;;
    mips64el-*) platform=efi;;
    mipsel-*) platform=loongson ;;
    mips-*) platform=arc ;;
    ia64-*) platform=efi ;;
    arm-*) platform=uboot ;;
    arm64-*) platform=efi ;;
    riscv32-*) platform=efi ;;
    riscv64-*) platform=efi ;;
    *)
      AC_MSG_WARN([unsupported CPU: "$target_cpu" - only building utilities])
      platform=none
      ;;
  esac
else
  platform="$with_platform"
fi

case "$target_cpu"-"$platform" in
  x86_64-efi) ;;
  x86_64-emu) ;;
  x86_64-xen) ;;
  x86_64-none) ;;
  x86_64-*) target_cpu=i386 ;;
  powerpc64-ieee1275) target_cpu=powerpc ;;
  powerpc64le-ieee1275) target_cpu=powerpc ;;
esac

# Check if the platform is supported, make final adjustments.
case "$target_cpu"-"$platform" in
  i386-efi) ;;
  x86_64-efi) ;;
  i386-xen) ;;
  i386-xen_pvh) ;;
  x86_64-xen) ;;
  i386-pc) ;;
  i386-multiboot) ;;
  i386-coreboot) ;;
  i386-linuxbios) platform=coreboot ;;
  i386-ieee1275) ;;
  i386-qemu) ;;
  powerpc-ieee1275) ;;
  sparc64-ieee1275) ;;
  ia64-efi) ;;
  mips-qemu_mips) ;;
  mips-qemu-mips) platform=qemu_mips;;
  mips-arc) ;;
  mipsel-arc) ;;
  mipsel-qemu_mips) ;;
  mipsel-qemu-mips) platform=qemu_mips;;
  mipsel-yeeloong) platform=loongson ;;
  mipsel-fuloong) platform=loongson ;;
  mipsel-loongson) ;;
  mips64el-efi) ;;
  arm-uboot) ;;
  arm-coreboot) ;;
  arm-efi) ;;
  arm64-efi) ;;
  riscv32-efi) ;;
  riscv64-efi) ;;
  *-emu) ;;
  *-none) ;;
  *) AC_MSG_ERROR([platform "$platform" is not supported for target CPU "$target_cpu"]) ;;
esac

if test x$platform != xemu ; then
   case "$target_cpu" in
	i386 | powerpc) target_m32=1 ;;
	x86_64 | sparc64) target_m64=1 ;;
   esac
fi

if test x"$target_cpu-$platform" = xsparc64-emu ; then
   target_m64=1
fi

case "$target_os" in
  windows* | mingw32*) target_os=cygwin ;;
esac

# This normalizes the names, and creates a new variable ("host_kernel")
# while at it, since the mapping is not always 1:1 (e.g. different OSes
# using the same kernel type).
case "$host_os" in
  gnu*)				host_kernel=hurd ;;
  linux*)			host_kernel=linux ;;
  freebsd* | kfreebsd*-gnu)	host_kernel=kfreebsd ;;
  netbsd*)			host_kernel=netbsd ;;
  solaris*)			host_kernel=illumos ;;
  darwin*)			host_kernel=xnu ;;
  cygwin | windows* | mingw32*)	host_kernel=windows ;;
esac

case "$host_os" in
  cygwin) have_exec=y ;;
  windows* | mingw32*) have_exec=n ;;
  aros*) have_exec=n ;;
  *) have_exec=y;;
esac

case "$platform" in
  coreboot)	machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_MACHINE_COREBOOT=1" ;;
  multiboot)	machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_MACHINE_MULTIBOOT=1" ;;
  efi)		machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_MACHINE_EFI=1" ;;
  xen)		machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_MACHINE_XEN=1" ;;
  xen_pvh)	machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_MACHINE_XEN_PVH=1" ;;
  ieee1275)	machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_MACHINE_IEEE1275=1" ;;
  uboot)	machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_MACHINE_UBOOT=1" ;;
  qemu)		machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_MACHINE_QEMU=1" ;;
  pc)		machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_MACHINE_PCBIOS=1" ;;
  emu)		machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_MACHINE_EMU=1" ;;
  loongson)	machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_MACHINE_MIPS_LOONGSON=1" ;;
  qemu_mips)	machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_MACHINE_MIPS_QEMU_MIPS=1" ;;
  arc)	machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_MACHINE_ARC=1" ;;
esac
if test x${target_cpu} = xmipsel ; then
   machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_MACHINE=`echo mips_$platform | sed y,abcdefghijklmnopqrstuvwxyz,ABCDEFGHIJKLMNOPQRSTUVWXYZ,`"
else
   machine_CPPFLAGS="$machine_CPPFLAGS -DGRUB_MACHINE=`echo ${target_cpu}_$platform | sed y,abcdefghijklmnopqrstuvwxyz,ABCDEFGHIJKLMNOPQRSTUVWXYZ,`"
fi

case "${target_cpu}-$platform" in
     mips-arc)
        TARGET_LINK_ADDR=0x88200000
        TARGET_DECOMPRESSOR_LINK_ADDR=0x88100000
	;;
     mipsel-arc)
        TARGET_LINK_ADDR=0x80700000
	TARGET_DECOMPRESSOR_LINK_ADDR=0x80600000
	;;
     mips*-qemu_mips | mips*-loongson)
        TARGET_DECOMPRESSOR_LINK_ADDR=0x80100000
	;;
esac

AC_SUBST(TARGET_LINK_ADDR)
AC_SUBST(TARGET_DECOMPRESSOR_LINK_ADDR)

TARGET_CPPFLAGS="$TARGET_CPPFLAGS $machine_CPPFLAGS"

AC_SUBST(host_cpu)
AC_SUBST(host_os)
AC_SUBST(host_kernel)

AC_SUBST(target_cpu)
AC_SUBST(platform)

# Define default variables

have_with_bootdir=n
AC_ARG_WITH([bootdir],
            AS_HELP_STRING([--with-bootdir=DIR],
                           [set the name of /boot directory [[guessed]]]),
			   [have_with_bootdir=y],
			   [have_with_bootdir=n])
if test x$have_with_bootdir = xy; then
   bootdirname="$with_bootdir"
else
   case "$host_os" in
     netbsd* | openbsd*)
        # Because /boot is used for the boot block in NetBSD and OpenBSD,
           bootdirname=''      ;;
     *)    bootdirname='boot'  ;;
   esac
fi

AC_SUBST(bootdirname)
AC_DEFINE_UNQUOTED(GRUB_BOOT_DIR_NAME, "$bootdirname",
    [Default boot directory name])

AC_ARG_WITH([grubdir],
            AS_HELP_STRING([--with-grubdir=DIR],
                           [set the name of grub directory [[guessed]]]),
			   [grubdirname="$with_grubdir"],
			   [grubdirname="$PACKAGE"])

AC_SUBST(grubdirname)
AC_DEFINE_UNQUOTED(GRUB_DIR_NAME, "$grubdirname",
    [Default grub directory name])

#
# Checks for build programs.
#

# Although cmp is listed in the GNU Coding Standards as a command which
# can used directly, OpenBSD lacks cmp in the default installation.
AC_CHECK_PROGS([CMP], [cmp])
if test "x$CMP" = x; then
  AC_MSG_ERROR([cmp is not found])
fi

AC_CHECK_PROGS([YACC], [bison])
if test "x$YACC" = x; then
  AC_MSG_ERROR([bison is not found])
fi

AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_AWK
AC_PROG_LEX
AC_PROG_YACC
AC_PROG_MAKE_SET
AC_PROG_MKDIR_P
AC_PROG_LN_S

if test "x$LEX" = "x:"; then
  AC_MSG_ERROR([flex is not found])
else
  version=`$LEX --version | $AWK '{ split($2,x,"."); print x[[1]]*10000+x[[2]]*100+x[[3]]; }'`
  if test -n "$version" -a "$version" -ge 20535; then
    :
  else
    AC_MSG_ERROR([flex is too old. GRUB requires 2.5.35 or above])
  fi
fi

# These are not a "must".
AC_PATH_PROGS(MAKEINFO, makeinfo true)

#
# Checks for host programs.
#

AC_PROG_CC
gl_EARLY
AC_PROG_CXX
AM_PROG_CC_C_O
AM_PROG_AS
AM_PATH_PYTHON([2.6])

# Must be GCC.
test "x$GCC" = xyes || AC_MSG_ERROR([GCC is required])

AC_CHECK_PROG(HAVE_CXX, $CXX, yes, no)

AC_GNU_SOURCE
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.18.3])
AC_SYS_LARGEFILE

# Identify characteristics of the host architecture.
unset ac_cv_c_bigendian

if test x"$target_cpu-$platform" = xsparc64-emu ; then
  CFLAGS="$CFLAGS -m64"
  HOST_CFLAGS="$HOST_CFLAGS -m64"
fi

CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=64"
HOST_CPPFLAGS="$HOST_CPPFLAGS -D_FILE_OFFSET_BITS=64"

AC_C_BIGENDIAN
AC_CHECK_SIZEOF(void *)
AC_CHECK_SIZEOF(long)

case "$host_os" in
  cygwin | windows* | mingw32*)
     HOST_CPPFLAGS="$HOST_CPPFLAGS -DUNICODE=1 -D_WIN32_WINNT=0x0500"
     CPPFLAGS="$CPPFLAGS -DUNICODE=1 -D_WIN32_WINNT=0x0500"
     AC_CHECK_SIZEOF(TCHAR,,[#include <windows.h>])
   ;;
esac

case "$host_os" in
  cygwin | windows* | mingw32* | aros*)
     ;;
  *)
     AC_CHECK_SIZEOF(off_t)
     if test x"$ac_cv_sizeof_off_t" != x8 ; then
       AC_CHECK_SIZEOF(off64_t)
       test x"$ac_cv_sizeof_off64_t" = x8 || AC_MSG_ERROR([Large file support is required])
     fi;;
esac

if test x$USE_NLS = xno; then
  HOST_CFLAGS="$HOST_CFLAGS -fno-builtin-gettext"
fi

if test "x$cross_compiling" = xyes; then
  AC_MSG_WARN([cannot generate manual pages while cross compiling])
else
  AC_PATH_PROG(HELP2MAN, help2man)
fi

# Check for functions and headers.
AC_CHECK_FUNCS(posix_memalign memalign getextmntent)
AC_CHECK_HEADERS(sys/param.h sys/mount.h sys/mnttab.h limits.h)

# glibc 2.25 still includes sys/sysmacros.h in sys/types.h but emits deprecation
# warning which causes compilation failure later with -Werror. So use -Werror here
# as well to force proper sys/sysmacros.h detection.
SAVED_CFLAGS="$CFLAGS"
CFLAGS="$HOST_CFLAGS -Werror"
AC_HEADER_MAJOR
CFLAGS="$SAVED_CFLAGS"

AC_CHECK_MEMBERS([struct statfs.f_fstypename],,,[$ac_includes_default
#include <sys/param.h>
#include <sys/mount.h>])

AC_CHECK_MEMBERS([struct statfs.f_mntfromname],,,[$ac_includes_default
#include <sys/param.h>
#include <sys/mount.h>])

# For opendisk() and getrawpartition() on NetBSD.
# Used in util/deviceiter.c and in util/hostdisk.c.
AC_CHECK_HEADER([util.h], [
  AC_CHECK_LIB([util], [opendisk], [
    LIBUTIL="-lutil"
    AC_DEFINE(HAVE_OPENDISK, 1, [Define if opendisk() in -lutil can be used])
  ])
  AC_CHECK_LIB([util], [getrawpartition], [
    LIBUTIL="-lutil"
    AC_DEFINE(HAVE_GETRAWPARTITION, 1, [Define if getrawpartition() in -lutil can be used])
  ])
])
AC_SUBST([LIBUTIL])

AC_CACHE_CHECK([whether -Wtrampolines work], [grub_cv_host_cc_wtrampolines], [
  SAVED_CFLAGS="$CFLAGS"
  CFLAGS="$HOST_CFLAGS -Wtrampolines -Werror"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdarg.h>
int va_arg_func (int fixed, va_list args);]], [[]])],
      [grub_cv_host_cc_wtrampolines=yes],
      [grub_cv_host_cc_wtrampolines=no])
  CFLAGS="$SAVED_CFLAGS"
])

if test x"$grub_host_cv_cc_wtrampolines" = xyes ; then
  HOST_CFLAGS="$HOST_CFLAGS -Wtrampolines"
fi

#
# Check for host and build compilers.
#
HOST_CC=$CC
AC_CHECK_PROGS(BUILD_CC, [gcc egcs cc])
test -z "$BUILD_CC" && AC_MSG_ERROR([none of gcc, egcs and cc is found. set BUILD_CC manually.])
BUILD_CPP="$BUILD_CC -E"

case "$build_os" in
  haiku*)				BUILD_LIBM= ;;
  *) 					BUILD_LIBM=-lm ;;
esac

dnl FIXME proper test seems to require too deep dive into Autoconf internals.
dnl For now just list known platforms that we support.

case "$build_os" in
  cygwin*|mingw32*|mingw64*)		BUILD_EXEEXT=.exe ;;
  *)					BUILD_EXEEXT= ;;
esac
AC_SUBST(BUILD_EXEEXT)

# In some build environments like termux /bin/sh is not a valid
# shebang. Use $SHELL instead if it's executable and /bin/sh isn't
BUILD_SHEBANG=/bin/sh
for she in /bin/sh "$SHELL"; do
  if test -x "$she" ; then
    BUILD_SHEBANG="$she"
  fi
done
AC_SUBST(BUILD_SHEBANG)

# For gnulib.
gl_INIT

WARN_FLAGS="-Wall -W -Wshadow -Wpointer-arith -Wundef -Wchar-subscripts -Wcomment -Wdeprecated-declarations -Wdisabled-optimization -Wdiv-by-zero -Wfloat-equal -Wformat-extra-args -Wformat-security -Wformat-y2k -Wimplicit -Wimplicit-function-declaration -Wimplicit-int -Wmain -Wmissing-braces -Wmissing-format-attribute -Wmultichar -Wparentheses -Wreturn-type -Wsequence-point -Wshadow -Wsign-compare -Wswitch -Wtrigraphs -Wunknown-pragmas -Wunused -Wunused-function -Wunused-label -Wunused-parameter -Wunused-value  -Wunused-variable -Wwrite-strings -Wnested-externs -Wstrict-prototypes"
EXTRA_WARN_FLAGS="-Wextra -Wattributes -Wendif-labels -Winit-self -Wint-to-pointer-cast -Winvalid-pch -Wmissing-field-initializers -Wnonnull -Woverflow -Wvla -Wpointer-to-int-cast -Wstrict-aliasing -Wvariadic-macros -Wvolatile-register-var -Wpointer-sign -Wmissing-include-dirs -Wmissing-prototypes -Wmissing-declarations -Wformat=2"

HOST_CFLAGS="$HOST_CFLAGS $WARN_FLAGS -Wcast-align"

AC_CACHE_CHECK([which extra warnings work], [grub_cv_cc_w_extra_flags], [
  SAVED_CFLAGS="$CFLAGS"
  grub_cv_cc_w_extra_flags=
  for x in $EXTRA_WARN_FLAGS; do
      CFLAGS="$HOST_CFLAGS $x -Werror"
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[]])], [flag=1], [flag=0])
      if test x$flag = x1 ; then
         grub_cv_cc_w_extra_flags="$grub_cv_cc_w_extra_flags $x"
      fi
  done
  CFLAGS="$SAVED_CFLAGS"
])

HOST_CFLAGS="$HOST_CFLAGS $grub_cv_cc_w_extra_flags"

#
# Check for target programs.
#

# Find tools for the target.
if test "x$target_alias" != x && test "x$host_alias" != "x$target_alias"; then
  tmp_ac_tool_prefix="$ac_tool_prefix"
  ac_tool_prefix=$target_alias-

  AC_CHECK_TOOLS(TARGET_CC, [gcc egcs cc],
                 [AC_MSG_ERROR([none of gcc, egcs and cc is found. set TARGET_CC manually.])])
  AC_CHECK_TOOL(TARGET_OBJCOPY, objcopy)
  AC_CHECK_TOOL(TARGET_STRIP, strip)
  AC_CHECK_TOOL(TARGET_NM, nm)
  AC_CHECK_TOOL(TARGET_RANLIB, ranlib)

  ac_tool_prefix="$tmp_ac_tool_prefix"
else
  if test "x$TARGET_CC" = x; then
    TARGET_CC=$CC
  fi
  AC_CHECK_TOOL(TARGET_OBJCOPY, objcopy)
  AC_CHECK_TOOL(TARGET_STRIP, strip)
  AC_CHECK_TOOL(TARGET_NM, nm)
  AC_CHECK_TOOL(TARGET_RANLIB, ranlib)
fi

AC_SUBST(HOST_CC)
AC_SUBST(BUILD_CC)
AC_SUBST(BUILD_CFLAGS)
AC_SUBST(BUILD_CPPFLAGS)
AC_SUBST(BUILD_LDFLAGS)
AC_SUBST(TARGET_CC)
AC_SUBST(TARGET_NM)
AC_SUBST(TARGET_RANLIB)
AC_SUBST(TARGET_STRIP)
AC_SUBST(TARGET_OBJCOPY)

# Test the C compiler for the target environment.
tmp_CC="$CC"
tmp_CFLAGS="$CFLAGS"
tmp_LDFLAGS="$LDFLAGS"
tmp_CPPFLAGS="$CPPFLAGS"
tmp_LIBS="$LIBS"
CC="$TARGET_CC"
CFLAGS="$TARGET_CFLAGS"
CPPFLAGS="$TARGET_CPPFLAGS"
LDFLAGS="$TARGET_LDFLAGS"
LIBS=""

# debug flags.
TARGET_CFLAGS="$TARGET_CFLAGS $WARN_FLAGS -g -Wredundant-decls -Wmissing-prototypes -Wmissing-declarations"
TARGET_CCASFLAGS="$TARGET_CCASFLAGS -g"

if test "x$target_cpu" != xi386 && test "x$target_cpu" != xx86_64; then
TARGET_CFLAGS="$TARGET_CFLAGS -Wcast-align"
fi

TARGET_CC_VERSION="$(LC_ALL=C $TARGET_CC --version | head -n1)"

AC_CACHE_CHECK([which extra warnings work], [grub_cv_target_cc_w_extra_flags], [
  LDFLAGS="$TARGET_LDFLAGS -nostdlib -static"

  grub_cv_target_cc_w_extra_flags=
  for x in $EXTRA_WARN_FLAGS; do
      CFLAGS="$TARGET_CFLAGS $x -Werror"
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[
asm (".globl start; start:");
void __main (void);
void __main (void) {}
int main (void);
]], [[]])], [flag=1], [flag=0])
      if test x$flag = x1 ; then
         grub_cv_target_cc_w_extra_flags="$grub_cv_target_cc_w_extra_flags $x"
      fi
  done
])

TARGET_CFLAGS="$TARGET_CFLAGS $grub_cv_target_cc_w_extra_flags"

AC_CACHE_CHECK([if compiling with clang], [grub_cv_cc_target_clang],
[
CFLAGS="$TARGET_CFLAGS"
AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([], [[
#ifdef __clang__
#error "is clang"
#endif
]])],
[grub_cv_cc_target_clang=no], [grub_cv_cc_target_clang=yes])])

if test x$target_cpu = xpowerpc -o x$target_cpu = xmips; then
  AC_CACHE_CHECK([for options to get big-endian compilation], grub_cv_target_cc_big_endian, [
    grub_cv_target_cc_big_endian=no
    for cand in "-target $target_cpu -Wl,-EB" "-target $target_cpu" \
		"-target $target_cpu-linux-gnu -Wl,-EB" "-target $target_cpu-linux-gnu" \
		"-EB" "-mbig-endian"; do
      if test x"$grub_cv_target_cc_big_endian" != xno ; then
        break
      fi
      CFLAGS="$TARGET_CFLAGS $cand -Werror"
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__ORDER_BIG_ENDIAN__ != __BYTE_ORDER__)
#error still little endian
#endif
asm (".globl start; start:");
asm (".globl _start; _start:");
asm (".globl __start; __start:");
void __main (void);
void __main (void) {}
int main (void);
]], [[]])],
		        [grub_cv_target_cc_big_endian="$cand"], [])
    done
  ])

  if test x"$grub_cv_target_cc_big_endian" = xno ; then
    AC_MSG_ERROR([could not force big-endian])
  fi

  skip_linkflags="$(echo "$grub_cv_target_cc_big_endian"|sed 's@-Wl,-EB@@')"

  TARGET_CFLAGS="$TARGET_CFLAGS $skip_linkflags"
  TARGET_CPPFLAGS="$TARGET_CPPFLAGS $skip_linkflags"
  TARGET_CCASFLAGS="$TARGET_CCASFLAGS $skip_linkflags"
  TARGET_LDFLAGS="$TARGET_LDFLAGS $grub_cv_target_cc_big_endian"
elif test x$target_cpu = xmipsel; then
  AC_CACHE_CHECK([for options to get little-endian compilation], grub_cv_target_cc_little_endian, [
    grub_cv_target_cc_little_endian=no
    for cand in "-target $target_cpu -Wl,-EL" "-target $target_cpu" \
		"-target $target_cpu-linux-gnu -Wl,-EL" "-target $target_cpu-linux-gnu" \
		"-EL"; do
      if test x"$grub_cv_target_cc_little_endian" != xno ; then
        break
      fi
      CFLAGS="$TARGET_CFLAGS $cand -Werror"
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__ORDER_BIG_ENDIAN__ == __BYTE_ORDER__)
#error still big endian
#endif
asm (".globl start; start:");
asm (".globl _start; _start:");
asm (".globl __start; __start:");
void __main (void);
void __main (void) {}
int main (void);
]], [[]])],
		        [grub_cv_target_cc_little_endian="$cand"], [])
    done
  ])

  if test x"$grub_cv_target_cc_little_endian" = xno ; then
    AC_MSG_ERROR([could not force little-endian])
  fi

  skip_linkflags="$(echo "$grub_cv_target_cc_little_endian"|sed 's@-Wl,-EL@@')"

  TARGET_CFLAGS="$TARGET_CFLAGS $skip_linkflags"
  TARGET_CPPFLAGS="$TARGET_CPPFLAGS $skip_linkflags"
  TARGET_CCASFLAGS="$TARGET_CCASFLAGS $skip_linkflags"
  TARGET_LDFLAGS="$TARGET_LDFLAGS $grub_cv_target_cc_little_endian"
fi

# GRUB code is N32-compliant but it's experimental and we would prefer to
# avoid having too much variety when it doesn't result in any real improvement.
# Moreover N64 isn't supported.
if test "x$target_cpu" = xmips || test "x$target_cpu" = xmipsel ; then
  AC_CACHE_CHECK([for options to force MIPS o32 ABI], grub_cv_target_cc_mips_o32_abi, [
    grub_cv_target_cc_mips_o32_abi=no
    for arg in "" "-mabi=32" "-target $target_cpu -mabi=32" ; do
      if test x"$grub_cv_target_cc_mips_o32_abi" != xno ; then
        break
      fi
      CFLAGS="$TARGET_CFLAGS $arg -Werror"
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#if !defined(_ABIO32) || !defined(_MIPS_SIM) || (_MIPS_SIM != _ABIO32)
#error not o32 ABI
#endif
asm (".globl start; start:");
asm (".globl _start; _start:");
asm (".globl __start; __start:");
void __main (void);
void __main (void) {}
int main (void);
]], [[]])],
		        [grub_cv_target_cc_mips_o32_abi="$arg"], [])
    done
  ])

  if test x"$grub_cv_target_cc_mips_o32_abi" = xno ; then
    AC_MSG_ERROR([could not force MIPS o32 ABI])
  fi

  TARGET_CFLAGS="$TARGET_CFLAGS $grub_cv_target_cc_mips_o32_abi"
  TARGET_CCASFLAGS="$TARGET_CCASFLAGS $grub_cv_target_cc_mips_o32_abi"
fi

AC_CACHE_CHECK([for options to compile assembly], [grub_cv_cc_target_asm_compile], [
test_program=
case "x$target_cpu-$platform" in
     xmips-* | xmipsel-*)
        test_program=mips
	;;
     xi386-pc)
       test_program=i386-pc
	;;
     xi386-* | xx86_64-*)
       test_program=i386
	;;
     xpowerpc-* | xsparc64-* | xarm-*)
        test_program=$target_cpu
	;;
esac
if test x"$test_program" = x ; then
  grub_cv_cc_target_asm_compile=
else
  found=no
  for arg in "" "-no-integrated-as"; do
    cmdline="$TARGET_CC -c -o /dev/null $TARGET_CCASFLAGS $arg $TARGET_CPPFLAGS $srcdir/asm-tests/$test_program.S"
    echo "Running $cmdline" >&AS_MESSAGE_LOG_FD
    if $cmdline >&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD; then
      grub_cv_cc_target_asm_compile="$arg"
      found=yes
      break
    fi
  done
  if test x"$found" = xno ; then
    AC_MSG_ERROR([could not compile assembly])
  fi
fi
])

TARGET_CCASFLAGS="$TARGET_CCASFLAGS $grub_cv_cc_target_asm_compile"

if test "x$target_cpu" = xi386 && test "x$platform" != xemu; then
  TARGET_CFLAGS="$TARGET_CFLAGS -march=i386"
fi

if test "x$target_m32" = x1; then
  # Force 32-bit mode.
  TARGET_CFLAGS="$TARGET_CFLAGS -m32"
  TARGET_CCASFLAGS="$TARGET_CCASFLAGS -m32"
  TARGET_CPPFLAGS="$TARGET_CPPFLAGS -m32"
  TARGET_LDFLAGS="$TARGET_LDFLAGS -m32"
  TARGET_MODULE_FORMAT="elf32"
fi

if test "x$target_m64" = x1; then
  # Force 64-bit mode.
  TARGET_CFLAGS="$TARGET_CFLAGS -m64"
  TARGET_CCASFLAGS="$TARGET_CCASFLAGS -m64"
  TARGET_CPPFLAGS="$TARGET_CPPFLAGS -m64"
  TARGET_LDFLAGS="$TARGET_LDFLAGS -m64"
  TARGET_MODULE_FORMAT="elf64"
fi

if test "x$grub_cv_cc_target_clang" = xno && test "x$target_cpu" = xi386 && test "x$platform" != xemu && test "x$platform" != xefi; then
   TARGET_CFLAGS="$TARGET_CFLAGS -mrtd -mregparm=3"
fi

# on mips redirect cache flushing function to non-existant one.
if test "x$target_cpu" = xmips || test "x$target_cpu" = xmipsel ; then
  AC_CACHE_CHECK([whether -mflush-func=grub_red_herring works], [grub_cv_cc_mflush_func], [
    CFLAGS="$TARGET_CFLAGS -mflush-func=grub_red_herring -Werror"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
        [grub_cv_cc_mflush_func=yes],
	[grub_cv_cc_mflush_func=no])
  ])

  if test "x$grub_cv_cc_mflush_func" = xyes; then
    TARGET_CFLAGS="$TARGET_CFLAGS -mflush-func=grub_red_herring"
  fi
fi


# Force no alignment to save space on i386.
if test "x$target_cpu" = xi386; then
  AC_CACHE_CHECK([whether -falign-loops works], [grub_cv_cc_falign_loop], [
    CFLAGS="$TARGET_CFLAGS -falign-loops=1 -Werror"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
        [grub_cv_cc_falign_loop=yes],
	[grub_cv_cc_falign_loop=no])
  ])

  AC_CACHE_CHECK([whether -malign-loops works], [grub_cv_cc_malign_loop], [
    CFLAGS="$TARGET_CFLAGS -malign-loops=1 -Werror"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
        [grub_cv_cc_malign_loop=yes],
	[grub_cv_cc_malign_loop=no])
  ])

  if test "x$grub_cv_cc_falign_loop" = xyes; then
    TARGET_CFLAGS="$TARGET_CFLAGS -falign-jumps=1 -falign-loops=1 -falign-functions=1"
  elif test "x$grub_cv_cc_malign_loop" = xyes; then
    TARGET_CFLAGS="$TARGET_CFLAGS -malign-jumps=1 -malign-loops=1 -malign-functions=1"
  fi
fi

AC_CACHE_CHECK([whether -freg-struct-return works], [grub_cv_cc_freg_struct_return], [
    CFLAGS="$TARGET_CFLAGS -freg-struct-return -Werror"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
        [grub_cv_cc_freg_struct_return=yes],
	[grub_cv_cc_freg_struct_return=no])
])

if test "x$grub_cv_cc_freg_struct_return" = xyes; then
    TARGET_CFLAGS="$TARGET_CFLAGS -freg-struct-return"
fi

if ( test "x$target_cpu" = xi386 || test "x$target_cpu" = xx86_64 ) && test "x$platform" != xemu; then
  # Some toolchains enable these features by default, but they need
  # registers that aren't set up properly in GRUB.
  TARGET_CFLAGS="$TARGET_CFLAGS -mno-mmx -mno-sse -mno-sse2 -mno-sse3 -mno-3dnow"
fi

# GRUB doesn't use float or doubles at all. Yet some toolchains may decide
# that floats are a good fit to run instead of what's written in the code.
# Given that floating point unit is disabled (if present to begin with)
# when GRUB is running which may result in various hard crashes.
#if test x"$platform" != xemu ; then
if ( test x"$platform" != xemu && x"$platform" != mips64el-efi ) ; then
  AC_CACHE_CHECK([for options to get soft-float], grub_cv_target_cc_soft_float, [
    grub_cv_target_cc_soft_float=no
    if test "x$target_cpu" = xarm64; then
       CFLAGS="$TARGET_CFLAGS -mgeneral-regs-only -Werror"
       AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
		         [grub_cv_target_cc_soft_float="-mgeneral-regs-only"], [])
    fi
    if test "x$target_cpu" = xriscv32; then
       CFLAGS="$TARGET_CFLAGS -march=rv32imac -mabi=ilp32 -Werror"
       AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
		         [grub_cv_target_cc_soft_float="-march=rv32imac -mabi=ilp32"], [])
    fi
    if test "x$target_cpu" = xriscv64; then
       CFLAGS="$TARGET_CFLAGS -march=rv64imac -mabi=lp64 -Werror"
       AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
		         [grub_cv_target_cc_soft_float="-march=rv64imac -mabi=lp64"], [])
    fi
    if test "x$target_cpu" = xia64; then
       CFLAGS="$TARGET_CFLAGS -mno-inline-float-divide -mno-inline-sqrt -Werror"
       AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
		         [grub_cv_target_cc_soft_float="-mno-inline-float-divide -mno-inline-sqrt"], [])
    fi
    for cand in "-msoft-float -Xclang -msoft-float -Xclang -no-implicit-float" \
		"-Xclang -msoft-float -Xclang -no-implicit-float" \
		"-Xclang -msoft-float" "-msoft-float"; do
      if test x"$grub_cv_target_cc_soft_float" != xno ; then
        break
      fi
      CFLAGS="$TARGET_CFLAGS $cand -Werror"
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
		        [grub_cv_target_cc_soft_float="$cand"], [])
    done
  ])

  if test x"$grub_cv_target_cc_soft_float" = xno ; then
    AC_MSG_ERROR([could not force soft-float])
  fi

  case x"$grub_cv_target_cc_soft_float" in
    x*"-Xclang"*)
      # A trick so that clang doesn't see it on link stаge
      TARGET_CPPFLAGS="$TARGET_CPPFLAGS $grub_cv_target_cc_soft_float"
      ;;
    *)
      TARGET_CFLAGS="$TARGET_CFLAGS $grub_cv_target_cc_soft_float"
      ;;
  esac
  TARGET_CCASFLAGS="$TARGET_CCASFLAGS $grub_cv_target_cc_soft_float"

fi

if test x"$target_cpu" = xsparc64 ; then
  AC_CACHE_CHECK([for options to reserve application registers], grub_cv_target_cc_mno_app_regs, [
    grub_cv_target_cc_mno_app_regs=no
    for cand in "-mllvm -sparc-reserve-app-registers" \
		"-mno-app-regs"; do
      if test x"$grub_cv_target_cc_mno_app_regs" != xno ; then
        break
      fi
      CFLAGS="$TARGET_CFLAGS $cand -Werror"
      CPPFLAGS="$TARGET_CPPFLAGS"
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
		        [grub_cv_target_cc_mno_app_regs="$cand"], [])
    done
  ])

  if test x"$grub_cv_target_cc_mno_app_regs" = xno ; then
    AC_MSG_ERROR([could not reserve application registers])
  fi
  if test x"$grub_cv_target_cc_mno_app_regs" = x"-mllvm -sparc-reserve-app-registers" ; then
    # A trick so that clang doesn't see it on link stаge
    TARGET_CPPFLAGS="$TARGET_CPPFLAGS $grub_cv_target_cc_mno_app_regs"
  else
    TARGET_CFLAGS="$TARGET_CFLAGS $grub_cv_target_cc_mno_app_regs"
  fi

  AC_CACHE_CHECK([for no-relax options], grub_cv_target_cc_mno_relax, [
    grub_cv_target_cc_mno_relax=no
    for cand in "-mno-relax" "-Wl,--no-relax"; do
      if test x"$grub_cv_target_cc_mno_relax" != xno ; then
        break
      fi
      LDFLAGS="$TARGET_LDFLAGS $cand -nostdlib -static"
      CFLAGS="$TARGET_CFLAGS -Werror"
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[
	    asm (".globl start; start:");
	    void __main (void);
	    void __main (void) {}
	    int main (void);
	    ]], [[]])], [grub_cv_target_cc_mno_relax="$cand"], [])
    done
  ])
  LDFLAGS="$TARGET_LDFLAGS"
  CFLAGS="$TARGET_CFLAGS"

  if test x"$grub_cv_target_cc_mno_relax" = xno ; then
    AC_MSG_ERROR([could not find no-relax options])
  fi
  TARGET_LDFLAGS="$TARGET_LDFLAGS $grub_cv_target_cc_mno_relax"
fi

# By default, GCC 4.4 generates .eh_frame sections containing unwind
# information in some cases where it previously did not. GRUB doesn't need
# these and they just use up vital space. Restore the old compiler
# behaviour.
AC_CACHE_CHECK([whether -fno-dwarf2-cfi-asm works], [grub_cv_cc_fno_dwarf2_cfi_asm], [
  CFLAGS="$TARGET_CFLAGS -fno-dwarf2-cfi-asm"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
      [grub_cv_cc_fno_dwarf2_cfi_asm=yes],
      [grub_cv_cc_fno_dwarf2_cfi_asm=no])
])

if test "x$grub_cv_cc_fno_dwarf2_cfi_asm" = xyes; then
  TARGET_CFLAGS="$TARGET_CFLAGS -fno-dwarf2-cfi-asm"
fi

if test x"$target_os" = xcygwin; then
  AC_CACHE_CHECK([whether option -fno-reorder-functions works], grub_cv_cc_no_reorder_functions, [
    CFLAGS="$TARGET_CFLAGS -fno-reorder-functions"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
		      [grub_cv_cc_no_reorder_functions=yes],
		      [grub_cv_cc_no_reorder_functions=no])
  ])
fi

if test x"$target_os" = xcygwin && test "x$grub_cv_cc_no_reorder_functions" = xyes; then
  TARGET_CFLAGS="$TARGET_CFLAGS -fno-reorder-functions"
fi

AC_CACHE_CHECK([whether -mno-stack-arg-probe works], [grub_cv_cc_mno_stack_arg_probe], [
  CFLAGS="$TARGET_CFLAGS -mno-stack-arg-probe"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
      [grub_cv_cc_mno_stack_arg_probe=yes],
      [grub_cv_cc_mno_stack_arg_probe=no])
])

if test "x$grub_cv_cc_mno_stack_arg_probe" = xyes; then
  TARGET_CFLAGS="$TARGET_CFLAGS -mno-stack-arg-probe"
fi


# By default, GCC 4.6 generates .eh_frame sections containing unwind
# information in some cases where it previously did not. GRUB doesn't need
# these and they just use up vital space. Restore the old compiler
# behaviour.
AC_CACHE_CHECK([whether -fno-asynchronous-unwind-tables works], [grub_cv_cc_fno_asynchronous_unwind_tables], [
  CFLAGS="$TARGET_CFLAGS -fno-asynchronous-unwind-tables"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
      [grub_cv_cc_fno_asynchronous_unwind_tables=yes],
      [grub_cv_cc_fno_asynchronous_unwind_tables=no])
])

if test "x$grub_cv_cc_fno_asynchronous_unwind_tables" = xyes; then
  TARGET_CFLAGS="$TARGET_CFLAGS -fno-asynchronous-unwind-tables"
fi

AC_CACHE_CHECK([whether -fno-unwind-tables works], [grub_cv_cc_fno_unwind_tables], [
  CFLAGS="$TARGET_CFLAGS -fno-unwind-tables"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
      [grub_cv_cc_fno_unwind_tables=yes],
      [grub_cv_cc_fno_unwind_tables=no])
])

if test "x$grub_cv_cc_fno_unwind_tables" = xyes; then
  TARGET_CFLAGS="$TARGET_CFLAGS -fno-unwind-tables"
fi


CFLAGS="$TARGET_CFLAGS"


if test x"$platform" = xemu ; then
  TARGET_OBJ2ELF=
  grub_cv_target_cc_link_format=
  case "$host_os" in
    *darwin* | *mac*)
       grub_cv_target_cc_link_format="-arch,${target_cpu}"
       TARGET_LDFLAGS="$TARGET_LDFLAGS -Wl,$grub_cv_target_cc_link_format"
        ;;
    *windows* | *cygwin* | *mingw*)
      if test x${target_cpu} = xi386 ; then
        grub_cv_target_cc_link_format=-mi386pe
	TARGET_OBJ2ELF='./build-grub-pe2elf$(BUILD_EXEEXT)'
      fi
      if test x${target_cpu} = xx86_64 ; then
        grub_cv_target_cc_link_format=-mi386pep
	TARGET_OBJ2ELF='./build-grub-pep2elf$(BUILD_EXEEXT)'
      fi
      TARGET_LDFLAGS="$TARGET_LDFLAGS -Wl,$grub_cv_target_cc_link_format"
      ;;
  esac
elif test x"$target_cpu" = xi386 || test x"$target_cpu" = xx86_64; then
  AC_CACHE_CHECK([for target linking format], [grub_cv_target_cc_link_format], [
    grub_cv_target_cc_link_format=unknown
    for format in -melf_${target_cpu} -melf_${target_cpu}_fbsd -melf_${target_cpu}_obsd -melf_${target_cpu}_haiku -mi386pe -mi386pep -arch,${target_cpu}; do
      if test x${target_cpu} != xi386 && test x$format = x-mi386pe; then
        continue
      fi
      if test x${target_cpu} != xx86_64 && test x$format = x-mi386pep; then
        continue
      fi
      CFLAGS="$TARGET_CFLAGS"
      LDFLAGS="$TARGET_LDFLAGS -Wl,$format -nostdlib -static"
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[
      asm (".globl start; start:");
      asm (".globl _start; _start:");
      asm (".globl __start; __start:");
      void __main (void);
      void __main (void) {}
      ]], [[]])], [flag=1], [flag=0])
      if test x"$flag" = x1; then
        grub_cv_target_cc_link_format="$format"
	break
      fi
    done])
  if test x"$grub_cv_target_cc_link_format" = xunknown; then
    AC_MSG_ERROR([no suitable link format found])
  fi
  TARGET_LDFLAGS="$TARGET_LDFLAGS -Wl,$grub_cv_target_cc_link_format"
  if test x"$grub_cv_target_cc_link_format" = x-mi386pe ; then
    TARGET_OBJ2ELF='./build-grub-pe2elf$(BUILD_EXEEXT)'
  fi
  if test x"$grub_cv_target_cc_link_format" = x-mi386pep ; then
    TARGET_OBJ2ELF='./build-grub-pep2elf$(BUILD_EXEEXT)'
  fi
fi

if test x$grub_cv_target_cc_link_format = x-arch,i386 || test x$grub_cv_target_cc_link_format = x-arch,x86_64; then
   TARGET_APPLE_LINKER=1
   AC_CHECK_PROG([TARGET_OBJCONV], [objconv], [objconv], [])
   if test "x$TARGET_OBJCONV" = x ; then
      AC_CHECK_PROG([TARGET_OBJCONV], [objconv], [./objconv], [], [.])
   fi
   if test "x$TARGET_OBJCONV" = x ; then
      AC_MSG_ERROR([objconv not found which is required when building with apple compiler])
   fi
   TARGET_IMG_LDSCRIPT=
   TARGET_IMG_CFLAGS="-static"
   TARGET_IMG_LDFLAGS='-nostdlib -static -Wl,-preload -Wl,-segalign,20'
   TARGET_IMG_LDFLAGS_AC='-nostdlib -static -Wl,-preload -Wl,-segalign,20'
   TARGET_IMG_BASE_LDOPT="-Wl,-image_base"
   TARGET_LDFLAGS_OLDMAGIC=""
elif test x$grub_cv_target_cc_link_format = x-mi386pe || test x$grub_cv_target_cc_link_format = x-mi386pep ; then
  TARGET_APPLE_LINKER=0
  TARGET_LDFLAGS_OLDMAGIC="-Wl,-N"
  TARGET_IMG_LDSCRIPT='$(top_srcdir)'"/${grub_coredir}/conf/i386-cygwin-img-ld.sc"
  TARGET_IMG_LDFLAGS="-Wl,-T${TARGET_IMG_LDSCRIPT}"
  TARGET_IMG_LDFLAGS_AC="-Wl,-T${srcdir}/${grub_coredir}/conf/i386-cygwin-img-ld.sc"
  TARGET_IMG_BASE_LDOPT="-Wl,-Ttext"
  TARGET_IMG_CFLAGS=
else
  TARGET_APPLE_LINKER=0
  TARGET_LDFLAGS_OLDMAGIC="-Wl,-N"
  TARGET_IMG_LDSCRIPT=
  TARGET_IMG_LDFLAGS='-Wl,-N'
  TARGET_IMG_LDFLAGS_AC='-Wl,-N'
  TARGET_IMG_BASE_LDOPT="-Wl,-Ttext"
  TARGET_IMG_CFLAGS=
fi

CFLAGS="$TARGET_CFLAGS"

AC_ARG_ENABLE([efiemu],
	      [AS_HELP_STRING([--enable-efiemu],
                             [build and install the efiemu runtimes (default=guessed)])])
if test x"$enable_efiemu" = xno ; then
  efiemu_excuse="explicitly disabled"
fi

if test x"$grub_cv_target_cc_link_format" = x-mi386pe || test x"$grub_cv_target_cc_link_format" = x-mi386pep ; then
  efiemu_excuse="not available on cygwin"
fi
if test x"$target_cpu" != xi386 ; then
  efiemu_excuse="only available on i386"
fi
if test x"$platform" = xefi ; then
  efiemu_excuse="not available on efi"
fi

if test x"$efiemu_excuse" = x ; then
  AC_CACHE_CHECK([whether options required for efiemu work], grub_cv_cc_efiemu, [
    CFLAGS="-m64 -nostdlib -O2 -mcmodel=large -mno-red-zone"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
		      [grub_cv_cc_efiemu=yes],
		      [grub_cv_cc_efiemu=no])
  ])
  if test x$grub_cv_cc_efiemu = xno; then
     efiemu_excuse="cannot compile with -m64 -mcmodel=large -mno-red-zone -nostdlib"
  fi
fi
if test x"$efiemu_excuse" = x ; then
  AC_CACHE_CHECK([for efiemu64 linking format], [grub_cv_target_cc_efiemu64_link_format], [
    grub_cv_target_cc_efiemu64_link_format=unknown
    for format in -melf_x86_64 -melf_x86_64_fbsd -melf_x86_64_obsd -melf_x86_64_haiku -arch,x86_64; do
      CFLAGS="-m64 -nostdlib -O2 -mcmodel=large -mno-red-zone"
      LDFLAGS="-m64 -Wl,$format -nostdlib -static"
      AC_LINK_IFELSE([AC_LANG_PROGRAM([[
      asm (".globl start; start:");
      asm (".globl _start; _start:");
      asm (".globl __start; __start:");
      void __main (void);
      void __main (void) {}
      ]], [[]])], [flag=1], [flag=0])
      if test x"$flag" = x1; then
        grub_cv_target_cc_efiemu64_link_format="$format"
	break
      fi
    done])
  if test x"$grub_cv_target_cc_efiemu64_link_format" = xunknown; then
    efiemu_excuse="no suitable link format for efiemu64 found"
  else
    EFIEMU64_LINK_FORMAT="-Wl,$grub_cv_target_cc_efiemu64_link_format"
  fi
fi
if test x"$enable_efiemu" = xyes && test x"$efiemu_excuse" != x ; then
  AC_MSG_ERROR([efiemu runtime was explicitly requested but can't be compiled ($efiemu_excuse)])
fi
if test x"$efiemu_excuse" = x ; then
enable_efiemu=yes
else
enable_efiemu=no
fi
AC_SUBST([enable_efiemu])
AC_SUBST([EFIEMU64_LINK_FORMAT])

CFLAGS="$TARGET_CFLAGS"

AC_SUBST(TARGET_LDFLAGS_OLDMAGIC)


LDFLAGS="$TARGET_LDFLAGS"

if test "$target_cpu" = x86_64 || test "$target_cpu" = sparc64 || test "$target_cpu" = riscv64 ; then
  # Use large model to support 4G memory
  AC_CACHE_CHECK([whether option -mcmodel=large works], grub_cv_cc_mcmodel, [
    CFLAGS="$TARGET_CFLAGS -mcmodel=large"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
		      [grub_cv_cc_mcmodel=yes],
		      [grub_cv_cc_mcmodel=no])
  ])
  if test "x$grub_cv_cc_mcmodel" = xyes; then
    TARGET_CFLAGS="$TARGET_CFLAGS -mcmodel=large"
  elif test "$target_cpu" = sparc64 || test "$target_cpu" = riscv64; then
    TARGET_CFLAGS="$TARGET_CFLAGS -mcmodel=medany"
  fi
fi

if test "$target_cpu"-"$platform" = x86_64-efi; then
  # EFI writes to stack below %rsp, we must not use the red zone
  AC_CACHE_CHECK([whether option -mno-red-zone works], grub_cv_cc_no_red_zone, [
    CFLAGS="$TARGET_CFLAGS -mno-red-zone"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
		      [grub_cv_cc_no_red_zone=yes],
		      [grub_cv_cc_no_red_zone=no])
  ])
  if test "x$grub_cv_cc_no_red_zone" = xno; then
    AC_MSG_ERROR([-mno-red-zone not supported, upgrade your gcc])
  fi

  TARGET_CFLAGS="$TARGET_CFLAGS -mno-red-zone"
fi

if test "x$target_cpu" = xarm; then
  AC_CACHE_CHECK([for options to disable movt and movw], grub_cv_target_cc_mno_movt, [
    grub_cv_target_cc_mno_movt=no
    for cand in "-mno-movt" \
		"-mllvm -arm-use-movt=0"; do
      if test x"$grub_cv_target_cc_mno_movt" != xno ; then
        break
      fi
      CFLAGS="$TARGET_CFLAGS $cand -Werror"
      CPPFLAGS="$TARGET_CPPFLAGS"
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
		        [grub_cv_target_cc_mno_movt="$cand"], [])
    done
  ])

  if test x"$grub_cv_target_cc_mno_movt" != xno ; then
    # A trick so that clang doesn't see it on link stage
    TARGET_CPPFLAGS="$TARGET_CPPFLAGS $grub_cv_target_cc_mno_movt"
  fi
  AC_CACHE_CHECK([whether option -mthumb-interwork works], grub_cv_cc_mthumb_interwork, [
    CFLAGS="$TARGET_CFLAGS -mthumb-interwork -Werror"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
		      [grub_cv_cc_mthumb_interwork=yes],
		      [grub_cv_cc_mthumb_interwork=no])
  ])
  if test "x$grub_cv_cc_mthumb_interwork" = xyes; then
    TARGET_CFLAGS="$TARGET_CFLAGS -mthumb-interwork"
  # Clang defaults to thumb interworking
  elif test "x$grub_cv_cc_target_clang" = xno ; then
    AC_MSG_ERROR([your compiler doesn't support -mthumb-interwork])
  fi
fi

AC_CACHE_CHECK([whether option -Qn works], grub_cv_target_cc_qn, [
  CFLAGS="$TARGET_CFLAGS -Qn -Qunused-arguments -Werror"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
		    [grub_cv_target_cc_qn=yes],
		    [grub_cv_target_cc_qn=no])])
if test "x$grub_cv_target_cc_qn" = xyes; then
  TARGET_CFLAGS="$TARGET_CFLAGS -Qn -Qunused-arguments"
fi

#
# Compiler features.
#

CFLAGS="$TARGET_CFLAGS"

# Position independent executable.
grub_CHECK_PIE
grub_CHECK_NO_PIE
grub_CHECK_NO_PIE_ONEWORD
grub_CHECK_LINK_PIE
[# Need that, because some distributions ship compilers that include
# `-fPIE' or '-fpie' and '-pie' in the default specs.
if [ x"$pie_possible" = xyes ]; then
  TARGET_CFLAGS="$TARGET_CFLAGS -fno-PIE -fno-pie"
fi

if [ x"$link_nopie_needed" = xyes ] || [ x"$pie_possible" = xyes ]; then
  if [ x"$nopie_possible" = xyes ]; then
    TARGET_LDFLAGS="$TARGET_LDFLAGS -no-pie"
  fi
  if [ x"$nopie_oneword_possible" = xyes ]; then
    TARGET_LDFLAGS="$TARGET_LDFLAGS -nopie"
  fi
fi]

CFLAGS="$TARGET_CFLAGS"
LDFLAGS="$TARGET_LDFLAGS"

# Position independent executable.
grub_CHECK_PIC
[# On most platforms we don't want PIC as it only makes relocations harder
# and code less efficient. On mips we want to have one got table per module
# and reload $gp in every function.
# GCC implements it using symbol __gnu_local_gp in non-PIC as well.
# However with clang we need PIC for this reloading to happen.
# With arm64 we need relocations that are in some way representable in
# PE as we need to support arm64-efi. Without -fPIC clang generates
# movk's which aren't representable.
# Since default varies across dictributions use either -fPIC or -fno-PIC
# explicitly.
if ( test x$target_cpu = xmips || test x$target_cpu = xmipsel || test x$target_cpu = xarm64 || test x$target_cpu = xmips64el ) && test "x$grub_cv_cc_target_clang" = xyes ; then
   TARGET_CFLAGS="$TARGET_CFLAGS -fPIC"
elif [ x"$pic_possible" = xyes ]; then
   TARGET_CFLAGS="$TARGET_CFLAGS -fno-PIC"
fi
# Don't generate SVR4-style position-independent code for MIPS64.
if test x$target_cpu = xmips64el ; then
   TARGET_CFLAGS="$TARGET_CFLAGS -mno-abicalls"
   TARGET_CCASFLAGS="$TARGET_CCASFLAGS -mno-abicalls"
fi]

CFLAGS="$TARGET_CFLAGS"

# Smashing stack protector.
grub_CHECK_STACK_PROTECTOR
# Need that, because some distributions ship compilers that include
# `-fstack-protector' in the default specs.
if test "x$ssp_possible" = xyes; then
  TARGET_CFLAGS="$TARGET_CFLAGS -fno-stack-protector"
fi

CFLAGS="$TARGET_CFLAGS"

grub_CHECK_STACK_ARG_PROBE
# Cygwin's GCC uses alloca() to probe the stackframe on static
# stack allocations above some threshold.
if test x"$sap_possible" = xyes; then
  TARGET_CFLAGS="$TARGET_CFLAGS -mno-stack-arg-probe"
fi

CFLAGS="$TARGET_CFLAGS"

# -mno-unaligned-access -mstrict-align
if test "$target_cpu" = arm; then
  AC_CACHE_CHECK([for compile options to get strict alignment], [grub_cv_target_cc_strict_align], [
    grub_cv_target_cc_strict_align=
    for arg in -mno-unaligned-access "-Xclang -mstrict-align" -mstrict-align; do
      CFLAGS="$TARGET_CFLAGS $arg -Werror"
      LDFLAGS="$TARGET_LDFLAGS"
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])], [flag=1], [flag=0])
      if test x"$flag" = x1; then
        grub_cv_target_cc_strict_align="$arg"
	break
      fi
    done])

  TARGET_CFLAGS="$TARGET_CFLAGS $grub_cv_target_cc_strict_align"
  if test x"$grub_cv_target_cc_strict_align" = x"-Xclang -mstrict-align"; then
    TARGET_LDFLAGS="$TARGET_LDFLAGS -Qunused-arguments"
  fi
  AC_CACHE_CHECK([if compiler generates unaligned accesses], [grub_cv_cc_target_emits_unaligned],
  [CFLAGS="$TARGET_CFLAGS"
   AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [[
#ifdef __ARM_FEATURE_UNALIGNED
#error "unaligned"
#endif
     ]])],
     [grub_cv_cc_target_emits_unaligned=no], [grub_cv_cc_target_emits_unaligned=yes])])
  if test x$grub_cv_cc_target_emits_unaligned = xyes; then
    AC_MSG_ERROR([compiler generates unaligned accesses])
  fi
fi

# Set them to their new values for the tests below.
CC="$TARGET_CC"
if test x"$platform" = xemu ; then
CFLAGS="$TARGET_CFLAGS -Wno-error"
elif test "x$TARGET_APPLE_LINKER" = x1 ; then
CFLAGS="$TARGET_CFLAGS -nostdlib -static -Wno-error"
else
CFLAGS="$TARGET_CFLAGS -nostdlib -Wno-error"
fi
CPPFLAGS="$TARGET_CPPFLAGS"

grub_ASM_USCORE
if test "x$TARGET_APPLE_LINKER" = x0 && test x"$platform" != xemu; then
if test x$grub_cv_asm_uscore = xyes; then
DEFSYM="-Wl,--defsym,_abort=_main -Wl,--defsym,__main=_main"
else
DEFSYM="-Wl,--defsym,abort=main -Wl,--defsym,_main=main -Wl,--defsym,__main=main"
fi
CFLAGS="$TARGET_CFLAGS -nostdlib $DEFSYM"
fi

# Check for libgcc symbols
if test x"$platform" = xemu; then
AC_CHECK_FUNCS(__udivsi3 __umodsi3 __divsi3 __modsi3 __divdi3 __moddi3 __udivdi3 __umoddi3 __ctzdi2 __ctzsi2 __aeabi_uidiv __aeabi_uidivmod __aeabi_idiv __aeabi_idivmod __aeabi_ulcmp __muldi3 __aeabi_lmul __aeabi_memcpy __aeabi_memcpy4 __aeabi_memcpy8 __aeabi_memclr __aeabi_memclr4 __aeabi_memclr8 __aeabi_memset __aeabi_lasr __aeabi_llsl __aeabi_llsr _restgpr_14_x __ucmpdi2 __ashldi3 __ashrdi3 __lshrdi3 __bswapsi2 __bswapdi2 __bzero __register_frame_info __deregister_frame_info ___chkstk_ms __chkstk_ms)
fi

if test "x$TARGET_APPLE_LINKER" = x1 ; then
CFLAGS="$TARGET_CFLAGS -nostdlib -static"
else
CFLAGS="$TARGET_CFLAGS -nostdlib"
fi
LIBS=""

# Defined in aclocal.m4.
grub_PROG_TARGET_CC
if test "x$TARGET_APPLE_LINKER" != x1 ; then
grub_PROG_OBJCOPY_ABSOLUTE
fi
grub_PROG_LD_BUILD_ID_NONE
if test "x$target_cpu" = xi386; then
  if test "$platform" != emu && test "x$TARGET_APPLE_LINKER" != x1 ; then
    if test ! -z "$TARGET_IMG_LDSCRIPT"; then
      # Check symbols provided by linker script.
      CFLAGS="$TARGET_CFLAGS -nostdlib ${TARGET_IMG_LDFLAGS_AC} ${TARGET_IMG_BASE_LDOPT},0x8000"
    fi
    grub_CHECK_BSS_START_SYMBOL
    grub_CHECK_END_SYMBOL
  fi
  CFLAGS="$TARGET_CFLAGS"
fi

grub_PROG_NM_WORKS
grub_PROG_NM_MINUS_P
grub_PROG_NM_DEFINED_ONLY
AC_SUBST(TARGET_NMFLAGS_MINUS_P)
AC_SUBST(TARGET_NMFLAGS_DEFINED_ONLY)

if test "$platform" != emu; then
AC_CACHE_CHECK([whether -nostdinc -isystem works], [grub_cv_cc_isystem], [
  SAVED_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$TARGET_CPPFLAGS -nostdlib -nostdinc -isystem `$TARGET_CC -print-file-name=include`"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdarg.h>
#include <stddef.h>
int va_arg_func (int fixed, va_list args);]], [[]])],
      [grub_cv_cc_isystem=yes],
      [grub_cv_cc_isystem=no])
  CPPFLAGS="$SAVED_CPPFLAGS"
])

if test x"$grub_cv_cc_isystem" = xyes ; then
  TARGET_CPPFLAGS="$TARGET_CPPFLAGS -nostdinc -isystem `$TARGET_CC -print-file-name=include`"
fi
fi

AC_CACHE_CHECK([whether -Wtrampolines work], [grub_cv_cc_wtrampolines], [
  CFLAGS="$TARGET_CFLAGS -Wtrampolines -Werror"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdarg.h>
int va_arg_func (int fixed, va_list args);]], [[]])],
      [grub_cv_cc_wtrampolines=yes],
      [grub_cv_cc_wtrampolines=no])
])

if test x"$grub_cv_cc_wtrampolines" = xyes ; then
  TARGET_CFLAGS="$TARGET_CFLAGS -Wtrampolines"
fi

# Restore the flags.
CC="$tmp_CC"
CFLAGS="$tmp_CFLAGS"
CPPFLAGS="$tmp_CPPFLAGS"
LDFLAGS="$tmp_LDFLAGS"
LIBS="$tmp_LIBS"

#
# Check for options.
#

# Memory manager debugging.
AC_ARG_ENABLE([mm-debug],
	      AS_HELP_STRING([--enable-mm-debug],
                             [include memory manager debugging]),
              [AC_DEFINE([MM_DEBUG], [1],
                         [Define to 1 if you enable memory manager debugging.])])

AC_ARG_ENABLE([cache-stats],
	      AS_HELP_STRING([--enable-cache-stats],
                             [enable disk cache statistics collection]))

if test x$enable_cache_stats = xyes; then
  DISK_CACHE_STATS=1
else
  DISK_CACHE_STATS=0
fi
AC_SUBST([DISK_CACHE_STATS])

AC_ARG_ENABLE([boot-time],
	      AS_HELP_STRING([--enable-boot-time],
                             [enable boot time statistics collection]))

if test x$enable_boot_time = xyes; then
  BOOT_TIME_STATS=1
else
  BOOT_TIME_STATS=0
fi
AC_SUBST([BOOT_TIME_STATS])

AC_ARG_ENABLE([grub-emu-sdl],
	      [AS_HELP_STRING([--enable-grub-emu-sdl],
                             [build and install the `grub-emu' debugging utility with SDL support (default=guessed)])])

AC_ARG_ENABLE([grub-emu-pci],
	      [AS_HELP_STRING([--enable-grub-emu-pci],
                             [build and install the `grub-emu' debugging utility with PCI support (potentially dangerous) (default=no)])])

if test "$platform" = emu; then

if test x"$enable_grub_emu_sdl" = xno ; then
  grub_emu_sdl_excuse="explicitly disabled"
fi
[if [ x"$grub_emu_sdl_excuse" = x ]; then
    # Check for libSDL libraries.]
AC_CHECK_LIB([SDL], [SDL_Init], [LIBSDL="-lSDL"],
    [grub_emu_sdl_excuse=["libSDL libraries are required to build \`grub-emu' with SDL support"]])
    AC_SUBST([LIBSDL])
[fi]

[if [ x"$grub_emu_sdl_excuse" = x ]; then
    # Check for headers.]
    AC_CHECK_HEADERS([SDL/SDL.h], [],
      [grub_emu_sdl_excuse=["libSDL header file is required to build \`grub-emu' with SDL support"]])
[fi]

if test x"enable_grub_emu_sdl" = xyes && test x"$grub_emu_sdl_excuse" != x ; then
  AC_MSG_ERROR([SDL support for grub-emu was explicitly requested but can't be compiled ($grub_emu_sdl_excuse)])
fi
if test x"$grub_emu_sdl_excuse" = x ; then
enable_grub_emu_sdl=yes
else
enable_grub_emu_sdl=no
fi

if test x"$enable_grub_emu_pci" != xyes ; then
   grub_emu_pci_excuse="not enabled"
fi

[if [ x"$grub_emu_pci_excuse" = x ]; then
      # Check for libpci libraries.]
   AC_CHECK_LIB([pciaccess], [pci_system_init], [LIBPCIACCESS="-lpciaccess"],
      [grub_emu_pci_excuse=["need libpciaccess library"]])
    AC_SUBST([LIBPCIACCESS])
[fi]
[if [ x"$grub_emu_pci_excuse" = x ]; then
    # Check for headers.]
    AC_CHECK_HEADERS([pciaccess.h], [],
      [grub_emu_pci_excuse=["need libpciaccess headers"]])
[fi]

if test x"$grub_emu_pci_excuse" = x ; then
enable_grub_emu_pci=yes
else

enable_grub_emu_pci=no
fi

AC_SUBST([enable_grub_emu_sdl])
AC_SUBST([enable_grub_emu_pci])

else

# Ignore --enable-emu-* if platform is not emu
enable_grub_emu_sdl=no
enable_grub_emu_pci=no
fi

AC_ARG_ENABLE([grub-mkfont],
	      [AS_HELP_STRING([--enable-grub-mkfont],
                             [build and install the `grub-mkfont' utility (default=guessed)])])
if test x"$enable_grub_mkfont" = xno ; then
  grub_mkfont_excuse="explicitly disabled"
fi

unset ac_cv_header_ft2build_h

if test x"$grub_mkfont_excuse" = x ; then
  # Check for freetype libraries.
  PKG_CHECK_MODULES([FREETYPE], [freetype2], [
    SAVED_CPPFLAGS="$CPPFLAGS"
    SAVED_LIBS="$LIBS"
    CPPFLAGS="$CPPFLAGS $FREETYPE_CFLAGS"
    LIBS="$LIBS $FREETYPE_LIBS"
    AC_CHECK_HEADERS([ft2build.h], [],
      [grub_mkfont_excuse=["need freetype2 headers"]])
    AC_LINK_IFELSE([AC_LANG_CALL([], [FT_Load_Glyph])], [],
      [grub_mkfont_excuse=["freetype2 library unusable"]])
    CPPFLAGS="$SAVED_CPPFLAGS"
    LIBS="$SAVED_LIBS"
  ], [grub_mkfont_excuse=["need freetype2 library"]])
fi

if test x"$enable_grub_mkfont" = xyes && test x"$grub_mkfont_excuse" != x ; then
  AC_MSG_ERROR([grub-mkfont was explicitly requested but can't be compiled ($grub_mkfont_excuse)])
fi
if test x"$grub_mkfont_excuse" = x ; then
enable_grub_mkfont=yes
else
enable_grub_mkfont=no
fi
AC_SUBST([enable_grub_mkfont])

SAVED_CC="$CC"
SAVED_CPP="$CPP"
SAVED_CFLAGS="$CFLAGS"
SAVED_CPPFLAGS="$CPPFLAGS"
SAVED_LDFLAGS="$LDFLAGS"
CC="$BUILD_CC"
CPP="$BUILD_CPP"
CFLAGS="$BUILD_CFLAGS"
CPPFLAGS="$BUILD_CPPFLAGS"
LDFLAGS="$BUILD_LDFAGS"

unset ac_cv_c_bigendian
unset ac_cv_header_ft2build_h

AC_COMPUTE_INT([BUILD_SIZEOF_VOID_P], [sizeof (void *)])
AC_COMPUTE_INT([BUILD_SIZEOF_LONG], [sizeof (long)])
AC_C_BIGENDIAN([BUILD_WORDS_BIGENDIAN=1], [BUILD_WORDS_BIGENDIAN=0], [BUILD_WORDS_BIGENDIAN=err], [BUILD_WORDS_BIGENDIAN=err])

if test x$BUILD_WORDS_BIGENDIAN = xerr ; then
   AC_MSG_ERROR([couldnt determine build endianness])
fi

AC_SUBST([BUILD_SIZEOF_LONG])
AC_SUBST([BUILD_SIZEOF_VOID_P])
AC_SUBST([BUILD_WORDS_BIGENDIAN])

if test x"$grub_build_mkfont_excuse" = x ; then
  # Check for freetype libraries.
  SAVED_PKG_CONFIG="$PKG_CONFIG"
  test -z "$BUILD_PKG_CONFIG" || PKG_CONFIG="$BUILD_PKG_CONFIG"
  PKG_CHECK_MODULES([BUILD_FREETYPE], [freetype2], [
    SAVED_CPPFLAGS_2="$CPPFLAGS"
    SAVED_LIBS="$LIBS"
    CPPFLAGS="$CPPFLAGS $BUILD_FREETYPE_CFLAGS"
    LIBS="$LIBS $BUILD_FREETYPE_LIBS"
    AC_CHECK_HEADERS([ft2build.h], [],
      [grub_build_mkfont_excuse=["need freetype2 headers"]])
    AC_LINK_IFELSE([AC_LANG_CALL([], [FT_Load_Glyph])], [],
      [grub_build_mkfont_excuse=["freetype2 library unusable"]])
    LIBS="$SAVED_LIBS"
    CPPFLAGS="$SAVED_CPPFLAGS_2"
  ], [grub_build_mkfont_excuse=["need freetype2 library"]])
  PKG_CONFIG="$SAVED_PKG_CONFIG"
fi

if test x"$enable_build_grub_mkfont" = xyes && test x"$grub_build_mkfont_excuse" != x ; then
  AC_MSG_ERROR([build-grub-mkfont was explicitly requested but can't be compiled ($grub_build_mkfont_excuse)])
fi
if test x"$grub_build_mkfont_excuse" = x ; then
  enable_build_grub_mkfont=yes
else
  enable_build_grub_mkfont=no
fi
if test x"$enable_build_grub_mkfont" = xno && ( test "x$platform" = xqemu || test "x$platform" = xloongson || test "x$platform" = xqemu_mips || test "x$platform" = xcoreboot ); then
  if test x"$grub_build_mkfont_excuse" = x ; then
    AC_MSG_ERROR([qemu, coreboot and loongson ports need build-time grub-mkfont])
  else
    AC_MSG_ERROR([qemu, coreboot and loongson ports need build-time grub-mkfont ($grub_build_mkfont_excuse)])
  fi
fi

CC="$SAVED_CC"
CPP="$SAVED_CPP"
CFLAGS="$SAVED_CFLAGS"
CPPFLAGS="$SAVED_CPPFLAGS"
LDFLAGS="$SAVED_LDFLAGS"


DJVU_FONT_SOURCE=

starfield_excuse=

AC_ARG_ENABLE([grub-themes],
	      [AS_HELP_STRING([--enable-grub-themes],
                             [build and install GRUB themes (default=guessed)])])
if test x"$enable_grub_themes" = xno ; then
  starfield_excuse="explicitly disabled"
fi

if test x"$starfield_excuse" = x && test x"$enable_build_grub_mkfont" = xno ; then
  starfield_excuse="No build-time grub-mkfont"
fi

if test x"$starfield_excuse" = x; then
   for ext in pcf pcf.gz bdf bdf.gz ttf ttf.gz; do
     for dir in . /usr/src /usr/share/fonts/X11/misc /usr/share/fonts/truetype/ttf-dejavu /usr/share/fonts/dejavu /usr/share/fonts/truetype; do
        if test -f "$dir/DejaVuSans.$ext"; then
          DJVU_FONT_SOURCE="$dir/DejaVuSans.$ext"
          break 2
        fi
     done
   done

   if test "x$DJVU_FONT_SOURCE" = x; then
     starfield_excuse="No DejaVu found"
   fi
fi

if test x"$enable_grub_themes" = xyes &&  test x"$starfield_excuse" != x; then
   AC_MSG_ERROR([themes were explicitly requested but requirements are not satisfied ($starfield_excuse)])
fi

AC_SUBST([DJVU_FONT_SOURCE])

FONT_SOURCE=

for ext in pcf pcf.gz bdf bdf.gz ttf ttf.gz; do
  for dir in . /usr/src /usr/share/fonts/X11/misc /usr/share/fonts/unifont /usr/share/fonts/uni /usr/share/fonts/truetype/unifont /usr/share/fonts/misc; do
    if test -f "$dir/unifont.$ext"; then
      md5="$(md5sum "$dir/unifont.$ext"|awk '{ print $1; }')"
      # PCF and BDF from version 6.3 isn't hanled properly by libfreetype.
      if test "$md5" = 0a54834d2788c83886a3e1785a6a1e61 || test "$md5" = 28f2565c7a41d8d407e2551159385edb || test "$md5" = dae5e588461b3b92b87b6ffee734f936 || test "$md5" = 4a3d687aa5bb329ed05f4263a1016791 ; then
        continue
      fi
      FONT_SOURCE="$dir/unifont.$ext"
      break 2
    fi
  done
done

if test x"$enable_build_grub_mkfont" = xno ; then
  FONT_SOURCE=
fi

if test "x$FONT_SOURCE" = x && ( test "x$platform" = xqemu || test "x$platform" = xloongson || test "x$platform" = xqemu_mips || test "x$platform" = xcoreboot ); then
  if test x"$grub_build_mkfont_excuse" = x ; then
    AC_MSG_ERROR([qemu, coreboot and loongson ports need unifont])
  else
    AC_MSG_ERROR([qemu, coreboot and loongson ports need unifont ($grub_build_mkfont_excuse)])
  fi
fi

AC_SUBST([FONT_SOURCE])

if test x"$FONT_SOURCE" = x &&  test x"$DJVU_FONT_SOURCE" = x && test x"$grub_build_mkfont_excuse" = x; then
  grub_build_mkfont_excuse="no fonts"
fi


AC_ARG_ENABLE([grub-mount],
	      [AS_HELP_STRING([--enable-grub-mount],
                             [build and install the `grub-mount' utility (default=guessed)])])
if test x"$enable_grub_mount" = xno ; then
  grub_mount_excuse="explicitly disabled"
fi

if test x"$grub_mount_excuse" = x ; then
  AC_CHECK_LIB([fuse], [fuse_main_real], [],
               [grub_mount_excuse="need FUSE library"])
fi

if test x"$grub_mount_excuse" = x ; then
  # Check for fuse headers.
  SAVED_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS -DFUSE_USE_VERSION=26"
  AC_CHECK_HEADERS([fuse/fuse.h], [],
  	[grub_mount_excuse=["need FUSE headers"]])
  CPPFLAGS="$SAVED_CPPFLAGS"
fi

if test x"$enable_grub_mount" = xyes && test x"$grub_mount_excuse" != x ; then
  AC_MSG_ERROR([grub-mount was explicitly requested but can't be compiled ($grub_mount_excuse)])
fi
if test x"$grub_mount_excuse" = x ; then
enable_grub_mount=yes
else
enable_grub_mount=no
fi
AC_SUBST([enable_grub_mount])

AC_ARG_ENABLE([device-mapper],
              [AS_HELP_STRING([--enable-device-mapper],
                              [enable Linux device-mapper support (default=guessed)])])
if test x"$enable_device_mapper" = xno ; then
  device_mapper_excuse="explicitly disabled"
fi

if test x"$device_mapper_excuse" = x ; then
  # Check for device-mapper header.
  AC_CHECK_HEADER([libdevmapper.h], [],
               [device_mapper_excuse="need libdevmapper header"])
fi

if test x"$device_mapper_excuse" = x ; then
  # Check for device-mapper library.
  AC_CHECK_LIB([devmapper], [dm_task_create], [],
               [device_mapper_excuse="need devmapper library"])
fi

if test x"$device_mapper_excuse" = x ; then
  # Check for device-mapper library.
  AC_CHECK_LIB([devmapper], [dm_log_with_errno_init],
               [],
               [device_mapper_excuse="need devmapper library"])
fi

if test x"$device_mapper_excuse" = x ; then
   LIBDEVMAPPER="-ldevmapper"
   AC_DEFINE([HAVE_DEVICE_MAPPER], [1],
             [Define to 1 if you have the devmapper library.])
fi

AC_SUBST([LIBDEVMAPPER])

LIBGEOM=
if test x$host_kernel = xkfreebsd; then
  AC_CHECK_LIB([geom], [geom_gettree], [],
               [AC_MSG_ERROR([Your platform requires libgeom])])
  LIBGEOM="-lgeom"
fi

AC_SUBST([LIBGEOM])

AC_ARG_ENABLE([liblzma],
              [AS_HELP_STRING([--enable-liblzma],
                              [enable liblzma integration (default=guessed)])])
if test x"$enable_liblzma" = xno ; then
  liblzma_excuse="explicitly disabled"
fi

if test x"$liblzma_excuse" = x ; then
AC_CHECK_LIB([lzma], [lzma_code],
             [],[liblzma_excuse="need lzma library"])
fi
if test x"$liblzma_excuse" = x ; then
AC_CHECK_HEADER([lzma.h], [], [liblzma_excuse="need lzma header"])
fi

if test x"$enable_liblzma" = xyes && test x"$liblzma_excuse" != x ; then
  AC_MSG_ERROR([liblzma support was explicitly requested but requirements are not satisfied ($liblzma_excuse)])
fi


if test x"$liblzma_excuse" = x ; then
   LIBLZMA="-llzma"
   AC_DEFINE([USE_LIBLZMA], [1],
   	     [Define to 1 if you have the LZMA library.])
fi

AC_SUBST([LIBLZMA])

AC_ARG_ENABLE([libzfs],
              [AS_HELP_STRING([--enable-libzfs],
                              [enable libzfs integration (default=guessed)])])
if test x"$enable_libzfs" = xno ; then
  libzfs_excuse="explicitly disabled"
fi

if test x"$libzfs_excuse" = x ; then
  # Only check for system headers if libzfs support has not been disabled.
  AC_CHECK_HEADERS(libzfs.h libnvpair.h)
fi

if test x"$libzfs_excuse" = x ; then
  AC_CHECK_LIB([zfs], [libzfs_init],
               [],
               [libzfs_excuse="need zfs library"])
fi

if test x"$libzfs_excuse" = x ; then
  AC_CHECK_LIB([nvpair], [nvlist_lookup_string],
               [],
               [libzfs_excuse="need nvpair library"])
fi

if test x"$enable_libzfs" = xyes && test x"$libzfs_excuse" != x ; then
  AC_MSG_ERROR([libzfs support was explicitly requested but requirements are not satisfied ($libzfs_excuse)])
fi

if test x"$libzfs_excuse" = x ; then
  # We need both libzfs and libnvpair for a successful build.
  LIBZFS="-lzfs"
  AC_DEFINE([HAVE_LIBZFS], [1],
            [Define to 1 if you have the ZFS library.])
  LIBNVPAIR="-lnvpair"
  AC_DEFINE([HAVE_LIBNVPAIR], [1],
            [Define to 1 if you have the NVPAIR library.])
fi

AC_SUBST([LIBZFS])
AC_SUBST([LIBNVPAIR])

LIBS=""

AC_SUBST([FONT_SOURCE])
AS_IF([test x$target_cpu = xi386 -a x$platform = xqemu],
	    [AC_SUBST([GRUB_BOOT_MACHINE_LINK_ADDR], 0xffe00)])

AC_SUBST(HAVE_ASM_USCORE)
AC_SUBST(BSS_START_SYMBOL)
AC_SUBST(END_SYMBOL)
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)

AC_ARG_ENABLE([werror],
	      [AS_HELP_STRING([--disable-werror],
                             [do not use -Werror when building GRUB])])
if test x"$enable_werror" != xno ; then
  TARGET_CFLAGS="$TARGET_CFLAGS -Werror"
  HOST_CFLAGS="$HOST_CFLAGS -Werror"
fi

TARGET_CPP="$TARGET_CC -E"
TARGET_CCAS=$TARGET_CC

# Includes which include make-time substitutions. They must come last
# as to avoid executing top_builddir in shell.
HOST_CPPFLAGS="$HOST_CPPFLAGS -I\$(top_builddir)/include"
TARGET_CPPFLAGS="$TARGET_CPPFLAGS -I\$(top_srcdir)/include"
TARGET_CPPFLAGS="$TARGET_CPPFLAGS -I\$(top_builddir)/include"

GRUB_TARGET_CPU="${target_cpu}"
GRUB_PLATFORM="${platform}"

AC_SUBST(GRUB_TARGET_CPU)
AC_SUBST(GRUB_PLATFORM)

AC_SUBST(TARGET_OBJCONV)
AC_SUBST(TARGET_CPP)
AC_SUBST(TARGET_CCAS)
AC_SUBST(TARGET_OBJ2ELF)
AC_SUBST(TARGET_MODULE_FORMAT)
AC_SUBST(TARGET_CC_VERSION)

AC_SUBST(TARGET_CFLAGS)
AC_SUBST(TARGET_LDFLAGS)
AC_SUBST(TARGET_CPPFLAGS)
AC_SUBST(TARGET_CCASFLAGS)

AC_SUBST(TARGET_IMG_LDFLAGS)
AC_SUBST(TARGET_IMG_CFLAGS)
AC_SUBST(TARGET_IMG_BASE_LDOPT)
AC_SUBST(TARGET_APPLE_LINKER)

AC_SUBST(HOST_CFLAGS)
AC_SUBST(HOST_LDFLAGS)
AC_SUBST(HOST_CPPFLAGS)
AC_SUBST(HOST_CCASFLAGS)

AC_SUBST(BUILD_LIBM)

#
# Automake conditionals
#

AM_CONDITIONAL([COND_real_platform], [test x$platform != xnone])
AM_CONDITIONAL([COND_emu], [test x$platform = xemu])
AM_CONDITIONAL([COND_i386_pc], [test x$target_cpu = xi386 -a x$platform = xpc])
AM_CONDITIONAL([COND_i386_efi], [test x$target_cpu = xi386 -a x$platform = xefi])
AM_CONDITIONAL([COND_ia64_efi], [test x$target_cpu = xia64 -a x$platform = xefi])
AM_CONDITIONAL([COND_i386_qemu], [test x$target_cpu = xi386 -a x$platform = xqemu])
AM_CONDITIONAL([COND_i386_ieee1275], [test x$target_cpu = xi386 -a x$platform = xieee1275])
AM_CONDITIONAL([COND_i386_coreboot], [test x$target_cpu = xi386 -a x$platform = xcoreboot])
AM_CONDITIONAL([COND_i386_multiboot], [test x$target_cpu = xi386 -a x$platform = xmultiboot])
AM_CONDITIONAL([COND_x86_64_efi], [test x$target_cpu = xx86_64 -a x$platform = xefi])
AM_CONDITIONAL([COND_i386_xen], [test x$target_cpu = xi386 -a x$platform = xxen])
AM_CONDITIONAL([COND_i386_xen_pvh], [test x$target_cpu = xi386 -a x$platform = xxen_pvh])
AM_CONDITIONAL([COND_x86_64_xen], [test x$target_cpu = xx86_64 -a x$platform = xxen])
AM_CONDITIONAL([COND_mips64_efi], [test x$target_cpu = xmips64el -a x$platform = xefi])
AM_CONDITIONAL([COND_mips_loongson], [test x$target_cpu = xmipsel -a x$platform = xloongson])
AM_CONDITIONAL([COND_mips_qemu_mips], [test "(" x$target_cpu = xmips -o x$target_cpu = xmipsel ")"  -a x$platform = xqemu_mips])
AM_CONDITIONAL([COND_mips_arc], [test "(" x$target_cpu = xmips -o x$target_cpu = xmipsel ")"  -a x$platform = xarc])
AM_CONDITIONAL([COND_sparc64_ieee1275], [test x$target_cpu = xsparc64 -a x$platform = xieee1275])
AM_CONDITIONAL([COND_sparc64_emu], [test x$target_cpu = xsparc64 -a x$platform = xemu])
AM_CONDITIONAL([COND_powerpc_ieee1275], [test x$target_cpu = xpowerpc -a x$platform = xieee1275])
AM_CONDITIONAL([COND_mips64el], [test x$target_cpu = xmips64el])
AM_CONDITIONAL([COND_mips], [test x$target_cpu = xmips -o x$target_cpu = xmipsel])
AM_CONDITIONAL([COND_mipsel], [test x$target_cpu = xmipsel])
AM_CONDITIONAL([COND_mipseb], [test x$target_cpu = xmips])
AM_CONDITIONAL([COND_arm], [test x$target_cpu = xarm ])
AM_CONDITIONAL([COND_arm_uboot], [test x$target_cpu = xarm -a x$platform = xuboot])
AM_CONDITIONAL([COND_arm_coreboot], [test x$target_cpu = xarm -a x$platform = xcoreboot])
AM_CONDITIONAL([COND_arm_efi], [test x$target_cpu = xarm -a x$platform = xefi])
AM_CONDITIONAL([COND_arm64], [test x$target_cpu = xarm64 ])
AM_CONDITIONAL([COND_arm64_efi], [test x$target_cpu = xarm64 -a x$platform = xefi])
AM_CONDITIONAL([COND_riscv32], [test x$target_cpu = xriscv32 ])
AM_CONDITIONAL([COND_riscv64], [test x$target_cpu = xriscv64 ])
AM_CONDITIONAL([COND_riscv32_efi], [test x$target_cpu = xriscv32 -a x$platform = xefi])
AM_CONDITIONAL([COND_riscv64_efi], [test x$target_cpu = xriscv64 -a x$platform = xefi])

AM_CONDITIONAL([COND_HOST_HURD], [test x$host_kernel = xhurd])
AM_CONDITIONAL([COND_HOST_LINUX], [test x$host_kernel = xlinux])
AM_CONDITIONAL([COND_HOST_NETBSD], [test x$host_kernel = xnetbsd])
AM_CONDITIONAL([COND_HOST_WINDOWS], [test x$host_kernel = xwindows])
AM_CONDITIONAL([COND_HOST_KFREEBSD], [test x$host_kernel = xkfreebsd])
AM_CONDITIONAL([COND_HOST_XNU], [test x$host_kernel = xxnu])
AM_CONDITIONAL([COND_HOST_ILLUMOS], [test x$host_kernel = xillumos])

AM_CONDITIONAL([COND_MAN_PAGES], [test x$cross_compiling = xno -a x$HELP2MAN != x])
AM_CONDITIONAL([COND_GRUB_EMU_SDL], [test x$enable_grub_emu_sdl = xyes])
AM_CONDITIONAL([COND_GRUB_EMU_PCI], [test x$enable_grub_emu_pci = xyes])
AM_CONDITIONAL([COND_GRUB_MKFONT], [test x$enable_grub_mkfont = xyes])
AM_CONDITIONAL([COND_GRUB_MOUNT], [test x$enable_grub_mount = xyes])
AM_CONDITIONAL([COND_HAVE_FONT_SOURCE], [test x$FONT_SOURCE != x])
if test x$FONT_SOURCE != x ; then
   HAVE_FONT_SOURCE=1
else
   HAVE_FONT_SOURCE=0
fi
AC_SUBST(HAVE_FONT_SOURCE)
AM_CONDITIONAL([COND_APPLE_LINKER], [test x$TARGET_APPLE_LINKER = x1])
AM_CONDITIONAL([COND_ENABLE_EFIEMU], [test x$enable_efiemu = xyes])
AM_CONDITIONAL([COND_ENABLE_CACHE_STATS], [test x$DISK_CACHE_STATS = x1])
AM_CONDITIONAL([COND_ENABLE_BOOT_TIME_STATS], [test x$BOOT_TIME_STATS = x1])

AM_CONDITIONAL([COND_HAVE_CXX], [test x$HAVE_CXX = xyes])

AM_CONDITIONAL([COND_HAVE_ASM_USCORE], [test x$HAVE_ASM_USCORE = x1])
AM_CONDITIONAL([COND_STARFIELD], [test "x$starfield_excuse" = x])
AM_CONDITIONAL([COND_HAVE_EXEC], [test "x$have_exec" = xy])

test "x$prefix" = xNONE && prefix="$ac_default_prefix"
test "x$exec_prefix" = xNONE && exec_prefix="${prefix}"
datarootdir="$(eval echo "$datarootdir")"
grub_libdir="$(eval echo "$libdir")"
grub_localedir="$(eval echo "$localedir")"
grub_datadir="$(eval echo "$datadir")"
grub_sysconfdir="$(eval echo "$sysconfdir")"
AC_DEFINE_UNQUOTED(LOCALEDIR, "$grub_localedir", [Locale dir])
AC_DEFINE_UNQUOTED(GRUB_LIBDIR, "$grub_libdir", [Library dir])
AC_DEFINE_UNQUOTED(GRUB_DATADIR, "$grub_datadir", [Data dir])
AC_DEFINE_UNQUOTED(GRUB_SYSCONFDIR, "$grub_sysconfdir", [Configuration dir])


# Output files.
if test "$platform" != none; then
  cpudir="${target_cpu}"
  if test x${cpudir} = xmips64el; then
    cpudir=mips64;
  elif test x${cpudir} = xmipsel; then
    cpudir=mips;
  fi
  grub_CHECK_LINK_DIR
  if test x"$link_dir" = xyes ; then
    AC_CONFIG_LINKS([include/grub/cpu:include/grub/$cpudir])
    if test "$platform" != emu ; then
      AC_CONFIG_LINKS([include/grub/machine:include/grub/$cpudir/$platform])
    fi
  else
    mkdir -p include/grub 2>/dev/null
    rm -rf include/grub/cpu
    cp -rp $srcdir/include/grub/$cpudir include/grub/cpu 2>/dev/null
    if test "$platform" != emu ; then
      rm -rf include/grub/machine
      cp -rp $srcdir/include/grub/$cpudir/$platform include/grub/machine 2>/dev/null
    fi
  fi
else
  # Just enough to stop the compiler failing with -I$(srcdir)/include.
  mkdir -p include 2>/dev/null
  rm -rf include/grub/cpu include/grub/machine
fi

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([grub-core/Makefile])
AC_CONFIG_FILES([grub-core/lib/gnulib/Makefile])
AC_CONFIG_FILES([po/Makefile.in])
AC_CONFIG_FILES([docs/Makefile])
AC_CONFIG_FILES([util/bash-completion.d/Makefile])
AC_CONFIG_FILES([stamp-h], [echo timestamp > stamp-h])
AC_CONFIG_FILES([config.h])

AC_OUTPUT
[
echo "*******************************************************"
echo GRUB2 will be compiled with following components:
echo Platform: "$target_cpu"-"$platform"
if [ x"$platform" = xemu ]; then
if [ x"$grub_emu_sdl_excuse" = x ]; then
echo SDL support for grub-emu: Yes
else
echo SDL support for grub-emu: No "($grub_emu_sdl_excuse)"
fi
if [ x"$grub_emu_pci_excuse" = x ]; then
echo PCI support for grub-emu: Yes
else
echo PCI support for grub-emu: No "($grub_emu_pci_excuse)"
fi
fi
if test x"$device_mapper_excuse" = x ; then
echo With devmapper support: Yes
else
echo With devmapper support: No "($device_mapper_excuse)"
fi
if [ x"$enable_mm_debug" = xyes ]; then
echo With memory debugging: Yes
else
echo With memory debugging: No
fi
if [ x"$enable_cache_stats" = xyes ]; then
echo With disk cache statistics: Yes
else
echo With disk cache statistics: No
fi

if [ x"$enable_boot_time" = xyes ]; then
echo With boot time statistics: Yes
else
echo With boot time statistics: No
fi

if [ x"$efiemu_excuse" = x ]; then
echo efiemu runtime: Yes
else
echo efiemu runtime: No "($efiemu_excuse)"
fi
if [ x"$grub_mkfont_excuse" = x ]; then
echo grub-mkfont: Yes
else
echo grub-mkfont: No "($grub_mkfont_excuse)"
fi
if [ x"$grub_mount_excuse" = x ]; then
echo grub-mount: Yes
else
echo grub-mount: No "($grub_mount_excuse)"
fi
if [ x"$starfield_excuse" = x ]; then
echo starfield theme: Yes
echo With DejaVuSans font from $DJVU_FONT_SOURCE
else
echo starfield theme: No "($starfield_excuse)"
fi
if [ x"$libzfs_excuse" = x ]; then
echo With libzfs support: Yes
else
echo With libzfs support: No "($libzfs_excuse)"
fi
if [ x"$grub_build_mkfont_excuse" = x ]; then
  echo Build-time grub-mkfont: Yes
  if test "x$FONT_SOURCE" = x ; then
    echo "Without unifont"
  else
    echo "With unifont from $FONT_SOURCE"
  fi
else
  echo Build-time grub-mkfont: No "($grub_build_mkfont_excuse)"
  echo "Without unifont (no build-time grub-mkfont)"
fi
if test x"$liblzma_excuse" != x ; then
echo "Without liblzma (no support for XZ-compressed mips images) ($liblzma_excuse)"
else
echo "With liblzma from $LIBLZMA (support for XZ-compressed mips images)"
fi
echo "*******************************************************"
]
