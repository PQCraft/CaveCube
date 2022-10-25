#!/bin/bash

{

source mkrelease.sh

NJOBS=""
rm -rf cavecube*.tar.gz cavecube*.zip
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
    [[ ! "${RESPONSE}" == "n" ]] || _exit 1
}
# Game: Linux x86_64
pkgrel() { _tar "cavecube_game_linux_x86_64.tar.gz" cavecube; }
buildrel "game" "Linux x86_64"
# Game: Linux i686
pkgrel() { _tar "cavecube_game_linux_i686.tar.gz" cavecube; }
buildrel "game" "Linux i686" M32=y
# Game: Windows x86_64
pkgrel() { _zip "cavecube_game_windows_x86_64.zip" cavecube.exe; }
buildrel "game" "Windows x86_64" WINCROSS=y
# Game: Windows i686
pkgrel() { _zip "cavecube_game_windows_i686.zip" cavecube.exe; }
buildrel "game" "Windows i686" M32=y WINCROSS=y
# Server: Linux x86_64
pkgrel() { _tar "cavecube_server_linux_x86_64.tar.gz" ccserver; }
buildrel "server" "Linux x86_64" SERVER=y
# Server: Linux i686
pkgrel() { _tar "cavecube_server_linux_i686.tar.gz" ccserver; }
buildrel "server" "Linux i686" M32=y SERVER=y
# Server: Windows x86_64
pkgrel() { _zip "cavecube_server_windows_x86_64.zip" ccserver.exe; }
buildrel "server" "Windows x86_64" SERVER=y WINCROSS=y
# Server: Windows i686
pkgrel() { _zip "cavecube_server_windows_i686.zip" ccserver.exe; }
buildrel "server" "Windows i686" M32=y SERVER=y WINCROSS=y

}
