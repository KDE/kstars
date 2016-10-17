AC_PREREQ(2.63)

AC_INIT([libnova],m4_esyscmd([./git-version-gen .tarball-version]),[lgirdwood (at) gmail (dot) com])

AC_CONFIG_MACRO_DIR([m4])

# Save this value here, since automake will set cflags later
cflags_set=${CFLAGS+set}

# libtool versioning
LIBNOVA_MAJOR_VERSION=0
LIBNOVA_MINOR_VERSION=16
LIBNOVA_MICRO_VERSION=0
LIBNOVA_VERSION=$LIBNOVA_MAJOR_VERSION.$LIBNOVA_MINOR_VERSION.$LIBNOVA_MICRO_VERSION
AC_SUBST(LIBNOVA_MAJOR_VERSION)
AC_SUBST(LIBNOVA_MINOR_VERSION)
AC_SUBST(LIBNOVA_MICRO_VERSION)
AC_SUBST(LIBNOVA_VERSION)

LT_VERSION=$LIBNOVA_MICRO_VERSION
LT_RELEASE=$LIBNOVA_MAJOR_VERSION.$LIBNOVA_MINOR_VERSION
AC_SUBST(LT_VERSION)
AC_SUBST(LT_RELEASE)

VERSION=$LIBNOVA_VERSION
PACKAGE=libnova

AM_INIT_AUTOMAKE([foreign 1.11 -Wall -Wno-portability silent-rules tar-pax])

# Specify a configuration file
AC_CONFIG_HEADERS([config.h])

dnl Initialize libtool
LT_PREREQ(2.2)
LT_INIT

dnl Initialize maintainer mode
AM_MAINTAINER_MODE([enable])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

dnl System functions
AC_C_CONST
AC_FUNC_ALLOCA
AC_CHECK_HEADERS([malloc.h])
AC_HEADER_STDC

dnl Time and date functions
AC_CHECK_HEADERS([sys/time.h])
AC_HEADER_TIME
AC_STRUCT_TM
AC_CHECK_FUNCS([gettimeofday])
AC_CHECK_FUNCS([tzset])

dnl String functions
AC_FUNC_STRTOD
AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([strpbrk])
AC_CHECK_FUNCS([strtol])
AC_CHECK_FUNCS([strtok_r])

dnl Math functions
AC_CHECK_FUNCS([modf])
AC_CHECK_FUNCS([pow])
AC_CHECK_FUNCS([sqrt])
AC_CHECK_FUNCS([cbrt])
AC_CHECK_LIB(m, [round], AC_DEFINE([HAVE_ROUND], [1], [Check for round]))
AC_CHECK_FUNCS([cosl])
AC_CHECK_FUNCS([sinl])
AC_CHECK_FUNCS([acosl])
AC_CHECK_FUNCS([asinl])
AC_CHECK_FUNCS([atan2l])
AC_CHECK_LIB(m,pow)
AC_CHECK_LIB(sunmath,acosl)


# AVX support
AC_ARG_ENABLE(avx, [AS_HELP_STRING([--enable-avx],[enable AVX optimizations])], have_avx=$enableval, have_avx=no)
if test "$have_avx" = "yes"; then
        AC_DEFINE(HAVE_AVX,1,[Define to enable AVX optimizations.])
fi
AM_CONDITIONAL(HAVE_AVX, test "$have_avx" = "yes")

if test "$have_avx" = "yes" -a "x$AVX_CFLAGS" = x; then
	AX_CHECK_COMPILE_FLAG(-mavx, [AVX_CFLAGS="-mavx"],
		[AC_MSG_ERROR([Need a version of gcc with -mavx])])
fi

AC_SUBST(AVX_CFLAGS)

# Set LIBNOVA_MACRO_DIR
if test "x${prefix}" = "xNONE"; then
  LIBNOVA_MACRO_DIR=${ac_default_prefix}/share/aclocal
  AC_SUBST(LIBNOVA_MACRO_DIR)
else
  LIBNOVA_MACRO_DIR=\${prefix}/share/aclocal
  AC_SUBST(LIBNOVA_MACRO_DIR)
fi

dnl Make sure we fail on all warnings
CFLAGS=-Wall

AC_CONFIG_FILES([
Makefile
tools/Makefile
src/Makefile
src/libnova/Makefile
src/elp/Makefile
lntest/Makefile
doc/Makefile
doc/doxyfile
examples/Makefile
m4/Makefile
])

AC_OUTPUT
