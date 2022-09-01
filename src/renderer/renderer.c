#ifndef SERVER

#include <main/main.h>
#include "renderer.h"
#include "glad.h"
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

#if defined(USESDL2)
    #include <SDL2/SDL.h>
#else
    #include <GLFW/glfw3.h>
#endif
#include "cglm/cglm.h"

int MESHER_THREADS;
int MESHER_THREADS_MAX = 2;

struct renderer_info rendinf;
//static resdata_bmd* blockmodel;

static pthread_mutex_t gllock;

static void setMat4(GLuint prog, char* name, mat4 val) {
    glUniformMatrix4fv(glGetUniformLocation(prog, name), 1, GL_FALSE, *val);
}

static void setUniform2f(GLuint prog, char* name, float val[2]) {
    glUniform2f(glGetUniformLocation(prog, name), val[0], val[1]);
}

static void setUniform3f(GLuint prog, char* name, float val[3]) {
    glUniform3f(glGetUniformLocation(prog, name), val[0], val[1], val[2]);
}

static void setUniform4f(GLuint prog, char* name, float val[4]) {
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

static inline bool makeShaderProg(char* hdrtext, char* _vstext, char* _fstext, GLuint* p) {
    if (!_vstext || !_fstext) return false;
    bool retval = true;
    char* vstext = malloc(strlen(hdrtext) + strlen(_vstext) + 1);
    char* fstext = malloc(strlen(hdrtext) + strlen(_fstext) + 1);
    strcpy(vstext, hdrtext);
    strcpy(fstext, hdrtext);
    strcat(vstext, _vstext);
    strcat(fstext, _fstext);
    GLuint vertexHandle = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexHandle, 1, (const GLchar * const*)&vstext, NULL);
    glCompileShader(vertexHandle);
    GLint ret = GL_FALSE;
    glGetShaderiv(vertexHandle, GL_COMPILE_STATUS, &ret);
    if (!ret) {
        GLint logSize = 0;
        glGetShaderiv(vertexHandle, GL_INFO_LOG_LENGTH, &logSize);
        fprintf(stderr, "Vertex shader compile error%s\n", (logSize > 0) ? ":" : "");
        if (logSize > 0) {
            GLchar* log = malloc(logSize * sizeof(GLchar));
            glGetShaderInfoLog(vertexHandle, logSize, &logSize, log);
            fputs((char*)log, stderr);
            free(log);
        }
        glDeleteShader(vertexHandle);
        retval = false;
        goto goret;
    }
    GLuint fragHandle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragHandle, 1, (const GLchar * const*)&fstext, NULL);
    glCompileShader(fragHandle);
    glGetShaderiv(fragHandle, GL_COMPILE_STATUS, &ret);
    if (!ret) {
        GLint logSize = 0;
        glGetShaderiv(fragHandle, GL_INFO_LOG_LENGTH, &logSize);
        fprintf(stderr, "Fragment shader compile error%s\n", (logSize > 0) ? ":" : "");
        if (logSize > 0) {
            GLchar* log = malloc(logSize * sizeof(GLchar));
            glGetShaderInfoLog(fragHandle, logSize, &logSize, log);
            fputs((char*)log, stderr);
            free(log);
        }
        glDeleteShader(vertexHandle);
        glDeleteShader(fragHandle);
        retval = false;
        goto goret;
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
        fprintf(stderr, "Shader program link error%s\n", (logSize > 0) ? ":" : "");
        if (logSize > 0) {
            GLchar* log = malloc(logSize * sizeof(GLchar));
            glGetProgramInfoLog(*p, logSize, &logSize, log);
            fputs((char*)log, stderr);
            free(log);
        }
        retval = false;
        goto goret;
    }
    goret:;
    free(vstext);
    free(fstext);
    return retval;
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

void updateScreen() {
    static int lv = -1;
    if (rendinf.vsync != lv) {
        #if defined(USESDL2)
        SDL_GL_SetSwapInterval(rendinf.vsync * -1);
        #else
        glfwSwapInterval(rendinf.vsync * -1);
        #endif
        lv = rendinf.vsync;
    }
    #if defined(USESDL2)
    SDL_GL_SwapWindow(rendinf.window);
    #else
    glfwSwapBuffers(rendinf.window);
    #endif
}

