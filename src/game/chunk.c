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

struct blockdata getBlock(struct chunkdata* data, int cx, int cy, int cz, int x, int y, int z) {
    cz *= -1;
    z *= -1;
    //printf("in: [%d, %d, %d] [%d, %d, %d]\n", cx, cy, cz, x, y, z);
    cx += data->dist;
    cz += data->dist;
    x += 8;
    z += 8;
    if (cx < 0 || cz < 0 || cx >= (int)data->width || cz >= (int)data->width || cy < 0 || cy > 15) return (struct blockdata){0, 0, 0, 0};
    int32_t c = cx + cz * data->width + cy * data->widthsq;
    while (x < 0 && c % data->width) {c -= 1; x += 16;}
    while (x > 15 && (c + 1) % data->width) {c += 1; x -= 16;}
    while (z > 15 && c % (int)data->widthsq >= (int)data->width) {c -= data->width; z -= 16;}
    while (z < 0 && c % (int)data->widthsq < (int)(data->widthsq - data->width)) {c += data->width; z += 16;}
    while (y < 0 && c >= (int)data->widthsq) {c -= data->widthsq; y += 16;}
    while (y > 15 && c < (int)(data->size - data->widthsq)) {c += data->widthsq; y -= 16;}
    cx = c % data->width;
    cz = c / data->width % data->width;
    cy = c / data->widthsq;
    //printf("resolved: [%d, %d, %d] [%d, %d, %d]\n", cx, cy, cz, x, y, z);
    if (c < 0 || c >= (int32_t)data->size || x < 0 || y < 0 || z < 0 || x > 15 || y > 15 || z > 15) return (struct blockdata){0, 0, 0, 0};
    if (!data->renddata[c].generated) return (struct blockdata){0, 0, 0, 0};
    return data->data[c][y * 256 + z * 16 + x];
}

void setBlock(struct chunkdata* data, int cx, int cy, int cz, int x, int y, int z, struct blockdata bdata) {
    cz *= -1;
    z *= -1;
    //printf("in: [%d, %d, %d] [%d, %d, %d]\n", cx, cy, cz, x, y, z);
    cx += data->dist;
    cz += data->dist;
    x += 8;
    z += 8;
    if (cx < 0 || cz < 0 || cx >= (int)data->width || cz >= (int)data->width || cy < 0 || cy > 15) return;
    int32_t c = cx + cz * data->width + cy * data->widthsq;
    while (x < 0 && c % data->width) {c -= 1; x += 16;}
    while (x > 15 && (c + 1) % data->width) {c += 1; x -= 16;}
    while (z > 15 && c % (int)data->widthsq >= (int)data->width) {c -= data->width; z -= 16;}
    while (z < 0 && c % (int)data->widthsq < (int)(data->widthsq - data->width)) {c += data->width; z += 16;}
    while (y < 0 && c >= (int)data->widthsq) {c -= data->widthsq; y += 16;}
    while (y > 15 && c < (int)(data->size - data->widthsq)) {c += data->widthsq; y -= 16;}
    //printf("resolved: [%d, %d, %d] [%d, %d, %d]\n", cx, cy, cz, x, y, z);
    if (c < 0 || c >= (int32_t)data->size || x < 0 || y < 0 || z < 0 || x > 15 || y > 15 || z > 15) return;
    if (!data->renddata[c].generated) return;
    data->data[c][y * 256 + z * 16 + x] = bdata;
    data->renddata[c].updated = false;
}

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

//static bool moved = false;

