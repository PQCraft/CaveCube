#if defined(MODULE_TOOLBOX)

#include <main/main.h>
#include "toolbox.h"
#include "version.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <common/glue.h>

#define GETARG() ({char* a = NULL; if (argc > 0) {--argc; a = *(argv++);} a;})

static void help() {
    printf("%s {COMMAND [OPTION]...}... [ARGUMENT]...\n", g_argv[0]);
}

static int serverlist(int argc, char** argv) {
    (void)argc;
    (void)argv;
    return 0;
}

int toolbox_main(int argc, char** argv) {
    if (argc <= 0) {
        help();
        return 0;
    }
    char* arg;
    while ((arg = GETARG())) {
        if (!strcmp(arg, "--help")) {
            help();
            return 0;
        } else if (!strcmp(arg, "--version")) {
            puts(PROG_NAME" "VER_STR);
            return 0;
        }
    }
    return 0;
}

#endif
