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
    updateChunk(x, z, CHUNKUPDATE_PRIO_NORMAL, 1);
    pthread_mutex_unlock(&chunks->lock);
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
    editUIElem(game_ui[UILAYER_CLIENT], hb, NULL, -1, "slot", hbslot, NULL);
}

static pthread_mutex_t gfxlock = PTHREAD_MUTEX_INITIALIZER;
static bool setskycolor = false;
static color newskycolor;
static bool setnatcolor = false;
static color newnatcolor;

static int shouldQuit() {
    getInput(NULL);
    return quitRequest;
}

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

void gameLoop() {
    struct input_info input;
    resetInput();
    setInputMode(INPUT_MODE_GAME);
    getInput(&input);

    startMesher();
    setRandSeed(8, altutime());

    rendinf.camrot.x = 0.0;
    rendinf.camrot.y = 0.0;
    rendinf.camrot.z = 0.0;
    rendinf.campos.x = 0.0;
    rendinf.campos.y = 0.0;
    rendinf.campos.z = 0.0;

    int invspot = 0;
    int invoff = 0;

    int viewdist = atoi(getConfigKey(config, "Game", "viewDist"));
    rendinf.chunks = allocChunks(viewdist);
    //rendinf.chunks->xoff = 230;
    //rendinf.chunks->zoff = 550;
    reqChunks(rendinf.chunks);
    printf("Allocated chunks: [%d] [%d] [%d]\n", rendinf.chunks->info.dist, rendinf.chunks->info.width, rendinf.chunks->info.widthsq);
    rendinf.campos.y = 201.5;
    setVisibility(0.25, 1.0);
    setScreenMult(1.0, 1.0, 1.0);

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

    uint64_t fpsupdate = altutime();
    int frames = 0;

    while (!quitRequest) {
        uint64_t frametime = altutime();
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
        {
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
            updateChunks();
            render();
            updateScreen();
            ++frames;
            if (rendinf.fps && (!rendinf.vsync || rendinf.fps < rendinf.disphz)) {
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
    }
}

bool doGame(char* addr, int port) {
    declareConfigKey(config, "Game", "viewDist", "8", false);
    declareConfigKey(config, "Player", "name", "Player", false);
    declareConfigKey(config, "Player", "skin", "", false);

    initInput();

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
        inf.in.timeout = 10000;
        inf.in.login.new = true;
        inf.in.login.username = getConfigKey(config, "Player", "name");
        if (!cliConnectAndSetup((addr) ? addr : "127.0.0.1", port, handleServer, err, sizeof(err), &inf)) {
            fprintf(stderr, "Failed to connect to server: %s\n", err);
            return false;
        }
    }
    puts("Connected to server.");
    if (quitRequest) return false;

    for (int i = 0; i < 4; ++i) {
        game_ui[i] = allocUI();
    }
    game_ui[UILAYER_DBGINF]->hidden = !showDebugInfo;
    //game_ui[UILAYER_INGAME]->hidden = input.focus;

    setSkyColor(0.5, 0.5, 0.5);
    setFullscreen(rendinf.fullscr);

    gameLoop();

    for (int i = 0; i < 4; ++i) {
        freeUI(game_ui[i]);
    }
    return true;
}

#endif
