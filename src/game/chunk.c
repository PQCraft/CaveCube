#ifndef SERVER

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

/*
void chunkUpdate(struct chunkdata* data, int32_t c) {
    if ((int)c >= (int)data->info.width) {
        int32_t c2 = c - data->info.width;
        if ((c2 >= 0 || c2 < (int32_t)data->info.widthsq) && data->renddata[c2].generated) data->renddata[c2].updated = false;
    }
    if ((int)c < (int)(data->info.widthsq - data->info.width)) {
        int32_t c2 = c + data->info.width;
        if ((c2 >= 0 || c2 < (int32_t)data->info.widthsq) && data->renddata[c2].generated) data->renddata[c2].updated = false;
    }
    if (c % data->info.width) {
        int32_t c2 = c - 1;
        if ((c2 >= 0 || c2 < (int32_t)data->info.widthsq) && data->renddata[c2].generated) data->renddata[c2].updated = false;
    }
    if ((c + 1) % data->info.width) {
        int32_t c2 = c + 1;
        if ((c2 >= 0 || c2 < (int32_t)data->info.widthsq) && data->renddata[c2].generated) data->renddata[c2].updated = false;
    }
}
*/

struct blockdata getBlock(struct chunkdata* data, int cx, int cz, int x, int y, int z) {
    cz *= -1;
    z *= -1;
    //printf("in: [%d, %d, %d] [%d, %d, %d]\n", cx, cy, cz, x, y, z);
    cx += data->info.dist;
    cz += data->info.dist;
    x += 8;
    z += 8;
    if (cx < 0 || cz < 0 || y < 0 || y > 255 || cx >= (int)data->info.width || cz >= (int)data->info.width ) return (struct blockdata){0, 0, 0};
    int32_t c = cx + cz * data->info.width;
    while (x < 0 && c % data->info.width) {c -= 1; x += 16;}
    while (x > 15 && (c + 1) % data->info.width) {c += 1; x -= 16;}
    while (z > 15 && c >= (int)data->info.width) {c -= data->info.width; z -= 16;}
    while (z < 0 && c < (int)(data->info.widthsq - data->info.width)) {c += data->info.width; z += 16;}
    cx = c % data->info.width;
    cz = c / data->info.width % data->info.width;
    //printf("resolved: [%d, %d, %d] [%d, %d, %d]\n", cx, cy, cz, x, y, z);
    if (c < 0 || c >= (int32_t)data->info.widthsq || x < 0 || z < 0 || x > 15 || z > 15) return (struct blockdata){0, 0, 0};
    if (!data->renddata[c].generated) return (struct blockdata){255, 0, 0};
    return data->data[c][y * 256 + z * 16 + x];
}

static int64_t cxo = 0, czo = 0;
//static pthread_mutex_t cidlock;

void setBlock(struct chunkdata* data, int cx, int cz, int x, int y, int z, struct blockdata bdata) {
    pthread_mutex_lock(&uclock);
    cz *= -1;
    z *= -1;
    //printf("in: [%d, %d, %d] [%d, %d, %d]\n", cx, cy, cz, x, y, z);
    cx += data->info.dist;
    cz += data->info.dist;
    x += 8;
    z += 8;
    if (cx < 0 || cz < 0 || y < 0 || y > 255 || cx >= (int)data->info.width || cz >= (int)data->info.width) return;
    while (x < 0) {cx -= 1; x += 16;}
    while (x > 15) {cx += 1; x -= 16;}
    while (z > 15) {cz -= 1; z -= 16;}
    while (z < 0) {cz += 1; z += 16;}
    //printf("resolved: [%d, %d, %d] [%d, %d, %d]\n", cx, cy, cz, x, y, z);
    if (cx >= (int)data->info.width || cz >= (int)data->info.width) return;
    int32_t c = cx + cz * data->info.width;
    if (!data->renddata[c].generated) return;
    data->data[c][y * 256 + z * 16 + x].id = bdata.id;
    updateChunk((int64_t)((int64_t)(cx) - data->info.dist + cxo), (int64_t)((int64_t)(-cz) + data->info.dist + czo), false);
    pthread_mutex_unlock(&uclock);
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
        chunks.data[i] = calloc(65536 * sizeof(struct blockdata), 1);
    }
    chunks.renddata = calloc(dist * sizeof(struct chunk_renddata), 1);
    return chunks;
}

//static bool moved = false;

