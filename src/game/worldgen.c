#include <main/main.h>
#include "worldgen.h"
#include "blocks.h"
#include <common/noise.h>

#include <math.h>
#include <stdlib.h>
//#include <stdio.h>
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

static force_inline float clamp(float x) {
    if (x > 1.0) x = 1.0;
    else if (x < -1.0) x = -1.0;
    return x;
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
            float height = nperlin2d(1, cx, cz, 0.0042, 5) * 1.1 + 0.15;
            float heightmult = tanhf(nperlin2d(2, cx, cz, 0.00267, 2) * 2.5 + 0.33) * 0.5 + 0.5;
            float humidity = clamp(nperlin2d(3, cx, cz, 0.00127, 1) * 6.9 + 3.33) * 0.5 + 0.5;
            heightmult *= (humidity * 0.9 + 0.15);
            float detail = perlin2d(4, cx, cz, 0.021, 3);
            float finalheight = (height * heightmult * 85.0) * humidity + 20.0 * (1.0 - humidity) + (detail * 12.8) + 128.0;
            for (int i = finalheight; i <= 128; ++i) {
                data[i].id = water;
            }
            for (int i = 0; i <= round(finalheight) && i < 512; ++i) {
                block[i] = true;
            }
            float extraheight = (tanhf(nperlin2d(5, cx, cz, 0.0145, 1) * 1.5 - (2.15 - (heightmult + (1.0 - humidity) * 0.75) * 1.0)) * 0.5 + 0.5) * heightmult * 64.33;
            extraheight *= clamp(height * 10.0 + 3.33);
            float extrafinalh = extraheight + finalheight;
            for (int i = finalheight; i < extrafinalh; ++i) {
                float fi = i;
                if (!block[i] && noise3(6, cx / 18.67, fi / 19.0, cz / 18.67) > -(((extrafinalh - fi) / extraheight)) * 1.33 + 0.45) {
                    block[i] = true;
                }
            }
            int maxblock = 511;
            while (maxblock > 0 && !block[maxblock - 1]) --maxblock;
            float seanoise = nperlin2d(7, cx, cz, 0.0125, 2);
            float dirtnoise = nperlin2d(8, cx, cz, 0.1, 2);
            for (int i = maxblock, lastair = i; i > 0; --i) {
                float fi = i;
                float fl = lastair;
                if (block[i]) {
                    if ((fl - (135.0 + seanoise * 2.25)) * (1.3 + seanoise * 0.1) <= (fi - (135.0 + seanoise * 2.25))) {
                        float mixnoise = noise3(9, cx / 28.56, fi / 4.2, cz / 28.56);
                        if (mixnoise > 0.62 + (fi - 128.0) / 38.0) {
                            if (mixnoise > 0.295 - seanoise * 0.33) {
                                data[i].id = dirt;
                            } else {
                                data[i].id = gravel;
                            }
                        } else {
                            data[i].id = sand;
                        }
                    } else if (i == lastair - 1) {
                        if (humidity >= 0.425 + dirtnoise * 0.159) {
                            if (i >= 128) {
                                data[i].id = grass_block;
                            } else {
                                data[i].id = dirt;
                            }
                        } else {
                            data[i].id = sand;
                        }
                    } else if (fi > fl - ((511.0 - fi + dirtnoise * 15.0) / 511.0) * 24.0 + 12.75) {
                        data[i].id = dirt;
                    } else {
                        data[i].id = stone;
                        if (noise3(10, cx / 7.33, fi / 2.1, cz / 7.33) < (fi / 512.0) * 2.5 - 1.25) {
                            data[i].subid = stone_granite;
                        } else {
                            float mix1 = noise3(11, cx / 6.2, fi / 5.6, cz / 6.2);
                            if (mix1 + 0.5 > fi / 60.0) {
                                data[i].subid = stone_basalt;
                            } else if (mix1 < -0.5) {
                                data[i].subid = stone_cobble;
                            } else {
                                float mix2 = noise3(12, cx / 17.34, fi / 15.8, cz / 17.34);
                                if (mix2 > 0.456) {
                                    data[i].id = dirt;
                                    data[i].subid = 0;
                                } else if (mix2 < -0.474) {
                                    data[i].id = gravel;
                                    data[i].subid = 0;
                                }
                            }
                        }
                    }
                } else {
                    lastair = i;
                }
            }
            for (int i = 0; i <= maxblock; ++i) {
                if (data[i].id && data[i].id != water) {
                    float fi = i - 0.25;
                    float cave = noise3(16, cx / 21.1473, fi / 15.21837, cz / 21.1473);
                    float cavemult = tanhf(((fabs(fi - (finalheight / 2.0)) / (finalheight * 1.044)) * 2.0 - 1.0) * 16.0) * 0.5 + 0.5;
                    if (cave > cavemult + 0.345) {
                        data[i].id = 0;
                        data[i].subid = 0;
                    }
                }
            }
            float n0 = nperlin2d(63, cx, cz, 0.4, 2);
            if (n0 > -0.25) {data[1].id = bedrock; data[1].subid = 0;}
            if (n0 > 0.0) {data[2].id = bedrock; data[2].subid = 0;}
            if (n0 > 0.25) {data[3].id = bedrock; data[3].subid = 0;}
            if (!data[4].id && n0 > 0.5) {data[4].id = bedrock; data[4].subid = 0;}
            data[0].id = bedrock;
            data[0].subid = 0;
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
    //uint64_t t = altutime();
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
    //t = altutime() - t;
    //printf("Generated [%"PRId64", %"PRId64"] in %"PRIu64"us\n", cx, cz, t);
}
