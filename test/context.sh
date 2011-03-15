#!/bin/bash

# charset: UTF-8

# WARNING: grep considers BOM when ugrep don't !
# Comparaison must be about an UTF-8 file with LF line ending

. test/assert.sh.inc

ARGS='--color=never -HnA 2 -B 3 after_context ugrep.c'
assertOutputValue "-A 2 -B 3 (without -v)" "LC_ALL=C ./ugrep ${ARGS} 2>/dev/null" "grep ${ARGS}"
assertOutputValue "-A 2 -B 3 (with -v)" "LC_ALL=C ./ugrep -v ${ARGS} 2>/dev/null" "grep -v ${ARGS}"

exit $?