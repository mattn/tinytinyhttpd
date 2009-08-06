#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT([tthttpd], [0.0.4])
AC_CONFIG_SRCDIR([httpd.cxx])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.8.5])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h \
		  fcntl.h \
		  limits.h \
		  netdb.h \
		  netinet/in.h \
		  string.h \
		  sys/socket.h \
		  unistd.h])

case "$host" in
*-*-aix*)
	# Some versions of VAC won't allow macro redefinitions at
	# -qlanglevel=ansi, and autoconf 2.60 sometimes insists on using that
	# particularly with older versions of vac or xlc.
	# It also throws errors about null macro argments, but these are
	# not fatal.
	AC_MSG_CHECKING(if compiler allows macro redefinitions)
	AC_COMPILE_IFELSE(
	    [AC_LANG_SOURCE([[
#define testmacro foo
#define testmacro bar
int main(void) { exit(0); }
	    ]])],
	    [ AC_MSG_RESULT(yes) ],
	    [ AC_MSG_RESULT(no)
	      CC="`echo $CC | sed 's/-qlanglvl\=ansi//g'`"
	      LD="`echo $LD | sed 's/-qlanglvl\=ansi//g'`"
	      CFLAGS="`echo $CFLAGS | sed 's/-qlanglvl\=ansi//g'`"
	      CPPFLAGS="`echo $CPPFLAGS | sed 's/-qlanglvl\=ansi//g'`"
	    ]
	)

	AC_MSG_CHECKING([how to specify blibpath for linker ($LD)])
	if (test -z "$blibpath"); then
		blibpath="/usr/lib:/lib"
	fi
	saved_LDFLAGS="$LDFLAGS"
	if test "$GCC" = "yes"; then
		flags="-Wl,-blibpath: -Wl,-rpath, -blibpath:"
	else
		flags="-blibpath: -Wl,-blibpath: -Wl,-rpath,"
	fi
	for tryflags in $flags ;do
		if (test -z "$blibflags"); then
			LDFLAGS="$saved_LDFLAGS $tryflags$blibpath"
			AC_TRY_LINK([], [], [blibflags=$tryflags])
		fi
	done
	if (test -z "$blibflags"); then
		AC_MSG_RESULT(not found)
		AC_MSG_ERROR([*** must be able to specify blibpath on AIX - check config.log])
	else
		AC_MSG_RESULT($blibflags)
	fi
	LDFLAGS="$saved_LDFLAGS"
	AC_DEFINE(SETEUID_BREAKS_SETUID, 1,
	    [Define if your platform breaks doing a seteuid before a setuid])
	AC_DEFINE(BROKEN_SETREUID, 1, [Define if your setreuid() is broken])
	AC_DEFINE(BROKEN_SETREGID, 1, [Define if your setregid() is broken])
	;;
*-*-cygwin*)
	AC_DEFINE(HAVE_CYGWIN, 1, [Define if you are on Cygwin])
	;;
*-*-dgux*)
	AC_DEFINE(SETEUID_BREAKS_SETUID, 1,
	    [Define if your platform breaks doing a seteuid before a setuid])
	AC_DEFINE(BROKEN_SETREUID, 1, [Define if your setreuid() is broken])
	AC_DEFINE(BROKEN_SETREGID, 1, [Define if your setregid() is broken])
	;;
*-*-darwin*)
	AC_MSG_CHECKING(if we have working getaddrinfo)
	AC_TRY_RUN([#include <mach-o/dyld.h>
main() { if (NSVersionOfRunTimeLibrary("System") >= (60 << 16))
		exit(0);
	else
		exit(1);
}], [AC_MSG_RESULT(working)],
	[AC_MSG_RESULT(buggy)
	AC_DEFINE(BROKEN_GETADDRINFO, 1, [getaddrinfo is broken (if present)])],
	[AC_MSG_RESULT(assume it is working)])
	AC_DEFINE(SETEUID_BREAKS_SETUID, 1,
	    [Define if your platform breaks doing a seteuid before a setuid])
	AC_DEFINE(BROKEN_SETREUID, 1, [Define if your setreuid() is broken])
	AC_DEFINE(BROKEN_SETREGID, 1, [Define if your setregid() is broken])
	;;
*-*-dragonfly*)
	;;