void moveChunks(struct chunkdata* chunks, int cx, int cz) {
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
            updateChunk((int64_t)((int64_t)(nx) + cxo), (int64_t)((int64_t)(-nz) + czo), false);
            uint32_t off = c + chunks->info.width - 1;
            chunks->data[off] = swap;
            chunks->renddata[off] = rdswap;
            if (chunks->renddata[off].generated) {
                chunks->renddata[off].vcount = 0;
                chunks->renddata[off].vcount2 = 0;
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
            updateChunk((int64_t)((int64_t)(nx) + cxo), (int64_t)((int64_t)(-nz) + czo), false);
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            if (chunks->renddata[c].generated) {
                chunks->renddata[c].vcount = 0;
                chunks->renddata[c].vcount2 = 0;
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
            updateChunk((int64_t)((int64_t)(nx) + cxo), (int64_t)((int64_t)(-nz) + czo), false);
            uint32_t off = c + (chunks->info.width - 1) * chunks->info.width;
            chunks->data[off] = swap;
            chunks->renddata[off] = rdswap;
            if (chunks->renddata[off].generated) {
                chunks->renddata[off].vcount = 0;
                chunks->renddata[off].vcount2 = 0;
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
            updateChunk((int64_t)((int64_t)(nx) + cxo), (int64_t)((int64_t)(-nz) + czo), false);
            //chunks->renddata[c].moved = true;
            for (uint32_t i = chunks->info.width - 1; i > 0; --i) {
                chunks->data[c + i * chunks->info.width] = chunks->data[c + (i - 1) * chunks->info.width];
                chunks->renddata[c + i * chunks->info.width] = chunks->renddata[c + (i - 1) * chunks->info.width];
            }
            //chunks->renddata[c + (chunks->info.width - 1) * chunks->info.width].updated = false;
            nx = ((c + (chunks->info.width - 1) * chunks->info.width) % chunks->info.width) - chunks->info.dist;
            nz = ((c + (chunks->info.width - 1) * chunks->info.width) / chunks->info.width) - chunks->info.dist;
            updateChunk((int64_t)((int64_t)(nx) + cxo), (int64_t)((int64_t)(-nz) + czo), false);
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            //chunks->renddata[c].moved = true;
            if (chunks->renddata[c].generated) {
                chunks->renddata[c].vcount = 0;
                chunks->renddata[c].vcount2 = 0;
                chunks->renddata[c].buffered = false;
                chunks->renddata[c].generated = false;
            }
        }
    }
    pthread_mutex_unlock(&uclock);
}

void writeChunk(struct chunkdata* chunks, int64_t x, int64_t z, struct blockdata* data) {
    pthread_mutex_lock(&uclock);
    int64_t nx = (x - cxo) + chunks->info.dist;
    int64_t nz = chunks->info.width - ((z - czo) + chunks->info.dist) - 1;
    if (nx < 0 || nz < 0 || nx >= chunks->info.width || nz >= chunks->info.width) {
        pthread_mutex_unlock(&uclock);
        return;
    }
    uint32_t coff = nx + nz * chunks->info.width;
    //printf("writing chunk to [%"PRId64", %"PRId64"] ([%"PRId64", %"PRId64"])\n", nx, nz, x, z);
    memcpy(chunks->data[coff], data, 65536 * sizeof(struct blockdata));
    //chunks->renddata[coff].updated = false;
    updateChunk(x, z, true);
    chunks->renddata[coff].generated = true;
    pthread_mutex_unlock(&uclock);
}

void reqChunks(struct chunkdata* chunks, int64_t xo, int64_t zo) {
    /*
    static bool init = false;
    if (!init) {
        pthread_mutex_init(&cidlock, NULL);
        init = true;
    }
    */
    pthread_mutex_lock(&uclock);
    cxo = xo;
    czo = zo;
    setMeshChunkOff(xo, zo);
    //printf("set [%u]\n", cid);
    pthread_mutex_unlock(&uclock);
    for (int i = 0; i <= (int)chunks->info.dist; ++i) {
        for (int z = -i; z <= i; ++z) {
            for (int x = -i; x <= i; ++x) {
                if (abs(z) == i || (abs(z) != i && abs(x) == i)) {
                    uint32_t coff = (z + chunks->info.dist) * chunks->info.width + (x + chunks->info.dist);
                    if (!chunks->renddata[coff].generated) {
                        //printf("REQ [%"PRId64", %"PRId64"]\n", (int64_t)((int64_t)(x) + xo), (int64_t)((int64_t)(-z) + zo));
                        cliSend(CLIENT_GETCHUNK, (int64_t)((int64_t)(x) + xo), (int64_t)((int64_t)(-z) + zo));
                    }
                }
            }
        }
    }
}

#endif
