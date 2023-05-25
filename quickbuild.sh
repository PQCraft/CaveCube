#!/bin/bash

{

source util.sh

NJOBS="$(nproc)"
rm -rf cavecube*.tar.gz cavecube*.zip

pkgrel() { :; }

build() {
    buildrel "${1}" "$(uname -o) using GLFW" ${@:2}
    buildrel "${1}" "$(uname -o) using SDL2" ${@:2} USESDL2=y
    buildrel "${1}" "Windows using GLFW" ${@:2} CROSS=win32
    buildrel "${1}" "Windows using SDL2" ${@:2} CROSS=win32 USESDL2=y
}
#buildmod "game"

build() {
    buildrel "${1}" "" ${@:2}
    buildrel "${1}" "Windows" ${@:2} CROSS=win32
}
buildmod "server"

build() {
    buildrel "${1}" "" ${@:2}
    buildrel "${1}" "Windows" ${@:2} CROSS=win32
}
buildmod "toolbox"

}
