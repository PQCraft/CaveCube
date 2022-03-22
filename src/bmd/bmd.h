#ifndef BMD_H
#define BMD_H

#include <inttypes.h>
#include <stdbool.h>

#include "../common/common.h"

unsigned char* createBMD(uint32_t*, uint32_t, float*, uint32_t, uint32_t*);
bool readBMD(unsigned char*, uint32_t, uint32_t**, uint32_t*, float**, uint32_t*);
bool loadBMD(filedata, uint32_t**, uint32_t*, float**, uint32_t*);
bool temploadBMD(filedata, uint32_t**, uint32_t*, float**, uint32_t*);

#endif
