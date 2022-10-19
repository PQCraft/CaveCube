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
static bool serveralive = false;

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

static struct msgdata servmsgin;
static struct msgdata servmsgout;

struct timerdata_timer {
    int event;
    uint64_t interval;
    uint64_t intertime;
};

struct timerdata {
    bool valid;
    int size;
    struct timerdata_timer* tmr;
    pthread_mutex_t lock;
};

static void initTimerData(struct timerdata* tdata) {
    tdata->valid = true;
    tdata->size = 0;
    tdata->tmr = malloc(0);
    pthread_mutex_init(&tdata->lock, NULL);
}

static void deinitTimerData(struct timerdata* tdata) {
    pthread_mutex_lock(&tdata->lock);
    tdata->valid = false;
    free(tdata->tmr);
    pthread_mutex_unlock(&tdata->lock);
    pthread_mutex_destroy(&tdata->lock);
}

static int addTimer(struct timerdata* tdata, int event, uint64_t interval) {
    int index = -1;
    pthread_mutex_lock(&tdata->lock);
    if (tdata->valid) {
        for (int i = 0; i < tdata->size; ++i) {
            if (tdata->tmr[i].event < 0) {
                index = i;
                break;
            }
        }
        if (index == -1) {
            index = tdata->size++;
            tdata->tmr = realloc(tdata->tmr, tdata->size * sizeof(*tdata->tmr));
        }
        tdata->tmr[index].event = event;
        tdata->tmr[index].interval = interval;
        tdata->tmr[index].intertime = altutime() + interval;
    }
    pthread_mutex_unlock(&tdata->lock);
    return index;
}

static void removeTimer(struct timerdata* tdata, int i) {
    pthread_mutex_lock(&tdata->lock);
    if (tdata->valid) {
        if (tdata->tmr[i].event >= 0) {
            tdata->tmr[i].event = -1;
            pthread_mutex_unlock(&tdata->lock);
        }
    }
    pthread_mutex_unlock(&tdata->lock);
}

static pthread_t servtimerh;

static void* servtimerthread(void* vargs) {
    struct timerdata* tdata = vargs;
    while (serveralive) {
        int64_t wait = INT64_MAX;
        int event = -1;
        int index = -1;
        pthread_mutex_lock(&tdata->lock);
        uint64_t time = altutime();
        for (int i = 0; i < tdata->size; ++i) {
            // find smallest wait time, wait for that time, then fire event
            if (tdata->tmr[i].event >= 0) {
                int64_t tdiff = (uint64_t)(tdata->tmr[i].intertime - time);
                if (tdiff < wait) {
                    wait = tdiff;
                    event = tdata->tmr[i].event;
                    index = i;
                }
            }
        }
        pthread_mutex_unlock(&tdata->lock);
        if (event >= 0 && wait != INT64_MAX) {
            //printf("Wait for: [%"PRId64"]\n", wait);
            if (wait > 0) {
                microwait(wait);
            }
            //addMsg(&servmsgin, event, NULL, -1, -1);
            tdata->tmr[index].intertime = altutime() + tdata->tmr[index].interval;
            //printf("Firing event [%d]: [%d]\n", index, event);
        }
    }
    return NULL;
}

static struct timerdata servtimer;

enum {
    MSGTYPE_DATA,
};

static inline int getInbufSize(struct netcxn* cxn) {
    return cxn->inbuf->dlen;
}

static inline int getOutbufLeft(struct netcxn* cxn) {
    return cxn->outbuf->size - cxn->outbuf->dlen;
}

static int maxclients = MAX_CLIENTS;

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
    int tmpsize;
    struct netcxn* cxn;
    struct player player;
};

static struct playerdata* pdata;
static pthread_mutex_t pdatalock;

