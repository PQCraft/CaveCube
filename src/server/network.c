#include "network.h"
#include <common/endian.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdbool.h>

#ifndef _WIN32
    #include <sys/fcntl.h>
    #include <arpa/inet.h>
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
    #ifdef _WIN32
    if (ret == SOCKET_ERROR) return -1;
    #endif
    return ret;
}

static void wsock(sock_t sock, void* buf, int len) {
    #ifndef _WIN32
    write(sock, buf, len);
    #else
    send(sock, buf, len, 0);
    #endif
}

static struct netbuf* allocBuf() {
    struct netbuf* buf = malloc(sizeof(struct netbuf));
    buf->size = 0;
    buf->bufsize = 256;
    buf->data = malloc(buf->bufsize);
    return buf;
}

static void freeBuf(struct netbuf* buf) {
    free(buf->data);
    free(buf);
}

static void addToBuf(struct netbuf* buf, unsigned char* data, int size) {
    if (size < 1 || !data) return;
    bool ch = false;
    while (buf->size + size > buf->bufsize) {
        buf->bufsize *= 2;
    }
    if (ch) buf->data = realloc(buf->data, buf->bufsize);
    memmove(&buf->data[buf->size], data, size);
    buf->size += size;
}

static int addSockToBuf(struct netbuf* buf, sock_t sock, int size) {
    if (size < 0 || SOCKINVAL(sock)) return 0;
    bool ch = false;
    while (buf->size + size > buf->bufsize) {
        buf->bufsize *= 2;
    }
    if (ch) buf->data = realloc(buf->data, buf->bufsize);
    int ret = rsock(sock, &buf->data[buf->size], size);
    /*
    for (int i = 0; i < ret; ++i) {
        printf("[%d]: [%d]{%c}\n", i, buf->data[buf->size + i], buf->data[buf->size + i]);
    }
    */
    buf->size += size;
    return ret;
}

struct netcxn* newCxn(char* addr, int port, int type) {
    sock_t newsock = INVALID_SOCKET;
    if (SOCKINVAL(newsock = socket(AF_INET, SOCK_STREAM, 0))) return NULL;
    if (type == CXN_SERVER) {
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
    if (type == CXN_SERVER) {
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
    struct netcxn* newinf = malloc(sizeof(*newinf));
    *newinf = (struct netcxn){.socket = newsock, .buffer = allocBuf(), .address = address};
    return newinf;
}

void closeCxn(struct netcxn* inf) {
    close(inf->socket);
    freeBuf(inf->buffer);
    free(inf->address);
    free(inf);
}

struct netcxn* acceptCxn(struct netcxn* inf) {
    sock_t newsock = INVALID_SOCKET;
    struct sockaddr_in* address = calloc(1, sizeof(*address));
    socklen_t socklen = sizeof(*address);
    if (SOCKINVAL(newsock = accept(inf->socket, (struct sockaddr*)address, &socklen))) {free(address); return NULL;}
    #ifndef _WIN32
    {int flags = fcntl(newsock, F_GETFL); fcntl(newsock, F_SETFL, flags | O_NONBLOCK);}
    #else
    {unsigned long o = 1; ioctlsocket(newsock, FIONBIO, &o);}
    #endif
    struct netcxn* newinf = malloc(sizeof(*newinf));
    *newinf = (struct netcxn){
        .socket = newsock,
        .buffer = allocBuf(),
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

bool pollCxn(struct netcxn* inf) {
    int bytes = 0;
    #ifndef _WIN32
    ioctl(inf->socket, FIONREAD, &bytes);
    #else
    ioctlsocket(inf->socket, FIONREAD, &bytes);
    #endif
    if (bytes < 0) return false;
    int ret = addSockToBuf(inf->buffer, inf->socket, bytes);
    if (!ret) return false;
    return true;
}

void setCxnBufSize(struct netcxn* inf, int rx, int tx) {
    if (rx > 0) setsockopt(inf->socket, SOL_SOCKET, SO_RCVBUF, (void*)&rx, sizeof(rx));
    if (tx > 0) setsockopt(inf->socket, SOL_SOCKET, SO_SNDBUF, (void*)&tx, sizeof(tx));
}

char* getCxnAddrStr(struct netcxn* inf) {
    static char str[32];
    sprintf(str, "%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8":%"PRIu16"", inf->info.addr[0], inf->info.addr[1], inf->info.addr[2], inf->info.addr[3], inf->info.port);
    return str;
}

bool initNet() {
    #ifdef _WIN32
    if (!startwsa()) return false;
    #endif
    return true;
}
