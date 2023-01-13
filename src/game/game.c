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

double fps;
double realfps;
bool showDebugInfo = true;
coord_3d_dbl pcoord;
coord_3d pvelocity;
int pblockx, pblocky, pblockz;

bool debug_wireframe = false;
bool debug_nocavecull = false;

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
    memcpy(chunks->data[coff], data, 131072 * sizeof(struct blockdata));
    chunks->renddata[coff].generated = true;
    chunks->renddata[coff].requested = false;
    updateChunk(x, z, CHUNKUPDATE_PRIO_LOW, 1);
    pthread_mutex_unlock(&chunks->lock);
}

static force_inline void reqChunk(struct chunkdata* chunks, int64_t x, int64_t z) {
    uint32_t coff = (z + chunks->info.dist) * chunks->info.width + (x + chunks->info.dist);
    if (!chunks->renddata[coff].generated && !chunks->renddata[coff].requested) {
        //printf("REQ [%"PRId64", %"PRId64"]\n", (int64_t)((int64_t)(x) + xo), (int64_t)((int64_t)(-z) + zo));
        //chunks->renddata[coff].requested = true; //TODO: figure out why this doesn't unset
        cliSend(CLIENT_GETCHUNK, (int64_t)((int64_t)(x) + chunks->xoff), (int64_t)((int64_t)(-z) + chunks->zoff));
    }
}

static force_inline void reqChunks(struct chunkdata* chunks) {
    pthread_mutex_lock(&chunks->lock);
    reqChunk(chunks, 0, 0);
    for (int i = 1; i <= (int)chunks->info.dist; ++i) {
        reqChunk(chunks, i, 0);
        reqChunk(chunks, -i, 0);
        reqChunk(chunks, 0, i);
        reqChunk(chunks, 0, -i);
        for (int j = 1; j < i; ++j) {
            reqChunk(chunks, -j, i);
            reqChunk(chunks, j, i);
            reqChunk(chunks, j, -i);
            reqChunk(chunks, -j, -i);
            reqChunk(chunks, -i, -j);
            reqChunk(chunks, -i, j);
            reqChunk(chunks, i, j);
            reqChunk(chunks, i, -j);
        }
        reqChunk(chunks, -i, i);
        reqChunk(chunks, i, -i);
        reqChunk(chunks, -i, -i);
        reqChunk(chunks, i, i);
    }
    pthread_mutex_unlock(&chunks->lock);
}

