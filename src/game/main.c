#include "main.h"
#include "game.h"
#include <common.h>
#include <resource.h>
#include <bmd.h>
#include <renderer.h>
#include <server.h>
#include <stb_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#ifdef _WIN32
    #include <windows.h>
#endif

uint32_t indices[] = {
    0, 1, 2,
    2, 3, 0,
};

float vertices[6][20] = {
    { 0.5,  0.5,  0.5,  0.0,  1.0, 
     -0.5,  0.5,  0.5,  1.0,  1.0, 
     -0.5, -0.5,  0.5,  1.0,  0.0, 
      0.5, -0.5,  0.5,  0.0,  0.0,},
    {-0.5,  0.5, -0.5,  0.0,  1.0, 
      0.5,  0.5, -0.5,  1.0,  1.0, 
      0.5, -0.5, -0.5,  1.0,  0.0, 
     -0.5, -0.5, -0.5,  0.0,  0.0,},
    {-0.5,  0.5,  0.5,  0.0,  1.0, 
     -0.5,  0.5, -0.5,  1.0,  1.0, 
     -0.5, -0.5, -0.5,  1.0,  0.0, 
     -0.5, -0.5,  0.5,  0.0,  0.0,},
    { 0.5,  0.5, -0.5,  0.0,  1.0, 
      0.5,  0.5,  0.5,  1.0,  1.0, 
      0.5, -0.5,  0.5,  1.0,  0.0, 
      0.5, -0.5, -0.5,  0.0,  0.0,},
    {-0.5,  0.5,  0.5,  0.0,  1.0, 
      0.5,  0.5,  0.5,  1.0,  1.0, 
      0.5,  0.5, -0.5,  1.0,  0.0, 
     -0.5,  0.5, -0.5,  0.0,  0.0,},
    {-0.5, -0.5, -0.5,  0.0,  1.0, 
      0.5, -0.5, -0.5,  1.0,  1.0, 
      0.5, -0.5,  0.5,  1.0,  0.0, 
     -0.5, -0.5,  0.5,  0.0,  0.0,},
};

char* config;
file_data config_filedata;
int quitRequest = 0;

int argc;
char** argv;

char* maindir = NULL;
char* startdir = NULL;

void sigh(int sig) {
    (void)sig;
    ++quitRequest;
}

#ifndef _WIN32
    #define OPTPREFIX '-'
    #define OPTPREFIXSTR "-"
#else
    #define OPTPREFIX '/'
    #define OPTPREFIXSTR "/"
#endif

int main(int _argc, char** _argv) {
    #ifndef _WIN32
    bool winopt = false;
    #else
    bool winopt = true;
    #endif
    if (_argc > 1 && (!strcmp(_argv[1], "-help") || !strcmp(_argv[1], "--help") || (winopt && !strcmp(_argv[1], "/help")))) {
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
    int cores = getCoreCt() - 4;
    if (cores < 1) cores = 1;
    while (!(config_filedata = getTextFile("config.cfg")).data) {
        FILE* fp = fopen("config.cfg", "w");
        fclose(fp);
    }
    config = (char*)config_filedata.data;
    stbi_set_flip_vertically_on_load(true);
    if (argc > 1 && (!strcmp(argv[1], "-server") || (winopt && !strcmp(argv[1], "/server")))) {
        SERVER_THREADS = cores;
        if (!initServer()) return 1;
        char* addr = (argc > 2 && argv[2][0]) ? argv[2] : NULL;
        int port = (argc > 3) ? atoi(argv[3]) : -1;
        int players = (argc > 4) ? atoi(argv[4]) : 0;
        puts("Starting server...");
        if (servStart(addr, port, NULL, players) < 0) {
            fputs("Server failed to start\n", stderr);
            return 1;
        }
        pause();
    } else if (argc > 1 && (!strcmp(argv[1], "-connect") || (winopt && !strcmp(argv[1], "/connect")))) {
        MESHER_THREADS = cores;
        //microwait(1000000);
        if (argc < 3) {fputs("Please provide address and port\n", stderr); return 1;}
        if (!initRenderer()) return 1;
        bool game_ecode = doGame(argv[2], atoi(argv[3]));
        quitRenderer();
        //freeAllResources();
        freeFile(config_filedata);
        return !game_ecode;
    } else if (argc > 1) {
        fprintf(stderr, "Invalid argument: %s\n", argv[1]);
        return 1;
    } else {
        if (cores < 2) cores = 2;
        SERVER_THREADS = cores / 2;
        MESHER_THREADS = cores / 2;
        if (!initServer()) return 1;
        if (!initRenderer()) return 1;
        bool game_ecode = doGame(NULL, -1);
        quitRenderer();
        //freeAllResources();
        freeFile(config_filedata);
        return !game_ecode;
    }
    return 0;
}
