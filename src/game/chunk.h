#ifndef CHUNK_H
#define CHUNK_H

#include <inttypes.h>
#include <renderer.h>

typedef struct {
    uint8_t id;
    uint8_t flags;
} blockdata;

typedef struct {
    uint32_t dist;
    uint32_t width;
    uint32_t widthsq;
    uint32_t size;
    blockdata** data;
    chunk_renddata* renddata;
} chunkdata;

chunkdata allocChunks(uint32_t);
void genChunks(chunkdata*, int, int);
void moveChunks(chunkdata*, int, int);

#endif
