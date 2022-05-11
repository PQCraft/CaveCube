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

int fps;

static float posmult = 0.125;
static float rotmult = 3;
static float fpsmult = 0;
static float mousesns = 0.05;

struct chunkdata chunks;

struct blockdata getBlockF(struct chunkdata* chunks, float x, float y, float z) {
    x -= (x < 0) ? 1.0 : 0.0;
    y -= (y < 0) ? 1.0 : 0.0;
    z += (z > 0) ? 1.0 : 0.0;
    return getBlock(chunks, 0, 0, 0, x, y, z);
}

coord_3d intCoord(coord_3d in) {
    in.x -= (in.x < 0) ? 1.0 : 0.0;
    in.y -= (in.y < 0) ? 1.0 : 0.0;
    in.z += (in.z > 0) ? 1.0 : 0.0;
    in.x = (int)in.x;
    in.y = (int)in.y;
    in.z = (int)in.z;
    return in;
}

coord_3d pcollide(struct chunkdata* chunks, coord_3d pos) {
    coord_3d new = pos;
    new = intCoord(new);
    //printf("collide check from: [%f][%f][%f] -> [%f][%f][%f]\n", pos.x, pos.y, pos.z, new.x, new.y, new.z);
    struct blockdata tmpbd[4] = {
        getBlock(chunks, 0, 0, 0, new.x, new.y, new.z + 1),
        getBlock(chunks, 0, 0, 0, new.x, new.y, new.z - 1),
        getBlock(chunks, 0, 0, 0, new.x + 1, new.y, new.z),
        getBlock(chunks, 0, 0, 0, new.x - 1, new.y, new.z),
    };
    /*
    stuct blockdata tmpebd[4] = {
        getBlock(chunks, 0, 0, 0, new.x + 1, new.y, new.z + 1),
        getBlock(chunks, 0, 0, 0, new.x + 1, new.y, new.z - 1),
        getBlock(chunks, 0, 0, 0, new.x - 1, new.y, new.z + 1),
        getBlock(chunks, 0, 0, 0, new.x - 1, new.y, new.z - 1),
    };
    */
    struct blockdata tmplbd[4];
    coord_3d alt = pos;
    alt.x += 0.15;
    alt = intCoord(alt);
    tmplbd[0] = getBlock(chunks, 0, 0, 0, alt.x, alt.y, alt.z + 1);
    alt = pos;
    alt.x -= 0.15;
    alt = intCoord(alt);
    tmplbd[1] = getBlock(chunks, 0, 0, 0, alt.x, alt.y, alt.z - 1);
    alt = pos;
    alt.z -= 0.15;
    alt = intCoord(alt);
    tmplbd[2] = getBlock(chunks, 0, 0, 0, alt.x + 1, alt.y, alt.z);
    alt = pos;
    alt.z += 0.15;
    alt = intCoord(alt);
    tmplbd[3] = getBlock(chunks, 0, 0, 0, alt.x - 1, alt.y, alt.z);
    struct blockdata tmprbd[4];
    alt = pos;
    alt.x -= 0.15;
    alt = intCoord(alt);
    tmprbd[0] = getBlock(chunks, 0, 0, 0, alt.x, alt.y, alt.z + 1);
    alt = pos;
    alt.x += 0.15;
    alt = intCoord(alt);
    tmprbd[1] = getBlock(chunks, 0, 0, 0, alt.x, alt.y, alt.z - 1);
    alt = pos;
    alt.z += 0.15;
    alt = intCoord(alt);
    tmprbd[2] = getBlock(chunks, 0, 0, 0, alt.x + 1, alt.y, alt.z);
    alt = pos;
    alt.z -= 0.15;
    alt = intCoord(alt);
    tmprbd[3] = getBlock(chunks, 0, 0, 0, alt.x - 1, alt.y, alt.z);
    bool frontblock = (tmpbd[1].id || ((tmplbd[1].id || tmprbd[1].id)/* && !tmplbd[3].id && !tmprbd[2].id*/));
    bool backblock = (tmpbd[0].id || ((tmplbd[0].id || tmprbd[0].id)/* && !tmplbd[3].id && !tmprbd[2].id*/));
    bool leftblock = (tmpbd[3].id || ((tmplbd[3].id || tmprbd[3].id) && !tmplbd[1].id && !tmprbd[0].id));
    bool rightblock = (tmpbd[2].id || ((tmplbd[2].id || tmprbd[2].id) && !tmprbd[1].id && !tmplbd[0].id));
    if (frontblock) {
        //puts("front");
        if (pos.z < 0 && pos.z < new.z - 0.75) {
            //puts("push < 0");
            pos.z = new.z - 0.75;
        }
        if (pos.z >= 0 && pos.z < new.z + 0.25 - 1) {
            //puts("push >= 0");
            pos.z = new.z + 0.25 - 1;
        }
    }
    if (backblock) {
        //puts("back");
        if (pos.z < 0 && pos.z > new.z + 0.75 - 1) {
            //puts("push < 0");
            pos.z = new.z + 0.75 - 1;
        }
        if (pos.z >= 0 && pos.z > new.z - 0.25) {
            //puts("push >= 0");
            pos.z = new.z - 0.25;
        }
    }
    if (leftblock) {
        //puts("left");
        if (pos.x < 0 && pos.x < new.x + 0.25) {
            //puts("push < 0");
            pos.x = new.x + 0.25;
        }
        if (pos.x >= 0 && pos.x < new.x + 0.25) {
            //puts("push >= 0");
            pos.x = new.x + 0.25;
        }
    }
    if (rightblock) {
        //puts("right");
        if (pos.x < 0 && pos.x > new.x - 0.25 + 1) {
            //puts("push < 0");
            pos.x = new.x - 0.25 + 1;
        }
        if (pos.x >= 0 && pos.x > new.x - 0.25 + 1) {
            //puts("push >= 0");
            pos.x = new.x - 0.25 + 1;
        }
    }
    return pos;
}

