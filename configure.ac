#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT(tinytinyhttpd, 0.0.9, mattn.jp@gmail.com)
AC_CONFIG_SRCDIR([httpd.cxx])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE(tinytinyhttpd, 0.0.9, no-define) 

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h poll.h netinet/in.h string.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STAT
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_CHECK_FUNCS([dup2 gethostbyname gethostname inet_ntoa mblen memset realpath select socket strchr strpbrk wcwidth poll])

# pthread
dnl FIXME: do we need -D_REENTRANT here?
ACX_PTHREAD([
LIBS="$LIBS $PTHREAD_LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS -D_REENTRANT"
CXXFLAGS="$CXXFLAGS $PTHREAD_CFLAGS -D_REENTRANT"
CC="$PTHREAD_CC"
], [ AC_MSG_RESULT(no) ])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
