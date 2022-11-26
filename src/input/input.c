#if MODULEID == MODULEID_GAME

#include <main/main.h>
#include "input.h" 
#include <common/common.h>
#include <renderer/renderer.h>
#include <renderer/ui.h>
#include <game/game.h>

#if defined(USESDL2)
    #include <SDL2/SDL.h>
    #define _SDL_SCROLL_UP 1
    #define _SDL_SCROLL_DOWN -1
#else
    #include <GLFW/glfw3.h>
    #define _GLFW_SCROLL_UP 1
    #define _GLFW_SCROLL_DOWN -1
#endif

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#define KEY(d1, t1, k1, d2, t2, k2) (input_keys){d1, t1, k1, d2, t2, k2}

typedef struct {
    unsigned char kd1;
    unsigned char kt1;
    int key1;
    unsigned char kd2;
    unsigned char kt2;
    int key2;
} input_keys;

char* input_mov_names[] = {
    "move.forward",
    "move.backward",
    "move.left",
    "move.right"
};
char* input_ma_names[] = {
    "multi.place",
    "multi.destroy",
    "multi.pick",
    "multi.jump",
    "multi.crouch",
    "multi.run",
    "multi.playerlist"
};
char* input_sa_names[] = {
    "single.escape",
    "single.inventory",
    "single.invSlot0",
    "single.invSlot1",
    "single.invSlot2",
    "single.invSlot3",
    "single.invSlot4",
    "single.invSlot5",
    "single.invSlot6",
    "single.invSlot7",
    "single.invSlot8",
    "single.invSlot9",
    "single.invNext",
    "single.invPrev",
    "single.invShiftUp",
    "single.invShiftDown",
    "single.rotBlockX",
    "single.rotBlockY",
    "single.rotBlockZ",
    "single.fullscreen",
    "single.debug",
    "single.leftClick",
    "single.rightClick"
};

input_keys input_mov[] = {
    #if defined(USESDL2)
    KEY('k', 'b', SDL_SCANCODE_W, 0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_S, 0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_A, 0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_D, 0, 0, 0),
    #else
    KEY('k', 'b', GLFW_KEY_W, 0, 0, 0),
    KEY('k', 'b', GLFW_KEY_S, 0, 0, 0),
    KEY('k', 'b', GLFW_KEY_A, 0, 0, 0),
    KEY('k', 'b', GLFW_KEY_D, 0, 0, 0),
    #endif
};
input_keys input_ma[INPUT_ACTION_MULTI__MAX] = {
    #if defined(USESDL2)
    KEY('m', 'b', SDL_BUTTON(1),       0, 0, 0),
    KEY('m', 'b', SDL_BUTTON(3),       0, 0, 0),
    KEY('m', 'b', SDL_BUTTON(2),       0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_SPACE,  0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_LSHIFT, 'k', 'b', SDL_SCANCODE_RSHIFT),
    KEY('k', 'b', SDL_SCANCODE_LCTRL,  'k', 'b', SDL_SCANCODE_RCTRL),
    KEY('k', 'b', SDL_SCANCODE_TAB,    0, 0, 0),
    #else
    KEY('m', 'b', GLFW_MOUSE_BUTTON_LEFT,   0, 0, 0),
    KEY('m', 'b', GLFW_MOUSE_BUTTON_RIGHT,  0, 0, 0),
    KEY('m', 'b', GLFW_MOUSE_BUTTON_MIDDLE, 0, 0, 0),
    KEY('k', 'b', GLFW_KEY_SPACE,           0, 0, 0),
    KEY('k', 'b', GLFW_KEY_LEFT_SHIFT,      'k', 'b', GLFW_KEY_RIGHT_SHIFT),
    KEY('k', 'b', GLFW_KEY_LEFT_CONTROL,    'k', 'b', GLFW_KEY_RIGHT_CONTROL),
    KEY('k', 'b', GLFW_KEY_TAB,             0, 0, 0),
    #endif
};
input_keys input_sa[INPUT_ACTION_SINGLE__MAX] = {
    #if defined(USESDL2)
    KEY('k', 'b', SDL_SCANCODE_ESCAPE,       0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_I,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_1,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_2,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_3,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_4,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_5,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_6,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_7,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_8,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_9,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_0,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_RIGHTBRACKET, 'm', 'w', _SDL_SCROLL_UP),
    KEY('k', 'b', SDL_SCANCODE_LEFTBRACKET,  'm', 'w', _SDL_SCROLL_DOWN),
    KEY('k', 'b', SDL_SCANCODE_EQUALS,       0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_MINUS,        0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_R,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_F,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_F11,          0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_F3,           0, 0, 0),
    KEY('m', 'b', SDL_BUTTON(1),             0, 0, 0),
    KEY('m', 'b', SDL_BUTTON(3),             0, 0, 0),
    #else
    KEY('k', 'b', GLFW_KEY_ESCAPE,         0, 0, 0),
    KEY('k', 'b', GLFW_KEY_I,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_1,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_2,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_3,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_4,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_5,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_6,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_7,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_8,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_9,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_0,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_RIGHT_BRACKET,  'm', 'w', _GLFW_SCROLL_UP),
    KEY('k', 'b', GLFW_KEY_LEFT_BRACKET,   'm', 'w', _GLFW_SCROLL_DOWN),
    KEY('k', 'b', GLFW_KEY_EQUAL,          0, 0, 0),
    KEY('k', 'b', GLFW_KEY_MINUS,          0, 0, 0),
    KEY('k', 'b', GLFW_KEY_R,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_F,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_C,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_F11,            0, 0, 0),
    KEY('k', 'b', GLFW_KEY_F3,             0, 0, 0),
    KEY('m', 'b', GLFW_MOUSE_BUTTON_LEFT,  0, 0, 0),
    KEY('m', 'b', GLFW_MOUSE_BUTTON_RIGHT, 0, 0, 0),
    #endif
};