static unsigned VAO;
static unsigned VBO2D;

static GLuint shader_block;
static GLuint shader_2d;
static GLuint shader_text;

struct chunkdata* chunks;

void setMeshChunks(void* vdata) {
    chunks = vdata;
}

static struct blockdata rendGetBlock(int32_t c, int x, int y, int z) {
    while (x < 0 && c % chunks->info.width) {c -= 1; x += 16;}
    while (x > 15 && (c + 1) % chunks->info.width) {c += 1; x -= 16;}
    while (z > 15 && c >= (int)chunks->info.width) {c -= chunks->info.width; z -= 16;}
    while (z < 0 && c < (int)(chunks->info.widthsq - chunks->info.width)) {c += chunks->info.width; z += 16;}
    if (c < 0 || x < 0 || z < 0 || x > 15 || z > 15) return (struct blockdata){255, 0, 0};
    if (c >= (int32_t)chunks->info.widthsq || y < 0 || y > 255) return (struct blockdata){0, 0, 0};
    if (!chunks->renddata[c].generated) return (struct blockdata){255, 0, 0};
    struct blockdata ret = chunks->data[c][y * 256 + z * 16 + x];
    return ret;
}

static float vert2D[] = {
    -1.0,  1.0,  0.0,  0.0,
     1.0,  1.0,  1.0,  0.0,
     1.0, -1.0,  1.0,  1.0,
     1.0, -1.0,  1.0,  1.0,
    -1.0, -1.0,  0.0,  1.0,
    -1.0,  1.0,  0.0,  0.0,
};

static pthread_t pthreads[MAX_THREADS];
pthread_mutex_t uclock;
static uint8_t water;
struct msgdata chunkmsgs;

struct msgdata_msg {
    bool valid;
    bool dep;
    bool full;
    int64_t x;
    int64_t z;
    uint64_t id;
};

struct msgdata {
    bool valid;
    int size;
    struct msgdata_msg* msg;
    pthread_mutex_t lock;
    uint64_t id;
};

static void initMsgData(struct msgdata* mdata) {
    mdata->valid = true;
    mdata->size = 0;
    mdata->msg = malloc(0);
    pthread_mutex_init(&mdata->lock, NULL);
}

static void deinitMsgData(struct msgdata* mdata) {
    pthread_mutex_lock(&mdata->lock);
    mdata->valid = false;
    free(mdata->msg);
    pthread_mutex_unlock(&mdata->lock);
    pthread_mutex_destroy(&mdata->lock);
}

static void addMsg(struct msgdata* mdata, int64_t x, int64_t z, uint64_t id, bool dep, bool full) {
    pthread_mutex_lock(&mdata->lock);
    if (mdata->valid) {
        int index = -1;
        for (int i = 0; i < mdata->size; ++i) {
            /*if (mdata->msg[i].valid && mdata->msg[i].x == x && mdata->msg[i].z == z) {
                printf("dup: [%"PRId64", %"PRId64"] with [%d]\n", x, z, i);
                goto skip;
            } else*/ if (!mdata->msg[i].valid && index == -1) {
                index = i;
                break;
            }
        }
        if (index == -1) {
            index = mdata->size++;
            mdata->msg = realloc(mdata->msg, mdata->size * sizeof(*mdata->msg));
        }
        //printf("adding [%d]/[%d]\n", index + 1, mdata->size);
        mdata->msg[index].valid = true;
        mdata->msg[index].dep = dep;
        mdata->msg[index].full = full;
        mdata->msg[index].x = x;
        mdata->msg[index].z = z;
        mdata->msg[index].id = (dep) ? id : mdata->id++;
        //skip:;
    }
    pthread_mutex_unlock(&mdata->lock);
}

void updateChunk(int64_t x, int64_t z, bool full) {
    addMsg(&chunkmsgs, x, z, 0, false, full);
}

static int64_t cxo, czo;

void setMeshChunkOff(int64_t x, int64_t z) {
    cxo = x;
    czo = z;
}

