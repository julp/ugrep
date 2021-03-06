#!/bin/bash

# charset: UTF-8

declare -r TESTDIR=$(dirname $(readlink -f "${BASH_SOURCE}"))
declare -r DATADIR="${TESTDIR}/data"

#        Words of the form $'string' are treated specially.   The  word  expands  to  string,  with  backslash-escaped  characters
#        replaced as specified by the ANSI C standard.  Backslash escape sequences, if present, are decoded as follows:
#               \a     alert (bell)
#               \b     backspace
#               \e
#               \E     an escape character
#               \f     form feed
#               \n     new line
#               \r     carriage return
#               \t     horizontal tab
#               \v     vertical tab
#               \\     backslash
#               \'     single quote
#               \"     double quote
#               \nnn   the eight-bit character whose value is the octal value nnn (one to three digits)
#               \xHH   the eight-bit character whose value is the hexadecimal value HH (one or two hex digits)
#               \cx    a control-x character
#
#        The expanded result is single-quoted, as if the dollar sign had not been present.
#
#        A double-quoted string preceded by a dollar sign ($"string") will cause the string to be translated according to the cur‐
#        rent locale.  If the current locale is C or POSIX, the dollar sign is ignored.  If the string is translated and replaced,
#        the replacement is double-quoted.

. ${TESTDIR}/assert.sh.inc

declare -r INPUT="a${A}b${B}c${C}d${D}e${E}"

# Full case not "supported"
# declare -r LSFI=$'\xEF\xAC\x81' # FB01, Ll
# declare -r FI=$'\x66\x69' # F + I

assertOutputValue "tr 1 CU => 0" "./utr ${UGREP_OPTS} -d [abcde] ${INPUT} 2> /dev/null" "${A}${B}${C}${D}${E}"
assertOutputValue "tr 2 CU => 0" "./utr ${UGREP_OPTS} -d [${A}${B}${C}${D}${E}] ${INPUT} 2> /dev/null" "abcde"

assertOutputValue "tr 1 CU => 1" "./utr ${UGREP_OPTS} abcde 12345 ${INPUT} 2> /dev/null" "1${A}2${B}3${C}4${D}5${E}"
assertOutputValue "tr 2 CU => 1" "./utr ${UGREP_OPTS} ${A}${B}${C}${D}${E} 12345 ${INPUT} 2> /dev/null" "a1b2c3d4e5"

assertOutputValue "tr 1 CU => 2" "./utr ${UGREP_OPTS} abcde ${N1}${N2}${N3}${N4}${N5} ${INPUT} 2> /dev/null" "${N1}${A}${N2}${B}${N3}${C}${N4}${D}${N5}${E}"
assertOutputValue "tr 2 CU => 2" "./utr ${UGREP_OPTS} ${A}${B}${C}${D}${E} ${N1}${N2}${N3}${N4}${N5} ${INPUT} 2> /dev/null" "a${N1}b${N2}c${N3}d${N4}e${N5}"

assertOutputValue "tr eliminate by function" "./utr ${UGREP_OPTS} -d fn:isalpha ${A}${N1}${B}${N2}${C}${N3} 2> /dev/null" "${N1}${N2}${N3}"
assertOutputValue "tr eliminate by set" "./utr ${UGREP_OPTS} -d \"[\p{Lu}]\" ${A}${N1}${B}${N2}${C}${N3} 2> /dev/null" "${N1}${N2}${N3}"

assertOutputValue "tr function lower => upper (1/2 => 2 CU)" "./utr ${UGREP_OPTS} fn:islower fn:toupper ${N1}${DSLLI}${N2} 2> /dev/null" "${N1}${DCLLI}${N2}"
assertOutputValue "tr replace by one (2 CU)" "./utr ${UGREP_OPTS} ${N1}${C}${A}a${B} ${DCLLI} ${INPUT} 2> /dev/null" "${DCLLI}${DCLLI}b${DCLLI}c${DCLLI}d${D}e${E}"

assertOutputValue "tr grapheme deletion" "./utr ${UGREP_OPTS} --unit=grapheme --form=d -d ${E_ACUTE_NFD} ${E_ACUTE_NFD}${A_ACUTE_NFD} 2> /dev/null" "${A_ACUTE_NFD}"
assertOutputValue "tr grapheme replacement" "./utr ${UGREP_OPTS} --unit=grapheme --form=d ${E_ACUTE_NFD} X ${E_ACUTE_NFD}${A_ACUTE_NFD} 2> /dev/null" "X${A_ACUTE_NFD}"

exit $?
