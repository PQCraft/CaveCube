#include <renderer.h>
#include <common.h>
#include <resource.h>
#include <main.h>
#include <input.h>
#include <noise.h>

#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

renderer_info rendinf;

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

model* loadModel(char* mpath, ...) {
    va_list args;
    va_start(args, mpath);
    resdata_bmd* mdata = loadResource(RESOURCE_BMD, mpath);
    model* m = malloc(sizeof(model));
    //memset(m, 0, sizeof(model));
    m->model = mdata;
    m->pos = (coord_3d){0.0, 0.0, 0.0};
    m->rot = (coord_3d){0.0, 0.0, 0.0};
    m->scale = (coord_3d){1.0, 1.0, 1.0};
    m->renddata = malloc(mdata->parts * sizeof(model_renddata));
    for (unsigned i = 0; i < mdata->parts; ++i) {
        resdata_texture* tdata = loadResource(RESOURCE_TEXTURE, va_arg(args, char*));
        m->renddata[i].texture = tdata;
        glGenVertexArrays(1, &m->renddata[i].VAO);
        glGenBuffers(1, &m->renddata[i].VBO);
        glGenBuffers(1, &m->renddata[i].EBO);
        glBindVertexArray(m->renddata[i].VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m->renddata[i].VBO);
        glBufferData(GL_ARRAY_BUFFER, m->model->part[i].vsize, m->model->part[i].vertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->renddata[i].EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m->model->part[i].isize, m->model->part[i].indices, GL_STATIC_DRAW);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    va_end(args);
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

void renderModelAt(model* m, coord_3d pos, bool advanced) {
    for (unsigned i = 0; i < m->model->parts; ++i) {
        glBindVertexArray(m->renddata[i].VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m->renddata[i].VBO);
        //glBufferData(GL_ARRAY_BUFFER, m->model->part[i].vsize, m->model->part[i].vertices, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->renddata[i].EBO);
        //glBufferData(GL_ELEMENT_ARRAY_BUFFER, m->model->part[i].isize, m->model->part[i].indices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        mat4 model __attribute__((aligned (32))) = GFX_DEFAULT_MAT4;
        glm_translate(model, (vec3){m->pos.x + pos.x, m->pos.y + pos.y, m->pos.z + pos.z});
        if (advanced) {
            glm_rotate(model, m->rot.x * M_PI / 180, (vec3){1, 0, 0});
            glm_rotate(model, m->rot.y * M_PI / 180, (vec3){0, 1, 0});
            glm_rotate(model, m->rot.z * M_PI / 180, (vec3){0, 0, 1});
            glm_scale(model, (vec3){m->scale.x, m->scale.y, m->scale.z});
        }
        setMat4(rendinf.shaderprog, "model", model);
        glBindTexture(GL_TEXTURE_2D, m->renddata[i].texture->data);
        //glUniform1i(glGetUniformLocation(rendinf.shaderprog, "TexData"), 0);
        glDrawElements(GL_TRIANGLES, m->model->part[i].isize / sizeof(uint32_t), GL_UNSIGNED_INT, 0);
        //glBindTexture(GL_TEXTURE_2D, 0);
        //glBindBuffer(GL_ARRAY_BUFFER, 0);
        //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

void renderModel(model* m, bool advanced) {
    renderModelAt(m, (coord_3d){0.0, 0.0, 0.0}, advanced);
}

int rendererQuitRequest() {
    return (glfwWindowShouldClose(rendinf.window) != 0);
}

extern float posmult;
extern float rotmult;
extern float fpsmult;

void testRenderer() {
    char* tpath = "game/textures/blocks/stone/0.png";
    model* m2 = loadModel("game/models/block/default.bmd", tpath, tpath, tpath, tpath, tpath, tpath);
    //tpath = "game/textures/blocks/dirt/0.png";
    #define PREFIX1 "game/textures/blocks/grass/"
    model* m1 = loadModel("game/models/block/default.bmd", PREFIX1"0.png", PREFIX1"1.png", PREFIX1"2.png", PREFIX1"3.png", PREFIX1"4.png", PREFIX1"5.png");
    tpath = "game/textures/blocks/gravel/0.png";
    model* m3 = loadModel("game/models/block/default.bmd", tpath, tpath, tpath, tpath, tpath, tpath);
    tpath = "game/textures/blocks/bedrock/0.png";
    model* m4 = loadModel("game/models/block/default.bmd", tpath, tpath, tpath, tpath, tpath, tpath);
    m1->pos = (coord_3d){0.0, 0.5, -2.0};
    m2->pos = (coord_3d){0.0, 2.5, 2.0};
    m3->pos = (coord_3d){-2.0, 1.5, 0.0};
    m4->pos = (coord_3d){2.0, 3.5, 0.0};
    rendinf.campos.y = 1.5;
    initInput();
    float opm = posmult;
    float orm = rotmult;
    initNoiseTable();
    while (!quitRequest) {
        uint64_t starttime = altutime();
        glfwPollEvents();
        testInput();
        updateCam();
        //printf("[%f]\n", rendinf.camrot.y);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //putchar('\n');
        for (uint32_t i = 0; i < 48; ++i) {
            for (uint32_t j = 0; j < 48; ++j) {
                double s = perlin2d((double)(j) / 5, (double)(i) / 5, 1.0, 1);
                s *= 4;
                //printf("[%f]\n", s);
                renderModelAt(m1, (coord_3d){0.0 - (float)j, -1.0 - (int)s, 0.0 - (float)i}, false);
            }
        }
        //putchar('\n');
        renderModel(m2, false);
        renderModel(m3, false);
        renderModel(m4, false);
        glfwSwapInterval(rendinf.vsync);
        glfwSwapBuffers(rendinf.window);
        fpsmult = (float)(altutime() - starttime) / (1000000.0f / 60.0f);
        posmult = opm * fpsmult;
        rotmult = orm * fpsmult;
    }
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
    return false;
}

void quitRenderer() {
    glfwTerminate();
}
