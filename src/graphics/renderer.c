#if defined(MODULE_GAME)

#include <main/main.h>
#include "renderer.h"
#include "ui.h"
#include "cglm/cglm.h"
#include <main/version.h>
#include <common/common.h>
#include <common/resource.h>
#include <common/noise.h>
#include <common/chunk.h>
#include <common/blocks.h>
#include <game/input.h>
#include <game/game.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>

#include <common/glue.h>

int MESHER_THREADS;
int MESHER_THREADS_MAX = 1;

struct renderer_info rendinf;
//static resdata_bmd* blockmodel;

static GLuint shader_block = 0;
static GLuint shader_2d = 0;
static GLuint shader_ui = 0;
static GLuint shader_text = 0;
static GLuint shader_framebuffer = 0;

static unsigned VAO;
static unsigned VBO2D;
static unsigned EBO;

static unsigned FBO;
static unsigned FBTEX;
static unsigned FBTEXID;
static unsigned DBUF;

static unsigned UIFBO;
static unsigned UIFBTEX;
static unsigned UIFBTEXID;
static unsigned UIDBUF;

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

static color nat;

void setNatColor(float r, float g, float b) {
    nat.r = r;
    nat.g = g;
    nat.b = b;
    setShaderProg(shader_block);
    setUniform3f(rendinf.shaderprog, "natLight", (float[]){r, g, b});
    //setShaderProg(shader_3d);
    //setUniform3f(rendinf.shaderprog, "natLight", (float[]){r, g, b});
}

static color screenmult;

void setScreenMult(float r, float g, float b) {
    screenmult.r = r;
    screenmult.g = g;
    screenmult.b = b;
    //setShaderProg(shader_framebuffer);
    //setUniform3f(rendinf.shaderprog, "mcolor", (float[]){r, g, b});
}

static float fognear, fogfar;

void setVisibility(float nearval, float farval) {
    fognear = nearval;
    fogfar = farval;
    setShaderProg(shader_block);
    setUniform1f(rendinf.shaderprog, "fogNear", nearval);
    setUniform1f(rendinf.shaderprog, "fogFar", farval);
    //setShaderProg(shader_3d);
    //setUniform1f(rendinf.shaderprog, "fogNear", nearval);
    //setUniform1f(rendinf.shaderprog, "fogFar", farval);
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
	for (int i = 0; i < 4; ++i) {
		frust->planes[plane][i] /= len;
	}
}

static float __attribute__((aligned (32))) cf_clip[16];

static void calcFrust(struct frustum* frust, float* proj, float* view) {
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

static bool isVisible(struct frustum* frust, float ax, float ay, float az, float bx, float by, float bz) {
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

static float sc_camx, sc_camy, sc_camz;
static bool uc_uproj = false;

void updateCam() {
    static float uc_fov = -1.0, uc_asp = -1.0;
    static avec3 uc_campos;
    static float uc_rotradx, uc_rotrady, uc_rotradz;
    static amat4 uc_proj;
    static amat4 uc_view;
    static avec3 uc_up;
    static avec3 uc_front;
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
    uc_campos[0] = sc_camx = rendinf.campos.x;
    uc_campos[1] = sc_camy = rendinf.campos.y;
    uc_campos[2] = -(sc_camz = rendinf.campos.z);
    uc_rotradx = rendinf.camrot.x * M_PI / 180.0;
    uc_rotrady = (rendinf.camrot.y - 180.0) * M_PI / 180.0;
    uc_rotradz = rendinf.camrot.z * M_PI / 180.0;
    uc_front[0] = (-sin(uc_rotrady)) * cos(uc_rotradx);
    uc_front[1] = sin(uc_rotradx);
    uc_front[2] = cos(uc_rotrady) * cos(uc_rotradx);
    uc_up[0] = sin(uc_rotrady) * sin(uc_rotradx) * cos(uc_rotradz) + cos(uc_rotrady) * (-sin(uc_rotradz));
    uc_up[1] = cos(uc_rotradx) * cos(uc_rotradz);
    uc_up[2] = (-cos(uc_rotrady)) * sin(uc_rotradx) * cos(uc_rotradz) + sin(uc_rotrady) * (-sin(uc_rotradz));
    glm_vec3_add(uc_campos, uc_front, uc_front);
    glm_lookat(uc_campos, uc_front, uc_up, uc_view);
    setShaderProg(shader_block);
    setMat4(rendinf.shaderprog, "view", uc_view);
    //setShaderProg(shader_3d);
    //setMat4(rendinf.shaderprog, "view", uc_view);
    calcFrust(&frust, (float*)uc_proj, (float*)uc_view);
}

void updateUIScale() {
    int x = rendinf.width / 1200 + 1;
    int y = rendinf.height / 900 + 1;
    int s = (x < y) ? x : y;
    if (game_ui[UILAYER_SERVER]) game_ui[UILAYER_SERVER]->scale = s;
    if (game_ui[UILAYER_CLIENT]) game_ui[UILAYER_CLIENT]->scale = s;
    if (game_ui[UILAYER_DBGINF]) game_ui[UILAYER_DBGINF]->scale = s;
    if (game_ui[UILAYER_INGAME]) game_ui[UILAYER_INGAME]->scale = s;
    //printf("Scale UI to [%d] (%dx%d)\n", s, rendinf.width, rendinf.height);
}

static void winch(int w, int h) {
    if (!rendinf.fullscr) {
        rendinf.win_width = w;
        rendinf.win_height = h;
    }
    setFullscreen(rendinf.fullscr);
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

void setFullscreen(bool fullscreen) {
    static int winox = -1, winoy = -1;

    #if defined(USESDL2)
    #else
    glfwSetFramebufferSizeCallback(rendinf.window, NULL);
    #endif

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
    setShaderProg(shader_ui);
    updateUIScale();
    setUniform1f(rendinf.shaderprog, "xsize", rendinf.width);
    setUniform1f(rendinf.shaderprog, "ysize", rendinf.height);
    setShaderProg(shader_framebuffer);
    setUniform2f(rendinf.shaderprog, "fbsize", (float[]){rendinf.width, rendinf.height});

    glBindTexture(GL_TEXTURE_2D, FBTEX);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rendinf.width, rendinf.height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, DBUF);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, rendinf.width, rendinf.height);

    glBindTexture(GL_TEXTURE_2D, UIFBTEX);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rendinf.width, rendinf.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, UIDBUF);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, rendinf.width, rendinf.height);

    glViewport(0, 0, rendinf.width, rendinf.height);

    #if defined(USESDL2)
    #else
    glfwSetFramebufferSizeCallback(rendinf.window, fbsize);
    glfwSwapInterval(rendinf.vsync);
    #endif

    if (inputMode == INPUT_MODE_GAME) {
        resetInput();
    }
}

