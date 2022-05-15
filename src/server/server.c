#include "server.h"
#include <game.h>
#include <chunk.h>

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <noise.h>

struct servmsg {
    bool valid;
    //bool ready;
    bool busy;
    //bool discard;
    bool freedata;
    int id;
    void* data;
};

struct servret {
    int msg;
    void* data;
};

static int msgsize = 0;
static struct servmsg* msgdata;
static pthread_mutex_t msglock;

static int retsize = 0;
static struct servret* retdata;
static pthread_mutex_t retlock;

static pthread_mutex_t cblock;

static int servmode = 0;

static int gxo = 0, gzo = 0;
static int worldtype = 2;

pthread_t servpthreads[MAX_THREADS];

static void pushRet(int m, void* d) {
    pthread_mutex_lock(&retlock);
    int index = -1;
    for (int i = 0; i < retsize; ++i) {
        if (retdata[i].msg == SERVER_RET_NONE) {index = i; break;}
    }
    if (index == -1) {
        index = retsize++;
        retdata = realloc(retdata, retsize * sizeof(struct servret));
    }
    retdata[index] = (struct servret){m, d};
    //printf("pushRet [%d]/[%d] [0x%016lX]\n", index, retsize, (uintptr_t)retdata[index].data);
    pthread_mutex_unlock(&retlock);
}

static void doneMsg(int index) {
    //msgdata[index].ready = true;
    if (msgdata[index].freedata) {
        //printf("freed block of [%d]\n", malloc_usable_size(msgdata[index].data));
        free(msgdata[index].data);
    }
    //printf("doneMsg: [%d]\n", msgdata[index].freedata);
    msgdata[index].valid = false;
    if (index + 1 == msgsize) {
        --msgsize;
        msgdata = realloc(msgdata, msgsize * sizeof(struct servmsg));
    }
}

static void* servthread(void* args) {
    int id = (intptr_t)args;
    printf("Server: Started thread [%d]\n", id);
    while (1) {
        uint64_t starttime = altutime();
        uint64_t delaytime = 33333 - (altutime() - starttime);
        pthread_mutex_lock(&msglock);
        int index = -1;
        for (int i = 0; i < msgsize; ++i) {
            if (msgdata[i].valid && !msgdata[i].busy) {index = i; /*printf("Server: executing task [%d]: [%d]\n", i, msgdata[index].id);*/ break;}
        }
        if (index == -1) {
            //puts("Server: No tasks");
        } else {
            msgdata[index].busy = true;
            pthread_mutex_unlock(&msglock);
            void* retdata = NULL;
            int retno = SERVER_RET_NONE;
            switch (msgdata[index].id) {
                default:; {
                        printf("Server thread [%d]: Recieved invalid task: [%d][%d]\n", id, index, msgdata[index].id);
                        break;
                    }
                case SERVER_MSG_PING:; {
                        //printf("Server thread [%d]: Pong\n", id);
                        retno = SERVER_RET_PONG;
                        break;
                    }
                case SERVER_MSG_GETCHUNK:; {
                        pthread_mutex_lock(&msglock);
                        bool tmp00 = (((struct server_msg_chunk*)msgdata[index].data)->xo != gxo || ((struct server_msg_chunk*)msgdata[index].data)->zo != gzo);
                        pthread_mutex_unlock(&msglock);
                        if (tmp00) {
                            /*
                            printf("Server thread [%d]: GETCHUNK: DROPPING [%d][%d][%d]\n", id,
                                   ((struct server_msg_chunk*)msgdata[index].data)->x + ((struct server_msg_chunk*)msgdata[index].data)->xo,
                                   ((struct server_msg_chunk*)msgdata[index].data)->y,
                                   ((struct server_msg_chunk*)msgdata[index].data)->z + ((struct server_msg_chunk*)msgdata[index].data)->zo);
                            */
                            break;
                        }
                        /*
                        printf("Server thread [%d]: GETCHUNK: GENERATING [%d][%d][%d]\n", id,
                               ((struct server_msg_chunk*)msgdata[index].data)->x + ((struct server_msg_chunk*)msgdata[index].data)->xo,
                               ((struct server_msg_chunk*)msgdata[index].data)->y,
                               ((struct server_msg_chunk*)msgdata[index].data)->z + ((struct server_msg_chunk*)msgdata[index].data)->zo);
                        */
                        //break;
                        struct server_ret_chunk* cdata = malloc(sizeof(struct server_ret_chunk));
                        //printf("alloced block of [%d]\n", malloc_usable_size(cdata));
                        *cdata = (struct server_ret_chunk){
                            .id = ((struct server_msg_chunk*)msgdata[index].data)->id,
                            .x = ((struct server_msg_chunk*)msgdata[index].data)->x,
                            .y = ((struct server_msg_chunk*)msgdata[index].data)->y,
                            .z = ((struct server_msg_chunk*)msgdata[index].data)->z,
                        };
                        genChunk(&((struct server_msg_chunk*)msgdata[index].data)->info,
                                 cdata->x,
                                 cdata->y,
                                 cdata->z,
                                 ((struct server_msg_chunk*)msgdata[index].data)->xo,
                                 ((struct server_msg_chunk*)msgdata[index].data)->zo,
                                 cdata->data,
                                 worldtype);
                        retdata = cdata;
                        retno = SERVER_RET_UPDATECHUNK;
                        /*
                        printf("Server thread [%d]: GETCHUNK: SENDING [%d][%d][%d]\n", id,
                               ((struct server_msg_chunk*)msgdata[index].data)->x + ((struct server_msg_chunk*)msgdata[index].data)->xo,
                               ((struct server_msg_chunk*)msgdata[index].data)->y,
                               ((struct server_msg_chunk*)msgdata[index].data)->z + ((struct server_msg_chunk*)msgdata[index].data)->zo);
                        */
                        break;
                    }
            }
            pthread_mutex_lock(&msglock);
            pushRet(retno, retdata);
            doneMsg(index);
        }
        pthread_mutex_unlock(&msglock);
        if (index == -1) if (delaytime > 0 && delaytime <= 33333) microwait(delaytime);
    }
    return NULL;
}

