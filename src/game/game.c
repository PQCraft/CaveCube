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
coord_3d_dbl pcoord;
coord_3d pvelocity;
int64_t pchunkx, pchunky, pchunkz;
int pblockx, pblocky, pblockz;

static float posmult = 6.5;
static float fpsmult = 0;

static struct chunkdata chunks;

static inline struct blockdata getBlockF(struct chunkdata* chunks, float x, float y, float z) {
    x -= (x < 0) ? 1.0 : 0.0;
    y -= (y < 0) ? 1.0 : 0.0;
    z += (z > 0) ? 1.0 : 0.0;
    return getBlock(chunks, 0, 0, 0, x, y, z);
}

static inline coord_3d intCoord(coord_3d in) {
    in.x -= (in.x < 0) ? 1.0 : 0.0;
    in.y -= (in.y < 0) ? 1.0 : 0.0;
    in.z += (in.z > 0) ? 1.0 : 0.0;
    in.x = (int)in.x;
    in.y = (int)in.y;
    in.z = (int)in.z;
    return in;
}

static inline coord_3d_dbl intCoord_dbl(coord_3d_dbl in) {
    in.x -= (in.x < 0) ? 1.0 : 0.0;
    in.y -= (in.y < 0) ? 1.0 : 0.0;
    in.z += (in.z > 0) ? 1.0 : 0.0;
    in.x = (int)in.x;
    in.y = (int)in.y;
    in.z = (int)in.z;
    return in;
}

static inline float dist2(float x1, float y1, float x2, float y2) {
    return sqrt(fabs(x2 - x1) * fabs(x2 - x1) + fabs(y2 - y1) * fabs(y2 - y1));
}

static inline float distz(float x1, float y1) {
    return sqrt(fabs(x1 * x1) + fabs(y1 * y1));
}

static inline bool bcollide(coord_3d bpos, coord_3d cpos) {
    if (cpos.x + 0.25 > bpos.x - 0.5 &&
        cpos.x - 0.25 < bpos.x + 0.5 &&
        cpos.z + 0.25 > bpos.z - 0.5 &&
        cpos.z - 0.25 < bpos.z + 0.5) {
        //puts("collide");
        return true;
    }
    return false;
}

static bool pcaxis[4];

static inline bool phitblock(coord_3d block, coord_3d boffset, coord_3d* pos) {
    block.x += boffset.x;
    block.y += boffset.y;
    block.z += boffset.z;
    block.x += 0.5;
    block.z -= 0.5;
    float distx = block.x - pos->x;
    float distz = block.z - pos->z;
    //printf("cam coords: [%f][%f][%f]\n", pos.x, pos.y, pos.z);
    //printf("block world coords: [%f][%f][%f]\n", block.x, block.y, block.z);
    //printf("x dist: [%f]\nz dist: [%f]\n", distx, distz);
    if (bcollide(block, *pos)) {
        if (fabs(distx) > fabs(distz)) {
            //puts("adjusting x");
            if (distx < 0) {
                pcaxis[0] = true;
                pos->x = block.x + 0.75;
            } else {
                pcaxis[1] = true;
                pos->x = block.x - 0.75;
            }
        } else {
            //puts("adjusting z");
            if (distz < 0) {
                pcaxis[2] = true;
                pos->z = block.z + 0.75;
            } else {
                pcaxis[3] = true;
                pos->z = block.z - 0.75;
            }
        }
        return true;
    }
    return false;
}

