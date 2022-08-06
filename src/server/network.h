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
    CXN__MIN = 0,
    CXN_INVAL = 0,
    CXN_ACTIVE,
    CXN_PASSIVE,
    CXN_ACTIVE_MULTICAST,
    CXN_PASSIVE_MULTICAST,
    CXN__MAX,
};

struct netbuf {
    int size;
    unsigned char* data;
    int dlen;
    int rptr;
    int wptr;
};

struct netinfo {
    uint8_t addr[4];
    uint16_t port;
};

struct netcxn {
    int type;
    sock_t socket;
    struct netbuf* inbuf;
    struct netbuf* outbuf;
    struct sockaddr_in* address;
    struct netinfo info;
};

bool initNet(void);
struct netcxn* newCxn(int, char*, int, int, int);
void closeCxn(struct netcxn*);
struct netcxn* acceptCxn(struct netcxn*, int, int);
int recvCxn(struct netcxn*);
int sendCxn(struct netcxn*);
void setCxnBufSize(struct netcxn*, int, int);
char* getCxnAddrStr(struct netcxn*);

#endif