enum {
    _SERVER_INTERNAL1 = 256,
    _SERVER_INTERNAL2,
};

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
                        struct client_data_compatinfo* data = msg.data;
                        struct server_data_compatinfo* outdata = malloc(sizeof(*outdata));
                        free(data->client_str);
                        outdata->ver_major = VER_MAJOR;
                        outdata->ver_minor = VER_MINOR;
                        outdata->ver_patch = VER_PATCH;
                        outdata->flags = SERVER_FLAG_NOAUTH;
                        outdata->server_str = strdup(PROG_NAME);
                        addMsg(&servmsgout, SERVER_COMPATINFO, outdata, msg.uuid, msg.uind);
                        break;
                    }
                    case CLIENT_LOGININFO:; {
                        struct client_data_logininfo* data = msg.data;
                        struct server_data_logininfo* outdata = calloc(1, sizeof(*outdata));
                        if (false /*purposely fail if true*/) {
                            outdata->failed = true;
                            outdata->reason = strdup("Debug.");
                        } else {
                            if (!*data->username) {
                                outdata->failed = true;
                                outdata->reason = strdup("Username cannot be empty.");
                            } else if (strlen(data->username) > 31 /*TODO: make max configurable*/) {
                                outdata->failed = true;
                                outdata->reason = strdup("Username cannot be longer than 31 characters.");
                            } else {
                                pthread_mutex_lock(&pdatalock);
                                if (pdata[msg.uind].uuid == msg.uuid) {
                                    pdata[msg.uind].player.login = true;
                                    pdata[msg.uind].player.username = strdup(data->username);
                                    // TODO: handle uid and password when no NOAUTH
                                    char addr[22];
                                    printf("Player on %s logged in as %s", getCxnAddrStr(pdata[msg.uind].cxn, addr), pdata[msg.uind].player.username);
                                }
                                pthread_mutex_unlock(&pdatalock);
                            }
                        }
                        addMsg(&servmsgout, SERVER_LOGININFO, outdata, msg.uuid, msg.uind);
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
                    default:; {
                        activity = false;
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
        } else if (altutime() - acttime > 100000) {
            microwait(50000);
        }
    }
    return NULL;
}

static struct netcxn* servcxn;

static pthread_t servnetthreadh;

