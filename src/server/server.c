#include <main.h>
#include "server.h"
#include <game.h>
#include <chunk.h>

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <noise.h>
#include <endian.h>
//#include <sys/socket.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
//#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
//#include <signal.h>
#include <poll.h>

static struct {
    int unamemax;
    int server_delay;
    int server_idledelay;
    int client_delay;
} cfgvar;

struct servmsg {
    uint64_t pid;
    bool valid;
    bool busy;
    int id;
    void* data;
};

struct servret {
    uint64_t pid;
    int msg;
    void* data;
};

struct climsg {
    bool valid;
    bool busy;
    bool important;
    bool freedata;
    int id;
    void* data;
};

struct cliret {
    int msg;
    void* data;
};

static int msgsize = 0;
static struct servmsg* msgdata;
static pthread_mutex_t msglock;

static int retsize = 0;
static struct servret* retdata;
static pthread_mutex_t retlock;

static int outsize = 0;
static struct climsg* outdata;
static pthread_mutex_t outlock;

static int insize = 0;
static struct cliret* indata;
static pthread_mutex_t inlock;

static pthread_mutex_t cblock;

struct player {
    bool valid;
    uint64_t uid;
    uint64_t pwd;
    int socket;
    char* username;
    int64_t chunkx;
    int64_t chunkz;
    double worldx;
    double worldy;
    double worldz;
};

static struct player* pinfo;
static pthread_mutex_t pinfolock;
static int maxclients = MAX_CLIENTS;

static struct player* getPlayerPtr(uint64_t uid) {
    //printf("[uid:%lu]\n", uid);
    for (int i = 0; i < 1; ++i) {
        //printf("trying [%d]/[%d]\n", i, maxclients);
        if (!pinfo[i].valid) continue;
        if (pinfo[i].uid == uid) return &pinfo[i];
    }
    //printf("could not find\n");
    return NULL;
}

static bool getPlayer(uint64_t uid, struct player* p) {
    pthread_mutex_lock(&pinfolock);
    struct player* tmp = getPlayerPtr(uid);
    if (!tmp) {
        pthread_mutex_unlock(&pinfolock);
        return false;
    }
    *p = *tmp;
    pthread_mutex_unlock(&pinfolock);
    return true;
}

static bool setPlayer(uint64_t uid, struct player* p) {
    pthread_mutex_lock(&pinfolock);
    struct player* tmp = getPlayerPtr(uid);
    if (!tmp) {
        pthread_mutex_unlock(&pinfolock);
        return false;
    }
    *tmp = *p;
    pthread_mutex_unlock(&pinfolock);
    return true;
}

struct bufinf {
    unsigned char* buffer;
    int offset;
    int cached;
    int max;
};

static struct bufinf* allocbuf(int max) {
    struct bufinf* inf = malloc(sizeof(struct bufinf));
    inf->buffer = calloc(max, 1);
    inf->offset = 0;
    inf->cached = 0;
    inf->max = max;
    return inf;
}

static int getbuf(int sockfd, void* _dest, int len, struct bufinf* buf) {
    unsigned char* dest = _dest;
    for (int i = 0; i < len; ++i) {
        if (buf->cached < 1) {
            int ret = read(sockfd, buf->buffer, buf->max);
            if (!ret) {
                return -1;
            } else if (ret < 0) {
                return 0;
            }
            buf->cached = ret;
            buf->offset = 0;
        }
        dest[i] = buf->buffer[buf->offset];
        ++buf->offset;
        --buf->cached;
    }
    return len;
}

static int worldtype = 4;

static pthread_t servpthreads[MAX_THREADS];

