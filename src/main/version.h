#ifndef MAIN_VERSION_H
#define MAIN_VERSION_H

#define VER_MAJOR 0
#define VER_MINOR 5
#define VER_PATCH 4

#define _STR(x) #x
#define STR(x) _STR(x)

#define VER_STR STR(VER_MAJOR) "." STR(VER_MINOR) "." STR(VER_PATCH)

#define PROG_NAME "CaveCube"
#define MODULE_NAME STR(MODULE)

#define FULL_PROG_NAME PROG_NAME " " MODULE_NAME

#if defined(USESDL2)
    #define IO_ABST "SDL2"
#else
    #define IO_ABST "GLFW"
#endif
#if defined(USEGLES)
    #define GFX_API "OpenGL ES"
#else
    #define GFX_API "OpenGL"
#endif
#define BUILD_STR "Built with " IO_ABST " for " GFX_API "."
    
#endif
