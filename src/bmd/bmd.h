#ifndef BMD_H
#define BMD_H

#include <common/common.h>

#include <inttypes.h>
#include <stdbool.h>

typedef struct {
    uint32_t isize;
    uint32_t* indices;
    uint32_t vsize;
    float* vertices;
} bmd_part;

typedef struct {
    uint32_t parts;
    bmd_part* part;
} bmd_data;

unsigned char* createBMD(bmd_data*, uint32_t*);
bool readBMD(unsigned char*, uint32_t, bmd_data*);
bool loadBMD(file_data, bmd_data*);
bool temploadBMD(file_data, bmd_data*);
void freeBMD(bmd_data*);

#endif
