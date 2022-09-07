#if defined(USESDL2)
    #define SDL_MAIN_HANDLED
#endif

#include "main.h"
#include <game/game.h>
#include <game/blocks.h>
#include <common/common.h>
#include <common/resource.h>
#include <common/config.h>
#include <bmd/bmd.h>
#include <renderer/renderer.h>
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

static bool altchdir(char* path) {
    if (chdir(path) < 0) {fprintf(stderr, "Could not chdir into '%s'\n", path); return false;}
    return true;
}

#ifndef SERVER
    #define SC_VAL "false"
#else
    #define SC_VAL "true"
#endif

static void commonSetup() {
    if (!altchdir(startdir)) exit(1);
    config = openConfig((isFile(configpath) == 1) ? configpath : NULL);
    if (!altchdir(maindir)) exit(1);
    #ifdef _WIN32
    declareConfigKey(config, "Main", "showConsole", SC_VAL, false);
    showcon = getBool(getConfigKey(config, "showConsole"));
    #endif
    initResource();
    initBlocks();
}

#define ARG_INVAL(x) {fprintf(stderr, "Invalid option: '%s'\n", x);}
#define ARG_INVALSYN() {fprintf(stderr, "Invalid option syntax\n");}
#define ARG_INVALVAR(x) {fprintf(stderr, "Invalid option variable: '%s'\n", x);}
#define ARG_INVALVARSYN() {fprintf(stderr, "Invalid option variable syntax\n");}

static inline int getNextArg(char* arg) {
    if (!arg || !arg[0] || arg[0] == '-') return -1;
    int i = 0;
    for (; arg[i]; ++i) {if (arg[i] == '=') break;}
    return i;
}

#define MAIN_READOPT() {\
    name = argv[i];\
    val = name + nlen;\
    val = (*val) ? val + 1 : NULL;\
    *(name + nlen) = 0;\
}

#define MAIN_STRPATH(x) {\
    uint32_t tmplen = strlen(x);\
    if (x[tmplen] != PATHSEP) {\
        x = realloc(x, tmplen + 2);\
        x[tmplen] = PATHSEP;\
        x[tmplen + 1] = 0;\
    }\
}

