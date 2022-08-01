#ifndef MAIN_H
#define MAIN_H

#define VER_MAJOR 0
#define VER_MINOR 0
#define VER_PATCH 0

//#define NAME_THREADS

#ifdef NAME_THREADS
    #define _GNU_SOURCE
#endif

#include <limits.h>

#ifndef MAX_THREADS
    #define MAX_THREADS 256
#endif

#ifndef MAX_PATH
    #ifdef PATH_MAX
        #define MAX_PATH PATH_MAX
    #else
        #define MAX_PATH 4095
    #endif
#endif

#ifndef _WIN32
    #define PRIdz "zd"
    #define PRIuz "zu"
    #define PRIxz "zx"
    #define PRIXz "zX"
#else
    #define PRIdz "Id"
    #define PRIuz "Iu"
    #define PRIxz "Ix"
    #define PRIXz "IX"
#endif

#ifdef _WIN32
    #define pause() Sleep(INFINITE)
#endif

extern char* config;
extern int quitRequest;
extern int argc;
extern char** argv;
extern char* maindir;
extern char* startdir;
extern char* localdir;

#endif
