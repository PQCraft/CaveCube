#if MODULEID == MODULEID_GAME

#include <main/main.h>
#include "renderer.h"
#include "ui.h"
#include "glad.h"
#include <main/version.h>
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
int MESHER_THREADS_MAX = 1;

struct renderer_info rendinf;
//static resdata_bmd* blockmodel;

static GLuint shader_block;
static GLuint shader_2d;
static GLuint shader_ui;
static GLuint shader_framebuffer;

static force_inline void setMat4(GLuint prog, char* name, mat4 val) {
    glUniformMatrix4fv(glGetUniformLocation(prog, name), 1, GL_FALSE, *val);
}

static force_inline void setUniform1f(GLuint prog, char* name, float val) {
    glUniform1f(glGetUniformLocation(prog, name), val);
}

static force_inline void setUniform2f(GLuint prog, char* name, float val[2]) {
    glUniform2f(glGetUniformLocation(prog, name), val[0], val[1]);
}

static force_inline void setUniform3f(GLuint prog, char* name, float val[3]) {
    glUniform3f(glGetUniformLocation(prog, name), val[0], val[1], val[2]);
}

static force_inline void setUniform4f(GLuint prog, char* name, float val[4]) {
    glUniform4f(glGetUniformLocation(prog, name), val[0], val[1], val[2], val[3]);
}

static force_inline void setShaderProg(GLuint s) {
    rendinf.shaderprog = s;
    glUseProgram(rendinf.shaderprog);
}

static color sky;

void setSkyColor(float r, float g, float b) {
    sky.r = r;
    sky.g = g;
    sky.b = b;
    glClearColor(r, g, b, 1);
    setShaderProg(shader_block);
    setUniform3f(rendinf.shaderprog, "skycolor", (float[]){r, g, b});
    //setShaderProg(shader_3d);
    //setUniform3f(rendinf.shaderprog, "skycolor", (float[]){r, g, b});
}

void setScreenMult(float r, float g, float b) {
    setShaderProg(shader_framebuffer);
    setUniform3f(rendinf.shaderprog, "mcolor", (float[]){r, g, b});
}

void setVisibility(int vis, float vismul) {
    setShaderProg(shader_block);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "vis"), vis);
    glUniform1f(glGetUniformLocation(rendinf.shaderprog, "vismul"), vismul);
    //setShaderProg(shader_3d);
    //glUniform1i(glGetUniformLocation(rendinf.shaderprog, "vis"), vis);
    //glUniform1f(glGetUniformLocation(rendinf.shaderprog, "vismul"), vismul);
}

#define avec2 vec2 __attribute__((aligned (32)))
#define avec3 vec3 __attribute__((aligned (32)))
#define avec4 vec4 __attribute__((aligned (32)))
#define amat4 mat4 __attribute__((aligned (32)))

struct frustum {
	float planes[6][16];
};

static struct frustum __attribute__((aligned (32))) frust;

static force_inline void normFrustPlane(struct frustum* frust, int plane) {
	float len = sqrtf(frust->planes[plane][0] * frust->planes[plane][0] + frust->planes[plane][1] * frust->planes[plane][1] + frust->planes[plane][2] * frust->planes[plane][2]);
	for (int i = 0; i < 4; i++) {
		frust->planes[plane][i] /= len;
	}
}

static float __attribute__((aligned (32))) cf_clip[16];

static force_inline void calcFrust(struct frustum* frust, float* proj, float* view) {
	cf_clip[0] = view[0] * proj[0] + view[1] * proj[4] + view[2] * proj[8] + view[3] * proj[12];
	cf_clip[1] = view[0] * proj[1] + view[1] * proj[5] + view[2] * proj[9] + view[3] * proj[13];
	cf_clip[2] = view[0] * proj[2] + view[1] * proj[6] + view[2] * proj[10] + view[3] * proj[14];
	cf_clip[3] = view[0] * proj[3] + view[1] * proj[7] + view[2] * proj[11] + view[3] * proj[15];
	cf_clip[4] = view[4] * proj[0] + view[5] * proj[4] + view[6] * proj[8] + view[7] * proj[12];
	cf_clip[5] = view[4] * proj[1] + view[5] * proj[5] + view[6] * proj[9] + view[7] * proj[13];
	cf_clip[6] = view[4] * proj[2] + view[5] * proj[6] + view[6] * proj[10] + view[7] * proj[14];
	cf_clip[7] = view[4] * proj[3] + view[5] * proj[7] + view[6] * proj[11] + view[7] * proj[15];
	cf_clip[8] = view[8] * proj[0] + view[9] * proj[4] + view[10] * proj[8] + view[11] * proj[12];
	cf_clip[9] = view[8] * proj[1] + view[9] * proj[5] + view[10] * proj[9] + view[11] * proj[13];
	cf_clip[10] = view[8] * proj[2] + view[9] * proj[6] + view[10] * proj[10] + view[11] * proj[14];
	cf_clip[11] = view[8] * proj[3] + view[9] * proj[7] + view[10] * proj[11] + view[11] * proj[15];
	cf_clip[12] = view[12] * proj[0] + view[13] * proj[4] + view[14] * proj[8] + view[15] * proj[12];
	cf_clip[13] = view[12] * proj[1] + view[13] * proj[5] + view[14] * proj[9] + view[15] * proj[13];
	cf_clip[14] = view[12] * proj[2] + view[13] * proj[6] + view[14] * proj[10] + view[15] * proj[14];
	cf_clip[15] = view[12] * proj[3] + view[13] * proj[7] + view[14] * proj[11] + view[15] * proj[15];
	frust->planes[0][0] = cf_clip[3] - cf_clip[0];
	frust->planes[0][1] = cf_clip[7] - cf_clip[4];
	frust->planes[0][2] = cf_clip[11] - cf_clip[8];
	frust->planes[0][3] = cf_clip[15] - cf_clip[12];
	normFrustPlane(frust, 0);
	frust->planes[1][0] = cf_clip[3] + cf_clip[0];
	frust->planes[1][1] = cf_clip[7] + cf_clip[4];
	frust->planes[1][2] = cf_clip[11] + cf_clip[8];
	frust->planes[1][3] = cf_clip[15] + cf_clip[12];
	normFrustPlane(frust, 1);
	frust->planes[2][0] = cf_clip[3] - cf_clip[1];
	frust->planes[2][1] = cf_clip[7] - cf_clip[5];
	frust->planes[2][2] = cf_clip[11] - cf_clip[9];
	frust->planes[2][3] = cf_clip[15] - cf_clip[13];
	normFrustPlane(frust, 2);
	frust->planes[3][0] = cf_clip[3] + cf_clip[1];
	frust->planes[3][1] = cf_clip[7] + cf_clip[5];
	frust->planes[3][2] = cf_clip[11] + cf_clip[9];
	frust->planes[3][3] = cf_clip[15] + cf_clip[13];
	normFrustPlane(frust, 3);
	frust->planes[4][0] = cf_clip[3] - cf_clip[2];
	frust->planes[4][1] = cf_clip[7] - cf_clip[6];
	frust->planes[4][2] = cf_clip[11] - cf_clip[10];
	frust->planes[4][3] = cf_clip[15] - cf_clip[14];
	normFrustPlane(frust, 4);
	frust->planes[5][0] = cf_clip[3] + cf_clip[2];
	frust->planes[5][1] = cf_clip[7] + cf_clip[6];
	frust->planes[5][2] = cf_clip[11] + cf_clip[10];
	frust->planes[5][3] = cf_clip[15] + cf_clip[14];
	normFrustPlane(frust, 5);
}

