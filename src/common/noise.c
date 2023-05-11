#include <main/main.h>
#include "noise.h"
#include "common.h"

#include <math.h>
#include <stdlib.h>

perm_t perm[NOISE_TABLES][512];

void initNoiseTable(int s) {
    for (int i = 0; i < NOISE_TABLES; ++i) {
        //if (!hash[i]) hash[i] = malloc(512);
        //printf("TABLE: [%d]\n", i);
        for (int j = 0; j < 512; ++j) {
            perm[i][j] = getRandByte(s);
            //printf("%02X ", perm[i][j]);
            //if (j % 32 == 31) putchar('\n');
        }
        /*
        for (int j = 0; j < 511; ++j) {
            if (perm[i][j] == perm[i][j + 1]) {
                printf("FIX: [%d][%d]\n", i, j);
            }
        }
        */
    }
    for (int i = 0; i < NOISE_TABLES; ++i) {
        for (int j = 0; j < 511; ++j) {
            if (perm[i][j] == perm[i][j + 1]) {
                perm[i][j] += (getRandByte(s) % 7) + 3;
            }
        }
    }
}

#define SEED (1984)

static force_inline int _noise2d(int tbl, noiseint x, noiseint y) {
    noiseint yindex = (y + SEED) % 256;
    if (yindex < 0) yindex += 256;
    noiseint xindex = (perm[tbl][yindex] + x) % 256;
    if (xindex < 0) xindex += 256;
    return perm[tbl][xindex];
}

/*
static force_inline noisefloat lin_inter(noisefloat x, noisefloat y, noisefloat s) {
    return x + s * (y - x);
}
*/
#define lin_inter(x, y, s) ((x) + (s) * ((y) - (x)))

/*
static force_inline noisefloat smooth_inter(noisefloat x, noisefloat y, noisefloat s) {
    return lin_inter(x, y, s * s * (3 - 2 * s));
}
*/
#define smooth_inter(x, y, s) (lin_inter((x), (y), (s) * (s) * (3.0 - 2.0 * (s))))

noise_t noise2d(int tbl, noisefloat x, noisefloat y) {
    const noiseint x_int = floor(x);
    const noiseint y_int = floor(y);
    const noisefloat x_frac = x - x_int;
    const noisefloat y_frac = y - y_int;
    const noiseint s = _noise2d(tbl, x_int, y_int);
    const noiseint t = _noise2d(tbl, x_int + 1, y_int);
    const noiseint u = _noise2d(tbl, x_int, y_int + 1);
    const noiseint v = _noise2d(tbl, x_int + 1, y_int + 1);
    const noisefloat low = smooth_inter(s, t, x_frac);
    const noisefloat high = smooth_inter(u, v, x_frac);
    const noisefloat result = smooth_inter(low, high, y_frac);
    return result;
}

noise_t nnoise2d(int tbl, noisefloat x, noisefloat y) {
    const noiseint x_int = floor(x);
    const noiseint y_int = floor(y);
    const noisefloat x_frac = x - x_int;
    const noisefloat y_frac = y - y_int;
    const noiseint s = _noise2d(tbl, x_int, y_int);
    const noiseint t = _noise2d(tbl, x_int + 1, y_int);
    const noiseint u = _noise2d(tbl, x_int, y_int + 1);
    const noiseint v = _noise2d(tbl, x_int + 1, y_int + 1);
    const noisefloat low = smooth_inter(s, t, x_frac);
    const noisefloat high = smooth_inter(u, v, x_frac);
    const noisefloat result = smooth_inter(low, high, y_frac);
    return result * 2 - 1;
}

noise_t perlin2d(int t, noisefloat x, noisefloat y, noisefloat freq, int depth) {
    noisefloat xa = x * freq;
    noisefloat ya = y * freq;
    noisefloat amp = 1.0;
    noisefloat fin = 0;
    noisefloat div = 0.0;
    for (int i = 0; i < depth; i++) {
        div += 256 * amp;
        fin += noise2d(t, xa, ya) * amp;
        amp /= 2.0;
        xa += xa;
        ya += ya;
    }
    return fin / div;
}

noise_t nperlin2d(int t, noisefloat x, noisefloat y, noisefloat freq, int depth) {
    return perlin2d(t, x, y, freq, depth) * 2.0 - 1.0;
}

// Modified version of noise1234 at https://github.com/stegu/perlin-noise

#define FADE(t) (t * t * t * (t * (t * 6 - 15) + 10))

#define FASTFLOOR(x) (((noiseint)(x) < (x)) ? ((noiseint)x) : ((noiseint)x - 1))
#define LERP(t, a, b) ((a) + (t) * ((b) - (a)))