static bool getNextMsg(struct msgdata* mdata, struct msgdata_msg* msg) {
    pthread_mutex_lock(&mdata->lock);
    if (mdata->valid) {
        for (int i = 0; i < mdata->size; ++i) {
            if (mdata->msg[i].valid) {
                msg->valid = mdata->msg[i].valid;
                msg->dep = mdata->msg[i].dep;
                msg->full = mdata->msg[i].full;
                msg->x = mdata->msg[i].x;
                msg->z = mdata->msg[i].z;
                msg->id = mdata->msg[i].id;
                mdata->msg[i].valid = false;
                pthread_mutex_unlock(&mdata->lock);
                //printf("returning [%d]/[%d]\n", i + 1, mdata->size);
                return true;
            }
        }
    }
    pthread_mutex_unlock(&mdata->lock);
    return false;
}

static uint32_t constBlockVert1[6][6] = {
    {0x0000F0F3, 0x0F00F0F7, 0x0F00F006, 0x0F00F006, 0x0000F002, 0x0000F0F3}, // U
    {0x0F00F006, 0x0F00F0F7, 0x0F0000F5, 0x0F0000F5, 0x0F000004, 0x0F00F006}, // R
    {0x0F00F0F7, 0x0000F0F3, 0x000000F1, 0x000000F1, 0x0F0000F5, 0x0F00F0F7}, // F
    {0x00000000, 0x0F000004, 0x0F0000F5, 0x0F0000F5, 0x000000F1, 0x00000000}, // D
    {0x0000F0F3, 0x0000F002, 0x00000000, 0x00000000, 0x000000F1, 0x0000F0F3}, // L
    {0x0000F002, 0x0F00F006, 0x0F000004, 0x0F000004, 0x00000000, 0x0000F002}, // B
};

static uint32_t constBlockVert2[6][6] = {
    {0x00000000, 0x000F0200, 0x000FF300, 0x000FF300, 0x0000F100, 0x00000000}, // U
    {0x00000000, 0x000F0200, 0x000FF300, 0x000FF300, 0x0000F100, 0x00000000}, // R
    {0x00000000, 0x000F0200, 0x000FF300, 0x000FF300, 0x0000F100, 0x00000000}, // F
    {0x00000000, 0x000F0200, 0x000FF300, 0x000FF300, 0x0000F100, 0x00000000}, // D
    {0x00000000, 0x000F0200, 0x000FF300, 0x000FF300, 0x0000F100, 0x00000000}, // L
    {0x00000000, 0x000F0200, 0x000FF300, 0x000FF300, 0x0000F100, 0x00000000}, // B
};

static void mtsetvert(uint32_t** _v, int* s, int* l, uint32_t** v, uint32_t bv) {
    bool r = false;
    while (*l >= *s) {*s *= 2; r = true;}
    if (r) {*_v = realloc(*_v, *s * sizeof(uint32_t)); *v = *_v + *l;}
    **v = bv;
    ++*v; ++*l;
}

