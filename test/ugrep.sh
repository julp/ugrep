#!/bin/bash

# charset: UTF-8

# WARNING:
# * only file in UTF-8 *without BOM* should be compared (grep considers BOM when ugrep don't)
# * comparaison must be about an UTF-8 file with LF line ending (grep considers CR when ugrep don't)
# * search string should be ASCII only when LC_ALL=C

declare -r TESTDIR=$(dirname $(readlink -f "${BASH_SOURCE}"))
declare -r DATADIR="${TESTDIR}/data"

. ${TESTDIR}/assert.sh.inc

if [ "${UGREP_SYSTEM}" == 'UTF-8' ]; then
    FILE="${DATADIR}/utf8_eleve.txt"
    UFILE="${DATADIR}/iso88591_eleve.txt"
else
    FILE="${DATADIR}/iso88591_eleve.txt"
    UFILE="${DATADIR}/utf8_eleve.txt"
fi

ARGS='--color=never -HnA 2 -B 3 after_context bin/ugrep.c'
assertOutputValueEx "-A 2 -B 3 (without -v)" "LC_ALL=C ./ugrep ${UGREP_OPTS} ${ARGS} 2>/dev/null" "grep ${ARGS}"
assertOutputValueEx "-A 2 -B 3 (with -v)" "LC_ALL=C ./ugrep ${UGREP_OPTS} -v ${ARGS} 2>/dev/null" "grep -v ${ARGS}"

#ARGS='--color=never -HnA 4 -B 6 after_context bin/ugrep.c'
#assertOutputValueEx "-A 4 -B 6 (without -v)" "LC_ALL=C ./ugrep ${UGREP_OPTS} ${ARGS} 2>/dev/null" "grep ${ARGS}"

ARGS='--color=never -HnA 6 -B 4 after_context bin/ugrep.c'
assertOutputValueEx "-A 6 -B 4 (without -v)" "LC_ALL=C ./ugrep ${UGREP_OPTS} ${ARGS} 2>/dev/null" "grep ${ARGS}"

ARGS='--color=never -HnA 4 -B 6 "^[^{}]*$" bin/ugrep.c'
assertOutputValueEx "-A 4 -B 6 (with -v)" "LC_ALL=C ./ugrep ${UGREP_OPTS} -v ${ARGS} 2>/dev/null" "grep -v ${ARGS}"

ARGS='élève'
assertOutputCommand "count matching lines (-c)" "./ugrep ${UGREP_OPTS} -c ${ARGS} ${UFILE} 2>/dev/null" "grep -c ${ARGS} ${FILE}" "-eq"
ARGS='élève'
assertOutputCommand "count non-matching lines (-vc)" "./ugrep ${UGREP_OPTS} -vc ${ARGS} ${UFILE} 2>/dev/null" "grep -vc ${ARGS} ${FILE}" "-eq"

# ./ugrep ${UGREP_OPTS} -q élève test/utf8_eleve.txt 2>/dev/null
# assertTrue "[[ $? -eq 0 ]]"
# ./ugrep ${UGREP_OPTS} -q zzz test/utf8_eleve.txt 2>/dev/null
# assertTrue "[[ $? -eq 1 ]]"
# ./ugrep ${UGREP_OPTS} -q élève /unexistant 2>/dev/null
# assertTrue "[[ $? -gt 1 ]]"
# ./ugrep ${UGREP_OPTS} -q élève /unexistant test/utf8_eleve.txt 2>/dev/null
# assertTrue "[[ $? -eq 0 ]]"

assertExitValue "exit value with one or more lines selected" "./ugrep ${UGREP_OPTS} -q élève ${FILE} 2>/dev/null" 0
assertExitValue "exit value with no lines selected" "./ugrep ${UGREP_OPTS} -q zzz ${UFILE} 2>/dev/null" 1
assertExitValue "exit value with error and no more file" "./ugrep ${UGREP_OPTS} -q élève /unexistant 2>/dev/null" 1 "-gt"

ARGS='--color=never élève'
assertOutputCommand "file with match (-l)" "./ugrep ${UGREP_OPTS} -l ${ARGS} ${FILE} 2>/dev/null" "grep -l ${ARGS} ${FILE}"
assertOutputCommand "file without match (-L)" "./ugrep ${UGREP_OPTS} -L ${ARGS} ${FILE} 2>/dev/null" "grep -L ${ARGS} ${FILE}"

ARGS='--color=never zzz'
assertOutputCommand "file with match (-l)" "./ugrep ${UGREP_OPTS} -l ${ARGS} ${FILE} 2>/dev/null" "grep -l ${ARGS} ${FILE}"
assertOutputCommand "file without match (-L)" "./ugrep ${UGREP_OPTS} -L ${ARGS} ${FILE} 2>/dev/null" "grep -L ${ARGS} ${FILE}"

exit $?
