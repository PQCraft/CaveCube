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

struct server_chunk {
    struct chunkdata* data;
    int x;
    int z;
    int xo;
    int zo;
};

bool initServer(int);
bool servMsgReady(int);
int servSend(int, void*, bool, bool);

#endif