static bool makeShaderProg(char* hdrtext, char* _vstext, char* _fstext, GLuint* p) {
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

static inline void swapBuffers() {
    #if defined(USESDL2)
    SDL_GL_SwapWindow(rendinf.window);
    #else
    glfwSwapBuffers(rendinf.window);
    #endif
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
    swapBuffers();
}

//static force_inline int64_t i64_mod(int64_t v, int64_t m) {return ((v % m) + m) % m;}

static force_inline void rendGetBlock(int cx, int cz, int x, int y, int z, struct blockdata* b) {
    if (y < 0 || y > 511) {
        bdsetid(b, BLOCKNO_NULL);
        bdsetsubid(b, 0);
        bdsetlight(b, 0, 0, 0, 31);
        return;
    }
    cx += x / 16 - (x < 0);
    cz -= z / 16 - (z < 0);
    if (cx < 0 || cz < 0 || cx >= (int)rendinf.chunks->info.width || cz >= (int)rendinf.chunks->info.width) {
        bdsetid(b, BLOCKNO_BORDER);
        bdsetsubid(b, 0);
        bdsetlight(b, 0, 0, 0, 0);
        return;
    }
    x &= 0xF;
    z &= 0xF;
    int c = cx + cz * rendinf.chunks->info.width;
    if (!rendinf.chunks->renddata[c].generated) {
        bdsetid(b, BLOCKNO_BORDER);
        bdsetsubid(b, 0);
        bdsetlight(b, 0, 0, 0, 0);
        return;
    }
    if (y > rendinf.chunks->metadata[c].top) {
        bdsetid(b, BLOCKNO_NULL);
        bdsetsubid(b, 0);
        bdsetlight(b, 0, 0, 0, 31);
        return;
    }
    *b = rendinf.chunks->data[c][y * 256 + z * 16 + x];
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
//static uint8_t water;

struct msgdata_msg {
    bool valid;
    bool dep;
    int lvl;
    int64_t x;
    int64_t z;
    uint64_t id;
};

struct msgdata {
    bool async;
    int size;
    int rptr;
    int wptr;
    struct msgdata_msg* msg;
    pthread_mutex_t lock;
};

struct msgdata chunkmsgs[CHUNKUPDATE_PRIO__MAX];

static void initMsgData(struct msgdata* mdata) {
    mdata->async = false;
    mdata->size = 0;
    mdata->rptr = -1;
    mdata->wptr = -1;
    mdata->msg = malloc(0);
    pthread_mutex_init(&mdata->lock, NULL);
}

static void deinitMsgData(struct msgdata* mdata) {
    pthread_mutex_lock(&mdata->lock);
    free(mdata->msg);
    pthread_mutex_unlock(&mdata->lock);
    pthread_mutex_destroy(&mdata->lock);
}

static void addMsg(struct msgdata* mdata, int64_t x, int64_t z, uint64_t id, bool dep, int lvl) {
    static uint64_t mdataid = 0;
    pthread_mutex_lock(&mdata->lock);
    if (mdata->async) {
        int index = -1;
        for (int i = 0; i < mdata->size; ++i) {
            if (!mdata->msg[i].valid) {
                index = i;
                break;
            }
        }
        if (index == -1) {
            index = mdata->size++;
            mdata->msg = realloc(mdata->msg, mdata->size * sizeof(*mdata->msg));
        }
        mdata->msg[index].valid = true;
        mdata->msg[index].dep = dep;
        mdata->msg[index].lvl = lvl;
        mdata->msg[index].x = x;
        mdata->msg[index].z = z;
        mdata->msg[index].id = (dep) ? id : mdataid++;
    } else {
        if (mdata->wptr < 0 || mdata->rptr >= mdata->size) {
            mdata->rptr = 0;
            mdata->size = 0;
        }
        mdata->wptr = mdata->size++;
        //printf("[%lu]: wptr: [%d] size: [%d]\n", (uintptr_t)mdata, mdata->wptr, mdata->size);
        mdata->msg = realloc(mdata->msg, mdata->size * sizeof(*mdata->msg));
        mdata->msg[mdata->wptr].valid = true;
        mdata->msg[mdata->wptr].dep = dep;
        mdata->msg[mdata->wptr].lvl = lvl;
        mdata->msg[mdata->wptr].x = x;
        mdata->msg[mdata->wptr].z = z;
        mdata->msg[mdata->wptr].id = (dep) ? id : mdataid++;
    }
    pthread_mutex_unlock(&mdata->lock);
}

static bool getNextMsg(struct msgdata* mdata, struct msgdata_msg* msg) {
    pthread_mutex_lock(&mdata->lock);
    if (mdata->async) {
        for (int i = 0; i < mdata->size; ++i) {
            if (mdata->msg[i].valid) {
                msg->valid = mdata->msg[i].valid;
                msg->dep = mdata->msg[i].dep;
                msg->lvl = mdata->msg[i].lvl;
                msg->x = mdata->msg[i].x;
                msg->z = mdata->msg[i].z;
                msg->id = mdata->msg[i].id;
                mdata->msg[i].valid = false;
                pthread_mutex_unlock(&mdata->lock);
                return true;
            }
        }
    } else {
        if (mdata->rptr >= 0) {
            for (int i = mdata->rptr; i < mdata->size; ++i) {
                if (mdata->msg[i].valid) {
                    msg->valid = mdata->msg[i].valid;
                    msg->dep = mdata->msg[i].dep;
                    msg->lvl = mdata->msg[i].lvl;
                    msg->x = mdata->msg[i].x;
                    msg->z = mdata->msg[i].z;
                    msg->id = mdata->msg[i].id;
                    mdata->msg[i].valid = false;
                    mdata->rptr = i + 1;
                    //printf("[%lu]: rptr: [%d] size: [%d]\n", (uintptr_t)mdata, mdata->rptr, mdata->size);
                    pthread_mutex_unlock(&mdata->lock);
                    //printf("returning [%d]/[%d]\n", i + 1, mdata->size);
                    return true;
                }
            }
        }
    }
    pthread_mutex_unlock(&mdata->lock);
    return false;
}

static bool mesheractive = false;
void updateChunk(int64_t x, int64_t z, int p, int updatelvl) {
    if (!mesheractive) return;
    addMsg(&chunkmsgs[p], x, z, 0, false, updatelvl);
}

static uint32_t constBlockVert[2][6][7] = {
    {
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // U
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // R
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // F
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // D
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // L
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // B
    },
    {
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // U
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // R
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // F
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // D
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // L
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // B
    }
};

//static int light_n_tweak[6] = {0, 0, 0, 0, 0, 0};
static int light_n_tweak[6] = {0, 2, 1, 4, 2, 3};
//static int light_n_tweak[6] = {0, 4, 2, 8, 4, 6};
//static int light_n_tweak[6] = {0, 1, 1, 2, 1, 1};
//static int light_n_tweak[6] = {0, 1, 1, 1, 1, 1};

struct quadcmp {
    float dist;
    uint32_t data[11];
};
static int compare(const void* b, const void* a) {
    float fa = ((struct quadcmp*)a)->dist;
    float fb = ((struct quadcmp*)b)->dist;
    return (fa > fb) - (fa < fb);
}
static force_inline float dist3d(float x0, float y0, float z0, float x1, float y1, float z1) {
    float dx = x1 - x0;
    dx *= dx;
    float dy = y1 - y0;
    dy *= dy;
    float dz = z1 - z0;
    dz *= dz;
    return sqrtf(dx + dy + dz);
}
//TODO: Optimize
static force_inline void _sortChunk_inline(int32_t c, int xoff, int zoff, bool update) {
    if (c < 0) c = (xoff + rendinf.chunks->info.dist) + (rendinf.chunks->info.width - (zoff + rendinf.chunks->info.dist) - 1) * rendinf.chunks->info.width;
    if (update) {
        if (!rendinf.chunks->renddata[c].visfull || !rendinf.chunks->renddata[c].sortvert || !rendinf.chunks->renddata[c].qcount[1]) return;
        float camx = sc_camx - xoff * 16.0;
        float camy = sc_camy;
        float camz = sc_camz - zoff * 16.0;
        int32_t tmpsize = rendinf.chunks->renddata[c].qcount[1];
        struct quadcmp* data = malloc(tmpsize * sizeof(struct quadcmp));
        uint32_t* dptr = rendinf.chunks->renddata[c].sortvert;
        for (int i = 0; i < tmpsize; ++i) {
            uint32_t sv1;
            float vx, vy, vz;
            float dist = 0.0;
            for (int j = 0; j < 4; ++j) {
                data[i].data[j] = sv1 = *dptr++;
                vx = (float)((int)(((sv1 >> 24) & 255) + ((sv1 >> 23) & 1))) / 16.0 - 8.0;
                vy = (float)((int)(((sv1 >> 8) & 8191) + ((sv1 >> 22) & 1))) / 16.0;
                vz = (float)((int)((sv1 & 255) + ((sv1 >> 21) & 1))) / 16.0 - 8.0;
                float tmpdist = dist3d(camx, camy, camz, vx, vy, vz);
                if (tmpdist > dist) dist = tmpdist;
            }
            data[i].dist = dist;
            for (int j = 4; j < 11; ++j) {
                data[i].data[j] = *dptr++;
            }
        }
        qsort(data, tmpsize, sizeof(struct quadcmp), compare);
        dptr = rendinf.chunks->renddata[c].sortvert;
        for (int i = 0; i < tmpsize; ++i) {
            for (int j = 0; j < 11; ++j) {
                *dptr++ = data[i].data[j];
            }
        }
        free(data);
        glBindBuffer(GL_ARRAY_BUFFER, rendinf.chunks->renddata[c].VBO[1]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, tmpsize * sizeof(uint32_t) * 11, rendinf.chunks->renddata[c].sortvert);
    } else {
        rendinf.chunks->renddata[c].remesh[1] = true;
        rendinf.chunks->renddata[c].ready = true;
    }
}

static void _sortChunk(int32_t c, int xoff, int zoff, bool update) {
    _sortChunk_inline(c, xoff, zoff, update);
}

void sortChunk(int32_t c, int xoff, int zoff, bool update) {
    _sortChunk_inline(c, xoff, zoff, update);
}

static inline void calcNLight_stub(struct blockdata* data, int cx, int cz, int x, int y, int z) {
    int off = x + z * 16 + y * 256;
    uint8_t id = bdgetid(data[off]);
    uint8_t subid = bdgetsubid(data[off]);
    if (id && !blockinf[id].data[subid].transparency) {
        bdsetlightn(&data[off], 0);
        return;
    }
    int8_t maxlight = 0;
    uint8_t tmplight;
    struct blockdata bdata;
    memset(&bdata, 0, sizeof(bdata));
    rendGetBlock(cx, cz, x, y + 1, z, &bdata);
    if ((tmplight = bdgetlightn(bdata)) > maxlight) {
        maxlight = tmplight;
    }
    rendGetBlock(cx, cz, x + 1, y, z, &bdata);
    if ((tmplight = bdgetlightn(bdata)) > maxlight) {
        maxlight = tmplight;
    }
    rendGetBlock(cx, cz, x, y, z + 1, &bdata);
    if ((tmplight = bdgetlightn(bdata)) > maxlight) {
        maxlight = tmplight;
    }
    rendGetBlock(cx, cz, x, y - 1, z, &bdata);
    if ((tmplight = bdgetlightn(bdata)) > maxlight) {
        maxlight = tmplight;
    }
    rendGetBlock(cx, cz, x - 1, y, z, &bdata);
    if ((tmplight = bdgetlightn(bdata)) > maxlight) {
        maxlight = tmplight;
    }
    rendGetBlock(cx, cz, x, y, z - 1, &bdata);
    if ((tmplight = bdgetlightn(bdata)) > maxlight) {
        maxlight = tmplight;
    }
    --maxlight;
    if (maxlight < 0) maxlight = 0;
    tmplight = bdgetlightn(data[off]);
    if (maxlight > tmplight) bdsetlightn(&data[off], maxlight);
}

static bool clientlighting = false;
static void calcNLight(int cx, int cz, int maxy) {
    uint32_t c = cx + cz * rendinf.chunks->info.width;
    /*
    if (cx < 0 || cz < 0 || cx >= (int)rendinf.chunks->info.width || cz >= (int)rendinf.chunks->info.width || !rendinf.chunks->renddata[c].generated) {
        return;
    }
    */
    //if (maxy > 511) maxy = 511;
    struct blockdata* data = rendinf.chunks->data[c];
    if (!clientlighting) {
        for (int i = 0; i < 256; ++i) {
            for (int y = maxy; y >= 0; --y) {
                int off = y * 256 + i;
                bdsetlightn(&data[off], 31);
            }
        }
        return;
    }
    {
        static uint8_t water = 0;
        if (!water) water = blockNoFromID("water");
        for (int i = 0; i < 256; ++i) {
            uint8_t nlight = 31;
            for (int y = maxy; y >= 0; --y) {
                int off = y * 256 + i;
                uint8_t id = bdgetid(data[off]);
                if (id == water) {
                    if (nlight > 0) --nlight;
                } else if (id) {
                    nlight = 0;
                }
                bdsetlightn(&data[off], nlight);
            }
        }
    }
    for (int y = 0; y <= maxy; ++y) {
        for (int z = 0; z <= 15; ++z) {
            for (int x = 0; x <= 15; ++x) {
                calcNLight_stub(data, cx, cz, x, y, z);
            }
        }
    }
    for (int y = 0; y <= maxy; ++y) {
        for (int z = 0; z <= 15; ++z) {
            for (int x = 15; x >= 0; --x) {
                calcNLight_stub(data, cx, cz, x, y, z);
            }
        }
    }
    for (int y = 0; y <= maxy; ++y) {
        for (int z = 15; z >= 0; --z) {
            for (int x = 0; x <= 15; ++x) {
                calcNLight_stub(data, cx, cz, x, y, z);
            }
        }
    }
    for (int y = 0; y <= maxy; ++y) {
        for (int z = 15; z >= 0; --z) {
            for (int x = 15; x >= 0; --x) {
                calcNLight_stub(data, cx, cz, x, y, z);
            }
        }
    }
    for (int y = maxy; y >= 0; --y) {
        for (int z = 0; z <= 15; ++z) {
            for (int x = 0; x <= 15; ++x) {
                calcNLight_stub(data, cx, cz, x, y, z);
            }
        }
    }
    for (int y = maxy; y >= 0; --y) {
        for (int z = 0; z <= 15; ++z) {
            for (int x = 15; x >= 0; --x) {
                calcNLight_stub(data, cx, cz, x, y, z);
            }
        }
    }
    for (int y = maxy; y >= 0; --y) {
        for (int z = 15; z >= 0; --z) {
            for (int x = 0; x <= 15; ++x) {
                calcNLight_stub(data, cx, cz, x, y, z);
            }
        }
    }
    for (int y = maxy; y >= 0; --y) {
        for (int z = 15; z >= 0; --z) {
            for (int x = 15; x >= 0; --x) {
                calcNLight_stub(data, cx, cz, x, y, z);
            }
        }
    }
}

// X = 8, Y = 4, XY = C
// Z = 2, XZ = A, YZ = 6, XYZ = E
static const uint32_t constVertPos[6][4] = {
    {0x00600F0F, 0x0FE00F0F, 0x00400F00, 0x0FC00F00}, // U
    {0x0FC00F00, 0x0FE00F0F, 0x0F800000, 0x0FA0000F}, // R
    {0x0FE00F0F, 0x00600F0F, 0x0FA0000F, 0x0020000F}, // F
    {0x00000000, 0x0F800000, 0x0020000F, 0x0FA0000F}, // D
    {0x00600F0F, 0x00400F00, 0x0020000F, 0x00000000}, // L
    {0x00400F00, 0x0FC00F00, 0x00000000, 0x0F800000}  // B
};
#define maddvert(_v, s, l, v, bv) {\
    if (*(l) >= *(s)) {\
        *(s) *= 2;\
        *(_v) = realloc(*(_v), *(s) * sizeof(**(_v)));\
        *(v) = *(_v) + *(l);\
    }\
    **(v) = (bv);\
    ++*(v); ++*(l);\
}
static void mesh(int64_t x, int64_t z, uint64_t id) {
    struct blockdata bdata;
    struct blockdata bdata2[6];
    pthread_mutex_lock(&rendinf.chunks->lock);
    int64_t nx = (x - rendinf.chunks->xoff) + rendinf.chunks->info.dist;
    int64_t nz = rendinf.chunks->info.width - ((z - rendinf.chunks->zoff) + rendinf.chunks->info.dist) - 1;
    {
        uint32_t c = nx + nz * rendinf.chunks->info.width;
        if (nx < 0 || nz < 0 || nx >= rendinf.chunks->info.width || nz >= rendinf.chunks->info.width || !rendinf.chunks->renddata[c].generated) {
            goto lblcontinue;
        }
        if (id < rendinf.chunks->renddata[c].updateid) {goto lblcontinue;}
        uint64_t time = altutime();
        while ((rendinf.chunks->renddata[c].visfull && (rendinf.chunks->renddata[c].remesh[0] || rendinf.chunks->renddata[c].remesh[1])) &&
        altutime() - time < 200000) {
            //static uint64_t tmp = 0;
            //printf("! [%"PRId64"]\n", tmp++);
            pthread_mutex_unlock(&rendinf.chunks->lock);
            microwait(0);
            pthread_mutex_lock(&rendinf.chunks->lock);
            if (quitRequest) goto lblcontinue;
            if (nx < 0 || nz < 0 || nx >= rendinf.chunks->info.width || nz >= rendinf.chunks->info.width || !rendinf.chunks->renddata[c].generated) {
                goto lblcontinue;
            }
        }
        //printf("meshing [%"PRId64", %"PRId64"] -> [%"PRId64", %"PRId64"] (c=%d, offset=[%"PRId64", %"PRId64"])\n",
        //    x, z, nx, nz, c, rendinf.chunks->xoff, rendinf.chunks->zoff);
    }
    /*
    printf("mesh: [%"PRId64", %"PRId64"]\n", x, z);
    uint64_t stime = altutime();
    */
    //uint64_t secttime[4] = {0};
    int vpsize = 65536;
    int vpsize2 = 65536;
    uint32_t* _vptr = malloc(vpsize * sizeof(uint32_t));
    uint32_t* _vptr2 = malloc(vpsize2 * sizeof(uint32_t));
    int vplen = 0;
    int vplen2 = 0;
    uint32_t* vptr = _vptr;
    uint32_t* vptr2 = _vptr2;
    struct {
        uint32_t pos[4];
        uint32_t texinf;
        uint16_t texcoord[4];
        uint32_t extexinf;
        uint8_t texplus[4];
        uint8_t light_n[4];
        uint16_t light_rgb[4];
    } vertData;
    //secttime[0] = altutime();
    nx = (x - rendinf.chunks->xoff) + rendinf.chunks->info.dist;
    nz = rendinf.chunks->info.width - ((z - rendinf.chunks->zoff) + rendinf.chunks->info.dist) - 1;
    if (nx < 0 || nz < 0 || nx >= rendinf.chunks->info.width || nz >= rendinf.chunks->info.width) {
        free(_vptr);
        free(_vptr2);
        goto lblcontinue;
    }
    uint32_t yvoff[32];
    uint32_t yvcount[32];
    for (int i = 0; i < 32; ++i) {
        yvoff[i] = 0;
        yvcount[i] = 0;
    }
    int c = nx + nz * rendinf.chunks->info.width;
    //secttime[0] = altutime() - secttime[0];
    calcNLight(nx, nz, rendinf.chunks->metadata[c].top);
    uint8_t vispass[32][6][6];
    memset(vispass, 1, sizeof(vispass));
    pthread_mutex_unlock(&rendinf.chunks->lock);
    microwait(0);
    uint32_t vplenold = 0;
    int maxy = rendinf.chunks->metadata[c].alignedtop - 15;
    int ychunk = rendinf.chunks->metadata[c].sects - 1;
    for (; maxy >= 0; maxy -= 16) {
        //uint64_t secttime2[2];
        //secttime2[0] = altutime();
        pthread_mutex_lock(&rendinf.chunks->lock);
        nx = (x - rendinf.chunks->xoff) + rendinf.chunks->info.dist;
        nz = rendinf.chunks->info.width - ((z - rendinf.chunks->zoff) + rendinf.chunks->info.dist) - 1;
        if (nx < 0 || nz < 0 || nx >= rendinf.chunks->info.width || nz >= rendinf.chunks->info.width) {
            free(_vptr);
            free(_vptr2);
            goto lblcontinue;
        }
        for (int y = maxy + 15; y >= maxy; --y) {
            for (int z = 0; z < 16; ++z) {
                for (int x = 0; x < 16; ++x) {
                    /*
                    if (x == 0 && z == 0) {
                        printf("MESH: READ: [%d, %d, %d] on [%"PRId64", %"PRId64"] (%d): top=%d, alignedtop=%d, sects=%d\n",
                            x, y, z, nx, nz, c,
                            rendinf.chunks->metadata[c].top, rendinf.chunks->metadata[c].alignedtop, rendinf.chunks->metadata[c].sects);
                    }
                    */
                    rendGetBlock(nx, nz, x, y, z, &bdata);
                    /*
                    if (x == 0 && z == 0) {
                        puts("OK");
                    }
                    */
                    uint8_t bdataid = bdgetid(bdata);
                    if (!bdataid || !blockinf[bdataid].id) continue;
                    uint8_t bdatasubid = bdgetsubid(bdata);
                    if (!blockinf[bdataid].data[bdatasubid].id) continue;
                    rendGetBlock(nx, nz, x, y + 1, z, &bdata2[0]);
                    rendGetBlock(nx, nz, x + 1, y, z, &bdata2[1]);
                    rendGetBlock(nx, nz, x, y, z + 1, &bdata2[2]);
                    rendGetBlock(nx, nz, x, y - 1, z, &bdata2[3]);
                    rendGetBlock(nx, nz, x - 1, y, z, &bdata2[4]);
                    rendGetBlock(nx, nz, x, y, z - 1, &bdata2[5]);
                    for (int i = 0; i < 6; ++i) {
                        uint8_t bdata2id = bdgetid(bdata2[i]);
                        uint8_t bdata2subid = bdgetsubid(bdata2[i]);
                        if (bdata2id && blockinf[bdata2id].id) {
                            if (blockinf[bdata2id].data[bdata2subid].transparency) {
                                if (bdataid == bdata2id && bdatasubid == bdata2subid) continue;
                            } else {
                                continue;
                            }
                        }
                        if (bdata2id == BLOCKNO_BORDER) continue;

                        vertData.pos[0] = ((x << 28) | (y << 12) | (z << 4)) & 0xF01FF0F0;
                        vertData.pos[1] = vertData.pos[0] | constVertPos[i][1];
                        vertData.pos[2] = vertData.pos[0] | constVertPos[i][2];
                        vertData.pos[3] = vertData.pos[0] | constVertPos[i][3];
                        vertData.pos[0] |= constVertPos[i][0];

                        vertData.texinf = ((blockinf[bdataid].data[bdatasubid].texoff[i] << 16) & 0xFFFF0000);
                        vertData.texinf |= ((blockinf[bdataid].data[bdatasubid].anict[i] << 8) & 0xFF00);
                        vertData.texinf |= (blockinf[bdataid].data[bdatasubid].anidiv & 0xFF);

                        vertData.texcoord[0] = 0x0000;
                        vertData.texcoord[1] = 0x0F00;
                        vertData.texcoord[2] = 0x000F;
                        vertData.texcoord[3] = 0x0F0F;

                        vertData.extexinf = (blockinf[bdataid].data[bdatasubid].transparency << 16);

                        vertData.texplus[0] = 0x0;
                        vertData.texplus[1] = 0x2;
                        vertData.texplus[2] = 0x1;
                        vertData.texplus[3] = 0x3;

                        int8_t light_n = bdgetlightn(bdata2[i]) - light_n_tweak[i];
                        if (light_n < 0) light_n = 0;
                        vertData.light_n[0] = light_n;
                        vertData.light_n[1] = light_n;
                        vertData.light_n[2] = light_n;
                        vertData.light_n[3] = light_n;

                        int8_t light_r = bdgetlightr(bdata2[i]);
                        int8_t light_g = bdgetlightg(bdata2[i]);
                        int8_t light_b = bdgetlightb(bdata2[i]);
                        vertData.light_rgb[0] = (light_r << 10) | (light_g << 5) | light_b;
                        vertData.light_rgb[1] = vertData.light_rgb[0];
                        vertData.light_rgb[2] = vertData.light_rgb[0];
                        vertData.light_rgb[3] = vertData.light_rgb[0];

                        #define pushvert(v) maddvert(&_vptr, &vpsize, &vplen, &vptr, (v))
                        #define pushvert2(v) maddvert(&_vptr2, &vpsize2, &vplen2, &vptr2, (v))
                        if (blockinf[bdataid].data[bdatasubid].transparency >= 2) {
                            if (!bdata2id && blockinf[bdataid].data[bdatasubid].backfaces) {
                                pushvert2(vertData.pos[3]);
                                pushvert2(vertData.pos[1]);
                                pushvert2(vertData.pos[2]);
                                pushvert2(vertData.pos[0]);
                                pushvert2(vertData.texinf);
                                pushvert2((vertData.texcoord[3] << 16) | vertData.texcoord[1]);
                                pushvert2((vertData.texcoord[2] << 16) | vertData.texcoord[0]);
                                pushvert2(vertData.extexinf | (vertData.texplus[3] << 6) | (vertData.texplus[1] << 4) | (vertData.texplus[2] << 2) | vertData.texplus[0]);
                                pushvert2((vertData.light_n[3] << 24) | (vertData.light_n[1] << 16) | (vertData.light_n[2] << 8) | vertData.light_n[0]);
                                pushvert2((vertData.light_rgb[3] << 16) | vertData.light_rgb[1]);
                                pushvert2((vertData.light_rgb[2] << 16) | vertData.light_rgb[0]);
                            }
                            pushvert2(vertData.pos[0]);
                            pushvert2(vertData.pos[1]);
                            pushvert2(vertData.pos[2]);
                            pushvert2(vertData.pos[3]);
                            pushvert2(vertData.texinf);
                            pushvert2((vertData.texcoord[0] << 16) | vertData.texcoord[1]);
                            pushvert2((vertData.texcoord[2] << 16) | vertData.texcoord[3]);
                            pushvert2(vertData.extexinf | (vertData.texplus[0] << 6) | (vertData.texplus[1] << 4) | (vertData.texplus[2] << 2) | vertData.texplus[3]);
                            pushvert2((vertData.light_n[0] << 24) | (vertData.light_n[1] << 16) | (vertData.light_n[2] << 8) | vertData.light_n[3]);
                            pushvert2((vertData.light_rgb[0] << 16) | vertData.light_rgb[1]);
                            pushvert2((vertData.light_rgb[2] << 16) | vertData.light_rgb[3]);
                        } else {
                            pushvert(vertData.pos[0]);
                            pushvert(vertData.pos[1]);
                            pushvert(vertData.pos[2]);
                            pushvert(vertData.pos[3]);
                            pushvert(vertData.texinf);
                            pushvert((vertData.texcoord[0] << 16) | vertData.texcoord[1]);
                            pushvert((vertData.texcoord[2] << 16) | vertData.texcoord[3]);
                            pushvert(vertData.extexinf | (vertData.texplus[0] << 6) | (vertData.texplus[1] << 4) | (vertData.texplus[2] << 2) | vertData.texplus[3]);
                            pushvert((vertData.light_n[0] << 24) | (vertData.light_n[1] << 16) | (vertData.light_n[2] << 8) | vertData.light_n[3]);
                            pushvert((vertData.light_rgb[0] << 16) | vertData.light_rgb[1]);
                            pushvert((vertData.light_rgb[2] << 16) | vertData.light_rgb[3]);
                        }
                    }
                }
            }
        }
        //secttime2[0] = altutime() - secttime2[0];
        //secttime2[1] = altutime();
        pthread_mutex_unlock(&rendinf.chunks->lock);
        microwait(0);
        struct pq {
            uint8_t x;
            uint16_t y;
            uint8_t z;
        } posqueue[4096];
        uint8_t visited[16][16][16] = {{{0}}};
        pthread_mutex_lock(&rendinf.chunks->lock);
        nx = (x - rendinf.chunks->xoff) + rendinf.chunks->info.dist;
        nz = rendinf.chunks->info.width - ((z - rendinf.chunks->zoff) + rendinf.chunks->info.dist) - 1;
        if (nx < 0 || nz < 0 || nx >= rendinf.chunks->info.width || nz >= rendinf.chunks->info.width) {
            free(_vptr);
            free(_vptr2);
            goto lblcontinue;
        }
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                vispass[ychunk][i][j] = false;
            }
        }
        for (int _y = maxy + 15; _y >= maxy; --_y) {
            for (int _z = 0; _z < 16; ++_z) {
                for (int _x = 0; _x < 16; ++_x) {
                    rendGetBlock(nx, nz, _x, _y, _z, &bdata);
                    if (blockinf[bdgetid(bdata)].data[bdgetsubid(bdata)].transparency && !visited[_x][_y % 16][_z]) {
                        uint8_t touched[6] = {0};
                        //memset(posqueue, 0, sizeof(posqueue));
                        int pqptr = 0;

                        posqueue[pqptr++] = (struct pq){.x = _x, .y = _y, .z = _z};
                        visited[_x][_y % 16][_z] = 1;

                        //printf("FILL: [%d, %d, %d] [%d, %d]\n", _x, _y, _z, maxy, maxy + 15);
                        int x, y, z;
                        while (pqptr > 0) {
                            --pqptr;
                            x = posqueue[pqptr].x;
                            y = posqueue[pqptr].y;
                            z = posqueue[pqptr].z;

                            if (y < maxy + 15) {
                                rendGetBlock(nx, nz, x, y + 1, z, &bdata);
                                if (blockinf[bdgetid(bdata)].data[bdgetsubid(bdata)].transparency && !visited[x][(y + 1) % 16][z]) {
                                    posqueue[pqptr++] = (struct pq){.x = x, .y = y + 1, .z = z};
                                    visited[x][(y + 1) % 16][z] = 1;
                                    //printf("ADD Y+1 [%d, %d, %d] [%d]\n", posqueue[pqptr - 1].x, posqueue[pqptr - 1].y, posqueue[pqptr - 1].z, pqptr);
                                }
                            } else {
                                touched[CVIS_UP] = 1;
                            }
                            if (x < 15) {
                                rendGetBlock(nx, nz, x + 1, y, z, &bdata);
                                if (blockinf[bdgetid(bdata)].data[bdgetsubid(bdata)].transparency && !visited[x + 1][y % 16][z]) {
                                    posqueue[pqptr++] = (struct pq){.x = x + 1, .y = y, .z = z};
                                    visited[x + 1][y % 16][z] = 1;
                                    //printf("ADD X+1 [%d, %d, %d] [%d]\n", posqueue[pqptr - 1].x, posqueue[pqptr - 1].y, posqueue[pqptr - 1].z, pqptr);
                                }
                            } else {
                                touched[CVIS_RIGHT] = 1;
                            }
                            if (z < 15) {
                                rendGetBlock(nx, nz, x, y, z + 1, &bdata);
                                if (blockinf[bdgetid(bdata)].data[bdgetsubid(bdata)].transparency && !visited[x][y % 16][z + 1]) {
                                    posqueue[pqptr++] = (struct pq){.x = x, .y = y, .z = z + 1};
                                    visited[x][y % 16][z + 1] = 1;
                                    //printf("ADD Z+1 [%d, %d, %d] [%d]\n", posqueue[pqptr - 1].x, posqueue[pqptr - 1].y, posqueue[pqptr - 1].z, pqptr);
                                }
                            } else {
                                touched[CVIS_FRONT] = 1;
                            }
                            if (y > maxy) {
                                rendGetBlock(nx, nz, x, y - 1, z, &bdata);
                                if (blockinf[bdgetid(bdata)].data[bdgetsubid(bdata)].transparency && !visited[x][(y - 1) % 16][z]) {
                                    posqueue[pqptr++] = (struct pq){.x = x, .y = y - 1, .z = z};
                                    visited[x][(y - 1) % 16][z] = 1;
                                    //printf("ADD Y-1 [%d, %d, %d] [%d]\n", posqueue[pqptr - 1].x, posqueue[pqptr - 1].y, posqueue[pqptr - 1].z, pqptr);
                                }
                            } else {
                                touched[CVIS_DOWN] = 1;
                            }
                            if (x > 0) {
                                rendGetBlock(nx, nz, x - 1, y, z, &bdata);
                                if (blockinf[bdgetid(bdata)].data[bdgetsubid(bdata)].transparency && !visited[x - 1][y % 16][z]) {
                                    posqueue[pqptr++] = (struct pq){.x = x - 1, .y = y, .z = z};
                                    visited[x - 1][y % 16][z] = 1;
                                    //printf("ADD X-1 [%d, %d, %d] [%d]\n", posqueue[pqptr - 1].x, posqueue[pqptr - 1].y, posqueue[pqptr - 1].z, pqptr);
                                }
                            } else {
                                touched[CVIS_LEFT] = 1;
                            }
                            if (z > 0) {
                                rendGetBlock(nx, nz, x, y, z - 1, &bdata);
                                if (blockinf[bdgetid(bdata)].data[bdgetsubid(bdata)].transparency && !visited[x][y % 16][z - 1]) {
                                    posqueue[pqptr++] = (struct pq){.x = x, .y = y, .z = z - 1};
                                    visited[x][y % 16][z - 1] = 1;
                                    //printf("ADD Z-1 [%d, %d, %d] [%d]\n", posqueue[pqptr - 1].x, posqueue[pqptr - 1].y, posqueue[pqptr - 1].z, pqptr);
                                }
                            } else {
                                touched[CVIS_BACK] = 1;
                            }

                            //printf("[%d, %d, %d] [%d]\n", x, y, z, pqptr);
                        }

                        /*
                        printf("FILL: [%d, %d, %d] [%d, %d] [", _x, _y, _z, maxy, maxy + 15);
                        for (int i = 0; i < 6; ++i) {
                            printf("%d", touched[i]);
                        }
                        printf("]\n");
                        */
                        //printf("touched: [%d, %d, %d, %d, %d, %d]\n", touched[0], touched[1], touched[2], touched[3], touched[4], touched[5]);
                        for (int i = 0; i < 6; ++i) {
                            for (int j = 0; j < 6; ++j) {
                                if (i != j && touched[i] && touched[j]) {
                                    vispass[ychunk][j][i] = vispass[ychunk][i][j] = true;
                                }
                            }
                        }
                    }
                }
            }
        }
        //secttime2[1] = altutime() - secttime2[1];
        /*
        {
            printf("VISPASS [%"PRId64", %d, %"PRId64"]:\n", nx, ychunk, nz);
            for (int i = 0; i < 6; ++i) {
                for (int j = 0; j < 6; ++j) {
                    printf("%d", vispass[ychunk][i][j]);
                }
                putchar('\n');
            }
        }
        */
        //secttime[1] += secttime2[0];
        //secttime[2] += secttime2[1];
        yvoff[ychunk] = vplenold;
        yvcount[ychunk] = vplen - vplenold;
        vplenold = vplen;
        --ychunk;
        pthread_mutex_unlock(&rendinf.chunks->lock);
        microwait(0);
    }
    pthread_mutex_lock(&rendinf.chunks->lock);
    nx = (x - rendinf.chunks->xoff) + rendinf.chunks->info.dist;
    nz = rendinf.chunks->info.width - ((z - rendinf.chunks->zoff) + rendinf.chunks->info.dist) - 1;
    if (nx < 0 || nz < 0 || nx >= rendinf.chunks->info.width || nz >= rendinf.chunks->info.width) {
        free(_vptr);
        free(_vptr2);
        goto lblcontinue;
    }
    c = nx + nz * rendinf.chunks->info.width;
    if (id >= rendinf.chunks->renddata[c].updateid) {
        if (rendinf.chunks->renddata[c].vertices[0]) free(rendinf.chunks->renddata[c].vertices[0]);
        if (rendinf.chunks->renddata[c].vertices[1]) free(rendinf.chunks->renddata[c].vertices[1]);
        rendinf.chunks->renddata[c].vcount[0] = vplen;
        rendinf.chunks->renddata[c].vcount[1] = vplen2;
        for (int i = 0; i < 32; ++i) {
            rendinf.chunks->renddata[c].yvoff[i] = yvoff[i];
            rendinf.chunks->renddata[c].yvcount[i] = yvcount[i];
        }
        memcpy(rendinf.chunks->renddata[c].vispass, vispass, sizeof(rendinf.chunks->renddata->vispass));
        rendinf.chunks->renddata[c].vertices[0] = _vptr;
        rendinf.chunks->renddata[c].vertices[1] = _vptr2;
        rendinf.chunks->renddata[c].updateid = id;
        rendinf.chunks->renddata[c].remesh[0] = true;
        rendinf.chunks->renddata[c].remesh[1] = true;
        //printf("meshed: [%"PRId64", %"PRId64"] ([%"PRId64", %"PRId64"])\n", x, z, nx, nz);
        //printf("meshed: [%"PRId64", %"PRId64"] -> [%d][%d], [%d][%d]\n", x, z, vpsize, vplen, vpsize2, vplen2);
        /*
        double time = (altutime() - stime) / 1000.0;
        printf("meshed: [%"PRId64", %"PRId64"] in [%lgms]\n", x, z, time);
        */
        /*
        time = secttime[0] / 1000.0;
        printf("    find low: [%lgms]\n", time);
        time = secttime[1] / 1000.0;
        printf("    mesh: [%lgms]\n", time);
        time = secttime[2] / 1000.0;
        printf("    flood fill: [%lgms]\n", time);
        */
    } else {
        free(_vptr);
        free(_vptr2);
    }
    lblcontinue:;
    pthread_mutex_unlock(&rendinf.chunks->lock);
    microwait(0);
}

