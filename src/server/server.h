#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

#include <game/chunk.h>

#include <stdbool.h>
#include <stdarg.h>

#ifndef MAX_CLIENTS
    #define MAX_CLIENTS 256
#endif

#ifndef SERVER_SNDBUF_SIZE
    #define SERVER_SNDBUF_SIZE 262144
#endif

#ifndef CLIENT_SNDBUF_SIZE
    #define CLIENT_SNDBUF_SIZE 262144
#endif

#ifndef SERVER_OUTBUF_SIZE
    #define SERVER_OUTBUF_SIZE (1 << 22)
#endif

#ifndef CLIENT_OUTBUF_SIZE
    #define CLIENT_OUTBUF_SIZE (1 << 18)
#endif

#ifndef SERVER_READACK
    //#define SERVER_READACK
#endif

#ifndef CLIENT_READACK
    //#define CLIENT_READACK
#endif

enum {
    SERVER__MIN = -1,
    SERVER_PONG,
    SERVER_COMPATINFO,
    SERVER_LOGININFO,
    SERVER_UPDATECHUNK,
    SERVER_SETSKYCOLOR,
    SERVER__MAX,
};

enum {
    CLIENT__MIN = -1,
    CLIENT_PING,
    CLIENT_COMPATINFO,
    CLIENT_LOGININFO,
    CLIENT_GETCHUNK,
    CLIENT__MAX,
};

struct server_data_compatinfo {
    uint16_t ver_major;
    uint16_t ver_minor;
    uint16_t ver_patch;
    uint8_t flags;
    char* server_str;
};

struct server_data_logininfo {
    uint8_t failed;
    char* reason;
    uint64_t uid;
    uint64_t password;
};

struct server_data_updatechunk {
    int64_t x;
    int64_t z;
    struct blockdata data[65536];
};

struct server_data_setskycolor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct client_data_compatinfo {
    uint16_t ver_major;
    uint16_t ver_minor;
    uint16_t ver_patch;
    char* client_str;
};

struct client_data_logininfo {
    uint64_t uid;
    uint64_t password;
    char* username;
};

struct client_data_getchunk {
    int64_t x;
    int64_t z;
};

enum {
    SERVER_FLAG_NOAUTH = 1 << 0,
    SERVER_FLAG_PASSWD = 1 << 1,
};

extern int SERVER_THREADS;

bool initServer(void);
int startServer(char*, int, int, char*);
void stopServer(void);

bool cliConnect(char*, int, void (*)(int, void*));
void cliDisconnect(void);
void cliSend(int, ...);

#endif
