#include <renderer.h>
#include <common.h>
#include <resource.h>
#include <main.h>
#include <input.h>
#include <noise.h>
#include <game.h>
#include <chunk.h>

#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

struct renderer_info rendinf;
//static resdata_bmd* blockmodel;
unsigned char* texmap;
texture_t texmaph;

float gfx_aspect = 1.0;

void setMat4(GLuint prog, char* name, mat4 val) {
    glUniformMatrix4fv(glGetUniformLocation(prog, name), 1, GL_FALSE, *val);
}

void setUniform3f(GLuint prog, char* name, float val[3]) {
    glUniform3f(glGetUniformLocation(prog, name), val[0], val[1], val[2]);
}

void setUniform4f(GLuint prog, char* name, float val[4]) {
    glUniform4f(glGetUniformLocation(prog, name), val[0], val[1], val[2], val[3]);
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
    updateCam();
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
    static int lv = 0;
    if (rendinf.vsync != lv) {glfwSwapInterval(rendinf.vsync); lv = rendinf.vsync;}
    glfwSwapBuffers(rendinf.window);
}

uint32_t chunkcachesize = 0;
bool chunkcacheinit = false;
//chunkcachedata chunkcache;

static uint32_t maxblockid = 0;

static unsigned VAO;

static struct blockdata getBlock(struct chunkdata* data, int32_t c, int x, int y, int z) {
    //x += data->dist;
    //z += data->dist;
    int max = 16 - 1;
    if (x < 0 && c % data->width) {c -= 1; x += 16;}
    else if (x > 15 && (c + 1) % data->width) {c += 1; x -= 16;}
    if (z > 15 && c % (int)data->widthsq >= (int)data->width) {c -= data->width; z -= 16;}
    else if (z < 0 && c % (int)data->widthsq < (int)(data->widthsq - data->width)) {c += data->width; z += 16;}
    if (y < 0 && c >= (int)data->widthsq) {c -= data->widthsq; y += 16;}
    else if (y > 15 && c < (int)(data->size - data->widthsq)) {c += data->widthsq; y -= 16;}
    if (c < 0 || c >= (int32_t)data->size || x < 0 || y < 0 || z < 0 || x > max || y > 15 || z > max) return (struct blockdata){255, 0, 0, 0};
    if (!data->renddata[c].generated) return (struct blockdata){255, 0, 0, 0};
    //return (struct blockdata){0, 0, 0 ,0};
    //printf("block [%d, %d, %d]: [%d]\n", x, y, z, y * 225 + (z % 15) * 15 + (x % 15));
    //printf("[%d] [%d]: [%d]\n", x, z, ((x / 15) % data->width) + ((x / 15) / data->width));
    return data->data[c][y * 256 + z * 16 + x];
}

//[1 bit: x + 1][4 bits: x][1 bit: y + 1][4 bits: y][1 bit: z + 1][4 bits: z][3 bits: tex map][2 bits: tex coords][4 bits: lighting][8 bits: block id]
static uint32_t constBlockVert[6][6] = {
    {0x84201000, 0x04203000, 0x00202000, 0x00202000, 0x80200000, 0x84201000},
    {0x04001000, 0x84003000, 0x80002000, 0x80002000, 0x00000000, 0x04001000},
    {0x04201000, 0x04003000, 0x00002000, 0x00002000, 0x00200000, 0x04201000},
    {0x84001000, 0x84203000, 0x80202000, 0x80202000, 0x80000000, 0x84001000},
    {0x04201000, 0x84203000, 0x84002000, 0x84002000, 0x04000000, 0x04201000},
    {0x00001000, 0x80003000, 0x80202000, 0x80202000, 0x00200000, 0x00001000},
};

