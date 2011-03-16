#!/bin/bash

# charset: UTF-8

# WARNING:
# * only file in UTF-8 *without BOM* should be compared (grep considers BOM when ugrep don't)
# * comparaison must be about an UTF-8 file with LF line ending (grep considers CR when ugrep don't)
# * search string should be ASCII only when LC_ALL=C

. test/assert.sh.inc

ARGS='--color=never -HnA 2 -B 3 after_context ugrep.c'
assertOutputValueEx "-A 2 -B 3 (without -v)" "LC_ALL=C ./ugrep ${ARGS} 2>/dev/null" "grep ${ARGS}"
assertOutputValueEx "-A 2 -B 3 (with -v)" "LC_ALL=C ./ugrep -v ${ARGS} 2>/dev/null" "grep -v ${ARGS}"

exit $?