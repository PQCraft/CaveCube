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
double mperlin2d(int, double, double, double, int, int);

#endif 
