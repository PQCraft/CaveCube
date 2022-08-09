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
    uint64_t uid;
    int uind;
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

static void addMsg(struct msgdata* mdata, int id, void* data, uint64_t uid, int uind) {
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
    mdata->msg[index].uid = uid;
    mdata->msg[index].uind = uind;
    pthread_mutex_unlock(&mdata->lock);
}

static bool getNextMsg(struct msgdata* mdata, struct msgdata_msg* msg) {
    pthread_mutex_lock(&mdata->lock);
    for (int i = 0; i < mdata->size; ++i) {
        if (mdata->msg[i].id >= 0) {
            msg->id = mdata->msg[i].id;
            msg->data = mdata->msg[i].data;
            msg->uid = mdata->msg[i].uid;
            msg->uind = mdata->msg[i].uind;
            mdata->msg[i].id = -1;
            pthread_mutex_unlock(&mdata->lock);
            return true;
        }
    }
    pthread_mutex_unlock(&mdata->lock);
    return false;
}

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

static struct msgdata servmsgin;
static struct msgdata servmsgout;

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
    (void)args;
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
    initMsgData(&servmsgin);
    initMsgData(&servmsgout);
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
    deinitMsgData(&servmsgin);
    deinitMsgData(&servmsgout);
}

#ifndef SERVER

#include <stdarg.h>

//static int client_delay;

struct netcxn* clicxn;

static void (*callback)(int, void*);

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
                    return 19 + 4096 * sizeof(struct blockdata);
                }
                case SERVER_UPDATECHUNKCOL:; {
                    return 18 + 16 * 4096 * sizeof(struct blockdata);
                }
            }
        }
    }
    return 0;
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
                                    callback(SERVER_PONG, NULL);
                                    break;
                                }
                                case SERVER_COMPATINFO:; {
                                    struct server_data_compatinfo data;
                                    memcpy(&data.ver_major, &buf[ptr], 2);
                                    ptr += 2;
                                    data.ver_major = net2host16(data.ver_major);
                                    memcpy(&data.ver_minor, &buf[ptr], 2);
                                    ptr += 2;
                                    data.ver_major = net2host16(data.ver_minor);
                                    memcpy(&data.ver_patch, &buf[ptr], 2);
                                    ptr += 2;
                                    data.ver_major = net2host16(data.ver_patch);
                                    data.flags = buf[ptr++];
                                    uint16_t tmpword = 0;
                                    memcpy(&tmpword, &buf[ptr], 2);
                                    ptr += 2;
                                    tmpword = net2host16(tmpword);
                                    data.server_str = malloc((int)tmpword + 1);
                                    memcpy(&data.server_str, &buf[ptr], tmpword);
                                    data.server_str[tmpword] = 0;
                                    callback(SERVER_PONG, &data);
                                    free(data.server_str);
                                    break;
                                }
                                case SERVER_UPDATECHUNK:; {
                                    struct server_data_updatechunk data;
                                    memcpy(&data.id, &buf[ptr], 2);
                                    ptr += 2;
                                    data.id = net2host16(data.id);
                                    memcpy(&data.x, &buf[ptr], 8);
                                    ptr += 8;
                                    data.x = net2host64(data.x);
                                    data.y = buf[ptr++];
                                    memcpy(&data.z, &buf[ptr], 8);
                                    ptr += 8;
                                    data.z = net2host64(data.z);
                                    memcpy(data.data, &buf[ptr], 4096 * sizeof(struct blockdata));
                                    callback(SERVER_UPDATECHUNK, &data);
                                    break;
                                }
                                case SERVER_UPDATECHUNKCOL:; {
                                    struct server_data_updatechunkcol data;
                                    memcpy(&data.id, &buf[ptr], 2);
                                    ptr += 2;
                                    data.id = net2host16(data.id);
                                    memcpy(&data.x, &buf[ptr], 8);
                                    ptr += 8;
                                    data.x = net2host64(data.x);
                                    memcpy(&data.z, &buf[ptr], 8);
                                    ptr += 8;
                                    data.z = net2host64(data.z);
                                    memcpy(data.data, &buf[ptr], 16 * 4096 * sizeof(struct blockdata));
                                    callback(SERVER_UPDATECHUNK, &data);
                                    break;
                                }
                            }
                            tmpbyte = MSGTYPE_ACK;
                            writeToCxnBuf(clicxn, &tmpbyte, 1);
                            break;
                        }
                    }
                    free(buf);
                    tmpsize = 0;
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
                    case CLIENT_GETCHUNK:; {
                        struct client_data_getchunk* tmpdata = msg.data;
                        uint16_t tmpword = host2net16(tmpdata->id);
                        writeToCxnBuf(clicxn, &tmpword, 2);
                        uint64_t tmpqword = host2net64(tmpdata->x);
                        writeToCxnBuf(clicxn, &tmpqword, 8);
                        writeToCxnBuf(clicxn, &tmpdata->y, 1);
                        tmpqword = host2net64(tmpdata->z);
                        writeToCxnBuf(clicxn, &tmpqword, 8);
                        break;
                    }
                    case CLIENT_GETCHUNKCOL:; {
                        struct client_data_getchunkcol* tmpdata = msg.data;
                        uint16_t tmpword = host2net16(tmpdata->id);
                        writeToCxnBuf(clicxn, &tmpword, 2);
                        uint64_t tmpqword = host2net64(tmpdata->x);
                        writeToCxnBuf(clicxn, &tmpqword, 8);
                        tmpqword = host2net64(tmpdata->z);
                        writeToCxnBuf(clicxn, &tmpqword, 8);
                        break;
                    }
                    case CLIENT_SETCHUNKPOS:; {
                        struct client_data_setchunkpos* tmpdata = msg.data;
                        uint64_t tmpqword = host2net64(tmpdata->x);
                        writeToCxnBuf(clicxn, &tmpqword, 8);
                        tmpqword = host2net64(tmpdata->z);
                        writeToCxnBuf(clicxn, &tmpqword, 8);
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

bool cliConnect(char* addr, int port, void (*cb)(int, void*)) {
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
    va_list args;
    va_start(args, id);
    void* data = NULL;
    switch (id) {
        case CLIENT_COMPATINFO:; {
            struct client_data_compatinfo* tmpdata = malloc(sizeof(*tmpdata));
            tmpdata->ver_major = va_arg(args, int);
            tmpdata->ver_minor = va_arg(args, int);
            tmpdata->ver_patch = va_arg(args, int);
            tmpdata->client_str = va_arg(args, char*);
            data = tmpdata;
            break;
        }
        case CLIENT_GETCHUNK:; {
            struct client_data_getchunk* tmpdata = malloc(sizeof(*tmpdata));
            tmpdata->id = va_arg(args, int);
            tmpdata->x = va_arg(args, int64_t);
            tmpdata->y = va_arg(args, int);
            tmpdata->z = va_arg(args, int64_t);
            data = tmpdata;
            break;
        }
        case CLIENT_GETCHUNKCOL:; {
            struct client_data_getchunkcol* tmpdata = malloc(sizeof(*tmpdata));
            tmpdata->id = va_arg(args, int);
            tmpdata->x = va_arg(args, int64_t);
            tmpdata->z = va_arg(args, int64_t);
            data = tmpdata;
            break;
        }
        case CLIENT_SETCHUNKPOS:; {
            struct client_data_setchunkpos* tmpdata = malloc(sizeof(*tmpdata));
            tmpdata->x = va_arg(args, int64_t);
            tmpdata->z = va_arg(args, int64_t);
            break;
        }
    }
    addMsg(&climsgout, id, data, 0, 0);
    va_end(args);
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
