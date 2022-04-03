#include "chunk.h"
#include <common.h>
#include <noise.h>
#include <renderer.h>
#include <server.h>

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <math.h>

/*
struct blockdata getBlock(int cx, int cy, int cz, int x, int y, int z) {

}
*/

static int compare(const void* b, const void* a) {
    float fa = ((struct rendorder*)a)->dist;
    float fb = ((struct rendorder*)b)->dist;
    //printf("[%f]\n", ((struct rendorder*)a)->dist);
    return (fa > fb) - (fa < fb);
}

static float distance(float x1, float z1, float x2, float z2) {
    //float f = sqrt(fabs(x2 - x1) * fabs(x2 - x1) + fabs(z2 - z1) * fabs(z2 - z1));
    //printf("[%f][%f][%f][%f] [%f]\n", x1, z1, x2, z2, f);
    return /*f*/ sqrt(fabs(x2 - x1) * fabs(x2 - x1) + fabs(z2 - z1) * fabs(z2 - z1));
}

struct chunkdata allocChunks(uint32_t dist) {
    struct chunkdata chunks;
    chunks.dist = dist;
    dist = 1 + dist * 2;
    chunks.width = dist;
    dist *= dist;
    chunks.widthsq = dist;
    dist *= 16;
    chunks.size = dist;
    chunks.data = malloc(dist * sizeof(struct blockdata*));
    for (unsigned i = 0; i < dist; ++i) {
        chunks.data[i] = calloc(4096 * sizeof(struct blockdata), 1);
    }
    chunks.renddata = calloc(dist * sizeof(struct chunk_renddata), 1);
    chunks.rordr = calloc(chunks.widthsq * sizeof(struct rendorder), 1);
    for (unsigned x = 0; x < chunks.width; ++x) {
        for (unsigned z = 0; z < chunks.width; ++z) {
            uint32_t c = x + z * chunks.width;
            //printf("[%f]\n", distance(x + chunks.dist, z + chunks.dist, 0, 0));
            chunks.rordr[c].dist = distance((int)(x - chunks.dist), (int)(z - chunks.dist), 0, 0);
            chunks.rordr[c].c = c;
        }
    }
    qsort(chunks.rordr, chunks.widthsq, sizeof(struct rendorder), compare);
    /*
    for (unsigned i = 0; i < chunks.widthsq; ++i) {
        printf("[%9f][%3d]\n", chunks.rordr[i].dist, chunks.rordr[i].c);
    }
    */
    return chunks;
}

void moveChunks(struct chunkdata* chunks, int cx, int cz) {
    struct blockdata* swap = NULL;
    struct chunk_renddata rdswap;
    if (cx > 0) {
        for (uint32_t c = 0; c < chunks->size; c += chunks->width) {
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            chunks->renddata[c + chunks->width - 1].updated = false;
            for (uint32_t i = 0; i < chunks->width - 1; ++i) {
                chunks->data[c + i] = chunks->data[c + i + 1];
                chunks->renddata[c + i] = chunks->renddata[c + i + 1];
            }
            uint32_t off = c + chunks->width - 1;
            chunks->data[off] = swap;
            chunks->renddata[off] = rdswap;
            chunks->renddata[off].updated = false;
            chunks->renddata[off].generated = false;
            chunks->renddata[off].vcount = 0;
            chunks->renddata[off].vcount2 = 0;
        }
    } else if (cx < 0) {
        for (uint32_t c = 0; c < chunks->size; c += chunks->width) {
            swap = chunks->data[c + chunks->width - 1];
            rdswap = chunks->renddata[c + chunks->width - 1];
            chunks->renddata[c].updated = false;
            for (uint32_t i = chunks->width - 1; i > 0; --i) {
                chunks->data[c + i] = chunks->data[c + i - 1];
                chunks->renddata[c + i] = chunks->renddata[c + i - 1];
            }
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            chunks->renddata[c].updated = false;
            chunks->renddata[c].generated = false;
            chunks->renddata[c].vcount = 0;
            chunks->renddata[c].vcount2 = 0;
        }
    }
    if (cz > 0) {
        for (uint32_t c = 0; c < chunks->size; c += ((c + 1) % chunks->width) ? 1 : chunks->widthsq - chunks->width + 1) {
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            chunks->renddata[c + (chunks->width - 1) * chunks->width].updated = false;
            for (uint32_t i = 0; i < chunks->width - 1; ++i) {
                chunks->data[c + i * chunks->width] = chunks->data[c + (i + 1) * chunks->width];
                chunks->renddata[c + i * chunks->width] = chunks->renddata[c + (i + 1) * chunks->width];
            }
            uint32_t off = c + (chunks->width - 1) * chunks->width;
            chunks->data[off] = swap;
            chunks->renddata[off] = rdswap;
            chunks->renddata[off].updated = false;
            chunks->renddata[off].generated = false;
            chunks->renddata[off].vcount = 0;
            chunks->renddata[off].vcount2 = 0;
        }
    } else if (cz < 0) {
        for (uint32_t c = 0; c < chunks->size; c += ((c + 1) % chunks->width) ? 1 : chunks->widthsq - chunks->width + 1) {
            swap = chunks->data[c + (chunks->width - 1) * chunks->width];
            rdswap = chunks->renddata[c + (chunks->width - 1) * chunks->width];
            chunks->renddata[c].updated = false;
            for (uint32_t i = chunks->width - 1; i > 0; --i) {
                chunks->data[c + i * chunks->width] = chunks->data[c + (i - 1) * chunks->width];
                chunks->renddata[c + i * chunks->width] = chunks->renddata[c + (i - 1) * chunks->width];
            }
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            chunks->renddata[c].updated = false;
            chunks->renddata[c].generated = false;
            chunks->renddata[c].vcount = 0;
            chunks->renddata[c].vcount2 = 0;
        }
    }
}

