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
            /*
            int64_t chunkx = (cx + 8.0);
            int64_t chunkz = (cz + 8.0);
            if (chunkx < 0) chunkx -= 16;
            if (chunkz > 0) chunkz += 16;
            chunkx /= 16;
            chunkz /= 16;
            if ((chunkx + chunkz) % 2) {
            */
            float heightmult = tanhf((perlin2d(0, cx, cz, 0.003649, 2) * 3.75) * 0.5 + 0.65);
            float height = tanhf(nperlin2d(1, cx, cz, 0.001253, 7) * 3.0) * heightmult;
            float detail = nperlin2d(2, cx, cz, 0.03153, 3);
            height *= (1.0 - (height * 0.5 - 0.33)) * 0.825 * heightmult;
            height += 0.33;
            float caveheight = height * 1.2;
            float finalheight = round((height) * 75.0 + detail * 2.0 * heightmult + 128.0);
            float grounddiff = round((perlin2d(3, cx, cz, 0.05, 4) * 0.2 + 4.0) - tanhf((finalheight + (detail - 0.75) * 25.0 - 128.0) / 95.0) * 4.25);
            for (int i = 0; i <= finalheight && i < 512; ++i) {
                if (i > finalheight - grounddiff) {
                    if ((height) > 0.05 + detail * 0.05) {
                        data[i].id = (i == finalheight) ? grass_block : dirt;
                    } else {
                        if ((float)i + (detail * 0.5 + 1.0) * 2.0 >= finalheight) {
                            if (noise3(4, cx / 21.124, (float)(i) / 16.384, cz / 21.124) < -0.64 - ((finalheight - 128.0) / 128.0)) {
                                data[i].id = gravel;
                            } else {
                                data[i].id = sand;
                            }
                        } else {
                            data[i].id = dirt;
                        }
                    }
                } else {
                    data[i].id = stone;
                    if (noise3(5, cx / 12.75, (float)(i) / 3.5, cz / 12.75) < ((float)(i) / 512.0) * 1.75 - 1.0) {
                        data[i].subid = stone_granite;
                    } else if (noise3(6, cx / 10.45, (float)(i) / 10.45, cz / 10.45) + 0.5 > (float)(i) / 40.0) {
                        data[i].subid = stone_basalt;
                    } else if (noise3(7, cx / 5.56, (float)(i) / 5.56, cz / 5.56) + 0.25 < 0.0) {
                        data[i].subid = stone_cobble;
                    }
                }
            }
            for (int i = 0; i < 512 && i < 512; ++i) {
                float fi = i;
                float cave = noise3(15, cx / 23.25, fi / (25.0 - fi / 512.0 * 20.0), cz / 23.25) + fabs((fi - (30.0 + caveheight * 20.0)) / (300.0 + caveheight * 175.0));
                if (cave < -(0.25 + fi / 512.0 * 0.04)) {
                    data[i].id = 0;
                    data[i].subid = 0;
                }
            }
            for (int i = 127; i > finalheight && i > 0; --i) {
                data[i].id = water;
                data[i].subid = 0;
            }
            data[0].id = bedrock;
            data[0].subid = 0;
            float n0 = nperlin2d(63, cx, cz, 0.4, 2);
            if (n0 > -0.25) {data[1].id = bedrock; data[1].subid = 0;}
            if (n0 > 0.0) {data[2].id = bedrock; data[2].subid = 0;}
            if (n0 > 0.25) {data[3].id = bedrock; data[3].subid = 0;}
            if (!data[4].id && n0 > 0.5) {data[4].id = bedrock; data[4].subid = 0;}
            for (int i = 4; i > 0; --i) {
                if (!data[i].id) {data[i].id = lava; data[i].subid = 0;}
            }
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
            for (int i = 511; i >= 0; --i) {
                struct blockdata* tdata = &data[256 * i + xzoff];
                *tdata = (struct blockdata){
                    .id = sliver[i].id,
                    .subid = sliver[i].subid,
                    .light_n_r = 30,
                    .light_n_g = 30,
                    .light_n_b = 30
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
