#include "main.h"
#include "game.h"
#include <common.h>
#include <resource.h>
#include <bmd.h>
#include <renderer.h>
#include <stb_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/*
float vertices[] = {
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
};
float verticesn[] = {
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  0.0f,  0.0f, -1.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f,  0.0f,  0.0f, -1.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f,  0.0f, -1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  0.0f, -1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f,  0.0f,  1.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  0.0f,  1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  0.0f,  0.0f,  1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,  0.0f,  0.0f,  1.0f,
    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, -1.0f,  0.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  1.0f,  0.0f,  0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  1.0f,  0.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  1.0f,  0.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, -1.0f,  0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f, -1.0f,  0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  1.0f,  0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,  0.0f,  1.0f,  0.0f,
};
uint32_t indices[] = {
     0,  1,  2,
     2,  3,  0,
     4,  5,  6,
     6,  7,  4,
     8,  9, 10,
    10, 11,  8,
    12, 13, 14,
    14, 15, 12,
    16, 17, 18,
    18, 19, 16,
    20, 21, 22,
    22, 23, 20,
};
*/

uint32_t indices[] = {
    0, 1, 2,
    2, 3, 0,
};

float vertices[6][20] = {
    { 0.5,  0.5,  0.5,  0.0,  1.0, 
     -0.5,  0.5,  0.5,  1.0,  1.0, 
     -0.5, -0.5,  0.5,  1.0,  0.0, 
      0.5, -0.5,  0.5,  0.0,  0.0,},
    {-0.5,  0.5, -0.5,  0.0,  1.0, 
      0.5,  0.5, -0.5,  1.0,  1.0, 
      0.5, -0.5, -0.5,  1.0,  0.0, 
     -0.5, -0.5, -0.5,  0.0,  0.0,},
    {-0.5,  0.5,  0.5,  0.0,  1.0, 
     -0.5,  0.5, -0.5,  1.0,  1.0, 
     -0.5, -0.5, -0.5,  1.0,  0.0, 
     -0.5, -0.5,  0.5,  0.0,  0.0,},
    { 0.5,  0.5, -0.5,  0.0,  1.0, 
      0.5,  0.5,  0.5,  1.0,  1.0, 
      0.5, -0.5,  0.5,  1.0,  0.0, 
      0.5, -0.5, -0.5,  0.0,  0.0,},
    {-0.5,  0.5,  0.5,  0.0,  1.0, 
      0.5,  0.5,  0.5,  1.0,  1.0, 
      0.5,  0.5, -0.5,  1.0,  0.0, 
     -0.5,  0.5, -0.5,  0.0,  0.0,},
    {-0.5, -0.5, -0.5,  0.0,  1.0, 
      0.5, -0.5, -0.5,  1.0,  1.0, 
      0.5, -0.5,  0.5,  1.0,  0.0, 
     -0.5, -0.5,  0.5,  0.0,  0.0,},
};

char* config;
file_data config_filedata;
int quitRequest = 0;

void sigh(int sig) {
    (void)sig;
    ++quitRequest;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    signal(SIGINT, sigh);
    while (!(config_filedata = getTextFile("config.cfg")).data) {
        FILE* fp = fopen("config.cfg", "w");
        fclose(fp);
    }
    config = (char*)config_filedata.data;
    /*
    uint32_t size = 0;
    bmd_data bmd;
    memset(&bmd, 0, sizeof(bmd));
    bmd.parts = 6;
    bmd.part = malloc(6 * sizeof(bmd_part));
    for (int i = 0; i < 6; ++i) {
        bmd.part[i].isize = 6 * sizeof(uint32_t);
        bmd.part[i].indices = indices;
        bmd.part[i].vsize = 20 * sizeof(float);
        bmd.part[i].vertices = vertices[i];
    }
    unsigned char* data = createBMD(&bmd, &size);
    //printf("[%u]\n", size);
    FILE* file = fopen("resources/base/game/models/block/default.bmd", "wb");
    fwrite(data, 1, size, file);
    fclose(file);
    free(data);
    bmd_data bmd2;
    temploadBMD(getBinFile("resources/base/game/models/block/default.bmd"), &bmd2);
    for (unsigned i = 0; i < bmd2.parts; ++i) {
        printf("indices[%u]:\n", i);
        for (unsigned i = 0; i < bmd2.part[i].isize / sizeof(uint32_t) / 3; ++i) {
            printf("\t[%u] [%u] [%u]\n",
                bmd2.part[i].indices[i * 3],
                bmd2.part[i].indices[i * 3 + 1],
                bmd2.part[i].indices[i * 3 + 2]);
        }
        printf("vertices[%u]:\n", i);
        for (unsigned i = 0; i < bmd2.part[i].vsize / sizeof(float) / 5; ++i) {
            printf("\t[%lf] [%lf] [%lf] [%lf] [%lf]\n",
                bmd2.part[i].vertices[i * 5],
                bmd2.part[i].vertices[i * 5 + 1],
                bmd2.part[i].vertices[i * 5 + 2],
                bmd2.part[i].vertices[i * 5 + 3],
                bmd2.part[i].vertices[i * 5 + 4]);
        }
    }
    freeBMD(&bmd2);
    free(bmd.part);
    */
    /*
    if (argc > 1) setRandSeed(atoi(argv[1]));
    for (int i = 0; i < 512; ++i) {
        printf("[%03u] ", (uint8_t)getRandByte());
        if ((i % 16) == 15) putchar('\n');
    }
    */
    /*
    resdata_file* info = loadResource(RESOURCE_TEXTFILE, "game/models/block/default.bmd");
    printf("file data [%ld]:\n%s\n", info->size, info->data);
    resdata_file* info2 = loadResource(RESOURCE_TEXTFILE, "info.inf");
    printf("pointers: [%lu] vs [%lu]\n", (uintptr_t)info, (uintptr_t)info2);
    */
    /*
    uint32_t* ia = NULL;
    uint32_t is = 0;
    float* va = NULL;
    uint32_t vs = 0;
    temploadBMD(getBinFile("resources/base/game/models/block/default.bmd"), &ia, &is, &va, &vs);
    printf("indices:\n");
    for (int i = 0; i < is / 3 / sizeof(uint32_t); ++i) {
        printf("\t[%u] [%u] [%u]\n", ia[i * 3], ia[i * 3 + 1], ia[i * 3 + 2]);
    }
    printf("vertices:\n");
    for (int i = 0; i < vs / 5 / sizeof(float); ++i) {
        printf("\t[%lf] [%lf] [%lf] [%lf] [%lf]\n", va[i * 5], va[i * 5 + 1], va[i * 5 + 2], va[i * 5 + 3], va[i * 5 + 4]);
    }
    */
    stbi_set_flip_vertically_on_load(true);
    if (!initRenderer()) return 1;
    //testRenderer();
    doGame();
    freeAllResources();
    quitRenderer();
    freeFile(config_filedata);
    return 0;
}