static void* meshthread(void* args) {
    (void)args;
    struct blockdata bdata;
    struct blockdata bdata2[6];
    uint64_t acttime = altutime();
    struct msgdata_msg msg;
    while (!quitRequest) {
        bool activity = false;
        if (getNextMsg(&chunkmsgs, &msg)) {
            //printf("mesh: [%"PRId64", %"PRId64"]\n", msg.x, msg.z);
            activity = true;
            pthread_mutex_lock(&uclock);
            {
                int64_t nx = (msg.x - cxo) + chunks->info.dist;
                int64_t nz = chunks->info.width - ((msg.z - czo) + chunks->info.dist) - 1;
                if (nx < 0 || nz < 0 || nx >= chunks->info.width || nz >= chunks->info.width) {
                    goto lblcontinue;
                }
                uint64_t c = nx + nz * chunks->info.width;
                if (msg.id < chunks->renddata[c].updateid) {goto lblcontinue;}
            }
            pthread_mutex_unlock(&uclock);
            if (!msg.dep) {
                addMsg(&chunkmsgs, msg.x, msg.z + 1, msg.id, true, false);
                addMsg(&chunkmsgs, msg.x, msg.z - 1, msg.id, true, false);
                addMsg(&chunkmsgs, msg.x + 1, msg.z, msg.id, true, false);
                addMsg(&chunkmsgs, msg.x - 1, msg.z, msg.id, true, false);
                if (msg.full) {
                    addMsg(&chunkmsgs, msg.x + 1, msg.z + 1, msg.id, true, false);
                    addMsg(&chunkmsgs, msg.x + 1, msg.z - 1, msg.id, true, false);
                    addMsg(&chunkmsgs, msg.x - 1, msg.z + 1, msg.id, true, false);
                    addMsg(&chunkmsgs, msg.x - 1, msg.z - 1, msg.id, true, false);
                }
            }
            int vpsize = 256;
            int vpsize2 = 256;
            int vpsize3 = 256;
            uint32_t* _vptr = malloc(vpsize * sizeof(uint32_t));
            uint32_t* _vptr2 = malloc(vpsize2 * sizeof(uint32_t));
            uint32_t* _vptr3 = malloc(vpsize3 * sizeof(uint32_t));
            int vplen = 0;
            int vplen2 = 0;
            int vplen3 = 0;
            uint32_t* vptr = _vptr;
            uint32_t* vptr2 = _vptr2;
            uint32_t* vptr3 = _vptr3;
            for (int y = 255; y >= 0; --y) {
                pthread_mutex_lock(&uclock);
                int64_t nx = (msg.x - cxo) + chunks->info.dist;
                int64_t nz = chunks->info.width - ((msg.z - czo) + chunks->info.dist) - 1;
                if (nx < 0 || nz < 0 || nx >= chunks->info.width || nz >= chunks->info.width) {
                    free(_vptr);
                    free(_vptr2);
                    free(_vptr3);
                    goto lblcontinue;
                }
                uint64_t c = nx + nz * chunks->info.width;
                for (int z = 0; z < 16; ++z) {
                    for (int x = 0; x < 16; ++x) {
                        bdata = rendGetBlock(c, x, y, z);
                        if (!bdata.id || !blockinf[bdata.id].id) continue;
                        bdata2[0] = rendGetBlock(c, x, y + 1, z);
                        bdata2[1] = rendGetBlock(c, x + 1, y, z);
                        bdata2[2] = rendGetBlock(c, x, y, z + 1);
                        bdata2[3] = rendGetBlock(c, x, y - 1, z);
                        bdata2[4] = rendGetBlock(c, x - 1, y, z);
                        bdata2[5] = rendGetBlock(c, x, y, z - 1);
                        for (int i = 0; i < 6; ++i) {
                            if (bdata2[i].id && blockinf[bdata2[i].id].id) {
                                if (!blockinf[bdata2[i].id].transparency) continue;
                                if (blockinf[bdata.id].transparency && (bdata.id == bdata2[i].id)) continue;
                            }
                            if (bdata2[i].id == 255) continue;
                            uint32_t baseVert1 = ((x << 28) | (y << 16) | (z << 8)) & 0xF0FF0F00;
                            uint32_t baseVert2 = ((bdata2[i].light << 28) | (bdata2[i].light << 24) | (bdata2[i].light << 20)) & 0xFFF00000;
                            uint32_t baseVert3 = ((blockinf[bdata.id].texoff[i] << 16) & 0xFFFF0000) | (blockinf[bdata.id].anict[i] & 0x0000FFFF);
                            if (bdata.id == water) {
                                if (!bdata2[i].id) {
                                    for (int j = 0; j < 6; ++j) {
                                        mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, constBlockVert1[i][j] | baseVert1);
                                        mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, constBlockVert2[i][j] | baseVert2);
                                        mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, baseVert3);
                                    }
                                } else {
                                    for (int j = 0; j < 6; ++j) {
                                        mtsetvert(&_vptr3, &vpsize3, &vplen3, &vptr3, constBlockVert1[i][j] | baseVert1);
                                        mtsetvert(&_vptr3, &vpsize3, &vplen3, &vptr3, constBlockVert2[i][j] | baseVert2);
                                        mtsetvert(&_vptr3, &vpsize3, &vplen3, &vptr3, baseVert3);
                                    }
                                }
                                //printf("added [%d][%d %d %d][%d]: [%u]: [%08X]...\n", c, x, y, z, i, (uint8_t)bdata.id, baseVert1);
                            } else {
                                for (int j = 0; j < 6; ++j) {
                                    mtsetvert(&_vptr, &vpsize, &vplen, &vptr, constBlockVert1[i][j] | baseVert1);
                                    mtsetvert(&_vptr, &vpsize, &vplen, &vptr, constBlockVert2[i][j] | baseVert2);
                                    mtsetvert(&_vptr, &vpsize, &vplen, &vptr, baseVert3);
                                }
                                //printf("added [%d][%d %d %d][%d]: [%u]: [%08X]...\n", c, x, y, z, i, (uint8_t)bdata.id, baseVert1);
                            }
                        }
                    }
                }
                pthread_mutex_unlock(&uclock);
            }
            pthread_mutex_lock(&uclock);
            int64_t nx = (msg.x - cxo) + chunks->info.dist;
            int64_t nz = chunks->info.width - ((msg.z - czo) + chunks->info.dist) - 1;
            if (nx < 0 || nz < 0 || nx >= chunks->info.width || nz >= chunks->info.width) {
                free(_vptr);
                free(_vptr2);
                free(_vptr3);
                goto lblcontinue;
            }
            uint64_t c = nx + nz * chunks->info.width;
            if (msg.id >= chunks->renddata[c].updateid) {
                chunks->renddata[c].vcount = vplen / 3;
                chunks->renddata[c].vcount2 = vplen2 / 3;
                chunks->renddata[c].vcount3 = vplen3 / 3;
                chunks->renddata[c].vertices = _vptr;
                chunks->renddata[c].vertices2 = _vptr2;
                chunks->renddata[c].vertices3 = _vptr3;
                chunks->renddata[c].ready = true;
                chunks->renddata[c].updateid = msg.id;
                //printf("meshed: [%"PRId64", %"PRId64"] ([%"PRId64", %"PRId64"])\n", msg.x, msg.z, nx, nz);
            }
            lblcontinue:;
            pthread_mutex_unlock(&uclock);
        }
        if (activity) {
            acttime = altutime();
        } else if (altutime() - acttime > 3000000) {
            microwait(500000);
        }
    }
    return NULL;
}

