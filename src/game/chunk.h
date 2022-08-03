#ifndef GAME_CHUNK_H
#define GAME_CHUNK_H

#include <renderer/renderer.h>

#include <inttypes.h>

struct blockdata {
    uint8_t id;
    uint8_t light:4;
    uint8_t rot:4;
};

struct chunkinfo {
    uint32_t dist;
    uint32_t width;
    uint32_t widthsq;
    uint32_t size;
};

#ifndef SERVER
struct chunkdata {
    struct chunkinfo info;
    struct blockdata** data;
    struct chunk_renddata* renddata;
};

struct chunkdata allocChunks(uint32_t);
void genChunks(struct chunkdata*, int64_t, int64_t);
void genChunks_cb(struct chunkdata*, void*);
void genChunks_cb2(struct chunkdata*, void*);
void moveChunks(struct chunkdata*, int, int);
struct blockdata getBlock(struct chunkdata*, int, int, int, int, int, int);
void setBlock(struct chunkdata*, int, int, int, int, int, int, struct blockdata);
#endif

#endif
