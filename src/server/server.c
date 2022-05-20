#include "server.h"
#include <game.h>
#include <chunk.h>
#include <main.h>

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <noise.h>
#include <endian.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

struct servmsg {
    uint32_t uid;
    bool valid;
    bool busy;
    int id;
    void* data;
};

struct servret {
    uint32_t uid;
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

static int server_ackdelay;
static int client_ackdelay;

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

//static int servmode = 0;

static int64_t gxo = 0, gzo = 0;
static int worldtype = 2;

static pthread_t servpthreads[MAX_THREADS];

static void pushRet(int uid, int m, void* d) {
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
        retdata = realloc(retdata, retsize * sizeof(struct servret));
    }
    retdata[index] = (struct servret){uid, m, d};
    //printf("pushRet [%d]/[%d] [0x%016lX]\n", index, retsize, (uintptr_t)retdata[index].data);
    //puts("pushRet: UNLOCKING");
    pthread_mutex_unlock(&retlock);
    //puts("pushRet: UNLOCKED");
}

static void pushMsg(int uid, int m, void* d) {
    pthread_mutex_lock(&msglock);
    switch (m) {
        case SERVER_CMD_SETCHUNK:;
            gxo = ((struct server_msg_chunkpos*)d)->x;
            gzo = ((struct server_msg_chunkpos*)d)->z;
            //printf("Set player pos to [%ld][%ld]\n", gxo, gzo);
            free(d);
            break;
        default:;
            int index = -1;
            for (int i = 0; i < msgsize; ++i) {
                if (!msgdata[i].valid) {index = i; break;}
            }
            if (index == -1) {
                index = msgsize++;
                msgdata = realloc(msgdata, msgsize * sizeof(struct servmsg));
            }
            msgdata[index] = (struct servmsg){uid, true, false, m, d};
    }
    //if (msgsize) printf("pushMsg [%d]/[%d] [0x%016lX]\n", index, msgsize, (uintptr_t)msgdata[index].data);
    pthread_mutex_unlock(&msglock);
}

static void doneMsg(int index) {
    //msgdata[index].ready = true;
    //printf("freed block of [%d]\n", malloc_usable_size(msgdata[index].data));
    free(msgdata[index].data);
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
            pushRet(msgdata[index].uid, retno, retdata);
            doneMsg(index);
        }
        pthread_mutex_unlock(&msglock);
        if (index == -1) if (delaytime > 0 && delaytime <= 33333) microwait(delaytime);
    }
    return NULL;
}

bool initServer() {
    /*
    setRandSeed(0, 347);
    initNoiseTable(0);
    */
    pthread_mutex_init(&msglock, NULL);
    pthread_mutex_init(&cblock, NULL);
    /*
    for (int i = 0; i < SERVER_THREADS && i < MAX_THREADS; ++i) {
        pthread_create(&servpthreads[i], NULL, &servthread, (void*)(intptr_t)i);
    }
    */
    return true;
}

struct servnetinf {
    int socket;
    struct sockaddr_in address;
};

static pthread_t servnetthreadh;

static int maxclients = MAX_CLIENTS;

struct servnetwriteinf {
    struct servnetinf* netinf;
    int* clisock;
};

static pthread_mutex_t writelock;
static pthread_mutex_t acklock;

static bool servackcmd = true;