/*
void moveChunksMult(struct chunkdata* chunks, int cx, int cz) {
}
*/

bool genChunkColumn(struct chunkdata* chunks, int cx, int cz, int xo, int zo) {
    int nx = (cx + xo) * 16;
    int nz = (cz * -1 + zo) * 16;
    cx += chunks->dist;
    cz += chunks->dist;
    uint32_t coff = cz * chunks->width + cx;
    bool ct = 0;
    for (int cy = 0; cy < 16; ++cy) {
        if (chunks->renddata[coff].updated) goto skipfor;
        memset(chunks->data[coff], 0, 4096 * sizeof(struct blockdata));
        //chunks->renddata[coff].pos = (coord_3d){(float)(cx - (int)chunks->dist), (float)(cy), (float)(cz - (int)chunks->dist)};
        //printf("set chunk pos [%u]: [%d][%d][%d]\n", coff, cx - chunks->dist, cy, cz - chunks->dist);
        int btm = cy * 16;
        int top = (cy + 1) * 16 - 1;
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                //if (!cy) printf("noise coords (% 2d, % 2d, % 2d) [% 3d][% 3d]\n", cx, cy, cz, nx + x, nz + z);
                float s1 = perlin2d(0, (float)(nx + x) / 19, (float)(nz + z) / 19, 1.0, 1);
                float s2 = perlin2d(1, (float)(nx + x) / 257, (float)(nz + z) / 257, .5, 1);
                float s3 = perlin2d(2, (float)(nx + x) / 167, (float)(nz + z) / 167, 1.0, 1);
                float s4 = perlin2d(3, (float)(nx + x) / 74, (float)(nz + z) / 74, 1.0, 1);
                float s5 = perlin2d(4, (float)(nx + x) / 64, (float)(nz + z) / 64, 1.0, 1);
                //printf("[%lf][%lf]", s, s2);
                for (int y = btm; y <= top && y < 75; ++y) {
                    //printf("placing water at chunk [%u] [%d, %d, %d]\n", coff, x, y, z);
                    chunks->data[coff][(y - btm) * 256 + z * 16 + x] = (struct blockdata){7, 0, 0, 0};
                }
                float s = s1;
                s *= s5 * 4;
                s += s2 * 48 + (1 - s5) * 4;
                s -= 10;
                float sf = (float)(s * 2) - 8;
                int si = (int)sf + 70;
                //printf("[%d]\n", si);
                uint8_t blockid = (s1 + s2 < s3 * 1.125) ? 8 : 3;
                blockid = (sf < 5.5) ? ((sf < -((s4 + 2.25) * 7)) ? 2 : 8) : blockid;
                if (si >= btm && si <= top) chunks->data[coff][(si - btm) * 256 + z * 16 + x] = (struct blockdata){blockid, 0, 0, 0};
                for (int y = ((si - 1) > top) ? top : si - 1; y >= btm; --y) {
                    chunks->data[coff][(y - btm) * 256 + z * 16 + x] = (struct blockdata){(y < (float)(si) * 0.9) ? 1 : 2, 0, 0, 0};
                }
                if (!btm) chunks->data[coff][z * 16 + x] = (struct blockdata){6, 0, 0, 0};
                //if (si < 3) si = 3;
                //if (!x && !z) rendinf.campos.y += (float)si;
            }
        }
        ct = true;
        chunks->renddata[coff].generated = true;
        if ((int)coff % (int)chunks->widthsq >= (int)chunks->width) {
            int32_t coff2 = coff - chunks->width;
            if (coff2 >= 0 || coff2 < (int32_t)chunks->size) chunks->renddata[coff2].updated = false;
        }
        if ((int)coff % (int)chunks->widthsq < (int)(chunks->widthsq - chunks->width)) {
            int32_t coff2 = coff + chunks->width;
            if (coff2 >= 0 || coff2 < (int32_t)chunks->size) chunks->renddata[coff2].updated = false;
        }
        if (coff % chunks->width) {
            int32_t coff2 = coff - 1;
            if (coff2 >= 0 || coff2 < (int32_t)chunks->size) chunks->renddata[coff2].updated = false;
        }
        if ((coff + 1) % chunks->width) {
            int32_t coff2 = coff + 1;
            if (coff2 >= 0 || coff2 < (int32_t)chunks->size) chunks->renddata[coff2].updated = false;
        }
        skipfor:;
        coff += chunks->widthsq;
    }
    return ct;
}

void genChunks(struct chunkdata* chunks, int xo, int zo) {
    //uint64_t starttime = altutime();
    //uint32_t ct = 0;
    //uint32_t maxct = 1;
    for (int i = 0; i <= (int)chunks->dist; ++i) {
        for (int z = -i; z <= i; ++z) {
            for (int x = -i; x <= i; ++x) {
                if (abs(z) == i || (abs(z) != i && abs(x) == i)) {
                    /*ct += *///genChunkColumn(chunks, x, z, xo, zo);
                    //printf("[%d][%d] [%u]\n", x, z, ct);
                    //if (ct > maxct - 1) goto ret;
                    //while (
                    struct server_chunk* srvchunk = malloc(sizeof(struct server_chunk));
                    *srvchunk = (struct server_chunk){chunks, x, z, xo, zo};
                    servSend(SERVER_MSG_GETCHUNK, srvchunk, true, true);
                    //if (altutime() - starttime >= 1000000 / (((rendinf.fps) ? rendinf.fps : 60) * 3)) goto ret;
                }
            }
        }
    }
    //ret:;
    //printf("generated in: [%f]s\n", (float)(altutime() - starttime) / 1000000.0);
}
