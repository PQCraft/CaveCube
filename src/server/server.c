#include <main/main.h>
#include "network.h"
#include "server.h"
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
    bool async;
    int size;
    int rptr;
    int wptr;
    struct msgdata_msg* msg;
    pthread_mutex_t lock;
};

static force_inline void initMsgData(struct msgdata* mdata) {
    mdata->async = false;
    mdata->size = 0;
    mdata->rptr = -1;
    mdata->wptr = -1;
    mdata->msg = malloc(0);
    pthread_mutex_init(&mdata->lock, NULL);
}

static force_inline void deinitMsgData(struct msgdata* mdata) {
    pthread_mutex_lock(&mdata->lock);
    free(mdata->msg);
    pthread_mutex_unlock(&mdata->lock);
    pthread_mutex_destroy(&mdata->lock);
}

static force_inline void addMsg(struct msgdata* mdata, int id, void* data, uint64_t uuid, int uind) {
    pthread_mutex_lock(&mdata->lock);
    if (mdata->async) {
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
    } else {
        if (mdata->wptr < 0 || mdata->rptr >= mdata->size) {
            mdata->rptr = 0;
            mdata->size = 0;
        }
        mdata->wptr = mdata->size++;
        //printf("[%lu]: wptr: [%d] size: [%d]\n", (uintptr_t)mdata, mdata->wptr, mdata->size);
        mdata->msg = realloc(mdata->msg, mdata->size * sizeof(*mdata->msg));
        mdata->msg[mdata->wptr].id = id;
        mdata->msg[mdata->wptr].data = data;
        mdata->msg[mdata->wptr].uuid = uuid;
        mdata->msg[mdata->wptr].uind = uind;
    }
    pthread_mutex_unlock(&mdata->lock);
}

static force_inline bool getNextMsg(struct msgdata* mdata, struct msgdata_msg* msg) {
    pthread_mutex_lock(&mdata->lock);
    if (mdata->async) {
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
    } else {
        if (mdata->rptr >= 0) {
            for (int i = mdata->rptr; i < mdata->size; ++i) {
                if (mdata->msg[i].id >= 0) {
                    msg->id = mdata->msg[i].id;
                    msg->data = mdata->msg[i].data;
                    msg->uuid = mdata->msg[i].uuid;
                    msg->uind = mdata->msg[i].uind;
                    mdata->msg[i].id = -1;
                    mdata->rptr = i + 1;
                    //printf("[%lu]: rptr: [%d] size: [%d]\n", (uintptr_t)mdata, mdata->rptr, mdata->size);
                    pthread_mutex_unlock(&mdata->lock);
                    return true;
                }
            }
        }
    }
    pthread_mutex_unlock(&mdata->lock);
    return false;
}

static force_inline bool getNextMsgForUUID(struct msgdata* mdata, struct msgdata_msg* msg, uint64_t uuid) {
    pthread_mutex_lock(&mdata->lock);
    if (mdata->async) {
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
    } else {
        if (mdata->rptr >= 0) {
            for (int i = mdata->rptr; i < mdata->size; ++i) {
                if (mdata->msg[i].id >= 0 && mdata->msg[i].uuid == uuid) {
                    msg->id = mdata->msg[i].id;
                    msg->data = mdata->msg[i].data;
                    msg->uuid = mdata->msg[i].uuid;
                    msg->uind = mdata->msg[i].uind;
                    mdata->msg[i].id = -1;
                    mdata->rptr = i + 1;
                    //printf("[%lu]: rptr: [%d] size: [%d]\n", (uintptr_t)mdata, mdata->rptr, mdata->size);
                    pthread_mutex_unlock(&mdata->lock);
                    return true;
                }
            }
        }
    }
    pthread_mutex_unlock(&mdata->lock);
    return false;
}

enum {
    MSG_PRIO_LOW,
    MSG_PRIO_NORMAL,
    MSG_PRIO_HIGH,
    MSG_PRIO__MAX,
};

static struct msgdata servmsgin[3];
static struct msgdata servmsgout[3];

struct timerdata_timer {
    int event;
    int priority;
    uint64_t interval;
    uint64_t intertime;
    bool ack;
};

struct timerdata {
    bool valid;
    int size;
    struct timerdata_timer* tmr;
    pthread_mutex_t lock;
};

static force_inline void initTimerData(struct timerdata* tdata) {
    tdata->valid = true;
    tdata->size = 0;
    tdata->tmr = malloc(0);
    pthread_mutex_init(&tdata->lock, NULL);
}

static force_inline void deinitTimerData(struct timerdata* tdata) {
    pthread_mutex_lock(&tdata->lock);
    tdata->valid = false;
    free(tdata->tmr);
    pthread_mutex_unlock(&tdata->lock);
    pthread_mutex_destroy(&tdata->lock);
}

static force_inline int addTimer(struct timerdata* tdata, int event, int prio, uint64_t interval) {
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
        tdata->tmr[index].priority = prio;
        tdata->tmr[index].interval = interval;
        tdata->tmr[index].intertime = altutime() + interval;
    }
    pthread_mutex_unlock(&tdata->lock);
    return index;
}

static force_inline void removeTimer(struct timerdata* tdata, int i) {
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
            pthread_mutex_lock(&tdata->lock);
            //addMsg(&servmsgin[tdata->tmr[index].priority], event, NULL, 0, -1);
            tdata->tmr[index].intertime = altutime() + tdata->tmr[index].interval;
            //printf("Firing event [%d]: [%d]\n", index, event);
            pthread_mutex_unlock(&tdata->lock);
        }
    }
    return NULL;
}

enum {
    _SERVER_TIMER1 = 512,
    _SERVER_TIMER2,
    _SERVER_TICK,
};

static struct timerdata servtimer;

static force_inline unsigned getInbufSize(struct netcxn* cxn) {
    return cxn->inbuf->dlen;
}

static force_inline int getOutbufLeft(struct netcxn* cxn) {
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
    unsigned tmpsize;
    struct netcxn* cxn;
    struct player player;
};

static struct playerdata* pdata;
static pthread_mutex_t pdatalock;

