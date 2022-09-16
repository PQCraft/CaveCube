#include <main/main.h>
#include "worldgen.h"
#include "blocks.h"
#include <common/noise.h>

#include <math.h>
#include <string.h>
#include <inttypes.h>

static uint8_t stone;
static uint8_t dirt;
static uint8_t grass_block;
static uint8_t gravel;
static uint8_t sand;
static uint8_t water;
static uint8_t bedrock;

bool initWorldgen() {
    stone = blockNoFromID("stone");
    dirt = blockNoFromID("dirt");
    grass_block = blockNoFromID("grass_block");
    gravel = blockNoFromID("gravel");
    sand = blockNoFromID("sand");
    water = blockNoFromID("water");
    bedrock = blockNoFromID("bedrock");
    return true;
}

static inline void genSliver(int type, double cx, double cz, uint8_t* data) {
    switch (type) {
        default:; {
            data[0] = bedrock;
            data[1] = stone;
            data[2] = dirt;
            data[3] = grass_block;
            break;
        }
        case 1:; {
            for (int y = 0; y < 65; ++y) {
                data[y] = water;
            }
            double p0 = tanhf(nperlin2d(0, cx, cz, 0.006675, 5));
            if (p0 < 0) p0 *= 0.5;
            double p1 = tanhf((nperlin2d(1, cx, cz, 0.001142, 2) + 0.15) * 3);
            if (p1 < 0) p1 *= 0.7;
            double h0 = p0 * 20 + p1 * 40;
            double m0 = h0 / 60;
            if (m0 < 0) m0 *= 0.75;
            m0 += 0.25;
            if (m0 > 1) m0 = 1;
            if (m0 < -1) m0 = -1;
            double m1 = (1 - (cos(2 * fabs(m0) * M_PI) / 2 + 0.5)) * 0.9 + 0.1;
            double p2 = (tanhf((nperlin2d(2, cx, cz, 0.010231, 1) - 0.025) * 12.5) + tanhf((nperlin2d(6, cx, cz, 0.009628, 2) - 0.05) * 12.5) * 0.5) * 1.89;
            double p3 = tanhf((nperlin2d(3, cx, cz, 0.01, 3) - 0.33) * (5 + perlin2d(7, cx, cz, 0.025, 2) * 10));
            double p4 = (tanhf(nperlin2d(4, cx, cz, 0.012847, 3) * 2) * 1.095) * 0.33 + 0.67;
            if (p4 < 0.05) p4 = 0.05;
            double h1 = (p2 * 8 + p3 * 7) * p4 * m1;
            double h2 = (tanhf(nperlin2d(5, cx, cz, 0.005291, 1) * 2) / 2 + 0.5) * 0.9 + 0.2;
            double h = (h1 + h0) * h2 + 65 + tanhf(nperlin2d(12, cx, cz, 0.0375, 3) * 1.5) * 2;
            double p9 = nperlin2d(13, cx, cz, 0.025148, 5);
            double p10 = perlin2d(14, cx, cz, 0.184541, 1);
            double p11 = perlin2d(15, cx, cz, 0.049216, 2);
            int hi = round(h);
            for (int y = hi; y >= 0; --y) {
                bool c1 = y < 67 + p9 * 5;
                bool c2 = p11 > 0.4 && y < 56 + p10 * 3;
                uint8_t b1 = (c1) ? ((c2) ? gravel : sand) : dirt;
                uint8_t b2 = (c1) ? ((c2) ? gravel : sand) : ((y < 64) ? dirt : grass_block);
                uint8_t blockid = (y < hi) ? ((y < hi - (3 - round(fabs(p4) * 2))) ? stone : b1) : b2;
                data[y] = blockid;
            }
            int n0 = noise2d(8, cx, cz);
            data[0] = bedrock;
            if (!(n0 % 2) || !(n0 % 3)) data[1] = bedrock;
            if (!(n0 % 4)) data[2] = bedrock;
            break;
        }
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
            uint8_t sliver[256];
            memset(&sliver, 0, 256);
            genSliver(type, cx, cz, sliver);
            for (int i = 0; i < 256; ++i) {
                struct blockdata* tdata = &data[256 * i + xzoff];
                memset(tdata, 0, sizeof(struct blockdata));
                *tdata = (struct blockdata){.id = sliver[i], .light = 15};
            }
        }
    }
}
