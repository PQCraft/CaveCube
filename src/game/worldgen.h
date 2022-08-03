#ifndef GAME_WORLDGEN_H
#define GAME_WORLDGEN_H

#include <game/chunk.h>

#include <stdbool.h>

bool genChunk(struct chunkinfo*, int, int, int, int64_t, int64_t, struct blockdata*, int);

#endif
