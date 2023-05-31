#if defined(MODULE_GAME)

#include <main/main.h>
#include "game.h"
#include "input.h"
#include <main/version.h>
#include <common/chunk.h>
#include <common/blocks.h>
#include <common/common.h>
#include <common/resource.h>
#include <graphics/renderer.h>
#include <graphics/ui.h>
#include <server/server.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include <string.h>

#include <common/glue.h>

double fps;
double realfps;
#if DBGLVL(0)
    bool showDebugInfo = true;
#else
    bool showDebugInfo = false;
#endif
coord_3d_dbl pcoord;
coord_3d pvelocity;
int pblockx, pblocky, pblockz;

bool debug_wireframe = false;
bool debug_nocavecull = false;

struct ui_layer* game_ui[4];

static inline void writeChunk(struct chunkdata* chunks, int64_t x, int64_t z, struct blockdata* data) {
    pthread_mutex_lock(&chunks->lock);
    int64_t nx = (x - chunks->xoff) + chunks->info.dist;
    int64_t nz = chunks->info.width - ((z - chunks->zoff) + chunks->info.dist) - 1;
    if (nx < 0 || nz < 0 || nx >= chunks->info.width || nz >= chunks->info.width) {
        pthread_mutex_unlock(&chunks->lock);
        return;
    }
    uint32_t coff = nx + nz * chunks->info.width;
    int top = findChunkDataTop(data);
    resizeChunkTo(chunks, coff, top);
    //printf("writing chunk to [%"PRId64", %"PRId64"] ([%"PRId64", %"PRId64"])\n", nx, nz, x, z);
    memcpy(chunks->data[coff], data, (chunks->metadata[coff].top + 1) * 256 * sizeof(struct blockdata));
    chunks->renddata[coff].generated = true;
    chunks->renddata[coff].requested = false;
    pthread_mutex_unlock(&chunks->lock);
    updateChunk(x, z, CHUNKUPDATE_PRIO_NORMAL, 1);
}

static inline void reqChunk(struct chunkdata* chunks, int64_t x, int64_t z) {
    uint32_t coff = (z + chunks->info.dist) * chunks->info.width + (x + chunks->info.dist);
    if (!chunks->renddata[coff].generated && !chunks->renddata[coff].requested) {
        //printf("REQ [%"PRId64", %"PRId64"]\n", (int64_t)((int64_t)(x) + chunks->xoff), (int64_t)((int64_t)(-z) + chunks->zoff));
        chunks->renddata[coff].requested = true;
        cliSend(CLIENT_GETCHUNK, (int64_t)((int64_t)(x) + chunks->xoff), (int64_t)((int64_t)(-z) + chunks->zoff));
    }
}

static inline void reqChunks(struct chunkdata* chunks) {
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
    in.x = (int64_t)floor(in.x);
    in.y = (int64_t)floor(in.y);
    in.z = (int64_t)floor(in.z);
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
    //printf("updateHotbar: hb=%d, slot=%d\n", hb, slot);
    editUIElem(game_ui[UILAYER_CLIENT], hb,
        UI_ATTR_HOTBAR_SLOT, slot, UI_END);
}

static pthread_mutex_t gfxlock = PTHREAD_MUTEX_INITIALIZER;
static bool setskycolor = false;
static color newskycolor;
static bool setnatcolor = false;
static color newnatcolor;