enum {
    _SERVER_USERCONNECT = 256,
    _SERVER_USERDISCONNECT,
    _SERVER_USERLOGIN,
};
struct _server_data_userlogin {
    uint64_t uuid;
    int uind;
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
        int p;
        if (getNextMsg(&servmsgin[(p = MSG_PRIO_HIGH)], &msg) ||
        getNextMsg(&servmsgin[(p = MSG_PRIO_NORMAL)], &msg) ||
        getNextMsg(&servmsgin[(p = MSG_PRIO_LOW)], &msg)) {
            activity = true;
            if (msg.uuid) {
                //printf("Received message [%d] for player handle [%d]:[%"PRId64"]\n", msg.id, msg.uind, msg.uuid);
                pthread_mutex_lock(&pdatalock);
                bool cond = (pdata[msg.uind].valid && pdata[msg.uind].uuid == msg.uuid);
                bool login = pdata[msg.uind].player.login;
                pthread_mutex_unlock(&pdatalock);
                if (cond) {
                    //puts("Processing message...");
                    switch (msg.id) {
                        case CLIENT_PING:; {
                            addMsg(&servmsgout[MSG_PRIO_NORMAL], SERVER_PONG, NULL, msg.uuid, msg.uind);
                        } break;
                        case CLIENT_COMPATINFO:; {
                            struct client_data_compatinfo* data = msg.data;
                            struct server_data_compatinfo* outdata = malloc(sizeof(*outdata));
                            free(data->client_str);
                            outdata->ver_major = VER_MAJOR;
                            outdata->ver_minor = VER_MINOR;
                            outdata->ver_patch = VER_PATCH;
                            outdata->flags = SERVER_COMPATINFO_FLAG_NOAUTH;
                            outdata->server_str = strdup(PROG_NAME);
                            addMsg(&servmsgout[MSG_PRIO_HIGH], SERVER_COMPATINFO, outdata, msg.uuid, msg.uind);
                        } break;
                        case CLIENT_NEWUID:; {
                            if (login) goto invalmsg;
                            // TODO: only pay attention to 1 NEWID request per connection
                            // TODO: add configurable max number ids that can be created per ip
                            struct server_data_disconnect* outdata = calloc(1, sizeof(*outdata));
                            outdata->reason = strdup("Server authentication not enabled.");
                            addMsg(&servmsgout[MSG_PRIO_HIGH], SERVER_DISCONNECT, outdata, msg.uuid, msg.uind);
                        } break;
                        case CLIENT_LOGIN:; {
                            if (login) goto invalmsg;
                            struct client_data_login* data = msg.data;
                            if (false /*purposely fail if true*/) {
                                struct server_data_disconnect* outdata = calloc(1, sizeof(*outdata));
                                outdata->reason = strdup("Debug.");
                                addMsg(&servmsgout[MSG_PRIO_HIGH], SERVER_DISCONNECT, outdata, msg.uuid, msg.uind);
                            } else {
                                if (!*data->username) {
                                    struct server_data_disconnect* outdata = calloc(1, sizeof(*outdata));
                                    outdata->reason = strdup("Username cannot be empty.");
                                    addMsg(&servmsgout[MSG_PRIO_HIGH], SERVER_DISCONNECT, outdata, msg.uuid, msg.uind);
                                } else if (strlen(data->username) > 31 /*TODO: make max configurable*/) {
                                    struct server_data_disconnect* outdata = calloc(1, sizeof(*outdata));
                                    outdata->reason = strdup("Username cannot be longer than 31 characters.");
                                    addMsg(&servmsgout[MSG_PRIO_HIGH], SERVER_DISCONNECT, outdata, msg.uuid, msg.uind);
                                } else {
                                    pthread_mutex_lock(&pdatalock);
                                    if (pdata[msg.uind].uuid == msg.uuid && !pdata[msg.uind].player.login) {
                                        pdata[msg.uind].player.login = true;
                                        pdata[msg.uind].player.username = strdup(data->username);
                                        // TODO: handle uid and password when no NOAUTH
                                        char addr[22];
                                        printf("Player on %s logged in as %s\n", getCxnAddrStr(pdata[msg.uind].cxn, addr), pdata[msg.uind].player.username);
                                    }
                                    struct server_data_loginok* outdata = calloc(1, sizeof(*outdata));
                                    outdata->username = strdup(pdata[msg.uind].player.username);
                                    pthread_mutex_unlock(&pdatalock);
                                    addMsg(&servmsgout[MSG_PRIO_HIGH], SERVER_LOGINOK, outdata, msg.uuid, msg.uind);
                                    {
                                        struct _server_data_userlogin* _outdata = calloc(1, sizeof(*_outdata));
                                        _outdata->uuid = msg.uuid;
                                        _outdata->uind = msg.uind;
                                        addMsg(&servmsgin[MSG_PRIO_HIGH], _SERVER_USERLOGIN, _outdata, 0, -1);
                                    }
                                }
                            }
                        } break;
                        case CLIENT_GETCHUNK:; {
                            if (!login) goto invalmsg;
                            struct client_data_getchunk* data = msg.data;
                            struct server_data_updatechunk* outdata = malloc(sizeof(*outdata));
                            outdata->x = data->x;
                            outdata->z = data->z;
                            struct blockdata* blocks = malloc(131072 * sizeof(*blocks));
                            genChunk(data->x, data->z, blocks, worldtype);
                            uint16_t* netorder = malloc(393216 * sizeof(*netorder));
                            for (int i = 0, n = 0; i < 131072; ++i) {
                                netorder[n] = (blocks[i].id << 8) | (blocks[i].subid << 2) | (blocks[i].rotx);
                                netorder[n] = host2net16(netorder[n]);
                                ++n;
                                netorder[n] = (blocks[i].roty << 14) | (blocks[i].rotz << 12) | (blocks[i].charge << 8) | (blocks[i].light_n);
                                netorder[n] = host2net16(netorder[n]);
                                ++n;
                                netorder[n] = (blocks[i].light_r << 10) | (blocks[i].light_g << 5) | (blocks[i].light_b);
                                netorder[n] = host2net16(netorder[n]);
                                ++n;
                            }
                            free(blocks);
                            int len = 1 << 20;
                            int complvl;
                            #if MODULEID == MODULEID_SERVER
                            complvl = 3;
                            #else
                            complvl = 1;
                            #endif
                            len = ezCompress(complvl, 393216 * sizeof(uint16_t), netorder, len, (outdata->cdata = malloc(len)));
                            free(netorder);
                            if (len >= 0) {
                                outdata->len = len;
                                outdata->cdata = realloc(outdata->cdata, outdata->len);
                                //printf("chunk [%"PRId64", %"PRId64"]: [%d]\n", outdata->x, outdata->z, outdata->len);
                                addMsg(&servmsgout[MSG_PRIO_LOW], SERVER_UPDATECHUNK, outdata, msg.uuid, msg.uind);
                            }
                            //microwait(100000);
                        } break;
                        case CLIENT_SETBLOCK:; {
                            if (!login) goto invalmsg;
                            struct client_data_setblock* data = msg.data;
                            //printf("!!! [%"PRId64", %d, %"PRId64"]\n", data->x, data->y, data->z);
                            pthread_mutex_lock(&pdatalock);
                            for (int i = 0; i < maxclients; ++i) {
                                if (pdata[i].valid) {
                                    struct server_data_setblock* tmpdata = malloc(sizeof(*tmpdata));
                                    tmpdata->x = data->x;
                                    tmpdata->z = data->z;
                                    tmpdata->y = data->y;
                                    tmpdata->data = data->data;
                                    addMsg(&servmsgout[MSG_PRIO_NORMAL], SERVER_SETBLOCK, tmpdata, pdata[i].uuid, i);
                                }
                            }
                            pthread_mutex_unlock(&pdatalock);
                        } break;
                        invalmsg:;
                        default:; {
                            printf("Invalid message id [%d] from player [%d] (uuid: [%"PRId64"])\n", msg.id, msg.uind, msg.uuid);
                            //activity = false;
                        } break;
                    }
                    if (msg.data) free(msg.data);
                    //puts("Done");
                } else {
                    //puts("Message dropped (player handle is not valid)");
                    switch (msg.id) {
                        case CLIENT_COMPATINFO:; {
                            struct client_data_compatinfo* data = msg.data;
                            free(data->client_str);
                        } break;
                    }
                    if (msg.data) free(msg.data);
                }
            } else {
                switch (msg.id) {
                    case _SERVER_USERCONNECT:; {
                    } break;
                    case _SERVER_USERDISCONNECT:; {
                    } break;
                    case _SERVER_USERLOGIN:; {
                        struct _server_data_userlogin* data = msg.data;

                        struct server_data_setskycolor* skycolor = malloc(sizeof(*skycolor));
                        struct server_data_setnatcolor* natcolor = malloc(sizeof(*natcolor));

                        *skycolor = (struct server_data_setskycolor){0xA0, 0xC8, 0xFF};
                        *natcolor = (struct server_data_setnatcolor){0xFF, 0xFF, 0xE0};
                        //*skycolor = (struct server_data_setskycolor){0xDF, 0x40, 0x37};
                        //*natcolor = (struct server_data_setnatcolor){0xBC, 0x23, 0x12};

                        addMsg(&servmsgout[MSG_PRIO_HIGH], SERVER_SETSKYCOLOR, skycolor, data->uuid, data->uind);
                        addMsg(&servmsgout[MSG_PRIO_HIGH], SERVER_SETNATCOLOR, natcolor, data->uuid, data->uind);
                    } break;
                    default:; {
                        printf("Invalid internal message id [%d]\n", msg.id);
                        //activity = false;
                    } break;
                }
                if (msg.data) free(msg.data);
            }
        }
        if (activity) {
            acttime = altutime();
        } else if (altutime() - acttime > 100000) {
            microwait(50000);
        }
        microwait(0);
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
        if ((newcxn = acceptCxn(servcxn, SERVER_OUTBUF_SIZE, SERVER_INBUF_SIZE))) {
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
                    static uint64_t uuid = 1;
                    pdata[i].uuid = uuid++;
                    pdata[i].tmpsize = 0;
                    pdata[i].cxn = newcxn;
                    memset(&pdata[i].player, 0, sizeof(pdata[i].player));
                    added = true;
                    addMsg(&servmsgin[MSG_PRIO_HIGH], _SERVER_USERCONNECT, NULL, 0, -1);
                    break;
                }
            }
            if (!added) {
                char str[22];
                printf("Connection to %s dropped due to client limit\n", getCxnAddrStr(newcxn, str));
                closeCxn(newcxn);
            }
            pthread_mutex_unlock(&pdatalock);
            setCxnBufSize(newcxn, SERVER_SNDBUF_SIZE, SERVER_RCVBUF_SIZE);
        }
        for (int i = 0; i < maxclients; ++i) {
            pthread_mutex_lock(&pdatalock);
            if (pdata[i].valid) {
                //int bytes = 0;
                if ((/*bytes = */recvCxn(pdata[i].cxn)) < 0) {
                    activity = true;
                    char str[22];
                    printf("Connection to %s dropped (client disconnected)\n", getCxnAddrStr(pdata[i].cxn, str));
                    closeCxn(pdata[i].cxn);
                    pdata[i].valid = false;
                } else {
                    //static uint64_t tmp0 = 0;
                    //printf("client[%d]: [%d] (read #%"PRIu64")\n", i, bytes, tmp0++);
                    if (pdata[i].tmpsize < 1) {
                        int dsize = getInbufSize(pdata[i].cxn);
                        if (dsize >= 4) {
                            readFromCxnBuf(pdata[i].cxn, &pdata[i].tmpsize, 4);
                            pdata[i].tmpsize = net2host32(pdata[i].tmpsize);
                            //printf("CLIENT[%d] TMPSIZE: [%u]\n", i, pdata[i].tmpsize);
                            if (pdata[i].tmpsize > SERVER_INBUF_SIZE) {
                                char str[22];
                                printf("Connection to %s dropped (message size %u > %u)\n", getCxnAddrStr(pdata[i].cxn, str), pdata[i].tmpsize, SERVER_INBUF_SIZE);
                                closeCxn(pdata[i].cxn);
                                pdata[i].valid = false;
                            }
                        }
                        if (dsize > 0) {
                            activity = true;
                        }
                    } else if (getInbufSize(pdata[i].cxn) >= pdata[i].tmpsize) {
                        activity = true;
                        unsigned char* buf = malloc(pdata[i].tmpsize);
                        readFromCxnBuf(pdata[i].cxn, buf, pdata[i].tmpsize);
                        int ptr = 0;
                        void* _data = NULL;
                        uint8_t msgdataid = buf[ptr++];
                        int priority = MSG_PRIO_NORMAL;
                        //printf("FROM CLIENT[%d]: [%d]:[%d]\n", i, msgdataid, pdata[i].tmpsize);
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
                                int tmplen = pdata[i].tmpsize - ptr;
                                data->client_str = malloc(tmplen + 1);
                                memcpy(data->client_str, &buf[ptr], tmplen);
                                data->client_str[tmplen] = 0;
                                _data = data;
                            } break;
                            case CLIENT_NEWUID:; {
                                struct client_data_newuid* data = malloc(sizeof(*data));
                                memcpy(&data->password, &buf[ptr], 8);
                                data->password = net2host64(data->password);
                                _data = data;
                            } break;
                            case CLIENT_LOGIN:; {
                                struct client_data_login* data = malloc(sizeof(*data));
                                data->flags = buf[ptr++];
                                memcpy(&data->password, &buf[ptr], 8);
                                ptr += 8;
                                data->uid = net2host64(data->uid);
                                memcpy(&data->password, &buf[ptr], 8);
                                ptr += 8;
                                data->password = net2host64(data->password);
                                int tmplen = pdata[i].tmpsize - ptr;
                                data->username = malloc(tmplen + 1);
                                memcpy(data->username, &buf[ptr], tmplen);
                                data->username[tmplen] = 0;
                                _data = data;
                            } break;
                            case CLIENT_GETCHUNK:; {
                                struct client_data_getchunk* data = malloc(sizeof(*data));
                                memcpy(&data->x, &buf[ptr], 8);
                                ptr += 8;
                                data->x = net2host64(data->x);
                                memcpy(&data->z, &buf[ptr], 8);
                                data->z = net2host64(data->z);
                                _data = data;
                                priority = MSG_PRIO_LOW;
                            } break;
                            case CLIENT_SETBLOCK:; {
                                struct client_data_setblock* data = malloc(sizeof(*data));
                                memcpy(&data->x, &buf[ptr], 8);
                                ptr += 8;
                                data->x = net2host64(data->x);
                                memcpy(&data->z, &buf[ptr], 8);
                                ptr += 8;
                                data->z = net2host64(data->z);
                                memcpy(&data->y, &buf[ptr], 2);
                                ptr += 2;
                                data->y = net2host16(data->y);
                                memcpy(&data->data, &buf[ptr], sizeof(struct blockdata));
                                _data = data;
                            } break;
                        }
                        addMsg(&servmsgin[priority], msgdataid, _data, pdata[i].uuid, i);
                        free(buf);
                        pdata[i].tmpsize = 0;
                    }
                    struct msgdata_msg msg;
                    int priority;
                    if ((getNextMsgForUUID(&servmsgout[(priority = MSG_PRIO_HIGH)], &msg, pdata[i].uuid) ||
                    getNextMsgForUUID(&servmsgout[(priority = MSG_PRIO_NORMAL)], &msg, pdata[i].uuid) ||
                    getNextMsgForUUID(&servmsgout[(priority = MSG_PRIO_LOW)], &msg, pdata[i].uuid)) && msg.uind == i) {
                        activity = true;
                        bool disconnect = false;
                        if (pdata[i].valid) {
                            uint8_t tmpbyte = msg.id;
                            uint32_t msgsize = 1;
                            switch (msg.id) {
                                case SERVER_COMPATINFO:; {
                                    struct server_data_compatinfo* tmpdata = msg.data;
                                    msgsize += 2 + 2 + 2 + 1 + strlen(tmpdata->server_str);
                                } break;
                                case SERVER_NEWUID:; {
                                    msgsize += 8;
                                } break;
                                case SERVER_LOGINOK:; {
                                    struct server_data_loginok* tmpdata = msg.data;
                                    if (tmpdata->username) msgsize += strlen(tmpdata->username);
                                } break;
                                case SERVER_DISCONNECT:; {
                                    struct server_data_disconnect* tmpdata = msg.data;
                                    if (tmpdata->reason) msgsize += strlen(tmpdata->reason);
                                } break;
                                case SERVER_UPDATECHUNK:; {
                                    struct server_data_updatechunk* tmpdata = msg.data;
                                    msgsize += 8 + 8 + tmpdata->len;
                                    //printf("msgsize: [%d]\n", tmpdata->len);
                                } break;
                                case SERVER_SETSKYCOLOR:; {
                                    msgsize += 1 + 1 + 1;
                                } break;
                                case SERVER_SETNATCOLOR:; {
                                    msgsize += 1 + 1 + 1;
                                } break;
                                case SERVER_SETBLOCK:; {
                                    msgsize += 8 + 8 + 2 + sizeof(struct blockdata);
                                } break;
                            }
                            if (getOutbufLeft(pdata[i].cxn) < (int)msgsize) {
                                addMsg(&servmsgout[priority], msg.id, msg.data, msg.uuid, msg.uind);
                                goto srv_nosend;
                            }
                            //printf("TO CLIENT[%d]: [%d]\n", i, msg.id);
                            msgsize = host2net32(msgsize);
                            writeToCxnBuf(pdata[i].cxn, &msgsize, 4);
                            writeToCxnBuf(pdata[i].cxn, &tmpbyte, 1);
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
                                    writeToCxnBuf(pdata[i].cxn, tmpdata->server_str, strlen(tmpdata->server_str));
                                    free(tmpdata->server_str);
                                } break;
                                case SERVER_NEWUID:; {
                                    struct server_data_newuid* tmpdata = msg.data;
                                    uint64_t tmpqword = host2net64(tmpdata->uid);
                                    writeToCxnBuf(pdata[i].cxn, &tmpqword, 8);
                                } break;
                                case SERVER_LOGINOK:; {
                                    struct server_data_loginok* tmpdata = msg.data;
                                    writeToCxnBuf(pdata[i].cxn, tmpdata->username, strlen(tmpdata->username));
                                    free(tmpdata->username);
                                } break;
                                case SERVER_DISCONNECT:; {
                                    struct server_data_disconnect* tmpdata = msg.data;
                                    if (tmpdata->reason) {
                                        writeToCxnBuf(pdata[i].cxn, tmpdata->reason, strlen(tmpdata->reason));
                                        free(tmpdata->reason);
                                    }
                                    disconnect = true;
                                } break;
                                case SERVER_UPDATECHUNK:; {
                                    struct server_data_updatechunk* tmpdata = msg.data;
                                    uint64_t tmpqword = host2net64(tmpdata->x);
                                    writeToCxnBuf(pdata[i].cxn, &tmpqword, 8);
                                    tmpqword = host2net64(tmpdata->z);
                                    writeToCxnBuf(pdata[i].cxn, &tmpqword, 8);
                                    writeToCxnBuf(pdata[i].cxn, tmpdata->cdata, tmpdata->len);
                                    free(tmpdata->cdata);
                                } break;
                                case SERVER_SETSKYCOLOR:; {
                                    struct server_data_setskycolor* tmpdata = msg.data;
                                    writeToCxnBuf(pdata[i].cxn, &tmpdata->r, 1);
                                    writeToCxnBuf(pdata[i].cxn, &tmpdata->g, 1);
                                    writeToCxnBuf(pdata[i].cxn, &tmpdata->b, 1);
                                } break;
                                case SERVER_SETNATCOLOR:; {
                                    struct server_data_setnatcolor* tmpdata = msg.data;
                                    writeToCxnBuf(pdata[i].cxn, &tmpdata->r, 1);
                                    writeToCxnBuf(pdata[i].cxn, &tmpdata->g, 1);
                                    writeToCxnBuf(pdata[i].cxn, &tmpdata->b, 1);
                                } break;
                                case SERVER_SETBLOCK:; {
                                    struct server_data_setblock* tmpdata = msg.data;
                                    uint64_t tmpqword = host2net64(tmpdata->x);
                                    writeToCxnBuf(pdata[i].cxn, &tmpqword, 8);
                                    tmpqword = host2net64(tmpdata->z);
                                    writeToCxnBuf(pdata[i].cxn, &tmpqword, 8);
                                    uint16_t tmpword = host2net16(tmpdata->y);
                                    writeToCxnBuf(pdata[i].cxn, &tmpword, 2);
                                    writeToCxnBuf(pdata[i].cxn, &tmpdata->data, sizeof(struct blockdata));
                                } break;
                            }
                            if (msg.data) free(msg.data);
                            srv_nosend:;
                            int bytes_sent = sendCxn(pdata[i].cxn);
                            //if (bytes_sent) printf("server write [%d]\n", bytes_sent);
                            if (bytes_sent < 0) {
                                char str[22];
                                printf("Connection to %s dropped (client disconnected)\n", getCxnAddrStr(pdata[i].cxn, str));
                                closeCxn(pdata[i].cxn);
                                pdata[i].valid = false;
                            } else if (disconnect) {
                                char str[22];
                                printf("Connection to %s dropped (server closed connection)\n", getCxnAddrStr(pdata[i].cxn, str));
                                closeCxn(pdata[i].cxn);
                                pdata[i].valid = false;
                            }
                        }
                    }
                }
            }
            pthread_mutex_unlock(&pdatalock);
            if (!activity) {
                //microwait(1000);
            }
            microwait(0);
        }
        if (!activity) {
            pthread_mutex_lock(&pdatalock);
            pthread_mutex_lock(&servmsgout->lock);
            for (int i = 0; i < servmsgout->size; ++i) {
                if (servmsgout->msg[i].id >= 0 && servmsgout->msg[i].uuid > 0) {
                    if (!(pdata[servmsgout->msg[i].uind].valid && pdata[servmsgout->msg[i].uind].uuid == servmsgout->msg[i].uuid)) {
                        //puts("Dropping");
                        if (servmsgout->msg[i].data) {
                            switch (servmsgout->msg[i].id) {
                                case SERVER_COMPATINFO:; {
                                    struct server_data_compatinfo* tmpdata = servmsgout->msg[i].data;
                                    free(tmpdata->server_str);
                                } break;
                                case SERVER_LOGINOK:; {
                                    struct server_data_loginok* tmpdata = servmsgout->msg[i].data;
                                    free(tmpdata->username);
                                } break;
                                case SERVER_DISCONNECT:; {
                                    struct server_data_disconnect* tmpdata = servmsgout->msg[i].data;
                                    free(tmpdata->reason);
                                } break;
                                case SERVER_UPDATECHUNK:; {
                                    struct server_data_updatechunk* tmpdata = servmsgout->msg[i].data;
                                    free(tmpdata->cdata);
                                } break;
                            }
                            free(servmsgout->msg[i].data);
                        }
                        servmsgout->msg[i].id = -1;
                    }
                }
            }
            pthread_mutex_unlock(&servmsgout->lock);
            pthread_mutex_unlock(&pdatalock);
            microwait(0);
        }
        if (activity) {
            acttime = altutime();
        } else if (altutime() - acttime > 500000) {
            microwait(5000);
        }
    }
    return NULL;
}

