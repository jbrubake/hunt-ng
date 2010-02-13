#! /bin/sh

rm -rf autom4te.cache
aclocal
autoheader
touch stamp-h
darcs changes >| ChangeLog
automake --add-missing --copy
autoconf
./configure $*
