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

struct ui_data* game_ui[4];

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
    pthread_mutex_unlock(&chunks->lock);
    updateChunk(x, z, CHUNKUPDATE_PRIO_NORMAL, 1);
}

static force_inline void reqChunk(struct chunkdata* chunks, int64_t x, int64_t z) {
    uint32_t coff = (z + chunks->info.dist) * chunks->info.width + (x + chunks->info.dist);
    if (!chunks->renddata[coff].generated && !chunks->renddata[coff].requested) {
        //printf("REQ [%"PRId64", %"PRId64"]\n", (int64_t)((int64_t)(x) + chunks->xoff), (int64_t)((int64_t)(-z) + chunks->zoff));
        chunks->renddata[coff].requested = true;
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
    char hbslot[2] = {slot + '0', 0};
    editUIElem(game_ui[UILAYER_CLIENT], hb, UI_ATTR_DONE, "slot", hbslot, NULL);
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
static void doRender() {
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

static void gameLoop() {
    struct input_info input;
    resetInput();
    setInputMode(INPUT_MODE_GAME);
    getInput(&input);

    startMesher();
    setRandSeed(8, altutime());

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
    //rendinf.chunks->xoff = 2354;
    //rendinf.chunks->zoff = 8523;
    reqChunks(rendinf.chunks);
    printf("Allocated chunks: [%d] [%d] [%d]\n", rendinf.chunks->info.dist, rendinf.chunks->info.width, rendinf.chunks->info.widthsq);
    rendinf.campos.y = 201.5;
    setVisibility(0.5, 1.0);
    setScreenMult(1.0, 1.0, 1.0);

    int ui_main = newUIElem(game_ui[UILAYER_INGAME], UI_ELEM_BOX,
        UI_ATTR_NAME, "main", UI_ATTR_DONE,
        "width", "100%", "height", "100%", "color", "#000000", "alpha", "0.25", "z", "-100", NULL);
    int ui_placeholder = newUIElem(game_ui[UILAYER_INGAME], UI_ELEM_FANCYBOX,
        UI_ATTR_NAME, "placeholder", UI_ATTR_PARENT, ui_main, UI_ATTR_DONE,
        "width", "128", "height", "36", "text", "[Placeholder]", NULL);

    int ui_hotbar = newUIElem(game_ui[UILAYER_CLIENT], UI_ELEM_HOTBAR,
        UI_ATTR_NAME, "hotbar", UI_ATTR_DONE,
        "align", "0,1", "margin", "0,10,0,10", NULL);
    updateHotbar(ui_hotbar, invspot);

    #if 0
    int ui_inv_main = newUIElem(game_ui[UILAYER_SERVER], UI_ELEM_BOX, "main", -1, -1, "width", "100%", "height", "100%", "color", "#000000", "alpha", "0.25", "z", "-100", NULL);
    int ui_inventory = newUIElem(game_ui[UILAYER_SERVER], UI_ELEM_FANCYBOX, "inventory", ui_inv_main, -1, "width", "332", "height", "360", NULL);
    int ui_inv_grid = newUIElem(game_ui[UILAYER_SERVER], UI_ELEM_ITEMGRID, "inv_grid", ui_inventory, -1, "width", "10", "height", "4", "align", "0,1", "margin", "0,6,0,16", NULL);
    /*int ui_inv_hb = */newUIElem(game_ui[UILAYER_SERVER], UI_ELEM_ITEMGRID, "inv_hotbar", ui_inventory, ui_inv_grid, "width", "10", "height", "1", "align", "0,1", "margin", "0,6,0,6", NULL);
    #endif

    while (!quitRequest) {
        frametime = altutime();
        getInput(&input);
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
            case INPUT_ACTION_SINGLE_DEBUG:;
                game_ui[UILAYER_DBGINF]->hidden = !(showDebugInfo = !showDebugInfo);
                break;
        }
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
                //puts("START");
                for (int i = 3; i >= 0; --i) {
                    if (doUIEvents(&input, game_ui[i])) break;
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
        if (rendinf.camrot.x > 89.99) rendinf.camrot.x = 89.99;
        if (rendinf.camrot.x < -89.99) rendinf.camrot.x = -89.99;
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

    deleteUIElem(game_ui[UILAYER_INGAME], ui_main);

    deleteUIElem(game_ui[UILAYER_CLIENT], ui_hotbar);

    stopMesher();
}

static int clickedbtn = -1;

int ui_spbtn;
int ui_mpbtn;
int ui_opbtn;
int ui_qbtn;
int ui_errbtn;
int ui_okbtn;

static void btncb(struct ui_data* elemdata, int id, struct ui_elem* e, int event) {
    (void)elemdata;
    switch (event) {
        case UI_EVENT_CLICK:;
            printf("Clicked on {%s}\n", e->name);
            clickedbtn = id;
            break;
    }
}

static int ui_status;

static int shouldQuit() {
    getInput(NULL);
    render();
    updateScreen();
    return quitRequest;
}

static void setText(char* _text) {
    char text[4096] = "Connecting to server...\n";
    strcat(text, _text);
    editUIElem(game_ui[UILAYER_INGAME], ui_status, UI_ATTR_DONE, "text", text, NULL);
}

static bool connectToServ(char* addr, int port, char* error, int errlen) {
    puts("Connecting to server...");
    {
        char err[4096];
        static bool firsttime = true;
        static struct cliSetupInfo inf;
        if (firsttime) {
            memset(&inf, 0, sizeof(inf));
            firsttime = false;
        }
        inf.in.quit = shouldQuit;
        inf.in.settext = setText;
        inf.in.timeout = 1000;
        inf.in.login.new = true;
        inf.in.login.username = getConfigKey(config, "Player", "name");
        if (!cliConnectAndSetup((addr) ? addr : "127.0.0.1", port, handleServer, err, sizeof(err), &inf)) {
            fprintf(stderr, "Failed to connect to server: %s\n", err);
            snprintf(error, errlen, "%s", err);
            return false;
        }
    }
    puts("Connected to server.");
    return true;
}

static bool startSPGame(char* error, int errlen) {
    int cores = getCoreCt();
    if (cores < 1) cores = 1;
    {
        cores -= 4;
        if (cores < 2) cores = 2;
        SERVER_THREADS = cores / 2;
        cores -= SERVER_THREADS;
        MESHER_THREADS = cores;

        if (!initServer()) {
            fputs("Failed to init server\n", stderr);
            snprintf(error, errlen, "Failed to init server");
            return false;
        }

        editUIElem(game_ui[UILAYER_INGAME], ui_status, UI_ATTR_DONE, "hidden", "false", NULL);
        int servport;
        if ((servport = startServer(NULL, 0, 1, "World")) < 0) {
            fputs("Failed to start server\n", stderr);
            snprintf(error, errlen, "Failed to start server");
            editUIElem(game_ui[UILAYER_INGAME], ui_status, UI_ATTR_DONE, "hidden", "true", NULL);
            return false;
        }
        char serverr[4096];
        if (!connectToServ(NULL, servport, serverr, sizeof(serverr))) {
            snprintf(error, errlen, "Failed to connect to server: %s", serverr);
            editUIElem(game_ui[UILAYER_INGAME], ui_status, UI_ATTR_DONE, "hidden", "true", NULL);
            return false;
        }
        editUIElem(game_ui[UILAYER_INGAME], ui_status, UI_ATTR_DONE, "hidden", "true", NULL);

        game_ui[UILAYER_DBGINF]->hidden = !showDebugInfo;
        gameLoop();

        stopServer();
    }
    return true;
}

bool doGame() {
    declareConfigKey(config, "Game", "viewDist", "8", false);
    declareConfigKey(config, "Player", "name", "Player", false);
    declareConfigKey(config, "Player", "skin", "", false);
    declareConfigKey(config, "Renderer", "waitWithVsync", "true", false);
    waitwithvsync = getBool(getConfigKey(config, "Renderer", "waitWithVsync"));

    fpsupdate = altutime();

    initRenderer();
    startRenderer();
    initInput();
    setInputMode(INPUT_MODE_UI);
    rendergame = false;
    setSkyColor(0.5, 0.5, 0.5);
    setFullscreen(rendinf.fullscr);

    for (int i = 0; i < 4; ++i) {
        game_ui[i] = allocUI();
    }
    game_ui[UILAYER_DBGINF]->hidden = !showDebugInfo;
    game_ui[UILAYER_INGAME]->hidden = false;

    int ui_main_menu = newUIElem(game_ui[UILAYER_INGAME], UI_ELEM_BOX,
        UI_ATTR_NAME, "main_menu", UI_ATTR_DONE,
        "width", "100%", "height", "100%", "color", "#000000", "alpha", "0.0", "z", "-76", NULL);

    ui_spbtn = newUIElem(game_ui[UILAYER_INGAME], UI_ELEM_BUTTON,
        UI_ATTR_NAME, "spbtn", UI_ATTR_PARENT, ui_main_menu, UI_ATTR_CALLBACK, btncb, UI_ATTR_DONE,
        "width", "320", "height", "32", "x_offset", "-18", "y_offset", "-44", "text", "Singleplayer", NULL);
    ui_mpbtn = newUIElem(game_ui[UILAYER_INGAME], UI_ELEM_BUTTON,
        UI_ATTR_NAME, "mpbtn", UI_ATTR_PARENT, ui_main_menu, UI_ATTR_CALLBACK, btncb, UI_ATTR_DONE,
        "width", "320", "height", "32", "x_offset", "-6", "y_offset", "0", "text", "Multiplayer", NULL);
    ui_opbtn = newUIElem(game_ui[UILAYER_INGAME], UI_ELEM_BUTTON,
        UI_ATTR_NAME, "opbtn", UI_ATTR_PARENT, ui_main_menu, UI_ATTR_CALLBACK, btncb, UI_ATTR_DONE,
        "width", "320", "height", "32", "x_offset", "6", "y_offset", "44", "text", "Options", NULL);
    ui_qbtn = newUIElem(game_ui[UILAYER_INGAME], UI_ELEM_BUTTON,
        UI_ATTR_NAME, "qbtn", UI_ATTR_PARENT, ui_main_menu, UI_ATTR_CALLBACK, btncb, UI_ATTR_DONE,
        "width", "320", "height", "32", "x_offset", "18", "y_offset", "88", "text", "Quit", NULL);
    int ui_logo = newUIElem(game_ui[UILAYER_INGAME], UI_ELEM_CONTAINER,
        UI_ATTR_NAME, "logo", UI_ATTR_PARENT, ui_main_menu, UI_ATTR_DONE,
        "width", "448", "height", "112", "x_offset", "3", "y_offset", "-160", "text", PROG_NAME, "text_scale", "7", "z", "100", NULL);
    /*int ui_version =*/ newUIElem(game_ui[UILAYER_INGAME], UI_ELEM_CONTAINER,
        UI_ATTR_NAME, "version", UI_ATTR_PARENT, ui_main_menu, UI_ATTR_PREV, ui_logo, UI_ATTR_DONE,
        "width", "448", "height", "16", "x_offset", "-3", "y_offset", "-16", "text", "Version "VER_STR, "align", "0,-1", NULL);

    ui_status = newUIElem(game_ui[UILAYER_INGAME], UI_ELEM_FANCYBOX,
        UI_ATTR_NAME, "status", UI_ATTR_DONE,
        "width", "50%", "height", "160", "text_margin", "8,8", "text", "", "hidden", "true", NULL);
    ui_okbtn = newUIElem(game_ui[UILAYER_INGAME], UI_ELEM_BUTTON,
        UI_ATTR_NAME, "okbtn", UI_ATTR_PREV, ui_status, UI_ATTR_CALLBACK, btncb, UI_ATTR_DONE,
        "width", "50%", "height", "32", "align", "0,-1", "y_offset", "-2", "text", "OK", "hidden", "true", NULL);

    {
        struct input_info input;
        while (!quitRequest) {
            frametime = altutime();
            getInput(&input);
            switch (input.single_action) {
                case INPUT_ACTION_SINGLE_FULLSCR:;
                    setFullscreen(!rendinf.fullscr);
                    break;
                case INPUT_ACTION_SINGLE_DEBUG:;
                    game_ui[UILAYER_DBGINF]->hidden = showDebugInfo;
                    showDebugInfo = !showDebugInfo;
                    break;
            }
            for (int i = 3; i >= 0; --i) {
                if (doUIEvents(&input, game_ui[i])) break;
            }
            doRender();
            microwait(0);
            if (clickedbtn >= 0) {
                game_ui[UILAYER_DBGINF]->hidden = true;
                char error[4096];
                if (clickedbtn == ui_spbtn) {
                    editUIElem(game_ui[UILAYER_INGAME], ui_main_menu, UI_ATTR_DONE, "hidden", "true", NULL);
                    if (!startSPGame(error, sizeof(error))) {
                        char text[8192];
                        snprintf(text, sizeof(text), "Failed to start singleplayer game: %s", error);
                        editUIElem(game_ui[UILAYER_INGAME], ui_status, UI_ATTR_DONE, "text", text, "hidden", "false", NULL);
                        editUIElem(game_ui[UILAYER_INGAME], ui_okbtn, UI_ATTR_DONE, "hidden", "false", NULL);
                    } else {
                        editUIElem(game_ui[UILAYER_INGAME], ui_main_menu, UI_ATTR_DONE, "hidden", "false", NULL);
                    }
                } else if (clickedbtn == ui_qbtn) {
                    goto longbreak;
                } else if (clickedbtn == ui_okbtn) {
                    editUIElem(game_ui[UILAYER_INGAME], ui_status, UI_ATTR_DONE, "text", "", "hidden", "true", NULL);
                    editUIElem(game_ui[UILAYER_INGAME], ui_okbtn, UI_ATTR_DONE, "hidden", "true", NULL);
                    editUIElem(game_ui[UILAYER_INGAME], ui_main_menu, UI_ATTR_DONE, "hidden", "false", NULL);
                }
                clickedbtn = -1;
                game_ui[UILAYER_DBGINF]->hidden = !showDebugInfo;
            }
        }
    }
    longbreak:;

    for (int i = 0; i < 4; ++i) {
        freeUI(game_ui[i]);
    }
    stopRenderer();

    return true;
}

#endif
