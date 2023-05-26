#if defined(MODULE_GAME)

#ifndef GAME_GAME_H
#define GAME_GAME_H

#include <graphics/renderer.h>

#include <stdbool.h>

enum {
    UILAYER_CLIENT,  // For client UI (chat, hotbar, etc.)
    UILAYER_SERVER,  // For server UI (inventory, block-specific menus, etc.)
    UILAYER_DBGINF,  // For debug info
    UILAYER_INGAME,  // For in-game menu (main menu and pressing Esc while in a game)
};

bool doGame();

extern double fps;
extern double realfps;
extern bool showDebugInfo;
extern coord_3d_dbl pcoord;
extern coord_3d pvelocity;
extern int pblockx, pblocky, pblockz;

extern bool debug_wireframe;
extern bool debug_nocavecull;

extern struct ui_layer* game_ui[4];

#endif

#endif