static void pushRet(uint64_t pid, int m, void* d) {
    //puts("START");
    //puts("pushRet: LOCKING");
    pthread_mutex_lock(&retlock);
    //puts("pushRet: LOCKED");
    //puts("STOP");
    int index = -1;
    for (int i = 0; i < retsize; ++i) {
        if (retdata[i].msg == SERVER_RET_NONE) {index = i; break;}
    }
    if (index == -1) {
        index = retsize++;
        //printf("resized ret to [%d]\n", retsize);
        retdata = realloc(retdata, retsize * sizeof(struct servret));
    }
    retdata[index] = (struct servret){pid, m, d};
    //printf("pushRet [%d]/[%d] [0x%016lX]\n", index, retsize, (uintptr_t)retdata[index].data);
    //puts("pushRet: UNLOCKING");
    pthread_mutex_unlock(&retlock);
    //puts("pushRet: UNLOCKED");
}

static void pushMsg(uint64_t uid, int m, void* d) {
    //printf("pushMsg: [%lu]\n", uid);
    switch (m) {
        case SERVER_CMD_SETCHUNK:;
            pthread_mutex_lock(&pinfolock);
            struct player* p = getPlayerPtr(uid);
            if (p) {
                p->chunkx = ((struct server_msg_chunkpos*)d)->x;
                p->chunkz = ((struct server_msg_chunkpos*)d)->z;
            }
            pthread_mutex_unlock(&pinfolock);
            //printf("Set player pos to [%ld][%ld]\n", gxo, gzo);
            free(d);
            break;
        default:;
            pthread_mutex_lock(&msglock);
            int index = -1;
            for (int i = 0; i < msgsize; ++i) {
                if (!msgdata[i].valid) {index = i; break;}
            }
            if (index == -1) {
                index = msgsize++;
                //printf("resized msg to [%d]\n", msgsize);
                msgdata = realloc(msgdata, msgsize * sizeof(struct servmsg));
            }
            msgdata[index] = (struct servmsg){uid, true, false, m, d};
            pthread_mutex_unlock(&msglock);
            break;
    }
    //if (msgsize) printf("pushMsg [%d]/[%d] [0x%016lX]\n", index, msgsize, (uintptr_t)msgdata[index].data);
}

static void doneMsg(int index) {
    //msgdata[index].ready = true;
    //printf("freed block of [%d]\n", malloc_usable_size(msgdata[index].data));
    free(msgdata[index].data);
    //printf("doneMsg: [%d]\n", msgdata[index].freedata);
    msgdata[index].valid = false;
    /*
    if (index + 1 == msgsize) {
        --msgsize;
        msgdata = realloc(msgdata, msgsize * sizeof(struct servmsg));
    }
    */
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
            struct player p;
            bool pvalid = getPlayer(msgdata[index].pid, &p);
            //printf("[%d][%lu]\n", pvalid, msgdata[index].pid);
            void* retdata = NULL;
            int retno = SERVER_RET_NONE;
            if (pvalid) {
                switch (msgdata[index].id) {
                    default:; {
                            printf("Server thread [%d]: Recieved invalid task: [%d][%d]\n", id, index, msgdata[index].id);
                        }
                        break;
                    case SERVER_MSG_PING:; {
                            //printf("Server thread [%d]: Pong\n", id);
                            retno = SERVER_RET_PONG;
                        }
                        break;
                    case SERVER_MSG_GETCHUNK:; {
                            if (((struct server_msg_chunk*)msgdata[index].data)->xo != p.chunkx || ((struct server_msg_chunk*)msgdata[index].data)->zo != p.chunkz) break;
                            struct server_ret_chunk* cdata = malloc(sizeof(struct server_ret_chunk));
                            *cdata = (struct server_ret_chunk){
                                .x = ((struct server_msg_chunk*)msgdata[index].data)->x,
                                .y = ((struct server_msg_chunk*)msgdata[index].data)->y,
                                .z = ((struct server_msg_chunk*)msgdata[index].data)->z,
                                .xo = ((struct server_msg_chunk*)msgdata[index].data)->xo,
                                .zo = ((struct server_msg_chunk*)msgdata[index].data)->zo,
                                .id = ((struct server_msg_chunk*)msgdata[index].data)->id,
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
                        }
                        break;
                    case SERVER_MSG_GETCHUNKCOL:; {
                            if (((struct server_msg_chunk*)msgdata[index].data)->xo != p.chunkx || ((struct server_msg_chunk*)msgdata[index].data)->zo != p.chunkz) break;
                            struct server_ret_chunkcol* cdata = malloc(sizeof(struct server_ret_chunkcol));
                            *cdata = (struct server_ret_chunkcol){
                                .x = ((struct server_msg_chunkcol*)msgdata[index].data)->x,
                                .z = ((struct server_msg_chunkcol*)msgdata[index].data)->z,
                                .xo = ((struct server_msg_chunkcol*)msgdata[index].data)->xo,
                                .zo = ((struct server_msg_chunkcol*)msgdata[index].data)->zo,
                                .id = ((struct server_msg_chunkcol*)msgdata[index].data)->id,
                            };
                            for (int y = 0; y < 16; ++y) {
                                genChunk(&((struct server_msg_chunkcol*)msgdata[index].data)->info,
                                         cdata->x,
                                         y,
                                         cdata->z,
                                         ((struct server_msg_chunkcol*)msgdata[index].data)->xo,
                                         ((struct server_msg_chunkcol*)msgdata[index].data)->zo,
                                         cdata->data[y],
                                         worldtype);
                            }
                            retdata = cdata;
                            retno = SERVER_RET_UPDATECHUNKCOL;
                        }
                        break;
                }
            } else {
                puts("invalid user");
            }
            pthread_mutex_lock(&msglock);
            if (pvalid) pushRet(msgdata[index].pid, retno, retdata);
            doneMsg(index);
        }
        pthread_mutex_unlock(&msglock);
        if (index == -1) if (delaytime > 0 && delaytime <= 33333) microwait(delaytime);
    }
    return NULL;
}

