lib_LTLIBRARIES = libsstbl.la

libsstbl_la_SOURCES = \
  ssftbl.h ssftbl.c \
  ssmtbl.h ssmtbl.c \
  ssbf.h ssbf.c \
  ssutil.h ssutil.c \
  compress.h compress.c \
  compress/rollinghash.h compress/rollinghash.c \
  compress/blkhash.h compress/blkhash.c \
  compress/vcdiff.c

libsstbl_la_LIBADD = \
  -lpthread -lm -ltokyocabinet

check_PROGRAMS = \
  ssftbl_test_none ssftbl_test_compress \
  ssmtbl_test compress_test \
  rollinghash_test blkhash_test

ssftbl_test_none_SOURCES = ssftbl_test.cpp
ssftbl_test_none_CXXFLAGS = -I$(top_srcdir)/src -DSSFTBLCMETHOD=1
ssftbl_test_none_LDADD = -lgtest_main -lsstbl

ssftbl_test_compress_SOURCES = ssftbl_test.cpp
ssftbl_test_compress_CXXFLAGS = -I$(top_srcdir)/src -DSSFTBLCMETHOD=2
ssftbl_test_compress_LDADD = -lgtest_main -lsstbl

ssmtbl_test_SOURCES = ssmtbl_test.cpp
ssmtbl_test_CXXFLAGS = -I$(top_srcdir)/src
ssmtbl_test_LDADD = -lgtest_main -lsstbl

compress_test_SOURCES = compress_test.cpp
compress_test_CXXFLAGS = -I$(top_srcdir)/src
compress_test_LDADD = -lgtest_main -lsstbl

rollinghash_test_SOURCES = compress/rollinghash_test.cpp
rollinghash_test_CXXFLAGS = -I$(top_srcdir)/src
rollinghash_test_LDADD = -lgtest_main -lsstbl

blkhash_test_SOURCES = compress/blkhash_test.cpp
blkhash_test_CXXFLAGS = -I$(top_srcdir)/src
blkhash_test_LDADD = -lgtest_main -lsstbl

TESTS = $(check_PROGRAMS)
