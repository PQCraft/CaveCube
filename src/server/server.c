#include <main/main.h>
#include "server.h"
#include "network.h"
#include <common/common.h>
#include <common/endian.h>
#include <common/noise.h>
#include <game/game.h>
#include <game/worldgen.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifndef SERVER_STRING
    #define SERVER_STRING "CaveCube"
#endif

int SERVER_THREADS;

struct msgdata_msg {
    int id;
    void* data;
};

struct msgdata {
    int size;
    struct msgdata_msg* msg;
    pthread_mutex_t lock;
};

static void initMsgData(struct msgdata* mdata) {
    mdata->size = 0;
    mdata->msg = malloc(0);
    pthread_mutex_init(&mdata->lock, NULL);
}

static void deinitMsgData(struct msgdata* mdata) {
    free(mdata->msg);
    pthread_mutex_destroy(&mdata->lock);
}

static void addMsg(struct msgdata* mdata, int id, void* data) {
    pthread_mutex_lock(&mdata->lock);
    int index = -1;
    for (int i = 0; i < mdata->size; ++i) {
        if (mdata->msg[i].id < 0) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        index = mdata->size++;
        mdata->msg = realloc(mdata->msg, mdata->size * sizeof(*mdata->msg));
    }
    mdata->msg[index].id = id;
    mdata->msg[index].data = data;
    pthread_mutex_unlock(&mdata->lock);
}

static bool getNextMsg(struct msgdata* mdata, struct msgdata_msg* msg) {
    pthread_mutex_lock(&mdata->lock);
    for (int i = 0; i < mdata->size; ++i) {
        if (mdata->msg[i].id >= 0) {
            msg->id = mdata->msg[i].id;
            msg->data = mdata->msg[i].data;
            mdata->msg[i].id = -1;
            pthread_mutex_unlock(&mdata->lock);
            return true;
        }
    }
    pthread_mutex_unlock(&mdata->lock);
    return false;
}

struct server_data_compatinfo {
    uint16_t ver_major;
    uint16_t ver_minor;
    uint16_t ver_patch;
    uint8_t flags;
    char* server_str;
};

struct server_data_logininfo {
    uint8_t failed;
    char* reason;
    uint64_t uid;
    uint64_t password;
};

struct server_data_updatechunk {
    uint16_t id;
    int64_t x;
    int8_t y;
    int64_t z;
    struct blockdata data[4096];
};

struct server_data_updatechunkcol {
    uint16_t id;
    int64_t x;
    int64_t z;
    struct blockdata data[16][4096];
};

struct client_data_compatinfo {
    uint16_t ver_major;
    uint16_t ver_minor;
    uint16_t ver_patch;
    char* client_str;
};

struct client_data_logininfo {
    uint64_t uid;
    uint64_t password;
    char* username;
};

struct client_data_getchunk {
    struct chunkinfo info;
    uint16_t id;
    int64_t x;
    int8_t y;
    int64_t z;
};

struct client_data_getchunkcol {
    uint16_t id;
    int64_t x;
    int64_t z;
};

struct client_data_setchunkpos {
    int64_t x;
    int64_t z;
};

enum {
    MSGTYPE_ACK,
    MSGTYPE_DATA,
};

static int getInbufSize(struct netcxn* cxn) {
    return cxn->inbuf->dlen;
}

static int maxclients = MAX_CLIENTS;

static bool serveralive = false;

//static int unamemax;
//static int server_delay;
//static int server_idledelay;

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

static pthread_t servpthreads[MAX_THREADS];

static void* servthread(void* args) {
    int id = (intptr_t)args;
    printf("Server: Started thread [%d]\n", id);
    while (serveralive) {
        microwait(1000);
    }
    return NULL;
}

struct netcxn* servcxn;

static pthread_t servnetthreadh;

