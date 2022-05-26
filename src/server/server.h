#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>
#include <chunk.h>

#ifndef SERVER_THREADS
    #define SERVER_THREADS 4
#endif

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
    SERVER_MODE_SP,
    SERVER_MODE_MP,
    SERVER_MODE_BRIDGE,
};

enum {
    SERVER_MSG_PING,
    SERVER_MSG_GETCHUNK,
    SERVER_MSG_GETCHUNKCOL,
};

enum {
    SERVER_CMD_SETCHUNK = 128,
};

enum {
    SERVER_RET_NONE,
    SERVER_RET_PONG,
    SERVER_RET_UPDATECHUNK,
    SERVER_RET_UPDATECHUNKCOL,
};

struct server_msg_chunk {
    struct chunkinfo info;
    uint16_t id;
    int x;
    int y;
    int z;
    int64_t xo;
    int64_t zo;
};

struct server_msg_chunkcol {
    struct chunkinfo info;
    uint16_t id;
    int x;
    int z;
    int64_t xo;
    int64_t zo;
};

struct server_ret_chunk {
    uint16_t id;
    int x;
    int y;
    int z;
    int64_t xo;
    int64_t zo;
    struct blockdata data[4096];
};

struct server_ret_chunkcol {
    uint16_t id;
    int x;
    int z;
    int64_t xo;
    int64_t zo;
    struct blockdata data[16][4096];
};

struct server_msg_chunkpos {
    int64_t x;
    int64_t z;
};

struct server_ret {
    int msg;
    void* data;
};

bool initServer(void);
int servStart(char*, int, char*, int);
bool servConnect(char*, int);
void servSend(int, void*, bool);
void servRecv(void (*)(int, void*), int);

#endif