int startServer(char* addr, int port, int mcli, char* world) {
    (void)world;
    setRandSeed(1, altutime() + (uintptr_t)"");
    if (port < 0 || port > 0xFFFF) port = 46001 + (getRandWord(1) % 998);
    if (mcli > 0) maxclients = mcli;
    printf("Starting server on %s:%d with a max of %d player%s...\n", (addr) ? addr : "0.0.0.0", port, maxclients, (maxclients == 1) ? "" : "s");
    if (!(servcxn = newCxn(CXN_PASSIVE, addr, port, -1, -1))) {
        fputs("servStart: Failed to create connection\n", stderr);
        return -1;
    }
    serveralive = true;
    #if DBGLVL(1)
    puts("  Initializing connection...");
    #endif
    port = servcxn->info.port;
    setCxnBufSize(servcxn, SERVER_SNDBUF_SIZE, SERVER_RCVBUF_SIZE);
    pdata = calloc(maxclients, sizeof(*pdata));
    for (int i = 0; i < MSG_PRIO__MAX; ++i) {
        initMsgData(&servmsgin[i]);
    }
    for (int i = 0; i < MSG_PRIO__MAX; ++i) {
        initMsgData(&servmsgout[i]);
    }
    //servmsgin[MSG_PRIO_LOW].async = true;
    //servmsgout[MSG_PRIO_LOW].async = true;
    //servmsgout[MSG_PRIO_NORMAL].async = true;
    #if DBGLVL(1)
    puts("  Initializing noise...");
    #endif
    uint32_t rand = getRandDWord(1);
    printf("rand: [%u]\n", rand);
    setRandSeed(0, rand);
    //setRandSeed(0, 3270604736);
    initNoiseTable(0);
    initWorldgen();
    #if DBGLVL(1)
    puts("  Initializing timer and events...");
    #endif
    initTimerData(&servtimer);
    addTimer(&servtimer, _SERVER_TIMER1, MSG_PRIO_LOW, 2000000);
    addTimer(&servtimer, _SERVER_TIMER2, MSG_PRIO_LOW, 1200000);
    addTimer(&servtimer, _SERVER_TICK, MSG_PRIO_HIGH, 50000);
    #ifdef NAME_THREADS
    char name[256];
    char name2[256];
    name[0] = 0;
    name2[0] = 0;
    #endif
    #if DBGLVL(1)
    puts("  Starting server network thread...");
    #endif
    pthread_create(&servnetthreadh, NULL, &servnetthread, NULL);
    #ifdef NAME_THREADS
    pthread_getname_np(servnetthreadh, name2, 256);
    sprintf(name, "%.8s:snet", name2);
    pthread_setname_np(servnetthreadh, name);
    #endif
    #if DBGLVL(1)
    puts("  Starting server timer thread...");
    #endif
    pthread_create(&servtimerh, NULL, &servtimerthread, &servtimer);
    #ifdef NAME_THREADS
    pthread_getname_np(servtimerh, name2, 256);
    sprintf(name, "%.8s:stmr", name2);
    pthread_setname_np(servtimerh, name);
    #endif
    if (SERVER_THREADS < 2) SERVER_THREADS = 2;
    for (int i = 0; i < SERVER_THREADS && i < MAX_THREADS; ++i) {
        #ifdef NAME_THREADS
        name[0] = 0;
        name2[0] = 0;
        #endif
        #if DBGLVL(1)
        printf("  Starting server thread [%d]...\n", i);
        #endif
        pthread_create(&servpthreads[i], NULL, &servthread, (void*)(intptr_t)i);
        #ifdef NAME_THREADS
        pthread_getname_np(servpthreads[i], name2, 256);
        sprintf(name, "%.8s:srv%d", name2, i);
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
        #if DBGLVL(1)
        printf("  Waiting for server thread [%d]...\n", i);
        #endif
        pthread_join(servpthreads[i], NULL);
    }
    #if DBGLVL(1)
    puts("  Waiting for server network thread...");
    #endif
    pthread_join(servnetthreadh, NULL);
    #if DBGLVL(1)
    puts("  Waiting for server timer thread...");
    #endif
    pthread_join(servtimerh, NULL);
    #if DBGLVL(1)
    puts("  Closing connections...");
    #endif
    for (int i = 0; i < maxclients; ++i) {
        if (pdata[i].valid) {
            closeCxn(pdata[i].cxn);
        }
    }
    closeCxn(servcxn);
    #if DBGLVL(1)
    puts("  Cleaning up...");
    #endif
    free(pdata);
    deinitTimerData(&servtimer);
    for (int i = 0; i < MSG_PRIO__MAX; ++i) {
        deinitMsgData(&servmsgin[i]);
    }
    for (int i = 0; i < MSG_PRIO__MAX; ++i) {
        deinitMsgData(&servmsgout[i]);
    }
    puts("Server stopped");
}

