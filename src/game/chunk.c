#include "chunk.h"
#include <common.h>
#include <noise.h>

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

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

void genChunkColumn(chunkdata* chunks, int cx, int cz, int xo, int zo) {
    int nx = (cx + xo) * 16;
    int nz = (cz * -1 + zo) * 16;
    cx += chunks->dist;
    cz += chunks->dist;
    uint32_t coff = cz * chunks->width + cx;
    for (int cy = 0; cy < 16; ++cy) {
        chunks->renddata[coff].pos = (coord_3d){(float)(cx - (int)chunks->dist), (float)(cy), (float)(cz - (int)chunks->dist)};
        //printf("set chunk pos [%u]: [%d][%d][%d]\n", coff, cx - chunks->dist, cy, cz - chunks->dist);
        int btm = cy * 16;
        int top = (cy + 1) * 16 - 1;
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                //if (!cy) printf("noise coords (% 2d, % 2d, % 2d) [% 3d][% 3d]\n", cx, cy, cz, nx + x, nz + z);
                double s = perlin2d(0, (double)(nx + x) / 16, (double)(nz + z) / 16, 1.0, 1);
                double s2 = perlin2d(1, (double)(nx + x) / 160, (double)(nz + z) / 160, 1.0, 1);
                //printf("[%lf][%lf]", s, s2);
                for (register int y = btm; y < 4; ++y) {
                    chunks->data[coff][y * 256 + z * 16 + x] = (blockdata){7, 0};
                }
                s *= 5;
                s += s2 * 32;
                int si = (int)(float)(s * 2) - 4;
                //printf("[%d]\n", si);
                if (si >= btm && si <= top) chunks->data[coff][(si - btm) * 256 + z * 16 + x] = (blockdata){(si > 12) ? 5 : 3, 0};
                for (register int y = ((si - 1) > top) ? top : si - 1; y > btm; --y) {
                    chunks->data[coff][y * 256 + z * 16 + x] = (blockdata){2, 0};
                }
                if (!btm) chunks->data[coff][z * 16 + x] = (blockdata){6, 0};
                //if (si < 3) si = 3;
                //if (!x && !z) rendinf.campos.y += (float)si;
            }
        }
        coff += chunks->widthsq;
    }
}

void genChunks(chunkdata* chunks, int xo, int zo) {
    for (int z = -(int)chunks->dist; z <= (int)chunks->dist; ++z) {
        for (int x = -(int)chunks->dist; x <= (int)chunks->dist; ++x) {
            genChunkColumn(chunks, x, z, xo, zo);
        }
    }
}
