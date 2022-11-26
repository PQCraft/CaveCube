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
int64_t pchunkx, pchunky, pchunkz;
int pblockx, pblocky, pblockz;

struct ui_data* game_ui[4];

static float posmult = 6.5;
static float fpsmult = 0;

static struct chunkdata chunks;

static int64_t cxo = 0, czo = 0;

static force_inline void writeChunk(struct chunkdata* chunks, int64_t x, int64_t z, struct blockdata* data) {
    pthread_mutex_lock(&uclock);
    int64_t nx = (x - cxo) + chunks->info.dist;
    int64_t nz = chunks->info.width - ((z - czo) + chunks->info.dist) - 1;
    if (nx < 0 || nz < 0 || nx >= chunks->info.width || nz >= chunks->info.width) {
        pthread_mutex_unlock(&uclock);
        return;
    }
    uint32_t coff = nx + nz * chunks->info.width;
    //printf("writing chunk to [%"PRId64", %"PRId64"] ([%"PRId64", %"PRId64"])\n", nx, nz, x, z);
    memcpy(chunks->data[coff], data, 65536 * sizeof(struct blockdata));
    //chunks->renddata[coff].updated = false;
    updateChunk(x, z, 1);
    chunks->renddata[coff].generated = true;
    pthread_mutex_unlock(&uclock);
}

static force_inline void reqChunks(struct chunkdata* chunks, int64_t xo, int64_t zo) {
    /*
    static bool init = false;
    if (!init) {
        pthread_mutex_init(&cidlock, NULL);
        init = true;
    }
    */
    pthread_mutex_lock(&uclock);
    cxo = xo;
    czo = zo;
    setMeshChunkOff(xo, zo);
    //printf("set [%u]\n", cid);
    pthread_mutex_unlock(&uclock);
    for (int i = 0; i <= (int)chunks->info.dist; ++i) {
        for (int z = -i; z <= i; ++z) {
            for (int x = -i; x <= i; ++x) {
                if (abs(z) == i || (abs(z) != i && abs(x) == i)) {
                    uint32_t coff = (z + chunks->info.dist) * chunks->info.width + (x + chunks->info.dist);
                    if (!chunks->renddata[coff].generated/* && !chunks->renddata[coff].requested*/) {
                        //printf("REQ [%"PRId64", %"PRId64"]\n", (int64_t)((int64_t)(x) + xo), (int64_t)((int64_t)(-z) + zo));
                        cliSend(CLIENT_GETCHUNK, (int64_t)((int64_t)(x) + xo), (int64_t)((int64_t)(-z) + zo));
                    }
                }
            }
        }
    }
}

static force_inline struct blockdata getBlockF(struct chunkdata* chunks, float x, float y, float z) {
    x -= (x < 0) ? 1.0 : 0.0;
    y -= (y < 0) ? 1.0 : 0.0;
    z += (z > 0) ? 1.0 : 0.0;
    return getBlock(chunks, 0, 0, x, y, z);
}

