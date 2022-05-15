#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>
#include <chunk.h>

#ifndef SERVER_THREADS
    #define SERVER_THREADS 4
#endif

enum {
    SERVER_MODE_SP,
    SERVER_MODE_MP,
    SERVER_MODE_BRIDGE,
};

enum {
    SERVER_MSG_PING,
    SERVER_MSG_GETCHUNK,
};

enum {
    SERVER_CMD_SETCHUNK = 128,
};

enum {
    SERVER_RET_NONE,
    SERVER_RET_PONG,
    SERVER_RET_UPDATECHUNK,
};

struct server_msg_chunk {
    uint16_t id;
    struct chunkinfo info;
    int x;
    int y;
    int z;
    int64_t xo;
    int64_t zo;
};

struct server_ret_chunk {
    uint16_t id;
    //struct chunkinfo* info;
    int x;
    int y;
    int z;
    struct blockdata data[4096];
};

struct server_msg_chunkpos {
    int64_t x;
    int64_t z;
};

struct server_ret {
    int msg;
    void* data;
};

bool initServer(int);
void servSend(int, void*, bool);
void servRecv(void (*)(int, void*), int);

#endif