static force_inline void setready(int64_t x, int64_t z, bool val) {
    int64_t nx = (x - rendinf.chunks->xoff) + rendinf.chunks->info.dist;
    int64_t nz = rendinf.chunks->info.width - ((z - rendinf.chunks->zoff) + rendinf.chunks->info.dist) - 1;
    if (nx < 0 || nz < 0 || nx >= (int32_t)rendinf.chunks->info.width || nz >= (int32_t)rendinf.chunks->info.width) return;
    uint32_t c = nx + nz * rendinf.chunks->info.width;
    rendinf.chunks->renddata[c].ready = val;
    //printf("set ready on [%"PRId64", %"PRId64"] -> [%"PRId64", %"PRId64"] (c=%d, offset=[%"PRId64", %"PRId64"])\n", x, z, nx, nz, c, cxo, czo);
}

bool lazymesh;

static void* meshthread(void* args) {
    (void)args;
    uint64_t acttime = altutime();
    struct msgdata_msg msg;
    while (!quitRequest && mesheractive) {
        bool activity = false;
        int p = 0;
        if (getNextMsg(&chunkmsgs[(p = CHUNKUPDATE_PRIO_HIGH)], &msg) ||
        getNextMsg(&chunkmsgs[(p = CHUNKUPDATE_PRIO_NORMAL)], &msg) ||
        getNextMsg(&chunkmsgs[(p = CHUNKUPDATE_PRIO_LOW)], &msg)) {
            //uint64_t stime = altutime();
            activity = true;
            mesh(msg.x, msg.z, msg.id);
            if (!msg.dep) {
                if (lazymesh || p != CHUNKUPDATE_PRIO_HIGH) {
                    if (msg.lvl >= 1) {
                        addMsg(&chunkmsgs[p], msg.x, msg.z + 1, msg.id, true, msg.lvl);
                        addMsg(&chunkmsgs[p], msg.x, msg.z - 1, msg.id, true, msg.lvl);
                        addMsg(&chunkmsgs[p], msg.x + 1, msg.z, msg.id, true, msg.lvl);
                        addMsg(&chunkmsgs[p], msg.x - 1, msg.z, msg.id, true, msg.lvl);
                        if (msg.lvl >= 2) {
                            addMsg(&chunkmsgs[p], msg.x + 1, msg.z + 1, msg.id, true, msg.lvl);
                            addMsg(&chunkmsgs[p], msg.x + 1, msg.z - 1, msg.id, true, msg.lvl);
                            addMsg(&chunkmsgs[p], msg.x - 1, msg.z + 1, msg.id, true, msg.lvl);
                            addMsg(&chunkmsgs[p], msg.x - 1, msg.z - 1, msg.id, true, msg.lvl);
                        }
                    }
                } else {
                    if (msg.lvl >= 1) {
                        mesh(msg.x, msg.z + 1, msg.id);
                        mesh(msg.x, msg.z - 1, msg.id);
                        mesh(msg.x + 1, msg.z, msg.id);
                        mesh(msg.x - 1, msg.z, msg.id);
                        if (msg.lvl >= 2) {
                            mesh(msg.x + 1, msg.z + 1, msg.id);
                            mesh(msg.x + 1, msg.z - 1, msg.id);
                            mesh(msg.x - 1, msg.z + 1, msg.id);
                            mesh(msg.x - 1, msg.z - 1, msg.id);
                        }
                    }
                }
            }
            //printf("x: [%"PRId64"], z: [%"PRId64"], p: [%d]\n", msg.x, msg.z, p);
            pthread_mutex_lock(&rendinf.chunks->lock);
            setready(msg.x, msg.z, true);
            if (!msg.dep && !(lazymesh || p != CHUNKUPDATE_PRIO_HIGH)) {
                if (msg.lvl >= 1) {
                    setready(msg.x, msg.z + 1, true);
                    setready(msg.x, msg.z - 1, true);
                    setready(msg.x + 1, msg.z, true);
                    setready(msg.x - 1, msg.z, true);
                    if (msg.lvl >= 2) {
                        setready(msg.x + 1, msg.z + 1, true);
                        setready(msg.x + 1, msg.z - 1, true);
                        setready(msg.x - 1, msg.z + 1, true);
                        setready(msg.x - 1, msg.z - 1, true);
                    }
                }
            }
            pthread_mutex_unlock(&rendinf.chunks->lock);
            microwait(0);
            /*
            double time = (altutime() - stime) / 1000.0;
            printf("meshed: [%"PRId64", %"PRId64"] in [%lgms]\n", msg.x, msg.z, time);
            */
        }
        if (activity) {
            acttime = altutime();
            //microwait(250);
            microwait(0);
        } else if (altutime() - acttime > 200000) {
            microwait(1000);
        } else {
            microwait(0);
        }
    }
    return NULL;
}

