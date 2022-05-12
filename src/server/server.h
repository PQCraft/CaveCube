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

struct server_chunk {
    uint16_t id;
    struct chunkdata* chunks;
    int x;
    int y;
    int z;
    int xo;
    int zo;
    struct blockdata data[4096];
};

struct server_chunkpos {
    int x;
    int z;
};

bool initServer(int);
bool servMsgReady(int);
int servSend(int, void*, bool, void*);

#endif
