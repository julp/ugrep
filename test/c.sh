#!/bin/bash

# charset: UTF-8

declare -r TESTDIR=$(dirname $(readlink -f "${BASH_SOURCE}"))
declare -r DATADIR="${TESTDIR}/data"

. ${TESTDIR}/assert.sh.inc

if [ -x ./unescape ]; then
    assertExitValue "unescape" "./unescape &> /dev/null" 0
fi
if [ -x ./intervals ]; then
    assertExitValue "intervals" "./intervals &> /dev/null" 0
fi
if [ -x ./readuchars ]; then
    assertOutputValueEx "readuchars (CP split)" "./readuchars test/data/readuchars_utf8.txt 2> /dev/null" "echo -e '>a< (1)\n>${A}< (2)\n>b< (1)\n>${B}< (2)\n>c< (1)\n>${C}< (2)\n>fo< (2)\n>ob< (2)\n>ar< (2)\n>d< (1)\n>${D}< (2)\n>e< (1)\n>${E}< (2)\n>f< (1)\n>${F}< (2)\n>g< (1)\n>${G}< (2)\n>h< (1)\n>${H}< (2)'"
fi

exit $?