static force_inline void getBlockF(struct chunkdata* chunks, int64_t xoff, int64_t zoff, float x, float y, float z, struct blockdata* b) {
    x -= (x < 0) ? 1.0 : 0.0;
    y -= (y < 0) ? 1.0 : 0.0;
    z -= (z < 0) ? 1.0 : 0.0;
    getBlock(chunks, (int64_t)x + xoff * 16, y, (int64_t)z + zoff * 16, b);
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

static pthread_mutex_t gfxlock = PTHREAD_MUTEX_INITIALIZER;
static bool ping = false;
static int compat = 0;
static bool setskycolor = false;
static color newskycolor;
static bool setnatcolor = false;
static color newnatcolor;

static void handleServer(int msg, void* _data) {
    //printf("Recieved [%d] from server\n", msg);
    switch (msg) {
        case SERVER_PONG:; {
            printf("Server ponged\n");
            ping = true;
        } break;
        case SERVER_COMPATINFO:; {
            struct server_data_compatinfo* data = _data;
            printf("Server version is %s %d.%d.%d\n", data->server_str, data->ver_major, data->ver_minor, data->ver_patch);
            if (data->flags & SERVER_FLAG_NOAUTH) puts("- No authentication required");
            if (data->flags & SERVER_FLAG_PASSWD) puts("- Password protected");
            compat = (!strcasecmp(data->server_str, PROG_NAME) && data->ver_major == VER_MAJOR && data->ver_minor == VER_MINOR && data->ver_patch == VER_PATCH) ? 1 : -1;
        } break;
        case SERVER_UPDATECHUNK:; {
            struct server_data_updatechunk* data = _data;
            writeChunk(rendinf.chunks, data->x, data->z, data->data);
        } break;
        case SERVER_SETSKYCOLOR:; {
            struct server_data_setskycolor* data = _data;
            //printf("set sky color to [#%02x%02x%02x]\n", data->r, data->g, data->b);
            pthread_mutex_lock(&gfxlock);
            newskycolor = (color){(float)data->r / 255.0, (float)data->g / 255.0, (float)data->b / 255.0, 1.0};
            setskycolor = true;
            pthread_mutex_unlock(&gfxlock);
        } break;
        case SERVER_SETNATCOLOR:; {
            struct server_data_setnatcolor* data = _data;
            //printf("set natural light to [#%02x%02x%02x]\n", data->r, data->g, data->b);
            pthread_mutex_lock(&gfxlock);
            newnatcolor = (color){(float)data->r / 255.0, (float)data->g / 255.0, (float)data->b / 255.0, 1.0};
            setnatcolor = true;
            pthread_mutex_unlock(&gfxlock);
        } break;
        case SERVER_SETBLOCK:; {
            struct server_data_setblock* data = _data;
            int64_t ucx, ucz;
            getChunkOfBlock(data->x, data->z, &ucx, &ucz);
            //printf("set block at [%"PRId64", %d, %"PRId64"] ([%"PRId64", %"PRId64"]) to [%d]\n", data->x, data->y, data->z, ucx, ucz, data->data.id);
            setBlock(rendinf.chunks, data->x, data->y, data->z, data->data);
            updateChunk(ucx, ucz, CHUNKUPDATE_PRIO_HIGH, 1);
        } break;
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
    int viewdist = atoi(getConfigKey(config, "Game", "viewDist"));
    rendinf.chunks = allocChunks(viewdist);
    if (rendinf.fps || rendinf.vsync) loopdelay = atoi(getConfigKey(config, "Game", "loopDelay"));
    printf("Allocated chunks: [%d] [%d] [%d]\n", rendinf.chunks->info.dist, rendinf.chunks->info.width, rendinf.chunks->info.widthsq);
    rendinf.campos.y = 201.5;
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
        emscripten_sleep(0);
        getInput(NULL);
        microwait(100000);
    }
    if (quitRequest) return false;
    puts("Server responded to ping");
    puts("Exchanging compatibility info...");
    cliSend(CLIENT_COMPATINFO, VER_MAJOR, VER_MINOR, VER_PATCH, 0, PROG_NAME);
    while (!compat && !quitRequest) {
        emscripten_sleep(0);
        getInput(NULL);
        microwait(100000);
    }
    if (compat < 0) {
        fputs("Server version mismatch\n", stderr);
        return false;
    }
    if (quitRequest) return false;
    reqChunks(rendinf.chunks);

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
    setSkyColor(0.5, 0.5, 0.5);
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
        float bps = 25;
        getInput(&input);
        {
            if (!input.focus && inputMode != INPUT_MODE_UI) {
                setInputMode(INPUT_MODE_UI);
                resetInput();
            }

            #if defined(USESDL2)
            #else
            static int debugkey = 0;
            if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_DEBUG)) {
                //input = INPUT_EMPTY_INFO;
                input.mov_mult = 0;
                input.mov_up = 0;
                input.mov_right = 0;
                input.mov_bal = 0;
                input.single_action = INPUT_ACTION_SINGLE__NONE;
                input.multi_actions = INPUT_ACTION_MULTI__NONE;
                if (!debugkey) {
                    if (glfwGetKey(rendinf.window, (debugkey = GLFW_KEY_H)) == GLFW_PRESS) {
                        puts("DEBUG HELP:");
                        puts("Hold the multi.debug key (F4 by default) and press one of the following:");
                        puts("H - Display the debug help");
                        puts("W - Toggle wireframe mode");
                        puts("M - Remesh chunks");
                        puts("R - Reload chunks");
                        puts("- - Decrease view distance");
                        puts("= - Increase view distance");
                        puts("C - Toggle disabling cave culling");
                    } else if (glfwGetKey(rendinf.window, (debugkey = GLFW_KEY_W)) == GLFW_PRESS) {
                        printf("DEBUG: Wireframe: [%d]\n", (debug_wireframe = !debug_wireframe));
                    } else if (glfwGetKey(rendinf.window, (debugkey = GLFW_KEY_M)) == GLFW_PRESS) {
                        printf("DEBUG: Remeshing chunks...\n");
                        int64_t xo = rendinf.chunks->xoff;
                        int64_t zo = rendinf.chunks->zoff;
                        for (int i = 1; i <= (int)rendinf.chunks->info.dist; ++i) {
                            updateChunk(xo + i, zo, CHUNKUPDATE_PRIO_HIGH, 0);
                            updateChunk(xo - i, zo, CHUNKUPDATE_PRIO_HIGH, 0);
                            updateChunk(xo, zo + i, CHUNKUPDATE_PRIO_HIGH, 0);
                            updateChunk(xo, zo - i, CHUNKUPDATE_PRIO_HIGH, 0);
                            for (int j = 1; j < i; ++j) {
                                updateChunk(xo - j, zo + i, CHUNKUPDATE_PRIO_HIGH, 0);
                                updateChunk(xo + j, zo + i, CHUNKUPDATE_PRIO_HIGH, 0);
                                updateChunk(xo + j, zo - i, CHUNKUPDATE_PRIO_HIGH, 0);
                                updateChunk(xo - j, zo - i, CHUNKUPDATE_PRIO_HIGH, 0);
                                updateChunk(xo - i, zo - j, CHUNKUPDATE_PRIO_HIGH, 0);
                                updateChunk(xo - i, zo + j, CHUNKUPDATE_PRIO_HIGH, 0);
                                updateChunk(xo + i, zo + j, CHUNKUPDATE_PRIO_HIGH, 0);
                                updateChunk(xo + i, zo - j, CHUNKUPDATE_PRIO_HIGH, 0);
                            }
                            updateChunk(xo - i, zo + i, CHUNKUPDATE_PRIO_HIGH, 0);
                            updateChunk(xo + i, zo - i, CHUNKUPDATE_PRIO_HIGH, 0);
                            updateChunk(xo - i, zo - i, CHUNKUPDATE_PRIO_HIGH, 0);
                            updateChunk(xo + i, zo + i, CHUNKUPDATE_PRIO_HIGH, 0);
                        }
                    } else if (glfwGetKey(rendinf.window, (debugkey = GLFW_KEY_R)) == GLFW_PRESS) {
                        printf("DEBUG: Reloading chunks...\n");
                        resizeChunks(rendinf.chunks, viewdist);
                        reqChunks(rendinf.chunks);
                    } else if (glfwGetKey(rendinf.window, (debugkey = GLFW_KEY_MINUS)) == GLFW_PRESS && (viewdist > 1)) {
                        printf("DEBUG: View distance: [%d]\n", (--viewdist));
                        resizeChunks(rendinf.chunks, viewdist);
                        reqChunks(rendinf.chunks);
                    } else if (glfwGetKey(rendinf.window, (debugkey = GLFW_KEY_EQUAL)) == GLFW_PRESS) {
                        printf("DEBUG: View distance: [%d]\n", (++viewdist));
                        resizeChunks(rendinf.chunks, viewdist);
                        reqChunks(rendinf.chunks);
                    } else if (glfwGetKey(rendinf.window, (debugkey = GLFW_KEY_C)) == GLFW_PRESS) {
                        printf("DEBUG: Disable cave culling: [%d]\n", (debug_nocavecull = !debug_nocavecull));
                    } else {
                        debugkey = 0;
                    }
                } else if (!(glfwGetKey(rendinf.window, debugkey) == GLFW_PRESS)) {
                    debugkey = 0;
                }
            }
            #endif

            switch (input.single_action) {
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
                        case INPUT_ACTION_SINGLE_DEBUG:;
                            game_ui[UILAYER_DBGINF]->hidden = !(showDebugInfo = !showDebugInfo);
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
        float zoomrotmult;
        if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_ZOOM)) {
            zoomrotmult = 0.25;
        } else {
            zoomrotmult = 1.0;
        }
        float runmult = 1.0;
        bool crouch = false;
        if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_CROUCH)) {
            crouch = true;
            //bps *= 0.375;
        } /*else*/ if ((input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_RUN)) && input.mov_up > 0.0) {
            runmult = 1.6875;
        }
        float leanmult = ((bps < 10.0) ? bps : 10.0) * 0.125 * 1.0;
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
        if (cmx || cmz) {
            moveChunks(rendinf.chunks, cmx, cmz);
            reqChunks(rendinf.chunks);
            //microwait(1000000);
        }
        float f1 = input.mov_up * runmult * sinf(yrotrad);
        float f2 = input.mov_right * cosf(yrotrad);
        float f3 = input.mov_up * runmult * cosf(yrotrad);
        float f4 = (input.mov_right * sinf(yrotrad)) * -1;
        xcm = (f1 * bps) + (f2 * bps);
        zcm = (f3 * bps) + (f4 * bps);
        float yvel = 0.0;
        if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_JUMP)) {
            yvel += 2.0 * (1.0 + (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_RUN)) * 0.0175);
        }
        if (crouch) {
            yvel -= 2.0 * (1.0 + (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_RUN)) * 0.0175);
        }
        pvelocity.y = yvel;
        rendinf.campos.y += yvel * pmult;
        //if (rendinf.campos.y < -11.5) rendinf.campos.y = -11.5;
        if ((!rendinf.vsync && !rendinf.fps) || !rendinf.fps || (altutime() - fpsstarttime2) >= (1000000 / rendinf.fps) - loopdelay) {
            if (rendinf.fps) {
                uint64_t mwdtime = (1000000 / rendinf.fps) - (altutime() - fpsstarttime2);
                if (mwdtime < (1000000 / rendinf.fps)) microwait(mwdtime);
            }
            //puts("render");
            double tmp = (double)(altutime() - fpsstarttime2);
            fpsstarttime2 = altutime();
            //if (crouch) rendinf.campos.y -= 0.375;
            float oldfov = rendinf.camfov;
            bool zoom = (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_ZOOM));
            bool run = (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_RUN));
            if (zoom) rendinf.camfov = 12.5;
            else if (run) rendinf.camfov += ((input.mov_up > 0.0) ? input.mov_up : 0.0) * 1.25;
            struct blockdata curbdata;
            getBlockF(rendinf.chunks, rendinf.chunks->xoff, rendinf.chunks->zoff, rendinf.campos.x, rendinf.campos.y, rendinf.campos.z, &curbdata);
            if (false && curbdata.id == 7) {
                setVisibility(-0.75, 0.5);
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
            if (setnatcolor) {
                setNatColor(newnatcolor.r, newnatcolor.g, newnatcolor.b);
                setnatcolor = false;
            }
            pthread_mutex_unlock(&gfxlock);
            updateCam();
            if (zoom || run) rendinf.camfov = oldfov;
            updateChunks();
            //if (crouch) rendinf.campos.y += 0.375;
            render();
            updateScreen();
            fpstime += tmp;
            if (tmp > lowframe) lowframe = tmp;
            ++fpsct;
            uint64_t curtime = altutime();
            if (curtime - fpsstarttime >= 200000) {
                fps = 1000000.0 / (double)((fpstime / (double)fpsct));
                realfps = 1000000.0 / (double)lowframe;
                fpsstarttime = curtime;
                fpsct = 0;
                fpstime = 0;
                lowframe = 1000000.0 / (double)rendinf.disphz;
            }
        }
        pcoord.x = rendinf.campos.x + rendinf.chunks->xoff * 16;
        pcoord.y = rendinf.campos.y;
        pcoord.z = rendinf.campos.z + rendinf.chunks->zoff * 16;
        coord_3d_dbl bcoord = w2bCoord(pcoord);
        pblockx = bcoord.x;
        pblocky = bcoord.y;
        pblockz = bcoord.z;
        fpsmult = (double)((uint64_t)altutime() - (uint64_t)st1) / 1000000.0;
        pmult = posmult * fpsmult;
        emscripten_sleep(0);
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
