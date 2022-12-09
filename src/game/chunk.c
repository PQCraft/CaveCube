#if MODULEID == MODULEID_GAME

#include <main/main.h>
#include "chunk.h"
#include <common/common.h>
#include <renderer/renderer.h>
#include <server/server.h>

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <math.h>

static force_inline int64_t i64_abs(int64_t v) {return (v < 0) ? -v : v;}
static force_inline int64_t i64_mod(int64_t v, int64_t m) {return ((v % m) + m) % m;}

static force_inline void _getChunkOfBlock(int64_t x, int64_t z, int64_t* chunkx, int64_t* chunkz) {
    if (x < 0) x += 1;
    if (z < 0) z += 1;
    *chunkx = (i64_abs(x) + 8) / 16;
    *chunkz = (i64_abs(z) + 8) / 16;
    if (x < 0) *chunkx = -(*chunkx);
    if (z < 0) *chunkz = -(*chunkz);
}

void getChunkOfBlock(int64_t x, int64_t z, int64_t* chunkx, int64_t* chunkz) {
    _getChunkOfBlock(x, z, chunkx, chunkz);
}

struct blockdata getBlock(struct chunkdata* data, int64_t x, int y, int64_t z) {
    if (y < 0 || y > 255) return BLOCKDATA_NULL;
    int64_t cx, cz;
    _getChunkOfBlock(x, z, &cx, &cz);
    cx = (cx - data->xoff) + data->info.dist;
    cz = data->info.width - ((cz - data->zoff) + data->info.dist) - 1;
    if (cx < 0 || cz < 0 || cx >= data->info.width || cz >= data->info.width) return BLOCKDATA_BORDER;
    x = i64_mod(x + 8, 16);
    z = 15 - i64_mod(z + 8, 16);
    return data->data[cx + cz * data->info.width][y * 256 + z * 16 + x];
}

void setBlock(struct chunkdata* data, int64_t x, int y, int64_t z, struct blockdata bdata) {
    pthread_mutex_lock(&data->lock);
    pthread_mutex_unlock(&data->lock);
}

static int compare(const void* b, const void* a) {
    float fa = ((struct rendorder*)a)->dist;
    float fb = ((struct rendorder*)b)->dist;
    //printf("[%f]\n", ((struct rendorder*)a)->dist);
    return (fa > fb) - (fa < fb);
}

static force_inline float distance(float x1, float z1, float x2, float z2) {
    //float f = sqrt(fabs(x2 - x1) * fabs(x2 - x1) + fabs(z2 - z1) * fabs(z2 - z1));
    //printf("[%f][%f][%f][%f] [%f]\n", x1, z1, x2, z2, f);
    return /*f*/ sqrt(fabs(x2 - x1) * fabs(x2 - x1) + fabs(z2 - z1) * fabs(z2 - z1));
}

struct chunkdata* allocChunks(int dist) {
    if (dist < 1) dist = 1;
    struct chunkdata* chunks = malloc(sizeof(*chunks));
    pthread_mutex_init(&chunks->lock, NULL);
    chunks->info.dist = dist;
    dist = 1 + dist * 2;
    chunks->info.width = dist;
    dist *= dist;
    chunks->info.widthsq = dist;
    chunks->data = malloc(dist * sizeof(*chunks->data));
    for (int i = 0; i < dist; ++i) {
        chunks->data[i] = calloc(sizeof(**chunks->data), 65536);
    }
    chunks->renddata = calloc(sizeof(*chunks->renddata), dist);
    chunks->rordr = calloc(sizeof(*chunks->rordr), dist);
    for (unsigned x = 0; x < chunks->info.width; ++x) {
        for (unsigned z = 0; z < chunks->info.width; ++z) {
            uint32_t c = x + z * chunks->info.width;
            chunks->rordr[c].dist = distance((int)(x - chunks->info.dist), (int)(chunks->info.dist - z), 0, 0);
            chunks->rordr[c].c = c;
        }
    }
    qsort(chunks->rordr, chunks->info.widthsq, sizeof(*chunks->rordr), compare);
    chunks->xoff = 0;
    chunks->zoff = 0;
    return chunks;
}

