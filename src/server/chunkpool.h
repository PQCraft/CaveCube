#if defined(MODULE_SERVER)

#ifndef SERVER_CHUNKPOOL_H
#define SERVER_CHUNKPOOL_H

#include <game/chunk.h>

#include <pthread.h>

struct chunkpooldata {
    pthread_mutex_t lock;
    bool valid;
    int64_t x;
    int64_t z;
    struct blockdata* data;
};

struct chunkpool {
    pthread_mutex_t lock;
    int chunks;
    struct chunkpooldata chunkdata;
};

struct chunkpool* createChunkPool(void);
void destroyChunkPool(struct chunkpool* /*pool*/);
struct chunkpooldata* getPoolChunk(struct chunkpool* /*pool*/, int64_t /*x*/, int64_t /*z*/);
void retPoolChunk(struct chunkpool* /*pool*/, struct chunkpooldata* /*chunk*/);

#endif

#endif
