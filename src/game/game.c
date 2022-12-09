#if MODULEID == MODULEID_GAME

#include <main/main.h>
#include "game.h"
#include "chunk.h"
#include "blocks.h"
#include <main/version.h>
#include <common/common.h>
#include <common/resource.h>
#include <renderer/renderer.h>
#include <renderer/ui.h>
#include <input/input.h>
#include <server/server.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include <string.h>

#include <common/glue.h>

int fps;
int realfps;
bool showDebugInfo = true;
coord_3d_dbl pcoord;
coord_3d pvelocity;
int pblockx, pblocky, pblockz;

struct ui_data* game_ui[4];

static float posmult = 6.5;
static float fpsmult = 0;

static force_inline void writeChunk(struct chunkdata* chunks, int64_t x, int64_t z, struct blockdata* data) {
    pthread_mutex_lock(&chunks->lock);
    int64_t nx = (x - chunks->xoff) + chunks->info.dist;
    int64_t nz = chunks->info.width - ((z - chunks->zoff) + chunks->info.dist) - 1;
    if (nx < 0 || nz < 0 || nx >= chunks->info.width || nz >= chunks->info.width) {
        pthread_mutex_unlock(&chunks->lock);
        return;
    }
    uint32_t coff = nx + nz * chunks->info.width;
    //printf("writing chunk to [%"PRId64", %"PRId64"] ([%"PRId64", %"PRId64"])\n", nx, nz, x, z);
    memcpy(chunks->data[coff], data, 65536 * sizeof(struct blockdata));
    //chunks->renddata[coff].updated = false;
    chunks->renddata[coff].generated = true;
    updateChunk(x, z, CHUNKUPDATE_PRIO_LOW, 1);
    pthread_mutex_unlock(&chunks->lock);
}

static force_inline void reqChunks(struct chunkdata* chunks) {
    pthread_mutex_lock(&chunks->lock);
    //printf("set [%u]\n", cid);
    pthread_mutex_unlock(&chunks->lock);
    for (int i = 0; i <= (int)chunks->info.dist; ++i) {
        for (int z = -i; z <= i; ++z) {
            for (int x = -i; x <= i; ++x) {
                if (abs(z) == i || (abs(z) != i && abs(x) == i)) {
                    uint32_t coff = (z + chunks->info.dist) * chunks->info.width + (x + chunks->info.dist);
                    if (!chunks->renddata[coff].generated/* && !chunks->renddata[coff].requested*/) {
                        //printf("REQ [%"PRId64", %"PRId64"]\n", (int64_t)((int64_t)(x) + xo), (int64_t)((int64_t)(-z) + zo));
                        cliSend(CLIENT_GETCHUNK, (int64_t)((int64_t)(x) + chunks->xoff), (int64_t)((int64_t)(-z) + chunks->zoff));
                    }
                }
            }
        }
    }
}

static force_inline struct blockdata getBlockF(struct chunkdata* chunks, float x, float y, float z) {
    x -= (x < 0) ? 1.0 : 0.0;
    y -= (y < 0) ? 1.0 : 0.0;
    z -= (z < 0) ? 1.0 : 0.0;
    return getBlock(chunks, x, y, z);
}

static force_inline coord_3d_dbl w2bCoord(coord_3d_dbl in) {
    in.x -= (in.x < 0) ? 1.0 : 0.0;
    in.y -= (in.y < 0) ? 1.0 : 0.0;
    in.z -= (in.z < 0) ? 1.0 : 0.0;
    in.x = (int64_t)in.x;
    in.y = (int64_t)in.y;
    in.z = (int64_t)in.z;
    return in;
}

static force_inline coord_3d_dbl icoord2wcoord(coord_3d cam, int64_t cx, int64_t cz) {
    coord_3d_dbl ret;
    ret.x = cx * 16 + (double)cam.x;
    ret.y = (double)cam.y - 0.5;
    ret.z = cz * 16 + (double)-cam.z;
    return ret;
}

static force_inline void updateHotbar(int hb, int slot) {
    char hbslot[2] = {slot + '0', 0};
    editUIElem(game_ui[UILAYER_CLIENT], hb, NULL, -1, "slot", hbslot, NULL);
}

//#define mod(x, n) (((x) % (n) + (n)) % (n))

//int64_t farlands = -((int64_t)1 << 55);

