#ifndef SERVER_H
#define SERVER_H

#include <game/chunk.h>

#include <stdbool.h>

#ifndef MAX_CLIENTS
    #define MAX_CLIENTS 256
#endif

#ifndef SERVER_BUF_SIZE
    #define SERVER_BUF_SIZE 262144
#endif

#ifndef CLIENT_BUF_SIZE
    #define CLIENT_BUF_SIZE 32768
#endif

enum {
    SERVER_MSG_ACK,
    SERVER_MSG_DATA,
};

enum {
    SERVER_DATA_PING,
    SERVER_DATA_GETCHUNK,
    SERVER_DATA_GETCHUNKCOL,
    SERVER_DATA_SETCHUNKPOS,
};

enum {
    SERVER_RET_NONE,
    SERVER_RET_PONG,
    SERVER_RET_UPDATECHUNK,
    SERVER_RET_UPDATECHUNKCOL,
};

struct server_data_getchunk {
    struct chunkinfo info;
    uint16_t id;
    int x;
    int y;
    int z;
    int64_t xo;
    int64_t zo;
};

struct server_data_getchunkcol {
    struct chunkinfo info;
    uint16_t id;
    int x;
    int z;
    int64_t xo;
    int64_t zo;
};

struct server_data_setchunkpos {
    int64_t x;
    int64_t z;
};

struct server_ret_updatechunk {
    uint16_t id;
    int x;
    int y;
    int z;
    int64_t xo;
    int64_t zo;
    struct blockdata data[4096];
};

struct server_ret_updatechunkcol {
    uint16_t id;
    int x;
    int z;
    int64_t xo;
    int64_t zo;
    struct blockdata data[16][4096];
};

struct server_ret {
    int msg;
    void* data;
};

extern int SERVER_THREADS;

bool initServer(void);
int startServer(char*, int, char*, int);
void stopServer(void);

bool cliConnect(char*, int);
void cliSend(int, void*, bool);
void cliRecv(void (*)(int, void*), int);

#endif
