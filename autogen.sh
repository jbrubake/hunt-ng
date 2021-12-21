#! /bin/sh

rm -rf autom4te.cache
aclocal
autoheader
touch stamp-h
git log >| ChangeLog
automake --add-missing --copy
autoconf
./configure $*