static bool handleServer(int msg, void* _data) {
    //printf("Recieved [%d] from server\n", msg);
    switch (msg) {
        /*
        case SERVER_PONG:; {
            //printf("Server ponged\n");
            ping = true;
        } break;
        */
        case SERVER_UPDATECHUNK:; {
            struct server_data_updatechunk* data = _data;
            writeChunk(rendinf.chunks, data->x, data->z, data->bdata);
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
        case SERVER_DISCONNECT:; {
            struct server_data_disconnect* data = _data;
            printf("Disconnected: %s\n", data->reason);
            return false;
        } break;
        default:; {
            printf("Unhandled message from server: [%d]\n", msg);
        } break;
    }
    return true;
}

static bool waitwithvsync;

static uint64_t fpsupdate;
static uint64_t frametime;
static int frames = 0;
static inline void doRender() {
    render();
    updateScreen();
    ++frames;
    if (rendinf.fps && (!rendinf.vsync || (waitwithvsync || rendinf.fps < rendinf.disphz))) {
        int64_t framediff = (1000000 / rendinf.fps) - (altutime() - frametime);
        //printf("Wait for %"PRId64"us\n", framediff);
        if (framediff > 0) microwait(framediff);
    }
    static uint64_t totalframetime = 0;
    static uint64_t highframetime = 0;
    frametime = altutime() - frametime;
    totalframetime += frametime;
    if (frametime > highframetime) highframetime = frametime;
    if (altutime() - fpsupdate >= 200000) {
        fpsupdate = altutime();
        fps = 1000000.0 / ((double)totalframetime / (double)frames);
        realfps = 1000000.0 / (double)highframetime;
        if (realfps > rendinf.disphz) realfps = rendinf.disphz;
        //printf("Rendered %d frames in %lfus\n", frames, ((double)totalframetime / (double)frames));
        frames = 0;
        totalframetime = 0;
        highframetime = 0;
    }
}

static inline void commonEvents(struct input_info* input) {
    for (int i = 3; i >= 0; --i) {
        struct ui_doeventinfo info = {.init = false};
        doUIEvents(game_ui[i], input, &info);
    }
    switch (input->single_action) {
        case INPUT_ACTION_SINGLE_FULLSCR:;
            setFullscreen(!rendinf.fullscr);
            break;
        case INPUT_ACTION_SINGLE_DEBUG:;
            game_ui[UILAYER_DBGINF]->hidden = showDebugInfo;
            showDebugInfo = !showDebugInfo;
            break;
    }
}

static void gameLoop_hud_callback(struct ui_event* event) {
    printf("Event %d on layer \"%s\", elem \"%s\" (%d)\n",
        event->event, event->layer->name, event->elem->attribs.name, event->elemid);
}
static void gameLoop() {
    struct input_info input;
    resetInput();
    setInputMode(INPUT_MODE_GAME);
    getInput(&input);

    startMesher();
    setRandSeed(8, altutime());

    game_ui[UILAYER_INGAME]->hidden = true;
    rendergame = true;

    rendinf.camrot.x = 0.0;
    rendinf.camrot.y = 0.0;
    rendinf.camrot.z = 0.0;
    rendinf.campos.x = 0.0;
    rendinf.campos.y = 0.0;
    rendinf.campos.z = 0.0;

    int invspot = 0;
    int invoff = 0;

    float bps = 25;

    coord_3d tmpcamrot = rendinf.camrot;

    int viewdist = atoi(getConfigKey(config, "Game", "viewDist"));
    rendinf.chunks = allocChunks(viewdist);
    rendinf.chunks->xoff = 25;
    rendinf.chunks->zoff = 24;
    //rendinf.chunks->zoff = rendinf.chunks->xoff = ((int64_t)1 << 30) + (int64_t)324359506;
    //rendinf.chunks->zoff = rendinf.chunks->xoff = ((int64_t)1 << 31) + (int64_t)648719012;
    reqChunks(rendinf.chunks);
    printf("Allocated chunks: [%d] [%d] [%d]\n", rendinf.chunks->info.dist, rendinf.chunks->info.width, rendinf.chunks->info.widthsq);
    rendinf.campos.y = 201.5;
    setVisibility(0.5, 1.0);
    setScreenMult(1.0, 1.0, 1.0);

    int ui_hud = newUIElem(game_ui[UILAYER_CLIENT], UI_ELEM_CONTAINER, -1,
        UI_ATTR_NAME, "hud", UI_ATTR_SIZE, "100%", "100%", UI_ATTR_CALLBACK, gameLoop_hud_callback, UI_END);

    int ui_hotbar = newUIElem(game_ui[UILAYER_CLIENT], UI_ELEM_HOTBAR, ui_hud,
        UI_ATTR_NAME, "hotbar", UI_ATTR_ALIGN, 0, 1, UI_ATTR_MARGIN, "10", "10", "0", "0", UI_ATTR_CALLBACK, gameLoop_hud_callback, UI_END);
    updateHotbar(ui_hotbar, invspot);

    while (!quitRequest) {
        frametime = altutime();
        getInput(&input);
        if (!input.focus && inputMode != INPUT_MODE_UI) {
            game_ui[UILAYER_INGAME]->hidden = false;
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
                    puts("h - Display the debug help");
                    puts("w - Toggle wireframe mode");
                    puts("m - Remesh chunks");
                    puts("r - Reload chunks");
                    puts("R - Reload renderer");
                    puts("- - Decrease view distance");
                    puts("= - Increase view distance");
                    puts("c - Toggle disabling cave culling");
                } else if (glfwGetKey(rendinf.window, (debugkey = GLFW_KEY_W)) == GLFW_PRESS) {
                    printf("DEBUG: Wireframe: [%d]\n", (debug_wireframe = !debug_wireframe));
                } else if (glfwGetKey(rendinf.window, (debugkey = GLFW_KEY_M)) == GLFW_PRESS) {
                    printf("DEBUG: Remeshing chunks...\n");
                    int64_t xo = rendinf.chunks->xoff;
                    int64_t zo = rendinf.chunks->zoff;
                    updateChunk(xo, zo, CHUNKUPDATE_PRIO_HIGH, 0);
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
                    if (glfwGetKey(rendinf.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
                        printf("DEBUG: Reloading renderer...\n");
                        reloadRenderer();
                    } else {
                        printf("DEBUG: Reloading chunks...\n");
                        resizeChunks(rendinf.chunks, viewdist);
                        reqChunks(rendinf.chunks);
                    }
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

        commonEvents(&input);
        switch (inputMode) {
            case INPUT_MODE_GAME:; {
                switch (input.single_action) {
                    case INPUT_ACTION_SINGLE_INV_0 ... INPUT_ACTION_SINGLE_INV_9:;
                        invspot = input.single_action - INPUT_ACTION_SINGLE_INV_0;
                        updateHotbar(ui_hotbar, invspot);
                        //blocksub = 0;
                        break;
                    case INPUT_ACTION_SINGLE_INV_NEXT:;
                        ++invspot;
                        if (invspot > 9) invspot = 0;
                        updateHotbar(ui_hotbar, invspot);
                        //blocksub = 0;
                        break;
                    case INPUT_ACTION_SINGLE_INV_PREV:;
                        --invspot;
                        if (invspot < 0) invspot = 9;
                        updateHotbar(ui_hotbar, invspot);
                        //blocksub = 0;
                        break;
                    case INPUT_ACTION_SINGLE_INVOFF_NEXT:;
                        ++invoff;
                        if (invoff > 4) invoff = 0;
                        //blocksub = 0;
                        break;
                    case INPUT_ACTION_SINGLE_INVOFF_PREV:;
                        --invoff;
                        if (invoff < 0) invoff = 4;
                        //blocksub = 0;
                        break;
                    case INPUT_ACTION_SINGLE_ROT_X:;
                        //++blocksub;
                        break;
                    case INPUT_ACTION_SINGLE_ROT_Y:;
                        //--blocksub;
                        //if (blocksub < 0) blocksub = 0;
                        break;
                    case INPUT_ACTION_SINGLE_ESC:;
                        game_ui[UILAYER_INGAME]->hidden = false;
                        setInputMode(INPUT_MODE_UI);
                        resetInput();
                        break;
                }
                break;
            }
            case INPUT_MODE_UI:; {
                switch (input.single_action) {
                    case INPUT_ACTION_SINGLE_ESC:;
                        game_ui[UILAYER_INGAME]->hidden = true;
                        setInputMode(INPUT_MODE_GAME);
                        resetInput();
                        break;
                }
                break;
            }
        }
        float speed = bps;
        float runmult = (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_RUN)) ? 1.6875 : 1.0;
        float leanmult = ((speed < 10.0) ? speed : 10.0) * 0.125 * 1.0;
        float zoomrotmult;
        if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_ZOOM)) {
            zoomrotmult = 0.5;
        } else {
            zoomrotmult = 1.0;
        }
        tmpcamrot.x += input.rot_up * input.rot_mult_y * zoomrotmult;
        tmpcamrot.y -= input.rot_right * input.rot_mult_x * zoomrotmult;
        rendinf.camrot.x = tmpcamrot.x - input.mov_up * leanmult;
        if (rendinf.camrot.x > 90.0) rendinf.camrot.x = 90.0;
        if (rendinf.camrot.x < -90.0) rendinf.camrot.x = -90.0;
        rendinf.camrot.y = tmpcamrot.y;
        rendinf.camrot.z = input.mov_right * leanmult;
        tmpcamrot.y = fmod(tmpcamrot.y, 360.0);
        if (tmpcamrot.y < 0) tmpcamrot.y += 360;
        if (tmpcamrot.x > 90.0) tmpcamrot.x = 90.0;
        if (tmpcamrot.x < -90.0) tmpcamrot.x = -90.0;
        float yrotrad = (rendinf.camrot.y / 180 * M_PI);
        float f1 = input.mov_up * ((input.mov_up > 0.0) ? runmult : 1.0) * sinf(yrotrad);
        float f2 = input.mov_right * cosf(yrotrad);
        float f3 = input.mov_up * ((input.mov_up > 0.0) ? runmult : 1.0) * cosf(yrotrad);
        float f4 = (input.mov_right * sinf(yrotrad)) * -1;
        float xcm = (f1 * speed) + (f2 * speed);
        float zcm = (f3 * speed) + (f4 * speed);
        pvelocity.x = xcm;
        pvelocity.z = zcm;
        rendinf.campos.z += zcm * input.mov_mult;
        rendinf.campos.x += xcm * input.mov_mult;
        if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_JUMP)) {
            rendinf.campos.y += 17.5 * runmult * input.mov_mult;
        }
        if (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_CROUCH)) {
            rendinf.campos.y -= 17.5 * runmult * input.mov_mult;
        }
        pcoord.x = rendinf.campos.x + (double)((int64_t)(rendinf.chunks->xoff * 16));
        pcoord.y = rendinf.campos.y;
        pcoord.z = rendinf.campos.z + (double)((int64_t)(rendinf.chunks->zoff * 16));

        int cmx = 0, cmz = 0;
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
        float oldfov = rendinf.camfov;
        bool zoom = (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_ZOOM));
        bool run = (input.multi_actions & INPUT_GETMAFLAG(INPUT_ACTION_MULTI_RUN) && input.mov_up > 0.0);
        if (zoom) rendinf.camfov = 15.0;
        else if (run) rendinf.camfov += input.mov_up * 1.25;
        updateCam();
        if (zoom || run) rendinf.camfov = oldfov;
        updateChunks();
        doRender();
        microwait(0);
    }

    rendergame = false;
    deleteUIElem(game_ui[UILAYER_CLIENT], ui_hud);

    //puts("cliDisconnect");
    cliDisconnect();
    //puts("stopMesher");
    stopMesher();
    //puts("freeChunks");
    freeChunks(rendinf.chunks);
    //rendinf.chunks = NULL;
    //puts(".done");

    game_ui[UILAYER_INGAME]->hidden = false;

    setInputMode(INPUT_MODE_UI);
}

