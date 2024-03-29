#if defined(MODULE_GAME)

#include <main/main.h>
#include "input.h" 
#include "game.h"
#include <common/common.h>
#include <graphics/renderer.h>
#ifdef __EMSCRIPTEN__
    #include <emscripten/html5.h>
#endif

#if defined(USESDL2)
    #if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
        #include <SDL2/SDL.h>
    #else
        #include <SDL.h>
    #endif
#else
    #include <GLFW/glfw3.h>
#endif

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include <common/glue.h>

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
    KEY('k', 'b', SDL_SCANCODE_RIGHTBRACKET, 'm', 'w', -1),
    KEY('k', 'b', SDL_SCANCODE_LEFTBRACKET,  'm', 'w', 1),
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
    KEY('k', 'b', GLFW_KEY_RIGHT_BRACKET,  'm', 'w', -1),
    KEY('k', 'b', GLFW_KEY_LEFT_BRACKET,   'm', 'w', 1),
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

int inputMode = INPUT_MODE_UI;

void setInputMode(int mode) {
    inputMode = mode;
    switch (mode) {
        case INPUT_MODE_GAME:;
            #if defined(USESDL2)
            SDL_SetRelativeMouseMode(SDL_TRUE);
            SDL_CaptureMouse(SDL_TRUE);
            #else
            glfwSetInputMode(rendinf.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            #endif
            #ifdef __EMSCRIPTEN__
            emscripten_request_pointerlock("canvas", true);
            #endif
            break;
        default:;
            #if defined(USESDL2)
            SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_CaptureMouse(SDL_FALSE);
            #else
            glfwSetInputMode(rendinf.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            #endif
            /*
            #ifdef __EMSCRIPTEN__
            emscripten_exit_pointerlock();
            #endif
            */
            break;
    }
}

#if defined(USESDL2)
void sdl2getmouse(double* mx, double* my) {
    int imx, imy;
    SDL_GetMouseState(&imx, &imy);
    *mx = (float)imx;
    *my = (float)imy;
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

#ifndef __EMSCRIPTEN__
static bool glfwgp = false;
GLFWgamepadstate glfwgpstate;
#endif
#endif

static double mmovx, mmovy;
static int mscrollup, mscrolldown;

static inline float _keyState(int device, int type, int key, bool* repeat) {
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
                            *repeat = true;
                            return mscrollup;
                            break;
                        case -1:;
                            *repeat = true;
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
                    #ifndef __EMSCRIPTEN__
                    if (glfwgp) {
                        if (fabs(glfwgpstate.axes[key]) >= 0.2) return glfwgpstate.axes[key];
                    }
                    #endif
                    #endif
                } break;
                case 'b':; {
                    #if defined(USESDL2)
                    #else
                    #ifndef __EMSCRIPTEN__
                    if (glfwgp) {
                        return glfwgpstate.buttons[key];
                    }
                    #endif
                    #endif
                } break;
            }
        } break;
    }
    return 0.0;
}

static inline float keyState(input_keys k, bool* r) {
    float v1 = 0, v2 = 0;
    bool r1 = false, r2 = false;
    v1 = _keyState(k.kd1, k.kt1, k.key1, &r1);
    v2 = _keyState(k.kd2, k.kt2, k.key2, &r2);
    if (fabs(v2) > fabs(v1)) {
        if (r) *r = r2;
        return v2;
    } else {
        if (r) *r = r1;
        return v1;
    }
}

static uint64_t polltime;

#ifndef USESDL2
static double oldmxpos, oldmypos;
static double newmxpos, newmypos;
#else
static double sdlmxmov, sdlmymov;
#endif

static void getMouseChange(double* x, double* y) {
    #if defined(USESDL2)
    *x = sdlmxmov;
    *y = sdlmymov;
    sdlmxmov = 0.0;
    sdlmymov = 0.0;
    #else
    glfwGetCursorPos(rendinf.window, &newmxpos, &newmypos);
    #endif
    #ifndef USESDL2
    *x = newmxpos - oldmxpos;
    *y = newmypos - oldmypos;
    oldmxpos = newmxpos;
    oldmypos = newmypos;
    #endif
}

