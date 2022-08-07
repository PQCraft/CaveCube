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
    #define CLIENT_SNDBUF_SIZE -1
#endif

#ifndef SERVER_OUTBUF_SIZE
    #define SERVER_OUTBUF_SIZE (1 << 21)
#endif

#ifndef CLIENT_OUTBUF_SIZE
    #define CLIENT_OUTBUF_SIZE (1 << 16)
#endif

enum {
    SERVER_FLAG_NOAUTH = 1 << 0,
    SERVER_FLAG_PASSWD = 1 << 1,
};

enum {
    SERVER_PONG,
    SERVER_COMPATINFO,
    SERVER_LOGININFO,
    SERVER_UPDATECHUNK,
    SERVER_UPDATECHUNKCOL,
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
    uint16_t id;
    int64_t x;
    int8_t y;
    int64_t z;
    struct blockdata data[4096];
};

struct server_data_updatechunkcol {
    uint16_t id;
    int64_t x;
    int64_t z;
    struct blockdata data[16][4096];
};

struct server_data {
    int msg;
    void* data;
};

enum {
    CLIENT_PING,
    CLIENT_COMPATINFO,
    CLIENT_LOGININFO,
    CLIENT_GETCHUNK,
    CLIENT_GETCHUNKCOL,
    CLIENT_SETCHUNKPOS,
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
    struct chunkinfo info;
    uint16_t id;
    int64_t x;
    int8_t y;
    int64_t z;
};

struct client_data_getchunkcol {
    uint16_t id;
    int64_t x;
    int64_t z;
};

struct client_data_setchunkpos {
    int64_t x;
    int64_t z;
};

extern int SERVER_THREADS;

bool initServer(void);
int startServer(char*, int, char*, int);
void stopServer(void);

bool cliConnect(char*, int, void (*)(int, ...));
void cliDisconnect(void);
void cliSend(int, ...);

#endif
