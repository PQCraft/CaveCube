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

static force_inline bool altchdir(char* path) {
    if (chdir(path) < 0) {fprintf(stderr, "Could not chdir into '%s'\n", path); return false;}
    return true;
}

#if MODULEID == MODULEID_GAME
    #define SC_VAL "false"
#else
    #define SC_VAL "true"
#endif

static force_inline void commonSetup() {
    if (!altchdir(startdir)) exit(1);
    config = openConfig((isFile(configpath) == 1) ? configpath : NULL);
    if (!altchdir(maindir)) exit(1);
    #ifdef _WIN32
    declareConfigKey(config, "Main", "showConsole", SC_VAL, false);
    showcon = getBool(getConfigKey(config, "Main", "showConsole"));
    #endif
    initResource();
    initBlocks();
}

#ifdef __EMSCRIPTEN__
struct emscr_doGame_args {
    char* addr;
    int port;
};

static void emscr_doGame(void* _args) {
    struct emscr_doGame_args* args = _args;
    doGame(args->addr, args->port);
    emscripten_cancel_main_loop();
}
#endif

#define ARG_INVAL(x) {fprintf(stderr, "Invalid option: '%s'\n", x);}
#define ARG_INVALSYN() {fprintf(stderr, "Invalid option syntax\n");}
#define ARG_INVALVAR(x) {fprintf(stderr, "Invalid option variable: '%s'\n", x);}
#define ARG_INVALVARSYN() {fprintf(stderr, "Invalid option variable syntax\n");}