void moveChunks(struct chunkdata* chunks, int cx, int cz) {
    //moved = true;
    struct blockdata* swap = NULL;
    struct chunk_renddata rdswap;
    if (cx > 0) {
        for (uint32_t c = 0; c < chunks->size; c += chunks->width) {
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            //chunks->renddata[c + chunks->width - 1].updated = false;
            //chunks->renddata[c + chunks->width - 1].moved = true;
            for (uint32_t i = 0; i < chunks->width - 1; ++i) {
                chunks->data[c + i] = chunks->data[c + i + 1];
                chunks->renddata[c + i] = chunks->renddata[c + i + 1];
            }
            chunks->renddata[c].updated = false;
            uint32_t off = c + chunks->width - 1;
            chunks->data[off] = swap;
            chunks->renddata[off] = rdswap;
            //chunks->renddata[off].moved = true;
            if (chunks->renddata[off].generated) {
                chunks->renddata[off].vcount = 0;
                chunks->renddata[off].vcount2 = 0;
                chunks->renddata[off].updated = false;
                chunks->renddata[off].generated = false;
            }
        }
    } else if (cx < 0) {
        for (uint32_t c = 0; c < chunks->size; c += chunks->width) {
            swap = chunks->data[c + chunks->width - 1];
            rdswap = chunks->renddata[c + chunks->width - 1];
            //chunks->renddata[c].updated = false;
            //chunks->renddata[c].moved = true;
            for (uint32_t i = chunks->width - 1; i > 0; --i) {
                chunks->data[c + i] = chunks->data[c + i - 1];
                chunks->renddata[c + i] = chunks->renddata[c + i - 1];
            }
            chunks->renddata[c + chunks->width - 1].updated = false;
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            //chunks->renddata[c].moved = true;
            if (chunks->renddata[c].generated) {
                chunks->renddata[c].vcount = 0;
                chunks->renddata[c].vcount2 = 0;
                chunks->renddata[c].updated = false;
                chunks->renddata[c].generated = false;
            }
        }
    }
    if (cz > 0) {
        for (uint32_t c = 0; c < chunks->size; c += ((c + 1) % chunks->width) ? 1 : chunks->widthsq - chunks->width + 1) {
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            //chunks->renddata[c + (chunks->width - 1) * chunks->width].updated = false;
            //chunks->renddata[c + (chunks->width - 1) * chunks->width].moved = true;
            for (uint32_t i = 0; i < chunks->width - 1; ++i) {
                chunks->data[c + i * chunks->width] = chunks->data[c + (i + 1) * chunks->width];
                chunks->renddata[c + i * chunks->width] = chunks->renddata[c + (i + 1) * chunks->width];
            }
            chunks->renddata[c].updated = false;
            uint32_t off = c + (chunks->width - 1) * chunks->width;
            chunks->data[off] = swap;
            chunks->renddata[off] = rdswap;
            //chunks->renddata[off].moved = true;
            if (chunks->renddata[off].generated) {
                chunks->renddata[off].vcount = 0;
                chunks->renddata[off].vcount2 = 0;
                chunks->renddata[off].updated = false;
                chunks->renddata[off].generated = false;
            }
        }
    } else if (cz < 0) {
        for (uint32_t c = 0; c < chunks->size; c += ((c + 1) % chunks->width) ? 1 : chunks->widthsq - chunks->width + 1) {
            swap = chunks->data[c + (chunks->width - 1) * chunks->width];
            rdswap = chunks->renddata[c + (chunks->width - 1) * chunks->width];
            chunks->renddata[c].updated = false;
            //chunks->renddata[c].moved = true;
            for (uint32_t i = chunks->width - 1; i > 0; --i) {
                chunks->data[c + i * chunks->width] = chunks->data[c + (i - 1) * chunks->width];
                chunks->renddata[c + i * chunks->width] = chunks->renddata[c + (i - 1) * chunks->width];
            }
            chunks->renddata[c + (chunks->width - 1) * chunks->width].updated = false;
            chunks->data[c] = swap;
            chunks->renddata[c] = rdswap;
            //chunks->renddata[c].moved = true;
            if (chunks->renddata[c].generated) {
                chunks->renddata[c].vcount = 0;
                chunks->renddata[c].vcount2 = 0;
                chunks->renddata[c].updated = false;
                chunks->renddata[c].generated = false;
            }
        }
    }
}

/*
void moveChunksMult(struct chunkdata* chunks, int cx, int cz) {
}
*/

