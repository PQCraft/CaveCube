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
    uint8_t anict[6];
    uint8_t anidiv;
    float light_r;
    float light_g;
    float light_b;
    bool backfaces;
};

struct blockinfo {
    char* id;
    struct blockinfo_data data[64];
};

extern struct blockinfo blockinf[256];

void initBlocks(void);
int blockNoFromID(char*);
int blockSubNoFromID(int, char*);

#define BLOCKNO_NULL (0)
#define BLOCKNO_BORDER (255)

#endif
