#if defined(USESDL2)
    #define SDL_MAIN_HANDLED
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include "main.h"
#include "version.h"
#include <game/game.h>
#include <game/blocks.h>
#include <common/common.h>
#include <common/resource.h>
#include <common/config.h>
#if MODULEID == MODULEID_GAME
    #include <renderer/renderer.h>
#endif
#include <server/server.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef _WIN32
    #include <windows.h>
#endif

#include <common/glue.h>

char configpath[MAX_PATH] = "";
CONFIG* config = NULL;
int quitRequest = 0;

int argc;
char** argv;

char* maindir = NULL;
char* startdir = NULL;
char* localdir = NULL;

static void sigh(int sig) {
    (void)sig;
    ++quitRequest;
    signal(sig, sigh);
}

#ifdef _WIN32
static void sigsegvh(int sig) {
    (void)sig;
    fprintf(stderr, "Segmentation fault\n");
    exit(0xC0000005);
}
#endif

#ifdef _WIN32
static bool showcon;
#endif

static inline bool altchdir(char* path) {
    if (chdir(path) < 0) {fprintf(stderr, "Could not chdir into '%s'\n", path); return false;}
    return true;
}

#define MAIN_STRPATH(x) {\
    uint32_t tmplen = strlen(x);\
    if (x[tmplen] != PATHSEP) {\
        x = realloc(x, tmplen + 2);\
        x[tmplen] = PATHSEP;\
        x[tmplen + 1] = 0;\
    }\
}

#if MODULEID == MODULEID_GAME
int game_main() {
    #if defined(_WIN32)
        DWORD procs[2];
        DWORD procct = GetConsoleProcessList((LPDWORD)procs, 2);
        bool owncon = (procct < 2);
        if (owncon) ShowWindow(GetConsoleWindow(), SW_HIDE);
        QueryPerformanceFrequency(&perfctfreq);
    #endif
    #ifndef __EMSCRIPTEN__
        maindir = strdup(pathfilename(execpath()));
    #else
        maindir = strdup("/");
    #endif
    startdir = realpath(".", NULL);
    MAIN_STRPATH(startdir);
    #ifndef __EMSCRIPTEN__
        {
            #ifndef _WIN32
                char* tmpdir = getenv("HOME");
            #else
                char* tmpdir = getenv("AppData");
            #endif
            char tmpdn[MAX_PATH] = "";
            strcpy(tmpdn, tmpdir);
            strcat(tmpdn, "/.cavecube");
            if (!md(tmpdn)) return 1;
            if (!altchdir(tmpdn)) return 1;
            if (!md("worlds")) return 1;
            if (!md("resources")) return 1;
            localdir = realpath(".", NULL);
            MAIN_STRPATH(localdir);
        }
        strcpy(configpath, localdir);
        strcat(configpath, "config.cfg");
    #endif
    #if DBGLVL(1)
        printf("Main directory: {%s}\n", maindir);
        printf("Start directory: {%s}\n", startdir);
        printf("Local directory: {%s}\n", localdir);
        printf("Config path: {%s}\n", configpath);
    #endif
    if (!altchdir(maindir)) return 1;
    signal(SIGINT, sigh);
    #ifndef _WIN32
        signal(SIGPIPE, SIG_IGN);
    #else
        signal(SIGSEGV, sigsegvh);
    #endif
    if (!altchdir(startdir)) exit(1);
    if (isFile(configpath) == 1) {
        config = openConfig(configpath);
    } else {
        fprintf(stderr, "Config path is not a file: %s\n", configpath);
        config = openConfig(NULL);
    }
    if (!altchdir(maindir)) exit(1);
    #ifdef _WIN32
        declareConfigKey(config, "Main", "showConsole", "false", false);
        showcon = getBool(getConfigKey(config, "Main", "showConsole"));
    #endif
    initResource();
    initBlocks();
    if (!initServer()) {
        fputs("Failed to init server\n", stderr);
        return 1;
    }
    #ifdef _WIN32
        if (owncon && showcon) ShowWindow(GetConsoleWindow(), SW_SHOW);
    #endif
    if (!doGame()) return 1;
    if (!altchdir(startdir)) return 1;
    if (config->changed) if (!writeConfig(config, configpath)) return 1;
    closeConfig(config);
    free(maindir);
    free(startdir);
    free(localdir);
    return 0;
}
#endif

int main(int _argc, char** _argv) {
    argc = _argc;
    argv = _argv;
    int ret = 0;
    #if defined(_WIN32) && (MODULEID == MODULEID_GAME || MODULEID == MODULEID_SERVER)
    #endif
    #if MODULEID == MODULEID_GAME
        ret = game_main();
    #elif MODULEID == MODULEID_SERVER
        ret = server_main();
    #elif MODULEID == MODULEID_TOOLBOX
        ret = toolbox_main();
    #else
        return 1;
    #endif
    return ret;
}
