if [ -z "${UTIL_SH+x}" ]; then

UTIL_SH=''

I="\e[0m\e[1m\e[37m[\e[36mi\e[37m]\e[0m"
E="\e[0m\e[1m\e[37m[\e[31mX\e[37m]\e[0m"
Q="\e[0m\e[1m\e[37m[\e[32m?\e[37m]\e[0m"
H="\e[0m\e[1m\e[37m[\e[34m-\e[37m]\e[0m"
T="\e[0m\e[1m\e[33m>>>\e[0m"
TB="\e[0m\e[1m\e[37m"
TR="\e[0m"
inf() { printf "${I} ${TB}${1}${TR}\n"; }
err() { printf "${E} ${TB}${1}${TR}\n"; }
qry() { printf "${Q} ${TB}${1}${TR}\n"; }
tsk() { printf "${T} ${TB}${1}${TR}\n"; }

ask() {
    RESPONSE=""
    printf "${Q} ${1}"
    read RESPONSE
}
pause() {
    printf "${H} ${TB}Press enter to continue...${TR}"
    read
}
_exit() {
    local ERR="${?}"
    [[ ${#} -eq 0 ]] || local ERR="${1}"
    err "Error ${ERR}"
    exit "${ERR}"
}

_tar() { rm -f -- "${1}"; tar --transform 's/.*\///g' -zc -f "${1}" -- ${@:2} 1> /dev/null; }
_zip() { rm -f -- "${1}"; zip -qjr9 "./${1}" -- ${@:2}; }
_tar_r() { rm -f -- "${1}"; tar -zc -f "${1}" -- ${@:2} 1> /dev/null; }
_zip_r() { rm -f -- "${1}"; zip -qr9 "./${1}" -- ${@:2}; }

buildrel() {
    local TYPE="${1}"
    local PLATFORM="${2}"
    if [ ! -z "${PLATFORM}" ]; then PLATFORM=" for ${PLATFORM}"; fi
    inf "Building ${TYPE}${PLATFORM}..."
    make ${@:3} clean 1> /dev/null || _exit
    RESPONSE=""
    while ! make ${@:3} "-j${NJOBS}" 1> /dev/null; do
        while [[ -z "${RESPONSE}" ]]; do
            ask "${TB}Build failed. Retry?${TR} (${TB}Y${TR}es/${TB}N${TR}o/${TB}S${TR}kip/${TB}C${TR}lean): "
            case "${RESPONSE,,}" in
                y | yes | n | no | s | skip)
                    break
                    ;;
                c | clean)
                    make ${@:3} clean 1> /dev/null || _exit
                    ;;
                *)
                    RESPONSE=""
                    ;;
            esac
        done
        case "${RESPONSE,,}" in
            n | no)
                RESPONSE="n"
                break
                ;;
            s | skip)
                RESPONSE="s"
                break
                ;;
            *)
                RESPONSE=""
                ;;
        esac
    done
    [[ "${RESPONSE}" == "n" ]] || [[ "${RESPONSE}" == "s" ]] || pkgrel || _exit
    make ${@:3} clean 1> /dev/null || _exit
    [[ ! "${RESPONSE}" == "n" ]] || _exit 1
}
buildmod() {
    #inf "Building ${1}..."
    build "${1}" MODULE="${1}" ${@:2} || _exit
}

fi
