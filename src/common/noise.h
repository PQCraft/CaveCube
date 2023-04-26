#ifndef COMMON_NOISE_H
#define COMMON_NOISE_H

#ifndef NOISE_TABLES
    #define NOISE_TABLES 64
#endif

#include <inttypes.h>

#define noiseint int64_t
#define noisefloat double

void initNoiseTable(int);
float noise2d(int, noisefloat, noisefloat);
float nnoise2d(int, noisefloat, noisefloat);
float perlin2d(int, noisefloat, noisefloat, noisefloat, int);
float nperlin2d(int, noisefloat, noisefloat, noisefloat, int);
float noise1(int, noisefloat);
float pnoise1(int, noisefloat, noiseint);
float noise2(int, noisefloat, noisefloat);
float pnoise2(int, noisefloat, noisefloat, noiseint, noiseint);
float noise3(int, noisefloat, noisefloat, noisefloat);
float pnoise3(int, noisefloat, noisefloat, noisefloat, noiseint, noiseint, noiseint);
float noise4(int, noisefloat, noisefloat, noisefloat, noisefloat);
float pnoise4(int, noisefloat, noisefloat, noisefloat, noisefloat, noiseint, noiseint, noiseint, noiseint);

#endif
