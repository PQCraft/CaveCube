#include "blocks.h"
#include <common/common.h>
#include <common/resource.h>

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct block_info blockinf[256];

void initBlocks() {
    char buf[32];
    for (int i = 0; i < 256; ++i) {
        blockinf[i].id = NULL;
        sprintf(buf, "game/data/blocks/%d.inf", i);
        if (resourceExists(buf) == -1) continue;
        resdata_file* blockcfg = loadResource(RESOURCE_TEXTFILE, buf);
        blockinf[i].name = getConfigVarAlloc((char*)blockcfg->data, "name", "Unknown", 256);
        blockinf[i].id = getConfigVarAlloc((char*)blockcfg->data, "id", "", 256);
        int anict = atoi(getConfigVarStatic((char*)blockcfg->data, "animation", "0", 16));
        blockinf[i].singletexoff = getConfigValBool(getConfigVarStatic((char*)blockcfg->data, "singletexoff", "0", 16));
        if (anict < 1) {
            blockinf[i].anict[0] = atoi(getConfigVarStatic((char*)blockcfg->data, "animation0", "0", 16));
            blockinf[i].anict[1] = atoi(getConfigVarStatic((char*)blockcfg->data, "animation1", "0", 16));
            blockinf[i].anict[2] = atoi(getConfigVarStatic((char*)blockcfg->data, "animation2", "0", 16));
            blockinf[i].anict[3] = atoi(getConfigVarStatic((char*)blockcfg->data, "animation3", "0", 16));
            blockinf[i].anict[4] = atoi(getConfigVarStatic((char*)blockcfg->data, "animation4", "0", 16));
            blockinf[i].anict[5] = atoi(getConfigVarStatic((char*)blockcfg->data, "animation5", "0", 16));
        } else {
            for (int j = 0; j < 6; ++j) {
                blockinf[i].anict[j] = anict;
            }
        }
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