static void showProgressBox(char* title, char* text, float progress, bool showcancel) {
}
static void hideProgressBox() {
}
static bool progressBoxCancelled() {
    return false;
}

static void dispErrorBox(char* title, char* text) {
}

static int connectToServ_shouldQuit() {
    static struct input_info input;
    getInput(&input);
    commonEvents(&input);
    doRender();
    frametime = altutime();
    return quitRequest || progressBoxCancelled();
}

static void connectToServ_setText(char* _text, float progress) {
    char text[4096] = "Connecting to server...\n";
    snprintf(text, sizeof(text), "Connecting to server: %s", _text);
    showProgressBox(NULL, text, progress, true);
}

static int connectToServ(char* addr, int port, char* error, int errlen) {
    puts("Connecting to server...");
    {
        char err[4096];
        static bool firsttime = true;
        static struct cliSetupInfo inf;
        if (firsttime) {
            memset(&inf, 0, sizeof(inf));
            firsttime = false;
        }
        inf.in.quit = connectToServ_shouldQuit;
        inf.in.settext = connectToServ_setText;
        inf.in.timeout = 5000;
        inf.in.login.new = true;
        inf.in.login.username = getConfigKey(config, "Player", "name");
        int ecode = cliConnectAndSetup((addr) ? addr : "127.0.0.1", port, handleServer, err, sizeof(err), &inf);
        if (ecode == 0) {
            fprintf(stderr, "Failed to connect to server: %s\n", err);
            snprintf(error, errlen, "%s", err);
            return 0;
        } else if (ecode == -1) {
            fputs("Connection to server cancelled by user\n", stderr);
            return -1;
        }
    }
    puts("Connected to server.");
    return 1;
}

