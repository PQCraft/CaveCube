#ifndef GAME_BLOCKS_H
#define GAME_BLOCKS_H

#include <inttypes.h>
#include <stdbool.h>

struct blockinfo_data {
    char* name;
    char* id;
    int8_t transparency;
    char* texstr[6];
    uint16_t texstart;
    uint16_t texcount;
    uint16_t texoff[6];
    uint16_t anict[6];
    uint8_t anidiv;
    uint8_t light_r:4;
    uint8_t light_g:4;
    uint8_t light_b:4;
};

struct blockinfo {
    char* id;
    struct blockinfo_data data[64];
};

extern struct blockinfo blockinf[256];

void initBlocks(void);
int blockNoFromID(char*);
int blockSubNoFromID(int, char*);

#endif
