#include "network.h"
#include <common/endian.h>
#include <common/common.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <errno.h>

#ifndef _WIN32
    #include <sys/fcntl.h>
    #include <arpa/inet.h>
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
    int ret = recv(sock, buf, len, 0);
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
        buf->data[buf->wptr] = data[i];
        buf->wptr = (buf->wptr + 1) % buf->size;
    }
    free(data);
    return ret;
}

static int writeBufToSock(struct netbuf* buf, sock_t sock) {
    int size = buf->dlen;
    if (size < 1) return;
}

struct netcxn* newCxn(int type, char* addr, int port, int obs, int ibs) {
    sock_t newsock = INVALID_SOCKET;
    if (SOCKINVAL(newsock = socket(AF_INET, SOCK_STREAM, 0))) return NULL;
    if (type == CXN_MULTI) {
        #ifndef _WIN32
        int opt = 1;
        setsockopt(newsock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        #else
        BOOL opt = true;
        setsockopt(newsock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
        #endif
    }
    struct sockaddr_in* address = calloc(1, sizeof(*address));
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = (addr) ? inet_addr(addr) : INADDR_ANY;
    address->sin_port = host2net16(port);
    if (type == CXN_MULTI) {
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
    } else {
        if (connect(newsock, (struct sockaddr*)address, sizeof(*address))) {
            fputs("newCxn: Failed to connect\n", stderr);
            close(newsock);
            return NULL;
        }
    }
    #ifndef _WIN32
    {int flags = fcntl(newsock, F_GETFL); fcntl(newsock, F_SETFL, flags | O_NONBLOCK);}
    #else
    {unsigned long o = 1; ioctlsocket(newsock, FIONBIO, &o);}
    #endif
    struct netcxn* newinf = calloc(1, sizeof(*newinf));
    *newinf = (struct netcxn){
        .type = type,
        .socket = newsock,
        .address = address,
        .info = (struct netinfo){
            .addr[0] = (address->sin_addr.s_addr) & 0xFF,
            .addr[1] = (address->sin_addr.s_addr >> 8) & 0xFF,
            .addr[2] = (address->sin_addr.s_addr >> 16) & 0xFF,
            .addr[3] = (address->sin_addr.s_addr >> 24) & 0xFF,
            .port = address->sin_port
        }
    };
    if (type != CXN_MULTI) {
        newinf->inbuf = allocBuf(ibs);
        newinf->outbuf = allocBuf(obs);
    }
    return newinf;
}

void closeCxn(struct netcxn* cxn) {
    close(cxn->socket);
    if (cxn->type != CXN_MULTI) {
        freeBuf(cxn->inbuf);
        freeBuf(cxn->outbuf);
    }
    free(cxn->address);
    free(cxn);
}

struct netcxn* acceptCxn(struct netcxn* cxn, int obs, int ibs) {
    sock_t newsock = INVALID_SOCKET;
    struct sockaddr_in* address = calloc(1, sizeof(*address));
    socklen_t socklen = sizeof(*address);
    if (SOCKINVAL(newsock = accept(cxn->socket, (struct sockaddr*)address, &socklen))) {free(address); return NULL;}
    #ifndef _WIN32
    {int flags = fcntl(newsock, F_GETFL); fcntl(newsock, F_SETFL, flags | O_NONBLOCK);}
    #else
    {unsigned long o = 1; ioctlsocket(newsock, FIONBIO, &o);}
    #endif
    struct netcxn* newinf = malloc(sizeof(*newinf));
    *newinf = (struct netcxn){
        .type = CXN_SINGLE,
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
}

int recvCxn(struct netcxn* cxn) {
    int bytes = 0;
    if (SOCKERR(ioctlsocket(cxn->socket, FIONREAD, &bytes))) return -1;
    if (bytes < 0) return -1;
    return writeSockToBuf(cxn->inbuf, cxn->socket, bytes);
}

int recvCxn(struct netcxn* cxn) {
    
}

void setCxnBufSize(struct netcxn* cxn, int tx, int rx) {
    if (tx > 0) setsockopt(cxn->socket, SOL_SOCKET, SO_SNDBUF, (void*)&tx, sizeof(tx));
    if (rx > 0) setsockopt(cxn->socket, SOL_SOCKET, SO_RCVBUF, (void*)&rx, sizeof(rx));
}

char* getCxnAddrStr(struct netcxn* cxn) {
    static char str[32];
    sprintf(str, "%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8":%"PRIu16"", cxn->info.addr[0], cxn->info.addr[1], cxn->info.addr[2], cxn->info.addr[3], cxn->info.port);
    return str;
}

bool initNet() {
    #ifdef _WIN32
    if (!startwsa()) return false;
    #endif
    return true;
}
