#!/bin/bash

. test/assert.sh.inc

ARGS='élève test/utf8_eleve.txt'
assertOutputValue "count matching lines (-c)" "./ugrep -c ${ARGS} 2>/dev/null" "grep -c ${ARGS}" "-eq"
assertOutputValue "count non-matching lines (-vc)" "./ugrep -vc ${ARGS} 2>/dev/null" "grep -vc ${ARGS}" "-eq"

exit $?