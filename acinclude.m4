### The following is due to JCF
## Based upon Mark Pulford's MP_WITH_CURSES macro from the autoconf archive,
## licensed under the GPL.  See http://www.gnu.org/software/ac-archive/

AC_DEFUN([JCF_WITH_CURSES],
    [AC_ARG_WITH([curses],
		 [  --with-curses           force use of curses over ncurses],,)
    jcf_save_LIBS="$LIBS"
    CURSES_LIB=""

    if test "$with_curses" != yes
    then
	AC_CACHE_CHECK([for working ncurses], jcf_cv_ncurses,
	    [LIBS="$LIBS -lncurses"
	     AC_TRY_LINK(
		 [#include <ncurses.h>],
		 [chtype a; int b=A_STANDOUT, c=KEY_LEFT; initscr(); ],
		 jcf_cv_ncurses=yes, jcf_cv_ncurses=no)])
	if test "$jcf_cv_ncurses" = yes
	then
	    AC_DEFINE([HAVE_NCURSES_H], [1],
		      [Define to 1 if you have the <ncurses.h> header file.])
	    CURSES_LIB="-lncurses"
	fi
	LIBS="$jcf_save_LIBS"
    fi

    if test ! "$CURSES_LIB"
    then
	AC_CACHE_CHECK([for working curses], jcf_cv_curses,
	    [LIBS="$LIBS -lcurses"
	     AC_TRY_LINK(
		 [#include <curses.h>],
		 [chtype a; int b=A_STANDOUT, c=KEY_LEFT; initscr(); ],
		 jcf_cv_curses=yes, jcf_cv_curses=no)])
	if test "$jcf_cv_curses" = yes
	then
	    AC_DEFINE([HAVE_CURSES_H], [1],
		      [Define to 1 if you have the <curses.h> header file.])
	    CURSES_LIB="-lcurses"
	fi
	LIBS="$jcf_save_LIBS"
    fi
    AC_SUBST([CURSES_LIB])
])dnl