void updateChunks() {
    //uint32_t c2 = 0;
    pthread_mutex_lock(&uclock);
    for (uint32_t c = 0; !quitRequest && c < chunks->info.widthsq; ++c) {
        if (!chunks->renddata[c].ready) continue;
        //if (c2 >= chunks->info.widthsq) break;
        //++c2;
        uint32_t tmpsize = chunks->renddata[c].vcount * 3 * sizeof(uint32_t);
        uint32_t tmpsize2 = chunks->renddata[c].vcount2 * 3 * sizeof(uint32_t);
        uint32_t tmpsize3 = chunks->renddata[c].vcount3 * 3 * sizeof(uint32_t);
        if (!chunks->renddata[c].VBO) glGenBuffers(1, &chunks->renddata[c].VBO);
        if (!chunks->renddata[c].VBO2) glGenBuffers(1, &chunks->renddata[c].VBO2);
        if (!chunks->renddata[c].VBO3) glGenBuffers(1, &chunks->renddata[c].VBO3);
        if (tmpsize) {
            glBindBuffer(GL_ARRAY_BUFFER, chunks->renddata[c].VBO);
            glBufferData(GL_ARRAY_BUFFER, tmpsize, chunks->renddata[c].vertices, GL_STATIC_DRAW);
        }
        if (tmpsize2) {
            glBindBuffer(GL_ARRAY_BUFFER, chunks->renddata[c].VBO2);
            glBufferData(GL_ARRAY_BUFFER, tmpsize2, chunks->renddata[c].vertices2, GL_STATIC_DRAW);
        }
        if (tmpsize3) {
            glBindBuffer(GL_ARRAY_BUFFER, chunks->renddata[c].VBO3);
            glBufferData(GL_ARRAY_BUFFER, tmpsize3, chunks->renddata[c].vertices3, GL_STATIC_DRAW);
        }
        free(chunks->renddata[c].vertices);
        free(chunks->renddata[c].vertices2);
        free(chunks->renddata[c].vertices3);
        chunks->renddata[c].vertices = NULL;
        chunks->renddata[c].vertices2 = NULL;
        chunks->renddata[c].vertices3 = NULL;
        chunks->renddata[c].ready = false;
        chunks->renddata[c].buffered = true;
    }
    pthread_mutex_unlock(&uclock);
}

