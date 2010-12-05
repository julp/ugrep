#!/bin/bash

. test/assert.sh.inc

# ./ugrep -q élève test/utf8_eleve.txt 2>/dev/null
# assertTrue "[[ $? -eq 0 ]]"
# ./ugrep -q zzz test/utf8_eleve.txt 2>/dev/null
# assertTrue "[[ $? -eq 1 ]]"
# ./ugrep -q élève /unexistant 2>/dev/null
# assertTrue "[[ $? -gt 1 ]]"
# ./ugrep -q élève /unexistant test/utf8_eleve.txt 2>/dev/null
# assertTrue "[[ $? -eq 0 ]]"

assertExitValue "exit value with one or more lines selected" "./ugrep -q élève test/utf8_eleve.txt 2>/dev/null" 0
assertExitValue "exit value with no lines selected" "./ugrep -q zzz test/utf8_eleve.txt 2>/dev/null" 1
assertExitValue "exit value with error and no more file" "./ugrep -q élève /unexistant 2>/dev/null" 1 "-gt"

exit $?