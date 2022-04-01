#ifndef NOISE_H
#define NOISE_H

void initNoiseTable(void);
double noise2d(int, double, double);
double perlin2d(int, double, double, double, int);
double mperlin2d(int, double, double, double, int, int);

#endif 
