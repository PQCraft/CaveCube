#ifndef RENDERER_H

#ifndef RENDERER_H_STUB

#define RENDERER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <resource.h>
//#include <game.h>

typedef struct {
    float x;
    float y;
    float z;
} coord_3d;

typedef struct {
    float r;
    float g;
    float b;
} color;

typedef struct {
    unsigned int win_width;
    unsigned int win_height;
    unsigned int win_fps;
    unsigned int full_width;
    unsigned int full_height;
    unsigned int full_fps;
    unsigned int width;
    unsigned int height;
    unsigned int fps;
    bool vsync;
    bool fullscr;
    GLFWmonitor* monitor;
    GLFWwindow* window;
    coord_3d campos;
    coord_3d camrot;
    float camfov;
    GLuint shaderprog;
} renderer_info;

typedef struct {
    unsigned VAO;
    unsigned VBO;
    unsigned EBO;
    resdata_texture* texture;
} model_renddata;

typedef struct {
    coord_3d pos;
    coord_3d rot;
    coord_3d scale;
    resdata_bmd* model;
    model_renddata* renddata;
} model;

typedef struct {
    //unsigned VAO;
    unsigned VBO;
    uint32_t vcount;
    uint32_t* vertices;
    coord_3d pos;
} chunk_renddata;

#endif

typedef unsigned int texture_t;

#ifndef RENDERER_H_STUB

extern renderer_info rendinf;

bool initRenderer(void);
void quitRenderer(void);
int rendererQuitRequest(void);
void createTexture(unsigned char*, resdata_texture*);
void destroyTexture(resdata_texture*);
model* loadModel(char*, char**);
void renderModelAt(model*, coord_3d, bool);
void renderModel(model*, bool);
void renderPartAt(model*, unsigned, coord_3d, bool);
void renderPart(model*, unsigned, bool);
void updateCam(void);
void updateScreen(void);

void updateChunks(void*);
void renderChunks(void*);

#define GFX_DEFAULT_POS (coord_3d){0.0, 0.0, 0.0}
#define GFX_DEFAULT_ROT (coord_3d){0.0, 0.0, 0.0}
#define GFX_DEFAULT_SCALE (coord_3d){1.0, 1.0, 1.0}
#define GFX_DEFAULT_MAT4 {{1.0, 0.0, 0.0, 0.0}, {0.0, 1.0, 0.0, 0.0}, {0.0, 0.0, -1.0, 0.0}, {0.0, 0.0, 0.0, 1.0}}

extern renderer_info rendinf;

#endif

#endif
