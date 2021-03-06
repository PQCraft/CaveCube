#ifndef SERVER

#ifndef GAME_H
#define GAME_H

#include <renderer/renderer.h>

extern int fps;
extern coord_3d_dbl pcoord;
extern coord_3d pvelocity;
extern int64_t pchunkx, pchunky, pchunkz;
extern int pblockx, pblocky, pblockz;

bool doGame(char*, int);

#endif

#endif
