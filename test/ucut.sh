#!/bin/bash

# charset: UTF-8

# WARNING:
# * only file in UTF-8 *without BOM* should be compared (grep considers BOM when ugrep don't)
# * comparaison must be about an UTF-8 file with LF line ending (grep considers CR when ugrep don't)
# * search string should be ASCII only when LC_ALL=C

declare -r TESTDIR=$(dirname $(readlink -f "${BASH_SOURCE}"))
declare -r DATADIR="${TESTDIR}/data"

. ${TESTDIR}/assert.sh.inc

declare -r RING_ABOVE=$'\xCC\x8A' # 030A

INPUT='echo "0123456789"'
ARGS='-c 1-3,6-8'
assertOutputCommand "-c (simple)" "${INPUT} | ./ucut ${ARGS} 2>/dev/null" "${INPUT} | cut ${ARGS}"

INPUT='echo "0123456789"'
ARGS='--complement -c 1-3,6-8'
assertOutputCommand "-c complemented (simple)" "${INPUT} | ./ucut ${ARGS} 2>/dev/null" "${INPUT} | cut ${ARGS}"

INPUT="echo \"${E_ACUTE_NFD}l${RING_ABOVE}${E_GRAVE_NFD}v${RING_ABOVE}e${RING_ABOVE}\""
ARGS='--form=d -c 4-'
assertOutputValue "-c (graphemes)" "${INPUT} | ./ucut ${ARGS} 2>/dev/null" "v${RING_ABOVE}e${RING_ABOVE}"

# Disabled: need to fix -E separator handling first
# INPUT='echo "0,1,2,3,4,5,6,7,8,9"'
# ARGS='-d , -f 1-3,6-8'
# assertOutputCommand "-f (simple)" "${INPUT} | ./ucut ${ARGS} 2>/dev/null" "${INPUT} | cut ${ARGS}"