void doGame() {
    char** tmpbuf = malloc(16 * sizeof(char*));
    for (int i = 0; i < 16; ++i) {
        tmpbuf[i] = malloc(4096);
    }
    chunks = allocChunks(atoi(getConfigVarStatic(config, "game.chunks", "9", 64)));
    rendinf.campos.y = 101.5;
    initInput();
    float pmult = posmult;
    float rmult = rotmult;
    initServer(SERVER_MODE_SP);
    //int farlands = 17616074;
    int cx = 0;
    int cz = 0;
    //genChunks(&chunks, cx, cz);
    uint64_t fpsstarttime2 = altutime();
    uint64_t ptime = fpsstarttime2;
    uint64_t dtime = fpsstarttime2;
    uint64_t ptime2 = fpsstarttime2;
    uint64_t dtime2 = fpsstarttime2;
    uint64_t rendtime = 750000;
    uint64_t fpsstarttime = fpsstarttime2;
    int fpsct = 0;
    float yvel = 0.0;
    float xcm = 0.0;
    float zcm = 0.0;
    while (!quitRequest) {
        uint64_t starttime = altutime();
        float npmult = 0.5;
        float nrmult = 1.0;
        input_info input = getInput();
        bool crouch = false;
        if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_CROUCH)) {
            crouch = true;
            npmult /= 2.0;
        } else if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_RUN)) {
            npmult *= 2.0;
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
        int cmx = 0, cmz = 0;
        static bool first = true;
        while (rendinf.campos.z > 8.0) {
            --cz;
            rendinf.campos.z -= 16.0;
            ++cmz;
        }
        while (rendinf.campos.z < -8.0) {
            ++cz;
            rendinf.campos.z += 16.0;
            --cmz;
        }
        while (rendinf.campos.x > 8.0) {
            ++cx;
            rendinf.campos.x -= 16.0;
            ++cmx;
        }
        while (rendinf.campos.x < -8.0) {
            --cx;
            rendinf.campos.x += 16.0;
            --cmx;
        }
        if (cmx || cmz || first) {
            first = false;
            moveChunks(&chunks, cmx, cmz);
            genChunks(&chunks, cx, cz);
        }
        //genChunks(&chunks, cx, cz);
        updateChunks(&chunks);
        //printf("old x [%f] y [%f]\n", rendinf.campos.x, rendinf.campos.z);
        struct blockdata curbdata = getBlockF(&chunks, rendinf.campos.x, rendinf.campos.y, rendinf.campos.z);
        struct blockdata curbdata2 = getBlockF(&chunks, rendinf.campos.x, rendinf.campos.y - 1, rendinf.campos.z);
        //struct blockdata underbdata = getBlockF(&chunks, rendinf.campos.x, rendinf.campos.y - 1.51, rendinf.campos.z);
        struct blockdata tmpbd2[4] = {
            getBlockF(&chunks, rendinf.campos.x + 0.2, rendinf.campos.y - 1.51, rendinf.campos.z + 0.2),
            getBlockF(&chunks, rendinf.campos.x - 0.2, rendinf.campos.y - 1.51, rendinf.campos.z + 0.2),
            getBlockF(&chunks, rendinf.campos.x + 0.2, rendinf.campos.y - 1.51, rendinf.campos.z - 0.2),
            getBlockF(&chunks, rendinf.campos.x - 0.2, rendinf.campos.y - 1.51, rendinf.campos.z - 0.2),
        };
        bool onblock = (tmpbd2[0].id || tmpbd2[1].id || tmpbd2[2].id || tmpbd2[3].id);
        struct blockdata overbdata = getBlockF(&chunks, rendinf.campos.x, rendinf.campos.y + 1.5, rendinf.campos.z);
        if (onblock) {
            float mul = pmult * 3;
            xcm = ((input.zmov * sinf(yrotrad) * ((input.movti) ? posmult : pmult) * npmult) + (input.xmov * cosf(yrotrad) * ((input.movti) ? posmult : pmult) * npmult)) * mul + xcm * (1.0 - mul);
            zcm = (-(input.zmov * cosf(yrotrad) * ((input.movti) ? posmult : pmult) * npmult) + (input.xmov * sinf(yrotrad) * ((input.movti) ? posmult : pmult) * npmult)) * mul + zcm * (1.0 - mul);
            if (rendinf.campos.y < (float)((int)(rendinf.campos.y)) + 0.5 && yvel <= 0.0) {
                rendinf.campos.y = (float)((int)(rendinf.campos.y)) + 0.5;
            }
            if (yvel <= 0 && (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_JUMP))) {
                yvel = 1.0;
            }
            if (yvel < 0) yvel = 0.0;
        } else {
            float mul = pmult * 0.5;
            xcm = ((input.zmov * sinf(yrotrad) * ((input.movti) ? posmult : pmult) * npmult) + (input.xmov * cosf(yrotrad) * ((input.movti) ? posmult : pmult) * npmult)) * mul + xcm * (1.0 - mul);
            zcm = (-(input.zmov * cosf(yrotrad) * ((input.movti) ? posmult : pmult) * npmult) + (input.xmov * sinf(yrotrad) * ((input.movti) ? posmult : pmult) * npmult)) * mul + zcm * (1.0 - mul);
        }
        //printf("yvel: [%f] [%f] [%f] [%f] [%u]\n", rendinf.campos.y, (float)((int)(blocky2)) + 0.5, yvel, pmult, underbdata.id & 0xFF);
        if (yvel < 0 && !onblock) {
            rendinf.campos.y += yvel * pmult;
        } else if (yvel > 0) {
            rendinf.campos.y += yvel * pmult;
        }
        if (!onblock) {
            if (yvel > -7.5) {
                yvel -= 0.5 * pmult;
            }
        }
        rendinf.campos.z += zcm / div;
        rendinf.campos.x += xcm / div;
        float ncz = rendinf.campos.z;
        float ncx = rendinf.campos.x;
        //coord_3d newcoord = {(int)rendinf.campos.x, (int)rendinf.campos.y, (int)rendinf.campos.z};
        coord_3d newcoord = rendinf.campos;
        newcoord = intCoord(newcoord);
        //printf("cam: [%f][%f][%f]; block: [%f][%f]\n", rendinf.campos.x, rendinf.campos.y, rendinf.campos.z, newcoord.x, newcoord.z);
        float oldy = rendinf.campos.y;
        rendinf.campos.y = oldy - 0.5;
        rendinf.campos = pcollide(&chunks, rendinf.campos);
        rendinf.campos.y = oldy + 0.49 - ((crouch) ? 0.375 : 0);
        rendinf.campos = pcollide(&chunks, rendinf.campos);
        rendinf.campos.y = oldy - 1.24;
        rendinf.campos = pcollide(&chunks, rendinf.campos);
        rendinf.campos.y = oldy;
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
            blocky = lookaty * depth + (rendinf.campos.y - ((crouch) ? 0.375 : 0));
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
                    setBlock(&chunks, 0, 0, 0, lastblockx, lastblocky, lastblockz, (struct blockdata){1, 0, 0});
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
                    setBlock(&chunks, 0, 0, 0, blockx, blocky, blockz, (struct blockdata){0, 0, 0});
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
                //setSpace(SPACE_PARALLEL);
            }
            if (crouch) rendinf.campos.y -= 0.375;
            updateCam();
            if (crouch) rendinf.campos.y += 0.375;
            renderChunks(&chunks);
            updateScreen();
            fpsstarttime2 = altutime();
            ++fpsct;
        }
        uint64_t curtime = altutime();
        if (curtime - fpsstarttime >= 1000000) {
            #ifdef SHOWFPS
            printf("Rendered [%d] frames in %f seconds with a goal of [%d] frames in 1.000000 seconds.\n", fpsct, (float)(curtime - fpsstarttime) / 1000000.0, rendinf.fps);
            #endif
            fps = fpsct;
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
