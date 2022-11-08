#if MODULEID == MODULEID_GAME

#ifndef GAME_GAME_H
#define GAME_GAME_H

#include <renderer/renderer.h>

#include <stdbool.h>

bool doGame(char*, int);

extern int fps;
extern int realfps;
extern bool showDebugInfo;
extern coord_3d_dbl pcoord;
extern coord_3d pvelocity;
extern int64_t pchunkx, pchunky, pchunkz;
extern int pblockx, pblocky, pblockz;

extern struct ui_data* game_ui;

#endif

#endif