bool updateChunks(void* vdata) {
    struct chunkdata* data = vdata;
    static uint32_t ucleftoff = 0;
    /*
    static uint64_t totaltime = 0;
    */
    uint64_t starttime = altutime();
    struct blockdata bdata;
    struct blockdata bdata2[6];
    /*
    for (uint32_t c = 0; c < data->size; ++c) {
        if (!data->renddata[c].updated) data->renddata[c].vcount = 0;
    }
    */
    for (uint32_t c = 0/*, c2 = 0*/; ; ++c) {
        if (c >= data->size) {ucleftoff = 0; break;}
        //if (c2 > 25) {ucleftoff = c; break;}
        if (altutime() - starttime >= 1000000 / (((rendinf.fps) ? rendinf.fps : 60) * 2)) {ucleftoff = c; break;}
        //uint64_t starttime2 = altutime();
        if (!data->renddata[c].generated) {data->renddata[c].updated = false; continue;}
        if (data->renddata[c].updated) continue;
        //++c2;
        data->renddata[c].vertices = realloc(data->renddata[c].vertices, 147456 * sizeof(uint32_t));
        data->renddata[c].vertices2 = realloc(data->renddata[c].vertices2, 147456 * sizeof(uint32_t));
        uint32_t* vptr = data->renddata[c].vertices;
        uint32_t* vptr2 = data->renddata[c].vertices2;
        uint32_t tmpsize = 0;
        uint32_t tmpsize2 = 0;
        for (int y = 0; y < 16; ++y) {
            for (int z = 0; z < 16; ++z) {
                for (int x = 0; x < 16; ++x) {
                    bdata = getBlock(data, c, x, y, z);
                    if (!bdata.id || bdata.id > maxblockid) continue;
                    bdata2[0] = getBlock(data, c, x, y, z + 1);
                    bdata2[1] = getBlock(data, c, x, y, z - 1);
                    bdata2[2] = getBlock(data, c, x - 1, y, z);
                    bdata2[3] = getBlock(data, c, x + 1, y, z);
                    bdata2[4] = getBlock(data, c, x, y + 1, z);
                    bdata2[5] = getBlock(data, c, x, y - 1, z);
                    for (int i = 0; i < 6; ++i) {
                        if (bdata2[i].id && bdata2[i].id <= maxblockid && (bdata2[i].id != 5 || bdata.id == 5) && (bdata2[i].id != 7 || bdata.id == 7)) continue;
                        if (bdata2[i].id == 255) continue;
                        uint32_t baseVert = (x << 27) | (y << 22) | (z << 17) | (i << 14) | bdata.id;
                        if (bdata.id == 7) {
                            for (int j = 0; j < 6; ++j) {
                                *vptr2 = baseVert | constBlockVert[i][j];
                                ++vptr2;
                            }
                            //printf("added [%d][%d %d %d][%d]: [%u]: [%x]...\n", c, x, y, z, i, (uint8_t)bdata.id, baseVert);
                            ++tmpsize2;
                        } else {
                            for (int j = 0; j < 6; ++j) {
                                *vptr = baseVert | constBlockVert[i][j];
                                ++vptr;
                            }
                            //printf("added [%d][%d %d %d][%d]: [%u]: [%x]...\n", c, x, y, z, i, (uint8_t)bdata.id, baseVert);
                            ++tmpsize;
                        }
                    }
                }
            }
        }
        //uint32_t tmpsize2 = tmpsize;
        chunkcachesize += tmpsize + tmpsize2;
        tmpsize *= 6;
        tmpsize2 *= 6;
        /*
        for (uint32_t i = 0; i < tmpsize; ++i) {
            data->renddata[c].vertices[i] = (getRandByte() << 24) | (getRandByte() << 16) | (getRandByte() << 8) | getRandByte();
        }
        */
        data->renddata[c].vcount = tmpsize;
        data->renddata[c].vcount2 = tmpsize2;
        tmpsize *= sizeof(uint32_t);
        tmpsize2 *= sizeof(uint32_t);
        data->renddata[c].vertices = realloc(data->renddata[c].vertices, tmpsize);
        data->renddata[c].vertices2 = realloc(data->renddata[c].vertices2, tmpsize2);
        //glGenVertexArrays(1, &data->renddata[c].VAO);
        //glBindVertexArray(data->renddata[c].VAO);
        if (!data->renddata[c].VBO) glGenBuffers(1, &data->renddata[c].VBO);
        if (!data->renddata[c].VBO2) glGenBuffers(1, &data->renddata[c].VBO2);
        data->renddata[c].updated = true;
        if (tmpsize) {
            glBindBuffer(GL_ARRAY_BUFFER, data->renddata[c].VBO);
            glBufferData(GL_ARRAY_BUFFER, tmpsize, data->renddata[c].vertices, GL_STATIC_DRAW);
        }
        if (tmpsize2) {
            glBindBuffer(GL_ARRAY_BUFFER, data->renddata[c].VBO2);
            glBufferData(GL_ARRAY_BUFFER, tmpsize2, data->renddata[c].vertices2, GL_STATIC_DRAW);
        }
        //printf("meshed chunk [%d] ([%u] surfaces, [%u] triangles, [%u] points) ([%u] bytes) in [%f]s\n",
        //    c, tmpsize2, tmpsize2 * 2, tmpsize2 * 6, tmpsize, (float)(altutime() - starttime2) / 1000000.0);
        //glfwPollEvents();
        //if (rendererQuitRequest()) return;
    }
    bool leftoff = (ucleftoff > 0);
    /*
    if (leftoff) {
        totaltime += (altutime() - starttime);
    } else {
        printf("meshed [%u] surfaces total ([%u] triangles, [%u] points) ([%lu] bytes)\n", chunkcachesize, chunkcachesize * 2, chunkcachesize * 6, chunkcachesize * 6 * sizeof(uint32_t));
        printf("meshed in: [%f]s\n", (float)(totaltime) / 1000000.0);
        chunkcachesize = 0;
        totaltime = 0;
    }
    */
    return leftoff;
}

