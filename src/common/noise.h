#ifndef COMMON_NOISE_H
#define COMMON_NOISE_H

#ifndef NOISE_TABLES
    #define NOISE_TABLES 64
#endif

void initNoiseTable(int);
double noise2d(int, double, double);
double nnoise2d(int, double, double);
double perlin2d(int, double, double, double, int);
double nperlin2d(int, double, double, double, int);
double noise1(int, double);
double pnoise1(int, double, int);
double noise2(int, double, double);
double pnoise2(int, double, double, int, int);
double noise3(int, double, double, double);
double pnoise3(int, double, double, double, int, int, int);
double noise4(int, double, double, double, double);
double pnoise4(int, double, double, double, double, int, int, int, int);

#endif
