#ifndef NOISE_H
#define NOISE_H

void initNoiseTable(void);
float noise2d(int, float, float);
float perlin2d(int, float, float, float, int);
float mperlin2d(int, float, float, float, int, int);

#endif 
