#include "worldgen.h"
#include <noise.h>

#include <math.h>
#include <string.h>
#include <stdlib.h>

static inline void genSliver(int type, double cx, double cz, int top, int btm, uint8_t* data) {
    switch (type) {
        default:; {
                if (!btm) {
                    data[0] = 6;
                    data[1] = 1;
                    data[2] = 2;
                    data[3] = 3;
                }
            }
            break;
        case 1:; {
                for (int y = btm; y <= top && y < 65; ++y) {
                    data[y - btm] = 7;
                }
                double p0 = tanhf(nperlin2d(0, cx, cz, 0.006675, 6));
                if (p0 < 0) p0 *= 0.5;
                double p1 = tanhf(nperlin2d(1, cx, cz, 0.001442, 2) * 3);
                if (p1 < 0) p1 *= 0.5;
                double h0 = p0 * 20 + p1 * 40;
                double m0 = h0 / 60;
                if (m0 < 0) m0 *= 0.75;
                m0 += 0.25;
                if (m0 > 1) m0 = 1;
                if (m0 < -1) m0 = -1;
                double m1 = (1 - (cos(2 * fabs(m0) * M_PI) / 2 + 0.5)) * 0.9 + 0.1;
                double p2 = (tanhf(nperlin2d(2, cx, cz, 0.010628, 2) * 4) + tanhf(nperlin2d(6, cx, cz, 0.020628, 2) * 1.5) * 0.5) * 2.54;
                if (p2 < 0) p2 *= 0.33;
                double p3 = tanhf(nperlin2d(3, cx, cz, 0.025, 2) * (3 + perlin2d(7, cx, cz, 0.01, 2) * 3));
                double p4 = (tanhf(nperlin2d(4, cx, cz, 0.012847, 3) * 2) * 1.095) * 0.33 + 0.67;
                if (p4 < 0.05) p4 = 0.05;
                double h1 = (p2 * 10 + p3 * 5) * p4 * m1;
                double h2 = (tanhf(nperlin2d(5, cx, cz, 0.005291, 1) * 2) / 2 + 0.5) * 0.9 + 0.2;
                double h = (h1 + h0) * h2 + 65 + tanhf(nperlin2d(12, cx, cz, 0.0375, 2) * 2) * 2;
                double p9 = perlin2d(13, cx, cz, 0.025148, 5);
                double p10 = perlin2d(14, cx, cz, 0.184541, 1);
                double p11 = perlin2d(15, cx, cz, 0.049216, 2);
                int hi = round(h);
                for (int y = (hi > top) ? top : hi; y >= btm; --y) {
                    bool c1 = y < 62 + p9 * 6;
                    bool c2 = p11 > 0.4 && y < 56 + p10 * 3;
                    uint8_t b1 = (c1) ? ((c2) ? 4 : 8) : 2;
                    uint8_t b2 = (c1) ? ((c2) ? 4 : 8) : ((y < 64) ? 2 : 3);
                    uint8_t blockid = (y < hi) ? ((y < hi - (3 - round(fabs(p4) * 2))) ? 1 : b1) : b2;
                    data[y - btm] = blockid;
                }
                int n0 = noise2d(8, cx, cz);
                if (!btm) {
                    data[0] = 6;
                    if (!(n0 % 2) || !(n0 % 3)) data[1] = 6;
                    if (!(n0 % 4)) data[2] = 6;
                }
            }
            break;
    }
}

bool genChunk(struct chunkinfo* chunks, int cx, int cy, int cz, int64_t xo, int64_t zo, struct blockdata* data, int type) {
    int64_t nx = ((int64_t)cx + xo) * 16;
    int64_t nz = ((int64_t)cz * -1 + zo) * 16;
    cx += chunks->dist;
    cz += chunks->dist;
    bool ct = 0;
    int btm = cy * 16;
    int top = (cy + 1) * 16 - 1;
    for (int z = 0; z < 16; ++z) {
        for (int x = 0; x < 16; ++x) {
            int xzoff = z * 16 + x;
            double cx = (double)(nx + x);
            double cz = (double)(nz + z);
            uint8_t sliver[16];
            memset(&sliver, 0, 16);
            genSliver(type, cx, cz, top, btm, sliver);
            for (int i = 0; i < 16; ++i) {
                struct blockdata* tdata = &data[256 * i + xzoff];
                memset(tdata, 0, sizeof(struct blockdata));
                *tdata = (struct blockdata){.id = sliver[i], .light = 14};
            }
        }
    }
    ct = true;
    return ct;
}
