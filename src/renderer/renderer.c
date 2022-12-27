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
    uc_campos[2] = sc_camz = -rendinf.campos.z;
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
    if (game_ui[UILAYER_INGAME]) game_ui[UILAYER_INGAME]->scale = s;
    //printf("Scale UI to [%d] (%dx%d)\n", s, rendinf.width, rendinf.height);
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
    #endif
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

static force_inline int64_t i64_mod(int64_t v, int64_t m) {return ((v % m) + m) % m;}

static force_inline void rendGetBlock(int64_t cx, int64_t cz, int x, int y, int z, struct blockdata* b) {
    if (y < 0 || y > 255) {b->id = BLOCKNO_NULL; return;}
    cx += x / 16 - (x < 0);
    cz -= z / 16 - (z < 0);
    if (cx < 0 || cz < 0 || cx >= rendinf.chunks->info.width || cz >= rendinf.chunks->info.width) {b->id = BLOCKNO_BORDER; return;}
    x = i64_mod(x, 16);
    z = i64_mod(z, 16);
    int c = cx + cz * rendinf.chunks->info.width;
    if (!rendinf.chunks->renddata[c].generated) {b->id = BLOCKNO_BORDER; return;};
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
    int size;
    int rptr;
    int wptr;
    struct msgdata_msg* msg;
    pthread_mutex_t lock;
};

struct msgdata chunkmsgs[CHUNKUPDATE_PRIO__MAX];

