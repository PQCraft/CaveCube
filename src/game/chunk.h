#ifndef CHUNK_H
#define CHUNK_H

#include <inttypes.h>
#include <renderer.h>

struct blockdata {
    uint8_t id;
    uint8_t flags;
};

struct rendorder {
    uint32_t c;
    float dist;
};

struct chunkdata {
    uint32_t dist;
    uint32_t width;
    uint32_t widthsq;
    uint32_t size;
    struct rendorder* rordr;
    struct blockdata** data;
    struct chunk_renddata* renddata;
};

struct chunkdata allocChunks(uint32_t);
void genChunks(struct chunkdata*, int, int);
void moveChunks(struct chunkdata*, int, int);

#endif
