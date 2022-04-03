#include "server.h"
#include <game.h>
#include <chunk.h>

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>

#ifndef SERVER_THREADS
    #define SERVER_THREADS 4
#endif

struct servmsg {
    bool valid;
    bool ready;
    bool busy;
    bool discard;
    bool freedata;
    int id;
    void* data;
};

static int msgsize = 0;
struct servmsg* msgdata;
pthread_mutex_t msglock;

static int servmode = 0;

pthread_t servpthreads[MAX_THREADS];

static void* servthread(void* args) {
    int id = (intptr_t)args;
    printf("Server: Started thread [%d]\n", id);
    while (1) {
        uint64_t starttime = altutime();
        uint64_t delaytime = 33333 - (altutime() - starttime);
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
                    printf("Server thread [%d]: Recieved invalid task\n", id);
                    break;
                case SERVER_MSG_PING:;
                    printf("Server thread [%d]: Pong\n", id);
                    break;
                case SERVER_MSG_GETCHUNK:;
                    //printf("Server thread [%d]: GETCHUNK\n", id);
                    genChunkColumn(((struct server_chunk*)msgdata[index].data)->data,
                                   ((struct server_chunk*)msgdata[index].data)->x,
                                   ((struct server_chunk*)msgdata[index].data)->z,
                                   ((struct server_chunk*)msgdata[index].data)->xo,
                                   ((struct server_chunk*)msgdata[index].data)->zo);
                    break;
            }
            pthread_mutex_lock(&msglock);
            msgdata[index].ready = true;
            if (msgdata[index].discard) msgdata[index].valid = false;
            //if (msgdata[index].freedata) free(msgdata[index].data);
        }
        pthread_mutex_unlock(&msglock);
        if (index == -1) if (delaytime > 0 && delaytime <= 33333) microwait(delaytime);
    }
    return NULL;
}

bool initServer(int mode) {
    servmode = mode;
    msgdata = malloc(0);
    pthread_mutex_init(&msglock, NULL);
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

static int pushMsg(int m, void* d, bool dsc, bool f) {
    int index = -1;
    pthread_mutex_lock(&msglock);
    for (int i = 0; i < msgsize; ++i) {
        if (!msgdata[i].valid) {index = i; break;}
    }
    if (index == -1) {
        index = msgsize++;
        msgdata = realloc(msgdata, msgsize * sizeof(struct servmsg));
    }
    msgdata[index] = (struct servmsg){true, false, false, dsc, f, m, d};
    //printf("pushMsg [%lu]\n", (uintptr_t)msgdata[index].data);
    pthread_mutex_unlock(&msglock);
    return index;
}

int servSend(int msg, void* data, bool dsc, bool f) {
    //printf("Server: Adding task [%d]\n", msgsize);
    return pushMsg(msg, data, dsc, f);
}
