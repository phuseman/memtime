dnl Process this file with autoconf to produce a configure script.
##############################################################################
#
# Copyright (c) 2000, Johan Bengtsson
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
        [ltsmin-support@cs.utwente.nl])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([autotools])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([memtime.c])

AC_CANONICAL_HOST

AM_CFLAGS="-g -O2"
AC_PROG_CC
AC_PROG_INSTALL

AX_TRY_CFLAGS_IFELSE([-W -Wall])
AX_TRY_CFLAGS_IFELSE([-pipe])

AC_EXEEXT
AC_OBJEXT

case "$host_os" in
    linux*) machdep=linux ;;
  solaris*) machdep=sunos5 ;;
  darwin9*) machdep=darwin9 ;;
  *) AC_MSG_ERROR([System "$host_os" is not yet supported.]) ;;
esac

AM_CONDITIONAL([LINUX], [test x$machdep = xlinux ])
AM_CONDITIONAL([SUNOS5], [test x$machdep = xsunos5 ])
AM_CONDITIONAL([DARWIN9], [test x$machdep = xdarwin9 ])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