#if MODULEID == MODULEID_GAME

#include <stdarg.h>

static struct netcxn* clicxn;
static struct msgdata climsgout;
static bool clientalive = false;

static bool (*callback)(int, void*);

static pthread_t clinetthreadh;

static void* clinetthread(void* args) {
    (void)args;
    unsigned tmpsize = 0;
    uint64_t acttime = altutime();
    bool cbresult = true;
    while (clientalive && cbresult) {
        bool activity = false;
        {
            struct msgdata_msg msg;
            if (getNextMsg(&climsgout, &msg)) {
                activity = true;
                uint8_t tmpbyte = msg.id;
                uint32_t msgsize = 1;
                switch (msg.id) {
                    case CLIENT_COMPATINFO:; {
                        struct client_data_compatinfo* tmpdata = msg.data;
                        msgsize += 2 + 2 + 2 + 2 + strlen(tmpdata->client_str);
                    } break;
                    case CLIENT_NEWUID:; {
                        msgsize += 8;
                    } break;
                    case CLIENT_LOGIN:; {
                        struct client_data_login* tmpdata = msg.data;
                        msgsize += 1 + 8 + 8 + strlen(tmpdata->username);
                    } break;
                    case CLIENT_GETCHUNK:; {
                        msgsize += 8 + 8;
                    } break;
                    case CLIENT_SETBLOCK:; {
                        msgsize += 8 + 8 + 2 + sizeof(struct blockdata);
                    } break;
                }
                if (getOutbufLeft(clicxn) < (int)msgsize) {
                    addMsg(&climsgout, msg.id, msg.data, msg.uuid, msg.uind);
                    goto cli_nosend;
                }
                //printf("TO SERVER: [%d]\n", msg.id);
                msgsize = host2net32(msgsize);
                writeToCxnBuf(clicxn, &msgsize, 4);
                writeToCxnBuf(clicxn, &tmpbyte, 1);
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
                    } break;
                    case CLIENT_NEWUID:; {
                        struct client_data_newuid* tmpdata = msg.data;
                        uint64_t tmpqword = host2net64(tmpdata->password);
                        writeToCxnBuf(clicxn, &tmpqword, 8);
                    } break;
                    case CLIENT_LOGIN:; {
                        struct client_data_login* tmpdata = msg.data;
                        writeToCxnBuf(clicxn, &tmpdata->flags, 1);
                        uint64_t tmpqword = host2net64(tmpdata->uid);
                        writeToCxnBuf(clicxn, &tmpqword, 8);
                        tmpqword = host2net64(tmpdata->password);
                        writeToCxnBuf(clicxn, &tmpqword, 8);
                        writeToCxnBuf(clicxn, tmpdata->username, strlen(tmpdata->username));
                        free(tmpdata->username);
                    } break;
                    case CLIENT_GETCHUNK:; {
                        struct client_data_getchunk* tmpdata = msg.data;
                        uint64_t tmpqword = host2net64(tmpdata->x);
                        writeToCxnBuf(clicxn, &tmpqword, 8);
                        tmpqword = host2net64(tmpdata->z);
                        writeToCxnBuf(clicxn, &tmpqword, 8);
                    } break;
                    case CLIENT_SETBLOCK:; {
                        struct client_data_setblock* tmpdata = msg.data;
                        uint64_t tmpqword = host2net64(tmpdata->x);
                        writeToCxnBuf(clicxn, &tmpqword, 8);
                        tmpqword = host2net64(tmpdata->z);
                        writeToCxnBuf(clicxn, &tmpqword, 8);
                        uint16_t tmpword = host2net16(tmpdata->y);
                        writeToCxnBuf(clicxn, &tmpword, 2);
                        writeToCxnBuf(clicxn, &tmpdata->data, sizeof(struct blockdata));
                    } break;
                }
                if (msg.data) free(msg.data);
                cli_nosend:;
            }
        }
        sendCxn(clicxn);
        /*int bytes = */recvCxn(clicxn);
        //if (bytes) printf("client read [%d]\n", bytes);
        if (tmpsize < 1) {
            int dsize = getInbufSize(clicxn);
            if (dsize >= 4) {
                readFromCxnBuf(clicxn, &tmpsize, 4);
                tmpsize = net2host32(tmpsize);
                //printf("SERVER TMPSIZE: [%u]\n", tmpsize);
                // TODO: Disconnect if size is larger than half of buffer
            }
            if (dsize > 0) activity = true;
        } else if (getInbufSize(clicxn) >= tmpsize) {
            activity = true;
            unsigned char* buf = malloc(tmpsize);
            readFromCxnBuf(clicxn, buf, tmpsize);
            int ptr = 0;
            uint8_t tmpbyte = buf[ptr++];
            //printf("FROM SERVER: [%d]:[%d]\n", tmpbyte, tmpsize);
            switch (tmpbyte) {
                case SERVER_PONG:; {
                    cbresult = callback(SERVER_PONG, NULL);
                } break;
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
                    int tmplen = tmpsize - ptr;
                    data.server_str = malloc(tmplen + 1);
                    memcpy(data.server_str, &buf[ptr], tmplen);
                    data.server_str[tmplen] = 0;
                    cbresult = callback(SERVER_COMPATINFO, &data);
                    free(data.server_str);
                } break;
                case SERVER_NEWUID:; {
                    struct server_data_newuid data;
                    memcpy(&data.uid, &buf[ptr], 8);
                    data.uid = net2host64(data.uid);
                    cbresult = callback(SERVER_NEWUID, &data);
                } break;
                case SERVER_LOGINOK:; {
                    struct server_data_loginok data;
                    int tmplen = tmpsize - ptr;
                    data.username = malloc(tmplen + 1);
                    memcpy(data.username, &buf[ptr], tmplen);
                    data.username[tmplen] = 0;
                    cbresult = callback(SERVER_LOGINOK, &data);
                    free(data.username);
                } break;
                case SERVER_DISCONNECT:; {
                    struct server_data_disconnect data;
                    int tmplen = tmpsize - ptr;
                    data.reason = malloc(tmplen + 1);
                    memcpy(data.reason, &buf[ptr], tmplen);
                    data.reason[tmplen] = 0;
                    cbresult = callback(SERVER_DISCONNECT, &data);
                    free(data.reason);
                } break;
                case SERVER_UPDATECHUNK:; {
                    struct server_data_updatechunk data;
                    memcpy(&data.x, &buf[ptr], 8);
                    ptr += 8;
                    data.x = net2host64(data.x);
                    memcpy(&data.z, &buf[ptr], 8);
                    ptr += 8;
                    data.z = net2host64(data.z);
                    //uint64_t stime = altutime();
                    uint16_t* netorder = malloc(393216 * sizeof(*netorder));
                    if (ezDecompress(tmpsize - 8 - 8 - 1, &buf[ptr], 393216 * sizeof(uint16_t), netorder) >= 0) {
                        //printf("recv [%"PRId64", %"PRId64"]: [%d]\n", data.x, data.z, tmpsize - 8 - 8 - 1);
                        data.bdata = malloc(131072 * sizeof(*data.bdata));
                        for (int i = 0, n = 0; i < 131072; ++i) {
                            netorder[n] = host2net16(netorder[n]);
                            data.bdata[i].id = netorder[n] >> 8;
                            data.bdata[i].subid = netorder[n] >> 2;
                            data.bdata[i].rotx = netorder[n];
                            ++n;

                            netorder[n] = host2net16(netorder[n]);
                            data.bdata[i].roty = netorder[n] >> 14;
                            data.bdata[i].rotz = netorder[n] >> 12;
                            data.bdata[i].charge = netorder[n] >> 8;
                            data.bdata[i].light_n = netorder[n];
                            ++n;

                            netorder[n] = host2net16(netorder[n]);
                            data.bdata[i].light_r = netorder[n] >> 10;
                            data.bdata[i].light_g = netorder[n] >> 5;
                            data.bdata[i].light_b = netorder[n];
                            ++n;
                        }
                        cbresult = callback(SERVER_UPDATECHUNK, &data);
                        free(data.bdata);
                        /*
                        double time = (altutime() - stime) / 1000.0;
                        printf("decompressed: [%"PRId64", %"PRId64"] in [%lgms]\n", data.x, data.z, time);
                        */
                    }
                    free(netorder);
                } break;
                case SERVER_SETSKYCOLOR:; {
                    struct server_data_setskycolor data;
                    data.r = buf[ptr++];
                    data.g = buf[ptr++];
                    data.b = buf[ptr++];
                    cbresult = callback(SERVER_SETSKYCOLOR, &data);
                } break;
                case SERVER_SETNATCOLOR:; {
                    struct server_data_setnatcolor data;
                    data.r = buf[ptr++];
                    data.g = buf[ptr++];
                    data.b = buf[ptr++];
                    cbresult = callback(SERVER_SETNATCOLOR, &data);
                } break;
                case SERVER_SETBLOCK:; {
                    struct server_data_setblock data;
                    memcpy(&data.x, &buf[ptr], 8);
                    ptr += 8;
                    data.x = net2host64(data.x);
                    memcpy(&data.z, &buf[ptr], 8);
                    ptr += 8;
                    data.z = net2host64(data.z);
                    memcpy(&data.y, &buf[ptr], 2);
                    ptr += 2;
                    data.y = net2host16(data.y);
                    memcpy(&data.data, &buf[ptr], sizeof(struct blockdata));
                    cbresult = callback(SERVER_SETBLOCK, &data);
                } break;
            }
            free(buf);
            tmpsize = 0;
        }
        microwait(0);
        if (activity) {
            acttime = altutime();
        } else if (altutime() - acttime > 100000) {
            microwait(50000);
        }
    }
    return NULL;
}

