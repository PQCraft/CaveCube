#ifndef SERVER

#include <main/main.h>
#include "input.h" 
#include <common/common.h>
#include <renderer/renderer.h>

#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <math.h>

#define INPUT_KEY(k1, k2) (input_keys){k1, k2}

typedef struct {
    int key1;
    int key2;
} input_keys;

input_keys input_mov[4] = {
    INPUT_KEY(GLFW_KEY_W, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_S, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_A, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_D, GLFW_KEY_UNKNOWN),
};
input_keys input_ma[INPUT_ACTION_MULTI__MAX] = {
    INPUT_KEY(GLFW_MOUSE_BUTTON_LEFT, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_MOUSE_BUTTON_RIGHT, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_MOUSE_BUTTON_MIDDLE, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_SPACE, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_LEFT_SHIFT, GLFW_KEY_RIGHT_SHIFT),
    INPUT_KEY(GLFW_KEY_LEFT_CONTROL, GLFW_KEY_RIGHT_CONTROL),
    INPUT_KEY(GLFW_KEY_TAB, GLFW_KEY_UNKNOWN),
};
input_keys input_sa[INPUT_ACTION_SINGLE__MAX] = {
    INPUT_KEY(GLFW_KEY_ESCAPE, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_MOUSE_BUTTON_LEFT, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_MOUSE_BUTTON_RIGHT, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_0, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_1, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_2, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_3, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_4, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_5, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_6, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_7, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_8, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_9, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_UNKNOWN, GLFW_KEY_UNKNOWN),
    INPUT_KEY(GLFW_KEY_UNKNOWN, GLFW_KEY_UNKNOWN),
};

static uint16_t tmpmods = 0;
static int tmpkey = -1;

static float sensitivity = 1.0;

bool initInput() {
    if (glfwRawMouseMotionSupported()) glfwSetInputMode(rendinf.window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    setInputMode(INPUT_MODE_GAME);
    sensitivity = atof(getConfigVarStatic(config, "input.sensitivity", "1", 64));
    return true;
}

static int inputMode = INPUT_MODE_GAME;

void setInputMode(int mode) {
    inputMode = mode;
    if (mode == INPUT_MODE_GAME) {
        glfwSetInputMode(rendinf.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSetInputMode(rendinf.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

static bool keyDown(int key) {
    if (key < 0) return false;
    if (key < 32) return (glfwGetMouseButton(rendinf.window, key) == GLFW_PRESS);
    return (glfwGetKey(rendinf.window, key) == GLFW_PRESS);
}

static uint64_t polltime;
static double mxpos, mypos;

void resetInput() {
    polltime = altutime();
    glfwGetCursorPos(rendinf.window, &mxpos, &mypos);
}

struct input_info getInput() {
    glfwPollEvents();
    quitRequest += rendererQuitRequest();
    struct input_info inf = INPUT_EMPTY_INFO;
    if (quitRequest) return inf;
    inf.rot_mult = sensitivity * 0.15;
    static double nmxpos, nmypos;
    if (glfwGetWindowAttrib(rendinf.window, GLFW_HOVERED)) {
        glfwGetCursorPos(rendinf.window, &nmxpos, &nmypos);
        inf.rot_right += mxpos - nmxpos;
        inf.rot_up += mypos - nmypos;
        mxpos = nmxpos;
        mypos = nmypos;
    }
    inf.mov_mult = ((double)((uint64_t)altutime() - (uint64_t)polltime) / (double)1000000);
    if (inputMode == INPUT_MODE_GAME) {
        if (keyDown(input_mov[0].key1) || keyDown(input_mov[0].key2)) inf.mov_up += 1.0;
        if (keyDown(input_mov[1].key1) || keyDown(input_mov[1].key2)) inf.mov_up -= 1.0;
        if (keyDown(input_mov[2].key1) || keyDown(input_mov[2].key2)) inf.mov_right -= 1.0;
        if (keyDown(input_mov[3].key1) || keyDown(input_mov[3].key2)) inf.mov_right += 1.0;
        float mul = atan2(fabs(inf.mov_right), fabs(inf.mov_up));
        mul = fabs(1 / (cos(mul) + sin(mul)));
        inf.mov_up *= mul;
        inf.mov_right *= mul;
        for (int i = 0; i < INPUT_ACTION_MULTI__MAX; ++i) {
            if (keyDown(input_ma[i].key1) || keyDown(input_ma[i].key2)) {
                inf.multi_actions |= 1 << i;
            }
        }
    }
    polltime = altutime();
    return inf;
}

#endif
