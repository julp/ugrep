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

declare -r A=$'\xF0\x9D\x98\xBC'
declare -r B=$'\xF0\x9D\x98\xBD'
declare -r C=$'\xF0\x9D\x98\xBE'
declare -r D=$'\xF0\x9D\x98\xBF'
declare -r E=$'\xF0\x9D\x99\x80'
declare -r INPUT="a${A}b${B}c${C}d${D}e${E}"
declare -r N1=$'\xF0\x9D\x9F\x8F'
declare -r N2=$'\xF0\x9D\x9F\x90'
declare -r N3=$'\xF0\x9D\x9F\x91'
declare -r N4=$'\xF0\x9D\x9F\x92'
declare -r N5=$'\xF0\x9D\x9F\x93'

. ${TESTDIR}/assert.sh.inc

assertOutputValue "tr 1 CU => 0" "./utr -d [abcde] ${INPUT} 2> /dev/null" "${A}${B}${C}${D}${E}"
assertOutputValue "tr 2 CU => 0" "./utr -d [${A}${B}${C}${D}${E}] ${INPUT} 2> /dev/null" "abcde"

assertOutputValue "tr 1 CU => 1" "./utr abcde 12345 ${INPUT} 2> /dev/null" "1${A}2${B}3${C}4${D}5${E}"
assertOutputValue "tr 2 CU => 1" "./utr ${A}${B}${C}${D}${E} 12345 ${INPUT} 2> /dev/null" "a1b2c3d4e5"

assertOutputValue "tr 1 CU => 2" "./utr abcde ${N1}${N2}${N3}${N4}${N5} ${INPUT} 2> /dev/null" "${N1}${A}${N2}${B}${N3}${C}${N4}${D}${N5}${E}"
assertOutputValue "tr 2 CU => 2" "./utr ${A}${B}${C}${D}${E} ${N1}${N2}${N3}${N4}${N5} ${INPUT} 2> /dev/null" "a${N1}b${N2}c${N3}d${N4}e${N5}"

exit $?
