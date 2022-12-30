#include <main/main.h>
#include "noise.h"
#include "common.h"

#include <math.h>
#include <stdlib.h>

#define NMAGIC(x) (fmod(fabs(x), 256.0) / 128.0)

unsigned char perm[NOISE_TABLES][512];

void initNoiseTable(int s) {
    for (int i = 0; i < NOISE_TABLES; ++i) {
        //if (!hash[i]) hash[i] = malloc(512);
        for (int j = 0; j < 512; ++j) {
            perm[i][j] = getRandByte(s);
        }
    }
}

// Modified version of noise1234 at https://github.com/stegu/perlin-noise

#define FADE(t) (t * t * t * (t * (t * 6 - 15) + 10))

#define FASTFLOOR(x) (((int)(x) < (x)) ? ((int)x) : ((int)x - 1))
#define LERP(t, a, b) ((a) + (t) * ((b) - (a)))

static inline double grad1(int hash, double x) {
    int h = hash & 15;
    double grad = 1.0 + (h & 7);
    if (h & 8) grad = -grad;
    return (grad * x);
}

static inline double grad2(int hash, double x, double y) {
    int h = hash & 7;
    double u = (h < 4) ? x : y;
    double v = (h < 4) ? y : x;
    return ((h & 1) ? -u : u) + ((h&2)? -2.0*v : 2.0*v);
}

