#!/bin/bash

{

source util.sh

tsk "Getting info..."
VER_MAJOR="$(grep '#define VER_MAJOR ' src/main/version.h | sed 's/#define .* //')"
VER_MINOR="$(grep '#define VER_MINOR ' src/main/version.h | sed 's/#define .* //')"
VER_PATCH="$(grep '#define VER_PATCH ' src/main/version.h | sed 's/#define .* //')"
VER="${VER_MAJOR}.${VER_MINOR}.${VER_PATCH}"
printf "${I} ${TB}Version:${TR} [%s]\n" "${VER}"
getchanges() {
    sed -n '/^### DONE:$/,$p' TODO.md | tail -n +2
}
CNGTEXT="$(getchanges)"
getreltext() {
    echo '### Changes'
    echo "${CNGTEXT}"
    echo '### How to run'
    echo '1. Download the `zip` or `tar.gz` matching your platform (the format is `cavecube_<module>[_<differences>][_<OS>_<architecture>]`)'
    echo '    - `data` is the game resources'
    echo '    - `extras` are extra files such as docs and icons'
    echo '    - `game` is the client (connect to servers and host local worlds)'
    echo '    - `server` is the server (host worlds)'
    echo '    - `toolbox` is the toolbox (a Swiss Army Knife for CaveCube data formats)'
    echo '2. If downloading the game or server, also download `cavecube_data.zip`'
    echo '3. Extract the archives into the same folder'
    echo '4. Run the executable'
}
RELTEXT="$(getreltext)"
printf "${I} ${TB}Release text:${TR}\n%s\n${TB}EOF${TR}\n" "${RELTEXT}"
pause

tsk "Building..."
./build.sh || _exit
inf "Making cavecube_data.zip..."
_zip_r "cavecube_data.zip" resources/
inf "Making cavecube_extras.zip..."
_zip_r "cavecube_extras.zip" extras/
pause

tsk "Pushing..."
git add . || _exit
git commit -S -m "${VER}" -m "${CNGTEXT}" || _exit
git push || _exit

tsk "Making release..."
git tag -s "${VER}" -m "${CNGTEXT}" || _exit
git push --tags || _exit
gh release create "${VER}" --title "${VER}" --notes "${RELTEXT}" cavecube*.tar.gz cavecube*.zip || _exit

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
    cd "${OLDCD}"
}
updatepkg cavecube
updatepkg cavecube-bin
updatepkg cavecube-sdl2
updatepkg cavecube-sdl2-bin

tsk "Cleaning up..."
rm -rf cavecube*.tar.gz cavecube*.zip

tsk "Done"
exit

fi

}
