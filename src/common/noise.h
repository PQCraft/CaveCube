#ifndef NOISE_H
#define NOISE_H

void initNoiseTable(void);
float noise2(float, float);
float noise3(float, float, float);
float simplex2(float, float, int, float, float);
float simplex3(float, float, float, int, float, float);

#endif 