bool genChunk(struct chunkdata* chunks, int cx, int cy, int cz, int xo, int zo, struct blockdata* data) {
    //microwait(25000);
    int nx = (cx + xo) * 16;
    int nz = (cz * -1 + zo) * 16;
    cx += chunks->dist;
    cz += chunks->dist;
    uint32_t coff = cz * chunks->width + cy * chunks->widthsq + cx;
    bool ct = 0;
    if (chunks->renddata[coff].updated) goto skipfor;
    //memset(data, 0, 4096 * sizeof(struct blockdata));
    //chunks->renddata[coff].pos = (coord_3d){(float)(cx - (int)chunks->dist), (float)(cy), (float)(cz - (int)chunks->dist)};
    //printf("set chunk pos [%u]: [%d][%d][%d]\n", coff, cx - chunks->dist, cy, cz - chunks->dist);
    int btm = cy * 16;
    int top = (cy + 1) * 16 - 1;
    for (int z = 0; z < 16; ++z) {
        for (int x = 0; x < 16; ++x) {
            //if (!cy) printf("noise coords (% 2d, % 2d, % 2d) [% 3d][% 3d]\n", cx, cy, cz, nx + x, nz + z);
            float s1 = perlin2d(0, (float)(nx + x) / 19, (float)(nz + z) / 19, 1.0, 1);
            float s2 = perlin2d(1, (float)(nx + x) / 179, (float)(nz + z) / 257, .5, 1);
            float s3 = perlin2d(2, (float)(nx + x) / 87, (float)(nz + z) / 167, 1.0, 1);
            float s4 = perlin2d(3, (float)(nx + x) / 63, (float)(nz + z) / 74, 1.0, 1);
            float s5 = perlin2d(4, (float)(nx + x) / 61, (float)(nz + z) / 64, 1.0, 1);
            //printf("[%lf][%lf]", s, s2);
            data[z * 16 + x] = (struct blockdata){0, 0, 0, 0};
            for (int y = btm; y <= top && y < 65; ++y) {
                //printf("placing water at chunk [%u] [%d, %d, %d]\n", coff, x, y, z);
                data[(y - btm) * 256 + z * 16 + x] = (struct blockdata){7, 0, 0, 0};
            }
            float s = s1;
            s *= s5 * 4;
            s += s2 * 48 + (1 - s5) * 4;
            s -= 10;
            float sf = (float)(s * 2) - 8;
            int si = (int)sf + 60;
            //printf("[%d]\n", si);
            uint8_t blockid = (s1 + s2 < s3 * 1.125) ? 8 : 3;
            blockid = (sf < 5.5) ? ((sf < -((s4 + 2.25) * 7)) ? 2 : 8) : blockid;
            if (si >= btm && si <= top) data[(si - btm) * 256 + z * 16 + x] = (struct blockdata){blockid, 0, 0, 0};
            for (int y = ((si - 1) > top) ? top : si - 1; y >= btm; --y) {
                data[(y - btm) * 256 + z * 16 + x] = (struct blockdata){(y < (float)(si) * 0.9) ? 1 : 2, 0, 0, 0};
            }
            if (!btm) data[z * 16 + x] = (struct blockdata){6, 0, 0, 0};
            //if (si < 3) si = 3;
            //if (!x && !z) rendinf.campos.y += (float)si;
        }
    }
    ct = true;
    skipfor:;
    return ct;
}

static int cxo = 0, czo = 0;

static uint16_t cid = 0;

