#ifndef CHUNK_H
#define CHUNK_H

#include <inttypes.h>
#include <renderer.h>

struct blockdata {
    uint8_t id;
    uint8_t light:4;
    uint8_t rot:4;
};

struct rendorder {
    uint32_t c;
    float dist;
};

struct chunkinfo {
    uint32_t dist;
    uint32_t width;
    uint32_t widthsq;
    uint32_t size;
};

struct chunkdata {
    struct chunkinfo info;
    struct rendorder* rordr;
    struct blockdata** data;
    struct chunk_renddata* renddata;
};

struct chunkdata allocChunks(uint32_t);
void genChunks(struct chunkdata*, int64_t, int64_t);
void genChunks_cb(struct chunkdata*, void*);
bool genChunk(struct chunkinfo*, int, int, int, int64_t, int64_t, struct blockdata*, int);
void moveChunks(struct chunkdata*, int, int);
struct blockdata getBlock(struct chunkdata*, int, int, int, int, int, int);
void setBlock(struct chunkdata*, int, int, int, int, int, int, struct blockdata);

#endif
