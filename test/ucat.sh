#!/bin/bash

# charset: UTF-8

# WARNING:
# * only file in UTF-8 *without BOM* should be compared (grep considers BOM when ugrep don't)
# * comparaison must be about an UTF-8 file with LF line ending (grep considers CR when ugrep don't)
# * search string should be ASCII only when LC_ALL=C

declare -r TESTDIR=$(dirname $(readlink -f "${BASH_SOURCE}"))
declare -r DATADIR="${TESTDIR}/data"

. ${TESTDIR}/assert.sh.inc

FILE="${DATADIR}/utf8_eleve.txt"
UFILE="${DATADIR}/iso88591_eleve.txt"

assertExitValue "exit value with error" "./ucat ${FILE} /unexistant ${FILE} &> /dev/null" 0 "-gt"
assertExitValue "exit value without error" "./ucat ${FILE} ${UFILE} &> /dev/null" 0

for ARGS in "-b" "-s" "-n" "-bs" "-ns"; do
    assertOutputValueExIgnoreBlanks "${ARGS}" "./ucat ${ARGS} ${UFILE} 2> /dev/null" "cat ${ARGS} ${FILE}"
done

exit $?