bool initServer() {
    pthread_mutex_init(&msglock, NULL);
    pthread_mutex_init(&cblock, NULL);
    return true;
}

struct servnetinf {
    int socket;
    struct sockaddr_in address;
};

static pthread_t servnetthreadh;

void* servnetthread(void* args) {
    struct servnetinf* inf = args;
    //signal(SIGUSR1, usr1handler);
    //pthread_mutex_init(&writelock, NULL);
    //pthread_mutex_init(&acklock, NULL);
    struct bufinf* buf = allocbuf(SERVER_BUF_SIZE);
    unsigned char* buf2 = calloc(SERVER_BUF_SIZE, 1);
    int flags = fcntl(inf->socket, F_GETFL);
    fcntl(inf->socket, F_SETFL, flags | O_NONBLOCK);
    struct timeval tmptime;
    tmptime.tv_sec = 1;
    tmptime.tv_usec = 0;
    setsockopt(inf->socket, SOL_SOCKET, SO_RCVTIMEO, &tmptime, sizeof(tmptime));
    setsockopt(inf->socket, SOL_SOCKET, SO_SNDTIMEO, &tmptime, sizeof(tmptime));
    //pthread_t netwrite;
    //struct servnetwriteinf* writeinf = calloc(1, sizeof(struct servnetwriteinf));
    //writeinf->netinf = inf;
    //writeinf->pinfo = pinfo;
    //pthread_create(&netwrite, NULL, &servnetthreadwrite, (void*)writeinf);
    socklen_t socklen = sizeof(inf->address);
    bool servackcmd = true;
    bool activity = true;
    while (1) {
        microwait((activity) ? cfgvar.server_delay : cfgvar.server_idledelay);
        activity = false;
        int newsock;
        if ((newsock = accept(inf->socket, (struct sockaddr*)&inf->address, &socklen)) >= 0) {
            pthread_mutex_lock(&pinfolock);
            for (int i = 0; i < maxclients; ++i) {
                if (!pinfo[i].valid) {
                    pinfo[i].socket = newsock;
                    //fcntl(newsock, F_SETFL, fcntl(newsock, F_GETFL) | O_NONBLOCK);
                    int tmpint = SERVER_BUF_SIZE;
                    setsockopt(newsock, SOL_SOCKET, SO_SNDBUF, &tmpint, sizeof(tmpint));
                    pinfo[i].uid = getRandQWord(1);
                    printf("New connection: [%d] {%s:%d} [%d] [uid: %lu]\n", i, inet_ntoa(inf->address.sin_addr), ntohs(inf->address.sin_port), newsock, pinfo[i].uid);
                    pinfo[i].valid = true;
                    goto cont;
                }
            }
            printf("New connection refused due to exceeding maximum clients: {%s:%d}\n", inet_ntoa(inf->address.sin_addr), ntohs(inf->address.sin_port));
            close(newsock);
            cont:;
            pthread_mutex_unlock(&pinfolock);
        }
        for (int i = 0; i < maxclients; ++i) {
            pthread_mutex_lock(&pinfolock);
            if (!pinfo[i].valid) {
                pthread_mutex_unlock(&pinfolock);
                continue;
            }
            struct player p = pinfo[i];
            pthread_mutex_unlock(&pinfolock);
            int tmpflags = fcntl(p.socket, F_GETFL);
            //printf("DATA: [%d]\n", i);
            fcntl(inf->socket, F_SETFL, flags);
            fcntl(p.socket, F_SETFL, tmpflags);
            //puts("SERVER GETBUF");
            int msglen = getbuf(p.socket, buf2, 1, buf);
            //printf("SERVER GOTBUF [%d]\n", msglen);
            if (msglen > 0) {
                //if (buf2[0] != 'A') printf("server: gotmsg: [%c] [%02X]\n", buf2[0], buf2[0]);
                //microwait(server_readdelay);
                switch (buf2[0]) {
                    case 'M':;
                        getbuf(p.socket, buf2, 1, buf);
                        int msg = buf2[0];
                        //printf("M: [%d]\n", msg);
                        int datasize = 0;
                        switch (msg) {
                            case SERVER_MSG_GETCHUNK:;
                                //printf("S:R\n");
                                datasize = sizeof(struct server_msg_chunk);
                                break;
                            case SERVER_MSG_GETCHUNKCOL:;
                                //printf("S:R\n");
                                datasize = sizeof(struct server_msg_chunkcol);
                                break;
                            case SERVER_CMD_SETCHUNK:;
                                //printf("S!\n");
                                datasize = sizeof(struct server_msg_chunkpos);
                                break;
                        }
                        unsigned char* data = calloc(datasize, 1);
                        getbuf(p.socket, data, datasize, buf);
                        //read(p.socket, data, datasize);
                        pushMsg(pinfo[i].uid, msg, data);
                        //puts("server: sending A");
                        //pthread_mutex_lock(&writelock);
                        //microwait(10000);
                        //microwait(server_ackdelay);
                        write(p.socket, "A", 1);
                        //pthread_mutex_unlock(&writelock);
                        break;
                    case 'A':;
                        //puts("server: recieved A");
                        //pthread_mutex_lock(&acklock);
                        servackcmd = true;
                        //puts("server: set ack to true");
                        //pthread_mutex_unlock(&acklock);
                        break;
                }
                activity = true;
            } else if (msglen < 0) {
                getpeername(p.socket, (struct sockaddr*)&inf->address, &socklen);
                printf("Closing connection: [%d] {%s:%d} [%d]\n", i, inet_ntoa(inf->address.sin_addr), ntohs(inf->address.sin_port), p.socket);
                close(p.socket);
                pthread_mutex_lock(&pinfolock);
                memset(&pinfo[i], 0, sizeof(struct player));
                pthread_mutex_unlock(&pinfolock);
            }
            fcntl(p.socket, F_SETFL, tmpflags | O_NONBLOCK);
            fcntl(inf->socket, F_SETFL, flags | O_NONBLOCK);
            //} else {
            //    //printf("NO DATA: [%d]\n", i);
            //}
            //pthread_mutex_unlock(&pinfolock);
        }
        if (servackcmd) {
            pthread_mutex_lock(&retlock);
            int msgct = 0;
            static int leftoffi = 0;
            for (int i = leftoffi, j = 0; retsize > 0 /*&& i < retsize*/; ++i, ++j) {
                if (i >= retsize) {
                    i = 0;
                }
                if (j > 0 && i == leftoffi) {
                    break;
                }
                //printf("trying message [%d]/[%d]\n", i + 1, retsize);
                buf2[0] = 'R';
                int msg = SERVER_RET_NONE;
                void* data = NULL;
                buf2[1] = msg = retdata[i].msg;
                if (msg != SERVER_RET_NONE) {
                    //pthread_mutex_lock(&writelock);
                    data = retdata[i].data;
                    int datasize = 0;
                    switch (msg) {
                        case SERVER_RET_UPDATECHUNK:;
                            datasize = sizeof(struct server_ret_chunk);
                            break;
                        case SERVER_RET_UPDATECHUNKCOL:;
                            datasize = sizeof(struct server_ret_chunkcol);
                            break;
                    }
                    if (data) memcpy(&buf2[2], data, datasize);
                    //printf("Pushing task to [%d]\n", retdata[i].uid);
                    struct player tmppi;
                    bool tmpret = getPlayer(retdata[i].pid, &tmppi);
                    if (tmpret) {
                        //printf("server: sendmsg: [%c] [%02X]\n", 'R', 'R');
                        ++msgct;
                        write(tmppi.socket, buf2, datasize + 2);
                        //pthread_mutex_lock(&acklock);
                        servackcmd = false;
                        //puts("server: set ack to false");
                        //pthread_mutex_unlock(&acklock);
                        /*
                        int tmpflags = fcntl(pi.socket, F_GETFL);
                        fcntl(tmppi.socket, F_SETFL, tmpflags);
                        read(tmppi.socket, buf2, 2);
                        fcntl(tmppi.socket, F_SETFL, tmpflags | O_NONBLOCK);
                        */
                    }
                    retdata[i].msg = SERVER_RET_NONE;
                    if (data) free(data);
                    //pthread_mutex_unlock(&writelock);
                    leftoffi = i;
                    break;
                }
            }
            //if (msgct) puts("server: no messages to send");
            pthread_mutex_unlock(&retlock);
            activity = true;
        //} else {
            //puts("server: waiting for ack");
        }
        //puts("LOOP");
    }
    free(buf2);
    free(pinfo);
    close(inf->socket);
    return NULL;
}

