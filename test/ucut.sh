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
ARGS='--unit=grapheme --form=d -c 4-'
assertOutputValue "-c (graphemes)" "${INPUT} | ./ucut ${ARGS} 2>/dev/null" "v${RING_ABOVE}e${RING_ABOVE}"

INPUT="echo \"${E_ACUTE_NFD}l${RING_ABOVE}${E_GRAVE_NFD}v${RING_ABOVE}e${RING_ABOVE}\""
ARGS='--unit=grapheme --form=d -c 2-4'
assertOutputValue "-c (graphemes)" "${INPUT} | ./ucut ${ARGS} 2>/dev/null" "l${RING_ABOVE}${E_GRAVE_NFD}v${RING_ABOVE}"

INPUT="echo \"1:2:3:4:5:6:7:8:9\""
ARGS='-d: -f 3-6'
assertOutputValue "-f (fields)" "${INPUT} | ./ucut ${ARGS} 2>/dev/null" "3:4:5:6"

# TODO: without -s, whole line is printed (GNU doesn't)
ARGS='-sd: -f 10'
assertOutputValue "-f (field out of range)" "${INPUT} | ./ucut ${ARGS} 2>/dev/null" ""

INPUT="echo \"abc1def2ghi3jkl4mno5pqr\""
ARGS="-Ed '\d' -f 2-4"
assertOutputValue "-Ef (fields + RE)" "${INPUT} | ./ucut ${ARGS} 2>/dev/null" "def2ghi3jkl"

# Disabled: need to fix -E separator handling first
INPUT='echo "0,1,2,3,4,5,6,7,8,9"'
ARGS='-d , -f 1-3,6-8'
assertOutputCommand "-f (simple)" "${INPUT} | ./ucut ${ARGS} 2>/dev/null" "${INPUT} | cut ${ARGS}"
ARGS='-Ed , -f 1-3,6-8'
assertOutputCommand "-Ef (simple)" "${INPUT} | ./ucut ${ARGS} 2>/dev/null" "${INPUT} | cut ${ARGS}"