static pthread_mutex_t gfxlock = PTHREAD_MUTEX_INITIALIZER;
static bool ping = false;
static int compat = 0;
static bool setskycolor = false;
static color newskycolor;

static void handleServer(int msg, void* _data) {
    //printf("Recieved [%d] from server\n", msg);
    switch (msg) {
        case SERVER_PONG:; {
            printf("Server ponged\n");
            ping = true;
            break;
        }
        case SERVER_COMPATINFO:; {
            struct server_data_compatinfo* data = _data;
            printf("Server version is %s %d.%d.%d\n", data->server_str, data->ver_major, data->ver_minor, data->ver_patch);
            if (data->flags & SERVER_FLAG_NOAUTH) puts("- No authentication required");
            if (data->flags & SERVER_FLAG_PASSWD) puts("- Password protected");
            compat = (!strcasecmp(data->server_str, PROG_NAME) && data->ver_major == VER_MAJOR && data->ver_minor == VER_MINOR && data->ver_patch == VER_PATCH) ? 1 : -1;
            break;
        }
        case SERVER_UPDATECHUNK:; {
            struct server_data_updatechunk* data = _data;
            writeChunk(rendinf.chunks, data->x, data->z, data->data);
            break;
        }
        case SERVER_SETSKYCOLOR:; {
            struct server_data_setskycolor* data = _data;
            //printf("set sky color to [#%02x%02x%02x]\n", data->r, data->g, data->b);
            pthread_mutex_lock(&gfxlock);
            newskycolor = (color){(float)data->r / 255.0, (float)data->g / 255.0, (float)data->b / 255.0, 1.0};
            setskycolor = true;
            pthread_mutex_unlock(&gfxlock);
            break;
        }
        case SERVER_SETBLOCK:; {
            struct server_data_setblock* data = _data;
            int64_t ucx, ucz;
            getChunkOfBlock(data->x, data->z, &ucx, &ucz);
            //printf("set block at [%"PRId64", %d, %"PRId64"] ([%"PRId64", %"PRId64"]) to [%d]\n", data->x, data->y, data->z, ucx, ucz, data->data.id);
            setBlock(rendinf.chunks, data->x, data->y, data->z, data->data);
            updateChunk(ucx, ucz, CHUNKUPDATE_PRIO_HIGH, 1);
            break;
        }
    }
}

static int loopdelay = 0;

