#include <renderer.h>
#include <common.h>
#include <resource.h>
#include <main.h>
#include <input.h>
#include <noise.h>
#include <game.h>

#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

renderer_info rendinf;
static resdata_bmd* blockmodel;
unsigned char* texmap[6];
texture_t texmaph[6];

float gfx_aspect = 1.0;

void setMat4(GLuint prog, char* name, mat4 val) {
    int uHandle = glGetUniformLocation(prog, name);
    glUniformMatrix4fv(uHandle, 1, GL_FALSE, *val);
}

void setUniform3f(GLuint prog, char* name, float val[3]) {
    int uHandle = glGetUniformLocation(prog, name);
    glUniform3f(uHandle, val[0], val[1], val[2]);
}

void updateCam() {
    mat4 view __attribute__((aligned (32)));
    mat4 projection __attribute__((aligned (32)));
    glm_perspective(rendinf.camfov * M_PI / 180.0, gfx_aspect, 0.1, 1024.0, projection);
    setMat4(rendinf.shaderprog, "projection", projection);
    vec3 direction __attribute__((aligned (32))) = {cosf((rendinf.camrot.y - 90.0) * M_PI / 180.0) * cosf(rendinf.camrot.x * M_PI / 180.0),
        sin(rendinf.camrot.x * M_PI / 180.0),
        sinf((rendinf.camrot.y - 90.0) * M_PI / 180.0) * cosf(rendinf.camrot.x * M_PI / 180.0)};
    direction[0] += rendinf.campos.x;
    direction[1] += rendinf.campos.y;
    direction[2] += rendinf.campos.z;
    glm_lookat((vec3){rendinf.campos.x, rendinf.campos.y, rendinf.campos.z},
        direction, (vec3){0.0, 1.0, 0.0}, view);
    setMat4(rendinf.shaderprog, "view", view);
    /*
    puts("view:");
    for (int i = 0; i < 4; ++i) {
        printf("[%f] [%f] [%f] [%f]\n", view[i][0], view[i][1], view[i][2], view[i][3]);
    }
    */
    //setUniform3f(rendinf.shaderprog, "viewPos", (float[]){rendinf.campos.x, rendinf.campos.y, rendinf.campos.z});
}

void setFullscreen(bool fullscreen) {
    static int winox, winoy = 0;
    if (fullscreen) {
        gfx_aspect = (float)rendinf.full_width / (float)rendinf.full_height;
        rendinf.width = rendinf.full_width;
        rendinf.height = rendinf.full_height;
        rendinf.fps = rendinf.full_fps;
        glfwGetWindowPos(rendinf.window, &winox, &winoy);
        glfwSetWindowMonitor(rendinf.window, rendinf.monitor, 0, 0, rendinf.full_width, rendinf.full_height, rendinf.full_fps);
        rendinf.fullscr = true;
    } else {
        gfx_aspect = (float)rendinf.win_width / (float)rendinf.win_height;
        rendinf.width = rendinf.win_width;
        rendinf.height = rendinf.win_height;
        rendinf.fps = rendinf.win_fps;
        int twinx, twiny;
        if (rendinf.fullscr) {
            uint64_t offset = altutime();
            glfwSetWindowMonitor(rendinf.window, NULL, 0, 0, rendinf.win_width, rendinf.win_height, rendinf.win_fps);
            do {
                glfwSetWindowPos(rendinf.window, winox, winoy);
                glfwGetWindowPos(rendinf.window, &twinx, &twiny);
            } while (altutime() - offset < 3000000 && (twinx != winox || twiny != winoy));
        }
        rendinf.fullscr = false;
    }
    mat4 projection __attribute__((aligned (32)));
    updateCam();
    setMat4(rendinf.shaderprog, "projection", projection);
}