int main(int _argc, char** _argv) {
    int ret = 0;
    #ifdef _WIN32
    DWORD procs[2];
    DWORD procct = GetConsoleProcessList((LPDWORD)procs, 2);
    bool owncon = (procct < 2);
    if (owncon) ShowWindow(GetConsoleWindow(), SW_HIDE);
    #endif
    argc = _argc;
    argv = _argv;
    maindir = strdup(pathfilename(execpath()));
    startdir = realpath(".", NULL);
    MAIN_STRPATH(startdir);
    #ifndef SERVER
    {
        #ifndef _WIN32
        char* tmpdir = getenv("HOME");
        char* tmpdn = ".cavecube";
        #else
        char* tmpdir = getenv("AppData");
        char* tmpdn = "cavecube";
        #endif
        if (!altchdir(tmpdir)) return 1;
        switch (isFile(tmpdn)) {
            case -1:;
                mkdir(tmpdn);
                break;
            case 1:;
                fprintf(stderr, "'%s' is not a directory", tmpdn);
                return 1;
                break;
        }
        if (!altchdir(tmpdn)) return 1;
        localdir = realpath(".", NULL);
        MAIN_STRPATH(localdir);
    }
    strcpy(configpath, localdir);
    strcat(configpath, "cavecube.cfg");
    #else
    strcpy(configpath, "ccserver.cfg");
    #endif
    #if DBGLVL(1)
    printf("Main directory: {%s}\n", maindir);
    printf("Start directory: {%s}\n", startdir);
    #ifndef SERVER
    printf("Local directory: {%s}\n", localdir);
    #endif
    #endif
    if (!altchdir(maindir)) return 1;
    signal(SIGINT, sigh);
    #ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
    #else
    signal(SIGSEGV, sigsegvh);
    #endif
    int cores = getCoreCt();
    if (cores < 1) cores = 1;
    #ifndef SERVER
    int gametype = -1;
    bool servopt = false;
    struct {
        char* world;
        bool lan;
        int players;
    } loc_opt = {"world", false, 8};
    struct {
        char* addr;
        int port;
    } cli_opt = {NULL, 46000};
    #else
    bool servopt = true;
    #endif
    struct {
        char* world;
        char* addr;
        int port;
        int players;
    } srv_opt = {"world", NULL, 46000, 32};
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' || servopt) {
            int nlen = 0;
            char* name = NULL;
            char* val = NULL;
            if (!servopt) {
                nlen = getNextArg(&argv[i][1]);
                name = &argv[i][1];
                val = name + nlen;
                val = (*val) ? val + 1 : NULL;
                *(name + nlen) = 0;
            }
            if (nlen == -1) {
                ARG_INVAL(argv[i]);
                return 1;
            } else if (!servopt && !strcmp(name, "help")) {
                if (val || getNextArg(argv[i + 1]) != -1) {ARG_INVALSYN(); return 1;}
                return 0;
            } else if (!servopt && !strcmp(name, "version")) {
                if (val || getNextArg(argv[i + 1]) != -1) {ARG_INVALSYN(); return 1;}
                #ifndef SERVER
                printf("CaveCube");
                #else
                printf("CaveCube server");
                #endif
                printf(" version %d.%d.%d\n", VER_MAJOR, VER_MINOR, VER_PATCH);
                return 0;
            } else if (!servopt && !strcmp(name, "config")) {
                if (!val || getNextArg(argv[i + 1]) != -1) {ARG_INVALSYN(); return 1;}
                switch (isFile(val)) {
                    case -1:;
                        fprintf(stderr, "File '%s' does not exist", val);
                        return 1;
                        break;
                    case 0:;
                        fprintf(stderr, "'%s' is not a file", val);
                        return 1;
                        break;
                }
                strcpy(configpath, val);
                printf("Changed config to '%s'\n", configpath);
            #ifndef SERVER
            } else if (!strcmp(name, "world")) {
                if (gametype >= 0) {ARG_INVALSYN(); return 1;}
                gametype = 0;
                if (val) loc_opt.world = val;
                ++i;
                while ((nlen = getNextArg(argv[i])) != -1) {
                    MAIN_READOPT();
                    if (!strcmp(name, "w") || !strcmp(name, "world")) {if (!val) {ARG_INVALVARSYN(); return 1;} loc_opt.world = val;}
                    else if (!strcmp(name, "l") || !strcmp(name, "lan")) loc_opt.lan = (val) ? getBool(val) : true;
                    else if (!strcmp(name, "pl") || !strcmp(name, "players")) {if (!val) {ARG_INVALVARSYN(); return 1;} loc_opt.players = atoi(val); if (loc_opt.players < 1) loc_opt.players = 1;}
                    else {ARG_INVALVAR(name); return 1;}
                    ++i;
                }
                --i;
            } else if (!strcmp(name, "client") || !strcmp(name, "connect")) {
                if (val || gametype >= 0) {ARG_INVALSYN(); return 1;}
                gametype = 1;
                ++i;
                while ((nlen = getNextArg(argv[i])) != -1) {
                    MAIN_READOPT();
                    if (!strcmp(name, "a") || !strcmp(name, "addr") || !strcmp(name, "address")) {if (!val) {ARG_INVALVARSYN(); return 1;} cli_opt.addr = val;}
                    else if (!strcmp(name, "p") || !strcmp(name, "port")) {if (!val) {ARG_INVALVARSYN(); return 1;} cli_opt.port = atoi(val); if (cli_opt.port < 0) cli_opt.port = 0;}
                    else {ARG_INVALVAR(name); return 1;}
                    ++i;
                }
                --i;
            #endif
            } else if (servopt || !strcmp(name, "server")) {
                if (val) {ARG_INVALSYN(); return 1;}
                #ifndef SERVER
                if (gametype >= 0) {ARG_INVALSYN(); return 1;}
                gametype = 2;
                #endif
                if (!servopt) ++i;
                servopt = false;
                while ((nlen = getNextArg(argv[i])) != -1) {
                    MAIN_READOPT();
                    if (!strcmp(name, "w") || !strcmp(name, "world")) {if (!val) {ARG_INVALVARSYN(); return 1;} srv_opt.world = val;}
                    else if (!strcmp(name, "a") || !strcmp(name, "addr") || !strcmp(name, "address")) {if (!val) {ARG_INVALVARSYN(); return 1;} srv_opt.addr = val;}
                    else if (!strcmp(name, "p") || !strcmp(name, "port")) {if (!val) {ARG_INVALVARSYN(); return 1;} srv_opt.port = atoi(val); if (srv_opt.port < 0) srv_opt.port = 0;}
                    else if (!strcmp(name, "pl") || !strcmp(name, "players")) {if (!val) {ARG_INVALVARSYN(); return 1;} srv_opt.players = atoi(val); if (srv_opt.players < 1) srv_opt.players = 1;}
                    else {ARG_INVALVAR(name); return 1;}
                    ++i;
                }
                --i;
            } else {
                ARG_INVAL(argv[i]);
                return 1;
            }
        } else {
            ARG_INVAL(argv[i]);
            return 1;
        }
    }
    #if DBGLVL(1)
    printf("Config path: {%s}\n", configpath);
    #endif
    commonSetup();
    #ifdef _WIN32
    if (owncon && showcon) ShowWindow(GetConsoleWindow(), SW_SHOW);
    #endif
    #ifndef SERVER
    switch (gametype) {
        default:; {
            cores -= 4;
            if (cores < 2) cores = 2;
            SERVER_THREADS = cores / 2;
            cores -= SERVER_THREADS;
            MESHER_THREADS = cores;
            if (!initServer()) return 1;
            if (!initRenderer()) return 1;
            printf("Starting world '%s'%s...\n", loc_opt.world, (loc_opt.lan) ? " on LAN" : "");
            int servport;
            if ((servport = startServer(NULL, -1, (loc_opt.lan) ? loc_opt.players : 1, loc_opt.world)) < 0) {
                fputs("Server failed to start\n", stderr);
                return 1;
            }
            bool game_ecode = doGame(NULL, servport);
            stopServer();
            quitRenderer();
            ret = !game_ecode;
            break;
        }
        case 1:; {
            cores -= 3;
            if (cores < 1) cores = 1;
            MESHER_THREADS = cores;
            if (!initRenderer()) return 1;
            bool game_ecode = doGame(cli_opt.addr, cli_opt.port);
            stopServer();
            quitRenderer();
            ret = !game_ecode;
            break;
        }
        case 2:; {
            cores -= 1;
            if (cores < 1) cores = 1;
            SERVER_THREADS = cores;
            if (!initServer()) return 1;
            if (startServer(srv_opt.addr, srv_opt.port, srv_opt.players, srv_opt.world) < 0) {
                fputs("Server failed to start\n", stderr);
                return 1;
            }
            pause();
            stopServer();
            break;
        }
    }
    #else
    cores -= 1;
    if (cores < 1) cores = 1;
    SERVER_THREADS = cores;
    if (!initServer()) return 1;
    if (startServer(srv_opt.addr, srv_opt.port, srv_opt.players, srv_opt.world) < 0) {
        fputs("Server failed to start\n", stderr);
        return 1;
    }
    pause();
    stopServer();
    #endif
    #ifndef SERVER
    if (!altchdir(startdir)) return 1;
    if (config->changed) if (!writeConfig(config, configpath)) return 1;
    #endif
    closeConfig(config);
    free(maindir);
    free(startdir);
    #ifndef SERVER
    free(localdir);
    #endif
    return ret;
}
