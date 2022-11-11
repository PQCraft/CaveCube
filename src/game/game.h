#if MODULEID == MODULEID_GAME

#ifndef GAME_GAME_H
#define GAME_GAME_H

#include <renderer/renderer.h>

#include <stdbool.h>

enum {
    UILAYER_SERVER,  // For server UI (inventory, block-specific menus, etc.)
    UILAYER_CLIENT,  // For client UI (chat, hotbar, etc.)
    UILAYER_DBGINF,  // For debug info
    UILAYER_INGAME,  // For in-game menu (pressing Esc while in a game)
};

bool doGame(char*, int);

extern int fps;
extern int realfps;
extern bool showDebugInfo;
extern coord_3d_dbl pcoord;
extern coord_3d pvelocity;
extern int64_t pchunkx, pchunky, pchunkz;
extern int pblockx, pblocky, pblockz;

extern struct ui_data* game_ui[4];

#endif

#endif
