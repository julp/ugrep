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
assertOutputCommand "count non-matching lines (-vc)" "./ugrep ${UGREP_OPTS} -vc ${ARGS} ${UFILE} 2>/dev/null" "grep -vc ${ARGS} ${FILE}" "-eq"

ARGS='-c e'
INPUT="echo -en \"${E_ACUTE_NFD}\nl\n${E_GRAVE_NFD}\nv\ne\""
assertOutputValue "grapheme consistent (-c)" "${INPUT} | ./ugrep ${UGREP_OPTS} --unit=grapheme ${ARGS} 2>/dev/null" 1 "-eq"
assertOutputValue "grapheme inconsistent (-c)" "${INPUT} | ./ugrep ${UGREP_OPTS} --unit=codepoint ${ARGS} 2>/dev/null" 3 "-eq"
assertOutputValue "grapheme consistent (-Ec)" "${INPUT} | ./ugrep ${UGREP_OPTS} --unit=grapheme -E ${ARGS} 2>/dev/null" 1 "-eq"
assertOutputValue "grapheme inconsistent (-Ec)" "${INPUT} | ./ugrep ${UGREP_OPTS} --unit=codepoint -E ${ARGS} 2>/dev/null" 3 "-eq"

ARGS='--color=never -m 2 a'
INPUT="echo -en \"a\na\na\na\""
assertOutputCommand "max-count" "${INPUT} | ./ugrep ${UGREP_OPTS} ${ARGS} 2>/dev/null" "echo -en \"a\na\""
assertOutputValue "count + max-count" "${INPUT} | ./ugrep ${UGREP_OPTS} -c ${ARGS} 2>/dev/null" 2 "-eq"
ARGS='--color=never -vm 2 z'
assertOutputCommand "revert-match + max-count" "${INPUT} | ./ugrep ${UGREP_OPTS} ${ARGS} 2>/dev/null" "echo -en \"a\na\""
assertOutputValue "revert-match + count + max-count" "${INPUT} | ./ugrep -c ${UGREP_OPTS} ${ARGS} 2>/dev/null" 2 "-eq"

declare -r SDBDA_NFC=$'\xE1\xB9\xA9'
declare -r SDBDA_NFD=$'\x73\xCC\xA3\xCC\x87'
declare -r SDBDA_NONE=$'\x73\xCC\x87\xCC\xA3'

INPUT="echo -en \"${SDBDA_NONE}\n${SDBDA_NFC}\n${SDBDA_NFD}\n\""
assertOutputValue "NFC" "${INPUT} | ./ugrep ${UGREP_OPTS} --unit=grapheme --form=c -c '${SDBDA_NFD}' 2>/dev/null" 3 "-eq"
assertOutputValue "NFD" "${INPUT} | ./ugrep ${UGREP_OPTS} --unit=grapheme --form=d -c '${SDBDA_NFC}' 2>/dev/null" 3 "-eq"

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

FILE='engine.h' # Others are too "particular"
ARGS="--color=never -nw ''"
assertOutputValueEx "empty pattern" "./ugrep ${UGREP_OPTS} -E ${ARGS} ${FILE} 2>/dev/null" "grep ${ARGS} ${FILE}"
assertOutputValueEx "empty pattern + word (-w)" "./ugrep ${UGREP_OPTS} -Ew ${ARGS} ${FILE} 2>/dev/null" "grep -w ${ARGS} ${FILE}"
assertOutputValueEx "empty pattern + whole line (-x)" "./ugrep ${UGREP_OPTS} -Ex ${ARGS} ${FILE} 2>/dev/null" "grep -x ${ARGS} ${FILE}"

exit $?
