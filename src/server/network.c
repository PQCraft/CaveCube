#include <main/main.h>
#include "network.h"
#include <common/endian.h>
#include <common/common.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#ifndef _WIN32
    #include <netdb.h>
    #include <sys/ioctl.h>
    #include <sys/fcntl.h>
    #include <arpa/inet.h>
    #include <netinet/tcp.h>
    #define PRIsock "d"
    #define SOCKINVAL(s) ((s) < 0)
    #define SOCKERR(x) ((x) < 0)
    #define SOCKET_ERROR -1
    #define ioctlsocket ioctl
#else
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #define PRIsock PRIu64
    #define SOCKINVAL(s) ((s) == INVALID_SOCKET)
    #define SOCKERR(x) ((x) == SOCKET_ERROR)
    #define in_addr_t uint32_t
    static WSADATA wsadata;
    static WORD wsaver = MAKEWORD(2, 2);
    static bool wsainit = false;
#endif

#ifdef _WIN32
static bool startwsa() {
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

static int rsock(sock_t sock, void* buf, int len) {
    //puts("RECV...");
    //uint64_t t = altutime();
    int ret = recv(sock, buf, len, 0);
    //if (ret > 0) printf("RECV: [%d]: [%"PRIu64"]\n", ret, altutime() - t);
    if (SOCKERR(ret)) {
        bool cond;
        #ifndef _WIN32
        cond = (errno == EAGAIN || errno == EWOULDBLOCK);
        #else
        cond = (WSAGetLastError() == WSAEWOULDBLOCK);
        #endif
        if (cond) {
            return 0;
        } else {
            return -1;
        }
    } else if (!ret) {
        return -1;
    }
    return ret;
}

static int wsock(sock_t sock, void* buf, int len) {
    int ret = send(sock, buf, len, 0);
    if (SOCKERR(ret)) {
        bool cond;
        #ifndef _WIN32
        cond = (errno == EAGAIN || errno == EWOULDBLOCK);
        #else
        cond = (WSAGetLastError() == WSAEWOULDBLOCK);
        #endif
        if (cond) {
            return 0;
        } else {
            return -1;
        }
    }
    return ret;
}

static struct netbuf* allocBuf(int size) {
    struct netbuf* buf = calloc(1, sizeof(struct netbuf));
    buf->size = size;
    buf->data = malloc(size);
    return buf;
}

static void freeBuf(struct netbuf* buf) {
    free(buf->data);
    free(buf);
}

static int writeDataToBuf(struct netbuf* buf, unsigned char* data, int size) {
    if (size < 1) return 0;
    if (buf->dlen + size > buf->size) {
        size = buf->size - buf->dlen;
        if (size < 1) return 0;
    }
    buf->dlen += size;
    for (int i = 0; i < size; ++i) {
        buf->data[buf->wptr] = data[i];
        buf->wptr = (buf->wptr + 1) % buf->size;
    }
    return size;
}

static int writeBufToData(struct netbuf* buf, unsigned char* data, int size) {
    if (size > buf->dlen) size = buf->dlen;
    if (size < 1) return 0;
    for (int i = 0; i < size; ++i) {
        data[i] = buf->data[buf->rptr];
        buf->rptr = (buf->rptr + 1) % buf->size;
    }
    buf->dlen -= size;
    return size;
}

static int writeSockToBuf(struct netbuf* buf, sock_t sock, int size) {
    if (size < 0) return 0;
    if (buf->dlen + size > buf->size) {
        size = buf->size - buf->dlen;
        if (size < 1) return 0;
    }
    unsigned char* data = malloc(size);
    int ret = rsock(sock, data, size);
    size = ret;
    if (size < 0) size = 0;
    buf->dlen += size;
    for (int i = 0; i < size; ++i) {
        //printf("[%d] [%d]: [%d]{%s}\n", buf->wptr, i, data[i], spCharToStr(data[i]));
        //putchar(data[i]);
        buf->data[buf->wptr] = data[i];
        buf->wptr = (buf->wptr + 1) % buf->size;
    }
    free(data);
    return ret;
}

static int writeBufToSock(struct netbuf* buf, sock_t sock) {
    int size = buf->dlen;
    if (size < 1) return 0;
    unsigned char* data = malloc(size);
    int oldrptr = buf->rptr;
    for (int i = 0; i < size; ++i) {
        data[i] = buf->data[buf->rptr];
        buf->rptr = (buf->rptr + 1) % buf->size;
    }
    int ret = wsock(sock, data, size);
    free(data);
    size = ret;
    if (size < 0) size = 0;
    buf->dlen -= size;
    buf->rptr = (oldrptr + size) % buf->size;
    return ret;
}

struct netcxn* newCxn(int type, char* addr, int port, int obs, int ibs) {
    if (type <= CXN__MIN || type >= CXN__MAX) return NULL;
    sock_t newsock = INVALID_SOCKET;
    if (SOCKINVAL(newsock = socket(AF_INET, SOCK_STREAM, 0))) return NULL;
    struct sockaddr_in* address = calloc(1, sizeof(*address));
    address->sin_family = AF_INET;
    if (addr) {
        struct hostent* hentry;
        if (!(hentry = gethostbyname(addr))) {
            fputs("newCxn: Failed to get IP address\n", stderr);
            close(newsock);
            return NULL;
        }
        address->sin_addr.s_addr = *((in_addr_t*)hentry->h_addr_list[0]);
    } else {
        address->sin_addr.s_addr = INADDR_ANY;
    }
    address->sin_port = host2net16(port);
    switch (type) {
        case CXN_PASSIVE:; {
            int opt = 1;
            setsockopt(newsock, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
            opt = 1;
            setsockopt(newsock, IPPROTO_TCP, TCP_NODELAY, (void*)&opt, sizeof(opt));
            if (bind(newsock, (const struct sockaddr*)address, sizeof(*address))) {
                fputs("newCxn: Failed to bind socket\n", stderr);
                close(newsock);
                return NULL;
            }
            if (listen(newsock, 256)) {
                fputs("newCxn: Failed to listen\n", stderr);
                close(newsock);
                return NULL;
            }
            break;
        }
        case CXN_ACTIVE:; {
            int opt = 1;
            setsockopt(newsock, IPPROTO_TCP, TCP_NODELAY, (void*)&opt, sizeof(opt));
            if (connect(newsock, (struct sockaddr*)address, sizeof(*address))) {
                fputs("newCxn: Failed to connect\n", stderr);
                close(newsock);
                return NULL;
            }
            break;
        }
    }
    #ifndef _WIN32
    {int flags = fcntl(newsock, F_GETFL); fcntl(newsock, F_SETFL, flags | O_NONBLOCK);}
    #else
    {unsigned long o = 1; ioctlsocket(newsock, FIONBIO, &o);}
    #endif
    struct netcxn* newinf = calloc(1, sizeof(*newinf));
    socklen_t socklen = sizeof(*address);
    getsockname(newsock, (struct sockaddr*)address, &socklen);
    *newinf = (struct netcxn){
        .type = type,
        .socket = newsock,
        .address = address,
        .info = (struct netinfo){
            .addr[0] = (address->sin_addr.s_addr) & 0xFF,
            .addr[1] = (address->sin_addr.s_addr >> 8) & 0xFF,
            .addr[2] = (address->sin_addr.s_addr >> 16) & 0xFF,
            .addr[3] = (address->sin_addr.s_addr >> 24) & 0xFF,
            .port = net2host16(address->sin_port)
        }
    };
    if (type == CXN_ACTIVE) {
        newinf->inbuf = allocBuf(ibs);
        newinf->outbuf = allocBuf(obs);
    }
    return newinf;
}

void closeCxn(struct netcxn* cxn) {
    close(cxn->socket);
    if (cxn->inbuf) freeBuf(cxn->inbuf);
    if (cxn->outbuf) freeBuf(cxn->outbuf);
    free(cxn->address);
    free(cxn);
}

struct netcxn* acceptCxn(struct netcxn* cxn, int obs, int ibs) {
    switch (cxn->type) {
        case CXN_PASSIVE:; {
            sock_t newsock = INVALID_SOCKET;
            struct sockaddr_in* address = calloc(1, sizeof(*address));
            socklen_t socklen = sizeof(*address);
            if (SOCKINVAL(newsock = accept(cxn->socket, (struct sockaddr*)address, &socklen))) {free(address); return NULL;}
            #ifndef _WIN32
            {int flags = fcntl(newsock, F_GETFL); fcntl(newsock, F_SETFL, flags | O_NONBLOCK);}
            #else
            {unsigned long o = 1; ioctlsocket(newsock, FIONBIO, &o);}
            #endif
            int opt = 1;
            setsockopt(newsock, IPPROTO_TCP, TCP_NODELAY, (void*)&opt, sizeof(opt));
            struct netcxn* newinf = malloc(sizeof(*newinf));
            *newinf = (struct netcxn){
                .type = CXN_ACTIVE,
                .socket = newsock,
                .inbuf = allocBuf(ibs),
                .outbuf = allocBuf(obs),
                .address = address,
                .info = (struct netinfo){
                    .addr[0] = (address->sin_addr.s_addr) & 0xFF,
                    .addr[1] = (address->sin_addr.s_addr >> 8) & 0xFF,
                    .addr[2] = (address->sin_addr.s_addr >> 16) & 0xFF,
                    .addr[3] = (address->sin_addr.s_addr >> 24) & 0xFF,
                    .port = address->sin_port
                }
            };
            return newinf;
            break;
        }
    }
    return NULL;
}

int recvCxn(struct netcxn* cxn) {
    switch (cxn->type) {
        case CXN_ACTIVE:; {
            //int bytes = 0;
            //if (SOCKERR(ioctlsocket(cxn->socket, FIONREAD, &bytes))) return -1;
            //if (bytes < 0) return -1;
            return writeSockToBuf(cxn->inbuf, cxn->socket, cxn->inbuf->size - cxn->inbuf->dlen);
            break;
        }
    }
    return 0;
}

int sendCxn(struct netcxn* cxn) {
    switch (cxn->type) {
        case CXN_ACTIVE:; {
            return writeBufToSock(cxn->outbuf, cxn->socket);
            break;
        }
    }
    return 0;
}

int readFromCxnBuf(struct netcxn* cxn, void* data, int size) {
    switch (cxn->type) {
        case CXN_ACTIVE:; {
            return writeBufToData(cxn->inbuf, data, size);
            break;
        }
    }
    return 0;
}

int writeToCxnBuf(struct netcxn* cxn, void* data, int size) {
    switch (cxn->type) {
        case CXN_ACTIVE:; {
            return writeDataToBuf(cxn->outbuf, data, size);
            break;
        }
    }
    return 0;
}

void setCxnBufSize(struct netcxn* cxn, int tx, int rx) {
    if (tx > 0) setsockopt(cxn->socket, SOL_SOCKET, SO_SNDBUF, (void*)&tx, sizeof(tx));
    if (rx > 0) setsockopt(cxn->socket, SOL_SOCKET, SO_RCVBUF, (void*)&rx, sizeof(rx));
}

char* getCxnAddrStr(struct netcxn* cxn, char* str) {
    if (!str) str = malloc(22);
    sprintf(str, "%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8":%"PRIu16"", cxn->info.addr[0], cxn->info.addr[1], cxn->info.addr[2], cxn->info.addr[3], cxn->info.port);
    return str;
}

bool initNet() {
    #ifdef _WIN32
    if (!startwsa()) return false;
    #endif
    return true;
}
