#ifndef MAIN_MAIN_H
#define MAIN_MAIN_H

#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#endif

#include <common/config.h>

#ifndef DEBUG
    #define DEBUG -1
#endif

// Debug levels:
// =< -1: No debug (no debug symbols)
// 0: Silent debug (debug symbols)
// 1: Simple debug (debug symbols + print out non-constant debug text)
// 2: Verbose debug (debug symbols + print out all debug text)

#define DBGLVL(x) (DEBUG >= (x))

#if DBGLVL(0) && !defined(__EMSCRIPTEN__)
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