static float rotsen;

static force_inline int _writeKeyCfg(char* data, unsigned char kd, unsigned char kt, int key) {
    int off = 0;
    switch (kd) {
        case 0:;
            strcpy(data, "\\0,\\0,0");
            off += 7;
            break;
        default:;
            off += sprintf(data, "%c,%c,%d", kd, kt, key);
            break;
    }
    return off;
}

static force_inline void writeKeyCfg(input_keys k, char* sect, char* name) {
    char str[256];
    int off = _writeKeyCfg(str, k.kd1, k.kt1, k.key1);
    str[off++] = ';';
    _writeKeyCfg(&str[off], k.kd2, k.kt2, k.key2);
    declareConfigKey(config, sect, name, str, false);
}

static force_inline void _readKeyCfg(char* data, unsigned char* kd, unsigned char* kt, int* key) {
    char str[2][3];
    sscanf(data, "%2[^,],%2[^,],%d", str[0], str[1], key);
    if (str[0][0] == '\'' && str[0][1] == '0') *kd = 0;
    else *kd = str[0][0];
    if (str[1][0] == '\'' && str[1][1] == '0') *kt = 0;
    else *kt = str[1][0];
}

static force_inline void readKeyCfg(input_keys* k, char* sect, char* name) {
    char str[2][128];
    sscanf(getConfigKey(config, sect, name), "%[^;];%[^;]", str[0], str[1]);
    _readKeyCfg(str[0], &k->kd1, &k->kt1, &k->key1);
    _readKeyCfg(str[1], &k->kd2, &k->kt2, &k->key2);
}

