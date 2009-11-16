dnl Process this file with autoconf to produce a configure script.
##############################################################################
#
# Copyright (c) 2000, Johan Bengtsson
# Copyright (c) 2009, Michael Weber
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#
##############################################################################

AC_PREREQ(2.60)
AC_INIT([memtime], [1.3.1],
        [michaelw+memtime@foldr.org])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([autotools])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([memtime.c])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_HOST

AM_CFLAGS="-g -O2"
AC_PROG_CC
AC_PROG_INSTALL

AC_PROG_CC_C99
AS_IF([test x"$ac_cv_prog_cc_c99" = xno],
  [AC_MSG_FAILURE([no acceptable C99 compiler found.])])
AX_TRY_CFLAGS_IFELSE([-W -Wall])
AX_TRY_CFLAGS_IFELSE([-pipe])

AC_OBJEXT

case "$host_os" in
    linux*) machdep=linux ;;
  solaris*) machdep=sunos5 ;;
   darwin*) machdep=darwin9 ;;
  openbsd*) machdep=openbsd ;;
dragonfly*) machdep=dragonfly ;;
   netbsd*) machdep=netbsd ;;
  *) AC_MSG_ERROR([System "$host_os" is not yet supported.]) ;;
esac

AC_CHECK_HEADERS([mach/shared_memory_server.h mach/shared_region.h])

AC_CHECK_DECLS([SHARED_REGION_BASE, SHARED_REGION_SIZE],
[have_shared_region_syms=yes],,
[#ifdef HAVE_MACH_SHARED_REGION_H
#include <mach/shared_region.h>
#endif
])dnl
AC_CHECK_DECLS([GLOBAL_SHARED_TEXT_SEGMENT, GLOBAL_SHARED_DATA_SEGMENT,
  SHARED_TEXT_REGION_SIZE],
[have_shared_region_syms=yes],,
[#ifdef HAVE_MACH_SHARED_MEMORY_SERVER_H
#include <mach/shared_memory_server.h>
#endif
])dnl
if test x"$have_shared_region_syms" != xyes dnl
   && test "$machdep" = "darwin9"; then
  AC_MSG_ERROR([unknown mach/shared_region API])
fi

AM_CONDITIONAL([LINUX], [test x$machdep = xlinux ])
AM_CONDITIONAL([SUNOS5], [test x$machdep = xsunos5 ])
AM_CONDITIONAL([DARWIN9], [test x$machdep = xdarwin9 ])
AM_CONDITIONAL([OPENBSD], [test x$machdep = xopenbsd ])
AM_CONDITIONAL([NETBSD], [test x$machdep = xnetbsd ])
AM_CONDITIONAL([DRAGONFLY], [test x$machdep = xdragonfly ])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
