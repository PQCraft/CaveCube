#!/bin/bash

{

I="\e[0m\e[1m\e[37m[\e[36mI\e[37m]\e[0m"
E="\e[0m\e[1m\e[37m[\e[31mX\e[37m]\e[0m"
Q="\e[0m\e[1m\e[37m[\e[32m?\e[37m]\e[0m"
T="\e[0m\e[1m\e[33m>>>\e[0m"
TB="\e[0m\e[1m\e[37m"
TR="\e[0m"
inf() { printf "${I} ${TB}${1}${TR}\n"; }
err() { printf "${E} ${TB}${1}${TR}\n"; }
qry() { printf "${Q} ${TB}${1}${TR}\n"; }
tsk() { printf "${T} ${TB}${1}${TR}\n"; }

ask() {
    RESPONSE=""
    printf "${Q} ${1}" >&2
    trap 'trap - INT; kill -INT $$' INT
    read RESPONSE
    trap - INT
}
pause() {
    ask "${TB}Press enter to continue...${TR}"
}
_exit() {
    local ERR="$?"
    [[ $# -eq 0 ]] || local ERR="$1"
    err "Error ${ERR}"
    exit "${ERR}"
}

tsk "Getting info..."
VER_MAJOR="$(grep '#define VER_MAJOR ' src/main/version.h | sed 's/#define .* //')"
VER_MINOR="$(grep '#define VER_MINOR ' src/main/version.h | sed 's/#define .* //')"
VER_PATCH="$(grep '#define VER_PATCH ' src/main/version.h | sed 's/#define .* //')"
VER="${VER_MAJOR}.${VER_MINOR}.${VER_PATCH}"
printf "${I} ${TB}Version:${TR} [%s]\n" "${VER}"
getreltext() {
    sed -n '/^### DONE:$/,$p' TODO.md | tail -n +2
}
RELTEXT="$(getreltext)"
printf "${I} ${TB}Release text:${TR}\n%s\n${TB}EOF${TR}\n" "${RELTEXT}"
pause

tsk "Building..."
NJOBS=""
#NJOBS="$(nproc)"
rm -rf cavecube*.tar.gz cavecube*.zip
_tar() { rm -f "${1}"; tar -zc -f "${1}" ${@:2} 1> /dev/null; }
_zip() { rm -f "${1}"; zip -r -9 "${1}" ${@:2} 1> /dev/null; }
buildrel() {
    local TYPE="${1}"
    local OS="${2}"
    inf "Building ${TYPE} for ${OS}..."
    make ${@:3} clean 1> /dev/null || _exit
    RESPONSE=""
    while ! make ${@:3} "-j${NJOBS}" 1> /dev/null; do
        while [[ -z "${RESPONSE}" ]]; do
            ask "${TB}Build failed. Retry?${TR} (${TB}Y${TR}es/${TB}N${TR}o/${TB}C${TR}lean): "
            case "${RESPONSE,,}" in
                y | yes)
                    break
                    ;;
                n | no)
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
            *)
                RESPONSE=""
                ;;
        esac
    done
    [[ "${RESPONSE}" == "n" ]] || pkgrel || _exit
    make ${@:3} clean 1> /dev/null || _exit
    [[ "${RESPONSE}" == "n" ]] && _exit 1
}
pkgrel() { _tar "cavecube-linux.tar.gz" cavecube; }
buildrel "game" "Linux"
pkgrel() { _zip "cavecube-windows.zip" cavecube.exe; }
buildrel "game" "Windows" WIN32=y
pkgrel() { _tar "cavecube-server-linux.tar.gz" ccserver; }
buildrel "server" "Linux" SERVER=y
pkgrel() { _zip "cavecube-server-windows.zip" ccserver.exe; }
buildrel "server" "Windows" SERVER=y WIN32=y
inf "Making cavecube-data.zip..."
_zip "cavecube-data.zip" extras/ resources/
pause

tsk "Pushing..."
git add . || _exit
git commit -S -m "${VER}" -m "${RELTEXT}" || _exit
git push || _exit

tsk "Making release..."
git tag -s "${VER}" -m "${RELTEXT}" || _exit
git push --tags || _exit
gh release create "${VER}" --title "${VER}" --notes "${RELTEXT}" cavecube*.tar.gz cavecube*.zip || _exit
git checkout master || _exit
git merge dev || _exit
git push || _exit
git checkout dev || _exit

tsk "Updating AUR..."
updatepkg() {
    inf "Updating ${1}..."
    local OLDCD="$(pwd)"
    cd ".aur/${1}" || _exit
    sed -e "s/^pkgver=.*/pkgver=${VER}/" PKGBUILD > .PKGBUILD
    cat .PKGBUILD > PKGBUILD
    rm .PKGBUILD
    makepkg --printsrcinfo > .SRCINFO || _exit
    git add PKGBUILD .SRCINFO || _exit
    git commit -m "${VER}" || _exit
    git push || _exit
    cd "$OLDCD"
}
updatepkg cavecube
updatepkg cavecube-bin

tsk "Cleaning up..."
rm -rf cavecube*.tar.gz cavecube*.zip

tsk "Done"
exit

}
