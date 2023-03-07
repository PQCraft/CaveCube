#ifndef GAME_CHUNK_H
#define GAME_CHUNK_H

#include <renderer/renderer.h>

#include <inttypes.h>

struct __attribute__((packed)) blockdata {
    uint16_t id:8;              // 1
    uint16_t subid:6;
    uint16_t rotx:2;
    uint16_t roty:2;            // 2
    uint16_t rotz:2;
    uint16_t charge:4;
    uint16_t /*padding*/:3;
    uint16_t light_n:5;
    uint16_t /*padding*/:1;     // 3
    uint16_t light_r:5;
    uint16_t light_g:5;
    uint16_t light_b:5;
};

#if MODULEID == MODULEID_GAME
struct rendorder {
    uint32_t c;
    float dist;
};

struct chunkinfo {
    uint32_t dist;
    uint32_t width;
    uint32_t widthsq;
};

struct chunkdata {
    pthread_mutex_t lock;
    struct chunkinfo info;
    struct rendorder* rordr;
    struct blockdata** data;
    int64_t xoff;
    int64_t zoff;
    struct chunk_renddata* renddata;
};

void getBlock(struct chunkdata*, int64_t, int, int64_t, struct blockdata*);
void setBlock(struct chunkdata*, int64_t, int, int64_t, struct blockdata);
void getChunkOfBlock(int64_t, int64_t, int64_t*, int64_t*);
struct chunkdata* allocChunks(int);
void resizeChunks(struct chunkdata*, int);
void moveChunks(struct chunkdata*, int, int);
#endif

#endif
