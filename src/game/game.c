#include "main.h"
#include <common.h>
#include <resource.h>
#include <bmd.h>
#include <renderer.h>
#include <input.h>
#include <noise.h>

#include <stdio.h>
#include <stdlib.h>

static float posmult = 0.125;
static float rotmult = 3;
static float fpsmult = 0;
static float mousesns = 0.125;

void doGame() {
    char* tpath = "game/textures/blocks/stone/0.png";
    model* m2 = loadModel("game/models/block/default.bmd", tpath, tpath, tpath, tpath, tpath, tpath);
    //tpath = "game/textures/blocks/dirt/0.png";
    #define PREFIX1 "game/textures/blocks/grass/"
    model* m1 = loadModel("game/models/block/default.bmd", PREFIX1"0.png", PREFIX1"1.png", PREFIX1"2.png", PREFIX1"3.png", PREFIX1"4.png", PREFIX1"5.png");
    tpath = "game/textures/blocks/gravel/0.png";
    model* m3 = loadModel("game/models/block/default.bmd", tpath, tpath, tpath, tpath, tpath, tpath);
    tpath = "game/textures/blocks/bedrock/0.png";
    model* m4 = loadModel("game/models/block/default.bmd", tpath, tpath, tpath, tpath, tpath, tpath);
    m1->pos = (coord_3d){0.0, -0.5, -2.0};
    m2->pos = (coord_3d){0.0, 2.5, 2.0};
    m3->pos = (coord_3d){-2.0, 1.5, 0.0};
    m4->pos = (coord_3d){2.0, 3.5, 0.0};
    rendinf.campos.y = 1.5;
    initInput();
    float opm = posmult;
    float orm = rotmult;
    float rmult = rotmult;
    initNoiseTable();
    while (!quitRequest) {
        uint64_t starttime = altutime();
        glfwPollEvents();
        //testInput();
        input_info input = getInput();
        rendinf.camrot.x += input.mymov * mousesns * rmult / rotmult;
        rendinf.camrot.y -= input.mxmov * mousesns * rmult / rotmult;
        if (rendinf.camrot.y < -360) rendinf.camrot.y += 360;
        else if (rendinf.camrot.y > 360) rendinf.camrot.y -= 360;
        if (rendinf.camrot.x > 89.99) rendinf.camrot.x = 89.99;
        if (rendinf.camrot.x < -89.99) rendinf.camrot.x = -89.99;
        updateCam();
        //printf("[%f]\n", rendinf.camrot.y);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //putchar('\n');
        for (uint32_t i = 0; i < 48; ++i) {
            for (uint32_t j = 0; j < 48; ++j) {
                double s = perlin2d((double)(j) / 7, (double)(i) / 7, 1.0, 1);
                s *= 4;
                //printf("[%f]\n", s);
                renderModelAt(m1, (coord_3d){0.0 - (float)j, -1.0 + (int)s, 0.0 - (float)i}, false);
            }
        }
        //putchar('\n');
        renderModel(m2, false);
        renderModel(m3, false);
        renderModel(m4, false);
        glfwSwapInterval(rendinf.vsync);
        glfwSwapBuffers(rendinf.window);
        fpsmult = (float)(altutime() - starttime) / (1000000.0f / 60.0f);
        posmult = opm * fpsmult;
        rotmult = orm * fpsmult;
    }
}
