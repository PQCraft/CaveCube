#ifndef MAIN_H
#define MAIN_H

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

extern char* config;
extern int quitRequest;
extern int argc;
extern char** argv;
extern char* maindir;
extern char* startdir;

#endif
