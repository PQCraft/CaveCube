#ifndef SERVER_H
#define SERVER_H

#include <game/chunk.h>

#include <stdbool.h>

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
    SERVER_DATA_PONG,
    SERVER_DATA_COMPATINFO,
    SERVER_DATA_UPDATECHUNK,
    SERVER_DATA_UPDATECHUNKCOL,
};

struct server_data_compatinfo {
    uint16_t ver_major;
    uint16_t ver_minor;
    uint16_t ver_patch;
    uint16_t flags;
    char server_str[256];
};

struct server_data_updatechunk {
    uint16_t id;
    int x;
    int y;
    int z;
    int64_t xo;
    int64_t zo;
    struct blockdata data[4096];
};

struct server_data_updatechunkcol {
    uint16_t id;
    int x;
    int z;
    int64_t xo;
    int64_t zo;
    struct blockdata data[16][4096];
};

struct server_data {
    int msg;
    void* data;
};

enum {
    CLIENT_DATA_PING,
    CLIENT_DATA_COMPATINFO,
    CLIENT_DATA_GETCHUNK,
    CLIENT_DATA_GETCHUNKCOL,
    CLIENT_DATA_SETCHUNKPOS,
};

struct client_data_compatinfo {
    uint16_t ver_major;
    uint16_t ver_minor;
    uint16_t ver_patch;
    uint16_t flags;
    char client_str[256];
};

struct client_data_getchunk {
    struct chunkinfo info;
    uint16_t id;
    int x;
    int y;
    int z;
    int64_t xo;
    int64_t zo;
};

struct client_data_getchunkcol {
    struct chunkinfo info;
    uint16_t id;
    int x;
    int z;
    int64_t xo;
    int64_t zo;
};

struct client_data_setchunkpos {
    int64_t x;
    int64_t z;
};

extern int SERVER_THREADS;

bool initServer(void);
int startServer(char*, int, char*, int);
void stopServer(void);

bool cliConnect(char*, int);
void cliSend(int, void*, bool);
void cliRecv(void (*)(int, void*), int);

#endif
