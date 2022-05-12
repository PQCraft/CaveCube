#include <common.h>
#include <renderer.h>
#include <main.h>
#include "input.h" 

#include <GLFW/glfw3.h>

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

bool initInput() {
    if (glfwRawMouseMotionSupported()) glfwSetInputMode(rendinf.window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    setInputMode(INPUT_MODE_GAME);
    return true;
}

int inputMode = INPUT_MODE_GAME;

void setInputMode(int mode) {
    inputMode = mode;
    if (mode == INPUT_MODE_GAME) {
        glfwSetInputMode(rendinf.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSetInputMode(rendinf.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

bool keyDown(int key) {
    if (key < 0) return false;
    if (key < 32) return (glfwGetMouseButton(rendinf.window, key) == GLFW_PRESS);
    return (glfwGetKey(rendinf.window, key) == GLFW_PRESS);
}

struct input_info getInput() {
    glfwPollEvents();
    quitRequest += rendererQuitRequest();
    struct input_info inf = INPUT_EMPTY_INFO;
    inf.mmovti = true;
    if (quitRequest) return inf;
    static bool mposset = false;
    static double mxpos, mypos;
    static double nmxpos, nmypos;
    if (glfwGetWindowAttrib(rendinf.window, GLFW_HOVERED)) {
        if (!mposset) {
            glfwGetCursorPos(rendinf.window, &mxpos, &mypos); mposset = true;
        } else {
            glfwGetCursorPos(rendinf.window, &nmxpos, &nmypos);
            inf.mxmov += mxpos - nmxpos;
            inf.mymov += mypos - nmypos;
            mxpos = nmxpos;
            mypos = nmypos;
        }
    } else {
        mposset = false;
    }
    if (inputMode == INPUT_MODE_GAME) {
        if (keyDown(input_mov[0].key1) || keyDown(input_mov[0].key2)) inf.zmov += 1.0;
        if (keyDown(input_mov[1].key1) || keyDown(input_mov[1].key2)) inf.zmov -= 1.0;
        if (keyDown(input_mov[2].key1) || keyDown(input_mov[2].key2)) inf.xmov -= 1.0;
        if (keyDown(input_mov[3].key1) || keyDown(input_mov[3].key2)) inf.xmov += 1.0;
        for (int i = 0; i < INPUT_ACTION_MULTI__MAX; ++i) {
            if (keyDown(input_ma[i].key1) || keyDown(input_ma[i].key2)) {
                inf.multi_actions |= 1 << i;
            }
        }
    }
    return inf;
}

/*
void testInput() {
    quitRequest += rendererQuitRequest();
    float pmult = posmult;
    float rmult = rotmult;
    static int fullscreen = -1;
    if (fullscreen == -1) fullscreen = rendinf.fullscr;
    static bool setfullscr = false;
    if (glfwGetKey(rendinf.window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(rendinf.window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS) {
        glfwGetKey(rendinf.window, GLFW_KEY_ENTER);
        if (glfwGetKey(rendinf.window, GLFW_KEY_ENTER) == GLFW_PRESS) {
            if (!setfullscr) {
                setfullscr = true;
                fullscreen = !fullscreen;
                setFullscreen(fullscreen);
            }
        }
    } else {
        setfullscr = false;
    }
    if (glfwGetKey(rendinf.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(rendinf.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
        rendinf.campos.y -= fpsmult / M_PI * 0.75;
        if (rendinf.campos.y < 1.25) rendinf.campos.y = 1.25;
        pmult /= 2;
    } else {
        if (rendinf.campos.y < 1.5) rendinf.campos.y = 1.5;
    }
    if (glfwGetKey(rendinf.window, GLFW_KEY_Z) == GLFW_PRESS) {
        rendinf.camfov = 10;
        pmult /= 2;
        rmult /= 4;
    } else {
        rendinf.camfov = 50;
    }
    if (glfwGetKey(rendinf.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(rendinf.window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) {
        pmult *= 2;
    }
    static bool mrelease = false;
    static bool mposset = false;
    static double mxpos, mypos;
    if ((glfwGetWindowAttrib(rendinf.window, GLFW_HOVERED) || mrelease) && glfwGetMouseButton(rendinf.window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        mrelease = true;
        if (!mposset) {glfwGetCursorPos(rendinf.window, &mxpos, &mypos); mposset = true;}
        static double nmxpos, nmypos;
        glfwGetCursorPos(rendinf.window, &nmxpos, &nmypos);
        rendinf.camrot.x += (mypos - nmypos) * mousesns * rmult / rotmult;
        rendinf.camrot.y -= (mxpos - nmxpos) * mousesns * rmult / rotmult;
        if (rendinf.camrot.y < -360) rendinf.camrot.y += 360;
        else if (rendinf.camrot.y > 360) rendinf.camrot.y -= 360;
        if (rendinf.camrot.x > 89.99) rendinf.camrot.x = 89.99;
        if (rendinf.camrot.x < -89.99) rendinf.camrot.x = -89.99;
        mxpos = nmxpos;
        mypos = nmypos;
        glfwSetInputMode(rendinf.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        mrelease = false;
        mposset = false;
        glfwSetInputMode(rendinf.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwGetCursorPos(rendinf.window, &mxpos, &mypos);
    }
    if (glfwGetKey(rendinf.window, GLFW_KEY_UP) == GLFW_PRESS) {
        rendinf.camrot.x += rmult;
        if (rendinf.camrot.x > 89.99) rendinf.camrot.x = 89.99;
    }
    if (glfwGetKey(rendinf.window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        rendinf.camrot.x -= rmult;
        if (rendinf.camrot.x < -89.99) rendinf.camrot.x = -89.99;
    }
    if (glfwGetKey(rendinf.window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        rendinf.camrot.y -= rmult;
        if (rendinf.camrot.y < -360) rendinf.camrot.y += 360;
    }
    if (glfwGetKey(rendinf.window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        rendinf.camrot.y += rmult;
        if (rendinf.camrot.y > 360) rendinf.camrot.y -= 360;
    }
    if (glfwGetKey(rendinf.window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        rendinf.campos.y += fpsmult / M_PI * 0.75;
    }
    if (glfwGetKey(rendinf.window, GLFW_KEY_W) == GLFW_PRESS) {
        float yrotrad = (rendinf.camrot.y / 180 * M_PI);
        rendinf.campos.x += sinf(yrotrad) * pmult;
        rendinf.campos.z -= cosf(yrotrad) * pmult;
    }
    if (glfwGetKey(rendinf.window, GLFW_KEY_S) == GLFW_PRESS) {
        float yrotrad = (rendinf.camrot.y / 180 * M_PI);
        rendinf.campos.x -= sinf(yrotrad) * pmult;
        rendinf.campos.z += cosf(yrotrad) * pmult;
    }
    if (glfwGetKey(rendinf.window, GLFW_KEY_A) == GLFW_PRESS) {
        float yrotrad;
        yrotrad = (rendinf.camrot.y / 180 * M_PI);
        rendinf.campos.x -= cosf(yrotrad) * pmult;
        rendinf.campos.z -= sinf(yrotrad) * pmult;
    }
    if (glfwGetKey(rendinf.window, GLFW_KEY_D) == GLFW_PRESS) {
        float yrotrad;
        yrotrad = (rendinf.camrot.y / 180 * M_PI);
        rendinf.campos.x += cosf(yrotrad) * pmult;
        rendinf.campos.z += sinf(yrotrad) * pmult;
    }
}
*/
