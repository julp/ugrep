#!/bin/bash

TRUE=0
FALSE=1

__assertTrue() {
    if eval $2; then
        printf "%s, %s: [ \e[%d;01m%s\e[0m ]\n" `basename $0` "$1" 32 OK
        return $TRUE
    else
        #echo -e "`basename $0`: \e[31;01mKO\e[0m"
        printf "%s, %s: [ \e[%d;01m%s\e[0m ]\n" `basename $0` "$1" 31 KO
        echo "assertion: $2 fails"
        return $FALSE
    fi
}

assertExitValue() {
    if [ $# -lt 3 ]; then
        echo "ArgumentError: ${FUNCNAME[0]}(message, command, expected value[, operator = 'eq'])"
        return $FALSE
    fi
    eval $2
    __assertTrue "$1" "[ \$? ${4:--eq} $3 ]"
    return $?
}

assertOutputValue() {
    if [ $# -lt 3 ]; then
        echo "ArgumentError: ${FUNCNAME[0]}(message, command1, command2[, operator = '='])"
        return $FALSE
    fi
    __assertTrue "$1" "[ \`$2\` ${4:-=} \`$3\` ]"
    return $?
}
