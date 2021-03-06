# CaveCube 
**An in-development Minecraft/Infiniminer clone**

---
[![image](https://raw.githubusercontent.com/PQCraft/PQCraft/master/Screenshot_20220709_023207.png)](#?)

---
### Prerequisites
GLFW or SDL2, GNU Make, and a C compiler with POSIX thread support are needed to build CaveCube.<br>
For Windows, the GLFW or SDL2 development binaries must be installed from their respective websites: [GLFW](https://www.glfw.org/download), [SDL2](https://www.libsdl.org/download-2.0.php).<br>
For outdated/stable Linux distributions (such as Debian or Ubuntu), GLFW must be built from source due to the package being out of date.<br>

---
### Bulding
- To build, run `make -j`.<br>
- To build and run once done compiling, run `make -j run`.<br>
- To build for debugging, add `DEBUG=[level]` after `make` (eg. `make DEBUG=0 -j run`). This will build the executable with debug symbols, disable symbol and section stripping, and define the internal `DEBUG` macro with the level specified.<br>
- To change the target to the standalone server, add `SERVER=y` after `make` (eg. `make SERVER=y -j`). You must also apply this to clean (`make SERVER=y clean`) to remove the server binary.<br>
- To cross compile to Windows on a non-Windows OS, add `WIN32=y` after `make` (eg. `make WIN32=y -j`). You must also use this when running the clean rule (`make WIN32=y clean`) to remove the correct files.<br>
- To compile with using SDL2 instead of GLFW, add `USESDL2=y` after `make` (eg. `make USESDL2=y -j`).<br>

These variables can be combined.<br>
For example, `make USESDL2=y WIN32=y -j run` will build CaveCube for Windows with SDL2 and run when compilation finishes.