static void* servnetthread(void* args) {
    while (serveralive) {
        microwait(1000);
        struct netcxn* newcxn;
        if ((newcxn = acceptCxn(servcxn, SERVER_OUTBUF_SIZE, CLIENT_OUTBUF_SIZE))) {
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
                    sendCxn(pdata[i].cxn);
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
    if (!(servcxn = newCxn(CXN_PASSIVE, addr, port, -1, -1))) {
        fputs("servStart: Failed to create connection\n", stderr);
        return -1;
    }
    setCxnBufSize(servcxn, SERVER_SNDBUF_SIZE, CLIENT_SNDBUF_SIZE);
    pdata = calloc(maxclients, sizeof(*pdata));
    setRandSeed(0, 32464);
    initNoiseTable(0);
    serveralive = true;
    #ifdef NAME_THREADS
    char name[256];
    char name2[256];
    name[0] = 0;
    name2[0] = 0;
    #endif
    pthread_create(&servnetthreadh, NULL, &servnetthread, NULL);
    #ifdef NAME_THREADS
    pthread_getname_np(servnetthreadh, name2, 256);
    sprintf(name, "%s:st", name2);
    pthread_setname_np(servnetthreadh, name);
    #endif
    for (int i = 0; i < SERVER_THREADS && i < MAX_THREADS; ++i) {
        #ifdef NAME_THREADS
        name[0] = 0;
        name2[0] = 0;
        #endif
        pthread_create(&servpthreads[i], NULL, &servthread, (void*)(intptr_t)i);
        #ifdef NAME_THREADS
        pthread_getname_np(servpthreads[i], name2, 256);
        sprintf(name, "%s:srv%d", name2, i);
        pthread_setname_np(servpthreads[i], name);
        #endif
    }
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

#include <stdarg.h>

#ifndef CLIENT_STRING
    #define CLIENT_STRING "CaveCube"
#endif

//static int client_delay;

struct netcxn* clicxn;

static void (*callback)(int, ...);

static struct msgdata climsgout;

#define pSML_nbyte() ({int byte; if (ptr < buf->rptr) {byte = buf->data[ptr]; ptr = (ptr + 1) % buf->size;} else {byte = -1;}; byte;})
static int peekServMsgLen(struct netcxn* cxn) {
    struct netbuf* buf = cxn->inbuf;
    if (buf->dlen < 1) return 0;
    int ptr = (buf->rptr + (buf->size - buf->dlen)) % buf->size;
    int tmp = pSML_nbyte();
    switch (tmp) {
        case MSGTYPE_ACK:; {
            return 1;
        }
        case MSGTYPE_DATA:; {
            tmp = pSML_nbyte();
            if (tmp < 0) return 0;
            switch (tmp) {
                case SERVER_PONG:; {
                    return 2;
                }
                case SERVER_COMPATINFO:; {
                    for (int i = 0; i < 7; ++i) {
                        pSML_nbyte();
                    }
                    tmp = pSML_nbyte();
                    if (tmp < 0) return 0;
                    uint16_t tmp2 = (tmp << 8) & 0xFF00;
                    tmp = pSML_nbyte();
                    if (tmp < 0) return 0;
                    tmp2 |= tmp & 0xFF;
                    tmp2 = net2host16(tmp2);
                    return 7 + tmp2;
                }
                case SERVER_UPDATECHUNK:; {
                    return 8211;
                }
                case SERVER_UPDATECHUNKCOL:; {
                    return 131090;
                }
            }
        }
    }
}

static pthread_t clinetthreadh;

static void* clinetthread(void* args) {
    (void)args;
    bool ack = true;
    int tmpsize = 0;
    while (true) {
        microwait(1000);
        recvCxn(clicxn);
        {
            if (tmpsize < 1) {
                tmpsize = peekServMsgLen(clicxn);
            } else {
                if (getInbufSize(clicxn) >= tmpsize) {
                    unsigned char* buf = malloc(tmpsize);
                    readFromCxnBuf(clicxn, buf, tmpsize);
                    int ptr = 0;
                    uint8_t tmpbyte = buf[ptr++];
                    switch (tmpbyte) {
                        case MSGTYPE_ACK:; {
                            ack = true;
                            break;
                        }
                        case MSGTYPE_DATA:; {
                            tmpbyte = buf[ptr++];
                            switch (tmpbyte) {
                                case SERVER_PONG:; {
                                    callback(SERVER_PONG);
                                    break;
                                }
                                case SERVER_COMPATINFO:; {
                                    
                                    break;
                                }
                            }
                            tmpbyte = MSGTYPE_ACK;
                            writeToCxnBuf(clicxn, &tmpbyte, 1);
                            break;
                        }
                    }
                }
            }
        }
        if (ack) {
            struct msgdata_msg msg;
            if (getNextMsg(&climsgout, &msg)) {
                uint8_t tmpbyte[2] = {MSGTYPE_DATA, msg.id};
                writeToCxnBuf(clicxn, tmpbyte, 2);
                switch (msg.id) {
                    case CLIENT_COMPATINFO:; {
                        struct client_data_compatinfo* tmpdata = msg.data;
                        uint16_t tmpword = host2net16(tmpdata->ver_major);
                        writeToCxnBuf(clicxn, &tmpword, 2);
                        tmpword = host2net16(tmpdata->ver_minor);
                        writeToCxnBuf(clicxn, &tmpword, 2);
                        tmpword = host2net16(tmpdata->ver_patch);
                        writeToCxnBuf(clicxn, &tmpword, 2);
                        tmpword = host2net16(strlen(tmpdata->client_str));
                        writeToCxnBuf(clicxn, tmpdata->client_str, net2host16(tmpword));
                        free(tmpdata->client_str);
                        free(tmpdata);
                        break;
                    }
                }
                ack = false;
            }
        }
        sendCxn(clicxn);
    }
    return NULL;
}

bool cliConnect(char* addr, int port, void (*cb)(int, ...)) {
    if (!(clicxn = newCxn(CXN_ACTIVE, addr, port, CLIENT_OUTBUF_SIZE, SERVER_OUTBUF_SIZE))) {
        fputs("cliConnect: Failed to create connection\n", stderr);
        return false;
    }
    callback = cb;
    #ifdef NAME_THREADS
    char name[256];
    char name2[256];
    name[0] = 0;
    name2[0] = 0;
    #endif
    pthread_create(&clinetthreadh, NULL, &clinetthread, NULL);
    #ifdef NAME_THREADS
    pthread_getname_np(clinetthreadh, name2, 256);
    sprintf(name, "%s:ct", name2);
    pthread_setname_np(clinetthreadh, name);
    #endif
    return true;
}

void cliSend(int id, ...) {
    if (id <= CLIENT__MIN || id >= CLIENT__MAX) return;
    void* data = NULL;
    switch (id) {
        case CLIENT_COMPATINFO:; {
            struct client_data_compatinfo* tmpdata = malloc(sizeof(*tmpdata));
            *tmpdata = (struct client_data_compatinfo){
                .ver_major = VER_MAJOR,
                .ver_minor = VER_MINOR,
                .ver_patch = VER_PATCH,
                .client_str = strdup(CLIENT_STRING)
            };
            break;
        }
    }
    addMsg(&climsgout, id, data);
}

#endif

bool initServer() {
    if (!initNet()) return false;
    pthread_mutex_init(&pdatalock, NULL);
    #ifndef SERVER
    initMsgData(&climsgout);
    #endif
    return true;
}
