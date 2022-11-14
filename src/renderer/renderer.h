#if MODULEID == MODULEID_GAME

#ifndef RENDERER_RENDERER_H

#ifndef RENDERER_H_STUB

#define RENDERER_RENDERER_H

#include "glad.h"
#include <common/resource.h>

#if defined(USESDL2)
#include <SDL2/SDL.h>
#else
#include <GLFW/glfw3.h>
#endif

#include <stdbool.h>
#include <pthread.h>

typedef struct {
    float x;
    float y;
    float z;
} coord_3d;

typedef struct {
    double x;
    double y;
    double z;
} coord_3d_dbl;

typedef struct {
    float r;
    float g;
    float b;
    float a;
} color;

struct renderer_info {
    #if defined(USESDL2)
    SDL_GLContext context;
    SDL_GLContext mainctx;
    SDL_GLContext threadctx;
    #endif
    unsigned win_width;
    unsigned win_height;
    unsigned win_fps;
    unsigned full_width;
    unsigned full_height;
    unsigned full_fps;
    unsigned disp_width;
    unsigned disp_height;
    unsigned disphz;
    unsigned width;
    unsigned height;
    unsigned fps;
    bool vsync;
    bool fullscr;
    #if defined(USESDL2)
    SDL_DisplayMode monitor;
    SDL_Window* window;
    #else
    GLFWmonitor* monitor;
    GLFWwindow* window;
    #endif
    coord_3d campos;
    coord_3d camrot;
    float camfov;
    float camnear;
    float camfar;
    float aspect;
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
    unsigned VBO;
    uint32_t vcount;
    uint32_t* vertices;
    unsigned VBO2;
    uint32_t vcount2;
    uint32_t* vertices2;
    unsigned VBO3;
    uint32_t vcount3;
    uint32_t* vertices3;
    uint64_t updateid;
    bool ready:1;
    bool buffered:1;
    bool generated:1;
    bool visible:1;
};

struct ui_renddata {
    unsigned VBO;
    uint32_t vcount;
};

enum {
    SPACE_NORMAL,
    SPACE_UNDERWATER,
    SPACE_UNDERWORLD,
};

#endif

typedef unsigned int texture_t;

#ifndef RENDERER_H_STUB

extern struct renderer_info rendinf;
extern pthread_mutex_t uclock;
extern int MESHER_THREADS;
extern int MESHER_THREADS_MAX;

bool initRenderer(void);
bool startRenderer(void);
void stopRenderer(void);
void createTexture(unsigned char*, resdata_texture*);
void destroyTexture(resdata_texture*);
struct model* loadModel(char*, char**);
void updateCam(void);
void updateScreen(void);
void setMeshChunks(void*);
void updateChunks(void);
void startMesher(void);
void updateChunk(int64_t, int64_t, bool);
void setMeshChunkOff(int64_t, int64_t);
void render(void);
void setSkyColor(float, float, float);
void setScreenMult(float, float, float);
void setVisibility(int, float);
void setFullscreen(bool);
#if defined(USESDL2)
void sdlreszevent(int, int);
#endif

extern struct renderer_info rendinf;

#define GFX_DEFAULT_POS (coord_3d){0.0, 0.0, 0.0}
#define GFX_DEFAULT_ROT (coord_3d){0.0, 0.0, 0.0}
#define GFX_DEFAULT_SCALE (coord_3d){1.0, 1.0, 1.0}
#define GFX_DEFAULT_MAT4 {{1.0, 0.0, 0.0, 0.0}, {0.0, 1.0, 0.0, 0.0}, {0.0, 0.0, -1.0, 0.0}, {0.0, 0.0, 0.0, 1.0}}

#endif

#endif

#endif