static force_inline bool isVisible(struct frustum* frust, float ax, float ay, float az, float bx, float by, float bz) {
	for (int i = 0; i < 6; i++) {
		if ((frust->planes[i][0] * ax + frust->planes[i][1] * ay + frust->planes[i][2] * az + frust->planes[i][3] <= 0.0) &&
		    (frust->planes[i][0] * bx + frust->planes[i][1] * ay + frust->planes[i][2] * az + frust->planes[i][3] <= 0.0) &&
		    (frust->planes[i][0] * ax + frust->planes[i][1] * by + frust->planes[i][2] * az + frust->planes[i][3] <= 0.0) &&
		    (frust->planes[i][0] * bx + frust->planes[i][1] * by + frust->planes[i][2] * az + frust->planes[i][3] <= 0.0) &&
		    (frust->planes[i][0] * ax + frust->planes[i][1] * ay + frust->planes[i][2] * bz + frust->planes[i][3] <= 0.0) &&
		    (frust->planes[i][0] * bx + frust->planes[i][1] * ay + frust->planes[i][2] * bz + frust->planes[i][3] <= 0.0) &&
		    (frust->planes[i][0] * ax + frust->planes[i][1] * by + frust->planes[i][2] * bz + frust->planes[i][3] <= 0.0) &&
		    (frust->planes[i][0] * bx + frust->planes[i][1] * by + frust->planes[i][2] * bz + frust->planes[i][3] <= 0.0)) return false;
	}
	return true;
}

void updateCam() {
    static float uc_fov = -1.0, uc_asp = -1.0;
    static avec3 uc_campos;
    static float uc_rotradx, uc_rotrady, uc_rotradz;
    static amat4 uc_proj;
    static amat4 uc_view;
    static avec3 uc_direction;
    static avec3 uc_up;
    static avec3 uc_front;
    static bool uc_uproj = false;
    static avec3 uc_z1z = {0.0, 1.0, 0.0};
    if (rendinf.aspect != uc_asp) {uc_asp = rendinf.aspect; uc_uproj = true;}
    if (rendinf.camfov != uc_fov) {uc_fov = rendinf.camfov; uc_uproj = true;}
    if (uc_uproj) {
        glm_mat4_copy((mat4)GLM_MAT4_IDENTITY_INIT, uc_proj);
        glm_perspective(uc_fov * M_PI / 180.0, uc_asp, rendinf.camnear, rendinf.camfar, uc_proj);
        setShaderProg(shader_block);
        setMat4(rendinf.shaderprog, "projection", uc_proj);
        //setShaderProg(shader_3d);
        //setMat4(rendinf.shaderprog, "projection", uc_proj);
        uc_uproj = false;
    }
    uc_campos[0] = rendinf.campos.x;
    uc_campos[1] = rendinf.campos.y;
    uc_campos[2] = rendinf.campos.z;
    uc_rotradx = rendinf.camrot.x * M_PI / 180.0;
    uc_rotrady = (rendinf.camrot.y - 90.0) * M_PI / 180.0;
    uc_rotradz = rendinf.camrot.z * M_PI / 180.0;
    uc_direction[0] = cos(uc_rotrady) * cos(uc_rotradx);
    uc_direction[1] = sin(uc_rotradx);
    uc_direction[2] = sin(uc_rotrady) * cos(uc_rotradx);
    glm_vec3_copy(uc_z1z, uc_up);
    glm_vec3_copy(uc_direction, uc_front);
    glm_vec3_add(uc_campos, uc_direction, uc_direction);
    glm_vec3_rotate(uc_up, uc_rotradz, uc_front);
    glm_mat4_copy((mat4)GLM_MAT4_IDENTITY_INIT, uc_view);
    glm_lookat(uc_campos, uc_direction, uc_up, uc_view);
    setShaderProg(shader_block);
    setMat4(rendinf.shaderprog, "view", uc_view);
    //setShaderProg(shader_3d);
    //setMat4(rendinf.shaderprog, "view", uc_view);
    calcFrust(&frust, (float*)uc_proj, (float*)uc_view);
}

