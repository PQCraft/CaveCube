#if defined(MODULE_SERVER)

#ifndef SERVER_WORLDGEN_H
#define SERVER_WORLDGEN_H

#include <common/chunk.h>

#include <stdbool.h>

bool initWorldgen(void);
void genChunk(int64_t, int64_t, struct blockdata*, int);

#endif

#endif