static void stopServerWithBox() {
    showProgressBox("Stopping singleplayer game...", "Stopping server...", 0.0, false);
    stopServer();
    showProgressBox(NULL, "Stopping server...", 100.0, false);
    hideProgressBox();
}

static int startSPGame(char* error, int errlen) {
    int cores = getCoreCt();
    if (cores < 1) cores = 1;
    cores -= 4;
    if (cores < 2) cores = 2;
    SERVER_THREADS = cores / 2;
    cores -= SERVER_THREADS;
    MESHER_THREADS = cores;

    showProgressBox("Starting singleplayer game...", "Starting server...", 0.0, false);
    int servport;
    if ((servport = startServer(NULL, 0, 1, "World")) < 0) {
        fputs("Failed to start server\n", stderr);
        snprintf(error, errlen, "Failed to start server");
        hideProgressBox();
        return 0;
    }
    showProgressBox(NULL, "Connecting to server...", 0.0, true);
    if (progressBoxCancelled()) {
        hideProgressBox();
        goto usercancel;
    }
    char serverr[4096];
    int ecode = connectToServ(NULL, servport, serverr, sizeof(serverr));
    hideProgressBox();
    if (ecode == 0) {
        snprintf(error, errlen, "Failed to connect to server: %s", serverr);
        stopServerWithBox();
        return 0;
    } else if (ecode == -1) {
        goto usercancel;
    }

    gameLoop();
    usercancel:;

    stopServerWithBox();

    return 1;
}