static bool opaqueUpdate = false;

void updateChunks() {
    pthread_mutex_lock(&rendinf.chunks->lock);
    for (uint32_t c = 0; !quitRequest && c < rendinf.chunks->info.widthsq; ++c) {
        if (!rendinf.chunks->renddata[c].ready || !rendinf.chunks->renddata[c].visfull) continue;
        if (!rendinf.chunks->renddata[c].init) {
            glGenBuffers(2, rendinf.chunks->renddata[c].VBO);
            rendinf.chunks->renddata[c].init = true;
        }
        if (rendinf.chunks->renddata[c].remesh[0]) {
            uint32_t tmpsize = rendinf.chunks->renddata[c].vcount[0] * sizeof(uint32_t);
            glBindBuffer(GL_ARRAY_BUFFER, rendinf.chunks->renddata[c].VBO[0]);
            glBufferData(GL_ARRAY_BUFFER, tmpsize, rendinf.chunks->renddata[c].vertices[0], GL_DYNAMIC_DRAW);
            rendinf.chunks->renddata[c].qcount[0] = rendinf.chunks->renddata[c].vcount[0] / 11;
            for (int i = 0; i < 32; ++i) {
                rendinf.chunks->renddata[c].yqoff[i] = rendinf.chunks->renddata[c].yvoff[i] / 11;
                rendinf.chunks->renddata[c].yqcount[i] = rendinf.chunks->renddata[c].yvcount[i] / 11;
            }
            free(rendinf.chunks->renddata[c].vertices[0]);
            rendinf.chunks->renddata[c].vertices[0] = NULL;
            rendinf.chunks->renddata[c].remesh[0] = false;
            opaqueUpdate = true;
            //printf("O: [%d]\n", c);
        }
        if (rendinf.chunks->renddata[c].remesh[1]) {
            uint32_t tmpsize = rendinf.chunks->renddata[c].vcount[1] * sizeof(uint32_t);
            glBindBuffer(GL_ARRAY_BUFFER, rendinf.chunks->renddata[c].VBO[1]);
            glBufferData(GL_ARRAY_BUFFER, tmpsize, rendinf.chunks->renddata[c].vertices[1], GL_DYNAMIC_DRAW);
            rendinf.chunks->renddata[c].qcount[1] = rendinf.chunks->renddata[c].vcount[1] / 11;
            rendinf.chunks->renddata[c].sortvert = realloc(rendinf.chunks->renddata[c].sortvert, tmpsize);
            memcpy(rendinf.chunks->renddata[c].sortvert, rendinf.chunks->renddata[c].vertices[1], tmpsize);
            rendinf.chunks->renddata[c].remesh[1] = false;
            if (tmpsize) {
                _sortChunk(
                    c,
                    (int)(c % rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist,
                    -((int)(c / rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist),
                    true
                );
            }
            //printf("T: [%d]\n", c);
        }
        rendinf.chunks->renddata[c].ready = false;
        rendinf.chunks->renddata[c].buffered = true;
    }
    pthread_mutex_unlock(&rendinf.chunks->lock);
}

void startMesher() {
    if (!mesheractive) {
        mesheractive = true;
        for (int i = 0; i < CHUNKUPDATE_PRIO__MAX; ++i) {
            initMsgData(&chunkmsgs[i]);
        }
        chunkmsgs[CHUNKUPDATE_PRIO_LOW].async = true;
        //chunkmsgs[CHUNKUPDATE_PRIO_NORMAL].async = true;
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
            sprintf(name, "%.8s:msh%d", name2, i);
            pthread_setname_np(pthreads[i], name);
            #endif
        }
    }
}

void stopMesher() {
    if (mesheractive) {
        mesheractive = false;
        for (int i = 0; i < MESHER_THREADS && i < MESHER_THREADS_MAX; ++i) {
            pthread_join(pthreads[i], NULL);
        }
        for (int i = 0; i < CHUNKUPDATE_PRIO__MAX; ++i) {
            deinitMsgData(&chunkmsgs[i]);
        }
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

static force_inline bool renderUI(struct ui_layer* data) {
    /*
    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 4 * sizeof(uint32_t), (void*)(0));
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(uint32_t), (void*)(sizeof(uint32_t)));
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 4 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 2));
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, 4 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 3));
    */
    return false;
}

const unsigned char* glver;
const unsigned char* glslver;
const unsigned char* glvend;
const unsigned char* glrend;

static int chwidth;
static int chheight;

struct pq {
    int8_t x;
    int8_t y;
    int8_t z;
    int8_t from;
    int8_t dir[6];
};

static struct pq* posqueue;
static int pqptr = 0;
static force_inline void pqpush(int8_t _x, int8_t _y, int8_t _z, int8_t _f) {
    posqueue[pqptr].x = (_x);
    posqueue[pqptr].y = (_y);
    posqueue[pqptr].z = (_z);
    posqueue[pqptr].from = (_f);
    ++pqptr;
}
static force_inline void pqpop() {
    --pqptr;
}
uint8_t* visited;

//char* dirstr[] = {"UP", "RIGHT", "FRONT", "BACK", "LEFT", "DOWN"};

static force_inline bool pqvisit(struct pq* p, int x, int y, int z, int face) {
    //printf("visit: [%d, %d, %d]: [%d]\n", x, y, z, pqptr);
    if (p->dir[5 - face]) return false;
    if (x < 0 || z < 0 || x >= (int)rendinf.chunks->info.width || z >= (int)rendinf.chunks->info.width) return false;
    if (y < -1 || y > 32) return false;
    int c = x + z * rendinf.chunks->info.width;
    int v = c + y * rendinf.chunks->info.widthsq;
    if (visited[v]) return false;
    if (p->y >= 0 && p->y < 32 && p->from > -1) {
        int c2 = p->x + p->z * rendinf.chunks->info.width;
        if (!rendinf.chunks->renddata[c2].vispass[p->y][p->from][face]) {
            /*
            printf("[%d, %d, %d] -> [%d, %d, %d]: Cannot go from [%s] to [%s]\n", p->x, p->y, p->z, x, y, z, dirstr[p->from], dirstr[face]);
            {
                printf("VISPASS [%d, %d, %d]:\n", p->x, p->y, p->z);
                for (int i = 0; i < 6; ++i) {
                    for (int j = 0; j < 6; ++j) {
                        printf("%d", rendinf.chunks->renddata[c2].vispass[p->y][i][j]);
                    }
                    putchar('\n');
                }
            }
            */
            return false;
        }
        //int nx = x - rendinf.chunks->info.dist;
        //int nz = z - rendinf.chunks->info.dist;
        //if (!isVisible(&frust, nx * 16 - 8, y * 16, nz * 16 - 8, nx * 16 + 8, (y + 1) * 16, nz * 16 + 8)) return false;
    }
    visited[v] = true;
    memcpy(posqueue[pqptr].dir, p->dir, 6);
    posqueue[pqptr].dir[face] = true;
    pqpush(x, y, z, 5 - face);
    //printf("OK: [%d, %d, %d]\n", x, y, z);
    return true;
}

static inline void setvis(int8_t _x, int8_t _y, int8_t _z) {
    if ((_x) >= 0 && (_y) >= 0 && (_z) >= 0 && (_x) < (int)rendinf.chunks->info.width && (_y) < 32 && (_z) < (int)rendinf.chunks->info.width) {
        rendinf.chunks->renddata[(_x) + (_z) * rendinf.chunks->info.width].visible |= (1 << (_y));
    }
}
static inline void fpush(int8_t x, int8_t y, int8_t z, bool vis) {
    if (y < -1 || y > 32) return;
    x += rendinf.chunks->info.dist;
    z += rendinf.chunks->info.dist;
    pqpush(x, y, z, -1);
    int v = x + z * rendinf.chunks->info.width + y * rendinf.chunks->info.widthsq;
    visited[v] = vis;
}

static int cavecull;
bool rendergame = false;

static int dbgtextuih = -1;
void render() {
    //printf("rendergame: [%d]\n", rendergame);
    if (showDebugInfo) {
        static char tbuf[1][32768];
        static int toff = 0;
        if (game_ui[UILAYER_DBGINF] && dbgtextuih == -1) {
            dbgtextuih = newUIElem(
                game_ui[UILAYER_DBGINF], UI_ELEM_CONTAINER, -1,
                UI_ATTR_NAME, "debugText",
                UI_ATTR_SIZE, "100%", "100%",
                UI_ATTR_TEXTALIGN, -1, -1, UI_ATTR_TEXTCOLOR, 14, -1, UI_ATTR_TEXTALPHA, -1, 127,
                UI_ATTR_TEXT, "",
                UI_END
            );
        }
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
        if (rendergame) {
            sprintf(
                &tbuf[0][toff],
                "Resolution: %ux%u@%u vsync %s\n"
                "FPS: %.2lf (%.2lf)\n"
                "Position: (%lf, %lf, %lf)\n"
                "Velocity: (%f, %f, %f)\n"
                "Rotation: (%f, %f, %f)\n"
                "Block: (%d, %d, %d)\n"
                "Chunk: (%"PRId64", %"PRId64")",
                rendinf.width, rendinf.height, rendinf.fps, (rendinf.vsync) ? "on" : "off",
                fps, realfps,
                pcoord.x, pcoord.y, pcoord.z,
                pvelocity.x, pvelocity.y, pvelocity.z,
                rendinf.camrot.x, rendinf.camrot.y, rendinf.camrot.z,
                pblockx, pblocky, pblockz,
                rendinf.chunks->xoff, rendinf.chunks->zoff
            );
        } else {
            sprintf(
                &tbuf[0][toff],
                "Resolution: %ux%u@%u vsync %s\n"
                "FPS: %.2lf (%.2lf)",
                rendinf.width, rendinf.height, rendinf.fps, (rendinf.vsync) ? "on" : "off",
                fps, realfps
            );
        }
        if (game_ui[UILAYER_DBGINF]) editUIElem(game_ui[UILAYER_DBGINF], dbgtextuih, UI_ATTR_TEXT, tbuf, UI_END);
    }

    if (rendergame) {
        setShaderProg(shader_block);
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        glClearColor(sky.r, sky.g, sky.b, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_BLEND);
        static uint64_t aMStart = 0;
        if (!aMStart) aMStart = altutime();
        glUniform1ui(glGetUniformLocation(rendinf.shaderprog, "aniMult"), (uint64_t)(altutime() - aMStart) / 10000);
        glUniform1i(glGetUniformLocation(rendinf.shaderprog, "dist"), rendinf.chunks->info.dist);
        setUniform3f(rendinf.shaderprog, "cam", (float[]){rendinf.campos.x, rendinf.campos.y, -rendinf.campos.z});

        pthread_mutex_lock(&rendinf.chunks->lock);

        _sortChunk(-1, 0, 0, true);

        _sortChunk(-1, 1, 0, true);
        _sortChunk(-1, -1, 0, true);
        _sortChunk(-1, 0, 1, true);
        _sortChunk(-1, 0, -1, true);

        _sortChunk(-1, 1, 1, true);
        _sortChunk(-1, -1, 1, true);
        _sortChunk(-1, 1, -1, true);
        _sortChunk(-1, -1, -1, true);

        static int64_t cx = 0;
        static int cy = -INT_MAX;
        static int64_t cz = 0;
        int64_t newcx = rendinf.chunks->xoff;
        int newcy = (int)((rendinf.campos.y < 0) ? rendinf.campos.y - 16 : rendinf.campos.y) / 16;
        int64_t newcz = rendinf.chunks->zoff;
        if ((opaqueUpdate || newcz != cz || newcx != cx || newcy != cy) && !debug_nocavecull) {
            //if (newcz != cz || newcx != cx || newcy != cy) printf("MOVED: [%"PRId64", %d, %"PRId64"]\n", cx, cy, cz);

            //int64_t cx = rendinf.chunks->xoff;
            //int cy = (int)((rendinf.campos.y < 0) ? rendinf.campos.y - 16 : rendinf.campos.y) / 16;
            //int64_t cz = rendinf.chunks->zoff;
            cx = newcx;
            cy = newcy;
            cz = newcz;

            //puts("opaqueUpdate");
            opaqueUpdate = false;

            int w = rendinf.chunks->info.dist;
            if (w < 2) w = 2;
            w = 1 + w * 2;
            w *= w;
            posqueue = malloc((w * 6) * sizeof(*posqueue));
            pqptr = 0;
            visited = calloc(rendinf.chunks->info.widthsq * 34, 1);
            visited += rendinf.chunks->info.widthsq;

            for (int i = 0; i < (int)rendinf.chunks->info.widthsq; ++i) {
                rendinf.chunks->renddata[i].visible = 0;
            }

            {
                memset(posqueue[pqptr].dir, 0, 6);
                int ncy = cy;
                if (ncy < -1) ncy = -1;
                else if (ncy > 32) ncy = 32;
                //printf("ncy: [%d]\n", ncy);
                /*
                fpush(0, ncy + 1, 0, false);
                fpush(1, ncy, 0, false);
                fpush(0, ncy, 1, false);
                fpush(0, ncy - 1, 0, false);
                fpush(-1, ncy, 0, false);
                fpush(0, ncy, -1, false);
                */
                fpush(0, ncy, 0, true);
            }
            int pqptrmax = 0;
            while (pqptr > 0) {
                if (pqptr > pqptrmax) pqptrmax = pqptr;
                pqpop();
                struct pq p = posqueue[pqptr];

                pqvisit(&p, p.x + 1, p.y, p.z, CVIS_RIGHT);
                pqvisit(&p, p.x - 1, p.y, p.z, CVIS_LEFT);
                pqvisit(&p, p.x, p.y, p.z - 1, CVIS_FRONT);
                pqvisit(&p, p.x, p.y, p.z + 1, CVIS_BACK);
                pqvisit(&p, p.x, p.y - 1, p.z, CVIS_DOWN);
                pqvisit(&p, p.x, p.y + 1, p.z, CVIS_UP);
            }

            for (int z = 0; z < (int)rendinf.chunks->info.width; ++z) {
                for (int x = 0; x < (int)rendinf.chunks->info.width; ++x) {
                    for (int y = 0; y < 32; ++y) {
                        if (visited[x + z * rendinf.chunks->info.width + y * rendinf.chunks->info.widthsq]) {
                            setvis(x, y, z);
                            if (cavecull >= 1) {
                                setvis(x + 1, y, z);
                                setvis(x - 1, y, z);
                                setvis(x, y + 1, z);
                                setvis(x, y - 1, z);
                                setvis(x, y, z + 1);
                                setvis(x, y, z - 1);
                                if (cavecull >= 2) {
                                    setvis(x + 1, y + 1, z + 1);
                                    setvis(x - 1, y + 1, z + 1);
                                    setvis(x + 1, y - 1, z + 1);
                                    setvis(x - 1, y - 1, z + 1);
                                    setvis(x + 1, y + 1, z - 1);
                                    setvis(x - 1, y + 1, z - 1);
                                    setvis(x + 1, y - 1, z - 1);
                                    setvis(x - 1, y - 1, z - 1);
                                }
                            }
                        }
                    }
                }
            }

            free(posqueue);
            visited -= rendinf.chunks->info.widthsq;
            free(visited);
        }

        pthread_mutex_unlock(&rendinf.chunks->lock);

        int32_t rendc = 0;
        avec2 coord;
        avec2 corner1;
        avec2 corner2;
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        if (debug_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glVertexAttribDivisor(0, 1);
        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        for (int32_t c = rendinf.chunks->info.widthsq - 1; c >= 0; --c) {
            rendc = rendinf.chunks->rordr[c].c;
            coord[0] = (int)(rendc % rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist;
            coord[1] = (int)(rendc / rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist;
            corner1[0] = coord[0] * 16.0 - 8.0;
            corner1[1] = coord[1] * 16.0 + 8.0;
            corner2[0] = coord[0] * 16.0 + 8.0;
            corner2[1] = coord[1] * 16.0 - 8.0;
            if ((rendinf.chunks->renddata[rendc].visfull = isVisible(&frust, corner1[0], 512.0, corner1[1], corner2[0], 0.0, corner2[1]))) {
                if (rendinf.chunks->renddata[rendc].buffered) {
                    if (rendinf.chunks->renddata[rendc].qcount[0]) {
                        coord[0] = (int)(rendc % rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist;
                        coord[1] = (int)(rendc / rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist;
                        setUniform2f(rendinf.shaderprog, "ccoord", coord);
                        glBindBuffer(GL_ARRAY_BUFFER, rendinf.chunks->renddata[rendc].VBO[0]);
                        for (int y = 31; y >= 0; --y) {
                            if ((!debug_nocavecull && !(rendinf.chunks->renddata[rendc].visible & (1 << y))) || !rendinf.chunks->renddata[rendc].yqcount[y]) continue;
                            if (isVisible(&frust, corner1[0], y * 16.0, corner1[1], corner2[0], (y + 1) * 16.0, corner2[1])) {
                                //printf("REND OPAQUE: [%d]:[%d]\n", rendc, y);
                                int off = rendinf.chunks->renddata[rendc].yqoff[y] * 11;
                                glVertexAttribIPointer(0, 4, GL_UNSIGNED_INT, 11 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * off));
                                glVertexAttribIPointer(1, 4, GL_UNSIGNED_INT, 11 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * (4 + off)));
                                glVertexAttribIPointer(2, 3, GL_UNSIGNED_INT, 11 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * (8 + off)));
                                glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, rendinf.chunks->renddata[rendc].yqcount[y]);
                            }
                        }
                    }
                }
            }
        }
        glEnable(GL_BLEND);
        glDepthMask(false);
        for (int32_t c = 0; c < (int)rendinf.chunks->info.widthsq; ++c) {
            rendc = rendinf.chunks->rordr[c].c;
            if (rendinf.chunks->renddata[rendc].visfull && rendinf.chunks->renddata[rendc].buffered) {
                if (rendinf.chunks->renddata[rendc].qcount[1]) {
                    coord[0] = (int)(rendc % rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist;
                    coord[1] = (int)(rendc / rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist;
                    setUniform2f(rendinf.shaderprog, "ccoord", coord);
                    glBindBuffer(GL_ARRAY_BUFFER, rendinf.chunks->renddata[rendc].VBO[1]);
                    glVertexAttribIPointer(0, 4, GL_UNSIGNED_INT, 11 * sizeof(uint32_t), (void*)(0));
                    glVertexAttribIPointer(1, 4, GL_UNSIGNED_INT, 11 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 4));
                    glVertexAttribIPointer(2, 3, GL_UNSIGNED_INT, 11 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 8));
                    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, rendinf.chunks->renddata[rendc].qcount[1]);
                }
            }
        }
        glDepthMask(true);
        glVertexAttribDivisor(0, 0);
        glVertexAttribDivisor(1, 0);
        glVertexAttribDivisor(2, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        if (debug_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glDisable(GL_CULL_FACE);
    if (rendergame) {
        glDisable(GL_DEPTH_TEST);
        setShaderProg(shader_2d);
        glBindBuffer(GL_ARRAY_BUFFER, VBO2D);
        glUniform1f(glGetUniformLocation(rendinf.shaderprog, "xratio"), (float)chwidth / (float)rendinf.width);
        glUniform1f(glGetUniformLocation(rendinf.shaderprog, "yratio"), (float)chheight / (float)rendinf.height);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    setShaderProg(shader_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "fbtype"), 0);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "texData"), FBTEXID - GL_TEXTURE0);
    setUniform3f(rendinf.shaderprog, "mcolor", (float[]){screenmult.r, screenmult.g, screenmult.b});
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "fbtype"), 1);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "texData"), UIFBTEXID - GL_TEXTURE0);
    setUniform3f(rendinf.shaderprog, "mcolor", (float[]){1, 1, 1});
    if (!rendergame) {
        //TODO: gmod-like fading background of screenshots
        uint64_t time = altutime() / 1000;
        glClearColor(
            (sin(time / 1735.0) * 0.5 + 0.5) * 0.1,
            (cos(time / 3164.0) * 0.5 + 0.5) * 0.2,
            (sin(time / 5526.0) * 0.5 + 0.5) * 0.3,
            1.0
        );
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    glClearColor(0.0, 0.0, 0.0, 0.0);
    for (int i = 0; i < 4; ++i) {
        setShaderProg(shader_ui);
        glBindFramebuffer(GL_FRAMEBUFFER, UIFBO);
        if (renderUI(game_ui[i])) {
            //printf("renderUI(game_ui[%d])\n", i);
            setShaderProg(shader_framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindBuffer(GL_ARRAY_BUFFER, VBO2D);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

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

#ifndef USEGLES
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
        case GL_DEBUG_SEVERITY_MEDIUM:;
            ignore = false;
            sevstr = "GL_DEBUG_SEVERITY_MEDIUM";
            break;
        #if DBGLVL(1)
        case GL_DEBUG_SEVERITY_LOW:;
            ignore = false;
            sevstr = "GL_DEBUG_SEVERITY_LOW";
            break;
        #endif
        #if DBGLVL(2)
        case GL_DEBUG_SEVERITY_NOTIFICATION:;
            ignore = false;
            sevstr = "GL_DEBUG_SEVERITY_NOTIFICATION";
            break;
        #endif
    }
    if (ignore) return;
    fprintf(stderr, "OpenGL debug [%s]: [%d] {%s}\n", sevstr, id, msg);
}
#endif

bool initRenderer() {
    declareConfigKey(config, "Renderer", "compositing", "true", false);
    declareConfigKey(config, "Renderer", "resolution", "1024x768", false);
    declareConfigKey(config, "Renderer", "fullScreenRes", "", false);
    declareConfigKey(config, "Renderer", "vSync", "true", false);
    #ifndef __ANDROID__
        declareConfigKey(config, "Renderer", "fullScreen", "false", false);
    #else
        declareConfigKey(config, "Renderer", "fullScreen", "true", false);
    #endif
    declareConfigKey(config, "Renderer", "FOV", "85", false);
    declareConfigKey(config, "Renderer", "mipmaps", "true", false);
    declareConfigKey(config, "Renderer", "clientLighting", "false", false);
    declareConfigKey(config, "Renderer", "nearPlane", "0.05", false);
    declareConfigKey(config, "Renderer", "farPlane", "2500", false);
    declareConfigKey(config, "Renderer", "lazyMesher", "false", false);
    declareConfigKey(config, "Renderer", "sortTransparent", "true", false);
    declareConfigKey(config, "Renderer", "caveCullLevel", "1", false);
    declareConfigKey(config, "Renderer", "fancyFog", "true", false);
    declareConfigKey(config, "Renderer", "mesherThreadsMax", "1", false);

    rendinf.campos = GFX_DEFAULT_POS;
    rendinf.camrot = GFX_DEFAULT_ROT;

    cavecull = atoi(getConfigKey(config, "Renderer", "caveCullLevel"));
    clientlighting = getBool(getConfigKey(config, "Renderer", "clientLighting"));
    MESHER_THREADS_MAX = atoi(getConfigKey(config, "Renderer", "mesherThreadsMax"));

    #if defined(USESDL2)
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    if (SDL_Init(SDL_INIT_VIDEO)) {
        sdlerror("initRenderer: Failed to init video");
        return false;
    }
    #else
    glfwSetErrorCallback(errorcb);
    if (!glfwInit()) return false;
    #endif

    #if defined(USESDL2)
    #ifndef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    #endif
    #if defined(USEGLES)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    #else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    #endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    #ifndef __EMSCRIPTEN__
    if (getBool(getConfigKey(config, "Renderer", "compositing"))) SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    #endif
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
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    //glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    #endif

    #if defined(USESDL2)
    if (!(rendinf.window = SDL_CreateWindow(PROG_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_OPENGL))) {
        sdlerror("initRenderer: Failed to create window");
        return false;
    }
    if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(rendinf.window), &rendinf.monitor) < 0) {
        sdlerror("initRenderer: Failed to fetch display info");
        return false;
    }
    rendinf.disp_width = rendinf.monitor.w;
    rendinf.disp_height = rendinf.monitor.h;
    rendinf.disphz = rendinf.monitor.refresh_rate;
    SDL_DestroyWindow(rendinf.window);
    #else
    if (!(rendinf.monitor = glfwGetPrimaryMonitor())) {
        fputs("initRenderer: Failed to fetch primary monitor handle\n", stderr);
        return false;
    }
    const GLFWvidmode* vmode = glfwGetVideoMode(rendinf.monitor);
    rendinf.disp_width = vmode->width;
    rendinf.disp_height = vmode->height;
    rendinf.disphz = vmode->refreshRate;
    #endif
    if (rendinf.disphz <= 0) rendinf.disphz = 60;
    rendinf.win_fps = rendinf.disphz;

    sscanf(getConfigKey(config, "Renderer", "resolution"), "%ux%u@%f",
        &rendinf.win_width, &rendinf.win_height, &rendinf.win_fps);
    if (!rendinf.win_width || rendinf.win_width > 32767) rendinf.win_width = 1024;
    if (!rendinf.win_height || rendinf.win_height > 32767) rendinf.win_height = 768;
    rendinf.full_fps = rendinf.win_fps;
    sscanf(getConfigKey(config, "Renderer", "fullScreenRes"), "%ux%u@%f",
        &rendinf.full_width, &rendinf.full_height, &rendinf.full_fps);
    if (!rendinf.full_width || rendinf.full_width > 32767) rendinf.full_width = rendinf.disp_width;
    if (!rendinf.full_height || rendinf.full_height > 32767) rendinf.full_height = rendinf.disp_height;
    rendinf.vsync = getBool(getConfigKey(config, "Renderer", "vSync"));
    rendinf.fullscr = getBool(getConfigKey(config, "Renderer", "fullScreen"));
    rendinf.camfov = atof(getConfigKey(config, "Renderer", "FOV"));
    rendinf.camnear = atof(getConfigKey(config, "Renderer", "nearPlane"));
    rendinf.camfar = atof(getConfigKey(config, "Renderer", "farPlane"));
    lazymesh = getBool(getConfigKey(config, "Renderer", "lazyMesher"));
    return true;
}

bool makeShader(file_data* hdr, char* name, char* dirname, GLuint* shader) {
    if (!dirname) dirname = name;
    if (*shader) glDeleteProgram(*shader);
    printf("Compiling %s shader...\n", name);
    static char tmpbuf[MAX_PATH];
    snprintf(tmpbuf, MAX_PATH, "engine/shaders/GLSL/vertex/%s.glsl", dirname);
    file_data* vs = loadResource(RESOURCE_TEXTFILE, tmpbuf);
    snprintf(tmpbuf, MAX_PATH, "engine/shaders/GLSL/fragment/%s.glsl", dirname);
    file_data* fs = loadResource(RESOURCE_TEXTFILE, tmpbuf);
    if (!vs || !fs || !makeShaderProg((char*)hdr->data, (char*)vs->data, (char*)fs->data, shader)) {
        putchar('\n');
        fprintf(stderr, "makeShader: Failed to compile %s shader\n", name);
        return false;
    }
    freeResource(vs);
    freeResource(fs);
    return true;
}

struct atlas_box {
    int x;
    int y;
    int width;
    int height;
    resdata_image* img;
};

struct atlas {
    int size;
    unsigned char* data;
    int boxes;
    struct atlas_box* boxdata;
};

static int atlas_addBox(struct atlas* a, int x, int y, int width, int height) {
    if (!width || !height) return -1;
    int box = a->boxes++;
    a->boxdata = realloc(a->boxdata, a->boxes * sizeof(*a->boxdata));
    struct atlas_box* b = &a->boxdata[box];
    b->x = x;
    b->y = y;
    b->width = width;
    b->height = height;
    b->img = NULL;
    return box;
}

static int addToAtlas(struct atlas* a, char* path) {
    resdata_image* img = loadResource(RESOURCE_IMAGE, path);
    if (!img) return -1;
    int index = -1;
    for (int i = 0; i < a->boxes; ++i) {
        struct atlas_box* b = &a->boxdata[i];
        if (!b->img && img->width <= b->width && img->height <= b->height) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        int size = a->size;
        int newsize = size;
        int sizediff;
        while (1) {
            newsize *= 2;
            sizediff = newsize - size;
            if (img->width <= newsize && img->height <= sizediff) break;
        }
        a->size = newsize;
        atlas_addBox(a, size, 0, sizediff, size);
        index = atlas_addBox(a, 0, size, newsize, sizediff);
    }
    struct atlas_box* b = &a->boxdata[index];
    b->img = img;
    int oldwidth = b->width;
    int oldheight = b->height;
    b->width = img->width;
    b->height = img->height;
    atlas_addBox(a, b->x + b->width, b->y, oldwidth - b->width, b->height);
    b = &a->boxdata[index];
    atlas_addBox(a, b->x, b->y + b->height, oldwidth, oldheight - b->height);
    return index;
}

static void atlas_writeTexture(struct atlas* a, int bx, int by, int width, int height, unsigned char* data) {
    unsigned char* atex = a->data;
    int aw = a->size;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int i = ((x + bx) + (y + by) * aw) * 4;
            int texi = (x + y * width) * 4;
            atex[i] = data[texi];
            atex[i + 1] = data[texi + 1];
            atex[i + 2] = data[texi + 2];
            atex[i + 3] = data[texi + 3];
        }
    }
}

