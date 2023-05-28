#if defined(MODULE_GAME)

#ifndef GRAPHICS_RENDERER_H
#define GRAPHICS_RENDERER_H

#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
    #include "glad.h"
#endif
#include <common/resource.h>

#if defined(USESDL2)
    #if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
        #include <SDL2/SDL.h>
    #else
        #include <SDL.h>
        #include <GLES3/gl3.h>
    #endif
#else
    #include <GLFW/glfw3.h>
#endif
#if defined(USEGLES) && (defined(__EMSCRIPTEN__) || defined(__ANDROID__))
    #define glFramebufferTexture(a, b, c, d) glFramebufferTexture2D((a), (b), GL_TEXTURE_2D, (c), (d));
    #define glPolygonMode(a, b)
    #define GL_CLAMP_TO_BORDER GL_REPEAT
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
    #endif
    unsigned win_width;
    unsigned win_height;
    float win_fps;
    unsigned full_width;
    unsigned full_height;
    float full_fps;
    unsigned disp_width;
    unsigned disp_height;
    float disphz;
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
    struct chunkdata* chunks;
    coord_3d campos;
    coord_3d camrot;
    float camfov;
    float camnear;
    float camfar;
    float aspect;
    GLuint shaderprog;
};

enum {
    CVIS_UP,
    CVIS_RIGHT,
    CVIS_FRONT,
    CVIS_BACK,
    CVIS_LEFT,
    CVIS_DOWN
};

struct chunk_renddata {
    bool init;
    uint64_t updateid;
    unsigned VBO[2];
    uint32_t vcount[2];
    uint32_t* vertices[2];
    uint32_t tcount[2];
    uint32_t* sortvert;
    bool remesh[2];
    uint32_t yvoff[32];
    uint32_t yvcount[32];
    uint32_t ytoff[32];
    uint32_t ytcount[32];
    uint8_t vispass[32][6][6];
    uint32_t visible;
    bool visfull:1;
    bool ready:1;
    bool buffered:1;
    bool generated:1;
    bool requested:1;
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

enum {
    CHUNKUPDATE_PRIO_LOW,
    CHUNKUPDATE_PRIO_NORMAL,
    CHUNKUPDATE_PRIO_HIGH,
    CHUNKUPDATE_PRIO__MAX,
};

extern struct renderer_info rendinf;
extern int MESHER_THREADS;
extern int MESHER_THREADS_MAX;
extern bool rendergame;

bool initRenderer(void);
bool startRenderer(void);
bool reloadRenderer(void);
void stopRenderer(void);
void setRendererMode(int);
void updateCam(void);
void updateScreen(void);
void setMeshChunks(void*);
void updateChunks(void);
void sortChunk(int32_t, int, int, bool);
void startMesher(void);
void stopMesher(void);
void updateChunk(int64_t, int64_t, int, int);
void setMeshChunkOff(int64_t, int64_t);
void render(void);
void setSkyColor(float, float, float);
void setNatColor(float, float, float);
void setScreenMult(float, float, float);
void setVisibility(float, float);
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
