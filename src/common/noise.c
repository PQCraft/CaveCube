#include "common.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define NMAGIC(x) (fmod(fabs(x), 256.0) / 128.0)

static const int SEED = 1985;

unsigned char* hash[16];

void initNoiseTable() {
    for (int i = 0; i < 16; ++i) {
        if (!hash[i]) hash[i] = malloc(256);
        for (int j = 0; j < 256; ++j) {
            hash[i][j] = getRandByte();
        }
    }
}

static int noise2(int t, int x, int y) {
    int yindex = (y + SEED) % 256;
    if (yindex < 0) yindex += 256;
    int xindex = (hash[t][yindex] + x) % 256;
    if (xindex < 0)  xindex += 256;
    return hash[t][xindex];
}

static float lin_inter(float x, float y, float s) {
    return x + s * (y - x);
}

static float smooth_inter(float x, float y, float s) {
    return lin_inter(x, y, s * s * (3 - 2 * s));
}

float noise2d(int tbl, float x, float y) {
    const int x_int = floor(x);
    const int y_int = floor(y);
    const float x_frac = x - x_int;
    const float y_frac = y - y_int;
    const int s = noise2(tbl, x_int, y_int);
    const int t = noise2(tbl, x_int + 1, y_int);
    const int u = noise2(tbl, x_int, y_int + 1);
    const int v = noise2(tbl, x_int + 1, y_int + 1);
    const float low = smooth_inter(s, t, x_frac);
    const float high = smooth_inter(u, v, x_frac);
    const float result = smooth_inter(low, high, y_frac);
    return result;
}

float perlin2d(int t, float x, float y, float freq, int depth) {
    float xa = x * freq;
    float ya = y * freq;
    float amp = 1.0;
    float fin = 0;
    float div = 0.0;
    for (int i = 0; i < depth; i++) {
        div += 256 * amp;
        fin += noise2d(t, xa, ya) * amp;
        amp /= 2;
        xa *= 2;
        ya *= 2;
    }
    return fin/div;
}

float mperlin2d(int t, float x, float y, float freq, int depth, int samples) {
    float s = 0.0;
    float div = (1 + samples * 2) + samples * samples * 2;
    //printf("BEGIN: [%lf]\n", div);
    for (int i = -samples; i <= samples; ++i) {
        int s2 = samples - abs(i);
        for (int j = -s2; j <= s2; ++j) {
            //printf("[%d] [%d]\n", i, j);
            s += perlin2d(t, x, y, freq, depth);
        }
    }
    //puts("END");
    return s / div;
}
