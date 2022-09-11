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
    local ERR="${?}"
    err "Error ${ERR}"
    exit "${ERR}"
}

tsk "Getting info..."
VER_MAJOR="$(grep '#define VER_MAJOR ' src/main/version.h | sed 's/#define .* //')"
VER_MINOR="$(grep '#define VER_MINOR ' src/main/version.h | sed 's/#define .* //')"
VER_PATCH="$(grep '#define VER_PATCH ' src/main/version.h | sed 's/#define .* //')"
VER="${VER_MAJOR}.${VER_MINOR}.${VER_PATCH}"
printf "${I} ${TB}Version:${TR} [%s]\n" "${VER}"
RELTEXT="$(sed -n '/DONE/,$p' .progress.txt | tail -n +2)"
printf "${I} ${TB}Release text:${TR}\n%s\n${TB}EOF${TR}\n" "${RELTEXT}"
pause

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

tsk "Done"
exit

}
