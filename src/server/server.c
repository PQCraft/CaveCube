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
    bool ready;
    bool busy;
    //bool discard;
    bool freedata;
    int id;
    void* data;
    void* callback;
};

static int msgsize = 0;
static struct servmsg* msgdata;
static pthread_mutex_t msglock;

static pthread_mutex_t cblock;

static int servmode = 0;

static int gxo = 0, gzo = 0;
static int worldtype = 0;

pthread_t servpthreads[MAX_THREADS];

static void* servthread(void* args) {
    int id = (intptr_t)args;
    printf("Server: Started thread [%d]\n", id);
    while (1) {
        uint64_t starttime = altutime();
        uint64_t delaytime = 33333 - (altutime() - starttime);
        bool nocb = false;
        pthread_mutex_lock(&msglock);
        int index = -1;
        for (int i = 0; i < msgsize; ++i) {
            if (msgdata[i].valid && !msgdata[i].busy) {index = i; /*printf("Server: executing task [%d]\n", i);*/ break;}
        }
        if (index == -1) {
            //puts("Server: No tasks");
        } else {
            msgdata[index].busy = true;
            pthread_mutex_unlock(&msglock);
            switch (msgdata[index].id) {
                default:;
                    printf("Server thread [%d]: Recieved invalid task: [%d][%d]\n", id, index, msgdata[index].id);
                    break;
                case SERVER_MSG_PING:;
                    printf("Server thread [%d]: Pong\n", id);
                    break;
                case SERVER_MSG_GETCHUNK:;
                    pthread_mutex_lock(&msglock);
                    bool tmp00 = (((struct server_chunk*)msgdata[index].data)->xo != gxo || ((struct server_chunk*)msgdata[index].data)->zo != gzo);
                    pthread_mutex_unlock(&msglock);
                    if (tmp00) {
                        /*
                        printf("Server thread [%d]: GETCHUNK: DROPPING [%d][%d][%d]\n", id,
                               ((struct server_chunk*)msgdata[index].data)->x + ((struct server_chunk*)msgdata[index].data)->xo,
                               ((struct server_chunk*)msgdata[index].data)->y,
                               ((struct server_chunk*)msgdata[index].data)->z + ((struct server_chunk*)msgdata[index].data)->zo);
                        */
                        nocb = true; break;
                    }
                    /*
                    printf("Server thread [%d]: GETCHUNK: GENERATING [%d][%d][%d]\n", id,
                           ((struct server_chunk*)msgdata[index].data)->x + ((struct server_chunk*)msgdata[index].data)->xo,
                           ((struct server_chunk*)msgdata[index].data)->y,
                           ((struct server_chunk*)msgdata[index].data)->z + ((struct server_chunk*)msgdata[index].data)->zo);
                    */
                    genChunk(((struct server_chunk*)msgdata[index].data)->chunks,
                             ((struct server_chunk*)msgdata[index].data)->x,
                             ((struct server_chunk*)msgdata[index].data)->y,
                             ((struct server_chunk*)msgdata[index].data)->z,
                             ((struct server_chunk*)msgdata[index].data)->xo,
                             ((struct server_chunk*)msgdata[index].data)->zo,
                             ((struct server_chunk*)msgdata[index].data)->data,
                             worldtype);
                    /*
                    printf("Server thread [%d]: GETCHUNK: SENDING [%d][%d][%d]\n", id,
                           ((struct server_chunk*)msgdata[index].data)->x + ((struct server_chunk*)msgdata[index].data)->xo,
                           ((struct server_chunk*)msgdata[index].data)->y,
                           ((struct server_chunk*)msgdata[index].data)->z + ((struct server_chunk*)msgdata[index].data)->zo);
                    */
                    break;
            }
            pthread_mutex_lock(&msglock);
            msgdata[index].ready = true;
            if (!nocb && msgdata[index].callback) {
                void (*cbptr)(void*) = msgdata[index].callback;
                pthread_mutex_lock(&cblock);
                cbptr(msgdata[index].data);
                pthread_mutex_unlock(&cblock);
            }
            /*if (msgdata[index].discard) */msgdata[index].valid = false;
            if (msgdata[index].freedata) free(msgdata[index].data);
        }
        pthread_mutex_unlock(&msglock);
        if (index == -1) if (delaytime > 0 && delaytime <= 33333) microwait(delaytime);
    }
    return NULL;
}

bool initServer(int mode) {
    servmode = mode;
    setRandSeed(9135);
    initNoiseTable();
    msgdata = malloc(0);
    pthread_mutex_init(&msglock, NULL);
    pthread_mutex_init(&cblock, NULL);
    for (int i = 0; i < SERVER_THREADS && i < MAX_THREADS; ++i) {
        pthread_create(&servpthreads[i], NULL, &servthread, (void*)(intptr_t)i);
    }
    return true;
}

bool servMsgReady(int index) {
    pthread_mutex_lock(&msglock);
    bool ready = msgdata[index].ready;
    pthread_mutex_unlock(&msglock);
    return ready;
}

static int pushMsg(int m, void* d, /*bool dsc, */bool f, void* cb) {
    int index = -1;
    pthread_mutex_lock(&msglock);
    switch (m) {
        case SERVER_CMD_SETCHUNK:;
            gxo = ((struct server_chunkpos*)d)->x;
            gzo = ((struct server_chunkpos*)d)->z;
            break;
        default:;
            for (int i = 0; i < msgsize; ++i) {
                if (!msgdata[i].valid) {index = i; break;}
            }
            if (index == -1) {
                index = msgsize++;
                msgdata = realloc(msgdata, msgsize * sizeof(struct servmsg));
            }
            msgdata[index] = (struct servmsg){true, false, false, /*dsc, */f, m, d, cb};
    }
    //printf("pushMsg [%lu]\n", (uintptr_t)msgdata[index].data);
    pthread_mutex_unlock(&msglock);
    return index;
}

int servSend(int msg, void* data, /*bool dsc,*/ bool f, void* cb) {
    //printf("Server: Adding task [%d]\n", msgsize);
    return pushMsg(msg, data, /*dsc, */f, cb);
}
