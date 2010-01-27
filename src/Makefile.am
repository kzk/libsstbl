lib_LTLIBRARIES = libsstbl.la

libsstbl_la_SOURCES = \
  ssftbl.h ssftbl.c \
  ssmtbl.h ssmtbl.c \
  ssbf.h ssbf.c \
  ssutil.h ssutil.c \
  compress.h compress.c

libsstbl_la_LIBADD = \
  -lpthread -lm -ltokyocabinet

check_PROGRAMS = \
  ssftbl_test ssmtbl_test

ssftbl_test_SOURCES = ssftbl_test.cpp
ssftbl_test_CXXFLAGS = -I$(top_srcdir)/src
ssftbl_test_LDADD = -lgtest_main -lsstbl

ssmtbl_test_SOURCES = ssmtbl_test.cpp
ssmtbl_test_CXXFLAGS = -I$(top_srcdir)/src
ssmtbl_test_LDADD = -lgtest_main -lsstbl

TESTS = $(check_PROGRAMS)