int servStart(char* addr, int port, char* world, int mcli) {
    if (mcli > 0) maxclients = mcli;
    cfgvar.server_delay = atoi(getConfigVarStatic(config, "server.server_delay", "1000", 64));
    cfgvar.server_idledelay = atoi(getConfigVarStatic(config, "server.server_idledelay", "50000", 64));
    cfgvar.unamemax =  atoi(getConfigVarStatic(config, "server.unamemax", "32", 64));
    //server_ackdelay = atoi(getConfigVarStatic(config, "server.server_ackdelay", "0", 64));
    //server_readdelay = atoi(getConfigVarStatic(config, "server.server_readdelay", "100", 64));
    setRandSeed(0, 14876);
    initNoiseTable(0);
    setRandSeed(1, altutime() + (uintptr_t)"");
    int socketfd;
    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fputs("servStart: Failed to create socket\n", stderr);
        return -1;
    }
    int opt = 1;
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fputs("servStart: Failed to set socket options\n", stderr);
        close(socketfd);
        return -1;
    }
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = (addr) ? inet_addr(addr) : INADDR_ANY;
    address.sin_port = (port < 0 || port > 0xFFFF) ? htons((port = 46000 + (getRandWord(1) % 1000))) : htons(port);
    if (bind(socketfd, (const struct sockaddr*)&address, sizeof(address))) {
        fputs("servStart: Failed to bind socket\n", stderr);
        close(socketfd);
        return -1;
    }
    if (listen(socketfd, 64)) {
        fputs("servStart: Failed to begin listening\n", stderr);
        close(socketfd);
        return -1;
    }
    pthread_mutex_init(&pinfolock, NULL);
    pinfo = calloc(maxclients, sizeof(struct player));
    #ifdef NAME_THREADS
    char name[256];
    char name2[256];
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
    struct servnetinf* inf = calloc(1, sizeof(struct servnetinf));
    inf->socket = socketfd;
    inf->address = address;
    #ifdef NAME_THREADS
    name[0] = 0;
    name2[0] = 0;
    #endif
    pthread_create(&servnetthreadh, NULL, &servnetthread, (void*)inf);
    #ifdef NAME_THREADS
    pthread_getname_np(servnetthreadh, name2, 256);
    sprintf(name, "%s:st", name2);
    pthread_setname_np(servnetthreadh, name);
    #endif
    return port;
}