void setFullscreen(bool fullscreen) {
    static int winox = -1, winoy = -1;
    if (fullscreen) {
        rendinf.aspect = (float)rendinf.full_width / (float)rendinf.full_height;
        rendinf.width = rendinf.full_width;
        rendinf.height = rendinf.full_height;
        rendinf.fps = rendinf.full_fps;
        #if defined(USESDL2)
        if (!rendinf.fullscr) {
            SDL_GetWindowPosition(rendinf.window, &winox, &winoy);
        }
        SDL_SetWindowFullscreen(rendinf.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        #else
        if (!rendinf.fullscr) {
            glfwGetWindowPos(rendinf.window, &winox, &winoy);
        }
        glfwSetWindowMonitor(rendinf.window, rendinf.monitor, 0, 0, rendinf.full_width, rendinf.full_height, rendinf.full_fps);
        #endif
        rendinf.fullscr = true;
    } else {
        rendinf.aspect = (float)rendinf.win_width / (float)rendinf.win_height;
        rendinf.width = rendinf.win_width;
        rendinf.height = rendinf.win_height;
        rendinf.fps = rendinf.win_fps;
        int twinx, twiny;
        if (rendinf.fullscr) {
            uint64_t offset = altutime();
            #if defined(USESDL2)
            SDL_SetWindowFullscreen(rendinf.window, 0);
            do {
                SDL_SetWindowPosition(rendinf.window, winox, winoy);
                SDL_GetWindowPosition(rendinf.window, &twinx, &twiny);
            } while (altutime() - offset < 3000000 && (twinx != winox || twiny != winoy));
            #else
            glfwSetWindowMonitor(rendinf.window, NULL, 0, 0, rendinf.win_width, rendinf.win_height, GLFW_DONT_CARE);
            do {
                glfwSetWindowPos(rendinf.window, winox, winoy);
                glfwGetWindowPos(rendinf.window, &twinx, &twiny);
            } while (altutime() - offset < 3000000 && (twinx != winox || twiny != winoy));
            #endif
        }
        rendinf.fullscr = false;
    }
    //updateCam();
}

static force_inline bool makeShaderProg(char* hdrtext, char* _vstext, char* _fstext, GLuint* p) {
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
        SDL_GL_SetSwapInterval(rendinf.vsync);
        #else
        glfwSwapInterval(rendinf.vsync);
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

static unsigned FBO;
static unsigned FBTEX;
static unsigned FBTEXID;
static unsigned DBUF;

static unsigned UIFBO;
static unsigned UIFBTEX;
static unsigned UIFBTEXID;
static unsigned UIDBUF;

struct chunkdata* chunks;

void setMeshChunks(void* vdata) {
    chunks = vdata;
}

static force_inline struct blockdata rendGetBlock(int32_t c, int x, int y, int z) {
    while (x < 0 && c % chunks->info.width) {c -= 1; x += 16;}
    while (x > 15 && (c + 1) % chunks->info.width) {c += 1; x -= 16;}
    while (z > 15 && c >= (int)chunks->info.width) {c -= chunks->info.width; z -= 16;}
    while (z < 0 && c < (int)(chunks->info.widthsq - chunks->info.width)) {c += chunks->info.width; z += 16;}
    if (c < 0 || x < 0 || z < 0 || x > 15 || z > 15) return (struct blockdata){255, 0, 0, 0, 0, 0, 0, 0};
    if (c >= (int32_t)chunks->info.widthsq || y < 0 || y > 255) return (struct blockdata){0, 0, 0, 0, 0, 0, 0, 0};
    if (!chunks->renddata[c].generated) return (struct blockdata){255, 0, 0, 0, 0, 0, 0, 0};
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

static force_inline void initMsgData(struct msgdata* mdata) {
    mdata->valid = true;
    mdata->size = 0;
    mdata->msg = malloc(0);
    pthread_mutex_init(&mdata->lock, NULL);
}

/*
static void deinitMsgData(struct msgdata* mdata) {
    pthread_mutex_lock(&mdata->lock);
    mdata->valid = false;
    free(mdata->msg);
    pthread_mutex_unlock(&mdata->lock);
    pthread_mutex_destroy(&mdata->lock);
}
*/

static force_inline void addMsg(struct msgdata* mdata, int64_t x, int64_t z, uint64_t id, bool dep, bool full) {
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

static force_inline bool getNextMsg(struct msgdata* mdata, struct msgdata_msg* msg) {
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
    {0x0000F0F3, 0x0F00F006, 0x0F00F0F7, 0x0F00F006, 0x0000F0F3, 0x0000F002}, // U
    {0x0F00F006, 0x0F0000F5, 0x0F00F0F7, 0x0F0000F5, 0x0F00F006, 0x0F000004}, // R
    {0x0F00F0F7, 0x000000F1, 0x0000F0F3, 0x000000F1, 0x0F00F0F7, 0x0F0000F5}, // F
    {0x00000000, 0x0F0000F5, 0x0F000004, 0x0F0000F5, 0x00000000, 0x000000F1}, // D
    {0x0000F0F3, 0x00000000, 0x0000F002, 0x00000000, 0x0000F0F3, 0x000000F1}, // L
    {0x0000F002, 0x0F000004, 0x0F00F006, 0x0F000004, 0x0000F002, 0x00000000}, // B
};

static uint32_t constBlockVert2[6][6] = {
    {0x00000000, 0x000FF300, 0x000F0200, 0x000FF300, 0x00000000, 0x0000F100}, // U
    {0x00000000, 0x000FF300, 0x000F0200, 0x000FF300, 0x00000000, 0x0000F100}, // R
    {0x00000000, 0x000FF300, 0x000F0200, 0x000FF300, 0x00000000, 0x0000F100}, // F
    {0x00000000, 0x000FF300, 0x000F0200, 0x000FF300, 0x00000000, 0x0000F100}, // D
    {0x00000000, 0x000FF300, 0x000F0200, 0x000FF300, 0x00000000, 0x0000F100}, // L
    {0x00000000, 0x000FF300, 0x000F0200, 0x000FF300, 0x00000000, 0x0000F100}, // B
};

#define mtsetvert(_v, s, l, v, bv) {\
    if (*l >= *s) {\
        *s *= 2;\
        *_v = realloc(*_v, *s * sizeof(**_v));\
        *v = *_v + *l;\
    }\
    **v = bv;\
    ++*v; ++*l;\
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
            int vpsize = 1024;
            int vpsize2 = 1024;
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
            for (int y = 0; y < 256; ++y) {
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
                        if (!bdata.id || !blockinf[bdata.id].id || !blockinf[bdata.id].data[bdata.subid].id) continue;
                        bdata2[0] = rendGetBlock(c, x, y + 1, z);
                        bdata2[1] = rendGetBlock(c, x + 1, y, z);
                        bdata2[2] = rendGetBlock(c, x, y, z + 1);
                        bdata2[3] = rendGetBlock(c, x, y - 1, z);
                        bdata2[4] = rendGetBlock(c, x - 1, y, z);
                        bdata2[5] = rendGetBlock(c, x, y, z - 1);
                        for (int i = 0; i < 6; ++i) {
                            if (bdata2[i].id && blockinf[bdata2[i].id].id) {
                                if (!blockinf[bdata2[i].id].data[bdata2[i].subid].transparency) continue;
                                if (blockinf[bdata.id].data[bdata.subid].transparency && (bdata.id == bdata2[i].id)) continue;
                            }
                            if (bdata2[i].id == 255) continue;
                            uint32_t baseVert1 = ((x << 28) | (y << 16) | (z << 8)) & 0xF0FF0F00;
                            uint32_t baseVert2 = ((bdata2[i].light_r << 28) | (bdata2[i].light_g << 24) | (bdata2[i].light_b << 20) | blockinf[bdata.id].data[bdata.subid].anidiv) & 0xFFF000FF;
                            uint32_t baseVert3 = ((blockinf[bdata.id].data[bdata.subid].texoff[i] << 16) & 0xFFFF0000) | (blockinf[bdata.id].data[bdata.subid].anict[i] & 0x0000FFFF);
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
                if (chunks->renddata[c].vertices) free(chunks->renddata[c].vertices);
                if (chunks->renddata[c].vertices2) free(chunks->renddata[c].vertices2);
                if (chunks->renddata[c].vertices3) free(chunks->renddata[c].vertices3);
                chunks->renddata[c].vcount = vplen / 3;
                chunks->renddata[c].vcount2 = vplen2 / 3;
                chunks->renddata[c].vcount3 = vplen3 / 3;
                chunks->renddata[c].vertices = _vptr;
                chunks->renddata[c].vertices2 = _vptr2;
                chunks->renddata[c].vertices3 = _vptr3;
                chunks->renddata[c].ready = true;
                chunks->renddata[c].updateid = msg.id;
                //printf("meshed: [%"PRId64", %"PRId64"] ([%"PRId64", %"PRId64"])\n", msg.x, msg.z, nx, nz);
            } else {
                free(_vptr);
                free(_vptr2);
                free(_vptr3);
            }
            lblcontinue:;
            pthread_mutex_unlock(&uclock);
        }
        if (activity) {
            acttime = altutime();
        } else if (altutime() - acttime > 500000) {
            microwait(200000);
        }
    }
    return NULL;
}

static bool uchunks_slowmode = false;

void updateChunks() {
    pthread_mutex_lock(&uclock);
    for (uint32_t c = 0; !quitRequest && c < chunks->info.widthsq; ++c) {
        if (!chunks->renddata[c].ready || !chunks->renddata[c].visible) continue;
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
        if (uchunks_slowmode) break;
    }
    pthread_mutex_unlock(&uclock);
}

static bool mesheractive = false;

void startMesher() {
    if (!mesheractive) {
        #ifdef NAME_THREADS
        char name[256];
        char name2[256];
        #endif
        for (int i = 0; i < MESHER_THREADS && i < MAX_THREADS && i < MESHER_THREADS_MAX; ++i) {
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

static float textColor[16][3] = {
    {0x00 / 255.0, 0x00 / 255.0, 0x00 / 255.0},
    {0x00 / 255.0, 0x00 / 255.0, 0xAA / 255.0},
    {0x00 / 255.0, 0xAA / 255.0, 0x00 / 255.0},
    {0x00 / 255.0, 0xAA / 255.0, 0xAA / 255.0},
    {0xAA / 255.0, 0x00 / 255.0, 0x00 / 255.0},
    {0xAA / 255.0, 0x00 / 255.0, 0xAA / 255.0},
    {0xAA / 255.0, 0x55 / 255.0, 0x00 / 255.0},
    {0xAA / 255.0, 0xAA / 255.0, 0xAA / 255.0},
    {0x55 / 255.0, 0x55 / 255.0, 0x55 / 255.0},
    {0x55 / 255.0, 0x55 / 255.0, 0xFF / 255.0},
    {0x55 / 255.0, 0xFF / 255.0, 0x55 / 255.0},
    {0x55 / 255.0, 0xFF / 255.0, 0xFF / 255.0},
    {0xFF / 255.0, 0x55 / 255.0, 0x55 / 255.0},
    {0xFF / 255.0, 0x55 / 255.0, 0xFF / 255.0},
    {0xFF / 255.0, 0xFF / 255.0, 0x55 / 255.0},
    {0xFF / 255.0, 0xFF / 255.0, 0xFF / 255.0},
};

static inline void syncTextColors() {
    setShaderProg(shader_ui);
    setUniform3f(rendinf.shaderprog, "textColor[0]", textColor[0]);
    setUniform3f(rendinf.shaderprog, "textColor[1]", textColor[1]);
    setUniform3f(rendinf.shaderprog, "textColor[2]", textColor[2]);
    setUniform3f(rendinf.shaderprog, "textColor[3]", textColor[3]);
    setUniform3f(rendinf.shaderprog, "textColor[4]", textColor[4]);
    setUniform3f(rendinf.shaderprog, "textColor[5]", textColor[5]);
    setUniform3f(rendinf.shaderprog, "textColor[6]", textColor[6]);
    setUniform3f(rendinf.shaderprog, "textColor[7]", textColor[7]);
    setUniform3f(rendinf.shaderprog, "textColor[8]", textColor[8]);
    setUniform3f(rendinf.shaderprog, "textColor[9]", textColor[9]);
    setUniform3f(rendinf.shaderprog, "textColor[10]", textColor[10]);
    setUniform3f(rendinf.shaderprog, "textColor[11]", textColor[11]);
    setUniform3f(rendinf.shaderprog, "textColor[12]", textColor[12]);
    setUniform3f(rendinf.shaderprog, "textColor[13]", textColor[13]);
    setUniform3f(rendinf.shaderprog, "textColor[14]", textColor[14]);
    setUniform3f(rendinf.shaderprog, "textColor[15]", textColor[15]);
}

// ui elem: [16 bits: x][16 bits: y]
// ui elem: [1 bit: 0][23 bits: reserved][8 bits: z]
// ui elem: [8 bits: r][8 bits: g][8 bits: b][8 bits: a]

// text:    [16 bits: x][16 bits: y]
// text:    [1 bit: 1][5 bits: reserved][1 bit: texture x][1 bit: texture y][8 bits: reserved][8 bits: text char][8 bits: z]
// text:    [8 bits: fgc alpha][8 bits: bgc alpha][4 bits: text fgc][4 bits: text bgc][8 bits: reserved]

struct meshdata {
    int s;
    int l;
    uint32_t* _v;
    uint32_t* v;
};

#define writeuielemvert(md, x, y, z, r, g, b, a) {\
    mtsetvert(&md->_v, &md->s, &md->l, &md->v, (((x) << 16) & 0xFFFF0000) | ((y) & 0xFFFF));\
    mtsetvert(&md->_v, &md->s, &md->l, &md->v, ((z) & 0xFF));\
    mtsetvert(&md->_v, &md->s, &md->l, &md->v, (((r) << 24) & 0xFF000000) | (((g) << 16) & 0xFF0000) | (((b) << 8) & 0xFF00) | ((a) & 0xFF));\
}

#define writeuitextvert(md, x, y, z, c, tx, ty, fgc, bgc, fga, bga) {\
    mtsetvert(&md->_v, &md->s, &md->l, &md->v, (((x) << 16) & 0xFFFF0000) | ((y) & 0xFFFF));\
    mtsetvert(&md->_v, &md->s, &md->l, &md->v, 0x80000000 | (((tx) << 25) & 0x2000000) | (((tx) << 24) & 0x1000000) | (((c) << 8) & 0xFF00) | ((z) & 0xFF));\
    mtsetvert(&md->_v, &md->s, &md->l, &md->v, (((fga) << 24) & 0xFF000000) | (((bga) << 16) & 0xFF0000) | (((fgc) << 12) & 0xF000) | (((bgc) << 8) & 0xF00));\
}

#define writeuielemrect(md, x0, y0, x1, y1, z, r, g, b, a) {\
    writeuielemvert(md, x0, y0, z, r, g, b, a);\
    writeuielemvert(md, x0, y1, z, r, g, b, a);\
    writeuielemvert(md, x1, y0, z, r, g, b, a);\
    writeuielemvert(md, x1, y0, z, r, g, b, a);\
    writeuielemvert(md, x0, y1, z, r, g, b, a);\
    writeuielemvert(md, x1, y1, z, r, g, b, a);\
}

#define writeuitextchar(md, x0, y0, x1, y1, z, c, fgc, bgc, fga, bga) {\
    writeuitextvert(md, x0, y0, z, c, 0, 0, fgc, bgc, fga, bga);\
    writeuitextvert(md, x0, y1, z, c, 0, 1, fgc, bgc, fga, bga);\
    writeuitextvert(md, x1, y0, z, c, 1, 0, fgc, bgc, fga, bga);\
    writeuitextvert(md, x1, y0, z, c, 1, 0, fgc, bgc, fga, bga);\
    writeuitextvert(md, x0, y1, z, c, 0, 1, fgc, bgc, fga, bga);\
    writeuitextvert(md, x1, y1, z, c, 1, 1, fgc, bgc, fga, bga);\
}

static force_inline void meshUIElem(struct meshdata* md, struct ui_elem* e) {
    switch (e->type) {
        case UI_ELEM_BOX:; {
            writeuielemrect(
                md,
                e->calcprop.x, e->calcprop.y,
                e->calcprop.x + e->calcprop.width, e->calcprop.y + e->calcprop.height,
                e->calcprop.z,
                127, 127, 127, 127
            );
            break;
        }
        case UI_ELEM_FANCYBOX:; {
            writeuielemrect(
                md,
                e->calcprop.x, e->calcprop.y + 1,
                e->calcprop.x + e->calcprop.width, e->calcprop.y + e->calcprop.height - 1,
                e->calcprop.z,
                63, 63, 63, 127
            );
            writeuielemrect(
                md,
                e->calcprop.x + 1, e->calcprop.y,
                e->calcprop.x + e->calcprop.width - 1, e->calcprop.y + e->calcprop.height,
                e->calcprop.z,
                63, 63, 63, 127
            );
            writeuielemrect(
                md,
                e->calcprop.x + 1, e->calcprop.y + 1,
                e->calcprop.x + e->calcprop.width - 1, e->calcprop.y + e->calcprop.height - 1,
                e->calcprop.z,
                127, 127, 127, 127
            );
            break;
        }
    }
}

static inline void meshUIElemTree(struct meshdata* md, struct ui_data* elemdata, struct ui_elem* e) {
    if (e->calcprop.hidden) return;
    meshUIElem(md, e);
    for (int i = 0; i < e->children; ++i) {
        if (isUIIdValid(elemdata, e->childdata[i])) meshUIElemTree(md, elemdata, &elemdata->data[e->childdata[i]]);
    }
}

static force_inline void meshUIElems(struct meshdata* md, struct ui_data* elemdata) {
    for (int i = 0; i < elemdata->count; ++i) {
        struct ui_elem* e = &elemdata->data[i];
        if (e->parent < 0 && !e->calcprop.hidden) {
            meshUIElemTree(md, elemdata, e);
        }
    }
}

static force_inline void renderUI(struct ui_data* data) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindBuffer(GL_ARRAY_BUFFER, data->renddata.VBO);
    if (calcUIProperties(data)) {
        #if DBGLVL(2)
        puts("UI remesh");
        #endif
        struct meshdata mdata;
        mdata.s = 64;
        mdata._v = malloc(mdata.s * sizeof(uint32_t));
        mdata.l = 0;
        mdata.v = mdata._v;
        meshUIElems(&mdata, data);
        glBufferData(GL_ARRAY_BUFFER, mdata.l * sizeof(uint32_t), mdata._v, GL_STATIC_DRAW);
        data->renddata.vcount = mdata.l / 3;
        free(mdata._v);
    }
    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(0));
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t)));
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 2));
    glDrawArrays(GL_TRIANGLES, 0, data->renddata.vcount);
}

