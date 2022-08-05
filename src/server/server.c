#include <main/main.h>
#include "server.h"
#include "network.h"
#include <common/common.h>
#include <common/noise.h>
#include <game/game.h>
#include <game/worldgen.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

int SERVER_THREADS;

static int maxclients = MAX_CLIENTS;

static bool serveralive = false;

static int unamemax;
static int server_delay;
static int server_idledelay;

enum {
    PERM_ADMIN = 1 << 0,
    PERM_FLY = 1 << 1,
};

enum {
    STATUS_CROUCHING = 1 << 0,
    STATUS_RUNNING = 1 << 1,
    STATUS_FLYING = 1 << 2,
};

struct player {
    bool login;
    uint64_t uid;
    char* username;
    uint32_t perms;
    uint32_t status;
    uint8_t mode;
    int64_t chunkx;
    int64_t chunkz;
    double worldx;
    double worldy;
    double worldz;
};

struct playerdata {
    bool valid;
    bool ack;
    struct netcxn* cxn;
    struct player player;
};

static struct playerdata* pdata;
static pthread_mutex_t pdatalock;

bool initServer() {
    if (!initNet()) return false;
    pthread_mutex_init(&pdatalock, NULL);
    return true;
}

struct netcxn* servcxn;

static pthread_t servnetthreadh;

static void* servnetthread(void* args) {
    while (serveralive) {
        microwait(1000);
        struct netcxn* newcxn;
        if ((newcxn = acceptCxn(servcxn, CLIENT_OUTBUF_SIZE, SERVER_OUTBUF_SIZE))) {
            printf("New connection from %s\n", getCxnAddrStr(newcxn));
            bool added = false;
            pthread_mutex_lock(&pdatalock);
            for (int i = 0; i < maxclients; ++i) {
                if (!pdata[i].valid) {
                    pdata[i].valid = true;
                    pdata[i].cxn = newcxn;
                    added = true;
                    break;
                }
            }
            if (!added) {
                printf("Connection to %s dropped due to client limit\n", getCxnAddrStr(newcxn));
                closeCxn(newcxn);
            }
            pthread_mutex_unlock(&pdatalock);
        }
        for (int i = 0; i < maxclients; ++i) {
            pthread_mutex_lock(&pdatalock);
            if (pdata[i].valid) {
                if (recvCxn(pdata[i].cxn) < 0) {
                    printf("Connection to %s dropped due to disconnect\n", getCxnAddrStr(pdata[i].cxn));
                    closeCxn(pdata[i].cxn);
                    pdata[i].valid = false;
                } else {
                    // read buffer
                }
            }
            pthread_mutex_unlock(&pdatalock);
        }
    }
    return NULL;
}

int startServer(char* addr, int port, char* world, int mcli) {
    (void)world;
    setRandSeed(1, altutime() + (uintptr_t)"");
    if (port < 0 || port > 0xFFFF) port = 46000 + (getRandWord(1) % 1000);
    if (mcli > 0) maxclients = mcli;
    printf("Starting server on %s:%d with a max of %d %s...\n", (addr) ? addr : "0.0.0.0", port, maxclients, (maxclients == 1) ? "player" : "players");
    if (!(servcxn = newCxn(CXN_MULTI, addr, port, -1, -1))) {
        fputs("servStart: Failed to create connection\n", stderr);
        return -1;
    }
    setCxnBufSize(servcxn, SERVER_SNDBUF_SIZE, CLIENT_SNDBUF_SIZE);
    pdata = calloc(maxclients, sizeof(*pdata));
    setRandSeed(0, 32464);
    initNoiseTable(0);
    serveralive = true;
    pthread_create(&servnetthreadh, NULL, &servnetthread, (void*)NULL);
    return port;
}

void stopServer() {
    puts("Stopping server...");
    serveralive = false;
    pthread_join(servnetthreadh, NULL);
    for (int i = 0; i < maxclients; ++i) {
        if (pdata[i].valid) {
            closeCxn(pdata[i].cxn);
        }
    }
    closeCxn(servcxn);
    free(pdata);
}

#ifndef SERVER

static int client_delay;

bool cliConnect(char* addr, int port) {
    
}

#endif