static force_inline void initMsgData(struct msgdata* mdata) {
    mdata->size = 0;
    mdata->rptr = -1;
    mdata->wptr = -1;
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

static force_inline void addMsg(struct msgdata* mdata, int64_t x, int64_t z, uint64_t id, bool dep, int lvl) {
    static uint64_t mdataid = 0;
    pthread_mutex_lock(&mdata->lock);
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
    pthread_mutex_unlock(&mdata->lock);
}

void updateChunk(int64_t x, int64_t z, int p, int updatelvl) {
    addMsg(&chunkmsgs[p], x, z, 0, false, updatelvl);
}

static force_inline bool getNextMsg(struct msgdata* mdata, struct msgdata_msg* msg) {
    pthread_mutex_lock(&mdata->lock);
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

struct tricmp {
    float dist;
    uint32_t data[18];
};

static int compare(const void* b, const void* a) {
    float fa = ((struct tricmp*)a)->dist;
    float fb = ((struct tricmp*)b)->dist;
    return (fa > fb) - (fa < fb);
}

static force_inline float dist3d(float x0, float y0, float z0, float x1, float y1, float z1) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float dz = z1 - z0;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

static force_inline void _sortChunk(int32_t c, int xoff, int zoff, bool update) {
    if (c < 0) c = (xoff + rendinf.chunks->info.dist) + (rendinf.chunks->info.width - (zoff + rendinf.chunks->info.dist) - 1) * rendinf.chunks->info.width;
    if (update) {
        if (!rendinf.chunks->renddata[c].sortvert || !rendinf.chunks->renddata[c].tcount[1] || !rendinf.chunks->renddata[c].visible) return;
        float camx = sc_camx - xoff * 16;
        float camy = sc_camy;
        float camz = -sc_camz - zoff * 16;
        int32_t tmpsize = rendinf.chunks->renddata[c].tcount[1] / 3 / 2;
        struct tricmp* data = malloc(tmpsize * sizeof(struct tricmp));
        uint32_t* dptr = rendinf.chunks->renddata[c].sortvert;
        for (int i = 0; i < tmpsize; ++i) {
            uint32_t sv;
            float vx = 0, vy = 0, vz = 0;
            for (int j = 0; j < 6; ++j) {
                data[i].data[0 + j * 3] = sv = *dptr++;
                data[i].data[1 + j * 3] = *dptr++;
                data[i].data[2 + j * 3] = *dptr++;
                vx += (float)(((sv >> 24) & 255) + ((sv >> 2) & 1)) / 16.0 - 8.0;
                vy += (float)(((sv >> 12) & 4095) + ((sv >> 1) & 1)) / 16.0;
                vz += (float)(((sv >> 4) & 255) + (sv & 1)) / 16.0 - 8.0;
            }
            vx /= 6.0;
            vy /= 6.0;
            vz /= 6.0;
            data[i].dist = dist3d(camx, camy, camz, vx, vy, vz);
        }
        qsort(data, tmpsize, sizeof(struct tricmp), compare);
        dptr = rendinf.chunks->renddata[c].sortvert;
        for (int i = 0; i < tmpsize; ++i) {
            for (int j = 0; j < 18; ++j) {
                *dptr++ = data[i].data[j];
            }
        }
        free(data);
        glBindBuffer(GL_ARRAY_BUFFER, rendinf.chunks->renddata[c].VBO[1]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, tmpsize * sizeof(uint32_t) * 3 * 3 * 2, rendinf.chunks->renddata[c].sortvert);
    } else {
        rendinf.chunks->renddata[c].remesh[1] = true;
        rendinf.chunks->renddata[c].ready = true;
    }
}

void sortChunk(int32_t c, int xoff, int zoff, bool update) {
    _sortChunk(c, xoff, zoff, update);
}

static force_inline void mesh(int64_t x, int64_t z, uint64_t id) {
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
        //printf("meshing [%"PRId64", %"PRId64"] -> [%"PRId64", %"PRId64"] (c=%d, offset=[%"PRId64", %"PRId64"])\n", x, z, nx, nz, c, cxo, czo);
    }
    pthread_mutex_unlock(&rendinf.chunks->lock);
    //printf("mesh: [%"PRId64", %"PRId64"]\n", x, z);
    //uint64_t stime = altutime();
    int vpsize = 16384;
    int vpsize2 = 16384;
    uint32_t* _vptr = malloc(vpsize * sizeof(uint32_t));
    uint32_t* _vptr2 = malloc(vpsize2 * sizeof(uint32_t));
    int vplen = 0;
    int vplen2 = 0;
    uint32_t* vptr = _vptr;
    uint32_t* vptr2 = _vptr2;
    uint32_t baseVert1, baseVert2, baseVert3;
    for (int y = 0; y < 256; ++y) {
        pthread_mutex_lock(&rendinf.chunks->lock);
        nx = (x - rendinf.chunks->xoff) + rendinf.chunks->info.dist;
        nz = rendinf.chunks->info.width - ((z - rendinf.chunks->zoff) + rendinf.chunks->info.dist) - 1;
        if (nx < 0 || nz < 0 || nx >= rendinf.chunks->info.width || nz >= rendinf.chunks->info.width) {
            free(_vptr);
            free(_vptr2);
            goto lblcontinue;
        }
        //uint64_t c = nx + nz * rendinf.chunks->info.width;
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
                    baseVert1 = ((x << 28) | (y << 16) | (z << 8)) & 0xF0FF0F00;
                    baseVert2 = ((bdata2[i].light_r << 28) | (bdata2[i].light_g << 24) | (bdata2[i].light_b << 20) |
                                         blockinf[bdata.id].data[bdata.subid].anidiv) & 0xFFF000FF;
                    baseVert3 = ((blockinf[bdata.id].data[bdata.subid].texoff[i] << 16) & 0xFFFF0000) |
                                         ((blockinf[bdata.id].data[bdata.subid].anict[i] << 8) & 0xFF00) | (bdata2[i].light_n);
                    if (blockinf[bdata.id].data[bdata.subid].transparency >= 2) {
                        if (!bdata2[i].id && blockinf[bdata.id].data[bdata.subid].backfaces) {
                            for (int j = 5; j >= 0; --j) {
                                mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, constBlockVert1[i][j] | baseVert1);
                                mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, constBlockVert2[i][j] | baseVert2);
                                mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, baseVert3);
                            }
                        }
                        for (int j = 0; j < 6; ++j) {
                            mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, constBlockVert1[i][j] | baseVert1);
                            mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, constBlockVert2[i][j] | baseVert2);
                            mtsetvert(&_vptr2, &vpsize2, &vplen2, &vptr2, baseVert3);
                        }
                        //printf("added [%d][%d %d %d][%d]: [%u]: [%08X]...\n", c, x, y, z, i, (uint8_t)bdata.id, baseVert1);
                    } else {
                        for (int j = 0; j < 6; ++j) {
                            mtsetvert(&_vptr, &vpsize, &vplen, &vptr, constBlockVert1[i][j] | baseVert1);
                            mtsetvert(&_vptr, &vpsize, &vplen, &vptr, constBlockVert2[i][j] | baseVert2);
                            mtsetvert(&_vptr, &vpsize, &vplen, &vptr, baseVert3);
                        }
                        //printf("added [%"PRId64"][%d %d %d][%d]: [%u]: [%08X]...\n", c, x, y, z, i, (uint8_t)bdata.id, baseVert1);
                    }
                }
            }
        }
        pthread_mutex_unlock(&rendinf.chunks->lock);
    }
    pthread_mutex_lock(&rendinf.chunks->lock);
    nx = (x - rendinf.chunks->xoff) + rendinf.chunks->info.dist;
    nz = rendinf.chunks->info.width - ((z - rendinf.chunks->zoff) + rendinf.chunks->info.dist) - 1;
    if (nx < 0 || nz < 0 || nx >= rendinf.chunks->info.width || nz >= rendinf.chunks->info.width) {
        free(_vptr);
        free(_vptr2);
        goto lblcontinue;
    }
    uint64_t c = nx + nz * rendinf.chunks->info.width;
    if (id >= rendinf.chunks->renddata[c].updateid) {
        if (rendinf.chunks->renddata[c].vertices[0]) free(rendinf.chunks->renddata[c].vertices[0]);
        if (rendinf.chunks->renddata[c].vertices[1]) free(rendinf.chunks->renddata[c].vertices[1]);
        rendinf.chunks->renddata[c].remesh[0] = true;
        rendinf.chunks->renddata[c].remesh[1] = true;
        rendinf.chunks->renddata[c].vcount[0] = vplen;
        rendinf.chunks->renddata[c].vcount[1] = vplen2;
        rendinf.chunks->renddata[c].vertices[0] = _vptr;
        rendinf.chunks->renddata[c].vertices[1] = _vptr2;
        //_sortChunk(c, (x - cxo), (z - czo), false);
        //rendinf.chunks->renddata[c].ready = true;
        rendinf.chunks->renddata[c].updateid = id;
        //printf("meshed: [%"PRId64", %"PRId64"] ([%"PRId64", %"PRId64"])\n", x, z, nx, nz);
        /*
        double time = (altutime() - stime) / 1000.0;
        printf("meshed: [%"PRId64", %"PRId64"] in [%lgms]\n", x, z, time);
        */
    } else {
        free(_vptr);
        free(_vptr2);
    }
    lblcontinue:;
    pthread_mutex_unlock(&rendinf.chunks->lock);
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
    while (!quitRequest) {
        bool activity = false;
        int p = 0;
        if (getNextMsg(&chunkmsgs[(p = CHUNKUPDATE_PRIO_HIGH)], &msg) || getNextMsg(&chunkmsgs[(p = CHUNKUPDATE_PRIO_LOW)], &msg)) {
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
            /*
            double time = (altutime() - stime) / 1000.0;
            printf("meshed: [%"PRId64", %"PRId64"] in [%lgms]\n", msg.x, msg.z, time);
            */
        }
        if (activity) {
            acttime = altutime();
            microwait(1000);
        } else if (altutime() - acttime > 500000) {
            microwait(10000);
        }
    }
    return NULL;
}

