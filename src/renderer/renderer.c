#if MODULEID == MODULEID_GAME

#include <main/main.h>
#include "renderer.h"
#include "ui.h"
#ifndef __EMSCRIPTEN__
    #include "glad.h"
#endif
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
    #ifndef __EMSCRIPTEN__
        #include <SDL2/SDL.h>
    #else
        #include <SDL.h>
    #endif
#else
    #include <GLFW/glfw3.h>
#endif
#ifdef __EMSCRIPTEN__
    #include <GLES3/gl3.h>
    //#include <emscripten/html5.h>
    #define glFramebufferTexture(a, b, c, d) glFramebufferTexture2D((a), (b), GL_TEXTURE_2D, (c), (d));
    #define glPolygonMode(a, b)
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

void setNatColor(float r, float g, float b) {
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

void setVisibility(float nearval, float farval) {
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
    uc_campos[0] = sc_camx = rendinf.campos.x;
    uc_campos[1] = sc_camy = rendinf.campos.y;
    uc_campos[2] = -(sc_camz = rendinf.campos.z);
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

//static force_inline int64_t i64_mod(int64_t v, int64_t m) {return ((v % m) + m) % m;}

static force_inline void rendGetBlock(int cx, int cz, int x, int y, int z, struct blockdata* b) {
    if (y < 0 || y > 511) {
        b->id = BLOCKNO_NULL;
        b->subid = 0;
        b->light_r = 0;
        b->light_g = 0;
        b->light_b = 0;
        b->light_n = 31;
        return;
    }
    cx += x / 16 - (x < 0);
    cz -= z / 16 - (z < 0);
    if (cx < 0 || cz < 0 || cx >= (int)rendinf.chunks->info.width || cz >= (int)rendinf.chunks->info.width) {
        b->id = BLOCKNO_BORDER;
        b->subid = 0;
        b->light_r = 0;
        b->light_g = 0;
        b->light_b = 0;
        b->light_n = 31;
        return;
    }
    x &= 0xF;
    z &= 0xF;
    int c = cx + cz * rendinf.chunks->info.width;
    if (!rendinf.chunks->renddata[c].generated) {
        b->id = BLOCKNO_BORDER;
        b->subid = 0;
        b->light_r = 0;
        b->light_g = 0;
        b->light_b = 0;
        b->light_n = 31;
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

static uint32_t constBlockVert[4][6][6] = {
    {
        {0x00000F0F, 0x0F000F00, 0x0F000F0F, 0x0F000F00, 0x00000F0F, 0x00000F00}, // U
        {0x0F000F00, 0x0F00000F, 0x0F000F0F, 0x0F00000F, 0x0F000F00, 0x0F000000}, // R
        {0x0F000F0F, 0x0000000F, 0x00000F0F, 0x0000000F, 0x0F000F0F, 0x0F00000F}, // F
        {0x00000000, 0x0F00000F, 0x0F000000, 0x0F00000F, 0x00000000, 0x0000000F}, // D
        {0x00000F0F, 0x00000000, 0x00000F00, 0x00000000, 0x00000F0F, 0x0000000F}, // L
        {0x00000F00, 0x0F000000, 0x0F000F00, 0x0F000000, 0x00000F00, 0x00000000}  // B
    },
    {
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // U
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // R
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // F
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // D
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // L
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // B
    },
    {
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // U
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // R
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // F
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // D
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // L
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, // B
    },
    {
        {0x0000000C, 0x0F0F001B, 0x0F00001E, 0x0F0F001B, 0x0000000C, 0x000F0009}, // U
        {0x00000018, 0x0F0F0017, 0x0F00001E, 0x0F0F0017, 0x00000018, 0x000F0011}, // R
        {0x0000001C, 0x0F0F0007, 0x0F00000E, 0x0F0F0007, 0x0000001C, 0x000F0015}, // F
        {0x00000000, 0x0F0F0017, 0x0F000012, 0x0F0F0017, 0x00000000, 0x000F0005}, // D
        {0x0000000C, 0x0F0F0003, 0x0F00000A, 0x0F0F0003, 0x0000000C, 0x000F0005}, // L
        {0x00000008, 0x0F0F0013, 0x0F00001A, 0x0F0F0013, 0x00000008, 0x000F0001}, // B
    }
};

//static int light_n_tweak[6] = {0, 0, 0, 0, 0, 0};
//static int light_n_tweak[6] = {0, 2, 1, 4, 2, 3};
static int light_n_tweak[6] = {0, 4, 2, 8, 4, 6};
//static int light_n_tweak[6] = {0, 1, 1, 2, 1, 1};
//static int light_n_tweak[6] = {0, 1, 1, 1, 1, 1};

#define mtsetvert(_v, s, l, v, bv) {\
    if (*l >= *s) {\
        *s *= 2;\
        *_v = realloc(*_v, *s * sizeof(**_v));\
        *v = *_v + *l;\
    }\
    **v = bv;\
    ++*v; ++*l;\
}

struct tricmp {
    float dist;
    uint32_t data[12];
};

static int compare(const void* b, const void* a) {
    float fa = ((struct tricmp*)a)->dist;
    float fb = ((struct tricmp*)b)->dist;
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
        if (!rendinf.chunks->renddata[c].visfull || !rendinf.chunks->renddata[c].sortvert || !rendinf.chunks->renddata[c].tcount[1]) return;
        float camx = sc_camx - xoff * 16.0;
        float camy = sc_camy;
        float camz = sc_camz - zoff * 16.0;
        int32_t tmpsize = rendinf.chunks->renddata[c].tcount[1] / 3;
        struct tricmp* data = malloc(tmpsize * sizeof(struct tricmp));
        uint32_t* dptr = rendinf.chunks->renddata[c].sortvert;
        for (int i = 0; i < tmpsize; ++i) {
            uint32_t sv1;
            uint32_t sv2;
            float vx, vy, vz;
            float dist = 0.0;
            for (int j = 0; j < 3; ++j) {
                data[i].data[j * 4] = sv1 = *dptr++;
                data[i].data[j * 4 + 1] = *dptr++;
                data[i].data[j * 4 + 2] = *dptr++;
                data[i].data[j * 4 + 3] = sv2 = *dptr++;
                vx = (float)((int)(((sv1 >> 24) & 255) + ((sv2 >> 4) & 1))) / 16.0 - 8.0;
                vy = (float)((int)(((sv1 >> 8) & 65535) + ((sv2 >> 3) & 1))) / 16.0;
                vz = (float)((int)((sv1 & 255) + ((sv2 >> 2) & 1))) / 16.0 - 8.0;
                float tmpdist = dist3d(camx, camy, camz, vx, vy, vz);
                if (tmpdist > dist) dist = tmpdist;
            }
            data[i].dist = dist;
        }
        qsort(data, tmpsize, sizeof(struct tricmp), compare);
        dptr = rendinf.chunks->renddata[c].sortvert;
        for (int i = 0; i < tmpsize; ++i) {
            for (int j = 0; j < 12; ++j) {
                *dptr++ = data[i].data[j];
            }
        }
        free(data);
        glBindBuffer(GL_ARRAY_BUFFER, rendinf.chunks->renddata[c].VBO[1]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, tmpsize * sizeof(uint32_t) * 4 * 3, rendinf.chunks->renddata[c].sortvert);
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
    //printf("mesh: [%"PRId64", %"PRId64"]\n", x, z);
    //uint64_t stime = altutime();
    //uint64_t secttime[4] = {0};
    int vpsize = 65536;
    int vpsize2 = 65536;
    uint32_t* _vptr = malloc(vpsize * sizeof(uint32_t));
    uint32_t* _vptr2 = malloc(vpsize2 * sizeof(uint32_t));
    int vplen = 0;
    int vplen2 = 0;
    uint32_t* vptr = _vptr;
    uint32_t* vptr2 = _vptr2;
    uint32_t baseVert[4];
    int maxy = 511;
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
    struct blockdata* b = &rendinf.chunks->data[c][131071];
    while (maxy >= 0) {
        for (int i = 0; i < 256; ++i) {
            if (b->id) goto foundblock;
            --b;
        }
        --maxy;
    }
    foundblock:;
    uint8_t vispass[32][6][6];
    memset(vispass, 1, sizeof(vispass));
    //secttime[0] = altutime() - secttime[0];
    maxy /= 16;
    int ychunk = maxy;
    maxy *= 16;
    pthread_mutex_unlock(&rendinf.chunks->lock);
    microwait(0);
    uint32_t vplenold = 0;
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
                    rendGetBlock(nx, nz, x, y, z, &bdata);
                    if (!bdata.id || !blockinf[bdata.id].id || !blockinf[bdata.id].data[bdata.subid].id) continue;
                    rendGetBlock(nx, nz, x, y + 1, z, &bdata2[0]);
                    rendGetBlock(nx, nz, x + 1, y, z, &bdata2[1]);
                    rendGetBlock(nx, nz, x, y, z + 1, &bdata2[2]);
                    rendGetBlock(nx, nz, x, y - 1, z, &bdata2[3]);
                    rendGetBlock(nx, nz, x - 1, y, z, &bdata2[4]);
                    rendGetBlock(nx, nz, x, y, z - 1, &bdata2[5]);
                    for (int i = 0; i < 6; ++i) {
                        if (bdata2[i].id && blockinf[bdata2[i].id].id) {
                            if (blockinf[bdata2[i].id].data[bdata2[i].subid].transparency) {
                                if (bdata.id == bdata2[i].id && bdata.subid == bdata2[i].subid) continue;
                            } else {
                                continue;
                            }
                        }
                        if (bdata2[i].id == 255) continue;

                        baseVert[0] = ((x << 28) | (y << 12) | (z << 4)) & 0xF0FFF0F0;

                        int16_t light_n = bdata2[i].light_n - light_n_tweak[i];
                        if (light_n < 0) light_n = 0;
                        baseVert[1] = (bdata2[i].light_r << 24) | (bdata2[i].light_g << 16) | (bdata2[i].light_b << 8) | light_n;

                        baseVert[2] = ((blockinf[bdata.id].data[bdata.subid].texoff[i] << 16) & 0xFFFF0000);
                        baseVert[2] |= ((blockinf[bdata.id].data[bdata.subid].anict[i] << 8) & 0xFF00);
                        baseVert[2] |= (blockinf[bdata.id].data[bdata.subid].anidiv & 0xFF);

                        baseVert[3] = blockinf[bdata.id].data[bdata.subid].transparency << 8;

                        if (blockinf[bdata.id].data[bdata.subid].transparency >= 2) {
                            if (!bdata2[i].id && blockinf[bdata.id].data[bdata.subid].backfaces) {
                                for (int j = 5; j >= 0; --j) {
                                    mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, baseVert[0] | constBlockVert[0][i][j]);
                                    mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, baseVert[1]/* | constBlockVert[1][i][j]*/);
                                    mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, baseVert[2]/* | constBlockVert[2][i][j]*/);
                                    mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, baseVert[3] | constBlockVert[3][i][j]);
                                }
                            }
                            for (int j = 0; j < 6; ++j) {
                                mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, baseVert[0] | constBlockVert[0][i][j]);
                                mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, baseVert[1]/* | constBlockVert[1][i][j]*/);
                                mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, baseVert[2]/* | constBlockVert[2][i][j]*/);
                                mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, baseVert[3] | constBlockVert[3][i][j]);
                            }
                        } else {
                            for (int j = 0; j < 6; ++j) {
                                mtsetvert(&_vptr, &vpsize, &vplen, &vptr, baseVert[0] | constBlockVert[0][i][j]);
                                mtsetvert(&_vptr, &vpsize, &vplen, &vptr, baseVert[1]/* | constBlockVert[1][i][j]*/);
                                mtsetvert(&_vptr, &vpsize, &vplen, &vptr, baseVert[2]/* | constBlockVert[2][i][j]*/);
                                mtsetvert(&_vptr, &vpsize, &vplen, &vptr, baseVert[3] | constBlockVert[3][i][j]);
                            }
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
                    if (blockinf[bdata.id].data[bdata.subid].transparency && !visited[_x][_y % 16][_z]) {
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
                                if (blockinf[bdata.id].data[bdata.subid].transparency && !visited[x][(y + 1) % 16][z]) {
                                    posqueue[pqptr++] = (struct pq){.x = x, .y = y + 1, .z = z};
                                    visited[x][(y + 1) % 16][z] = 1;
                                    //printf("ADD Y+1 [%d, %d, %d] [%d]\n", posqueue[pqptr - 1].x, posqueue[pqptr - 1].y, posqueue[pqptr - 1].z, pqptr);
                                }
                            } else {
                                touched[CVIS_UP] = 1;
                            }
                            if (x < 15) {
                                rendGetBlock(nx, nz, x + 1, y, z, &bdata);
                                if (blockinf[bdata.id].data[bdata.subid].transparency && !visited[x + 1][y % 16][z]) {
                                    posqueue[pqptr++] = (struct pq){.x = x + 1, .y = y, .z = z};
                                    visited[x + 1][y % 16][z] = 1;
                                    //printf("ADD X+1 [%d, %d, %d] [%d]\n", posqueue[pqptr - 1].x, posqueue[pqptr - 1].y, posqueue[pqptr - 1].z, pqptr);
                                }
                            } else {
                                touched[CVIS_RIGHT] = 1;
                            }
                            if (z < 15) {
                                rendGetBlock(nx, nz, x, y, z + 1, &bdata);
                                if (blockinf[bdata.id].data[bdata.subid].transparency && !visited[x][y % 16][z + 1]) {
                                    posqueue[pqptr++] = (struct pq){.x = x, .y = y, .z = z + 1};
                                    visited[x][y % 16][z + 1] = 1;
                                    //printf("ADD Z+1 [%d, %d, %d] [%d]\n", posqueue[pqptr - 1].x, posqueue[pqptr - 1].y, posqueue[pqptr - 1].z, pqptr);
                                }
                            } else {
                                touched[CVIS_FRONT] = 1;
                            }
                            if (y > maxy) {
                                rendGetBlock(nx, nz, x, y - 1, z, &bdata);
                                if (blockinf[bdata.id].data[bdata.subid].transparency && !visited[x][(y - 1) % 16][z]) {
                                    posqueue[pqptr++] = (struct pq){.x = x, .y = y - 1, .z = z};
                                    visited[x][(y - 1) % 16][z] = 1;
                                    //printf("ADD Y-1 [%d, %d, %d] [%d]\n", posqueue[pqptr - 1].x, posqueue[pqptr - 1].y, posqueue[pqptr - 1].z, pqptr);
                                }
                            } else {
                                touched[CVIS_DOWN] = 1;
                            }
                            if (x > 0) {
                                rendGetBlock(nx, nz, x - 1, y, z, &bdata);
                                if (blockinf[bdata.id].data[bdata.subid].transparency && !visited[x - 1][y % 16][z]) {
                                    posqueue[pqptr++] = (struct pq){.x = x - 1, .y = y, .z = z};
                                    visited[x - 1][y % 16][z] = 1;
                                    //printf("ADD X-1 [%d, %d, %d] [%d]\n", posqueue[pqptr - 1].x, posqueue[pqptr - 1].y, posqueue[pqptr - 1].z, pqptr);
                                }
                            } else {
                                touched[CVIS_LEFT] = 1;
                            }
                            if (z > 0) {
                                rendGetBlock(nx, nz, x, y, z - 1, &bdata);
                                if (blockinf[bdata.id].data[bdata.subid].transparency && !visited[x][y % 16][z - 1]) {
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
        memcpy(rendinf.chunks->renddata[c].vispass, vispass, sizeof(rendinf.chunks->renddata[c].vispass));
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
            glBufferData(GL_ARRAY_BUFFER, tmpsize, rendinf.chunks->renddata[c].vertices[0], GL_STATIC_DRAW);
            rendinf.chunks->renddata[c].tcount[0] = rendinf.chunks->renddata[c].vcount[0] / 4;
            for (int i = 0; i < 32; ++i) {
                rendinf.chunks->renddata[c].ytoff[i] = rendinf.chunks->renddata[c].yvoff[i] / 4;
                rendinf.chunks->renddata[c].ytcount[i] = rendinf.chunks->renddata[c].yvcount[i] / 4;
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
            rendinf.chunks->renddata[c].tcount[1] = rendinf.chunks->renddata[c].vcount[1] / 4;
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

struct meshdata {
    int s;
    int l;
    uint32_t* _v;
    uint32_t* v;
};

static force_inline void writeuielemvert(struct meshdata* md, int x, int y, int8_t z, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    mtsetvert(&md->_v, &md->s, &md->l, &md->v, (((x) << 16) & 0xFFFF0000) | ((y) & 0xFFFF));
    mtsetvert(&md->_v, &md->s, &md->l, &md->v, ((z) & 0xFF));
    mtsetvert(&md->_v, &md->s, &md->l, &md->v, (((r) << 24) & 0xFF000000) | (((g) << 16) & 0xFF0000) | (((b) << 8) & 0xFF00) | ((a) & 0xFF));
    mtsetvert(&md->_v, &md->s, &md->l, &md->v, 0);
}

static force_inline void writeuitextvert(struct meshdata* md, int x, int y, int8_t z,
                                         char c, uint8_t tx, uint8_t ty, uint8_t tmx, uint8_t tmy,
                                         uint8_t fgc, uint8_t bgc, uint8_t fga, uint8_t bga, uint8_t attrib) {
    mtsetvert(&md->_v, &md->s, &md->l, &md->v, (((x) << 16) & 0xFFFF0000) | ((y) & 0xFFFF));
    mtsetvert(&md->_v, &md->s, &md->l, &md->v, 0x80000000 | (((attrib) << 24) & 0xF000000) | (((c) << 8) & 0xFF00) | ((z) & 0xFF));
    mtsetvert(&md->_v, &md->s, &md->l, &md->v, (((fga) << 24) & 0xFF000000) | (((bga) << 16) & 0xFF0000) | (((fgc) << 12) & 0xF000) | (((bgc) << 8) & 0xF00));
    mtsetvert(&md->_v, &md->s, &md->l, &md->v, (((tx) << 24) & 0xFF000000) | (((ty) << 16) & 0xFF0000) | (((tmx) << 8) & 0xFF00) | ((tmy) & 0xFF));
}

static force_inline void writeuielemrect(struct meshdata* md, int x0, int y0, int x1, int y1, int8_t z, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    writeuielemvert(md, x0, y0, z, r, g, b, a);
    writeuielemvert(md, x0, y1, z, r, g, b, a);
    writeuielemvert(md, x1, y0, z, r, g, b, a);
    writeuielemvert(md, x1, y0, z, r, g, b, a);
    writeuielemvert(md, x0, y1, z, r, g, b, a);
    writeuielemvert(md, x1, y1, z, r, g, b, a);
}

static force_inline void writeuitextchar(struct meshdata* md, int x, int y, int z,
                                         uint8_t ol, uint8_t ot, uint8_t or, uint8_t ob,
                                         uint8_t tmx, uint8_t tmy, char c, uint8_t attrib,
                                         uint8_t fgc, uint8_t bgc, uint8_t fga, uint8_t bga) {
    writeuitextvert(md, x + ol, y + ot, z, c, ol, ot, tmx, tmy, fgc, bgc, fga, bga, attrib);
    writeuitextvert(md, x + ol, y + ob, z, c, ol, ob, tmx, tmy, fgc, bgc, fga, bga, attrib);
    writeuitextvert(md, x + or, y + ot, z, c, or, ot, tmx, tmy, fgc, bgc, fga, bga, attrib);
    writeuitextvert(md, x + or, y + ot, z, c, or, ot, tmx, tmy, fgc, bgc, fga, bga, attrib);
    writeuitextvert(md, x + ol, y + ob, z, c, ol, ob, tmx, tmy, fgc, bgc, fga, bga, attrib);
    writeuitextvert(md, x + or, y + ob, z, c, or, ob, tmx, tmy, fgc, bgc, fga, bga, attrib);
}

struct muie_textline {
    int width;
    int chars;
    char* ptr;
};

static void meshUIElem(struct meshdata* md, struct ui_data* elemdata, struct ui_elem* e) {
    //printf("mesh: {%s} [hidden:%d]\n", e->name, e->calcprop.hidden);
    struct ui_elem_calcprop* p = &e->calcprop;
    int s = elemdata->scale;
    char* curprop;
    {
        int x0 = p->x, y0 = p->y, x1 = p->x + p->width, y1 = p->y + p->height;
        switch (e->type) {
            case UI_ELEM_BOX:; {
                writeuielemrect(md, x0, y0, x1, y1, p->z, p->r, p->g, p->b, p->a);
            } break;
            case UI_ELEM_FANCYBOX:; {
                writeuielemrect(md, x0, y0 + s, x1, y1 - s, p->z, p->r / 2, p->g / 2, p->b / 2, p->a);
                writeuielemrect(md, x0 + s, y0, x1 - s, y1, p->z, p->r / 2, p->g / 2, p->b / 2, p->a);
                writeuielemrect(md, x0 + 2 * s, y0 + 2 * s, x1 - 2 * s, y1 - 2 * s, p->z, p->r, p->g, p->b, p->a);
            } break;
            case UI_ELEM_HOTBAR:; {
                int slot = -1;
                curprop = getUIElemProperty(e, "slot");
                if (curprop) slot = atoi(curprop);
                writeuielemrect(md, x0, y0 + s, x1, y1 - s, p->z, 63, 63, 63, p->a);
                writeuielemrect(md, x0 + s, y0, x1 - s, y1, p->z, 63, 63, 63, p->a);
                for (int i = 0; i < 10; ++i) {
                    uint8_t r, g, b;
                    if (i == slot) {
                        r = 255;
                        g = 255;
                        b = 127;
                    } else {
                        r = 127;
                        g = 127;
                        b = 127;
                    }
                    writeuielemrect(
                        md,
                        x0 + (i * 30 + 2) * s, y0 + s * 2, x0 + ((i + 1) * 30) * s, y1 - s * 2,
                        p->z,
                        r, g, b, p->a / 2
                    );
                }
            } break;
            case UI_ELEM_ITEMGRID:; {
                writeuielemrect(md, x0, y0 + s, x1, y1 - s, p->z, 63, 63, 63, p->a);
                writeuielemrect(md, x0 + s, y0, x1 - s, y1, p->z, 63, 63, 63, p->a);
                curprop = getUIElemProperty(e, "width");
                int width = (curprop) ? atoi(curprop) : 1;
                curprop = getUIElemProperty(e, "height");
                int height = (curprop) ? atoi(curprop) : 1;
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        writeuielemrect(
                            md,
                            x0 + (x * 30 + 2) * s, y0 + (y * 30 + 2) * s, x0 + ((x + 1) * 30) * s, y0 + ((y + 1) * 30) * s,
                            p->z,
                            140, 140, 140, p->a
                        );
                    }
                }
            } break;
            case UI_ELEM_BUTTON:; {
                writeuielemrect(md, x0, y0 + s, x1, y1 - s, p->z, p->r / 2, p->g / 2, p->b / 2, p->a);
                writeuielemrect(md, x0 + s, y0, x1 - s, y1, p->z, p->r / 2, p->g / 2, p->b / 2, p->a);
                switch (e->state) {
                    case UI_STATE_HOVERED:;
                        writeuielemrect(md, x0 + 2 * s, y0 + 2 * s, x1 - 2 * s, y1 - 2 * s, p->z, 191, 191, 127, p->a);
                        break;
                    case UI_STATE_CLICKED:;
                        writeuielemrect(md, x0 + 2 * s, y0 + 2 * s, x1 - 2 * s, y1 - 2 * s, p->z, p->r / 2, p->g / 2, p->b / 2, p->a);
                        break;
                    default:;
                        writeuielemrect(md, x0 + 2 * s, y0 + 2 * s, x1 - 2 * s, y1 - 2 * s, p->z, p->r, p->g, p->b, p->a);
                        break;
                }
            } break;
            case UI_ELEM_PROGRESSBAR:; {
                writeuielemrect(md, x0, y0 + s, x1, y1 - s, p->z, p->r / 2, p->g / 2, p->b / 2, p->a);
                writeuielemrect(md, x0 + s, y0, x1 - s, y1, p->z, p->r / 2, p->g / 2, p->b / 2, p->a);
                writeuielemrect(md, x0 + 2 * s, y0 + 2 * s, x1 - 2 * s, y1 - 2 * s, p->z, p->r, p->g, p->b, p->a);
                curprop = getUIElemProperty(e, "progress");
                float progress = (curprop) ? atof(curprop) / 100.0 : 0.0;
                int newx1 = (int)((float)p->x + (float)p->width * progress) - 2 * s;
                int newx0 = x0 + 2 * s;
                if (newx1 > newx0) {
                    writeuielemrect(md, newx0, y0 + 2 * s, newx1, y1 - 2 * s, p->z, 0, 63, 191, p->a);
                }
            } break;
        }
    }
    char* text = getUIElemProperty(e, "text");
    if (text) {
        char* t = text;
        int ax = 0, ay = 0;
        curprop = getUIElemProperty(e, "text_align");
        if (curprop) sscanf(curprop, "%d,%d", &ax, &ay);
        uint8_t fgc = 15;
        curprop = getUIElemProperty(e, "text_fgc");
        if (curprop) fgc = atoi(curprop);
        uint8_t bgc = 0;
        curprop = getUIElemProperty(e, "text_bgc");
        if (curprop) bgc = atoi(curprop);
        uint8_t fga = 255;
        curprop = getUIElemProperty(e, "text_fga");
        if (curprop) fga = 255.0 * atof(curprop);
        uint8_t bga = 0;
        curprop = getUIElemProperty(e, "text_bga");
        if (curprop) bga = 255.0 * atof(curprop);
        curprop = getUIElemProperty(e, "text_margin");
        int mx = 0;
        int my = 0;
        if (curprop) {
            sscanf(curprop, "%d,%d", &mx, &my);
            mx *= s;
            my *= s;
            p->x += mx;
            p->width -= mx * 2;
            p->y += my;
            p->height -= my * 2;
        }
        uint8_t attrib = 0;
        curprop = getUIElemProperty(e, "text_bold");
        if (curprop) attrib |= getBool(curprop);
        curprop = getUIElemProperty(e, "text_italic");
        if (curprop) attrib |= getBool(curprop) << 1;
        curprop = getUIElemProperty(e, "text_underline");
        if (curprop) attrib |= getBool(curprop) << 2;
        curprop = getUIElemProperty(e, "text_strikethrough");
        if (curprop) attrib |= getBool(curprop) << 3;
        int text_scale = 1;
        curprop = getUIElemProperty(e, "text_scale");
        if (curprop) text_scale = atoi(curprop);
        int end = p->x + p->width;
        static int tcw = 8, tch = 16;
        int lines = 1;
        struct muie_textline* tdata = calloc(lines, sizeof(*tdata));
        {
            int l = 0;
            #define nextline(p) {\
                if (*p) {\
                    tdata = realloc(tdata, (++lines) * sizeof(*tdata));\
                    ++l;\
                    memset(&tdata[l], 0, sizeof(*tdata));\
                    tdata[l].ptr = (p);\
                }\
            }
            tdata[0].ptr = t;
            while (*t) {
                if (*t == ' ' || *t == '\t') {
                    int tmpw = tdata[l].width + tcw * s * text_scale;
                    for (int i = 1; t[i] && t[i] != ' ' && t[i] != '\t' && t[i] != '\n'; ++i) {
                        tmpw += tcw * s * text_scale;
                    }
                    if (tmpw > p->width) {
                        nextline(t + 1);
                    } else {
                        tdata[l].width += tcw * s * text_scale;
                        ++tdata[l].chars;
                    }
                } else if (*t == '\n') {
                    nextline(t + 1);
                } else {
                    tdata[l].width += tcw * s * text_scale;
                    if (tdata[l].width > p->width) {
                        tdata[l].width -= tcw * s * text_scale;
                        nextline(t);
                    }
                    ++tdata[l].chars;
                }
                ++t;
            }
        }
        {
            int x, y;
            switch (ay) {
                case -1:; y = p->y; break;
                default:; y = ((float)p->y + (float)p->height / 2.0) - (float)(lines * tch * s * text_scale) / 2.0; break;
                case 1:; y = p->y + (p->height - lines * tch * s * text_scale); break;
            }
            for (int i = 0; i < lines; ++i) {
                switch (ax) {
                    case -1:; x = p->x; break;
                    default:; x = ((float)p->x + (float)p->width / 2.0) - (float)tdata[i].width / 2.0; break;
                    case 1:; x = (end - tdata[i].width); break;
                }
                for (int j = 0; j < tdata[i].chars; ++j) {
                    uint8_t ol = 0, or = tcw * s * text_scale, ot = 0, ob = tch * s * text_scale, stcw = tcw * s * text_scale, stch = tch * s * text_scale;
                    if (/*x + or >= p->x && x <= p->x + p->width &&*/ y + ob >= p->y && y <= p->y + p->height) {
                        //if (x + or > p->x + p->width) or -= (x + or) - (p->x + p->width);
                        if (y + ob > p->y + p->height) ob -= (y + ob) - (p->y + p->height);
                        //if (x + ol < p->x) ol += (p->x) - (x + or);
                        if (y + ot < p->y) ot += (p->y) - (y + ot);
                        writeuitextchar(md, x, y, p->z, ol, ot, or, ob, stcw, stch, tdata[i].ptr[j], attrib, fgc, bgc, fga, bga);
                    }
                    x += tcw * s * text_scale;
                }
                y += tch * s * text_scale;
            }
        }
        free(tdata);
        p->x -= mx;
        p->width += mx * 2;
        p->y -= my;
        p->height += my * 2;
    }
}

static inline void meshUIElemTree(struct meshdata* md, struct ui_data* elemdata, struct ui_elem* e) {
    meshUIElem(md, elemdata, e);
    for (int i = 0; i < e->children; ++i) {
        if (isUIElemValid(elemdata, e->childdata[i])) {
            struct ui_elem* c = &elemdata->data[e->childdata[i]];
            if (!c->calcprop.hidden) {
                meshUIElemTree(md, elemdata, c);
            }
        }
    }
}

static force_inline void meshUIElems(struct meshdata* md, struct ui_data* elemdata) {
    for (int i = 0; i < elemdata->count; ++i) {
        struct ui_elem* e = &elemdata->data[i];
        if (isUIElemValid(elemdata, i) && e->parent < 0 && !e->calcprop.hidden) {
            //printf("mesh tree: %s\n", e->name);
            meshUIElemTree(md, elemdata, e);
        }
    }
}

static force_inline bool renderUI(struct ui_data* data) {
    if (data->hidden) return false;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindBuffer(GL_ARRAY_BUFFER, data->renddata.VBO);
    if (calcUIProperties(data)) {
        struct meshdata mdata;
        mdata.s = 64;
        mdata._v = malloc(mdata.s * sizeof(uint32_t));
        mdata.l = 0;
        mdata.v = mdata._v;
        meshUIElems(&mdata, data);
        glBufferData(GL_ARRAY_BUFFER, mdata.l * sizeof(uint32_t), mdata._v, GL_STATIC_DRAW);
        data->renddata.vcount = mdata.l / 4;
        free(mdata._v);
    }
    if (!data->renddata.vcount) return false;
    setUniform1f(rendinf.shaderprog, "scale", data->scale);
    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 4 * sizeof(uint32_t), (void*)(0));
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 4 * sizeof(uint32_t), (void*)(sizeof(uint32_t)));
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 4 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 2));
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, 4 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 3));
    glDrawArrays(GL_TRIANGLES, 0, data->renddata.vcount);
    return true;
}