static inline uint8_t pcollide(struct chunkdata* chunks, coord_3d* pos) {
    coord_3d new = *pos;
    new = intCoord(new);
    uint8_t ret = 0;
    //printf("collide check from: [%f][%f][%f] -> [%f][%f][%f]\n", pos.x, pos.y, pos.z, new.x, new.y, new.z);
    struct blockdata tmpbd[8] = {
        getBlock(chunks, 0, 0, 0, new.x, new.y, new.z + 1),
        getBlock(chunks, 0, 0, 0, new.x, new.y, new.z - 1),
        getBlock(chunks, 0, 0, 0, new.x + 1, new.y, new.z),
        getBlock(chunks, 0, 0, 0, new.x - 1, new.y, new.z),
        getBlock(chunks, 0, 0, 0, new.x + 1, new.y, new.z + 1),
        getBlock(chunks, 0, 0, 0, new.x + 1, new.y, new.z - 1),
        getBlock(chunks, 0, 0, 0, new.x - 1, new.y, new.z + 1),
        getBlock(chunks, 0, 0, 0, new.x - 1, new.y, new.z - 1),
    };
    if (tmpbd[0].id && tmpbd[0].id != 7) {
        //puts("back");
        ret |= phitblock(new, (coord_3d){0, 0, 1}, pos);
    }
    ret <<= 1;
    if (tmpbd[1].id && tmpbd[1].id != 7) {
        //puts("front");
        ret |= phitblock(new, (coord_3d){0, 0, -1}, pos);
    }
    ret <<= 1;
    if (tmpbd[2].id && tmpbd[2].id != 7) {
        //puts("right");
        ret |= phitblock(new, (coord_3d){1, 0, 0}, pos);
    }
    ret <<= 1;
    if (tmpbd[3].id && tmpbd[3].id != 7) {
        //puts("left");
        ret |= phitblock(new, (coord_3d){-1, 0, 0}, pos);
    }
    ret <<= 1;
    if (tmpbd[4].id && tmpbd[4].id != 7) {
        //puts("back right");
        ret |= phitblock(new, (coord_3d){1, 0, 1}, pos);
    }
    ret <<= 1;
    if (tmpbd[5].id && tmpbd[5].id != 7) {
        //puts("front right");
        ret |= phitblock(new, (coord_3d){1, 0, -1}, pos);
    }
    ret <<= 1;
    if (tmpbd[6].id && tmpbd[6].id != 7) {
        //puts("back left");
        ret |= phitblock(new, (coord_3d){-1, 0, 1}, pos);
    }
    ret <<= 1;
    if (tmpbd[7].id && tmpbd[7].id != 7) {
        //puts("front left");
        ret |= phitblock(new, (coord_3d){-1, 0, -1}, pos);
    }
    return ret;
}

static inline void pcollidepath(struct chunkdata* chunks, coord_3d oldpos, coord_3d* pos) {
    float changex = pos->x - oldpos.x;
    float changez = pos->z - oldpos.z;
    float dist = distz(changex, changez);
    int steps = (dist * 25.0) + 1;
    for (int i = 1; i <= steps; ++i) {
        float offset = (float)(i) / (float)(steps);
        coord_3d tmpcoord = {oldpos.x + changex * offset, pos->y, oldpos.z + changez * offset};
        if (pcollide(chunks, &tmpcoord)) {
            *pos = tmpcoord;
            return;
        }
    }
}

static inline coord_3d_dbl icoord2wcoord(coord_3d cam, int64_t cx, int64_t cz) {
    coord_3d_dbl ret;
    ret.x = cx * 16 + (double)cam.x;
    ret.y = (double)cam.y - 0.5;
    ret.z = cz * 16 + (double)-cam.z;
    return ret;
}

static bool ping = false;

static void handleServer(int msg, void* data) {
    //printf("Recieved [%d] from server\n", msg);
    switch (msg) {
        case SERVER_RET_PONG:;
            printf("Server ponged\n");
            ping = true;
            break;
        case SERVER_RET_UPDATECHUNK:;
            genChunks_cb(&chunks, data);
            break;
        case SERVER_RET_UPDATECHUNKCOL:;
            genChunks_cb2(&chunks, data);
            break;
    }
}

static pthread_t srthreadh;

void* srthread(void* args) {
    (void)args;
    while (1) {
        microwait(25000);
        servRecv(handleServer, 16);
    }
    return NULL;
}

static int loopdelay;