static void* servnetthread(void* args) {
    (void)args;
    uint64_t acttime = altutime();
    while (serveralive) {
        bool activity = false;
        struct netcxn* newcxn;
        if ((newcxn = acceptCxn(servcxn, SERVER_OUTBUF_SIZE, CLIENT_OUTBUF_SIZE))) {
            activity = true;
            {
                char str[22];
                printf("New connection from %s\n", getCxnAddrStr(newcxn, str));
            }
            bool added = false;
            pthread_mutex_lock(&pdatalock);
            for (int i = 0; i < maxclients; ++i) {
                if (!pdata[i].valid) {
                    pdata[i].valid = true;
                    static uint64_t uuid = 0;
                    pdata[i].uuid = uuid++;
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
                {
                    char str[22];
                    printf("Connection to %s dropped due to client limit\n", getCxnAddrStr(newcxn, str));
                }
                closeCxn(newcxn);
            }
            pthread_mutex_unlock(&pdatalock);
        }
        for (int i = 0; i < maxclients; ++i) {
            pthread_mutex_lock(&pdatalock);
            if (pdata[i].valid) {
                if (recvCxn(pdata[i].cxn) < 0) {
                    activity = true;
                    {
                        char str[22];
                        printf("Connection to %s dropped due to disconnect\n", getCxnAddrStr(pdata[i].cxn, str));
                    }
                    closeCxn(pdata[i].cxn);
                    pdata[i].valid = false;
                } else {
                    if (pdata[i].tmpsize < 1) {
                        int dsize = getInbufSize(pdata[i].cxn);
                        if (dsize >= 4) {
                            readFromCxnBuf(pdata[i].cxn, &pdata[i].tmpsize, 4);
                            pdata[i].tmpsize = net2host32(pdata[i].tmpsize);
                            // TODO: Disconnect if size is larger than half of buffer
                        }
                        if (dsize > 0) {
                            activity = true;
                        }
                    } else if (getInbufSize(pdata[i].cxn) >= pdata[i].tmpsize) {
                        activity = true;
                        unsigned char* buf = malloc(pdata[i].tmpsize);
                        readFromCxnBuf(pdata[i].cxn, buf, pdata[i].tmpsize);
                        int ptr = 0;
                        uint8_t tmpbyte = buf[ptr++];
                        switch (tmpbyte) {
                            case MSGTYPE_DATA:; {
                                void* _data = NULL;
                                uint8_t msgdataid = buf[ptr++];
                                //printf("FROM CLIENT[%d]: [%d]\n", i, msgdataid);
                                switch (msgdataid) {
                                    case CLIENT_COMPATINFO:; {
                                        struct client_data_compatinfo* data = malloc(sizeof(*data));
                                        memcpy(&data->ver_major, &buf[ptr], 2);
                                        ptr += 2;
                                        data->ver_major = net2host16(data->ver_major);
                                        memcpy(&data->ver_minor, &buf[ptr], 2);
                                        ptr += 2;
                                        data->ver_minor = net2host16(data->ver_minor);
                                        memcpy(&data->ver_patch, &buf[ptr], 2);
                                        ptr += 2;
                                        data->ver_patch = net2host16(data->ver_patch);
                                        data->flags = buf[ptr++];
                                        uint16_t tmpword = 0;
                                        memcpy(&tmpword, &buf[ptr], 2);
                                        ptr += 2;
                                        tmpword = net2host16(tmpword);
                                        data->client_str = malloc((int)tmpword + 1);
                                        memcpy(data->client_str, &buf[ptr], tmpword);
                                        data->client_str[tmpword] = 0;
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
                                break;
                            }
                            default:; {
                                printf("CLIENT[%d] SENT INVALID MSGTYPE: [%d]\n", i, tmpbyte);
                            }
                        }
                        free(buf);
                        pdata[i].tmpsize = 0;
                    }
                    {
                        struct msgdata_msg msg;
                        if (getNextMsgForUUID(&servmsgout, &msg, pdata[i].uuid) && msg.uind == i) {
                            activity = true;
                            uint8_t tmpbyte[2] = {MSGTYPE_DATA, msg.id};
                            uint32_t msgsize = 2;
                            switch (msg.id) {
                                case SERVER_COMPATINFO:; {
                                    struct server_data_compatinfo* tmpdata = msg.data;
                                    msgsize += 2 + 2 + 2 + 1 + 2 + (uint16_t)(strlen(tmpdata->server_str));
                                    break;
                                }
                                case SERVER_UPDATECHUNK:; {
                                    msgsize += 8 + 8 + 65536 * sizeof(struct blockdata);
                                    break;
                                }
                                case SERVER_SETSKYCOLOR:; {
                                    msgsize += 1 + 1 + 1;
                                    break;
                                }
                            }
                            if (getOutbufLeft(pdata[i].cxn) < (int)msgsize) {
                                addMsg(&servmsgout, msg.id, msg.data, msg.uuid, msg.uind);
                                goto srv_nosend;
                            }
                            //printf("TO CLIENT[%d]: [%d]\n", i, msg.id);
                            msgsize = host2net32(msgsize);
                            writeToCxnBuf(pdata[i].cxn, &msgsize, 4);
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
                            srv_nosend:;
                        }
                    }
                    sendCxn(pdata[i].cxn);
                }
            }
            pthread_mutex_unlock(&pdatalock);
        }
        if (activity) {
            acttime = altutime();
        } else if (altutime() - acttime > 1500000) {
            microwait(50000);
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
    serveralive = true;
    puts("- Initializing connection...");
    port = servcxn->info.port;
    setCxnBufSize(servcxn, SERVER_SNDBUF_SIZE, CLIENT_SNDBUF_SIZE);
    pdata = calloc(maxclients, sizeof(*pdata));
    initMsgData(&servmsgin);
    initMsgData(&servmsgout);
    puts("- Initializing noise...");
    setRandSeed(0, 32464);
    initNoiseTable(0);
    initWorldgen();
    puts("- Initializing timer and events...");
    initTimerData(&servtimer);
    addTimer(&servtimer, _SERVER_INTERNAL1, 2000000);
    addTimer(&servtimer, _SERVER_INTERNAL2, 1200000);
    #ifdef NAME_THREADS
    char name[256];
    char name2[256];
    name[0] = 0;
    name2[0] = 0;
    #endif
    puts("- Starting server network thread...");
    pthread_create(&servnetthreadh, NULL, &servnetthread, NULL);
    #ifdef NAME_THREADS
    pthread_getname_np(servnetthreadh, name2, 256);
    sprintf(name, "%s:snet", name2);
    pthread_setname_np(servnetthreadh, name);
    #endif
    puts("- Starting server timer thread...");
    pthread_create(&servtimerh, NULL, &servtimerthread, &servtimer);
    #ifdef NAME_THREADS
    pthread_getname_np(servtimerh, name2, 256);
    sprintf(name, "%s:stmr", name2);
    pthread_setname_np(servtimerh, name);
    #endif
    for (int i = 0; i < SERVER_THREADS && i < MAX_THREADS; ++i) {
        #ifdef NAME_THREADS
        name[0] = 0;
        name2[0] = 0;
        #endif
        printf("- Starting server thread [%d]...\n", i);
        pthread_create(&servpthreads[i], NULL, &servthread, (void*)(intptr_t)i);
        #ifdef NAME_THREADS
        pthread_getname_np(servpthreads[i], name2, 256);
        sprintf(name, "%s:srv%d", name2, i);
        pthread_setname_np(servpthreads[i], name);
        #endif
    }
    {
        char str[22];
        printf("Started server on %s\n", getCxnAddrStr(servcxn, str));
    }
    return port;
}

void stopServer() {
    puts("Stopping server...");
    serveralive = false;
    for (int i = 0; i < SERVER_THREADS && i < MAX_THREADS; ++i) {
        printf("- Waiting for server thread [%d]...\n", i);
        pthread_join(servpthreads[i], NULL);
    }
    puts("- Waiting for server network thread...");
    pthread_join(servnetthreadh, NULL);
    puts("- Waiting for server timer thread...");
    pthread_join(servtimerh, NULL);
    puts("- Closing connections...");
    for (int i = 0; i < maxclients; ++i) {
        if (pdata[i].valid) {
            closeCxn(pdata[i].cxn);
        }
    }
    closeCxn(servcxn);
    puts("- Cleaning up...");
    free(pdata);
    deinitTimerData(&servtimer);
    deinitMsgData(&servmsgin);
    deinitMsgData(&servmsgout);
    puts("Server stopped");
}

#ifndef SERVER

#include <stdarg.h>

static struct netcxn* clicxn;

static void (*callback)(int, void*);

static struct msgdata climsgout;

static pthread_t clinetthreadh;

static void* clinetthread(void* args) {
    (void)args;
    int tmpsize = 0;
    uint64_t acttime = altutime();
    while (true) {
        bool activity = false;
        recvCxn(clicxn);
        if (tmpsize < 1) {
            int dsize = getInbufSize(clicxn);
            if (dsize >= 4) {
                readFromCxnBuf(clicxn, &tmpsize, 4);
                tmpsize = net2host32(tmpsize);
                // TODO: Disconnect if size is larger than half of buffer
            }
            if (dsize > 0) activity = true;
        } else if (getInbufSize(clicxn) >= tmpsize) {
            activity = true;
            unsigned char* buf = malloc(tmpsize);
            readFromCxnBuf(clicxn, buf, tmpsize);
            int ptr = 0;
            uint8_t tmpbyte = buf[ptr++];
            switch (tmpbyte) {
                case MSGTYPE_DATA:; {
                    tmpbyte = buf[ptr++];
                    //printf("FROM SERVER: [%d]\n", tmpbyte);
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
                    break;
                }
                default:; {
                    printf("SERVER SENT INVALID MSGTYPE: [%d]\n", tmpbyte);
                }
            }
            free(buf);
            tmpsize = 0;
        }
        {
            struct msgdata_msg msg;
            if (getNextMsg(&climsgout, &msg)) {
                activity = true;
                uint8_t tmpbyte[2] = {MSGTYPE_DATA, msg.id};
                uint32_t msgsize = 2;
                switch (msg.id) {
                    case CLIENT_COMPATINFO:; {
                        struct client_data_compatinfo* tmpdata = msg.data;
                        msgsize += 2 + 2 + 2 + 1 + 2 + (uint16_t)(strlen(tmpdata->client_str));
                        break;
                    }
                    case CLIENT_GETCHUNK:; {
                        msgsize += 8 + 8;
                        break;
                    }
                }
                if (getOutbufLeft(clicxn) < (int)msgsize) {
                    addMsg(&climsgout, msg.id, msg.data, msg.uuid, msg.uind);
                    goto cli_nosend;
                }
                //printf("TO SERVER: [%d]\n", msg.id);
                msgsize = host2net32(msgsize);
                writeToCxnBuf(clicxn, &msgsize, 4);
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
                        writeToCxnBuf(clicxn, &tmpdata->flags, 1);
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
                if (msg.data) free(msg.data);
                cli_nosend:;
            }
        }
        sendCxn(clicxn);
        if (activity) {
            acttime = altutime();
        } else if (altutime() - acttime > 500000) {
            microwait(50000);
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
            tmpdata->flags = va_arg(args, unsigned);
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
