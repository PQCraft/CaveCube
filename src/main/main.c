#define SDL_MAIN_HANDLED

#include "main.h"
#include <game/game.h>
#include <game/blocks.h>
#include <common/common.h>
#include <common/resource.h>
#include <common/stb_image.h>
#include <bmd/bmd.h>
#include <renderer/renderer.h>
#include <server/server.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#ifdef _WIN32
    #include <windows.h>
#endif

char* config;
file_data config_filedata;
int quitRequest = 0;

int argc;
char** argv;

char* maindir = NULL;
char* startdir = NULL;

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

#ifndef _WIN32
    #define OPTPREFIX '-'
    #define OPTPREFIXSTR "-"
#else
    #define OPTPREFIX '/'
    #define OPTPREFIXSTR "/"
#endif

#ifdef _WIN32
static bool showcon;
#endif

#ifndef SERVER
    #define SC_VAL "false"
#else
    #define SC_VAL "true"
#endif

static void commonSetup() {
    while (!(config_filedata = getTextFile("config.cfg")).data) {
        FILE* fp = fopen("config.cfg", "w");
        fclose(fp);
    }
    config = (char*)config_filedata.data;
    #ifdef _WIN32
    showcon = getConfigValBool(getConfigVarStatic(config, "main.showcon", SC_VAL, 64));
    #endif
    stbi_set_flip_vertically_on_load(true);
    initResource();
    initBlocks();
}

int main(int _argc, char** _argv) {
    #ifndef _WIN32
    bool winopt = false;
    #else
    bool winopt = true;
    DWORD procs[2];
    DWORD procct = GetConsoleProcessList((LPDWORD)procs, 2);
    bool owncon = (procct < 2);
    if (owncon) ShowWindow(GetConsoleWindow(), SW_HIDE);
    #endif
    if (_argc > 1 && (!strcmp(_argv[1], "-help") || !strcmp(_argv[1], "--help") || (winopt && !strcmp(_argv[1], "/help")))) {
        #ifndef SERVER
        printf("%s [ARGUMENTS]\n", _argv[0]);
        puts("    With no arguments, the client is connected to a server started for 1 person on 0.0.0.0.");
        puts("    Arguments:");
        puts("        "OPTPREFIXSTR"help");
        puts("            Shows the help text.");
        puts("        "OPTPREFIXSTR"server [<ADDRESS> [<PORT> [<MAX PLAYERS>]]]");
        puts("            Starts the server.");
        puts("            ADDRESS: IP address to start server on (empty or not provided for 0.0.0.0).");
        puts("            PORT: Port to start server on (-1 or not provided for auto (assigns a random port from 46000 to 46999)).");
        puts("            MAX PLAYERS: Max amount of players allowed to connect (0 or less to use the internal maximum, not provided for 1).");
        puts("        "OPTPREFIXSTR"connect <ADDRESS> <PORT>");
        puts("            Connects to a server.");
        puts("            ADDRESS: IP address to connect to.");
        puts("            PORT: Port to connect to.");
        #else
        printf("%s [<ADDRESS> [<PORT> [<MAX PLAYERS>]]]\n", _argv[0]);
        puts("    Starts the server.");
        puts("    ADDRESS: IP address to start server on (empty or not provided for 0.0.0.0).");
        puts("    PORT: Port to start server on (-1 or not provided for auto (assigns a random port from 46000 to 46999)).");
        puts("    MAX PLAYERS: Max amount of players allowed to connect (0 or less to use the internal maximum, not provided for 1).");
        #endif
        return 0;
    }
    argc = _argc;
    argv = _argv;
    maindir = strdup(pathfilename(execpath()));
    #ifndef _WIN32
    startdir = realpath(".", NULL);
    #else
    startdir = _fullpath(NULL, ".", MAX_PATH);
    #endif
    {
        uint32_t tmplen = strlen(startdir);
        #ifndef _WIN32
        char echar = '/';
        #else
        char echar = '\\';
        #endif
        if (startdir[tmplen] != echar) {
            startdir = realloc(startdir, tmplen + 2);
            startdir[tmplen] = echar;
            startdir[tmplen + 1] = 0;
        }
    }
    printf("Main directory: {%s}\n", maindir);
    printf("Start directory: {%s}\n", startdir);
    chdir(maindir);
    signal(SIGINT, sigh);
    #ifdef _WIN32
    signal(SIGSEGV, sigsegvh);
    #endif
    int cores = getCoreCt();
    if (cores < 1) cores = 1;
    #ifndef SERVER
    if (argc > 1) {
        if (!strcmp(argv[1], "-server") || (winopt && !strcmp(argv[1], "/server"))) {
            SERVER_THREADS = cores;
            commonSetup();
            if (!initServer()) return 1;
            char* addr = (argc > 2 && argv[2][0]) ? argv[2] : NULL;
            int port = (argc > 3) ? atoi(argv[3]) : -1;
            int players = (argc > 4) ? atoi(argv[4]) : 0;
            puts("Starting server...");
            if (startServer(addr, port, NULL, players) < 0) {
                fputs("Server failed to start\n", stderr);
                return 1;
            }
            pause();
        } else if (!strcmp(argv[1], "-connect") || (winopt && !strcmp(argv[1], "/connect"))) {
            cores -= 2;
            if (cores < 1) cores = 1;
            MESHER_THREADS = cores;
            if (argc < 3) {fputs("Please provide address and port\n", stderr); return 1;}
            commonSetup();
            #ifdef _WIN32
            if (owncon && showcon) ShowWindow(GetConsoleWindow(), SW_SHOW);
            #endif
            if (!initRenderer()) return 1;
            bool game_ecode = doGame(argv[2], atoi(argv[3]));
            quitRenderer();
            freeFile(config_filedata);
            return !game_ecode;
        } else {
            fprintf(stderr, "Invalid argument: %s\n", argv[1]);
            return 1;
        }
    } else {
        cores -= 4;
        if (cores < 2) cores = 2;
        SERVER_THREADS = cores / 2;
        cores -= SERVER_THREADS;
        MESHER_THREADS = cores;
        commonSetup();
        #ifdef _WIN32
        if (owncon && showcon) ShowWindow(GetConsoleWindow(), SW_SHOW);
        #endif
        if (!initServer()) return 1;
        if (!initRenderer()) return 1;
        bool game_ecode = doGame(NULL, -1);
        quitRenderer();
        freeFile(config_filedata);
        return !game_ecode;
    }
    #else
    SERVER_THREADS = cores;
    commonSetup();
    if (!initServer()) return 1;
    char* addr = (argc > 1 && argv[1][0]) ? argv[1] : NULL;
    int port = (argc > 2) ? atoi(argv[2]) : -1;
    int players = (argc > 3) ? atoi(argv[3]) : 0;
    puts("Starting server...");
    if (startServer(addr, port, NULL, players) < 0) {
        fputs("Server failed to start\n", stderr);
        return 1;
    }
    pause();
    #endif
    return 0;
}