static inline double grad3(int hash, double x, double y , double z) {
    int h = hash & 15;
    double u = (h < 8) ? x : y;
    double v = (h < 4) ? y : ((h==12 || h==14) ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

static inline double grad4(int hash, double x, double y, double z, double t) {
    int h = hash & 31;
    double u = (h < 24) ? x : y;
    double v = (h < 16) ? y : z;
    double w = (h < 8) ? z : t;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v) + ((h & 4) ? -w : w);
}

double noise1(int tbl, double x) {
    int ix0, ix1;
    double fx0, fx1;
    double s, n0, n1;

    x += perm[tbl][0] * 0.1984;

    ix0 = FASTFLOOR(x);
    fx0 = x - ix0;
    fx1 = fx0 - 1.0;
    ix1 = (ix0 + 1) & 0xFF;
    ix0 = ix0 & 0xFF;

    s = FADE(fx0);

    n0 = grad1(perm[tbl][ix0], fx0);
    n1 = grad1(perm[tbl][ix1], fx1);

    return 0.188 * (LERP(s, n0, n1));
}

double pnoise1(int tbl, double x, int px) {
    int64_t ix0, ix1;
    double fx0, fx1;
    double s, n0, n1;

    x += perm[tbl][0] * 0.1984;

    ix0 = FASTFLOOR(x);
    fx0 = x - ix0;
    fx1 = fx0 - 1.0;
    ix1 = ((ix0 + 1) % px) & 0xFF;
    ix0 = (ix0 % px) & 0xFF;

    s = FADE(fx0);

    n0 = grad1(perm[tbl][ix0], fx0);
    n1 = grad1(perm[tbl][ix1], fx1);

    return 0.188 * (LERP(s, n0, n1));
}

double noise2(int tbl, double x, double y) {
    int64_t ix0, iy0, ix1, iy1;
    double fx0, fy0, fx1, fy1;
    double s, t, nx0, nx1, n0, n1;

    x += perm[tbl][0] * 0.1984;
    y += perm[tbl][1] * 0.1984;

    ix0 = FASTFLOOR(x);
    iy0 = FASTFLOOR(y);
    fx0 = x - ix0;
    fy0 = y - iy0;
    fx1 = fx0 - 1.0;
    fy1 = fy0 - 1.0;
    ix1 = (ix0 + 1) & 0xFF;
    iy1 = (iy0 + 1) & 0xFF;
    ix0 = ix0 & 0xFF;
    iy0 = iy0 & 0xFF;
    
    t = FADE(fy0);
    s = FADE(fx0);

    nx0 = grad2(perm[tbl][ix0 + perm[tbl][iy0]], fx0, fy0);
    nx1 = grad2(perm[tbl][ix0 + perm[tbl][iy1]], fx0, fy1);
    n0 = LERP(t, nx0, nx1);

    nx0 = grad2(perm[tbl][ix1 + perm[tbl][iy0]], fx1, fy0);
    nx1 = grad2(perm[tbl][ix1 + perm[tbl][iy1]], fx1, fy1);
    n1 = LERP(t, nx0, nx1);

    return 0.507 * (LERP(s, n0, n1));
}

double pnoise2(int tbl, double x, double y, int px, int py) {
    int64_t ix0, iy0, ix1, iy1;
    double fx0, fy0, fx1, fy1;
    double s, t, nx0, nx1, n0, n1;

    x += perm[tbl][0] * 0.1984;
    y += perm[tbl][1] * 0.1984;

    ix0 = FASTFLOOR(x);
    iy0 = FASTFLOOR(y);
    fx0 = x - ix0;
    fy0 = y - iy0;
    fx1 = fx0 - 1.0;
    fy1 = fy0 - 1.0;
    ix1 = ((ix0 + 1) % px) & 0xFF;
    iy1 = ((iy0 + 1) % py) & 0xFF;
    ix0 = (ix0 % px) & 0xFF;
    iy0 = (iy0 % py) & 0xFF;
    
    t = FADE(fy0);
    s = FADE(fx0);

    nx0 = grad2(perm[tbl][ix0 + perm[tbl][iy0]], fx0, fy0);
    nx1 = grad2(perm[tbl][ix0 + perm[tbl][iy1]], fx0, fy1);
    n0 = LERP(t, nx0, nx1);

    nx0 = grad2(perm[tbl][ix1 + perm[tbl][iy0]], fx1, fy0);
    nx1 = grad2(perm[tbl][ix1 + perm[tbl][iy1]], fx1, fy1);
    n1 = LERP(t, nx0, nx1);

    return 0.507 * (LERP(s, n0, n1));
}

double noise3(int tbl, double x, double y, double z) {
    int ix0, iy0, ix1, iy1, iz0, iz1;
    double fx0, fy0, fz0, fx1, fy1, fz1;
    double s, t, r;
    double nxy0, nxy1, nx0, nx1, n0, n1;

    x += perm[tbl][0] * 0.1984;
    y += perm[tbl][1] * 0.1984;
    z += perm[tbl][2] * 0.1984;

    ix0 = FASTFLOOR(x);
    iy0 = FASTFLOOR(y);
    iz0 = FASTFLOOR(z);
    fx0 = x - ix0;
    fy0 = y - iy0;
    fz0 = z - iz0;
    fx1 = fx0 - 1.0;
    fy1 = fy0 - 1.0;
    fz1 = fz0 - 1.0;
    ix1 = (ix0 + 1) & 0xFF;
    iy1 = (iy0 + 1) & 0xFF;
    iz1 = (iz0 + 1) & 0xFF;
    ix0 = ix0 & 0xFF;
    iy0 = iy0 & 0xFF;
    iz0 = iz0 & 0xFF;
    
    r = FADE(fz0);
    t = FADE(fy0);
    s = FADE(fx0);

    nxy0 = grad3(perm[tbl][ix0 + perm[tbl][iy0 + perm[tbl][iz0]]], fx0, fy0, fz0);
    nxy1 = grad3(perm[tbl][ix0 + perm[tbl][iy0 + perm[tbl][iz1]]], fx0, fy0, fz1);
    nx0 = LERP(r, nxy0, nxy1);

    nxy0 = grad3(perm[tbl][ix0 + perm[tbl][iy1 + perm[tbl][iz0]]], fx0, fy1, fz0);
    nxy1 = grad3(perm[tbl][ix0 + perm[tbl][iy1 + perm[tbl][iz1]]], fx0, fy1, fz1);
    nx1 = LERP(r, nxy0, nxy1);

    n0 = LERP(t, nx0, nx1);

    nxy0 = grad3(perm[tbl][ix1 + perm[tbl][iy0 + perm[tbl][iz0]]], fx1, fy0, fz0);
    nxy1 = grad3(perm[tbl][ix1 + perm[tbl][iy0 + perm[tbl][iz1]]], fx1, fy0, fz1);
    nx0 = LERP(r, nxy0, nxy1);

    nxy0 = grad3(perm[tbl][ix1 + perm[tbl][iy1 + perm[tbl][iz0]]], fx1, fy1, fz0);
    nxy1 = grad3(perm[tbl][ix1 + perm[tbl][iy1 + perm[tbl][iz1]]], fx1, fy1, fz1);
    nx1 = LERP(r, nxy0, nxy1);

    n1 = LERP(t, nx0, nx1);
    
    return 0.936 * (LERP(s, n0, n1));
}

double pnoise3(int tbl, double x, double y, double z, int px, int py, int pz) {
    int ix0, iy0, ix1, iy1, iz0, iz1;
    double fx0, fy0, fz0, fx1, fy1, fz1;
    double s, t, r;
    double nxy0, nxy1, nx0, nx1, n0, n1;

    x += perm[tbl][0] * 0.1984;
    y += perm[tbl][1] * 0.1984;
    z += perm[tbl][2] * 0.1984;

    ix0 = FASTFLOOR(x);
    iy0 = FASTFLOOR(y);
    iz0 = FASTFLOOR(z);
    fx0 = x - ix0;
    fy0 = y - iy0;
    fz0 = z - iz0;
    fx1 = fx0 - 1.0;
    fy1 = fy0 - 1.0;
    fz1 = fz0 - 1.0;
    ix1 = ((ix0 + 1) % px) & 0xFF;
    iy1 = ((iy0 + 1) % py) & 0xFF;
    iz1 = ((iz0 + 1) % pz) & 0xFF;
    ix0 = (ix0 % px) & 0xFF;
    iy0 = (iy0 % py) & 0xFF;
    iz0 = (iz0 % pz) & 0xFF;
    
    r = FADE(fz0);
    t = FADE(fy0);
    s = FADE(fx0);

    nxy0 = grad3(perm[tbl][ix0 + perm[tbl][iy0 + perm[tbl][iz0]]], fx0, fy0, fz0);
    nxy1 = grad3(perm[tbl][ix0 + perm[tbl][iy0 + perm[tbl][iz1]]], fx0, fy0, fz1);
    nx0 = LERP(r, nxy0, nxy1);

    nxy0 = grad3(perm[tbl][ix0 + perm[tbl][iy1 + perm[tbl][iz0]]], fx0, fy1, fz0);
    nxy1 = grad3(perm[tbl][ix0 + perm[tbl][iy1 + perm[tbl][iz1]]], fx0, fy1, fz1);
    nx1 = LERP(r, nxy0, nxy1);

    n0 = LERP(t, nx0, nx1);

    nxy0 = grad3(perm[tbl][ix1 + perm[tbl][iy0 + perm[tbl][iz0]]], fx1, fy0, fz0);
    nxy1 = grad3(perm[tbl][ix1 + perm[tbl][iy0 + perm[tbl][iz1]]], fx1, fy0, fz1);
    nx0 = LERP(r, nxy0, nxy1);

    nxy0 = grad3(perm[tbl][ix1 + perm[tbl][iy1 + perm[tbl][iz0]]], fx1, fy1, fz0);
    nxy1 = grad3(perm[tbl][ix1 + perm[tbl][iy1 + perm[tbl][iz1]]], fx1, fy1, fz1);
    nx1 = LERP(r, nxy0, nxy1);

    n1 = LERP(t, nx0, nx1);
    
    return 0.936 * (LERP(s, n0, n1));
}

double noise4(int tbl, double x, double y, double z, double w) {
    int ix0, iy0, iz0, iw0, ix1, iy1, iz1, iw1;
    double fx0, fy0, fz0, fw0, fx1, fy1, fz1, fw1;
    double s, t, r, q;
    double nxyz0, nxyz1, nxy0, nxy1, nx0, nx1, n0, n1;

    x += perm[tbl][0] * 0.1984;
    y += perm[tbl][1] * 0.1984;
    z += perm[tbl][2] * 0.1984;
    w += perm[tbl][3] * 0.1984;

    ix0 = FASTFLOOR(x);
    iy0 = FASTFLOOR(y);
    iz0 = FASTFLOOR(z);
    iw0 = FASTFLOOR(w);
    fx0 = x - ix0;
    fy0 = y - iy0;
    fz0 = z - iz0;
    fw0 = w - iw0;
    fx1 = fx0 - 1.0;
    fy1 = fy0 - 1.0;
    fz1 = fz0 - 1.0;
    fw1 = fw0 - 1.0;
    ix1 = (ix0 + 1) & 0xFF;
    iy1 = (iy0 + 1) & 0xFF;
    iz1 = (iz0 + 1) & 0xFF;
    iw1 = (iw0 + 1) & 0xFF;
    ix0 = ix0 & 0xFF;
    iy0 = iy0 & 0xFF;
    iz0 = iz0 & 0xFF;
    iw0 = iw0 & 0xFF;

    q = FADE(fw0);
    r = FADE(fz0);
    t = FADE(fy0);
    s = FADE(fx0);

    nxyz0 = grad4(perm[tbl][ix0 + perm[tbl][iy0 + perm[tbl][iz0 + perm[tbl][iw0]]]], fx0, fy0, fz0, fw0);
    nxyz1 = grad4(perm[tbl][ix0 + perm[tbl][iy0 + perm[tbl][iz0 + perm[tbl][iw1]]]], fx0, fy0, fz0, fw1);
    nxy0 = LERP(q, nxyz0, nxyz1);
        
    nxyz0 = grad4(perm[tbl][ix0 + perm[tbl][iy0 + perm[tbl][iz1 + perm[tbl][iw0]]]], fx0, fy0, fz1, fw0);
    nxyz1 = grad4(perm[tbl][ix0 + perm[tbl][iy0 + perm[tbl][iz1 + perm[tbl][iw1]]]], fx0, fy0, fz1, fw1);
    nxy1 = LERP(q, nxyz0, nxyz1);
        
    nx0 = LERP (r, nxy0, nxy1);

    nxyz0 = grad4(perm[tbl][ix0 + perm[tbl][iy1 + perm[tbl][iz0 + perm[tbl][iw0]]]], fx0, fy1, fz0, fw0);
    nxyz1 = grad4(perm[tbl][ix0 + perm[tbl][iy1 + perm[tbl][iz0 + perm[tbl][iw1]]]], fx0, fy1, fz0, fw1);
    nxy0 = LERP(q, nxyz0, nxyz1);
        
    nxyz0 = grad4(perm[tbl][ix0 + perm[tbl][iy1 + perm[tbl][iz1 + perm[tbl][iw0]]]], fx0, fy1, fz1, fw0);
    nxyz1 = grad4(perm[tbl][ix0 + perm[tbl][iy1 + perm[tbl][iz1 + perm[tbl][iw1]]]], fx0, fy1, fz1, fw1);
    nxy1 = LERP(q, nxyz0, nxyz1);

    nx1 = LERP (r, nxy0, nxy1);

    n0 = LERP(t, nx0, nx1);

    nxyz0 = grad4(perm[tbl][ix1 + perm[tbl][iy0 + perm[tbl][iz0 + perm[tbl][iw0]]]], fx1, fy0, fz0, fw0);
    nxyz1 = grad4(perm[tbl][ix1 + perm[tbl][iy0 + perm[tbl][iz0 + perm[tbl][iw1]]]], fx1, fy0, fz0, fw1);
    nxy0 = LERP(q, nxyz0, nxyz1);
        
    nxyz0 = grad4(perm[tbl][ix1 + perm[tbl][iy0 + perm[tbl][iz1 + perm[tbl][iw0]]]], fx1, fy0, fz1, fw0);
    nxyz1 = grad4(perm[tbl][ix1 + perm[tbl][iy0 + perm[tbl][iz1 + perm[tbl][iw1]]]], fx1, fy0, fz1, fw1);
    nxy1 = LERP(q, nxyz0, nxyz1);

    nx0 = LERP (r, nxy0, nxy1);

    nxyz0 = grad4(perm[tbl][ix1 + perm[tbl][iy1 + perm[tbl][iz0 + perm[tbl][iw0]]]], fx1, fy1, fz0, fw0);
    nxyz1 = grad4(perm[tbl][ix1 + perm[tbl][iy1 + perm[tbl][iz0 + perm[tbl][iw1]]]], fx1, fy1, fz0, fw1);
    nxy0 = LERP(q, nxyz0, nxyz1);
        
    nxyz0 = grad4(perm[tbl][ix1 + perm[tbl][iy1 + perm[tbl][iz1 + perm[tbl][iw0]]]], fx1, fy1, fz1, fw0);
    nxyz1 = grad4(perm[tbl][ix1 + perm[tbl][iy1 + perm[tbl][iz1 + perm[tbl][iw1]]]], fx1, fy1, fz1, fw1);
    nxy1 = LERP(q, nxyz0, nxyz1);

    nx1 = LERP (r, nxy0, nxy1);

    n1 = LERP(t, nx0, nx1);

    return 0.87 * (LERP(s, n0, n1));
}

double pnoise4(int tbl, double x, double y, double z, double w, int px, int py, int pz, int pw) {
    int ix0, iy0, iz0, iw0, ix1, iy1, iz1, iw1;
    double fx0, fy0, fz0, fw0, fx1, fy1, fz1, fw1;
    double s, t, r, q;
    double nxyz0, nxyz1, nxy0, nxy1, nx0, nx1, n0, n1;

    x += perm[tbl][0] * 0.1984;
    y += perm[tbl][1] * 0.1984;
    z += perm[tbl][2] * 0.1984;
    w += perm[tbl][3] * 0.1984;

    ix0 = FASTFLOOR(x);
    iy0 = FASTFLOOR(y);
    iz0 = FASTFLOOR(z);
    iw0 = FASTFLOOR(w);
    fx0 = x - ix0;
    fy0 = y - iy0;
    fz0 = z - iz0;
    fw0 = w - iw0;
    fx1 = fx0 - 1.0;
    fy1 = fy0 - 1.0;
    fz1 = fz0 - 1.0;
    fw1 = fw0 - 1.0;
    ix1 = ((ix0 + 1) % px) & 0xFF;
    iy1 = ((iy0 + 1) % py) & 0xFF;
    iz1 = ((iz0 + 1) % pz) & 0xFF;
    iw1 = ((iw0 + 1) % pw) & 0xFF;
    ix0 = (ix0 % px) & 0xFF;
    iy0 = (iy0 % py) & 0xFF;
    iz0 = (iz0 % pz) & 0xFF;
    iw0 = (iw0 % pw) & 0xFF;

    q = FADE(fw0);
    r = FADE(fz0);
    t = FADE(fy0);
    s = FADE(fx0);

    nxyz0 = grad4(perm[tbl][ix0 + perm[tbl][iy0 + perm[tbl][iz0 + perm[tbl][iw0]]]], fx0, fy0, fz0, fw0);
    nxyz1 = grad4(perm[tbl][ix0 + perm[tbl][iy0 + perm[tbl][iz0 + perm[tbl][iw1]]]], fx0, fy0, fz0, fw1);
    nxy0 = LERP(q, nxyz0, nxyz1);
        
    nxyz0 = grad4(perm[tbl][ix0 + perm[tbl][iy0 + perm[tbl][iz1 + perm[tbl][iw0]]]], fx0, fy0, fz1, fw0);
    nxyz1 = grad4(perm[tbl][ix0 + perm[tbl][iy0 + perm[tbl][iz1 + perm[tbl][iw1]]]], fx0, fy0, fz1, fw1);
    nxy1 = LERP(q, nxyz0, nxyz1);
        
    nx0 = LERP (r, nxy0, nxy1);

    nxyz0 = grad4(perm[tbl][ix0 + perm[tbl][iy1 + perm[tbl][iz0 + perm[tbl][iw0]]]], fx0, fy1, fz0, fw0);
    nxyz1 = grad4(perm[tbl][ix0 + perm[tbl][iy1 + perm[tbl][iz0 + perm[tbl][iw1]]]], fx0, fy1, fz0, fw1);
    nxy0 = LERP(q, nxyz0, nxyz1);
        
    nxyz0 = grad4(perm[tbl][ix0 + perm[tbl][iy1 + perm[tbl][iz1 + perm[tbl][iw0]]]], fx0, fy1, fz1, fw0);
    nxyz1 = grad4(perm[tbl][ix0 + perm[tbl][iy1 + perm[tbl][iz1 + perm[tbl][iw1]]]], fx0, fy1, fz1, fw1);
    nxy1 = LERP(q, nxyz0, nxyz1);

    nx1 = LERP (r, nxy0, nxy1);

    n0 = LERP(t, nx0, nx1);

    nxyz0 = grad4(perm[tbl][ix1 + perm[tbl][iy0 + perm[tbl][iz0 + perm[tbl][iw0]]]], fx1, fy0, fz0, fw0);
    nxyz1 = grad4(perm[tbl][ix1 + perm[tbl][iy0 + perm[tbl][iz0 + perm[tbl][iw1]]]], fx1, fy0, fz0, fw1);
    nxy0 = LERP(q, nxyz0, nxyz1);
        
    nxyz0 = grad4(perm[tbl][ix1 + perm[tbl][iy0 + perm[tbl][iz1 + perm[tbl][iw0]]]], fx1, fy0, fz1, fw0);
    nxyz1 = grad4(perm[tbl][ix1 + perm[tbl][iy0 + perm[tbl][iz1 + perm[tbl][iw1]]]], fx1, fy0, fz1, fw1);
    nxy1 = LERP(q, nxyz0, nxyz1);

    nx0 = LERP (r, nxy0, nxy1);

    nxyz0 = grad4(perm[tbl][ix1 + perm[tbl][iy1 + perm[tbl][iz0 + perm[tbl][iw0]]]], fx1, fy1, fz0, fw0);
    nxyz1 = grad4(perm[tbl][ix1 + perm[tbl][iy1 + perm[tbl][iz0 + perm[tbl][iw1]]]], fx1, fy1, fz0, fw1);
    nxy0 = LERP(q, nxyz0, nxyz1);
        
    nxyz0 = grad4(perm[tbl][ix1 + perm[tbl][iy1 + perm[tbl][iz1 + perm[tbl][iw0]]]], fx1, fy1, fz1, fw0);
    nxyz1 = grad4(perm[tbl][ix1 + perm[tbl][iy1 + perm[tbl][iz1 + perm[tbl][iw1]]]], fx1, fy1, fz1, fw1);
    nxy1 = LERP(q, nxyz0, nxyz1);

    nx1 = LERP (r, nxy0, nxy1);

    n1 = LERP(t, nx0, nx1);

    return 0.87 * (LERP(s, n0, n1));
}