void resetInput() {
    polltime = altutime();
    #if defined(USESDL2)
    if ((SDL_GetWindowFlags(rendinf.window) & SDL_WINDOW_INPUT_FOCUS) != 0) {
        SDL_WarpMouseInWindow(rendinf.window, floor((double)rendinf.width / 2.0), floor((double)rendinf.height / 2.0));
    }
    #else
    glfwPollEvents();
    if (glfwGetWindowAttrib(rendinf.window, GLFW_FOCUSED)) {
        glfwSetCursorPos(rendinf.window, floor((double)rendinf.width / 2.0), floor((double)rendinf.height / 2.0));
    }
    glfwGetCursorPos(rendinf.window, &oldmxpos, &oldmypos);
    #endif
    //getInput();
}

static int lastsa = INPUT_ACTION_SINGLE__NONE;

void getInput(struct input_info* _inf) {
    struct input_info* inf;
    if (!_inf) inf = malloc(sizeof(*inf));
    else inf = _inf;
    *inf = INPUT_EMPTY_INFO;
    #if defined(USESDL2)
    SDL_PumpEvents();
    sdl2keymap = SDL_GetKeyboardState(NULL);
    SDL_Event event;
    #ifndef __EMSCRIPTEN__
    inf->focus = ((SDL_GetWindowFlags(rendinf.window) & SDL_WINDOW_INPUT_FOCUS) != 0);
    #else
    inf->focus = true;
    #endif
    int sdl2mscroll = 0;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:;
                ++quitRequest;
                break;
            #ifdef __ANDROID__ // TODO: Back = Esc
            case SDL_KEYDOWN:;
                if (event.key.keysym.sym == SDLK_AC_BACK) {
                    ++quitRequest;
                }
                break;
            #endif
            case SDL_MOUSEWHEEL:;
                sdl2mscroll += event.wheel.y;
                break;
            case SDL_MOUSEMOTION:;
                sdlmxmov += event.motion.xrel;
                sdlmymov += event.motion.yrel;
                break;
            case SDL_WINDOWEVENT:;
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    sdlreszevent(event.window.data1, event.window.data2);
                }
                break;
        }
    }
    if (sdl2mscroll >= 0) {
        mscrollup = sdl2mscroll;
        mscrolldown = 0;
    } else {
        mscrollup = 0;
        mscrolldown = -sdl2mscroll;
    }
    #else
    glfwPollEvents();
    #ifndef __EMSCRIPTEN__
    inf->focus = glfwGetWindowAttrib(rendinf.window, GLFW_FOCUSED);
    #else
    inf->focus = true;
    #endif
    if (glfwWindowShouldClose(rendinf.window)) ++quitRequest;
    if (glfwmscroll >= 0) {
        mscrollup = glfwmscroll;
        mscrolldown = 0;
    } else {
        mscrollup = 0;
        mscrolldown = -glfwmscroll;
    }
    glfwmscroll = 0.0;
    #ifndef __EMSCRIPTEN__
    for (int i = GLFW_JOYSTICK_1; i < GLFW_JOYSTICK_LAST; ++i) {
        if ((glfwgp = glfwGetGamepadState(GLFW_JOYSTICK_1, &glfwgpstate))) break;
    }
    #endif
    #endif
    getMouseChange(&mmovx, &mmovy);
    //printf("Mouse movement: [%lf, %lf] [%lf, %lf]\n", newmxpos, newmypos, mmovx, mmovy);
    if (quitRequest) goto ret;
    /*
    #ifdef __EMSCRIPTEN__
    if (inputMode == INPUT_MODE_GAME) {
        EmscriptenPointerlockChangeEvent plock;
        emscripten_get_pointerlock_status(&plock);
        inf->focus = (inf->focus && plock.isActive);
    }
    #endif
    */
    inf->rot_mult_x = rotsenx * 0.15;
    inf->rot_mult_y = rotseny * 0.15;
    switch (inputMode) {
        case INPUT_MODE_GAME:; {
            inf->mov_mult = ((double)((uint64_t)altutime() - (uint64_t)polltime) / (double)1000000);
            inf->mov_up += keyState(input_mov[0], NULL);
            inf->mov_up -= keyState(input_mov[1], NULL);
            inf->mov_right -= keyState(input_mov[2], NULL);
            inf->mov_right += keyState(input_mov[3], NULL);
            //if (inf->focus) {
                inf->rot_up += keyState(input_mov[4], NULL);
                inf->rot_up -= keyState(input_mov[5], NULL);
                inf->rot_right -= keyState(input_mov[6], NULL);
                inf->rot_right += keyState(input_mov[7], NULL);
            //}
            float mul = atan2(fabs(inf->mov_right), fabs(inf->mov_up));
            mul = fabs(1 / (cos(mul) + sin(mul)));
            inf->mov_up *= mul;
            inf->mov_right *= mul;
            inf->mov_bal = mul;
            for (int i = 0; i < INPUT_ACTION_MULTI__MAX - 1; ++i) {
                if (keyState(input_ma[i], NULL) >= 0.2) {
                    inf->multi_actions |= 1 << i;
                }
            }
            if (lastsa == INPUT_ACTION_SINGLE__NONE) {
                for (int i = 0; i < INPUT_ACTION_SINGLE__MAX; ++i) {
                    bool repeat = false;
                    if (keyState(input_sa[i], &repeat) >= 0.2) {
                        inf->single_action = i;
                        if (!repeat) lastsa = i;
                        break;
                    }
                }
            } else {
                if (keyState(input_sa[lastsa], NULL) < 0.2) lastsa = INPUT_ACTION_SINGLE__NONE;
            }
        } break;
        case INPUT_MODE_UI:; {
            double dmx, dmy;
            #if defined(USESDL2)
            sdl2getmouse(&dmx, &dmy);
            #else
            glfwGetCursorPos(rendinf.window, &dmx, &dmy);
            #endif
            inf->ui_mouse_x = dmx;
            inf->ui_mouse_y = dmy;
            #if defined(USESDL2)
            inf->ui_mouse_click = ((SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(1)) != 0);
            #else
            inf->ui_mouse_click = (glfwGetMouseButton(rendinf.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
            #endif
            if (lastsa == INPUT_ACTION_SINGLE__NONE) {
                for (int i = 0; i < INPUT_ACTION_SINGLE__MAX; ++i) {
                    bool repeat = false;
                    if (keyState(input_sa[i], &repeat) >= 0.2) {
                        inf->single_action = i;
                        if (!repeat) lastsa = i;
                        break;
                    }
                }
            } else {
                if (keyState(input_sa[lastsa], NULL) < 0.2) lastsa = INPUT_ACTION_SINGLE__NONE;
            }
        } break;
    }
    polltime = altutime();
    ret:;
    if (!_inf) free(inf);
}

static inline int _writeKeyCfg(char* data, unsigned char kd, unsigned char kt, int key) {
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

static inline void writeKeyCfg(input_keys k, char* sect, char* name) {
    char str[256];
    int off = _writeKeyCfg(str, k.kd1, k.kt1, k.key1);
    str[off++] = ';';
    _writeKeyCfg(&str[off], k.kd2, k.kt2, k.key2);
    declareConfigKey(config, sect, name, str, false);
}

static inline void _readKeyCfg(char* data, unsigned char* kd, unsigned char* kt, int* key) {
    char str[2][3];
    sscanf(data, "%2[^,],%2[^,],%d", str[0], str[1], key);
    if (str[0][0] == '\'' && str[0][1] == '0') *kd = 0;
    else *kd = str[0][0];
    if (str[1][0] == '\'' && str[1][1] == '0') *kt = 0;
    else *kt = str[1][0];
}

static inline void readKeyCfg(input_keys* k, char* sect, char* name) {
    char str[2][128];
    sscanf(getConfigKey(config, sect, name), "%[^;];%[^;]", str[0], str[1]);
    _readKeyCfg(str[0], &k->kd1, &k->kt1, &k->key1);
    _readKeyCfg(str[1], &k->kd2, &k->kt2, &k->key2);
}

bool initInput() {
    declareConfigKey(config, "Input", "xSen", "1", false);
    declareConfigKey(config, "Input", "ySen", "1", false);
    declareConfigKey(config, "Input", "rawMouse", "false", false);
    rotsenx = atof(getConfigKey(config, "Input", "xSen"));
    rotseny = atof(getConfigKey(config, "Input", "ySen"));
    bool rawmouse = getBool(getConfigKey(config, "Input", "rawMouse"));
    #if defined(USESDL2)
    if (rawmouse) SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1", SDL_HINT_OVERRIDE);
    char* sect = "SDL2 Keybinds";
    #else
    if (rawmouse) {
        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(rendinf.window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        } else {
            fputs("Failed to enable raw mouse\n", stderr);
        }
    }
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
