#ifndef GAME_BLOCKS_H
#define GAME_BLOCKS_H

#include <inttypes.h>

struct block_info {
    char* name;
    char* id;
    int transparency;
};

extern struct block_info blockinf[256];

void initBlocks(void);
int blockNoFromID(char*);

#endif
