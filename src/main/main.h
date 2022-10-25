#ifndef MAIN_MAIN_H
#define MAIN_MAIN_H

#include <common/config.h>

#ifndef DEBUG
    #define DEBUG -1
#endif

#define DBGLVL(x) (DEBUG >= (x))

#if DBGLVL(0)
    #define NAME_THREADS
#endif

#ifndef MAX_THREADS
    #define MAX_THREADS 32
#endif

#define force_inline inline __attribute__((always_inline))

extern CONFIG* config;
extern int quitRequest;
extern int argc;
extern char** argv;
extern char* maindir;
extern char* startdir;
extern char* localdir;

#endif
