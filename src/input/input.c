#if MODULEID == MODULEID_GAME

#include <main/main.h>
#include "input.h" 
#include <common/common.h>
#include <renderer/renderer.h>
#include <renderer/ui.h>
#include <game/game.h>

#if defined(USESDL2)
    #include <SDL2/SDL.h>
#else
    #include <GLFW/glfw3.h>
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
    "move.right",
    "move.lookUp",
    "move.lookDown",
    "move.lookLeft",
    "move.lookRight"
};
char* input_ma_names[] = {
    "multi.place",
    "multi.destroy",
    "multi.pick",
    "multi.zoom",
    "multi.jump",
    "multi.crouch",
    "multi.run",
    "multi.playerlist",
    "multi.debug"
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
    "single.chat",
    "single.command",
    "single.fullscreen",
    "single.debug",
    //"single.leftClick",
    //"single.rightClick"
};

input_keys input_mov[] = {
    #if defined(USESDL2)
    KEY('k', 'b', SDL_SCANCODE_W, 0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_S, 0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_A, 0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_D, 0, 0, 0),
    KEY(0, 0, 0, 0, 0, 0),
    KEY('m', 'm', 1, 0, 0, 0),
    KEY('m', 'm', 0, 0, 0, 0),
    KEY(0, 0, 0, 0, 0, 0),
    #else
    KEY('k', 'b', GLFW_KEY_W, 0, 0, 0),
    KEY('k', 'b', GLFW_KEY_S, 0, 0, 0),
    KEY('k', 'b', GLFW_KEY_A, 0, 0, 0),
    KEY('k', 'b', GLFW_KEY_D, 0, 0, 0),
    KEY(0, 0, 0, 0, 0, 0),
    KEY('m', 'm', 1, 0, 0, 0),
    KEY('m', 'm', 0, 0, 0, 0),
    KEY(0, 0, 0, 0, 0, 0),
    #endif
};
input_keys input_ma[INPUT_ACTION_MULTI__MAX] = {
    #if defined(USESDL2)
    KEY('m', 'b', SDL_BUTTON(1),       0, 0, 0),
    KEY('m', 'b', SDL_BUTTON(3),       0, 0, 0),
    KEY('m', 'b', SDL_BUTTON(2),       0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_Z,      0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_SPACE,  0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_LSHIFT, 'k', 'b', SDL_SCANCODE_RSHIFT),
    KEY('k', 'b', SDL_SCANCODE_LCTRL,  'k', 'b', SDL_SCANCODE_RCTRL),
    KEY('k', 'b', SDL_SCANCODE_TAB,    0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_F4,     0, 0, 0),
    #else
    KEY('m', 'b', GLFW_MOUSE_BUTTON_LEFT,   0, 0, 0),
    KEY('m', 'b', GLFW_MOUSE_BUTTON_RIGHT,  0, 0, 0),
    KEY('m', 'b', GLFW_MOUSE_BUTTON_MIDDLE, 0, 0, 0),
    KEY('k', 'b', GLFW_KEY_Z,               0, 0, 0),
    KEY('k', 'b', GLFW_KEY_SPACE,           0, 0, 0),
    KEY('k', 'b', GLFW_KEY_LEFT_SHIFT,      'k', 'b', GLFW_KEY_RIGHT_SHIFT),
    KEY('k', 'b', GLFW_KEY_LEFT_CONTROL,    'k', 'b', GLFW_KEY_RIGHT_CONTROL),
    KEY('k', 'b', GLFW_KEY_TAB,             0, 0, 0),
    KEY('k', 'b', GLFW_KEY_F4,              0, 0, 0),
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
    KEY('k', 'b', SDL_SCANCODE_RIGHTBRACKET, 'm', 'w', 1),
    KEY('k', 'b', SDL_SCANCODE_LEFTBRACKET,  'm', 'w', -1),
    KEY('k', 'b', SDL_SCANCODE_EQUALS,       0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_MINUS,        0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_R,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_F,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_C,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_T,            0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_SLASH,        0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_F11,          0, 0, 0),
    KEY('k', 'b', SDL_SCANCODE_F3,           0, 0, 0),
    //KEY('m', 'b', SDL_BUTTON(1),             0, 0, 0),
    //KEY('m', 'b', SDL_BUTTON(3),             0, 0, 0),
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
    KEY('k', 'b', GLFW_KEY_RIGHT_BRACKET,  'm', 'w', 1),
    KEY('k', 'b', GLFW_KEY_LEFT_BRACKET,   'm', 'w', -1),
    KEY('k', 'b', GLFW_KEY_EQUAL,          0, 0, 0),
    KEY('k', 'b', GLFW_KEY_MINUS,          0, 0, 0),
    KEY('k', 'b', GLFW_KEY_R,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_F,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_C,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_T,              0, 0, 0),
    KEY('k', 'b', GLFW_KEY_SLASH,          0, 0, 0),
    KEY('k', 'b', GLFW_KEY_F11,            0, 0, 0),
    KEY('k', 'b', GLFW_KEY_F3,             0, 0, 0),
    //KEY('m', 'b', GLFW_MOUSE_BUTTON_LEFT,  0, 0, 0),
    //KEY('m', 'b', GLFW_MOUSE_BUTTON_RIGHT, 0, 0, 0),
    #endif
};

