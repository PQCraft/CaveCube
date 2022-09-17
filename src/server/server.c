#include <main/main.h>
#include "server.h"
#include "network.h"
#include <main/version.h>
#include <common/common.h>
#include <common/endian.h>
#include <common/noise.h>
#include <game/game.h>
#include <game/worldgen.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

int SERVER_THREADS;

struct msgdata_msg {
    int id;
    void* data;
    uint64_t uuid;
    int uind;
};

struct msgdata {
    bool valid;
    int size;
    struct msgdata_msg* msg;
    pthread_mutex_t lock;
};

static void initMsgData(struct msgdata* mdata) {
    mdata->valid = true;
    mdata->size = 0;
    mdata->msg = malloc(0);
    pthread_mutex_init(&mdata->lock, NULL);
}

static void deinitMsgData(struct msgdata* mdata) {
    pthread_mutex_lock(&mdata->lock);
    mdata->valid = false;
    free(mdata->msg);
    pthread_mutex_unlock(&mdata->lock);
    pthread_mutex_destroy(&mdata->lock);
}

static void addMsg(struct msgdata* mdata, int id, void* data, uint64_t uuid, int uind) {
    pthread_mutex_lock(&mdata->lock);
    if (mdata->valid) {
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
        mdata->msg[index].uuid = uuid;
        mdata->msg[index].uind = uind;
    }
    pthread_mutex_unlock(&mdata->lock);
}

static bool getNextMsg(struct msgdata* mdata, struct msgdata_msg* msg) {
    pthread_mutex_lock(&mdata->lock);
    if (mdata->valid) {
        for (int i = 0; i < mdata->size; ++i) {
            if (mdata->msg[i].id >= 0) {
                msg->id = mdata->msg[i].id;
                msg->data = mdata->msg[i].data;
                msg->uuid = mdata->msg[i].uuid;
                msg->uind = mdata->msg[i].uind;
                mdata->msg[i].id = -1;
                pthread_mutex_unlock(&mdata->lock);
                return true;
            }
        }
    }
    pthread_mutex_unlock(&mdata->lock);
    return false;
}

static bool getNextMsgForUUID(struct msgdata* mdata, struct msgdata_msg* msg, uint64_t uuid) {
    pthread_mutex_lock(&mdata->lock);
    if (mdata->valid) {
        for (int i = 0; i < mdata->size; ++i) {
            if (mdata->msg[i].id >= 0 && mdata->msg[i].uuid == uuid) {
                msg->id = mdata->msg[i].id;
                msg->data = mdata->msg[i].data;
                msg->uuid = mdata->msg[i].uuid;
                msg->uind = mdata->msg[i].uind;
                mdata->msg[i].id = -1;
                pthread_mutex_unlock(&mdata->lock);
                return true;
            }
        }
    }
    pthread_mutex_unlock(&mdata->lock);
    return false;
}

enum {
    //MSGTYPE_ACK,
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
    uint64_t uuid;
    //bool ack;
    int tmpsize;
    struct netcxn* cxn;
    struct player player;
};

static struct playerdata* pdata;
static pthread_mutex_t pdatalock;

static struct msgdata servmsgin;
static struct msgdata servmsgout;

static int worldtype = 1;

static pthread_t servpthreads[MAX_THREADS];

