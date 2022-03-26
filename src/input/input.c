#include <common.h>
#include <renderer.h>
#include <main.h>
#include "input.h" 

#include <GLFW/glfw3.h>

#include <math.h>

bool initInput() {
    if (glfwRawMouseMotionSupported()) glfwSetInputMode(rendinf.window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    return true;
}

float posmult = 0.125;
float rotmult = 3;
float fpsmult = 0;
float mousesns = 0.125;

float input_xrot = 0.0;
float input_yrot = 0.0;

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

input_info getInput() {
    quitRequest += rendererQuitRequest();
    input_info inf = INPUT_EMPTY_INFO;
    return inf;
}
