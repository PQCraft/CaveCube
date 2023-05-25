#if defined(MODULE_GAME)

#include <main/main.h>
#include "chunk.h"
#include "blocks.h"
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

int findChunkDataTop(struct blockdata* data) {
    int maxy = 511;
    struct blockdata* b = &data[131071];
    while (maxy >= 0) {
        for (int i = 0; i < 256; ++i) {
            if (bdgetid(*b)) goto foundblock;
            --b;
        }
        --maxy;
    }
    foundblock:;
    return maxy;
}

int findChunkTop(struct chunkdata* data, int c) {
    return findChunkDataTop(data->data[c]);
}

bool allocexactchunkheight = false;

void resizeChunkTo(struct chunkdata* data, int c, int top) {
    struct chunk_metadata* m = &data->metadata[c];
    if (top != m->top) {
        m->top = top;
        m->sects = (top + 16) / 16;
        m->alignedtop = m->sects * 16 - 1;
        int size;
        if (allocexactchunkheight) {
            size = (m->top + 1) * 256 * sizeof(**data->data);
        } else {
            size = m->sects * 4096 * sizeof(**data->data);
        }
        //printf("RESIZE [%d]: top=%d, alignedtop=%d, sects=%d, size=%gKiB\n",
        //    c, m->top, m->alignedtop, m->sects, (float)(m->sects * 4096 * sizeof(**data->data)) / 1024.0);
        data->data[c] = realloc(data->data[c], size);
    }
}

void resizeChunk(struct chunkdata* data, int c) {
    int top = findChunkTop(data, c);
    resizeChunkTo(data, c, top);
}
void extendChunk(struct chunkdata* data, int c) {
    struct chunk_metadata* m = &data->metadata[c];
    m->top = 511;
    m->alignedtop = 511;
    m->sects = 32;
    //printf("EXTEND [%d]: top=%d, alignedtop=%d, sects=%d, size=%gKiB\n",
    //    c, m->top, m->alignedtop, m->sects, (float)(131072 * sizeof(**data->data)) / 1024.0);
    data->data[c] = realloc(data->data[c], 131072 * sizeof(**data->data));
}

void getBlock(struct chunkdata* data, int64_t x, int y, int64_t z, struct blockdata* b) {
    if (y < 0 || y > 511) {bdsetid(b, BLOCKNO_NULL); return;}
    int64_t cx, cz;
    _getChunkOfBlock(x, z, &cx, &cz);
    //printf("[%"PRId64"][%"PRId64"] -> [%"PRId64"][%"PRId64"]\n", x, z, cx, cz);
    cx = (cx - data->xoff) + data->info.dist;
    cz = data->info.width - ((cz - data->zoff) + data->info.dist) - 1;
    if (cx < 0 || cz < 0 || cx >= data->info.width || cz >= data->info.width) {bdsetid(b, BLOCKNO_BORDER); return;}
    x += 8;
    z += 8;
    x = i64_mod(x, 16);
    z = i64_mod(z, 16);
    //printf("[%"PRId64"][%"PRId64"], [%"PRId64"][%"PRId64"]\n", x, z, cx, cz);
    int c = cx + cz * data->info.width;
    if (!data->renddata[c].generated) {bdsetid(b, BLOCKNO_BORDER); return;}
    if (y > data->metadata[c].top) {bdsetid(b, BLOCKNO_NULL); return;}
    *b = data->data[c][y * 256 + z * 16 + x];
}

