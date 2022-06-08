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

void chunkUpdate(struct chunkdata* data, int32_t c) {
    if ((int)c % (int)data->info.widthsq >= (int)data->info.width) {
        int32_t c2 = c - data->info.width;
        if ((c2 >= 0 || c2 < (int32_t)data->info.size) && data->renddata[c2].generated) data->renddata[c2].updated = false;
    }
    if ((int)c % (int)data->info.widthsq < (int)(data->info.widthsq - data->info.width)) {
        int32_t c2 = c + data->info.width;
        if ((c2 >= 0 || c2 < (int32_t)data->info.size) && data->renddata[c2].generated) data->renddata[c2].updated = false;
    }
    if (c % data->info.width) {
        int32_t c2 = c - 1;
        if ((c2 >= 0 || c2 < (int32_t)data->info.size) && data->renddata[c2].generated) data->renddata[c2].updated = false;
    }
    if ((c + 1) % data->info.width) {
        int32_t c2 = c + 1;
        if ((c2 >= 0 || c2 < (int32_t)data->info.size) && data->renddata[c2].generated) data->renddata[c2].updated = false;
    }
    if (c >= (int)data->info.widthsq) {
        int32_t c2 = c - data->info.widthsq;
        if ((c2 >= 0 || c2 < (int32_t)data->info.size) && data->renddata[c2].generated) data->renddata[c2].updated = false;
    }
    if (c < (int)(data->info.size - data->info.widthsq)) {
        int32_t c2 = c + data->info.widthsq;
        if ((c2 >= 0 || c2 < (int32_t)data->info.size) && data->renddata[c2].generated) data->renddata[c2].updated = false;
    }
}

struct blockdata getBlock(struct chunkdata* data, int cx, int cy, int cz, int x, int y, int z) {
    cz *= -1;
    z *= -1;
    //printf("in: [%d, %d, %d] [%d, %d, %d]\n", cx, cy, cz, x, y, z);
    cx += data->info.dist;
    cz += data->info.dist;
    x += 8;
    z += 8;
    if (cx < 0 || cz < 0 || cx >= (int)data->info.width || cz >= (int)data->info.width || cy < 0 || cy > 15) return (struct blockdata){0, 0, 0};
    int32_t c = cx + cz * data->info.width + cy * data->info.widthsq;
    while (x < 0 && c % data->info.width) {c -= 1; x += 16;}
    while (x > 15 && (c + 1) % data->info.width) {c += 1; x -= 16;}
    while (z > 15 && c % (int)data->info.widthsq >= (int)data->info.width) {c -= data->info.width; z -= 16;}
    while (z < 0 && c % (int)data->info.widthsq < (int)(data->info.widthsq - data->info.width)) {c += data->info.width; z += 16;}
    while (y < 0 && c >= (int)data->info.widthsq) {c -= data->info.widthsq; y += 16;}
    while (y > 15 && c < (int)(data->info.size - data->info.widthsq)) {c += data->info.widthsq; y -= 16;}
    cx = c % data->info.width;
    cz = c / data->info.width % data->info.width;
    cy = c / data->info.widthsq;
    //printf("resolved: [%d, %d, %d] [%d, %d, %d]\n", cx, cy, cz, x, y, z);
    if (c < 0 || c >= (int32_t)data->info.size || x < 0 || y < 0 || z < 0 || x > 15 || y > 15 || z > 15) return (struct blockdata){0, 0, 0};
    if (!data->renddata[c].generated) return (struct blockdata){255, 0, 0};
    return data->data[c][y * 256 + z * 16 + x];
}