static pthread_t clinetthreadh;

void* clinetthread(void* args) {
    struct servnetinf* inf = args;
    struct bufinf* buf = allocbuf(SERVER_BUF_SIZE);
    unsigned char* buf2 = calloc(CLIENT_BUF_SIZE, 1);
    int msglen;
    int flags = fcntl(inf->socket, F_GETFL);
    fcntl(inf->socket, F_SETFL, flags | O_NONBLOCK);
    //uint64_t starttime = altutime();
    bool ackcmd = true;
    while (1) {
        microwait(cfgvar.client_delay);
        //puts("CLIENT GETBUF");
        msglen = getbuf(inf->socket, buf2, 1, buf);
        //printf("CLIENT GOTBUF [%d]\n", msglen);
        if (msglen > 0) {
            //if (buf2[0] != 'A') printf("client: gotmsg: [%c] [%02X]\n", buf2[0], buf2[0]);
            switch (buf2[0]) {
                case 'R':;
                    fcntl(inf->socket, F_SETFL, flags);
                    int datasize = 0;
                    getbuf(inf->socket, buf2, 1, buf);
                    //printf("Client stage 2 received: {%d}\n", buf2[0]);
                    int msg = buf2[0];
                    switch (msg) {
                        case SERVER_RET_UPDATECHUNK:;
                            datasize = sizeof(struct server_ret_chunk);
                            //puts("SERVER_RET_UPDATECHUNK");
                            break;
                        case SERVER_RET_UPDATECHUNKCOL:;
                            datasize = sizeof(struct server_ret_chunkcol);
                            //puts("SERVER_RET_UPDATECHUNKCOL");
                            break;
                    }
                    unsigned char* data = calloc(datasize, 1);
                    msglen = getbuf(inf->socket, data, datasize, buf);
                    //printf("Client stage 3 received: {%d}\n", msglen);
                    fcntl(inf->socket, F_SETFL, flags | O_NONBLOCK);
                    pthread_mutex_lock(&inlock);
                    int index = -1;
                    for (int i = 0; i < insize; ++i) {
                        if (indata[i].msg == SERVER_RET_NONE) {index = i; break;}
                    }
                    if (index == -1) {
                        index = insize++;
                        //printf("resized in to [%d]\n", insize);
                        indata = realloc(indata, insize * sizeof(struct cliret));
                    }
                    indata[index] = (struct cliret){msg, data};
                    //printf("pushCliRet [%d]/[%d] [%d][0x%016lX]\n", index, insize, msg, (uintptr_t)indata[index].data);
                    pthread_mutex_unlock(&inlock);
                    //puts("client: sending A");
                    //microwait(client_ackdelay);
                    write(inf->socket, "A", 1);
                    break;
                case 'A':;
                    //puts("client: recieved A");
                    ackcmd = true;
                    //puts("client: set ack to true");
                    break;
            }
            //buf[msglen] = 0;
        } else if (msglen < 0) {
            puts("Disconnecting from server");
            return NULL;
        }
        //if (altutime() - starttime > 10000) {
        if (ackcmd) {
            pthread_mutex_lock(&outlock);
            int index = -1;
            for (int i = 0; i < outsize; ++i) {
                if (outdata[i].valid && !outdata[i].busy && outdata[i].important) {
                    //printf("![%d]\n", i);
                    index = i;
                    outdata[i].busy = true;
                    break;
                }
            }
            if (index == -1) {
                for (int i = 0; i < outsize; ++i) {
                    if (outdata[i].valid && !outdata[i].busy) {
                        index = i;
                        outdata[i].busy = true;
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&outlock);
            if (index == -1) {
                //puts("client: no messages to send");
                //microwait(100000);
            } else {
                //printf("client: sendmsg: [%c] [%02X]\n", 'M', 'M');
                msglen = 0;
                buf2[0] = 'M';
                ++msglen;
                pthread_mutex_lock(&outlock);
                buf2[1] = outdata[index].id;
                ++msglen;
                int tmplen = 0;
                switch (outdata[index].id) {
                    case SERVER_MSG_GETCHUNK:;
                        tmplen = sizeof(struct server_msg_chunk);
                        break;
                    case SERVER_MSG_GETCHUNKCOL:;
                        tmplen = sizeof(struct server_msg_chunkcol);
                        break;
                    case SERVER_CMD_SETCHUNK:;
                        tmplen = sizeof(struct server_msg_chunkpos);
                        break;
                }
                memcpy(&buf2[2], outdata[index].data, tmplen);
                msglen += tmplen;
                if (outdata[index].freedata) free(outdata[index].data);
                //fcntl(inf->socket, F_SETFL, flags);
                //microwait(client_senddelay);
                //printf("cli writing [%d] bytes (tmplen: [%d])\n", msglen, tmplen);
                write(inf->socket, buf2, msglen);
                ackcmd = false;
                //puts("client: set ack to false");
                //read(inf->socket, buf2, 2);
                //buf2[2] = 0;
                //printf("from server: {%s} [%02X%02X]\n", buf2, buf2[0], buf2[1]);
                //fcntl(inf->socket, F_SETFL, flags | O_NONBLOCK);
                outdata[index].valid = false;
                outdata[index].busy = false;
                pthread_mutex_unlock(&outlock);
            }
            //starttime = altutime();
        } else {
            //puts("client: waiting for ack");
        }
        //}
    }
    close(inf->socket);
    return NULL;
}

bool servConnect(char* addr, int port) {
    printf("Connecting to %s:%d...\n", addr, port);
    int socketfd;
    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fputs("servConnect: Failed to create socket\n", stderr);
        return false;
    }
    struct sockaddr_in server;
    struct sockaddr_in client;
    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(addr);
    server.sin_port = htons(port);
    struct timeval tmptime;
    tmptime.tv_sec = 15;
    tmptime.tv_usec = 0;
    if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &tmptime, sizeof(tmptime)) < 0 ||
        setsockopt(socketfd, SOL_SOCKET, SO_SNDTIMEO, &tmptime, sizeof(tmptime)) < 0) {
        fputs("servConnect: Failed to set socket options\n", stderr);
        close(socketfd);
        return false;
    }
    if (connect(socketfd, (struct sockaddr*)&server, sizeof(server))) {
        fputs("servConnect: Failed to connect to server\n", stderr);
        close(socketfd);
        return false;
    }
    tmptime.tv_sec = 1;
    tmptime.tv_usec = 0;
    if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &tmptime, sizeof(tmptime)) < 0 ||
        setsockopt(socketfd, SOL_SOCKET, SO_SNDTIMEO, &tmptime, sizeof(tmptime)) < 0) {
        fputs("servConnect: Failed to set socket options\n", stderr);
        close(socketfd);
        return false;
    }
    {
        int tmpint = SERVER_BUF_SIZE;
        setsockopt(socketfd, SOL_SOCKET, SO_RCVBUF, &tmpint, sizeof(tmpint));
    }
    //fcntl(socketfd, F_SETFL, fcntl(socketfd, F_GETFL) | O_NONBLOCK);
    cfgvar.client_delay = atoi(getConfigVarStatic(config, "server.client_delay", "1000", 64));
    //client_ackdelay = atoi(getConfigVarStatic(config, "server.client_ackdelay", "0", 64));
    //client_senddelay = atoi(getConfigVarStatic(config, "server.client_senddelay", "0", 64));
    struct servnetinf* inf = calloc(1, sizeof(struct servnetinf));
    inf->socket = socketfd;
    inf->address = server;
    #ifdef NAME_THREADS
    char name[256];
    char name2[256];
    name[0] = 0;
    name2[0] = 0;
    #endif
    pthread_create(&clinetthreadh, NULL, &clinetthread, (void*)inf);
    #ifdef NAME_THREADS
    pthread_getname_np(clinetthreadh, name2, 256);
    sprintf(name, "%s:ct", name2);
    pthread_setname_np(clinetthreadh, name);
    #endif
    return true;
}

