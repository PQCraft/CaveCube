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

// TODO: burn with fire

struct blockdata getBlock(struct chunkdata* data, int64_t cx, int64_t cz, int64_t x, int y, int64_t z) {
    //cz *= -1;
    z *= -1;
    //printf("in: [%d, %d, %d] [%d, %d, %d]\n", cx, cy, cz, x, y, z);
    cx += data->info.dist;
    cz += data->info.dist;
    x += 8;
    z += 8;
    if (cx < 0 || cz < 0 || y < 0 || y > 255 || cx >= (int)data->info.width || cz >= (int)data->info.width ) return (struct blockdata){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int32_t c = cx + cz * data->info.width;
    while (x < 0 && c % data->info.width) {c -= 1; x += 16;}
    while (x > 15 && (c + 1) % data->info.width) {c += 1; x -= 16;}
    while (z > 15 && c >= (int)data->info.width) {c -= data->info.width; z -= 16;}
    while (z < 0 && c < (int)(data->info.widthsq - data->info.width)) {c += data->info.width; z += 16;}
    cx = c % data->info.width;
    cz = c / data->info.width % data->info.width;
    //printf("resolved: [%d, %d, %d] [%d, %d, %d]\n", cx, cy, cz, x, y, z);
    if (c < 0 || c >= (int32_t)data->info.widthsq || x < 0 || z < 0 || x > 15 || z > 15) return (struct blockdata){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (!data->renddata[c].generated) return (struct blockdata){255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    return data->data[c][y * 256 + z * 16 + x];
}

void setBlock(struct chunkdata* data, int64_t x, int y, int64_t z, struct blockdata bdata) {
    pthread_mutex_lock(&uclock);
    int64_t cx = 0, cz = 0;
    z *= -1;
    cx += data->info.dist;
    cz += data->info.dist;
    x += 8;
    z += 8;
    if (cx < 0 || cz < 0 || y < 0 || y > 255 || cx >= (int)data->info.width || cz >= (int)data->info.width) return;
    while (x < 0) {cx -= 1; x += 16;}
    while (x > 15) {cx += 1; x -= 16;}
    while (z > 15) {cz -= 1; z -= 16;}
    while (z < 0) {cz += 1; z += 16;}
    if (cx < 0 || cz < 0 || cx >= (int)data->info.width || cz >= (int)data->info.width) return;
    int32_t c = cx + cz * data->info.width;
    if (!data->renddata[c].generated) return;
    int32_t off = y * 256 + z * 16 + x;
    data->data[c][off].id = bdata.id;
    data->data[c][off].subid = bdata.subid;
    //updateChunk(ocx, ocz, 2);
    pthread_mutex_unlock(&uclock);
}

static inline int compare(const void* b, const void* a) {
    float fa = ((struct rendorder*)a)->dist;
    float fb = ((struct rendorder*)b)->dist;
    //printf("[%f]\n", ((struct rendorder*)a)->dist);
    return (fa > fb) - (fa < fb);
}

static inline float distance(float x1, float z1, float x2, float z2) {
    //float f = sqrt(fabs(x2 - x1) * fabs(x2 - x1) + fabs(z2 - z1) * fabs(z2 - z1));
    //printf("[%f][%f][%f][%f] [%f]\n", x1, z1, x2, z2, f);
    return /*f*/ sqrt(fabs(x2 - x1) * fabs(x2 - x1) + fabs(z2 - z1) * fabs(z2 - z1));
}

struct chunkdata allocChunks(uint32_t dist) {
    struct chunkdata chunks;
    chunks.info.dist = dist;
    dist = 1 + dist * 2;
    chunks.info.width = dist;
    dist *= dist;
    chunks.info.widthsq = dist;
    chunks.data = malloc(dist * sizeof(struct blockdata*));
    for (unsigned i = 0; i < dist; ++i) {
        chunks.data[i] = calloc(sizeof(struct blockdata), 65536);
    }
    chunks.renddata = calloc(sizeof(struct chunk_renddata), dist);
    chunks.rordr = calloc(sizeof(struct rendorder), dist);
    for (unsigned x = 0; x < chunks.info.width; ++x) {
        for (unsigned z = 0; z < chunks.info.width; ++z) {
            uint32_t c = x + z * chunks.info.width;
            chunks.rordr[c].dist = distance((int)(x - chunks.info.dist), (int)(z - chunks.info.dist), 0, 0);
            chunks.rordr[c].c = c;
        }
    }
    qsort(chunks.rordr, chunks.info.widthsq, sizeof(struct rendorder), compare);
    return chunks;
}

//static bool moved = false;

void moveChunks(struct chunkdata* chunks, int64_t cxo, int64_t czo, int cx, int cz) {
    //moved = true;
    pthread_mutex_lock(&uclock);
    struct blockdata* swap = NULL;
    struct chunk_renddata rdswap;
    int32_t nx = 0, nz = 0;
    if (cx > 0) {
        for (uint32_t c = 0; c < chunks->info.widthsq; c += chunks->info.width) {
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            for (uint32_t i = 0; i < chunks->info.width - 1; ++i) {
                chunks->data[c + i] = chunks->data[c + i + 1];
                chunks->renddata[c + i] = chunks->renddata[c + i + 1];
            }
            //chunks->renddata[c].updated = false;
            nx = (c % chunks->info.width) - chunks->info.dist;
            nz = (c / chunks->info.width) - chunks->info.dist;
            updateChunk((int64_t)((int64_t)(nx) + cxo), (int64_t)((int64_t)(-nz) + czo), CHUNKUPDATE_PRIO_LOW, 1);
            uint32_t off = c + chunks->info.width - 1;
            chunks->data[off] = swap;
            chunks->renddata[off] = rdswap;
            if (chunks->renddata[off].generated) {
                chunks->renddata[off].vcount[0] = 0;
                chunks->renddata[off].vcount[1] = 0;
                chunks->renddata[off].vcount[2] = 0;
                chunks->renddata[off].tcount[0] = 0;
                chunks->renddata[off].tcount[1] = 0;
                chunks->renddata[off].tcount[2] = 0;
                chunks->renddata[off].buffered = false;
                chunks->renddata[off].generated = false;
            }
        }
    } else if (cx < 0) {
        for (uint32_t c = 0; c < chunks->info.widthsq; c += chunks->info.width) {
            swap = chunks->data[c + chunks->info.width - 1];
            rdswap = chunks->renddata[c + chunks->info.width - 1];
            for (uint32_t i = chunks->info.width - 1; i > 0; --i) {
                chunks->data[c + i] = chunks->data[c + i - 1];
                chunks->renddata[c + i] = chunks->renddata[c + i - 1];
            }
            //chunks->renddata[c + chunks->info.width - 1].updated = false;
            nx = ((c + chunks->info.width - 1) % chunks->info.width) - chunks->info.dist;
            nz = ((c + chunks->info.width - 1) / chunks->info.width) - chunks->info.dist;
            updateChunk((int64_t)((int64_t)(nx) + cxo), (int64_t)((int64_t)(-nz) + czo), CHUNKUPDATE_PRIO_LOW, 1);
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            if (chunks->renddata[c].generated) {
                chunks->renddata[c].vcount[0] = 0;
                chunks->renddata[c].vcount[1] = 0;
                chunks->renddata[c].vcount[2] = 0;
                chunks->renddata[c].tcount[0] = 0;
                chunks->renddata[c].tcount[1] = 0;
                chunks->renddata[c].tcount[2] = 0;
                chunks->renddata[c].buffered = false;
                chunks->renddata[c].generated = false;
            }
        }
    }
    if (cz > 0) {
        for (uint32_t c = 0; c < chunks->info.widthsq; c += ((c + 1) % chunks->info.width) ? 1 : chunks->info.widthsq - chunks->info.width + 1) {
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            for (uint32_t i = 0; i < chunks->info.width - 1; ++i) {
                chunks->data[c + i * chunks->info.width] = chunks->data[c + (i + 1) * chunks->info.width];
                chunks->renddata[c + i * chunks->info.width] = chunks->renddata[c + (i + 1) * chunks->info.width];
            }
            //chunks->renddata[c].updated = false;
            nx = (c % chunks->info.width) - chunks->info.dist;
            nz = (c / chunks->info.width) - chunks->info.dist;
            updateChunk((int64_t)((int64_t)(nx) + cxo), (int64_t)((int64_t)(-nz) + czo), CHUNKUPDATE_PRIO_LOW, 1);
            uint32_t off = c + (chunks->info.width - 1) * chunks->info.width;
            chunks->data[off] = swap;
            chunks->renddata[off] = rdswap;
            if (chunks->renddata[off].generated) {
                chunks->renddata[off].vcount[0] = 0;
                chunks->renddata[off].vcount[1] = 0;
                chunks->renddata[off].vcount[2] = 0;
                chunks->renddata[off].tcount[0] = 0;
                chunks->renddata[off].tcount[1] = 0;
                chunks->renddata[off].tcount[2] = 0;
                chunks->renddata[off].buffered = false;
                chunks->renddata[off].generated = false;
            }
        }
    } else if (cz < 0) {
        for (uint32_t c = 0; c < chunks->info.widthsq; c += ((c + 1) % chunks->info.width) ? 1 : chunks->info.widthsq - chunks->info.width + 1) {
            swap = chunks->data[c + (chunks->info.width - 1) * chunks->info.width];
            rdswap = chunks->renddata[c + (chunks->info.width - 1) * chunks->info.width];
            //chunks->renddata[c].updated = false;
            nx = (c % chunks->info.width) - chunks->info.dist;
            nz = (c / chunks->info.width) - chunks->info.dist;
            updateChunk((int64_t)((int64_t)(nx) + cxo), (int64_t)((int64_t)(-nz) + czo), CHUNKUPDATE_PRIO_LOW, 1);
            //chunks->renddata[c].moved = true;
            for (uint32_t i = chunks->info.width - 1; i > 0; --i) {
                chunks->data[c + i * chunks->info.width] = chunks->data[c + (i - 1) * chunks->info.width];
                chunks->renddata[c + i * chunks->info.width] = chunks->renddata[c + (i - 1) * chunks->info.width];
            }
            //chunks->renddata[c + (chunks->info.width - 1) * chunks->info.width].updated = false;
            nx = ((c + (chunks->info.width - 1) * chunks->info.width) % chunks->info.width) - chunks->info.dist;
            nz = ((c + (chunks->info.width - 1) * chunks->info.width) / chunks->info.width) - chunks->info.dist;
            updateChunk((int64_t)((int64_t)(nx) + cxo), (int64_t)((int64_t)(-nz) + czo), CHUNKUPDATE_PRIO_LOW, 1);
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            //chunks->renddata[c].moved = true;
            if (chunks->renddata[c].generated) {
                chunks->renddata[c].vcount[0] = 0;
                chunks->renddata[c].vcount[1] = 0;
                chunks->renddata[c].vcount[2] = 0;
                chunks->renddata[c].tcount[0] = 0;
                chunks->renddata[c].tcount[1] = 0;
                chunks->renddata[c].tcount[2] = 0;
                chunks->renddata[c].buffered = false;
                chunks->renddata[c].generated = false;
            }
        }
    }
    pthread_mutex_unlock(&uclock);
}

#endif