const unsigned char* glver;
const unsigned char* glslver;
const unsigned char* glvend;
const unsigned char* glrend;

static force_inline coord_3d_dbl intCoord_dbl(coord_3d_dbl in) {
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
    if (showDebugInfo) {
        static char tbuf[1][32768];
        static int toff = 0;
        if (!tbuf[0][0]) {
            sprintf(
                tbuf[0],
                PROG_NAME " %d.%d.%d\n"
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
            "FPS: %d (%d)\n"
            "Position: (%lf, %lf, %lf)\n"
            "Velocity: (%f, %f, %f)\n"
            "Rotation: (%f, %f, %f)\n"
            "Block: (%d, %d, %d)\n"
            "Chunk: (%"PRId64", %"PRId64")\n",
            fps, realfps,
            pcoord.x, pcoord.y, pcoord.z,
            pvelocity.x, pvelocity.y, pvelocity.z,
            rendinf.camrot.x, rendinf.camrot.y, rendinf.camrot.z,
            pblockx, pblocky, pblockz,
            pchunkx, pchunkz
        );
    }

    setShaderProg(shader_block);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glClearColor(sky.r, sky.g, sky.b, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    static uint64_t aMStart = 0;
    if (!aMStart) aMStart = altutime();
    glUniform1ui(glGetUniformLocation(rendinf.shaderprog, "aniMult"), (aMStart - altutime()) / 10000);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "dist"), chunks->info.dist);
    setUniform3f(rendinf.shaderprog, "cam", (float[]){rendinf.campos.x, rendinf.campos.y, rendinf.campos.z});
    for (rendc = 0; rendc < chunks->info.widthsq; ++rendc) {
        avec2 coord = {(int)(rendc % chunks->info.width) - (int)chunks->info.dist, (int)(rendc / chunks->info.width) - (int)chunks->info.dist};
        if (!(chunks->renddata[rendc].visible = isVisible(&frust, coord[0] * 16 - 8, 0, coord[1] * 16 - 8, coord[0] * 16 + 8, 256, coord[1] * 16 + 8))
            || !chunks->renddata[rendc].buffered || !chunks->renddata[rendc].vcount) continue;
        setUniform2f(rendinf.shaderprog, "ccoord", coord);
        glBindBuffer(GL_ARRAY_BUFFER, chunks->renddata[rendc].VBO);
        glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(0));
        glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t)));
        glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 2));
        glDrawArrays(GL_TRIANGLES, 0, chunks->renddata[rendc].vcount);
    }
    glDisable(GL_CULL_FACE);
    glDepthMask(false);
    for (rendc = 0; rendc < chunks->info.widthsq; ++rendc) {
        if (!chunks->renddata[rendc].visible || !chunks->renddata[rendc].buffered || !chunks->renddata[rendc].vcount2) continue;
        setUniform2f(rendinf.shaderprog, "ccoord", (float[]){(int)(rendc % chunks->info.width) - (int)chunks->info.dist, (int)(rendc / chunks->info.width) - (int)chunks->info.dist});
        glBindBuffer(GL_ARRAY_BUFFER, chunks->renddata[rendc].VBO2);
        glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(0));
        glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t)));
        glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 2));
        glDrawArrays(GL_TRIANGLES, 0, chunks->renddata[rendc].vcount2);
    }
    glEnable(GL_CULL_FACE);
    for (rendc = 0; rendc < chunks->info.widthsq; ++rendc) {
        if (!chunks->renddata[rendc].visible || !chunks->renddata[rendc].buffered || !chunks->renddata[rendc].vcount3) continue;
        setUniform2f(rendinf.shaderprog, "ccoord", (float[]){(int)(rendc % chunks->info.width) - (int)chunks->info.dist, (int)(rendc / chunks->info.width) - (int)chunks->info.dist});
        glBindBuffer(GL_ARRAY_BUFFER, chunks->renddata[rendc].VBO3);
        glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(0));
        glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t)));
        glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 2));
        glDrawArrays(GL_TRIANGLES, 0, chunks->renddata[rendc].vcount3);
    }
    glDepthMask(true);

    glDisable(GL_CULL_FACE);
    setShaderProg(shader_2d);
    glBindBuffer(GL_ARRAY_BUFFER, VBO2D);
    glUniform1f(glGetUniformLocation(rendinf.shaderprog, "xratio"), ((float)(crosshair->width)) / (float)rendinf.width);
    glUniform1f(glGetUniformLocation(rendinf.shaderprog, "yratio"), ((float)(crosshair->height)) / (float)rendinf.height);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    setShaderProg(shader_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "texData"), FBTEXID - GL_TEXTURE0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "texData"), UIFBTEXID - GL_TEXTURE0);
    glClearColor(0, 0, 0, 0);
    for (int i = 0; i < 4; ++i) {
        setShaderProg(shader_ui);
        glBindFramebuffer(GL_FRAMEBUFFER, UIFBO);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        renderUI(game_ui[i]);
        setShaderProg(shader_framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBindBuffer(GL_ARRAY_BUFFER, VBO2D);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}

static void winch(int w, int h) {
    if (inputMode == INPUT_MODE_GAME) {
        resetInput();
    }
    if (!rendinf.fullscr) {
        rendinf.win_width = w;
        rendinf.win_height = h;
    }
    setFullscreen(rendinf.fullscr);

    glBindTexture(GL_TEXTURE_2D, FBTEX);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rendinf.width, rendinf.height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, DBUF);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, rendinf.width, rendinf.height);

    glBindTexture(GL_TEXTURE_2D, UIFBTEX);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rendinf.width, rendinf.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, UIDBUF);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, rendinf.width, rendinf.height);

    setShaderProg(shader_ui);
    setUniform1f(rendinf.shaderprog, "xsize", rendinf.width);
    setUniform1f(rendinf.shaderprog, "ysize", rendinf.height);

    glViewport(0, 0, rendinf.width, rendinf.height);
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

    rendinf.camfov = 85;
    rendinf.camnear = 0.05;
    rendinf.camfar = 1000;
    rendinf.campos = GFX_DEFAULT_POS;
    rendinf.camrot = GFX_DEFAULT_ROT;
    //rendinf.camrot.y = 180;

    return true;
}