bool cliConnect(char* addr, int port, bool (*cb)(int, void*)) {
    if (!(clicxn = newCxn(CXN_ACTIVE, addr, port, CLIENT_OUTBUF_SIZE, CLIENT_INBUF_SIZE))) {
        fputs("cliConnect: Failed to create connection\n", stderr);
        return false;
    }
    clientalive = true;
    setCxnBufSize(clicxn, CLIENT_SNDBUF_SIZE, CLIENT_RCVBUF_SIZE);
    initMsgData(&climsgout);
    climsgout.async = false;
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
    sprintf(name, "%.8s:ct", name2);
    pthread_setname_np(clinetthreadh, name);
    #endif
    return true;
}

static char* setuperr;
static int setuperrsz;
static bool ping = false;
static bool compatinfo = false;
static bool newuid = false;
static bool loginok = false;
static bool disconnect = false;
static struct cliSetupInfo* setupinf;

static bool _tmpcb(int msg, void* _data) {
    switch (msg) {
        case SERVER_PONG:; {
            //printf("Server ponged\n");
            ping = true;
        } break;
        case SERVER_COMPATINFO:; {
            struct server_data_compatinfo* data = _data;
            setupinf->out.srv.ver.major = data->ver_major;
            setupinf->out.srv.ver.minor = data->ver_minor;
            setupinf->out.srv.ver.patch = data->ver_patch;
            setupinf->out.srv.flags = data->flags;
            free(setupinf->out.srv.name);
            setupinf->out.srv.name = strdup(data->server_str);
            compatinfo = true;
        } break;
        case SERVER_NEWUID:; {
            struct server_data_newuid* data = _data;
            setupinf->out.login.uid = data->uid;
            newuid = true;
        } break;
        case SERVER_LOGINOK:; {
            struct server_data_loginok* data = _data;
            free(setupinf->out.login.username);
            setupinf->out.login.username = strdup(data->username);
            loginok = true;
            return false;
        } break;
        case SERVER_DISCONNECT:; {
            struct server_data_disconnect* data = _data;
            setupinf->out.login.failed = true;
            free(setupinf->out.login.failreason);
            setupinf->out.login.failreason = strdup(data->reason);
            disconnect = true;
            return false;
        } break;
        default:; {
            printf("_tmpcb: Unhandled message: [%d]\n", msg);
        } break;
    }
    return true;
}

