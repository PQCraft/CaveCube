#include "chunk.h"
#include <common.h>
#include <noise.h>
#include <renderer.h>

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

/*
#ifdef THREADING
    #undef THREADING // Not worth it atm (the overhead of threading outweighs the overhead of chunk generation)
#endif
*/

chunkdata allocChunks(uint32_t dist) {
    chunkdata chunks;
    chunks.dist = dist;
    dist = 1 + dist * 2;
    chunks.width = dist;
    dist *= dist;
    chunks.widthsq = dist;
    dist *= 16;
    chunks.size = dist;
    chunks.data = malloc(dist * sizeof(blockdata*));
    for (unsigned i = 0; i < dist; ++i) {
        chunks.data[i] = calloc(4096 * sizeof(blockdata), 1);
    }
    chunks.renddata = calloc(dist * sizeof(chunk_renddata), 1);
    return chunks;
}

void moveChunks(chunkdata* chunks, int cx, int cz) {
    blockdata* swap = NULL;
    chunk_renddata rdswap;
    if (cx > 0) {
        for (uint32_t c = 0; c < chunks->size; c += chunks->width) {
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            chunks->renddata[c + chunks->width - 1].updated = false;
            for (uint32_t i = 0; i < chunks->width - 1; ++i) {
                chunks->data[c + i] = chunks->data[c + i + 1];
                chunks->renddata[c + i] = chunks->renddata[c + i + 1];
            }
            chunks->data[c + chunks->width - 1] = swap;
            chunks->renddata[c + chunks->width - 1] = rdswap;
            chunks->renddata[c + chunks->width - 1].updated = false;
            chunks->renddata[c + chunks->width - 1].vcount = 0;
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
            chunks->renddata[c].vcount = 0;
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
            chunks->data[c + (chunks->width - 1) * chunks->width] = swap;
            chunks->renddata[c + (chunks->width - 1) * chunks->width] = rdswap;
            chunks->renddata[c + (chunks->width - 1) * chunks->width].updated = false;
            chunks->renddata[c + (chunks->width - 1) * chunks->width].vcount = 0;
        }
    } else if (cz < 0) {
        for (uint32_t c = 0; c < chunks->size; c += ((c + 1) % chunks->width) ? 1 : chunks->widthsq - chunks->width + 1) {
            //printf("saving buffer [%u]\n", c);
            swap = chunks->data[c + (chunks->width - 1) * chunks->width];
            rdswap = chunks->renddata[c + (chunks->width - 1) * chunks->width];
            chunks->renddata[c].updated = false;
            for (uint32_t i = chunks->width - 1; i > 0; --i) {
                //printf("replacing buffer [%u] with [%u]\n", c + i * chunks->width, c + (i + 1) * chunks->width);
                chunks->data[c + i * chunks->width] = chunks->data[c + (i - 1) * chunks->width];
                chunks->renddata[c + i * chunks->width] = chunks->renddata[c + (i - 1) * chunks->width];
            }
            //printf("writing buffer [%u] to [%u]\n", c, c + (chunks->width - 1) * chunks->width);
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            //printf("flagging update on buffer [%u]\n", c + (chunks->width - 1) * chunks->width);
            chunks->renddata[c].updated = false;
            chunks->renddata[c].vcount = 0;
        }
    }
}

/*
void moveChunksMult(chunkdata* chunks, int cx, int cz) {
}
*/

void genChunkColumn(chunkdata* chunks, int cx, int cz, int xo, int zo) {
    int nx = (cx + xo) * 16;
    int nz = (cz * -1 + zo) * 16;
    cx += chunks->dist;
    cz += chunks->dist;
    uint32_t coff = cz * chunks->width + cx;
    for (int cy = 0; cy < 16; ++cy) {
        if (chunks->renddata[coff].updated) goto skipfor;
        memset(chunks->data[coff], 0, 4096 * sizeof(blockdata));
        //chunks->renddata[coff].pos = (coord_3d){(float)(cx - (int)chunks->dist), (float)(cy), (float)(cz - (int)chunks->dist)};
        //printf("set chunk pos [%u]: [%d][%d][%d]\n", coff, cx - chunks->dist, cy, cz - chunks->dist);
        int btm = cy * 16;
        int top = (cy + 1) * 16 - 1;
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                //if (!cy) printf("noise coords (% 2d, % 2d, % 2d) [% 3d][% 3d]\n", cx, cy, cz, nx + x, nz + z);
                double s1 = perlin2d(0, (double)(nx + x) / 16, (double)(nz + z) / 16, 1.0, 1);
                double s2 = perlin2d(1, (double)(nx + x) / 160, (double)(nz + z) / 160, 1.0, 1);
                //printf("[%lf][%lf]", s, s2);
                for (int y = btm; y < 14; ++y) {
                    chunks->data[coff][y * 256 + z * 16 + x] = (blockdata){7, 0};
                }
                double s = s1;
                s *= 4;
                s += s2 * 32;
                float sf = (float)(s * 2) - 8;
                int si = (int)sf;
                //printf("[%d]\n", si);
                uint8_t blockid = (s1 + s2 < perlin2d(2, (double)(nx + x) / 67, (double)(nz + z) / 67, 1.0, 1) * 1.125) ? 8 : 3;
                blockid = (sf < 14.5) ? 8 : blockid;
                if (si >= btm && si <= top) chunks->data[coff][(si - btm) * 256 + z * 16 + x] = (blockdata){blockid, 0};
                for (int y = ((si - 1) > top) ? top : si - 1; y >= btm; --y) {
                    chunks->data[coff][(y - btm) * 256 + z * 16 + x] = (blockdata){2, 0};
                }
                if (!btm) chunks->data[coff][z * 16 + x] = (blockdata){6, 0};
                //if (si < 3) si = 3;
                //if (!x && !z) rendinf.campos.y += (float)si;
            }
        }
        skipfor:;
        coff += chunks->widthsq;
    }
}

void genChunks(chunkdata* chunks, int xo, int zo) {
    uint64_t starttime = altutime();
    for (int z = -(int)chunks->dist; z <= (int)chunks->dist; ++z) {
        for (int x = -(int)chunks->dist; x <= (int)chunks->dist; ++x) {
            genChunkColumn(chunks, x, z, xo, zo);
        }
    }
    printf("generated in: [%f]s\n", (float)(altutime() - starttime) / 1000000.0);
}
