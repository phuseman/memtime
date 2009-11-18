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

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = memtime

AM_CPPFLAGS =
memtime_SOURCES = memtime.c machdep.h
if LINUX
memtime_SOURCES += linux.c
AM_CPPFLAGS += -DCAN_USE_RLIMIT_RSS -DCAN_USE_RLIMIT_CPU
endif
if DARWIN9
memtime_SOURCES += darwin9.c
AM_CPPFLAGS += -DCAN_USE_RLIMIT_CPU
endif
if SUNOS5
memtime_SOURCES += sunos5.c
AM_CPPFLAGS += -DCAN_USE_RLIMIT_RSS -DCAN_USE_RLIMIT_CPU
endif
if OPENBSD
memtime_SOURCES += netbsd.c
memtime_LDADD = -lkvm
AM_CPPFLAGS += -DCAN_USE_RLIMIT_CPU
endif
if DRAGONFLY
memtime_SOURCES += dragonfly.c
memtime_LDADD = -lkvm
AM_CPPFLAGS += -DCAN_USE_RLIMIT_RSS -DCAN_USE_RLIMIT_CPU
endif
if NETBSD
memtime_SOURCES += netbsd.c
memtime_LDADD = -lkvm
AM_CPPFLAGS += -DCAN_USE_RLIMIT_RSS -DCAN_USE_RLIMIT_CPU
endif
if FREEBSD
memtime_SOURCES += freebsd.c
memtime_LDADD = -lkvm
AM_CPPFLAGS += -DCAN_USE_RLIMIT_RSS -DCAN_USE_RLIMIT_CPU
endif

EXTRA_DIST = License
doc_DATA = License README
