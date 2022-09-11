#ifndef SERVER

#ifndef GAME_GAME_H
#define GAME_GAME_H

#include <renderer/renderer.h>

#include <stdbool.h>

extern int fps;
extern int realfps;
extern bool showDebugInfo;
extern coord_3d_dbl pcoord;
extern coord_3d pvelocity;
extern int64_t pchunkx, pchunky, pchunkz;
extern int pblockx, pblocky, pblockz;

bool doGame(char*, int);

#endif

#endif
