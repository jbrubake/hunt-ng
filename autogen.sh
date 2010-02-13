#! /bin/sh

rm -rf autom4te.cache
aclocal
autoheader
touch stamp-h
automake --add-missing --copy
autoconf
./configure $*
