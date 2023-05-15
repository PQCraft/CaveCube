#ifndef COMMON_NOISE_H
#define COMMON_NOISE_H

#ifndef NOISE_TABLES
    #define NOISE_TABLES 64
#endif

#include <inttypes.h>

#define noiseint int64_t
#define noisefloat double
#define perm_t int
#define noise_t float

void initNoiseTable(int);
noise_t noise2d(int, noisefloat, noisefloat);
noise_t nnoise2d(int, noisefloat, noisefloat);
noise_t perlin2d(int, noisefloat, noisefloat, noisefloat, int);
noise_t nperlin2d(int, noisefloat, noisefloat, noisefloat, int);
noise_t noise2(int, noisefloat, noisefloat);
noise_t noise3(int, noisefloat, noisefloat, noisefloat);

#endif
