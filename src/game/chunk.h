#ifndef GAME_CHUNK_H
#define GAME_CHUNK_H

#include <renderer/renderer.h>

#include <inttypes.h>

struct __attribute__((packed)) blockdata {
    uint8_t id:8;
    uint8_t subid:6;
    uint8_t rotx:2;
    uint8_t roty:2;
    uint8_t rotz:2;
    uint8_t light_r:4;
    uint8_t light_g:4;
    uint8_t light_b:4;
};

struct chunkinfo {
    uint32_t dist;
    uint32_t width;
    uint32_t widthsq;
};

#if MODULEID == MODULEID_GAME
struct chunkdata {
    struct chunkinfo info;
    struct blockdata** data;
    struct chunk_renddata* renddata;
};

struct chunkdata allocChunks(uint32_t);
void moveChunks(struct chunkdata*, int64_t, int64_t, int, int);
struct blockdata getBlock(struct chunkdata*, int64_t, int64_t, int, int, int);
void setBlock(struct chunkdata*, int64_t, int64_t, int, int, int, struct blockdata);
#endif

#endif
