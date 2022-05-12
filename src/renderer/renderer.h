#ifndef RENDERER_H

#ifndef RENDERER_H_STUB

#define RENDERER_H

#include <glad.h>
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

struct renderer_info {
    unsigned int win_width;
    unsigned int win_height;
    unsigned int win_fps;
    unsigned int full_width;
    unsigned int full_height;
    unsigned int full_fps;
    int mon_x;
    int mon_y;
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
};

struct model_renddata {
    unsigned VAO;
    unsigned VBO;
    unsigned EBO;
    resdata_texture* texture;
};

struct model {
    coord_3d pos;
    coord_3d rot;
    coord_3d scale;
    resdata_bmd* model;
    struct model_renddata* renddata;
};

struct chunk_renddata {
    //unsigned VAO;
    unsigned VBO;
    uint32_t vcount;
    uint32_t* vertices;
    unsigned VBO2;
    uint32_t vcount2;
    uint32_t* vertices2;
    //bool sent:1;
    //bool moved:1;
    //bool busy:1;
    bool updated:1;
    bool generated:1;
};

enum {
    SPACE_NORMAL,
    SPACE_UNDERWATER,
    SPACE_PARALLEL,
};

#endif

typedef unsigned int texture_t;

#ifndef RENDERER_H_STUB

extern struct renderer_info rendinf;

bool initRenderer(void);
void quitRenderer(void);
int rendererQuitRequest(void);
void createTexture(unsigned char*, resdata_texture*);
void destroyTexture(resdata_texture*);
struct model* loadModel(char*, char**);
//void renderModelAt(struct model*, coord_3d, bool);
//void renderModel(struct model*, bool);
//void renderPartAt(struct model*, unsigned, coord_3d, bool);
//void renderPart(struct model*, unsigned, bool);
void updateCam(void);
void updateScreen(void);
bool updateChunks(void*);
void renderChunks(void*);
void setSkyColor(float, float, float);
void setScreenMult(float, float, float);
void setSpace(int);

#define GFX_DEFAULT_POS (coord_3d){0.0, 0.0, 0.0}
#define GFX_DEFAULT_ROT (coord_3d){0.0, 0.0, 0.0}
#define GFX_DEFAULT_SCALE (coord_3d){1.0, 1.0, 1.0}
#define GFX_DEFAULT_MAT4 {{1.0, 0.0, 0.0, 0.0}, {0.0, 1.0, 0.0, 0.0}, {0.0, 0.0, -1.0, 0.0}, {0.0, 0.0, 0.0, 1.0}}

extern struct renderer_info rendinf;

#endif

#endif
