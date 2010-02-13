LDADD = @CURSES_LIB@ -lhunt
AM_LDFLAGS = -L$(top_builddir)/lib
INCLUDES = -I$(top_srcdir)/huntd -I$(top_srcdir)/lib

bin_PROGRAMS = hunt-ng

hunt_ng_SOURCES = connect.c display.c hunt.c list.c otto.c playit.c client.h display.h list.h $(top_srcdir)/huntd/hunt.h
