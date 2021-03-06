#ifndef SERVER

#include <main/main.h>
#include "renderer.h"
#include <common/common.h>
#include <common/resource.h>
#include <common/noise.h>
#include <input/input.h>
#include <game/game.h>
#include <game/chunk.h>
#include <game/blocks.h>

#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include "glad_platform.h"
#if defined(USESDL2)
    #include <SDL2/SDL.h>
#else
    #include <GLFW/glfw3.h>
#endif
#include "cglm/cglm.h"

#ifndef _WIN32
    #define GLTEAREXT GLX_EXT_swap_control_tear
#else
    #define GLTEAREXT WGL_EXT_swap_control_tear
#endif

#ifdef RENDERER_SINGLECORE
    #define GETCONTEXT()
    #define RELEASECONTEXT()
#else
    #if defined(USESDL2)
        #define _GETCONTEXT_FUNC() {SDL_GL_MakeCurrent(rendinf.window, rendinf.context);}
    #else
        #define _GETCONTEXT_FUNC() {glfwMakeContextCurrent(rendinf.window);}
    #endif
    #ifdef RENDERER_LAZY
        #define _RELEASECONTEXT_FUNC()
    #else
        #if defined(USESDL2)
            #define _RELEASECONTEXT_FUNC() {SDL_GL_MakeCurrent(rendinf.window, NULL);}
        #else
            #define _RELEASECONTEXT_FUNC() {glfwMakeContextCurrent(NULL);}
        #endif
    #endif
    #define GETCONTEXT() {pthread_mutex_lock(&gllock); _GETCONTEXT_FUNC();}
    #define RELEASECONTEXT() {pthread_mutex_unlock(&gllock); _RELEASECONTEXT_FUNC();}
#endif

int MESHER_THREADS;

struct renderer_info rendinf;
//static resdata_bmd* blockmodel;

static pthread_mutex_t gllock;

void setMat4(GLuint prog, char* name, mat4 val) {
    glUniformMatrix4fv(glGetUniformLocation(prog, name), 1, GL_FALSE, *val);
}

void setUniform3f(GLuint prog, char* name, float val[3]) {
    glUniform3f(glGetUniformLocation(prog, name), val[0], val[1], val[2]);
}

void setUniform4f(GLuint prog, char* name, float val[4]) {
    glUniform4f(glGetUniformLocation(prog, name), val[0], val[1], val[2], val[3]);
}

static void setShaderProg(GLuint s) {
    rendinf.shaderprog = s;
    glUseProgram(rendinf.shaderprog);
}

static color sky;

void setSkyColor(float r, float g, float b) {
    sky.r = r;
    sky.g = g;
    sky.b = b;
    glClearColor(r, g, b, 1);
    setUniform3f(rendinf.shaderprog, "skycolor", (float[]){r, g, b});
}

void setScreenMult(float r, float g, float b) {
    setUniform3f(rendinf.shaderprog, "mcolor", (float[]){r, g, b});
}

static int curspace = -1;

void setSpace(int space) {
    if (space == curspace) return;
    curspace = space;
    GETCONTEXT();
    switch (space) {
        default:;
            setSkyColor(0, 0.7, 0.9);
            //setSkyColor(0, 0.01, 0.025);
            setScreenMult(1, 1, 1);
            glUniform1i(glGetUniformLocation(rendinf.shaderprog, "vis"), 3);
            glUniform1f(glGetUniformLocation(rendinf.shaderprog, "vismul"), 1.0);
            break;
        case SPACE_UNDERWATER:;
            setSkyColor(0, 0.33, 0.75);
            setScreenMult(0.4, 0.55, 0.8);
            glUniform1i(glGetUniformLocation(rendinf.shaderprog, "vis"), -7);
            glUniform1f(glGetUniformLocation(rendinf.shaderprog, "vismul"), 0.85);
            break;
        case SPACE_UNDERWORLD:;
            setSkyColor(0.25, 0, 0.05);
            setScreenMult(0.9, 0.75, 0.7);
            glUniform1i(glGetUniformLocation(rendinf.shaderprog, "vis"), -10);
            glUniform1f(glGetUniformLocation(rendinf.shaderprog, "vismul"), 0.8);
    }
    RELEASECONTEXT();
}

static float uc_fov = -1.0, uc_asp = -1.0;
static float uc_rotradx, uc_rotrady;
static mat4 uc_projection __attribute__((aligned (32)));
static mat4 uc_view __attribute__((aligned (32)));
static vec3 uc_direction __attribute__((aligned (32)));
static vec3 uc_up __attribute__((aligned (32)));
static vec3 uc_front __attribute__((aligned (32)));
static bool uc_uproj = false;

void updateCam() {
    GETCONTEXT();
    if (rendinf.aspect != uc_asp) {uc_asp = rendinf.aspect; uc_uproj = true;}
    if (rendinf.camfov != uc_fov) {uc_fov = rendinf.camfov; uc_uproj = true;}
    if (uc_uproj) {
        glm_perspective(uc_fov * M_PI / 180.0, uc_asp, 0.05, 1024.0, uc_projection);
        setMat4(rendinf.shaderprog, "projection", uc_projection);
        uc_uproj = false;
    }
    uc_rotradx = rendinf.camrot.x * M_PI / 180.0;
    uc_rotrady = (rendinf.camrot.y - 90.0) * M_PI / 180.0;
    uc_direction[0] = cos(uc_rotrady) * cos(uc_rotradx);
    uc_direction[1] = sin(uc_rotradx);
    uc_direction[2] = sin(uc_rotrady) * cos(uc_rotradx);
    uc_up[0] = 0;
    uc_up[1] = 1;
    uc_up[2] = 0;
    uc_front[0] = uc_direction[0];
    uc_front[1] = uc_direction[1];
    uc_front[2] = uc_direction[2];
    uc_direction[0] += rendinf.campos.x;
    uc_direction[1] += rendinf.campos.y;
    uc_direction[2] += rendinf.campos.z;
    glm_vec3_rotate(uc_up, rendinf.camrot.z * M_PI / 180.0, uc_front);
    glm_lookat((vec3){rendinf.campos.x, rendinf.campos.y, rendinf.campos.z},
        uc_direction, uc_up, uc_view);
    setMat4(rendinf.shaderprog, "view", uc_view);
    RELEASECONTEXT();
}