static force_inline coord_3d intCoord(coord_3d in) {
    in.x -= (in.x < 0) ? 1.0 : 0.0;
    in.y -= (in.y < 0) ? 1.0 : 0.0;
    in.z += (in.z > 0) ? 1.0 : 0.0;
    in.x = (int)in.x;
    in.y = (int)in.y;
    in.z = (int)in.z;
    return in;
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

static force_inline float dist2(float x1, float y1, float x2, float y2) {
    return sqrt(fabs(x2 - x1) * fabs(x2 - x1) + fabs(y2 - y1) * fabs(y2 - y1));
}

static force_inline float distz(float x1, float y1) {
    return sqrt(fabs(x1 * x1) + fabs(y1 * y1));
}

static force_inline bool bcollide(coord_3d bpos, coord_3d cpos) {
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

static force_inline bool phitblock(coord_3d block, coord_3d boffset, coord_3d* pos) {
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

static force_inline uint8_t pcollide(struct chunkdata* chunks, coord_3d* pos) {
    coord_3d new = *pos;
    new = intCoord(new);
    uint8_t ret = 0;
    //printf("collide check from: [%f][%f][%f] -> [%f][%f][%f]\n", pos.x, pos.y, pos.z, new.x, new.y, new.z);
    struct blockdata tmpbd[8] = {
        getBlock(chunks, 0, 0, new.x, new.y, new.z + 1),
        getBlock(chunks, 0, 0, new.x, new.y, new.z - 1),
        getBlock(chunks, 0, 0, new.x + 1, new.y, new.z),
        getBlock(chunks, 0, 0, new.x - 1, new.y, new.z),
        getBlock(chunks, 0, 0, new.x + 1, new.y, new.z + 1),
        getBlock(chunks, 0, 0, new.x + 1, new.y, new.z - 1),
        getBlock(chunks, 0, 0, new.x - 1, new.y, new.z + 1),
        getBlock(chunks, 0, 0, new.x - 1, new.y, new.z - 1),
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

static force_inline void pcollidepath(struct chunkdata* chunks, coord_3d oldpos, coord_3d* pos) {
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
            writeChunk(&chunks, data->x, data->z, data->data);
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
    }
}

static int loopdelay = 0;

bool doGame(char* addr, int port) {
    char** tmpbuf = malloc(16 * sizeof(char*));
    for (int i = 0; i < 16; ++i) {
        tmpbuf[i] = malloc(4096);
    }
    declareConfigKey(config, "Game", "viewDist", "8", false);
    declareConfigKey(config, "Game", "loopDelay", "5000", false);
    declareConfigKey(config, "Player", "name", "Player", false);
    declareConfigKey(config, "Player", "skin", "", false);
    chunks = allocChunks(atoi(getConfigKey(config, "Game", "viewDist")));
    if (rendinf.fps || rendinf.vsync) loopdelay = atoi(getConfigKey(config, "Game", "loopDelay"));
    printf("Allocated chunks: [%d] [%d]\n", chunks.info.width, chunks.info.widthsq);
    rendinf.campos.y = 121.5;
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
        getInput();
        microwait(100000);
    }
    if (quitRequest) return false;
    puts("Server responded to ping");
    puts("Exchanging compatibility info...");
    cliSend(CLIENT_COMPATINFO, VER_MAJOR, VER_MINOR, VER_PATCH, 0, PROG_NAME);
    while (!compat && !quitRequest) {
        getInput();
        microwait(100000);
    }
    if (compat < 0) {
        fputs("Server version mismatch\n", stderr);
        return false;
    }
    if (quitRequest) return false;
    //int64_t farlands = -((int64_t)1 << 55);
    int64_t cx = 0;
    int64_t cz = 0;
    //genChunks(&chunks, cx, cz);
    double fpstime = 0;
    double lowframe = 1000000.0 / (double)rendinf.disphz;
    uint64_t fpsstarttime2 = altutime();
    uint64_t ptime = fpsstarttime2;
    uint64_t dtime = fpsstarttime2;
    uint64_t ptime2 = fpsstarttime2;
    uint64_t dtime2 = fpsstarttime2;
    uint64_t fpsstarttime = fpsstarttime2;
    int fpsct = 0;
    float yvel = 0.0;
    float xcm = 0.0;
    float zcm = 0.0;
    int invspot = 0;
    int invoff = 0;
    int blocksub = 0;
    setMeshChunks(&chunks);
    startMesher();
    setRandSeed(8, altutime());
    coord_3d tmpcamrot = {0, 0, 0};
    resetInput();
    setInputMode(INPUT_MODE_GAME);
    //setSkyColor(0.5, 0.5, 0.5);
    for (int i = 0; i < 4; ++i) {
        game_ui[i] = allocUI();
    }
    game_ui[UILAYER_DBGINF]->hidden = !showDebugInfo;
    game_ui[UILAYER_INGAME]->hidden = getInput().focus;

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
        struct input_info input = getInput();
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
                        case INPUT_ACTION_SINGLE_VARIANT_NEXT:;
                            ++blocksub;
                            break;
                        case INPUT_ACTION_SINGLE_VARIANT_PREV:;
                            --blocksub;
                            if (blocksub < 0) blocksub = 0;
                            break;
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
        bool crouch = false;
        if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_CROUCH)) {
            crouch = true;
            bps *= 0.375;
        } else if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_RUN)) {
            bps *= 1.6875;
        }
        float speedmult = 1.0;
        float leanmult = ((bps < 10.0) ? bps : 10.0) / 3.5 * (1.0 + (speedmult * 0.1));
        bps *= speedmult;
        tmpcamrot.x += input.rot_up * input.rot_mult_y;
        tmpcamrot.y -= input.rot_right * input.rot_mult_x;
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
            moveChunks(&chunks, cx, cz, cmx, cmz);
            reqChunks(&chunks, cx, cz);
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
            struct blockdata bdata = getBlock(&chunks, 0, 0, blockx, blocky, blockz);
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
                    int blocknum = invspot + 1 + invoff * 10;
                    if (blockinf[blocknum].id && blockinf[blocknum].data[blocksub].id) {
                        setBlock(&chunks, cx, cz, lastblockx, lastblocky, lastblockz, (struct blockdata){blocknum, blocksub, 0, 0, 0, 0, 0, 0});
                    }
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
                    setBlock(&chunks, cx, cz, blockx, blocky, blockz, (struct blockdata){0, 0, 0, 0, 0, 0, 0, 0});
                }
            destroyhold = true;
        } else {
            destroyhold = false;
            dtime = altutime();
        }
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
                setScreenMult(0.4, 0.55, 0.8);
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
            if (crouch) rendinf.campos.y -= 0.375;
            updateCam();
            updateChunks();
            if (crouch) rendinf.campos.y += 0.375;
            render();
            updateScreen();
            fpstime += tmp;
            if (tmp > lowframe) lowframe = tmp;
            ++fpsct;
        }
        //uint64_t et2 = altutime() - st1;
        //if (et2 > 16667) printf("OY!: logic:[%lu]; rend:[%lu]\n", et1, et2);
        pcoord = icoord2wcoord(rendinf.campos, pchunkx, pchunkz);
        coord_3d_dbl bcoord = w2bCoord(pcoord);
        pblockx = bcoord.x;
        pblocky = bcoord.y;
        pblockz = bcoord.z;
        pchunky = pblocky / 16;
        if (pchunky < 0) pchunky = 0;
        if (pchunky > 15) pchunky = 15;
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
