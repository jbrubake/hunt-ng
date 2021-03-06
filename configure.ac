#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([hunt-ng], [1.0], [Martin Bays <mbays@sdf.lonestar.org>])
#AC_CONFIG_SRCDIR([confdefs.h])
AC_CONFIG_HEADER([config.h])

# Use automake
AM_INIT_AUTOMAKE
AC_PROG_RANLIB

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_ARG_WITH([libwrap],
	    [AS_HELP_STRING([--without-libwrap], [disable support for libwrap])],
	    [],
	    [with_libwrap=yes])
AS_IF([test "x$with_libwrap" != xno],
      [AC_CHECK_LIB([wrap], [request_init])])

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC

# Autoscan says:
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h malloc.h memory.h netdb.h netinet/in.h paths.h stddef.h stdint.h stdlib.h string.h strings.h sys/file.h sys/ioctl.h sys/socket.h sys/time.h syslog.h termios.h unistd.h],,AC_MSG_ERROR([missing required header]))

# I add:
# FIXME: when do we need this, and what do we need when we don't have it?
#   It appears to be used on (at least Net)BSD for the SIO* ioctls. On linux
#   these are included with ioctl.h.
AC_CHECK_HEADERS([sys/sockio.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_HEADER_STDBOOL

AC_CHECK_MEMBER([struct sockaddr_in.sin_len], AC_DEFINE([SIN_HAS_SIN_LEN], [], [sockaddr_in has sin_len member]),,[#include <netinet/in.h>])

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL

JCF_WITH_CURSES
if test ! "$CURSES_LIB"
then
    AC_MSG_ERROR([working curses installation required])
fi

# Autoscan says:
AC_CHECK_FUNCS([gethostbyaddr gethostbyname gettimeofday inet_ntoa memmove memset select socket strchr strpbrk strrchr strtol],, AC_MSG_ERROR([missing required function]))

# The following are defined in lib/ if the system doesn't have them:
AC_CHECK_FUNCS([fgetln strlcat strlcpy])

# FIXME: GCC 3.2.3 segfaults with -O2 (on my machine anyway)
#   Until tested further, disable -O2 for everyone
CFLAGS=`echo $CFLAGS | sed 's/-O2//'`

AC_CONFIG_FILES([Makefile
                 hunt/Makefile
                 huntd/Makefile
                 lib/Makefile])
AC_OUTPUT
