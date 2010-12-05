#!/bin/bash

. test/assert.sh.inc

ARGS='élève test/utf8_eleve.txt'
assertOutputValue "file with match (-l)" "./ugrep -l ${ARGS} 2>/dev/null" "grep -l ${ARGS}"
assertOutputValue "file without match (-L)" "./ugrep -L ${ARGS} 2>/dev/null" "grep -L ${ARGS}"

ARGS='zzz test/utf8_eleve.txt'
assertOutputValue "file with match (-l)" "./ugrep -l ${ARGS} 2>/dev/null" "grep -l ${ARGS}"
assertOutputValue "file without match (-L)" "./ugrep -L ${ARGS} 2>/dev/null" "grep -L ${ARGS}"

exit $?