void gfx_winch(GLFWwindow* win, int w, int h) {
    if (win == rendinf.window) glViewport(0, 0, w, h);
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

model* loadModel(char* mpath, char** tpath) {
    resdata_bmd* mdata = loadResource(RESOURCE_BMD, mpath);
    model* m = malloc(sizeof(model));
    //memset(m, 0, sizeof(model));
    m->model = mdata;
    m->pos = (coord_3d){0.0, 0.0, 0.0};
    m->rot = (coord_3d){0.0, 0.0, 0.0};
    m->scale = (coord_3d){1.0, 1.0, 1.0};
    m->renddata = malloc(mdata->parts * sizeof(model_renddata));
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

void renderPartAt(model* m, unsigned i, coord_3d pos, bool advanced) {
    //glBindVertexArray(m->renddata[i].VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m->renddata[i].VBO);
    //glBufferData(GL_ARRAY_BUFFER, m->model->part[i].vsize, m->model->part[i].vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->renddata[i].EBO);
    //glBufferData(GL_ELEMENT_ARRAY_BUFFER, m->model->part[i].isize, m->model->part[i].indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    //mat4 model __attribute__((aligned (32))) = GFX_DEFAULT_MAT4;
    //glm_translate(model, (vec3){m->pos.x + pos.x, m->pos.y + pos.y, m->pos.z + pos.z});
    /*
    if (advanced) {
        glm_rotate(model, m->rot.x * M_PI / 180, (vec3){1, 0, 0});
        glm_rotate(model, m->rot.y * M_PI / 180, (vec3){0, 1, 0});
        glm_rotate(model, m->rot.z * M_PI / 180, (vec3){0, 0, 1});
        glm_scale(model, (vec3){m->scale.x, m->scale.y, m->scale.z});
    }
    */
    setUniform3f(rendinf.shaderprog, "mPos", (float[]){m->pos.x + pos.x, m->pos.y + pos.y, m->pos.z + pos.z});
    //setMat4(rendinf.shaderprog, "model", model);
    //glBindTexture(GL_TEXTURE_2D, m->renddata[i].texture->data);
    //glUniform1i(glGetUniformLocation(rendinf.shaderprog, "TexData"), 0);
    glDrawElements(GL_TRIANGLES, m->model->part[i].isize / sizeof(uint32_t), GL_UNSIGNED_INT, 0);
    //glBindTexture(GL_TEXTURE_2D, 0);
    //glBindBuffer(GL_ARRAY_BUFFER, 0);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void renderPart(model* m, unsigned i, bool advanced) {
    renderPartAt(m, i, (coord_3d){0.0, 0.0, 0.0}, advanced);
}

void renderModelAt(model* m, coord_3d pos, bool advanced) {
    for (unsigned i = 0; i < m->model->parts; ++i) {
        renderPartAt(m, i, pos, advanced);
    }
}

void renderModel(model* m, bool advanced) {
    renderModelAt(m, (coord_3d){0.0, 0.0, 0.0}, advanced);
}

bool chunkcacheinit = false;
//chunkcachedata chunkcache;

static uint32_t maxblockid = 0;

static unsigned VAO;
static unsigned VBO[6];
static unsigned EBO[6];

void updateChunks(void* vdata) {
    chunkdata* data = vdata;
    /*
    if (!chunkcacheinit) {
        chunkcacheinit = true;
        //glGenVertexArrays(1, &chunkcache.VAO);
        //glGenBuffers(1, &chunkcache.VBO);
        //glGenBuffers(1, &chunkcache.EBO);
    }
    */
    register int w = data->coff;
    uint32_t sides = 0;
    uint64_t starttime = altutime();
    blockdata bdata;
    blockdata bdata2[6];
    //glBindVertexArray(chunkcache.VAO);
    //glBindBuffer(GL_ARRAY_BUFFER, chunkcache.VBO);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunkcache.EBO);
    for (register int y = 0; y < 256; ++y) {
        for (register int z = -w; z <= w; ++z) {
            for (register int x = -w; x <= w; ++x) {
                //printf("rendering [%d, %d, %d]...\n", x, y, z);
                bdata = getBlock(data, x, y, z);
                if (!bdata.id || bdata.id > maxblockid) continue;
                bdata2[0] = getBlock(data, x, y, z + 1);
                bdata2[1] = getBlock(data, x, y, z - 1);
                bdata2[2] = getBlock(data, x - 1, y, z);
                bdata2[3] = getBlock(data, x + 1, y, z);
                bdata2[4] = getBlock(data, x, y + 1, z);
                bdata2[5] = getBlock(data, x, y - 1, z);
                int x2 = x + data->coff;
                int z2 = z + data->coff;
                GETBLOCKPOS(data->data, x2, z2).mflags = 0;
                for (int i = 0; i < 6; ++i) {
                    if (!bdata2[i].id || bdata2[i].id > maxblockid || (bdata2[i].id == 5 && bdata.id != 5)/* || (blockinfo[bdata2[i].id].alpha && !blockinfo[bdata.id].alpha)*/) {
                        GETBLOCKPOS(data->data, x2, z2).mflags |= 1 << i;
                        ++sides;
                    }
                }
            }
        }
    }
    printf("cached in: [%f]s ([%d] sides)\n", (float)(altutime() - starttime) / 1000000.0, sides);
}

void renderChunks(void* vdata) {
    chunkdata* data = vdata;
    int yroti;
    if (rendinf.camrot.x < -45.0) {
        yroti = 5;
    } else if (rendinf.camrot.x > 45.0) {
        yroti = 6;
    } else {
        yroti = ((rendinf.camrot.y + 45) / 90);
        yroti %= 4;
        switch (yroti) {
            case 1:;
                yroti = 3;
                break;
            case 2:;
                yroti = 1;
                break;
            case 3:;
                yroti = 2;
                break;
        }
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    updateCam();
    blockdata bdata;
    register int w = data->coff;
    uint64_t starttime = altutime();
    for (int i = 0; i < 6; ++i) {
        if (i == yroti) continue;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO[i]);
        glBindBuffer(GL_ARRAY_BUFFER, VBO[i]);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glUniform1i(glGetUniformLocation(rendinf.shaderprog, "TexData"), i);
        for (register int y = 0; y < 256; ++y) {
            for (register int z = -w; z <= w; ++z) {
                for (register int x = -w; x <= w; ++x) {
                    //printf("rendering [%d, %d, %d]...\n", x, y, z);
                    int x2 = x + data->coff;
                    int z2 = z + data->coff;
                    bdata = GETBLOCKPOS(data->data, x2, z2);
                    if (!bdata.id || bdata.id > maxblockid) continue;
                    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "blockID"), bdata.id);
                    setUniform3f(rendinf.shaderprog, "mPos", (float[]){(float)x, (float)y + 0.5, (float)z});
                    if ((bdata.mflags >> i) & 1) glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                }
            }
        }
    }
    printf("rendered in: [%f]s\n", (float)(altutime() - starttime) / 1000000.0);
}