void setBlock(struct chunkdata* data, int cx, int cy, int cz, int x, int y, int z, struct blockdata bdata) {
    cz *= -1;
    z *= -1;
    //printf("in: [%d, %d, %d] [%d, %d, %d]\n", cx, cy, cz, x, y, z);
    cx += data->info.dist;
    cz += data->info.dist;
    x += 8;
    z += 8;
    if (cx < 0 || cz < 0 || cx >= (int)data->info.width || cz >= (int)data->info.width || cy < 0 || cy > 15) return;
    int32_t c = cx + cz * data->info.width + cy * data->info.widthsq;
    while (x < 0 && c % data->info.width) {c -= 1; x += 16;}
    while (x > 15 && (c + 1) % data->info.width) {c += 1; x -= 16;}
    while (z > 15 && c % (int)data->info.widthsq >= (int)data->info.width) {c -= data->info.width; z -= 16;}
    while (z < 0 && c % (int)data->info.widthsq < (int)(data->info.widthsq - data->info.width)) {c += data->info.width; z += 16;}
    while (y < 0 && c >= (int)data->info.widthsq) {c -= data->info.widthsq; y += 16;}
    while (y > 15 && c < (int)(data->info.size - data->info.widthsq)) {c += data->info.widthsq; y -= 16;}
    //printf("resolved: [%d, %d, %d] [%d, %d, %d]\n", cx, cy, cz, x, y, z);
    if (c < 0 || c >= (int32_t)data->info.size || x < 0 || y < 0 || z < 0 || x > 15 || y > 15 || z > 15) return;
    if (!data->renddata[c].generated) return;
    data->data[c][y * 256 + z * 16 + x].id = bdata.id;
    data->renddata[c].updated = false;
    pthread_mutex_lock(&uclock);
    chunkUpdate(data, c);
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
    dist *= 16;
    chunks.info.size = dist;
    chunks.data = malloc(dist * sizeof(struct blockdata*));
    for (unsigned i = 0; i < dist; ++i) {
        chunks.data[i] = calloc(4096 * sizeof(struct blockdata), 1);
    }
    chunks.renddata = calloc(dist * sizeof(struct chunk_renddata), 1);
    chunks.rordr = calloc(chunks.info.widthsq * sizeof(struct rendorder), 1);
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

void moveChunks(struct chunkdata* chunks, int cx, int cz) {
    //moved = true;
    pthread_mutex_lock(&uclock);
    struct blockdata* swap = NULL;
    struct chunk_renddata rdswap;
    if (cx > 0) {
        for (uint32_t c = 0; c < chunks->info.size; c += chunks->info.width) {
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            //chunks->renddata[c + chunks->info.width - 1].updated = false;
            //chunks->renddata[c + chunks->info.width - 1].moved = true;
            for (uint32_t i = 0; i < chunks->info.width - 1; ++i) {
                chunks->data[c + i] = chunks->data[c + i + 1];
                chunks->renddata[c + i] = chunks->renddata[c + i + 1];
            }
            chunks->renddata[c].updated = false;
            uint32_t off = c + chunks->info.width - 1;
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
        for (uint32_t c = 0; c < chunks->info.size; c += chunks->info.width) {
            swap = chunks->data[c + chunks->info.width - 1];
            rdswap = chunks->renddata[c + chunks->info.width - 1];
            //chunks->renddata[c].updated = false;
            //chunks->renddata[c].moved = true;
            for (uint32_t i = chunks->info.width - 1; i > 0; --i) {
                chunks->data[c + i] = chunks->data[c + i - 1];
                chunks->renddata[c + i] = chunks->renddata[c + i - 1];
            }
            chunks->renddata[c + chunks->info.width - 1].updated = false;
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
        for (uint32_t c = 0; c < chunks->info.size; c += ((c + 1) % chunks->info.width) ? 1 : chunks->info.widthsq - chunks->info.width + 1) {
            swap = chunks->data[c];
            rdswap = chunks->renddata[c];
            //chunks->renddata[c + (chunks->info.width - 1) * chunks->info.width].updated = false;
            //chunks->renddata[c + (chunks->info.width - 1) * chunks->info.width].moved = true;
            for (uint32_t i = 0; i < chunks->info.width - 1; ++i) {
                chunks->data[c + i * chunks->info.width] = chunks->data[c + (i + 1) * chunks->info.width];
                chunks->renddata[c + i * chunks->info.width] = chunks->renddata[c + (i + 1) * chunks->info.width];
            }
            chunks->renddata[c].updated = false;
            uint32_t off = c + (chunks->info.width - 1) * chunks->info.width;
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
        for (uint32_t c = 0; c < chunks->info.size; c += ((c + 1) % chunks->info.width) ? 1 : chunks->info.widthsq - chunks->info.width + 1) {
            swap = chunks->data[c + (chunks->info.width - 1) * chunks->info.width];
            rdswap = chunks->renddata[c + (chunks->info.width - 1) * chunks->info.width];
            chunks->renddata[c].updated = false;
            //chunks->renddata[c].moved = true;
            for (uint32_t i = chunks->info.width - 1; i > 0; --i) {
                chunks->data[c + i * chunks->info.width] = chunks->data[c + (i - 1) * chunks->info.width];
                chunks->renddata[c + i * chunks->info.width] = chunks->renddata[c + (i - 1) * chunks->info.width];
            }
            chunks->renddata[c + (chunks->info.width - 1) * chunks->info.width].updated = false;
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
    pthread_mutex_unlock(&uclock);
}

bool genChunk(struct chunkinfo* chunks, int cx, int cy, int cz, int64_t xo, int64_t zo, struct blockdata* data, int type) {
    //microwait(50000 + (rand() % 50000));
    int64_t nx = ((int64_t)cx + xo) * 16;
    int64_t nz = ((int64_t)cz * -1 + zo) * 16;
    cx += chunks->dist;
    cz += chunks->dist;
    //uint32_t coff = cz * chunks->info.width + cy * chunks->info.widthsq + cx;
    bool ct = 0;
    //memset(data, 0, 4096 * sizeof(struct blockdata));
    //chunks->renddata[coff].pos = (coord_3d){(float)(cx - (int)chunks->dist), (float)(cy), (float)(cz - (int)chunks->dist)};
    //printf("set chunk pos [%u]: [%d][%d][%d]\n", coff, cx - chunks->dist, cy, cz - chunks->dist);
    //printf("Generating [%d][%d][%d] [%ld][%ld]\n", cx, cy, cz, xo, zo);
    int btm = cy * 16;
    int top = (cy + 1) * 16 - 1;
    for (int z = 0; z < 16; ++z) {
        for (int x = 0; x < 16; ++x) {
            for (int y = top; y >= btm; --y) {
                data[(y - btm) * 256 + z * 16 + x] = (struct blockdata){0, 14, 0};
            }
            switch (type) {
                default:; {
                        if (!btm) {
                            data[0 * 256 + z * 16 + x].id = 6;
                            data[1 * 256 + z * 16 + x].id = 1;
                            data[2 * 256 + z * 16 + x].id = 2;
                            data[3 * 256 + z * 16 + x].id = 3;
                        }
                    }
                    break;
                case 1:; {
                        //if (!cy) printf("noise coords (% 2d, % 2d, % 2d) [% 3d][% 3d]\n", cx, cy, cz, nx + x, nz + z);
                        double s1 = perlin2d(0, (double)(nx + x) / 27, (double)(nz + z) / 27, 2.0, 1);
                        double s2m = perlin2d(8, (double)(nx + x) / 43, (double)(nz + z) / 43, 1.0, 1) * 2.0;
                        double s2 = perlin2d(1, (double)(nx + x) / (149 + 30 * s2m), (double)(nz + z) / (149 + 30 * s2m), .5, 4);
                        double s3 = perlin2d(2, (double)(nx + x) / 87, (double)(nz + z) / 87, 1.0, 1);
                        double s4 = perlin2d(3, (double)(nx + x) / 63, (double)(nz + z) / 63, 1.0, 1);
                        double s5 = perlin2d(4, (double)(nx + x) / 105, (double)(nz + z) / 105, 1.0, 1);
                        //printf("[%lf][%lf]", s, s2);
                        for (int y = btm; y <= top && y < 65; ++y) {
                            //printf("placing water at chunk [%u] [%d, %d, %d]\n", coff, x, y, z);
                            data[(y - btm) * 256 + z * 16 + x].id = 7;
                        }
                        double s = s1;
                        s *= s5 * 5 * (1.0 - s2);
                        s += s2 * 50 + (1 - s5) * 4;
                        s -= 18;
                        double sf = (double)(s * 2) - 8;
                        int si = (int)sf + 60;
                        //printf("[%d]\n", si);
                        uint8_t blockid;
                        blockid = (s1 + s2 < s3 * 1.125) ? 8 : 3;
                        blockid = (sf < 5 + s1 * 3) ? ((sf < -((s4 + 2.25) * 7)) ? 2 : 8) : blockid;
                        if (si >= btm && si <= top) data[(si - btm) * 256 + z * 16 + x].id = blockid;
                        for (int y = ((si - 1) > top) ? top : si - 1; y >= btm; --y) {
                            data[(y - btm) * 256 + z * 16 + x].id = (y < (double)(si) * 0.9) ? 1 : ((blockid == 8 && y > si - 4) ? 8 : 2);
                        }
                        if (!btm) data[z * 16 + x].id = 6;
                        //if (si < 3) si = 3;
                        //if (!x && !z) rendinf.campos.y += (double)si;
                    }
                    break;
                case 2:; {
                        double s1 = perlin2d(1, (double)(nx + x) / 30, (double)(nz + z) / 30, 1, 3);
                        double s2 = perlin2d(0, (double)(nx + x) / 187, (double)(nz + z) / 187, 2, 4);
                        double s3 = perlin2d(2, (double)(nx + x) / 56, (double)(nz + z) / 56, 1, 8);
                        int s4 = noise2d(3, (double)(nx + x), (double)(nz + z));
                        double s5 = perlin2d(4, (double)(nx + x) / 329, (double)(nz + z) / 329, 1, 2);
                        for (int y = btm; y <= top && y < 65; ++y) {
                            //printf("placing water at chunk [%u] [%d, %d, %d]\n", coff, x, y, z);
                            data[(y - btm) * 256 + z * 16 + x].id = 7;
                        }
                        double s = ((s1 * 6 - 3) + (s2 * 32 - 16)) * (1.0 - s5 * 0.5) + 45 + s5 * 45;
                        int si = round(s);
                        if (si >= btm && si <= top) data[(si - btm) * 256 + z * 16 + x].id = ((double)si - round(s3 * 10) < 62) ? 8 : ((si < 64) ? 2 : 3);
                        for (int y = ((si - 1) > top) ? top : si - 1; y >= btm; --y) {
                            data[(y - btm) * 256 + z * 16 + x].id = (y < ((s2 * 26 - 16) + 62)) ? 1 : ((double)y - round(s3 * 10) < 62 && y > ((s2 * 30 - 16) + 62)) ? 8 : 2;
                        }
                        if (!btm) data[z * 16 + x].id = 6;
                        if (!btm && !(s4 % 2)) data[256 + z * 16 + x].id = 6;
                        if (!btm && !(s4 % 4)) data[512 + z * 16 + x].id = 6;
                    }
                    break;
                case 3:; {
                        double s1 = perlin2d(9, (double)(nx + x) / 2, (double)(nz + z) / 2, 1.25, 8);
                        double s2 = perlin2d(5, (double)(nx + x) / 102, (double)(nz + z) / 102, 2.0, 1);
                        double s3 = perlin2d(4, (double)(nx + x) / 5, (double)(nz + z) / 5, 1.0, 1);
                        int s4 = noise2d(3, (double)(nx + x), (double)(nz + z));
                        for (int y = btm; y <= top && y < 65; ++y) {
                            //printf("placing water at chunk [%u] [%d, %d, %d]\n", coff, x, y, z);
                            data[(y - btm) * 256 + z * 16 + x].id = 7;
                        }
                        double s = (s1 * 20 - 10) + (s2 * 40 - 20) + 70;
                        int si = round(s);
                        for (int y = (si > top) ? top : si; y >= btm; --y) {
                            data[(y - btm) * 256 + z * 16 + x].id = (y < ((s2 * 32 - 20) + 65)) ? 1 : ((double)y - round(s3 * 10) < 62 && y > ((s2 * 36 - 20) + 65)) ? 2 : 1;
                        }
                        if (!btm) data[z * 16 + x].id = 6;
                        if (!btm && !(s4 % 2)) data[256 + z * 16 + x].id = 6;
                        if (!btm && !(s4 % 4)) data[512 + z * 16 + x].id = 6;
                    }
                    break;
                case 4:; {
                        double s1 = perlin2d(0, (double)(nx + x) / 44, (double)(nz + z) / 44, 1, 5);
                        double s6 = perlin2d(1, (double)(nx + x) / 174, (double)(nz + z) / 174, 1, 3) * 5.0 - 2.0;
                        if (s6 < 0.0) s6 = 0.0;
                        else if (s6 > 1.0) s6 = 1.0;
                        double s2 = perlin2d(2, (double)(nx + x) / 78.214, (double)(nz + z) / 78.214, 1, 3) * 0.85 * s6 +
                                    perlin2d(3, (double)(nx + x) / 87.153, (double)(nz + z) / 87.153, 1, 3) * (1.0 - s6);
                        double s3 = perlin2d(4, (double)(nx + x) / 18, (double)(nz + z) / 18, 1, 3);
                        double s7 = perlin2d(7, (double)(nx + x) / 18, (double)(nz + z) / 18, 1, 3);
                        int s4 = noise2d(5, (double)(nx + x), (double)(nz + z));
                        double s5 = perlin2d(6, (double)(nx + x) / 397.352, (double)(nz + z) / 397.352, 1, 10) * 2 - 0.5;
                        if (s5 < 0.2) s5 = 0.2;
                        else if (s5 > 1.0) s5 = 1.0;
                        for (int y = btm; y <= top && y < 65; ++y) {
                            data[(y - btm) * 256 + z * 16 + x].id = 7;
                        }
                        double s = roundf((s1 * 20 - 10) + (s2 * 50.2 - 25) * (1.0 - fabs(s5) * 0.5) + (s5 * 67)) + 40;
                        int si = round(s);
                        if (si >= btm && si <= top) {
                            data[(si - btm) * 256 + z * 16 + x].id = ((round(s7 * 10) < 7 || si > 58 - s3 * 5) && si < 67 + s3 * 3 && si < ((s2 * 32 - 20) + 75)) ? 8 : ((si < 64) ? 2 : 3);
                        }
                        for (int y = ((si - 1) > top) ? top : si - 1; y >= btm; --y) {
                            data[(y - btm) * 256 + z * 16 + x].id = (y + 4 < s * 0.975) ? 1 : ((round(s7 * 10) < 7 || y > 58 - s3 * 5) && si < 67 + s3 * 3) ? 8 : 2;
                        }
                        if (!btm) {
                            data[z * 16 + x].id = 6;
                            if (!(s4 % 2) || !(s4 % 3)) data[256 + z * 16 + x].id = 6;
                            if (!(s4 % 4)) data[512 + z * 16 + x].id = 6;
                        }
                    }
                    break;
                case 5:; {
                        int xzoff = z * 16 + x;
                        double cx = (double)(nx + x);
                        double cz = (double)(nz + z);

                        for (int y = btm; y <= top && y < 65; ++y) {
                            data[(y - btm) * 256 + xzoff].id = 7;
                        }

                        double p0 = tanhf(nperlin2d(0, cx, cz, 0.008675, 5) * 2);
                        if (p0 < 0) p0 *= 0.5;
                        double p1 = tanhf(nperlin2d(1, cx, cz, 0.001942, 3) * 3);
                        if (p1 < 0) p1 *= 0.5;
                        double h0 = p0 * 18 + p1 * 39;
                        double p3 = tanhf(nperlin2d(3, cx, cz, 0.045462, 2) * 4) * 1.9653;
                        double m0 = h0 / 67;
                        if (m0 < 0) m0 *= 1.5;
                        if (m0 > 1) m0 = 1;
                        if (m0 < -1) m0 = -1;
                        double m1 = cos(2 * m0 * M_PI) / 2 + 0.5;
                        double p4 = tanhf((nperlin2d(4, cx, cz, 0.009612, 2) - 0.1) * 4) * 1.095;
                        if (p4 < 0.05) p4 = 0.05;
                        double p2 = tanhf((nperlin2d(2, cx, cz, 0.010358, 4) - 0.1) * 7) * 2.14;
                        if (p2 < 0) p2 *= 0.33;
                        double h1 = (p2 * 9.75 * p4 + p3 * 5) * m1;
                        double h2 = (tanhf(nperlin2d(5, cx, cz, 0.005291, 1) * 2) / 2 + 0.5) * 0.9 + 0.2;
                        double h = round((h1 + h0) * h2 + 65);

                        double p9 = perlin2d(9, (double)(nx + x), (double)(nz + z), 0.025148, 5);
                        double p10 = perlin2d(10, (double)(nx + x), (double)(nz + z), 0.184541, 1);
                        double p11 = perlin2d(11, (double)(nx + x), (double)(nz + z), 0.049216, 2);
                        int hi = h;
                        for (int y = (hi > top) ? top : hi; y >= btm; --y) {
                            bool c1 = y < 62 + p9 * 6;
                            bool c2 = p11 > 0.33 && y < 56 + p10 * 3;
                            uint8_t b1 = (c1) ? ((c2) ? 4 : 8) : 2;
                            uint8_t b2 = (c1) ? ((c2) ? 4 : 8) : ((y < 64) ? 2 : 3);
                            uint8_t blockid = (y < hi) ? ((y < hi - (3 - round(fabs(p4) * 2))) ? 1 : b1) : b2;
                            data[(y - btm) * 256 + xzoff].id = blockid;
                        }

                        int n0 = noise2d(8, (double)(nx + x), (double)(nz + z));
                        if (!btm) {
                            data[xzoff].id = 6;
                            if (!(n0 % 2) || !(n0 % 3)) data[256 + xzoff].id = 6;
                            if (!(n0 % 4)) data[512 + xzoff].id = 6;
                        }
                    }
                    break;
            }
        }
    }
    ct = true;
    return ct;
}

static uint16_t cid = 0;
static int64_t cxo = 0, czo = 0;
static pthread_mutex_t cidlock;

void genChunks_cb(struct chunkdata* chunks, void* ptr) {
    struct server_ret_chunk* srvchunk = ptr;
    pthread_mutex_lock(&uclock);
    if (srvchunk->id != cid || srvchunk->xo != cxo || srvchunk->zo != czo) {
        //printf("Dropping chunk ([s:%d] != [c:%d])\n", srvchunk->id, cid);
        pthread_mutex_unlock(&uclock);
        return;
    }
    uint32_t coff = (srvchunk->z + chunks->info.dist) * chunks->info.width + srvchunk->y * chunks->info.widthsq + (srvchunk->x + chunks->info.dist);
    memcpy(chunks->data[coff], srvchunk->data, 4096 * sizeof(struct blockdata));
    chunks->renddata[coff].updated = false;
    chunkUpdate(chunks, coff);
    chunks->renddata[coff].generated = true;
    pthread_mutex_unlock(&uclock);
}

void genChunks_cb2(struct chunkdata* chunks, void* ptr) {
    struct server_ret_chunkcol* srvchunk = ptr;
    pthread_mutex_lock(&uclock);
    if (srvchunk->id != cid || srvchunk->xo != cxo || srvchunk->zo != czo) {
        //printf("Dropping chunk ([s:%ld,%ld] != [c:%ld,%ld])\n", srvchunk->xo, srvchunk->zo, curxo, curzo);
        pthread_mutex_unlock(&uclock);
        return;
    }
    uint32_t coff = (srvchunk->z + chunks->info.dist) * chunks->info.width + (srvchunk->x + chunks->info.dist);
    uint32_t coff2 = coff;
    for (int i = 0; i < 16; ++i) {
        memcpy(chunks->data[coff2], srvchunk->data[i], 4096 * sizeof(struct blockdata));
        chunks->renddata[coff2].updated = false;
        chunkUpdate(chunks, coff2);
        chunks->renddata[coff2].generated = true;
        coff2 += chunks->info.widthsq;
    }
    pthread_mutex_unlock(&uclock);
}

void genChunks(struct chunkdata* chunks, int64_t xo, int64_t zo) {
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
    ++cid;
    //printf("set [%u]\n", cid);
    pthread_mutex_unlock(&uclock);
    struct server_msg_chunkpos* chunkpos = malloc(sizeof(struct server_msg_chunkpos));
    *chunkpos = (struct server_msg_chunkpos){xo, zo};
    servSend(SERVER_CMD_SETCHUNK, chunkpos, true);
    for (int i = 0; i <= (int)chunks->info.dist; ++i) {
        for (int z = -i; z <= i; ++z) {
            for (int x = -i; x <= i; ++x) {
                if (abs(z) == i || (abs(z) != i && abs(x) == i)) {
                    int gen = 0;
                    uint32_t coff = (z + chunks->info.dist) * chunks->info.width + (x + chunks->info.dist);
                    uint32_t coff2 = coff;
                    for (int y = 0; y < 16; ++y) {
                        if (chunks->renddata[coff2].generated) ++gen;
                        coff2 += chunks->info.widthsq;
                    }
                    if (!gen) {
                        //printf("REQ [%d][%d]\n", x, z);
                        struct server_msg_chunkcol* srvchunk = malloc(sizeof(struct server_msg_chunkcol));
                        *srvchunk = (struct server_msg_chunkcol){.id = cid, .info = chunks->info, .x = x, .z = z, .xo = xo, .zo = zo};
                        servSend(SERVER_MSG_GETCHUNKCOL, srvchunk, true);
                    } else if (gen < 16) {
                        coff2 = coff;
                        for (int y = 0; y < 16; ++y) {
                            if (chunks->renddata[coff2].generated) {
                                struct server_msg_chunk* srvchunk2 = malloc(sizeof(struct server_msg_chunk));
                                *srvchunk2 = (struct server_msg_chunk){.id = cid, .info = chunks->info, .x = x, .y = y, .z = z, .xo = xo, .zo = zo};
                                servSend(SERVER_MSG_GETCHUNK, srvchunk2, true);
                            }
                            coff2 += chunks->info.widthsq;
                        }
                    }
                }
            }
        }
    }
}
