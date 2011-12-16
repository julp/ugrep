#!/bin/sh

#export CC="/usr/bin/clang"
BUILD_TYPE="Maintainer"
if [ $# -eq 1 ]; then
    case "$1" in
        prod|release)
            BUILD_TYPE="Release"
        ;;
    esac
fi

rm -f CMakeCache.txt
touch *.c
cmake . -G"Unix Makefiles" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DDYNAMIC_READER=ON
LANG=C make # VERBOSE=1
