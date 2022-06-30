#include "blocks.h"
#include <common.h>
#include <resource.h>

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct block_info blockinf[256];

void initBlocks() {
    char buf[32];
    for (int i = 0; i < 256; ++i) {
        sprintf(buf, "game/data/blocks/%d.inf", i);
        if (resourceExists(buf) == -1) break;
        resdata_file* blockcfg = loadResource(RESOURCE_TEXTFILE, buf);
        blockinf[i].name = getConfigVarAlloc((char*)blockcfg->data, "name", "Unknown", 256);
        blockinf[i].id = getConfigVarAlloc((char*)blockcfg->data, "id", "", 256);
        printf("Set block %d to \"%s\" (\"%s\")\n", i, blockinf[i].name, blockinf[i].id);
        freeResource(blockcfg);
    }
}

int blockNoFromID(char* id) {
    for (int i = 0; i < 256; ++i) {
        if (blockinf[i].id && !strcmp(id, blockinf[i].id)) return i;
    }
    return -1;
}