static void genChunks_cb(struct server_chunk* srvchunk) {
    //if (!moved) return;
    uint32_t coff = (srvchunk->z + srvchunk->chunks->dist) * srvchunk->chunks->width + srvchunk->y * srvchunk->chunks->widthsq + (srvchunk->x + srvchunk->chunks->dist);
    //static unsigned chunk = 0;
    if (srvchunk->id != cid) {
        //printf("Dropping chunk [%u]\n", chunk);
        //++chunk;
        return;
    }
    //printf("Writing chunk [%u]\n", chunk);
    //++chunk;
    memcpy(srvchunk->chunks->data[coff], srvchunk->data, 4096 * sizeof(struct blockdata));
    srvchunk->chunks->renddata[coff].updated = false;
    //chunks->renddata[coff].sent = false;
    if ((int)coff % (int)srvchunk->chunks->widthsq >= (int)srvchunk->chunks->width) {
        int32_t coff2 = coff - srvchunk->chunks->width;
        if ((coff2 >= 0 || coff2 < (int32_t)srvchunk->chunks->size) && srvchunk->chunks->renddata[coff2].generated) srvchunk->chunks->renddata[coff2].updated = false;
    }
    if ((int)coff % (int)srvchunk->chunks->widthsq < (int)(srvchunk->chunks->widthsq - srvchunk->chunks->width)) {
        int32_t coff2 = coff + srvchunk->chunks->width;
        if ((coff2 >= 0 || coff2 < (int32_t)srvchunk->chunks->size) && srvchunk->chunks->renddata[coff2].generated) srvchunk->chunks->renddata[coff2].updated = false;
    }
    if (coff % srvchunk->chunks->width) {
        int32_t coff2 = coff - 1;
        if ((coff2 >= 0 || coff2 < (int32_t)srvchunk->chunks->size) && srvchunk->chunks->renddata[coff2].generated) srvchunk->chunks->renddata[coff2].updated = false;
    }
    if ((coff + 1) % srvchunk->chunks->width) {
        int32_t coff2 = coff + 1;
        if ((coff2 >= 0 || coff2 < (int32_t)srvchunk->chunks->size) && srvchunk->chunks->renddata[coff2].generated) srvchunk->chunks->renddata[coff2].updated = false;
    }
    srvchunk->chunks->renddata[coff].generated = true;
    /*
    if (srvchunk->data->renddata[coff].moved) {
        srvchunk->data->renddata[coff].sent = false;
        srvchunk->data->renddata[coff].updated = false;
    }
    */
}

void genChunks(struct chunkdata* chunks, int xo, int zo) {
    ++cid;
    cxo = xo;
    czo = zo;
    //uint64_t starttime = altutime();
    //uint32_t ct = 0;
    //uint32_t maxct = 1;
    //int sent = 0;
    int count = 0;
    for (int i = 0; i <= (int)chunks->dist; ++i) {
        for (int z = -i; z <= i; ++z) {
            for (int x = -i; x <= i; ++x) {
                if (abs(z) == i || (abs(z) != i && abs(x) == i)) {
                    /*ct += *///genChunkColumn(chunks, x, z, xo, zo);
                    //printf("[%d][%d] [%u]\n", x, z, ct);
                    //if (ct > maxct - 1) goto ret;
                    //while (
                    for (int y = 0; y < 16; ++y) {
                        uint32_t coff = (z + chunks->dist) * chunks->width + y * chunks->widthsq + (x + chunks->dist);
                        if (chunks->renddata[coff].generated) continue;
                        ++count;
                        //if (chunks->renddata[coff].moved || !chunks->renddata[coff].sent) ++sent;
                        //else continue;
                        struct server_chunk* srvchunk = malloc(sizeof(struct server_chunk));
                        *srvchunk = (struct server_chunk){.id = cid, .chunks = chunks, .x = x, .y = y, .z = z, .xo = xo, .zo = zo};
                        servSend(SERVER_MSG_GETCHUNK, srvchunk, true, true, genChunks_cb);
                        //chunks->renddata[coff].sent = true;
                        //chunks->renddata[coff].moved = false;
                    }
                    //if (altutime() - starttime >= 1000000 / (((rendinf.fps) ? rendinf.fps : 60) * 3)) goto ret;
                }
            }
        }
    }
    printf("Requested [%d] chunks\n", count);
    //ret:;
    //printf("generated in: [%f]s\n", (float)(altutime() - starttime) / 1000000.0);
}