int cliConnectAndSetup(char* addr, int port, bool (*cb)(int, void*), char* err, int errlen, struct cliSetupInfo* inf) {
    setuperr = err;
    setuperrsz = errlen;
    setupinf = inf;
    uint64_t timeout = inf->in.timeout * 1000;
    if (!cliConnect(addr, port, _tmpcb)) {
        snprintf(err, errlen, "Could not make connection to server");
        return 0;
    }

    float taskct = 5.0;
    float task = 0.0;
    if (inf->in.settext) inf->in.settext("Pinging server...", (++task / taskct) * 100.0);
    puts("Pinging server...");
    cliSend(CLIENT_PING);
    uint64_t time = altutime();
    while (!ping) {
        if (inf->in.quit && inf->in.quit()) {err[0] = 0; goto retuser;}
        if (altutime() - time > timeout) {
            snprintf(err, errlen, "Timed out waiting for pong");
            goto retfalse;
        }
    }
    time = (altutime() - time) / 1000;
    printf("Server responded in %"PRId64" ms\n", time);

    if (inf->in.settext) inf->in.settext("Checking compatibility...", (++task / taskct) * 100.0);
    puts("Sending compatibility info...");
    cliSend(CLIENT_COMPATINFO, VER_MAJOR, VER_MINOR, VER_PATCH, PROG_NAME);
    time = altutime();
    while (!compatinfo) {
        if (inf->in.quit && inf->in.quit()) {err[0] = 0; goto retuser;}
        if (altutime() - time > timeout) {
            snprintf(err, errlen, "Timed out waiting for compatibility info");
            goto retfalse;
        }
    }
    printf("Server version is %s %d.%d.%d\n", inf->out.srv.name, inf->out.srv.ver.major, inf->out.srv.ver.minor, inf->out.srv.ver.patch);
    if (inf->out.srv.flags & SERVER_COMPATINFO_FLAG_NOAUTH) puts("- No authentication required");
    if (inf->out.srv.flags & SERVER_COMPATINFO_FLAG_PASSWD) puts("- Password protected");
    if (strcasecmp(inf->out.srv.name, PROG_NAME)) {
        snprintf(err, errlen, "Incompatible game (%s (server) vs %s (client))", inf->out.srv.name, PROG_NAME);
        goto retfalse;
    } else if (inf->out.srv.ver.major != VER_MAJOR || inf->out.srv.ver.minor != VER_MINOR || inf->out.srv.ver.patch != VER_PATCH) {
        snprintf(err, errlen, "Incompatible game version (%d.%d.%d (server) vs %d.%d.%d (client))",
            inf->out.srv.ver.major, inf->out.srv.ver.minor, inf->out.srv.ver.patch,
            VER_MAJOR, VER_MINOR, VER_PATCH
        );
        goto retfalse;
    }

    uint64_t uid;
    if (setupinf->out.srv.flags & SERVER_COMPATINFO_FLAG_NOAUTH) {
        uid = 0;
        ++task;
    } else if (inf->in.login.new) {
        puts("Requesting new UID...");
        if (inf->in.settext) inf->in.settext("Requesting new UID...", (++task / taskct) * 100.0);
        cliSend(CLIENT_NEWUID, inf->in.login.password);
        time = altutime();
        while (!newuid && !disconnect) {
            if (inf->in.quit && inf->in.quit()) {err[0] = 0; goto retuser;}
            if (altutime() - time > timeout) {
                snprintf(err, errlen, "Timed out waiting for new UID");
                goto retfalse;
            }
        }
        if (disconnect) {
            snprintf(err, errlen, "%s", inf->out.login.failreason);
            goto retfalse;
        }
        printf("New UID from server: %016"PRIX64"\n", setupinf->out.login.uid);
        uid = setupinf->out.login.uid;
    } else {
        uid = setupinf->in.login.uid;
        ++task;
    }
    if (inf->in.settext) inf->in.settext("Logging in...", (++task / taskct) * 100.0);
    puts("Sending login info...");
    printf("- Login code: %016"PRIX64"%016"PRIX64"\n", uid, inf->in.login.password);
    printf("- Username: %s\n", inf->in.login.username);
    cliSend(CLIENT_LOGIN, inf->in.login.flags, uid, inf->in.login.password, inf->in.login.username);
    time = altutime();
    while (!loginok && !disconnect) {
        if (inf->in.quit && inf->in.quit()) {err[0] = 0; goto retuser;}
        if (altutime() - time > timeout) {
            snprintf(err, errlen, "Timed out waiting for login");
            goto retfalse;
        }
    }
    if (disconnect) {
        snprintf(err, errlen, "%s", inf->out.login.failreason);
        goto retfalse;
    }
    printf("Logged into server as %s\n", inf->out.login.username);

    clientalive = false;
    pthread_join(clinetthreadh, NULL);

    callback = cb;
    clientalive = true;
    #ifdef NAME_THREADS
    char name[256];
    char name2[256];
    name[0] = 0;
    name2[0] = 0;
    #endif
    pthread_create(&clinetthreadh, NULL, &clinetthread, NULL);
    #ifdef NAME_THREADS
    pthread_getname_np(clinetthreadh, name2, 256);
    sprintf(name, "%.8s:ct", name2);
    pthread_setname_np(clinetthreadh, name);
    #endif
    return 1;

    retfalse:;
    cliDisconnect();
    return 0;

    retuser:;
    cliDisconnect();
    return -1;
}

