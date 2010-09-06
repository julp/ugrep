#!/bin/sh

rm -f CMakeCache.txt
touch *.c
cmake . -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Maintainer
LANG=C make # VERBOSE=1
