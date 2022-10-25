#ifndef MAIN_VERSION_H
#define MAIN_VERSION_H

#define VER_MAJOR 0
#define VER_MINOR 4
#define VER_PATCH 5

#define _STR(x) #x
#define STR(x) _STR(x)

#define VER_STR STR(VER_MAJOR) "." STR(VER_MINOR) "." STR(VER_PATCH)

#define PROG_NAME "CaveCube"

#ifndef SERVER
    #define FULL_PROG_NAME PROG_NAME
#else
    #define FULL_PROG_NAME PROG_NAME " server"
#endif

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