void servSend(int msg, void* data, bool f) {
    //printf("Server: Adding task [%d]\n", msgsize);
    static struct server_msg_chunkpos cpos;
    bool important;
    if (msg >= 128) {
        important = true;
    } else {
        important = false;
    }
    switch (msg) {
        case SERVER_CMD_SETCHUNK:;
            cpos = *(struct server_msg_chunkpos*)data;
            break;
        case SERVER_MSG_GETCHUNK:;
            if (((struct server_msg_chunk*)data)->xo != cpos.x || ((struct server_msg_chunk*)data)->zo != cpos.z) {
                if (f) free(data);
                return;
            }
            break;
        case SERVER_MSG_GETCHUNKCOL:;
            if (((struct server_msg_chunkcol*)data)->xo != cpos.x || ((struct server_msg_chunkcol*)data)->zo != cpos.z) {
                if (f) free(data);
                return;
            }
            break;
    }
    int index = -1;
    pthread_mutex_lock(&outlock);
    for (int i = 0; i < outsize; ++i) {
        if (!outdata[i].valid) {index = i; break;}
    }
    if (index == -1) {
        index = outsize++;
        //printf("resized out to [%d]\n", outsize);
        outdata = realloc(outdata, outsize * sizeof(struct climsg));
    }
    outdata[index] = (struct climsg){.valid = true, .busy = false, .important = important, .freedata = f, .id = msg, .data = data};
    pthread_mutex_unlock(&outlock);
}

void servRecv(void (*callback)(int, void*), int msgs) {
    int ct = 0;
    pthread_mutex_lock(&inlock);
    static int index = 0;
    int iend = index;
    //printf("running max of [%d] messages\n", msgs);
    if (!insize) {/*printf("No messages\n");*/ goto _ret;}
    while (ct < msgs) {
        int msg = SERVER_RET_NONE;
        void* data = NULL;
        msg = indata[index].msg;
        //if (msg) printf("passing [%d]: [%d]\n", index, msg);
        if (msg != SERVER_RET_NONE) {
            ++ct;
            data = indata[index].data;
            callback(msg, data);
            indata[index].msg = SERVER_RET_NONE;
            if (data) {
                //printf("freed block of [%d]\n", malloc_usable_size(data));
                free(data);
            }
        }
        //printf("index: [%d]\n", index);
        ++index;
        if (index >= insize) index = 0;
        if (index == iend) break;
    }
    //if (ct) printf("ran [%d] messages\n", ct);
    _ret:;
    pthread_mutex_unlock(&inlock);
}