const unsigned char* glver;
const unsigned char* glslver;
const unsigned char* glvend;
const unsigned char* glrend;

static resdata_image* crosshair;

static int dbgtextuih = -1;

struct pq {
    int8_t x;
    int8_t y;
    int8_t z;
    int8_t from;
    int8_t dir[6];
};

struct pq* posqueue;
int pqptr;
#define pqpush(_x, _y, _z, _f) {\
    posqueue[pqptr].x = (_x);\
    posqueue[pqptr].y = (_y);\
    posqueue[pqptr].z = (_z);\
    posqueue[pqptr].from = (_f);\
    ++pqptr;\
}
#define pqpop() {\
    --pqptr;\
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

static int cavecull;
bool rendergame = false;

void render() {
    //printf("rendergame: [%d]\n", rendergame);
    if (showDebugInfo) {
        static char tbuf[1][32768];
        static int toff = 0;
        if (game_ui[UILAYER_DBGINF] && dbgtextuih == -1) {
            dbgtextuih = newUIElem(
                game_ui[UILAYER_DBGINF], UI_ELEM_CONTAINER,
                UI_ATTR_NAME, "debugText", UI_ATTR_DONE,
                "width", "100%", "height", "100%",
                "text_align", "-1,-1", "text_fgc", "14", "text_bga", "0.5",
                "text", "",
                NULL
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
                "Chunk: (%"PRId64", %"PRId64")\n",
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
                "FPS: %.2lf (%.2lf)\n",
                rendinf.width, rendinf.height, rendinf.fps, (rendinf.vsync) ? "on" : "off",
                fps, realfps
            );
        }
        if (game_ui[UILAYER_DBGINF]) editUIElem(game_ui[UILAYER_DBGINF], dbgtextuih, UI_ATTR_DONE, "text", tbuf, NULL);
    }

    if (rendergame) {
        setShaderProg(shader_block);
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        glClearColor(sky.r, sky.g, sky.b, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_BLEND);
        static uint64_t aMStart = 0;
        if (!aMStart) aMStart = altutime();
        glUniform1ui(glGetUniformLocation(rendinf.shaderprog, "aniMult"), (aMStart - altutime()) / 10000);
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
                pqpush(rendinf.chunks->info.dist, ncy, rendinf.chunks->info.dist, -1);
                int v = rendinf.chunks->info.dist + rendinf.chunks->info.dist * rendinf.chunks->info.width + ncy * rendinf.chunks->info.widthsq;
                visited[v] = true;
            }
            int pqptrmax = 0;
            while (pqptr > 0) {
                if (pqptr > pqptrmax) pqptrmax = pqptr;
                pqpop();
                struct pq p = posqueue[pqptr];

                pqvisit(&p, p.x, p.y + 1, p.z, CVIS_UP);
                pqvisit(&p, p.x + 1, p.y, p.z, CVIS_RIGHT);
                pqvisit(&p, p.x, p.y, p.z + 1, CVIS_BACK);
                pqvisit(&p, p.x, p.y - 1, p.z, CVIS_DOWN);
                pqvisit(&p, p.x - 1, p.y, p.z, CVIS_LEFT);
                pqvisit(&p, p.x, p.y, p.z - 1, CVIS_FRONT);
            }

            #define setvis(_x, _y, _z) {\
                if ((_x) >= 0 && (_y) >= 0 && (_z) >= 0 && (_x) < (int)rendinf.chunks->info.width && (_y) < 32 && (_z) < (int)rendinf.chunks->info.width) {\
                    rendinf.chunks->renddata[(_x) + (_z) * rendinf.chunks->info.width].visible |= (1 << (_y));\
                }\
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
                    if (rendinf.chunks->renddata[rendc].tcount[0]) {
                        coord[0] = (int)(rendc % rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist;
                        coord[1] = (int)(rendc / rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist;
                        setUniform2f(rendinf.shaderprog, "ccoord", coord);
                        glBindBuffer(GL_ARRAY_BUFFER, rendinf.chunks->renddata[rendc].VBO[0]);
                        glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 4 * sizeof(uint32_t), (void*)(0));
                        glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 4 * sizeof(uint32_t), (void*)(sizeof(uint32_t)));
                        glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 4 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 2));
                        glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, 4 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 3));
                        for (int y = 31; y >= 0; --y) {
                            if ((!debug_nocavecull && !(rendinf.chunks->renddata[rendc].visible & (1 << y))) || !rendinf.chunks->renddata[rendc].ytcount[y]) continue;
                            if (isVisible(&frust, corner1[0], y * 16.0, corner1[1], corner2[0], (y + 1) * 16.0, corner2[1])) {
                                //printf("REND OPAQUE: [%d]:[%d]\n", rendc, y);
                                glDrawArrays(GL_TRIANGLES, rendinf.chunks->renddata[rendc].ytoff[y], rendinf.chunks->renddata[rendc].ytcount[y]);
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
                if (rendinf.chunks->renddata[rendc].tcount[1]) {
                    coord[0] = (int)(rendc % rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist;
                    coord[1] = (int)(rendc / rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist;
                    setUniform2f(rendinf.shaderprog, "ccoord", coord);
                    glBindBuffer(GL_ARRAY_BUFFER, rendinf.chunks->renddata[rendc].VBO[1]);
                    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 4 * sizeof(uint32_t), (void*)(0));
                    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 4 * sizeof(uint32_t), (void*)(sizeof(uint32_t)));
                    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 4 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 2));
                    glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, 4 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 3));
                    glDrawArrays(GL_TRIANGLES, 0, rendinf.chunks->renddata[rendc].tcount[1]);
                }
            }
        }
        glDepthMask(true);
        if (debug_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glDisable(GL_CULL_FACE);
    if (rendergame) {
        glDisable(GL_DEPTH_TEST);
        setShaderProg(shader_2d);
        glBindBuffer(GL_ARRAY_BUFFER, VBO2D);
        glUniform1f(glGetUniformLocation(rendinf.shaderprog, "xratio"), ((float)(crosshair->width)) / (float)rendinf.width);
        glUniform1f(glGetUniformLocation(rendinf.shaderprog, "yratio"), ((float)(crosshair->height)) / (float)rendinf.height);
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
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "texData"), UIFBTEXID - GL_TEXTURE0);
    setUniform3f(rendinf.shaderprog, "mcolor", (float[]){1, 1, 1});
    if (!rendergame) {
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
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        if (renderUI(game_ui[i])) {
            //printf("renderUI(game_ui[%d])\n", i);
            setShaderProg(shader_framebuffer);
            glUniform1i(glGetUniformLocation(rendinf.shaderprog, "fbtype"), 1);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
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

#ifndef __EMSCRIPTEN__
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
    declareConfigKey(config, "Renderer", "fullScreen", "false", false);
    declareConfigKey(config, "Renderer", "FOV", "85", false);
    declareConfigKey(config, "Renderer", "mipmaps", "true", false);
    declareConfigKey(config, "Renderer", "nearPlane", "0.05", false);
    declareConfigKey(config, "Renderer", "farPlane", "2500", false);
    declareConfigKey(config, "Renderer", "lazyMesher", "false", false);
    declareConfigKey(config, "Renderer", "caveCullLevel", "1", false);
    declareConfigKey(config, "Renderer", "mesherThreadsMax", "1", false);

    rendinf.campos = GFX_DEFAULT_POS;
    rendinf.camrot = GFX_DEFAULT_ROT;

    cavecull = atoi(getConfigKey(config, "Renderer", "caveCullLevel"));
    MESHER_THREADS_MAX = atoi(getConfigKey(config, "Renderer", "mesherThreadsMax"));

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

    #if defined(USESDL2)
    #ifndef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    #endif
    #if defined(USEGLES)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    #else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
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

bool reloadRenderer() {
    #if defined(USEGLES)
    char* hdrpath = "engine/shaders/headers/OpenGL ES/header.glsl";
    #else
    char* hdrpath = "engine/shaders/headers/OpenGL/header.glsl";
    #endif
    file_data* hdr = loadResource(RESOURCE_TEXTFILE, hdrpath);
    if (!hdr) {
        fputs("reloadRenderer: Failed to load shader header\n", stderr);
        return false;
    }
    #if DBGLVL(1)
    puts("Compiling block shader...");
    #endif
    file_data* vs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/block/vertex.glsl");
    file_data* fs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/block/fragment.glsl");
    if (!vs || !fs || !makeShaderProg((char*)hdr->data, (char*)vs->data, (char*)fs->data, &shader_block)) {
        fputs("reloadRenderer: Failed to compile block shader\n", stderr);
        return false;
    }
    freeResource(vs);
    freeResource(fs);
    #if DBGLVL(1)
    puts("Compiling 2D shader...");
    #endif
    vs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/2D/vertex.glsl");
    fs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/2D/fragment.glsl");
    if (!vs || !fs || !makeShaderProg((char*)hdr->data, (char*)vs->data, (char*)fs->data, &shader_2d)) {
        fputs("reloadRenderer: Failed to compile 2D shader\n", stderr);
        return false;
    }
    freeResource(vs);
    freeResource(fs);
    #if DBGLVL(1)
    puts("Compiling UI shader...");
    #endif
    vs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/ui/vertex.glsl");
    fs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/ui/fragment.glsl");
    if (!vs || !fs || !makeShaderProg((char*)hdr->data, (char*)vs->data, (char*)fs->data, &shader_ui)) {
        fputs("reloadRenderer: Failed to compile UI shader\n", stderr);
        return false;
    }
    freeResource(vs);
    freeResource(fs);
    #if DBGLVL(1)
    puts("Compiling framebuffer shader...");
    #endif
    vs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/framebuffer/vertex.glsl");
    fs = loadResource(RESOURCE_TEXTFILE, "engine/shaders/code/GLSL/framebuffer/fragment.glsl");
    if (!vs || !fs || !makeShaderProg((char*)hdr->data, (char*)vs->data, (char*)fs->data, &shader_framebuffer)) {
        fputs("reloadRenderer: Failed to compile framebuffer shader\n", stderr);
        return false;
    }
    freeResource(vs);
    freeResource(fs);

    int gltex = GL_TEXTURE0;

    #if DBGLVL(1)
    puts("Creating UI framebuffer...");
    #endif
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

    #if DBGLVL(1)
    puts("Creating game framebuffer...");
    #endif
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

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

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
    unsigned texmaph;
    unsigned charseth;
    unsigned crosshairh;

    //puts("creating texture map...");
    int texmapsize = 0;
    texmap = malloc(texmapsize * 1024);
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
                if (j == 0) blockinf[i].data[s].texstart = texmapsize;
                ++blockinf[i].data[s].texcount;
                resdata_image* img = loadResource(RESOURCE_IMAGE, tmpbuf);
                for (int k = 3; k < 1024; k += 4) {
                    if (img->data[k] < 255) {
                        if (img->data[k]) {
                            blockinf[i].data[s].transparency = 2;
                            break;
                        } else {
                            blockinf[i].data[s].transparency = 1;
                        }
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
    glGenTextures(1, &texmaph);
    glActiveTexture(gltex++);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texmaph);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 16, 16, texmapsize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texmap);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "mipmap"), mipmaps);
    if (mipmaps) {
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    } else {
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    free(texmap);

    glGenBuffers(1, &VBO2D);
    glBindBuffer(GL_ARRAY_BUFFER, VBO2D);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vert2D), vert2D, GL_STATIC_DRAW);

    setShaderProg(shader_2d);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "texData"), gltex - GL_TEXTURE0);
    glGenTextures(1, &crosshairh);
    glActiveTexture(gltex++);
    crosshair = loadResource(RESOURCE_IMAGE, "game/textures/ui/crosshair.png");
    glBindTexture(GL_TEXTURE_2D, crosshairh);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, crosshair->width, crosshair->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, crosshair->data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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

    #ifndef __EMSCRIPTEN__
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
    #ifndef __EMSCRIPTEN__
    if (GL_KHR_debug) {
        puts("KHR_debug supported");
        glDebugMessageCallback(oglCallback, NULL);
    }
    #endif

    #if DBGLVL(1)
    GLint range[2];
    glGetIntegerv(GL_ALIASED_LINE_WIDTH_RANGE, range);
    printf("GL_ALIASED_LINE_WIDTH_RANGE: [%d, %d]\n", range[0], range[1]);
    #ifndef __EMSCRIPTEN__
    glGetIntegerv(GL_SMOOTH_LINE_WIDTH_RANGE, range);
    printf("GL_SMOOTH_LINE_WIDTH_RANGE: [%d, %d]\n", range[0], range[1]);
    #endif
    GLint texunits;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texunits);
    printf("GL_MAX_TEXTURE_IMAGE_UNITS: [%d]\n", texunits);
    #endif

    printf("Display resolution: [%ux%u@%g]\n", rendinf.disp_width, rendinf.disp_height, rendinf.disphz);
    printf("Windowed resolution: [%ux%u@%g]\n", rendinf.win_width, rendinf.win_height, rendinf.win_fps);
    printf("Fullscreen resolution: [%ux%u@%g]\n", rendinf.full_width, rendinf.full_height, rendinf.full_fps);

    #if defined(USESDL2)
    #else
    glfwSetFramebufferSizeCallback(rendinf.window, fbsize);
    #endif
    glGenRenderbuffers(1, &UIDBUF);
    glGenRenderbuffers(1, &DBUF);

    setShaderProg(shader_block);
    glViewport(0, 0, rendinf.width, rendinf.height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    //glEnable(GL_LINE_SMOOTH);
    //glLineWidth(3.0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    #if defined(USESDL2)
    SDL_GL_SwapWindow(rendinf.window);
    #else
    glfwSwapBuffers(rendinf.window);
    #endif

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

    if (!reloadRenderer()) return false;

    glClearColor(0, 0, 0.25, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    updateCam();
    setFullscreen(rendinf.fullscr);

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
