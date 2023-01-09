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
static uint8_t dirt;
static uint8_t grass_block;
static uint8_t gravel;
static uint8_t sand;
static uint8_t water;
static uint8_t bedrock;

bool initWorldgen() {
    stone = blockNoFromID("stone");
    stone_cobble = blockSubNoFromID(stone, "cobble");
    dirt = blockNoFromID("dirt");
    grass_block = blockNoFromID("grass_block");
    gravel = blockNoFromID("gravel");
    sand = blockNoFromID("sand");
    water = blockNoFromID("water");
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
            /*
            int64_t chunkx = (cx + 8.0);
            int64_t chunkz = (cz + 8.0);
            if (chunkx < 0) chunkx -= 16;
            if (chunkz > 0) chunkz += 16;
            chunkx /= 16;
            chunkz /= 16;
            if ((chunkx + chunkz) % 2) {
            */
            double heightmult = tanh((noise2(0, cx / 274.0, cz / 274.0) * 2.0) * 0.5 + 0.5);
            double detail = nperlin2d(1, cx, cz, 0.034559, 5) * 1.5;
            double height = tanh(nperlin2d(2, cx, cz, 0.002253, 2) * 5.0) * heightmult;
            height += ((1.0 - tanhf(perlin2d(3, cx, cz, 0.004992, 4) * 2.5)) * 2.5 - 0.5) * (heightmult + 0.5 + height * 0.1);
            double finalheight = round(height * 50 + detail * 2 + 128.0);
            for (int i = 0; i < finalheight; ++i) {
                data[i].id = stone;
            }
            for (int i = 0; i < 512; ++i) {
                if ((noise3(15, cx / 24.5, i / 14.0, cz / 24.5) + fabs((i - (50.0 + height * 35.0)) / (400.0 + height * 200.0))) < -0.25) {
                    data[i].id = 0;
                }
            }
            for (int i = 127; i >= finalheight; --i) {
                data[i].id = water;
            }
            double n0 = noise3(63, cx / 2.0, cz / 2.0, 0);
            data[0].id = bedrock; data[0].subid = 0;
            if (n0 > -0.25) {data[1].id = bedrock; data[1].subid = 0;}
            if (n0 > 0.0) {data[2].id = bedrock; data[2].subid = 0;}
            if (n0 > 0.25) {data[3].id = bedrock; data[3].subid = 0;}
            /*
            }
            */
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
            float nlight = 31;
            for (int i = 511; i >= 0; --i) {
                struct blockdata* tdata = &data[256 * i + xzoff];
                //if (sliver[i].id == water) {nlight -= 1.75; if (nlight < -128) nlight = -128;}
                int8_t nlight_r = nlight;
                if (nlight_r < 0) nlight_r = 0;
                int8_t nlight_g = 31 - (31 - nlight) * 0.33;
                if (nlight_g < 0) nlight_g = 0;
                int8_t nlight_b = 31 - (31 - nlight) * 0.25;
                if (nlight_b < 0) nlight_b = 0;
                *tdata = (struct blockdata){
                    .id = sliver[i].id,
                    .subid = sliver[i].subid,
                    .light_n_r = nlight_r,
                    .light_n_g = nlight_g,
                    .light_n_b = nlight_b
                };
                /*
                ((uint64_t*)tdata)[0] = getRandQWord(15);
                int maxsub = 64;
                while (1) {
                    if (blockinf[tdata->id].data[maxsub - 1].id) break;
                    if (maxsub <= 1) break;
                    --maxsub;
                }
                tdata->subid = tdata->subid % maxsub;
                //tdata->light_n_r = 31;
                //tdata->light_n_g = 31;
                //tdata->light_n_b = 31;
                */
            }
        }
    }
}
