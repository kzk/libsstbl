#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(libsstbl, 0.0.1)
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_CYGWIN
AC_PROG_LIBTOOL

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for functions.
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_CHECK_FUNCS([memmove memset select socket strchr strdup posix_fadvise madvise])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h memory.h netdb.h netinet/in.h stddef.h stdlib.h string.h sys/param.h sys/socket.h unistd.h])
AC_CHECK_HEADERS([malloc.h])

# Checks for system libraries.
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([sendfile], [sendfile])
AC_CHECK_LIB([socket], [socket])
AC_CHECK_LIB([nsl], [gethostbyname])
AC_CHECK_LIB([kstat], [kstat_open])
AC_CHECK_LIB([z], [gzopen])

#------------------------------------------------------------------------------
# Compiler
#------------------------------------------------------------------------------
# Default Compiler Option
if test "x$CC" = "xgcc" -o "x$CC" = "xccache gcc"; then
  CFLAGS="$CFLAGS -O2 -g -Wswitch-enum -Wall -D_REENTRANT -Wformat=2 -pipe -D_FORTIFY_SOURCE=1 -Wcast-qual -Wcast-align -Wpointer-arith -Wwrite-strings -fno-omit-frame-pointer"
fi

if test "x$CXX" = "xg++" -o "x$CXX" = "xccache g++"; then
  CXXFLAGS="$CXXFLAGS -O2 -g -Wswitch-enum -Wall -D_REENTRANT -Wformat=2 -pipe -D_FORTIFY_SOURCE=1 -Wcast-qual -Wcast-align -Wpointer-arith -Wwrite-strings -Woverloaded-virtual -fno-omit-frame-pointer"
fi

#------------------------------------------------------------------------------
# Generate Makefiles
#------------------------------------------------------------------------------
AC_CONFIG_FILES([Makefile
                 src/Makefile
		 ])
AC_OUTPUT
