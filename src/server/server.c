#include <main.h>
#include "server.h"
#include <game.h>
#include <worldgen.h>
#include <noise.h>

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifndef _WIN32
    #include <sys/fcntl.h>
    #include <arpa/inet.h>
    #include <endian.h>
    #define PRIsock "d"
    #define SOCKINVAL(s) ((s) < 0)
#else
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #define PRIsock PRIu64
    #define SOCKINVAL(s) ((s) == INVALID_SOCKET)
    static WSADATA wsadata;
    static WORD wsaver = MAKEWORD(2, 2);
    static bool wsainit = false;
#endif

#ifdef _WIN32
bool startwsa() {
    if (wsainit) return true;
    int wsaerror;
    if ((wsaerror = WSAStartup(wsaver, &wsadata))) {
        fprintf(stderr, "WSA init failed: [%d]\n", wsaerror);
        return false;
    }
    wsainit = true;
    return true;
}
#endif

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

int SERVER_THREADS;

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

struct bufinf {
    unsigned char* buffer;
    int offset;
    int cached;
    int max;
};

#ifndef _WIN32
    typedef int sock_t;
#else
    typedef SOCKET sock_t;
#endif

struct player {
    bool valid;
    bool login;
    bool ack;
    uint64_t uid;
    uint64_t pwd;
    struct bufinf* buf;
    bool admin;
    bool fly;
    uint8_t mode;
    sock_t socket;
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
    //printf("[uid:0x%lX]\n", uid);
    for (int i = 0; i < maxclients; ++i) {
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

static struct bufinf* allocbuf(int max) {
    struct bufinf* inf = malloc(sizeof(struct bufinf));
    inf->buffer = calloc(max, 1);
    inf->offset = 0;
    inf->cached = 0;
    inf->max = max;
    return inf;
}

void freebuf(struct bufinf* buf) {
    free(buf->buffer);
    free(buf);
}

int rsock(sock_t sock, void* buf, int len) {
    #ifndef _WIN32
    return read(sock, buf, len);
    #else
    int ret = recv(sock, buf, len, 0);
    if (ret == SOCKET_ERROR) return -1;
    return ret;
    #endif
}

void wsock(sock_t sock, void* buf, int len) {
    #ifndef _WIN32
    write(sock, buf, len);
    #else
    send(sock, buf, len, 0);
    #endif
}

static int getbuf(sock_t sockfd, void* _dest, int len, struct bufinf* buf) {
    unsigned char* dest = _dest;
    for (int i = 0; i < len; ++i) {
        if (buf->cached < 1) {
            //puts("READ");
            int ret = rsock(sockfd, buf->buffer, buf->max);
            //printf("RET [%d]\n", ret);
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

static int worldtype = 1;

static pthread_t servpthreads[MAX_THREADS];

static void pushRet(uint64_t pid, int m, void* d) {
    //printf("ret for [0x%lX]\n", pid);
    pthread_mutex_lock(&retlock);
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
    pthread_mutex_unlock(&retlock);
}

static void pushMsg(uint64_t uid, int m, void* d) {
    //printf("pushMsg: [0x%lX]\n", uid);
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
            if (msgdata[i].valid && !msgdata[i].busy) {index = i; /*printf("Server: executing task [%d]: [%d] for [0x%lX]\n", i, msgdata[index].id, msgdata[index].pid);*/ break;}
        }
        if (index == -1) {
            //puts("Server: No tasks");
        } else {
            msgdata[index].busy = true;
            pthread_mutex_unlock(&msglock);
            struct player p;
            bool pvalid = getPlayer(msgdata[index].pid, &p);
            //printf("[%d][0x%lX]\n", pvalid, msgdata[index].pid);
            void* retdata = NULL;
            int retno = SERVER_RET_NONE;
            if (pvalid) {
                switch (msgdata[index].id) {
                    default:; {
                            printf("Server thread [%d]: Recieved invalid task: [%d][%d] from [0x%"PRIX64"]\n", id, index, msgdata[index].id, p.uid);
                        }
                        break;
                    case SERVER_MSG_PING:; {
                            //printf("Server thread [%d]: Pong [0x%"PRIX64"]\n", id, p.uid);
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
    return true;
}

struct servnetinf {
    sock_t socket;
    struct sockaddr_in address;
};

static pthread_t servnetthreadh;

void* servnetthread(void* args) {
    struct servnetinf* inf = args;
    unsigned char* buf2 = calloc(SERVER_BUF_SIZE, 1);
    #ifndef _WIN32
    int flags = fcntl(inf->socket, F_GETFL);
    fcntl(inf->socket, F_SETFL, flags | O_NONBLOCK);
    #else
    {unsigned long o = 1; ioctlsocket(inf->socket, FIONBIO, &o);}
    #endif
    #ifndef _WIN32
    struct timeval tmptime = (struct timeval){.tv_sec = 1, .tv_usec = 0};
    setsockopt(inf->socket, SOL_SOCKET, SO_RCVTIMEO, &tmptime, sizeof(tmptime));
    setsockopt(inf->socket, SOL_SOCKET, SO_SNDTIMEO, &tmptime, sizeof(tmptime));
    #else
    DWORD tmptime = 1000;
    setsockopt(inf->socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tmptime, sizeof(tmptime));
    setsockopt(inf->socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tmptime, sizeof(tmptime));
    #endif
    socklen_t socklen = sizeof(inf->address);
    bool activity = true;
    while (1) {
        microwait((activity) ? cfgvar.server_delay : cfgvar.server_idledelay);
        activity = false;
        sock_t newsock;
        #ifndef _WIN32
        if ((newsock = accept(inf->socket, (struct sockaddr*)&inf->address, &socklen)) >= 0) {
        #else
        if ((newsock = accept(inf->socket, (struct sockaddr*)&inf->address, &socklen)) != INVALID_SOCKET) {
        #endif
            pthread_mutex_lock(&pinfolock);
            for (int i = 0; i < maxclients; ++i) {
                if (!pinfo[i].valid) {
                    int tmpint = SERVER_BUF_SIZE;
                    #ifndef _WIN32
                    setsockopt(newsock, SOL_SOCKET, SO_SNDBUF, &tmpint, sizeof(tmpint));
                    #else
                    setsockopt(newsock, SOL_SOCKET, SO_SNDBUF, (const char*)&tmpint, sizeof(tmpint));
                    #endif
                    memset(&pinfo[i], 0, sizeof(struct player));
                    pinfo[i] = (struct player){.valid = true, .socket = newsock, .uid = getRandQWord(1), .buf = allocbuf(SERVER_BUF_SIZE), .ack = true};
                    printf("New connection: [%d] {%s:%d} [%"PRIsock"] [uid: 0x%"PRIX64"]\n", i, inet_ntoa(inf->address.sin_addr), ntohs(inf->address.sin_port), newsock, pinfo[i].uid);
                    goto cont;
                }
            }
            printf("New connection refused due to exceeding maximum clients: {%s:%d}\n", inet_ntoa(inf->address.sin_addr), ntohs(inf->address.sin_port));
            close(newsock);
            cont:;
            pthread_mutex_unlock(&pinfolock);
        }
        for (int pn = 0; pn < maxclients; ++pn) {
            pthread_mutex_lock(&pinfolock);
            if (!pinfo[pn].valid) {
                pthread_mutex_unlock(&pinfolock);
                continue;
            }
            //printf("doing [%d]\n", pn);
            struct player p = pinfo[pn];
            pthread_mutex_unlock(&pinfolock);
            //printf("DATA: [%d]\n", pn);
            #ifndef _WIN32
            int tmpflags = fcntl(p.socket, F_GETFL);
            fcntl(inf->socket, F_SETFL, flags);
            fcntl(p.socket, F_SETFL, tmpflags);
            #else
            //{unsigned long o = 0; ioctlsocket(inf->socket, FIONBIO, &o); ioctlsocket(p.socket, FIONBIO, &o);}
            #endif
            //puts("SERVER GETBUF");
            int msglen = getbuf(p.socket, buf2, 1, p.buf);
            //printf("SERVER GOTBUF [%d]\n", msglen);
            if (msglen > 0) {
                //if (buf2[0] != 'A') printf("server: gotmsg: [%c] [%02X] from [0x%"PRIX64"]\n", buf2[0], buf2[0], p.uid);
                //microwait(server_readdelay);
                switch (buf2[0]) {
                    case 'M':;
                        getbuf(p.socket, buf2, 1, p.buf);
                        int msg = buf2[0];
                        //printf("M: [%d] [0x%"PRIX64"]\n", msg, p.uid);
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
                        getbuf(p.socket, data, datasize, p.buf);
                        //rsock(p.socket, data, datasize);
                        pushMsg(pinfo[pn].uid, msg, data);
                        //printf("server: sending A to 0x%"PRIX64"\n", pinfo[pn].uid);
                        wsock(p.socket, "A", 1);
                        break;
                    case 'A':;
                        //printf("server: recieved A from 0x%lX\n", pinfo[pn].uid);
                        pthread_mutex_lock(&pinfolock);
                        if (pinfo[pn].valid) {
                            pinfo[pn].ack = true;
                            p.ack = true;
                        }
                        pthread_mutex_unlock(&pinfolock);
                        //puts("server: set ack to true");
                        break;
                }
                activity = true;
            } else if (msglen < 0) {
                pthread_mutex_lock(&pinfolock);
                getpeername(p.socket, (struct sockaddr*)&inf->address, &socklen);
                printf("Closing connection: [%d] {%s:%d} [%"PRIsock"]\n", pn, inet_ntoa(inf->address.sin_addr), ntohs(inf->address.sin_port), p.socket);
                close(p.socket);
                freebuf(p.buf);
                memset(&pinfo[pn], 0, sizeof(struct player));
                pthread_mutex_unlock(&pinfolock);
            }
            #ifndef _WIN32
            fcntl(p.socket, F_SETFL, tmpflags | O_NONBLOCK);
            fcntl(inf->socket, F_SETFL, flags | O_NONBLOCK);
            #else
            //{unsigned long o = 1; ioctlsocket(inf->socket, FIONBIO, &o); ioctlsocket(p.socket, FIONBIO, &o);}
            #endif
            //pthread_mutex_unlock(&pinfolock);
            if (p.ack) {
                pthread_mutex_lock(&retlock);
                //printf("msg for [%d]\n", pn);
                int msgct = 0;
                static int leftoffi = 0;
                for (int i = leftoffi, j = 0; retsize > 0; ++i, ++j) {
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
                        //printf("Pushing message to [0x%lX] (chk [0x%lX])\n", retdata[i].pid, p.uid);
                        if (retdata[i].pid == p.uid) {
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
                            struct player tmppi;
                            bool tmpret = getPlayer(retdata[i].pid, &tmppi);
                            if (tmpret) {
                                pthread_mutex_lock(&pinfolock);
                                if (pinfo[pn].valid) {
                                    //printf("server: sendmsg to [0x%"PRIX64"]: [%c] [%02X]\n", pinfo[pn].uid, 'R', 'R');
                                    ++msgct;
                                    wsock(tmppi.socket, buf2, datasize + 2);
                                    pinfo[pn].ack = false;
                                    p.ack = false;
                                    //puts("server: set ack to false");
                                }
                                pthread_mutex_unlock(&pinfolock);
                            }
                            retdata[i].msg = SERVER_RET_NONE;
                            if (data) free(data);
                            leftoffi = i;
                            break;
                        }
                    }
                }
                //if (msgct) puts("server: no messages to send");
                pthread_mutex_unlock(&retlock);
                activity = true;
            } else {
                //printf("waiting for ack for user [0x%lX]\n", p.uid);
            }
        }
    }
    free(buf2);
    free(pinfo);
    close(inf->socket);
    return NULL;
}

int servStart(char* addr, int port, char* world, int mcli) {
    #ifdef _WIN32
    if (!startwsa()) return -1;
    #endif
    if (mcli > 0) maxclients = mcli;
    cfgvar.server_delay = atoi(getConfigVarStatic(config, "server.server_delay", "1000", 64));
    cfgvar.server_idledelay = atoi(getConfigVarStatic(config, "server.server_idledelay", "5000", 64));
    cfgvar.unamemax =  atoi(getConfigVarStatic(config, "server.unamemax", "32", 64));
    setRandSeed(0, 14876);
    initNoiseTable(0);
    setRandSeed(1, altutime() + (uintptr_t)"");
    sock_t socketfd;
    if (SOCKINVAL(socketfd = socket(AF_INET, SOCK_STREAM, 0))) {
        fputs("servStart: Failed to create socket\n", stderr);
        return -1;
    }
    #ifndef _WIN32
    int opt = 1;
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    #else
    BOOL opt = true;
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt))) {
    #endif
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
    printf("Starting server on %s:%d with a max of %d player%c...\n", (addr) ? addr : "0.0.0.0", port, maxclients, (maxclients == 1) ? 0 : 's');
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
    #ifndef _WIN32
    int flags = fcntl(inf->socket, F_GETFL);
    fcntl(inf->socket, F_SETFL, flags | O_NONBLOCK);
    #else
    {unsigned long o = 1; ioctlsocket(inf->socket, FIONBIO, &o);}
    #endif
    //uint64_t starttime = altutime();
    bool ackcmd = true;
    while (1) {
        microwait(cfgvar.client_delay);
        //puts("CLIENT GETBUF");
        msglen = getbuf(inf->socket, buf2, 1, buf);
        //printf("CLIENT GOTBUF [%d]\n", msglen);
        if (msglen > 0) {
            //if (buf2[0] != 'A') printf("client [%s]: gotmsg: [%c] [%02X]\n", argv[4], buf2[0], buf2[0]);
            switch (buf2[0]) {
                case 'R':;
                    #ifndef _WIN32
                    fcntl(inf->socket, F_SETFL, flags);
                    #else
                    {unsigned long o = 0; ioctlsocket(inf->socket, FIONBIO, &o);}
                    #endif
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
                    #ifndef _WIN32
                    fcntl(inf->socket, F_SETFL, flags | O_NONBLOCK);
                    #else
                    {unsigned long o = 1; ioctlsocket(inf->socket, FIONBIO, &o);}
                    #endif
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
                    //printf("client [%s]: sending A\n", argv[4]);
                    //microwait(client_ackdelay);
                    wsock(inf->socket, "A", 1);
                    break;
                case 'A':;
                    //printf("client: recieved A\n");
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
                //printf("client [%s]: sendmsg: [%c] [%02X]\n", argv[4], 'M', 'M');
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
                wsock(inf->socket, buf2, msglen);
                ackcmd = false;
                //puts("client: set ack to false");
                //rsock(inf->socket, buf2, 2);
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
    #ifdef _WIN32
    if (!startwsa()) return false;
    #endif
    printf("Connecting to %s:%d...\n", addr, port);
    sock_t socketfd;
    if (SOCKINVAL(socketfd = socket(AF_INET, SOCK_STREAM, 0))) {
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
    #ifndef _WIN32
    struct timeval tmptime;
    tmptime.tv_sec = 15;
    tmptime.tv_usec = 0;
    if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &tmptime, sizeof(tmptime)) ||
        setsockopt(socketfd, SOL_SOCKET, SO_SNDTIMEO, &tmptime, sizeof(tmptime))) {
    #else
    DWORD tmptime = 15000;
    if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tmptime, sizeof(tmptime)) ||
        setsockopt(socketfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tmptime, sizeof(tmptime))) {
    #endif
        fputs("servConnect: Failed to set socket options\n", stderr);
        close(socketfd);
        return false;
    }
    if (connect(socketfd, (struct sockaddr*)&server, sizeof(server))) {
        fputs("servConnect: Failed to connect to server\n", stderr);
        close(socketfd);
        return false;
    }
    #ifndef _WIN32
    tmptime.tv_sec = 1;
    tmptime.tv_usec = 0;
    if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &tmptime, sizeof(tmptime)) ||
        setsockopt(socketfd, SOL_SOCKET, SO_SNDTIMEO, &tmptime, sizeof(tmptime))) {
    #else
    tmptime = 1000;
    if (setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tmptime, sizeof(tmptime)) ||
        setsockopt(socketfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tmptime, sizeof(tmptime))) {
    #endif
        fputs("servConnect: Failed to set socket options\n", stderr);
        close(socketfd);
        return false;
    }
    {
        int tmpint = SERVER_BUF_SIZE;
        #ifndef _WIN32
        setsockopt(socketfd, SOL_SOCKET, SO_RCVBUF, &tmpint, sizeof(tmpint));
        #else
        setsockopt(socketfd, SOL_SOCKET, SO_RCVBUF, (const char*)&tmpint, sizeof(tmpint));
        #endif
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
