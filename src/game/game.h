#ifndef GAME_H
#define GAME_H

#include <renderer.h>

typedef struct {
    char* name;
    model* mdl;
    bool alpha;
} block;

typedef struct {
    uint8_t id;
    uint8_t flags;
    uint8_t mflags;
} blockdata;

typedef struct {
    uint32_t dist;
    uint32_t width;
    uint32_t size;
    uint32_t off;
    uint32_t coff;
    blockdata** data;
} chunkdata;

void doGame(void);
blockdata getBlock(chunkdata*, register int, register int, register int);
void setBlock(chunkdata*, register int, register int, register int, blockdata);

extern block blockinfo[256];

#define GETBLOCKPOS(d, x, z) d[((x / 15) % data->width) + ((z / 15) * data->width)][y * 225 + (z % 15) * 15 + (x % 15)]

#endif