bool initServer(int mode) {
    servmode = mode;
    setRandSeed(0xDEADBEEF);
    initNoiseTable();
    pthread_mutex_init(&msglock, NULL);
    pthread_mutex_init(&cblock, NULL);
    for (int i = 0; i < SERVER_THREADS && i < MAX_THREADS; ++i) {
        pthread_create(&servpthreads[i], NULL, &servthread, (void*)(intptr_t)i);
    }
    return true;
}

static void pushMsg(int m, void* d, /*bool dsc, */bool f) {
    int index = -1;
    pthread_mutex_lock(&msglock);
    switch (m) {
        case SERVER_CMD_SETCHUNK:;
            gxo = ((struct server_msg_chunkpos*)d)->x;
            gzo = ((struct server_msg_chunkpos*)d)->z;
            if (f) {
                //printf("freed block of [%d]\n", malloc_usable_size(d));
                free(d);
            }
            break;
        default:;
            for (int i = 0; i < msgsize; ++i) {
                if (!msgdata[i].valid) {index = i; break;}
            }
            if (index == -1) {
                index = msgsize++;
                msgdata = realloc(msgdata, msgsize * sizeof(struct servmsg));
            }
            msgdata[index] = (struct servmsg){true, /*false, */false, /*dsc, */f, m, d};
    }
    //if (msgsize) printf("pushMsg [%d]/[%d] [0x%016lX]\n", index, msgsize, (uintptr_t)msgdata[index].data);
    pthread_mutex_unlock(&msglock);
}

void servSend(int msg, void* data, /*bool dsc,*/ bool f) {
    //printf("Server: Adding task [%d]\n", msgsize);
    pushMsg(msg, data, /*dsc, */f);
}

void servRecv(void (*callback)(int, void*), int msgs) {
    int ct = 0;
    pthread_mutex_lock(&retlock);
    static int index = 0;
    int iend = index;
    //printf("running max of [%d] messages\n", msgs);
    if (!retsize) {/*printf("No messages\n");*/ goto _ret;}
    while (ct < msgs) {
        int msg = SERVER_RET_NONE;
        void* data = NULL;
        msg = retdata[index].msg;
        if (msg != SERVER_RET_NONE) {
            ++ct;
            data = retdata[index].data;
            callback(msg, data);
            retdata[index].msg = SERVER_RET_NONE;
            if (data) {
                //printf("freed block of [%d]\n", malloc_usable_size(data));
                free(data);
            }
        }
        //printf("index: [%d]\n", index);
        ++index;
        if (index >= retsize) index = 0;
        if (index == iend) break;
    }
    //if (ct) printf("ran [%d] messages\n", ct);
    _ret:;
    pthread_mutex_unlock(&retlock);
}