void renderChunks(void* vdata) {
    struct chunkdata* data = vdata;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "dist"), data->dist);
    setUniform3f(rendinf.shaderprog, "cam", (float[]){rendinf.campos.x, rendinf.campos.y, rendinf.campos.z});
    //uint64_t starttime = altutime();
    for (uint32_t c = 0; c < data->size; ++c) {
        if (!data->renddata[c].vcount) continue;
        int x = c % data->width;
        int z = c / data->width % data->width;
        int y = c / data->widthsq;
        setUniform3f(rendinf.shaderprog, "ccoord", (float[]){x - (int)data->dist, y, z - (int)data->dist});
        glBindBuffer(GL_ARRAY_BUFFER, data->renddata[c].VBO);
        glEnableVertexAttribArray(0);
        glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 0, (void*)0);
        glDrawArrays(GL_TRIANGLES, 0, data->renddata[c].vcount);
    }
    glDisable(GL_CULL_FACE);
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "isAni"), 1);
    glUniform1ui(glGetUniformLocation(rendinf.shaderprog, "TexAni"), (altutime() / 200000) % 6);
    for (uint32_t i = 0; i < data->widthsq; ++i) {
        uint32_t c = data->rordr[i].c;
        for (uint32_t y = 0; y < 16; ++y) {
            if (data->renddata[c].vcount2) {
                int x = c % data->width;
                int z = c / data->width % data->width;
                setUniform3f(rendinf.shaderprog, "ccoord", (float[]){x - (int)data->dist, y, z - (int)data->dist});
                glBindBuffer(GL_ARRAY_BUFFER, data->renddata[c].VBO2);
                glEnableVertexAttribArray(0);
                glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, 0, (void*)0);
                glDrawArrays(GL_TRIANGLES, 0, data->renddata[c].vcount2);
            }
            c += data->widthsq;
        }
    }
    glUniform1i(glGetUniformLocation(rendinf.shaderprog, "isAni"), 0);
    glEnable(GL_CULL_FACE);
    //printf("rendered in: [%f]s\n", (float)(altutime() - starttime) / 1000000.0);
}

