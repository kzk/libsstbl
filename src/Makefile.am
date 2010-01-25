lib_LTLIBRARIES = libsstbl.la

libsstbl_la_SOURCES = \
  ssmtbl.h ssmtbl.c \
  ssbf.h ssbf.c \
  ssutil.h ssutil.c

libsstbl_la_LIBADD = \
  -ltokyocabinet

check_PROGRAMS = \
  ssmtbl_test

ssmtbl_test_SOURCES = ssmtbl_test.cpp
ssmtbl_test_CXXFLAGS = -I$(top_srcdir)/src
ssmtbl_test_LDADD = -lgtest_main -lsstbl

TESTS = $(check_PROGRAMS)
