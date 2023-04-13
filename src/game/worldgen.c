#include <main/main.h>
#include "worldgen.h"
#include "blocks.h"
#include <common/noise.h>

#include <math.h>
#include <stdlib.h>
//#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static uint16_t stone;
static uint16_t stone_cobble;
static uint16_t stone_basalt;
static uint16_t stone_granite;
static uint16_t dirt;
static uint16_t grass_block;
static uint16_t gravel;
static uint16_t sand;
static uint16_t water;
static uint16_t lava;
static uint16_t bedrock;

bool initWorldgen() {
    stone = blockNoFromID("stone");
    stone_cobble = blockSubNoFromID(stone, "cobble") << 8;
    stone_basalt = blockSubNoFromID(stone, "basalt") << 8;
    stone_granite = blockSubNoFromID(stone, "granite") << 8;
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
    memset(data, 0, 131072 * sizeof(*data));
    int64_t nx = cx * 16;
    int64_t nz = cz * 16;
    unsigned sliver[512];
    unsigned block[512];
    for (int z = 0; z < 16; ++z) {
        for (int x = 0; x < 16; ++x) {
            double cx = (double)(nx + x) - 8;
            double cz = (double)(nz + z) - 8;
            memset(&sliver, 0, sizeof(sliver));
            memset(&block, 0, sizeof(block));
            switch (type) {
                default:; {
                    sliver[0] = bedrock;
                    sliver[1] = stone;
                    sliver[2] = dirt;
                    sliver[3] = grass_block;
                } break;
                case 1:; {
                    float height = nperlin2d(1, cx, cz, 0.0042, 4) * 1.2;
                    float heightmult = tanhf(nperlin2d(2, cx, cz, 0.00267, 2) * 2.5 + 0.225) * 0.5 + 0.5;
                    float humidity = clamp(nperlin2d(3, cx, cz, 0.00127, 1) * 6.9 + 3.33) * 0.5 + 0.5;
                    heightmult *= (humidity * 0.9 + 0.15);
                    float detail = perlin2d(4, cx, cz, 0.028, 3);
                    float finalheight = (height * heightmult * 85.0) * humidity + 20.0 * (1.0 - humidity) + (detail * 11.7) + 128.0;
                    {
                        unsigned i = finalheight;
                        if (i > 511) i = 511;
                        for (; i <= 128; ++i) {
                            sliver[i] = water;
                        }
                    }
                    {
                        unsigned tmp = round(finalheight);
                        if (tmp > 511) tmp = 511;
                        for (unsigned i = 0; i <= tmp; ++i) {
                            block[i] = true;
                        }
                    }
                    float exhm = heightmult + (1.0 - humidity) * 0.5;
                    float extraheight = (tanhf((nperlin2d(5, cx, cz, 0.01, 2) * 1.67 - (2.2 - exhm) + 0.28) * 2.0) * 0.5 + 0.5) * exhm * 37.33;
                    extraheight *= clamp(height * 5.0 + 5.0);
                    float extrafinalh = extraheight + finalheight;
                    {
                        unsigned i = finalheight;
                        if (i > 511) i = 511;
                        for (; i < extrafinalh; ++i) {
                            float fi = i;
                            if (!block[i] && noise3(6, cx / 16.3, fi / 14.2, cz / 16.3) > -(((extrafinalh - fi) / extraheight)) * 1.5 + 0.15) {
                                block[i] = true;
                            }
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
                                        sliver[i] = dirt;
                                    } else {
                                        sliver[i] = gravel;
                                    }
                                } else {
                                    sliver[i] = sand;
                                }
                            } else if (i == lastair - 1) {
                                if (humidity >= 0.425 + dirtnoise * 0.159) {
                                    if (i >= 128) {
                                        sliver[i] = grass_block;
                                    } else {
                                        sliver[i] = dirt;
                                    }
                                } else {
                                    sliver[i] = sand;
                                }
                            } else if (fi > fl - ((511.0 - fi + dirtnoise * 15.0) / 511.0) * 24.0 + 12.75) {
                                sliver[i] = dirt;
                            } else {
                                sliver[i] = stone;
                                if (noise3(10, cx / 7.33, fi / 2.1, cz / 7.33) < (fi / 512.0) * 2.5 - 1.25) {
                                    sliver[i] |= stone_granite;
                                } else {
                                    float mix1 = noise3(11, cx / 6.2, fi / 5.6, cz / 6.2);
                                    if (mix1 + 0.5 > fi / 60.0) {
                                        sliver[i] |= stone_basalt;
                                    } else if (mix1 < -0.5) {
                                        sliver[i] |= stone_cobble;
                                    } else if (mix1 > -0.25 && mix1 < 0.25) {
                                        float mix2 = noise3(12, cx / 14.1, fi / 7.8, cz / 14.1);
                                        if (mix2 > 0.56) {
                                            sliver[i] = dirt;
                                        } else if (mix2 < -0.62) {
                                            sliver[i] = gravel;
                                        }
                                    }
                                }
                            }
                        } else {
                            lastair = i;
                        }
                    }
                    float fhtweak = (finalheight + extrafinalh) / 2.0;
                    for (int i = 0; i <= maxblock; ++i) {
                        if (sliver[i] && sliver[i] != water) {
                            float fi = i;
                            float cave = noise3(16, cx / 20.1473, fi / 15.21837, cz / 20.1473);
                            float cavemult = tanhf(((fabs(fi - (fhtweak / 2.0)) / round(extrafinalh * 1.05)) * 2.0 - 1.0) * 10.0) * 0.5 + 0.5;
                            if (cave > cavemult + 0.37) {
                                sliver[i] = 0;
                            }
                        }
                    }
                    float n0 = nperlin2d(63, cx, cz, 0.4, 2);
                    if (n0 > -0.25) {sliver[1] = bedrock;}
                    if (n0 > 0.0) {sliver[2] = bedrock;}
                    if (n0 > 0.2) {sliver[3] = bedrock;}
                    if (!sliver[4] && n0 > 0.5) {sliver[4] = bedrock;}
                    sliver[0] = bedrock;
                } break;
            }
            int xzoff = z * 16 + x;
            for (int i = 511; i >= 0; --i) {
                struct blockdata* tdata = &data[256 * i + xzoff];
                bdsetid(tdata, sliver[i] & 0xFF);
                bdsetsubid(tdata, (sliver[i] >> 8) & 0x3F);
                bdsetlightn(tdata, 30);
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
