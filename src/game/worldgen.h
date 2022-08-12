#ifndef GAME_WORLDGEN_H
#define GAME_WORLDGEN_H

#include <game/chunk.h>

#include <stdbool.h>

bool initWorldgen(void);
void genChunk(int64_t, int64_t, struct blockdata*, int);

#endif