void setFullscreen(bool fullscreen) {
    static int winox, winoy = 0;
    if (fullscreen) {
        rendinf.aspect = (float)rendinf.full_width / (float)rendinf.full_height;
        rendinf.width = rendinf.full_width;
        rendinf.height = rendinf.full_height;
        rendinf.fps = rendinf.full_fps;
        #if defined(USESDL2)
        SDL_GetWindowPosition(rendinf.window, &winox, &winoy);
        SDL_SetWindowFullscreen(rendinf.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        #else
        glfwGetWindowPos(rendinf.window, &winox, &winoy);
        glfwSetWindowMonitor(rendinf.window, rendinf.monitor, 0, 0, rendinf.full_width, rendinf.full_height, rendinf.full_fps);
        #endif
        rendinf.fullscr = true;
    } else {
        rendinf.aspect = (float)rendinf.win_width / (float)rendinf.win_height;
        rendinf.width = rendinf.win_width;
        rendinf.height = rendinf.win_height;
        rendinf.fps = rendinf.win_fps;
        #if defined(USESDL2)
        #else
        int twinx, twiny;
        #endif
        if (rendinf.fullscr) {
            #if defined(USESDL2)
            SDL_SetWindowFullscreen(rendinf.window, 0);
            SDL_SetWindowPosition(rendinf.window, winox, winoy);
            #else
            uint64_t offset = altutime();
            glfwSetWindowMonitor(rendinf.window, NULL, 0, 0, rendinf.win_width, rendinf.win_height, rendinf.win_fps);
            do {
                glfwSetWindowPos(rendinf.window, winox, winoy);
                glfwGetWindowPos(rendinf.window, &twinx, &twiny);
            } while (altutime() - offset < 3000000 && (twinx != winox || twiny != winoy));
            #endif
        }
        rendinf.fullscr = false;
    }
    updateCam();
}

static inline bool makeShaderProg(char* vstext, char* fstext, GLuint* p) {
    if (!vstext || !fstext) return false;
    GLuint vertexHandle = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexHandle, 1, (const GLchar * const*)&vstext, NULL);
    glCompileShader(vertexHandle);
    GLint ret = GL_FALSE;
    glGetShaderiv(vertexHandle, GL_COMPILE_STATUS, &ret);
    if (!ret) {
        GLint logSize = 0;
        glGetShaderiv(vertexHandle, GL_INFO_LOG_LENGTH, &logSize);
        GLchar* log = malloc((logSize + 1) * sizeof(GLchar));
        glGetShaderInfoLog(vertexHandle, logSize, &logSize, log);
        log[logSize - 1] = 0;
        fprintf(stderr, "Vertex shader compile error: %s\n", (char*)log);
        free(log);
        glDeleteShader(vertexHandle);
        goto retfalse;
    }
    GLuint fragHandle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragHandle, 1, (const GLchar * const*)&fstext, NULL);
    glCompileShader(fragHandle);
    glGetShaderiv(fragHandle, GL_COMPILE_STATUS, &ret);
    if (!ret) {
        GLint logSize = 0;
        glGetShaderiv(fragHandle, GL_INFO_LOG_LENGTH, &logSize);
        GLchar* log = malloc((logSize + 1) * sizeof(GLchar));
        glGetShaderInfoLog(fragHandle, logSize, &logSize, log);
        log[logSize - 1] = 0;
        fprintf(stderr, "Fragment shader compile error: %s\n", (char*)log);
        free(log);
        glDeleteShader(vertexHandle);
        glDeleteShader(fragHandle);
        goto retfalse;
    }
    *p = glCreateProgram();
    glAttachShader(*p, vertexHandle);
    glAttachShader(*p, fragHandle);
    glLinkProgram(*p);
    glGetProgramiv(*p, GL_LINK_STATUS, &ret);
    glDeleteShader(vertexHandle);
    glDeleteShader(fragHandle);
    if (!ret) {
        GLint logSize = 0;
        glGetProgramiv(*p, GL_INFO_LOG_LENGTH, &logSize);
        GLchar* log = malloc((logSize + 1) * sizeof(GLchar));
        glGetProgramInfoLog(*p, logSize, &logSize, log);
        log[logSize - 1] = 0;
        fprintf(stderr, "Shader program link error: %s\n", (char*)log);
        free(log);
        goto retfalse;
    }
    return true;
    retfalse:;
    return false;
}

//extern float vertices[];
//extern uint32_t indices[];