static void* servthread(void* args) {
    int id = (intptr_t)args;
    printf("Server: Started thread [%d]\n", id);
    uint64_t acttime = altutime();
    while (serveralive) {
        bool activity = false;
        struct msgdata_msg msg;
        if (getNextMsg(&servmsgin, &msg)) {
            activity = true;
            //printf("Received message [%d] for player handle [%d]\n", msg.id, msg.uind);
            pthread_mutex_lock(&pdatalock);
            bool cond = (pdata[msg.uind].valid && pdata[msg.uind].uuid == msg.uuid);
            pthread_mutex_unlock(&pdatalock);
            if (cond) {
                //puts("Processing message...");
                switch (msg.id) {
                    case CLIENT_PING:; {
                        addMsg(&servmsgout, SERVER_PONG, NULL, msg.uuid, msg.uind);
                        break;
                    }
                    case CLIENT_COMPATINFO:; {
                        //struct client_data_compatinfo* data = msg.data;
                        struct server_data_compatinfo* outdata = malloc(sizeof(*outdata));
                        outdata->ver_major = VER_MAJOR;
                        outdata->ver_minor = VER_MINOR;
                        outdata->ver_patch = VER_PATCH;
                        outdata->flags = SERVER_FLAG_NOAUTH;
                        outdata->server_str = strdup(PROG_NAME);
                        addMsg(&servmsgout, SERVER_COMPATINFO, outdata, msg.uuid, msg.uind);
                        break;
                    }
                    case CLIENT_GETCHUNK:; {
                        struct client_data_getchunk* data = msg.data;
                        struct server_data_updatechunk* outdata = malloc(sizeof(*outdata));
                        outdata->x = data->x;
                        outdata->z = data->z;
                        genChunk(data->x, data->z, outdata->data, worldtype);
                        addMsg(&servmsgout, SERVER_UPDATECHUNK, outdata, msg.uuid, msg.uind);
                        break;
                    }
                }
                if (msg.data) free(msg.data);
                //puts("Done");
            } else {
                //puts("Message dropped (player handle is not valid)");
            }
        }
        if (activity) {
            acttime = altutime();
        } else if (altutime() - acttime > 1000000) {
            microwait(500000);
        }
    }
    return NULL;
}

static struct netcxn* servcxn;

#define pCML_nbyte() ({int byte; if (tmpsize > 0) {byte = buf->data[ptr]; ptr = (ptr + 1) % buf->size; --tmpsize;} else {byte = -1;}; byte;})
static int peekCliMsgLen(struct netcxn* cxn) {
    struct netbuf* buf = cxn->inbuf;
    if (buf->dlen < 1) return 0;
    int ptr = buf->rptr;
    int tmpsize = buf->dlen;
    int tmp = pCML_nbyte();
    switch (tmp) {
        /*
        case MSGTYPE_ACK:; {
            return 1;
        }
        */
        case MSGTYPE_DATA:; {
            tmp = pCML_nbyte();
            if (tmp < 0) return 0;
            switch (tmp) {
                case CLIENT_COMPATINFO:; {
                    for (int i = 0; i < 6; ++i) {
                        pCML_nbyte();
                    }
                    tmp = pCML_nbyte();
                    if (tmp < 0) return 0;
                    uint16_t tmp2 = (tmp << 8) & 0xFF00;
                    tmp = pCML_nbyte();
                    if (tmp < 0) return 0;
                    tmp2 |= tmp & 0xFF;
                    //tmp2 = net2host16(tmp2);
                    return 2 + 8 + tmp2;
                }
                case CLIENT_GETCHUNK:; {
                    return 2 + 16;
                }
                default:; /*has CLIENT_PING*/ {
                    return 2;
                }
            }
        }
    }
    return 0;
}

static pthread_t servnetthreadh;