bool doGame(char* addr, int port) {
    char** tmpbuf = malloc(16 * sizeof(char*));
    for (int i = 0; i < 16; ++i) {
        tmpbuf[i] = malloc(4096);
    }
    declareConfigKey(config, "Game", "viewDist", "8", false);
    declareConfigKey(config, "Game", "loopDelay", "1000", false);
    declareConfigKey(config, "Player", "name", "Player", false);
    declareConfigKey(config, "Player", "skin", "", false);
    rendinf.chunks = allocChunks(atoi(getConfigKey(config, "Game", "viewDist")));
    if (rendinf.fps || rendinf.vsync) loopdelay = atoi(getConfigKey(config, "Game", "loopDelay"));
    printf("Allocated chunks: [%d] [%d]\n", rendinf.chunks->info.width, rendinf.chunks->info.widthsq);
    rendinf.campos.y = 76.5;
    initInput();
    float pmult = posmult;
    puts("Connecting to server...");
    if (!cliConnect((addr) ? addr : "127.0.0.1", port, handleServer)) {
        fputs("Failed to connect to server\n", stderr);
        return false;
    }
    puts("Sending ping...");
    cliSend(CLIENT_PING);
    while (!ping && !quitRequest) {
        getInput(NULL);
        microwait(100000);
    }
    if (quitRequest) return false;
    puts("Server responded to ping");
    puts("Exchanging compatibility info...");
    cliSend(CLIENT_COMPATINFO, VER_MAJOR, VER_MINOR, VER_PATCH, 0, PROG_NAME);
    while (!compat && !quitRequest) {
        getInput(NULL);
        microwait(100000);
    }
    if (compat < 0) {
        fputs("Server version mismatch\n", stderr);
        return false;
    }
    if (quitRequest) return false;

    struct input_info input;
    //genChunks(&chunks, cx, cz);
    double fpstime = 0;
    double lowframe = 1000000.0 / (double)rendinf.disphz;
    uint64_t fpsstarttime2 = altutime();
    uint64_t fpsstarttime = fpsstarttime2;
    int fpsct = 0;
    float xcm = 0.0;
    float zcm = 0.0;
    int invspot = 0;
    int invoff = 0;
    int blocksub = 0;
    startMesher();
    setRandSeed(8, altutime());
    coord_3d tmpcamrot = {0, 0, 0};

    resetInput();
    setInputMode(INPUT_MODE_GAME);
    //setSkyColor(0.5, 0.5, 0.5);
    for (int i = 0; i < 4; ++i) {
        game_ui[i] = allocUI();
    }
    getInput(&input);
    game_ui[UILAYER_DBGINF]->hidden = !showDebugInfo;
    game_ui[UILAYER_INGAME]->hidden = input.focus;

    int ui_main = newUIElem(game_ui[UILAYER_INGAME], UI_ELEM_BOX, "main", -1, -1, "width", "100%", "height", "100%", "color", "#000000", "alpha", "0.25", "z", "-100", NULL);
    /*int ui_placeholder = */newUIElem(game_ui[UILAYER_INGAME], UI_ELEM_BUTTON, "placeholder", ui_main, -1, "width", "128", "height", "36", "text", "[Placeholder]", NULL);

    int ui_hotbar = newUIElem(game_ui[UILAYER_CLIENT], UI_ELEM_HOTBAR, "hotbar", -1, -1, "align", "0,1", "margin", "0,10,0,10", NULL);
    updateHotbar(ui_hotbar, invspot);
#if 0
    int ui_inv_main = newUIElem(game_ui[UILAYER_SERVER], UI_ELEM_BOX, "main", -1, -1, "width", "100%", "height", "100%", "color", "#000000", "alpha", "0.25", "z", "-100", NULL);
    int ui_inventory = newUIElem(game_ui[UILAYER_SERVER], UI_ELEM_FANCYBOX, "inventory", ui_inv_main, -1, "width", "332", "height", "360", NULL);
    int ui_inv_grid = newUIElem(game_ui[UILAYER_SERVER], UI_ELEM_ITEMGRID, "inv_grid", ui_inventory, -1, "width", "10", "height", "4", "align", "0,1", "margin", "0,6,0,16", NULL);
    /*int ui_inv_hb = */newUIElem(game_ui[UILAYER_SERVER], UI_ELEM_ITEMGRID, "inv_hotbar", ui_inventory, ui_inv_grid, "width", "10", "height", "1", "align", "0,1", "margin", "0,6,0,6", NULL);
#endif
    setFullscreen(rendinf.fullscr);
    while (!quitRequest) {
        uint64_t st1 = altutime();
        if (loopdelay) microwait(loopdelay);
        float bps = 4;
        getInput(&input);
        {
            if (!input.focus && inputMode != INPUT_MODE_UI) {
                setInputMode(INPUT_MODE_UI);
                resetInput();
            }
            switch (input.single_action) {
                case INPUT_ACTION_SINGLE_DEBUG:;
                    game_ui[UILAYER_DBGINF]->hidden = !(showDebugInfo = !showDebugInfo);
                    break;
                case INPUT_ACTION_SINGLE_FULLSCR:;
                    setFullscreen(!rendinf.fullscr);
                    break;
            }
            switch (inputMode) {
                case INPUT_MODE_GAME:; {
                    switch (input.single_action) {
                        case INPUT_ACTION_SINGLE_INV_0 ... INPUT_ACTION_SINGLE_INV_9:;
                            invspot = input.single_action - INPUT_ACTION_SINGLE_INV_0;
                            updateHotbar(ui_hotbar, invspot);
                            blocksub = 0;
                            break;
                        case INPUT_ACTION_SINGLE_INV_NEXT:;
                            ++invspot;
                            if (invspot > 9) invspot = 0;
                            updateHotbar(ui_hotbar, invspot);
                            blocksub = 0;
                            break;
                        case INPUT_ACTION_SINGLE_INV_PREV:;
                            --invspot;
                            if (invspot < 0) invspot = 9;
                            updateHotbar(ui_hotbar, invspot);
                            blocksub = 0;
                            break;
                        case INPUT_ACTION_SINGLE_INVOFF_NEXT:;
                            ++invoff;
                            if (invoff > 4) invoff = 0;
                            blocksub = 0;
                            break;
                        case INPUT_ACTION_SINGLE_INVOFF_PREV:;
                            --invoff;
                            if (invoff < 0) invoff = 4;
                            blocksub = 0;
                            break;
                        case INPUT_ACTION_SINGLE_ROT_X:;
                            ++blocksub;
                            break;
                        case INPUT_ACTION_SINGLE_ROT_Y:;
                            --blocksub;
                            if (blocksub < 0) blocksub = 0;
                            break;
                        /*
                        case INPUT_ACTION_SINGLE_ROT_Z:;
                            for (int z = -chunks->info.dist; z <= (int)chunks->info.dist; ++z) {
                                for (int x = -chunks->info.dist; x <= (int)chunks->info.dist; ++x) {
                                    updateChunk(x + cx, z + cz, CHUNKUPDATE_PRIO_HIGH, 0);
                                }
                            }
                            break;
                        */
                        case INPUT_ACTION_SINGLE_ESC:;
                            setInputMode(INPUT_MODE_UI);
                            resetInput();
                            break;
                    }
                    break;
                }
                case INPUT_MODE_UI:; {
                    switch (input.single_action) {
                        case INPUT_ACTION_SINGLE_ESC:;
                            setInputMode(INPUT_MODE_GAME);
                            resetInput();
                            break;
                    }
                    break;
                }
            }
        }
        float zoomrotmult;
        if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_ZOOM)) {
            rendinf.camfov = 12.5;
            zoomrotmult = 0.25;
        } else {
            rendinf.camfov = 85;
            zoomrotmult = 1.0;
        }
        bool crouch = false;
        if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_CROUCH)) {
            crouch = true;
            //bps *= 0.375;
        } /*else*/ if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_RUN)) {
            bps *= 1.6875;
        }
        float speedmult = 3.0;
        float leanmult = ((bps < 10.0) ? bps : 10.0) * 0.125 * 1.0;
        bps *= speedmult;
        tmpcamrot.x += input.rot_up * input.rot_mult_y * zoomrotmult;
        tmpcamrot.y -= input.rot_right * input.rot_mult_x * zoomrotmult;
        rendinf.camrot.x = tmpcamrot.x - input.mov_up * leanmult;
        if (rendinf.camrot.x > 89.99) rendinf.camrot.x = 89.99;
        if (rendinf.camrot.x < -89.99) rendinf.camrot.x = -89.99;
        rendinf.camrot.y = tmpcamrot.y;
        rendinf.camrot.z = input.mov_right * leanmult;
        if (tmpcamrot.y < 0) tmpcamrot.y += 360;
        else if (tmpcamrot.y >= 360) tmpcamrot.y -= 360;
        if (tmpcamrot.x > 90.0) tmpcamrot.x = 90.0;
        if (tmpcamrot.x < -90.0) tmpcamrot.x = -90.0;
        float yrotrad = (tmpcamrot.y / 180 * M_PI);
        int cmx = 0, cmz = 0;
        static bool first = true;
        rendinf.campos.z += zcm * input.mov_mult;
        rendinf.campos.x += xcm * input.mov_mult;
        pvelocity.x = xcm;
        pvelocity.z = zcm;
        while (rendinf.campos.z > 8.0) {
            rendinf.campos.z -= 16.0;
            ++cmz;
        }
        while (rendinf.campos.z < -8.0) {
            rendinf.campos.z += 16.0;
            --cmz;
        }
        while (rendinf.campos.x > 8.0) {
            rendinf.campos.x -= 16.0;
            ++cmx;
        }
        while (rendinf.campos.x < -8.0) {
            rendinf.campos.x += 16.0;
            --cmx;
        }
        if (cmx || cmz || first) {
            first = false;
            moveChunks(rendinf.chunks, cmx, cmz);
            reqChunks(rendinf.chunks);
            //microwait(1000000);
        }
        //genChunks(&chunks, cx, cz);
        //printf("x [%f] z [%f]\n", rendinf.campos.x, rendinf.campos.z);
        struct blockdata curbdata = getBlockF(rendinf.chunks, rendinf.campos.x, rendinf.campos.y, rendinf.campos.z);
        //struct blockdata curbdata2 = getBlockF(&chunks, rendinf.campos.x, rendinf.campos.y - 1, rendinf.campos.z);
        //struct blockdata underbdata = getBlockF(&chunks, rendinf.campos.x, rendinf.campos.y - 1.51, rendinf.campos.z);
        float f1 = input.mov_up * sinf(yrotrad);
        float f2 = input.mov_right * cosf(yrotrad);
        float f3 = input.mov_up * cosf(yrotrad);
        float f4 = (input.mov_right * sinf(yrotrad)) * -1;
        xcm = (f1 * bps) + (f2 * bps);
        zcm = (f3 * bps) + (f4 * bps);
        //printf("[yr:%f]:[us:%f][rc:%f][uc:%f][rs:%f]:[x:%f][z:%f]\n", yrotrad, f1, f2, f3, f4, xcm, zcm);
        float yvel = 0.0;
        /*
        if (rendinf.campos.y < (float)((int)(rendinf.campos.y)) + 0.5 && yvel <= 0.0) {
            rendinf.campos.y = (float)((int)(rendinf.campos.y)) + 0.5;
        }
        */
        speedmult /= 4.0;
        speedmult += 0.75;
        if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_JUMP)) {
            yvel += 1.0 * (speedmult + (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_RUN)) * 0.0175);
        }
        if (crouch) {
            yvel -= 1.0 * (speedmult + (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_RUN)) * 0.0175);
        }
        //printf("yvel: [%f] [%f] [%f] [%f] [%u]\n", rendinf.campos.y, (float)((int)(blocky2)) + 0.5, yvel, pmult, underbdata.id & 0xFF);
        pvelocity.y = yvel;
        rendinf.campos.y += yvel * pmult;
        if (rendinf.campos.y < -2.5) rendinf.campos.y = -2.5;
        //printf("[%d] [%d] [%d]\n", (!rendinf.vsync && !rendinf.fps), !rendinf.fps, (altutime() - fpsstarttime2) >= rendtime / rendinf.fps);
        //uint64_t et1 = altutime() - st1;
        if ((!rendinf.vsync && !rendinf.fps) || !rendinf.fps || (altutime() - fpsstarttime2) >= (1000000 / rendinf.fps) - loopdelay) {
            if (rendinf.fps) {
                uint64_t mwdtime = (1000000 / rendinf.fps) - (altutime() - fpsstarttime2);
                if (mwdtime < (1000000 / rendinf.fps)) microwait(mwdtime);
            }
            //puts("render");
            double tmp = (double)(altutime() - fpsstarttime2);
            fpsstarttime2 = altutime();
            if (curbdata.id == 7) {
                setVisibility(-1.0, 0.5);
                setScreenMult(0.425, 0.6, 0.75);
            } else {
                setVisibility(0.5, 1.0);
                setScreenMult(1.0, 1.0, 1.0);
            }
            pthread_mutex_lock(&gfxlock);
            if (setskycolor) {
                setSkyColor(newskycolor.r, newskycolor.g, newskycolor.b);
                setskycolor = false;
            }
            pthread_mutex_unlock(&gfxlock);
            //if (crouch) rendinf.campos.y -= 0.375;
            updateCam();
            updateChunks();
            //if (crouch) rendinf.campos.y += 0.375;
            render();
            updateScreen();
            fpstime += tmp;
            if (tmp > lowframe) lowframe = tmp;
            ++fpsct;
        }
        //uint64_t et2 = altutime() - st1;
        //if (et2 > 16667) printf("OY!: logic:[%lu]; rend:[%lu]\n", et1, et2);
        coord_3d_dbl bcoord = w2bCoord(pcoord);
        pblockx = bcoord.x;
        pblocky = bcoord.y;
        pblockz = bcoord.z;
        uint64_t curtime = altutime();
        if (curtime - fpsstarttime >= 150000) {
            fps = round(1000000.0 / (double)((fpstime / (double)fpsct)));
            realfps = round(1000000.0 / (double)lowframe);
            fpsstarttime = curtime;
            /*
            if (rendinf.fps) {
                if (rendtime > 50000 && fpsct < (int)rendinf.fps) rendtime -= 100000;
                if (rendtime < 100000 && fpsct > (int)rendinf.fps + 5) rendtime += 100000;
            }
            */
            fpsct = 0;
            fpstime = 0;
            lowframe = 1000000.0 / (double)rendinf.disphz;
        }
        fpsmult = (double)((uint64_t)altutime() - (uint64_t)st1) / 1000000.0;
        pmult = posmult * fpsmult;
    }
    for (int i = 0; i < 4; ++i) {
        freeUI(game_ui[i]);
    }
    for (int i = 0; i < 16; ++i) {
        free(tmpbuf[i]);
    }
    free(tmpbuf);
    return true;
}

#endif