struct model* loadModel(char* mpath, char** tpath) {
    resdata_bmd* mdata = loadResource(RESOURCE_BMD, mpath);
    struct model* m = malloc(sizeof(struct model));
    //memset(m, 0, sizeof(struct model));
    m->model = mdata;
    m->pos = (coord_3d){0.0, 0.0, 0.0};
    m->rot = (coord_3d){0.0, 0.0, 0.0};
    m->scale = (coord_3d){1.0, 1.0, 1.0};
    m->renddata = malloc(mdata->parts * sizeof(struct model_renddata));
    for (unsigned i = 0; i < mdata->parts; ++i) {
        resdata_texture* tdata = loadResource(RESOURCE_TEXTURE, *tpath++);
        m->renddata[i].texture = tdata;
        glGenVertexArrays(1, &m->renddata[i].VAO);
        glGenBuffers(1, &m->renddata[i].VBO);
        glGenBuffers(1, &m->renddata[i].EBO);
        glBindVertexArray(m->renddata[i].VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m->renddata[i].VBO);
        glBufferData(GL_ARRAY_BUFFER, m->model->part[i].vsize, m->model->part[i].vertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->renddata[i].EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m->model->part[i].isize, m->model->part[i].indices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    return m;
}

void createTexture(unsigned char* data, resdata_texture* tdata) {
    glGenTextures(1, &tdata->data);
    glBindTexture(GL_TEXTURE_2D, tdata->data);
    GLenum colors = GL_RGB;
    if (tdata->channels == 1) colors = GL_RED;
    else if (tdata->channels == 4) colors = GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, colors, tdata->width, tdata->height, 0, colors, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void destroyTexture(resdata_texture* tdata) {
    glDeleteTextures(1, &tdata->data);
}

/*
void renderPartAt(struct model* m, unsigned i, coord_3d pos, bool advanced) {
    //glBindVertexArray(m->renddata[i].VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m->renddata[i].VBO);
    //glBufferData(GL_ARRAY_BUFFER, m->model->part[i].vsize, m->model->part[i].vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->renddata[i].EBO);
    //glBufferData(GL_ELEMENT_ARRAY_BUFFER, m->model->part[i].isize, m->model->part[i].indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    //mat4 struct model __attribute__((aligned (32))) = GFX_DEFAULT_MAT4;
    //glm_translate(struct model, (vec3){m->pos.x + pos.x, m->pos.y + pos.y, m->pos.z + pos.z});
    if (advanced) {
        glm_rotate(struct model, m->rot.x * M_PI / 180, (vec3){1, 0, 0});
        glm_rotate(struct model, m->rot.y * M_PI / 180, (vec3){0, 1, 0});
        glm_rotate(struct model, m->rot.z * M_PI / 180, (vec3){0, 0, 1});
        glm_scale(struct model, (vec3){m->scale.x, m->scale.y, m->scale.z});
    }
    setUniform3f(rendinf.shaderprog, "mPos", (float[]){m->pos.x + pos.x, m->pos.y + pos.y, m->pos.z + pos.z});
    //setMat4(rendinf.shaderprog, "struct model", struct model);
    //glBindTexture(GL_TEXTURE_2D, m->renddata[i].texture->data);
    //glUniform1i(glGetUniformLocation(rendinf.shaderprog, "TexData"), 0);
    glDrawElements(GL_TRIANGLES, m->model->part[i].isize / sizeof(uint32_t), GL_UNSIGNED_INT, 0);
    //glBindTexture(GL_TEXTURE_2D, 0);
    //glBindBuffer(GL_ARRAY_BUFFER, 0);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void renderPart(struct model* m, unsigned i, bool advanced) {
    renderPartAt(m, i, (coord_3d){0.0, 0.0, 0.0}, advanced);
}

void renderModelAt(struct model* m, coord_3d pos, bool advanced) {
    for (unsigned i = 0; i < m->model->parts; ++i) {
        renderPartAt(m, i, pos, advanced);
    }
}

void renderModel(struct model* m, bool advanced) {
    renderModelAt(m, (coord_3d){0.0, 0.0, 0.0}, advanced);
}
*/

void updateScreen() {
    static int lv = -1;
    GETCONTEXT();
    if (rendinf.vsync != lv) {
        #if defined(USESDL2)
        SDL_GL_SetSwapInterval(rendinf.vsync * ((GLTEAREXT) ? -1 : 1));
        #else
        glfwSwapInterval(rendinf.vsync * ((GLTEAREXT) ? -1 : 1));
        #endif
        lv = rendinf.vsync;
    }
    #if defined(USESDL2)
    SDL_GL_SwapWindow(rendinf.window);
    #else
    glfwSwapBuffers(rendinf.window);
    #endif
    RELEASECONTEXT();
}

static uint32_t maxblockid = 0;

static unsigned VAO;
static unsigned VBO2D;

static GLuint shader_block;
static GLuint shader_2d;
static GLuint shader_text;

static struct blockdata rendGetBlock(struct chunkdata* data, int32_t c, int x, int y, int z) {
    //x += data->info.dist;
    //z += data->info.dist;
    while (x < 0 && c % data->info.width) {c -= 1; x += 16;}
    while (x > 15 && (c + 1) % data->info.width) {c += 1; x -= 16;}
    while (z > 15 && c % (int)data->info.widthsq >= (int)data->info.width) {c -= data->info.width; z -= 16;}
    while (z < 0 && c % (int)data->info.widthsq < (int)(data->info.widthsq - data->info.width)) {c += data->info.width; z += 16;}
    while (y < 0 && c >= (int)data->info.widthsq) {c -= data->info.widthsq; y += 16;}
    while (y > 15 && c < (int)(data->info.size - data->info.widthsq)) {c += data->info.widthsq; y -= 16;}
    if (c < 0 || x < 0 || y < 0 || z < 0 || x > 15 || z > 15) return (struct blockdata){255, 0, 0};
    if (c >= (int32_t)data->info.size || y > 15) return (struct blockdata){0, 0, 0};
    //while (!data->renddata[c].generated) {}
    if (!data->renddata[c].generated) return (struct blockdata){255, 0, 0};
    //return (struct blockdata){0, 0, 0 ,0};
    //printf("block [%d, %d, %d]: [%d]\n", x, y, z, y * 225 + (z % 15) * 15 + (x % 15));
    //printf("[%d] [%d]: [%d]\n", x, z, ((x / 15) % data->info.width) + ((x / 15) / data->info.width));
    struct blockdata ret = data->data[c][y * 256 + z * 16 + x];
    return ret;
}

/*
uint16_t getr5g6b5(float r, float g, float b) {
    return (((uint8_t)((float)((r + 1.0 / 62.0) * 31.0)) & 31) << 11) |
           (((uint8_t)((float)((g + 1.0 / 126.0) * 63.0)) & 63) << 5) |
           ((uint8_t)((float)((b + 1.0 / 62.0) * 31.0)) & 31);
}
*/

// old:
// [1 bit: x + 1][4 bits: x][1 bit: y + 1][4 bits: y][1 bit: z + 1][4 bits: z][3 bits: tex map][2 bits: tex coords][4 bits: lighting][8 bits: block id]
// current:
// [8 bits: x ([0...255]/16)][8 bits: y ([0...255]/16)][8 bits: z ([0...255]/16)][1 bit: x + 1/16][1 bit: y + 1/16][1 bit: z + 1/16][3 bits: tex map][2 bits: tex coords]
// [8 bits: block id][4 bits: lighting][20 bits: reserved]
static uint32_t constBlockVert[6][6] = {
    /*
    {0x84201000, 0x04203000, 0x00202000, 0x00202000, 0x80200000, 0x84201000},
    {0x04001000, 0x84003000, 0x80002000, 0x80002000, 0x00000000, 0x04001000},
    {0x04201000, 0x04003000, 0x00002000, 0x00002000, 0x00200000, 0x04201000},
    {0x84001000, 0x84203000, 0x80202000, 0x80202000, 0x80000000, 0x84001000},
    {0x04201000, 0x84203000, 0x84002000, 0x84002000, 0x04000000, 0x04201000},
    {0x00001000, 0x80003000, 0x80202000, 0x80202000, 0x00200000, 0x00001000},
    */
    {0x0F0F0FE1, 0x000F0F63, 0x00000F22, 0x00000F22, 0x0F000FA0, 0x0F0F0FE1},
    {0x000F0041, 0x0F0F00C3, 0x0F000082, 0x0F000082, 0x00000000, 0x000F0041},
    {0x000F0F61, 0x000F0043, 0x00000002, 0x00000002, 0x00000F20, 0x000F0F61},
    {0x0F0F00C1, 0x0F0F0FE3, 0x0F000FA2, 0x0F000FA2, 0x0F000080, 0x0F0F00C1},
    {0x000F0F61, 0x0F0F0FE3, 0x0F0F00C2, 0x0F0F00C2, 0x000F0040, 0x000F0F61},
    {0x00000001, 0x0F000083, 0x0F000FA2, 0x0F000FA2, 0x00000F20, 0x00000001},
};

static float vert2D[] = {
    -1.0,  1.0,  0.0,  1.0,
     1.0,  1.0,  1.0,  1.0,
     1.0, -1.0,  1.0,  0.0,
     1.0, -1.0,  1.0,  0.0,
    -1.0, -1.0,  0.0,  0.0,
    -1.0,  1.0,  0.0,  1.0,
};

static pthread_t pthreads[MAX_THREADS];
pthread_mutex_t uclock;
static uint8_t water;

static void* meshthread(void* args) {
    struct chunkdata* data = args;
    struct blockdata bdata;
    struct blockdata bdata2[6];
    bool activity = false;
    for (int32_t c = -1; !quitRequest; --c) {
        if (c < 0) {
            c = data->info.size - 1;
            activity = false;
        }
        pthread_mutex_lock(&uclock);
        bool cond = !data->renddata[c].generated || data->renddata[c].updated || data->renddata[c].busy || data->renddata[c].ready;
        //if (data->renddata[c].busy) printf("BUSY [%d]\n", c);
        if (!cond) {
            data->renddata[c].busy = true;
            activity = true;
        }
        //pthread_mutex_unlock(&uclock);
        if (cond) {
            pthread_mutex_unlock(&uclock);
            if (!c && !activity) microwait(25000);
            continue;
        }
        uint32_t* _vptr = malloc(147456 * sizeof(uint32_t) * 2);
        uint32_t* _vptr2 = malloc(147456 * sizeof(uint32_t) * 2);
        uint32_t* _vptr3 = malloc(147456 * sizeof(uint32_t) * 2);
        uint32_t* vptr = _vptr;
        uint32_t* vptr2 = _vptr2;
        uint32_t* vptr3 = _vptr3;
        uint32_t tmpsize = 0;
        uint32_t tmpsize2 = 0;
        uint32_t tmpsize3 = 0;
        //printf("MESHING [%d]\n", c);
        for (int y = 15; y >= 0; --y) {
            for (int z = 0; z < 16; ++z) {
                for (int x = 0; x < 16; ++x) {
                    bdata = rendGetBlock(data, c, x, y, z);
                    if (!bdata.id || bdata.id > maxblockid) continue;
                    bdata2[0] = rendGetBlock(data, c, x, y, z + 1);
                    bdata2[1] = rendGetBlock(data, c, x, y, z - 1);
                    bdata2[2] = rendGetBlock(data, c, x - 1, y, z);
                    bdata2[3] = rendGetBlock(data, c, x + 1, y, z);
                    bdata2[4] = rendGetBlock(data, c, x, y + 1, z);
                    bdata2[5] = rendGetBlock(data, c, x, y - 1, z);
                    for (int i = 0; i < 6; ++i) {
                        if (bdata2[i].id && bdata2[i].id <= maxblockid) {
                            if (!blockinf[bdata2[i].id].transparency) continue;
                            if (blockinf[bdata.id].transparency && (bdata.id == bdata2[i].id)) continue;
                        }
                        if (bdata2[i].id == 255) continue;
                        uint32_t baseVert1 = (x << 28) | (y << 20) | (z << 12) | (i << 2);
                        uint8_t light = bdata2[i].light;
                        uint32_t baseVert2 = (bdata.id << 24) | (light << 20);
                        if (bdata.id == water) {
                            if (!bdata2[i].id) {
                                for (int j = 0; j < 6; ++j) {
                                    *vptr2 = baseVert1 | constBlockVert[i][j];
                                    ++vptr2;
                                    *vptr2 = baseVert2;
                                    ++vptr2;
                                }
                                ++tmpsize2;
                            } else {
                                for (int j = 0; j < 6; ++j) {
                                    *vptr3 = baseVert1 | constBlockVert[i][j];
                                    ++vptr3;
                                    *vptr3 = baseVert2;
                                    ++vptr3;
                                }
                                ++tmpsize3;
                            }
                            //printf("added [%d][%d %d %d][%d]: [%u]: [%08X]...\n", c, x, y, z, i, (uint8_t)bdata.id, baseVert1);
                        } else {
                            for (int j = 0; j < 6; ++j) {
                                *vptr = baseVert1 | constBlockVert[i][j];
                                ++vptr;
                                *vptr = baseVert2;
                                ++vptr;
                            }
                            //printf("added [%d][%d %d %d][%d]: [%u]: [%08X]...\n", c, x, y, z, i, (uint8_t)bdata.id, baseVert1);
                            ++tmpsize;
                        }
                    }
                }
            }
        }
        tmpsize *= 6;
        tmpsize2 *= 6;
        tmpsize3 *= 6;
        data->renddata[c].vcount = tmpsize;
        data->renddata[c].vcount2 = tmpsize2;
        data->renddata[c].vcount3 = tmpsize3;
        data->renddata[c].vertices = _vptr;
        data->renddata[c].vertices2 = _vptr2;
        data->renddata[c].vertices3 = _vptr3;
        data->renddata[c].ready = true;
        #ifndef RENDERER_SINGLECORE
        tmpsize *= sizeof(uint32_t) * 2;
        tmpsize2 *= sizeof(uint32_t) * 2;
        tmpsize3 *= sizeof(uint32_t) * 2;
        GETCONTEXT();
        if (!data->renddata[c].VBO) glGenBuffers(1, &data->renddata[c].VBO);
        if (!data->renddata[c].VBO2) glGenBuffers(1, &data->renddata[c].VBO2);
        if (!data->renddata[c].VBO3) glGenBuffers(1, &data->renddata[c].VBO3);
        if (tmpsize) {
            glBindBuffer(GL_ARRAY_BUFFER, data->renddata[c].VBO);
            glBufferData(GL_ARRAY_BUFFER, tmpsize, _vptr, GL_STATIC_DRAW);
        }
        if (tmpsize2) {
            glBindBuffer(GL_ARRAY_BUFFER, data->renddata[c].VBO2);
            glBufferData(GL_ARRAY_BUFFER, tmpsize2, _vptr2, GL_STATIC_DRAW);
        }
        if (tmpsize3) {
            glBindBuffer(GL_ARRAY_BUFFER, data->renddata[c].VBO3);
            glBufferData(GL_ARRAY_BUFFER, tmpsize3, _vptr3, GL_STATIC_DRAW);
        }
        RELEASECONTEXT();
        free(_vptr);
        free(_vptr2);
        free(_vptr3);
        data->renddata[c].updated = true;
        data->renddata[c].ready = false;
        data->renddata[c].buffered = true;
        #endif
        data->renddata[c].busy = false;
        pthread_mutex_unlock(&uclock);
    }
    return NULL;
}

void updateChunks(void* vdata) {
    #ifndef RENDERER_SINGLECORE
    (void)vdata;
    #else
    struct chunkdata* data = (struct chunkdata*)vdata;
    uint32_t c2 = 0;
    pthread_mutex_lock(&uclock);
    for (uint32_t c = 0; !quitRequest && c < data->info.size; ++c) {
        if (!data->renddata[c].ready) continue;
        if (c2 >= data->info.size) break;
        ++c2;
        uint32_t tmpsize = data->renddata[c].vcount * sizeof(uint32_t) * 2;
        uint32_t tmpsize2 = data->renddata[c].vcount2 * sizeof(uint32_t) * 2;
        uint32_t tmpsize3 = data->renddata[c].vcount3 * sizeof(uint32_t) * 2;
        if (!data->renddata[c].VBO) glGenBuffers(1, &data->renddata[c].VBO);
        if (!data->renddata[c].VBO2) glGenBuffers(1, &data->renddata[c].VBO2);
        if (!data->renddata[c].VBO3) glGenBuffers(1, &data->renddata[c].VBO3);
        if (tmpsize) {
            glBindBuffer(GL_ARRAY_BUFFER, data->renddata[c].VBO);
            glBufferData(GL_ARRAY_BUFFER, tmpsize, data->renddata[c].vertices, GL_STATIC_DRAW);
        }
        if (tmpsize2) {
            glBindBuffer(GL_ARRAY_BUFFER, data->renddata[c].VBO2);
            glBufferData(GL_ARRAY_BUFFER, tmpsize2, data->renddata[c].vertices2, GL_STATIC_DRAW);
        }
        if (tmpsize3) {
            glBindBuffer(GL_ARRAY_BUFFER, data->renddata[c].VBO3);
            glBufferData(GL_ARRAY_BUFFER, tmpsize3, data->renddata[c].vertices3, GL_STATIC_DRAW);
        }
        free(data->renddata[c].vertices);
        free(data->renddata[c].vertices2);
        free(data->renddata[c].vertices3);
        data->renddata[c].updated = true;
        data->renddata[c].ready = false;
        data->renddata[c].buffered = true;
        data->renddata[c].busy = false;
    }
    pthread_mutex_unlock(&uclock);
    #endif
}

static bool setchunks = false;

void startMesher(void* vdata) {
    if (!setchunks) {
        #ifdef NAME_THREADS
        char name[256];
        char name2[256];
        #endif
        for (int i = 0; i < MESHER_THREADS && i < MAX_THREADS; ++i) {
            #ifdef NAME_THREADS
            name[0] = 0;
            name2[0] = 0;
            #endif
            pthread_create(&pthreads[i], NULL, &meshthread, vdata);
            printf("Mesher: Started thread [%d]\n", i);
            #ifdef NAME_THREADS
            pthread_getname_np(pthreads[i], name2, 256);
            sprintf(name, "%s:msh%d", name2, i);
            pthread_setname_np(pthreads[i], name);
            #endif
        }
        setchunks = true;
    }
}

void renderText(float x, float y, float scale, unsigned end, char* text, void* extinfo) {
    (void)extinfo;
    glUniform1f(glGetUniformLocation(rendinf.shaderprog, "xratio"), 8.0 / (float)rendinf.width);
    glUniform1f(glGetUniformLocation(rendinf.shaderprog, "yratio"), 16.0 / (float)rendinf.height);
    glUniform1f(glGetUniformLocation(rendinf.shaderprog, "scale"), scale);
    for (int i = 0; *text; ++i) {
        if (*text == '\n') {
            x = 0;
            y += scale * 16;
        } else {
            glUniform1ui(glGetUniformLocation(rendinf.shaderprog, "chr"), (unsigned char)(*text));
            glUniform2f(glGetUniformLocation(rendinf.shaderprog, "mpos"), x / (float)rendinf.width, y / (float)rendinf.height);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            x += scale * 8;
            if (x + 8.0 * scale > end) {x = 0; y += scale * 16;}
        }
        ++text;
    }
}

static char tbuf[1][32768];

const unsigned char* glver;
const unsigned char* glslver;
const unsigned char* glvend;
const unsigned char* glrend;

static inline coord_3d_dbl intCoord_dbl(coord_3d_dbl in) {
    in.x -= (in.x < 0) ? 1.0 : 0.0;
    in.y -= (in.y < 0) ? 1.0 : 0.0;
    in.z += (in.z > 0) ? 1.0 : 0.0;
    in.x = (int)in.x;
    in.y = (int)in.y;
    in.z = (int)in.z;
    return in;
}

static uint32_t rendc;

void renderChunks(void* vdata) {
    struct chunkdata* data = vdata;
    GETCONTEXT();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "dist"), data->info.dist);
    setUniform3f(rendinf.shaderprog, "cam", (float[]){rendinf.campos.x, rendinf.campos.y, rendinf.campos.z});
    for (rendc = 0; rendc < data->info.size; ++rendc) {
        if (!data->renddata[rendc].buffered || !data->renddata[rendc].vcount) continue;
        setUniform3f(rendinf.shaderprog, "ccoord", (float[]){(int)(rendc % data->info.width) - (int)data->info.dist,
                                                             (int)(rendc / data->info.widthsq),
                                                             (int)(rendc / data->info.width % data->info.width) - (int)data->info.dist});
        glBindBuffer(GL_ARRAY_BUFFER, data->renddata[rendc].VBO);
        glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 2 * sizeof(uint32_t), (void*)0);
        glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 2 * sizeof(uint32_t), (void*)sizeof(uint32_t));
        glDrawArrays(GL_TRIANGLES, 0, data->renddata[rendc].vcount);
    }
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "isAni"), 1);
    glUniform1ui(glGetUniformLocation(rendinf.shaderprog, "TexAni"), (altutime() / 200000) % 6);
    glDisable(GL_CULL_FACE);
    glDepthMask(false);
    for (rendc = 0; rendc < data->info.size; ++rendc) {
        if (!data->renddata[rendc].buffered || !data->renddata[rendc].vcount2) continue;
        setUniform3f(rendinf.shaderprog, "ccoord", (float[]){(int)(rendc % data->info.width) - (int)data->info.dist,
                                                             (int)(rendc / data->info.widthsq),
                                                             (int)(rendc / data->info.width % data->info.width) - (int)data->info.dist});
        glBindBuffer(GL_ARRAY_BUFFER, data->renddata[rendc].VBO2);
        glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 2 * sizeof(uint32_t), (void*)0);
        glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 2 * sizeof(uint32_t), (void*)sizeof(uint32_t));
        glDrawArrays(GL_TRIANGLES, 0, data->renddata[rendc].vcount2);
    }
    glEnable(GL_CULL_FACE);
    for (rendc = 0; rendc < data->info.size; ++rendc) {
        if (!data->renddata[rendc].buffered || !data->renddata[rendc].vcount3) continue;
        setUniform3f(rendinf.shaderprog, "ccoord", (float[]){(int)(rendc % data->info.width) - (int)data->info.dist,
                                                             (int)(rendc / data->info.widthsq),
                                                             (int)(rendc / data->info.width % data->info.width) - (int)data->info.dist});
        glBindBuffer(GL_ARRAY_BUFFER, data->renddata[rendc].VBO3);
        glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 2 * sizeof(uint32_t), (void*)0);
        glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 2 * sizeof(uint32_t), (void*)sizeof(uint32_t));
        glDrawArrays(GL_TRIANGLES, 0, data->renddata[rendc].vcount3);
    }
    glDepthMask(true);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "isAni"), 0);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    setShaderProg(shader_2d);
    glBindBuffer(GL_ARRAY_BUFFER, VBO2D);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    setShaderProg(shader_text);
    static uint64_t frames = 0;
    ++frames;
    static int toff = 0;
    if (!tbuf[0][0]) {
        sprintf(
            tbuf[0],
            "OpenGL version: %s\n"
            "GLSL version: %s\n"
            "Vendor string: %s\n"
            "Renderer string: %s\n",
            glver,
            glslver,
            glvend,
            glrend
        );
        toff = strlen(tbuf[0]);
    }
    sprintf(
        &tbuf[0][toff],
        "FPS: %d\n"
        "Position: (%lf, %lf, %lf)\n"
        "Velocity: (%f, %f, %f)\n"
        "Rotation: (%f, %f, %f)\n"
        "Block: (%d, %d, %d)\n"
        "Chunk: (%"PRId64", %"PRId64", %"PRId64")\n",
        fps,
        pcoord.x, pcoord.y, pcoord.z,
        pvelocity.x, pvelocity.y, pvelocity.z,
        rendinf.camrot.x, rendinf.camrot.y, rendinf.camrot.z,
        pblockx, pblocky, pblockz,
        pchunkx, pchunky, pchunkz
    );
    renderText(0, 0, 1, rendinf.width, tbuf[0], NULL);

    setShaderProg(shader_block);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    RELEASECONTEXT();
}