int rendererQuitRequest() {
    return (glfwWindowShouldClose(rendinf.window) != 0);
}

bool initRenderer() {
    glfwInit();
    rendinf.camfov = 50;
    rendinf.campos = GFX_DEFAULT_POS;
    rendinf.camrot = GFX_DEFAULT_ROT;
    //rendinf.camrot.y = 180;
    sscanf(getConfigVarStatic(config, "renderer.resolution", "1024x768@0", 256), "%ux%u@%u",
        &rendinf.win_width, &rendinf.win_height, &rendinf.win_fps);
    sscanf(getConfigVarStatic(config, "renderer.fullresolution", "1280x720@0", 256), "%ux%u@%u",
        &rendinf.full_width, &rendinf.full_height, &rendinf.full_fps);
    if (!rendinf.win_width || rendinf.win_width > 32767) rendinf.win_width = 640;
    if (!rendinf.win_height || rendinf.win_height > 32767) rendinf.win_height = 480;
    rendinf.vsync = getConfigValBool(getConfigVarStatic(config, "renderer.vsync", "true", 64));
    rendinf.fullscr = getConfigValBool(getConfigVarStatic(config, "renderer.fullscreen", "false", 64));
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    if (!(rendinf.monitor = glfwGetPrimaryMonitor())) {
        fputs("glfwGetPrimaryMonitor: Failed to fetch primary monitor handle\n", stderr);
        return NULL;
    }
    if (!(rendinf.window = glfwCreateWindow(rendinf.win_width, rendinf.win_height, "CaveCube", NULL, NULL))) {
        fputs("glfwCreateWindow: Failed to create window\n", stderr);
        return NULL;
    }
    glfwMakeContextCurrent(rendinf.window);
    glfwSetInputMode(rendinf.window, GLFW_STICKY_KEYS, GLFW_TRUE);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fputs("gladLoadGLLoader: Failed to initialize GLAD\n", stderr);
        return NULL;
    }
    glfwSetFramebufferSizeCallback(rendinf.window, gfx_winch);
    file_data* vs = loadResource(RESOURCE_TEXTFILE, "engine/renderer/shaders/OpenGL/default/vertex.glsl");
    file_data* fs = loadResource(RESOURCE_TEXTFILE, "engine/renderer/shaders/OpenGL/default/fragment.glsl");
    if (!makeShaderProg((char*)vs->data, (char*)fs->data, &rendinf.shaderprog)) {
        return NULL;
    }
    glUseProgram(rendinf.shaderprog);
    setFullscreen(rendinf.fullscr);
    glViewport(0, 0, rendinf.width, rendinf.height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glClearColor(0, 0, 0.2, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(rendinf.window);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(rendinf.window);
    updateCam();
    glfwPollEvents();
    //glUniform1i(glGetUniformLocation(rendinf.shaderprog, "is2D"), 1);
    //glUniform1i(glGetUniformLocation(rendinf.shaderprog, "fIs2D"), 1);
    //glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CCW);
    //return true;
    puts("loading block model...");
    blockmodel = loadResource(RESOURCE_BMD, "game/models/block/default.bmd");
    for (int i = 0; i < 6; ++i) {
        texmap[i] = malloc(262144);
        memset(texmap[i], 255, 262144);
    }
    puts("creating texture map...");
    char* tmpbuf = malloc(4096);
    for (int i = 1; i < 256; ++i) {
        sprintf(tmpbuf, "game/textures/blocks/%d/", i);
        printf("loading block [%d]...\n", i);
        if (resourceExists(tmpbuf) == -1) break;
        maxblockid = i;
        for (int j = 0; j < 6; ++j) {
            sprintf(tmpbuf, "game/textures/blocks/%d/%d.png", i, j);
            resdata_image* img = loadResource(RESOURCE_IMAGE, tmpbuf);
            printf("adding texture {%s} at offset [%u] of map [%d]...\n", tmpbuf, 1024 * i, j);
            memcpy(&texmap[j][1024 * i], img->data, 1024);
            freeResource(img);
        }
    }
    for (int i = 0; i < 6; ++i) {
        glGenBuffers(1, &VBO[i]);
        glGenBuffers(1, &EBO[i]);
        glBindBuffer(GL_ARRAY_BUFFER, VBO[i]);
        glBufferData(GL_ARRAY_BUFFER, blockmodel->part[i].vsize, blockmodel->part[i].vertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO[i]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, blockmodel->part[i].isize, blockmodel->part[i].indices, GL_STATIC_DRAW);
        glGenTextures(1, &texmaph[i]);
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_3D, texmaph[i]);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 16, 16, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, texmap[i]);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        //sprintf(tmpbuf, "TexData%d", i);
        //glUniform1i(glGetUniformLocation(rendinf.shaderprog, tmpbuf), i);
        //glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE6);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "TexData2D"), 6);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "notBlock"), 0);
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    free(tmpbuf);
    return true;
}

void quitRenderer() {
    glfwTerminate();
}
