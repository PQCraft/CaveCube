#include "main.h"
#include "game.h"
#include <common.h>
#include <resource.h>
#include <renderer.h>
#include <input.h>
#include <chunk.h>
#include <server.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>

#ifndef SHOWFPS
    //#define SHOWFPS
#endif

static float posmult = 0.125;
static float rotmult = 3;
static float fpsmult = 0;
static float mousesns = 0.05;

struct chunkdata chunks;

void doGame() {
    char** tmpbuf = malloc(16 * sizeof(char*));
    for (int i = 0; i < 16; ++i) {
        tmpbuf[i] = malloc(4096);
    }
    chunks = allocChunks(atoi(getConfigVarStatic(config, "game.chunks", "9", 64)));
    rendinf.campos.y = 67.5;
    initInput();
    float pmult = posmult;
    float rmult = rotmult;
    initServer(SERVER_MODE_SP);
    int cx = 0;
    int cz = 0;
    genChunks(&chunks, cx, cz);
    uint64_t fpsstarttime2 = altutime();
    uint64_t ptime = fpsstarttime2;
    uint64_t dtime = fpsstarttime2;
    uint64_t ptime2 = fpsstarttime2;
    uint64_t dtime2 = fpsstarttime2;
    uint64_t rendtime = 750000;
    uint64_t fpsstarttime = fpsstarttime2;
    int fpsct = 0;
    //setSkyColor(0.0, 0.7, 0.9);
    setSpace(SPACE_NORMAL);
    while (!quitRequest) {
        uint64_t starttime = altutime();
        float npmult = 1.0;
        float nrmult = 1.0;
        input_info input = getInput();
        if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_CROUCH)) {
            rendinf.campos.y -= pmult;
            if (rendinf.campos.y < 1.125) rendinf.campos.y = 1.125;
        } else {
            if (rendinf.campos.y < 1.5) rendinf.campos.y = 1.5;
        }
        if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_JUMP)) {
            rendinf.campos.y += pmult;
        }
        if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_RUN)) {
            npmult *= 2.5;
        }
        rendinf.camrot.x += input.mymov * mousesns * ((input.mmovti) ? rotmult : rmult) * nrmult;
        rendinf.camrot.y -= input.mxmov * mousesns * ((input.mmovti) ? rotmult : rmult) * nrmult;
        if (rendinf.camrot.y < 0) rendinf.camrot.y += 360;
        else if (rendinf.camrot.y >= 360) rendinf.camrot.y -= 360;
        if (rendinf.camrot.x > 89.99) rendinf.camrot.x = 89.99;
        if (rendinf.camrot.x < -89.99) rendinf.camrot.x = -89.99;
        float yrotrad = (rendinf.camrot.y / 180 * M_PI);
        float div = fabs(input.zmov) + fabs(input.xmov);
        if (div < 1.0) div = 1.0;
        rendinf.campos.z -= (input.zmov * cosf(yrotrad) * ((input.movti) ? posmult : pmult) * npmult) / div;
        rendinf.campos.x += (input.zmov * sinf(yrotrad) * ((input.movti) ? posmult : pmult) * npmult) / div;
        rendinf.campos.x += (input.xmov * cosf(yrotrad) * ((input.movti) ? posmult : pmult) * npmult) / div;
        rendinf.campos.z += (input.xmov * sinf(yrotrad) * ((input.movti) ? posmult : pmult) * npmult) / div;
        if (rendinf.campos.z >= 8.0) {   
            --cz;
            rendinf.campos.z -= 16.0;
            moveChunks(&chunks, 0, 1);
            genChunks(&chunks, cx, cz);
        } else if (rendinf.campos.z <= -8.0) {
            ++cz;
            rendinf.campos.z += 16.0;
            moveChunks(&chunks, 0, -1);
            genChunks(&chunks, cx, cz);
        } else if (rendinf.campos.x >= 8.0) {
            ++cx;
            rendinf.campos.x -= 16.0;
            moveChunks(&chunks, 1, 0);
            genChunks(&chunks, cx, cz);
        } else if (rendinf.campos.x <= -8.0) {
            --cx;
            rendinf.campos.x += 16.0;
            moveChunks(&chunks, -1, 0);
            genChunks(&chunks, cx, cz);
        }
        //genChunks(&chunks, cx, cz);
        updateChunks(&chunks);
        updateCam();
        float blockx2 = 0, blocky2 = 0, blockz2 = 0;
        blockx2 = rendinf.campos.x;
        blockx2 -= (blockx2 < 0) ? 1.0 : 0.0;
        blocky2 = rendinf.campos.y;
        blocky2 -= (blocky2 < 0) ? 1.0 : 0.0;
        blockz2 = rendinf.campos.z;
        blockz2 += (blockz2 > 0) ? 1.0 : 0.0;
        struct blockdata curbdata = getBlock(&chunks, 0, 0, 0, blockx2, blocky2, blockz2);
        float accuracy = 0.1;
        float lookatx = cos(rendinf.camrot.x * M_PI / 180.0) * sin(rendinf.camrot.y * M_PI / 180.0);
        float lookaty = sin(rendinf.camrot.x * M_PI / 180.0);
        float lookatz = (cos(rendinf.camrot.x * M_PI / 180.0) * cos(rendinf.camrot.y * M_PI / 180.0)) * -1;
        float blockx = 0, blocky = 0, blockz = 0;
        float lastblockx = 0, lastblocky = 0, lastblockz = 0;
        uint8_t blockid = 0, blockid2 = 0;
        int dist = (float)(7.0 / accuracy);
        for (int i = 1; i < dist; ++i) {
            blockid2 = blockid;
            lastblockx = blockx;
            lastblocky = blocky;
            lastblockz = blockz;
            float depth = (float)i * accuracy;
            blockx = lookatx * depth + rendinf.campos.x;
            blockx -= (blockx < 0) ? 1.0 : 0.0;
            blocky = lookaty * depth + rendinf.campos.y;
            blocky -= (blocky < 0) ? 1.0 : 0.0;
            blockz = lookatz * depth + rendinf.campos.z;
            blockz += (blockz > 0) ? 1.0 : 0.0;
            struct blockdata bdata = getBlock(&chunks, 0, 0, 0, blockx, blocky, blockz);
            blockid = bdata.id;
            if (bdata.id && bdata.id != 7) break;
        }
        /*
        printf("info:\n[camx:%f][camy:%f][camz:%f]\n[x:%f][y:%f][z:%f]\n[%f][%f][%f]\n[%f][%f][%f]\n[%d]\n",
            rendinf.campos.x, rendinf.campos.y, rendinf.campos.z, lookatx, lookaty, lookatz, blockx, blocky, blockz, round(blockx), blocky, round(blockz), blockid);
        */
        static bool placehold = false;
        static bool destroyhold = false;
        if (!destroyhold && input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_PLACE)) {
            if (!placehold || (altutime() - ptime) >= 500000)
                if ((altutime() - ptime2) >= 125000 && blockid && blockid != 7 && (!blockid2 || blockid2 == 7)) {
                    ptime2 = altutime();
                    setBlock(&chunks, 0, 0, 0, lastblockx, lastblocky, lastblockz, (struct blockdata){1, 0, 0, 0});
                }
            placehold = true;
        } else {
            placehold = false;
            ptime = altutime();
        }
        if (!placehold && input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_DESTROY)) {
            if (!destroyhold || (altutime() - dtime) >= 500000)
                if ((altutime() - dtime2) >= 125000 && blockid && blockid != 7 && (!blockid2 || blockid2 == 7)) {
                    dtime2 = altutime();
                    setBlock(&chunks, 0, 0, 0, blockx, blocky, blockz, (struct blockdata){0, 0, 0, 0});
                }
            destroyhold = true;
        } else {
            destroyhold = false;
            dtime = altutime();
        }
        if (!rendinf.vsync || !rendinf.fps || (altutime() - fpsstarttime2) >= rendtime / rendinf.fps) {
            if (curbdata.id == 7) {
                setSpace(SPACE_UNDERWATER);
            } else {
                setSpace(SPACE_NORMAL);
            }
            renderChunks(&chunks);
            updateScreen();
            fpsstarttime2 = altutime();
            #ifdef SHOWFPS
            ++fpsct;
            #endif
        }
        uint64_t curtime = altutime();
        if (curtime - fpsstarttime >= 1000000) {
            #ifdef SHOWFPS
            printf("rendered [%d] frames in %f seconds with a goal of [%d] frames\n", fpsct, (float)(curtime - fpsstarttime) / 1000000.0, rendinf.fps);
            #endif
            fpsstarttime = curtime;
            if (rendinf.vsync) {
                if (rendtime > 500000 && fpsct < (int)rendinf.fps) rendtime -= 10000;
                if (rendtime < 1000000 && fpsct > (int)rendinf.fps + 5) rendtime += 10000;
            }
            fpsct = 0;
        }
        fpsmult = (float)(altutime() - starttime) / (1000000.0f / 60.0f);
        pmult = posmult * fpsmult;
        rmult = rotmult * fpsmult;
    }
    for (int i = 0; i < 16; ++i) {
        free(tmpbuf[i]);
    }
    free(tmpbuf);
}
