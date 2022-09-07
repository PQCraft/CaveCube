#ifndef MAIN_MAIN_H
#define MAIN_MAIN_H

#define VER_MAJOR 0
#define VER_MINOR 3
#define VER_PATCH 0

#include <common/config.h>

#ifndef DEBUG
    #define DEBUG 0
#endif

#define DBGLVL(x) (DEBUG >= (x))

#if DBGLVL(1)
    #define NAME_THREADS
#endif

#ifdef NAME_THREADS
    #define _GNU_SOURCE
#endif

#ifndef MAX_THREADS
    #define MAX_THREADS 256
#endif

extern CONFIG* config;
extern int quitRequest;
extern int argc;
extern char** argv;
extern char* maindir;
extern char* startdir;
extern char* localdir;

#endif