void* servnetthreadwrite(void* args) {
    struct servnetwriteinf* inf = args;
    unsigned char* buf2 = calloc(SERVER_BUF_SIZE / 2, 1);
    int i = 0;
    while (1) {
        buf2[0] = 'R';
        int msg = SERVER_RET_NONE;
        void* data = NULL;
        //puts("servnetthreadwrite: LOCKING");
        pthread_mutex_lock(&retlock);
        //puts("servnetthreadwrite: LOCKED");
        if (!retsize) {
            //puts("retsize is 0");
            goto cont;
        }
        buf2[1] = msg = retdata[i].msg;
        if (msg != SERVER_RET_NONE) {
            //puts("START");
            pthread_mutex_lock(&writelock);
            //puts("STOP");
            data = retdata[i].data;
            int datasize = 0;
            switch (msg) {
                case SERVER_RET_UPDATECHUNK:;
                    datasize = sizeof(struct server_ret_chunk);
                    break;
            }
            if (data) memcpy(&buf2[2], data, datasize);
            //printf("Pushing task to [%d]\n", retdata[i].uid);
            int tmpsock = inf->clisock[retdata[i].uid];
            pthread_mutex_lock(&acklock);
            servackcmd = false;
            pthread_mutex_unlock(&acklock);
            write(tmpsock, buf2, datasize + 2);
            /*
            int tmpflags = fcntl(tmpsock, F_GETFL);
            fcntl(tmpsock, F_SETFL, tmpflags);
            read(tmpsock, buf2, 2);
            fcntl(tmpsock, F_SETFL, tmpflags | O_NONBLOCK);
            */
            retdata[i].msg = SERVER_RET_NONE;
            if (data) free(data);
            pthread_mutex_unlock(&writelock);
        }
        cont:;
        //puts("servnetthreadwrite: UNLOCKING");
        ++i;
        if (i >= retsize) i = 0;
        pthread_mutex_unlock(&retlock);
        //puts("servnetthreadwrite: UNLOCKED");
        //puts("waiting for ack...");
        pthread_mutex_lock(&acklock);
        while (!servackcmd) {
            pthread_mutex_unlock(&acklock);
            //microwait(1000);
            pthread_mutex_lock(&acklock);
        }
        pthread_mutex_unlock(&acklock);
        //puts("got ack");
        //microwait(10000);
    }
    free(buf2);
}

