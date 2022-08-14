#ifndef GAME_BLOCKS_H
#define GAME_BLOCKS_H

#include <inttypes.h>
#include <stdbool.h>

struct block_info {
    char* name;
    char* id;
    int transparency;
    bool singletexoff;
    uint16_t texoff[6];
    uint16_t anict[6];
};

extern struct block_info blockinf[256];

void initBlocks(void);
int blockNoFromID(char*);

#endif
