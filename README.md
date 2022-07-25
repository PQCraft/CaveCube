# CaveCube 
**An in-development Minecraft/Infiniminer clone**

---
[![image](https://raw.githubusercontent.com/PQCraft/PQCraft/master/Screenshot_20220709_023207.png)](#?)

---
### Prerequisites
GLFW, GNU Make, and a C compiler are needed to build CaveCube.<br>
For Windows, the GLFW development binaries must be installed.<br>
For outdated/stable Linux distributions (such as Debian or Ubuntu), GLFW must be built from source due to the package being out of date.<br>

---
### Bulding
- To build, run `make -j`.<br>
- To build and run when done, run `make -j run`.<br>
- To build for debugging, add `DEBUG=[level]` after `make` (eg. `make DEBUG=0 -j run`). This will build the executable with debug symbols, disable symbol and section stripping, and define the internal `DEBUG` macro with the level specified.<br>
- To change the target to the standalone server, add `SERVER=y` after `make` (eg. `make SERVER=y -j`). You must also apply this to clean (`make SERVER=y clean`) to remove the server binary.<br>