*-*-hpux*)
	# first we define all of the options common to all HP-UX releases
	CPPFLAGS="$CPPFLAGS -D_HPUX_SOURCE -D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED=1"
	LIBS="$LIBS -lsec"
	AC_CHECK_LIB(xnet, t_error, ,
	    AC_MSG_ERROR([*** -lxnet needed on HP-UX - check config.log ***]))

	# next, we define all of the options specific to major releases
	case "$host" in
	*-*-hpux10*)
		if test -z "$GCC"; then
			CFLAGS="$CFLAGS -Ae"
		fi
		;;
	*-*-hpux11*)
		;;
	esac

	# lastly, we define options specific to minor releases
	case "$host" in
	*-*-hpux10.26)
		;;
	esac
	;;
*-*-irix5*)
	PATH="$PATH:/usr/etc"
	AC_DEFINE(BROKEN_INET_NTOA, 1,
		[Define if you system's inet_ntoa is busted
		(e.g. Irix gcc issue)])
	AC_DEFINE(SETEUID_BREAKS_SETUID, 1,
	    [Define if your platform breaks doing a seteuid before a setuid])
	AC_DEFINE(BROKEN_SETREUID, 1, [Define if your setreuid() is broken])
	AC_DEFINE(BROKEN_SETREGID, 1, [Define if your setregid() is broken])
	;;
*-*-irix6*)
	PATH="$PATH:/usr/etc"
	AC_DEFINE(BROKEN_INET_NTOA)
	AC_DEFINE(SETEUID_BREAKS_SETUID, 1,
	    [Define if your platform breaks doing a seteuid before a setuid])
	AC_DEFINE(BROKEN_SETREUID, 1, [Define if your setreuid() is broken])
	AC_DEFINE(BROKEN_SETREGID, 1, [Define if your setregid() is broken])
	;;
*-*-linux*)
	AC_DEFINE(DONT_TRY_OTHER_AF, 1, [Workaround more Linux IPv6 quirks])
	inet6_default_4in6=yes
	;;
mips-sony-bsd|mips-sony-newsos4)
	SONY=1
	;;
*-*-netbsd*)
	;;
*-*-freebsd*)
	;;
*-*-bsdi*)
	AC_DEFINE(SETEUID_BREAKS_SETUID, 1,
	    [Define if your platform breaks doing a seteuid before a setuid])
	AC_DEFINE(BROKEN_SETREUID, 1, [Define if your setreuid() is broken])
	AC_DEFINE(BROKEN_SETREGID, 1, [Define if your setregid() is broken])
	;;
*-next-*)
	;;
*-*-openbsd*)
	AC_DEFINE(HAVE_ATTRIBUTE__SENTINEL__, 1, [OpenBSD's gcc has sentinel])
	AC_DEFINE(HAVE_ATTRIBUTE__BOUNDED__, 1, [OpenBSD's gcc has bounded])
	;;
*-*-solaris*)
	;;
*-*-sunos4*)
	CPPFLAGS="$CPPFLAGS -DSUNOS4"
	;;
*-ncr-sysv*)
	LIBS="$LIBS -lc89"
	AC_DEFINE(SETEUID_BREAKS_SETUID, 1,
	    [Define if your platform breaks doing a seteuid before a setuid])
	AC_DEFINE(BROKEN_SETREUID, 1, [Define if your setreuid() is broken])
	AC_DEFINE(BROKEN_SETREGID, 1, [Define if your setregid() is broken])
	;;
*-sni-sysv*)
	# /usr/ucblib MUST NOT be searched on ReliantUNIX
	AC_CHECK_LIB(dl, dlsym, ,)
	# -lresolv needs to be at the end of LIBS or DNS lookups break
	AC_CHECK_LIB(resolv, res_query, [ LIBS="$LIBS -lresolv" ])
	# /usr/ucblib/libucb.a no longer needed on ReliantUNIX
	# Attention: always take care to bind libsocket and libnsl before libc,
	# otherwise you will find lots of "SIOCGPGRP errno 22" on syslog
	AC_DEFINE(SETEUID_BREAKS_SETUID, 1,
	    [Define if your platform breaks doing a seteuid before a setuid])
	AC_DEFINE(BROKEN_SETREUID, 1, [Define if your setreuid() is broken])
	AC_DEFINE(BROKEN_SETREGID, 1, [Define if your setregid() is broken])
	;;
# UnixWare 1.x, UnixWare 2.x, and others based on code from Univel.
*-*-sysv4.2*)
	AC_DEFINE(SETEUID_BREAKS_SETUID, 1,
	    [Define if your platform breaks doing a seteuid before a setuid])
	AC_DEFINE(BROKEN_SETREUID, 1, [Define if your setreuid() is broken])
	AC_DEFINE(BROKEN_SETREGID, 1, [Define if your setregid() is broken])
	;;