static void* servnetthread(void* args) {
    (void)args;
    uint64_t acttime = altutime();
    while (serveralive) {
        bool activity = false;
        struct netcxn* newcxn;
        if ((newcxn = acceptCxn(servcxn, SERVER_OUTBUF_SIZE, CLIENT_OUTBUF_SIZE))) {
            activity = true;
            printf("New connection from %s\n", getCxnAddrStr(newcxn));
            bool added = false;
            pthread_mutex_lock(&pdatalock);
            for (int i = 0; i < maxclients; ++i) {
                if (!pdata[i].valid) {
                    pdata[i].valid = true;
                    static uint64_t uuid = 0;
                    pdata[i].uuid = uuid++;
                    //pdata[i].ack = true;
                    pdata[i].tmpsize = 0;
                    pdata[i].cxn = newcxn;
                    memset(&pdata[i].player, 0, sizeof(pdata[i].player));
                    added = true;
                    struct server_data_setskycolor* tmpdata = malloc(sizeof(*tmpdata));
                    *tmpdata = (struct server_data_setskycolor){0x8A, 0xC3, 0xFF};
                    addMsg(&servmsgout, SERVER_SETSKYCOLOR, tmpdata, pdata[i].uuid, i);
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
                    activity = true;
                    printf("Connection to %s dropped due to disconnect\n", getCxnAddrStr(pdata[i].cxn));
                    closeCxn(pdata[i].cxn);
                    pdata[i].valid = false;
                } else {
                    if (pdata[i].tmpsize < 1) {
                        pdata[i].tmpsize = peekCliMsgLen(pdata[i].cxn);
                        if (pdata[i].tmpsize > 0) activity = true;
                    } else if (getInbufSize(pdata[i].cxn) >= pdata[i].tmpsize) {
                        activity = true;
                        unsigned char* buf = malloc(pdata[i].tmpsize);
                        readFromCxnBuf(pdata[i].cxn, buf, pdata[i].tmpsize);
                        int ptr = 0;
                        uint8_t tmpbyte = buf[ptr++];
                        switch (tmpbyte) {
                            /*
                            case MSGTYPE_ACK:; {
                                pdata[i].ack = true;
                                break;
                            }
                            */
                            case MSGTYPE_DATA:; {
                                void* _data = NULL;
                                uint8_t msgdataid = buf[ptr++];
                                switch (msgdataid) {
                                    case CLIENT_COMPATINFO:; {
                                        struct server_data_compatinfo* data = malloc(sizeof(*data));
                                        memcpy(&data->ver_major, &buf[ptr], 2);
                                        ptr += 2;
                                        data->ver_major = net2host16(data->ver_major);
                                        memcpy(&data->ver_minor, &buf[ptr], 2);
                                        ptr += 2;
                                        data->ver_minor = net2host16(data->ver_minor);
                                        memcpy(&data->ver_patch, &buf[ptr], 2);
                                        ptr += 2;
                                        data->ver_patch = net2host16(data->ver_patch);
                                        uint16_t tmpword = 0;
                                        memcpy(&tmpword, &buf[ptr], 2);
                                        ptr += 2;
                                        tmpword = net2host16(tmpword);
                                        data->server_str = malloc((int)tmpword + 1);
                                        memcpy(data->server_str, &buf[ptr], tmpword);
                                        data->server_str[tmpword] = 0;
                                        _data = data;
                                        break;
                                    }
                                    case CLIENT_GETCHUNK:; {
                                        struct server_data_updatechunk* data = malloc(sizeof(*data));
                                        memcpy(&data->x, &buf[ptr], 8);
                                        ptr += 8;
                                        data->x = net2host64(data->x);
                                        memcpy(&data->z, &buf[ptr], 8);
                                        data->z = net2host64(data->z);
                                        _data = data;
                                        break;
                                    }
                                }
                                addMsg(&servmsgin, msgdataid, _data, pdata[i].uuid, i);
                                //tmpbyte = MSGTYPE_ACK;
                                //writeToCxnBuf(pdata[i].cxn, &tmpbyte, 1);
                                break;
                            }
                        }
                        free(buf);
                        pdata[i].tmpsize = 0;
                    }
                    /*if (pdata[i].ack || true)*/ {
                        struct msgdata_msg msg;
                        if (getNextMsgForUUID(&servmsgout, &msg, pdata[i].uuid) && msg.uind == i) {
                            activity = true;
                            uint8_t tmpbyte[2] = {MSGTYPE_DATA, msg.id};
                            writeToCxnBuf(pdata[i].cxn, tmpbyte, 2);
                            switch (msg.id) {
                                case SERVER_COMPATINFO:; {
                                    struct server_data_compatinfo* tmpdata = msg.data;
                                    uint16_t tmpword = host2net16(tmpdata->ver_major);
                                    writeToCxnBuf(pdata[i].cxn, &tmpword, 2);
                                    tmpword = host2net16(tmpdata->ver_minor);
                                    writeToCxnBuf(pdata[i].cxn, &tmpword, 2);
                                    tmpword = host2net16(tmpdata->ver_patch);
                                    writeToCxnBuf(pdata[i].cxn, &tmpword, 2);
                                    writeToCxnBuf(pdata[i].cxn, &tmpdata->flags, 1);
                                    tmpword = host2net16(strlen(tmpdata->server_str));
                                    writeToCxnBuf(pdata[i].cxn, &tmpword, 2);
                                    writeToCxnBuf(pdata[i].cxn, tmpdata->server_str, net2host16(tmpword));
                                    free(tmpdata->server_str);
                                    break;
                                }
                                case SERVER_UPDATECHUNK:; {
                                    struct server_data_updatechunk* tmpdata = msg.data;
                                    uint64_t tmpqword = host2net64(tmpdata->x);
                                    writeToCxnBuf(pdata[i].cxn, &tmpqword, 8);
                                    tmpqword = host2net64(tmpdata->z);
                                    writeToCxnBuf(pdata[i].cxn, &tmpqword, 8);
                                    writeToCxnBuf(pdata[i].cxn, &tmpdata->data, 65536 * sizeof(struct blockdata));
                                    break;
                                }
                                case SERVER_SETSKYCOLOR:; {
                                    struct server_data_setskycolor* tmpdata = msg.data;
                                    writeToCxnBuf(pdata[i].cxn, &tmpdata->r, 1);
                                    writeToCxnBuf(pdata[i].cxn, &tmpdata->g, 1);
                                    writeToCxnBuf(pdata[i].cxn, &tmpdata->b, 1);
                                }
                            }
                            if (msg.data) free(msg.data);
                            //pdata[i].ack = false;
                        }
                    }
                    sendCxn(pdata[i].cxn);
                }
            }
            pthread_mutex_unlock(&pdatalock);
        }
        if (activity) {
            acttime = altutime();
        } else if (altutime() - acttime > 1000000) {
            microwait(500000);
        }
    }
    return NULL;
}