void moveChunks(struct chunkdata* chunks, int cx, int cz) {
    pthread_mutex_lock(&chunks->lock);
    for (int d = -chunks->info.dist; d <= (int)chunks->info.dist; ++d) {
        if (d < -2 || d > 2) {
            sortChunk(-1, d, 0, false);
            sortChunk(-1, d, 1, false);
            sortChunk(-1, d, -1, false);
            sortChunk(-1, 0, d, false);
            sortChunk(-1, 1, d, false);
            sortChunk(-1, -1, d, false);
        }
    }
    chunks->xoff += cx;
    chunks->zoff += cz;
    struct blockdata* swap;
    struct chunk_renddata rdswap;
    int c;
    for (; cx < 0; ++cx) { // move left (player goes right)
        for (int z = 0; z < chunks->info.width; ++z) {
            c = (chunks->info.width - 1) + z * chunks->info.width;
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            for (int x = chunks->info.width - 1; x >= 1; --x) {
                c = x + z * chunks->info.width;
                chunks->data[c] = chunks->data[c - 1];
                chunks->renddata[c] = chunks->renddata[c - 1];
            }
            c = z * chunks->info.width;
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            if (chunks->renddata[c].generated) {
                chunks->renddata[c].vcount[0] = 0;
                chunks->renddata[c].vcount[1] = 0;
                chunks->renddata[c].tcount[0] = 0;
                chunks->renddata[c].tcount[1] = 0;
                chunks->renddata[c].buffered = false;
                chunks->renddata[c].generated = false;
            }
        }
    }
    for (; cx > 0; --cx) { // move right (player goes left)
        for (int z = 0; z < chunks->info.width; ++z) {
            c = z * chunks->info.width;
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            for (int x = 0; x < chunks->info.width - 1; ++x) {
                c = x + z * chunks->info.width;
                chunks->data[c] = chunks->data[c + 1];
                chunks->renddata[c] = chunks->renddata[c + 1];
            }
            c = (chunks->info.width - 1) + z * chunks->info.width;
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            if (chunks->renddata[c].generated) {
                chunks->renddata[c].vcount[0] = 0;
                chunks->renddata[c].vcount[1] = 0;
                chunks->renddata[c].tcount[0] = 0;
                chunks->renddata[c].tcount[1] = 0;
                chunks->renddata[c].buffered = false;
                chunks->renddata[c].generated = false;
            }
        }
    }
    for (; cz < 0; ++cz) { // move forward (player goes backward)
        for (int x = 0; x < chunks->info.width; ++x) {
            c = x;
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            for (int z = 0; z < chunks->info.width - 1; ++z) {
                c = x + z * chunks->info.width;
                chunks->data[c] = chunks->data[c + chunks->info.width];
                chunks->renddata[c] = chunks->renddata[c + chunks->info.width];
            }
            c = x + (chunks->info.width - 1) * chunks->info.width;
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            if (chunks->renddata[c].generated) {
                chunks->renddata[c].vcount[0] = 0;
                chunks->renddata[c].vcount[1] = 0;
                chunks->renddata[c].tcount[0] = 0;
                chunks->renddata[c].tcount[1] = 0;
                chunks->renddata[c].buffered = false;
                chunks->renddata[c].generated = false;
            }
        }
    }
    for (; cz > 0; --cz) { // move backward (player goes forward)
        for (int x = 0; x < chunks->info.width; ++x) {
            c = x + (chunks->info.width - 1) * chunks->info.width;
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            for (int z = chunks->info.width - 1; z >= 1; --z) {
                c = x + z * chunks->info.width;
                chunks->data[c] = chunks->data[c - chunks->info.width];
                chunks->renddata[c] = chunks->renddata[c - chunks->info.width];
            }
            c = x;
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            if (chunks->renddata[c].generated) {
                chunks->renddata[c].vcount[0] = 0;
                chunks->renddata[c].vcount[1] = 0;
                chunks->renddata[c].tcount[0] = 0;
                chunks->renddata[c].tcount[1] = 0;
                chunks->renddata[c].buffered = false;
                chunks->renddata[c].generated = false;
            }
        }
    }
    pthread_mutex_unlock(&chunks->lock);
}

#endif