static float rotsenx;
static float rotseny;

int inputMode = INPUT_MODE_GAME;

void setInputMode(int mode) {
    inputMode = mode;
    switch (mode) {
        case INPUT_MODE_GAME:;
            #if defined(USESDL2)
            SDL_SetRelativeMouseMode(SDL_TRUE);
            #else
            glfwSetInputMode(rendinf.window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
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
void sdl2getmouse(double* mx, double* my) {
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
        } break;
        case INPUT_MODE_UI:; {
            int imx, imy;
            SDL_GetMouseState(&imx, &imy);
            *mx = imx;
            *my = imy;
        } break;
    }
}

static const uint8_t* sdl2keymap;
#else
static double glfwmscroll = 0;

void glfwmscrollcb(GLFWwindow* w, double x, double y) {
    (void)w;
    (void)x;
    glfwmscroll += y;
    //printf("scroll: [%lf]\n", y);
}

static bool glfwgp = false;
GLFWgamepadstate glfwgpstate;
#endif

static double mmovx, mmovy;
static int mscrollup, mscrolldown;

static force_inline float _keyState(int device, int type, int key) {
    switch (device) {
        case 'k':; {
            switch (type) {
                case 'b':; {
                    #if defined(USESDL2)
                    return sdl2keymap[key];
                    #else
                    return (glfwGetKey(rendinf.window, key) == GLFW_PRESS);
                    #endif
                } break;
            }
        } break;
        case 'm':; {
            switch (type) {
                case 'b':; {
                    #if defined(USESDL2)
                    return ((SDL_GetMouseState(NULL, NULL) & key) != 0);
                    #else
                    return (glfwGetMouseButton(rendinf.window, key) == GLFW_PRESS);
                    #endif
                } break;
                case 'm':; {
                    switch (key) {
                        case 0:;
                            return mmovx;
                            break;
                        case 1:;
                            return mmovy;
                            break;
                    }
                } break;
                case 'w':; {
                    switch (key) {
                        case 1:;
                            return mscrollup;
                            break;
                        case -1:;
                            return mscrolldown;
                            break;
                    }
                } break;
            }
        } break;
        case 'g':; {
            switch (type) {
                case 'a':; {
                    #if defined(USESDL2)
                    #else
                    if (glfwgp) {
                        if (fabs(glfwgpstate.axes[key]) >= 0.2) return glfwgpstate.axes[key];
                    }
                    #endif
                } break;
                case 'b':; {
                    #if defined(USESDL2)
                    #else
                    if (glfwgp) {
                        return glfwgpstate.buttons[key];
                    }
                    #endif
                } break;
            }
        } break;
    }
    return 0.0;
}

static force_inline float keyState(input_keys k) {
    float v1 = 0, v2 = 0;
    v1 = _keyState(k.kd1, k.kt1, k.key1);
    v2 = _keyState(k.kd2, k.kt2, k.key2);
    return (fabs(v2) > fabs(v1)) ? v2 : v1;
}

static uint64_t polltime;

//static double oldmxpos, oldmypos;
static double newmxpos, newmypos;

void resetInput() {
    polltime = altutime();
    #if defined(USESDL2)
    SDL_WarpMouseInWindow(rendinf.window, rendinf.width / 2, rendinf.height / 2);
    //sdl2getmouse(&oldmxpos, &oldmypos);
    #else
    glfwPollEvents();
    glfwSetCursorPos(rendinf.window, rendinf.width / 2, rendinf.height / 2);
    //glfwGetCursorPos(rendinf.window, &oldmxpos, &oldmypos);
    #endif
    //getInput();
}

static int lastsa = INPUT_ACTION_SINGLE__NONE;

void getInput(struct input_info* _inf) {
    struct input_info* inf;
    if (!_inf) inf = malloc(sizeof(*inf));
    else inf = _inf;
    #if defined(USESDL2)
    SDL_PumpEvents();
    SDL_Event event;
    SDL_PollEvent(&event);
    quitRequest += (event.type == SDL_QUIT);
    sdl2keymap = SDL_GetKeyboardState(NULL);
    int sdl2mscroll = (event.type == SDL_MOUSEWHEEL) ? event.wheel.y : 0;
    static int mscrolltoggle = 0;
    if (mscrolltoggle) {
        if (sdl2mscroll >= 0) {
            mscrollup = sdl2mscroll;
            mscrolldown = 0;
        } else {
            mscrollup = 0;
            mscrolldown = -sdl2mscroll;
        }
    } else {
        mscrollup = 0;
        mscrolldown = 0;
    }
    if (sdl2mscroll != 0) mscrolltoggle = !mscrolltoggle;
    else mscrolltoggle = 1;
    sdl2getmouse(&newmxpos, &newmypos);
    #else
    glfwPollEvents();
    quitRequest += (glfwWindowShouldClose(rendinf.window) != 0);
    static int mscrolltoggle = 1;
    if (mscrolltoggle) {
        if (glfwmscroll >= 0) {
            mscrollup = glfwmscroll;
            mscrolldown = 0;
        } else {
            mscrollup = 0;
            mscrolldown = -glfwmscroll;
        }
    } else {
        mscrollup = 0;
        mscrolldown = 0;
    }
    if (glfwmscroll != 0.0) mscrolltoggle = !mscrolltoggle;
    else mscrolltoggle = 1;
    glfwmscroll = 0.0;
    for (int i = GLFW_JOYSTICK_1; i < GLFW_JOYSTICK_LAST; ++i) {
        if ((glfwgp = glfwGetGamepadState(GLFW_JOYSTICK_1, &glfwgpstate))) break;
    }
    if (inputMode == INPUT_MODE_GAME && inf->focus) {
        glfwGetCursorPos(rendinf.window, &newmxpos, &newmypos);
        glfwSetCursorPos(rendinf.window, rendinf.width / 2, rendinf.height / 2);
    }
    #endif
    mmovx = newmxpos - ((double)rendinf.width / 2.0);
    mmovy = newmypos - ((double)rendinf.height / 2.0);
    //printf("Mouse movement: [%lf, %lf] [%lf, %lf]\n", newmxpos, newmypos, mmovx, mmovy);
    //oldmxpos = newmxpos;
    //oldmypos = newmypos;
    *inf = INPUT_EMPTY_INFO;
    if (quitRequest) goto ret;
    #if defined(USESDL2)
    if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) sdlreszevent(event.window.data1, event.window.data2);
    #endif
    #if defined(USESDL2)
    inf->focus = ((SDL_GetWindowFlags(rendinf.window) & SDL_WINDOW_INPUT_FOCUS) != 0);
    #else
    inf->focus = glfwGetWindowAttrib(rendinf.window, GLFW_FOCUSED);
    #endif
    inf->rot_mult_x = rotsenx * 0.15;
    inf->rot_mult_y = rotseny * 0.15;
    switch (inputMode) {
        case INPUT_MODE_GAME:; {
            inf->mov_mult = ((double)((uint64_t)altutime() - (uint64_t)polltime) / (double)1000000);
            inf->mov_up += keyState(input_mov[0]);
            inf->mov_up -= keyState(input_mov[1]);
            inf->mov_right -= keyState(input_mov[2]);
            inf->mov_right += keyState(input_mov[3]);
            //if (inf->focus) {
                inf->rot_up += keyState(input_mov[4]);
                inf->rot_up -= keyState(input_mov[5]);
                inf->rot_right -= keyState(input_mov[6]);
                inf->rot_right += keyState(input_mov[7]);
            //}
            float mul = atan2(fabs(inf->mov_right), fabs(inf->mov_up));
            mul = fabs(1 / (cos(mul) + sin(mul)));
            inf->mov_up *= mul;
            inf->mov_right *= mul;
            inf->mov_bal = mul;
            for (int i = 0; i < INPUT_ACTION_MULTI__MAX - 1; ++i) {
                if (keyState(input_ma[i]) >= 0.2) {
                    inf->multi_actions |= 1 << i;
                }
            }
            if (lastsa == INPUT_ACTION_SINGLE__NONE) {
                for (int i = 0; i < INPUT_ACTION_SINGLE__MAX; ++i) {
                    if (keyState(input_sa[i]) >= 0.2) {
                        lastsa = inf->single_action = i;
                        break;
                    }
                }
            } else {
                if (keyState(input_sa[lastsa]) < 0.2) lastsa = INPUT_ACTION_SINGLE__NONE;
            }
        } break;
        case INPUT_MODE_UI:; {
            if (lastsa == INPUT_ACTION_SINGLE__NONE) {
                for (int i = 0; i < INPUT_ACTION_SINGLE__MAX; ++i) {
                    if (keyState(input_sa[i]) >= 0.2) {
                        lastsa = inf->single_action = i;
                        break;
                    }
                }
            } else {
                if (keyState(input_sa[lastsa]) < 0.2) lastsa = INPUT_ACTION_SINGLE__NONE;
            }
        } break;
    }
    polltime = altutime();
    ret:;
    if (!_inf) free(inf);
}

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
    declareConfigKey(config, "Input", "xSen", "1", false);
    declareConfigKey(config, "Input", "ySen", "1", false);
    declareConfigKey(config, "Input", "rawMouse", "true", false);
    rotsenx = atof(getConfigKey(config, "Input", "xSen"));
    rotseny = atof(getConfigKey(config, "Input", "ySen"));
    bool rawmouse = getBool(getConfigKey(config, "Input", "rawMouse"));
    #if defined(USESDL2)
    if (rawmouse) SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1", SDL_HINT_OVERRIDE);
    char* sect = "SDL2 Keybinds";
    #else
    if (rawmouse && glfwRawMouseMotionSupported()) glfwSetInputMode(rendinf.window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    glfwSetScrollCallback(rendinf.window, glfwmscrollcb);
    char* sect = "GLFW Keybinds";
    #endif
    for (int i = 0; i < 8; ++i) {
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

#endif