int startServer(char* addr, int port, int mcli, char* world) {
    (void)world;
    setRandSeed(1, altutime() + (uintptr_t)"");
    if (port < 0 || port > 0xFFFF) port = 46000 + (getRandWord(1) % 1000);
    if (mcli > 0) maxclients = mcli;
    printf("Starting server on %s:%d with a max of %d player%s...\n", (addr) ? addr : "0.0.0.0", port, maxclients, (maxclients == 1) ? "" : "s");
    if (!(servcxn = newCxn(CXN_PASSIVE, addr, port, -1, -1))) {
        fputs("servStart: Failed to create connection\n", stderr);
        return -1;
    }
    port = servcxn->info.port;
    printf("Started server on %s\n", getCxnAddrStr(servcxn));
    setCxnBufSize(servcxn, SERVER_SNDBUF_SIZE, CLIENT_SNDBUF_SIZE);
    pdata = calloc(maxclients, sizeof(*pdata));
    initMsgData(&servmsgin);
    initMsgData(&servmsgout);
    setRandSeed(0, 32464);
    initNoiseTable(0);
    initWorldgen();
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
    for (int i = 0; i < SERVER_THREADS && i < MAX_THREADS; ++i) {
        pthread_join(servpthreads[i], NULL);
    }
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

static struct netcxn* clicxn;

static void (*callback)(int, void*);

static struct msgdata climsgout;

#define pSML_nbyte() ({int byte; if (tmpsize > 0) {byte = buf->data[ptr]; ptr = (ptr + 1) % buf->size; --tmpsize;} else {byte = -1;}; byte;})
static int peekServMsgLen(struct netcxn* cxn) {
    struct netbuf* buf = cxn->inbuf;
    if (buf->dlen < 1) return 0;
    int ptr = buf->rptr;
    int tmpsize = buf->dlen;
    int tmp = pSML_nbyte();
    switch (tmp) {
        /*
        case MSGTYPE_ACK:; {
            return 1;
        }
        */
        case MSGTYPE_DATA:; {
            tmp = pSML_nbyte();
            if (tmp < 0) return 0;
            switch (tmp) {
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
                    //tmp2 = net2host16(tmp2);
                    return 2 + 9 + tmp2;
                }
                case SERVER_UPDATECHUNK:; {
                    return 2 + 16 + 65536 * sizeof(struct blockdata);
                }
                case SERVER_SETSKYCOLOR:; {
                    return 2 + 3;
                }
                default:; /*has SERVER_PONG*/ {
                    return 2;
                }
            }
        }
    }
    return 0;
}

static pthread_t clinetthreadh;

