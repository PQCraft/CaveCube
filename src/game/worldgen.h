#ifndef WORLDGEN_H
#define WORLDGEN_H

#include <chunk.h>

bool genChunk(struct chunkinfo*, int, int, int, int64_t, int64_t, struct blockdata*, int);

#endif
