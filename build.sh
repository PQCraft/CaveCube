#!/bin/bash
# Expected evnironment is Linux x86_64

{

source util.sh

NJOBS="$(nproc)"
rm -rf cavecube*.tar.gz cavecube*.zip

#######################
##  GAME USING GLFW  ##
#######################
build() {
    # Linux x86_64 GLFW
    pkgrel() { _tar "cavecube_game_glfw_linux_x86_64.tar.gz" cavecube; }
    buildrel "${1}" "Linux x86_64 using GLFW" ${@:2}
    # Linux i686 GLFW
    pkgrel() { _tar "cavecube_game_glfw_linux_i686.tar.gz" cavecube; }
    buildrel "${1}" "Linux i686 using GLFW" ${@:2} M32=y
    # Windows x86_64 GLFW
    pkgrel() { _zip "cavecube_game_glfw_windows_x86_64.zip" cavecube.exe; }
    buildrel "${1}" "Windows x86_64 using GLFW" ${@:2} CROSS=win32
    # Windows i686 GLFW
    pkgrel() { _zip "cavecube_game_glfw_windows_i686.zip" cavecube.exe; }
    buildrel "${1}" "Windows i686 using GLFW" ${@:2} CROSS=win32 M32=y
    # Linux x86_64 SDL2
}
buildmod "game"

#######################
##  GAME USING SDL2  ##
#######################
build() {
    pkgrel() { _tar "cavecube_game_sdl2_linux_x86_64.tar.gz" cavecube; }
    buildrel "${1}" "Linux x86_64 using SDL2" ${@:2}
    # Linux i686 SDL2
    pkgrel() { _tar "cavecube_game_sdl2_linux_i686.tar.gz" cavecube; }
    buildrel "${1}" "Linux i686 using SDL2" ${@:2} M32=y
    # Windows x86_64 SDL2
    pkgrel() { _zip "cavecube_game_sdl2_windows_x86_64.zip" cavecube.exe; }
    buildrel "${1}" "Windows x86_64 using SDL2" ${@:2} CROSS=win32
    # Windows i686 SDL2
    pkgrel() { _zip "cavecube_game_sdl2_windows_i686.zip" cavecube.exe; }
    buildrel "${1}" "Windows i686 using SDL2" ${@:2} CROSS=win32 M32=y
}
buildmod "game" USESDL2=y

##############
##  SERVER  ##
##############
build() {
    # Linux x86_64
    pkgrel() { _tar "cavecube_server_linux_x86_64.tar.gz" ccserver; }
    buildrel "${1}" "Linux x86_64" ${@:2}
    # Linux i686
    pkgrel() { _tar "cavecube_server_linux_i686.tar.gz" ccserver; }
    buildrel "${1}" "Linux i686" ${@:2} M32=y
    # Windows x86_64
    pkgrel() { _zip "cavecube_server_windows_x86_64.zip" ccserver.exe; }
    buildrel "${1}" "Windows x86_64" ${@:2} CROSS=win32
    # Windows i686
    pkgrel() { _zip "cavecube_server_windows_i686.zip" ccserver.exe; }
    buildrel "${1}" "Windows i686" ${@:2} CROSS=win32 M32=y
}
buildmod "server"

###############
##  TOOLBOX  ##
###############
build() {
    # Linux x86_64
    pkgrel() { _tar "cavecube_toolbox_linux_x86_64.tar.gz" cctoolbx; }
    buildrel "${1}" "Linux x86_64" ${@:2}
    # Linux i686
    pkgrel() { _tar "cavecube_toolbox_linux_i686.tar.gz" cctoolbx; }
    buildrel "${1}" "Linux i686" ${@:2} M32=y
    # Windows x86_64
    pkgrel() { _zip "cavecube_toolbox_windows_x86_64.zip" cctoolbx.exe; }
    buildrel "${1}" "Windows x86_64" ${@:2} CROSS=win32
    # Windows i686
    pkgrel() { _zip "cavecube_toolbox_windows_i686.zip" cctoolbx.exe; }
    buildrel "${1}" "Windows i686" ${@:2} CROSS=win32 M32=y
}
buildmod "toolbox"

}
