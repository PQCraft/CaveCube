#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>

#ifndef _WIN32
    #include <sys/fcntl.h>
    #include <arpa/inet.h>
    #define PRIsock "d"
#else
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #define PRIsock PRIu64
#endif

#ifndef _WIN32
    typedef int sock_t;
    #define INVALID_SOCKET -1
#else
    typedef SOCKET sock_t;
#endif

enum {
    CXN_SERVER,
    CXN_CLIENT,
};

struct netbuf {
    unsigned char* data;
    int size;
    int bufsize;
};

struct netinfo {
    uint8_t addr[4];
    uint16_t port;
};

struct netcxn {
    sock_t socket;
    struct netbuf* buffer;
    struct sockaddr_in* address;
    struct netinfo info;
};

bool initNet(void);
struct netcxn* newCxn(char*, int, int);
void closeCxn(struct netcxn*);
struct netcxn* acceptCxn(struct netcxn*);
bool pollCxn(struct netcxn*);
void setCxnBufSize(struct netcxn*, int, int);
char* getCxnAddrStr(struct netcxn*);

#endif