static bool mesheractive = false;

void startMesher() {
    if (!mesheractive) {
        if (MESHER_THREADS > MESHER_THREADS_MAX) MESHER_THREADS = MESHER_THREADS_MAX;
        #ifdef NAME_THREADS
        char name[256];
        char name2[256];
        #endif
        for (int i = 0; i < MESHER_THREADS && i < MAX_THREADS; ++i) {
            #ifdef NAME_THREADS
            name[0] = 0;
            name2[0] = 0;
            #endif
            pthread_create(&pthreads[i], NULL, &meshthread, NULL);
            printf("Mesher: Started thread [%d]\n", i);
            #ifdef NAME_THREADS
            pthread_getname_np(pthreads[i], name2, 256);
            sprintf(name, "%s:msh%d", name2, i);
            pthread_setname_np(pthreads[i], name);
            #endif
        }
        mesheractive = true;
    }
}

void renderText(float x, float y, float scale, unsigned end, char* text, void* extinfo) {
    (void)extinfo;
    glUniform1f(glGetUniformLocation(rendinf.shaderprog, "xratio"), 8.0 / (float)rendinf.width);
    glUniform1f(glGetUniformLocation(rendinf.shaderprog, "yratio"), 16.0 / (float)rendinf.height);
    glUniform1f(glGetUniformLocation(rendinf.shaderprog, "scale"), scale);
    glUniform4f(glGetUniformLocation(rendinf.shaderprog, "bmcolor"), 0, 0, 0, 0.35);
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

static resdata_texture* crosshair;

static uint32_t rendc;

void render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "dist"), chunks->info.dist);
    setUniform3f(rendinf.shaderprog, "cam", (float[]){rendinf.campos.x, rendinf.campos.y, rendinf.campos.z});
    for (rendc = 0; rendc < chunks->info.widthsq; ++rendc) {
        if (!chunks->renddata[rendc].buffered || !chunks->renddata[rendc].vcount) continue;
        setUniform2f(rendinf.shaderprog, "ccoord", (float[]){(int)(rendc % chunks->info.width) - (int)chunks->info.dist, (int)(rendc / chunks->info.width) - (int)chunks->info.dist});
        glBindBuffer(GL_ARRAY_BUFFER, chunks->renddata[rendc].VBO);
        glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(0));
        glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t)));
        glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 2));
        glDrawArrays(GL_TRIANGLES, 0, chunks->renddata[rendc].vcount);
    }
    glUniform1ui(glGetUniformLocation(rendinf.shaderprog, "aniMult"), (altutime() / 16) % 65536);
    glDisable(GL_CULL_FACE);
    glDepthMask(false);
    for (rendc = 0; rendc < chunks->info.widthsq; ++rendc) {
        if (!chunks->renddata[rendc].buffered || !chunks->renddata[rendc].vcount2) continue;
        setUniform2f(rendinf.shaderprog, "ccoord", (float[]){(int)(rendc % chunks->info.width) - (int)chunks->info.dist, (int)(rendc / chunks->info.width) - (int)chunks->info.dist});
        glBindBuffer(GL_ARRAY_BUFFER, chunks->renddata[rendc].VBO2);
        glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(0));
        glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t)));
        glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 2));
        glDrawArrays(GL_TRIANGLES, 0, chunks->renddata[rendc].vcount2);
    }
    glEnable(GL_CULL_FACE);
    for (rendc = 0; rendc < chunks->info.widthsq; ++rendc) {
        if (!chunks->renddata[rendc].buffered || !chunks->renddata[rendc].vcount3) continue;
        setUniform2f(rendinf.shaderprog, "ccoord", (float[]){(int)(rendc % chunks->info.width) - (int)chunks->info.dist, (int)(rendc / chunks->info.width) - (int)chunks->info.dist});
        glBindBuffer(GL_ARRAY_BUFFER, chunks->renddata[rendc].VBO3);
        glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(0));
        glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t)));
        glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 2));
        glDrawArrays(GL_TRIANGLES, 0, chunks->renddata[rendc].vcount3);
    }
    glDepthMask(true);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    setShaderProg(shader_2d);
    glBindBuffer(GL_ARRAY_BUFFER, VBO2D);
    glUniform1f(glGetUniformLocation(rendinf.shaderprog, "xratio"), ((float)(crosshair->width)) / (float)rendinf.width);
    glUniform1f(glGetUniformLocation(rendinf.shaderprog, "yratio"), ((float)(crosshair->height)) / (float)rendinf.height);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    setShaderProg(shader_text);
    static uint64_t frames = 0;
    ++frames;
    static int toff = 0;
    if (!tbuf[0][0]) {
        sprintf(
            tbuf[0],
            "CaveCube %d.%d.%d\n"
            "OpenGL version: %s\n"
            "GLSL version: %s\n"
            "Vendor string: %s\n"
            "Renderer string: %s\n",
            VER_MAJOR, VER_MINOR, VER_PATCH,
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
        "Chunk: (%"PRId64", %"PRId64")\n",
        fps,
        pcoord.x, pcoord.y, pcoord.z,
        pvelocity.x, pvelocity.y, pvelocity.z,
        rendinf.camrot.x, rendinf.camrot.y, rendinf.camrot.z,
        pblockx, pblocky, pblockz,
        pchunkx, pchunkz
    );
    renderText(0, 0, 1, rendinf.width, tbuf[0], NULL);

    setShaderProg(shader_block);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
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
    #if defined(USEGLES)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    #else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    #endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    if (compositing) SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    #else
    #if defined(USEGLES)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    #else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    #endif
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
    #if defined(USESDL2)
    SDL_GL_MakeCurrent(rendinf.window, rendinf.context);
    #else
    glfwMakeContextCurrent(rendinf.window);
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

    #if defined(USEGLES)
    char* hdrpath = "engine/shaders/headers/OpenGL ES/header.glsl";
    #else
    char* hdrpath = "engine/shaders/headers/OpenGL/header.glsl";
    #endif
    file_data* hdr = loadResource(RESOURCE_TEXTFILE, hdrpath);
    if (!hdr) {
        fputs("initRenderer: Failed to load shader header\n", stderr);
        return false;
    }
    file_data* vs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/block/vertex.glsl");
    file_data* fs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/block/fragment.glsl");
    if (!vs || !fs || !makeShaderProg((char*)hdr->data, (char*)vs->data, (char*)fs->data, &shader_block)) {
        fputs("initRenderer: Failed to compile block shader\n", stderr);
        return false;
    }
    freeResource(vs);
    freeResource(fs);
    vs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/2D/vertex.glsl");
    fs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/2D/fragment.glsl");
    if (!vs || !fs || !makeShaderProg((char*)hdr->data, (char*)vs->data, (char*)fs->data, &shader_2d)) {
        fputs("initRenderer: Failed to compile 2D shader\n", stderr);
        return false;
    }
    freeResource(vs);
    freeResource(fs);
    vs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/text/vertex.glsl");
    fs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/text/fragment.glsl");
    if (!vs || !fs || !makeShaderProg((char*)hdr->data, (char*)vs->data, (char*)fs->data, &shader_text)) {
        fputs("initRenderer: Failed to compile text shader\n", stderr);
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
    #if defined(USESDL2)
    #else
    glfwSetFramebufferSizeCallback(rendinf.window, fbsize);
    #endif
    setFullscreen(rendinf.fullscr);

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
    updateCam();

    glEnable(GL_CULL_FACE);
    //glCullFace(GL_FRONT);
    glFrontFace(GL_CW);

    glActiveTexture(GL_TEXTURE0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rendinf.width, rendinf.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //TODO: load and render splash
    #if defined(USESDL2)
    SDL_GL_SwapWindow(rendinf.window);
    #else
    glfwSwapBuffers(rendinf.window);
    #endif

    glActiveTexture(GL_TEXTURE1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rendinf.width, rendinf.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    unsigned char* texmap;
    texture_t texmaph;
    texture_t charseth;

    //puts("creating texture map...");
    //TODO: change map format and add mapoffset var to block info
    int texmapsize = 0;
    texmap = malloc(texmapsize * 1024);
    char* tmpbuf = malloc(4096);
    for (int i = 1; i < 255; ++i) {
        sprintf(tmpbuf, "game/textures/blocks/%d/", i);
        if (resourceExists(tmpbuf) == -1) {
            break;
        }
        //printf("loading block [%d]...\n", i);
        for (int j = 0; j < 6; ++j) {
            sprintf(tmpbuf, "game/textures/blocks/%d/%d.png", i, j);
            if (resourceExists(tmpbuf) == -1) {
                if (j == 1) {
                    for (int k = 0; k < 5; ++k) {
                        blockinf[i].texoff[1 + k] = blockinf[i].texoff[0];
                    }
                } else if (j == 3) {
                    for (int k = 0; k < 3; ++k) {
                        blockinf[i].texoff[3 + k] = blockinf[i].texoff[0 + k];
                    }
                }
                break;
            }
            if (j > 0 && blockinf[i].singletexoff) {
                blockinf[i].texoff[j] = blockinf[i].texoff[0];
            } else {
                blockinf[i].texoff[j] = texmapsize;
            }
            resdata_image* img = loadResource(RESOURCE_IMAGE, tmpbuf);
            for (int j = 3; j < 1024; j += 4) {
                if (img->data[j] < 255) {
                    blockinf[i].transparency = 1;
                    //printf("! [%d]: [%u]\n", i, (uint8_t)img->data[j]);
                    break;
                }
            }
            //printf("adding texture {%s} at offset [%u] of map [%d]...\n", tmpbuf, mapoff, i);
            texmap = realloc(texmap, (texmapsize + 1) * 1024);
            memcpy(texmap + texmapsize * 1024, img->data, 1024);
            ++texmapsize;
            freeResource(img);
        }
    }
    glGenTextures(1, &texmaph);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texmaph);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 16, 16, texmapsize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texmap);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "texData"), 2);
    glUniform1f(glGetUniformLocation(rendinf.shaderprog, "texmapdiv"), texmapsize - 1);
    free(texmap);

    glGenBuffers(1, &VBO2D);
    glBindBuffer(GL_ARRAY_BUFFER, VBO2D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vert2D), vert2D, GL_STATIC_DRAW);

    setShaderProg(shader_2d);
    glActiveTexture(GL_TEXTURE3);
    crosshair = loadResource(RESOURCE_TEXTURE, "game/textures/ui/crosshair.png");
    glBindTexture(GL_TEXTURE_2D, crosshair->data);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "texData"), 3);
    setUniform4f(rendinf.shaderprog, "mcolor", (float[]){1.0, 1.0, 1.0, 1.0});

    setShaderProg(shader_text);
    glGenTextures(1, &charseth);
    glActiveTexture(GL_TEXTURE4);
    resdata_image* charset = loadResource(RESOURCE_IMAGE, "game/textures/ui/charset.png");
    glBindTexture(GL_TEXTURE_2D_ARRAY, charseth);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 8, 16, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, charset->data);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "texData"), 4);
    setUniform4f(rendinf.shaderprog, "mcolor", (float[]){1.0, 1.0, 1.0, 1.0});

    setShaderProg(shader_block);

    glActiveTexture(GL_TEXTURE3);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    water = blockNoFromID("water");
    blockinf[water].transparency = 1;

    free(tmpbuf);

    initMsgData(&chunkmsgs);

    return true;
}

void quitRenderer() {
    if (mesheractive) {
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