static force_inline int getNextArg(char* arg) {
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
    QueryPerformanceFrequency(&perfctfreq);
    #endif
    argc = _argc;
    argv = _argv;
    #if MODULEID == MODULEID_GAME
    int gametype = -1;
    bool chconfig = false;
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
                printf("SYNTAX: %s [-[OPTION] [VARIABLE]...]...\n", argv[0]);
                puts("OPTIONS AND VARIABLES:");
                puts("    help - Shows the help text.");
                puts("    version - Shows the version information.");
                puts("    config[=FILE] - Changes the configuration file to FILE.");
                #if MODULEID == MODULEID_GAME
                puts("    world[=NAME] - Starts a local game on the world named NAME.");
                puts("        {w|world}=NAME - Sets the world name to start to NAME.");
                puts("        {pl|players}=AMOUNT - Changes the maximum amount of players to AMOUNT.");
                puts("        {l|lan}[=BOOL] - Broadcasts the game to LAN (true if BOOL is not provided).");
                puts("    {client|connect} - Connects to a server.");
                puts("        {a|addr|address}=IP - Server address to connect to (127.0.0.1 if IP is not provided).");
                puts("        {p|port}=PORT - Server port to connect to (46000 if PORT is not provided).");
                #endif
                printf("    server - Starts a server%s.\n", (servopt) ? " (always assumed to be the first option passed)" : "");
                puts("        {w|world}=NAME - Sets the world name to host to NAME.");
                puts("        {pl|players}=AMOUNT - Changes the maximum amount of players to AMOUNT.");
                puts("        {a|addr|address}=IP - Address to start server on (0.0.0.0 if IP is not provided).");
                puts("        {p|port}=PORT - Port to start server on (46000 if PORT is not provided).");
                return 0;
            } else if (!servopt && !strcmp(name, "version")) {
                if (val || getNextArg(argv[i + 1]) != -1) {ARG_INVALSYN(); return 1;}
                printf("%s version %s\n%s\n", PROG_NAME, VER_STR, BUILD_STR);
                return 0;
            } else if (!servopt && !strcmp(name, "config")) {
                if (!val || getNextArg(argv[i + 1]) != -1) {ARG_INVALSYN(); return 1;}
                switch (isFile(val)) {
                    case -1:;
                        fprintf(stderr, "File '%s' does not exist\n", val);
                        return 1;
                        break;
                    case 0:;
                        fprintf(stderr, "'%s' is not a file\n", val);
                        return 1;
                        break;
                }
                strcpy(configpath, val);
                #if MODULEID == MODULEID_GAME
                chconfig = true;
                #endif
                #if DBGLVL(1)
                printf("Changed config to '%s'\n", configpath);
                #endif
            #if MODULEID == MODULEID_GAME
            } else if (!strcmp(name, "world")) {
                if (gametype >= 0 && gametype != 0) {ARG_INVALSYN(); return 1;}
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
                if (val || (gametype >= 0 && gametype != 1)) {ARG_INVALSYN(); return 1;}
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
                #if MODULEID == MODULEID_GAME
                if (gametype >= 0 && gametype != 2) {ARG_INVALSYN(); return 1;}
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
    #ifndef __EMSCRIPTEN__
    maindir = strdup(pathfilename(execpath()));
    #else
    maindir = strdup("/");
    #endif
    startdir = realpath(".", NULL);
    MAIN_STRPATH(startdir);
    #ifndef __EMSCRIPTEN__
    #if MODULEID == MODULEID_GAME
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
    #else
    strcpy(configpath, "ccserver.cfg");
    #endif
    #endif
    #if DBGLVL(1)
    printf("Main directory: {%s}\n", maindir);
    printf("Start directory: {%s}\n", startdir);
    #if MODULEID == MODULEID_GAME
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
    #if DBGLVL(1)
    printf("Config path: {%s}\n", configpath);
    #endif
    commonSetup();
    #ifdef _WIN32
    if (owncon && showcon) ShowWindow(GetConsoleWindow(), SW_SHOW);
    #endif
    #if MODULEID == MODULEID_GAME
    switch (gametype) {
        default:; {
            cores -= 4;
            if (cores < 2) cores = 2;
            SERVER_THREADS = cores / 2;
            cores -= SERVER_THREADS;
            MESHER_THREADS = cores;
            if (!initServer()) {
                fputs("Failed to init server\n", stderr);
                return 1;
            }
            if (!initRenderer()) {
                fputs("Failed to init renderer\n", stderr);
                return 1;
            }
            if (!startRenderer()) {
                fputs("Failed to start renderer\n", stderr);
                return 1;
            }
            printf("Starting world '%s'%s...\n", loc_opt.world, (loc_opt.lan) ? " on LAN" : "");
            int servport;
            if ((servport = startServer(NULL, 0, (loc_opt.lan) ? loc_opt.players : 1, loc_opt.world)) < 0) {
                fputs("Failed to start server\n", stderr);
                return 1;
            }
            bool game_ecode = doGame(NULL, servport);
            stopRenderer();
            stopServer();
            ret = !game_ecode;
        } break;
        case 1:; {
            cores -= 3;
            if (cores < 1) cores = 1;
            MESHER_THREADS = cores;
            if (!initServer()) {
                fputs("Failed to init server\n", stderr);
                return 1;
            }
            if (!initRenderer()) {
                fputs("Failed to init renderer\n", stderr);
                return 1;
            }
            if (!startRenderer()) {
                fputs("Failed to start renderer\n", stderr);
                return 1;
            }
            bool game_ecode = doGame(cli_opt.addr, cli_opt.port);
            stopRenderer();
            ret = !game_ecode;
        } break;
        case 2:; {
            cores -= 1;
            if (cores < 1) cores = 1;
            SERVER_THREADS = cores;
            if (!initServer()) {
                fputs("Failed to init server\n", stderr);
                return 1;
            }
            if (startServer(srv_opt.addr, srv_opt.port, srv_opt.players, srv_opt.world) < 0) {
                fputs("Failed to start server\n", stderr);
                return 1;
            }
            pause();
            stopServer();
        } break;
    }
    #else
    cores -= 1;
    if (cores < 1) cores = 1;
    SERVER_THREADS = cores;
    if (!initServer()) return 1;
    if (startServer(srv_opt.addr, srv_opt.port, srv_opt.players, srv_opt.world) < 0) {
        fputs("Failed to start server\n", stderr);
        return 1;
    }
    pause();
    stopServer();
    #endif
    #if MODULEID == MODULEID_GAME
    if (!altchdir(startdir)) return 1;
    if (!chconfig && config->changed) if (!writeConfig(config, configpath)) return 1;
    #endif
    closeConfig(config);
    free(maindir);
    free(startdir);
    #if MODULEID == MODULEID_GAME
    free(localdir);
    #endif
    return ret;
}