static void makeAtlas(struct atlas* a) {
    a->data = malloc(a->size * a->size * 4);
    //printf("makeAtlas: size=%d\n", a->size);
    for (int i = 0; i < a->boxes; ++i) {
        struct atlas_box* b = &a->boxdata[i];
        resdata_image* img = b->img;
        if (img) {
            atlas_writeTexture(a, b->x, b->y, img->width, img->height, img->data);
            freeResource(img);
        }
    }
}

static void initAtlas(struct atlas* a, int size) {
    if (size <= 0) size = 16;
    a->size = size;
    a->data = NULL;
    a->boxes = 0;
    a->boxdata = NULL;
    atlas_addBox(a, 0, 0, size, size);
}

static void deinitAtlas(struct atlas* a) {
    free(a->data);
    free(a->boxdata);
}

static unsigned texarrayh;
static unsigned crosshairh;
static unsigned charseth;

bool reloadRenderer() {
    bool sorttransparent = getBool(getConfigKey(config, "Renderer", "sortTransparent"));
    bool fancyfog = getBool(getConfigKey(config, "Renderer", "fancyFog"));
    #if defined(USEGLES)
    char* hdrpath = "engine/shaders/headers/OpenGL ES.glsl";
    #else
    char* hdrpath = "engine/shaders/headers/OpenGL.glsl";
    #endif
    file_data* _hdr = loadResource(RESOURCE_TEXTFILE, hdrpath);
    if (!_hdr) {
        fputs("reloadRenderer: Failed to load shader header\n", stderr);
        return false;
    }
    file_data hdr;
    hdr.size = _hdr->size;
    hdr.data = (unsigned char*)strdup((char*)_hdr->data);
    freeResource(_hdr);
    #if defined(USEGLES)
    addTextToFile(&hdr, "#define USEGLES\n");
    #endif
    if (sorttransparent) {
        addTextToFile(&hdr, "#define OPT_SORTTRANSPARENT\n");
    }
    if (fancyfog) {
        addTextToFile(&hdr, "#define OPT_FANCYFOG\n");
    }
    addTextToFile(&hdr, "#line 1\n");

    if (!makeShader(&hdr, "block", NULL, &shader_block)) return false;
    if (!makeShader(&hdr, "2D", "2d", &shader_2d)) return false;
    if (!makeShader(&hdr, "UI", "ui", &shader_ui)) return false;
    if (!makeShader(&hdr, "text", NULL, &shader_text)) return false;
    if (!makeShader(&hdr, "framebuffer", NULL, &shader_framebuffer)) return false;

    free(hdr.data);

    int gltex = GL_TEXTURE0;

    puts("Creating UI framebuffer...");
    glBindRenderbuffer(GL_RENDERBUFFER, UIDBUF);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, rendinf.width, rendinf.height);
    UIFBTEXID = gltex++;
    glActiveTexture(UIFBTEXID);
    glBindFramebuffer(GL_FRAMEBUFFER, UIFBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, UIDBUF);
    glBindTexture(GL_TEXTURE_2D, UIFBTEX);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rendinf.width, rendinf.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, UIFBTEX, 0);

    puts("Creating game framebuffer...");
    glBindRenderbuffer(GL_RENDERBUFFER, DBUF);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, rendinf.width, rendinf.height);
    FBTEXID = gltex++;
    glActiveTexture(FBTEXID);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, DBUF);
    glBindTexture(GL_TEXTURE_2D, FBTEX);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rendinf.width, rendinf.height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FBTEX, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    setShaderProg(shader_framebuffer);
    setUniform3f(rendinf.shaderprog, "mcolor", (float[]){1.0, 1.0, 1.0});

    glActiveTexture(gltex++);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rendinf.width, rendinf.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //TODO: load and render splash
    swapBuffers();

    glActiveTexture(gltex++);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rendinf.width, rendinf.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    unsigned char* texarray;

    //puts("creating texture map...");
    int texarraysize = 0;
    texarray = malloc(texarraysize * 1024);
    char* tmpbuf = malloc(4096);
    blockinf[0].data[0].transparency = 1;
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
                if (j == 0) blockinf[i].data[s].texstart = texarraysize;
                ++blockinf[i].data[s].texcount;
                resdata_image* img = loadResource(RESOURCE_IMAGE, tmpbuf);
                for (int k = 3; k < 1024; k += 4) {
                    if (img->data[k] < 255) {
                        if (sorttransparent || img->data[k]) {
                            blockinf[i].data[s].transparency = 2;
                            break;
                        } else {
                            blockinf[i].data[s].transparency = 1;
                        }
                    }
                }
                //printf("adding texture {%s} at offset [%u] of map [%d]...\n", tmpbuf, texarraysize * 1024, i);
                texarray = realloc(texarray, (texarraysize + 1) * 1024);
                memcpy(texarray + texarraysize * 1024, img->data, 1024);
                ++texarraysize;
                freeResource(img);
            }
        }
    }
    int texlayers = 0;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &texlayers);
    printf("Block text layer: %d/%d (%.2f%%)\n", texarraysize, texlayers, ((float)texarraysize / (float)texlayers) * 100.0);
    free(tmpbuf);
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
                        } break;
                        case '$':; {
                            int stroff = 1;
                            stroff += readStrUntil(&texstr[1], '.', tmpstr) + 1;
                            int vtexid = blockSubNoFromID(i, tmpstr);
                            if (vtexid >= 0) texture = blockinf[i].data[vtexid].texstart;
                            texture += atoi(&texstr[stroff]);
                        } break;
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
                        } break;
                    }
                    blockinf[i].data[j].texoff[k] = texture;
                }
            }
        }
    }

    bool mipmaps = getBool(getConfigKey(config, "Renderer", "mipmaps"));

    setShaderProg(shader_block);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "texData"), gltex - GL_TEXTURE0);
    glActiveTexture(gltex++);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texarrayh);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 16, 16, texarraysize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texarray);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "mipmap"), mipmaps);
    if (mipmaps) {
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    } else {
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    free(texarray);

    setShaderProg(shader_2d);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "texData"), gltex - GL_TEXTURE0);
    glActiveTexture(gltex++);
    resdata_image* crosshair = loadResource(RESOURCE_IMAGE, "game/textures/ui/crosshair.png");
    chwidth = crosshair->width;
    chheight = crosshair->height;
    glBindTexture(GL_TEXTURE_2D, crosshairh);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, crosshair->width, crosshair->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, crosshair->data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    setUniform4f(rendinf.shaderprog, "mcolor", (float[]){1.0, 1.0, 1.0, 1.0});
    freeResource(crosshair);

    setShaderProg(shader_ui);
    syncTextColors();
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "fontTexData"), gltex - GL_TEXTURE0);
    setUniform1f(rendinf.shaderprog, "xsize", rendinf.width);
    setUniform1f(rendinf.shaderprog, "ysize", rendinf.height);
    glActiveTexture(gltex++);
    resdata_image* charset = loadResource(RESOURCE_IMAGE, "game/textures/ui/charset.png");
    glBindTexture(GL_TEXTURE_2D_ARRAY, charseth);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 8, 16, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, charset->data);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    {
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        #ifndef USEGLES
        float bcolor[] = {0.0, 0.0, 0.0, 0.0};
        glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, bcolor);
        #endif
    }
    freeResource(charset);

    setSkyColor(sky.r, sky.g, sky.b);
    setNatColor(nat.r, nat.g, nat.b);
    setScreenMult(screenmult.r, screenmult.g, screenmult.b);
    setVisibility(fognear, fogfar);

    opaqueUpdate = true;
    uc_uproj = true;
    updateCam();
    setFullscreen(rendinf.fullscr);
    if (rendinf.chunks) {
        int64_t xo = rendinf.chunks->xoff;
        int64_t zo = rendinf.chunks->zoff;
        updateChunk(xo, zo, CHUNKUPDATE_PRIO_HIGH, 0);
        for (int i = 1; i <= (int)rendinf.chunks->info.dist; ++i) {
            updateChunk(xo + i, zo, CHUNKUPDATE_PRIO_HIGH, 0);
            updateChunk(xo - i, zo, CHUNKUPDATE_PRIO_HIGH, 0);
            updateChunk(xo, zo + i, CHUNKUPDATE_PRIO_HIGH, 0);
            updateChunk(xo, zo - i, CHUNKUPDATE_PRIO_HIGH, 0);
            for (int j = 1; j < i; ++j) {
                updateChunk(xo - j, zo + i, CHUNKUPDATE_PRIO_HIGH, 0);
                updateChunk(xo + j, zo + i, CHUNKUPDATE_PRIO_HIGH, 0);
                updateChunk(xo + j, zo - i, CHUNKUPDATE_PRIO_HIGH, 0);
                updateChunk(xo - j, zo - i, CHUNKUPDATE_PRIO_HIGH, 0);
                updateChunk(xo - i, zo - j, CHUNKUPDATE_PRIO_HIGH, 0);
                updateChunk(xo - i, zo + j, CHUNKUPDATE_PRIO_HIGH, 0);
                updateChunk(xo + i, zo + j, CHUNKUPDATE_PRIO_HIGH, 0);
                updateChunk(xo + i, zo - j, CHUNKUPDATE_PRIO_HIGH, 0);
            }
            updateChunk(xo - i, zo + i, CHUNKUPDATE_PRIO_HIGH, 0);
            updateChunk(xo + i, zo - i, CHUNKUPDATE_PRIO_HIGH, 0);
            updateChunk(xo - i, zo - i, CHUNKUPDATE_PRIO_HIGH, 0);
            updateChunk(xo + i, zo + i, CHUNKUPDATE_PRIO_HIGH, 0);
        }
    }

    setShaderProg(shader_block);

    return true;
}

