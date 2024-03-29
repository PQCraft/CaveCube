# <img src="https://raw.githubusercontent.com/PQCraft/CaveCube/master/extras/icon/hicolor/512x512/apps/cavecube.png" align="right" height="95"/>CaveCube
**An in-development Minecraft/Infiniminer clone**<br>

---
<div align="center">
<a HREF="#?"><img src="https://img.shields.io/github/languages/top/PQCraft/CaveCube?color=%232267a6&label=%20&logo=C&logoColor=white" alt="Language"/></a>
<a HREF="#?"><img src="https://img.shields.io/github/license/PQCraft/CaveCube?color=informational&label=License" alt="License"/></a>
<a HREF="#?"><img src="https://img.shields.io/badge/dynamic/json?color=informational&label=Version&query=tag_name&url=https%3A%2F%2Fapi.github.com%2Frepos%2FPQCraft%2FCaveCube%2Freleases%2Flatest" alt="Version"/></a>
<a HREF="#?"><img src="https://img.shields.io/github/downloads/PQCraft/CaveCube/latest/total?color=informational&label=Downloads&logo=github" alt="Downloads"/></a>
<a HREF="#?"><img src="https://img.shields.io/github/downloads/PQCraft/CaveCube/total?color=informational&label=Total%20downloads&logo=github" alt="Total downloads"/></a>
<a HREF="#?"><img src="https://img.shields.io/github/issues/PQCraft/CaveCube?label=Issues" alt="Issues"/></a>
<a HREF="#?"><img src="https://img.shields.io/github/issues-pr/PQCraft/CaveCube?label=Pull%20requests" alt="Pull requests"/></a>
</div>

[![](https://raw.githubusercontent.com/PQCraft/PQCraft/master/Screenshot_20221128_010856.png)](#?)

---
### Contributing
Please read the [contributing guide](https://github.com/PQCraft/CaveCube/blob/master/CONTRIBUTING.md).<br>
The project status is written down in [TODO.md](https://github.com/PQCraft/CaveCube/blob/master/TODO.md).<br>

---
### Requirements
#### Building
GLFW or SDL2, GNU Make, and a C compiler with pthreads support.<br>
For Windows with MinGW, the GLFW or SDL2 development binaries must be installed from their respective websites: [GLFW](https://www.glfw.org/download), [SDL2](https://www.libsdl.org/download-2.0.php).<br>
For Windows with MSYS2, use the MINGW64 environment and install `make` for make, `mingw-w64-x86_64-gcc` for GCC, `mingw-w64-x86_64-glfw` for GLFW or `mingw-w64-x86_64-SDL2` for SDL2.<br>
For outdated/stable Linux distributions (such as Debian or Ubuntu), GLFW must be built from source due to the package being out of date.<br>
#### Running
OpenGL 3.3 or OpenGLES 3.0 support.

---
### Building
Command to download source code:<br>
```
git clone --filter=tree:0 https://github.com/PQCraft/CaveCube.git
```
- To build, run `make -j`.<br>
- To build and run once done compiling, run `make -j run`.<br>
- To build for debugging, add `DEBUG=[level]` after `make` (e.g. `make DEBUG=0 -j run`). This will build the executable with debug symbols, disable symbol stripping, and define the internal `DEBUG` macro with the level specified.<br>
- To compile with using SDL2 instead of GLFW, add `USESDL2=y` after `make`.<br>
- To compile using OpenGL ES instead of OpenGL, add `USEGLES=y` after `make`.<br>
- To change the target, add `MODULE=[target]` after `make`. The valid targets are `game` for the client (default), `server` for the standalone server, and `toolbox` for the toolbox. This variable may change the object directory and binary name.<br>
- To cross compile to another platform, add `CROSS=[platform]` after `make`. The valid cross-compilation platforms are `win32` for Windows, and `emscr` for Emscripten. This variable may change the object directory and binary name.<br>
- To compile a 32-bit binary on a 64-bit machine, add `M32=y` after `make`. This variable will change the object directory.<br>

These variables can be combined.<br>
For example, `make USESDL2=y CROSS=win32 -j run` will build CaveCube for Windows with SDL2 and run when compilation finishes.<br>
<br>
For any variables that change the object directory or binary name, you must use these flags again when running the `clean` rule in order to remove the correct files.<br>
<br>
When using MSYS2, the MINGW64 environment is recommended and always use `MSYS2=y` (e.g. `make MSYS2=y -j`, `make MSYS2=y clean`).<br>

---
### Notes <img src="https://repology.org/badge/vertical-allrepos/cavecube.svg" alt="Packaging status" align="right"/><br>
- CaveCube can be installed on Arch Linux using the [cavecube](https://aur.archlinux.org/packages/cavecube), [cavecube-bin](https://aur.archlinux.org/packages/cavecube-bin), [cavecube-sdl2](https://aur.archlinux.org/packages/cavecube-sdl2), or [cavecube-sdl2-bin](https://aur.archlinux.org/packages/cavecube-sdl2-bin) AUR package.<br>