static force_inline noise_t grad2(int hash, noisefloat x, noisefloat y) {
    int h = hash & 7;
    noisefloat u = (h < 4) ? x : y;
    noisefloat v = (h < 4) ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0 * v : 2.0 * v);
}

static force_inline noise_t grad3(int hash, noisefloat x, noisefloat y , noisefloat z) {
    int h = hash & 15;
    noisefloat u = (h < 8) ? x : y;
    noisefloat v = (h < 4) ? y : ((h == 12 || h == 14) ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

noise_t noise2(int tbl, noisefloat x, noisefloat y) {
    perm_t* _perm = perm[tbl];

    noiseint ix0, iy0, ix1, iy1;
    noisefloat fx0, fy0, fx1, fy1;
    noisefloat s, t, nx0, nx1, n0, n1;

    //x += _perm[0] * 0.003906;
    //y += _perm[1] * 0.003906;

    ix0 = FASTFLOOR(x);
    iy0 = FASTFLOOR(y);
    fx0 = x - ix0;
    fy0 = y - iy0;
    fx1 = fx0 - 1.0;
    fy1 = fy0 - 1.0;
    ix1 = (ix0 + 1) & 0x1FF;
    iy1 = (iy0 + 1) & 0x1FF;
    ix0 = ix0 & 0x1FF;
    iy0 = iy0 & 0x1FF;
    
    t = FADE(fy0);
    s = FADE(fx0);

    nx0 = grad2(_perm[ix0 + _perm[iy0]], fx0, fy0);
    nx1 = grad2(_perm[ix0 + _perm[iy1]], fx0, fy1);
    n0 = LERP(t, nx0, nx1);

    nx0 = grad2(_perm[ix1 + _perm[iy0]], fx1, fy0);
    nx1 = grad2(_perm[ix1 + _perm[iy1]], fx1, fy1);
    n1 = LERP(t, nx0, nx1);

    return 0.507 * (LERP(s, n0, n1));
}

noise_t noise3(int tbl, noisefloat x, noisefloat y, noisefloat z) {
    perm_t* _perm = perm[tbl];

    noiseint ix0, iy0, ix1, iy1, iz0, iz1;
    noisefloat fx0, fy0, fz0, fx1, fy1, fz1;
    noisefloat s, t, r;
    noisefloat nxy0, nxy1, nx0, nx1, n0, n1;

    //x += _perm[0] * 0.003906;
    //y += _perm[1] * 0.003906;
    //z += _perm[2] * 0.003906;

    ix0 = FASTFLOOR(x);
    iy0 = FASTFLOOR(y);
    iz0 = FASTFLOOR(z);
    fx0 = x - ix0;
    fy0 = y - iy0;
    fz0 = z - iz0;
    fx1 = fx0 - 1.0;
    fy1 = fy0 - 1.0;
    fz1 = fz0 - 1.0;
    ix1 = (ix0 + 1) & 0x1FF;
    iy1 = (iy0 + 1) & 0x1FF;
    iz1 = (iz0 + 1) & 0x1FF;
    ix0 = ix0 & 0x1FF;
    iy0 = iy0 & 0x1FF;
    iz0 = iz0 & 0x1FF;
    
    r = FADE(fz0);
    t = FADE(fy0);
    s = FADE(fx0);

    nxy0 = grad3(_perm[ix0 + _perm[iy0 + _perm[iz0]]], fx0, fy0, fz0);
    nxy1 = grad3(_perm[ix0 + _perm[iy0 + _perm[iz1]]], fx0, fy0, fz1);
    nx0 = LERP(r, nxy0, nxy1);

    nxy0 = grad3(_perm[ix0 + _perm[iy1 + _perm[iz0]]], fx0, fy1, fz0);
    nxy1 = grad3(_perm[ix0 + _perm[iy1 + _perm[iz1]]], fx0, fy1, fz1);
    nx1 = LERP(r, nxy0, nxy1);

    n0 = LERP(t, nx0, nx1);

    nxy0 = grad3(_perm[ix1 + _perm[iy0 + _perm[iz0]]], fx1, fy0, fz0);
    nxy1 = grad3(_perm[ix1 + _perm[iy0 + _perm[iz1]]], fx1, fy0, fz1);
    nx0 = LERP(r, nxy0, nxy1);

    nxy0 = grad3(_perm[ix1 + _perm[iy1 + _perm[iz0]]], fx1, fy1, fz0);
    nxy1 = grad3(_perm[ix1 + _perm[iy1 + _perm[iz1]]], fx1, fy1, fz1);
    nx1 = LERP(r, nxy0, nxy1);

    n1 = LERP(t, nx0, nx1);
    
    return 0.936 * (LERP(s, n0, n1));
}