void updateChunks() {
    pthread_mutex_lock(&rendinf.chunks->lock);
    for (uint32_t c = 0; !quitRequest && c < rendinf.chunks->info.widthsq; ++c) {
        if (!rendinf.chunks->renddata[c].ready || !rendinf.chunks->renddata[c].visible) continue;
        if (!rendinf.chunks->renddata[c].init) {
            glGenBuffers(2, rendinf.chunks->renddata[c].VBO);
            rendinf.chunks->renddata[c].init = true;
        }
        if (rendinf.chunks->renddata[c].remesh[0]) {
            uint32_t tmpsize = rendinf.chunks->renddata[c].vcount[0] * sizeof(uint32_t);
            glBindBuffer(GL_ARRAY_BUFFER, rendinf.chunks->renddata[c].VBO[0]);
            glBufferData(GL_ARRAY_BUFFER, tmpsize, rendinf.chunks->renddata[c].vertices[0], GL_STATIC_DRAW);
            rendinf.chunks->renddata[c].tcount[0] = rendinf.chunks->renddata[c].vcount[0] / 3;
            //printf("[%u][%d]: [%d]->[%d]\n", c, i, rendinf.chunks->renddata[c].vcount[0], rendinf.chunks->renddata[c].tcount[0]);
            free(rendinf.chunks->renddata[c].vertices[0]);
            rendinf.chunks->renddata[c].vertices[0] = NULL;
            rendinf.chunks->renddata[c].remesh[0] = false;
        }
        if (rendinf.chunks->renddata[c].remesh[1]) {
            uint32_t tmpsize = rendinf.chunks->renddata[c].vcount[1] * sizeof(uint32_t);
            glBindBuffer(GL_ARRAY_BUFFER, rendinf.chunks->renddata[c].VBO[1]);
            glBufferData(GL_ARRAY_BUFFER, tmpsize, rendinf.chunks->renddata[c].vertices[1], GL_DYNAMIC_DRAW);
            rendinf.chunks->renddata[c].tcount[1] = rendinf.chunks->renddata[c].vcount[1] / 3;
            rendinf.chunks->renddata[c].sortvert = realloc(rendinf.chunks->renddata[c].sortvert, tmpsize);
            memcpy(rendinf.chunks->renddata[c].sortvert, rendinf.chunks->renddata[c].vertices[1], tmpsize);
            //free(rendinf.chunks->renddata[c].vertices[1]);
            //rendinf.chunks->renddata[c].vertices[1] = NULL;
            rendinf.chunks->renddata[c].remesh[1] = false;
            if (tmpsize) {
                /*
                int tmpx = ((int)(c % rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist) + (int)cxo;
                int tmpy = -((int)(c / rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist) + (int)czo;
                if (!tmpx && !tmpy) printf("[%d][%d]\n", tmpx, tmpy);
                */
                _sortChunk(c, (int)(c % rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist, -((int)(c / rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist), true);
            }
        }
        rendinf.chunks->renddata[c].ready = false;
        rendinf.chunks->renddata[c].buffered = true;
    }
    pthread_mutex_unlock(&rendinf.chunks->lock);
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
            sprintf(name, "%.8s:msh%d", name2, i);
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

static force_inline void meshUIElem(struct meshdata* md, struct ui_data* elemdata, struct ui_elem* e) {
    struct ui_elem_calcprop* p = &e->calcprop;
    int s = elemdata->scale;
    char* curprop;
    {
        int x0 = p->x, y0 = p->y, x1 = p->x + p->width, y1 = p->y + p->height;
        switch (e->type) {
            case UI_ELEM_BOX:; {
                writeuielemrect(md, x0, y0, x1, y1, p->z, p->r, p->g, p->b, p->a);
                break;
            }
            case UI_ELEM_FANCYBOX:; {
                writeuielemrect(md, x0, y0 + s, x1, y1 - s, p->z, p->r / 2, p->g / 2, p->b / 2, p->a);
                writeuielemrect(md, x0 + s, y0, x1 - s, y1, p->z, p->r / 2, p->g / 2, p->b / 2, p->a);
                writeuielemrect(md, x0 + 2 * s, y0 + 2 * s, x1 - 2 * s, y1 - 2 * s, p->z, p->r, p->g, p->b, p->a);
                break;
            }
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
                break;
            }
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
                break;
            }
            case UI_ELEM_BUTTON:; {
                writeuielemrect(md, x0, y0 + s, x1, y1 - s, p->z, p->r / 2, p->g / 2, p->b / 2, p->a);
                writeuielemrect(md, x0 + s, y0, x1 - s, y1, p->z, p->r / 2, p->g / 2, p->b / 2, p->a);
                writeuielemrect(md, x0 + 2 * s,     y0 + 2 * s, x1 - 2 * s, y1 - 2 * s, p->z, p->r, p->g, p->b, p->a);
                break;
            }
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
                    int tmpw = tdata[l].width + tcw * s;
                    for (int i = 1; t[i] && t[i] != ' ' && t[i] != '\t' && t[i] != '\n'; ++i) {
                        tmpw += tcw * s;
                    }
                    if (tmpw > p->width) {
                        nextline(t + 1);
                    } else {
                        tdata[l].width += tcw * s;
                        ++tdata[l].chars;
                    }
                } else if (*t == '\n') {
                    nextline(t + 1);
                } else {
                    tdata[l].width += tcw * s;
                    if (tdata[l].width > p->width) {
                        tdata[l].width -= tcw * s;
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
                default:; y = ((float)p->y + (float)p->height / 2.0) - (float)(lines * tch * s) / 2.0; break;
                case 1:; y = p->y + (p->height - lines * tch * s); break;
            }
            for (int i = 0; i < lines; ++i) {
                switch (ax) {
                    case -1:; x = p->x; break;
                    default:; x = ((float)p->x + (float)p->width / 2.0) - (float)tdata[i].width / 2.0; break;
                    case 1:; x = (end - tdata[i].width); break;
                }
                for (int j = 0; j < tdata[i].chars; ++j) {
                    uint8_t ol = 0, or = tcw * s, ot = 0, ob = tch * s, stcw = tcw * s, stch = tch * s;
                    if (/*x + or >= p->x && x <= p->x + p->width &&*/ y + ob >= p->y && y <= p->y + p->height) {
                        //if (x + or > p->x + p->width) or -= (x + or) - (p->x + p->width);
                        if (y + ob > p->y + p->height) ob -= (y + ob) - (p->y + p->height);
                        //if (x + ol < p->x) ol += (p->x) - (x + or);
                        if (y + ot < p->y) ot += (p->y) - (y + ot);
                        writeuitextchar(md, x, y, p->z, ol, ot, or, ob, stcw, stch, tdata[i].ptr[j], attrib, fgc, bgc, fga, bga);
                    }
                    x += tcw * s;
                }
                y += tch * s;
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
    if (e->calcprop.hidden) return;
    meshUIElem(md, elemdata, e);
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

static int dbgtextuih = -1;

void render() {
    if (showDebugInfo) {
        static char tbuf[1][32768];
        static int toff = 0;
        if (game_ui[UILAYER_DBGINF] && dbgtextuih == -1) {
            dbgtextuih = newUIElem(
                game_ui[UILAYER_DBGINF], UI_ELEM_CONTAINER, "debugText", -1, -1,
                "width", "100%", "height", "100%",
                "text_align", "-1,-1", "text_fgc", "14", "text_bga", "0.5",
                "text", "[Please wait...]",
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
        sprintf(
            &tbuf[0][toff],
            "FPS: %.2lf (%.2lf)\n"
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
            rendinf.chunks->xoff, rendinf.chunks->zoff
        );
        if (game_ui[UILAYER_DBGINF]) editUIElem(game_ui[UILAYER_DBGINF], dbgtextuih, NULL, -1, "text", tbuf, NULL);
    }

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

    pthread_mutex_unlock(&rendinf.chunks->lock);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    int32_t rendc = 0;
    for (int32_t c = rendinf.chunks->info.widthsq - 1; c >= 0; --c) {
        rendc = rendinf.chunks->rordr[c].c;
        avec2 coord = {(int)(rendc % rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist, (int)(rendc / rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist};
        setUniform2f(rendinf.shaderprog, "ccoord", coord);
        if ((rendinf.chunks->renddata[rendc].visible = isVisible(&frust, coord[0] * 16 - 8, 0, coord[1] * 16 - 8, coord[0] * 16 + 8, 256, coord[1] * 16 + 8)) && rendinf.chunks->renddata[rendc].buffered) {
            if (rendinf.chunks->renddata[rendc].tcount[0]) {
                glBindBuffer(GL_ARRAY_BUFFER, rendinf.chunks->renddata[rendc].VBO[0]);
                glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(0));
                glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t)));
                glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 2));
                glDrawArrays(GL_TRIANGLES, 0, rendinf.chunks->renddata[rendc].tcount[0]);
            }
        }
    }
    glDepthMask(false);
    for (int32_t c = 0; c < (int)rendinf.chunks->info.widthsq; ++c) {
        rendc = rendinf.chunks->rordr[c].c;
        avec2 coord = {(int)(rendc % rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist, (int)(rendc / rendinf.chunks->info.width) - (int)rendinf.chunks->info.dist};
        setUniform2f(rendinf.shaderprog, "ccoord", coord);
        if (rendinf.chunks->renddata[rendc].visible && rendinf.chunks->renddata[rendc].buffered) {
            if (rendinf.chunks->renddata[rendc].tcount[1]) {
                glBindBuffer(GL_ARRAY_BUFFER, rendinf.chunks->renddata[rendc].VBO[1]);
                glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(0));
                glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t)));
                glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 3 * sizeof(uint32_t), (void*)(sizeof(uint32_t) * 2));
                glDrawArrays(GL_TRIANGLES, 0, rendinf.chunks->renddata[rendc].tcount[1]);
            }
        }
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
    setUniform3f(rendinf.shaderprog, "mcolor", (float[]){screenmult.r, screenmult.g, screenmult.b});
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "texData"), UIFBTEXID - GL_TEXTURE0);
    setUniform3f(rendinf.shaderprog, "mcolor", (float[]){1, 1, 1});
    glClearColor(0, 0, 0, 0);
    for (int i = 0; i < 4; ++i) {
        setShaderProg(shader_ui);
        glBindFramebuffer(GL_FRAMEBUFFER, UIFBO);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        if (renderUI(game_ui[i])) {
            //printf("renderUI(game_ui[%d])\n", i);
            setShaderProg(shader_framebuffer);
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

bool initRenderer() {
    declareConfigKey(config, "Renderer", "compositing", "true", false);
    declareConfigKey(config, "Renderer", "resolution", "1024x768", false);
    declareConfigKey(config, "Renderer", "fullScreenRes", "", false);
    declareConfigKey(config, "Renderer", "vSync", "true", false);
    declareConfigKey(config, "Renderer", "fullScreen", "false", false);
    declareConfigKey(config, "Renderer", "FOV", "85", false);
    declareConfigKey(config, "Renderer", "nearPlane", "0.05", false);
    declareConfigKey(config, "Renderer", "farPlane", "2500", false);
    declareConfigKey(config, "Renderer", "lazyMesher", "false", false);

    rendinf.campos = GFX_DEFAULT_POS;
    rendinf.camrot = GFX_DEFAULT_ROT;

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

bool startRenderer() {
    #if defined(USESDL2)
    if (!(rendinf.window = SDL_CreateWindow(PROG_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, rendinf.win_width, rendinf.win_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE))) {
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
    glver = glGetString(GL_VERSION);
    printf("OpenGL version: %s\n", glver);
    glslver = glGetString(GL_SHADING_LANGUAGE_VERSION);
    printf("GLSL version: %s\n", glslver);
    glvend = glGetString(GL_VENDOR);
    printf("Vendor string: %s\n", glvend);
    glrend = glGetString(GL_RENDERER);
    printf("Renderer string: %s\n", glrend);
    if (GL_KHR_debug) {
        puts("KHR_debug supported");
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
        fputs("startRenderer: Failed to compile UI shader\n", stderr);
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

    printf("Windowed resolution: [%ux%u@%g]\n", rendinf.win_width, rendinf.win_height, rendinf.win_fps);
    printf("Fullscreen resolution: [%ux%u@%g]\n", rendinf.full_width, rendinf.full_height, rendinf.full_fps);

    #if defined(USESDL2)
    #else
    glfwSetFramebufferSizeCallback(rendinf.window, fbsize);
    #endif
    glGenRenderbuffers(1, &UIDBUF);
    glGenRenderbuffers(1, &DBUF);
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
                        //printf("! [%d][%d]: [%u]\n", i, j, (uint8_t)img->data[k]);
                        if (img->data[k]) {
                            blockinf[i].data[s].transparency = 2;
                            //img->data[k] = 215;
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
    glEnableVertexAttribArray(3);

    //water = blockNoFromID("water");

    free(tmpbuf);

    initMsgData(&chunkmsgs[CHUNKUPDATE_PRIO_LOW]);
    initMsgData(&chunkmsgs[CHUNKUPDATE_PRIO_HIGH]);

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
