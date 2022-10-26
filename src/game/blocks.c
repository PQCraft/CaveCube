#include <main/main.h>
#include "blocks.h"
#include <common/common.h>
#include <common/resource.h>

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct blockinfo blockinf[256];

void initBlocks() {
    char buf[32];
    for (int i = 0; i < 256; ++i) {
        blockinf[i].id = NULL;
        sprintf(buf, "game/data/blocks/%d/block.inf", i);
        if (resourceExists(buf) == -1) continue;
        resdata_file* blockcfg = loadResource(RESOURCE_TEXTFILE, buf);
        blockinf[i].id = getInfoVarAlloc((char*)blockcfg->data, "id", "", 256);
        freeResource(blockcfg);
        #if DBGLVL(1)
        printf("Block #%d: id \"%s\"\n", i, blockinf[i].id);
        #endif
        for (int j = 0; j < 64; ++j) {
            blockinf[i].data[j].id = NULL;
            sprintf(buf, "game/data/blocks/%d/%d.inf", i, j);
            if (resourceExists(buf) == -1) continue;
            resdata_file* varcfg = loadResource(RESOURCE_TEXTFILE, buf);
            blockinf[i].data[j].name = getInfoVarAlloc((char*)varcfg->data, "name", "Unknown", 256);
            blockinf[i].data[j].id = getInfoVarAlloc((char*)varcfg->data, "id", "", 256);
            int anict = atoi(getInfoVarStatic((char*)varcfg->data, "animation", "1", 16));
            blockinf[i].data[j].singletexoff = getBool(getInfoVarStatic((char*)varcfg->data, "singletexoff", "0", 16));
            if (anict < 1) {
                blockinf[i].data[j].anict[0] = atoi(getInfoVarStatic((char*)varcfg->data, "animation0", "0", 16));
                blockinf[i].data[j].anict[1] = atoi(getInfoVarStatic((char*)varcfg->data, "animation1", "0", 16));
                blockinf[i].data[j].anict[2] = atoi(getInfoVarStatic((char*)varcfg->data, "animation2", "0", 16));
                blockinf[i].data[j].anict[3] = atoi(getInfoVarStatic((char*)varcfg->data, "animation3", "0", 16));
                blockinf[i].data[j].anict[4] = atoi(getInfoVarStatic((char*)varcfg->data, "animation4", "0", 16));
                blockinf[i].data[j].anict[5] = atoi(getInfoVarStatic((char*)varcfg->data, "animation5", "0", 16));
            } else {
                for (int k = 0; k < 6; ++k) {
                    blockinf[i].data[j].anict[k] = anict;
                }
            }
            blockinf[i].data[j].anidiv = atoi(getInfoVarStatic((char*)varcfg->data, "animationdiv", "1", 16));
            blockinf[i].data[j].light_r = atoi(getInfoVarStatic((char*)varcfg->data, "light_r", "0", 16));
            blockinf[i].data[j].light_g = atoi(getInfoVarStatic((char*)varcfg->data, "light_g", "0", 16));
            blockinf[i].data[j].light_b = atoi(getInfoVarStatic((char*)varcfg->data, "light_b", "0", 16));
            freeResource(varcfg);
            #if DBGLVL(1)
            printf("  Variant #%d: id \"%s\", name \"%s\"\n", j, blockinf[i].data[j].id, blockinf[i].data[j].name);
            #endif
        }
    }
}

int blockNoFromID(char* id) {
    for (int i = 0; i < 256; ++i) {
        if (blockinf[i].id && !strcasecmp(id, blockinf[i].id)) return i;
    }
    return -1;
}

int blockSubNoFromID(int block, char* id) {
    for (int i = 0; i < 64; ++i) {
        if (blockinf[block].data[i].id && !strcasecmp(id, blockinf[block].data[i].id)) return i;
    }
    return -1;
}