bool doGame() {
    declareConfigKey(config, "Game", "viewDist", "8", false);
    declareConfigKey(config, "Game", "allocExactHeight", "false", false);
    declareConfigKey(config, "Player", "name", "Player", false);
    declareConfigKey(config, "Player", "skin", "", false);
    declareConfigKey(config, "Renderer", "waitWithVsync", "true", false);
    waitwithvsync = getBool(getConfigKey(config, "Renderer", "waitWithVsync"));
    allocexactchunkheight = getBool(getConfigKey(config, "Game", "allocExactHeight"));

    fpsupdate = altutime();

    if (!startRenderer()) return false;
    if (!initInput()) return false;
    setInputMode(INPUT_MODE_UI);
    rendergame = false;
    setSkyColor(0.5, 0.5, 0.5);
    setFullscreen(rendinf.fullscr);

    game_ui[UILAYER_CLIENT] = allocUI("client");
    game_ui[UILAYER_SERVER] = allocUI("server");
    game_ui[UILAYER_DBGINF] = allocUI("debug info");
    game_ui[UILAYER_INGAME] = allocUI("in-game menu");

    game_ui[UILAYER_DBGINF]->hidden = !showDebugInfo;
    game_ui[UILAYER_INGAME]->hidden = false;

    char error[4096];
    startSPGame(error, sizeof(error));

    struct input_info input;
    while (!quitRequest) {
        frametime = altutime();
        getInput(&input);
        commonEvents(&input);
        doRender();
        microwait(0);
    }

    freeUI(game_ui[UILAYER_CLIENT]);
    freeUI(game_ui[UILAYER_SERVER]);
    freeUI(game_ui[UILAYER_DBGINF]);
    freeUI(game_ui[UILAYER_INGAME]);

    stopRenderer();

    return true;
}

#endif
