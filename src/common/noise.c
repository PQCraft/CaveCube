/* Derived from https://github.com/caseman/noise */

#include "common.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define F2 0.3660254037844386f
#define G2 0.21132486540518713f
#define F3 (1.0f / 3.0f)
#define G3 (1.0f / 6.0f)
#define ASSIGN(a, v0, v1, v2) (a)[0] = v0; (a)[1] = v1; (a)[2] = v2;
#define DOT3(v1, v2) ((v1)[0] * (v2)[0] + (v1)[1] * (v2)[1] + (v1)[2] * (v2)[2])

float GRAD3[16][3] = {
    { 1, 1, 0}, {-1, 1, 0}, { 1,-1, 0}, {-1,-1, 0}, 
    { 1, 0, 1}, {-1, 0, 1}, { 1, 0,-1}, {-1, 0,-1}, 
    { 0, 1, 1}, { 0,-1, 1}, { 0, 1,-1}, { 0,-1,-1},
    { 1, 0,-1}, {-1, 0,-1}, { 0,-1, 1}, { 0, 1, 1}
};

unsigned char* PERM = NULL;

void initNoiseTable() {
    if (!PERM) PERM = malloc(512);
    for (int i = 0; i < 256; ++i) {
        PERM[i] = getRandByte();
    }
    memcpy(PERM + 256, PERM, 256);
}

float noise2(float x, float y) {
    int i1, j1, I, J, c;
    float s = (x + y) * F2;
    float i = floorf(x + s);
    float j = floorf(y + s);
    float t = (i + j) * G2;
    float xx[3], yy[3], f[3];
    float noise[3] = {0.0f, 0.0f, 0.0f};
    int g[3];
    xx[0] = x - (i - t);
    yy[0] = y - (j - t);
    i1 = xx[0] > yy[0];
    j1 = xx[0] <= yy[0];
    xx[2] = xx[0] + G2 * 2.0f - 1.0f;
    yy[2] = yy[0] + G2 * 2.0f - 1.0f;
    xx[1] = xx[0] - i1 + G2;
    yy[1] = yy[0] - j1 + G2;
    I = (int) i & 255;
    J = (int) j & 255;
    g[0] = PERM[I + PERM[J]] % 12;
    g[1] = PERM[I + i1 + PERM[J + j1]] % 12;
    g[2] = PERM[I + 1 + PERM[J + 1]] % 12;
    for (c = 0; c <= 2; c++) {
        f[c] = 0.5f - xx[c]*xx[c] - yy[c]*yy[c];
    }
    for (c = 0; c <= 2; c++) {
        if (f[c] > 0) {
            noise[c] = f[c] * f[c] * f[c] * f[c] * (GRAD3[g[c]][0] * xx[c] + GRAD3[g[c]][1] * yy[c]);
        }
    }
    return (noise[0] + noise[1] + noise[2]) * 70.0f;
}

float noise3(float x, float y, float z) {
    int c, o1[3], o2[3], g[4], I, J, K;
    float f[4], noise[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float s = (x + y + z) * F3;
    float i = floorf(x + s);
    float j = floorf(y + s);
    float k = floorf(z + s);
    float t = (i + j + k) * G3;
    float pos[4][3];
    pos[0][0] = x - (i - t);
    pos[0][1] = y - (j - t);
    pos[0][2] = z - (k - t);
    if (pos[0][0] >= pos[0][1]) {
        if (pos[0][1] >= pos[0][2]) {
            ASSIGN(o1, 1, 0, 0);
            ASSIGN(o2, 1, 1, 0);
        } else if (pos[0][0] >= pos[0][2]) {
            ASSIGN(o1, 1, 0, 0);
            ASSIGN(o2, 1, 0, 1);
        } else {
            ASSIGN(o1, 0, 0, 1);
            ASSIGN(o2, 1, 0, 1);
        }
    } else {
        if (pos[0][1] < pos[0][2]) {
            ASSIGN(o1, 0, 0, 1);
            ASSIGN(o2, 0, 1, 1);
        } else if (pos[0][0] < pos[0][2]) {
            ASSIGN(o1, 0, 1, 0);
            ASSIGN(o2, 0, 1, 1);
        } else {
            ASSIGN(o1, 0, 1, 0);
            ASSIGN(o2, 1, 1, 0);
        }
    }
    for (c = 0; c <= 2; c++) {
        pos[3][c] = pos[0][c] - 1.0f + 3.0f * G3;
        pos[2][c] = pos[0][c] - o2[c] + 2.0f * G3;
        pos[1][c] = pos[0][c] - o1[c] + G3;
    }
    I = (int) i & 255; 
    J = (int) j & 255; 
    K = (int) k & 255;
    g[0] = PERM[I + PERM[J + PERM[K]]] % 12;
    g[1] = PERM[I + o1[0] + PERM[J + o1[1] + PERM[o1[2] + K]]] % 12;
    g[2] = PERM[I + o2[0] + PERM[J + o2[1] + PERM[o2[2] + K]]] % 12;
    g[3] = PERM[I + 1 + PERM[J + 1 + PERM[K + 1]]] % 12; 
    for (c = 0; c <= 3; c++) {
        f[c] = 0.6f - pos[c][0] * pos[c][0] - pos[c][1] * pos[c][1] - pos[c][2] * pos[c][2];
    }
    for (c = 0; c <= 3; c++) {
        if (f[c] > 0) {
            noise[c] = f[c] * f[c] * f[c] * f[c] * DOT3(pos[c], GRAD3[g[c]]);
        }
    }
    return (noise[0] + noise[1] + noise[2] + noise[3]) * 32.0f;
}

float simplex2(float x, float y, int o, float p, float l) {
    float freq = 1.0f;
    float amp = 1.0f;
    float max = 1.0f;
    float total = noise2(x, y);
    for (int i = 1; i < o; i++) {
        freq *= l;
        amp *= p;
        max += amp;
        total += noise2(x * freq, y * freq) * amp;
    }
    return (1 + total / max) / 2;
}

float simplex3(float x, float y, float z, int o, float p, float l) {
    float freq = 1.0f;
    float amp = 1.0f;
    float max = 1.0f;
    float total = noise3(x, y, z);
    for (int i = 1; i < o; ++i) {
        freq *= l;
        amp *= p;
        max += amp;
        total += noise3(x * freq, y * freq, z * freq) * amp;
    }
    return (1 + total / max) / 2;
}