static void* clinetthread(void* args) {
    (void)args;
    //bool ack = true;
    int tmpsize = 0;
    uint64_t acttime = altutime();
    while (true) {
        bool activity = false;
        recvCxn(clicxn);
        if (tmpsize < 1) {
            tmpsize = peekServMsgLen(clicxn);
            if (tmpsize > 0) activity = true;
        } else if (getInbufSize(clicxn) >= tmpsize) {
            activity = true;
            unsigned char* buf = malloc(tmpsize);
            readFromCxnBuf(clicxn, buf, tmpsize);
            int ptr = 0;
            uint8_t tmpbyte = buf[ptr++];
            switch (tmpbyte) {
                /*
                case MSGTYPE_ACK:; {
                    ack = true;
                    break;
                }
                */
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
                            data.ver_minor = net2host16(data.ver_minor);
                            memcpy(&data.ver_patch, &buf[ptr], 2);
                            ptr += 2;
                            data.ver_patch = net2host16(data.ver_patch);
                            data.flags = buf[ptr++];
                            uint16_t tmpword = 0;
                            memcpy(&tmpword, &buf[ptr], 2);
                            ptr += 2;
                            tmpword = net2host16(tmpword);
                            data.server_str = malloc((int)tmpword + 1);
                            memcpy(data.server_str, &buf[ptr], tmpword);
                            data.server_str[tmpword] = 0;
                            callback(SERVER_COMPATINFO, &data);
                            free(data.server_str);
                            break;
                        }
                        case SERVER_UPDATECHUNK:; {
                            struct server_data_updatechunk data;
                            memcpy(&data.x, &buf[ptr], 8);
                            ptr += 8;
                            data.x = net2host64(data.x);
                            memcpy(&data.z, &buf[ptr], 8);
                            ptr += 8;
                            data.z = net2host64(data.z);
                            memcpy(data.data, &buf[ptr], 65536 * sizeof(struct blockdata));
                            callback(SERVER_UPDATECHUNK, &data);
                            break;
                        }
                        case SERVER_SETSKYCOLOR:; {
                            struct server_data_setskycolor data;
                            data.r = buf[ptr++];
                            data.g = buf[ptr++];
                            data.b = buf[ptr++];
                            callback(SERVER_SETSKYCOLOR, &data);
                            break;
                        }
                    }
                    //tmpbyte = MSGTYPE_ACK;
                    //writeToCxnBuf(clicxn, &tmpbyte, 1);
                    break;
                }
            }
            free(buf);
            tmpsize = 0;
        }
        /*if (ack || true)*/ {
            struct msgdata_msg msg;
            if (getNextMsg(&climsgout, &msg)) {
                activity = true;
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
                        writeToCxnBuf(clicxn, &tmpword, 2);
                        writeToCxnBuf(clicxn, tmpdata->client_str, net2host16(tmpword));
                        free(tmpdata->client_str);
                        break;
                    }
                    case CLIENT_GETCHUNK:; {
                        struct client_data_getchunk* tmpdata = msg.data;
                        uint64_t tmpqword = host2net64(tmpdata->x);
                        writeToCxnBuf(clicxn, &tmpqword, 8);
                        tmpqword = host2net64(tmpdata->z);
                        writeToCxnBuf(clicxn, &tmpqword, 8);
                        break;
                    }
                }
                free(msg.data);
                //ack = false;
            }
        }
        sendCxn(clicxn);
        if (activity) {
            acttime = altutime();
        } else if (altutime() - acttime > 1000000) {
            microwait(500000);
        }
    }
    return NULL;
}

bool cliConnect(char* addr, int port, void (*cb)(int, void*)) {
    if (!(clicxn = newCxn(CXN_ACTIVE, addr, port, CLIENT_OUTBUF_SIZE, SERVER_OUTBUF_SIZE))) {
        fputs("cliConnect: Failed to create connection\n", stderr);
        return false;
    }
    initMsgData(&climsgout);
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
            tmpdata->client_str = strdup(va_arg(args, char*));
            data = tmpdata;
            break;
        }
        case CLIENT_GETCHUNK:; {
            struct client_data_getchunk* tmpdata = malloc(sizeof(*tmpdata));
            tmpdata->x = va_arg(args, int64_t);
            tmpdata->z = va_arg(args, int64_t);
            data = tmpdata;
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
    return true;
}
