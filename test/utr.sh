#!/bin/bash

# charset: UTF-8

# WARNING:
# * only file in UTF-8 *without BOM* should be compared (grep considers BOM when ugrep don't)
# * comparaison must be about an UTF-8 file with LF line ending (grep considers CR when ugrep don't)
# * search string should be ASCII only when LC_ALL=C

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

# declare -r A=$'\xF0\x9D\x98\xBC'
# declare -r B=$'\xF0\x9D\x98\xBD'
# declare -r C=$'\xF0\x9D\x98\xBE'
# declare -r D=$'\xF0\x9D\x98\xBF'
# declare -r E=$'\xF0\x9D\x99\x80'

declare -r A=$'\xF0\x9D\x9A\xA8'
declare -r B=$'\xF0\x9D\x9A\xA9'
# declare -r C=$''
# declare -r D=$''
# declare -r E=$''

# declare -r INPUT="a${A}b${B}c${C}d${D}e${E}"
declare -r INPUT="a${A}b${B}"

. ${TESTDIR}/assert.sh.inc


assertExpectingOutputValue "tr 1 cu => 0" "./utr -d '[ab]' '${INPUT}'" "${A}${B}"
assertExpectingOutputValue "tr 2 cu => 0" "./utr -d '[${A}${B}]' '${INPUT}'" "ab"

# Actually (but not working: characters on 2 code units lead to character repetitions? :o):
# 1 => 0
# 2 => 0

# TODO:

# 1 => 1
# 2 => 1

# 1 => 2
# 2 => 2

exit $?
