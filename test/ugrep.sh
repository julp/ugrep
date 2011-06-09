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

ARGS='--color=never -HnA 2 -B 3 after_context ugrep.c'
assertOutputValueEx "-A 2 -B 3 (without -v)" "LC_ALL=C ./ugrep ${ARGS} 2>/dev/null" "grep ${ARGS}"
assertOutputValueEx "-A 2 -B 3 (with -v)" "LC_ALL=C ./ugrep -v ${ARGS} 2>/dev/null" "grep -v ${ARGS}"

#ARGS='--color=never -HnA 4 -B 6 after_context ugrep.c'
#assertOutputValueEx "-A 4 -B 6 (without -v)" "LC_ALL=C ./ugrep ${ARGS} 2>/dev/null" "grep ${ARGS}"

ARGS='--color=never -HnA 6 -B 4 after_context ugrep.c'
assertOutputValueEx "-A 6 -B 4 (without -v)" "LC_ALL=C ./ugrep ${ARGS} 2>/dev/null" "grep ${ARGS}"

ARGS='--color=never -HnA 4 -B 6 "^[^{}]*$" ugrep.c'
assertOutputValueEx "-A 4 -B 6 (with -v)" "LC_ALL=C ./ugrep -v ${ARGS} 2>/dev/null" "grep -v ${ARGS}"

ARGS='élève'
assertOutputValue "count matching lines (-c)" "./ugrep -c ${ARGS} ${UFILE} 2>/dev/null" "grep -c ${ARGS} ${FILE}" "-eq"
ARGS='élève'
assertOutputValue "count non-matching lines (-vc)" "./ugrep -vc ${ARGS} ${UFILE} 2>/dev/null" "grep -vc ${ARGS} ${FILE}" "-eq"

# ./ugrep -q élève test/utf8_eleve.txt 2>/dev/null
# assertTrue "[[ $? -eq 0 ]]"
# ./ugrep -q zzz test/utf8_eleve.txt 2>/dev/null
# assertTrue "[[ $? -eq 1 ]]"
# ./ugrep -q élève /unexistant 2>/dev/null
# assertTrue "[[ $? -gt 1 ]]"
# ./ugrep -q élève /unexistant test/utf8_eleve.txt 2>/dev/null
# assertTrue "[[ $? -eq 0 ]]"

assertExitValue "exit value with one or more lines selected" "./ugrep -q élève ${FILE} 2>/dev/null" 0
assertExitValue "exit value with no lines selected" "./ugrep -q zzz ${UFILE} 2>/dev/null" 1
assertExitValue "exit value with error and no more file" "./ugrep -q élève /unexistant 2>/dev/null" 1 "-gt"

ARGS='--color=never élève'
assertOutputValue "file with match (-l)" "./ugrep -l ${ARGS} ${FILE} 2>/dev/null" "grep -l ${ARGS} ${FILE}"
assertOutputValue "file without match (-L)" "./ugrep -L ${ARGS} ${FILE} 2>/dev/null" "grep -L ${ARGS} ${FILE}"

ARGS='--color=never zzz'
assertOutputValue "file with match (-l)" "./ugrep -l ${ARGS} ${FILE} 2>/dev/null" "grep -l ${ARGS} ${FILE}"
assertOutputValue "file without match (-L)" "./ugrep -L ${ARGS} ${FILE} 2>/dev/null" "grep -L ${ARGS} ${FILE}"

exit $?