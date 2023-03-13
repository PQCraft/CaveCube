#include <main/main.h>
#include "worldgen.h"
#include "blocks.h"
#include <common/noise.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static uint8_t stone;
static uint8_t stone_cobble;
static uint8_t stone_basalt;
static uint8_t stone_granite;
static uint8_t dirt;
static uint8_t grass_block;
static uint8_t gravel;
static uint8_t sand;
static uint8_t water;
static uint8_t lava;
static uint8_t bedrock;

bool initWorldgen() {
    stone = blockNoFromID("stone");
    stone_cobble = blockSubNoFromID(stone, "cobble");
    stone_basalt = blockSubNoFromID(stone, "basalt");
    stone_granite = blockSubNoFromID(stone, "granite");
    dirt = blockNoFromID("dirt");
    grass_block = blockNoFromID("grass_block");
    gravel = blockNoFromID("gravel");
    sand = blockNoFromID("sand");
    water = blockNoFromID("water");
    lava = blockNoFromID("lava");
    bedrock = blockNoFromID("bedrock");
    //setRandSeed(15, altutime());
    return true;
}

static force_inline void genSliver(int type, double cx, double cz, struct blockdata* data) {
    switch (type) {
        default:; {
            data[0].id = bedrock;
            data[1].id = stone;
            data[2].id = dirt;
            data[3].id = grass_block;
        } break;
        case 1:; {
            bool block[512] = {0};
            float height = tanhf(nperlin2d(1, cx, cz, 0.00425, 5) * 1.5 - 0.1) + 0.15;
            float heightmult = tanhf(nperlin2d(2, cx, cz, 0.00267, 2) * 2.0 + 0.25) * 0.5 + 0.5;
            float detail = perlin2d(3, cx, cz, 0.02, 4);
            float finalheight = (height * heightmult * 80.0) + (detail * 14.5) + 128.0;
            for (int i = finalheight; i <= 128; ++i) {
                data[i].id = water;
            }
            for (int i = 0; i <= round(finalheight) && i < 512; ++i) {
                block[i] = true;
            }
            float extraheight = (tanhf(nperlin2d(4, cx, cz, 0.0145, 1) * 1.5 - (2.15 - heightmult * 1.0)) * 0.5 + 0.5) * heightmult * 55.0;
            float extrafinalh = extraheight + finalheight;
            for (int i = finalheight; i < extrafinalh; ++i) {
                float fi = i;
                if (noise3(5, cx / 18.0, fi / 18.0, cz / 18.0) > -(((extrafinalh - fi) / extraheight)) * 1.4 + 0.4) {
                    block[i] = true;
                }
            }
            data[0].id = bedrock;
            data[0].subid = 0;
            float seanoise = nperlin2d(5, cx, cz, 0.0125, 2) - detail * 0.75;
            float dirtnoise = nperlin2d(6, cx, cz, 0.2, 1);
            for (int i = 511, lastair = i; i > 0; --i) {
                float fi = i;
                float fl = lastair;
                if (block[i]) {
                    if ((fl - (135.0 + seanoise * 2.0)) * (1.3 + seanoise * 0.1) <= (fi - (135.0 + seanoise * 2.0))) {
                        data[i].id = sand;
                    } else if (i == lastair - 1) {
                        data[i].id = grass_block;
                    } else if (fi > fl - ((511.0 - fi + dirtnoise * 15.0) / 511.0) * 24.0 + 12.75) {
                        data[i].id = dirt;
                    } else {
                        data[i].id = stone;
                    }
                } else {
                    lastair = i;
                }
            }
        } break;
    }
}

void genChunk(int64_t cx, int64_t cz, struct blockdata* data, int type) {
    //printf("GEN [%"PRId64", %"PRId64"]\n", cx, cz);
    #if 0
    if ((cx + cz) % 2) {
        for (int z = 0; z < 16; ++z) {
            for (int x = 0; x < 16; ++x) {
                int xzoff = z * 16 + x;
                struct blockdata sliver[512];
                memset(&sliver, 0, sizeof(sliver));
                for (int i = 511; i >= 0; --i) {
                    struct blockdata* tdata = &data[256 * i + xzoff];
                    *tdata = (struct blockdata){
                        .id = sliver[i].id,
                        .subid = sliver[i].subid,
                        .light_n = 30
                    };
                }
            }
        }
        return;
    }
    #endif
    int64_t nx = cx * 16;
    int64_t nz = cz * 16;
    for (int z = 0; z < 16; ++z) {
        for (int x = 0; x < 16; ++x) {
            int xzoff = z * 16 + x;
            double cx = (double)(nx + x) - 8;
            double cz = (double)(nz + z) - 8;
            struct blockdata sliver[512];
            memset(&sliver, 0, sizeof(sliver));
            genSliver(type, cx, cz, sliver);
            for (int i = 511; i >= 0; --i) {
                struct blockdata* tdata = &data[256 * i + xzoff];
                *tdata = (struct blockdata){
                    .id = sliver[i].id,
                    .subid = sliver[i].subid,
                    .light_n = 30
                };
                /*
                ((uint16_t*)tdata)[0] = getRandDWord(15);
                ((uint16_t*)tdata)[1] = getRandDWord(15);
                ((uint16_t*)tdata)[2] = getRandDWord(15);
                int maxsub = 64;
                while (1) {
                    if (blockinf[tdata->id].data[maxsub - 1].id) break;
                    if (maxsub <= 1) break;
                    --maxsub;
                }
                tdata->subid = tdata->subid % maxsub;
                */
            }
        }
    }
}