void* servnetthread(void* args) {
    struct servnetinf* inf = args;
    pthread_mutex_init(&writelock, NULL);
    pthread_mutex_init(&acklock, NULL);
    struct bufinf* buf = allocbuf(SERVER_BUF_SIZE);
    unsigned char* buf2 = calloc(SERVER_BUF_SIZE / 2, 1);
    int* clisock = calloc(maxclients, sizeof(int));
    fd_set fdlist;
    int flags = fcntl(inf->socket, F_GETFL);
    fcntl(inf->socket, F_SETFL, flags | O_NONBLOCK);
    pthread_t netwrite;
    struct servnetwriteinf* writeinf = calloc(1, sizeof(struct servnetwriteinf));
    writeinf->netinf = inf;
    writeinf->clisock = clisock;
    pthread_create(&netwrite, NULL, &servnetthreadwrite, (void*)writeinf);
    socklen_t socklen = sizeof(inf->address);
    while (1) {
        //microwait(10000);
        FD_ZERO(&fdlist);
        FD_SET(inf->socket, &fdlist);
        int fdmax = inf->socket;
        for (int i = 0; i < maxclients; ++i) {
            int tmpfd = clisock[i];
            if (tmpfd > 0) {
                FD_SET(tmpfd, &fdlist);
                if (tmpfd > fdmax) fdmax = tmpfd;
            }
        }
        do {
            errno = 0;
            int status = select(fdmax + 1, &fdlist, NULL, NULL, NULL);
            if (status < 0 && errno != EINTR) {
                fputs("servnetthread: Failed to select socket\n", stderr);
                //return NULL;
            }
        } while (errno == EINTR);
        if (FD_ISSET(inf->socket, &fdlist)) {
            int newsock;
            if ((newsock = accept(inf->socket, (struct sockaddr*)&inf->address, &socklen)) < 0) {
                fputs("servnetthread: Failed to accept socket\n", stderr);
            }
            for (int i = 0; i < maxclients; ++i) {
                if (!clisock[i]) {
                    clisock[i] = newsock;
                    printf("New connection: [%d] {%s:%d} [%d]\n", i, inet_ntoa(inf->address.sin_addr), ntohs(inf->address.sin_port), newsock);
                    goto cont;
                }
            }
            printf("New connection refused due to exceeding maximum clients: {%s:%d}\n", inet_ntoa(inf->address.sin_addr), ntohs(inf->address.sin_port));
            close(newsock);
        }
        cont:;
        for (int i = 0; i < maxclients; ++i) {
            int tmpsock = clisock[i];
            int tmpflags = fcntl(tmpsock, F_GETFL);
            if (FD_ISSET(tmpsock, &fdlist)) {
                fcntl(inf->socket, F_SETFL, flags);
                fcntl(tmpsock, F_SETFL, tmpflags);
                //puts("SERVER GETBUF");
                int msglen = getbuf(tmpsock, buf2, 1, buf);
                //printf("SERVER GOTBUF [%d]\n", msglen);
                getpeername(tmpsock, (struct sockaddr*)&inf->address, &socklen);
                if (msglen > 0) {
                    //printf("server: gotmsg: [%c] [%02X]\n", buf2[0], buf2[0]);
                    switch (buf2[0]) {
                        case 'M':;
                            getbuf(tmpsock, buf2, 1, buf);
                            int msg = buf2[0];
                            //printf("M: [%d]\n", msg);
                            int datasize = 0;
                            switch (msg) {
                                case SERVER_MSG_GETCHUNK:;
                                    //printf("S:R\n");
                                    datasize = sizeof(struct server_msg_chunk);
                                    break;
                                case SERVER_CMD_SETCHUNK:;
                                    //printf("S!\n");
                                    datasize = sizeof(struct server_msg_chunkpos);
                                    break;
                            }
                            unsigned char* data = calloc(datasize, 1);
                            getbuf(tmpsock, data, datasize, buf);
                            //read(tmpsock, data, datasize);
                            pushMsg(i, msg, data);
                            //puts("server: sending A");
                            pthread_mutex_lock(&writelock);
                            //microwait(10000);
                            microwait(server_ackdelay);
                            write(tmpsock, "A", 1);
                            pthread_mutex_unlock(&writelock);
                            break;
                        case 'A':;
                            //puts("server: recieved A");
                            pthread_mutex_lock(&acklock);
                            servackcmd = true;
                            pthread_mutex_unlock(&acklock);
                            break;
                    }
                } else if (msglen < 0) {
                    printf("Closing connection: [%d] {%s:%d} [%d]\n", i, inet_ntoa(inf->address.sin_addr), ntohs(inf->address.sin_port), tmpsock);
                    close(tmpsock);
                    clisock[i] = 0;
                }
                fcntl(tmpsock, F_SETFL, tmpflags | O_NONBLOCK);
                fcntl(inf->socket, F_SETFL, flags | O_NONBLOCK);
            }
        }
    }
    free(buf2);
    free(clisock);
    close(inf->socket);
    return NULL;
}

