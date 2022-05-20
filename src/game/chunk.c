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
    if (!data->renddata[c].generated) return (struct blockdata){0, 0, 0};
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
    chunkUpdate(data, c);
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
                            data[(y - btm) * 256 + z * 16 + x].id = (y < ((s2 * 28 - 16) + 65)) ? 1 : ((double)y - round(s3 * 10) < 62 && y > ((s2 * 30 - 16) + 65)) ? 8 : 2;
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
            }
        }
    }
    ct = true;
    return ct;
}

static uint16_t cid = 0;

void genChunks_cb(struct chunkdata* chunks, void* ptr) {
    struct server_ret_chunk* srvchunk = ptr;
    if (srvchunk->id != cid) {
        //printf("Dropping chunk ([s:%d] != [c:%d])\n", srvchunk->id, cid);
        return;
    }
    uint32_t coff = (srvchunk->z + chunks->info.dist) * chunks->info.width + srvchunk->y * chunks->info.widthsq + (srvchunk->x + chunks->info.dist);
    //static unsigned chunk = 0;
    //printf("Writing chunk [%d][%d][%d]\n", srvchunk->x, srvchunk->y, srvchunk->z);
    //++chunk;
    pthread_mutex_lock(&uclock);
    memcpy(chunks->data[coff], srvchunk->data, 4096 * sizeof(struct blockdata));
    //chunks->renddata[coff].sent = false;
    chunks->renddata[coff].updated = false;
    chunkUpdate(chunks, coff);
    chunks->renddata[coff].generated = true;
    pthread_mutex_unlock(&uclock);
    /*
    if (srvchunk->data->renddata[coff].moved) {
        srvchunk->data->renddata[coff].sent = false;
        srvchunk->data->renddata[coff].updated = false;
    }
    */
}

void genChunks(struct chunkdata* chunks, int64_t xo, int64_t zo) {
    ++cid;
    struct server_msg_chunkpos* chunkpos = malloc(sizeof(struct server_msg_chunkpos));
    *chunkpos = (struct server_msg_chunkpos){xo, zo};
    //printf("alloced block of [%d]\n", malloc_usable_size(chunkpos));
    servSend(SERVER_CMD_SETCHUNK, chunkpos, true);
    //uint64_t starttime = altutime();
    //uint32_t ct = 0;
    //uint32_t maxct = 1;
    //int sent = 0;
    int count = 0;
    for (int i = 0; i <= (int)chunks->info.dist; ++i) {
        for (int z = -i; z <= i; ++z) {
            for (int x = -i; x <= i; ++x) {
                if (abs(z) == i || (abs(z) != i && abs(x) == i)) {
                    /*ct += *///genChunkColumn(chunks, x, z, xo, zo);
                    //printf("[%d][%d] [%u]\n", x, z, ct);
                    //if (ct > maxct - 1) goto ret;
                    //while (
                    for (int y = 0; y < 16; ++y) {
                        uint32_t coff = (z + chunks->info.dist) * chunks->info.width + y * chunks->info.widthsq + (x + chunks->info.dist);
                        if (chunks->renddata[coff].generated) continue;
                        ++count;
                        //if (chunks->renddata[coff].moved || !chunks->renddata[coff].sent) ++sent;
                        //else continue;
                        struct server_msg_chunk* srvchunk = malloc(sizeof(struct server_msg_chunk));
                        //printf("alloced block of [%d]\n", malloc_usable_size(srvchunk));
                        *srvchunk = (struct server_msg_chunk){.id = cid, .info = chunks->info, .x = x, .y = y, .z = z, .xo = xo, .zo = zo};
                        servSend(SERVER_MSG_GETCHUNK, srvchunk, true);
                        //chunks->renddata[coff].sent = true;
                        //chunks->renddata[coff].moved = false;
                    }
                    //if (altutime() - starttime >= 1000000 / (((rendinf.fps) ? rendinf.fps : 60) * 3)) goto ret;
                }
            }
        }
    }
    //printf("Requested [%d] chunks\n", count);
    //ret:;
    //printf("generated in: [%f]s\n", (float)(altutime() - starttime) / 1000000.0);
}