# UnixWare 7.x, OpenUNIX 8
*-*-sysv5*)
	AC_DEFINE(SETEUID_BREAKS_SETUID, 1,
	    [Define if your platform breaks doing a seteuid before a setuid])
	AC_DEFINE(BROKEN_SETREUID, 1, [Define if your setreuid() is broken])
	AC_DEFINE(BROKEN_SETREGID, 1, [Define if your setregid() is broken])
	;;
*-*-sysv*)
	;;
# SCO UNIX and OEM versions of SCO UNIX
*-*-sco3.2v4*)
	AC_MSG_ERROR("This Platform is no longer supported.")
	;;
# SCO OpenServer 5.x
*-*-sco3.2v5*)
	if test -z "$GCC"; then
		CFLAGS="$CFLAGS -belf"
	fi
	LIBS="$LIBS -lprot -lx -ltinfo -lm"
	no_dev_ptmx=1
	MANTYPE=man
	TEST_SHELL=ksh
	AC_DEFINE(SETEUID_BREAKS_SETUID, 1,
	    [Define if your platform breaks doing a seteuid before a setuid])
	AC_DEFINE(BROKEN_SETREUID, 1, [Define if your setreuid() is broken])
	AC_DEFINE(BROKEN_SETREGID, 1, [Define if your setregid() is broken])
	;;
*-*-unicosmk*)
	LDFLAGS="$LDFLAGS"
	LIBS="$LIBS -lgen -lrsc -lshare -luex -lacm"
	MANTYPE=cat
	AC_DEFINE(SETEUID_BREAKS_SETUID, 1,
	    [Define if your platform breaks doing a seteuid before a setuid])
	AC_DEFINE(BROKEN_SETREUID, 1, [Define if your setreuid() is broken])
	AC_DEFINE(BROKEN_SETREGID, 1, [Define if your setregid() is broken])
	;;
*-*-unicosmp*)
	LDFLAGS="$LDFLAGS"
	LIBS="$LIBS -lgen -lacid -ldb"
	MANTYPE=cat
	;;
*-*-unicos*)
	LDFLAGS="$LDFLAGS -Wl,-Dmsglevel=334:fatal"
	LIBS="$LIBS -lgen -lrsc -lshare -luex -lacm"
	MANTYPE=cat
	AC_DEFINE(SETEUID_BREAKS_SETUID, 1,
	    [Define if your platform breaks doing a seteuid before a setuid])
	AC_DEFINE(BROKEN_SETREUID, 1, [Define if your setreuid() is broken])
	AC_DEFINE(BROKEN_SETREGID, 1, [Define if your setregid() is broken])
	;;
*-dec-osf*)
	AC_DEFINE(BROKEN_GETADDRINFO)
	AC_DEFINE(SETEUID_BREAKS_SETUID, 1,
	    [Define if your platform breaks doing a seteuid before a setuid])
	AC_DEFINE(BROKEN_SETREUID, 1, [Define if your setreuid() is broken])
	AC_DEFINE(BROKEN_SETREGID, 1, [Define if your setregid() is broken])
	;;
*-*-nto-qnx*)
	;;
*-*-ultrix*)
	AC_DEFINE(BROKEN_GETGROUPS, 1, [getgroups(0,NULL) will return -1])
	AC_DEFINE(HAVE_SYS_SYSLOG_H, 1, [Force use of sys/syslog.h on Ultrix])
	;;
*-*-lynxos)
        CFLAGS="$CFLAGS -D__NO_INCLUDE_WARN__"
        ;;
esac

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_PID_T
AC_TYPE_SIZE_T

AC_CACHE_CHECK([for ssize_t], ac_cv_have_ssize_t, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
		],
		[ ssize_t foo; foo = 1235; ],
		[ ac_cv_have_ssize_t="yes" ],
		[ ac_cv_have_ssize_t="no" ]
	)
])
if test "x$ac_cv_have_ssize_t" = "xyes" ; then
	AC_DEFINE(HAVE_SSIZE_T, 1, [define if you have ssize_t data type])
fi

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([dup2 \
		gethostbyname \
		gethostname \
		inet_ntoa \
		mblen \
		memset \
		realpath \
		select \
		setproctitle \
		setresgid \
		setresuid \
		socket \
		strchr \
		strpbrk \
		strstr \
		wcwidth])

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