bool startRenderer() {
    #if defined(USESDL2)
    if (!(rendinf.window = SDL_CreateWindow(PROG_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, rendinf.win_width, rendinf.win_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE))) {
        sdlerror("startRenderer: Failed to create window");
        return false;
    }
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

    #if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
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
    #endif
    glver = glGetString(GL_VERSION);
    printf("OpenGL version: %s\n", glver);
    glslver = glGetString(GL_SHADING_LANGUAGE_VERSION);
    printf("GLSL version: %s\n", glslver);
    glvend = glGetString(GL_VENDOR);
    printf("Vendor string: %s\n", glvend);
    glrend = glGetString(GL_RENDERER);
    printf("Renderer string: %s\n", glrend);
    #ifndef USEGLES
    if (GL_KHR_debug) {
        puts("KHR_debug supported");
        glDebugMessageCallback(oglCallback, NULL);
    }
    #endif

    #if DBGLVL(1)
    GLint range[2];
    glGetIntegerv(GL_ALIASED_LINE_WIDTH_RANGE, range);
    printf("GL_ALIASED_LINE_WIDTH_RANGE: [%d, %d]\n", range[0], range[1]);
    #ifndef USEGLES
    glGetIntegerv(GL_SMOOTH_LINE_WIDTH_RANGE, range);
    printf("GL_SMOOTH_LINE_WIDTH_RANGE: [%d, %d]\n", range[0], range[1]);
    #endif
    #endif
    GLint texunits;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texunits);
    printf("GL_MAX_TEXTURE_IMAGE_UNITS: [%d]\n", texunits);
    GLint texsize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &texsize);
    printf("GL_MAX_TEXTURE_SIZE: [%d]\n", texsize);
    GLint64 ubosize;
    #ifndef USEGLES
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &ubosize);
    printf("GL_MAX_UNIFORM_BLOCK_SIZE: [%"PRId64"]\n", ubosize);
    #endif

    printf("Display resolution: [%ux%u@%g]\n", rendinf.disp_width, rendinf.disp_height, rendinf.disphz);
    printf("Windowed resolution: [%ux%u@%g]\n", rendinf.win_width, rendinf.win_height, rendinf.win_fps);
    printf("Fullscreen resolution: [%ux%u@%g]\n", rendinf.full_width, rendinf.full_height, rendinf.full_fps);

    #if defined(USESDL2)
    #else
    glfwSetFramebufferSizeCallback(rendinf.window, fbsize);
    #endif

    glGenBuffers(1, &VBO2D);
    glBindBuffer(GL_ARRAY_BUFFER, VBO2D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vert2D), vert2D, GL_STATIC_DRAW);

    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    uint32_t indices[] = {0, 2, 3, 3, 1, 0};
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenRenderbuffers(1, &UIDBUF);
    glGenRenderbuffers(1, &DBUF);
    glGenFramebuffers(1, &UIFBO);
    glGenFramebuffers(1, &FBO);

    glGenTextures(1, &UIFBTEX);
    glGenTextures(1, &FBTEX);
    glGenTextures(1, &texarrayh);
    glGenTextures(1, &crosshairh);
    glGenTextures(1, &charseth);

    setShaderProg(shader_block);
    glViewport(0, 0, rendinf.width, rendinf.height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    //glEnable(GL_LINE_SMOOTH);
    //glLineWidth(3.0);
    swapBuffers();

    glEnable(GL_CULL_FACE);
    //glCullFace(GL_FRONT);
    //glFrontFace(GL_CW);
    #if !defined(USEGLES)
    glDisable(GL_MULTISAMPLE);
    #endif

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);

    glClearColor(0, 0, 0.25, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!reloadRenderer()) return false;

    return true;
}

void stopRenderer() {
    #if defined(USESDL2)
    SDL_Quit();
    #else
    glfwTerminate();
    #endif
}

#endif