bool initInput() {
    declareConfigKey(config, "Input", "rotationMult", "1", false);
    declareConfigKey(config, "Input", "rawMouse", "true", false);
    rotsen = atof(getConfigKey(config, "Input", "rotationMult"));
    bool rawmouse = getBool(getConfigKey(config, "Input", "rawMouse"));
    #if defined(USESDL2)
    if (rawmouse) SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1", SDL_HINT_OVERRIDE);
    char* sect = "SDL2 Keybinds";
    #else
    if (rawmouse && glfwRawMouseMotionSupported()) glfwSetInputMode(rendinf.window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    char* sect = "GLFW Keybinds";
    #endif
    for (int i = 0; i < 4; ++i) {
        writeKeyCfg(input_mov[i], sect, input_mov_names[i]);
        readKeyCfg(&input_mov[i], sect, input_mov_names[i]);
    }
    for (int i = 0; i < INPUT_ACTION_MULTI__MAX - 1; ++i) {
        writeKeyCfg(input_ma[i], sect, input_ma_names[i]);
        readKeyCfg(&input_ma[i], sect, input_ma_names[i]);
    }
    for (int i = 0; i < INPUT_ACTION_SINGLE__MAX; ++i) {
        writeKeyCfg(input_sa[i], sect, input_sa_names[i]);
        readKeyCfg(&input_sa[i], sect, input_sa_names[i]);
    }
    return true;
}

int inputMode = INPUT_MODE_GAME;

void setInputMode(int mode) {
    inputMode = mode;
    switch (mode) {
        case INPUT_MODE_GAME:;
            #if defined(USESDL2)
            SDL_SetRelativeMouseMode(SDL_TRUE);
            #else
            glfwSetInputMode(rendinf.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            #endif
            if (game_ui[UILAYER_INGAME]) game_ui[UILAYER_INGAME]->hidden = true;
            break;
        default:;
            #if defined(USESDL2)
            SDL_SetRelativeMouseMode(SDL_FALSE);
            #else
            glfwSetInputMode(rendinf.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            #endif
            if (game_ui[UILAYER_INGAME]) game_ui[UILAYER_INGAME]->hidden = false;
            break;
    }
}

#if defined(USESDL2)
void sdlgetmouse(double* mx, double* my) {
    switch (inputMode) {
        case INPUT_MODE_GAME:; {
            static double mmx = 0, mmy = 0;
            if (!(SDL_GetWindowFlags(rendinf.window) & SDL_WINDOW_INPUT_FOCUS)) {
                *mx = mmx;
                *my = mmy;
                return;
            }
            int imx, imy;
            SDL_GetRelativeMouseState(&imx, &imy);
            mmx += imx;
            mmy += imy;
            *mx = mmx;
            *my = mmy;
            break;
        }
        case INPUT_MODE_UI:; {
            int imx, imy;
            SDL_GetMouseState(&imx, &imy);
            *mx = imx;
            *my = imy;
            break;
        }
    }
}

static const uint8_t* sdlkeymap;
#endif

static force_inline bool _keyState(int device, int type, int key) {
    switch (device) {
        case 'k':; {
            switch (type) {
                case 'b':; {
                    #if defined(USESDL2)
                    return sdlkeymap[key];
                    #else
                    return (glfwGetKey(rendinf.window, key) == GLFW_PRESS);
                    #endif
                    break;
                }
            }
            break;
        }
        case 'm':; {
            switch (type) {
                case 'b':; {
                    #if defined(USESDL2)
                    return ((SDL_GetMouseState(NULL, NULL) & key) != 0);
                    #else
                    return (glfwGetMouseButton(rendinf.window, key) == GLFW_PRESS);
                    #endif
                    break;
                }
            }
            break;
        }
    }
    return false;
}

static force_inline float keyState(input_keys k) {
    float v1 = 0, v2 = 0;
    v1 = _keyState(k.kd1, k.kt1, k.key1);
    v2 = _keyState(k.kd1, k.kt1, k.key1);
    return (v2 > v1) ? v2 : v1;
}

static uint64_t polltime;

static double mxpos, mypos;

void resetInput() {
    polltime = altutime();
    #if defined(USESDL2)
    SDL_WarpMouseInWindow(rendinf.window, rendinf.width / 2, rendinf.height / 2);
    sdlgetmouse(&mxpos, &mypos);
    #else
    glfwPollEvents();
    glfwSetCursorPos(rendinf.window, rendinf.width / 2, rendinf.height / 2);
    glfwGetCursorPos(rendinf.window, &mxpos, &mypos);
    #endif
    //getInput();
}

static int lastsa = INPUT_ACTION_SINGLE__NONE;

struct input_info getInput() {
    #if defined(USESDL2)
    SDL_PumpEvents();
    SDL_Event event;
    SDL_PollEvent(&event);
    quitRequest += (event.type == SDL_QUIT);
    #else
    glfwPollEvents();
    quitRequest += (glfwWindowShouldClose(rendinf.window) != 0);
    #endif
    struct input_info inf = INPUT_EMPTY_INFO;
    if (quitRequest) return inf;
    #if defined(USESDL2)
    if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) sdlreszevent(event.window.data1, event.window.data2);
    #endif
    #if defined(USESDL2)
    inf.focus = ((SDL_GetWindowFlags(rendinf.window) & SDL_WINDOW_INPUT_FOCUS) != 0);
    #else
    inf.focus = glfwGetWindowAttrib(rendinf.window, GLFW_FOCUSED);
    #endif
    inf.rot_mult = rotsen * 0.15;
    static double nmxpos, nmypos;
    #if defined(USESDL2)
    sdlkeymap = SDL_GetKeyboardState(NULL);
    sdlgetmouse(&nmxpos, &nmypos);
    #else
    glfwGetCursorPos(rendinf.window, &nmxpos, &nmypos);
    #endif
    switch (inputMode) {
        case INPUT_MODE_GAME:; {
            if (inf.focus) {
                inf.rot_right += mxpos - nmxpos;
                inf.rot_up += mypos - nmypos;
                //printf("[%lf, %lf] ([%lf, %lf] [%lf, %lf])\n", inf.rot_right, inf.rot_up, mxpos, mypos, nmxpos, nmypos);
                mxpos = nmxpos;
                mypos = nmypos;
            }
            inf.mov_mult = ((double)((uint64_t)altutime() - (uint64_t)polltime) / (double)1000000);
            inf.mov_up += keyState(input_mov[0]);
            inf.mov_up -= keyState(input_mov[1]);
            inf.mov_right -= keyState(input_mov[2]);
            inf.mov_right += keyState(input_mov[3]);
            float mul = atan2(fabs(inf.mov_right), fabs(inf.mov_up));
            mul = fabs(1 / (cos(mul) + sin(mul)));
            inf.mov_up *= mul;
            inf.mov_right *= mul;
            for (int i = 0; i < INPUT_ACTION_MULTI__MAX; ++i) {
                if (keyState(input_ma[i]) >= 0.5) {
                    inf.multi_actions |= 1 << i;
                }
            }
            if (lastsa == INPUT_ACTION_SINGLE__NONE) {
                for (int i = 0; i < INPUT_ACTION_SINGLE__MAX; ++i) {
                    if (keyState(input_sa[i]) >= 0.5) {
                        lastsa = inf.single_action = i;
                        break;
                    }
                }
            } else {
                if (keyState(input_sa[lastsa]) < 0.5) lastsa = INPUT_ACTION_SINGLE__NONE;
            }
            break;
        }
        case INPUT_MODE_UI:; {
            if (lastsa == INPUT_ACTION_SINGLE__NONE) {
                for (int i = 0; i < INPUT_ACTION_SINGLE__MAX; ++i) {
                    if (keyState(input_sa[i]) >= 0.5) {
                        lastsa = inf.single_action = i;
                        break;
                    }
                }
            } else {
                if (keyState(input_sa[lastsa]) < 0.5) lastsa = INPUT_ACTION_SINGLE__NONE;
            }
            break;
        }
    }
    polltime = altutime();
    return inf;
}

#endif