void setBlock(struct chunkdata* data, int64_t x, int y, int64_t z, struct blockdata bdata) {
    pthread_mutex_lock(&data->lock);
    if (y < 0 || y > 511) goto ret;
    int64_t cx, cz;
    _getChunkOfBlock(x, z, &cx, &cz);
    cx = (cx - data->xoff) + data->info.dist;
    cz = data->info.width - ((cz - data->zoff) + data->info.dist) - 1;
    if (cx < 0 || cz < 0 || cx >= data->info.width || cz >= data->info.width) goto ret;
    x = i64_mod(x + 8, 16);
    z = 15 - i64_mod(z + 8, 16);
    int c = cx + cz * data->info.width;
    if (!data->renddata[c].generated) goto ret;
    if (y > data->metadata[c].top) goto ret;
    data->data[c][y * 256 + z * 16 + x] = bdata;
    ret:;
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

static void initChunks(struct chunkdata* chunks, int dist) {
    chunks->info.dist = dist;
    dist = 1 + dist * 2;
    chunks->info.width = dist;
    dist *= dist;
    chunks->info.widthsq = dist;
    chunks->data = malloc(dist * sizeof(*chunks->data));
    for (int i = 0; i < dist; ++i) {
        chunks->data[i] = malloc(0);
    }
    chunks->metadata = calloc(sizeof(*chunks->metadata), dist);
    for (int i = 0; i < dist; ++i) {
        struct chunk_metadata* m = &chunks->metadata[i];
        m->top = -1;
        m->alignedtop = -1;
        m->sects = 0;
    }
    chunks->renddata = calloc(sizeof(*chunks->renddata), dist);
    for (int i = 0; i < dist; ++i) {
        memset(chunks->renddata[i].vispass, 1, sizeof(chunks->renddata->vispass));
    }
    chunks->rordr = calloc(sizeof(*chunks->rordr), dist);
    for (unsigned x = 0; x < chunks->info.width; ++x) {
        for (unsigned z = 0; z < chunks->info.width; ++z) {
            uint32_t c = x + z * chunks->info.width;
            chunks->rordr[c].dist = distance((int)(x - chunks->info.dist), (int)(chunks->info.dist - z), 0, 0);
            chunks->rordr[c].c = c;
        }
    }
    qsort(chunks->rordr, chunks->info.widthsq, sizeof(*chunks->rordr), compare);
}

static void deinitChunks(struct chunkdata* chunks) {
    for (int i = 0; i < (int)chunks->info.widthsq; ++i) {
        free(chunks->data[i]);
        free(chunks->renddata[i].vertices[0]);
        free(chunks->renddata[i].vertices[1]);
        free(chunks->renddata[i].sortvert);
    }
    free(chunks->data);
    free(chunks->metadata);
    free(chunks->renddata);
    free(chunks->rordr);
}

struct chunkdata* allocChunks(int dist) {
    if (dist < 1) dist = 1;
    struct chunkdata* chunks = malloc(sizeof(*chunks));
    pthread_mutex_init(&chunks->lock, NULL);
    initChunks(chunks, dist);
    chunks->xoff = 0;
    chunks->zoff = 0;
    return chunks;
}

void resizeChunks(struct chunkdata* chunks, int dist) {
    if (dist < 1) dist = 1;
    pthread_mutex_lock(&chunks->lock);
    deinitChunks(chunks);
    initChunks(chunks, dist);
    pthread_mutex_unlock(&chunks->lock);
}

void freeChunks(struct chunkdata* chunks) {
    pthread_mutex_lock(&chunks->lock);
    deinitChunks(chunks);
    pthread_mutex_unlock(&chunks->lock);
    pthread_mutex_destroy(&chunks->lock);
    free(chunks);
}

static inline void nullattrib(struct chunkdata* chunks, int c) {
    if (chunks->renddata[c].generated) {
        chunks->renddata[c].vcount[0] = 0;
        chunks->renddata[c].vcount[1] = 0;
        chunks->renddata[c].tcount[0] = 0;
        chunks->renddata[c].tcount[1] = 0;
        chunks->renddata[c].buffered = false;
        chunks->renddata[c].generated = false;
    }
    memset(chunks->renddata[c].vispass, 1, sizeof(chunks->renddata->vispass));
    chunks->renddata[c].requested = false;
    chunks->metadata[c].top = -1;
    chunks->metadata[c].alignedtop = -1;
    chunks->metadata[c].sects = 0;
    chunks->data[c] = realloc(chunks->data[c], 0);
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
    struct chunk_metadata mdswap;
    int c;
    for (; cx < 0; ++cx) { // move left (player goes right)
        for (unsigned z = 0; z < chunks->info.width; ++z) {
            c = (chunks->info.width - 1) + z * chunks->info.width;
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            mdswap = chunks->metadata[c];
            for (int x = chunks->info.width - 1; x >= 1; --x) {
                c = x + z * chunks->info.width;
                chunks->data[c] = chunks->data[c - 1];
                chunks->renddata[c] = chunks->renddata[c - 1];
                chunks->metadata[c] = chunks->metadata[c - 1];
            }
            c = z * chunks->info.width;
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            chunks->metadata[c] = mdswap;
            nullattrib(chunks, c);
        }
    }
    for (; cx > 0; --cx) { // move right (player goes left)
        for (unsigned z = 0; z < chunks->info.width; ++z) {
            c = z * chunks->info.width;
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            mdswap = chunks->metadata[c];
            for (int x = 0; x < (int)chunks->info.width - 1; ++x) {
                c = x + z * chunks->info.width;
                chunks->data[c] = chunks->data[c + 1];
                chunks->renddata[c] = chunks->renddata[c + 1];
                chunks->metadata[c] = chunks->metadata[c + 1];
            }
            c = (chunks->info.width - 1) + z * chunks->info.width;
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            chunks->metadata[c] = mdswap;
            nullattrib(chunks, c);
        }
    }
    for (; cz < 0; ++cz) { // move forward (player goes backward)
        for (unsigned x = 0; x < chunks->info.width; ++x) {
            c = x;
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            mdswap = chunks->metadata[c];
            for (int z = 0; z < (int)chunks->info.width - 1; ++z) {
                c = x + z * chunks->info.width;
                chunks->data[c] = chunks->data[c + chunks->info.width];
                chunks->renddata[c] = chunks->renddata[c + chunks->info.width];
                chunks->metadata[c] = chunks->metadata[c + chunks->info.width];
            }
            c = x + (chunks->info.width - 1) * chunks->info.width;
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            chunks->metadata[c] = mdswap;
            nullattrib(chunks, c);
        }
    }
    for (; cz > 0; --cz) { // move backward (player goes forward)
        for (unsigned x = 0; x < chunks->info.width; ++x) {
            c = x + (chunks->info.width - 1) * chunks->info.width;
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            mdswap = chunks->metadata[c];
            for (int z = chunks->info.width - 1; z >= 1; --z) {
                c = x + z * chunks->info.width;
                chunks->data[c] = chunks->data[c - chunks->info.width];
                chunks->renddata[c] = chunks->renddata[c - chunks->info.width];
                chunks->metadata[c] = chunks->metadata[c - chunks->info.width];
            }
            c = x;
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            chunks->metadata[c] = mdswap;
            nullattrib(chunks, c);
        }
    }
    pthread_mutex_unlock(&chunks->lock);
}

#endif