int servStart(char* addr, int port, char* world, int mcli) {
    if (mcli > 0) maxclients = mcli;
    server_ackdelay = atoi(getConfigVarStatic(config, "server.server_ackdelay", "2000", 64));
    setRandSeed(0, 347);
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
    }
    for (int i = 0; i < SERVER_THREADS && i < MAX_THREADS; ++i) {
        pthread_create(&servpthreads[i], NULL, &servthread, (void*)(intptr_t)i);
    }
    struct servnetinf* inf = calloc(1, sizeof(struct servnetinf));
    inf->socket = socketfd;
    inf->address = address;
    pthread_create(&servnetthreadh, NULL, &servnetthread, (void*)inf);
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
    uint64_t starttime = altutime();
    bool ackcmd = true;
    while (1) {
        //puts("CLIENT GETBUF");
        msglen = getbuf(inf->socket, buf2, 1, buf);
        //printf("CLIENT GOTBUF [%d]\n", msglen);
        if (msglen > 0) {
            //printf("client: gotmsg: [%c] [%02X]\n", buf2[0], buf2[0]);
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
                    }
                    unsigned char* data = calloc(datasize, 1);
                    msglen = getbuf(inf->socket, data, datasize, buf);
                    //printf("Client stage 3 received: {%d}\n", msglen);
                    //fcntl(inf->socket, F_SETFL, flags | O_NONBLOCK);
                    pthread_mutex_lock(&inlock);
                    int index = -1;
                    for (int i = 0; i < insize; ++i) {
                        if (indata[i].msg == SERVER_RET_NONE) {index = i; break;}
                    }
                    if (index == -1) {
                        index = insize++;
                        indata = realloc(indata, insize * sizeof(struct cliret));
                    }
                    indata[index] = (struct cliret){msg, data};
                    //printf("pushCliRet [%d]/[%d] [%d][0x%016lX]\n", index, insize, msg, (uintptr_t)indata[index].data);
                    pthread_mutex_unlock(&inlock);
                    //puts("client: sending A");
                    microwait(client_ackdelay);
                    write(inf->socket, "A", 1);
                    break;
                case 'A':;
                    //puts("client: recieved A");
                    ackcmd = true;
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
                //microwait(100000);
            } else {
                //puts("SEND");
                ackcmd = false;
                msglen = 0;
                buf2[0] = 'M';
                ++msglen;
                buf2[1] = outdata[index].id;
                ++msglen;
                int tmplen = 0;
                switch (outdata[index].id) {
                    case SERVER_MSG_GETCHUNK:;
                        tmplen = sizeof(struct server_msg_chunk);
                        break;
                    case SERVER_CMD_SETCHUNK:;
                        tmplen = sizeof(struct server_msg_chunkpos);
                        break;
                }
                memcpy(&buf2[2], outdata[index].data, tmplen);
                msglen += tmplen;
                //fcntl(inf->socket, F_SETFL, flags);
                write(inf->socket, buf2, msglen);
                //read(inf->socket, buf2, 2);
                //buf2[2] = 0;
                //printf("from server: {%s} [%02X%02X]\n", buf2, buf2[0], buf2[1]);
                //fcntl(inf->socket, F_SETFL, flags | O_NONBLOCK);
                pthread_mutex_lock(&outlock);
                outdata[index].valid = false;
                outdata[index].busy = false;
                pthread_mutex_unlock(&outlock);
            }
            //starttime = altutime();
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
    tmptime.tv_sec = 0;
    tmptime.tv_usec = 10000;
    if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &tmptime, sizeof(tmptime)) < 0 ||
        setsockopt(socketfd, SOL_SOCKET, SO_SNDTIMEO, &tmptime, sizeof(tmptime)) < 0) {
        fputs("servConnect: Failed to set socket options\n", stderr);
        close(socketfd);
        return false;
    }
    //fcntl(socketfd, F_SETFL, fcntl(socketfd, F_GETFL) | O_NONBLOCK);
    client_ackdelay = atoi(getConfigVarStatic(config, "server.client_ackdelay", "500", 64));
    struct servnetinf* inf = calloc(1, sizeof(struct servnetinf));
    inf->socket = socketfd;
    inf->address = server;
    pthread_create(&clinetthreadh, NULL, &clinetthread, (void*)inf);
    return true;
}

void servSend(int msg, void* data, bool f) {
    //printf("Server: Adding task [%d]\n", msgsize);
    bool important;
    switch (msg) {
        case SERVER_CMD_SETCHUNK:;
            important = true;
            break;
        default:;
            important = false;
            break;
    }
    int index = -1;
    pthread_mutex_lock(&outlock);
    for (int i = 0; i < outsize; ++i) {
        if (!outdata[i].valid) {index = i; break;}
    }
    if (index == -1) {
        index = outsize++;
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
