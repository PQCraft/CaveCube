#include <main/main.h>
#include "noise.h"
#include "common.h"

#include <math.h>
#include <stdlib.h>

#define NMAGIC(x) (fmod(fabs(x), 256.0) / 128.0)

static const int SEED = 1985;

unsigned char* hash[NOISE_TABLES];

void initNoiseTable(int s) {
    for (int i = 0; i < NOISE_TABLES; ++i) {
        if (!hash[i]) hash[i] = malloc(256);
        for (int j = 0; j < 256; ++j) {
            hash[i][j] = getRandByte(s);
        }
    }
}

static _inline int64_t noise2(int t, int64_t x, int64_t y) {
    int64_t yindex = (y + SEED) % 256;
    if (yindex < 0) yindex += 256;
    int64_t xindex = (hash[t][yindex] + x) % 256;
    if (xindex < 0) xindex += 256;
    return hash[t][xindex];
}

static _inline double lin_inter(double x, double y, double s) {
    return x + s * (y - x);
}

static _inline double smooth_inter(double x, double y, double s) {
    return lin_inter(x, y, s * s * (3 - 2 * s));
}

double noise2d(int tbl, double x, double y) {
    const int64_t x_int = floor(x);
    const int64_t y_int = floor(y);
    const double x_frac = x - x_int;
    const double y_frac = y - y_int;
    const int64_t s = noise2(tbl, x_int, y_int);
    const int64_t t = noise2(tbl, x_int + 1, y_int);
    const int64_t u = noise2(tbl, x_int, y_int + 1);
    const int64_t v = noise2(tbl, x_int + 1, y_int + 1);
    const double low = smooth_inter(s, t, x_frac);
    const double high = smooth_inter(u, v, x_frac);
    const double result = smooth_inter(low, high, y_frac);
    return result;
}

double nnoise2d(int tbl, double x, double y) {
    const int64_t x_int = floor(x);
    const int64_t y_int = floor(y);
    const double x_frac = x - x_int;
    const double y_frac = y - y_int;
    const int64_t s = noise2(tbl, x_int, y_int);
    const int64_t t = noise2(tbl, x_int + 1, y_int);
    const int64_t u = noise2(tbl, x_int, y_int + 1);
    const int64_t v = noise2(tbl, x_int + 1, y_int + 1);
    const double low = smooth_inter(s, t, x_frac);
    const double high = smooth_inter(u, v, x_frac);
    const double result = smooth_inter(low, high, y_frac);
    return result * 2 - 1;
}

double perlin2d(int t, double x, double y, double freq, int depth) {
    double xa = x * freq;
    double ya = y * freq;
    double amp = 1.0;
    double fin = 0;
    double div = 0.0;
    for (int i = 0; i < depth; i++) {
        div += 256 * amp;
        fin += noise2d(t, xa, ya) * amp;
        amp /= 2;
        xa += xa;
        ya += ya;
    }
    return fin / div;
}

double nperlin2d(int t, double x, double y, double freq, int depth) {
    return perlin2d(t, x, y, freq, depth) * 2 - 1;
}

double mperlin2d(int t, double x, double y, double freq, int depth, int samples) {
    double s = 0.0;
    double div = (1 + samples * 2) + samples * samples * 2;
    for (int i = -samples; i <= samples; ++i) {
        int s2 = samples - abs(i);
        for (int j = -s2; j <= s2; ++j) {
            s += perlin2d(t, x, y, freq, depth);
        }
    }
    return s / div;
}
