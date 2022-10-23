#ifndef MAIN_MAIN_H
#define MAIN_MAIN_H

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
    #define MAX_THREADS 32
#endif

#define _inline inline __attribute__((always_inline))

extern CONFIG* config;
extern int quitRequest;
extern int argc;
extern char** argv;
extern char* maindir;
extern char* startdir;
extern char* localdir;

#endif