void cliDisconnect() {
    clientalive = false;
    pthread_join(clinetthreadh, NULL);
    deinitMsgData(&climsgout);
    closeCxn(clicxn);
    ping = false;
    compatinfo = false;
    newuid = false;
    loginok = false;
    disconnect = false;
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
        } break;
        case CLIENT_NEWUID:; {
            struct client_data_newuid* tmpdata = malloc(sizeof(*tmpdata));
            tmpdata->password = va_arg(args, uint64_t);
            data = tmpdata;
        } break;
        case CLIENT_LOGIN:; {
            struct client_data_login* tmpdata = malloc(sizeof(*tmpdata));
            tmpdata->flags = va_arg(args, int);
            tmpdata->uid = va_arg(args, uint64_t);
            tmpdata->password = va_arg(args, uint64_t);
            tmpdata->username = strdup(va_arg(args, char*));
            data = tmpdata;
        } break;
        case CLIENT_GETCHUNK:; {
            struct client_data_getchunk* tmpdata = malloc(sizeof(*tmpdata));
            tmpdata->x = va_arg(args, int64_t);
            tmpdata->z = va_arg(args, int64_t);
            data = tmpdata;
        } break;
        case CLIENT_SETBLOCK:; {
            struct client_data_setblock* tmpdata = malloc(sizeof(*tmpdata));
            tmpdata->x = va_arg(args, int64_t);
            tmpdata->y = va_arg(args, int);
            tmpdata->z = va_arg(args, int64_t);
            tmpdata->data = va_arg(args, struct blockdata);
            data = tmpdata;
        } break;
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
