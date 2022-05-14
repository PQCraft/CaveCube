#ifndef GAME_H
#define GAME_H

#include <renderer.h>

extern int fps;
extern coord_3d pcoord;
extern coord_3d pvelocity;
extern int pchunkx, pchunky, pchunkz;
extern int pblockx, pblocky, pblockz;

void doGame(void);

#endif