static void winch(int w, int h) {
    rendinf.win_width = w;
    rendinf.win_height = h;
    setFullscreen(rendinf.fullscr);
    pthread_mutex_lock(&gllock);
    glViewport(0, 0, rendinf.width, rendinf.height);
    pthread_mutex_unlock(&gllock);
}

#if defined(USESDL2)
void sdlreszevent(int w, int h) {
    winch(w, h);
}
#else
static void fbsize(GLFWwindow* win, int w, int h) {
    (void)win;
    winch(w, h);
}
#endif

#if defined(USESDL2)
static void sdlerror(char* m) {
    fprintf(stderr, "SDL2 error: {%s}: {%s}\n", m, SDL_GetError());
    SDL_ClearError();
}
#else
static void errorcb(int e, const char* m) {
    fprintf(stderr, "GLFW error [%d]: {%s}\n", e, m);
}
#endif

bool initRenderer() {
    pthread_mutex_init(&uclock, NULL);
    pthread_mutex_init(&gllock, NULL);

    #if defined(USESDL2)
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    if (SDL_Init(SDL_INIT_VIDEO)) {
        sdlerror("initRenderer: Failed to init video");
        return false;
    }
    #else
    glfwSetErrorCallback(errorcb);
    if (!glfwInit()) return false;
    #endif

    rendinf.camfov = 85;
    rendinf.campos = GFX_DEFAULT_POS;
    rendinf.camrot = GFX_DEFAULT_ROT;
    //rendinf.camrot.y = 180;

    #if defined(USESDL2)
    bool compositing = getConfigValBool(getConfigVarStatic(config, "renderer.compositing", "true", 64));
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    if (compositing) SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    #else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    //glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
    #endif

    #if defined(USESDL2)
    if (!(rendinf.window = SDL_CreateWindow("CaveCube", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_OPENGL))) {
        sdlerror("initRenderer: Failed to create window");
        return false;
    }
    if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(rendinf.window), &rendinf.monitor) < 0) {
        sdlerror("initRenderer: Failed to fetch display info");
        return false;
    }
    rendinf.full_width = rendinf.monitor.w;
    rendinf.full_height = rendinf.monitor.h;
    rendinf.disphz = rendinf.monitor.refresh_rate;
    rendinf.win_fps = rendinf.disphz;
    SDL_DestroyWindow(rendinf.window);
    #else
    if (!(rendinf.monitor = glfwGetPrimaryMonitor())) {
        fputs("initRenderer: Failed to fetch primary monitor handle\n", stderr);
        return false;
    }
    const GLFWvidmode* vmode = glfwGetVideoMode(rendinf.monitor);
    rendinf.full_width = vmode->width;
    rendinf.full_height = vmode->height;
    rendinf.disphz = vmode->refreshRate;
    rendinf.win_fps = rendinf.disphz;
    #endif

    sscanf(getConfigVarStatic(config, "renderer.resolution", "", 256), "%ux%u@%u",
        &rendinf.win_width, &rendinf.win_height, &rendinf.win_fps);
    if (!rendinf.win_width || rendinf.win_width > 32767) rendinf.win_width = 1024;
    if (!rendinf.win_height || rendinf.win_height > 32767) rendinf.win_height = 768;
    rendinf.full_fps = rendinf.win_fps;
    sscanf(getConfigVarStatic(config, "renderer.fullresolution", "", 256), "%ux%u@%u",
        &rendinf.full_width, &rendinf.full_height, &rendinf.full_fps);
    rendinf.vsync = getConfigValBool(getConfigVarStatic(config, "renderer.vsync", "false", 64));
    rendinf.fullscr = getConfigValBool(getConfigVarStatic(config, "renderer.fullscreen", "false", 64));

    #if defined(USESDL2)
    if (!(rendinf.window = SDL_CreateWindow("CaveCube", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, rendinf.win_width, rendinf.win_height, SDL_WINDOW_OPENGL))) {
        sdlerror("initRenderer: Failed to create window");
        return false;
    }
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    rendinf.context = SDL_GL_CreateContext(rendinf.window);
    if (!rendinf.context) {
        sdlerror("initRenderer: Failed to create OpenGL context");
        return false;
    }
    #else
    if (!(rendinf.window = glfwCreateWindow(rendinf.win_width, rendinf.win_height, "CaveCube", NULL, NULL))) {
        fputs("initRenderer: Failed to create window\n", stderr);
        return false;
    }
    #endif
    #ifdef RENDERER_SINGLECORE
        #if defined(USESDL2)
            SDL_GL_MakeCurrent(rendinf.window, rendinf.context);
        #else
            glfwMakeContextCurrent(rendinf.window);
        #endif
    #else
        GETCONTEXT();
    #endif
    #if defined(USESDL2)
    #else
    glfwSetInputMode(rendinf.window, GLFW_STICKY_KEYS, GLFW_TRUE);
    #endif

    GLADloadproc glproc;
    #if defined(USESDL2)
    glproc = (GLADloadproc)SDL_GL_GetProcAddress;
    #else
    glproc = (GLADloadproc)glfwGetProcAddress;
    #endif
    if (!gladLoadGLLoader(glproc)) {
        fputs("initRenderer: Failed to initialize GLAD\n", stderr);
        return false;
    }
    file_data* vs = loadResource(RESOURCE_TEXTFILE, "engine/renderer/shaders/OpenGL/default/block/vertex.glsl");
    file_data* fs = loadResource(RESOURCE_TEXTFILE, "engine/renderer/shaders/OpenGL/default/block/fragment.glsl");
    if (!vs || !fs || !makeShaderProg((char*)vs->data, (char*)fs->data, &shader_block)) {
        return false;
    }
    freeResource(vs);
    freeResource(fs);
    vs = loadResource(RESOURCE_TEXTFILE, "engine/renderer/shaders/OpenGL/default/2D/vertex.glsl");
    fs = loadResource(RESOURCE_TEXTFILE, "engine/renderer/shaders/OpenGL/default/2D/fragment.glsl");
    if (!vs || !fs || !makeShaderProg((char*)vs->data, (char*)fs->data, &shader_2d)) {
        return false;
    }
    freeResource(vs);
    freeResource(fs);
    vs = loadResource(RESOURCE_TEXTFILE, "engine/renderer/shaders/OpenGL/default/text/vertex.glsl");
    fs = loadResource(RESOURCE_TEXTFILE, "engine/renderer/shaders/OpenGL/default/text/fragment.glsl");
    if (!vs || !fs || !makeShaderProg((char*)vs->data, (char*)fs->data, &shader_text)) {
        return false;
    }
    freeResource(vs);
    freeResource(fs);

    printf("Windowed resolution: [%ux%u@%d]\n", rendinf.win_width, rendinf.win_height, rendinf.win_fps);
    printf("Fullscreen resolution: [%ux%u@%d]\n", rendinf.full_width, rendinf.full_height, rendinf.full_fps);

    glver = glGetString(GL_VERSION);
    printf("OpenGL version: %s\n", glver);
    glslver = glGetString(GL_SHADING_LANGUAGE_VERSION);
    printf("GLSL version: %s\n", glslver);
    glvend = glGetString(GL_VENDOR);
    printf("Vendor string: %s\n", glvend);
    glrend = glGetString(GL_RENDERER);
    printf("Renderer string: %s\n", glrend);

    setShaderProg(shader_block);
    RELEASECONTEXT();
    #if defined(USESDL2)
    #else
    glfwSetFramebufferSizeCallback(rendinf.window, fbsize);
    #endif
    setFullscreen(rendinf.fullscr);
    GETCONTEXT();

    glViewport(0, 0, rendinf.width, rendinf.height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    #if defined(USESDL2)
    SDL_GL_SwapWindow(rendinf.window);
    #else
    glfwSwapBuffers(rendinf.window);
    #endif
    RELEASECONTEXT();
    updateCam();
    GETCONTEXT();

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CCW);

    glActiveTexture(GL_TEXTURE0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rendinf.width, rendinf.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glActiveTexture(GL_TEXTURE1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rendinf.width, rendinf.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    unsigned char* texmap;
    texture_t texmaph;
    texture_t charseth;

    //puts("creating texture map...");
    texmap = malloc(1572864);
    memset(texmap, 255, 1572864);

    char* tmpbuf = malloc(4096);
    for (int i = 1; i < 256; ++i) {
        sprintf(tmpbuf, "game/textures/blocks/%d/", i);
        if (resourceExists(tmpbuf) == -1) {
            maxblockid = i - 1;
            break;
        }
        //printf("loading block [%d]...\n", i);
        bool st = false;
        int o = 0;
        for (int j = 0; j < 6; ++j) {
            if (st) o = j;
            sprintf(tmpbuf, "game/textures/blocks/%d/%d.png", i, j - o);
            if (resourceExists(tmpbuf) == -1) {
                if (j == 1) st = true;
                else if (j == 3) o = 3;
                else break;
                --j;
                continue;
            }
            resdata_image* img = loadResource(RESOURCE_IMAGE, tmpbuf);
            for (int j = 3; j < 1024; j += 4) {
                if (img->data[j] < 255) {
                    blockinf[i].transparency = 1;
                    //printf("! [%d]: [%u]\n", i, (uint8_t)img->data[j]);
                }
            }
            int mapoff = i * 6144 + j * 1024;
            //printf("adding texture {%s} at offset [%u] of map [%d]...\n", tmpbuf, mapoff, i);
            memcpy(&texmap[mapoff], img->data, 1024);
            freeResource(img);
        }
    }
    glGenTextures(1, &texmaph);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_3D, texmaph);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 16, 16, 1536, 0, GL_RGBA, GL_UNSIGNED_BYTE, texmap);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "TexData"), 2);

    glGenBuffers(1, &VBO2D);
    glBindBuffer(GL_ARRAY_BUFFER, VBO2D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vert2D), vert2D, GL_STATIC_DRAW);

    setShaderProg(shader_2d);
    glActiveTexture(GL_TEXTURE3);
    resdata_texture* crosshair = loadResource(RESOURCE_TEXTURE, "game/textures/ui/crosshair.png");
    glBindTexture(GL_TEXTURE_2D, crosshair->data);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "TexData"), 3);
    setUniform4f(rendinf.shaderprog, "mcolor", (float[]){1.0, 1.0, 1.0, 1.0});

    setShaderProg(shader_text);
    glGenTextures(1, &charseth);
    glActiveTexture(GL_TEXTURE4);
    resdata_image* charset = loadResource(RESOURCE_IMAGE, "game/textures/ui/charset.png");
    glBindTexture(GL_TEXTURE_3D, charseth);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 8, 16, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, charset->data);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "TexData"), 4);
    setUniform4f(rendinf.shaderprog, "mcolor", (float[]){1.0, 1.0, 1.0, 1.0});

    setShaderProg(shader_block);

    glActiveTexture(GL_TEXTURE3);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    water = blockNoFromID("water");
    blockinf[water].transparency = 1;

    RELEASECONTEXT();
    free(tmpbuf);
    return true;
}

void quitRenderer() {
    if (setchunks) {
        for (int i = 0; i < MESHER_THREADS && i < MAX_THREADS; ++i) {
            pthread_join(pthreads[i], NULL);
        }
    }
    #if defined(USESDL2)
    SDL_Quit();
    #else
    glfwTerminate();
    #endif
}

#endif
