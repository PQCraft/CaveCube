#include "worldgen.h"
#include <noise.h>

#include <math.h>
#include <string.h>

static inline void genSliver_v5(double cx, double cz, int top, int btm, uint8_t* data) {
    for (int y = btm; y <= top && y < 65; ++y) {
        data[y - btm] = 7;
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
    double p2 = tanhf((nperlin2d(2, cx, cz, 0.009128, 3) - 0.1) * 6) * 2.14;
    if (p2 < 0) p2 *= 0.33;
    double p4 = tanhf((nperlin2d(4, cx, cz, 0.006847, 2) - 0.1) * 4) * 1.095;
    if (p4 < 0.05) p4 = 0.05;
    double h1 = (p2 * 9.75 * p4 + p3 * 5) * m1;
    double h2 = (tanhf(nperlin2d(5, cx, cz, 0.005291, 1) * 2) / 2 + 0.5) * 0.9 + 0.2;
    double h = (h1 + h0) * h2 + 65;
    double p9 = perlin2d(9, cx, cz, 0.025148, 5);
    double p10 = perlin2d(10, cx, cz, 0.184541, 1);
    double p11 = perlin2d(11, cx, cz, 0.049216, 2);
    int hi = round(h);
    for (int y = (hi > top) ? top : hi; y >= btm; --y) {
        bool c1 = y < 62 + p9 * 6;
        bool c2 = p11 > 0.33 && y < 56 + p10 * 3;
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
                        double s1 = perlin2d(0, (double)(nx + x) / 27, (double)(nz + z) / 27, 2.0, 1);
                        double s2m = perlin2d(8, (double)(nx + x) / 43, (double)(nz + z) / 43, 1.0, 1) * 2.0;
                        double s2 = perlin2d(1, (double)(nx + x) / (149 + 30 * s2m), (double)(nz + z) / (149 + 30 * s2m), .5, 4);
                        double s3 = perlin2d(2, (double)(nx + x) / 87, (double)(nz + z) / 87, 1.0, 1);
                        double s4 = perlin2d(3, (double)(nx + x) / 63, (double)(nz + z) / 63, 1.0, 1);
                        double s5 = perlin2d(4, (double)(nx + x) / 105, (double)(nz + z) / 105, 1.0, 1);
                        for (int y = btm; y <= top && y < 65; ++y) {
                            data[(y - btm) * 256 + z * 16 + x].id = 7;
                        }
                        double s = s1;
                        s *= s5 * 5 * (1.0 - s2);
                        s += s2 * 50 + (1 - s5) * 4;
                        s -= 18;
                        double sf = (double)(s * 2) - 8;
                        int si = (int)sf + 60;
                        uint8_t blockid;
                        blockid = (s1 + s2 < s3 * 1.125) ? 8 : 3;
                        blockid = (sf < 5 + s1 * 3) ? ((sf < -((s4 + 2.25) * 7)) ? 2 : 8) : blockid;
                        if (si >= btm && si <= top) data[(si - btm) * 256 + z * 16 + x].id = blockid;
                        for (int y = ((si - 1) > top) ? top : si - 1; y >= btm; --y) {
                            data[(y - btm) * 256 + z * 16 + x].id = (y < (double)(si) * 0.9) ? 1 : ((blockid == 8 && y > si - 4) ? 8 : 2);
                        }
                        if (!btm) data[z * 16 + x].id = 6;
                    }
                    break;
                case 2:; {
                        double s1 = perlin2d(1, (double)(nx + x) / 30, (double)(nz + z) / 30, 1, 3);
                        double s2 = perlin2d(0, (double)(nx + x) / 187, (double)(nz + z) / 187, 2, 4);
                        double s3 = perlin2d(2, (double)(nx + x) / 56, (double)(nz + z) / 56, 1, 8);
                        int s4 = noise2d(3, (double)(nx + x), (double)(nz + z));
                        double s5 = perlin2d(4, (double)(nx + x) / 329, (double)(nz + z) / 329, 1, 2);
                        for (int y = btm; y <= top && y < 65; ++y) {
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
                        uint8_t sliver[16];
                        memset(&sliver, 0, 16);
                        genSliver_v5(cx, cz, top, btm, sliver);
                        for (int i = 0; i < 16; ++i) {
                            data[256 * i + xzoff].id = sliver[i];
                        }
                    }
                    break;
            }
        }
    }
    ct = true;
    return ct;
}