bool doGame(char* addr, int port) {
    char** tmpbuf = malloc(16 * sizeof(char*));
    for (int i = 0; i < 16; ++i) {
        tmpbuf[i] = malloc(4096);
    }
    chunks = allocChunks(atoi(getConfigVarStatic(config, "game.chunks", "8", 64)));
    loopdelay = atoi(getConfigVarStatic(config, "game.loop_delay", "2500", 64));
    printf("Allocated chunks: [%d] [%d] [%d]\n", chunks.info.width, chunks.info.widthsq, chunks.info.size);
    rendinf.campos.y = 151.5;
    initInput();
    float pmult = posmult;
    int servport = port;
    if (!addr) {
        puts("Starting server...");
        if ((servport = startServer(NULL, servport, NULL, 1)) == -1) {
            puts("Server failed to start");
            return false;
        }
        printf("Started server on port [%d]\n", servport);
    }
    puts("Connecting to server...");
    if (!servConnect((addr) ? addr : "127.0.0.1", servport)) {
        fputs("Failed to connect to server\n", stderr);
        return false;
    }
    puts("Sending ping...");
    servSend(SERVER_MSG_PING, NULL, false);
    servRecv(handleServer, 1);
    while (!ping && !quitRequest) {
        getInput();
        microwait(100000);
        servRecv(handleServer, 1);
    }
    if (quitRequest) return false;
    puts("Server responded to ping");
    //int64_t farlands = -((int64_t)1 << 55);
    int64_t cx = 0;
    int64_t cz = 0;
    //genChunks(&chunks, cx, cz);
    uint64_t fpsstarttime2 = altutime();
    uint64_t ptime = fpsstarttime2;
    uint64_t dtime = fpsstarttime2;
    uint64_t ptime2 = fpsstarttime2;
    uint64_t dtime2 = fpsstarttime2;
    uint64_t rendtime = 1000000 - loopdelay;
    uint64_t fpsstarttime = fpsstarttime2;
    int fpsct = 0;
    float yvel = 0.0;
    float xcm = 0.0;
    float zcm = 0.0;
    #ifdef NAME_THREADS
    {
        char name[256];
        char name2[256];
        name[0] = 0;
        name2[0] = 0;
    #endif
        pthread_create(&srthreadh, NULL, &srthread, NULL);
    #ifdef NAME_THREADS
        pthread_getname_np(srthreadh, name2, 256);
        sprintf(name, "%s:cmsg", name2);
        pthread_setname_np(srthreadh, name);
    }
    #endif
    startMesher(&chunks);
    setRandSeed(8, altutime());
    coord_3d tmpcamrot = {0, 0, 0};
    resetInput();
    while (!quitRequest) {
        //uint64_t st1 = altutime();
        glfwSetTime(0);
        float bps = 4; //blocks per second
        struct input_info input = getInput();
        bool crouch = false;
        if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_CROUCH)) {
            crouch = true;
            bps = 1.5;
        } else if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_RUN)) {
            bps = 6.75;
        }
        float speedmult = 1.0;
        float leanmult = bps / 3.5 * (1.0 + (speedmult * 0.1));
        bps *= speedmult;
        tmpcamrot.x += input.rot_up * input.rot_mult;
        tmpcamrot.y -= input.rot_right * input.rot_mult;
        rendinf.camrot.x = tmpcamrot.x - input.mov_up * leanmult;
        if (rendinf.camrot.x > 89.99) rendinf.camrot.x = 89.99;
        if (rendinf.camrot.x < -89.99) rendinf.camrot.x = -89.99;
        rendinf.camrot.y = tmpcamrot.y;
        rendinf.camrot.z = input.mov_right * leanmult;
        if (tmpcamrot.y < 0) tmpcamrot.y += 360;
        else if (tmpcamrot.y >= 360) tmpcamrot.y -= 360;
        if (tmpcamrot.x > 89.99) tmpcamrot.x = 89.99;
        if (tmpcamrot.x < -89.99) tmpcamrot.x = -89.99;
        float yrotrad = (tmpcamrot.y / 180 * M_PI);
        int cmx = 0, cmz = 0;
        static bool first = true;
        coord_3d oldpos = rendinf.campos;
        rendinf.campos.z += zcm * input.mov_mult;
        rendinf.campos.x += xcm * input.mov_mult;
        pvelocity.x = xcm;
        pvelocity.z = -zcm;
        float oldy = rendinf.campos.y;
        rendinf.campos.y = oldy - 0.5;
        pcollidepath(&chunks, oldpos, &rendinf.campos);
        rendinf.campos.y = oldy + 0.49 - ((crouch) ? 0.375 : 0);
        pcollidepath(&chunks, oldpos, &rendinf.campos);
        rendinf.campos.y = oldy - 1.15;
        pcollidepath(&chunks, oldpos, &rendinf.campos);
        rendinf.campos.y = oldy;
        if (pcaxis[0] && xcm < 0) xcm *= 0;
        if (pcaxis[1] && xcm > 0) xcm *= 0;
        if (pcaxis[2] && zcm < 0) zcm *= 0;
        if (pcaxis[3] && zcm > 0) zcm *= 0;
        pcaxis[0] = false;
        pcaxis[1] = false;
        pcaxis[2] = false;
        pcaxis[3] = false;
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
            pchunkx = cx;
            pchunkz = cz;
            //microwait(1000000);
        }
        //genChunks(&chunks, cx, cz);
        //printf("old x [%f] y [%f]\n", rendinf.campos.x, rendinf.campos.z);
        struct blockdata curbdata = getBlockF(&chunks, rendinf.campos.x, rendinf.campos.y, rendinf.campos.z);
        //struct blockdata curbdata2 = getBlockF(&chunks, rendinf.campos.x, rendinf.campos.y - 1, rendinf.campos.z);
        //struct blockdata underbdata = getBlockF(&chunks, rendinf.campos.x, rendinf.campos.y - 1.51, rendinf.campos.z);
        struct blockdata tmpbd2[4] = {
            getBlockF(&chunks, rendinf.campos.x + 0.2, rendinf.campos.y - 1.51, rendinf.campos.z + 0.2),
            getBlockF(&chunks, rendinf.campos.x - 0.2, rendinf.campos.y - 1.51, rendinf.campos.z + 0.2),
            getBlockF(&chunks, rendinf.campos.x + 0.2, rendinf.campos.y - 1.51, rendinf.campos.z - 0.2),
            getBlockF(&chunks, rendinf.campos.x - 0.2, rendinf.campos.y - 1.51, rendinf.campos.z - 0.2),
        };
        bool onblock = ((tmpbd2[0].id && tmpbd2[0].id != 7) ||
                        (tmpbd2[1].id && tmpbd2[1].id != 7) ||
                        (tmpbd2[2].id && tmpbd2[2].id != 7) ||
                        (tmpbd2[3].id && tmpbd2[3].id != 7));
        //struct blockdata overbdata = getBlockF(&chunks, rendinf.campos.x, rendinf.campos.y + 1.5, rendinf.campos.z);
        //printf("pmult: [%f]\n", pmult);
        if (onblock) {
            xcm = ((input.mov_up * sinf(yrotrad) * bps) + (input.mov_right * cosf(yrotrad) * bps))/* * mul + xcm * (1.0 - mul)*/;
            zcm = (-(input.mov_up * cosf(yrotrad) * bps) + (input.mov_right * sinf(yrotrad) * bps))/* * mul + zcm * (1.0 - mul)*/;
            if (rendinf.campos.y < (float)((int)(rendinf.campos.y)) + 0.5 && yvel <= 0.0) {
                rendinf.campos.y = (float)((int)(rendinf.campos.y)) + 0.5;
            }
            if (yvel <= 0 && (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_JUMP))) {
                yvel = 1;
            }
            if (yvel < 0) yvel = 0.0;
        } else {
            xcm = ((input.mov_up * sinf(yrotrad) * bps) + (input.mov_right * cosf(yrotrad) * bps))/* * mul + xcm * (1.0 - mul)*/;
            zcm = (-(input.mov_up * cosf(yrotrad) * bps) + (input.mov_right * sinf(yrotrad) * bps))/* * mul + zcm * (1.0 - mul)*/;
        }
        //printf("yvel: [%f] [%f] [%f] [%f] [%u]\n", rendinf.campos.y, (float)((int)(blocky2)) + 0.5, yvel, pmult, underbdata.id & 0xFF);
        pvelocity.y = yvel;
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
        if (rendinf.campos.y < 2.5) rendinf.campos.y = 2.5;
        //float ncz = rendinf.campos.z;
        //float ncx = rendinf.campos.x;
        //coord_3d newcoord = {(int)rendinf.campos.x, (int)rendinf.campos.y, (int)rendinf.campos.z};
        //coord_3d newcoord = rendinf.campos;
        //newcoord = intCoord(newcoord);
        //printf("cam: [%f][%f][%f]; block: [%f][%f]\n", rendinf.campos.x, rendinf.campos.y, rendinf.campos.z, newcoord.x, newcoord.z);
        float granularity = 0.05;
        float lookatx = cos(rendinf.camrot.x * M_PI / 180.0) * sin(rendinf.camrot.y * M_PI / 180.0);
        float lookaty = sin(rendinf.camrot.x * M_PI / 180.0);
        float lookatz = (cos(rendinf.camrot.x * M_PI / 180.0) * cos(rendinf.camrot.y * M_PI / 180.0)) * -1;
        float blockx = 0, blocky = 0, blockz = 0;
        float lastblockx = 0, lastblocky = 0, lastblockz = 0;
        uint8_t blockid = 0, blockid2 = 0;
        int dist = (float)(6.0 / granularity);
        for (int i = 1; i < dist; ++i) {
            blockid2 = blockid;
            lastblockx = blockx;
            lastblocky = blocky;
            lastblockz = blockz;
            float depth = (float)i * granularity;
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
                    setBlock(&chunks, 0, 0, 0, lastblockx, lastblocky, lastblockz, (struct blockdata){(getRandWord(8) % 10) + 1, 0, 0});
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
        //printf("[%d] [%d] [%d]\n", (!rendinf.vsync && !rendinf.fps), !rendinf.fps, (altutime() - fpsstarttime2) >= rendtime / rendinf.fps);
        //uint64_t et1 = altutime() - st1;
        if ((!rendinf.vsync && !rendinf.fps) || !rendinf.fps || (altutime() - fpsstarttime2) >= rendtime / rendinf.fps) {
            //puts("render");
            if (curbdata.id == 7) {
                setSpace(SPACE_UNDERWATER);
            } else {
                setSpace(SPACE_NORMAL);
                //setSpace(SPACE_UNDERWORLD);
            }
            if (crouch) rendinf.campos.y -= 0.375;
            updateCam();
            if (crouch) rendinf.campos.y += 0.375;
            renderChunks(&chunks);
            updateScreen();
            fpsstarttime2 = altutime();
            ++fpsct;
        }
        //uint64_t et2 = altutime() - st1;
        //if (et2 > 16667) printf("OY!: logic:[%lu]; rend:[%lu]\n", et1, et2);
        pcoord = icoord2wcoord(rendinf.campos, pchunkx, pchunkz);
        coord_3d_dbl bcoord = intCoord_dbl(pcoord);
        pblockx = bcoord.x;
        pblocky = bcoord.y;
        pblockz = bcoord.z;
        pchunky = pblocky / 16;
        if (pchunky < 0) pchunky = 0;
        if (pchunky > 15) pchunky = 15;
        uint64_t curtime = altutime();
        if (curtime - fpsstarttime >= 1000000) {
            #ifdef SHOWFPS
            printf("Rendered [%d] frames in %f seconds with a goal of [%d] frames in 1.000000 seconds.\n", fpsct, (float)(curtime - fpsstarttime) / 1000000.0, rendinf.fps);
            #endif
            fps = fpsct;
            fpsstarttime = curtime;
            if (rendinf.fps) {
                if (rendtime > 500000 && fpsct < (int)rendinf.fps) rendtime -= 10000;
                if (rendtime < 1000000 && fpsct > (int)rendinf.fps + 5) rendtime += 10000;
            }
            fpsct = 0;
        }
        fpsmult = glfwGetTime();
        pmult = posmult * fpsmult;
    }
    for (int i = 0; i < 16; ++i) {
        free(tmpbuf[i]);
    }
    free(tmpbuf);
    return true;
}
