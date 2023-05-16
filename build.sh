#!/bin/bash
# Expected evnironment is Linux x86_64

{

source mkrelease.sh

NJOBS="$(nproc)"
rm -rf cavecube*.tar.gz cavecube*.zip
buildrel() {
    local TYPE="${1}"
    local PLATFORM="${2}"
    inf "Building ${TYPE} for ${PLATFORM}..."
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

#######################
##  GAME USING GLFW  ##
#######################
# Linux x86_64
pkgrel() { _tar "cavecube_game_glfw_linux_x86_64.tar.gz" cavecube; }
buildrel "game" "Linux x86_64 using GLFW"
# Linux i686
pkgrel() { _tar "cavecube_game_glfw_linux_i686.tar.gz" cavecube; }
buildrel "game" "Linux i686 using GLFW" M32=y
# Windows x86_64
pkgrel() { _zip "cavecube_game_glfw_windows_x86_64.zip" cavecube.exe; }
buildrel "game" "Windows x86_64 using GLFW" WINCROSS=y
# Windows i686
pkgrel() { _zip "cavecube_game_glfw_windows_i686.zip" cavecube.exe; }
buildrel "game" "Windows i686 using GLFW" WINCROSS=y M32=y

#######################
##  GAME USING SDL2  ##
#######################
# Linux x86_64
pkgrel() { _tar "cavecube_game_sdl2_linux_x86_64.tar.gz" cavecube; }
buildrel "game" "Linux x86_64 using SDL2" USESDL2=y
# Linux i686
pkgrel() { _tar "cavecube_game_sdl2_linux_i686.tar.gz" cavecube; }
buildrel "game" "Linux i686 using SDL2" USESDL2=y M32=y
# Windows x86_64
pkgrel() { _zip "cavecube_game_sdl2_windows_x86_64.zip" cavecube.exe; }
buildrel "game" "Windows x86_64 using SDL2" WINCROSS=y USESDL2=y
# Windows i686
pkgrel() { _zip "cavecube_game_sdl2_windows_i686.zip" cavecube.exe; }
buildrel "game" "Windows i686 using SDL2" WINCROSS=y USESDL2=y M32=y

##############
##  SERVER  ##
##############
# Linux x86_64
pkgrel() { _tar "cavecube_server_linux_x86_64.tar.gz" ccserver; }
buildrel "server" "Linux x86_64" MODULE=server
# Linux i686
pkgrel() { _tar "cavecube_server_linux_i686.tar.gz" ccserver; }
buildrel "server" "Linux i686" MODULE=server M32=y
# Windows x86_64
pkgrel() { _zip "cavecube_server_windows_x86_64.zip" ccserver.exe; }
buildrel "server" "Windows x86_64" MODULE=server WINCROSS=y
# Windows i686
pkgrel() { _zip "cavecube_server_windows_i686.zip" ccserver.exe; }
buildrel "server" "Windows i686" MODULE=server WINCROSS=y M32=y

}
