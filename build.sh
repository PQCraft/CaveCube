#!/bin/bash

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
    [[ ! "${RESPONSE}" == "n" ]] || _exit 1
}

#######################
##  GAME USING GLFW  ##
#######################
# Game: Linux x86_64 GLFW
pkgrel() { _tar "cavecube_game_glfw_linux_x86_64.tar.gz" cavecube; }
buildrel "game" "Linux x86_64 using GLFW"
# Game: Linux i686 GLFW
pkgrel() { _tar "cavecube_game_glfw_linux_i686.tar.gz" cavecube; }
buildrel "game" "Linux i686 using GLFW" M32=y
# Game: Windows x86_64 GLFW
pkgrel() { _zip "cavecube_game_glfw_windows_x86_64.zip" cavecube.exe; }
buildrel "game" "Windows x86_64 using GLFW" WINCROSS=y
# Game: Windows i686 GLFW
pkgrel() { _zip "cavecube_game_glfw_windows_i686.zip" cavecube.exe; }
buildrel "game" "Windows i686 using GLFW" WINCROSS=y M32=y

#######################
##  GAME USING SDL2  ##
#######################
# Game: Linux x86_64 SDL2
pkgrel() { _tar "cavecube_game_sdl2_linux_x86_64.tar.gz" cavecube; }
buildrel "game" "Linux x86_64 using SDL2" USESDL2=y
# Game: Linux i686 SDL2
pkgrel() { _tar "cavecube_game_sdl2_linux_i686.tar.gz" cavecube; }
buildrel "game" "Linux i686 using SDL2" USESDL2=y M32=y
# Game: Windows x86_64 SDL2
pkgrel() { _zip "cavecube_game_sdl2_windows_x86_64.zip" cavecube.exe lib/windows/x86_64/SDL2.dll; }
buildrel "game" "Windows x86_64 using SDL2" WINCROSS=y USESDL2=y
# Game: Windows i686 SDL2
pkgrel() { _zip "cavecube_game_sdl2_windows_i686.zip" cavecube.exe lib/windows/i686/SDL2.dll; }
buildrel "game" "Windows i686 using SDL2" WINCROSS=y USESDL2=y M32=y

##############
##  SERVER  ##
##############
# Server: Linux x86_64
pkgrel() { _tar "cavecube_server_linux_x86_64.tar.gz" ccserver; }
buildrel "server" "Linux x86_64" MODULE=server
# Server: Linux i686
pkgrel() { _tar "cavecube_server_linux_i686.tar.gz" ccserver; }
buildrel "server" "Linux i686" MODULE=server M32=y
# Server: Windows x86_64
pkgrel() { _zip "cavecube_server_windows_x86_64.zip" ccserver.exe; }
buildrel "server" "Windows x86_64" MODULE=server WINCROSS=y
# Server: Windows i686
pkgrel() { _zip "cavecube_server_windows_i686.zip" ccserver.exe; }
buildrel "server" "Windows i686" MODULE=server WINCROSS=y M32=y

}