#if defined(_WIN32) && !defined(_WIN64)
__attribute__((stdcall))
#endif
static void oglCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* msg, const void* data) {
    (void)source;
    (void)type;
    (void)length;
    (void)data;
    bool ignore = true;
    char* sevstr = "Unknown";
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:;
            ignore = false;
            sevstr = "GL_DEBUG_SEVERITY_HIGH";
            break;
        #if DBGLVL(1)
        case GL_DEBUG_SEVERITY_MEDIUM:;
            ignore = false;
            sevstr = "GL_DEBUG_SEVERITY_MEDIUM";
            break;
        #endif
        #if DBGLVL(2)
        case GL_DEBUG_SEVERITY_LOW:;
            ignore = false;
            sevstr = "GL_DEBUG_SEVERITY_LOW";
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:;
            ignore = false;
            sevstr = "GL_DEBUG_SEVERITY_NOTIFICATION";
            break;
        #endif
    }
    if (ignore) return;
    fprintf(stderr, "OpenGL debug [%s]: [%d] {%s}\n", sevstr, id, msg);
}

bool startRenderer() {
    #if defined(USESDL2)
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    if (SDL_Init(SDL_INIT_VIDEO)) {
        sdlerror("startRenderer: Failed to init video");
        return false;
    }
    #else
    glfwSetErrorCallback(errorcb);
    if (!glfwInit()) return false;
    #endif

    #if defined(USESDL2)
    declareConfigKey(config, "Renderer", "compositing", "true", false);
    bool compositing = getBool(getConfigKey(config, "Renderer", "compositing"));
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
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    //glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    #endif

    #if defined(USESDL2)
    if (!(rendinf.window = SDL_CreateWindow(PROG_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_OPENGL))) {
        sdlerror("startRenderer: Failed to create window");
        return false;
    }
    if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(rendinf.window), &rendinf.monitor) < 0) {
        sdlerror("startRenderer: Failed to fetch display info");
        return false;
    }
    rendinf.full_width = rendinf.monitor.w;
    rendinf.full_height = rendinf.monitor.h;
    rendinf.disphz = rendinf.monitor.refresh_rate;
    rendinf.win_fps = rendinf.disphz;
    SDL_DestroyWindow(rendinf.window);
    #else
    if (!(rendinf.monitor = glfwGetPrimaryMonitor())) {
        fputs("startRenderer: Failed to fetch primary monitor handle\n", stderr);
        return false;
    }
    const GLFWvidmode* vmode = glfwGetVideoMode(rendinf.monitor);
    rendinf.disp_width = vmode->width;
    rendinf.disp_height = vmode->height;
    rendinf.disphz = vmode->refreshRate;
    rendinf.win_fps = rendinf.disphz;
    #endif

    declareConfigKey(config, "Renderer", "resolution", "1024x768", false);
    declareConfigKey(config, "Renderer", "fullScreenRes", "", false);
    declareConfigKey(config, "Renderer", "vSync", "true", false);
    declareConfigKey(config, "Renderer", "fullScreen", "false", false);
    declareConfigKey(config, "Renderer", "FOV", "85", false);
    declareConfigKey(config, "Renderer", "nearPlane", "0.05", false);
    declareConfigKey(config, "Renderer", "farPlane", "2500", false);
    sscanf(getConfigKey(config, "Renderer", "resolution"), "%ux%u@%u",
        &rendinf.win_width, &rendinf.win_height, &rendinf.win_fps);
    if (!rendinf.win_width || rendinf.win_width > 32767) rendinf.win_width = 1024;
    if (!rendinf.win_height || rendinf.win_height > 32767) rendinf.win_height = 768;
    rendinf.full_fps = rendinf.win_fps;
    sscanf(getConfigKey(config, "Renderer", "fullScreenRes"), "%ux%u@%u",
        &rendinf.full_width, &rendinf.full_height, &rendinf.full_fps);
    if (!rendinf.full_width || rendinf.full_width > 32767) rendinf.full_width = rendinf.disp_width;
    if (!rendinf.full_height || rendinf.full_height > 32767) rendinf.full_height = rendinf.disp_height;
    rendinf.vsync = getBool(getConfigKey(config, "Renderer", "vSync"));
    rendinf.fullscr = getBool(getConfigKey(config, "Renderer", "fullScreen"));
    rendinf.camfov = atof(getConfigKey(config, "Renderer", "FOV"));
    rendinf.camnear = atof(getConfigKey(config, "Renderer", "nearPlane"));
    rendinf.camfar = atof(getConfigKey(config, "Renderer", "farPlane"));

    #if defined(USESDL2)
    if (!(rendinf.window = SDL_CreateWindow(PROG_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, rendinf.win_width, rendinf.win_height, SDL_WINDOW_OPENGL))) {
        sdlerror("startRenderer: Failed to create window");
        return false;
    }
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1", SDL_HINT_OVERRIDE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    rendinf.context = SDL_GL_CreateContext(rendinf.window);
    if (!rendinf.context) {
        sdlerror("startRenderer: Failed to create OpenGL context");
        return false;
    }
    #else
    if (!(rendinf.window = glfwCreateWindow(rendinf.win_width, rendinf.win_height, PROG_NAME, NULL, NULL))) {
        fputs("startRenderer: Failed to create window\n", stderr);
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
        fputs("startRenderer: Failed to initialize GLAD\n", stderr);
        return false;
    }
    if (GL_KHR_debug) {
        glDebugMessageCallback(oglCallback, NULL);
    }

    #if defined(USEGLES)
    char* hdrpath = "engine/shaders/headers/OpenGL ES/header.glsl";
    #else
    char* hdrpath = "engine/shaders/headers/OpenGL/header.glsl";
    #endif
    file_data* hdr = loadResource(RESOURCE_TEXTFILE, hdrpath);
    if (!hdr) {
        fputs("startRenderer: Failed to load shader header\n", stderr);
        return false;
    }
    file_data* vs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/block/vertex.glsl");
    file_data* fs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/block/fragment.glsl");
    if (!vs || !fs || !makeShaderProg((char*)hdr->data, (char*)vs->data, (char*)fs->data, &shader_block)) {
        fputs("startRenderer: Failed to compile block shader\n", stderr);
        return false;
    }
    freeResource(vs);
    freeResource(fs);
    vs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/2D/vertex.glsl");
    fs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/2D/fragment.glsl");
    if (!vs || !fs || !makeShaderProg((char*)hdr->data, (char*)vs->data, (char*)fs->data, &shader_2d)) {
        fputs("startRenderer: Failed to compile 2D shader\n", stderr);
        return false;
    }
    freeResource(vs);
    freeResource(fs);
    vs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/ui/vertex.glsl");
    fs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/ui/fragment.glsl");
    if (!vs || !fs || !makeShaderProg((char*)hdr->data, (char*)vs->data, (char*)fs->data, &shader_ui)) {
        fputs("startRenderer: Failed to compile text shader\n", stderr);
        return false;
    }
    freeResource(vs);
    freeResource(fs);
    vs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/framebuffer/vertex.glsl");
    fs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/framebuffer/fragment.glsl");
    if (!vs || !fs || !makeShaderProg((char*)hdr->data, (char*)vs->data, (char*)fs->data, &shader_framebuffer)) {
        fputs("startRenderer: Failed to compile framebuffer shader\n", stderr);
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

    #if defined(USESDL2)
    #else
    glfwSetFramebufferSizeCallback(rendinf.window, fbsize);
    #endif
    setFullscreen(rendinf.fullscr);

    setShaderProg(shader_block);
    glViewport(0, 0, rendinf.width, rendinf.height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
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
    //glFrontFace(GL_CW);
    #if !defined(USEGLES)
    glDisable(GL_MULTISAMPLE);
    #endif

    int gltex = GL_TEXTURE0;

    glGenRenderbuffers(1, &UIDBUF);
    glBindRenderbuffer(GL_RENDERBUFFER, UIDBUF);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, rendinf.width, rendinf.height);
    UIFBTEXID = gltex++;
    glActiveTexture(UIFBTEXID);
    glGenFramebuffers(1, &UIFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, UIFBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, UIDBUF);
    glGenTextures(1, &UIFBTEX);
    glBindTexture(GL_TEXTURE_2D, UIFBTEX);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rendinf.width, rendinf.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, UIFBTEX, 0);

    glGenRenderbuffers(1, &DBUF);
    glBindRenderbuffer(GL_RENDERBUFFER, DBUF);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, rendinf.width, rendinf.height);
    FBTEXID = gltex++;
    glActiveTexture(FBTEXID);
    glGenFramebuffers(1, &FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, DBUF);
    glGenTextures(1, &FBTEX);
    glBindTexture(GL_TEXTURE_2D, FBTEX);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rendinf.width, rendinf.height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, FBTEX, 0);

    //glBindFramebuffer(GL_FRAMEBUFFER, 0);

    setShaderProg(shader_framebuffer);
    setUniform3f(rendinf.shaderprog, "mcolor", (float[]){1.0, 1.0, 1.0});

    glActiveTexture(gltex++);
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

    glActiveTexture(gltex++);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rendinf.width, rendinf.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    unsigned char* texmap;
    texture_t texmaph;
    texture_t charseth;

    //puts("creating texture map...");
    int texmapsize = 0;
    texmap = malloc(texmapsize * 1024);
    char* tmpbuf = malloc(4096);
    for (int i = 1; i < 255; ++i) {
        for (int s = 0; s < 64; ++s) {
            sprintf(tmpbuf, "game/textures/blocks/%d/%d/", i, s);
            if (resourceExists(tmpbuf) == -1) {
                break;
            }
            //printf("loading block [%d]:[%d]...\n", i, s);
            for (int j = 0; j < 32; ++j) {
                sprintf(tmpbuf, "game/textures/blocks/%d/%d/%d.png", i, s, j);
                if (resourceExists(tmpbuf) == -1) break;
                if (j == 0) blockinf[i].data[s].texstart = texmapsize;
                ++blockinf[i].data[s].texcount;
                resdata_image* img = loadResource(RESOURCE_IMAGE, tmpbuf);
                for (int k = 3; k < 1024; k += 4) {
                    if (img->data[k] < 255) {
                        blockinf[i].data[s].transparency = 1;
                        //printf("! [%d]: [%u]\n", i, (uint8_t)img->data[j]);
                        break;
                    }
                }
                //printf("adding texture {%s} at offset [%u] of map [%d]...\n", tmpbuf, texmapsize * 1024, i);
                texmap = realloc(texmap, (texmapsize + 1) * 1024);
                memcpy(texmap + texmapsize * 1024, img->data, 1024);
                ++texmapsize;
                freeResource(img);
            }
        }
    }
    {
        char tmpstr[256];
        int texture;
        for (int i = 1; i < 255; ++i) {
            if (!blockinf[i].id) continue;
            for (int j = 0; j < 64; ++j) {
                if (!blockinf[i].data[j].id) continue;
                for (int k = 0; k < 6; ++k) {
                    char* texstr = blockinf[i].data[j].texstr[k];
                    char c = *texstr;
                    texture = blockinf[i].data[j].texstart;
                    switch (c) {
                        case '.':; {
                            texture += atoi(&texstr[1]);
                            break;
                        }
                        case '$':; {
                            int stroff = 1;
                            stroff += readStrUntil(&texstr[1], '.', tmpstr) + 1;
                            int vtexid = blockSubNoFromID(i, tmpstr);
                            if (vtexid >= 0) texture = blockinf[i].data[vtexid].texstart;
                            texture += atoi(&texstr[stroff]);
                            break;
                        }
                        case '@':; {
                            int stroff = 1;
                            stroff += readStrUntil(&texstr[1], '.', tmpstr) + 1;
                            int btexid = blockNoFromID(tmpstr);
                            if (btexid >= 0) {
                                stroff += readStrUntil(&texstr[stroff], '.', tmpstr) + 1;
                                int vtexid = blockSubNoFromID(btexid, tmpstr);
                                if (vtexid >= 0) texture = blockinf[btexid].data[vtexid].texstart;
                            }
                            texture += atoi(&texstr[stroff]);
                            break;
                        }
                    }
                    blockinf[i].data[j].texoff[k] = texture;
                }
            }
        }
    }
    setShaderProg(shader_block);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "texData"), gltex - GL_TEXTURE0);
    glGenTextures(1, &texmaph);
    glActiveTexture(gltex++);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texmaph);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 16, 16, texmapsize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texmap);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    free(texmap);

    glGenBuffers(1, &VBO2D);
    glBindBuffer(GL_ARRAY_BUFFER, VBO2D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vert2D), vert2D, GL_STATIC_DRAW);

    setShaderProg(shader_2d);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "texData"), gltex - GL_TEXTURE0);
    glActiveTexture(gltex++);
    crosshair = loadResource(RESOURCE_TEXTURE, "game/textures/ui/crosshair.png");
    glBindTexture(GL_TEXTURE_2D, crosshair->data);
    setUniform4f(rendinf.shaderprog, "mcolor", (float[]){1.0, 1.0, 1.0, 1.0});

    setShaderProg(shader_ui);
    syncTextColors();
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "fontTexData"), gltex - GL_TEXTURE0);
    setUniform1f(rendinf.shaderprog, "xsize", rendinf.width);
    setUniform1f(rendinf.shaderprog, "ysize", rendinf.height);
    glGenTextures(1, &charseth);
    glActiveTexture(gltex++);
    resdata_image* charset = loadResource(RESOURCE_IMAGE, "game/textures/ui/charset.png");
    glBindTexture(GL_TEXTURE_2D_ARRAY, charseth);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 8, 16, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, charset->data);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    setShaderProg(shader_block);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    water = blockNoFromID("water");

    free(tmpbuf);

    initMsgData(&chunkmsgs);

    return true;
}

void stopRenderer() {
    if (mesheractive) {
        for (int i = 0; i < MESHER_THREADS && i < MESHER_THREADS_MAX; ++i) {
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
