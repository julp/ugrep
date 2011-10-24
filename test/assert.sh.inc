#!/bin/bash

declare -ri TRUE=0
declare -ri FALSE=1

declare -r A=$'\xF0\x9D\x98\xBC' # 1D63C, Lu
declare -r B=$'\xF0\x9D\x98\xBD'
declare -r C=$'\xF0\x9D\x98\xBE'
declare -r D=$'\xF0\x9D\x98\xBF'
declare -r E=$'\xF0\x9D\x99\x80'
declare -r F=$'\xF0\x9D\x99\x81'
declare -r G=$'\xF0\x9D\x99\x82'
declare -r H=$'\xF0\x9D\x99\x83'

declare -r N1=$'\xF0\x9D\x9F\x8F' # 1D7CE, Nd
declare -r N2=$'\xF0\x9D\x9F\x90'
declare -r N3=$'\xF0\x9D\x9F\x91'
declare -r N4=$'\xF0\x9D\x9F\x92'
declare -r N5=$'\xF0\x9D\x9F\x93'

declare -r DCLLI=$'\xF0\x90\x90\x80' # 10400, Lu
declare -r DSLLI=$'\xF0\x90\x90\xA8' # 10428, Ll

declare -r GRAVE=$'\xCC\x80'
declare -r ACUTE=$'\xCC\x81'

declare -r E_ACUTE_NFD="e${ACUTE}"
declare -r E_GRAVE_NFD="e${GRAVE}"
declare -r A_ACUTE_NFD="a${ACUTE}"
declare -r A_GRAVE_NFD="a${GRAVE}"

trap "rm -f /tmp/${PPID}.*" EXIT ERR SIGINT

UGREP_OPTS="--input=UTF-8 --stdin=UTF-8"
if [ -z "${UGREP_SYSTEM}" ]; then
    export UGREP_SYSTEM='UTF-8'
fi
if [ -n "${READER}" ]; then
    UGREP_OPTS="${UGREP_OPTS} --reader=${READER}"
fi
#echo "UGREP_OPTS = ${UGREP_OPTS}"

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
        echo "ArgumentError: ${FUNCNAME[0]}(message, command1, value[, operator = '=='])"
        return $FALSE
    fi
    __assertTrue "$1" "[[ \`$2\` ${4:-==} '$3' ]]"

    if [ $? -ne $TRUE -a -n "${DEBUG}" ]; then
        X=$(${2% 2> /dev/null})
        echo "Get    : '$X'"
        echo "Expect : '$3'"
    fi

    return $?
}

assertOutputCommand() {
    if [ $# -lt 3 ]; then
        echo "ArgumentError: ${FUNCNAME[0]}(message, command1, command2[, operator = '=='])"
        return $FALSE
    fi
    __assertTrue "$1" "[[ \`$2\` ${4:-==} \`$3\` ]]"

    return $?
}

assertOutputValueEx() {
    local tmp1 tmp2

    if [ $# -ne 3 ]; then
        echo "ArgumentError: ${FUNCNAME[0]}(message, command1, command2)"
        return $FALSE
    fi

    tmp1=`mktemp /tmp/${PPID}.XXXXXX`
    tmp2=`mktemp /tmp/${PPID}.XXXXXX`
    eval "\`$2 > $tmp1\`"
    eval "\`$3 > $tmp2\`"

    assertExitValue "$1" "diff -quN ${tmp1} ${tmp2} > /dev/null" $TRUE

    if [ $? -ne $TRUE -a -n "${DEBUG}" ]; then
        diff --text -uN "${tmp1}" "${tmp2}"
    fi

    return $?
}

assertOutputValueExIgnoreBlanks() {
    local tmp1 tmp2

    if [ $# -ne 3 ]; then
        echo "ArgumentError: ${FUNCNAME[0]}(message, command1, command2)"
        return $FALSE
    fi

    tmp1=`mktemp /tmp/${PPID}.XXXXXX`
    tmp2=`mktemp /tmp/${PPID}.XXXXXX`
    eval "\`$2 > $tmp1\`"
    eval "\`$3 > $tmp2\`"

    assertExitValue "$1" "diff -bquN ${tmp1} ${tmp2} > /dev/null" $TRUE

    if [ $? -ne $TRUE -a -n "${DEBUG}" ]; then
        diff --text -uN ${tmp1} ${tmp2}
    fi

    return $?
}