int rendererQuitRequest() {
    return (glfwWindowShouldClose(rendinf.window) != 0);
}

static void errorcb(int e, const char* m) {
    printf("GLFW error [%d]: {%s}\n", e, m);
}

bool initRenderer() {
    glfwSetErrorCallback(errorcb);
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
        return false;
    }
    if (!(rendinf.window = glfwCreateWindow(rendinf.win_width, rendinf.win_height, "CaveCube", NULL, NULL))) {
        fputs("glfwCreateWindow: Failed to create window\n", stderr);
        return false;
    }
    glfwMakeContextCurrent(rendinf.window);
    glfwSetInputMode(rendinf.window, GLFW_STICKY_KEYS, GLFW_TRUE);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fputs("gladLoadGLLoader: Failed to initialize GLAD\n", stderr);
        return false;
    }
    glfwSetFramebufferSizeCallback(rendinf.window, gfx_winch);
    file_data* vs = loadResource(RESOURCE_TEXTFILE, "engine/renderer/shaders/OpenGL/default/block/vertex.glsl");
    file_data* fs = loadResource(RESOURCE_TEXTFILE, "engine/renderer/shaders/OpenGL/default/block/fragment.glsl");
    if (!makeShaderProg((char*)vs->data, (char*)fs->data, &rendinf.shaderprog)) {
        return false;
    }
    glUseProgram(rendinf.shaderprog);
    setFullscreen(rendinf.fullscr);
    glViewport(0, 0, rendinf.width, rendinf.height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glClearColor(0, 0.7, 0.9, 1);
    glfwSwapInterval(rendinf.vsync);
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
    //puts("loading block struct model...");
    //blockmodel = loadResource(RESOURCE_BMD, "game/models/block/default.bmd");
    for (int i = 0; i < 6; ++i) {
        texmap = malloc(1572864);
        memset(texmap, 255, 1572864);
    }
    puts("creating texture map...");
    char* tmpbuf = malloc(4096);
    for (int i = 1; i < 256; ++i) {
        sprintf(tmpbuf, "game/textures/blocks/%d/", i);
        if (resourceExists(tmpbuf) == -1) break;
        printf("loading block [%d]...\n", i);
        maxblockid = i;
        for (int j = 0; j < 6; ++j) {
            sprintf(tmpbuf, "game/textures/blocks/%d/%d.png", i, j);
            resdata_image* img = loadResource(RESOURCE_IMAGE, tmpbuf);
            printf("adding texture {%s} at offset [%u] of map [%d]...\n", tmpbuf, 1024 * i, j);
            memcpy(&texmap[j * 262144 + i * 1024], img->data, 1024);
            freeResource(img);
        }
    }
    //glGenBuffers(1, &VBO[i]);
    //glGenBuffers(1, &EBO[i]);
    //glBindBuffer(GL_ARRAY_BUFFER, VBO[i]);
    //glBufferData(GL_ARRAY_BUFFER, blockmodel->part[i].vsize, blockmodel->part[i].vertices, GL_STATIC_DRAW);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO[i]);
    //glBufferData(GL_ELEMENT_ARRAY_BUFFER, blockmodel->part[i].isize, blockmodel->part[i].indices, GL_STATIC_DRAW);
    glGenTextures(1, &texmaph);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, texmaph);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 16, 16, 1536, 0, GL_RGBA, GL_UNSIGNED_BYTE, texmap);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    //sprintf(tmpbuf, "TexData%d", i);
    //glUniform1i(glGetUniformLocation(rendinf.shaderprog, tmpbuf), i);
    //glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    //glUniform1i(glGetUniformLocation(rendinf.shaderprog, "TexData2D"), 6);
    //glUniform1i(glGetUniformLocation(rendinf.shaderprog, "notBlock"), 0);
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    //glEnableVertexAttribArray(0);
    //glEnableVertexAttribArray(1);
    free(tmpbuf);
    return true;
}

void quitRenderer() {
    glfwTerminate();
}
