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
            float heightmult = tanhf(nperlin2d(2, cx, cz, 0.0015, 2) * 2.5 + 0.5) * 0.5 + 0.5;
            float detail = perlin2d(3, cx, cz, 0.0175, 4);
            float finalheight = (height * heightmult * 80.0) + (detail * 15.0) + 128.0;
            for (int i = finalheight; i <= 128; ++i) {
                data[i].id = water;
            }
            for (int i = 0; i <= finalheight && i < 512; ++i) {
                block[i] = true;
            }
            float extraheight = (tanhf(nperlin2d(4, cx, cz, 0.015, 1) * 1.5 - (2.0 - heightmult * 1.0)) * 0.5 + 0.5) * 55.0 * heightmult;
            float extrafinalh = extraheight + finalheight;
            for (int i = finalheight; i <= extrafinalh; ++i) {
                float fi = i;
                if (noise3(5, cx / 18.0, fi / 18.0, cz / 18.0) > -(((extrafinalh - fi) / extraheight)) * 1.5 + 0.15) {
                    block[i] = true;
                }
            }
            data[0].id = bedrock;
            data[0].subid = 0;
            for (int i = 1; i < 512; ++i) {
                if (block[i]) {
                    data[i].id = stone;
                }
            }
        } break;
    }
}

void genChunk(int64_t cx, int64_t cz, struct blockdata* data, int type) {
    //printf("GEN [%"PRId64", %"PRId64"]\n", cx, cz);
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
