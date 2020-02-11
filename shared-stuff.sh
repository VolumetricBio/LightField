#!/bin/bash
#shellcheck disable=SC2034

function clear () {
    echo -ne "\x1B[0m\x1B[H\x1B[J\x1B[3J"
}

function blue-bar () {
    echo -e "\r\e[1;37;44m$*\e[K\e[0m" 1>&2
}

function red-bar () {
    echo -e "\r\e[1;33;41m$*\e[K\e[0m" 1>&2
}

function error-trap () {
    red-bar 'Failed!'
    exit 1
}

function apply-atsign-substitution () {
    local CODE='\e[0;30;48;2;146;208;80m'
    local RST='\e[0m'
    # shellcheck disable=SC2145
    echo -e "Substituting value ${CODE}${2}${RST} for token ${CODE}@@${1}@@${RST} in files ${CODE}${@:3}${RST}"
    sed -i "s/@@${1}@@/${2}/g" "${@:3}"
}

function apply-assignment-substitution () {
    local CODE='\e[0;30;48;2;0;146;208m'
    local RST='\e[0m'
    # shellcheck disable=SC2145
    echo -e "Substituting value ${CODE}${2}${RST} into variable assignment ${CODE}${1}${RST} in files ${CODE}${@:3}${RST}"
    sed -i 's/^'"${1}"'=.*$/'"${1}"'='"${2}"'/g' "${@:3}"
}

function generate-suffix () {
    local RELEASE_TRAIN="${1}"
    local BUILDTYPE="${2}"

    if [ -z "${RELEASE_TRAIN}" ]
    then
        RELEASE_TRAIN=base
    fi

    if [ "${RELEASE_TRAIN}" = "base" ]
    then
        echo "${BUILDTYPE}"
    else
        echo "${RELEASE_TRAIN}-${BUILDTYPE}"
    fi
}

DEFAULT_RELEASE_TRAIN=base
DEFAULT_ARCHITECTURE=$(uname -m)
if [ "${DEFAULT_ARCHITECTURE}" = "x86_64" ]
then
    DEFAULT_ARCHITECTURE=amd64
fi

ARCHITECTURE=amd64
RELEASE_TRAIN=base
VERSION=1.0.10.5

trap error-trap ERR
set -e
